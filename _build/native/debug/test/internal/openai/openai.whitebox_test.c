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

struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse;

struct _M0DTPB4Json5Array;

struct _M0TWssbEu;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE3Err;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0R195Iter_3a_3afilter__map_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_2c_20clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoice_5d_7c_2eanon__u2665__l364__;

struct _M0TWEOc;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal6openai14ChatCompletion;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails;

struct _M0TPB6Logger;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6openai33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TWRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderERP48clawteam8clawteam8internal6openai20ChatCompletionChoice;

struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice;

struct _M0R125ArrayView_3a_3aiter_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_5d_7c_2eanon__u2385__l570__;

struct _M0TPB19MulShiftAll64Result;

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2643__l591__;

struct _M0TWEOUsRPB4JsonE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json;

struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB6ToJson;

struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk;

struct _M0TWEOs;

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder;

struct _M0TP48clawteam8clawteam8internal6openai11CostDetails;

struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction;

struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok;

struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0DTPC16result6ResultGORP48clawteam8clawteam8internal6openai20ChatCompletionChoiceRPB7NoErrorE3Err;

struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3747__l428__;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE4Some;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TWRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceERPB4Json;

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage;

struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1661;

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceRPB7NoErrorE3Err;

struct _M0DTPC16option6OptionGdE4Some;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err;

struct _M0DTPC16result6ResultGsRPB7NoErrorE2Ok;

struct _M0R38String_3a_3aiter_2eanon__u2353__l247__;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3751__l427__;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE;

struct _M0TPB9ArrayViewGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0DTPC16result6ResultGORP48clawteam8clawteam8internal6openai20ChatCompletionChoiceRPB7NoErrorE2Ok;

struct _M0TPB9ArrayViewGUsRPB4JsonEE;

struct _M0DTPC16result6ResultGsRPB7NoErrorE3Err;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceRPB7NoErrorE2Ok;

struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice;

struct _M0DTPC16result6ResultGOsRPB7NoErrorE3Err;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok;

struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction;

struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE;

struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE;

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE;

struct _M0DTPC16result6ResultGOsRPB7NoErrorE2Ok;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2373__l570__;

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall;

struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails;

struct _M0DTPB4Json6Object;

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE;

struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6openai33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal6openai21ChatCompletionMessage;

struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TWRPB13StringBuilderEs;

struct _M0TPB7Umul128;

struct _M0TPB8Pow5Pair;

struct _M0DTPC14json10WriteFrame6Object {
  int32_t $1;
  struct _M0TWEOUsRPB4JsonE* $0;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse {
  int32_t $0;
  moonbit_string_t $1;
  void* $2;
  
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

struct _M0R195Iter_3a_3afilter__map_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_2c_20clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoice_5d_7c_2eanon__u2665__l364__ {
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*(* code)(
    struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice*
  );
  struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* $0;
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* $1;
  
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

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal6openai14ChatCompletion {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice {
  int64_t $0;
  int32_t $1;
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* $2;
  
};

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* $1;
  moonbit_string_t $4;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $5;
  
};

struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails {
  int64_t $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6openai33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0TWRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderERP48clawteam8clawteam8internal6openai20ChatCompletionChoice {
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*(* code)(
    struct _M0TWRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderERP48clawteam8clawteam8internal6openai20ChatCompletionChoice*,
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*
  );
  
};

struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice {
  int32_t $0;
  int64_t $2;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* $1;
  
};

struct _M0R125ArrayView_3a_3aiter_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_5d_7c_2eanon__u2385__l570__ {
  void*(* code)(
    struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*
  );
  int32_t $0_1;
  int32_t $0_2;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2643__l591__ {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* $0;
  
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

struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json {
  void*(* code)(
    struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*,
    struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*
  );
  
};

struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder {
  struct _M0TPB13StringBuilder* $0;
  struct _M0TPB13StringBuilder* $1;
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* $2;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0TPB6ToJson {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk {
  int32_t $2;
  int64_t $6;
  moonbit_string_t $0;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE* $1;
  moonbit_string_t $3;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* $4;
  moonbit_string_t $5;
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* $7;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder {
  int64_t $2;
  int64_t $6;
  moonbit_string_t $0;
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* $1;
  moonbit_string_t $3;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* $4;
  moonbit_string_t $5;
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* $7;
  
};

struct _M0TP48clawteam8clawteam8internal6openai11CostDetails {
  void* $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder {
  moonbit_string_t $0;
  moonbit_string_t $1;
  struct _M0TPB13StringBuilder* $2;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE2Ok {
  struct _M0TPB5ArrayGRPB4JsonE* $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage {
  int32_t $0;
  int32_t $4;
  int32_t $6;
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* $1;
  void* $2;
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* $3;
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* $5;
  
};

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE {
  struct _M0TPB5EntryGsRPB4JsonE* $0;
  
};

struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
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

struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some {
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* $0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGORP48clawteam8clawteam8internal6openai20ChatCompletionChoiceRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3747__l428__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0TWsRPB4JsonEORPB4Json {
  void*(* code)(struct _M0TWsRPB4JsonEORPB4Json*, moonbit_string_t, void*);
  
};

struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE4Some {
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* $0;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TWRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceERPB4Json {
  void*(* code)(
    struct _M0TWRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceERPB4Json*,
    struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*
  );
  
};

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage {
  int32_t $2;
  moonbit_string_t $0;
  moonbit_string_t $1;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* $3;
  
};

struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1661 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE {
  int32_t $1;
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice** $0;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0DTPC16option6OptionGdE4Some {
  double $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGRPB5ArrayGRPB4JsonERPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGsRPB7NoErrorE2Ok {
  moonbit_string_t $0;
  
};

struct _M0R38String_3a_3aiter_2eanon__u2353__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
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

struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3751__l427__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE {
  int32_t $1;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** $0;
  
};

struct _M0TPB9ArrayViewGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE {
  int32_t $1;
  int32_t $2;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** $0;
  
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

struct _M0DTPC16result6ResultGORP48clawteam8clawteam8internal6openai20ChatCompletionChoiceRPB7NoErrorE2Ok {
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* $0;
  
};

struct _M0TPB9ArrayViewGUsRPB4JsonEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB4JsonE** $0;
  
};

struct _M0DTPC16result6ResultGsRPB7NoErrorE3Err {
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

struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall {
  int32_t $0;
  moonbit_string_t $1;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* $2;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceRPB7NoErrorE2Ok {
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* $0;
  
};

struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice {
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*(* code)(
    struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice*,
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*
  );
  
};

struct _M0DTPC16result6ResultGOsRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall {
  moonbit_string_t $0;
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* $1;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC16result6ResultGRPB4JsonRPB7NoErrorE2Ok {
  void* $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE {
  int32_t $1;
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** $0;
  
};

struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice {
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*(* code)(
    struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice*
  );
  
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

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE {
  int32_t $1;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall** $0;
  
};

struct _M0DTPC16result6ResultGOsRPB7NoErrorE2Ok {
  moonbit_string_t $0;
  
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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2373__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta {
  int64_t $2;
  moonbit_string_t $0;
  moonbit_string_t $1;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* $3;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion {
  int32_t $2;
  int64_t $6;
  moonbit_string_t $0;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* $1;
  moonbit_string_t $3;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* $4;
  moonbit_string_t $5;
  
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

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails {
  int64_t $0;
  int64_t $1;
  
};

struct _M0DTPB4Json6Object {
  struct _M0TPB3MapGsRPB4JsonE* $0;
  
};

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE {
  int32_t $1;
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** $0;
  
};

struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder {
  int64_t $0;
  int32_t $1;
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* $2;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal6openai33MoonBitTestDriverInternalSkipTestE3Err {
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

struct _M0KTPB6ToJsonTP48clawteam8clawteam8internal6openai21ChatCompletionMessage {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE {
  void*(* code)(
    struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*
  );
  
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

struct _M0TWRPB13StringBuilderEs {
  moonbit_string_t(* code)(
    struct _M0TWRPB13StringBuilderEs*,
    struct _M0TPB13StringBuilder*
  );
  
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

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

void* _M0IP48clawteam8clawteam8internal6openai20ChatCompletionChoicePB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceERPB4Json*,
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*
);

int32_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN17error__to__stringS1670(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN14handle__resultS1661(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3751l427(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3747l428(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal6openai45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1595(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1590(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1583(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1577(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal6openai28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal6openai34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai49____test__6275696c6465725f7762746573742e6d6274__2(
  
);

struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completion(
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder*
);

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completionC3438l223(
  struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice*,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*
);

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completionC3441l223(
  struct _M0TWRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderERP48clawteam8clawteam8internal6openai20ChatCompletionChoice*,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*
);

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder10to__choice(
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*
);

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder3new(
  
);

int32_t _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder10add__chunk(
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder*,
  struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk*
);

struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0MP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder3new(
  int32_t
);

int32_t _M0MP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder10add__chunk(
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*,
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai49____test__6275696c6465725f7762746573742e6d6274__1(
  
);

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__message(
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder*
);

moonbit_string_t _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__messageC3299l103(
  struct _M0TWRPB13StringBuilderEs*,
  struct _M0TPB13StringBuilder*
);

moonbit_string_t _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__messageC3295l104(
  struct _M0TWRPB13StringBuilderEs*,
  struct _M0TPB13StringBuilder*
);

struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder3new(
  
);

int32_t _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder10add__delta(
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder*,
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai49____test__6275696c6465725f7762746573742e6d6274__0(
  
);

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder14to__tool__call(
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*
);

struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder3new(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder10add__delta(
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*,
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall*
);

void* _M0IP48clawteam8clawteam8internal6openai14ChatCompletionPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion*
);

struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion* _M0MP48clawteam8clawteam8internal6openai14ChatCompletion3new(
  moonbit_string_t,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE*,
  int32_t,
  moonbit_string_t,
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage*,
  moonbit_string_t,
  int64_t
);

void* _M0IP48clawteam8clawteam8internal6openai20ChatCompletionChoicePB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*
);

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MP48clawteam8clawteam8internal6openai20ChatCompletionChoice3new(
  int64_t,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage*
);

void* _M0IP48clawteam8clawteam8internal6openai32ChatCompletionChoiceFinishReasonPB6ToJson8to__json(
  int32_t
);

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionMessage3new(
  moonbit_string_t,
  moonbit_string_t,
  int64_t,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*
);

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionMessage11new_2einner(
  moonbit_string_t,
  moonbit_string_t,
  int32_t,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*
);

void* _M0IP48clawteam8clawteam8internal6openai25ChatCompletionMessageRolePB6ToJson8to__json(
  int32_t
);

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*
);

void* _M0IP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunctionPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction*
);

void* _M0IP48clawteam8clawteam8internal6openai25ChatCompletionServiceTierPB6ToJson8to__json(
  int32_t
);

struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0MP48clawteam8clawteam8internal6openai15CompletionUsage3new(
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails*,
  void*,
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails*,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails*,
  int32_t
);

struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice* _M0FP48clawteam8clawteam8internal6openai13chunk__choice(
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta*,
  int64_t
);

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(
  moonbit_string_t,
  moonbit_string_t,
  int64_t,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE*
);

struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(
  int32_t,
  moonbit_string_t,
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction*
);

struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk* _M0FP48clawteam8clawteam8internal6openai5chunk(
  moonbit_string_t,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE*,
  int32_t,
  moonbit_string_t,
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage*,
  moonbit_string_t,
  int64_t,
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse*
);

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0FP48clawteam8clawteam8internal6openai10tool__call(
  moonbit_string_t,
  moonbit_string_t,
  moonbit_string_t
);

void* _M0IP48clawteam8clawteam8internal6openai23CompletionTokensDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails*
);

void* _M0IP48clawteam8clawteam8internal6openai19PromptTokensDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails*
);

void* _M0IP48clawteam8clawteam8internal6openai21ChatCompletionMessagePB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage*
);

void* _M0IP48clawteam8clawteam8internal6openai11CostDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails*
);

void* _M0IP48clawteam8clawteam8internal6openai15CompletionUsagePB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage*
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

int32_t _M0MPC15array5Array6resizeGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE*,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*
);

int32_t _M0MPC15array5Array6resizeGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*
);

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE*
);

void* _M0MPC15array5Array3getGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE*,
  int32_t
);

void* _M0MPC15array5Array3getGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*,
  int32_t
);

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

int32_t _M0MPC15array5Array28unsafe__truncate__to__lengthGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE*,
  int32_t
);

int32_t _M0MPC15array5Array28unsafe__truncate__to__lengthGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*,
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

struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MPB4Iter11filter__mapGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*,
  struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice*
);

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MPB4Iter11filter__mapGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceEC2665l364(
  struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice*
);

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*
);

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*,
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*
);

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE*,
  struct _M0TWRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceERPB4Json*
);

void* _M0IPC16double6DoublePB6ToJson8to__json(double);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t);

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE*);

void* _M0MPC14json4Json6string(moonbit_string_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2643l591(
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

int32_t _M0MPC15array5Array3setGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE*,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*
);

int32_t _M0MPC15array5Array3setGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*
);

moonbit_string_t _M0MPC16option6Option3mapGRPB13StringBuildersE(
  struct _M0TPB13StringBuilder*,
  struct _M0TWRPB13StringBuilderEs*
);

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MPC16option6Option3mapGRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*,
  struct _M0TWRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderERP48clawteam8clawteam8internal6openai20ChatCompletionChoice*
);

moonbit_string_t _M0MPC16option6Option10unwrap__orGsE(
  moonbit_string_t,
  moonbit_string_t
);

moonbit_string_t _M0MPC16option6Option6unwrapGsE(moonbit_string_t);

int32_t _M0MPC16option6Option6unwrapGiE(int64_t);

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

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(uint64_t*, int32_t);

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(uint32_t*, int32_t);

struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0MPC15array5Array4iterGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*
);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0MPC15array9ArrayView4iterGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB9ArrayViewGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE
);

void* _M0MPC15array9ArrayView4iterGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEC2385l570(
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*
);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2373l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0MPB4Iter9to__arrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice*
);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2353l247(struct _M0TWEOc*);

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
);

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  struct _M0TUsiE*
);

int32_t _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*
);

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*,
  void*
);

int32_t _M0MPC15array5Array4pushGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE*,
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*
);

int32_t _M0MPC15array5Array4pushGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*
);

int32_t _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE*,
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*
);

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE*);

int32_t _M0MPC15array5Array7reallocGUsiEE(struct _M0TPB5ArrayGUsiEE*);

int32_t _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*
);

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

int32_t _M0MPC15array5Array7reallocGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE*
);

int32_t _M0MPC15array5Array7reallocGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*
);

int32_t _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE*
);

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*,
  int32_t
);

int32_t _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE*,
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

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE
);

int32_t _M0MPC15array9ArrayView6lengthGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB9ArrayViewGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE
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

struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0MPB4Iter3newGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*
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

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MPB4Iter4nextGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice*
);

void* _M0MPB4Iter4nextGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*
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

int32_t _M0MPC15array5Array6lengthGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE*
);

int32_t _M0MPC15array5Array6lengthGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*
);

moonbit_string_t* _M0MPC15array5Array6bufferGsE(struct _M0TPB5ArrayGsE*);

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE*
);

struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0MPC15array5Array6bufferGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE*
);

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*
);

struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0MPC15array5Array6bufferGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*
);

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE*
);

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE*
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

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall**,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall**,
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

int32_t _M0MPB18UninitializedArray12unsafe__blitGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder**,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder**,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder**,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder**,
  int32_t,
  int32_t
);

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice**,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice**,
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallEE(
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall**,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall**,
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderEE(
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder**,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder**,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEE(
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder**,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder**,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceEE(
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice**,
  int32_t,
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice**,
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

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE*);

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

void* _M0IP48clawteam8clawteam8internal6openai21ChatCompletionMessagePB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

void* _M0IP48clawteam8clawteam8internal6openai14ChatCompletionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
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

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 112, 101, 110, 
    97, 105, 34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 
    58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    73, 32, 119, 105, 108, 108, 32, 99, 97, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 98, 117, 105, 108, 100, 101, 114, 95, 
    119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 57, 58, 51, 
    45, 49, 49, 50, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    115, 99, 97, 108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_142 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    98, 117, 105, 108, 100, 101, 114, 95, 119, 98, 116, 101, 115, 116, 
    46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_135 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    83, 117, 114, 101, 44, 32, 73, 32, 99, 97, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    99, 111, 109, 112, 108, 101, 116, 105, 111, 110, 95, 116, 111, 107, 
    101, 110, 115, 95, 100, 101, 116, 97, 105, 108, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    34, 58, 32, 34, 83, 97, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    116, 111, 111, 108, 95, 99, 97, 108, 108, 95, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_128 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_105 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    115, 121, 115, 116, 101, 109, 95, 102, 105, 110, 103, 101, 114, 112, 
    114, 105, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    103, 112, 116, 45, 52, 111, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    99, 104, 97, 116, 99, 109, 112, 108, 45, 49, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 97, 114, 
    114, 97, 121, 46, 109, 98, 116, 58, 49, 50, 48, 53, 58, 53, 45, 49, 
    50, 48, 53, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[28]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 27), 
    83, 117, 114, 101, 44, 32, 73, 32, 99, 97, 110, 32, 104, 101, 108, 
    112, 32, 119, 105, 116, 104, 32, 116, 104, 97, 116, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    102, 105, 110, 105, 115, 104, 95, 114, 101, 97, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_83 =
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
} const moonbit_string_literal_134 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 121, 112, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_93 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 98, 117, 105, 108, 100, 101, 114, 95, 
    119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 48, 55, 58, 
    51, 45, 50, 51, 57, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    99, 111, 110, 116, 101, 110, 116, 95, 102, 105, 108, 116, 101, 114, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    123, 32, 34, 108, 111, 99, 97, 116, 105, 111, 110, 34, 58, 32, 34, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    100, 101, 102, 97, 117, 108, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 98, 117, 105, 108, 100, 101, 114, 95, 
    119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 48, 55, 58, 
    53, 52, 45, 50, 51, 57, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    112, 114, 105, 111, 114, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_126 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    115, 116, 111, 112, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    102, 105, 110, 103, 101, 114, 112, 114, 105, 110, 116, 95, 97, 98, 
    99, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_127 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    102, 117, 110, 99, 116, 105, 111, 110, 95, 99, 97, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    117, 115, 97, 103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_109 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    83, 97, 110, 32, 70, 114, 97, 110, 99, 105, 115, 99, 111, 34, 32, 
    125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    97, 117, 100, 105, 111, 95, 116, 111, 107, 101, 110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    99, 111, 115, 116, 95, 100, 101, 116, 97, 105, 108, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    99, 97, 99, 104, 101, 100, 95, 116, 111, 107, 101, 110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    99, 111, 110, 116, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    114, 101, 97, 115, 111, 110, 105, 110, 103, 95, 116, 111, 107, 101, 
    110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 98, 117, 105, 108, 100, 101, 114, 95, 
    119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 56, 58, 49, 
    54, 45, 51, 56, 58, 50, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    99, 114, 101, 97, 116, 101, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    114, 101, 102, 117, 115, 97, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_125 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    99, 111, 109, 112, 108, 101, 116, 105, 111, 110, 95, 116, 111, 107, 
    101, 110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 40), 
    73, 32, 119, 105, 108, 108, 32, 99, 97, 108, 108, 32, 116, 104, 101, 
    32, 116, 111, 111, 108, 32, 116, 111, 32, 103, 101, 116, 32, 116, 
    104, 101, 32, 119, 101, 97, 116, 104, 101, 114, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    109, 111, 100, 101, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_122 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    97, 115, 115, 105, 115, 116, 97, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_132 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    117, 112, 115, 116, 114, 101, 97, 109, 95, 105, 110, 102, 101, 114, 
    101, 110, 99, 101, 95, 99, 111, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[100]; 
} const moonbit_string_literal_136 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 99), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 112, 
    101, 110, 97, 105, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 
    66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 
    110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_118 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_119 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    45, 73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[32]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 31), 
    123, 32, 34, 108, 111, 99, 97, 116, 105, 111, 110, 34, 58, 32, 34, 
    83, 97, 110, 32, 70, 114, 97, 110, 99, 105, 115, 99, 111, 34, 32, 
    125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[102]; 
} const moonbit_string_literal_137 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 101), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 112, 
    101, 110, 97, 105, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 
    110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 
    73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 
    115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    32, 70, 114, 97, 110, 99, 105, 115, 99, 111, 34, 32, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_141 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 21), 
    67, 104, 97, 116, 67, 111, 109, 112, 108, 101, 116, 105, 111, 110, 
    66, 117, 105, 108, 100, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[68]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 67), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 98, 117, 105, 108, 100, 101, 114, 95, 
    119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 48, 55, 58, 
    49, 54, 45, 50, 48, 55, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    105, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_131 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    99, 104, 111, 105, 99, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_120 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_116 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 98, 117, 105, 108, 100, 101, 114, 95, 
    119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 56, 58, 51, 
    45, 52, 53, 58, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_138 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_84 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    114, 111, 108, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    105, 110, 100, 101, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    103, 101, 116, 95, 119, 101, 97, 116, 104, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    116, 111, 116, 97, 108, 95, 116, 111, 107, 101, 110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_133 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[30]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 29), 
    32, 116, 104, 101, 32, 116, 111, 111, 108, 32, 116, 111, 32, 103, 
    101, 116, 32, 116, 104, 101, 32, 119, 101, 97, 116, 104, 101, 114, 
    46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_5 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    34, 44, 32, 34, 116, 101, 115, 116, 95, 110, 97, 109, 101, 34, 58, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[29]; 
} const moonbit_string_literal_140 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 28), 
    67, 104, 97, 116, 67, 111, 109, 112, 108, 101, 116, 105, 111, 110, 
    77, 101, 115, 115, 97, 103, 101, 66, 117, 105, 108, 100, 101, 114, 
    0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    110, 101, 103, 97, 116, 105, 118, 101, 32, 110, 101, 119, 32, 108, 
    101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    109, 101, 115, 115, 97, 103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    99, 111, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 98, 117, 105, 108, 100, 101, 114, 95, 
    119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 57, 58, 49, 
    54, 45, 57, 57, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_123 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    97, 117, 116, 111, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 101, 114, 118, 105, 99, 101, 95, 116, 105, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    123, 32, 34, 108, 111, 99, 97, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_130 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 21), 
    112, 114, 111, 109, 112, 116, 95, 116, 111, 107, 101, 110, 115, 95, 
    100, 101, 116, 97, 105, 108, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    112, 114, 111, 109, 112, 116, 95, 116, 111, 107, 101, 110, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_121 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 97, 109, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_124 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_129 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    97, 114, 103, 117, 109, 101, 110, 116, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_139 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    67, 104, 97, 116, 67, 111, 109, 112, 108, 101, 116, 105, 111, 110, 
    77, 101, 115, 115, 97, 103, 101, 84, 111, 111, 108, 67, 97, 108, 
    108, 66, 117, 105, 108, 100, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    102, 117, 110, 99, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    116, 111, 111, 108, 95, 99, 97, 108, 108, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    32, 104, 101, 108, 112, 32, 119, 105, 116, 104, 32, 116, 104, 97, 
    116, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 98, 117, 105, 108, 100, 101, 114, 95, 
    119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 56, 58, 51, 
    53, 45, 52, 53, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 111, 
    112, 101, 110, 97, 105, 58, 98, 117, 105, 108, 100, 101, 114, 95, 
    119, 98, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 57, 58, 51, 
    51, 45, 49, 49, 50, 58, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    91, 93, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    102, 108, 101, 120, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWRPB13StringBuilderEs data; 
} const _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__messageC3295l104$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__messageC3295l104
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderERP48clawteam8clawteam8internal6openai20ChatCompletionChoice data;
  
} const _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completionC3441l223$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completionC3441l223
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPB13StringBuilderEs data; 
} const _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__messageC3299l103$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__messageC3299l103
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN17error__to__stringS1670$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN17error__to__stringS1670
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceERPB4Json data;
  
} const _M0IP48clawteam8clawteam8internal6openai20ChatCompletionChoicePB6ToJson18to__json_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0IP48clawteam8clawteam8internal6openai20ChatCompletionChoicePB6ToJson18to__json_2edyncall
  };

struct {
  int32_t rc;
  uint32_t meta;
  struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice data;
  
} const _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completionC3438l223$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completionC3438l223
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__2_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__0_2edyncall
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

struct _M0TWRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceERPB4Json* _M0IP48clawteam8clawteam8internal6openai20ChatCompletionChoicePB6ToJson14to__json_2eclo =
  (struct _M0TWRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceERPB4Json*)&_M0IP48clawteam8clawteam8internal6openai20ChatCompletionChoicePB6ToJson18to__json_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal6openai55____test__6275696c6465725f7762746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal6openai55____test__6275696c6465725f7762746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__1_2edyncall$closure.data;

struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson14to__json_2eclo =
  (struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json*)&_M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal6openai55____test__6275696c6465725f7762746573742e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__2_2edyncall$closure.data;

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
} _M0FP0131clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletion_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IP48clawteam8clawteam8internal6openai14ChatCompletionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP0131clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletion_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0131clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletion_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0146clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageToolCall_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP0146clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageToolCall_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0146clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageToolCall_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP0138clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessage_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IP48clawteam8clawteam8internal6openai21ChatCompletionMessagePB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP0138clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessage_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP0138clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessage_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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
} _M0FPB31ryu__to__string_2erecord_2f1141$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB31ryu__to__string_2erecord_2f1141 =
  &_M0FPB31ryu__to__string_2erecord_2f1141$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal6openai48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3786
) {
  return _M0FP48clawteam8clawteam8internal6openai49____test__6275696c6465725f7762746573742e6d6274__2();
}

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json* _M0L6_2aenvS3785,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L4selfS1381
) {
  return _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson8to__json(_M0L4selfS1381);
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3784
) {
  return _M0FP48clawteam8clawteam8internal6openai49____test__6275696c6465725f7762746573742e6d6274__1();
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai59____test__6275696c6465725f7762746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3783
) {
  return _M0FP48clawteam8clawteam8internal6openai49____test__6275696c6465725f7762746573742e6d6274__0();
}

void* _M0IP48clawteam8clawteam8internal6openai20ChatCompletionChoicePB6ToJson18to__json_2edyncall(
  struct _M0TWRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceERPB4Json* _M0L6_2aenvS3782,
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L4selfS1401
) {
  return _M0IP48clawteam8clawteam8internal6openai20ChatCompletionChoicePB6ToJson8to__json(_M0L4selfS1401);
}

int32_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1691,
  moonbit_string_t _M0L8filenameS1666,
  int32_t _M0L5indexS1669
) {
  struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1661* _closure_4385;
  struct _M0TWssbEu* _M0L14handle__resultS1661;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1670;
  void* _M0L11_2atry__errS1685;
  struct moonbit_result_0 _tmp_4387;
  int32_t _handle__error__result_4388;
  int32_t _M0L6_2atmpS3770;
  void* _M0L3errS1686;
  moonbit_string_t _M0L4nameS1688;
  struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1689;
  moonbit_string_t _M0L8_2afieldS3787;
  int32_t _M0L6_2acntS4221;
  moonbit_string_t _M0L7_2anameS1690;
  #line 526 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  moonbit_incref(_M0L8filenameS1666);
  _closure_4385
  = (struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1661*)moonbit_malloc(sizeof(struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1661));
  Moonbit_object_header(_closure_4385)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1661, $1) >> 2, 1, 0);
  _closure_4385->code
  = &_M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN14handle__resultS1661;
  _closure_4385->$0 = _M0L5indexS1669;
  _closure_4385->$1 = _M0L8filenameS1666;
  _M0L14handle__resultS1661 = (struct _M0TWssbEu*)_closure_4385;
  _M0L17error__to__stringS1670
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN17error__to__stringS1670$closure.data;
  moonbit_incref(_M0L12async__testsS1691);
  moonbit_incref(_M0L17error__to__stringS1670);
  moonbit_incref(_M0L8filenameS1666);
  moonbit_incref(_M0L14handle__resultS1661);
  #line 560 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _tmp_4387
  = _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__test(_M0L12async__testsS1691, _M0L8filenameS1666, _M0L5indexS1669, _M0L14handle__resultS1661, _M0L17error__to__stringS1670);
  if (_tmp_4387.tag) {
    int32_t const _M0L5_2aokS3779 = _tmp_4387.data.ok;
    _handle__error__result_4388 = _M0L5_2aokS3779;
  } else {
    void* const _M0L6_2aerrS3780 = _tmp_4387.data.err;
    moonbit_decref(_M0L12async__testsS1691);
    moonbit_decref(_M0L17error__to__stringS1670);
    moonbit_decref(_M0L8filenameS1666);
    _M0L11_2atry__errS1685 = _M0L6_2aerrS3780;
    goto join_1684;
  }
  if (_handle__error__result_4388) {
    moonbit_decref(_M0L12async__testsS1691);
    moonbit_decref(_M0L17error__to__stringS1670);
    moonbit_decref(_M0L8filenameS1666);
    _M0L6_2atmpS3770 = 1;
  } else {
    struct moonbit_result_0 _tmp_4389;
    int32_t _handle__error__result_4390;
    moonbit_incref(_M0L12async__testsS1691);
    moonbit_incref(_M0L17error__to__stringS1670);
    moonbit_incref(_M0L8filenameS1666);
    moonbit_incref(_M0L14handle__resultS1661);
    #line 563 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
    _tmp_4389
    = _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1691, _M0L8filenameS1666, _M0L5indexS1669, _M0L14handle__resultS1661, _M0L17error__to__stringS1670);
    if (_tmp_4389.tag) {
      int32_t const _M0L5_2aokS3777 = _tmp_4389.data.ok;
      _handle__error__result_4390 = _M0L5_2aokS3777;
    } else {
      void* const _M0L6_2aerrS3778 = _tmp_4389.data.err;
      moonbit_decref(_M0L12async__testsS1691);
      moonbit_decref(_M0L17error__to__stringS1670);
      moonbit_decref(_M0L8filenameS1666);
      _M0L11_2atry__errS1685 = _M0L6_2aerrS3778;
      goto join_1684;
    }
    if (_handle__error__result_4390) {
      moonbit_decref(_M0L12async__testsS1691);
      moonbit_decref(_M0L17error__to__stringS1670);
      moonbit_decref(_M0L8filenameS1666);
      _M0L6_2atmpS3770 = 1;
    } else {
      struct moonbit_result_0 _tmp_4391;
      int32_t _handle__error__result_4392;
      moonbit_incref(_M0L12async__testsS1691);
      moonbit_incref(_M0L17error__to__stringS1670);
      moonbit_incref(_M0L8filenameS1666);
      moonbit_incref(_M0L14handle__resultS1661);
      #line 566 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _tmp_4391
      = _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1691, _M0L8filenameS1666, _M0L5indexS1669, _M0L14handle__resultS1661, _M0L17error__to__stringS1670);
      if (_tmp_4391.tag) {
        int32_t const _M0L5_2aokS3775 = _tmp_4391.data.ok;
        _handle__error__result_4392 = _M0L5_2aokS3775;
      } else {
        void* const _M0L6_2aerrS3776 = _tmp_4391.data.err;
        moonbit_decref(_M0L12async__testsS1691);
        moonbit_decref(_M0L17error__to__stringS1670);
        moonbit_decref(_M0L8filenameS1666);
        _M0L11_2atry__errS1685 = _M0L6_2aerrS3776;
        goto join_1684;
      }
      if (_handle__error__result_4392) {
        moonbit_decref(_M0L12async__testsS1691);
        moonbit_decref(_M0L17error__to__stringS1670);
        moonbit_decref(_M0L8filenameS1666);
        _M0L6_2atmpS3770 = 1;
      } else {
        struct moonbit_result_0 _tmp_4393;
        int32_t _handle__error__result_4394;
        moonbit_incref(_M0L12async__testsS1691);
        moonbit_incref(_M0L17error__to__stringS1670);
        moonbit_incref(_M0L8filenameS1666);
        moonbit_incref(_M0L14handle__resultS1661);
        #line 569 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        _tmp_4393
        = _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1691, _M0L8filenameS1666, _M0L5indexS1669, _M0L14handle__resultS1661, _M0L17error__to__stringS1670);
        if (_tmp_4393.tag) {
          int32_t const _M0L5_2aokS3773 = _tmp_4393.data.ok;
          _handle__error__result_4394 = _M0L5_2aokS3773;
        } else {
          void* const _M0L6_2aerrS3774 = _tmp_4393.data.err;
          moonbit_decref(_M0L12async__testsS1691);
          moonbit_decref(_M0L17error__to__stringS1670);
          moonbit_decref(_M0L8filenameS1666);
          _M0L11_2atry__errS1685 = _M0L6_2aerrS3774;
          goto join_1684;
        }
        if (_handle__error__result_4394) {
          moonbit_decref(_M0L12async__testsS1691);
          moonbit_decref(_M0L17error__to__stringS1670);
          moonbit_decref(_M0L8filenameS1666);
          _M0L6_2atmpS3770 = 1;
        } else {
          struct moonbit_result_0 _tmp_4395;
          moonbit_incref(_M0L14handle__resultS1661);
          #line 572 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
          _tmp_4395
          = _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1691, _M0L8filenameS1666, _M0L5indexS1669, _M0L14handle__resultS1661, _M0L17error__to__stringS1670);
          if (_tmp_4395.tag) {
            int32_t const _M0L5_2aokS3771 = _tmp_4395.data.ok;
            _M0L6_2atmpS3770 = _M0L5_2aokS3771;
          } else {
            void* const _M0L6_2aerrS3772 = _tmp_4395.data.err;
            _M0L11_2atry__errS1685 = _M0L6_2aerrS3772;
            goto join_1684;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3770) {
    void* _M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3781 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3781)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3781)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1685
    = _M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3781;
    goto join_1684;
  } else {
    moonbit_decref(_M0L14handle__resultS1661);
  }
  goto joinlet_4386;
  join_1684:;
  _M0L3errS1686 = _M0L11_2atry__errS1685;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1689
  = (struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1686;
  _M0L8_2afieldS3787 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1689->$0;
  _M0L6_2acntS4221
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1689)->rc;
  if (_M0L6_2acntS4221 > 1) {
    int32_t _M0L11_2anew__cntS4222 = _M0L6_2acntS4221 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1689)->rc
    = _M0L11_2anew__cntS4222;
    moonbit_incref(_M0L8_2afieldS3787);
  } else if (_M0L6_2acntS4221 == 1) {
    #line 579 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1689);
  }
  _M0L7_2anameS1690 = _M0L8_2afieldS3787;
  _M0L4nameS1688 = _M0L7_2anameS1690;
  goto join_1687;
  goto joinlet_4396;
  join_1687:;
  #line 580 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN14handle__resultS1661(_M0L14handle__resultS1661, _M0L4nameS1688, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_4396:;
  joinlet_4386:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN17error__to__stringS1670(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3769,
  void* _M0L3errS1671
) {
  void* _M0L1eS1673;
  moonbit_string_t _M0L1eS1675;
  #line 549 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L6_2aenvS3769);
  switch (Moonbit_object_tag(_M0L3errS1671)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1676 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1671;
      moonbit_string_t _M0L8_2afieldS3788 = _M0L10_2aFailureS1676->$0;
      int32_t _M0L6_2acntS4223 =
        Moonbit_object_header(_M0L10_2aFailureS1676)->rc;
      moonbit_string_t _M0L4_2aeS1677;
      if (_M0L6_2acntS4223 > 1) {
        int32_t _M0L11_2anew__cntS4224 = _M0L6_2acntS4223 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1676)->rc
        = _M0L11_2anew__cntS4224;
        moonbit_incref(_M0L8_2afieldS3788);
      } else if (_M0L6_2acntS4223 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1676);
      }
      _M0L4_2aeS1677 = _M0L8_2afieldS3788;
      _M0L1eS1675 = _M0L4_2aeS1677;
      goto join_1674;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1678 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1671;
      moonbit_string_t _M0L8_2afieldS3789 = _M0L15_2aInspectErrorS1678->$0;
      int32_t _M0L6_2acntS4225 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1678)->rc;
      moonbit_string_t _M0L4_2aeS1679;
      if (_M0L6_2acntS4225 > 1) {
        int32_t _M0L11_2anew__cntS4226 = _M0L6_2acntS4225 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1678)->rc
        = _M0L11_2anew__cntS4226;
        moonbit_incref(_M0L8_2afieldS3789);
      } else if (_M0L6_2acntS4225 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1678);
      }
      _M0L4_2aeS1679 = _M0L8_2afieldS3789;
      _M0L1eS1675 = _M0L4_2aeS1679;
      goto join_1674;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1680 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1671;
      moonbit_string_t _M0L8_2afieldS3790 = _M0L16_2aSnapshotErrorS1680->$0;
      int32_t _M0L6_2acntS4227 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1680)->rc;
      moonbit_string_t _M0L4_2aeS1681;
      if (_M0L6_2acntS4227 > 1) {
        int32_t _M0L11_2anew__cntS4228 = _M0L6_2acntS4227 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1680)->rc
        = _M0L11_2anew__cntS4228;
        moonbit_incref(_M0L8_2afieldS3790);
      } else if (_M0L6_2acntS4227 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1680);
      }
      _M0L4_2aeS1681 = _M0L8_2afieldS3790;
      _M0L1eS1675 = _M0L4_2aeS1681;
      goto join_1674;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1682 =
        (struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1671;
      moonbit_string_t _M0L8_2afieldS3791 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1682->$0;
      int32_t _M0L6_2acntS4229 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1682)->rc;
      moonbit_string_t _M0L4_2aeS1683;
      if (_M0L6_2acntS4229 > 1) {
        int32_t _M0L11_2anew__cntS4230 = _M0L6_2acntS4229 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1682)->rc
        = _M0L11_2anew__cntS4230;
        moonbit_incref(_M0L8_2afieldS3791);
      } else if (_M0L6_2acntS4229 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1682);
      }
      _M0L4_2aeS1683 = _M0L8_2afieldS3791;
      _M0L1eS1675 = _M0L4_2aeS1683;
      goto join_1674;
      break;
    }
    default: {
      _M0L1eS1673 = _M0L3errS1671;
      goto join_1672;
      break;
    }
  }
  join_1674:;
  return _M0L1eS1675;
  join_1672:;
  #line 555 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1673);
}

int32_t _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__executeN14handle__resultS1661(
  struct _M0TWssbEu* _M0L6_2aenvS3755,
  moonbit_string_t _M0L8testnameS1662,
  moonbit_string_t _M0L7messageS1663,
  int32_t _M0L7skippedS1664
) {
  struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1661* _M0L14_2acasted__envS3756;
  moonbit_string_t _M0L8_2afieldS3801;
  moonbit_string_t _M0L8filenameS1666;
  int32_t _M0L8_2afieldS3800;
  int32_t _M0L6_2acntS4231;
  int32_t _M0L5indexS1669;
  int32_t _if__result_4399;
  moonbit_string_t _M0L10file__nameS1665;
  moonbit_string_t _M0L10test__nameS1667;
  moonbit_string_t _M0L7messageS1668;
  moonbit_string_t _M0L6_2atmpS3768;
  moonbit_string_t _M0L6_2atmpS3799;
  moonbit_string_t _M0L6_2atmpS3767;
  moonbit_string_t _M0L6_2atmpS3798;
  moonbit_string_t _M0L6_2atmpS3765;
  moonbit_string_t _M0L6_2atmpS3766;
  moonbit_string_t _M0L6_2atmpS3797;
  moonbit_string_t _M0L6_2atmpS3764;
  moonbit_string_t _M0L6_2atmpS3796;
  moonbit_string_t _M0L6_2atmpS3762;
  moonbit_string_t _M0L6_2atmpS3763;
  moonbit_string_t _M0L6_2atmpS3795;
  moonbit_string_t _M0L6_2atmpS3761;
  moonbit_string_t _M0L6_2atmpS3794;
  moonbit_string_t _M0L6_2atmpS3759;
  moonbit_string_t _M0L6_2atmpS3760;
  moonbit_string_t _M0L6_2atmpS3793;
  moonbit_string_t _M0L6_2atmpS3758;
  moonbit_string_t _M0L6_2atmpS3792;
  moonbit_string_t _M0L6_2atmpS3757;
  #line 533 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS3756
  = (struct _M0R113_24clawteam_2fclawteam_2finternal_2fopenai_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1661*)_M0L6_2aenvS3755;
  _M0L8_2afieldS3801 = _M0L14_2acasted__envS3756->$1;
  _M0L8filenameS1666 = _M0L8_2afieldS3801;
  _M0L8_2afieldS3800 = _M0L14_2acasted__envS3756->$0;
  _M0L6_2acntS4231 = Moonbit_object_header(_M0L14_2acasted__envS3756)->rc;
  if (_M0L6_2acntS4231 > 1) {
    int32_t _M0L11_2anew__cntS4232 = _M0L6_2acntS4231 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3756)->rc
    = _M0L11_2anew__cntS4232;
    moonbit_incref(_M0L8filenameS1666);
  } else if (_M0L6_2acntS4231 == 1) {
    #line 533 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3756);
  }
  _M0L5indexS1669 = _M0L8_2afieldS3800;
  if (!_M0L7skippedS1664) {
    _if__result_4399 = 1;
  } else {
    _if__result_4399 = 0;
  }
  if (_if__result_4399) {
    
  }
  #line 539 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L10file__nameS1665 = _M0MPC16string6String6escape(_M0L8filenameS1666);
  #line 540 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L10test__nameS1667 = _M0MPC16string6String6escape(_M0L8testnameS1662);
  #line 541 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L7messageS1668 = _M0MPC16string6String6escape(_M0L7messageS1663);
  #line 542 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 544 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3768
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1665);
  #line 543 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3799
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3768);
  moonbit_decref(_M0L6_2atmpS3768);
  _M0L6_2atmpS3767 = _M0L6_2atmpS3799;
  #line 543 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3798
  = moonbit_add_string(_M0L6_2atmpS3767, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3767);
  _M0L6_2atmpS3765 = _M0L6_2atmpS3798;
  #line 544 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3766
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1669);
  #line 543 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3797 = moonbit_add_string(_M0L6_2atmpS3765, _M0L6_2atmpS3766);
  moonbit_decref(_M0L6_2atmpS3765);
  moonbit_decref(_M0L6_2atmpS3766);
  _M0L6_2atmpS3764 = _M0L6_2atmpS3797;
  #line 543 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3796
  = moonbit_add_string(_M0L6_2atmpS3764, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3764);
  _M0L6_2atmpS3762 = _M0L6_2atmpS3796;
  #line 544 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3763
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1667);
  #line 543 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3795 = moonbit_add_string(_M0L6_2atmpS3762, _M0L6_2atmpS3763);
  moonbit_decref(_M0L6_2atmpS3762);
  moonbit_decref(_M0L6_2atmpS3763);
  _M0L6_2atmpS3761 = _M0L6_2atmpS3795;
  #line 543 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3794
  = moonbit_add_string(_M0L6_2atmpS3761, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3761);
  _M0L6_2atmpS3759 = _M0L6_2atmpS3794;
  #line 544 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3760
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1668);
  #line 543 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3793 = moonbit_add_string(_M0L6_2atmpS3759, _M0L6_2atmpS3760);
  moonbit_decref(_M0L6_2atmpS3759);
  moonbit_decref(_M0L6_2atmpS3760);
  _M0L6_2atmpS3758 = _M0L6_2atmpS3793;
  #line 543 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3792
  = moonbit_add_string(_M0L6_2atmpS3758, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3758);
  _M0L6_2atmpS3757 = _M0L6_2atmpS3792;
  #line 543 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3757);
  #line 546 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1660,
  moonbit_string_t _M0L8filenameS1657,
  int32_t _M0L5indexS1651,
  struct _M0TWssbEu* _M0L14handle__resultS1647,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1649
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1627;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1656;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1629;
  moonbit_string_t* _M0L5attrsS1630;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1650;
  moonbit_string_t _M0L4nameS1633;
  moonbit_string_t _M0L4nameS1631;
  int32_t _M0L6_2atmpS3754;
  struct _M0TWEOs* _M0L5_2aitS1635;
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3751__l427__* _closure_4408;
  struct _M0TWEOc* _M0L6_2atmpS3745;
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3747__l428__* _closure_4409;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3746;
  struct moonbit_result_0 _result_4410;
  #line 407 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1660);
  moonbit_incref(_M0FP48clawteam8clawteam8internal6openai48moonbit__test__driver__internal__no__args__tests);
  #line 414 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS1656
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal6openai48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1657);
  if (_M0L7_2abindS1656 == 0) {
    struct moonbit_result_0 _result_4401;
    if (_M0L7_2abindS1656) {
      moonbit_decref(_M0L7_2abindS1656);
    }
    moonbit_decref(_M0L17error__to__stringS1649);
    moonbit_decref(_M0L14handle__resultS1647);
    _result_4401.tag = 1;
    _result_4401.data.ok = 0;
    return _result_4401;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1658 =
      _M0L7_2abindS1656;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1659 =
      _M0L7_2aSomeS1658;
    _M0L10index__mapS1627 = _M0L13_2aindex__mapS1659;
    goto join_1626;
  }
  join_1626:;
  #line 416 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS1650
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1627, _M0L5indexS1651);
  if (_M0L7_2abindS1650 == 0) {
    struct moonbit_result_0 _result_4403;
    if (_M0L7_2abindS1650) {
      moonbit_decref(_M0L7_2abindS1650);
    }
    moonbit_decref(_M0L17error__to__stringS1649);
    moonbit_decref(_M0L14handle__resultS1647);
    _result_4403.tag = 1;
    _result_4403.data.ok = 0;
    return _result_4403;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1652 =
      _M0L7_2abindS1650;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1653 = _M0L7_2aSomeS1652;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3805 = _M0L4_2axS1653->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1654 = _M0L8_2afieldS3805;
    moonbit_string_t* _M0L8_2afieldS3804 = _M0L4_2axS1653->$1;
    int32_t _M0L6_2acntS4233 = Moonbit_object_header(_M0L4_2axS1653)->rc;
    moonbit_string_t* _M0L8_2aattrsS1655;
    if (_M0L6_2acntS4233 > 1) {
      int32_t _M0L11_2anew__cntS4234 = _M0L6_2acntS4233 - 1;
      Moonbit_object_header(_M0L4_2axS1653)->rc = _M0L11_2anew__cntS4234;
      moonbit_incref(_M0L8_2afieldS3804);
      moonbit_incref(_M0L4_2afS1654);
    } else if (_M0L6_2acntS4233 == 1) {
      #line 414 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      moonbit_free(_M0L4_2axS1653);
    }
    _M0L8_2aattrsS1655 = _M0L8_2afieldS3804;
    _M0L1fS1629 = _M0L4_2afS1654;
    _M0L5attrsS1630 = _M0L8_2aattrsS1655;
    goto join_1628;
  }
  join_1628:;
  _M0L6_2atmpS3754 = Moonbit_array_length(_M0L5attrsS1630);
  if (_M0L6_2atmpS3754 >= 1) {
    moonbit_string_t _M0L6_2atmpS3803 = (moonbit_string_t)_M0L5attrsS1630[0];
    moonbit_string_t _M0L7_2anameS1634 = _M0L6_2atmpS3803;
    moonbit_incref(_M0L7_2anameS1634);
    _M0L4nameS1633 = _M0L7_2anameS1634;
    goto join_1632;
  } else {
    _M0L4nameS1631 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_4404;
  join_1632:;
  _M0L4nameS1631 = _M0L4nameS1633;
  joinlet_4404:;
  #line 417 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L5_2aitS1635 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1630);
  while (1) {
    moonbit_string_t _M0L4attrS1637;
    moonbit_string_t _M0L7_2abindS1644;
    int32_t _M0L6_2atmpS3738;
    int64_t _M0L6_2atmpS3737;
    moonbit_incref(_M0L5_2aitS1635);
    #line 419 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
    _M0L7_2abindS1644 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1635);
    if (_M0L7_2abindS1644 == 0) {
      if (_M0L7_2abindS1644) {
        moonbit_decref(_M0L7_2abindS1644);
      }
      moonbit_decref(_M0L5_2aitS1635);
    } else {
      moonbit_string_t _M0L7_2aSomeS1645 = _M0L7_2abindS1644;
      moonbit_string_t _M0L7_2aattrS1646 = _M0L7_2aSomeS1645;
      _M0L4attrS1637 = _M0L7_2aattrS1646;
      goto join_1636;
    }
    goto joinlet_4406;
    join_1636:;
    _M0L6_2atmpS3738 = Moonbit_array_length(_M0L4attrS1637);
    _M0L6_2atmpS3737 = (int64_t)_M0L6_2atmpS3738;
    moonbit_incref(_M0L4attrS1637);
    #line 420 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1637, 5, 0, _M0L6_2atmpS3737)
    ) {
      int32_t _M0L6_2atmpS3744 = _M0L4attrS1637[0];
      int32_t _M0L4_2axS1638 = _M0L6_2atmpS3744;
      if (_M0L4_2axS1638 == 112) {
        int32_t _M0L6_2atmpS3743 = _M0L4attrS1637[1];
        int32_t _M0L4_2axS1639 = _M0L6_2atmpS3743;
        if (_M0L4_2axS1639 == 97) {
          int32_t _M0L6_2atmpS3742 = _M0L4attrS1637[2];
          int32_t _M0L4_2axS1640 = _M0L6_2atmpS3742;
          if (_M0L4_2axS1640 == 110) {
            int32_t _M0L6_2atmpS3741 = _M0L4attrS1637[3];
            int32_t _M0L4_2axS1641 = _M0L6_2atmpS3741;
            if (_M0L4_2axS1641 == 105) {
              int32_t _M0L6_2atmpS3802 = _M0L4attrS1637[4];
              int32_t _M0L6_2atmpS3740;
              int32_t _M0L4_2axS1642;
              moonbit_decref(_M0L4attrS1637);
              _M0L6_2atmpS3740 = _M0L6_2atmpS3802;
              _M0L4_2axS1642 = _M0L6_2atmpS3740;
              if (_M0L4_2axS1642 == 99) {
                void* _M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3739;
                struct moonbit_result_0 _result_4407;
                moonbit_decref(_M0L17error__to__stringS1649);
                moonbit_decref(_M0L14handle__resultS1647);
                moonbit_decref(_M0L5_2aitS1635);
                moonbit_decref(_M0L1fS1629);
                _M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3739
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3739)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3739)->$0
                = _M0L4nameS1631;
                _result_4407.tag = 0;
                _result_4407.data.err
                = _M0L111clawteam_2fclawteam_2finternal_2fopenai_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3739;
                return _result_4407;
              }
            } else {
              moonbit_decref(_M0L4attrS1637);
            }
          } else {
            moonbit_decref(_M0L4attrS1637);
          }
        } else {
          moonbit_decref(_M0L4attrS1637);
        }
      } else {
        moonbit_decref(_M0L4attrS1637);
      }
    } else {
      moonbit_decref(_M0L4attrS1637);
    }
    continue;
    joinlet_4406:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1647);
  moonbit_incref(_M0L4nameS1631);
  _closure_4408
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3751__l427__*)moonbit_malloc(sizeof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3751__l427__));
  Moonbit_object_header(_closure_4408)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3751__l427__, $0) >> 2, 2, 0);
  _closure_4408->code
  = &_M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3751l427;
  _closure_4408->$0 = _M0L14handle__resultS1647;
  _closure_4408->$1 = _M0L4nameS1631;
  _M0L6_2atmpS3745 = (struct _M0TWEOc*)_closure_4408;
  _closure_4409
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3747__l428__*)moonbit_malloc(sizeof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3747__l428__));
  Moonbit_object_header(_closure_4409)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3747__l428__, $0) >> 2, 3, 0);
  _closure_4409->code
  = &_M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3747l428;
  _closure_4409->$0 = _M0L17error__to__stringS1649;
  _closure_4409->$1 = _M0L14handle__resultS1647;
  _closure_4409->$2 = _M0L4nameS1631;
  _M0L6_2atmpS3746 = (struct _M0TWRPC15error5ErrorEu*)_closure_4409;
  #line 425 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0FP48clawteam8clawteam8internal6openai45moonbit__test__driver__internal__catch__error(_M0L1fS1629, _M0L6_2atmpS3745, _M0L6_2atmpS3746);
  _result_4410.tag = 1;
  _result_4410.data.ok = 1;
  return _result_4410;
}

int32_t _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3751l427(
  struct _M0TWEOc* _M0L6_2aenvS3752
) {
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3751__l427__* _M0L14_2acasted__envS3753;
  moonbit_string_t _M0L8_2afieldS3807;
  moonbit_string_t _M0L4nameS1631;
  struct _M0TWssbEu* _M0L8_2afieldS3806;
  int32_t _M0L6_2acntS4235;
  struct _M0TWssbEu* _M0L14handle__resultS1647;
  #line 427 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS3753
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3751__l427__*)_M0L6_2aenvS3752;
  _M0L8_2afieldS3807 = _M0L14_2acasted__envS3753->$1;
  _M0L4nameS1631 = _M0L8_2afieldS3807;
  _M0L8_2afieldS3806 = _M0L14_2acasted__envS3753->$0;
  _M0L6_2acntS4235 = Moonbit_object_header(_M0L14_2acasted__envS3753)->rc;
  if (_M0L6_2acntS4235 > 1) {
    int32_t _M0L11_2anew__cntS4236 = _M0L6_2acntS4235 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3753)->rc
    = _M0L11_2anew__cntS4236;
    moonbit_incref(_M0L4nameS1631);
    moonbit_incref(_M0L8_2afieldS3806);
  } else if (_M0L6_2acntS4235 == 1) {
    #line 427 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3753);
  }
  _M0L14handle__resultS1647 = _M0L8_2afieldS3806;
  #line 427 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L14handle__resultS1647->code(_M0L14handle__resultS1647, _M0L4nameS1631, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal6openai41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testC3747l428(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3748,
  void* _M0L3errS1648
) {
  struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3747__l428__* _M0L14_2acasted__envS3749;
  moonbit_string_t _M0L8_2afieldS3810;
  moonbit_string_t _M0L4nameS1631;
  struct _M0TWssbEu* _M0L8_2afieldS3809;
  struct _M0TWssbEu* _M0L14handle__resultS1647;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3808;
  int32_t _M0L6_2acntS4237;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1649;
  moonbit_string_t _M0L6_2atmpS3750;
  #line 428 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS3749
  = (struct _M0R195_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fopenai_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3747__l428__*)_M0L6_2aenvS3748;
  _M0L8_2afieldS3810 = _M0L14_2acasted__envS3749->$2;
  _M0L4nameS1631 = _M0L8_2afieldS3810;
  _M0L8_2afieldS3809 = _M0L14_2acasted__envS3749->$1;
  _M0L14handle__resultS1647 = _M0L8_2afieldS3809;
  _M0L8_2afieldS3808 = _M0L14_2acasted__envS3749->$0;
  _M0L6_2acntS4237 = Moonbit_object_header(_M0L14_2acasted__envS3749)->rc;
  if (_M0L6_2acntS4237 > 1) {
    int32_t _M0L11_2anew__cntS4238 = _M0L6_2acntS4237 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3749)->rc
    = _M0L11_2anew__cntS4238;
    moonbit_incref(_M0L4nameS1631);
    moonbit_incref(_M0L14handle__resultS1647);
    moonbit_incref(_M0L8_2afieldS3808);
  } else if (_M0L6_2acntS4237 == 1) {
    #line 428 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3749);
  }
  _M0L17error__to__stringS1649 = _M0L8_2afieldS3808;
  #line 428 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3750
  = _M0L17error__to__stringS1649->code(_M0L17error__to__stringS1649, _M0L3errS1648);
  #line 428 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L14handle__resultS1647->code(_M0L14handle__resultS1647, _M0L4nameS1631, _M0L6_2atmpS3750, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal6openai45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1622,
  struct _M0TWEOc* _M0L6on__okS1623,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1620
) {
  void* _M0L11_2atry__errS1618;
  struct moonbit_result_0 _tmp_4412;
  void* _M0L3errS1619;
  #line 375 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _tmp_4412 = _M0L1fS1622->code(_M0L1fS1622);
  if (_tmp_4412.tag) {
    int32_t const _M0L5_2aokS3735 = _tmp_4412.data.ok;
    moonbit_decref(_M0L7on__errS1620);
  } else {
    void* const _M0L6_2aerrS3736 = _tmp_4412.data.err;
    moonbit_decref(_M0L6on__okS1623);
    _M0L11_2atry__errS1618 = _M0L6_2aerrS3736;
    goto join_1617;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6on__okS1623->code(_M0L6on__okS1623);
  goto joinlet_4411;
  join_1617:;
  _M0L3errS1619 = _M0L11_2atry__errS1618;
  #line 383 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L7on__errS1620->code(_M0L7on__errS1620, _M0L3errS1619);
  joinlet_4411:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1577;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1583;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1590;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1595;
  struct _M0TUsiE** _M0L6_2atmpS3734;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1602;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1603;
  moonbit_string_t _M0L6_2atmpS3733;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1604;
  int32_t _M0L7_2abindS1605;
  int32_t _M0L2__S1606;
  #line 193 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1577 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1583
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1590
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1583;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1595 = 0;
  _M0L6_2atmpS3734 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1602
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1602)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1602->$0 = _M0L6_2atmpS3734;
  _M0L16file__and__indexS1602->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L9cli__argsS1603
  = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1590(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1590);
  #line 284 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3733 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1603, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L10test__argsS1604
  = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1595(_M0L51moonbit__test__driver__internal__split__mbt__stringS1595, _M0L6_2atmpS3733, 47);
  _M0L7_2abindS1605 = _M0L10test__argsS1604->$1;
  _M0L2__S1606 = 0;
  while (1) {
    if (_M0L2__S1606 < _M0L7_2abindS1605) {
      moonbit_string_t* _M0L8_2afieldS3812 = _M0L10test__argsS1604->$0;
      moonbit_string_t* _M0L3bufS3732 = _M0L8_2afieldS3812;
      moonbit_string_t _M0L6_2atmpS3811 =
        (moonbit_string_t)_M0L3bufS3732[_M0L2__S1606];
      moonbit_string_t _M0L3argS1607 = _M0L6_2atmpS3811;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1608;
      moonbit_string_t _M0L4fileS1609;
      moonbit_string_t _M0L5rangeS1610;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1611;
      moonbit_string_t _M0L6_2atmpS3730;
      int32_t _M0L5startS1612;
      moonbit_string_t _M0L6_2atmpS3729;
      int32_t _M0L3endS1613;
      int32_t _M0L1iS1614;
      int32_t _M0L6_2atmpS3731;
      moonbit_incref(_M0L3argS1607);
      #line 288 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _M0L16file__and__rangeS1608
      = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1595(_M0L51moonbit__test__driver__internal__split__mbt__stringS1595, _M0L3argS1607, 58);
      moonbit_incref(_M0L16file__and__rangeS1608);
      #line 289 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _M0L4fileS1609
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1608, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _M0L5rangeS1610
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1608, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _M0L15start__and__endS1611
      = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1595(_M0L51moonbit__test__driver__internal__split__mbt__stringS1595, _M0L5rangeS1610, 45);
      moonbit_incref(_M0L15start__and__endS1611);
      #line 294 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS3730
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1611, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _M0L5startS1612
      = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1577(_M0L45moonbit__test__driver__internal__parse__int__S1577, _M0L6_2atmpS3730);
      #line 295 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS3729
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1611, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _M0L3endS1613
      = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1577(_M0L45moonbit__test__driver__internal__parse__int__S1577, _M0L6_2atmpS3729);
      _M0L1iS1614 = _M0L5startS1612;
      while (1) {
        if (_M0L1iS1614 < _M0L3endS1613) {
          struct _M0TUsiE* _M0L8_2atupleS3727;
          int32_t _M0L6_2atmpS3728;
          moonbit_incref(_M0L4fileS1609);
          _M0L8_2atupleS3727
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3727)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3727->$0 = _M0L4fileS1609;
          _M0L8_2atupleS3727->$1 = _M0L1iS1614;
          moonbit_incref(_M0L16file__and__indexS1602);
          #line 297 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1602, _M0L8_2atupleS3727);
          _M0L6_2atmpS3728 = _M0L1iS1614 + 1;
          _M0L1iS1614 = _M0L6_2atmpS3728;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1609);
        }
        break;
      }
      _M0L6_2atmpS3731 = _M0L2__S1606 + 1;
      _M0L2__S1606 = _M0L6_2atmpS3731;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1604);
    }
    break;
  }
  return _M0L16file__and__indexS1602;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1595(
  int32_t _M0L6_2aenvS3708,
  moonbit_string_t _M0L1sS1596,
  int32_t _M0L3sepS1597
) {
  moonbit_string_t* _M0L6_2atmpS3726;
  struct _M0TPB5ArrayGsE* _M0L3resS1598;
  struct _M0TPC13ref3RefGiE* _M0L1iS1599;
  struct _M0TPC13ref3RefGiE* _M0L5startS1600;
  int32_t _M0L3valS3721;
  int32_t _M0L6_2atmpS3722;
  #line 261 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS3726 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1598
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1598)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1598->$0 = _M0L6_2atmpS3726;
  _M0L3resS1598->$1 = 0;
  _M0L1iS1599
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1599)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1599->$0 = 0;
  _M0L5startS1600
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1600)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1600->$0 = 0;
  while (1) {
    int32_t _M0L3valS3709 = _M0L1iS1599->$0;
    int32_t _M0L6_2atmpS3710 = Moonbit_array_length(_M0L1sS1596);
    if (_M0L3valS3709 < _M0L6_2atmpS3710) {
      int32_t _M0L3valS3713 = _M0L1iS1599->$0;
      int32_t _M0L6_2atmpS3712;
      int32_t _M0L6_2atmpS3711;
      int32_t _M0L3valS3720;
      int32_t _M0L6_2atmpS3719;
      if (
        _M0L3valS3713 < 0
        || _M0L3valS3713 >= Moonbit_array_length(_M0L1sS1596)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3712 = _M0L1sS1596[_M0L3valS3713];
      _M0L6_2atmpS3711 = _M0L6_2atmpS3712;
      if (_M0L6_2atmpS3711 == _M0L3sepS1597) {
        int32_t _M0L3valS3715 = _M0L5startS1600->$0;
        int32_t _M0L3valS3716 = _M0L1iS1599->$0;
        moonbit_string_t _M0L6_2atmpS3714;
        int32_t _M0L3valS3718;
        int32_t _M0L6_2atmpS3717;
        moonbit_incref(_M0L1sS1596);
        #line 270 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        _M0L6_2atmpS3714
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1596, _M0L3valS3715, _M0L3valS3716);
        moonbit_incref(_M0L3resS1598);
        #line 270 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1598, _M0L6_2atmpS3714);
        _M0L3valS3718 = _M0L1iS1599->$0;
        _M0L6_2atmpS3717 = _M0L3valS3718 + 1;
        _M0L5startS1600->$0 = _M0L6_2atmpS3717;
      }
      _M0L3valS3720 = _M0L1iS1599->$0;
      _M0L6_2atmpS3719 = _M0L3valS3720 + 1;
      _M0L1iS1599->$0 = _M0L6_2atmpS3719;
      continue;
    } else {
      moonbit_decref(_M0L1iS1599);
    }
    break;
  }
  _M0L3valS3721 = _M0L5startS1600->$0;
  _M0L6_2atmpS3722 = Moonbit_array_length(_M0L1sS1596);
  if (_M0L3valS3721 < _M0L6_2atmpS3722) {
    int32_t _M0L8_2afieldS3813 = _M0L5startS1600->$0;
    int32_t _M0L3valS3724;
    int32_t _M0L6_2atmpS3725;
    moonbit_string_t _M0L6_2atmpS3723;
    moonbit_decref(_M0L5startS1600);
    _M0L3valS3724 = _M0L8_2afieldS3813;
    _M0L6_2atmpS3725 = Moonbit_array_length(_M0L1sS1596);
    #line 276 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
    _M0L6_2atmpS3723
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1596, _M0L3valS3724, _M0L6_2atmpS3725);
    moonbit_incref(_M0L3resS1598);
    #line 276 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1598, _M0L6_2atmpS3723);
  } else {
    moonbit_decref(_M0L5startS1600);
    moonbit_decref(_M0L1sS1596);
  }
  return _M0L3resS1598;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1590(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1583
) {
  moonbit_bytes_t* _M0L3tmpS1591;
  int32_t _M0L6_2atmpS3707;
  struct _M0TPB5ArrayGsE* _M0L3resS1592;
  int32_t _M0L1iS1593;
  #line 250 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L3tmpS1591
  = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3707 = Moonbit_array_length(_M0L3tmpS1591);
  #line 254 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L3resS1592 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3707);
  _M0L1iS1593 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3703 = Moonbit_array_length(_M0L3tmpS1591);
    if (_M0L1iS1593 < _M0L6_2atmpS3703) {
      moonbit_bytes_t _M0L6_2atmpS3814;
      moonbit_bytes_t _M0L6_2atmpS3705;
      moonbit_string_t _M0L6_2atmpS3704;
      int32_t _M0L6_2atmpS3706;
      if (
        _M0L1iS1593 < 0 || _M0L1iS1593 >= Moonbit_array_length(_M0L3tmpS1591)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3814 = (moonbit_bytes_t)_M0L3tmpS1591[_M0L1iS1593];
      _M0L6_2atmpS3705 = _M0L6_2atmpS3814;
      moonbit_incref(_M0L6_2atmpS3705);
      #line 256 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS3704
      = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1583(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1583, _M0L6_2atmpS3705);
      moonbit_incref(_M0L3resS1592);
      #line 256 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1592, _M0L6_2atmpS3704);
      _M0L6_2atmpS3706 = _M0L1iS1593 + 1;
      _M0L1iS1593 = _M0L6_2atmpS3706;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1591);
    }
    break;
  }
  return _M0L3resS1592;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1583(
  int32_t _M0L6_2aenvS3617,
  moonbit_bytes_t _M0L5bytesS1584
) {
  struct _M0TPB13StringBuilder* _M0L3resS1585;
  int32_t _M0L3lenS1586;
  struct _M0TPC13ref3RefGiE* _M0L1iS1587;
  #line 206 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L3resS1585 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1586 = Moonbit_array_length(_M0L5bytesS1584);
  _M0L1iS1587
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1587)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1587->$0 = 0;
  while (1) {
    int32_t _M0L3valS3618 = _M0L1iS1587->$0;
    if (_M0L3valS3618 < _M0L3lenS1586) {
      int32_t _M0L3valS3702 = _M0L1iS1587->$0;
      int32_t _M0L6_2atmpS3701;
      int32_t _M0L6_2atmpS3700;
      struct _M0TPC13ref3RefGiE* _M0L1cS1588;
      int32_t _M0L3valS3619;
      if (
        _M0L3valS3702 < 0
        || _M0L3valS3702 >= Moonbit_array_length(_M0L5bytesS1584)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3701 = _M0L5bytesS1584[_M0L3valS3702];
      _M0L6_2atmpS3700 = (int32_t)_M0L6_2atmpS3701;
      _M0L1cS1588
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1588)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1588->$0 = _M0L6_2atmpS3700;
      _M0L3valS3619 = _M0L1cS1588->$0;
      if (_M0L3valS3619 < 128) {
        int32_t _M0L8_2afieldS3815 = _M0L1cS1588->$0;
        int32_t _M0L3valS3621;
        int32_t _M0L6_2atmpS3620;
        int32_t _M0L3valS3623;
        int32_t _M0L6_2atmpS3622;
        moonbit_decref(_M0L1cS1588);
        _M0L3valS3621 = _M0L8_2afieldS3815;
        _M0L6_2atmpS3620 = _M0L3valS3621;
        moonbit_incref(_M0L3resS1585);
        #line 215 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1585, _M0L6_2atmpS3620);
        _M0L3valS3623 = _M0L1iS1587->$0;
        _M0L6_2atmpS3622 = _M0L3valS3623 + 1;
        _M0L1iS1587->$0 = _M0L6_2atmpS3622;
      } else {
        int32_t _M0L3valS3624 = _M0L1cS1588->$0;
        if (_M0L3valS3624 < 224) {
          int32_t _M0L3valS3626 = _M0L1iS1587->$0;
          int32_t _M0L6_2atmpS3625 = _M0L3valS3626 + 1;
          int32_t _M0L3valS3635;
          int32_t _M0L6_2atmpS3634;
          int32_t _M0L6_2atmpS3628;
          int32_t _M0L3valS3633;
          int32_t _M0L6_2atmpS3632;
          int32_t _M0L6_2atmpS3631;
          int32_t _M0L6_2atmpS3630;
          int32_t _M0L6_2atmpS3629;
          int32_t _M0L6_2atmpS3627;
          int32_t _M0L8_2afieldS3816;
          int32_t _M0L3valS3637;
          int32_t _M0L6_2atmpS3636;
          int32_t _M0L3valS3639;
          int32_t _M0L6_2atmpS3638;
          if (_M0L6_2atmpS3625 >= _M0L3lenS1586) {
            moonbit_decref(_M0L1cS1588);
            moonbit_decref(_M0L1iS1587);
            moonbit_decref(_M0L5bytesS1584);
            break;
          }
          _M0L3valS3635 = _M0L1cS1588->$0;
          _M0L6_2atmpS3634 = _M0L3valS3635 & 31;
          _M0L6_2atmpS3628 = _M0L6_2atmpS3634 << 6;
          _M0L3valS3633 = _M0L1iS1587->$0;
          _M0L6_2atmpS3632 = _M0L3valS3633 + 1;
          if (
            _M0L6_2atmpS3632 < 0
            || _M0L6_2atmpS3632 >= Moonbit_array_length(_M0L5bytesS1584)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3631 = _M0L5bytesS1584[_M0L6_2atmpS3632];
          _M0L6_2atmpS3630 = (int32_t)_M0L6_2atmpS3631;
          _M0L6_2atmpS3629 = _M0L6_2atmpS3630 & 63;
          _M0L6_2atmpS3627 = _M0L6_2atmpS3628 | _M0L6_2atmpS3629;
          _M0L1cS1588->$0 = _M0L6_2atmpS3627;
          _M0L8_2afieldS3816 = _M0L1cS1588->$0;
          moonbit_decref(_M0L1cS1588);
          _M0L3valS3637 = _M0L8_2afieldS3816;
          _M0L6_2atmpS3636 = _M0L3valS3637;
          moonbit_incref(_M0L3resS1585);
          #line 222 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1585, _M0L6_2atmpS3636);
          _M0L3valS3639 = _M0L1iS1587->$0;
          _M0L6_2atmpS3638 = _M0L3valS3639 + 2;
          _M0L1iS1587->$0 = _M0L6_2atmpS3638;
        } else {
          int32_t _M0L3valS3640 = _M0L1cS1588->$0;
          if (_M0L3valS3640 < 240) {
            int32_t _M0L3valS3642 = _M0L1iS1587->$0;
            int32_t _M0L6_2atmpS3641 = _M0L3valS3642 + 2;
            int32_t _M0L3valS3658;
            int32_t _M0L6_2atmpS3657;
            int32_t _M0L6_2atmpS3650;
            int32_t _M0L3valS3656;
            int32_t _M0L6_2atmpS3655;
            int32_t _M0L6_2atmpS3654;
            int32_t _M0L6_2atmpS3653;
            int32_t _M0L6_2atmpS3652;
            int32_t _M0L6_2atmpS3651;
            int32_t _M0L6_2atmpS3644;
            int32_t _M0L3valS3649;
            int32_t _M0L6_2atmpS3648;
            int32_t _M0L6_2atmpS3647;
            int32_t _M0L6_2atmpS3646;
            int32_t _M0L6_2atmpS3645;
            int32_t _M0L6_2atmpS3643;
            int32_t _M0L8_2afieldS3817;
            int32_t _M0L3valS3660;
            int32_t _M0L6_2atmpS3659;
            int32_t _M0L3valS3662;
            int32_t _M0L6_2atmpS3661;
            if (_M0L6_2atmpS3641 >= _M0L3lenS1586) {
              moonbit_decref(_M0L1cS1588);
              moonbit_decref(_M0L1iS1587);
              moonbit_decref(_M0L5bytesS1584);
              break;
            }
            _M0L3valS3658 = _M0L1cS1588->$0;
            _M0L6_2atmpS3657 = _M0L3valS3658 & 15;
            _M0L6_2atmpS3650 = _M0L6_2atmpS3657 << 12;
            _M0L3valS3656 = _M0L1iS1587->$0;
            _M0L6_2atmpS3655 = _M0L3valS3656 + 1;
            if (
              _M0L6_2atmpS3655 < 0
              || _M0L6_2atmpS3655 >= Moonbit_array_length(_M0L5bytesS1584)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3654 = _M0L5bytesS1584[_M0L6_2atmpS3655];
            _M0L6_2atmpS3653 = (int32_t)_M0L6_2atmpS3654;
            _M0L6_2atmpS3652 = _M0L6_2atmpS3653 & 63;
            _M0L6_2atmpS3651 = _M0L6_2atmpS3652 << 6;
            _M0L6_2atmpS3644 = _M0L6_2atmpS3650 | _M0L6_2atmpS3651;
            _M0L3valS3649 = _M0L1iS1587->$0;
            _M0L6_2atmpS3648 = _M0L3valS3649 + 2;
            if (
              _M0L6_2atmpS3648 < 0
              || _M0L6_2atmpS3648 >= Moonbit_array_length(_M0L5bytesS1584)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3647 = _M0L5bytesS1584[_M0L6_2atmpS3648];
            _M0L6_2atmpS3646 = (int32_t)_M0L6_2atmpS3647;
            _M0L6_2atmpS3645 = _M0L6_2atmpS3646 & 63;
            _M0L6_2atmpS3643 = _M0L6_2atmpS3644 | _M0L6_2atmpS3645;
            _M0L1cS1588->$0 = _M0L6_2atmpS3643;
            _M0L8_2afieldS3817 = _M0L1cS1588->$0;
            moonbit_decref(_M0L1cS1588);
            _M0L3valS3660 = _M0L8_2afieldS3817;
            _M0L6_2atmpS3659 = _M0L3valS3660;
            moonbit_incref(_M0L3resS1585);
            #line 231 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1585, _M0L6_2atmpS3659);
            _M0L3valS3662 = _M0L1iS1587->$0;
            _M0L6_2atmpS3661 = _M0L3valS3662 + 3;
            _M0L1iS1587->$0 = _M0L6_2atmpS3661;
          } else {
            int32_t _M0L3valS3664 = _M0L1iS1587->$0;
            int32_t _M0L6_2atmpS3663 = _M0L3valS3664 + 3;
            int32_t _M0L3valS3687;
            int32_t _M0L6_2atmpS3686;
            int32_t _M0L6_2atmpS3679;
            int32_t _M0L3valS3685;
            int32_t _M0L6_2atmpS3684;
            int32_t _M0L6_2atmpS3683;
            int32_t _M0L6_2atmpS3682;
            int32_t _M0L6_2atmpS3681;
            int32_t _M0L6_2atmpS3680;
            int32_t _M0L6_2atmpS3672;
            int32_t _M0L3valS3678;
            int32_t _M0L6_2atmpS3677;
            int32_t _M0L6_2atmpS3676;
            int32_t _M0L6_2atmpS3675;
            int32_t _M0L6_2atmpS3674;
            int32_t _M0L6_2atmpS3673;
            int32_t _M0L6_2atmpS3666;
            int32_t _M0L3valS3671;
            int32_t _M0L6_2atmpS3670;
            int32_t _M0L6_2atmpS3669;
            int32_t _M0L6_2atmpS3668;
            int32_t _M0L6_2atmpS3667;
            int32_t _M0L6_2atmpS3665;
            int32_t _M0L3valS3689;
            int32_t _M0L6_2atmpS3688;
            int32_t _M0L3valS3693;
            int32_t _M0L6_2atmpS3692;
            int32_t _M0L6_2atmpS3691;
            int32_t _M0L6_2atmpS3690;
            int32_t _M0L8_2afieldS3818;
            int32_t _M0L3valS3697;
            int32_t _M0L6_2atmpS3696;
            int32_t _M0L6_2atmpS3695;
            int32_t _M0L6_2atmpS3694;
            int32_t _M0L3valS3699;
            int32_t _M0L6_2atmpS3698;
            if (_M0L6_2atmpS3663 >= _M0L3lenS1586) {
              moonbit_decref(_M0L1cS1588);
              moonbit_decref(_M0L1iS1587);
              moonbit_decref(_M0L5bytesS1584);
              break;
            }
            _M0L3valS3687 = _M0L1cS1588->$0;
            _M0L6_2atmpS3686 = _M0L3valS3687 & 7;
            _M0L6_2atmpS3679 = _M0L6_2atmpS3686 << 18;
            _M0L3valS3685 = _M0L1iS1587->$0;
            _M0L6_2atmpS3684 = _M0L3valS3685 + 1;
            if (
              _M0L6_2atmpS3684 < 0
              || _M0L6_2atmpS3684 >= Moonbit_array_length(_M0L5bytesS1584)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3683 = _M0L5bytesS1584[_M0L6_2atmpS3684];
            _M0L6_2atmpS3682 = (int32_t)_M0L6_2atmpS3683;
            _M0L6_2atmpS3681 = _M0L6_2atmpS3682 & 63;
            _M0L6_2atmpS3680 = _M0L6_2atmpS3681 << 12;
            _M0L6_2atmpS3672 = _M0L6_2atmpS3679 | _M0L6_2atmpS3680;
            _M0L3valS3678 = _M0L1iS1587->$0;
            _M0L6_2atmpS3677 = _M0L3valS3678 + 2;
            if (
              _M0L6_2atmpS3677 < 0
              || _M0L6_2atmpS3677 >= Moonbit_array_length(_M0L5bytesS1584)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3676 = _M0L5bytesS1584[_M0L6_2atmpS3677];
            _M0L6_2atmpS3675 = (int32_t)_M0L6_2atmpS3676;
            _M0L6_2atmpS3674 = _M0L6_2atmpS3675 & 63;
            _M0L6_2atmpS3673 = _M0L6_2atmpS3674 << 6;
            _M0L6_2atmpS3666 = _M0L6_2atmpS3672 | _M0L6_2atmpS3673;
            _M0L3valS3671 = _M0L1iS1587->$0;
            _M0L6_2atmpS3670 = _M0L3valS3671 + 3;
            if (
              _M0L6_2atmpS3670 < 0
              || _M0L6_2atmpS3670 >= Moonbit_array_length(_M0L5bytesS1584)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3669 = _M0L5bytesS1584[_M0L6_2atmpS3670];
            _M0L6_2atmpS3668 = (int32_t)_M0L6_2atmpS3669;
            _M0L6_2atmpS3667 = _M0L6_2atmpS3668 & 63;
            _M0L6_2atmpS3665 = _M0L6_2atmpS3666 | _M0L6_2atmpS3667;
            _M0L1cS1588->$0 = _M0L6_2atmpS3665;
            _M0L3valS3689 = _M0L1cS1588->$0;
            _M0L6_2atmpS3688 = _M0L3valS3689 - 65536;
            _M0L1cS1588->$0 = _M0L6_2atmpS3688;
            _M0L3valS3693 = _M0L1cS1588->$0;
            _M0L6_2atmpS3692 = _M0L3valS3693 >> 10;
            _M0L6_2atmpS3691 = _M0L6_2atmpS3692 + 55296;
            _M0L6_2atmpS3690 = _M0L6_2atmpS3691;
            moonbit_incref(_M0L3resS1585);
            #line 242 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1585, _M0L6_2atmpS3690);
            _M0L8_2afieldS3818 = _M0L1cS1588->$0;
            moonbit_decref(_M0L1cS1588);
            _M0L3valS3697 = _M0L8_2afieldS3818;
            _M0L6_2atmpS3696 = _M0L3valS3697 & 1023;
            _M0L6_2atmpS3695 = _M0L6_2atmpS3696 + 56320;
            _M0L6_2atmpS3694 = _M0L6_2atmpS3695;
            moonbit_incref(_M0L3resS1585);
            #line 243 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1585, _M0L6_2atmpS3694);
            _M0L3valS3699 = _M0L1iS1587->$0;
            _M0L6_2atmpS3698 = _M0L3valS3699 + 4;
            _M0L1iS1587->$0 = _M0L6_2atmpS3698;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1587);
      moonbit_decref(_M0L5bytesS1584);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1585);
}

int32_t _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1577(
  int32_t _M0L6_2aenvS3610,
  moonbit_string_t _M0L1sS1578
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1579;
  int32_t _M0L3lenS1580;
  int32_t _M0L1iS1581;
  int32_t _M0L8_2afieldS3819;
  #line 197 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L3resS1579
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1579)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1579->$0 = 0;
  _M0L3lenS1580 = Moonbit_array_length(_M0L1sS1578);
  _M0L1iS1581 = 0;
  while (1) {
    if (_M0L1iS1581 < _M0L3lenS1580) {
      int32_t _M0L3valS3615 = _M0L3resS1579->$0;
      int32_t _M0L6_2atmpS3612 = _M0L3valS3615 * 10;
      int32_t _M0L6_2atmpS3614;
      int32_t _M0L6_2atmpS3613;
      int32_t _M0L6_2atmpS3611;
      int32_t _M0L6_2atmpS3616;
      if (
        _M0L1iS1581 < 0 || _M0L1iS1581 >= Moonbit_array_length(_M0L1sS1578)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3614 = _M0L1sS1578[_M0L1iS1581];
      _M0L6_2atmpS3613 = _M0L6_2atmpS3614 - 48;
      _M0L6_2atmpS3611 = _M0L6_2atmpS3612 + _M0L6_2atmpS3613;
      _M0L3resS1579->$0 = _M0L6_2atmpS3611;
      _M0L6_2atmpS3616 = _M0L1iS1581 + 1;
      _M0L1iS1581 = _M0L6_2atmpS3616;
      continue;
    } else {
      moonbit_decref(_M0L1sS1578);
    }
    break;
  }
  _M0L8_2afieldS3819 = _M0L3resS1579->$0;
  moonbit_decref(_M0L3resS1579);
  return _M0L8_2afieldS3819;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1557,
  moonbit_string_t _M0L12_2adiscard__S1558,
  int32_t _M0L12_2adiscard__S1559,
  struct _M0TWssbEu* _M0L12_2adiscard__S1560,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1561
) {
  struct moonbit_result_0 _result_4419;
  #line 34 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1561);
  moonbit_decref(_M0L12_2adiscard__S1560);
  moonbit_decref(_M0L12_2adiscard__S1558);
  moonbit_decref(_M0L12_2adiscard__S1557);
  _result_4419.tag = 1;
  _result_4419.data.ok = 0;
  return _result_4419;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1562,
  moonbit_string_t _M0L12_2adiscard__S1563,
  int32_t _M0L12_2adiscard__S1564,
  struct _M0TWssbEu* _M0L12_2adiscard__S1565,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1566
) {
  struct moonbit_result_0 _result_4420;
  #line 34 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1566);
  moonbit_decref(_M0L12_2adiscard__S1565);
  moonbit_decref(_M0L12_2adiscard__S1563);
  moonbit_decref(_M0L12_2adiscard__S1562);
  _result_4420.tag = 1;
  _result_4420.data.ok = 0;
  return _result_4420;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1567,
  moonbit_string_t _M0L12_2adiscard__S1568,
  int32_t _M0L12_2adiscard__S1569,
  struct _M0TWssbEu* _M0L12_2adiscard__S1570,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1571
) {
  struct moonbit_result_0 _result_4421;
  #line 34 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1571);
  moonbit_decref(_M0L12_2adiscard__S1570);
  moonbit_decref(_M0L12_2adiscard__S1568);
  moonbit_decref(_M0L12_2adiscard__S1567);
  _result_4421.tag = 1;
  _result_4421.data.ok = 0;
  return _result_4421;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal6openai21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal6openai50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1572,
  moonbit_string_t _M0L12_2adiscard__S1573,
  int32_t _M0L12_2adiscard__S1574,
  struct _M0TWssbEu* _M0L12_2adiscard__S1575,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1576
) {
  struct moonbit_result_0 _result_4422;
  #line 34 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1576);
  moonbit_decref(_M0L12_2adiscard__S1575);
  moonbit_decref(_M0L12_2adiscard__S1573);
  moonbit_decref(_M0L12_2adiscard__S1572);
  _result_4422.tag = 1;
  _result_4422.data.ok = 0;
  return _result_4422;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal6openai28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal6openai34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1556
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1556);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai49____test__6275696c6465725f7762746573742e6d6274__2(
  
) {
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder* _M0L7builderS1549;
  moonbit_string_t _M0L6_2atmpS3453;
  moonbit_string_t _M0L6_2atmpS3454;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3455;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L6_2atmpS3452;
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice* _M0L6_2atmpS3451;
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice** _M0L6_2atmpS3450;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE* _M0L6_2atmpS3446;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L6_2atmpS3447;
  moonbit_string_t _M0L6_2atmpS3448;
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L6_2atmpS3449;
  struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk* _M0L6_2atmpS3445;
  moonbit_string_t _M0L6_2atmpS3464;
  moonbit_string_t _M0L6_2atmpS3465;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3466;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L6_2atmpS3463;
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice* _M0L6_2atmpS3462;
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice** _M0L6_2atmpS3461;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE* _M0L6_2atmpS3457;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L6_2atmpS3458;
  moonbit_string_t _M0L6_2atmpS3459;
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L6_2atmpS3460;
  struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk* _M0L6_2atmpS3456;
  moonbit_string_t _M0L6_2atmpS3475;
  moonbit_string_t _M0L6_2atmpS3476;
  moonbit_string_t _M0L6_2atmpS3481;
  moonbit_string_t _M0L6_2atmpS3483;
  moonbit_string_t _M0L6_2atmpS3484;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L6_2atmpS3482;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L6_2atmpS3480;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall** _M0L6_2atmpS3479;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3478;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3477;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L6_2atmpS3474;
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice* _M0L6_2atmpS3473;
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice** _M0L6_2atmpS3472;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE* _M0L6_2atmpS3468;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L6_2atmpS3469;
  moonbit_string_t _M0L6_2atmpS3470;
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L6_2atmpS3471;
  struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk* _M0L6_2atmpS3467;
  moonbit_string_t _M0L6_2atmpS3493;
  moonbit_string_t _M0L6_2atmpS3494;
  moonbit_string_t _M0L6_2atmpS3499;
  moonbit_string_t _M0L6_2atmpS3501;
  moonbit_string_t _M0L6_2atmpS3502;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L6_2atmpS3500;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L6_2atmpS3498;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall** _M0L6_2atmpS3497;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3496;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3495;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L6_2atmpS3492;
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice* _M0L6_2atmpS3491;
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice** _M0L6_2atmpS3490;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE* _M0L6_2atmpS3486;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L6_2atmpS3487;
  moonbit_string_t _M0L6_2atmpS3488;
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L6_2atmpS3489;
  struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk* _M0L6_2atmpS3485;
  moonbit_string_t _M0L6_2atmpS3516;
  moonbit_string_t _M0L6_2atmpS3517;
  moonbit_string_t _M0L6_2atmpS3522;
  moonbit_string_t _M0L6_2atmpS3524;
  moonbit_string_t _M0L6_2atmpS3525;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L6_2atmpS3523;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L6_2atmpS3521;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall** _M0L6_2atmpS3520;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3519;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3518;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L6_2atmpS3515;
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice* _M0L6_2atmpS3514;
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice** _M0L6_2atmpS3513;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE* _M0L6_2atmpS3504;
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L6_2atmpS3509;
  void* _M0L4SomeS3510;
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L6_2atmpS3511;
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L6_2atmpS3512;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L6_2atmpS3508;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L6_2atmpS3505;
  moonbit_string_t _M0L6_2atmpS3506;
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L6_2atmpS3507;
  struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk* _M0L6_2atmpS3503;
  struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion* _M0L6_2atmpS3609;
  struct _M0TPB6ToJson _M0L6_2atmpS3526;
  void* _M0L6_2atmpS3608;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3538;
  moonbit_string_t _M0L6_2atmpS3607;
  void* _M0L6_2atmpS3606;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3573;
  void* _M0L6_2atmpS3605;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3581;
  void* _M0L6_2atmpS3604;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3582;
  void* _M0L6_2atmpS3603;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3591;
  void* _M0L6_2atmpS3602;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3599;
  void* _M0L6_2atmpS3601;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3600;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1554;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3598;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3597;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3596;
  void* _M0L6_2atmpS3595;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3592;
  void* _M0L6_2atmpS3594;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3593;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1553;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3590;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3589;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3588;
  void* _M0L6_2atmpS3587;
  void** _M0L6_2atmpS3586;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3585;
  void* _M0L6_2atmpS3584;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3583;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1552;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3580;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3579;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3578;
  void* _M0L6_2atmpS3577;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3574;
  void* _M0L6_2atmpS3576;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3575;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1551;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3572;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3571;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3570;
  void* _M0L6_2atmpS3569;
  void** _M0L6_2atmpS3568;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3567;
  void* _M0L6_2atmpS3566;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3539;
  moonbit_string_t _M0L6_2atmpS3565;
  void* _M0L6_2atmpS3564;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3540;
  void* _M0L6_2atmpS3563;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3541;
  moonbit_string_t _M0L6_2atmpS3562;
  void* _M0L6_2atmpS3561;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3551;
  moonbit_string_t _M0L6_2atmpS3560;
  void* _M0L6_2atmpS3559;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3552;
  moonbit_string_t _M0L6_2atmpS3558;
  void* _M0L6_2atmpS3557;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3553;
  moonbit_string_t _M0L6_2atmpS3556;
  void* _M0L6_2atmpS3555;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3554;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1555;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3550;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3549;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3548;
  void* _M0L6_2atmpS3547;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3542;
  void* _M0L6_2atmpS3546;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3543;
  void* _M0L6_2atmpS3545;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3544;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1550;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3537;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3536;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3535;
  void* _M0L6_2atmpS3534;
  void* _M0L6_2atmpS3527;
  moonbit_string_t _M0L6_2atmpS3530;
  moonbit_string_t _M0L6_2atmpS3531;
  moonbit_string_t _M0L6_2atmpS3532;
  moonbit_string_t _M0L6_2atmpS3533;
  moonbit_string_t* _M0L6_2atmpS3529;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3528;
  #line 116 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  #line 117 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L7builderS1549
  = _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder3new();
  _M0L6_2atmpS3453 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3454 = 0;
  _M0L6_2atmpS3455 = 0;
  #line 126 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3452
  = _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(_M0L6_2atmpS3453, _M0L6_2atmpS3454, 3ll, _M0L6_2atmpS3455);
  #line 124 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3451
  = _M0FP48clawteam8clawteam8internal6openai13chunk__choice(0, _M0L6_2atmpS3452, 4294967296ll);
  _M0L6_2atmpS3450
  = (struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3450[0] = _M0L6_2atmpS3451;
  _M0L6_2atmpS3446
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE));
  Moonbit_object_header(_M0L6_2atmpS3446)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3446->$0 = _M0L6_2atmpS3450;
  _M0L6_2atmpS3446->$1 = 1;
  _M0L6_2atmpS3447 = 0;
  _M0L6_2atmpS3448 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6_2atmpS3449 = 0;
  #line 119 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3445
  = _M0FP48clawteam8clawteam8internal6openai5chunk((moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS3446, 1677652288, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS3447, _M0L6_2atmpS3448, 2ll, _M0L6_2atmpS3449);
  moonbit_incref(_M0L7builderS1549);
  #line 118 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder10add__chunk(_M0L7builderS1549, _M0L6_2atmpS3445);
  _M0L6_2atmpS3464 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS3465 = 0;
  _M0L6_2atmpS3466 = 0;
  #line 140 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3463
  = _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(_M0L6_2atmpS3464, _M0L6_2atmpS3465, 3ll, _M0L6_2atmpS3466);
  #line 138 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3462
  = _M0FP48clawteam8clawteam8internal6openai13chunk__choice(0, _M0L6_2atmpS3463, 4294967296ll);
  _M0L6_2atmpS3461
  = (struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3461[0] = _M0L6_2atmpS3462;
  _M0L6_2atmpS3457
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE));
  Moonbit_object_header(_M0L6_2atmpS3457)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3457->$0 = _M0L6_2atmpS3461;
  _M0L6_2atmpS3457->$1 = 1;
  _M0L6_2atmpS3458 = 0;
  _M0L6_2atmpS3459 = 0;
  _M0L6_2atmpS3460 = 0;
  #line 137 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3456
  = _M0FP48clawteam8clawteam8internal6openai5chunk((moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS3457, 1677652288, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS3458, _M0L6_2atmpS3459, 4294967296ll, _M0L6_2atmpS3460);
  moonbit_incref(_M0L7builderS1549);
  #line 136 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder10add__chunk(_M0L7builderS1549, _M0L6_2atmpS3456);
  _M0L6_2atmpS3475 = 0;
  _M0L6_2atmpS3476 = 0;
  _M0L6_2atmpS3481 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS3483 = 0;
  _M0L6_2atmpS3484 = (moonbit_string_t)moonbit_string_literal_15.data;
  #line 155 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3482
  = _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(_M0L6_2atmpS3483, _M0L6_2atmpS3484);
  #line 152 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3480
  = _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(0, _M0L6_2atmpS3481, _M0L6_2atmpS3482);
  _M0L6_2atmpS3479
  = (struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3479[0] = _M0L6_2atmpS3480;
  _M0L6_2atmpS3478
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE));
  Moonbit_object_header(_M0L6_2atmpS3478)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3478->$0 = _M0L6_2atmpS3479;
  _M0L6_2atmpS3478->$1 = 1;
  _M0L6_2atmpS3477 = _M0L6_2atmpS3478;
  #line 151 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3474
  = _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(_M0L6_2atmpS3475, _M0L6_2atmpS3476, 3ll, _M0L6_2atmpS3477);
  #line 149 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3473
  = _M0FP48clawteam8clawteam8internal6openai13chunk__choice(0, _M0L6_2atmpS3474, 4294967296ll);
  _M0L6_2atmpS3472
  = (struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3472[0] = _M0L6_2atmpS3473;
  _M0L6_2atmpS3468
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE));
  Moonbit_object_header(_M0L6_2atmpS3468)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3468->$0 = _M0L6_2atmpS3472;
  _M0L6_2atmpS3468->$1 = 1;
  _M0L6_2atmpS3469 = 0;
  _M0L6_2atmpS3470 = 0;
  _M0L6_2atmpS3471 = 0;
  #line 148 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3467
  = _M0FP48clawteam8clawteam8internal6openai5chunk((moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS3468, 1677652288, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS3469, _M0L6_2atmpS3470, 4294967296ll, _M0L6_2atmpS3471);
  moonbit_incref(_M0L7builderS1549);
  #line 147 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder10add__chunk(_M0L7builderS1549, _M0L6_2atmpS3467);
  _M0L6_2atmpS3493 = 0;
  _M0L6_2atmpS3494 = 0;
  _M0L6_2atmpS3499 = 0;
  _M0L6_2atmpS3501 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS3502 = 0;
  #line 168 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3500
  = _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(_M0L6_2atmpS3501, _M0L6_2atmpS3502);
  #line 166 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3498
  = _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(0, _M0L6_2atmpS3499, _M0L6_2atmpS3500);
  _M0L6_2atmpS3497
  = (struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3497[0] = _M0L6_2atmpS3498;
  _M0L6_2atmpS3496
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE));
  Moonbit_object_header(_M0L6_2atmpS3496)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3496->$0 = _M0L6_2atmpS3497;
  _M0L6_2atmpS3496->$1 = 1;
  _M0L6_2atmpS3495 = _M0L6_2atmpS3496;
  #line 165 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3492
  = _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(_M0L6_2atmpS3493, _M0L6_2atmpS3494, 3ll, _M0L6_2atmpS3495);
  #line 163 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3491
  = _M0FP48clawteam8clawteam8internal6openai13chunk__choice(0, _M0L6_2atmpS3492, 4294967296ll);
  _M0L6_2atmpS3490
  = (struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3490[0] = _M0L6_2atmpS3491;
  _M0L6_2atmpS3486
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE));
  Moonbit_object_header(_M0L6_2atmpS3486)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3486->$0 = _M0L6_2atmpS3490;
  _M0L6_2atmpS3486->$1 = 1;
  _M0L6_2atmpS3487 = 0;
  _M0L6_2atmpS3488 = 0;
  _M0L6_2atmpS3489 = 0;
  #line 162 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3485
  = _M0FP48clawteam8clawteam8internal6openai5chunk((moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS3486, 1677652288, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS3487, _M0L6_2atmpS3488, 4294967296ll, _M0L6_2atmpS3489);
  moonbit_incref(_M0L7builderS1549);
  #line 161 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder10add__chunk(_M0L7builderS1549, _M0L6_2atmpS3485);
  _M0L6_2atmpS3516 = 0;
  _M0L6_2atmpS3517 = 0;
  _M0L6_2atmpS3522 = 0;
  _M0L6_2atmpS3524 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS3525 = 0;
  #line 189 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3523
  = _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(_M0L6_2atmpS3524, _M0L6_2atmpS3525);
  #line 187 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3521
  = _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(0, _M0L6_2atmpS3522, _M0L6_2atmpS3523);
  _M0L6_2atmpS3520
  = (struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3520[0] = _M0L6_2atmpS3521;
  _M0L6_2atmpS3519
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE));
  Moonbit_object_header(_M0L6_2atmpS3519)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3519->$0 = _M0L6_2atmpS3520;
  _M0L6_2atmpS3519->$1 = 1;
  _M0L6_2atmpS3518 = _M0L6_2atmpS3519;
  #line 186 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3515
  = _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(_M0L6_2atmpS3516, _M0L6_2atmpS3517, 3ll, _M0L6_2atmpS3518);
  #line 184 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3514
  = _M0FP48clawteam8clawteam8internal6openai13chunk__choice(0, _M0L6_2atmpS3515, 0ll);
  _M0L6_2atmpS3513
  = (struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3513[0] = _M0L6_2atmpS3514;
  _M0L6_2atmpS3504
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE));
  Moonbit_object_header(_M0L6_2atmpS3504)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3504->$0 = _M0L6_2atmpS3513;
  _M0L6_2atmpS3504->$1 = 1;
  _M0L6_2atmpS3509 = 0;
  _M0L4SomeS3510
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGdE4Some));
  Moonbit_object_header(_M0L4SomeS3510)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC16option6OptionGdE4Some) >> 2, 0, 1);
  ((struct _M0DTPC16option6OptionGdE4Some*)_M0L4SomeS3510)->$0
  = 0x1.0624dd2f1a9fcp-9;
  _M0L6_2atmpS3511 = 0;
  _M0L6_2atmpS3512 = 0;
  #line 199 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3508
  = _M0MP48clawteam8clawteam8internal6openai15CompletionUsage3new(20, _M0L6_2atmpS3509, _M0L4SomeS3510, _M0L6_2atmpS3511, 10, _M0L6_2atmpS3512, 30);
  _M0L6_2atmpS3505 = _M0L6_2atmpS3508;
  _M0L6_2atmpS3506 = 0;
  _M0L6_2atmpS3507 = 0;
  #line 179 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3503
  = _M0FP48clawteam8clawteam8internal6openai5chunk((moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS3504, 1677652288, (moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS3505, _M0L6_2atmpS3506, 4294967296ll, _M0L6_2atmpS3507);
  moonbit_incref(_M0L7builderS1549);
  #line 178 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder10add__chunk(_M0L7builderS1549, _M0L6_2atmpS3503);
  #line 207 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3609
  = _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completion(_M0L7builderS1549);
  _M0L6_2atmpS3526
  = (struct _M0TPB6ToJson){
    _M0FP0131clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletion_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L6_2atmpS3609
  };
  #line 208 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3608
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_11.data);
  _M0L8_2atupleS3538
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3538)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3538->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3538->$1 = _M0L6_2atmpS3608;
  _M0L6_2atmpS3607 = 0;
  #line 211 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3606 = _M0MPC14json4Json6number(0x0p+0, _M0L6_2atmpS3607);
  _M0L8_2atupleS3573
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3573)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3573->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3573->$1 = _M0L6_2atmpS3606;
  #line 213 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3605
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_20.data);
  _M0L8_2atupleS3581
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3581)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3581->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3581->$1 = _M0L6_2atmpS3605;
  #line 214 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3604
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3582
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3582)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3582->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3582->$1 = _M0L6_2atmpS3604;
  #line 217 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3603
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_14.data);
  _M0L8_2atupleS3591
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3591)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3591->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3591->$1 = _M0L6_2atmpS3603;
  #line 219 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3602
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_15.data);
  _M0L8_2atupleS3599
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3599)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3599->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3599->$1 = _M0L6_2atmpS3602;
  #line 220 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3601
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3600
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3600)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3600->$0 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L8_2atupleS3600->$1 = _M0L6_2atmpS3601;
  _M0L7_2abindS1554 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1554[0] = _M0L8_2atupleS3599;
  _M0L7_2abindS1554[1] = _M0L8_2atupleS3600;
  _M0L6_2atmpS3598 = _M0L7_2abindS1554;
  _M0L6_2atmpS3597
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3598
  };
  #line 218 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3596 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3597);
  #line 218 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3595 = _M0MPC14json4Json6object(_M0L6_2atmpS3596);
  _M0L8_2atupleS3592
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3592)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3592->$0 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L8_2atupleS3592->$1 = _M0L6_2atmpS3595;
  #line 222 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3594
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_27.data);
  _M0L8_2atupleS3593
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3593)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3593->$0 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L8_2atupleS3593->$1 = _M0L6_2atmpS3594;
  _M0L7_2abindS1553 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1553[0] = _M0L8_2atupleS3591;
  _M0L7_2abindS1553[1] = _M0L8_2atupleS3592;
  _M0L7_2abindS1553[2] = _M0L8_2atupleS3593;
  _M0L6_2atmpS3590 = _M0L7_2abindS1553;
  _M0L6_2atmpS3589
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3590
  };
  #line 216 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3588 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3589);
  #line 216 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3587 = _M0MPC14json4Json6object(_M0L6_2atmpS3588);
  _M0L6_2atmpS3586 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3586[0] = _M0L6_2atmpS3587;
  _M0L6_2atmpS3585
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3585)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3585->$0 = _M0L6_2atmpS3586;
  _M0L6_2atmpS3585->$1 = 1;
  #line 215 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3584 = _M0MPC14json4Json5array(_M0L6_2atmpS3585);
  _M0L8_2atupleS3583
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3583)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3583->$0 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L8_2atupleS3583->$1 = _M0L6_2atmpS3584;
  _M0L7_2abindS1552 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1552[0] = _M0L8_2atupleS3581;
  _M0L7_2abindS1552[1] = _M0L8_2atupleS3582;
  _M0L7_2abindS1552[2] = _M0L8_2atupleS3583;
  _M0L6_2atmpS3580 = _M0L7_2abindS1552;
  _M0L6_2atmpS3579
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3580
  };
  #line 212 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3578 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3579);
  #line 212 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3577 = _M0MPC14json4Json6object(_M0L6_2atmpS3578);
  _M0L8_2atupleS3574
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3574)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3574->$0 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L8_2atupleS3574->$1 = _M0L6_2atmpS3577;
  #line 226 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3576
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_31.data);
  _M0L8_2atupleS3575
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3575)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3575->$0 = (moonbit_string_t)moonbit_string_literal_32.data;
  _M0L8_2atupleS3575->$1 = _M0L6_2atmpS3576;
  _M0L7_2abindS1551 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1551[0] = _M0L8_2atupleS3573;
  _M0L7_2abindS1551[1] = _M0L8_2atupleS3574;
  _M0L7_2abindS1551[2] = _M0L8_2atupleS3575;
  _M0L6_2atmpS3572 = _M0L7_2abindS1551;
  _M0L6_2atmpS3571
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3572
  };
  #line 210 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3570 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3571);
  #line 210 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3569 = _M0MPC14json4Json6object(_M0L6_2atmpS3570);
  _M0L6_2atmpS3568 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3568[0] = _M0L6_2atmpS3569;
  _M0L6_2atmpS3567
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3567)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3567->$0 = _M0L6_2atmpS3568;
  _M0L6_2atmpS3567->$1 = 1;
  #line 209 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3566 = _M0MPC14json4Json5array(_M0L6_2atmpS3567);
  _M0L8_2atupleS3539
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3539)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3539->$0 = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L8_2atupleS3539->$1 = _M0L6_2atmpS3566;
  _M0L6_2atmpS3565 = 0;
  #line 229 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3564
  = _M0MPC14json4Json6number(0x1.8ffbc5p+30, _M0L6_2atmpS3565);
  _M0L8_2atupleS3540
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3540)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3540->$0 = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L8_2atupleS3540->$1 = _M0L6_2atmpS3564;
  #line 230 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3563
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_12.data);
  _M0L8_2atupleS3541
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3541)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3541->$0 = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L8_2atupleS3541->$1 = _M0L6_2atmpS3563;
  _M0L6_2atmpS3562 = 0;
  #line 232 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3561 = _M0MPC14json4Json6number(0x1.4p+4, _M0L6_2atmpS3562);
  _M0L8_2atupleS3551
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3551)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3551->$0 = (moonbit_string_t)moonbit_string_literal_36.data;
  _M0L8_2atupleS3551->$1 = _M0L6_2atmpS3561;
  _M0L6_2atmpS3560 = 0;
  #line 233 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3559
  = _M0MPC14json4Json6number(0x1.0624dd2f1a9fcp-9, _M0L6_2atmpS3560);
  _M0L8_2atupleS3552
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3552)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3552->$0 = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L8_2atupleS3552->$1 = _M0L6_2atmpS3559;
  _M0L6_2atmpS3558 = 0;
  #line 234 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3557 = _M0MPC14json4Json6number(0x1.4p+3, _M0L6_2atmpS3558);
  _M0L8_2atupleS3553
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3553)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3553->$0 = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L8_2atupleS3553->$1 = _M0L6_2atmpS3557;
  _M0L6_2atmpS3556 = 0;
  #line 235 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3555 = _M0MPC14json4Json6number(0x1.ep+4, _M0L6_2atmpS3556);
  _M0L8_2atupleS3554
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3554)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3554->$0 = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L8_2atupleS3554->$1 = _M0L6_2atmpS3555;
  _M0L7_2abindS1555 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS1555[0] = _M0L8_2atupleS3551;
  _M0L7_2abindS1555[1] = _M0L8_2atupleS3552;
  _M0L7_2abindS1555[2] = _M0L8_2atupleS3553;
  _M0L7_2abindS1555[3] = _M0L8_2atupleS3554;
  _M0L6_2atmpS3550 = _M0L7_2abindS1555;
  _M0L6_2atmpS3549
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 4, _M0L6_2atmpS3550
  };
  #line 231 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3548 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3549);
  #line 231 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3547 = _M0MPC14json4Json6object(_M0L6_2atmpS3548);
  _M0L8_2atupleS3542
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3542)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3542->$0 = (moonbit_string_t)moonbit_string_literal_40.data;
  _M0L8_2atupleS3542->$1 = _M0L6_2atmpS3547;
  #line 237 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3546
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_10.data);
  _M0L8_2atupleS3543
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3543)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3543->$0 = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L8_2atupleS3543->$1 = _M0L6_2atmpS3546;
  #line 238 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3545
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_42.data);
  _M0L8_2atupleS3544
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3544)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3544->$0 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L8_2atupleS3544->$1 = _M0L6_2atmpS3545;
  _M0L7_2abindS1550 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(7);
  _M0L7_2abindS1550[0] = _M0L8_2atupleS3538;
  _M0L7_2abindS1550[1] = _M0L8_2atupleS3539;
  _M0L7_2abindS1550[2] = _M0L8_2atupleS3540;
  _M0L7_2abindS1550[3] = _M0L8_2atupleS3541;
  _M0L7_2abindS1550[4] = _M0L8_2atupleS3542;
  _M0L7_2abindS1550[5] = _M0L8_2atupleS3543;
  _M0L7_2abindS1550[6] = _M0L8_2atupleS3544;
  _M0L6_2atmpS3537 = _M0L7_2abindS1550;
  _M0L6_2atmpS3536
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 7, _M0L6_2atmpS3537
  };
  #line 207 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3535 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3536);
  #line 207 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3534 = _M0MPC14json4Json6object(_M0L6_2atmpS3535);
  _M0L6_2atmpS3527 = _M0L6_2atmpS3534;
  _M0L6_2atmpS3530 = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L6_2atmpS3531 = (moonbit_string_t)moonbit_string_literal_45.data;
  _M0L6_2atmpS3532 = 0;
  _M0L6_2atmpS3533 = 0;
  _M0L6_2atmpS3529 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3529[0] = _M0L6_2atmpS3530;
  _M0L6_2atmpS3529[1] = _M0L6_2atmpS3531;
  _M0L6_2atmpS3529[2] = _M0L6_2atmpS3532;
  _M0L6_2atmpS3529[3] = _M0L6_2atmpS3533;
  _M0L6_2atmpS3528
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3528)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3528->$0 = _M0L6_2atmpS3529;
  _M0L6_2atmpS3528->$1 = 4;
  #line 207 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3526, _M0L6_2atmpS3527, (moonbit_string_t)moonbit_string_literal_46.data, _M0L6_2atmpS3528);
}

struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completion(
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder* _M0L4selfS1546
) {
  moonbit_string_t _M0L8_2afieldS3825;
  moonbit_string_t _M0L2idS3444;
  moonbit_string_t _M0L6_2atmpS3426;
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L8_2afieldS3824;
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L7choicesS3443;
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L6_2atmpS3436;
  struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2atmpS3437;
  struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2atmpS3435;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0L6_2atmpS3427;
  int64_t _M0L7createdS3434;
  int32_t _M0L6_2atmpS3428;
  moonbit_string_t _M0L8_2afieldS3823;
  moonbit_string_t _M0L5modelS3433;
  moonbit_string_t _M0L6_2atmpS3429;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L8_2afieldS3822;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L5usageS3430;
  moonbit_string_t _M0L8_2afieldS3821;
  moonbit_string_t _M0L19system__fingerprintS3431;
  int64_t _M0L8_2afieldS3820;
  int32_t _M0L6_2acntS4239;
  int64_t _M0L13service__tierS3432;
  #line 216 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L8_2afieldS3825 = _M0L4selfS1546->$0;
  _M0L2idS3444 = _M0L8_2afieldS3825;
  if (_M0L2idS3444) {
    moonbit_incref(_M0L2idS3444);
  }
  #line 220 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3426 = _M0MPC16option6Option6unwrapGsE(_M0L2idS3444);
  _M0L8_2afieldS3824 = _M0L4selfS1546->$1;
  _M0L7choicesS3443 = _M0L8_2afieldS3824;
  moonbit_incref(_M0L7choicesS3443);
  #line 221 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3436
  = _M0MPC15array5Array4iterGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L7choicesS3443);
  _M0L6_2atmpS3437
  = (struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice*)&_M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completionC3438l223$closure.data;
  #line 221 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3435
  = _M0MPB4Iter11filter__mapGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(_M0L6_2atmpS3436, _M0L6_2atmpS3437);
  #line 221 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3427
  = _M0MPB4Iter9to__arrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(_M0L6_2atmpS3435);
  _M0L7createdS3434 = _M0L4selfS1546->$2;
  #line 225 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3428 = _M0MPC16option6Option6unwrapGiE(_M0L7createdS3434);
  _M0L8_2afieldS3823 = _M0L4selfS1546->$3;
  _M0L5modelS3433 = _M0L8_2afieldS3823;
  if (_M0L5modelS3433) {
    moonbit_incref(_M0L5modelS3433);
  }
  #line 226 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3429 = _M0MPC16option6Option6unwrapGsE(_M0L5modelS3433);
  _M0L8_2afieldS3822 = _M0L4selfS1546->$4;
  _M0L5usageS3430 = _M0L8_2afieldS3822;
  _M0L8_2afieldS3821 = _M0L4selfS1546->$5;
  _M0L19system__fingerprintS3431 = _M0L8_2afieldS3821;
  _M0L8_2afieldS3820 = _M0L4selfS1546->$6;
  _M0L6_2acntS4239 = Moonbit_object_header(_M0L4selfS1546)->rc;
  if (_M0L6_2acntS4239 > 1) {
    int32_t _M0L11_2anew__cntS4244 = _M0L6_2acntS4239 - 1;
    Moonbit_object_header(_M0L4selfS1546)->rc = _M0L11_2anew__cntS4244;
    if (_M0L19system__fingerprintS3431) {
      moonbit_incref(_M0L19system__fingerprintS3431);
    }
    if (_M0L5usageS3430) {
      moonbit_incref(_M0L5usageS3430);
    }
  } else if (_M0L6_2acntS4239 == 1) {
    struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L8_2afieldS4243 =
      _M0L4selfS1546->$7;
    moonbit_string_t _M0L8_2afieldS4242;
    struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L8_2afieldS4241;
    moonbit_string_t _M0L8_2afieldS4240;
    if (_M0L8_2afieldS4243) {
      moonbit_decref(_M0L8_2afieldS4243);
    }
    _M0L8_2afieldS4242 = _M0L4selfS1546->$3;
    if (_M0L8_2afieldS4242) {
      moonbit_decref(_M0L8_2afieldS4242);
    }
    _M0L8_2afieldS4241 = _M0L4selfS1546->$1;
    moonbit_decref(_M0L8_2afieldS4241);
    _M0L8_2afieldS4240 = _M0L4selfS1546->$0;
    if (_M0L8_2afieldS4240) {
      moonbit_decref(_M0L8_2afieldS4240);
    }
    #line 229 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    moonbit_free(_M0L4selfS1546);
  }
  _M0L13service__tierS3432 = _M0L8_2afieldS3820;
  #line 219 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  return _M0MP48clawteam8clawteam8internal6openai14ChatCompletion3new(_M0L6_2atmpS3426, _M0L6_2atmpS3427, _M0L6_2atmpS3428, _M0L6_2atmpS3429, _M0L5usageS3430, _M0L19system__fingerprintS3431, _M0L13service__tierS3432);
}

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completionC3438l223(
  struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2aenvS3439,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L7builderS1547
) {
  struct _M0TWRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderERP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2atmpS3440;
  #line 223 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  moonbit_decref(_M0L6_2aenvS3439);
  _M0L6_2atmpS3440
  = (struct _M0TWRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderERP48clawteam8clawteam8internal6openai20ChatCompletionChoice*)&_M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completionC3441l223$closure.data;
  #line 223 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  return _M0MPC16option6Option3mapGRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(_M0L7builderS1547, _M0L6_2atmpS3440);
}

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder20to__chat__completionC3441l223(
  struct _M0TWRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderERP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2aenvS3442,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L1bS1548
) {
  #line 223 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  moonbit_decref(_M0L6_2aenvS3442);
  #line 223 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  return _M0MP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder10to__choice(_M0L1bS1548);
}

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder10to__choice(
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L4selfS1545
) {
  int64_t _M0L14finish__reasonS3422;
  int32_t _M0L5indexS3423;
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* _M0L8_2afieldS3826;
  int32_t _M0L6_2acntS4245;
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* _M0L7messageS3425;
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0L6_2atmpS3424;
  #line 137 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L14finish__reasonS3422 = _M0L4selfS1545->$0;
  _M0L5indexS3423 = _M0L4selfS1545->$1;
  _M0L8_2afieldS3826 = _M0L4selfS1545->$2;
  _M0L6_2acntS4245 = Moonbit_object_header(_M0L4selfS1545)->rc;
  if (_M0L6_2acntS4245 > 1) {
    int32_t _M0L11_2anew__cntS4246 = _M0L6_2acntS4245 - 1;
    Moonbit_object_header(_M0L4selfS1545)->rc = _M0L11_2anew__cntS4246;
    moonbit_incref(_M0L8_2afieldS3826);
  } else if (_M0L6_2acntS4245 == 1) {
    #line 143 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    moonbit_free(_M0L4selfS1545);
  }
  _M0L7messageS3425 = _M0L8_2afieldS3826;
  #line 143 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3424
  = _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__message(_M0L7messageS3425);
  #line 140 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  return _M0MP48clawteam8clawteam8internal6openai20ChatCompletionChoice3new(_M0L14finish__reasonS3422, _M0L5indexS3423, _M0L6_2atmpS3424);
}

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder3new(
  
) {
  moonbit_string_t _M0L6_2atmpS3415;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L6_2atmpS3421;
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L6_2atmpS3416;
  moonbit_string_t _M0L6_2atmpS3417;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L6_2atmpS3418;
  moonbit_string_t _M0L6_2atmpS3419;
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L6_2atmpS3420;
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder* _block_4423;
  #line 160 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3415 = 0;
  _M0L6_2atmpS3421
  = (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder**)moonbit_empty_ref_array;
  _M0L6_2atmpS3416
  = (struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE));
  Moonbit_object_header(_M0L6_2atmpS3416)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3416->$0 = _M0L6_2atmpS3421;
  _M0L6_2atmpS3416->$1 = 0;
  _M0L6_2atmpS3417 = 0;
  _M0L6_2atmpS3418 = 0;
  _M0L6_2atmpS3419 = 0;
  _M0L6_2atmpS3420 = 0;
  _block_4423
  = (struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder));
  Moonbit_object_header(_block_4423)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder, $0) >> 2, 6, 0);
  _block_4423->$0 = _M0L6_2atmpS3415;
  _block_4423->$1 = _M0L6_2atmpS3416;
  _block_4423->$2 = 4294967296ll;
  _block_4423->$3 = _M0L6_2atmpS3417;
  _block_4423->$4 = _M0L6_2atmpS3418;
  _block_4423->$5 = _M0L6_2atmpS3419;
  _block_4423->$6 = 4294967296ll;
  _block_4423->$7 = _M0L6_2atmpS3420;
  return _block_4423;
}

int32_t _M0MP48clawteam8clawteam8internal6openai21ChatCompletionBuilder10add__chunk(
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionBuilder* _M0L4selfS1517,
  struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk* _M0L5chunkS1518
) {
  moonbit_string_t _M0L8_2afieldS3860;
  moonbit_string_t _M0L7_2abindS1516;
  int32_t _M0L6_2atmpS3859;
  int64_t _M0L7_2abindS1519;
  moonbit_string_t _M0L8_2afieldS3856;
  moonbit_string_t _M0L7_2abindS1520;
  int32_t _M0L6_2atmpS3855;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L8_2afieldS3852;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L7_2abindS1521;
  int32_t _M0L6_2atmpS3851;
  moonbit_string_t _M0L8_2afieldS3846;
  moonbit_string_t _M0L7_2abindS1523;
  int32_t _M0L6_2atmpS3845;
  int64_t _M0L7_2abindS1525;
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L8_2afieldS3840;
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L7_2abindS1527;
  int32_t _M0L6_2atmpS3839;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE* _M0L8_2afieldS3834;
  int32_t _M0L6_2acntS4247;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE* _M0L7_2abindS1529;
  int32_t _M0L7_2abindS1530;
  int32_t _M0L2__S1531;
  #line 174 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L8_2afieldS3860 = _M0L4selfS1517->$0;
  _M0L7_2abindS1516 = _M0L8_2afieldS3860;
  _M0L6_2atmpS3859 = _M0L7_2abindS1516 == 0;
  if (_M0L6_2atmpS3859) {
    moonbit_string_t _M0L8_2afieldS3858 = _M0L5chunkS1518->$0;
    moonbit_string_t _M0L2idS3396 = _M0L8_2afieldS3858;
    moonbit_string_t _M0L6_2atmpS3395;
    moonbit_string_t _M0L6_2aoldS3857;
    moonbit_incref(_M0L2idS3396);
    _M0L6_2atmpS3395 = _M0L2idS3396;
    _M0L6_2aoldS3857 = _M0L4selfS1517->$0;
    if (_M0L6_2aoldS3857) {
      moonbit_decref(_M0L6_2aoldS3857);
    }
    _M0L4selfS1517->$0 = _M0L6_2atmpS3395;
  }
  _M0L7_2abindS1519 = _M0L4selfS1517->$2;
  if (_M0L7_2abindS1519 == 4294967296ll) {
    int32_t _M0L7createdS3398 = _M0L5chunkS1518->$2;
    int64_t _M0L6_2atmpS3397 = (int64_t)_M0L7createdS3398;
    _M0L4selfS1517->$2 = _M0L6_2atmpS3397;
  }
  _M0L8_2afieldS3856 = _M0L4selfS1517->$3;
  _M0L7_2abindS1520 = _M0L8_2afieldS3856;
  _M0L6_2atmpS3855 = _M0L7_2abindS1520 == 0;
  if (_M0L6_2atmpS3855) {
    moonbit_string_t _M0L8_2afieldS3854 = _M0L5chunkS1518->$3;
    moonbit_string_t _M0L5modelS3400 = _M0L8_2afieldS3854;
    moonbit_string_t _M0L6_2atmpS3399;
    moonbit_string_t _M0L6_2aoldS3853;
    moonbit_incref(_M0L5modelS3400);
    _M0L6_2atmpS3399 = _M0L5modelS3400;
    _M0L6_2aoldS3853 = _M0L4selfS1517->$3;
    if (_M0L6_2aoldS3853) {
      moonbit_decref(_M0L6_2aoldS3853);
    }
    _M0L4selfS1517->$3 = _M0L6_2atmpS3399;
  }
  _M0L8_2afieldS3852 = _M0L4selfS1517->$4;
  _M0L7_2abindS1521 = _M0L8_2afieldS3852;
  _M0L6_2atmpS3851 = _M0L7_2abindS1521 == 0;
  if (_M0L6_2atmpS3851) {
    struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L8_2afieldS3850 =
      _M0L5chunkS1518->$4;
    struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L7_2abindS1522 =
      _M0L8_2afieldS3850;
    int32_t _M0L6_2atmpS3849 = _M0L7_2abindS1522 == 0;
    if (_M0L6_2atmpS3849) {
      
    } else {
      struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L8_2afieldS3848 =
        _M0L5chunkS1518->$4;
      struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L5usageS3401 =
        _M0L8_2afieldS3848;
      struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L6_2aoldS3847 =
        _M0L4selfS1517->$4;
      if (_M0L5usageS3401) {
        moonbit_incref(_M0L5usageS3401);
      }
      if (_M0L6_2aoldS3847) {
        moonbit_decref(_M0L6_2aoldS3847);
      }
      _M0L4selfS1517->$4 = _M0L5usageS3401;
    }
  }
  _M0L8_2afieldS3846 = _M0L4selfS1517->$5;
  _M0L7_2abindS1523 = _M0L8_2afieldS3846;
  _M0L6_2atmpS3845 = _M0L7_2abindS1523 == 0;
  if (_M0L6_2atmpS3845) {
    moonbit_string_t _M0L8_2afieldS3844 = _M0L5chunkS1518->$5;
    moonbit_string_t _M0L7_2abindS1524 = _M0L8_2afieldS3844;
    int32_t _M0L6_2atmpS3843 = _M0L7_2abindS1524 == 0;
    if (_M0L6_2atmpS3843) {
      
    } else {
      moonbit_string_t _M0L8_2afieldS3842 = _M0L5chunkS1518->$5;
      moonbit_string_t _M0L19system__fingerprintS3402 = _M0L8_2afieldS3842;
      moonbit_string_t _M0L6_2aoldS3841 = _M0L4selfS1517->$5;
      if (_M0L19system__fingerprintS3402) {
        moonbit_incref(_M0L19system__fingerprintS3402);
      }
      if (_M0L6_2aoldS3841) {
        moonbit_decref(_M0L6_2aoldS3841);
      }
      _M0L4selfS1517->$5 = _M0L19system__fingerprintS3402;
    }
  }
  _M0L7_2abindS1525 = _M0L4selfS1517->$6;
  if (_M0L7_2abindS1525 == 4294967296ll) {
    int64_t _M0L7_2abindS1526 = _M0L5chunkS1518->$6;
    if (_M0L7_2abindS1526 == 4294967296ll) {
      
    } else {
      int64_t _M0L13service__tierS3403 = _M0L5chunkS1518->$6;
      _M0L4selfS1517->$6 = _M0L13service__tierS3403;
    }
  }
  _M0L8_2afieldS3840 = _M0L4selfS1517->$7;
  _M0L7_2abindS1527 = _M0L8_2afieldS3840;
  _M0L6_2atmpS3839 = _M0L7_2abindS1527 == 0;
  if (_M0L6_2atmpS3839) {
    struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L8_2afieldS3838 =
      _M0L5chunkS1518->$7;
    struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L7_2abindS1528 =
      _M0L8_2afieldS3838;
    int32_t _M0L6_2atmpS3837 = _M0L7_2abindS1528 == 0;
    if (_M0L6_2atmpS3837) {
      
    } else {
      struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L8_2afieldS3836 =
        _M0L5chunkS1518->$7;
      struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L5errorS3404 =
        _M0L8_2afieldS3836;
      struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L6_2aoldS3835 =
        _M0L4selfS1517->$7;
      if (_M0L5errorS3404) {
        moonbit_incref(_M0L5errorS3404);
      }
      if (_M0L6_2aoldS3835) {
        moonbit_decref(_M0L6_2aoldS3835);
      }
      _M0L4selfS1517->$7 = _M0L5errorS3404;
    }
  }
  _M0L8_2afieldS3834 = _M0L5chunkS1518->$1;
  _M0L6_2acntS4247 = Moonbit_object_header(_M0L5chunkS1518)->rc;
  if (_M0L6_2acntS4247 > 1) {
    int32_t _M0L11_2anew__cntS4253 = _M0L6_2acntS4247 - 1;
    Moonbit_object_header(_M0L5chunkS1518)->rc = _M0L11_2anew__cntS4253;
    moonbit_incref(_M0L8_2afieldS3834);
  } else if (_M0L6_2acntS4247 == 1) {
    struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L8_2afieldS4252 =
      _M0L5chunkS1518->$7;
    moonbit_string_t _M0L8_2afieldS4251;
    struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L8_2afieldS4250;
    moonbit_string_t _M0L8_2afieldS4249;
    moonbit_string_t _M0L8_2afieldS4248;
    if (_M0L8_2afieldS4252) {
      moonbit_decref(_M0L8_2afieldS4252);
    }
    _M0L8_2afieldS4251 = _M0L5chunkS1518->$5;
    if (_M0L8_2afieldS4251) {
      moonbit_decref(_M0L8_2afieldS4251);
    }
    _M0L8_2afieldS4250 = _M0L5chunkS1518->$4;
    if (_M0L8_2afieldS4250) {
      moonbit_decref(_M0L8_2afieldS4250);
    }
    _M0L8_2afieldS4249 = _M0L5chunkS1518->$3;
    moonbit_decref(_M0L8_2afieldS4249);
    _M0L8_2afieldS4248 = _M0L5chunkS1518->$0;
    moonbit_decref(_M0L8_2afieldS4248);
    #line 199 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    moonbit_free(_M0L5chunkS1518);
  }
  _M0L7_2abindS1529 = _M0L8_2afieldS3834;
  _M0L7_2abindS1530 = _M0L7_2abindS1529->$1;
  _M0L2__S1531 = 0;
  while (1) {
    if (_M0L2__S1531 < _M0L7_2abindS1530) {
      struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice** _M0L8_2afieldS3833 =
        _M0L7_2abindS1529->$0;
      struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice** _M0L3bufS3414 =
        _M0L8_2afieldS3833;
      struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice* _M0L6_2atmpS3832 =
        (struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice*)_M0L3bufS3414[
          _M0L2__S1531
        ];
      struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice* _M0L13choice__chunkS1532 =
        _M0L6_2atmpS3832;
      int32_t _M0L5indexS1533 = _M0L13choice__chunkS1532->$0;
      struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L7builderS1534;
      struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L7builderS1538;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L8_2afieldS3831 =
        _M0L4selfS1517->$1;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L7choicesS3412 =
        _M0L8_2afieldS3831;
      void* _M0L7_2abindS1539;
      struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L7builderS1536;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L8_2afieldS3829;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L7choicesS3406;
      int32_t _M0L6_2atmpS3405;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L8_2afieldS3827;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L7choicesS3410;
      struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2atmpS3411;
      int32_t _M0L6_2atmpS3413;
      moonbit_incref(_M0L7choicesS3412);
      moonbit_incref(_M0L13choice__chunkS1532);
      #line 201 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0L7_2abindS1539
      = _M0MPC15array5Array3getGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L7choicesS3412, _M0L5indexS1533);
      switch (Moonbit_object_tag(_M0L7_2abindS1539)) {
        case 1: {
          struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some* _M0L7_2aSomeS1540 =
            (struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some*)_M0L7_2abindS1539;
          struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L8_2afieldS3830 =
            _M0L7_2aSomeS1540->$0;
          int32_t _M0L6_2acntS4254 =
            Moonbit_object_header(_M0L7_2aSomeS1540)->rc;
          struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L4_2axS1541;
          if (_M0L6_2acntS4254 > 1) {
            int32_t _M0L11_2anew__cntS4255 = _M0L6_2acntS4254 - 1;
            Moonbit_object_header(_M0L7_2aSomeS1540)->rc
            = _M0L11_2anew__cntS4255;
            if (_M0L8_2afieldS3830) {
              moonbit_incref(_M0L8_2afieldS3830);
            }
          } else if (_M0L6_2acntS4254 == 1) {
            #line 201 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
            moonbit_free(_M0L7_2aSomeS1540);
          }
          _M0L4_2axS1541 = _M0L8_2afieldS3830;
          if (_M0L4_2axS1541 == 0) {
            if (_M0L4_2axS1541) {
              moonbit_decref(_M0L4_2axS1541);
            }
            goto join_1535;
          } else {
            struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L7_2aSomeS1542 =
              _M0L4_2axS1541;
            struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L10_2abuilderS1543 =
              _M0L7_2aSomeS1542;
            _M0L7builderS1538 = _M0L10_2abuilderS1543;
            goto join_1537;
          }
          break;
        }
        default: {
          moonbit_decref(_M0L7_2abindS1539);
          goto join_1535;
          break;
        }
      }
      goto joinlet_4426;
      join_1537:;
      _M0L7builderS1534 = _M0L7builderS1538;
      joinlet_4426:;
      goto joinlet_4425;
      join_1535:;
      #line 204 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0L7builderS1536
      = _M0MP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder3new(_M0L5indexS1533);
      _M0L8_2afieldS3829 = _M0L4selfS1517->$1;
      _M0L7choicesS3406 = _M0L8_2afieldS3829;
      moonbit_incref(_M0L7choicesS3406);
      #line 205 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0L6_2atmpS3405
      = _M0MPC15array5Array6lengthGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L7choicesS3406);
      if (_M0L5indexS1533 >= _M0L6_2atmpS3405) {
        struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L8_2afieldS3828 =
          _M0L4selfS1517->$1;
        struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L7choicesS3407 =
          _M0L8_2afieldS3828;
        int32_t _M0L6_2atmpS3408 = _M0L5indexS1533 + 1;
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2atmpS3409 =
          0;
        moonbit_incref(_M0L7choicesS3407);
        #line 206 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
        _M0MPC15array5Array6resizeGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L7choicesS3407, _M0L6_2atmpS3408, _M0L6_2atmpS3409);
      }
      _M0L8_2afieldS3827 = _M0L4selfS1517->$1;
      _M0L7choicesS3410 = _M0L8_2afieldS3827;
      moonbit_incref(_M0L7builderS1536);
      _M0L6_2atmpS3411 = _M0L7builderS1536;
      moonbit_incref(_M0L7choicesS3410);
      #line 208 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0MPC15array5Array3setGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L7choicesS3410, _M0L5indexS1533, _M0L6_2atmpS3411);
      _M0L7builderS1534 = _M0L7builderS1536;
      joinlet_4425:;
      #line 211 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0MP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder10add__chunk(_M0L7builderS1534, _M0L13choice__chunkS1532);
      _M0L6_2atmpS3413 = _M0L2__S1531 + 1;
      _M0L2__S1531 = _M0L6_2atmpS3413;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1529);
      moonbit_decref(_M0L4selfS1517);
    }
    break;
  }
  return 0;
}

struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0MP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder3new(
  int32_t _M0L5indexS1515
) {
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* _M0L6_2atmpS3394;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _block_4427;
  #line 117 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  #line 121 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3394
  = _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder3new();
  _block_4427
  = (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder));
  Moonbit_object_header(_block_4427)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder, $2) >> 2, 1, 0);
  _block_4427->$0 = 4294967296ll;
  _block_4427->$1 = _M0L5indexS1515;
  _block_4427->$2 = _M0L6_2atmpS3394;
  return _block_4427;
}

int32_t _M0MP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder10add__chunk(
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L4selfS1510,
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice* _M0L5chunkS1512
) {
  int32_t _M0L6reasonS1509;
  int64_t _M0L7_2abindS1511;
  int64_t _M0L6_2atmpS3391;
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* _M0L8_2afieldS3862;
  int32_t _M0L6_2acntS4256;
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* _M0L7messageS3392;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L8_2afieldS3861;
  int32_t _M0L6_2acntS4258;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L5deltaS3393;
  #line 126 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L7_2abindS1511 = _M0L5chunkS1512->$2;
  if (_M0L7_2abindS1511 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1513 = _M0L7_2abindS1511;
    int32_t _M0L9_2areasonS1514 = (int32_t)_M0L7_2aSomeS1513;
    _M0L6reasonS1509 = _M0L9_2areasonS1514;
    goto join_1508;
  }
  goto joinlet_4428;
  join_1508:;
  _M0L6_2atmpS3391 = (int64_t)_M0L6reasonS1509;
  _M0L4selfS1510->$0 = _M0L6_2atmpS3391;
  joinlet_4428:;
  _M0L8_2afieldS3862 = _M0L4selfS1510->$2;
  _M0L6_2acntS4256 = Moonbit_object_header(_M0L4selfS1510)->rc;
  if (_M0L6_2acntS4256 > 1) {
    int32_t _M0L11_2anew__cntS4257 = _M0L6_2acntS4256 - 1;
    Moonbit_object_header(_M0L4selfS1510)->rc = _M0L11_2anew__cntS4257;
    moonbit_incref(_M0L8_2afieldS3862);
  } else if (_M0L6_2acntS4256 == 1) {
    #line 133 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    moonbit_free(_M0L4selfS1510);
  }
  _M0L7messageS3392 = _M0L8_2afieldS3862;
  _M0L8_2afieldS3861 = _M0L5chunkS1512->$1;
  _M0L6_2acntS4258 = Moonbit_object_header(_M0L5chunkS1512)->rc;
  if (_M0L6_2acntS4258 > 1) {
    int32_t _M0L11_2anew__cntS4259 = _M0L6_2acntS4258 - 1;
    Moonbit_object_header(_M0L5chunkS1512)->rc = _M0L11_2anew__cntS4259;
    moonbit_incref(_M0L8_2afieldS3861);
  } else if (_M0L6_2acntS4258 == 1) {
    #line 133 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    moonbit_free(_M0L5chunkS1512);
  }
  _M0L5deltaS3393 = _M0L8_2afieldS3861;
  #line 133 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder10add__delta(_M0L7messageS3392, _M0L5deltaS3393);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai49____test__6275696c6465725f7762746573742e6d6274__1(
  
) {
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* _M0L7builderS1503;
  moonbit_string_t _M0L6_2atmpS3303;
  moonbit_string_t _M0L6_2atmpS3304;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3305;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L6_2atmpS3302;
  moonbit_string_t _M0L6_2atmpS3307;
  moonbit_string_t _M0L6_2atmpS3308;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3309;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L6_2atmpS3306;
  moonbit_string_t _M0L6_2atmpS3311;
  moonbit_string_t _M0L6_2atmpS3312;
  moonbit_string_t _M0L6_2atmpS3317;
  moonbit_string_t _M0L6_2atmpS3319;
  moonbit_string_t _M0L6_2atmpS3320;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L6_2atmpS3318;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L6_2atmpS3316;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall** _M0L6_2atmpS3315;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3314;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3313;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L6_2atmpS3310;
  moonbit_string_t _M0L6_2atmpS3322;
  moonbit_string_t _M0L6_2atmpS3323;
  moonbit_string_t _M0L6_2atmpS3328;
  moonbit_string_t _M0L6_2atmpS3330;
  moonbit_string_t _M0L6_2atmpS3331;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L6_2atmpS3329;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L6_2atmpS3327;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall** _M0L6_2atmpS3326;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3325;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3324;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L6_2atmpS3321;
  moonbit_string_t _M0L6_2atmpS3333;
  moonbit_string_t _M0L6_2atmpS3334;
  moonbit_string_t _M0L6_2atmpS3339;
  moonbit_string_t _M0L6_2atmpS3341;
  moonbit_string_t _M0L6_2atmpS3342;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L6_2atmpS3340;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L6_2atmpS3338;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall** _M0L6_2atmpS3337;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3336;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3335;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L6_2atmpS3332;
  moonbit_string_t _M0L6_2atmpS3344;
  moonbit_string_t _M0L6_2atmpS3345;
  moonbit_string_t _M0L6_2atmpS3350;
  moonbit_string_t _M0L6_2atmpS3352;
  moonbit_string_t _M0L6_2atmpS3353;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L6_2atmpS3351;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L6_2atmpS3349;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall** _M0L6_2atmpS3348;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3347;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L6_2atmpS3346;
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L6_2atmpS3343;
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0L7messageS1504;
  struct _M0TPB6ToJson _M0L6_2atmpS3354;
  void* _M0L6_2atmpS3390;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3366;
  void* _M0L6_2atmpS3389;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3367;
  void* _M0L6_2atmpS3388;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3376;
  void* _M0L6_2atmpS3387;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3384;
  void* _M0L6_2atmpS3386;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3385;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1507;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3383;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3382;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3381;
  void* _M0L6_2atmpS3380;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3377;
  void* _M0L6_2atmpS3379;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3378;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1506;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3375;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3374;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3373;
  void* _M0L6_2atmpS3372;
  void** _M0L6_2atmpS3371;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS3370;
  void* _M0L6_2atmpS3369;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3368;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1505;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3365;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3364;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3363;
  void* _M0L6_2atmpS3362;
  void* _M0L6_2atmpS3355;
  moonbit_string_t _M0L6_2atmpS3358;
  moonbit_string_t _M0L6_2atmpS3359;
  moonbit_string_t _M0L6_2atmpS3360;
  moonbit_string_t _M0L6_2atmpS3361;
  moonbit_string_t* _M0L6_2atmpS3357;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3356;
  #line 49 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  #line 50 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L7builderS1503
  = _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder3new();
  _M0L6_2atmpS3303 = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L6_2atmpS3304 = 0;
  _M0L6_2atmpS3305 = 0;
  #line 51 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3302
  = _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(_M0L6_2atmpS3303, _M0L6_2atmpS3304, 4294967296ll, _M0L6_2atmpS3305);
  moonbit_incref(_M0L7builderS1503);
  #line 51 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder10add__delta(_M0L7builderS1503, _M0L6_2atmpS3302);
  _M0L6_2atmpS3307 = (moonbit_string_t)moonbit_string_literal_48.data;
  _M0L6_2atmpS3308 = 0;
  _M0L6_2atmpS3309 = 0;
  #line 52 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3306
  = _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(_M0L6_2atmpS3307, _M0L6_2atmpS3308, 4294967296ll, _M0L6_2atmpS3309);
  moonbit_incref(_M0L7builderS1503);
  #line 52 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder10add__delta(_M0L7builderS1503, _M0L6_2atmpS3306);
  _M0L6_2atmpS3311 = 0;
  _M0L6_2atmpS3312 = 0;
  _M0L6_2atmpS3317 = (moonbit_string_t)moonbit_string_literal_14.data;
  _M0L6_2atmpS3319 = 0;
  _M0L6_2atmpS3320 = (moonbit_string_t)moonbit_string_literal_15.data;
  #line 58 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3318
  = _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(_M0L6_2atmpS3319, _M0L6_2atmpS3320);
  #line 55 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3316
  = _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(0, _M0L6_2atmpS3317, _M0L6_2atmpS3318);
  _M0L6_2atmpS3315
  = (struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3315[0] = _M0L6_2atmpS3316;
  _M0L6_2atmpS3314
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE));
  Moonbit_object_header(_M0L6_2atmpS3314)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3314->$0 = _M0L6_2atmpS3315;
  _M0L6_2atmpS3314->$1 = 1;
  _M0L6_2atmpS3313 = _M0L6_2atmpS3314;
  #line 54 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3310
  = _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(_M0L6_2atmpS3311, _M0L6_2atmpS3312, 4294967296ll, _M0L6_2atmpS3313);
  moonbit_incref(_M0L7builderS1503);
  #line 53 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder10add__delta(_M0L7builderS1503, _M0L6_2atmpS3310);
  _M0L6_2atmpS3322 = 0;
  _M0L6_2atmpS3323 = 0;
  _M0L6_2atmpS3328 = 0;
  _M0L6_2atmpS3330 = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L6_2atmpS3331 = 0;
  #line 66 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3329
  = _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(_M0L6_2atmpS3330, _M0L6_2atmpS3331);
  #line 64 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3327
  = _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(0, _M0L6_2atmpS3328, _M0L6_2atmpS3329);
  _M0L6_2atmpS3326
  = (struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3326[0] = _M0L6_2atmpS3327;
  _M0L6_2atmpS3325
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE));
  Moonbit_object_header(_M0L6_2atmpS3325)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3325->$0 = _M0L6_2atmpS3326;
  _M0L6_2atmpS3325->$1 = 1;
  _M0L6_2atmpS3324 = _M0L6_2atmpS3325;
  #line 63 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3321
  = _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(_M0L6_2atmpS3322, _M0L6_2atmpS3323, 4294967296ll, _M0L6_2atmpS3324);
  moonbit_incref(_M0L7builderS1503);
  #line 62 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder10add__delta(_M0L7builderS1503, _M0L6_2atmpS3321);
  _M0L6_2atmpS3333 = 0;
  _M0L6_2atmpS3334 = 0;
  _M0L6_2atmpS3339 = 0;
  _M0L6_2atmpS3341 = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS3342 = 0;
  #line 78 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3340
  = _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(_M0L6_2atmpS3341, _M0L6_2atmpS3342);
  #line 76 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3338
  = _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(0, _M0L6_2atmpS3339, _M0L6_2atmpS3340);
  _M0L6_2atmpS3337
  = (struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3337[0] = _M0L6_2atmpS3338;
  _M0L6_2atmpS3336
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE));
  Moonbit_object_header(_M0L6_2atmpS3336)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3336->$0 = _M0L6_2atmpS3337;
  _M0L6_2atmpS3336->$1 = 1;
  _M0L6_2atmpS3335 = _M0L6_2atmpS3336;
  #line 75 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3332
  = _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(_M0L6_2atmpS3333, _M0L6_2atmpS3334, 4294967296ll, _M0L6_2atmpS3335);
  moonbit_incref(_M0L7builderS1503);
  #line 74 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder10add__delta(_M0L7builderS1503, _M0L6_2atmpS3332);
  _M0L6_2atmpS3344 = 0;
  _M0L6_2atmpS3345 = 0;
  _M0L6_2atmpS3350 = 0;
  _M0L6_2atmpS3352 = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS3353 = 0;
  #line 90 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3351
  = _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(_M0L6_2atmpS3352, _M0L6_2atmpS3353);
  #line 88 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3349
  = _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(0, _M0L6_2atmpS3350, _M0L6_2atmpS3351);
  _M0L6_2atmpS3348
  = (struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3348[0] = _M0L6_2atmpS3349;
  _M0L6_2atmpS3347
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE));
  Moonbit_object_header(_M0L6_2atmpS3347)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3347->$0 = _M0L6_2atmpS3348;
  _M0L6_2atmpS3347->$1 = 1;
  _M0L6_2atmpS3346 = _M0L6_2atmpS3347;
  #line 87 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3343
  = _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(_M0L6_2atmpS3344, _M0L6_2atmpS3345, 4294967296ll, _M0L6_2atmpS3346);
  moonbit_incref(_M0L7builderS1503);
  #line 86 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder10add__delta(_M0L7builderS1503, _M0L6_2atmpS3343);
  #line 98 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L7messageS1504
  = _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__message(_M0L7builderS1503);
  _M0L6_2atmpS3354
  = (struct _M0TPB6ToJson){
    _M0FP0138clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessage_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L7messageS1504
  };
  #line 100 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3390
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_52.data);
  _M0L8_2atupleS3366
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3366)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3366->$0 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L8_2atupleS3366->$1 = _M0L6_2atmpS3390;
  #line 101 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3389
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
  _M0L8_2atupleS3367
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3367)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3367->$0 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L8_2atupleS3367->$1 = _M0L6_2atmpS3389;
  #line 104 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3388
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_14.data);
  _M0L8_2atupleS3376
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3376)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3376->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3376->$1 = _M0L6_2atmpS3388;
  #line 106 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3387
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_15.data);
  _M0L8_2atupleS3384
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3384)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3384->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3384->$1 = _M0L6_2atmpS3387;
  #line 107 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3386
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3385
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3385)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3385->$0 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L8_2atupleS3385->$1 = _M0L6_2atmpS3386;
  _M0L7_2abindS1507 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1507[0] = _M0L8_2atupleS3384;
  _M0L7_2abindS1507[1] = _M0L8_2atupleS3385;
  _M0L6_2atmpS3383 = _M0L7_2abindS1507;
  _M0L6_2atmpS3382
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3383
  };
  #line 105 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3381 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3382);
  #line 105 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3380 = _M0MPC14json4Json6object(_M0L6_2atmpS3381);
  _M0L8_2atupleS3377
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3377)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3377->$0 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L8_2atupleS3377->$1 = _M0L6_2atmpS3380;
  #line 109 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3379
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_27.data);
  _M0L8_2atupleS3378
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3378)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3378->$0 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L8_2atupleS3378->$1 = _M0L6_2atmpS3379;
  _M0L7_2abindS1506 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1506[0] = _M0L8_2atupleS3376;
  _M0L7_2abindS1506[1] = _M0L8_2atupleS3377;
  _M0L7_2abindS1506[2] = _M0L8_2atupleS3378;
  _M0L6_2atmpS3375 = _M0L7_2abindS1506;
  _M0L6_2atmpS3374
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3375
  };
  #line 103 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3373 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3374);
  #line 103 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3372 = _M0MPC14json4Json6object(_M0L6_2atmpS3373);
  _M0L6_2atmpS3371 = (void**)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS3371[0] = _M0L6_2atmpS3372;
  _M0L6_2atmpS3370
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_M0L6_2atmpS3370)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3370->$0 = _M0L6_2atmpS3371;
  _M0L6_2atmpS3370->$1 = 1;
  #line 102 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3369 = _M0MPC14json4Json5array(_M0L6_2atmpS3370);
  _M0L8_2atupleS3368
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3368)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3368->$0 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L8_2atupleS3368->$1 = _M0L6_2atmpS3369;
  _M0L7_2abindS1505 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1505[0] = _M0L8_2atupleS3366;
  _M0L7_2abindS1505[1] = _M0L8_2atupleS3367;
  _M0L7_2abindS1505[2] = _M0L8_2atupleS3368;
  _M0L6_2atmpS3365 = _M0L7_2abindS1505;
  _M0L6_2atmpS3364
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3365
  };
  #line 99 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3363 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3364);
  #line 99 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3362 = _M0MPC14json4Json6object(_M0L6_2atmpS3363);
  _M0L6_2atmpS3355 = _M0L6_2atmpS3362;
  _M0L6_2atmpS3358 = (moonbit_string_t)moonbit_string_literal_53.data;
  _M0L6_2atmpS3359 = (moonbit_string_t)moonbit_string_literal_54.data;
  _M0L6_2atmpS3360 = 0;
  _M0L6_2atmpS3361 = 0;
  _M0L6_2atmpS3357 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3357[0] = _M0L6_2atmpS3358;
  _M0L6_2atmpS3357[1] = _M0L6_2atmpS3359;
  _M0L6_2atmpS3357[2] = _M0L6_2atmpS3360;
  _M0L6_2atmpS3357[3] = _M0L6_2atmpS3361;
  _M0L6_2atmpS3356
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3356)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3356->$0 = _M0L6_2atmpS3357;
  _M0L6_2atmpS3356->$1 = 4;
  #line 99 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3354, _M0L6_2atmpS3355, (moonbit_string_t)moonbit_string_literal_55.data, _M0L6_2atmpS3356);
}

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__message(
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* _M0L4selfS1491
) {
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L6_2atmpS3301;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS1489;
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L8_2afieldS3867;
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L7_2abindS1490;
  int32_t _M0L7_2abindS1492;
  int32_t _M0L2__S1493;
  struct _M0TPB13StringBuilder* _M0L8_2afieldS3864;
  struct _M0TPB13StringBuilder* _M0L7contentS3297;
  struct _M0TWRPB13StringBuilderEs* _M0L6_2atmpS3298;
  moonbit_string_t _M0L6_2atmpS3290;
  struct _M0TPB13StringBuilder* _M0L8_2afieldS3863;
  int32_t _M0L6_2acntS4260;
  struct _M0TPB13StringBuilder* _M0L7refusalS3293;
  struct _M0TWRPB13StringBuilderEs* _M0L6_2atmpS3294;
  moonbit_string_t _M0L6_2atmpS3291;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L6_2atmpS3292;
  #line 94 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3301
  = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall**)moonbit_empty_ref_array;
  _M0L11tool__callsS1489
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE));
  Moonbit_object_header(_M0L11tool__callsS1489)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE, $0) >> 2, 1, 0);
  _M0L11tool__callsS1489->$0 = _M0L6_2atmpS3301;
  _M0L11tool__callsS1489->$1 = 0;
  _M0L8_2afieldS3867 = _M0L4selfS1491->$2;
  _M0L7_2abindS1490 = _M0L8_2afieldS3867;
  _M0L7_2abindS1492 = _M0L7_2abindS1490->$1;
  moonbit_incref(_M0L7_2abindS1490);
  _M0L2__S1493 = 0;
  while (1) {
    if (_M0L2__S1493 < _M0L7_2abindS1492) {
      struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L8_2afieldS3866 =
        _M0L7_2abindS1490->$0;
      struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L3bufS3289 =
        _M0L8_2afieldS3866;
      struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2atmpS3865 =
        (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*)_M0L3bufS3289[
          _M0L2__S1493
        ];
      struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L7builderS1494 =
        _M0L6_2atmpS3865;
      struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L7builderS1498;
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2atmpS3288;
      int32_t _M0L6_2atmpS3287;
      if (_M0L7builderS1494 == 0) {
        goto join_1495;
      } else {
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L7_2aSomeS1499 =
          _M0L7builderS1494;
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L10_2abuilderS1500 =
          _M0L7_2aSomeS1499;
        moonbit_incref(_M0L10_2abuilderS1500);
        _M0L7builderS1498 = _M0L10_2abuilderS1500;
        goto join_1497;
      }
      goto joinlet_4431;
      join_1497:;
      #line 100 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0L6_2atmpS3288
      = _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder14to__tool__call(_M0L7builderS1498);
      moonbit_incref(_M0L11tool__callsS1489);
      #line 100 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L11tool__callsS1489, _M0L6_2atmpS3288);
      joinlet_4431:;
      goto join_1495;
      goto joinlet_4430;
      join_1495:;
      _M0L6_2atmpS3287 = _M0L2__S1493 + 1;
      _M0L2__S1493 = _M0L6_2atmpS3287;
      continue;
      joinlet_4430:;
    } else {
      moonbit_decref(_M0L7_2abindS1490);
    }
    break;
  }
  _M0L8_2afieldS3864 = _M0L4selfS1491->$0;
  _M0L7contentS3297 = _M0L8_2afieldS3864;
  _M0L6_2atmpS3298
  = (struct _M0TWRPB13StringBuilderEs*)&_M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__messageC3299l103$closure.data;
  if (_M0L7contentS3297) {
    moonbit_incref(_M0L7contentS3297);
  }
  #line 103 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3290
  = _M0MPC16option6Option3mapGRPB13StringBuildersE(_M0L7contentS3297, _M0L6_2atmpS3298);
  _M0L8_2afieldS3863 = _M0L4selfS1491->$1;
  _M0L6_2acntS4260 = Moonbit_object_header(_M0L4selfS1491)->rc;
  if (_M0L6_2acntS4260 > 1) {
    int32_t _M0L11_2anew__cntS4263 = _M0L6_2acntS4260 - 1;
    Moonbit_object_header(_M0L4selfS1491)->rc = _M0L11_2anew__cntS4263;
    if (_M0L8_2afieldS3863) {
      moonbit_incref(_M0L8_2afieldS3863);
    }
  } else if (_M0L6_2acntS4260 == 1) {
    struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L8_2afieldS4262 =
      _M0L4selfS1491->$2;
    struct _M0TPB13StringBuilder* _M0L8_2afieldS4261;
    moonbit_decref(_M0L8_2afieldS4262);
    _M0L8_2afieldS4261 = _M0L4selfS1491->$0;
    if (_M0L8_2afieldS4261) {
      moonbit_decref(_M0L8_2afieldS4261);
    }
    #line 104 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    moonbit_free(_M0L4selfS1491);
  }
  _M0L7refusalS3293 = _M0L8_2afieldS3863;
  _M0L6_2atmpS3294
  = (struct _M0TWRPB13StringBuilderEs*)&_M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__messageC3295l104$closure.data;
  #line 104 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3291
  = _M0MPC16option6Option3mapGRPB13StringBuildersE(_M0L7refusalS3293, _M0L6_2atmpS3294);
  _M0L6_2atmpS3292 = _M0L11tool__callsS1489;
  #line 102 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  return _M0MP48clawteam8clawteam8internal6openai21ChatCompletionMessage3new(_M0L6_2atmpS3290, _M0L6_2atmpS3291, 4294967296ll, _M0L6_2atmpS3292);
}

moonbit_string_t _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__messageC3299l103(
  struct _M0TWRPB13StringBuilderEs* _M0L6_2aenvS3300,
  struct _M0TPB13StringBuilder* _M0L1bS1501
) {
  #line 103 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  moonbit_decref(_M0L6_2aenvS3300);
  #line 103 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L1bS1501);
}

moonbit_string_t _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder11to__messageC3295l104(
  struct _M0TWRPB13StringBuilderEs* _M0L6_2aenvS3296,
  struct _M0TPB13StringBuilder* _M0L1bS1502
) {
  #line 104 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  moonbit_decref(_M0L6_2aenvS3296);
  #line 104 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L1bS1502);
}

struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder3new(
  
) {
  struct _M0TPB13StringBuilder* _M0L6_2atmpS3283;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS3284;
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L6_2atmpS3286;
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L6_2atmpS3285;
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* _block_4432;
  #line 45 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3283 = 0;
  _M0L6_2atmpS3284 = 0;
  _M0L6_2atmpS3286
  = (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder**)moonbit_empty_ref_array;
  _M0L6_2atmpS3285
  = (struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE));
  Moonbit_object_header(_M0L6_2atmpS3285)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3285->$0 = _M0L6_2atmpS3286;
  _M0L6_2atmpS3285->$1 = 0;
  _block_4432
  = (struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder));
  Moonbit_object_header(_block_4432)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder, $0) >> 2, 3, 0);
  _block_4432->$0 = _M0L6_2atmpS3283;
  _block_4432->$1 = _M0L6_2atmpS3284;
  _block_4432->$2 = _M0L6_2atmpS3285;
  return _block_4432;
}

int32_t _M0MP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder10add__delta(
  struct _M0TP48clawteam8clawteam8internal6openai28ChatCompletionMessageBuilder* _M0L4selfS1451,
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L5deltaS1456
) {
  moonbit_string_t _M0L7contentS1447;
  moonbit_string_t _M0L8_2afieldS3884;
  moonbit_string_t _M0L7_2abindS1455;
  struct _M0TPB13StringBuilder* _M0L7builderS1449;
  struct _M0TPB13StringBuilder* _M0L8_2afieldS3883;
  struct _M0TPB13StringBuilder* _M0L7_2abindS1450;
  moonbit_string_t _M0L7refusalS1460;
  moonbit_string_t _M0L8_2afieldS3881;
  moonbit_string_t _M0L7_2abindS1467;
  struct _M0TPB13StringBuilder* _M0L7builderS1462;
  struct _M0TPB13StringBuilder* _M0L8_2afieldS3880;
  struct _M0TPB13StringBuilder* _M0L7_2abindS1463;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L11tool__callsS1471;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L8_2afieldS3878;
  int32_t _M0L6_2acntS4266;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L7_2abindS1486;
  int32_t _M0L7_2abindS1472;
  int32_t _M0L2__S1473;
  #line 50 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L8_2afieldS3884 = _M0L5deltaS1456->$0;
  _M0L7_2abindS1455 = _M0L8_2afieldS3884;
  if (_M0L7_2abindS1455 == 0) {
    
  } else {
    moonbit_string_t _M0L7_2aSomeS1457 = _M0L7_2abindS1455;
    moonbit_string_t _M0L10_2acontentS1458 = _M0L7_2aSomeS1457;
    moonbit_incref(_M0L10_2acontentS1458);
    _M0L7contentS1447 = _M0L10_2acontentS1458;
    goto join_1446;
  }
  goto joinlet_4433;
  join_1446:;
  _M0L8_2afieldS3883 = _M0L4selfS1451->$0;
  _M0L7_2abindS1450 = _M0L8_2afieldS3883;
  if (_M0L7_2abindS1450 == 0) {
    struct _M0TPB13StringBuilder* _M0L7builderS1454;
    struct _M0TPB13StringBuilder* _M0L6_2atmpS3262;
    struct _M0TPB13StringBuilder* _M0L6_2aoldS3882;
    #line 58 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    _M0L7builderS1454 = _M0MPB13StringBuilder11new_2einner(0);
    moonbit_incref(_M0L7builderS1454);
    #line 59 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L7builderS1454, _M0L7contentS1447);
    _M0L6_2atmpS3262 = _M0L7builderS1454;
    _M0L6_2aoldS3882 = _M0L4selfS1451->$0;
    if (_M0L6_2aoldS3882) {
      moonbit_decref(_M0L6_2aoldS3882);
    }
    _M0L4selfS1451->$0 = _M0L6_2atmpS3262;
  } else {
    struct _M0TPB13StringBuilder* _M0L7_2aSomeS1452 = _M0L7_2abindS1450;
    struct _M0TPB13StringBuilder* _M0L10_2abuilderS1453 = _M0L7_2aSomeS1452;
    moonbit_incref(_M0L10_2abuilderS1453);
    _M0L7builderS1449 = _M0L10_2abuilderS1453;
    goto join_1448;
  }
  goto joinlet_4434;
  join_1448:;
  #line 56 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7builderS1449, _M0L7contentS1447);
  joinlet_4434:;
  joinlet_4433:;
  _M0L8_2afieldS3881 = _M0L5deltaS1456->$1;
  _M0L7_2abindS1467 = _M0L8_2afieldS3881;
  if (_M0L7_2abindS1467 == 0) {
    
  } else {
    moonbit_string_t _M0L7_2aSomeS1468 = _M0L7_2abindS1467;
    moonbit_string_t _M0L10_2arefusalS1469 = _M0L7_2aSomeS1468;
    moonbit_incref(_M0L10_2arefusalS1469);
    _M0L7refusalS1460 = _M0L10_2arefusalS1469;
    goto join_1459;
  }
  goto joinlet_4435;
  join_1459:;
  _M0L8_2afieldS3880 = _M0L4selfS1451->$1;
  _M0L7_2abindS1463 = _M0L8_2afieldS3880;
  if (_M0L7_2abindS1463 == 0) {
    struct _M0TPB13StringBuilder* _M0L7builderS1466;
    struct _M0TPB13StringBuilder* _M0L6_2atmpS3263;
    struct _M0TPB13StringBuilder* _M0L6_2aoldS3879;
    #line 67 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    _M0L7builderS1466 = _M0MPB13StringBuilder11new_2einner(0);
    moonbit_incref(_M0L7builderS1466);
    #line 68 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    _M0IPB13StringBuilderPB6Logger13write__string(_M0L7builderS1466, _M0L7refusalS1460);
    _M0L6_2atmpS3263 = _M0L7builderS1466;
    _M0L6_2aoldS3879 = _M0L4selfS1451->$1;
    if (_M0L6_2aoldS3879) {
      moonbit_decref(_M0L6_2aoldS3879);
    }
    _M0L4selfS1451->$1 = _M0L6_2atmpS3263;
  } else {
    struct _M0TPB13StringBuilder* _M0L7_2aSomeS1464 = _M0L7_2abindS1463;
    struct _M0TPB13StringBuilder* _M0L10_2abuilderS1465 = _M0L7_2aSomeS1464;
    moonbit_incref(_M0L10_2abuilderS1465);
    _M0L7builderS1462 = _M0L10_2abuilderS1465;
    goto join_1461;
  }
  goto joinlet_4436;
  join_1461:;
  #line 65 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7builderS1462, _M0L7refusalS1460);
  joinlet_4436:;
  joinlet_4435:;
  _M0L8_2afieldS3878 = _M0L5deltaS1456->$3;
  _M0L6_2acntS4266 = Moonbit_object_header(_M0L5deltaS1456)->rc;
  if (_M0L6_2acntS4266 > 1) {
    int32_t _M0L11_2anew__cntS4269 = _M0L6_2acntS4266 - 1;
    Moonbit_object_header(_M0L5deltaS1456)->rc = _M0L11_2anew__cntS4269;
    if (_M0L8_2afieldS3878) {
      moonbit_incref(_M0L8_2afieldS3878);
    }
  } else if (_M0L6_2acntS4266 == 1) {
    moonbit_string_t _M0L8_2afieldS4268 = _M0L5deltaS1456->$1;
    moonbit_string_t _M0L8_2afieldS4267;
    if (_M0L8_2afieldS4268) {
      moonbit_decref(_M0L8_2afieldS4268);
    }
    _M0L8_2afieldS4267 = _M0L5deltaS1456->$0;
    if (_M0L8_2afieldS4267) {
      moonbit_decref(_M0L8_2afieldS4267);
    }
    #line 72 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    moonbit_free(_M0L5deltaS1456);
  }
  _M0L7_2abindS1486 = _M0L8_2afieldS3878;
  if (_M0L7_2abindS1486 == 0) {
    if (_M0L7_2abindS1486) {
      moonbit_decref(_M0L7_2abindS1486);
    }
    moonbit_decref(_M0L4selfS1451);
  } else {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L7_2aSomeS1487 =
      _M0L7_2abindS1486;
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L14_2atool__callsS1488 =
      _M0L7_2aSomeS1487;
    _M0L11tool__callsS1471 = _M0L14_2atool__callsS1488;
    goto join_1470;
  }
  goto joinlet_4437;
  join_1470:;
  _M0L7_2abindS1472 = _M0L11tool__callsS1471->$1;
  _M0L2__S1473 = 0;
  while (1) {
    if (_M0L2__S1473 < _M0L7_2abindS1472) {
      struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall** _M0L8_2afieldS3877 =
        _M0L11tool__callsS1471->$0;
      struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall** _M0L3bufS3282 =
        _M0L8_2afieldS3877;
      struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L6_2atmpS3876 =
        (struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall*)_M0L3bufS3282[
          _M0L2__S1473
        ];
      struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L10tool__callS1474 =
        _M0L6_2atmpS3876;
      struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L7builderS1475;
      struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L7builderS1479;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L8_2afieldS3875 =
        _M0L4selfS1451->$2;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L11tool__callsS3279 =
        _M0L8_2afieldS3875;
      int32_t _M0L5indexS3280 = _M0L10tool__callS1474->$0;
      void* _M0L7_2abindS1480;
      moonbit_string_t _M0L8_2afieldS3873;
      moonbit_string_t _M0L2idS3278;
      moonbit_string_t _M0L6_2atmpS3274;
      struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L8_2afieldS3872;
      struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L8functionS3277;
      moonbit_string_t _M0L8_2afieldS3871;
      moonbit_string_t _M0L4nameS3276;
      moonbit_string_t _M0L6_2atmpS3275;
      struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L7builderS1477;
      int32_t _M0L5indexS3264;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L8_2afieldS3870;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L11tool__callsS3266;
      int32_t _M0L6_2atmpS3265;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L8_2afieldS3868;
      struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L11tool__callsS3271;
      int32_t _M0L5indexS3272;
      struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2atmpS3273;
      int32_t _M0L6_2atmpS3281;
      moonbit_incref(_M0L11tool__callsS3279);
      moonbit_incref(_M0L10tool__callS1474);
      #line 74 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0L7_2abindS1480
      = _M0MPC15array5Array3getGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(_M0L11tool__callsS3279, _M0L5indexS3280);
      switch (Moonbit_object_tag(_M0L7_2abindS1480)) {
        case 1: {
          struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE4Some* _M0L7_2aSomeS1481 =
            (struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE4Some*)_M0L7_2abindS1480;
          struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L8_2afieldS3874 =
            _M0L7_2aSomeS1481->$0;
          int32_t _M0L6_2acntS4264 =
            Moonbit_object_header(_M0L7_2aSomeS1481)->rc;
          struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L4_2axS1482;
          if (_M0L6_2acntS4264 > 1) {
            int32_t _M0L11_2anew__cntS4265 = _M0L6_2acntS4264 - 1;
            Moonbit_object_header(_M0L7_2aSomeS1481)->rc
            = _M0L11_2anew__cntS4265;
            if (_M0L8_2afieldS3874) {
              moonbit_incref(_M0L8_2afieldS3874);
            }
          } else if (_M0L6_2acntS4264 == 1) {
            #line 74 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
            moonbit_free(_M0L7_2aSomeS1481);
          }
          _M0L4_2axS1482 = _M0L8_2afieldS3874;
          if (_M0L4_2axS1482 == 0) {
            if (_M0L4_2axS1482) {
              moonbit_decref(_M0L4_2axS1482);
            }
            goto join_1476;
          } else {
            struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L7_2aSomeS1483 =
              _M0L4_2axS1482;
            struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L10_2abuilderS1484 =
              _M0L7_2aSomeS1483;
            _M0L7builderS1479 = _M0L10_2abuilderS1484;
            goto join_1478;
          }
          break;
        }
        default: {
          moonbit_decref(_M0L7_2abindS1480);
          goto join_1476;
          break;
        }
      }
      goto joinlet_4440;
      join_1478:;
      _M0L7builderS1475 = _M0L7builderS1479;
      joinlet_4440:;
      goto joinlet_4439;
      join_1476:;
      _M0L8_2afieldS3873 = _M0L10tool__callS1474->$1;
      _M0L2idS3278 = _M0L8_2afieldS3873;
      if (_M0L2idS3278) {
        moonbit_incref(_M0L2idS3278);
      }
      #line 79 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0L6_2atmpS3274 = _M0MPC16option6Option6unwrapGsE(_M0L2idS3278);
      _M0L8_2afieldS3872 = _M0L10tool__callS1474->$2;
      _M0L8functionS3277 = _M0L8_2afieldS3872;
      _M0L8_2afieldS3871 = _M0L8functionS3277->$1;
      _M0L4nameS3276 = _M0L8_2afieldS3871;
      if (_M0L4nameS3276) {
        moonbit_incref(_M0L4nameS3276);
      }
      #line 80 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0L6_2atmpS3275 = _M0MPC16option6Option6unwrapGsE(_M0L4nameS3276);
      #line 78 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0L7builderS1477
      = _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder3new(_M0L6_2atmpS3274, _M0L6_2atmpS3275);
      _M0L5indexS3264 = _M0L10tool__callS1474->$0;
      _M0L8_2afieldS3870 = _M0L4selfS1451->$2;
      _M0L11tool__callsS3266 = _M0L8_2afieldS3870;
      moonbit_incref(_M0L11tool__callsS3266);
      #line 82 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0L6_2atmpS3265
      = _M0MPC15array5Array6lengthGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(_M0L11tool__callsS3266);
      if (_M0L5indexS3264 >= _M0L6_2atmpS3265) {
        struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L8_2afieldS3869 =
          _M0L4selfS1451->$2;
        struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L11tool__callsS3267 =
          _M0L8_2afieldS3869;
        int32_t _M0L5indexS3270 = _M0L10tool__callS1474->$0;
        int32_t _M0L6_2atmpS3268 = _M0L5indexS3270 + 1;
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2atmpS3269 =
          0;
        moonbit_incref(_M0L11tool__callsS3267);
        #line 83 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
        _M0MPC15array5Array6resizeGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(_M0L11tool__callsS3267, _M0L6_2atmpS3268, _M0L6_2atmpS3269);
      }
      _M0L8_2afieldS3868 = _M0L4selfS1451->$2;
      _M0L11tool__callsS3271 = _M0L8_2afieldS3868;
      _M0L5indexS3272 = _M0L10tool__callS1474->$0;
      moonbit_incref(_M0L7builderS1477);
      _M0L6_2atmpS3273 = _M0L7builderS1477;
      moonbit_incref(_M0L11tool__callsS3271);
      #line 85 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0MPC15array5Array3setGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(_M0L11tool__callsS3271, _M0L5indexS3272, _M0L6_2atmpS3273);
      _M0L7builderS1475 = _M0L7builderS1477;
      joinlet_4439:;
      #line 88 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
      _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder10add__delta(_M0L7builderS1475, _M0L10tool__callS1474);
      _M0L6_2atmpS3281 = _M0L2__S1473 + 1;
      _M0L2__S1473 = _M0L6_2atmpS3281;
      continue;
    } else {
      moonbit_decref(_M0L11tool__callsS1471);
      moonbit_decref(_M0L4selfS1451);
    }
    break;
  }
  joinlet_4437:;
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal6openai49____test__6275696c6465725f7762746573742e6d6274__0(
  
) {
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L7builderS1442;
  moonbit_string_t _M0L6_2atmpS3223;
  moonbit_string_t _M0L6_2atmpS3225;
  moonbit_string_t _M0L6_2atmpS3226;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L6_2atmpS3224;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L6_2atmpS3222;
  moonbit_string_t _M0L6_2atmpS3228;
  moonbit_string_t _M0L6_2atmpS3230;
  moonbit_string_t _M0L6_2atmpS3231;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L6_2atmpS3229;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L6_2atmpS3227;
  moonbit_string_t _M0L6_2atmpS3233;
  moonbit_string_t _M0L6_2atmpS3235;
  moonbit_string_t _M0L6_2atmpS3236;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L6_2atmpS3234;
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L6_2atmpS3232;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L10tool__callS1443;
  struct _M0TPB6ToJson _M0L6_2atmpS3237;
  void* _M0L6_2atmpS3261;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3249;
  void* _M0L6_2atmpS3260;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3257;
  void* _M0L6_2atmpS3259;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3258;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1445;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3256;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3255;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3254;
  void* _M0L6_2atmpS3253;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3250;
  void* _M0L6_2atmpS3252;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3251;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1444;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3248;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3247;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3246;
  void* _M0L6_2atmpS3245;
  void* _M0L6_2atmpS3238;
  moonbit_string_t _M0L6_2atmpS3241;
  moonbit_string_t _M0L6_2atmpS3242;
  moonbit_string_t _M0L6_2atmpS3243;
  moonbit_string_t _M0L6_2atmpS3244;
  moonbit_string_t* _M0L6_2atmpS3240;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3239;
  #line 2 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L7builderS1442
  = _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder3new((moonbit_string_t)moonbit_string_literal_14.data, (moonbit_string_t)moonbit_string_literal_15.data);
  _M0L6_2atmpS3223 = 0;
  _M0L6_2atmpS3225 = (moonbit_string_t)moonbit_string_literal_49.data;
  _M0L6_2atmpS3226 = 0;
  #line 10 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3224
  = _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(_M0L6_2atmpS3225, _M0L6_2atmpS3226);
  #line 8 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3222
  = _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(0, _M0L6_2atmpS3223, _M0L6_2atmpS3224);
  moonbit_incref(_M0L7builderS1442);
  #line 7 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder10add__delta(_M0L7builderS1442, _M0L6_2atmpS3222);
  _M0L6_2atmpS3228 = 0;
  _M0L6_2atmpS3230 = (moonbit_string_t)moonbit_string_literal_50.data;
  _M0L6_2atmpS3231 = 0;
  #line 20 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3229
  = _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(_M0L6_2atmpS3230, _M0L6_2atmpS3231);
  #line 18 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3227
  = _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(0, _M0L6_2atmpS3228, _M0L6_2atmpS3229);
  moonbit_incref(_M0L7builderS1442);
  #line 17 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder10add__delta(_M0L7builderS1442, _M0L6_2atmpS3227);
  _M0L6_2atmpS3233 = 0;
  _M0L6_2atmpS3235 = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS3236 = 0;
  #line 30 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3234
  = _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(_M0L6_2atmpS3235, _M0L6_2atmpS3236);
  #line 28 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3232
  = _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(0, _M0L6_2atmpS3233, _M0L6_2atmpS3234);
  moonbit_incref(_M0L7builderS1442);
  #line 27 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder10add__delta(_M0L7builderS1442, _M0L6_2atmpS3232);
  #line 37 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L10tool__callS1443
  = _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder14to__tool__call(_M0L7builderS1442);
  _M0L6_2atmpS3237
  = (struct _M0TPB6ToJson){
    _M0FP0146clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionMessageToolCall_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L10tool__callS1443
  };
  #line 39 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3261
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_14.data);
  _M0L8_2atupleS3249
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3249)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3249->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3249->$1 = _M0L6_2atmpS3261;
  #line 41 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3260
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_15.data);
  _M0L8_2atupleS3257
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3257)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3257->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3257->$1 = _M0L6_2atmpS3260;
  #line 42 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3259
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_25.data);
  _M0L8_2atupleS3258
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3258)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3258->$0 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L8_2atupleS3258->$1 = _M0L6_2atmpS3259;
  _M0L7_2abindS1445 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1445[0] = _M0L8_2atupleS3257;
  _M0L7_2abindS1445[1] = _M0L8_2atupleS3258;
  _M0L6_2atmpS3256 = _M0L7_2abindS1445;
  _M0L6_2atmpS3255
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3256
  };
  #line 40 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3254 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3255);
  #line 40 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3253 = _M0MPC14json4Json6object(_M0L6_2atmpS3254);
  _M0L8_2atupleS3250
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3250)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3250->$0 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L8_2atupleS3250->$1 = _M0L6_2atmpS3253;
  #line 44 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3252
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_27.data);
  _M0L8_2atupleS3251
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3251)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3251->$0 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L8_2atupleS3251->$1 = _M0L6_2atmpS3252;
  _M0L7_2abindS1444 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1444[0] = _M0L8_2atupleS3249;
  _M0L7_2abindS1444[1] = _M0L8_2atupleS3250;
  _M0L7_2abindS1444[2] = _M0L8_2atupleS3251;
  _M0L6_2atmpS3248 = _M0L7_2abindS1444;
  _M0L6_2atmpS3247
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3248
  };
  #line 38 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3246 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3247);
  #line 38 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  _M0L6_2atmpS3245 = _M0MPC14json4Json6object(_M0L6_2atmpS3246);
  _M0L6_2atmpS3238 = _M0L6_2atmpS3245;
  _M0L6_2atmpS3241 = (moonbit_string_t)moonbit_string_literal_56.data;
  _M0L6_2atmpS3242 = (moonbit_string_t)moonbit_string_literal_57.data;
  _M0L6_2atmpS3243 = 0;
  _M0L6_2atmpS3244 = 0;
  _M0L6_2atmpS3240 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS3240[0] = _M0L6_2atmpS3241;
  _M0L6_2atmpS3240[1] = _M0L6_2atmpS3242;
  _M0L6_2atmpS3240[2] = _M0L6_2atmpS3243;
  _M0L6_2atmpS3240[3] = _M0L6_2atmpS3244;
  _M0L6_2atmpS3239
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS3239)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS3239->$0 = _M0L6_2atmpS3240;
  _M0L6_2atmpS3239->$1 = 4;
  #line 38 "E:\\moonbit\\clawteam\\internal\\openai\\builder_wbtest.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3237, _M0L6_2atmpS3238, (moonbit_string_t)moonbit_string_literal_58.data, _M0L6_2atmpS3239);
}

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder14to__tool__call(
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L4selfS1441
) {
  moonbit_string_t _M0L8_2afieldS3887;
  moonbit_string_t _M0L2idS3217;
  moonbit_string_t _M0L8_2afieldS3886;
  moonbit_string_t _M0L4nameS3218;
  struct _M0TPB13StringBuilder* _M0L8_2afieldS3885;
  int32_t _M0L6_2acntS4270;
  struct _M0TPB13StringBuilder* _M0L9argumentsS3221;
  moonbit_string_t _M0L6_2atmpS3220;
  moonbit_string_t _M0L6_2atmpS3219;
  #line 31 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L8_2afieldS3887 = _M0L4selfS1441->$0;
  _M0L2idS3217 = _M0L8_2afieldS3887;
  _M0L8_2afieldS3886 = _M0L4selfS1441->$1;
  _M0L4nameS3218 = _M0L8_2afieldS3886;
  _M0L8_2afieldS3885 = _M0L4selfS1441->$2;
  _M0L6_2acntS4270 = Moonbit_object_header(_M0L4selfS1441)->rc;
  if (_M0L6_2acntS4270 > 1) {
    int32_t _M0L11_2anew__cntS4271 = _M0L6_2acntS4270 - 1;
    Moonbit_object_header(_M0L4selfS1441)->rc = _M0L11_2anew__cntS4271;
    moonbit_incref(_M0L8_2afieldS3885);
    moonbit_incref(_M0L4nameS3218);
    moonbit_incref(_M0L2idS3217);
  } else if (_M0L6_2acntS4270 == 1) {
    #line 34 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    moonbit_free(_M0L4selfS1441);
  }
  _M0L9argumentsS3221 = _M0L8_2afieldS3885;
  #line 34 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3220 = _M0MPB13StringBuilder10to__string(_M0L9argumentsS3221);
  _M0L6_2atmpS3219 = _M0L6_2atmpS3220;
  #line 34 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  return _M0FP48clawteam8clawteam8internal6openai10tool__call(_M0L2idS3217, _M0L4nameS3218, _M0L6_2atmpS3219);
}

struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder3new(
  moonbit_string_t _M0L2idS1439,
  moonbit_string_t _M0L4nameS1440
) {
  struct _M0TPB13StringBuilder* _M0L6_2atmpS3216;
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _block_4441;
  #line 9 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  #line 16 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L6_2atmpS3216 = _M0MPB13StringBuilder11new_2einner(0);
  _block_4441
  = (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder));
  Moonbit_object_header(_block_4441)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder, $0) >> 2, 3, 0);
  _block_4441->$0 = _M0L2idS1439;
  _block_4441->$1 = _M0L4nameS1440;
  _block_4441->$2 = _M0L6_2atmpS3216;
  return _block_4441;
}

int32_t _M0MP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder10add__delta(
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L4selfS1434,
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0L5deltaS1436
) {
  moonbit_string_t _M0L9argumentsS1433;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L8_2afieldS3890;
  int32_t _M0L6_2acntS4276;
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L8functionS3215;
  moonbit_string_t _M0L8_2afieldS3889;
  int32_t _M0L6_2acntS4279;
  moonbit_string_t _M0L7_2abindS1435;
  struct _M0TPB13StringBuilder* _M0L8_2afieldS3888;
  int32_t _M0L6_2acntS4272;
  struct _M0TPB13StringBuilder* _M0L9argumentsS3214;
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0L8_2afieldS3890 = _M0L5deltaS1436->$2;
  _M0L6_2acntS4276 = Moonbit_object_header(_M0L5deltaS1436)->rc;
  if (_M0L6_2acntS4276 > 1) {
    int32_t _M0L11_2anew__cntS4278 = _M0L6_2acntS4276 - 1;
    Moonbit_object_header(_M0L5deltaS1436)->rc = _M0L11_2anew__cntS4278;
    moonbit_incref(_M0L8_2afieldS3890);
  } else if (_M0L6_2acntS4276 == 1) {
    moonbit_string_t _M0L8_2afieldS4277 = _M0L5deltaS1436->$1;
    if (_M0L8_2afieldS4277) {
      moonbit_decref(_M0L8_2afieldS4277);
    }
    #line 25 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    moonbit_free(_M0L5deltaS1436);
  }
  _M0L8functionS3215 = _M0L8_2afieldS3890;
  _M0L8_2afieldS3889 = _M0L8functionS3215->$0;
  _M0L6_2acntS4279 = Moonbit_object_header(_M0L8functionS3215)->rc;
  if (_M0L6_2acntS4279 > 1) {
    int32_t _M0L11_2anew__cntS4281 = _M0L6_2acntS4279 - 1;
    Moonbit_object_header(_M0L8functionS3215)->rc = _M0L11_2anew__cntS4281;
    if (_M0L8_2afieldS3889) {
      moonbit_incref(_M0L8_2afieldS3889);
    }
  } else if (_M0L6_2acntS4279 == 1) {
    moonbit_string_t _M0L8_2afieldS4280 = _M0L8functionS3215->$1;
    if (_M0L8_2afieldS4280) {
      moonbit_decref(_M0L8_2afieldS4280);
    }
    #line 25 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    moonbit_free(_M0L8functionS3215);
  }
  _M0L7_2abindS1435 = _M0L8_2afieldS3889;
  if (_M0L7_2abindS1435 == 0) {
    if (_M0L7_2abindS1435) {
      moonbit_decref(_M0L7_2abindS1435);
    }
    moonbit_decref(_M0L4selfS1434);
  } else {
    moonbit_string_t _M0L7_2aSomeS1437 = _M0L7_2abindS1435;
    moonbit_string_t _M0L12_2aargumentsS1438 = _M0L7_2aSomeS1437;
    _M0L9argumentsS1433 = _M0L12_2aargumentsS1438;
    goto join_1432;
  }
  goto joinlet_4442;
  join_1432:;
  _M0L8_2afieldS3888 = _M0L4selfS1434->$2;
  _M0L6_2acntS4272 = Moonbit_object_header(_M0L4selfS1434)->rc;
  if (_M0L6_2acntS4272 > 1) {
    int32_t _M0L11_2anew__cntS4275 = _M0L6_2acntS4272 - 1;
    Moonbit_object_header(_M0L4selfS1434)->rc = _M0L11_2anew__cntS4275;
    moonbit_incref(_M0L8_2afieldS3888);
  } else if (_M0L6_2acntS4272 == 1) {
    moonbit_string_t _M0L8_2afieldS4274 = _M0L4selfS1434->$1;
    moonbit_string_t _M0L8_2afieldS4273;
    moonbit_decref(_M0L8_2afieldS4274);
    _M0L8_2afieldS4273 = _M0L4selfS1434->$0;
    moonbit_decref(_M0L8_2afieldS4273);
    #line 26 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
    moonbit_free(_M0L4selfS1434);
  }
  _M0L9argumentsS3214 = _M0L8_2afieldS3888;
  #line 26 "E:\\moonbit\\clawteam\\internal\\openai\\builder.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L9argumentsS3214, _M0L9argumentsS1433);
  joinlet_4442:;
  return 0;
}

void* _M0IP48clawteam8clawteam8internal6openai14ChatCompletionPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion* _M0L4selfS1416
) {
  moonbit_string_t _M0L8_2afieldS3896;
  moonbit_string_t _M0L2idS3213;
  void* _M0L6_2atmpS3212;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3202;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0L8_2afieldS3895;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0L7choicesS3211;
  void* _M0L6_2atmpS3210;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3203;
  int32_t _M0L7createdS3209;
  void* _M0L6_2atmpS3208;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3204;
  moonbit_string_t _M0L8_2afieldS3894;
  moonbit_string_t _M0L5modelS3207;
  void* _M0L6_2atmpS3206;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3205;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1415;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3201;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3200;
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1414;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L5usageS1418;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L8_2afieldS3893;
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L7_2abindS1419;
  void* _M0L6_2atmpS3197;
  moonbit_string_t _M0L11fingerprintS1423;
  moonbit_string_t _M0L8_2afieldS3892;
  moonbit_string_t _M0L7_2abindS1424;
  void* _M0L6_2atmpS3198;
  int32_t _M0L13service__tierS1428;
  int64_t _M0L8_2afieldS3891;
  int64_t _M0L7_2abindS1429;
  void* _M0L6_2atmpS3199;
  #line 27 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0L8_2afieldS3896 = _M0L4selfS1416->$0;
  _M0L2idS3213 = _M0L8_2afieldS3896;
  moonbit_incref(_M0L2idS3213);
  #line 29 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0L6_2atmpS3212 = _M0IPC16string6StringPB6ToJson8to__json(_M0L2idS3213);
  _M0L8_2atupleS3202
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3202)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3202->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3202->$1 = _M0L6_2atmpS3212;
  _M0L8_2afieldS3895 = _M0L4selfS1416->$1;
  _M0L7choicesS3211 = _M0L8_2afieldS3895;
  moonbit_incref(_M0L7choicesS3211);
  #line 30 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0L6_2atmpS3210
  = _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(_M0L7choicesS3211);
  _M0L8_2atupleS3203
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3203)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3203->$0 = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L8_2atupleS3203->$1 = _M0L6_2atmpS3210;
  _M0L7createdS3209 = _M0L4selfS1416->$2;
  #line 31 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0L6_2atmpS3208 = _M0IPC13int3IntPB6ToJson8to__json(_M0L7createdS3209);
  _M0L8_2atupleS3204
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3204)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3204->$0 = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L8_2atupleS3204->$1 = _M0L6_2atmpS3208;
  _M0L8_2afieldS3894 = _M0L4selfS1416->$3;
  _M0L5modelS3207 = _M0L8_2afieldS3894;
  moonbit_incref(_M0L5modelS3207);
  #line 32 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0L6_2atmpS3206 = _M0MPC14json4Json6string(_M0L5modelS3207);
  _M0L8_2atupleS3205
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3205)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3205->$0 = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L8_2atupleS3205->$1 = _M0L6_2atmpS3206;
  _M0L7_2abindS1415 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS1415[0] = _M0L8_2atupleS3202;
  _M0L7_2abindS1415[1] = _M0L8_2atupleS3203;
  _M0L7_2abindS1415[2] = _M0L8_2atupleS3204;
  _M0L7_2abindS1415[3] = _M0L8_2atupleS3205;
  _M0L6_2atmpS3201 = _M0L7_2abindS1415;
  _M0L6_2atmpS3200
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 4, _M0L6_2atmpS3201
  };
  #line 28 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0L4jsonS1414 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3200);
  _M0L8_2afieldS3893 = _M0L4selfS1416->$4;
  _M0L7_2abindS1419 = _M0L8_2afieldS3893;
  if (_M0L7_2abindS1419 == 0) {
    
  } else {
    struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L7_2aSomeS1420 =
      _M0L7_2abindS1419;
    struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L8_2ausageS1421 =
      _M0L7_2aSomeS1420;
    moonbit_incref(_M0L8_2ausageS1421);
    _M0L5usageS1418 = _M0L8_2ausageS1421;
    goto join_1417;
  }
  goto joinlet_4443;
  join_1417:;
  #line 35 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0L6_2atmpS3197
  = _M0IP48clawteam8clawteam8internal6openai15CompletionUsagePB6ToJson8to__json(_M0L5usageS1418);
  moonbit_incref(_M0L4jsonS1414);
  #line 35 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1414, (moonbit_string_t)moonbit_string_literal_40.data, _M0L6_2atmpS3197);
  joinlet_4443:;
  _M0L8_2afieldS3892 = _M0L4selfS1416->$5;
  _M0L7_2abindS1424 = _M0L8_2afieldS3892;
  if (_M0L7_2abindS1424 == 0) {
    
  } else {
    moonbit_string_t _M0L7_2aSomeS1425 = _M0L7_2abindS1424;
    moonbit_string_t _M0L14_2afingerprintS1426 = _M0L7_2aSomeS1425;
    moonbit_incref(_M0L14_2afingerprintS1426);
    _M0L11fingerprintS1423 = _M0L14_2afingerprintS1426;
    goto join_1422;
  }
  goto joinlet_4444;
  join_1422:;
  #line 38 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0L6_2atmpS3198 = _M0MPC14json4Json6string(_M0L11fingerprintS1423);
  moonbit_incref(_M0L4jsonS1414);
  #line 38 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1414, (moonbit_string_t)moonbit_string_literal_41.data, _M0L6_2atmpS3198);
  joinlet_4444:;
  _M0L8_2afieldS3891 = _M0L4selfS1416->$6;
  moonbit_decref(_M0L4selfS1416);
  _M0L7_2abindS1429 = _M0L8_2afieldS3891;
  if (_M0L7_2abindS1429 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1430 = _M0L7_2abindS1429;
    int32_t _M0L16_2aservice__tierS1431 = (int32_t)_M0L7_2aSomeS1430;
    _M0L13service__tierS1428 = _M0L16_2aservice__tierS1431;
    goto join_1427;
  }
  goto joinlet_4445;
  join_1427:;
  #line 41 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0L6_2atmpS3199
  = _M0IP48clawteam8clawteam8internal6openai25ChatCompletionServiceTierPB6ToJson8to__json(_M0L13service__tierS1428);
  moonbit_incref(_M0L4jsonS1414);
  #line 41 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1414, (moonbit_string_t)moonbit_string_literal_43.data, _M0L6_2atmpS3199);
  joinlet_4445:;
  #line 43 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  return _M0MPC14json4Json6object(_M0L4jsonS1414);
}

struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion* _M0MP48clawteam8clawteam8internal6openai14ChatCompletion3new(
  moonbit_string_t _M0L2idS1407,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0L7choicesS1408,
  int32_t _M0L7createdS1409,
  moonbit_string_t _M0L5modelS1410,
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L5usageS1411,
  moonbit_string_t _M0L19system__fingerprintS1412,
  int64_t _M0L13service__tierS1413
) {
  struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion* _block_4446;
  #line 14 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion.mbt"
  _block_4446
  = (struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion));
  Moonbit_object_header(_block_4446)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion, $0) >> 2, 5, 0);
  _block_4446->$0 = _M0L2idS1407;
  _block_4446->$1 = _M0L7choicesS1408;
  _block_4446->$2 = _M0L7createdS1409;
  _block_4446->$3 = _M0L5modelS1410;
  _block_4446->$4 = _M0L5usageS1411;
  _block_4446->$5 = _M0L19system__fingerprintS1412;
  _block_4446->$6 = _M0L13service__tierS1413;
  return _block_4446;
}

void* _M0IP48clawteam8clawteam8internal6openai20ChatCompletionChoicePB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L4selfS1401
) {
  int32_t _M0L5indexS3196;
  void* _M0L6_2atmpS3195;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3191;
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0L8_2afieldS3898;
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0L7messageS3194;
  void* _M0L6_2atmpS3193;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3192;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1400;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3190;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3189;
  struct _M0TPB3MapGsRPB4JsonE* _M0L4jsonS1399;
  int32_t _M0L14finish__reasonS1403;
  int64_t _M0L8_2afieldS3897;
  int64_t _M0L7_2abindS1404;
  void* _M0L6_2atmpS3188;
  #line 19 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice.mbt"
  _M0L5indexS3196 = _M0L4selfS1401->$1;
  #line 21 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice.mbt"
  _M0L6_2atmpS3195 = _M0IPC13int3IntPB6ToJson8to__json(_M0L5indexS3196);
  _M0L8_2atupleS3191
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3191)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3191->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L8_2atupleS3191->$1 = _M0L6_2atmpS3195;
  _M0L8_2afieldS3898 = _M0L4selfS1401->$2;
  _M0L7messageS3194 = _M0L8_2afieldS3898;
  moonbit_incref(_M0L7messageS3194);
  #line 22 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice.mbt"
  _M0L6_2atmpS3193
  = _M0IP48clawteam8clawteam8internal6openai21ChatCompletionMessagePB6ToJson8to__json(_M0L7messageS3194);
  _M0L8_2atupleS3192
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3192)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3192->$0 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L8_2atupleS3192->$1 = _M0L6_2atmpS3193;
  _M0L7_2abindS1400 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1400[0] = _M0L8_2atupleS3191;
  _M0L7_2abindS1400[1] = _M0L8_2atupleS3192;
  _M0L6_2atmpS3190 = _M0L7_2abindS1400;
  _M0L6_2atmpS3189
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3190
  };
  #line 20 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice.mbt"
  _M0L4jsonS1399 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3189);
  _M0L8_2afieldS3897 = _M0L4selfS1401->$0;
  moonbit_decref(_M0L4selfS1401);
  _M0L7_2abindS1404 = _M0L8_2afieldS3897;
  if (_M0L7_2abindS1404 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1405 = _M0L7_2abindS1404;
    int32_t _M0L17_2afinish__reasonS1406 = (int32_t)_M0L7_2aSomeS1405;
    _M0L14finish__reasonS1403 = _M0L17_2afinish__reasonS1406;
    goto join_1402;
  }
  goto joinlet_4447;
  join_1402:;
  #line 25 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice.mbt"
  _M0L6_2atmpS3188
  = _M0IP48clawteam8clawteam8internal6openai32ChatCompletionChoiceFinishReasonPB6ToJson8to__json(_M0L14finish__reasonS1403);
  moonbit_incref(_M0L4jsonS1399);
  #line 25 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L4jsonS1399, (moonbit_string_t)moonbit_string_literal_32.data, _M0L6_2atmpS3188);
  joinlet_4447:;
  #line 27 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice.mbt"
  return _M0MPC14json4Json6object(_M0L4jsonS1399);
}

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MP48clawteam8clawteam8internal6openai20ChatCompletionChoice3new(
  int64_t _M0L14finish__reasonS1396,
  int32_t _M0L5indexS1397,
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0L7messageS1398
) {
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _block_4448;
  #line 10 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice.mbt"
  _block_4448
  = (struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice));
  Moonbit_object_header(_block_4448)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice, $2) >> 2, 1, 0);
  _block_4448->$0 = _M0L14finish__reasonS1396;
  _block_4448->$1 = _M0L5indexS1397;
  _block_4448->$2 = _M0L7messageS1398;
  return _block_4448;
}

void* _M0IP48clawteam8clawteam8internal6openai32ChatCompletionChoiceFinishReasonPB6ToJson8to__json(
  int32_t _M0L4selfS1395
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice_finish_reason.mbt"
  switch (_M0L4selfS1395) {
    case 0: {
      #line 16 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice_finish_reason.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_31.data);
      break;
    }
    
    case 1: {
      #line 17 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice_finish_reason.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_59.data);
      break;
    }
    
    case 2: {
      #line 18 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice_finish_reason.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_29.data);
      break;
    }
    
    case 3: {
      #line 19 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice_finish_reason.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_60.data);
      break;
    }
    default: {
      #line 20 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_choice_finish_reason.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_61.data);
      break;
    }
  }
}

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionMessage3new(
  moonbit_string_t _M0L7contentS1393,
  moonbit_string_t _M0L7refusalS1394,
  int64_t _M0L10role_2eoptS1388,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L17tool__calls_2eoptS1391
) {
  int32_t _M0L4roleS1387;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS1390;
  if (_M0L10role_2eoptS1388 == 4294967296ll) {
    _M0L4roleS1387 = 0;
  } else {
    int64_t _M0L7_2aSomeS1389 = _M0L10role_2eoptS1388;
    _M0L4roleS1387 = (int32_t)_M0L7_2aSomeS1389;
  }
  if (_M0L17tool__calls_2eoptS1391 == 0) {
    struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L6_2atmpS3187;
    if (_M0L17tool__calls_2eoptS1391) {
      moonbit_decref(_M0L17tool__calls_2eoptS1391);
    }
    _M0L6_2atmpS3187
    = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall**)moonbit_empty_ref_array;
    _M0L11tool__callsS1390
    = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE));
    Moonbit_object_header(_M0L11tool__callsS1390)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE, $0) >> 2, 1, 0);
    _M0L11tool__callsS1390->$0 = _M0L6_2atmpS3187;
    _M0L11tool__callsS1390->$1 = 0;
  } else {
    struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L7_2aSomeS1392 =
      _M0L17tool__calls_2eoptS1391;
    _M0L11tool__callsS1390 = _M0L7_2aSomeS1392;
  }
  return _M0MP48clawteam8clawteam8internal6openai21ChatCompletionMessage11new_2einner(_M0L7contentS1393, _M0L7refusalS1394, _M0L4roleS1387, _M0L11tool__callsS1390);
}

struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0MP48clawteam8clawteam8internal6openai21ChatCompletionMessage11new_2einner(
  moonbit_string_t _M0L7contentS1383,
  moonbit_string_t _M0L7refusalS1384,
  int32_t _M0L4roleS1385,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS1386
) {
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _block_4449;
  #line 11 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  _block_4449
  = (struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage));
  Moonbit_object_header(_block_4449)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage, $0) >> 2, 3, 0);
  _block_4449->$0 = _M0L7contentS1383;
  _block_4449->$1 = _M0L7refusalS1384;
  _block_4449->$2 = _M0L4roleS1385;
  _block_4449->$3 = _M0L11tool__callsS1386;
  return _block_4449;
}

void* _M0IP48clawteam8clawteam8internal6openai25ChatCompletionMessageRolePB6ToJson8to__json(
  int32_t _M0L4selfS1382
) {
  #line 8 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_role.mbt"
  #line 11 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_role.mbt"
  return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_22.data);
}

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L4selfS1381
) {
  moonbit_string_t _M0L8_2afieldS3900;
  moonbit_string_t _M0L2idS3186;
  void* _M0L6_2atmpS3185;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3179;
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* _M0L8_2afieldS3899;
  int32_t _M0L6_2acntS4282;
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* _M0L8functionS3184;
  void* _M0L6_2atmpS3183;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3180;
  void* _M0L6_2atmpS3182;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3181;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1380;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3178;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3177;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3176;
  #line 10 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L8_2afieldS3900 = _M0L4selfS1381->$0;
  _M0L2idS3186 = _M0L8_2afieldS3900;
  moonbit_incref(_M0L2idS3186);
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS3185 = _M0IPC16string6StringPB6ToJson8to__json(_M0L2idS3186);
  _M0L8_2atupleS3179
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3179)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3179->$0 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L8_2atupleS3179->$1 = _M0L6_2atmpS3185;
  _M0L8_2afieldS3899 = _M0L4selfS1381->$1;
  _M0L6_2acntS4282 = Moonbit_object_header(_M0L4selfS1381)->rc;
  if (_M0L6_2acntS4282 > 1) {
    int32_t _M0L11_2anew__cntS4284 = _M0L6_2acntS4282 - 1;
    Moonbit_object_header(_M0L4selfS1381)->rc = _M0L11_2anew__cntS4284;
    moonbit_incref(_M0L8_2afieldS3899);
  } else if (_M0L6_2acntS4282 == 1) {
    moonbit_string_t _M0L8_2afieldS4283 = _M0L4selfS1381->$0;
    moonbit_decref(_M0L8_2afieldS4283);
    #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
    moonbit_free(_M0L4selfS1381);
  }
  _M0L8functionS3184 = _M0L8_2afieldS3899;
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS3183
  = _M0IP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunctionPB6ToJson8to__json(_M0L8functionS3184);
  _M0L8_2atupleS3180
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3180)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3180->$0 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L8_2atupleS3180->$1 = _M0L6_2atmpS3183;
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS3182
  = _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_27.data);
  _M0L8_2atupleS3181
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3181)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3181->$0 = (moonbit_string_t)moonbit_string_literal_28.data;
  _M0L8_2atupleS3181->$1 = _M0L6_2atmpS3182;
  _M0L7_2abindS1380 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1380[0] = _M0L8_2atupleS3179;
  _M0L7_2abindS1380[1] = _M0L8_2atupleS3180;
  _M0L7_2abindS1380[2] = _M0L8_2atupleS3181;
  _M0L6_2atmpS3178 = _M0L7_2abindS1380;
  _M0L6_2atmpS3177
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 3, _M0L6_2atmpS3178
  };
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  _M0L6_2atmpS3176 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3177);
  #line 13 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call.mbt"
  return _M0MPC14json4Json6object(_M0L6_2atmpS3176);
}

void* _M0IP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunctionPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* _M0L4selfS1379
) {
  moonbit_string_t _M0L8_2afieldS3902;
  moonbit_string_t _M0L4nameS3175;
  void* _M0L6_2atmpS3174;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3169;
  moonbit_string_t _M0L8_2afieldS3901;
  int32_t _M0L6_2acntS4285;
  moonbit_string_t _M0L9argumentsS3173;
  moonbit_string_t _M0L6_2atmpS3172;
  void* _M0L6_2atmpS3171;
  struct _M0TUsRPB4JsonE* _M0L8_2atupleS3170;
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1378;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3168;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3167;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_2atmpS3166;
  #line 9 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L8_2afieldS3902 = _M0L4selfS1379->$1;
  _M0L4nameS3175 = _M0L8_2afieldS3902;
  moonbit_incref(_M0L4nameS3175);
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS3174 = _M0IPC16string6StringPB6ToJson8to__json(_M0L4nameS3175);
  _M0L8_2atupleS3169
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3169)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3169->$0 = (moonbit_string_t)moonbit_string_literal_24.data;
  _M0L8_2atupleS3169->$1 = _M0L6_2atmpS3174;
  _M0L8_2afieldS3901 = _M0L4selfS1379->$0;
  _M0L6_2acntS4285 = Moonbit_object_header(_M0L4selfS1379)->rc;
  if (_M0L6_2acntS4285 > 1) {
    int32_t _M0L11_2anew__cntS4287 = _M0L6_2acntS4285 - 1;
    Moonbit_object_header(_M0L4selfS1379)->rc = _M0L11_2anew__cntS4287;
    if (_M0L8_2afieldS3901) {
      moonbit_incref(_M0L8_2afieldS3901);
    }
  } else if (_M0L6_2acntS4285 == 1) {
    moonbit_string_t _M0L8_2afieldS4286 = _M0L4selfS1379->$1;
    moonbit_decref(_M0L8_2afieldS4286);
    #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
    moonbit_free(_M0L4selfS1379);
  }
  _M0L9argumentsS3173 = _M0L8_2afieldS3901;
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS3172
  = _M0MPC16option6Option10unwrap__orGsE(_M0L9argumentsS3173, (moonbit_string_t)moonbit_string_literal_0.data);
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS3171
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L6_2atmpS3172);
  _M0L8_2atupleS3170
  = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
  Moonbit_object_header(_M0L8_2atupleS3170)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
  _M0L8_2atupleS3170->$0 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L8_2atupleS3170->$1 = _M0L6_2atmpS3171;
  _M0L7_2abindS1378 = (struct _M0TUsRPB4JsonE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1378[0] = _M0L8_2atupleS3169;
  _M0L7_2abindS1378[1] = _M0L8_2atupleS3170;
  _M0L6_2atmpS3168 = _M0L7_2abindS1378;
  _M0L6_2atmpS3167
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 2, _M0L6_2atmpS3168
  };
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  _M0L6_2atmpS3166 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3167);
  #line 12 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message_tool_call_function.mbt"
  return _M0MPC14json4Json6object(_M0L6_2atmpS3166);
}

void* _M0IP48clawteam8clawteam8internal6openai25ChatCompletionServiceTierPB6ToJson8to__json(
  int32_t _M0L4selfS1377
) {
  #line 32 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_service_tier.mbt"
  switch (_M0L4selfS1377) {
    case 0: {
      #line 36 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_service_tier.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_62.data);
      break;
    }
    
    case 1: {
      #line 37 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_service_tier.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_63.data);
      break;
    }
    
    case 2: {
      #line 38 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_service_tier.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_42.data);
      break;
    }
    
    case 3: {
      #line 39 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_service_tier.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_64.data);
      break;
    }
    default: {
      #line 40 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_service_tier.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_65.data);
      break;
    }
  }
}

struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0MP48clawteam8clawteam8internal6openai15CompletionUsage3new(
  int32_t _M0L18completion__tokensS1370,
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L27completion__tokens__detailsS1371,
  void* _M0L4costS1372,
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L13cost__detailsS1373,
  int32_t _M0L14prompt__tokensS1374,
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L23prompt__tokens__detailsS1375,
  int32_t _M0L13total__tokensS1376
) {
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _block_4450;
  #line 14 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _block_4450
  = (struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage));
  Moonbit_object_header(_block_4450)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage, $1) >> 2, 4, 0);
  _block_4450->$0 = _M0L18completion__tokensS1370;
  _block_4450->$1 = _M0L27completion__tokens__detailsS1371;
  _block_4450->$2 = _M0L4costS1372;
  _block_4450->$3 = _M0L13cost__detailsS1373;
  _block_4450->$4 = _M0L14prompt__tokensS1374;
  _block_4450->$5 = _M0L23prompt__tokens__detailsS1375;
  _block_4450->$6 = _M0L13total__tokensS1376;
  return _block_4450;
}

struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice* _M0FP48clawteam8clawteam8internal6openai13chunk__choice(
  int32_t _M0L5indexS1367,
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0L5deltaS1368,
  int64_t _M0L14finish__reasonS1369
) {
  struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice* _block_4451;
  #line 328 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _block_4451
  = (struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice));
  Moonbit_object_header(_block_4451)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoice, $1) >> 2, 1, 0);
  _block_4451->$0 = _M0L5indexS1367;
  _block_4451->$1 = _M0L5deltaS1368;
  _block_4451->$2 = _M0L14finish__reasonS1369;
  return _block_4451;
}

struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _M0FP48clawteam8clawteam8internal6openai20chunk__choice__delta(
  moonbit_string_t _M0L7contentS1363,
  moonbit_string_t _M0L7refusalS1364,
  int64_t _M0L4roleS1365,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCallE* _M0L11tool__callsS1366
) {
  struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta* _block_4452;
  #line 318 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _block_4452
  = (struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta));
  Moonbit_object_header(_block_4452)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai30ChatCompletionChunkChoiceDelta, $0) >> 2, 3, 0);
  _block_4452->$0 = _M0L7contentS1363;
  _block_4452->$1 = _M0L7refusalS1364;
  _block_4452->$2 = _M0L4roleS1365;
  _block_4452->$3 = _M0L11tool__callsS1366;
  return _block_4452;
}

struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _M0FP48clawteam8clawteam8internal6openai32chunk__choice__delta__tool__call(
  int32_t _M0L5indexS1360,
  moonbit_string_t _M0L2idS1361,
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0L8functionS1362
) {
  struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall* _block_4453;
  #line 309 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _block_4453
  = (struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall));
  Moonbit_object_header(_block_4453)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai38ChatCompletionChunkChoiceDeltaToolCall, $1) >> 2, 2, 0);
  _block_4453->$0 = _M0L5indexS1360;
  _block_4453->$1 = _M0L2idS1361;
  _block_4453->$2 = _M0L8functionS1362;
  return _block_4453;
}

struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _M0FP48clawteam8clawteam8internal6openai42chunk__choice__delta__tool__call__function(
  moonbit_string_t _M0L9argumentsS1358,
  moonbit_string_t _M0L4nameS1359
) {
  struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction* _block_4454;
  #line 301 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _block_4454
  = (struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction));
  Moonbit_object_header(_block_4454)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai46ChatCompletionChunkChoiceDeltaToolCallFunction, $0) >> 2, 2, 0);
  _block_4454->$0 = _M0L9argumentsS1358;
  _block_4454->$1 = _M0L4nameS1359;
  return _block_4454;
}

struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk* _M0FP48clawteam8clawteam8internal6openai5chunk(
  moonbit_string_t _M0L2idS1350,
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai25ChatCompletionChunkChoiceE* _M0L7choicesS1351,
  int32_t _M0L7createdS1352,
  moonbit_string_t _M0L5modelS1353,
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L5usageS1354,
  moonbit_string_t _M0L19system__fingerprintS1355,
  int64_t _M0L13service__tierS1356,
  struct _M0TP48clawteam8clawteam8internal6openai23OpenRouterErrorResponse* _M0L5errorS1357
) {
  struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk* _block_4455;
  #line 278 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _block_4455
  = (struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk));
  Moonbit_object_header(_block_4455)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai19ChatCompletionChunk, $0) >> 2, 6, 0);
  _block_4455->$0 = _M0L2idS1350;
  _block_4455->$1 = _M0L7choicesS1351;
  _block_4455->$2 = _M0L7createdS1352;
  _block_4455->$3 = _M0L5modelS1353;
  _block_4455->$4 = _M0L5usageS1354;
  _block_4455->$5 = _M0L19system__fingerprintS1355;
  _block_4455->$6 = _M0L13service__tierS1356;
  _block_4455->$7 = _M0L5errorS1357;
  return _block_4455;
}

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0FP48clawteam8clawteam8internal6openai10tool__call(
  moonbit_string_t _M0L2idS1347,
  moonbit_string_t _M0L4nameS1349,
  moonbit_string_t _M0L9argumentsS1348
) {
  struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction* _M0L6_2atmpS3165;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _block_4456;
  #line 266 "E:\\moonbit\\clawteam\\internal\\openai\\json.mbt"
  _M0L6_2atmpS3165
  = (struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction));
  Moonbit_object_header(_M0L6_2atmpS3165)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai37ChatCompletionMessageToolCallFunction, $0) >> 2, 2, 0);
  _M0L6_2atmpS3165->$0 = _M0L9argumentsS1348;
  _M0L6_2atmpS3165->$1 = _M0L4nameS1349;
  _block_4456
  = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall));
  Moonbit_object_header(_block_4456)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall, $0) >> 2, 2, 0);
  _block_4456->$0 = _M0L2idS1347;
  _block_4456->$1 = _M0L6_2atmpS3165;
  return _block_4456;
}

void* _M0IP48clawteam8clawteam8internal6openai23CompletionTokensDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L9_2ax__714S1344
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1340;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3164;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3163;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1339;
  int32_t _M0L8_24innerS1342;
  int64_t _M0L8_2afieldS3903;
  int64_t _M0L7_2abindS1343;
  void* _M0L6_2atmpS3162;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_tokens_details.mbt"
  _M0L7_2abindS1340 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3164 = _M0L7_2abindS1340;
  _M0L6_2atmpS3163
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS3164
  };
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_tokens_details.mbt"
  _M0L6_24mapS1339 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3163);
  _M0L8_2afieldS3903 = _M0L9_2ax__714S1344->$0;
  moonbit_decref(_M0L9_2ax__714S1344);
  _M0L7_2abindS1343 = _M0L8_2afieldS3903;
  if (_M0L7_2abindS1343 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1345 = _M0L7_2abindS1343;
    int32_t _M0L11_2a_24innerS1346 = (int32_t)_M0L7_2aSomeS1345;
    _M0L8_24innerS1342 = _M0L11_2a_24innerS1346;
    goto join_1341;
  }
  goto joinlet_4457;
  join_1341:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_tokens_details.mbt"
  _M0L6_2atmpS3162 = _M0IPC13int3IntPB6ToJson8to__json(_M0L8_24innerS1342);
  moonbit_incref(_M0L6_24mapS1339);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_tokens_details.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1339, (moonbit_string_t)moonbit_string_literal_66.data, _M0L6_2atmpS3162);
  joinlet_4457:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_tokens_details.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1339);
}

void* _M0IP48clawteam8clawteam8internal6openai19PromptTokensDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L9_2ax__764S1331
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1327;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3161;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3160;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1326;
  int32_t _M0L8_24innerS1329;
  int64_t _M0L7_2abindS1330;
  void* _M0L6_2atmpS3158;
  int32_t _M0L8_24innerS1335;
  int64_t _M0L8_2afieldS3904;
  int64_t _M0L7_2abindS1336;
  void* _M0L6_2atmpS3159;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0L7_2abindS1327 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3161 = _M0L7_2abindS1327;
  _M0L6_2atmpS3160
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS3161
  };
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0L6_24mapS1326 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3160);
  _M0L7_2abindS1330 = _M0L9_2ax__764S1331->$0;
  if (_M0L7_2abindS1330 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1332 = _M0L7_2abindS1330;
    int32_t _M0L11_2a_24innerS1333 = (int32_t)_M0L7_2aSomeS1332;
    _M0L8_24innerS1329 = _M0L11_2a_24innerS1333;
    goto join_1328;
  }
  goto joinlet_4458;
  join_1328:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0L6_2atmpS3158 = _M0IPC13int3IntPB6ToJson8to__json(_M0L8_24innerS1329);
  moonbit_incref(_M0L6_24mapS1326);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1326, (moonbit_string_t)moonbit_string_literal_67.data, _M0L6_2atmpS3158);
  joinlet_4458:;
  _M0L8_2afieldS3904 = _M0L9_2ax__764S1331->$1;
  moonbit_decref(_M0L9_2ax__764S1331);
  _M0L7_2abindS1336 = _M0L8_2afieldS3904;
  if (_M0L7_2abindS1336 == 4294967296ll) {
    
  } else {
    int64_t _M0L7_2aSomeS1337 = _M0L7_2abindS1336;
    int32_t _M0L11_2a_24innerS1338 = (int32_t)_M0L7_2aSomeS1337;
    _M0L8_24innerS1335 = _M0L11_2a_24innerS1338;
    goto join_1334;
  }
  goto joinlet_4459;
  join_1334:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0L6_2atmpS3159 = _M0IPC13int3IntPB6ToJson8to__json(_M0L8_24innerS1335);
  moonbit_incref(_M0L6_24mapS1326);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1326, (moonbit_string_t)moonbit_string_literal_68.data, _M0L6_2atmpS3159);
  joinlet_4459:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\prompt_tokens_details.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1326);
}

void* _M0IP48clawteam8clawteam8internal6openai21ChatCompletionMessagePB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0L9_2ax__861S1318
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1314;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3157;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3156;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1313;
  moonbit_string_t _M0L8_24innerS1316;
  moonbit_string_t _M0L8_2afieldS3907;
  moonbit_string_t _M0L7_2abindS1317;
  void* _M0L6_2atmpS3150;
  moonbit_string_t _M0L8_24innerS1322;
  moonbit_string_t _M0L8_2afieldS3906;
  moonbit_string_t _M0L7_2abindS1323;
  void* _M0L6_2atmpS3151;
  int32_t _M0L4roleS3153;
  void* _M0L6_2atmpS3152;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L8_2afieldS3905;
  int32_t _M0L6_2acntS4288;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L11tool__callsS3155;
  void* _M0L6_2atmpS3154;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  _M0L7_2abindS1314 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3157 = _M0L7_2abindS1314;
  _M0L6_2atmpS3156
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS3157
  };
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  _M0L6_24mapS1313 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3156);
  _M0L8_2afieldS3907 = _M0L9_2ax__861S1318->$0;
  _M0L7_2abindS1317 = _M0L8_2afieldS3907;
  if (_M0L7_2abindS1317 == 0) {
    
  } else {
    moonbit_string_t _M0L7_2aSomeS1319 = _M0L7_2abindS1317;
    moonbit_string_t _M0L11_2a_24innerS1320 = _M0L7_2aSomeS1319;
    moonbit_incref(_M0L11_2a_24innerS1320);
    _M0L8_24innerS1316 = _M0L11_2a_24innerS1320;
    goto join_1315;
  }
  goto joinlet_4460;
  join_1315:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  _M0L6_2atmpS3150
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L8_24innerS1316);
  moonbit_incref(_M0L6_24mapS1313);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1313, (moonbit_string_t)moonbit_string_literal_21.data, _M0L6_2atmpS3150);
  joinlet_4460:;
  _M0L8_2afieldS3906 = _M0L9_2ax__861S1318->$1;
  _M0L7_2abindS1323 = _M0L8_2afieldS3906;
  if (_M0L7_2abindS1323 == 0) {
    
  } else {
    moonbit_string_t _M0L7_2aSomeS1324 = _M0L7_2abindS1323;
    moonbit_string_t _M0L11_2a_24innerS1325 = _M0L7_2aSomeS1324;
    moonbit_incref(_M0L11_2a_24innerS1325);
    _M0L8_24innerS1322 = _M0L11_2a_24innerS1325;
    goto join_1321;
  }
  goto joinlet_4461;
  join_1321:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  _M0L6_2atmpS3151
  = _M0IPC16string6StringPB6ToJson8to__json(_M0L8_24innerS1322);
  moonbit_incref(_M0L6_24mapS1313);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1313, (moonbit_string_t)moonbit_string_literal_69.data, _M0L6_2atmpS3151);
  joinlet_4461:;
  _M0L4roleS3153 = _M0L9_2ax__861S1318->$2;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  _M0L6_2atmpS3152
  = _M0IP48clawteam8clawteam8internal6openai25ChatCompletionMessageRolePB6ToJson8to__json(_M0L4roleS3153);
  moonbit_incref(_M0L6_24mapS1313);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1313, (moonbit_string_t)moonbit_string_literal_23.data, _M0L6_2atmpS3152);
  _M0L8_2afieldS3905 = _M0L9_2ax__861S1318->$3;
  _M0L6_2acntS4288 = Moonbit_object_header(_M0L9_2ax__861S1318)->rc;
  if (_M0L6_2acntS4288 > 1) {
    int32_t _M0L11_2anew__cntS4291 = _M0L6_2acntS4288 - 1;
    Moonbit_object_header(_M0L9_2ax__861S1318)->rc = _M0L11_2anew__cntS4291;
    moonbit_incref(_M0L8_2afieldS3905);
  } else if (_M0L6_2acntS4288 == 1) {
    moonbit_string_t _M0L8_2afieldS4290 = _M0L9_2ax__861S1318->$1;
    moonbit_string_t _M0L8_2afieldS4289;
    if (_M0L8_2afieldS4290) {
      moonbit_decref(_M0L8_2afieldS4290);
    }
    _M0L8_2afieldS4289 = _M0L9_2ax__861S1318->$0;
    if (_M0L8_2afieldS4289) {
      moonbit_decref(_M0L8_2afieldS4289);
    }
    #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
    moonbit_free(_M0L9_2ax__861S1318);
  }
  _M0L11tool__callsS3155 = _M0L8_2afieldS3905;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  _M0L6_2atmpS3154
  = _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L11tool__callsS3155);
  moonbit_incref(_M0L6_24mapS1313);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1313, (moonbit_string_t)moonbit_string_literal_29.data, _M0L6_2atmpS3154);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\chat_completion_message.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1313);
}

void* _M0IP48clawteam8clawteam8internal6openai11CostDetailsPB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L9_2ax__882S1310
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1306;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3149;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3148;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1305;
  double _M0L8_24innerS1308;
  void* _M0L8_2afieldS3909;
  int32_t _M0L6_2acntS4292;
  void* _M0L7_2abindS1309;
  void* _M0L6_2atmpS3147;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
  _M0L7_2abindS1306 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3149 = _M0L7_2abindS1306;
  _M0L6_2atmpS3148
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS3149
  };
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
  _M0L6_24mapS1305 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3148);
  _M0L8_2afieldS3909 = _M0L9_2ax__882S1310->$0;
  _M0L6_2acntS4292 = Moonbit_object_header(_M0L9_2ax__882S1310)->rc;
  if (_M0L6_2acntS4292 > 1) {
    int32_t _M0L11_2anew__cntS4293 = _M0L6_2acntS4292 - 1;
    Moonbit_object_header(_M0L9_2ax__882S1310)->rc = _M0L11_2anew__cntS4293;
    moonbit_incref(_M0L8_2afieldS3909);
  } else if (_M0L6_2acntS4292 == 1) {
    #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
    moonbit_free(_M0L9_2ax__882S1310);
  }
  _M0L7_2abindS1309 = _M0L8_2afieldS3909;
  switch (Moonbit_object_tag(_M0L7_2abindS1309)) {
    case 0:
      break;
    default: {
      struct _M0DTPC16option6OptionGdE4Some* _M0L7_2aSomeS1311 =
        (struct _M0DTPC16option6OptionGdE4Some*)_M0L7_2abindS1309;
      double _M0L8_2afieldS3908 = _M0L7_2aSomeS1311->$0;
      double _M0L11_2a_24innerS1312;
      moonbit_decref(_M0L7_2aSomeS1311);
      _M0L11_2a_24innerS1312 = _M0L8_2afieldS3908;
      _M0L8_24innerS1308 = _M0L11_2a_24innerS1312;
      goto join_1307;
      break;
    }
  }
  goto joinlet_4462;
  join_1307:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
  _M0L6_2atmpS3147
  = _M0IPC16double6DoublePB6ToJson8to__json(_M0L8_24innerS1308);
  moonbit_incref(_M0L6_24mapS1305);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1305, (moonbit_string_t)moonbit_string_literal_70.data, _M0L6_2atmpS3147);
  joinlet_4462:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\cost_details.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1305);
}

void* _M0IP48clawteam8clawteam8internal6openai15CompletionUsagePB6ToJson8to__json(
  struct _M0TP48clawteam8clawteam8internal6openai15CompletionUsage* _M0L9_2ax__924S1284
) {
  struct _M0TUsRPB4JsonE** _M0L7_2abindS1283;
  struct _M0TUsRPB4JsonE** _M0L6_2atmpS3146;
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L6_2atmpS3145;
  struct _M0TPB3MapGsRPB4JsonE* _M0L6_24mapS1282;
  int32_t _M0L18completion__tokensS3136;
  void* _M0L6_2atmpS3135;
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L8_24innerS1286;
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L8_2afieldS3915;
  struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L7_2abindS1287;
  void* _M0L6_2atmpS3137;
  double _M0L8_24innerS1291;
  void* _M0L8_2afieldS3914;
  void* _M0L7_2abindS1292;
  void* _M0L6_2atmpS3138;
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L8_24innerS1296;
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L8_2afieldS3912;
  struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L7_2abindS1297;
  void* _M0L6_2atmpS3139;
  int32_t _M0L14prompt__tokensS3141;
  void* _M0L6_2atmpS3140;
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L8_24innerS1301;
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L8_2afieldS3911;
  struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L7_2abindS1302;
  void* _M0L6_2atmpS3142;
  int32_t _M0L8_2afieldS3910;
  int32_t _M0L13total__tokensS3144;
  void* _M0L6_2atmpS3143;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L7_2abindS1283 = (struct _M0TUsRPB4JsonE**)moonbit_empty_ref_array;
  _M0L6_2atmpS3146 = _M0L7_2abindS1283;
  _M0L6_2atmpS3145
  = (struct _M0TPB9ArrayViewGUsRPB4JsonEE){
    0, 0, _M0L6_2atmpS3146
  };
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_24mapS1282 = _M0MPB3Map11from__arrayGsRPB4JsonE(_M0L6_2atmpS3145);
  _M0L18completion__tokensS3136 = _M0L9_2ax__924S1284->$0;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS3135
  = _M0IPC13int3IntPB6ToJson8to__json(_M0L18completion__tokensS3136);
  moonbit_incref(_M0L6_24mapS1282);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1282, (moonbit_string_t)moonbit_string_literal_36.data, _M0L6_2atmpS3135);
  _M0L8_2afieldS3915 = _M0L9_2ax__924S1284->$1;
  _M0L7_2abindS1287 = _M0L8_2afieldS3915;
  if (_M0L7_2abindS1287 == 0) {
    
  } else {
    struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L7_2aSomeS1288 =
      _M0L7_2abindS1287;
    struct _M0TP48clawteam8clawteam8internal6openai23CompletionTokensDetails* _M0L11_2a_24innerS1289 =
      _M0L7_2aSomeS1288;
    moonbit_incref(_M0L11_2a_24innerS1289);
    _M0L8_24innerS1286 = _M0L11_2a_24innerS1289;
    goto join_1285;
  }
  goto joinlet_4463;
  join_1285:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS3137
  = _M0IP48clawteam8clawteam8internal6openai23CompletionTokensDetailsPB6ToJson8to__json(_M0L8_24innerS1286);
  moonbit_incref(_M0L6_24mapS1282);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1282, (moonbit_string_t)moonbit_string_literal_71.data, _M0L6_2atmpS3137);
  joinlet_4463:;
  _M0L8_2afieldS3914 = _M0L9_2ax__924S1284->$2;
  _M0L7_2abindS1292 = _M0L8_2afieldS3914;
  switch (Moonbit_object_tag(_M0L7_2abindS1292)) {
    case 0: {
      moonbit_incref(_M0L7_2abindS1292);
      break;
    }
    default: {
      struct _M0DTPC16option6OptionGdE4Some* _M0L7_2aSomeS1293 =
        (struct _M0DTPC16option6OptionGdE4Some*)_M0L7_2abindS1292;
      double _M0L8_2afieldS3913 = _M0L7_2aSomeS1293->$0;
      double _M0L11_2a_24innerS1294 = _M0L8_2afieldS3913;
      _M0L8_24innerS1291 = _M0L11_2a_24innerS1294;
      goto join_1290;
      break;
    }
  }
  goto joinlet_4464;
  join_1290:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS3138
  = _M0IPC16double6DoublePB6ToJson8to__json(_M0L8_24innerS1291);
  moonbit_incref(_M0L6_24mapS1282);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1282, (moonbit_string_t)moonbit_string_literal_37.data, _M0L6_2atmpS3138);
  joinlet_4464:;
  _M0L8_2afieldS3912 = _M0L9_2ax__924S1284->$3;
  _M0L7_2abindS1297 = _M0L8_2afieldS3912;
  if (_M0L7_2abindS1297 == 0) {
    
  } else {
    struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L7_2aSomeS1298 =
      _M0L7_2abindS1297;
    struct _M0TP48clawteam8clawteam8internal6openai11CostDetails* _M0L11_2a_24innerS1299 =
      _M0L7_2aSomeS1298;
    moonbit_incref(_M0L11_2a_24innerS1299);
    _M0L8_24innerS1296 = _M0L11_2a_24innerS1299;
    goto join_1295;
  }
  goto joinlet_4465;
  join_1295:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS3139
  = _M0IP48clawteam8clawteam8internal6openai11CostDetailsPB6ToJson8to__json(_M0L8_24innerS1296);
  moonbit_incref(_M0L6_24mapS1282);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1282, (moonbit_string_t)moonbit_string_literal_72.data, _M0L6_2atmpS3139);
  joinlet_4465:;
  _M0L14prompt__tokensS3141 = _M0L9_2ax__924S1284->$4;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS3140
  = _M0IPC13int3IntPB6ToJson8to__json(_M0L14prompt__tokensS3141);
  moonbit_incref(_M0L6_24mapS1282);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1282, (moonbit_string_t)moonbit_string_literal_38.data, _M0L6_2atmpS3140);
  _M0L8_2afieldS3911 = _M0L9_2ax__924S1284->$5;
  _M0L7_2abindS1302 = _M0L8_2afieldS3911;
  if (_M0L7_2abindS1302 == 0) {
    
  } else {
    struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L7_2aSomeS1303 =
      _M0L7_2abindS1302;
    struct _M0TP48clawteam8clawteam8internal6openai19PromptTokensDetails* _M0L11_2a_24innerS1304 =
      _M0L7_2aSomeS1303;
    moonbit_incref(_M0L11_2a_24innerS1304);
    _M0L8_24innerS1301 = _M0L11_2a_24innerS1304;
    goto join_1300;
  }
  goto joinlet_4466;
  join_1300:;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS3142
  = _M0IP48clawteam8clawteam8internal6openai19PromptTokensDetailsPB6ToJson8to__json(_M0L8_24innerS1301);
  moonbit_incref(_M0L6_24mapS1282);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1282, (moonbit_string_t)moonbit_string_literal_73.data, _M0L6_2atmpS3142);
  joinlet_4466:;
  _M0L8_2afieldS3910 = _M0L9_2ax__924S1284->$6;
  moonbit_decref(_M0L9_2ax__924S1284);
  _M0L13total__tokensS3144 = _M0L8_2afieldS3910;
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0L6_2atmpS3143
  = _M0IPC13int3IntPB6ToJson8to__json(_M0L13total__tokensS3144);
  moonbit_incref(_M0L6_24mapS1282);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  _M0MPB3Map3setGsRPB4JsonE(_M0L6_24mapS1282, (moonbit_string_t)moonbit_string_literal_39.data, _M0L6_2atmpS3143);
  #line 3 "E:\\moonbit\\clawteam\\internal\\openai\\completion_usage.mbt"
  return _M0MPC14json4Json6object(_M0L6_24mapS1282);
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1277,
  void* _M0L7contentS1279,
  moonbit_string_t _M0L3locS1273,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1275
) {
  moonbit_string_t _M0L3locS1272;
  moonbit_string_t _M0L9args__locS1274;
  void* _M0L6_2atmpS3133;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3134;
  moonbit_string_t _M0L6actualS1276;
  moonbit_string_t _M0L4wantS1278;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1272 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1273);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1274 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1275);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3133 = _M0L3objS1277.$0->$method_0(_M0L3objS1277.$1);
  _M0L6_2atmpS3134 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1276
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3133, 0, 0, _M0L6_2atmpS3134);
  if (_M0L7contentS1279 == 0) {
    void* _M0L6_2atmpS3130;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3131;
    if (_M0L7contentS1279) {
      moonbit_decref(_M0L7contentS1279);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3130
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS3131 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1278
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS3130, 0, 0, _M0L6_2atmpS3131);
  } else {
    void* _M0L7_2aSomeS1280 = _M0L7contentS1279;
    void* _M0L4_2axS1281 = _M0L7_2aSomeS1280;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS3132 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1278
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1281, 0, 0, _M0L6_2atmpS3132);
  }
  moonbit_incref(_M0L4wantS1278);
  moonbit_incref(_M0L6actualS1276);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1276, _M0L4wantS1278)
  ) {
    moonbit_string_t _M0L6_2atmpS3128;
    moonbit_string_t _M0L6_2atmpS3923;
    moonbit_string_t _M0L6_2atmpS3127;
    moonbit_string_t _M0L6_2atmpS3922;
    moonbit_string_t _M0L6_2atmpS3125;
    moonbit_string_t _M0L6_2atmpS3126;
    moonbit_string_t _M0L6_2atmpS3921;
    moonbit_string_t _M0L6_2atmpS3124;
    moonbit_string_t _M0L6_2atmpS3920;
    moonbit_string_t _M0L6_2atmpS3121;
    moonbit_string_t _M0L6_2atmpS3123;
    moonbit_string_t _M0L6_2atmpS3122;
    moonbit_string_t _M0L6_2atmpS3919;
    moonbit_string_t _M0L6_2atmpS3120;
    moonbit_string_t _M0L6_2atmpS3918;
    moonbit_string_t _M0L6_2atmpS3117;
    moonbit_string_t _M0L6_2atmpS3119;
    moonbit_string_t _M0L6_2atmpS3118;
    moonbit_string_t _M0L6_2atmpS3917;
    moonbit_string_t _M0L6_2atmpS3116;
    moonbit_string_t _M0L6_2atmpS3916;
    moonbit_string_t _M0L6_2atmpS3115;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3114;
    struct moonbit_result_0 _result_4467;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3128
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1272);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3923
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_74.data, _M0L6_2atmpS3128);
    moonbit_decref(_M0L6_2atmpS3128);
    _M0L6_2atmpS3127 = _M0L6_2atmpS3923;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3922
    = moonbit_add_string(_M0L6_2atmpS3127, (moonbit_string_t)moonbit_string_literal_75.data);
    moonbit_decref(_M0L6_2atmpS3127);
    _M0L6_2atmpS3125 = _M0L6_2atmpS3922;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3126
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1274);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3921 = moonbit_add_string(_M0L6_2atmpS3125, _M0L6_2atmpS3126);
    moonbit_decref(_M0L6_2atmpS3125);
    moonbit_decref(_M0L6_2atmpS3126);
    _M0L6_2atmpS3124 = _M0L6_2atmpS3921;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3920
    = moonbit_add_string(_M0L6_2atmpS3124, (moonbit_string_t)moonbit_string_literal_76.data);
    moonbit_decref(_M0L6_2atmpS3124);
    _M0L6_2atmpS3121 = _M0L6_2atmpS3920;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3123 = _M0MPC16string6String6escape(_M0L4wantS1278);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3122
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3123);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3919 = moonbit_add_string(_M0L6_2atmpS3121, _M0L6_2atmpS3122);
    moonbit_decref(_M0L6_2atmpS3121);
    moonbit_decref(_M0L6_2atmpS3122);
    _M0L6_2atmpS3120 = _M0L6_2atmpS3919;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3918
    = moonbit_add_string(_M0L6_2atmpS3120, (moonbit_string_t)moonbit_string_literal_77.data);
    moonbit_decref(_M0L6_2atmpS3120);
    _M0L6_2atmpS3117 = _M0L6_2atmpS3918;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3119 = _M0MPC16string6String6escape(_M0L6actualS1276);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3118
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS3119);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3917 = moonbit_add_string(_M0L6_2atmpS3117, _M0L6_2atmpS3118);
    moonbit_decref(_M0L6_2atmpS3117);
    moonbit_decref(_M0L6_2atmpS3118);
    _M0L6_2atmpS3116 = _M0L6_2atmpS3917;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3916
    = moonbit_add_string(_M0L6_2atmpS3116, (moonbit_string_t)moonbit_string_literal_78.data);
    moonbit_decref(_M0L6_2atmpS3116);
    _M0L6_2atmpS3115 = _M0L6_2atmpS3916;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3114
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3114)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3114)->$0
    = _M0L6_2atmpS3115;
    _result_4467.tag = 0;
    _result_4467.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS3114;
    return _result_4467;
  } else {
    int32_t _M0L6_2atmpS3129;
    struct moonbit_result_0 _result_4468;
    moonbit_decref(_M0L4wantS1278);
    moonbit_decref(_M0L6actualS1276);
    moonbit_decref(_M0L9args__locS1274);
    moonbit_decref(_M0L3locS1272);
    _M0L6_2atmpS3129 = 0;
    _result_4468.tag = 1;
    _result_4468.data.ok = _M0L6_2atmpS3129;
    return _result_4468;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1271,
  int32_t _M0L13escape__slashS1243,
  int32_t _M0L6indentS1238,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1264
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1230;
  void** _M0L6_2atmpS3113;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS1231;
  int32_t _M0Lm5depthS1232;
  void* _M0L6_2atmpS3112;
  void* _M0L8_2aparamS1233;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1230 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS3113 = (void**)moonbit_empty_ref_array;
  _M0L5stackS1231
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS1231)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS1231->$0 = _M0L6_2atmpS3113;
  _M0L5stackS1231->$1 = 0;
  _M0Lm5depthS1232 = 0;
  _M0L6_2atmpS3112 = _M0L4selfS1271;
  _M0L8_2aparamS1233 = _M0L6_2atmpS3112;
  _2aloop_1249:;
  while (1) {
    if (_M0L8_2aparamS1233 == 0) {
      int32_t _M0L3lenS3074;
      if (_M0L8_2aparamS1233) {
        moonbit_decref(_M0L8_2aparamS1233);
      }
      _M0L3lenS3074 = _M0L5stackS1231->$1;
      if (_M0L3lenS3074 == 0) {
        if (_M0L8replacerS1264) {
          moonbit_decref(_M0L8replacerS1264);
        }
        moonbit_decref(_M0L5stackS1231);
        break;
      } else {
        void** _M0L8_2afieldS3931 = _M0L5stackS1231->$0;
        void** _M0L3bufS3098 = _M0L8_2afieldS3931;
        int32_t _M0L3lenS3100 = _M0L5stackS1231->$1;
        int32_t _M0L6_2atmpS3099 = _M0L3lenS3100 - 1;
        void* _M0L6_2atmpS3930 = (void*)_M0L3bufS3098[_M0L6_2atmpS3099];
        void* _M0L4_2axS1250 = _M0L6_2atmpS3930;
        switch (Moonbit_object_tag(_M0L4_2axS1250)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS1251 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS1250;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3926 =
              _M0L8_2aArrayS1251->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1252 =
              _M0L8_2afieldS3926;
            int32_t _M0L4_2aiS1253 = _M0L8_2aArrayS1251->$1;
            int32_t _M0L3lenS3086 = _M0L6_2aarrS1252->$1;
            if (_M0L4_2aiS1253 < _M0L3lenS3086) {
              int32_t _if__result_4470;
              void** _M0L8_2afieldS3925;
              void** _M0L3bufS3092;
              void* _M0L6_2atmpS3924;
              void* _M0L7elementS1254;
              int32_t _M0L6_2atmpS3087;
              void* _M0L6_2atmpS3090;
              if (_M0L4_2aiS1253 < 0) {
                _if__result_4470 = 1;
              } else {
                int32_t _M0L3lenS3091 = _M0L6_2aarrS1252->$1;
                _if__result_4470 = _M0L4_2aiS1253 >= _M0L3lenS3091;
              }
              if (_if__result_4470) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3925 = _M0L6_2aarrS1252->$0;
              _M0L3bufS3092 = _M0L8_2afieldS3925;
              _M0L6_2atmpS3924 = (void*)_M0L3bufS3092[_M0L4_2aiS1253];
              _M0L7elementS1254 = _M0L6_2atmpS3924;
              _M0L6_2atmpS3087 = _M0L4_2aiS1253 + 1;
              _M0L8_2aArrayS1251->$1 = _M0L6_2atmpS3087;
              if (_M0L4_2aiS1253 > 0) {
                int32_t _M0L6_2atmpS3089;
                moonbit_string_t _M0L6_2atmpS3088;
                moonbit_incref(_M0L7elementS1254);
                moonbit_incref(_M0L3bufS1230);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 44);
                _M0L6_2atmpS3089 = _M0Lm5depthS1232;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3088
                = _M0FPC14json11indent__str(_M0L6_2atmpS3089, _M0L6indentS1238);
                moonbit_incref(_M0L3bufS1230);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, _M0L6_2atmpS3088);
              } else {
                moonbit_incref(_M0L7elementS1254);
              }
              _M0L6_2atmpS3090 = _M0L7elementS1254;
              _M0L8_2aparamS1233 = _M0L6_2atmpS3090;
              goto _2aloop_1249;
            } else {
              int32_t _M0L6_2atmpS3093 = _M0Lm5depthS1232;
              void* _M0L6_2atmpS3094;
              int32_t _M0L6_2atmpS3096;
              moonbit_string_t _M0L6_2atmpS3095;
              void* _M0L6_2atmpS3097;
              _M0Lm5depthS1232 = _M0L6_2atmpS3093 - 1;
              moonbit_incref(_M0L5stackS1231);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3094
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1231);
              if (_M0L6_2atmpS3094) {
                moonbit_decref(_M0L6_2atmpS3094);
              }
              _M0L6_2atmpS3096 = _M0Lm5depthS1232;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3095
              = _M0FPC14json11indent__str(_M0L6_2atmpS3096, _M0L6indentS1238);
              moonbit_incref(_M0L3bufS1230);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, _M0L6_2atmpS3095);
              moonbit_incref(_M0L3bufS1230);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 93);
              _M0L6_2atmpS3097 = 0;
              _M0L8_2aparamS1233 = _M0L6_2atmpS3097;
              goto _2aloop_1249;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS1255 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS1250;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3929 =
              _M0L9_2aObjectS1255->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS1256 =
              _M0L8_2afieldS3929;
            int32_t _M0L8_2afirstS1257 = _M0L9_2aObjectS1255->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS1258;
            moonbit_incref(_M0L11_2aiteratorS1256);
            moonbit_incref(_M0L9_2aObjectS1255);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS1258
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS1256);
            if (_M0L7_2abindS1258 == 0) {
              int32_t _M0L6_2atmpS3075;
              void* _M0L6_2atmpS3076;
              int32_t _M0L6_2atmpS3078;
              moonbit_string_t _M0L6_2atmpS3077;
              void* _M0L6_2atmpS3079;
              if (_M0L7_2abindS1258) {
                moonbit_decref(_M0L7_2abindS1258);
              }
              moonbit_decref(_M0L9_2aObjectS1255);
              _M0L6_2atmpS3075 = _M0Lm5depthS1232;
              _M0Lm5depthS1232 = _M0L6_2atmpS3075 - 1;
              moonbit_incref(_M0L5stackS1231);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3076
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS1231);
              if (_M0L6_2atmpS3076) {
                moonbit_decref(_M0L6_2atmpS3076);
              }
              _M0L6_2atmpS3078 = _M0Lm5depthS1232;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3077
              = _M0FPC14json11indent__str(_M0L6_2atmpS3078, _M0L6indentS1238);
              moonbit_incref(_M0L3bufS1230);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, _M0L6_2atmpS3077);
              moonbit_incref(_M0L3bufS1230);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 125);
              _M0L6_2atmpS3079 = 0;
              _M0L8_2aparamS1233 = _M0L6_2atmpS3079;
              goto _2aloop_1249;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1259 = _M0L7_2abindS1258;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1260 = _M0L7_2aSomeS1259;
              moonbit_string_t _M0L8_2afieldS3928 = _M0L4_2axS1260->$0;
              moonbit_string_t _M0L4_2akS1261 = _M0L8_2afieldS3928;
              void* _M0L8_2afieldS3927 = _M0L4_2axS1260->$1;
              int32_t _M0L6_2acntS4294 =
                Moonbit_object_header(_M0L4_2axS1260)->rc;
              void* _M0L4_2avS1262;
              void* _M0Lm2v2S1263;
              moonbit_string_t _M0L6_2atmpS3083;
              void* _M0L6_2atmpS3085;
              void* _M0L6_2atmpS3084;
              if (_M0L6_2acntS4294 > 1) {
                int32_t _M0L11_2anew__cntS4295 = _M0L6_2acntS4294 - 1;
                Moonbit_object_header(_M0L4_2axS1260)->rc
                = _M0L11_2anew__cntS4295;
                moonbit_incref(_M0L8_2afieldS3927);
                moonbit_incref(_M0L4_2akS1261);
              } else if (_M0L6_2acntS4294 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1260);
              }
              _M0L4_2avS1262 = _M0L8_2afieldS3927;
              _M0Lm2v2S1263 = _M0L4_2avS1262;
              if (_M0L8replacerS1264 == 0) {
                moonbit_incref(_M0Lm2v2S1263);
                moonbit_decref(_M0L4_2avS1262);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1265 =
                  _M0L8replacerS1264;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1266 =
                  _M0L7_2aSomeS1265;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1267 =
                  _M0L11_2areplacerS1266;
                void* _M0L7_2abindS1268;
                moonbit_incref(_M0L7_2afuncS1267);
                moonbit_incref(_M0L4_2akS1261);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1268
                = _M0L7_2afuncS1267->code(_M0L7_2afuncS1267, _M0L4_2akS1261, _M0L4_2avS1262);
                if (_M0L7_2abindS1268 == 0) {
                  void* _M0L6_2atmpS3080;
                  if (_M0L7_2abindS1268) {
                    moonbit_decref(_M0L7_2abindS1268);
                  }
                  moonbit_decref(_M0L4_2akS1261);
                  moonbit_decref(_M0L9_2aObjectS1255);
                  _M0L6_2atmpS3080 = 0;
                  _M0L8_2aparamS1233 = _M0L6_2atmpS3080;
                  goto _2aloop_1249;
                } else {
                  void* _M0L7_2aSomeS1269 = _M0L7_2abindS1268;
                  void* _M0L4_2avS1270 = _M0L7_2aSomeS1269;
                  _M0Lm2v2S1263 = _M0L4_2avS1270;
                }
              }
              if (!_M0L8_2afirstS1257) {
                int32_t _M0L6_2atmpS3082;
                moonbit_string_t _M0L6_2atmpS3081;
                moonbit_incref(_M0L3bufS1230);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 44);
                _M0L6_2atmpS3082 = _M0Lm5depthS1232;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS3081
                = _M0FPC14json11indent__str(_M0L6_2atmpS3082, _M0L6indentS1238);
                moonbit_incref(_M0L3bufS1230);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, _M0L6_2atmpS3081);
              }
              moonbit_incref(_M0L3bufS1230);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS3083
              = _M0FPC14json6escape(_M0L4_2akS1261, _M0L13escape__slashS1243);
              moonbit_incref(_M0L3bufS1230);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, _M0L6_2atmpS3083);
              moonbit_incref(_M0L3bufS1230);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 34);
              moonbit_incref(_M0L3bufS1230);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 58);
              if (_M0L6indentS1238 > 0) {
                moonbit_incref(_M0L3bufS1230);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 32);
              }
              _M0L9_2aObjectS1255->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS1255);
              _M0L6_2atmpS3085 = _M0Lm2v2S1263;
              _M0L6_2atmpS3084 = _M0L6_2atmpS3085;
              _M0L8_2aparamS1233 = _M0L6_2atmpS3084;
              goto _2aloop_1249;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS1234 = _M0L8_2aparamS1233;
      void* _M0L8_2avalueS1235 = _M0L7_2aSomeS1234;
      void* _M0L6_2atmpS3111;
      switch (Moonbit_object_tag(_M0L8_2avalueS1235)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS1236 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS1235;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3932 =
            _M0L9_2aObjectS1236->$0;
          int32_t _M0L6_2acntS4296 =
            Moonbit_object_header(_M0L9_2aObjectS1236)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS1237;
          if (_M0L6_2acntS4296 > 1) {
            int32_t _M0L11_2anew__cntS4297 = _M0L6_2acntS4296 - 1;
            Moonbit_object_header(_M0L9_2aObjectS1236)->rc
            = _M0L11_2anew__cntS4297;
            moonbit_incref(_M0L8_2afieldS3932);
          } else if (_M0L6_2acntS4296 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS1236);
          }
          _M0L10_2amembersS1237 = _M0L8_2afieldS3932;
          moonbit_incref(_M0L10_2amembersS1237);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS1237)) {
            moonbit_decref(_M0L10_2amembersS1237);
            moonbit_incref(_M0L3bufS1230);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, (moonbit_string_t)moonbit_string_literal_79.data);
          } else {
            int32_t _M0L6_2atmpS3106 = _M0Lm5depthS1232;
            int32_t _M0L6_2atmpS3108;
            moonbit_string_t _M0L6_2atmpS3107;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS3110;
            void* _M0L6ObjectS3109;
            _M0Lm5depthS1232 = _M0L6_2atmpS3106 + 1;
            moonbit_incref(_M0L3bufS1230);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 123);
            _M0L6_2atmpS3108 = _M0Lm5depthS1232;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3107
            = _M0FPC14json11indent__str(_M0L6_2atmpS3108, _M0L6indentS1238);
            moonbit_incref(_M0L3bufS1230);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, _M0L6_2atmpS3107);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3110
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS1237);
            _M0L6ObjectS3109
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS3109)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3109)->$0
            = _M0L6_2atmpS3110;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS3109)->$1
            = 1;
            moonbit_incref(_M0L5stackS1231);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1231, _M0L6ObjectS3109);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS1239 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS1235;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3933 =
            _M0L8_2aArrayS1239->$0;
          int32_t _M0L6_2acntS4298 =
            Moonbit_object_header(_M0L8_2aArrayS1239)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS1240;
          if (_M0L6_2acntS4298 > 1) {
            int32_t _M0L11_2anew__cntS4299 = _M0L6_2acntS4298 - 1;
            Moonbit_object_header(_M0L8_2aArrayS1239)->rc
            = _M0L11_2anew__cntS4299;
            moonbit_incref(_M0L8_2afieldS3933);
          } else if (_M0L6_2acntS4298 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS1239);
          }
          _M0L6_2aarrS1240 = _M0L8_2afieldS3933;
          moonbit_incref(_M0L6_2aarrS1240);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS1240)) {
            moonbit_decref(_M0L6_2aarrS1240);
            moonbit_incref(_M0L3bufS1230);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, (moonbit_string_t)moonbit_string_literal_80.data);
          } else {
            int32_t _M0L6_2atmpS3102 = _M0Lm5depthS1232;
            int32_t _M0L6_2atmpS3104;
            moonbit_string_t _M0L6_2atmpS3103;
            void* _M0L5ArrayS3105;
            _M0Lm5depthS1232 = _M0L6_2atmpS3102 + 1;
            moonbit_incref(_M0L3bufS1230);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 91);
            _M0L6_2atmpS3104 = _M0Lm5depthS1232;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS3103
            = _M0FPC14json11indent__str(_M0L6_2atmpS3104, _M0L6indentS1238);
            moonbit_incref(_M0L3bufS1230);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, _M0L6_2atmpS3103);
            _M0L5ArrayS3105
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS3105)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3105)->$0
            = _M0L6_2aarrS1240;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS3105)->$1
            = 0;
            moonbit_incref(_M0L5stackS1231);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS1231, _M0L5ArrayS3105);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS1241 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS1235;
          moonbit_string_t _M0L8_2afieldS3934 = _M0L9_2aStringS1241->$0;
          int32_t _M0L6_2acntS4300 =
            Moonbit_object_header(_M0L9_2aStringS1241)->rc;
          moonbit_string_t _M0L4_2asS1242;
          moonbit_string_t _M0L6_2atmpS3101;
          if (_M0L6_2acntS4300 > 1) {
            int32_t _M0L11_2anew__cntS4301 = _M0L6_2acntS4300 - 1;
            Moonbit_object_header(_M0L9_2aStringS1241)->rc
            = _M0L11_2anew__cntS4301;
            moonbit_incref(_M0L8_2afieldS3934);
          } else if (_M0L6_2acntS4300 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS1241);
          }
          _M0L4_2asS1242 = _M0L8_2afieldS3934;
          moonbit_incref(_M0L3bufS1230);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3101
          = _M0FPC14json6escape(_M0L4_2asS1242, _M0L13escape__slashS1243);
          moonbit_incref(_M0L3bufS1230);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, _M0L6_2atmpS3101);
          moonbit_incref(_M0L3bufS1230);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1230, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS1244 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS1235;
          double _M0L4_2anS1245 = _M0L9_2aNumberS1244->$0;
          moonbit_string_t _M0L8_2afieldS3935 = _M0L9_2aNumberS1244->$1;
          int32_t _M0L6_2acntS4302 =
            Moonbit_object_header(_M0L9_2aNumberS1244)->rc;
          moonbit_string_t _M0L7_2areprS1246;
          if (_M0L6_2acntS4302 > 1) {
            int32_t _M0L11_2anew__cntS4303 = _M0L6_2acntS4302 - 1;
            Moonbit_object_header(_M0L9_2aNumberS1244)->rc
            = _M0L11_2anew__cntS4303;
            if (_M0L8_2afieldS3935) {
              moonbit_incref(_M0L8_2afieldS3935);
            }
          } else if (_M0L6_2acntS4302 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS1244);
          }
          _M0L7_2areprS1246 = _M0L8_2afieldS3935;
          if (_M0L7_2areprS1246 == 0) {
            if (_M0L7_2areprS1246) {
              moonbit_decref(_M0L7_2areprS1246);
            }
            moonbit_incref(_M0L3bufS1230);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS1230, _M0L4_2anS1245);
          } else {
            moonbit_string_t _M0L7_2aSomeS1247 = _M0L7_2areprS1246;
            moonbit_string_t _M0L4_2arS1248 = _M0L7_2aSomeS1247;
            moonbit_incref(_M0L3bufS1230);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, _M0L4_2arS1248);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS1230);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, (moonbit_string_t)moonbit_string_literal_81.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS1230);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, (moonbit_string_t)moonbit_string_literal_82.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS1235);
          moonbit_incref(_M0L3bufS1230);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1230, (moonbit_string_t)moonbit_string_literal_83.data);
          break;
        }
      }
      _M0L6_2atmpS3111 = 0;
      _M0L8_2aparamS1233 = _M0L6_2atmpS3111;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1230);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS1229,
  int32_t _M0L6indentS1227
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS1227 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS1228 = _M0L6indentS1227 * _M0L5levelS1229;
    switch (_M0L6spacesS1228) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_84.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_85.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_86.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_87.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_88.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_89.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_90.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_91.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_92.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS3073;
        moonbit_string_t _M0L6_2atmpS3936;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3073
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_93.data, _M0L6spacesS1228);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3936
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_84.data, _M0L6_2atmpS3073);
        moonbit_decref(_M0L6_2atmpS3073);
        return _M0L6_2atmpS3936;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS1219,
  int32_t _M0L13escape__slashS1224
) {
  int32_t _M0L6_2atmpS3072;
  struct _M0TPB13StringBuilder* _M0L3bufS1218;
  struct _M0TWEOc* _M0L5_2aitS1220;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS3072 = Moonbit_array_length(_M0L3strS1219);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS1218 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS3072);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS1220 = _M0MPC16string6String4iter(_M0L3strS1219);
  while (1) {
    int32_t _M0L7_2abindS1221;
    moonbit_incref(_M0L5_2aitS1220);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS1221 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1220);
    if (_M0L7_2abindS1221 == -1) {
      moonbit_decref(_M0L5_2aitS1220);
    } else {
      int32_t _M0L7_2aSomeS1222 = _M0L7_2abindS1221;
      int32_t _M0L4_2acS1223 = _M0L7_2aSomeS1222;
      if (_M0L4_2acS1223 == 34) {
        moonbit_incref(_M0L3bufS1218);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1218, (moonbit_string_t)moonbit_string_literal_94.data);
      } else if (_M0L4_2acS1223 == 92) {
        moonbit_incref(_M0L3bufS1218);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1218, (moonbit_string_t)moonbit_string_literal_95.data);
      } else if (_M0L4_2acS1223 == 47) {
        if (_M0L13escape__slashS1224) {
          moonbit_incref(_M0L3bufS1218);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1218, (moonbit_string_t)moonbit_string_literal_96.data);
        } else {
          moonbit_incref(_M0L3bufS1218);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1218, _M0L4_2acS1223);
        }
      } else if (_M0L4_2acS1223 == 10) {
        moonbit_incref(_M0L3bufS1218);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1218, (moonbit_string_t)moonbit_string_literal_97.data);
      } else if (_M0L4_2acS1223 == 13) {
        moonbit_incref(_M0L3bufS1218);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1218, (moonbit_string_t)moonbit_string_literal_98.data);
      } else if (_M0L4_2acS1223 == 8) {
        moonbit_incref(_M0L3bufS1218);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1218, (moonbit_string_t)moonbit_string_literal_99.data);
      } else if (_M0L4_2acS1223 == 9) {
        moonbit_incref(_M0L3bufS1218);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1218, (moonbit_string_t)moonbit_string_literal_100.data);
      } else {
        int32_t _M0L4codeS1225 = _M0L4_2acS1223;
        if (_M0L4codeS1225 == 12) {
          moonbit_incref(_M0L3bufS1218);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1218, (moonbit_string_t)moonbit_string_literal_101.data);
        } else if (_M0L4codeS1225 < 32) {
          int32_t _M0L6_2atmpS3071;
          moonbit_string_t _M0L6_2atmpS3070;
          moonbit_incref(_M0L3bufS1218);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1218, (moonbit_string_t)moonbit_string_literal_102.data);
          _M0L6_2atmpS3071 = _M0L4codeS1225 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS3070 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS3071);
          moonbit_incref(_M0L3bufS1218);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS1218, _M0L6_2atmpS3070);
        } else {
          moonbit_incref(_M0L3bufS1218);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1218, _M0L4_2acS1223);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1218);
}

int32_t _M0MPC15array5Array6resizeGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L4selfS1207,
  int32_t _M0L8new__lenS1206,
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L1fS1210
) {
  int32_t _M0L3lenS3066;
  #line 1203 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  if (_M0L8new__lenS1206 < 0) {
    #line 1205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_103.data, (moonbit_string_t)moonbit_string_literal_104.data);
  }
  _M0L3lenS3066 = _M0L4selfS1207->$1;
  if (_M0L8new__lenS1206 < _M0L3lenS3066) {
    if (_M0L1fS1210) {
      moonbit_decref(_M0L1fS1210);
    }
    #line 1208 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0MPC15array5Array28unsafe__truncate__to__lengthGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(_M0L4selfS1207, _M0L8new__lenS1206);
  } else {
    int32_t _M0L3lenS1208 = _M0L4selfS1207->$1;
    int32_t _M0L2__S1209 = _M0L3lenS1208;
    while (1) {
      if (_M0L2__S1209 < _M0L8new__lenS1206) {
        int32_t _M0L6_2atmpS3067;
        if (_M0L1fS1210) {
          moonbit_incref(_M0L1fS1210);
        }
        moonbit_incref(_M0L4selfS1207);
        #line 1212 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
        _M0MPC15array5Array4pushGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(_M0L4selfS1207, _M0L1fS1210);
        _M0L6_2atmpS3067 = _M0L2__S1209 + 1;
        _M0L2__S1209 = _M0L6_2atmpS3067;
        continue;
      } else {
        if (_M0L1fS1210) {
          moonbit_decref(_M0L1fS1210);
        }
        moonbit_decref(_M0L4selfS1207);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array5Array6resizeGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS1213,
  int32_t _M0L8new__lenS1212,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L1fS1216
) {
  int32_t _M0L3lenS3068;
  #line 1203 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  if (_M0L8new__lenS1212 < 0) {
    #line 1205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_103.data, (moonbit_string_t)moonbit_string_literal_104.data);
  }
  _M0L3lenS3068 = _M0L4selfS1213->$1;
  if (_M0L8new__lenS1212 < _M0L3lenS3068) {
    if (_M0L1fS1216) {
      moonbit_decref(_M0L1fS1216);
    }
    #line 1208 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0MPC15array5Array28unsafe__truncate__to__lengthGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L4selfS1213, _M0L8new__lenS1212);
  } else {
    int32_t _M0L3lenS1214 = _M0L4selfS1213->$1;
    int32_t _M0L2__S1215 = _M0L3lenS1214;
    while (1) {
      if (_M0L2__S1215 < _M0L8new__lenS1212) {
        int32_t _M0L6_2atmpS3069;
        if (_M0L1fS1216) {
          moonbit_incref(_M0L1fS1216);
        }
        moonbit_incref(_M0L4selfS1213);
        #line 1212 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
        _M0MPC15array5Array4pushGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L4selfS1213, _M0L1fS1216);
        _M0L6_2atmpS3069 = _M0L2__S1215 + 1;
        _M0L2__S1215 = _M0L6_2atmpS3069;
        continue;
      } else {
        if (_M0L1fS1216) {
          moonbit_decref(_M0L1fS1216);
        }
        moonbit_decref(_M0L4selfS1213);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS1205
) {
  int32_t _M0L8_2afieldS3937;
  int32_t _M0L3lenS3065;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3937 = _M0L4selfS1205->$1;
  moonbit_decref(_M0L4selfS1205);
  _M0L3lenS3065 = _M0L8_2afieldS3937;
  return _M0L3lenS3065 == 0;
}

void* _M0MPC15array5Array3getGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L4selfS1200,
  int32_t _M0L5indexS1201
) {
  int32_t _M0L3lenS1199;
  int32_t _if__result_4474;
  #line 212 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS1199 = _M0L4selfS1200->$1;
  if (_M0L5indexS1201 >= 0) {
    _if__result_4474 = _M0L5indexS1201 < _M0L3lenS1199;
  } else {
    _if__result_4474 = 0;
  }
  if (_if__result_4474) {
    struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L8_2afieldS3939 =
      _M0L4selfS1200->$0;
    int32_t _M0L6_2acntS4304 = Moonbit_object_header(_M0L4selfS1200)->rc;
    struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L3bufS3062;
    struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2atmpS3938;
    struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2atmpS3061;
    void* _block_4475;
    if (_M0L6_2acntS4304 > 1) {
      int32_t _M0L11_2anew__cntS4305 = _M0L6_2acntS4304 - 1;
      Moonbit_object_header(_M0L4selfS1200)->rc = _M0L11_2anew__cntS4305;
      moonbit_incref(_M0L8_2afieldS3939);
    } else if (_M0L6_2acntS4304 == 1) {
      #line 215 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_free(_M0L4selfS1200);
    }
    _M0L3bufS3062 = _M0L8_2afieldS3939;
    _M0L6_2atmpS3938
    = (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*)_M0L3bufS3062[
        _M0L5indexS1201
      ];
    if (_M0L6_2atmpS3938) {
      moonbit_incref(_M0L6_2atmpS3938);
    }
    moonbit_decref(_M0L3bufS3062);
    _M0L6_2atmpS3061 = _M0L6_2atmpS3938;
    _block_4475
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE4Some));
    Moonbit_object_header(_block_4475)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE4Some, $0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE4Some*)_block_4475)->$0
    = _M0L6_2atmpS3061;
    return _block_4475;
  } else {
    moonbit_decref(_M0L4selfS1200);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

void* _M0MPC15array5Array3getGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS1203,
  int32_t _M0L5indexS1204
) {
  int32_t _M0L3lenS1202;
  int32_t _if__result_4476;
  #line 212 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS1202 = _M0L4selfS1203->$1;
  if (_M0L5indexS1204 >= 0) {
    _if__result_4476 = _M0L5indexS1204 < _M0L3lenS1202;
  } else {
    _if__result_4476 = 0;
  }
  if (_if__result_4476) {
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L8_2afieldS3941 =
      _M0L4selfS1203->$0;
    int32_t _M0L6_2acntS4306 = Moonbit_object_header(_M0L4selfS1203)->rc;
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L3bufS3064;
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2atmpS3940;
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2atmpS3063;
    void* _block_4477;
    if (_M0L6_2acntS4306 > 1) {
      int32_t _M0L11_2anew__cntS4307 = _M0L6_2acntS4306 - 1;
      Moonbit_object_header(_M0L4selfS1203)->rc = _M0L11_2anew__cntS4307;
      moonbit_incref(_M0L8_2afieldS3941);
    } else if (_M0L6_2acntS4306 == 1) {
      #line 215 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_free(_M0L4selfS1203);
    }
    _M0L3bufS3064 = _M0L8_2afieldS3941;
    _M0L6_2atmpS3940
    = (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*)_M0L3bufS3064[
        _M0L5indexS1204
      ];
    if (_M0L6_2atmpS3940) {
      moonbit_incref(_M0L6_2atmpS3940);
    }
    moonbit_decref(_M0L3bufS3064);
    _M0L6_2atmpS3063 = _M0L6_2atmpS3940;
    _block_4477
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some));
    Moonbit_object_header(_block_4477)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some, $0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some*)_block_4477)->$0
    = _M0L6_2atmpS3063;
    return _block_4477;
  } else {
    moonbit_decref(_M0L4selfS1203);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS1196
) {
  int32_t _M0L3lenS1195;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1195 = _M0L4selfS1196->$1;
  if (_M0L3lenS1195 == 0) {
    moonbit_decref(_M0L4selfS1196);
    return 0;
  } else {
    int32_t _M0L5indexS1197 = _M0L3lenS1195 - 1;
    void** _M0L8_2afieldS3945 = _M0L4selfS1196->$0;
    void** _M0L3bufS3060 = _M0L8_2afieldS3945;
    void* _M0L6_2atmpS3944 = (void*)_M0L3bufS3060[_M0L5indexS1197];
    void* _M0L1vS1198 = _M0L6_2atmpS3944;
    void** _M0L8_2afieldS3943 = _M0L4selfS1196->$0;
    void** _M0L3bufS3059 = _M0L8_2afieldS3943;
    void* _M0L6_2aoldS3942;
    if (
      _M0L5indexS1197 < 0
      || _M0L5indexS1197 >= Moonbit_array_length(_M0L3bufS3059)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3942 = (void*)_M0L3bufS3059[_M0L5indexS1197];
    moonbit_incref(_M0L1vS1198);
    moonbit_decref(_M0L6_2aoldS3942);
    if (
      _M0L5indexS1197 < 0
      || _M0L5indexS1197 >= Moonbit_array_length(_M0L3bufS3059)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS3059[_M0L5indexS1197]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS1196->$1 = _M0L5indexS1197;
    moonbit_decref(_M0L4selfS1196);
    return _M0L1vS1198;
  }
}

int32_t _M0MPC15array5Array28unsafe__truncate__to__lengthGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L4selfS1186,
  int32_t _M0L8new__lenS1187
) {
  int32_t _M0L3lenS1185;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1185 = _M0L4selfS1186->$1;
  if (_M0L8new__lenS1187 <= _M0L3lenS1185) {
    int32_t _M0L1iS1188 = _M0L8new__lenS1187;
    while (1) {
      if (_M0L1iS1188 < _M0L3lenS1185) {
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L8_2afieldS3947 =
          _M0L4selfS1186->$0;
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L3bufS3055 =
          _M0L8_2afieldS3947;
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2aoldS3946;
        int32_t _M0L6_2atmpS3056;
        if (
          _M0L1iS1188 < 0
          || _M0L1iS1188 >= Moonbit_array_length(_M0L3bufS3055)
        ) {
          #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3946
        = (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*)_M0L3bufS3055[
            _M0L1iS1188
          ];
        if (_M0L6_2aoldS3946) {
          moonbit_decref(_M0L6_2aoldS3946);
        }
        if (
          _M0L1iS1188 < 0
          || _M0L1iS1188 >= Moonbit_array_length(_M0L3bufS3055)
        ) {
          #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
          moonbit_panic();
        }
        _M0L3bufS3055[_M0L1iS1188] = 0;
        _M0L6_2atmpS3056 = _M0L1iS1188 + 1;
        _M0L1iS1188 = _M0L6_2atmpS3056;
        continue;
      }
      break;
    }
    _M0L4selfS1186->$1 = _M0L8new__lenS1187;
    moonbit_decref(_M0L4selfS1186);
  } else {
    moonbit_decref(_M0L4selfS1186);
    #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPC15array5Array28unsafe__truncate__to__lengthGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS1191,
  int32_t _M0L8new__lenS1192
) {
  int32_t _M0L3lenS1190;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1190 = _M0L4selfS1191->$1;
  if (_M0L8new__lenS1192 <= _M0L3lenS1190) {
    int32_t _M0L1iS1193 = _M0L8new__lenS1192;
    while (1) {
      if (_M0L1iS1193 < _M0L3lenS1190) {
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L8_2afieldS3949 =
          _M0L4selfS1191->$0;
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L3bufS3057 =
          _M0L8_2afieldS3949;
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2aoldS3948;
        int32_t _M0L6_2atmpS3058;
        if (
          _M0L1iS1193 < 0
          || _M0L1iS1193 >= Moonbit_array_length(_M0L3bufS3057)
        ) {
          #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3948
        = (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*)_M0L3bufS3057[
            _M0L1iS1193
          ];
        if (_M0L6_2aoldS3948) {
          moonbit_decref(_M0L6_2aoldS3948);
        }
        if (
          _M0L1iS1193 < 0
          || _M0L1iS1193 >= Moonbit_array_length(_M0L3bufS3057)
        ) {
          #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
          moonbit_panic();
        }
        _M0L3bufS3057[_M0L1iS1193] = 0;
        _M0L6_2atmpS3058 = _M0L1iS1193 + 1;
        _M0L1iS1193 = _M0L6_2atmpS3058;
        continue;
      }
      break;
    }
    _M0L4selfS1191->$1 = _M0L8new__lenS1192;
    moonbit_decref(_M0L4selfS1191);
  } else {
    moonbit_decref(_M0L4selfS1191);
    #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS1183,
  struct _M0TPB6Logger _M0L6loggerS1184
) {
  moonbit_string_t _M0L6_2atmpS3054;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS3053;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3054 = _M0L4selfS1183;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3053 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS3054);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS3053, _M0L6loggerS1184);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS1160,
  struct _M0TPB6Logger _M0L6loggerS1182
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3958;
  struct _M0TPC16string10StringView _M0L3pkgS1159;
  moonbit_string_t _M0L7_2adataS1161;
  int32_t _M0L8_2astartS1162;
  int32_t _M0L6_2atmpS3052;
  int32_t _M0L6_2aendS1163;
  int32_t _M0Lm9_2acursorS1164;
  int32_t _M0Lm13accept__stateS1165;
  int32_t _M0Lm10match__endS1166;
  int32_t _M0Lm20match__tag__saver__0S1167;
  int32_t _M0Lm6tag__0S1168;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS1169;
  struct _M0TPC16string10StringView _M0L8_2afieldS3957;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS1178;
  void* _M0L8_2afieldS3956;
  int32_t _M0L6_2acntS4308;
  void* _M0L16_2apackage__nameS1179;
  struct _M0TPC16string10StringView _M0L8_2afieldS3954;
  struct _M0TPC16string10StringView _M0L8filenameS3029;
  struct _M0TPC16string10StringView _M0L8_2afieldS3953;
  struct _M0TPC16string10StringView _M0L11start__lineS3030;
  struct _M0TPC16string10StringView _M0L8_2afieldS3952;
  struct _M0TPC16string10StringView _M0L13start__columnS3031;
  struct _M0TPC16string10StringView _M0L8_2afieldS3951;
  struct _M0TPC16string10StringView _M0L9end__lineS3032;
  struct _M0TPC16string10StringView _M0L8_2afieldS3950;
  int32_t _M0L6_2acntS4312;
  struct _M0TPC16string10StringView _M0L11end__columnS3033;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3958
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1160->$0_1, _M0L4selfS1160->$0_2, _M0L4selfS1160->$0_0
  };
  _M0L3pkgS1159 = _M0L8_2afieldS3958;
  moonbit_incref(_M0L3pkgS1159.$0);
  moonbit_incref(_M0L3pkgS1159.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS1161 = _M0MPC16string10StringView4data(_M0L3pkgS1159);
  moonbit_incref(_M0L3pkgS1159.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS1162
  = _M0MPC16string10StringView13start__offset(_M0L3pkgS1159);
  moonbit_incref(_M0L3pkgS1159.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3052 = _M0MPC16string10StringView6length(_M0L3pkgS1159);
  _M0L6_2aendS1163 = _M0L8_2astartS1162 + _M0L6_2atmpS3052;
  _M0Lm9_2acursorS1164 = _M0L8_2astartS1162;
  _M0Lm13accept__stateS1165 = -1;
  _M0Lm10match__endS1166 = -1;
  _M0Lm20match__tag__saver__0S1167 = -1;
  _M0Lm6tag__0S1168 = -1;
  while (1) {
    int32_t _M0L6_2atmpS3044 = _M0Lm9_2acursorS1164;
    if (_M0L6_2atmpS3044 < _M0L6_2aendS1163) {
      int32_t _M0L6_2atmpS3051 = _M0Lm9_2acursorS1164;
      int32_t _M0L10next__charS1173;
      int32_t _M0L6_2atmpS3045;
      moonbit_incref(_M0L7_2adataS1161);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS1173
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1161, _M0L6_2atmpS3051);
      _M0L6_2atmpS3045 = _M0Lm9_2acursorS1164;
      _M0Lm9_2acursorS1164 = _M0L6_2atmpS3045 + 1;
      if (_M0L10next__charS1173 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS3046;
          _M0Lm6tag__0S1168 = _M0Lm9_2acursorS1164;
          _M0L6_2atmpS3046 = _M0Lm9_2acursorS1164;
          if (_M0L6_2atmpS3046 < _M0L6_2aendS1163) {
            int32_t _M0L6_2atmpS3050 = _M0Lm9_2acursorS1164;
            int32_t _M0L10next__charS1174;
            int32_t _M0L6_2atmpS3047;
            moonbit_incref(_M0L7_2adataS1161);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS1174
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS1161, _M0L6_2atmpS3050);
            _M0L6_2atmpS3047 = _M0Lm9_2acursorS1164;
            _M0Lm9_2acursorS1164 = _M0L6_2atmpS3047 + 1;
            if (_M0L10next__charS1174 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS3048 = _M0Lm9_2acursorS1164;
                if (_M0L6_2atmpS3048 < _M0L6_2aendS1163) {
                  int32_t _M0L6_2atmpS3049 = _M0Lm9_2acursorS1164;
                  _M0Lm9_2acursorS1164 = _M0L6_2atmpS3049 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S1167 = _M0Lm6tag__0S1168;
                  _M0Lm13accept__stateS1165 = 0;
                  _M0Lm10match__endS1166 = _M0Lm9_2acursorS1164;
                  goto join_1170;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_1170;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_1170;
    }
    break;
  }
  goto joinlet_4480;
  join_1170:;
  switch (_M0Lm13accept__stateS1165) {
    case 0: {
      int32_t _M0L6_2atmpS3042;
      int32_t _M0L6_2atmpS3041;
      int64_t _M0L6_2atmpS3038;
      int32_t _M0L6_2atmpS3040;
      int64_t _M0L6_2atmpS3039;
      struct _M0TPC16string10StringView _M0L13package__nameS1171;
      int64_t _M0L6_2atmpS3035;
      int32_t _M0L6_2atmpS3037;
      int64_t _M0L6_2atmpS3036;
      struct _M0TPC16string10StringView _M0L12module__nameS1172;
      void* _M0L4SomeS3034;
      moonbit_decref(_M0L3pkgS1159.$0);
      _M0L6_2atmpS3042 = _M0Lm20match__tag__saver__0S1167;
      _M0L6_2atmpS3041 = _M0L6_2atmpS3042 + 1;
      _M0L6_2atmpS3038 = (int64_t)_M0L6_2atmpS3041;
      _M0L6_2atmpS3040 = _M0Lm10match__endS1166;
      _M0L6_2atmpS3039 = (int64_t)_M0L6_2atmpS3040;
      moonbit_incref(_M0L7_2adataS1161);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS1171
      = _M0MPC16string6String4view(_M0L7_2adataS1161, _M0L6_2atmpS3038, _M0L6_2atmpS3039);
      _M0L6_2atmpS3035 = (int64_t)_M0L8_2astartS1162;
      _M0L6_2atmpS3037 = _M0Lm20match__tag__saver__0S1167;
      _M0L6_2atmpS3036 = (int64_t)_M0L6_2atmpS3037;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS1172
      = _M0MPC16string6String4view(_M0L7_2adataS1161, _M0L6_2atmpS3035, _M0L6_2atmpS3036);
      _M0L4SomeS3034
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS3034)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3034)->$0_0
      = _M0L13package__nameS1171.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3034)->$0_1
      = _M0L13package__nameS1171.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS3034)->$0_2
      = _M0L13package__nameS1171.$2;
      _M0L7_2abindS1169
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1169)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1169->$0_0 = _M0L12module__nameS1172.$0;
      _M0L7_2abindS1169->$0_1 = _M0L12module__nameS1172.$1;
      _M0L7_2abindS1169->$0_2 = _M0L12module__nameS1172.$2;
      _M0L7_2abindS1169->$1 = _M0L4SomeS3034;
      break;
    }
    default: {
      void* _M0L4NoneS3043;
      moonbit_decref(_M0L7_2adataS1161);
      _M0L4NoneS3043
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS1169
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS1169)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS1169->$0_0 = _M0L3pkgS1159.$0;
      _M0L7_2abindS1169->$0_1 = _M0L3pkgS1159.$1;
      _M0L7_2abindS1169->$0_2 = _M0L3pkgS1159.$2;
      _M0L7_2abindS1169->$1 = _M0L4NoneS3043;
      break;
    }
  }
  joinlet_4480:;
  _M0L8_2afieldS3957
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS1169->$0_1, _M0L7_2abindS1169->$0_2, _M0L7_2abindS1169->$0_0
  };
  _M0L15_2amodule__nameS1178 = _M0L8_2afieldS3957;
  _M0L8_2afieldS3956 = _M0L7_2abindS1169->$1;
  _M0L6_2acntS4308 = Moonbit_object_header(_M0L7_2abindS1169)->rc;
  if (_M0L6_2acntS4308 > 1) {
    int32_t _M0L11_2anew__cntS4309 = _M0L6_2acntS4308 - 1;
    Moonbit_object_header(_M0L7_2abindS1169)->rc = _M0L11_2anew__cntS4309;
    moonbit_incref(_M0L8_2afieldS3956);
    moonbit_incref(_M0L15_2amodule__nameS1178.$0);
  } else if (_M0L6_2acntS4308 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS1169);
  }
  _M0L16_2apackage__nameS1179 = _M0L8_2afieldS3956;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS1179)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS1180 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS1179;
      struct _M0TPC16string10StringView _M0L8_2afieldS3955 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS1180->$0_1,
                                              _M0L7_2aSomeS1180->$0_2,
                                              _M0L7_2aSomeS1180->$0_0};
      int32_t _M0L6_2acntS4310 = Moonbit_object_header(_M0L7_2aSomeS1180)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS1181;
      if (_M0L6_2acntS4310 > 1) {
        int32_t _M0L11_2anew__cntS4311 = _M0L6_2acntS4310 - 1;
        Moonbit_object_header(_M0L7_2aSomeS1180)->rc = _M0L11_2anew__cntS4311;
        moonbit_incref(_M0L8_2afieldS3955.$0);
      } else if (_M0L6_2acntS4310 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS1180);
      }
      _M0L12_2apkg__nameS1181 = _M0L8_2afieldS3955;
      if (_M0L6loggerS1182.$1) {
        moonbit_incref(_M0L6loggerS1182.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1182.$0->$method_2(_M0L6loggerS1182.$1, _M0L12_2apkg__nameS1181);
      if (_M0L6loggerS1182.$1) {
        moonbit_incref(_M0L6loggerS1182.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS1182.$0->$method_3(_M0L6loggerS1182.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS1179);
      break;
    }
  }
  _M0L8_2afieldS3954
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1160->$1_1, _M0L4selfS1160->$1_2, _M0L4selfS1160->$1_0
  };
  _M0L8filenameS3029 = _M0L8_2afieldS3954;
  moonbit_incref(_M0L8filenameS3029.$0);
  if (_M0L6loggerS1182.$1) {
    moonbit_incref(_M0L6loggerS1182.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1182.$0->$method_2(_M0L6loggerS1182.$1, _M0L8filenameS3029);
  if (_M0L6loggerS1182.$1) {
    moonbit_incref(_M0L6loggerS1182.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1182.$0->$method_3(_M0L6loggerS1182.$1, 58);
  _M0L8_2afieldS3953
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1160->$2_1, _M0L4selfS1160->$2_2, _M0L4selfS1160->$2_0
  };
  _M0L11start__lineS3030 = _M0L8_2afieldS3953;
  moonbit_incref(_M0L11start__lineS3030.$0);
  if (_M0L6loggerS1182.$1) {
    moonbit_incref(_M0L6loggerS1182.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1182.$0->$method_2(_M0L6loggerS1182.$1, _M0L11start__lineS3030);
  if (_M0L6loggerS1182.$1) {
    moonbit_incref(_M0L6loggerS1182.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1182.$0->$method_3(_M0L6loggerS1182.$1, 58);
  _M0L8_2afieldS3952
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1160->$3_1, _M0L4selfS1160->$3_2, _M0L4selfS1160->$3_0
  };
  _M0L13start__columnS3031 = _M0L8_2afieldS3952;
  moonbit_incref(_M0L13start__columnS3031.$0);
  if (_M0L6loggerS1182.$1) {
    moonbit_incref(_M0L6loggerS1182.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1182.$0->$method_2(_M0L6loggerS1182.$1, _M0L13start__columnS3031);
  if (_M0L6loggerS1182.$1) {
    moonbit_incref(_M0L6loggerS1182.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1182.$0->$method_3(_M0L6loggerS1182.$1, 45);
  _M0L8_2afieldS3951
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1160->$4_1, _M0L4selfS1160->$4_2, _M0L4selfS1160->$4_0
  };
  _M0L9end__lineS3032 = _M0L8_2afieldS3951;
  moonbit_incref(_M0L9end__lineS3032.$0);
  if (_M0L6loggerS1182.$1) {
    moonbit_incref(_M0L6loggerS1182.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1182.$0->$method_2(_M0L6loggerS1182.$1, _M0L9end__lineS3032);
  if (_M0L6loggerS1182.$1) {
    moonbit_incref(_M0L6loggerS1182.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1182.$0->$method_3(_M0L6loggerS1182.$1, 58);
  _M0L8_2afieldS3950
  = (struct _M0TPC16string10StringView){
    _M0L4selfS1160->$5_1, _M0L4selfS1160->$5_2, _M0L4selfS1160->$5_0
  };
  _M0L6_2acntS4312 = Moonbit_object_header(_M0L4selfS1160)->rc;
  if (_M0L6_2acntS4312 > 1) {
    int32_t _M0L11_2anew__cntS4318 = _M0L6_2acntS4312 - 1;
    Moonbit_object_header(_M0L4selfS1160)->rc = _M0L11_2anew__cntS4318;
    moonbit_incref(_M0L8_2afieldS3950.$0);
  } else if (_M0L6_2acntS4312 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4317 =
      (struct _M0TPC16string10StringView){_M0L4selfS1160->$4_1,
                                            _M0L4selfS1160->$4_2,
                                            _M0L4selfS1160->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4316;
    struct _M0TPC16string10StringView _M0L8_2afieldS4315;
    struct _M0TPC16string10StringView _M0L8_2afieldS4314;
    struct _M0TPC16string10StringView _M0L8_2afieldS4313;
    moonbit_decref(_M0L8_2afieldS4317.$0);
    _M0L8_2afieldS4316
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1160->$3_1, _M0L4selfS1160->$3_2, _M0L4selfS1160->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4316.$0);
    _M0L8_2afieldS4315
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1160->$2_1, _M0L4selfS1160->$2_2, _M0L4selfS1160->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4315.$0);
    _M0L8_2afieldS4314
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1160->$1_1, _M0L4selfS1160->$1_2, _M0L4selfS1160->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4314.$0);
    _M0L8_2afieldS4313
    = (struct _M0TPC16string10StringView){
      _M0L4selfS1160->$0_1, _M0L4selfS1160->$0_2, _M0L4selfS1160->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4313.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS1160);
  }
  _M0L11end__columnS3033 = _M0L8_2afieldS3950;
  if (_M0L6loggerS1182.$1) {
    moonbit_incref(_M0L6loggerS1182.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1182.$0->$method_2(_M0L6loggerS1182.$1, _M0L11end__columnS3033);
  if (_M0L6loggerS1182.$1) {
    moonbit_incref(_M0L6loggerS1182.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1182.$0->$method_3(_M0L6loggerS1182.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS1182.$0->$method_2(_M0L6loggerS1182.$1, _M0L15_2amodule__nameS1178);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS1158) {
  moonbit_string_t _M0L6_2atmpS3028;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS3028
  = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS1158);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS3028);
  moonbit_decref(_M0L6_2atmpS3028);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS1157,
  struct _M0TPB6Logger _M0L6loggerS1156
) {
  moonbit_string_t _M0L6_2atmpS3027;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS3027 = _M0MPC16double6Double10to__string(_M0L4selfS1157);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS1156.$0->$method_0(_M0L6loggerS1156.$1, _M0L6_2atmpS3027);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS1155) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS1155);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS1142) {
  uint64_t _M0L4bitsS1143;
  uint64_t _M0L6_2atmpS3026;
  uint64_t _M0L6_2atmpS3025;
  int32_t _M0L8ieeeSignS1144;
  uint64_t _M0L12ieeeMantissaS1145;
  uint64_t _M0L6_2atmpS3024;
  uint64_t _M0L6_2atmpS3023;
  int32_t _M0L12ieeeExponentS1146;
  int32_t _if__result_4484;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS1147;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS1148;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3022;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS1142 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_105.data;
  }
  _M0L4bitsS1143 = *(int64_t*)&_M0L3valS1142;
  _M0L6_2atmpS3026 = _M0L4bitsS1143 >> 63;
  _M0L6_2atmpS3025 = _M0L6_2atmpS3026 & 1ull;
  _M0L8ieeeSignS1144 = _M0L6_2atmpS3025 != 0ull;
  _M0L12ieeeMantissaS1145 = _M0L4bitsS1143 & 4503599627370495ull;
  _M0L6_2atmpS3024 = _M0L4bitsS1143 >> 52;
  _M0L6_2atmpS3023 = _M0L6_2atmpS3024 & 2047ull;
  _M0L12ieeeExponentS1146 = (int32_t)_M0L6_2atmpS3023;
  if (_M0L12ieeeExponentS1146 == 2047) {
    _if__result_4484 = 1;
  } else if (_M0L12ieeeExponentS1146 == 0) {
    _if__result_4484 = _M0L12ieeeMantissaS1145 == 0ull;
  } else {
    _if__result_4484 = 0;
  }
  if (_if__result_4484) {
    int32_t _M0L6_2atmpS3011 = _M0L12ieeeExponentS1146 != 0;
    int32_t _M0L6_2atmpS3012 = _M0L12ieeeMantissaS1145 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS1144, _M0L6_2atmpS3011, _M0L6_2atmpS3012);
  }
  _M0Lm1vS1147 = _M0FPB31ryu__to__string_2erecord_2f1141;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS1148
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS1145, _M0L12ieeeExponentS1146);
  if (_M0L5smallS1148 == 0) {
    uint32_t _M0L6_2atmpS3013;
    if (_M0L5smallS1148) {
      moonbit_decref(_M0L5smallS1148);
    }
    _M0L6_2atmpS3013 = *(uint32_t*)&_M0L12ieeeExponentS1146;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS1147 = _M0FPB3d2d(_M0L12ieeeMantissaS1145, _M0L6_2atmpS3013);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS1149 = _M0L5smallS1148;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS1150 = _M0L7_2aSomeS1149;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS1151 = _M0L4_2afS1150;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3021 = _M0Lm1xS1151;
      uint64_t _M0L8_2afieldS3961 = _M0L6_2atmpS3021->$0;
      uint64_t _M0L8mantissaS3020 = _M0L8_2afieldS3961;
      uint64_t _M0L1qS1152 = _M0L8mantissaS3020 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3019 = _M0Lm1xS1151;
      uint64_t _M0L8_2afieldS3960 = _M0L6_2atmpS3019->$0;
      uint64_t _M0L8mantissaS3017 = _M0L8_2afieldS3960;
      uint64_t _M0L6_2atmpS3018 = 10ull * _M0L1qS1152;
      uint64_t _M0L1rS1153 = _M0L8mantissaS3017 - _M0L6_2atmpS3018;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3016;
      int32_t _M0L8_2afieldS3959;
      int32_t _M0L8exponentS3015;
      int32_t _M0L6_2atmpS3014;
      if (_M0L1rS1153 != 0ull) {
        break;
      }
      _M0L6_2atmpS3016 = _M0Lm1xS1151;
      _M0L8_2afieldS3959 = _M0L6_2atmpS3016->$1;
      moonbit_decref(_M0L6_2atmpS3016);
      _M0L8exponentS3015 = _M0L8_2afieldS3959;
      _M0L6_2atmpS3014 = _M0L8exponentS3015 + 1;
      _M0Lm1xS1151
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS1151)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS1151->$0 = _M0L1qS1152;
      _M0Lm1xS1151->$1 = _M0L6_2atmpS3014;
      continue;
      break;
    }
    _M0Lm1vS1147 = _M0Lm1xS1151;
  }
  _M0L6_2atmpS3022 = _M0Lm1vS1147;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS3022, _M0L8ieeeSignS1144);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS1136,
  int32_t _M0L12ieeeExponentS1138
) {
  uint64_t _M0L2m2S1135;
  int32_t _M0L6_2atmpS3010;
  int32_t _M0L2e2S1137;
  int32_t _M0L6_2atmpS3009;
  uint64_t _M0L6_2atmpS3008;
  uint64_t _M0L4maskS1139;
  uint64_t _M0L8fractionS1140;
  int32_t _M0L6_2atmpS3007;
  uint64_t _M0L6_2atmpS3006;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS3005;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S1135 = 4503599627370496ull | _M0L12ieeeMantissaS1136;
  _M0L6_2atmpS3010 = _M0L12ieeeExponentS1138 - 1023;
  _M0L2e2S1137 = _M0L6_2atmpS3010 - 52;
  if (_M0L2e2S1137 > 0) {
    return 0;
  }
  if (_M0L2e2S1137 < -52) {
    return 0;
  }
  _M0L6_2atmpS3009 = -_M0L2e2S1137;
  _M0L6_2atmpS3008 = 1ull << (_M0L6_2atmpS3009 & 63);
  _M0L4maskS1139 = _M0L6_2atmpS3008 - 1ull;
  _M0L8fractionS1140 = _M0L2m2S1135 & _M0L4maskS1139;
  if (_M0L8fractionS1140 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS3007 = -_M0L2e2S1137;
  _M0L6_2atmpS3006 = _M0L2m2S1135 >> (_M0L6_2atmpS3007 & 63);
  _M0L6_2atmpS3005
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS3005)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS3005->$0 = _M0L6_2atmpS3006;
  _M0L6_2atmpS3005->$1 = 0;
  return _M0L6_2atmpS3005;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS1109,
  int32_t _M0L4signS1107
) {
  int32_t _M0L6_2atmpS3004;
  moonbit_bytes_t _M0L6resultS1105;
  int32_t _M0Lm5indexS1106;
  uint64_t _M0Lm6outputS1108;
  uint64_t _M0L6_2atmpS3003;
  int32_t _M0L7olengthS1110;
  int32_t _M0L8_2afieldS3962;
  int32_t _M0L8exponentS3002;
  int32_t _M0L6_2atmpS3001;
  int32_t _M0Lm3expS1111;
  int32_t _M0L6_2atmpS3000;
  int32_t _M0L6_2atmpS2998;
  int32_t _M0L18scientificNotationS1112;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3004 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS1105
  = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS3004);
  _M0Lm5indexS1106 = 0;
  if (_M0L4signS1107) {
    int32_t _M0L6_2atmpS2873 = _M0Lm5indexS1106;
    int32_t _M0L6_2atmpS2874;
    if (
      _M0L6_2atmpS2873 < 0
      || _M0L6_2atmpS2873 >= Moonbit_array_length(_M0L6resultS1105)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1105[_M0L6_2atmpS2873] = 45;
    _M0L6_2atmpS2874 = _M0Lm5indexS1106;
    _M0Lm5indexS1106 = _M0L6_2atmpS2874 + 1;
  }
  _M0Lm6outputS1108 = _M0L1vS1109->$0;
  _M0L6_2atmpS3003 = _M0Lm6outputS1108;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS1110 = _M0FPB17decimal__length17(_M0L6_2atmpS3003);
  _M0L8_2afieldS3962 = _M0L1vS1109->$1;
  moonbit_decref(_M0L1vS1109);
  _M0L8exponentS3002 = _M0L8_2afieldS3962;
  _M0L6_2atmpS3001 = _M0L8exponentS3002 + _M0L7olengthS1110;
  _M0Lm3expS1111 = _M0L6_2atmpS3001 - 1;
  _M0L6_2atmpS3000 = _M0Lm3expS1111;
  if (_M0L6_2atmpS3000 >= -6) {
    int32_t _M0L6_2atmpS2999 = _M0Lm3expS1111;
    _M0L6_2atmpS2998 = _M0L6_2atmpS2999 < 21;
  } else {
    _M0L6_2atmpS2998 = 0;
  }
  _M0L18scientificNotationS1112 = !_M0L6_2atmpS2998;
  if (_M0L18scientificNotationS1112) {
    int32_t _M0L7_2abindS1113 = _M0L7olengthS1110 - 1;
    int32_t _M0L1iS1114 = 0;
    int32_t _M0L6_2atmpS2884;
    uint64_t _M0L6_2atmpS2889;
    int32_t _M0L6_2atmpS2888;
    int32_t _M0L6_2atmpS2887;
    int32_t _M0L6_2atmpS2886;
    int32_t _M0L6_2atmpS2885;
    int32_t _M0L6_2atmpS2893;
    int32_t _M0L6_2atmpS2894;
    int32_t _M0L6_2atmpS2895;
    int32_t _M0L6_2atmpS2896;
    int32_t _M0L6_2atmpS2897;
    int32_t _M0L6_2atmpS2903;
    int32_t _M0L6_2atmpS2936;
    while (1) {
      if (_M0L1iS1114 < _M0L7_2abindS1113) {
        uint64_t _M0L6_2atmpS2882 = _M0Lm6outputS1108;
        uint64_t _M0L1cS1115 = _M0L6_2atmpS2882 % 10ull;
        uint64_t _M0L6_2atmpS2875 = _M0Lm6outputS1108;
        int32_t _M0L6_2atmpS2881;
        int32_t _M0L6_2atmpS2880;
        int32_t _M0L6_2atmpS2876;
        int32_t _M0L6_2atmpS2879;
        int32_t _M0L6_2atmpS2878;
        int32_t _M0L6_2atmpS2877;
        int32_t _M0L6_2atmpS2883;
        _M0Lm6outputS1108 = _M0L6_2atmpS2875 / 10ull;
        _M0L6_2atmpS2881 = _M0Lm5indexS1106;
        _M0L6_2atmpS2880 = _M0L6_2atmpS2881 + _M0L7olengthS1110;
        _M0L6_2atmpS2876 = _M0L6_2atmpS2880 - _M0L1iS1114;
        _M0L6_2atmpS2879 = (int32_t)_M0L1cS1115;
        _M0L6_2atmpS2878 = 48 + _M0L6_2atmpS2879;
        _M0L6_2atmpS2877 = _M0L6_2atmpS2878 & 0xff;
        if (
          _M0L6_2atmpS2876 < 0
          || _M0L6_2atmpS2876 >= Moonbit_array_length(_M0L6resultS1105)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1105[_M0L6_2atmpS2876] = _M0L6_2atmpS2877;
        _M0L6_2atmpS2883 = _M0L1iS1114 + 1;
        _M0L1iS1114 = _M0L6_2atmpS2883;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2884 = _M0Lm5indexS1106;
    _M0L6_2atmpS2889 = _M0Lm6outputS1108;
    _M0L6_2atmpS2888 = (int32_t)_M0L6_2atmpS2889;
    _M0L6_2atmpS2887 = _M0L6_2atmpS2888 % 10;
    _M0L6_2atmpS2886 = 48 + _M0L6_2atmpS2887;
    _M0L6_2atmpS2885 = _M0L6_2atmpS2886 & 0xff;
    if (
      _M0L6_2atmpS2884 < 0
      || _M0L6_2atmpS2884 >= Moonbit_array_length(_M0L6resultS1105)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1105[_M0L6_2atmpS2884] = _M0L6_2atmpS2885;
    if (_M0L7olengthS1110 > 1) {
      int32_t _M0L6_2atmpS2891 = _M0Lm5indexS1106;
      int32_t _M0L6_2atmpS2890 = _M0L6_2atmpS2891 + 1;
      if (
        _M0L6_2atmpS2890 < 0
        || _M0L6_2atmpS2890 >= Moonbit_array_length(_M0L6resultS1105)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1105[_M0L6_2atmpS2890] = 46;
    } else {
      int32_t _M0L6_2atmpS2892 = _M0Lm5indexS1106;
      _M0Lm5indexS1106 = _M0L6_2atmpS2892 - 1;
    }
    _M0L6_2atmpS2893 = _M0Lm5indexS1106;
    _M0L6_2atmpS2894 = _M0L7olengthS1110 + 1;
    _M0Lm5indexS1106 = _M0L6_2atmpS2893 + _M0L6_2atmpS2894;
    _M0L6_2atmpS2895 = _M0Lm5indexS1106;
    if (
      _M0L6_2atmpS2895 < 0
      || _M0L6_2atmpS2895 >= Moonbit_array_length(_M0L6resultS1105)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS1105[_M0L6_2atmpS2895] = 101;
    _M0L6_2atmpS2896 = _M0Lm5indexS1106;
    _M0Lm5indexS1106 = _M0L6_2atmpS2896 + 1;
    _M0L6_2atmpS2897 = _M0Lm3expS1111;
    if (_M0L6_2atmpS2897 < 0) {
      int32_t _M0L6_2atmpS2898 = _M0Lm5indexS1106;
      int32_t _M0L6_2atmpS2899;
      int32_t _M0L6_2atmpS2900;
      if (
        _M0L6_2atmpS2898 < 0
        || _M0L6_2atmpS2898 >= Moonbit_array_length(_M0L6resultS1105)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1105[_M0L6_2atmpS2898] = 45;
      _M0L6_2atmpS2899 = _M0Lm5indexS1106;
      _M0Lm5indexS1106 = _M0L6_2atmpS2899 + 1;
      _M0L6_2atmpS2900 = _M0Lm3expS1111;
      _M0Lm3expS1111 = -_M0L6_2atmpS2900;
    } else {
      int32_t _M0L6_2atmpS2901 = _M0Lm5indexS1106;
      int32_t _M0L6_2atmpS2902;
      if (
        _M0L6_2atmpS2901 < 0
        || _M0L6_2atmpS2901 >= Moonbit_array_length(_M0L6resultS1105)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1105[_M0L6_2atmpS2901] = 43;
      _M0L6_2atmpS2902 = _M0Lm5indexS1106;
      _M0Lm5indexS1106 = _M0L6_2atmpS2902 + 1;
    }
    _M0L6_2atmpS2903 = _M0Lm3expS1111;
    if (_M0L6_2atmpS2903 >= 100) {
      int32_t _M0L6_2atmpS2919 = _M0Lm3expS1111;
      int32_t _M0L1aS1117 = _M0L6_2atmpS2919 / 100;
      int32_t _M0L6_2atmpS2918 = _M0Lm3expS1111;
      int32_t _M0L6_2atmpS2917 = _M0L6_2atmpS2918 / 10;
      int32_t _M0L1bS1118 = _M0L6_2atmpS2917 % 10;
      int32_t _M0L6_2atmpS2916 = _M0Lm3expS1111;
      int32_t _M0L1cS1119 = _M0L6_2atmpS2916 % 10;
      int32_t _M0L6_2atmpS2904 = _M0Lm5indexS1106;
      int32_t _M0L6_2atmpS2906 = 48 + _M0L1aS1117;
      int32_t _M0L6_2atmpS2905 = _M0L6_2atmpS2906 & 0xff;
      int32_t _M0L6_2atmpS2910;
      int32_t _M0L6_2atmpS2907;
      int32_t _M0L6_2atmpS2909;
      int32_t _M0L6_2atmpS2908;
      int32_t _M0L6_2atmpS2914;
      int32_t _M0L6_2atmpS2911;
      int32_t _M0L6_2atmpS2913;
      int32_t _M0L6_2atmpS2912;
      int32_t _M0L6_2atmpS2915;
      if (
        _M0L6_2atmpS2904 < 0
        || _M0L6_2atmpS2904 >= Moonbit_array_length(_M0L6resultS1105)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1105[_M0L6_2atmpS2904] = _M0L6_2atmpS2905;
      _M0L6_2atmpS2910 = _M0Lm5indexS1106;
      _M0L6_2atmpS2907 = _M0L6_2atmpS2910 + 1;
      _M0L6_2atmpS2909 = 48 + _M0L1bS1118;
      _M0L6_2atmpS2908 = _M0L6_2atmpS2909 & 0xff;
      if (
        _M0L6_2atmpS2907 < 0
        || _M0L6_2atmpS2907 >= Moonbit_array_length(_M0L6resultS1105)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1105[_M0L6_2atmpS2907] = _M0L6_2atmpS2908;
      _M0L6_2atmpS2914 = _M0Lm5indexS1106;
      _M0L6_2atmpS2911 = _M0L6_2atmpS2914 + 2;
      _M0L6_2atmpS2913 = 48 + _M0L1cS1119;
      _M0L6_2atmpS2912 = _M0L6_2atmpS2913 & 0xff;
      if (
        _M0L6_2atmpS2911 < 0
        || _M0L6_2atmpS2911 >= Moonbit_array_length(_M0L6resultS1105)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1105[_M0L6_2atmpS2911] = _M0L6_2atmpS2912;
      _M0L6_2atmpS2915 = _M0Lm5indexS1106;
      _M0Lm5indexS1106 = _M0L6_2atmpS2915 + 3;
    } else {
      int32_t _M0L6_2atmpS2920 = _M0Lm3expS1111;
      if (_M0L6_2atmpS2920 >= 10) {
        int32_t _M0L6_2atmpS2930 = _M0Lm3expS1111;
        int32_t _M0L1aS1120 = _M0L6_2atmpS2930 / 10;
        int32_t _M0L6_2atmpS2929 = _M0Lm3expS1111;
        int32_t _M0L1bS1121 = _M0L6_2atmpS2929 % 10;
        int32_t _M0L6_2atmpS2921 = _M0Lm5indexS1106;
        int32_t _M0L6_2atmpS2923 = 48 + _M0L1aS1120;
        int32_t _M0L6_2atmpS2922 = _M0L6_2atmpS2923 & 0xff;
        int32_t _M0L6_2atmpS2927;
        int32_t _M0L6_2atmpS2924;
        int32_t _M0L6_2atmpS2926;
        int32_t _M0L6_2atmpS2925;
        int32_t _M0L6_2atmpS2928;
        if (
          _M0L6_2atmpS2921 < 0
          || _M0L6_2atmpS2921 >= Moonbit_array_length(_M0L6resultS1105)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1105[_M0L6_2atmpS2921] = _M0L6_2atmpS2922;
        _M0L6_2atmpS2927 = _M0Lm5indexS1106;
        _M0L6_2atmpS2924 = _M0L6_2atmpS2927 + 1;
        _M0L6_2atmpS2926 = 48 + _M0L1bS1121;
        _M0L6_2atmpS2925 = _M0L6_2atmpS2926 & 0xff;
        if (
          _M0L6_2atmpS2924 < 0
          || _M0L6_2atmpS2924 >= Moonbit_array_length(_M0L6resultS1105)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1105[_M0L6_2atmpS2924] = _M0L6_2atmpS2925;
        _M0L6_2atmpS2928 = _M0Lm5indexS1106;
        _M0Lm5indexS1106 = _M0L6_2atmpS2928 + 2;
      } else {
        int32_t _M0L6_2atmpS2931 = _M0Lm5indexS1106;
        int32_t _M0L6_2atmpS2934 = _M0Lm3expS1111;
        int32_t _M0L6_2atmpS2933 = 48 + _M0L6_2atmpS2934;
        int32_t _M0L6_2atmpS2932 = _M0L6_2atmpS2933 & 0xff;
        int32_t _M0L6_2atmpS2935;
        if (
          _M0L6_2atmpS2931 < 0
          || _M0L6_2atmpS2931 >= Moonbit_array_length(_M0L6resultS1105)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS1105[_M0L6_2atmpS2931] = _M0L6_2atmpS2932;
        _M0L6_2atmpS2935 = _M0Lm5indexS1106;
        _M0Lm5indexS1106 = _M0L6_2atmpS2935 + 1;
      }
    }
    _M0L6_2atmpS2936 = _M0Lm5indexS1106;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1105, 0, _M0L6_2atmpS2936);
  } else {
    int32_t _M0L6_2atmpS2937 = _M0Lm3expS1111;
    int32_t _M0L6_2atmpS2997;
    if (_M0L6_2atmpS2937 < 0) {
      int32_t _M0L6_2atmpS2938 = _M0Lm5indexS1106;
      int32_t _M0L6_2atmpS2939;
      int32_t _M0L6_2atmpS2940;
      int32_t _M0L6_2atmpS2941;
      int32_t _M0L1iS1122;
      int32_t _M0L7currentS1124;
      int32_t _M0L1iS1125;
      if (
        _M0L6_2atmpS2938 < 0
        || _M0L6_2atmpS2938 >= Moonbit_array_length(_M0L6resultS1105)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1105[_M0L6_2atmpS2938] = 48;
      _M0L6_2atmpS2939 = _M0Lm5indexS1106;
      _M0Lm5indexS1106 = _M0L6_2atmpS2939 + 1;
      _M0L6_2atmpS2940 = _M0Lm5indexS1106;
      if (
        _M0L6_2atmpS2940 < 0
        || _M0L6_2atmpS2940 >= Moonbit_array_length(_M0L6resultS1105)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS1105[_M0L6_2atmpS2940] = 46;
      _M0L6_2atmpS2941 = _M0Lm5indexS1106;
      _M0Lm5indexS1106 = _M0L6_2atmpS2941 + 1;
      _M0L1iS1122 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2942 = _M0Lm3expS1111;
        if (_M0L1iS1122 > _M0L6_2atmpS2942) {
          int32_t _M0L6_2atmpS2943 = _M0Lm5indexS1106;
          int32_t _M0L6_2atmpS2944;
          int32_t _M0L6_2atmpS2945;
          if (
            _M0L6_2atmpS2943 < 0
            || _M0L6_2atmpS2943 >= Moonbit_array_length(_M0L6resultS1105)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1105[_M0L6_2atmpS2943] = 48;
          _M0L6_2atmpS2944 = _M0Lm5indexS1106;
          _M0Lm5indexS1106 = _M0L6_2atmpS2944 + 1;
          _M0L6_2atmpS2945 = _M0L1iS1122 - 1;
          _M0L1iS1122 = _M0L6_2atmpS2945;
          continue;
        }
        break;
      }
      _M0L7currentS1124 = _M0Lm5indexS1106;
      _M0L1iS1125 = 0;
      while (1) {
        if (_M0L1iS1125 < _M0L7olengthS1110) {
          int32_t _M0L6_2atmpS2953 = _M0L7currentS1124 + _M0L7olengthS1110;
          int32_t _M0L6_2atmpS2952 = _M0L6_2atmpS2953 - _M0L1iS1125;
          int32_t _M0L6_2atmpS2946 = _M0L6_2atmpS2952 - 1;
          uint64_t _M0L6_2atmpS2951 = _M0Lm6outputS1108;
          uint64_t _M0L6_2atmpS2950 = _M0L6_2atmpS2951 % 10ull;
          int32_t _M0L6_2atmpS2949 = (int32_t)_M0L6_2atmpS2950;
          int32_t _M0L6_2atmpS2948 = 48 + _M0L6_2atmpS2949;
          int32_t _M0L6_2atmpS2947 = _M0L6_2atmpS2948 & 0xff;
          uint64_t _M0L6_2atmpS2954;
          int32_t _M0L6_2atmpS2955;
          int32_t _M0L6_2atmpS2956;
          if (
            _M0L6_2atmpS2946 < 0
            || _M0L6_2atmpS2946 >= Moonbit_array_length(_M0L6resultS1105)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS1105[_M0L6_2atmpS2946] = _M0L6_2atmpS2947;
          _M0L6_2atmpS2954 = _M0Lm6outputS1108;
          _M0Lm6outputS1108 = _M0L6_2atmpS2954 / 10ull;
          _M0L6_2atmpS2955 = _M0Lm5indexS1106;
          _M0Lm5indexS1106 = _M0L6_2atmpS2955 + 1;
          _M0L6_2atmpS2956 = _M0L1iS1125 + 1;
          _M0L1iS1125 = _M0L6_2atmpS2956;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2958 = _M0Lm3expS1111;
      int32_t _M0L6_2atmpS2957 = _M0L6_2atmpS2958 + 1;
      if (_M0L6_2atmpS2957 >= _M0L7olengthS1110) {
        int32_t _M0L1iS1127 = 0;
        int32_t _M0L6_2atmpS2970;
        int32_t _M0L6_2atmpS2974;
        int32_t _M0L7_2abindS1129;
        int32_t _M0L2__S1130;
        while (1) {
          if (_M0L1iS1127 < _M0L7olengthS1110) {
            int32_t _M0L6_2atmpS2967 = _M0Lm5indexS1106;
            int32_t _M0L6_2atmpS2966 = _M0L6_2atmpS2967 + _M0L7olengthS1110;
            int32_t _M0L6_2atmpS2965 = _M0L6_2atmpS2966 - _M0L1iS1127;
            int32_t _M0L6_2atmpS2959 = _M0L6_2atmpS2965 - 1;
            uint64_t _M0L6_2atmpS2964 = _M0Lm6outputS1108;
            uint64_t _M0L6_2atmpS2963 = _M0L6_2atmpS2964 % 10ull;
            int32_t _M0L6_2atmpS2962 = (int32_t)_M0L6_2atmpS2963;
            int32_t _M0L6_2atmpS2961 = 48 + _M0L6_2atmpS2962;
            int32_t _M0L6_2atmpS2960 = _M0L6_2atmpS2961 & 0xff;
            uint64_t _M0L6_2atmpS2968;
            int32_t _M0L6_2atmpS2969;
            if (
              _M0L6_2atmpS2959 < 0
              || _M0L6_2atmpS2959 >= Moonbit_array_length(_M0L6resultS1105)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1105[_M0L6_2atmpS2959] = _M0L6_2atmpS2960;
            _M0L6_2atmpS2968 = _M0Lm6outputS1108;
            _M0Lm6outputS1108 = _M0L6_2atmpS2968 / 10ull;
            _M0L6_2atmpS2969 = _M0L1iS1127 + 1;
            _M0L1iS1127 = _M0L6_2atmpS2969;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2970 = _M0Lm5indexS1106;
        _M0Lm5indexS1106 = _M0L6_2atmpS2970 + _M0L7olengthS1110;
        _M0L6_2atmpS2974 = _M0Lm3expS1111;
        _M0L7_2abindS1129 = _M0L6_2atmpS2974 + 1;
        _M0L2__S1130 = _M0L7olengthS1110;
        while (1) {
          if (_M0L2__S1130 < _M0L7_2abindS1129) {
            int32_t _M0L6_2atmpS2971 = _M0Lm5indexS1106;
            int32_t _M0L6_2atmpS2972;
            int32_t _M0L6_2atmpS2973;
            if (
              _M0L6_2atmpS2971 < 0
              || _M0L6_2atmpS2971 >= Moonbit_array_length(_M0L6resultS1105)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1105[_M0L6_2atmpS2971] = 48;
            _M0L6_2atmpS2972 = _M0Lm5indexS1106;
            _M0Lm5indexS1106 = _M0L6_2atmpS2972 + 1;
            _M0L6_2atmpS2973 = _M0L2__S1130 + 1;
            _M0L2__S1130 = _M0L6_2atmpS2973;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2996 = _M0Lm5indexS1106;
        int32_t _M0Lm7currentS1132 = _M0L6_2atmpS2996 + 1;
        int32_t _M0L1iS1133 = 0;
        int32_t _M0L6_2atmpS2994;
        int32_t _M0L6_2atmpS2995;
        while (1) {
          if (_M0L1iS1133 < _M0L7olengthS1110) {
            int32_t _M0L6_2atmpS2977 = _M0L7olengthS1110 - _M0L1iS1133;
            int32_t _M0L6_2atmpS2975 = _M0L6_2atmpS2977 - 1;
            int32_t _M0L6_2atmpS2976 = _M0Lm3expS1111;
            int32_t _M0L6_2atmpS2991;
            int32_t _M0L6_2atmpS2990;
            int32_t _M0L6_2atmpS2989;
            int32_t _M0L6_2atmpS2983;
            uint64_t _M0L6_2atmpS2988;
            uint64_t _M0L6_2atmpS2987;
            int32_t _M0L6_2atmpS2986;
            int32_t _M0L6_2atmpS2985;
            int32_t _M0L6_2atmpS2984;
            uint64_t _M0L6_2atmpS2992;
            int32_t _M0L6_2atmpS2993;
            if (_M0L6_2atmpS2975 == _M0L6_2atmpS2976) {
              int32_t _M0L6_2atmpS2981 = _M0Lm7currentS1132;
              int32_t _M0L6_2atmpS2980 = _M0L6_2atmpS2981 + _M0L7olengthS1110;
              int32_t _M0L6_2atmpS2979 = _M0L6_2atmpS2980 - _M0L1iS1133;
              int32_t _M0L6_2atmpS2978 = _M0L6_2atmpS2979 - 1;
              int32_t _M0L6_2atmpS2982;
              if (
                _M0L6_2atmpS2978 < 0
                || _M0L6_2atmpS2978 >= Moonbit_array_length(_M0L6resultS1105)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS1105[_M0L6_2atmpS2978] = 46;
              _M0L6_2atmpS2982 = _M0Lm7currentS1132;
              _M0Lm7currentS1132 = _M0L6_2atmpS2982 - 1;
            }
            _M0L6_2atmpS2991 = _M0Lm7currentS1132;
            _M0L6_2atmpS2990 = _M0L6_2atmpS2991 + _M0L7olengthS1110;
            _M0L6_2atmpS2989 = _M0L6_2atmpS2990 - _M0L1iS1133;
            _M0L6_2atmpS2983 = _M0L6_2atmpS2989 - 1;
            _M0L6_2atmpS2988 = _M0Lm6outputS1108;
            _M0L6_2atmpS2987 = _M0L6_2atmpS2988 % 10ull;
            _M0L6_2atmpS2986 = (int32_t)_M0L6_2atmpS2987;
            _M0L6_2atmpS2985 = 48 + _M0L6_2atmpS2986;
            _M0L6_2atmpS2984 = _M0L6_2atmpS2985 & 0xff;
            if (
              _M0L6_2atmpS2983 < 0
              || _M0L6_2atmpS2983 >= Moonbit_array_length(_M0L6resultS1105)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS1105[_M0L6_2atmpS2983] = _M0L6_2atmpS2984;
            _M0L6_2atmpS2992 = _M0Lm6outputS1108;
            _M0Lm6outputS1108 = _M0L6_2atmpS2992 / 10ull;
            _M0L6_2atmpS2993 = _M0L1iS1133 + 1;
            _M0L1iS1133 = _M0L6_2atmpS2993;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2994 = _M0Lm5indexS1106;
        _M0L6_2atmpS2995 = _M0L7olengthS1110 + 1;
        _M0Lm5indexS1106 = _M0L6_2atmpS2994 + _M0L6_2atmpS2995;
      }
    }
    _M0L6_2atmpS2997 = _M0Lm5indexS1106;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS1105, 0, _M0L6_2atmpS2997);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS1051,
  uint32_t _M0L12ieeeExponentS1050
) {
  int32_t _M0Lm2e2S1048;
  uint64_t _M0Lm2m2S1049;
  uint64_t _M0L6_2atmpS2872;
  uint64_t _M0L6_2atmpS2871;
  int32_t _M0L4evenS1052;
  uint64_t _M0L6_2atmpS2870;
  uint64_t _M0L2mvS1053;
  int32_t _M0L7mmShiftS1054;
  uint64_t _M0Lm2vrS1055;
  uint64_t _M0Lm2vpS1056;
  uint64_t _M0Lm2vmS1057;
  int32_t _M0Lm3e10S1058;
  int32_t _M0Lm17vmIsTrailingZerosS1059;
  int32_t _M0Lm17vrIsTrailingZerosS1060;
  int32_t _M0L6_2atmpS2772;
  int32_t _M0Lm7removedS1079;
  int32_t _M0Lm16lastRemovedDigitS1080;
  uint64_t _M0Lm6outputS1081;
  int32_t _M0L6_2atmpS2868;
  int32_t _M0L6_2atmpS2869;
  int32_t _M0L3expS1104;
  uint64_t _M0L6_2atmpS2867;
  struct _M0TPB17FloatingDecimal64* _block_4497;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S1048 = 0;
  _M0Lm2m2S1049 = 0ull;
  if (_M0L12ieeeExponentS1050 == 0u) {
    _M0Lm2e2S1048 = -1076;
    _M0Lm2m2S1049 = _M0L12ieeeMantissaS1051;
  } else {
    int32_t _M0L6_2atmpS2771 = *(int32_t*)&_M0L12ieeeExponentS1050;
    int32_t _M0L6_2atmpS2770 = _M0L6_2atmpS2771 - 1023;
    int32_t _M0L6_2atmpS2769 = _M0L6_2atmpS2770 - 52;
    _M0Lm2e2S1048 = _M0L6_2atmpS2769 - 2;
    _M0Lm2m2S1049 = 4503599627370496ull | _M0L12ieeeMantissaS1051;
  }
  _M0L6_2atmpS2872 = _M0Lm2m2S1049;
  _M0L6_2atmpS2871 = _M0L6_2atmpS2872 & 1ull;
  _M0L4evenS1052 = _M0L6_2atmpS2871 == 0ull;
  _M0L6_2atmpS2870 = _M0Lm2m2S1049;
  _M0L2mvS1053 = 4ull * _M0L6_2atmpS2870;
  if (_M0L12ieeeMantissaS1051 != 0ull) {
    _M0L7mmShiftS1054 = 1;
  } else {
    _M0L7mmShiftS1054 = _M0L12ieeeExponentS1050 <= 1u;
  }
  _M0Lm2vrS1055 = 0ull;
  _M0Lm2vpS1056 = 0ull;
  _M0Lm2vmS1057 = 0ull;
  _M0Lm3e10S1058 = 0;
  _M0Lm17vmIsTrailingZerosS1059 = 0;
  _M0Lm17vrIsTrailingZerosS1060 = 0;
  _M0L6_2atmpS2772 = _M0Lm2e2S1048;
  if (_M0L6_2atmpS2772 >= 0) {
    int32_t _M0L6_2atmpS2794 = _M0Lm2e2S1048;
    int32_t _M0L6_2atmpS2790;
    int32_t _M0L6_2atmpS2793;
    int32_t _M0L6_2atmpS2792;
    int32_t _M0L6_2atmpS2791;
    int32_t _M0L1qS1061;
    int32_t _M0L6_2atmpS2789;
    int32_t _M0L6_2atmpS2788;
    int32_t _M0L1kS1062;
    int32_t _M0L6_2atmpS2787;
    int32_t _M0L6_2atmpS2786;
    int32_t _M0L6_2atmpS2785;
    int32_t _M0L1iS1063;
    struct _M0TPB8Pow5Pair _M0L4pow5S1064;
    uint64_t _M0L6_2atmpS2784;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1065;
    uint64_t _M0L8_2avrOutS1066;
    uint64_t _M0L8_2avpOutS1067;
    uint64_t _M0L8_2avmOutS1068;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2790 = _M0FPB9log10Pow2(_M0L6_2atmpS2794);
    _M0L6_2atmpS2793 = _M0Lm2e2S1048;
    _M0L6_2atmpS2792 = _M0L6_2atmpS2793 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2791 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2792);
    _M0L1qS1061 = _M0L6_2atmpS2790 - _M0L6_2atmpS2791;
    _M0Lm3e10S1058 = _M0L1qS1061;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2789 = _M0FPB8pow5bits(_M0L1qS1061);
    _M0L6_2atmpS2788 = 125 + _M0L6_2atmpS2789;
    _M0L1kS1062 = _M0L6_2atmpS2788 - 1;
    _M0L6_2atmpS2787 = _M0Lm2e2S1048;
    _M0L6_2atmpS2786 = -_M0L6_2atmpS2787;
    _M0L6_2atmpS2785 = _M0L6_2atmpS2786 + _M0L1qS1061;
    _M0L1iS1063 = _M0L6_2atmpS2785 + _M0L1kS1062;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1064 = _M0FPB22double__computeInvPow5(_M0L1qS1061);
    _M0L6_2atmpS2784 = _M0Lm2m2S1049;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1065
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2784, _M0L4pow5S1064, _M0L1iS1063, _M0L7mmShiftS1054);
    _M0L8_2avrOutS1066 = _M0L7_2abindS1065.$0;
    _M0L8_2avpOutS1067 = _M0L7_2abindS1065.$1;
    _M0L8_2avmOutS1068 = _M0L7_2abindS1065.$2;
    _M0Lm2vrS1055 = _M0L8_2avrOutS1066;
    _M0Lm2vpS1056 = _M0L8_2avpOutS1067;
    _M0Lm2vmS1057 = _M0L8_2avmOutS1068;
    if (_M0L1qS1061 <= 21) {
      int32_t _M0L6_2atmpS2780 = (int32_t)_M0L2mvS1053;
      uint64_t _M0L6_2atmpS2783 = _M0L2mvS1053 / 5ull;
      int32_t _M0L6_2atmpS2782 = (int32_t)_M0L6_2atmpS2783;
      int32_t _M0L6_2atmpS2781 = 5 * _M0L6_2atmpS2782;
      int32_t _M0L6mvMod5S1069 = _M0L6_2atmpS2780 - _M0L6_2atmpS2781;
      if (_M0L6mvMod5S1069 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS1060
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS1053, _M0L1qS1061);
      } else if (_M0L4evenS1052) {
        uint64_t _M0L6_2atmpS2774 = _M0L2mvS1053 - 1ull;
        uint64_t _M0L6_2atmpS2775;
        uint64_t _M0L6_2atmpS2773;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2775 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS1054);
        _M0L6_2atmpS2773 = _M0L6_2atmpS2774 - _M0L6_2atmpS2775;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS1059
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2773, _M0L1qS1061);
      } else {
        uint64_t _M0L6_2atmpS2776 = _M0Lm2vpS1056;
        uint64_t _M0L6_2atmpS2779 = _M0L2mvS1053 + 2ull;
        int32_t _M0L6_2atmpS2778;
        uint64_t _M0L6_2atmpS2777;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2778
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2779, _M0L1qS1061);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2777 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2778);
        _M0Lm2vpS1056 = _M0L6_2atmpS2776 - _M0L6_2atmpS2777;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2808 = _M0Lm2e2S1048;
    int32_t _M0L6_2atmpS2807 = -_M0L6_2atmpS2808;
    int32_t _M0L6_2atmpS2802;
    int32_t _M0L6_2atmpS2806;
    int32_t _M0L6_2atmpS2805;
    int32_t _M0L6_2atmpS2804;
    int32_t _M0L6_2atmpS2803;
    int32_t _M0L1qS1070;
    int32_t _M0L6_2atmpS2795;
    int32_t _M0L6_2atmpS2801;
    int32_t _M0L6_2atmpS2800;
    int32_t _M0L1iS1071;
    int32_t _M0L6_2atmpS2799;
    int32_t _M0L1kS1072;
    int32_t _M0L1jS1073;
    struct _M0TPB8Pow5Pair _M0L4pow5S1074;
    uint64_t _M0L6_2atmpS2798;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS1075;
    uint64_t _M0L8_2avrOutS1076;
    uint64_t _M0L8_2avpOutS1077;
    uint64_t _M0L8_2avmOutS1078;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2802 = _M0FPB9log10Pow5(_M0L6_2atmpS2807);
    _M0L6_2atmpS2806 = _M0Lm2e2S1048;
    _M0L6_2atmpS2805 = -_M0L6_2atmpS2806;
    _M0L6_2atmpS2804 = _M0L6_2atmpS2805 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2803 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2804);
    _M0L1qS1070 = _M0L6_2atmpS2802 - _M0L6_2atmpS2803;
    _M0L6_2atmpS2795 = _M0Lm2e2S1048;
    _M0Lm3e10S1058 = _M0L1qS1070 + _M0L6_2atmpS2795;
    _M0L6_2atmpS2801 = _M0Lm2e2S1048;
    _M0L6_2atmpS2800 = -_M0L6_2atmpS2801;
    _M0L1iS1071 = _M0L6_2atmpS2800 - _M0L1qS1070;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2799 = _M0FPB8pow5bits(_M0L1iS1071);
    _M0L1kS1072 = _M0L6_2atmpS2799 - 125;
    _M0L1jS1073 = _M0L1qS1070 - _M0L1kS1072;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S1074 = _M0FPB19double__computePow5(_M0L1iS1071);
    _M0L6_2atmpS2798 = _M0Lm2m2S1049;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS1075
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2798, _M0L4pow5S1074, _M0L1jS1073, _M0L7mmShiftS1054);
    _M0L8_2avrOutS1076 = _M0L7_2abindS1075.$0;
    _M0L8_2avpOutS1077 = _M0L7_2abindS1075.$1;
    _M0L8_2avmOutS1078 = _M0L7_2abindS1075.$2;
    _M0Lm2vrS1055 = _M0L8_2avrOutS1076;
    _M0Lm2vpS1056 = _M0L8_2avpOutS1077;
    _M0Lm2vmS1057 = _M0L8_2avmOutS1078;
    if (_M0L1qS1070 <= 1) {
      _M0Lm17vrIsTrailingZerosS1060 = 1;
      if (_M0L4evenS1052) {
        int32_t _M0L6_2atmpS2796;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2796 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS1054);
        _M0Lm17vmIsTrailingZerosS1059 = _M0L6_2atmpS2796 == 1;
      } else {
        uint64_t _M0L6_2atmpS2797 = _M0Lm2vpS1056;
        _M0Lm2vpS1056 = _M0L6_2atmpS2797 - 1ull;
      }
    } else if (_M0L1qS1070 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS1060
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS1053, _M0L1qS1070);
    }
  }
  _M0Lm7removedS1079 = 0;
  _M0Lm16lastRemovedDigitS1080 = 0;
  _M0Lm6outputS1081 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS1059 || _M0Lm17vrIsTrailingZerosS1060) {
    int32_t _if__result_4494;
    uint64_t _M0L6_2atmpS2838;
    uint64_t _M0L6_2atmpS2844;
    uint64_t _M0L6_2atmpS2845;
    int32_t _if__result_4495;
    int32_t _M0L6_2atmpS2841;
    int64_t _M0L6_2atmpS2840;
    uint64_t _M0L6_2atmpS2839;
    while (1) {
      uint64_t _M0L6_2atmpS2821 = _M0Lm2vpS1056;
      uint64_t _M0L7vpDiv10S1082 = _M0L6_2atmpS2821 / 10ull;
      uint64_t _M0L6_2atmpS2820 = _M0Lm2vmS1057;
      uint64_t _M0L7vmDiv10S1083 = _M0L6_2atmpS2820 / 10ull;
      uint64_t _M0L6_2atmpS2819;
      int32_t _M0L6_2atmpS2816;
      int32_t _M0L6_2atmpS2818;
      int32_t _M0L6_2atmpS2817;
      int32_t _M0L7vmMod10S1085;
      uint64_t _M0L6_2atmpS2815;
      uint64_t _M0L7vrDiv10S1086;
      uint64_t _M0L6_2atmpS2814;
      int32_t _M0L6_2atmpS2811;
      int32_t _M0L6_2atmpS2813;
      int32_t _M0L6_2atmpS2812;
      int32_t _M0L7vrMod10S1087;
      int32_t _M0L6_2atmpS2810;
      if (_M0L7vpDiv10S1082 <= _M0L7vmDiv10S1083) {
        break;
      }
      _M0L6_2atmpS2819 = _M0Lm2vmS1057;
      _M0L6_2atmpS2816 = (int32_t)_M0L6_2atmpS2819;
      _M0L6_2atmpS2818 = (int32_t)_M0L7vmDiv10S1083;
      _M0L6_2atmpS2817 = 10 * _M0L6_2atmpS2818;
      _M0L7vmMod10S1085 = _M0L6_2atmpS2816 - _M0L6_2atmpS2817;
      _M0L6_2atmpS2815 = _M0Lm2vrS1055;
      _M0L7vrDiv10S1086 = _M0L6_2atmpS2815 / 10ull;
      _M0L6_2atmpS2814 = _M0Lm2vrS1055;
      _M0L6_2atmpS2811 = (int32_t)_M0L6_2atmpS2814;
      _M0L6_2atmpS2813 = (int32_t)_M0L7vrDiv10S1086;
      _M0L6_2atmpS2812 = 10 * _M0L6_2atmpS2813;
      _M0L7vrMod10S1087 = _M0L6_2atmpS2811 - _M0L6_2atmpS2812;
      if (_M0Lm17vmIsTrailingZerosS1059) {
        _M0Lm17vmIsTrailingZerosS1059 = _M0L7vmMod10S1085 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS1059 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS1060) {
        int32_t _M0L6_2atmpS2809 = _M0Lm16lastRemovedDigitS1080;
        _M0Lm17vrIsTrailingZerosS1060 = _M0L6_2atmpS2809 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS1060 = 0;
      }
      _M0Lm16lastRemovedDigitS1080 = _M0L7vrMod10S1087;
      _M0Lm2vrS1055 = _M0L7vrDiv10S1086;
      _M0Lm2vpS1056 = _M0L7vpDiv10S1082;
      _M0Lm2vmS1057 = _M0L7vmDiv10S1083;
      _M0L6_2atmpS2810 = _M0Lm7removedS1079;
      _M0Lm7removedS1079 = _M0L6_2atmpS2810 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS1059) {
      while (1) {
        uint64_t _M0L6_2atmpS2834 = _M0Lm2vmS1057;
        uint64_t _M0L7vmDiv10S1088 = _M0L6_2atmpS2834 / 10ull;
        uint64_t _M0L6_2atmpS2833 = _M0Lm2vmS1057;
        int32_t _M0L6_2atmpS2830 = (int32_t)_M0L6_2atmpS2833;
        int32_t _M0L6_2atmpS2832 = (int32_t)_M0L7vmDiv10S1088;
        int32_t _M0L6_2atmpS2831 = 10 * _M0L6_2atmpS2832;
        int32_t _M0L7vmMod10S1089 = _M0L6_2atmpS2830 - _M0L6_2atmpS2831;
        uint64_t _M0L6_2atmpS2829;
        uint64_t _M0L7vpDiv10S1091;
        uint64_t _M0L6_2atmpS2828;
        uint64_t _M0L7vrDiv10S1092;
        uint64_t _M0L6_2atmpS2827;
        int32_t _M0L6_2atmpS2824;
        int32_t _M0L6_2atmpS2826;
        int32_t _M0L6_2atmpS2825;
        int32_t _M0L7vrMod10S1093;
        int32_t _M0L6_2atmpS2823;
        if (_M0L7vmMod10S1089 != 0) {
          break;
        }
        _M0L6_2atmpS2829 = _M0Lm2vpS1056;
        _M0L7vpDiv10S1091 = _M0L6_2atmpS2829 / 10ull;
        _M0L6_2atmpS2828 = _M0Lm2vrS1055;
        _M0L7vrDiv10S1092 = _M0L6_2atmpS2828 / 10ull;
        _M0L6_2atmpS2827 = _M0Lm2vrS1055;
        _M0L6_2atmpS2824 = (int32_t)_M0L6_2atmpS2827;
        _M0L6_2atmpS2826 = (int32_t)_M0L7vrDiv10S1092;
        _M0L6_2atmpS2825 = 10 * _M0L6_2atmpS2826;
        _M0L7vrMod10S1093 = _M0L6_2atmpS2824 - _M0L6_2atmpS2825;
        if (_M0Lm17vrIsTrailingZerosS1060) {
          int32_t _M0L6_2atmpS2822 = _M0Lm16lastRemovedDigitS1080;
          _M0Lm17vrIsTrailingZerosS1060 = _M0L6_2atmpS2822 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS1060 = 0;
        }
        _M0Lm16lastRemovedDigitS1080 = _M0L7vrMod10S1093;
        _M0Lm2vrS1055 = _M0L7vrDiv10S1092;
        _M0Lm2vpS1056 = _M0L7vpDiv10S1091;
        _M0Lm2vmS1057 = _M0L7vmDiv10S1088;
        _M0L6_2atmpS2823 = _M0Lm7removedS1079;
        _M0Lm7removedS1079 = _M0L6_2atmpS2823 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS1060) {
      int32_t _M0L6_2atmpS2837 = _M0Lm16lastRemovedDigitS1080;
      if (_M0L6_2atmpS2837 == 5) {
        uint64_t _M0L6_2atmpS2836 = _M0Lm2vrS1055;
        uint64_t _M0L6_2atmpS2835 = _M0L6_2atmpS2836 % 2ull;
        _if__result_4494 = _M0L6_2atmpS2835 == 0ull;
      } else {
        _if__result_4494 = 0;
      }
    } else {
      _if__result_4494 = 0;
    }
    if (_if__result_4494) {
      _M0Lm16lastRemovedDigitS1080 = 4;
    }
    _M0L6_2atmpS2838 = _M0Lm2vrS1055;
    _M0L6_2atmpS2844 = _M0Lm2vrS1055;
    _M0L6_2atmpS2845 = _M0Lm2vmS1057;
    if (_M0L6_2atmpS2844 == _M0L6_2atmpS2845) {
      if (!_M0L4evenS1052) {
        _if__result_4495 = 1;
      } else {
        int32_t _M0L6_2atmpS2843 = _M0Lm17vmIsTrailingZerosS1059;
        _if__result_4495 = !_M0L6_2atmpS2843;
      }
    } else {
      _if__result_4495 = 0;
    }
    if (_if__result_4495) {
      _M0L6_2atmpS2841 = 1;
    } else {
      int32_t _M0L6_2atmpS2842 = _M0Lm16lastRemovedDigitS1080;
      _M0L6_2atmpS2841 = _M0L6_2atmpS2842 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2840 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2841);
    _M0L6_2atmpS2839 = *(uint64_t*)&_M0L6_2atmpS2840;
    _M0Lm6outputS1081 = _M0L6_2atmpS2838 + _M0L6_2atmpS2839;
  } else {
    int32_t _M0Lm7roundUpS1094 = 0;
    uint64_t _M0L6_2atmpS2866 = _M0Lm2vpS1056;
    uint64_t _M0L8vpDiv100S1095 = _M0L6_2atmpS2866 / 100ull;
    uint64_t _M0L6_2atmpS2865 = _M0Lm2vmS1057;
    uint64_t _M0L8vmDiv100S1096 = _M0L6_2atmpS2865 / 100ull;
    uint64_t _M0L6_2atmpS2860;
    uint64_t _M0L6_2atmpS2863;
    uint64_t _M0L6_2atmpS2864;
    int32_t _M0L6_2atmpS2862;
    uint64_t _M0L6_2atmpS2861;
    if (_M0L8vpDiv100S1095 > _M0L8vmDiv100S1096) {
      uint64_t _M0L6_2atmpS2851 = _M0Lm2vrS1055;
      uint64_t _M0L8vrDiv100S1097 = _M0L6_2atmpS2851 / 100ull;
      uint64_t _M0L6_2atmpS2850 = _M0Lm2vrS1055;
      int32_t _M0L6_2atmpS2847 = (int32_t)_M0L6_2atmpS2850;
      int32_t _M0L6_2atmpS2849 = (int32_t)_M0L8vrDiv100S1097;
      int32_t _M0L6_2atmpS2848 = 100 * _M0L6_2atmpS2849;
      int32_t _M0L8vrMod100S1098 = _M0L6_2atmpS2847 - _M0L6_2atmpS2848;
      int32_t _M0L6_2atmpS2846;
      _M0Lm7roundUpS1094 = _M0L8vrMod100S1098 >= 50;
      _M0Lm2vrS1055 = _M0L8vrDiv100S1097;
      _M0Lm2vpS1056 = _M0L8vpDiv100S1095;
      _M0Lm2vmS1057 = _M0L8vmDiv100S1096;
      _M0L6_2atmpS2846 = _M0Lm7removedS1079;
      _M0Lm7removedS1079 = _M0L6_2atmpS2846 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2859 = _M0Lm2vpS1056;
      uint64_t _M0L7vpDiv10S1099 = _M0L6_2atmpS2859 / 10ull;
      uint64_t _M0L6_2atmpS2858 = _M0Lm2vmS1057;
      uint64_t _M0L7vmDiv10S1100 = _M0L6_2atmpS2858 / 10ull;
      uint64_t _M0L6_2atmpS2857;
      uint64_t _M0L7vrDiv10S1102;
      uint64_t _M0L6_2atmpS2856;
      int32_t _M0L6_2atmpS2853;
      int32_t _M0L6_2atmpS2855;
      int32_t _M0L6_2atmpS2854;
      int32_t _M0L7vrMod10S1103;
      int32_t _M0L6_2atmpS2852;
      if (_M0L7vpDiv10S1099 <= _M0L7vmDiv10S1100) {
        break;
      }
      _M0L6_2atmpS2857 = _M0Lm2vrS1055;
      _M0L7vrDiv10S1102 = _M0L6_2atmpS2857 / 10ull;
      _M0L6_2atmpS2856 = _M0Lm2vrS1055;
      _M0L6_2atmpS2853 = (int32_t)_M0L6_2atmpS2856;
      _M0L6_2atmpS2855 = (int32_t)_M0L7vrDiv10S1102;
      _M0L6_2atmpS2854 = 10 * _M0L6_2atmpS2855;
      _M0L7vrMod10S1103 = _M0L6_2atmpS2853 - _M0L6_2atmpS2854;
      _M0Lm7roundUpS1094 = _M0L7vrMod10S1103 >= 5;
      _M0Lm2vrS1055 = _M0L7vrDiv10S1102;
      _M0Lm2vpS1056 = _M0L7vpDiv10S1099;
      _M0Lm2vmS1057 = _M0L7vmDiv10S1100;
      _M0L6_2atmpS2852 = _M0Lm7removedS1079;
      _M0Lm7removedS1079 = _M0L6_2atmpS2852 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2860 = _M0Lm2vrS1055;
    _M0L6_2atmpS2863 = _M0Lm2vrS1055;
    _M0L6_2atmpS2864 = _M0Lm2vmS1057;
    _M0L6_2atmpS2862
    = _M0L6_2atmpS2863 == _M0L6_2atmpS2864 || _M0Lm7roundUpS1094;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2861 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2862);
    _M0Lm6outputS1081 = _M0L6_2atmpS2860 + _M0L6_2atmpS2861;
  }
  _M0L6_2atmpS2868 = _M0Lm3e10S1058;
  _M0L6_2atmpS2869 = _M0Lm7removedS1079;
  _M0L3expS1104 = _M0L6_2atmpS2868 + _M0L6_2atmpS2869;
  _M0L6_2atmpS2867 = _M0Lm6outputS1081;
  _block_4497
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4497)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_4497->$0 = _M0L6_2atmpS2867;
  _block_4497->$1 = _M0L3expS1104;
  return _block_4497;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS1047) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1047) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS1046) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1046) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS1045) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS1045) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS1044) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS1044 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS1044 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS1044 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS1044 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS1044 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS1044 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS1044 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS1044 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS1044 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS1044 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS1044 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS1044 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS1044 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS1044 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS1044 >= 100ull) {
    return 3;
  }
  if (_M0L1vS1044 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS1027) {
  int32_t _M0L6_2atmpS2768;
  int32_t _M0L6_2atmpS2767;
  int32_t _M0L4baseS1026;
  int32_t _M0L5base2S1028;
  int32_t _M0L6offsetS1029;
  int32_t _M0L6_2atmpS2766;
  uint64_t _M0L4mul0S1030;
  int32_t _M0L6_2atmpS2765;
  int32_t _M0L6_2atmpS2764;
  uint64_t _M0L4mul1S1031;
  uint64_t _M0L1mS1032;
  struct _M0TPB7Umul128 _M0L7_2abindS1033;
  uint64_t _M0L7_2alow1S1034;
  uint64_t _M0L8_2ahigh1S1035;
  struct _M0TPB7Umul128 _M0L7_2abindS1036;
  uint64_t _M0L7_2alow0S1037;
  uint64_t _M0L8_2ahigh0S1038;
  uint64_t _M0L3sumS1039;
  uint64_t _M0Lm5high1S1040;
  int32_t _M0L6_2atmpS2762;
  int32_t _M0L6_2atmpS2763;
  int32_t _M0L5deltaS1041;
  uint64_t _M0L6_2atmpS2761;
  uint64_t _M0L6_2atmpS2753;
  int32_t _M0L6_2atmpS2760;
  uint32_t _M0L6_2atmpS2757;
  int32_t _M0L6_2atmpS2759;
  int32_t _M0L6_2atmpS2758;
  uint32_t _M0L6_2atmpS2756;
  uint32_t _M0L6_2atmpS2755;
  uint64_t _M0L6_2atmpS2754;
  uint64_t _M0L1aS1042;
  uint64_t _M0L6_2atmpS2752;
  uint64_t _M0L1bS1043;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2768 = _M0L1iS1027 + 26;
  _M0L6_2atmpS2767 = _M0L6_2atmpS2768 - 1;
  _M0L4baseS1026 = _M0L6_2atmpS2767 / 26;
  _M0L5base2S1028 = _M0L4baseS1026 * 26;
  _M0L6offsetS1029 = _M0L5base2S1028 - _M0L1iS1027;
  _M0L6_2atmpS2766 = _M0L4baseS1026 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S1030
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2766);
  _M0L6_2atmpS2765 = _M0L4baseS1026 * 2;
  _M0L6_2atmpS2764 = _M0L6_2atmpS2765 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S1031
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2764);
  if (_M0L6offsetS1029 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S1030, _M0L4mul1S1031};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS1032
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1029);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1033 = _M0FPB7umul128(_M0L1mS1032, _M0L4mul1S1031);
  _M0L7_2alow1S1034 = _M0L7_2abindS1033.$0;
  _M0L8_2ahigh1S1035 = _M0L7_2abindS1033.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1036 = _M0FPB7umul128(_M0L1mS1032, _M0L4mul0S1030);
  _M0L7_2alow0S1037 = _M0L7_2abindS1036.$0;
  _M0L8_2ahigh0S1038 = _M0L7_2abindS1036.$1;
  _M0L3sumS1039 = _M0L8_2ahigh0S1038 + _M0L7_2alow1S1034;
  _M0Lm5high1S1040 = _M0L8_2ahigh1S1035;
  if (_M0L3sumS1039 < _M0L8_2ahigh0S1038) {
    uint64_t _M0L6_2atmpS2751 = _M0Lm5high1S1040;
    _M0Lm5high1S1040 = _M0L6_2atmpS2751 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2762 = _M0FPB8pow5bits(_M0L5base2S1028);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2763 = _M0FPB8pow5bits(_M0L1iS1027);
  _M0L5deltaS1041 = _M0L6_2atmpS2762 - _M0L6_2atmpS2763;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2761
  = _M0FPB13shiftright128(_M0L7_2alow0S1037, _M0L3sumS1039, _M0L5deltaS1041);
  _M0L6_2atmpS2753 = _M0L6_2atmpS2761 + 1ull;
  _M0L6_2atmpS2760 = _M0L1iS1027 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2757
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2760);
  _M0L6_2atmpS2759 = _M0L1iS1027 % 16;
  _M0L6_2atmpS2758 = _M0L6_2atmpS2759 << 1;
  _M0L6_2atmpS2756 = _M0L6_2atmpS2757 >> (_M0L6_2atmpS2758 & 31);
  _M0L6_2atmpS2755 = _M0L6_2atmpS2756 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2754 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2755);
  _M0L1aS1042 = _M0L6_2atmpS2753 + _M0L6_2atmpS2754;
  _M0L6_2atmpS2752 = _M0Lm5high1S1040;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS1043
  = _M0FPB13shiftright128(_M0L3sumS1039, _M0L6_2atmpS2752, _M0L5deltaS1041);
  return (struct _M0TPB8Pow5Pair){_M0L1aS1042, _M0L1bS1043};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS1009) {
  int32_t _M0L4baseS1008;
  int32_t _M0L5base2S1010;
  int32_t _M0L6offsetS1011;
  int32_t _M0L6_2atmpS2750;
  uint64_t _M0L4mul0S1012;
  int32_t _M0L6_2atmpS2749;
  int32_t _M0L6_2atmpS2748;
  uint64_t _M0L4mul1S1013;
  uint64_t _M0L1mS1014;
  struct _M0TPB7Umul128 _M0L7_2abindS1015;
  uint64_t _M0L7_2alow1S1016;
  uint64_t _M0L8_2ahigh1S1017;
  struct _M0TPB7Umul128 _M0L7_2abindS1018;
  uint64_t _M0L7_2alow0S1019;
  uint64_t _M0L8_2ahigh0S1020;
  uint64_t _M0L3sumS1021;
  uint64_t _M0Lm5high1S1022;
  int32_t _M0L6_2atmpS2746;
  int32_t _M0L6_2atmpS2747;
  int32_t _M0L5deltaS1023;
  uint64_t _M0L6_2atmpS2738;
  int32_t _M0L6_2atmpS2745;
  uint32_t _M0L6_2atmpS2742;
  int32_t _M0L6_2atmpS2744;
  int32_t _M0L6_2atmpS2743;
  uint32_t _M0L6_2atmpS2741;
  uint32_t _M0L6_2atmpS2740;
  uint64_t _M0L6_2atmpS2739;
  uint64_t _M0L1aS1024;
  uint64_t _M0L6_2atmpS2737;
  uint64_t _M0L1bS1025;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS1008 = _M0L1iS1009 / 26;
  _M0L5base2S1010 = _M0L4baseS1008 * 26;
  _M0L6offsetS1011 = _M0L1iS1009 - _M0L5base2S1010;
  _M0L6_2atmpS2750 = _M0L4baseS1008 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S1012
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2750);
  _M0L6_2atmpS2749 = _M0L4baseS1008 * 2;
  _M0L6_2atmpS2748 = _M0L6_2atmpS2749 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S1013
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2748);
  if (_M0L6offsetS1011 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S1012, _M0L4mul1S1013};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS1014
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS1011);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1015 = _M0FPB7umul128(_M0L1mS1014, _M0L4mul1S1013);
  _M0L7_2alow1S1016 = _M0L7_2abindS1015.$0;
  _M0L8_2ahigh1S1017 = _M0L7_2abindS1015.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS1018 = _M0FPB7umul128(_M0L1mS1014, _M0L4mul0S1012);
  _M0L7_2alow0S1019 = _M0L7_2abindS1018.$0;
  _M0L8_2ahigh0S1020 = _M0L7_2abindS1018.$1;
  _M0L3sumS1021 = _M0L8_2ahigh0S1020 + _M0L7_2alow1S1016;
  _M0Lm5high1S1022 = _M0L8_2ahigh1S1017;
  if (_M0L3sumS1021 < _M0L8_2ahigh0S1020) {
    uint64_t _M0L6_2atmpS2736 = _M0Lm5high1S1022;
    _M0Lm5high1S1022 = _M0L6_2atmpS2736 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2746 = _M0FPB8pow5bits(_M0L1iS1009);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2747 = _M0FPB8pow5bits(_M0L5base2S1010);
  _M0L5deltaS1023 = _M0L6_2atmpS2746 - _M0L6_2atmpS2747;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2738
  = _M0FPB13shiftright128(_M0L7_2alow0S1019, _M0L3sumS1021, _M0L5deltaS1023);
  _M0L6_2atmpS2745 = _M0L1iS1009 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2742
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2745);
  _M0L6_2atmpS2744 = _M0L1iS1009 % 16;
  _M0L6_2atmpS2743 = _M0L6_2atmpS2744 << 1;
  _M0L6_2atmpS2741 = _M0L6_2atmpS2742 >> (_M0L6_2atmpS2743 & 31);
  _M0L6_2atmpS2740 = _M0L6_2atmpS2741 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2739 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2740);
  _M0L1aS1024 = _M0L6_2atmpS2738 + _M0L6_2atmpS2739;
  _M0L6_2atmpS2737 = _M0Lm5high1S1022;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS1025
  = _M0FPB13shiftright128(_M0L3sumS1021, _M0L6_2atmpS2737, _M0L5deltaS1023);
  return (struct _M0TPB8Pow5Pair){_M0L1aS1024, _M0L1bS1025};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS982,
  struct _M0TPB8Pow5Pair _M0L3mulS979,
  int32_t _M0L1jS995,
  int32_t _M0L7mmShiftS997
) {
  uint64_t _M0L7_2amul0S978;
  uint64_t _M0L7_2amul1S980;
  uint64_t _M0L1mS981;
  struct _M0TPB7Umul128 _M0L7_2abindS983;
  uint64_t _M0L5_2aloS984;
  uint64_t _M0L6_2atmpS985;
  struct _M0TPB7Umul128 _M0L7_2abindS986;
  uint64_t _M0L6_2alo2S987;
  uint64_t _M0L6_2ahi2S988;
  uint64_t _M0L3midS989;
  uint64_t _M0L6_2atmpS2735;
  uint64_t _M0L2hiS990;
  uint64_t _M0L3lo2S991;
  uint64_t _M0L6_2atmpS2733;
  uint64_t _M0L6_2atmpS2734;
  uint64_t _M0L4mid2S992;
  uint64_t _M0L6_2atmpS2732;
  uint64_t _M0L3hi2S993;
  int32_t _M0L6_2atmpS2731;
  int32_t _M0L6_2atmpS2730;
  uint64_t _M0L2vpS994;
  uint64_t _M0Lm2vmS996;
  int32_t _M0L6_2atmpS2729;
  int32_t _M0L6_2atmpS2728;
  uint64_t _M0L2vrS1007;
  uint64_t _M0L6_2atmpS2727;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S978 = _M0L3mulS979.$0;
  _M0L7_2amul1S980 = _M0L3mulS979.$1;
  _M0L1mS981 = _M0L1mS982 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS983 = _M0FPB7umul128(_M0L1mS981, _M0L7_2amul0S978);
  _M0L5_2aloS984 = _M0L7_2abindS983.$0;
  _M0L6_2atmpS985 = _M0L7_2abindS983.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS986 = _M0FPB7umul128(_M0L1mS981, _M0L7_2amul1S980);
  _M0L6_2alo2S987 = _M0L7_2abindS986.$0;
  _M0L6_2ahi2S988 = _M0L7_2abindS986.$1;
  _M0L3midS989 = _M0L6_2atmpS985 + _M0L6_2alo2S987;
  if (_M0L3midS989 < _M0L6_2atmpS985) {
    _M0L6_2atmpS2735 = 1ull;
  } else {
    _M0L6_2atmpS2735 = 0ull;
  }
  _M0L2hiS990 = _M0L6_2ahi2S988 + _M0L6_2atmpS2735;
  _M0L3lo2S991 = _M0L5_2aloS984 + _M0L7_2amul0S978;
  _M0L6_2atmpS2733 = _M0L3midS989 + _M0L7_2amul1S980;
  if (_M0L3lo2S991 < _M0L5_2aloS984) {
    _M0L6_2atmpS2734 = 1ull;
  } else {
    _M0L6_2atmpS2734 = 0ull;
  }
  _M0L4mid2S992 = _M0L6_2atmpS2733 + _M0L6_2atmpS2734;
  if (_M0L4mid2S992 < _M0L3midS989) {
    _M0L6_2atmpS2732 = 1ull;
  } else {
    _M0L6_2atmpS2732 = 0ull;
  }
  _M0L3hi2S993 = _M0L2hiS990 + _M0L6_2atmpS2732;
  _M0L6_2atmpS2731 = _M0L1jS995 - 64;
  _M0L6_2atmpS2730 = _M0L6_2atmpS2731 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS994
  = _M0FPB13shiftright128(_M0L4mid2S992, _M0L3hi2S993, _M0L6_2atmpS2730);
  _M0Lm2vmS996 = 0ull;
  if (_M0L7mmShiftS997) {
    uint64_t _M0L3lo3S998 = _M0L5_2aloS984 - _M0L7_2amul0S978;
    uint64_t _M0L6_2atmpS2717 = _M0L3midS989 - _M0L7_2amul1S980;
    uint64_t _M0L6_2atmpS2718;
    uint64_t _M0L4mid3S999;
    uint64_t _M0L6_2atmpS2716;
    uint64_t _M0L3hi3S1000;
    int32_t _M0L6_2atmpS2715;
    int32_t _M0L6_2atmpS2714;
    if (_M0L5_2aloS984 < _M0L3lo3S998) {
      _M0L6_2atmpS2718 = 1ull;
    } else {
      _M0L6_2atmpS2718 = 0ull;
    }
    _M0L4mid3S999 = _M0L6_2atmpS2717 - _M0L6_2atmpS2718;
    if (_M0L3midS989 < _M0L4mid3S999) {
      _M0L6_2atmpS2716 = 1ull;
    } else {
      _M0L6_2atmpS2716 = 0ull;
    }
    _M0L3hi3S1000 = _M0L2hiS990 - _M0L6_2atmpS2716;
    _M0L6_2atmpS2715 = _M0L1jS995 - 64;
    _M0L6_2atmpS2714 = _M0L6_2atmpS2715 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS996
    = _M0FPB13shiftright128(_M0L4mid3S999, _M0L3hi3S1000, _M0L6_2atmpS2714);
  } else {
    uint64_t _M0L3lo3S1001 = _M0L5_2aloS984 + _M0L5_2aloS984;
    uint64_t _M0L6_2atmpS2725 = _M0L3midS989 + _M0L3midS989;
    uint64_t _M0L6_2atmpS2726;
    uint64_t _M0L4mid3S1002;
    uint64_t _M0L6_2atmpS2723;
    uint64_t _M0L6_2atmpS2724;
    uint64_t _M0L3hi3S1003;
    uint64_t _M0L3lo4S1004;
    uint64_t _M0L6_2atmpS2721;
    uint64_t _M0L6_2atmpS2722;
    uint64_t _M0L4mid4S1005;
    uint64_t _M0L6_2atmpS2720;
    uint64_t _M0L3hi4S1006;
    int32_t _M0L6_2atmpS2719;
    if (_M0L3lo3S1001 < _M0L5_2aloS984) {
      _M0L6_2atmpS2726 = 1ull;
    } else {
      _M0L6_2atmpS2726 = 0ull;
    }
    _M0L4mid3S1002 = _M0L6_2atmpS2725 + _M0L6_2atmpS2726;
    _M0L6_2atmpS2723 = _M0L2hiS990 + _M0L2hiS990;
    if (_M0L4mid3S1002 < _M0L3midS989) {
      _M0L6_2atmpS2724 = 1ull;
    } else {
      _M0L6_2atmpS2724 = 0ull;
    }
    _M0L3hi3S1003 = _M0L6_2atmpS2723 + _M0L6_2atmpS2724;
    _M0L3lo4S1004 = _M0L3lo3S1001 - _M0L7_2amul0S978;
    _M0L6_2atmpS2721 = _M0L4mid3S1002 - _M0L7_2amul1S980;
    if (_M0L3lo3S1001 < _M0L3lo4S1004) {
      _M0L6_2atmpS2722 = 1ull;
    } else {
      _M0L6_2atmpS2722 = 0ull;
    }
    _M0L4mid4S1005 = _M0L6_2atmpS2721 - _M0L6_2atmpS2722;
    if (_M0L4mid3S1002 < _M0L4mid4S1005) {
      _M0L6_2atmpS2720 = 1ull;
    } else {
      _M0L6_2atmpS2720 = 0ull;
    }
    _M0L3hi4S1006 = _M0L3hi3S1003 - _M0L6_2atmpS2720;
    _M0L6_2atmpS2719 = _M0L1jS995 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS996
    = _M0FPB13shiftright128(_M0L4mid4S1005, _M0L3hi4S1006, _M0L6_2atmpS2719);
  }
  _M0L6_2atmpS2729 = _M0L1jS995 - 64;
  _M0L6_2atmpS2728 = _M0L6_2atmpS2729 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS1007
  = _M0FPB13shiftright128(_M0L3midS989, _M0L2hiS990, _M0L6_2atmpS2728);
  _M0L6_2atmpS2727 = _M0Lm2vmS996;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS1007,
                                                _M0L2vpS994,
                                                _M0L6_2atmpS2727};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS976,
  int32_t _M0L1pS977
) {
  uint64_t _M0L6_2atmpS2713;
  uint64_t _M0L6_2atmpS2712;
  uint64_t _M0L6_2atmpS2711;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2713 = 1ull << (_M0L1pS977 & 63);
  _M0L6_2atmpS2712 = _M0L6_2atmpS2713 - 1ull;
  _M0L6_2atmpS2711 = _M0L5valueS976 & _M0L6_2atmpS2712;
  return _M0L6_2atmpS2711 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS974,
  int32_t _M0L1pS975
) {
  int32_t _M0L6_2atmpS2710;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2710 = _M0FPB10pow5Factor(_M0L5valueS974);
  return _M0L6_2atmpS2710 >= _M0L1pS975;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS970) {
  uint64_t _M0L6_2atmpS2698;
  uint64_t _M0L6_2atmpS2699;
  uint64_t _M0L6_2atmpS2700;
  uint64_t _M0L6_2atmpS2701;
  int32_t _M0Lm5countS971;
  uint64_t _M0Lm5valueS972;
  uint64_t _M0L6_2atmpS2709;
  moonbit_string_t _M0L6_2atmpS2708;
  moonbit_string_t _M0L6_2atmpS3963;
  moonbit_string_t _M0L6_2atmpS2707;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2698 = _M0L5valueS970 % 5ull;
  if (_M0L6_2atmpS2698 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2699 = _M0L5valueS970 % 25ull;
  if (_M0L6_2atmpS2699 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2700 = _M0L5valueS970 % 125ull;
  if (_M0L6_2atmpS2700 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2701 = _M0L5valueS970 % 625ull;
  if (_M0L6_2atmpS2701 != 0ull) {
    return 3;
  }
  _M0Lm5countS971 = 4;
  _M0Lm5valueS972 = _M0L5valueS970 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2702 = _M0Lm5valueS972;
    if (_M0L6_2atmpS2702 > 0ull) {
      uint64_t _M0L6_2atmpS2704 = _M0Lm5valueS972;
      uint64_t _M0L6_2atmpS2703 = _M0L6_2atmpS2704 % 5ull;
      uint64_t _M0L6_2atmpS2705;
      int32_t _M0L6_2atmpS2706;
      if (_M0L6_2atmpS2703 != 0ull) {
        return _M0Lm5countS971;
      }
      _M0L6_2atmpS2705 = _M0Lm5valueS972;
      _M0Lm5valueS972 = _M0L6_2atmpS2705 / 5ull;
      _M0L6_2atmpS2706 = _M0Lm5countS971;
      _M0Lm5countS971 = _M0L6_2atmpS2706 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2709 = _M0Lm5valueS972;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2708
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2709);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3963
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_106.data, _M0L6_2atmpS2708);
  moonbit_decref(_M0L6_2atmpS2708);
  _M0L6_2atmpS2707 = _M0L6_2atmpS3963;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2707, (moonbit_string_t)moonbit_string_literal_107.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS969,
  uint64_t _M0L2hiS967,
  int32_t _M0L4distS968
) {
  int32_t _M0L6_2atmpS2697;
  uint64_t _M0L6_2atmpS2695;
  uint64_t _M0L6_2atmpS2696;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2697 = 64 - _M0L4distS968;
  _M0L6_2atmpS2695 = _M0L2hiS967 << (_M0L6_2atmpS2697 & 63);
  _M0L6_2atmpS2696 = _M0L2loS969 >> (_M0L4distS968 & 63);
  return _M0L6_2atmpS2695 | _M0L6_2atmpS2696;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS957,
  uint64_t _M0L1bS960
) {
  uint64_t _M0L3aLoS956;
  uint64_t _M0L3aHiS958;
  uint64_t _M0L3bLoS959;
  uint64_t _M0L3bHiS961;
  uint64_t _M0L1xS962;
  uint64_t _M0L6_2atmpS2693;
  uint64_t _M0L6_2atmpS2694;
  uint64_t _M0L1yS963;
  uint64_t _M0L6_2atmpS2691;
  uint64_t _M0L6_2atmpS2692;
  uint64_t _M0L1zS964;
  uint64_t _M0L6_2atmpS2689;
  uint64_t _M0L6_2atmpS2690;
  uint64_t _M0L6_2atmpS2687;
  uint64_t _M0L6_2atmpS2688;
  uint64_t _M0L1wS965;
  uint64_t _M0L2loS966;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS956 = _M0L1aS957 & 4294967295ull;
  _M0L3aHiS958 = _M0L1aS957 >> 32;
  _M0L3bLoS959 = _M0L1bS960 & 4294967295ull;
  _M0L3bHiS961 = _M0L1bS960 >> 32;
  _M0L1xS962 = _M0L3aLoS956 * _M0L3bLoS959;
  _M0L6_2atmpS2693 = _M0L3aHiS958 * _M0L3bLoS959;
  _M0L6_2atmpS2694 = _M0L1xS962 >> 32;
  _M0L1yS963 = _M0L6_2atmpS2693 + _M0L6_2atmpS2694;
  _M0L6_2atmpS2691 = _M0L3aLoS956 * _M0L3bHiS961;
  _M0L6_2atmpS2692 = _M0L1yS963 & 4294967295ull;
  _M0L1zS964 = _M0L6_2atmpS2691 + _M0L6_2atmpS2692;
  _M0L6_2atmpS2689 = _M0L3aHiS958 * _M0L3bHiS961;
  _M0L6_2atmpS2690 = _M0L1yS963 >> 32;
  _M0L6_2atmpS2687 = _M0L6_2atmpS2689 + _M0L6_2atmpS2690;
  _M0L6_2atmpS2688 = _M0L1zS964 >> 32;
  _M0L1wS965 = _M0L6_2atmpS2687 + _M0L6_2atmpS2688;
  _M0L2loS966 = _M0L1aS957 * _M0L1bS960;
  return (struct _M0TPB7Umul128){_M0L2loS966, _M0L1wS965};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS951,
  int32_t _M0L4fromS955,
  int32_t _M0L2toS953
) {
  int32_t _M0L6_2atmpS2686;
  struct _M0TPB13StringBuilder* _M0L3bufS950;
  int32_t _M0L1iS952;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2686 = Moonbit_array_length(_M0L5bytesS951);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS950 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2686);
  _M0L1iS952 = _M0L4fromS955;
  while (1) {
    if (_M0L1iS952 < _M0L2toS953) {
      int32_t _M0L6_2atmpS2684;
      int32_t _M0L6_2atmpS2683;
      int32_t _M0L6_2atmpS2685;
      if (
        _M0L1iS952 < 0 || _M0L1iS952 >= Moonbit_array_length(_M0L5bytesS951)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2684 = (int32_t)_M0L5bytesS951[_M0L1iS952];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2683 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2684);
      moonbit_incref(_M0L3bufS950);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS950, _M0L6_2atmpS2683);
      _M0L6_2atmpS2685 = _M0L1iS952 + 1;
      _M0L1iS952 = _M0L6_2atmpS2685;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS951);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS950);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS949) {
  int32_t _M0L6_2atmpS2682;
  uint32_t _M0L6_2atmpS2681;
  uint32_t _M0L6_2atmpS2680;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2682 = _M0L1eS949 * 78913;
  _M0L6_2atmpS2681 = *(uint32_t*)&_M0L6_2atmpS2682;
  _M0L6_2atmpS2680 = _M0L6_2atmpS2681 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2680;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS948) {
  int32_t _M0L6_2atmpS2679;
  uint32_t _M0L6_2atmpS2678;
  uint32_t _M0L6_2atmpS2677;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2679 = _M0L1eS948 * 732923;
  _M0L6_2atmpS2678 = *(uint32_t*)&_M0L6_2atmpS2679;
  _M0L6_2atmpS2677 = _M0L6_2atmpS2678 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2677;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS946,
  int32_t _M0L8exponentS947,
  int32_t _M0L8mantissaS944
) {
  moonbit_string_t _M0L1sS945;
  moonbit_string_t _M0L6_2atmpS3964;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS944) {
    return (moonbit_string_t)moonbit_string_literal_108.data;
  }
  if (_M0L4signS946) {
    _M0L1sS945 = (moonbit_string_t)moonbit_string_literal_109.data;
  } else {
    _M0L1sS945 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS947) {
    moonbit_string_t _M0L6_2atmpS3965;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3965
    = moonbit_add_string(_M0L1sS945, (moonbit_string_t)moonbit_string_literal_110.data);
    moonbit_decref(_M0L1sS945);
    return _M0L6_2atmpS3965;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3964
  = moonbit_add_string(_M0L1sS945, (moonbit_string_t)moonbit_string_literal_111.data);
  moonbit_decref(_M0L1sS945);
  return _M0L6_2atmpS3964;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS943) {
  int32_t _M0L6_2atmpS2676;
  uint32_t _M0L6_2atmpS2675;
  uint32_t _M0L6_2atmpS2674;
  int32_t _M0L6_2atmpS2673;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2676 = _M0L1eS943 * 1217359;
  _M0L6_2atmpS2675 = *(uint32_t*)&_M0L6_2atmpS2676;
  _M0L6_2atmpS2674 = _M0L6_2atmpS2675 >> 19;
  _M0L6_2atmpS2673 = *(int32_t*)&_M0L6_2atmpS2674;
  return _M0L6_2atmpS2673 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS942,
  struct _M0TPB6Hasher* _M0L6hasherS941
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS941, _M0L4selfS942);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS940,
  struct _M0TPB6Hasher* _M0L6hasherS939
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS939, _M0L4selfS940);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS937,
  moonbit_string_t _M0L5valueS935
) {
  int32_t _M0L7_2abindS934;
  int32_t _M0L1iS936;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS934 = Moonbit_array_length(_M0L5valueS935);
  _M0L1iS936 = 0;
  while (1) {
    if (_M0L1iS936 < _M0L7_2abindS934) {
      int32_t _M0L6_2atmpS2671 = _M0L5valueS935[_M0L1iS936];
      int32_t _M0L6_2atmpS2670 = (int32_t)_M0L6_2atmpS2671;
      uint32_t _M0L6_2atmpS2669 = *(uint32_t*)&_M0L6_2atmpS2670;
      int32_t _M0L6_2atmpS2672;
      moonbit_incref(_M0L4selfS937);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS937, _M0L6_2atmpS2669);
      _M0L6_2atmpS2672 = _M0L1iS936 + 1;
      _M0L1iS936 = _M0L6_2atmpS2672;
      continue;
    } else {
      moonbit_decref(_M0L4selfS937);
      moonbit_decref(_M0L5valueS935);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS932,
  int32_t _M0L3idxS933
) {
  int32_t _M0L6_2atmpS3966;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3966 = _M0L4selfS932[_M0L3idxS933];
  moonbit_decref(_M0L4selfS932);
  return _M0L6_2atmpS3966;
}

struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MPB4Iter11filter__mapGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS925,
  struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L1fS929
) {
  struct _M0R195Iter_3a_3afilter__map_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_2c_20clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoice_5d_7c_2eanon__u2665__l364__* _closure_4501;
  #line 363 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _closure_4501
  = (struct _M0R195Iter_3a_3afilter__map_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_2c_20clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoice_5d_7c_2eanon__u2665__l364__*)moonbit_malloc(sizeof(struct _M0R195Iter_3a_3afilter__map_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_2c_20clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoice_5d_7c_2eanon__u2665__l364__));
  Moonbit_object_header(_closure_4501)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R195Iter_3a_3afilter__map_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_2c_20clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoice_5d_7c_2eanon__u2665__l364__, $0) >> 2, 2, 0);
  _closure_4501->code
  = &_M0MPB4Iter11filter__mapGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceEC2665l364;
  _closure_4501->$0 = _M0L1fS929;
  _closure_4501->$1 = _M0L4selfS925;
  return (struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice*)_closure_4501;
}

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MPB4Iter11filter__mapGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceEC2665l364(
  struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2aenvS2666
) {
  struct _M0R195Iter_3a_3afilter__map_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_2c_20clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoice_5d_7c_2eanon__u2665__l364__* _M0L14_2acasted__envS2667;
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L8_2afieldS3969;
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS925;
  struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L8_2afieldS3968;
  int32_t _M0L6_2acntS4319;
  struct _M0TWORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L1fS929;
  #line 364 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L14_2acasted__envS2667
  = (struct _M0R195Iter_3a_3afilter__map_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_2c_20clawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoice_5d_7c_2eanon__u2665__l364__*)_M0L6_2aenvS2666;
  _M0L8_2afieldS3969 = _M0L14_2acasted__envS2667->$1;
  _M0L4selfS925 = _M0L8_2afieldS3969;
  _M0L8_2afieldS3968 = _M0L14_2acasted__envS2667->$0;
  _M0L6_2acntS4319 = Moonbit_object_header(_M0L14_2acasted__envS2667)->rc;
  if (_M0L6_2acntS4319 > 1) {
    int32_t _M0L11_2anew__cntS4320 = _M0L6_2acntS4319 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2667)->rc
    = _M0L11_2anew__cntS4320;
    moonbit_incref(_M0L4selfS925);
    moonbit_incref(_M0L8_2afieldS3968);
  } else if (_M0L6_2acntS4319 == 1) {
    #line 364 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    moonbit_free(_M0L14_2acasted__envS2667);
  }
  _M0L1fS929 = _M0L8_2afieldS3968;
  _2awhile_931:;
  while (1) {
    void* _M0L7_2abindS924;
    moonbit_incref(_M0L4selfS925);
    #line 365 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    _M0L7_2abindS924
    = _M0MPB4Iter4nextGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L4selfS925);
    switch (Moonbit_object_tag(_M0L7_2abindS924)) {
      case 1: {
        struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some* _M0L7_2aSomeS926 =
          (struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some*)_M0L7_2abindS924;
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L8_2afieldS3967 =
          _M0L7_2aSomeS926->$0;
        int32_t _M0L6_2acntS4321 =
          Moonbit_object_header(_M0L7_2aSomeS926)->rc;
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L4_2axS927;
        struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L7_2abindS928;
        if (_M0L6_2acntS4321 > 1) {
          int32_t _M0L11_2anew__cntS4322 = _M0L6_2acntS4321 - 1;
          Moonbit_object_header(_M0L7_2aSomeS926)->rc
          = _M0L11_2anew__cntS4322;
          if (_M0L8_2afieldS3967) {
            moonbit_incref(_M0L8_2afieldS3967);
          }
        } else if (_M0L6_2acntS4321 == 1) {
          #line 365 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
          moonbit_free(_M0L7_2aSomeS926);
        }
        _M0L4_2axS927 = _M0L8_2afieldS3967;
        moonbit_incref(_M0L1fS929);
        #line 366 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
        _M0L7_2abindS928 = _M0L1fS929->code(_M0L1fS929, _M0L4_2axS927);
        if (_M0L7_2abindS928 == 0) {
          if (_M0L7_2abindS928) {
            moonbit_decref(_M0L7_2abindS928);
          }
        } else {
          struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L7_2aSomeS930;
          struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2atmpS2668;
          moonbit_decref(_M0L1fS929);
          moonbit_decref(_M0L4selfS925);
          _M0L7_2aSomeS930 = _M0L7_2abindS928;
          _M0L6_2atmpS2668
          = (struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*)_M0L7_2aSomeS930;
          return _M0L6_2atmpS2668;
        }
        goto _2awhile_931;
        break;
      }
      default: {
        moonbit_decref(_M0L1fS929);
        moonbit_decref(_M0L4selfS925);
        moonbit_decref(_M0L7_2abindS924);
        return 0;
        break;
      }
    }
    break;
  }
}

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS922
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2663;
  void* _block_4503;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson14to__json_2eclo);
  #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2663
  = _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB4JsonE(_M0L4selfS922, _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson14to__json_2eclo);
  _block_4503 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4503)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4503)->$0 = _M0L6_2atmpS2663;
  return _block_4503;
}

void* _M0IPC15array5ArrayPB6ToJson8to__jsonGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0L4selfS923
) {
  struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2atmpS2664;
  void* _block_4504;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  moonbit_incref(_M0IP48clawteam8clawteam8internal6openai20ChatCompletionChoicePB6ToJson14to__json_2eclo);
  #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2664
  = _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceRPB4JsonE(_M0L4selfS923, _M0IP48clawteam8clawteam8internal6openai20ChatCompletionChoicePB6ToJson14to__json_2eclo);
  _block_4504 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4504)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4504)->$0 = _M0L6_2atmpS2664;
  return _block_4504;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS909,
  struct _M0TWRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallERPB4Json* _M0L1fS913
) {
  int32_t _M0L3lenS2657;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS908;
  int32_t _M0L7_2abindS910;
  int32_t _M0L1iS911;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2657 = _M0L4selfS909->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS908 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2657);
  _M0L7_2abindS910 = _M0L4selfS909->$1;
  _M0L1iS911 = 0;
  while (1) {
    if (_M0L1iS911 < _M0L7_2abindS910) {
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L8_2afieldS3973 =
        _M0L4selfS909->$0;
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L3bufS2656 =
        _M0L8_2afieldS3973;
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2atmpS3972 =
        (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)_M0L3bufS2656[
          _M0L1iS911
        ];
      struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L1vS912 =
        _M0L6_2atmpS3972;
      void** _M0L8_2afieldS3971 = _M0L3arrS908->$0;
      void** _M0L3bufS2653 = _M0L8_2afieldS3971;
      void* _M0L6_2atmpS2654;
      void* _M0L6_2aoldS3970;
      int32_t _M0L6_2atmpS2655;
      moonbit_incref(_M0L3bufS2653);
      moonbit_incref(_M0L1fS913);
      moonbit_incref(_M0L1vS912);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2654 = _M0L1fS913->code(_M0L1fS913, _M0L1vS912);
      _M0L6_2aoldS3970 = (void*)_M0L3bufS2653[_M0L1iS911];
      moonbit_decref(_M0L6_2aoldS3970);
      _M0L3bufS2653[_M0L1iS911] = _M0L6_2atmpS2654;
      moonbit_decref(_M0L3bufS2653);
      _M0L6_2atmpS2655 = _M0L1iS911 + 1;
      _M0L1iS911 = _M0L6_2atmpS2655;
      continue;
    } else {
      moonbit_decref(_M0L1fS913);
      moonbit_decref(_M0L4selfS909);
    }
    break;
  }
  return _M0L3arrS908;
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array3mapGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceRPB4JsonE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0L4selfS916,
  struct _M0TWRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceERPB4Json* _M0L1fS920
) {
  int32_t _M0L3lenS2662;
  struct _M0TPB5ArrayGRPB4JsonE* _M0L3arrS915;
  int32_t _M0L7_2abindS917;
  int32_t _M0L1iS918;
  #line 580 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS2662 = _M0L4selfS916->$1;
  #line 585 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3arrS915 = _M0MPC15array5Array12make__uninitGRPB4JsonE(_M0L3lenS2662);
  _M0L7_2abindS917 = _M0L4selfS916->$1;
  _M0L1iS918 = 0;
  while (1) {
    if (_M0L1iS918 < _M0L7_2abindS917) {
      struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L8_2afieldS3977 =
        _M0L4selfS916->$0;
      struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L3bufS2661 =
        _M0L8_2afieldS3977;
      struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2atmpS3976 =
        (struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*)_M0L3bufS2661[
          _M0L1iS918
        ];
      struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L1vS919 =
        _M0L6_2atmpS3976;
      void** _M0L8_2afieldS3975 = _M0L3arrS915->$0;
      void** _M0L3bufS2658 = _M0L8_2afieldS3975;
      void* _M0L6_2atmpS2659;
      void* _M0L6_2aoldS3974;
      int32_t _M0L6_2atmpS2660;
      moonbit_incref(_M0L3bufS2658);
      moonbit_incref(_M0L1fS920);
      moonbit_incref(_M0L1vS919);
      #line 587 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2659 = _M0L1fS920->code(_M0L1fS920, _M0L1vS919);
      _M0L6_2aoldS3974 = (void*)_M0L3bufS2658[_M0L1iS918];
      moonbit_decref(_M0L6_2aoldS3974);
      _M0L3bufS2658[_M0L1iS918] = _M0L6_2atmpS2659;
      moonbit_decref(_M0L3bufS2658);
      _M0L6_2atmpS2660 = _M0L1iS918 + 1;
      _M0L1iS918 = _M0L6_2atmpS2660;
      continue;
    } else {
      moonbit_decref(_M0L1fS920);
      moonbit_decref(_M0L4selfS916);
    }
    break;
  }
  return _M0L3arrS915;
}

void* _M0IPC16double6DoublePB6ToJson8to__json(double _M0L4selfS907) {
  #line 229 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L4selfS907 != _M0L4selfS907) {
    #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_108.data);
  } else {
    int64_t _tmp_4507 = 9218868437227405311ll;
    double _M0L6_2atmpS2650 = *(double*)&_tmp_4507;
    if (_M0L4selfS907 > _M0L6_2atmpS2650) {
      #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
      return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_110.data);
    } else {
      int64_t _tmp_4508 = -4503599627370497ll;
      double _M0L6_2atmpS2651 = *(double*)&_tmp_4508;
      if (_M0L4selfS907 < _M0L6_2atmpS2651) {
        #line 235 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
        return _M0MPC14json4Json6string((moonbit_string_t)moonbit_string_literal_112.data);
      } else {
        moonbit_string_t _M0L6_2atmpS2652 = 0;
        #line 237 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
        return _M0MPC14json4Json6number(_M0L4selfS907, _M0L6_2atmpS2652);
      }
    }
  }
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS906) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS906;
}

void* _M0IPC13int3IntPB6ToJson8to__json(int32_t _M0L4selfS905) {
  double _M0L6_2atmpS2648;
  moonbit_string_t _M0L6_2atmpS2649;
  #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _M0L6_2atmpS2648 = (double)_M0L4selfS905;
  _M0L6_2atmpS2649 = 0;
  #line 210 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  return _M0MPC14json4Json6number(_M0L6_2atmpS2648, _M0L6_2atmpS2649);
}

void* _M0MPC14json4Json6object(struct _M0TPB3MapGsRPB4JsonE* _M0L6objectS904) {
  void* _block_4509;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4509 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Object));
  Moonbit_object_header(_block_4509)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Object, $0) >> 2, 1, 6);
  ((struct _M0DTPB4Json6Object*)_block_4509)->$0 = _M0L6objectS904;
  return _block_4509;
}

void* _M0MPC14json4Json6string(moonbit_string_t _M0L6stringS903) {
  void* _block_4510;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4510 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4510)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4510)->$0 = _M0L6stringS903;
  return _block_4510;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS896
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3978;
  int32_t _M0L6_2acntS4323;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2647;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS895;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2643__l591__* _closure_4511;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2642;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3978 = _M0L4selfS896->$5;
  _M0L6_2acntS4323 = Moonbit_object_header(_M0L4selfS896)->rc;
  if (_M0L6_2acntS4323 > 1) {
    int32_t _M0L11_2anew__cntS4325 = _M0L6_2acntS4323 - 1;
    Moonbit_object_header(_M0L4selfS896)->rc = _M0L11_2anew__cntS4325;
    if (_M0L8_2afieldS3978) {
      moonbit_incref(_M0L8_2afieldS3978);
    }
  } else if (_M0L6_2acntS4323 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4324 = _M0L4selfS896->$0;
    moonbit_decref(_M0L8_2afieldS4324);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS896);
  }
  _M0L4headS2647 = _M0L8_2afieldS3978;
  _M0L11curr__entryS895
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS895)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS895->$0 = _M0L4headS2647;
  _closure_4511
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2643__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2643__l591__));
  Moonbit_object_header(_closure_4511)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2643__l591__, $0) >> 2, 1, 0);
  _closure_4511->code = &_M0MPB3Map4iterGsRPB4JsonEC2643l591;
  _closure_4511->$0 = _M0L11curr__entryS895;
  _M0L6_2atmpS2642 = (struct _M0TWEOUsRPB4JsonE*)_closure_4511;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2642);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2643l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2644
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2643__l591__* _M0L14_2acasted__envS2645;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3984;
  int32_t _M0L6_2acntS4326;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS895;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3983;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS897;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2645
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2643__l591__*)_M0L6_2aenvS2644;
  _M0L8_2afieldS3984 = _M0L14_2acasted__envS2645->$0;
  _M0L6_2acntS4326 = Moonbit_object_header(_M0L14_2acasted__envS2645)->rc;
  if (_M0L6_2acntS4326 > 1) {
    int32_t _M0L11_2anew__cntS4327 = _M0L6_2acntS4326 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2645)->rc
    = _M0L11_2anew__cntS4327;
    moonbit_incref(_M0L8_2afieldS3984);
  } else if (_M0L6_2acntS4326 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2645);
  }
  _M0L11curr__entryS895 = _M0L8_2afieldS3984;
  _M0L8_2afieldS3983 = _M0L11curr__entryS895->$0;
  _M0L7_2abindS897 = _M0L8_2afieldS3983;
  if (_M0L7_2abindS897 == 0) {
    moonbit_decref(_M0L11curr__entryS895);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS898 = _M0L7_2abindS897;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS899 = _M0L7_2aSomeS898;
    moonbit_string_t _M0L8_2afieldS3982 = _M0L4_2axS899->$4;
    moonbit_string_t _M0L6_2akeyS900 = _M0L8_2afieldS3982;
    void* _M0L8_2afieldS3981 = _M0L4_2axS899->$5;
    void* _M0L8_2avalueS901 = _M0L8_2afieldS3981;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3980 = _M0L4_2axS899->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS902 = _M0L8_2afieldS3980;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3979 =
      _M0L11curr__entryS895->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2646;
    if (_M0L7_2anextS902) {
      moonbit_incref(_M0L7_2anextS902);
    }
    moonbit_incref(_M0L8_2avalueS901);
    moonbit_incref(_M0L6_2akeyS900);
    if (_M0L6_2aoldS3979) {
      moonbit_decref(_M0L6_2aoldS3979);
    }
    _M0L11curr__entryS895->$0 = _M0L7_2anextS902;
    moonbit_decref(_M0L11curr__entryS895);
    _M0L8_2atupleS2646
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2646)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2646->$0 = _M0L6_2akeyS900;
    _M0L8_2atupleS2646->$1 = _M0L8_2avalueS901;
    return _M0L8_2atupleS2646;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS894
) {
  int32_t _M0L8_2afieldS3985;
  int32_t _M0L4sizeS2641;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3985 = _M0L4selfS894->$1;
  moonbit_decref(_M0L4selfS894);
  _M0L4sizeS2641 = _M0L8_2afieldS3985;
  return _M0L4sizeS2641 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS881,
  int32_t _M0L3keyS877
) {
  int32_t _M0L4hashS876;
  int32_t _M0L14capacity__maskS2626;
  int32_t _M0L6_2atmpS2625;
  int32_t _M0L1iS878;
  int32_t _M0L3idxS879;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS876 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS877);
  _M0L14capacity__maskS2626 = _M0L4selfS881->$3;
  _M0L6_2atmpS2625 = _M0L4hashS876 & _M0L14capacity__maskS2626;
  _M0L1iS878 = 0;
  _M0L3idxS879 = _M0L6_2atmpS2625;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3989 =
      _M0L4selfS881->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2624 =
      _M0L8_2afieldS3989;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3988;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS880;
    if (
      _M0L3idxS879 < 0
      || _M0L3idxS879 >= Moonbit_array_length(_M0L7entriesS2624)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3988
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2624[
        _M0L3idxS879
      ];
    _M0L7_2abindS880 = _M0L6_2atmpS3988;
    if (_M0L7_2abindS880 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2613;
      if (_M0L7_2abindS880) {
        moonbit_incref(_M0L7_2abindS880);
      }
      moonbit_decref(_M0L4selfS881);
      if (_M0L7_2abindS880) {
        moonbit_decref(_M0L7_2abindS880);
      }
      _M0L6_2atmpS2613 = 0;
      return _M0L6_2atmpS2613;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS882 =
        _M0L7_2abindS880;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS883 =
        _M0L7_2aSomeS882;
      int32_t _M0L4hashS2615 = _M0L8_2aentryS883->$3;
      int32_t _if__result_4513;
      int32_t _M0L8_2afieldS3986;
      int32_t _M0L3pslS2618;
      int32_t _M0L6_2atmpS2620;
      int32_t _M0L6_2atmpS2622;
      int32_t _M0L14capacity__maskS2623;
      int32_t _M0L6_2atmpS2621;
      if (_M0L4hashS2615 == _M0L4hashS876) {
        int32_t _M0L3keyS2614 = _M0L8_2aentryS883->$4;
        _if__result_4513 = _M0L3keyS2614 == _M0L3keyS877;
      } else {
        _if__result_4513 = 0;
      }
      if (_if__result_4513) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3987;
        int32_t _M0L6_2acntS4328;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2617;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2616;
        moonbit_incref(_M0L8_2aentryS883);
        moonbit_decref(_M0L4selfS881);
        _M0L8_2afieldS3987 = _M0L8_2aentryS883->$5;
        _M0L6_2acntS4328 = Moonbit_object_header(_M0L8_2aentryS883)->rc;
        if (_M0L6_2acntS4328 > 1) {
          int32_t _M0L11_2anew__cntS4330 = _M0L6_2acntS4328 - 1;
          Moonbit_object_header(_M0L8_2aentryS883)->rc
          = _M0L11_2anew__cntS4330;
          moonbit_incref(_M0L8_2afieldS3987);
        } else if (_M0L6_2acntS4328 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4329 =
            _M0L8_2aentryS883->$1;
          if (_M0L8_2afieldS4329) {
            moonbit_decref(_M0L8_2afieldS4329);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS883);
        }
        _M0L5valueS2617 = _M0L8_2afieldS3987;
        _M0L6_2atmpS2616 = _M0L5valueS2617;
        return _M0L6_2atmpS2616;
      } else {
        moonbit_incref(_M0L8_2aentryS883);
      }
      _M0L8_2afieldS3986 = _M0L8_2aentryS883->$2;
      moonbit_decref(_M0L8_2aentryS883);
      _M0L3pslS2618 = _M0L8_2afieldS3986;
      if (_M0L1iS878 > _M0L3pslS2618) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2619;
        moonbit_decref(_M0L4selfS881);
        _M0L6_2atmpS2619 = 0;
        return _M0L6_2atmpS2619;
      }
      _M0L6_2atmpS2620 = _M0L1iS878 + 1;
      _M0L6_2atmpS2622 = _M0L3idxS879 + 1;
      _M0L14capacity__maskS2623 = _M0L4selfS881->$3;
      _M0L6_2atmpS2621 = _M0L6_2atmpS2622 & _M0L14capacity__maskS2623;
      _M0L1iS878 = _M0L6_2atmpS2620;
      _M0L3idxS879 = _M0L6_2atmpS2621;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS890,
  moonbit_string_t _M0L3keyS886
) {
  int32_t _M0L4hashS885;
  int32_t _M0L14capacity__maskS2640;
  int32_t _M0L6_2atmpS2639;
  int32_t _M0L1iS887;
  int32_t _M0L3idxS888;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS886);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS885 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS886);
  _M0L14capacity__maskS2640 = _M0L4selfS890->$3;
  _M0L6_2atmpS2639 = _M0L4hashS885 & _M0L14capacity__maskS2640;
  _M0L1iS887 = 0;
  _M0L3idxS888 = _M0L6_2atmpS2639;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3995 =
      _M0L4selfS890->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2638 =
      _M0L8_2afieldS3995;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3994;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS889;
    if (
      _M0L3idxS888 < 0
      || _M0L3idxS888 >= Moonbit_array_length(_M0L7entriesS2638)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3994
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2638[
        _M0L3idxS888
      ];
    _M0L7_2abindS889 = _M0L6_2atmpS3994;
    if (_M0L7_2abindS889 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2627;
      if (_M0L7_2abindS889) {
        moonbit_incref(_M0L7_2abindS889);
      }
      moonbit_decref(_M0L4selfS890);
      if (_M0L7_2abindS889) {
        moonbit_decref(_M0L7_2abindS889);
      }
      moonbit_decref(_M0L3keyS886);
      _M0L6_2atmpS2627 = 0;
      return _M0L6_2atmpS2627;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS891 =
        _M0L7_2abindS889;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS892 =
        _M0L7_2aSomeS891;
      int32_t _M0L4hashS2629 = _M0L8_2aentryS892->$3;
      int32_t _if__result_4515;
      int32_t _M0L8_2afieldS3990;
      int32_t _M0L3pslS2632;
      int32_t _M0L6_2atmpS2634;
      int32_t _M0L6_2atmpS2636;
      int32_t _M0L14capacity__maskS2637;
      int32_t _M0L6_2atmpS2635;
      if (_M0L4hashS2629 == _M0L4hashS885) {
        moonbit_string_t _M0L8_2afieldS3993 = _M0L8_2aentryS892->$4;
        moonbit_string_t _M0L3keyS2628 = _M0L8_2afieldS3993;
        int32_t _M0L6_2atmpS3992;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3992
        = moonbit_val_array_equal(_M0L3keyS2628, _M0L3keyS886);
        _if__result_4515 = _M0L6_2atmpS3992;
      } else {
        _if__result_4515 = 0;
      }
      if (_if__result_4515) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3991;
        int32_t _M0L6_2acntS4331;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2631;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2630;
        moonbit_incref(_M0L8_2aentryS892);
        moonbit_decref(_M0L4selfS890);
        moonbit_decref(_M0L3keyS886);
        _M0L8_2afieldS3991 = _M0L8_2aentryS892->$5;
        _M0L6_2acntS4331 = Moonbit_object_header(_M0L8_2aentryS892)->rc;
        if (_M0L6_2acntS4331 > 1) {
          int32_t _M0L11_2anew__cntS4334 = _M0L6_2acntS4331 - 1;
          Moonbit_object_header(_M0L8_2aentryS892)->rc
          = _M0L11_2anew__cntS4334;
          moonbit_incref(_M0L8_2afieldS3991);
        } else if (_M0L6_2acntS4331 == 1) {
          moonbit_string_t _M0L8_2afieldS4333 = _M0L8_2aentryS892->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4332;
          moonbit_decref(_M0L8_2afieldS4333);
          _M0L8_2afieldS4332 = _M0L8_2aentryS892->$1;
          if (_M0L8_2afieldS4332) {
            moonbit_decref(_M0L8_2afieldS4332);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS892);
        }
        _M0L5valueS2631 = _M0L8_2afieldS3991;
        _M0L6_2atmpS2630 = _M0L5valueS2631;
        return _M0L6_2atmpS2630;
      } else {
        moonbit_incref(_M0L8_2aentryS892);
      }
      _M0L8_2afieldS3990 = _M0L8_2aentryS892->$2;
      moonbit_decref(_M0L8_2aentryS892);
      _M0L3pslS2632 = _M0L8_2afieldS3990;
      if (_M0L1iS887 > _M0L3pslS2632) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2633;
        moonbit_decref(_M0L4selfS890);
        moonbit_decref(_M0L3keyS886);
        _M0L6_2atmpS2633 = 0;
        return _M0L6_2atmpS2633;
      }
      _M0L6_2atmpS2634 = _M0L1iS887 + 1;
      _M0L6_2atmpS2636 = _M0L3idxS888 + 1;
      _M0L14capacity__maskS2637 = _M0L4selfS890->$3;
      _M0L6_2atmpS2635 = _M0L6_2atmpS2636 & _M0L14capacity__maskS2637;
      _M0L1iS887 = _M0L6_2atmpS2634;
      _M0L3idxS888 = _M0L6_2atmpS2635;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS853
) {
  int32_t _M0L6lengthS852;
  int32_t _M0Lm8capacityS854;
  int32_t _M0L6_2atmpS2578;
  int32_t _M0L6_2atmpS2577;
  int32_t _M0L6_2atmpS2588;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS855;
  int32_t _M0L3endS2586;
  int32_t _M0L5startS2587;
  int32_t _M0L7_2abindS856;
  int32_t _M0L2__S857;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS853.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS852
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS853);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS854 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS852);
  _M0L6_2atmpS2578 = _M0Lm8capacityS854;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2577 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2578);
  if (_M0L6lengthS852 > _M0L6_2atmpS2577) {
    int32_t _M0L6_2atmpS2579 = _M0Lm8capacityS854;
    _M0Lm8capacityS854 = _M0L6_2atmpS2579 * 2;
  }
  _M0L6_2atmpS2588 = _M0Lm8capacityS854;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS855
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2588);
  _M0L3endS2586 = _M0L3arrS853.$2;
  _M0L5startS2587 = _M0L3arrS853.$1;
  _M0L7_2abindS856 = _M0L3endS2586 - _M0L5startS2587;
  _M0L2__S857 = 0;
  while (1) {
    if (_M0L2__S857 < _M0L7_2abindS856) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3999 =
        _M0L3arrS853.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2583 =
        _M0L8_2afieldS3999;
      int32_t _M0L5startS2585 = _M0L3arrS853.$1;
      int32_t _M0L6_2atmpS2584 = _M0L5startS2585 + _M0L2__S857;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3998 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2583[
          _M0L6_2atmpS2584
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS858 =
        _M0L6_2atmpS3998;
      moonbit_string_t _M0L8_2afieldS3997 = _M0L1eS858->$0;
      moonbit_string_t _M0L6_2atmpS2580 = _M0L8_2afieldS3997;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3996 =
        _M0L1eS858->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2581 =
        _M0L8_2afieldS3996;
      int32_t _M0L6_2atmpS2582;
      moonbit_incref(_M0L6_2atmpS2581);
      moonbit_incref(_M0L6_2atmpS2580);
      moonbit_incref(_M0L1mS855);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS855, _M0L6_2atmpS2580, _M0L6_2atmpS2581);
      _M0L6_2atmpS2582 = _M0L2__S857 + 1;
      _M0L2__S857 = _M0L6_2atmpS2582;
      continue;
    } else {
      moonbit_decref(_M0L3arrS853.$0);
    }
    break;
  }
  return _M0L1mS855;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS861
) {
  int32_t _M0L6lengthS860;
  int32_t _M0Lm8capacityS862;
  int32_t _M0L6_2atmpS2590;
  int32_t _M0L6_2atmpS2589;
  int32_t _M0L6_2atmpS2600;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS863;
  int32_t _M0L3endS2598;
  int32_t _M0L5startS2599;
  int32_t _M0L7_2abindS864;
  int32_t _M0L2__S865;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS861.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS860
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS861);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS862 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS860);
  _M0L6_2atmpS2590 = _M0Lm8capacityS862;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2589 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2590);
  if (_M0L6lengthS860 > _M0L6_2atmpS2589) {
    int32_t _M0L6_2atmpS2591 = _M0Lm8capacityS862;
    _M0Lm8capacityS862 = _M0L6_2atmpS2591 * 2;
  }
  _M0L6_2atmpS2600 = _M0Lm8capacityS862;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS863
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2600);
  _M0L3endS2598 = _M0L3arrS861.$2;
  _M0L5startS2599 = _M0L3arrS861.$1;
  _M0L7_2abindS864 = _M0L3endS2598 - _M0L5startS2599;
  _M0L2__S865 = 0;
  while (1) {
    if (_M0L2__S865 < _M0L7_2abindS864) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4002 =
        _M0L3arrS861.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2595 =
        _M0L8_2afieldS4002;
      int32_t _M0L5startS2597 = _M0L3arrS861.$1;
      int32_t _M0L6_2atmpS2596 = _M0L5startS2597 + _M0L2__S865;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4001 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2595[
          _M0L6_2atmpS2596
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS866 = _M0L6_2atmpS4001;
      int32_t _M0L6_2atmpS2592 = _M0L1eS866->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4000 =
        _M0L1eS866->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2593 =
        _M0L8_2afieldS4000;
      int32_t _M0L6_2atmpS2594;
      moonbit_incref(_M0L6_2atmpS2593);
      moonbit_incref(_M0L1mS863);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS863, _M0L6_2atmpS2592, _M0L6_2atmpS2593);
      _M0L6_2atmpS2594 = _M0L2__S865 + 1;
      _M0L2__S865 = _M0L6_2atmpS2594;
      continue;
    } else {
      moonbit_decref(_M0L3arrS861.$0);
    }
    break;
  }
  return _M0L1mS863;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11from__arrayGsRPB4JsonE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L3arrS869
) {
  int32_t _M0L6lengthS868;
  int32_t _M0Lm8capacityS870;
  int32_t _M0L6_2atmpS2602;
  int32_t _M0L6_2atmpS2601;
  int32_t _M0L6_2atmpS2612;
  struct _M0TPB3MapGsRPB4JsonE* _M0L1mS871;
  int32_t _M0L3endS2610;
  int32_t _M0L5startS2611;
  int32_t _M0L7_2abindS872;
  int32_t _M0L2__S873;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS869.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS868 = _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(_M0L3arrS869);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS870 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS868);
  _M0L6_2atmpS2602 = _M0Lm8capacityS870;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2601 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2602);
  if (_M0L6lengthS868 > _M0L6_2atmpS2601) {
    int32_t _M0L6_2atmpS2603 = _M0Lm8capacityS870;
    _M0Lm8capacityS870 = _M0L6_2atmpS2603 * 2;
  }
  _M0L6_2atmpS2612 = _M0Lm8capacityS870;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS871 = _M0MPB3Map11new_2einnerGsRPB4JsonE(_M0L6_2atmpS2612);
  _M0L3endS2610 = _M0L3arrS869.$2;
  _M0L5startS2611 = _M0L3arrS869.$1;
  _M0L7_2abindS872 = _M0L3endS2610 - _M0L5startS2611;
  _M0L2__S873 = 0;
  while (1) {
    if (_M0L2__S873 < _M0L7_2abindS872) {
      struct _M0TUsRPB4JsonE** _M0L8_2afieldS4006 = _M0L3arrS869.$0;
      struct _M0TUsRPB4JsonE** _M0L3bufS2607 = _M0L8_2afieldS4006;
      int32_t _M0L5startS2609 = _M0L3arrS869.$1;
      int32_t _M0L6_2atmpS2608 = _M0L5startS2609 + _M0L2__S873;
      struct _M0TUsRPB4JsonE* _M0L6_2atmpS4005 =
        (struct _M0TUsRPB4JsonE*)_M0L3bufS2607[_M0L6_2atmpS2608];
      struct _M0TUsRPB4JsonE* _M0L1eS874 = _M0L6_2atmpS4005;
      moonbit_string_t _M0L8_2afieldS4004 = _M0L1eS874->$0;
      moonbit_string_t _M0L6_2atmpS2604 = _M0L8_2afieldS4004;
      void* _M0L8_2afieldS4003 = _M0L1eS874->$1;
      void* _M0L6_2atmpS2605 = _M0L8_2afieldS4003;
      int32_t _M0L6_2atmpS2606;
      moonbit_incref(_M0L6_2atmpS2605);
      moonbit_incref(_M0L6_2atmpS2604);
      moonbit_incref(_M0L1mS871);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB4JsonE(_M0L1mS871, _M0L6_2atmpS2604, _M0L6_2atmpS2605);
      _M0L6_2atmpS2606 = _M0L2__S873 + 1;
      _M0L2__S873 = _M0L6_2atmpS2606;
      continue;
    } else {
      moonbit_decref(_M0L3arrS869.$0);
    }
    break;
  }
  return _M0L1mS871;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS843,
  moonbit_string_t _M0L3keyS844,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS845
) {
  int32_t _M0L6_2atmpS2574;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS844);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2574 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS844);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS843, _M0L3keyS844, _M0L5valueS845, _M0L6_2atmpS2574);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS846,
  int32_t _M0L3keyS847,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS848
) {
  int32_t _M0L6_2atmpS2575;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2575 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS847);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS846, _M0L3keyS847, _M0L5valueS848, _M0L6_2atmpS2575);
  return 0;
}

int32_t _M0MPB3Map3setGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS849,
  moonbit_string_t _M0L3keyS850,
  void* _M0L5valueS851
) {
  int32_t _M0L6_2atmpS2576;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS850);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2576 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS850);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS849, _M0L3keyS850, _M0L5valueS851, _M0L6_2atmpS2576);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS811
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4013;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS810;
  int32_t _M0L8capacityS2559;
  int32_t _M0L13new__capacityS812;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2554;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2553;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS4012;
  int32_t _M0L6_2atmpS2555;
  int32_t _M0L8capacityS2557;
  int32_t _M0L6_2atmpS2556;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2558;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4011;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS813;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4013 = _M0L4selfS811->$5;
  _M0L9old__headS810 = _M0L8_2afieldS4013;
  _M0L8capacityS2559 = _M0L4selfS811->$2;
  _M0L13new__capacityS812 = _M0L8capacityS2559 << 1;
  _M0L6_2atmpS2554 = 0;
  _M0L6_2atmpS2553
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS812, _M0L6_2atmpS2554);
  _M0L6_2aoldS4012 = _M0L4selfS811->$0;
  if (_M0L9old__headS810) {
    moonbit_incref(_M0L9old__headS810);
  }
  moonbit_decref(_M0L6_2aoldS4012);
  _M0L4selfS811->$0 = _M0L6_2atmpS2553;
  _M0L4selfS811->$2 = _M0L13new__capacityS812;
  _M0L6_2atmpS2555 = _M0L13new__capacityS812 - 1;
  _M0L4selfS811->$3 = _M0L6_2atmpS2555;
  _M0L8capacityS2557 = _M0L4selfS811->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2556 = _M0FPB21calc__grow__threshold(_M0L8capacityS2557);
  _M0L4selfS811->$4 = _M0L6_2atmpS2556;
  _M0L4selfS811->$1 = 0;
  _M0L6_2atmpS2558 = 0;
  _M0L6_2aoldS4011 = _M0L4selfS811->$5;
  if (_M0L6_2aoldS4011) {
    moonbit_decref(_M0L6_2aoldS4011);
  }
  _M0L4selfS811->$5 = _M0L6_2atmpS2558;
  _M0L4selfS811->$6 = -1;
  _M0L8_2aparamS813 = _M0L9old__headS810;
  while (1) {
    if (_M0L8_2aparamS813 == 0) {
      if (_M0L8_2aparamS813) {
        moonbit_decref(_M0L8_2aparamS813);
      }
      moonbit_decref(_M0L4selfS811);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS814 =
        _M0L8_2aparamS813;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS815 =
        _M0L7_2aSomeS814;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4010 =
        _M0L4_2axS815->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS816 =
        _M0L8_2afieldS4010;
      moonbit_string_t _M0L8_2afieldS4009 = _M0L4_2axS815->$4;
      moonbit_string_t _M0L6_2akeyS817 = _M0L8_2afieldS4009;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4008 =
        _M0L4_2axS815->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS818 =
        _M0L8_2afieldS4008;
      int32_t _M0L8_2afieldS4007 = _M0L4_2axS815->$3;
      int32_t _M0L6_2acntS4335 = Moonbit_object_header(_M0L4_2axS815)->rc;
      int32_t _M0L7_2ahashS819;
      if (_M0L6_2acntS4335 > 1) {
        int32_t _M0L11_2anew__cntS4336 = _M0L6_2acntS4335 - 1;
        Moonbit_object_header(_M0L4_2axS815)->rc = _M0L11_2anew__cntS4336;
        moonbit_incref(_M0L8_2avalueS818);
        moonbit_incref(_M0L6_2akeyS817);
        if (_M0L7_2anextS816) {
          moonbit_incref(_M0L7_2anextS816);
        }
      } else if (_M0L6_2acntS4335 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS815);
      }
      _M0L7_2ahashS819 = _M0L8_2afieldS4007;
      moonbit_incref(_M0L4selfS811);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS811, _M0L6_2akeyS817, _M0L8_2avalueS818, _M0L7_2ahashS819);
      _M0L8_2aparamS813 = _M0L7_2anextS816;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS822
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4019;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS821;
  int32_t _M0L8capacityS2566;
  int32_t _M0L13new__capacityS823;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2561;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2560;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS4018;
  int32_t _M0L6_2atmpS2562;
  int32_t _M0L8capacityS2564;
  int32_t _M0L6_2atmpS2563;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2565;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4017;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS824;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4019 = _M0L4selfS822->$5;
  _M0L9old__headS821 = _M0L8_2afieldS4019;
  _M0L8capacityS2566 = _M0L4selfS822->$2;
  _M0L13new__capacityS823 = _M0L8capacityS2566 << 1;
  _M0L6_2atmpS2561 = 0;
  _M0L6_2atmpS2560
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS823, _M0L6_2atmpS2561);
  _M0L6_2aoldS4018 = _M0L4selfS822->$0;
  if (_M0L9old__headS821) {
    moonbit_incref(_M0L9old__headS821);
  }
  moonbit_decref(_M0L6_2aoldS4018);
  _M0L4selfS822->$0 = _M0L6_2atmpS2560;
  _M0L4selfS822->$2 = _M0L13new__capacityS823;
  _M0L6_2atmpS2562 = _M0L13new__capacityS823 - 1;
  _M0L4selfS822->$3 = _M0L6_2atmpS2562;
  _M0L8capacityS2564 = _M0L4selfS822->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2563 = _M0FPB21calc__grow__threshold(_M0L8capacityS2564);
  _M0L4selfS822->$4 = _M0L6_2atmpS2563;
  _M0L4selfS822->$1 = 0;
  _M0L6_2atmpS2565 = 0;
  _M0L6_2aoldS4017 = _M0L4selfS822->$5;
  if (_M0L6_2aoldS4017) {
    moonbit_decref(_M0L6_2aoldS4017);
  }
  _M0L4selfS822->$5 = _M0L6_2atmpS2565;
  _M0L4selfS822->$6 = -1;
  _M0L8_2aparamS824 = _M0L9old__headS821;
  while (1) {
    if (_M0L8_2aparamS824 == 0) {
      if (_M0L8_2aparamS824) {
        moonbit_decref(_M0L8_2aparamS824);
      }
      moonbit_decref(_M0L4selfS822);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS825 =
        _M0L8_2aparamS824;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS826 =
        _M0L7_2aSomeS825;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4016 =
        _M0L4_2axS826->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS827 =
        _M0L8_2afieldS4016;
      int32_t _M0L6_2akeyS828 = _M0L4_2axS826->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4015 =
        _M0L4_2axS826->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS829 =
        _M0L8_2afieldS4015;
      int32_t _M0L8_2afieldS4014 = _M0L4_2axS826->$3;
      int32_t _M0L6_2acntS4337 = Moonbit_object_header(_M0L4_2axS826)->rc;
      int32_t _M0L7_2ahashS830;
      if (_M0L6_2acntS4337 > 1) {
        int32_t _M0L11_2anew__cntS4338 = _M0L6_2acntS4337 - 1;
        Moonbit_object_header(_M0L4_2axS826)->rc = _M0L11_2anew__cntS4338;
        moonbit_incref(_M0L8_2avalueS829);
        if (_M0L7_2anextS827) {
          moonbit_incref(_M0L7_2anextS827);
        }
      } else if (_M0L6_2acntS4337 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS826);
      }
      _M0L7_2ahashS830 = _M0L8_2afieldS4014;
      moonbit_incref(_M0L4selfS822);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS822, _M0L6_2akeyS828, _M0L8_2avalueS829, _M0L7_2ahashS830);
      _M0L8_2aparamS824 = _M0L7_2anextS827;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS833
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4026;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L9old__headS832;
  int32_t _M0L8capacityS2573;
  int32_t _M0L13new__capacityS834;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2568;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2atmpS2567;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L6_2aoldS4025;
  int32_t _M0L6_2atmpS2569;
  int32_t _M0L8capacityS2571;
  int32_t _M0L6_2atmpS2570;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2572;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4024;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2aparamS835;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4026 = _M0L4selfS833->$5;
  _M0L9old__headS832 = _M0L8_2afieldS4026;
  _M0L8capacityS2573 = _M0L4selfS833->$2;
  _M0L13new__capacityS834 = _M0L8capacityS2573 << 1;
  _M0L6_2atmpS2568 = 0;
  _M0L6_2atmpS2567
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L13new__capacityS834, _M0L6_2atmpS2568);
  _M0L6_2aoldS4025 = _M0L4selfS833->$0;
  if (_M0L9old__headS832) {
    moonbit_incref(_M0L9old__headS832);
  }
  moonbit_decref(_M0L6_2aoldS4025);
  _M0L4selfS833->$0 = _M0L6_2atmpS2567;
  _M0L4selfS833->$2 = _M0L13new__capacityS834;
  _M0L6_2atmpS2569 = _M0L13new__capacityS834 - 1;
  _M0L4selfS833->$3 = _M0L6_2atmpS2569;
  _M0L8capacityS2571 = _M0L4selfS833->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2570 = _M0FPB21calc__grow__threshold(_M0L8capacityS2571);
  _M0L4selfS833->$4 = _M0L6_2atmpS2570;
  _M0L4selfS833->$1 = 0;
  _M0L6_2atmpS2572 = 0;
  _M0L6_2aoldS4024 = _M0L4selfS833->$5;
  if (_M0L6_2aoldS4024) {
    moonbit_decref(_M0L6_2aoldS4024);
  }
  _M0L4selfS833->$5 = _M0L6_2atmpS2572;
  _M0L4selfS833->$6 = -1;
  _M0L8_2aparamS835 = _M0L9old__headS832;
  while (1) {
    if (_M0L8_2aparamS835 == 0) {
      if (_M0L8_2aparamS835) {
        moonbit_decref(_M0L8_2aparamS835);
      }
      moonbit_decref(_M0L4selfS833);
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS836 = _M0L8_2aparamS835;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS837 = _M0L7_2aSomeS836;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4023 = _M0L4_2axS837->$1;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS838 = _M0L8_2afieldS4023;
      moonbit_string_t _M0L8_2afieldS4022 = _M0L4_2axS837->$4;
      moonbit_string_t _M0L6_2akeyS839 = _M0L8_2afieldS4022;
      void* _M0L8_2afieldS4021 = _M0L4_2axS837->$5;
      void* _M0L8_2avalueS840 = _M0L8_2afieldS4021;
      int32_t _M0L8_2afieldS4020 = _M0L4_2axS837->$3;
      int32_t _M0L6_2acntS4339 = Moonbit_object_header(_M0L4_2axS837)->rc;
      int32_t _M0L7_2ahashS841;
      if (_M0L6_2acntS4339 > 1) {
        int32_t _M0L11_2anew__cntS4340 = _M0L6_2acntS4339 - 1;
        Moonbit_object_header(_M0L4_2axS837)->rc = _M0L11_2anew__cntS4340;
        moonbit_incref(_M0L8_2avalueS840);
        moonbit_incref(_M0L6_2akeyS839);
        if (_M0L7_2anextS838) {
          moonbit_incref(_M0L7_2anextS838);
        }
      } else if (_M0L6_2acntS4339 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS837);
      }
      _M0L7_2ahashS841 = _M0L8_2afieldS4020;
      moonbit_incref(_M0L4selfS833);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB4JsonE(_M0L4selfS833, _M0L6_2akeyS839, _M0L8_2avalueS840, _M0L7_2ahashS841);
      _M0L8_2aparamS835 = _M0L7_2anextS838;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS765,
  moonbit_string_t _M0L3keyS771,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS772,
  int32_t _M0L4hashS767
) {
  int32_t _M0L14capacity__maskS2516;
  int32_t _M0L6_2atmpS2515;
  int32_t _M0L3pslS762;
  int32_t _M0L3idxS763;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2516 = _M0L4selfS765->$3;
  _M0L6_2atmpS2515 = _M0L4hashS767 & _M0L14capacity__maskS2516;
  _M0L3pslS762 = 0;
  _M0L3idxS763 = _M0L6_2atmpS2515;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4031 =
      _M0L4selfS765->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2514 =
      _M0L8_2afieldS4031;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4030;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS764;
    if (
      _M0L3idxS763 < 0
      || _M0L3idxS763 >= Moonbit_array_length(_M0L7entriesS2514)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4030
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2514[
        _M0L3idxS763
      ];
    _M0L7_2abindS764 = _M0L6_2atmpS4030;
    if (_M0L7_2abindS764 == 0) {
      int32_t _M0L4sizeS2499 = _M0L4selfS765->$1;
      int32_t _M0L8grow__atS2500 = _M0L4selfS765->$4;
      int32_t _M0L7_2abindS768;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS769;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS770;
      if (_M0L4sizeS2499 >= _M0L8grow__atS2500) {
        int32_t _M0L14capacity__maskS2502;
        int32_t _M0L6_2atmpS2501;
        moonbit_incref(_M0L4selfS765);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS765);
        _M0L14capacity__maskS2502 = _M0L4selfS765->$3;
        _M0L6_2atmpS2501 = _M0L4hashS767 & _M0L14capacity__maskS2502;
        _M0L3pslS762 = 0;
        _M0L3idxS763 = _M0L6_2atmpS2501;
        continue;
      }
      _M0L7_2abindS768 = _M0L4selfS765->$6;
      _M0L7_2abindS769 = 0;
      _M0L5entryS770
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS770)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS770->$0 = _M0L7_2abindS768;
      _M0L5entryS770->$1 = _M0L7_2abindS769;
      _M0L5entryS770->$2 = _M0L3pslS762;
      _M0L5entryS770->$3 = _M0L4hashS767;
      _M0L5entryS770->$4 = _M0L3keyS771;
      _M0L5entryS770->$5 = _M0L5valueS772;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS765, _M0L3idxS763, _M0L5entryS770);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS773 =
        _M0L7_2abindS764;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS774 =
        _M0L7_2aSomeS773;
      int32_t _M0L4hashS2504 = _M0L14_2acurr__entryS774->$3;
      int32_t _if__result_4523;
      int32_t _M0L3pslS2505;
      int32_t _M0L6_2atmpS2510;
      int32_t _M0L6_2atmpS2512;
      int32_t _M0L14capacity__maskS2513;
      int32_t _M0L6_2atmpS2511;
      if (_M0L4hashS2504 == _M0L4hashS767) {
        moonbit_string_t _M0L8_2afieldS4029 = _M0L14_2acurr__entryS774->$4;
        moonbit_string_t _M0L3keyS2503 = _M0L8_2afieldS4029;
        int32_t _M0L6_2atmpS4028;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4028
        = moonbit_val_array_equal(_M0L3keyS2503, _M0L3keyS771);
        _if__result_4523 = _M0L6_2atmpS4028;
      } else {
        _if__result_4523 = 0;
      }
      if (_if__result_4523) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4027;
        moonbit_incref(_M0L14_2acurr__entryS774);
        moonbit_decref(_M0L3keyS771);
        moonbit_decref(_M0L4selfS765);
        _M0L6_2aoldS4027 = _M0L14_2acurr__entryS774->$5;
        moonbit_decref(_M0L6_2aoldS4027);
        _M0L14_2acurr__entryS774->$5 = _M0L5valueS772;
        moonbit_decref(_M0L14_2acurr__entryS774);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS774);
      }
      _M0L3pslS2505 = _M0L14_2acurr__entryS774->$2;
      if (_M0L3pslS762 > _M0L3pslS2505) {
        int32_t _M0L4sizeS2506 = _M0L4selfS765->$1;
        int32_t _M0L8grow__atS2507 = _M0L4selfS765->$4;
        int32_t _M0L7_2abindS775;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS776;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS777;
        if (_M0L4sizeS2506 >= _M0L8grow__atS2507) {
          int32_t _M0L14capacity__maskS2509;
          int32_t _M0L6_2atmpS2508;
          moonbit_decref(_M0L14_2acurr__entryS774);
          moonbit_incref(_M0L4selfS765);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS765);
          _M0L14capacity__maskS2509 = _M0L4selfS765->$3;
          _M0L6_2atmpS2508 = _M0L4hashS767 & _M0L14capacity__maskS2509;
          _M0L3pslS762 = 0;
          _M0L3idxS763 = _M0L6_2atmpS2508;
          continue;
        }
        moonbit_incref(_M0L4selfS765);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS765, _M0L3idxS763, _M0L14_2acurr__entryS774);
        _M0L7_2abindS775 = _M0L4selfS765->$6;
        _M0L7_2abindS776 = 0;
        _M0L5entryS777
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS777)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS777->$0 = _M0L7_2abindS775;
        _M0L5entryS777->$1 = _M0L7_2abindS776;
        _M0L5entryS777->$2 = _M0L3pslS762;
        _M0L5entryS777->$3 = _M0L4hashS767;
        _M0L5entryS777->$4 = _M0L3keyS771;
        _M0L5entryS777->$5 = _M0L5valueS772;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS765, _M0L3idxS763, _M0L5entryS777);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS774);
      }
      _M0L6_2atmpS2510 = _M0L3pslS762 + 1;
      _M0L6_2atmpS2512 = _M0L3idxS763 + 1;
      _M0L14capacity__maskS2513 = _M0L4selfS765->$3;
      _M0L6_2atmpS2511 = _M0L6_2atmpS2512 & _M0L14capacity__maskS2513;
      _M0L3pslS762 = _M0L6_2atmpS2510;
      _M0L3idxS763 = _M0L6_2atmpS2511;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS781,
  int32_t _M0L3keyS787,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS788,
  int32_t _M0L4hashS783
) {
  int32_t _M0L14capacity__maskS2534;
  int32_t _M0L6_2atmpS2533;
  int32_t _M0L3pslS778;
  int32_t _M0L3idxS779;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2534 = _M0L4selfS781->$3;
  _M0L6_2atmpS2533 = _M0L4hashS783 & _M0L14capacity__maskS2534;
  _M0L3pslS778 = 0;
  _M0L3idxS779 = _M0L6_2atmpS2533;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4034 =
      _M0L4selfS781->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2532 =
      _M0L8_2afieldS4034;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4033;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS780;
    if (
      _M0L3idxS779 < 0
      || _M0L3idxS779 >= Moonbit_array_length(_M0L7entriesS2532)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4033
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2532[
        _M0L3idxS779
      ];
    _M0L7_2abindS780 = _M0L6_2atmpS4033;
    if (_M0L7_2abindS780 == 0) {
      int32_t _M0L4sizeS2517 = _M0L4selfS781->$1;
      int32_t _M0L8grow__atS2518 = _M0L4selfS781->$4;
      int32_t _M0L7_2abindS784;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS785;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS786;
      if (_M0L4sizeS2517 >= _M0L8grow__atS2518) {
        int32_t _M0L14capacity__maskS2520;
        int32_t _M0L6_2atmpS2519;
        moonbit_incref(_M0L4selfS781);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS781);
        _M0L14capacity__maskS2520 = _M0L4selfS781->$3;
        _M0L6_2atmpS2519 = _M0L4hashS783 & _M0L14capacity__maskS2520;
        _M0L3pslS778 = 0;
        _M0L3idxS779 = _M0L6_2atmpS2519;
        continue;
      }
      _M0L7_2abindS784 = _M0L4selfS781->$6;
      _M0L7_2abindS785 = 0;
      _M0L5entryS786
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS786)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS786->$0 = _M0L7_2abindS784;
      _M0L5entryS786->$1 = _M0L7_2abindS785;
      _M0L5entryS786->$2 = _M0L3pslS778;
      _M0L5entryS786->$3 = _M0L4hashS783;
      _M0L5entryS786->$4 = _M0L3keyS787;
      _M0L5entryS786->$5 = _M0L5valueS788;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS781, _M0L3idxS779, _M0L5entryS786);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS789 =
        _M0L7_2abindS780;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS790 =
        _M0L7_2aSomeS789;
      int32_t _M0L4hashS2522 = _M0L14_2acurr__entryS790->$3;
      int32_t _if__result_4525;
      int32_t _M0L3pslS2523;
      int32_t _M0L6_2atmpS2528;
      int32_t _M0L6_2atmpS2530;
      int32_t _M0L14capacity__maskS2531;
      int32_t _M0L6_2atmpS2529;
      if (_M0L4hashS2522 == _M0L4hashS783) {
        int32_t _M0L3keyS2521 = _M0L14_2acurr__entryS790->$4;
        _if__result_4525 = _M0L3keyS2521 == _M0L3keyS787;
      } else {
        _if__result_4525 = 0;
      }
      if (_if__result_4525) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS4032;
        moonbit_incref(_M0L14_2acurr__entryS790);
        moonbit_decref(_M0L4selfS781);
        _M0L6_2aoldS4032 = _M0L14_2acurr__entryS790->$5;
        moonbit_decref(_M0L6_2aoldS4032);
        _M0L14_2acurr__entryS790->$5 = _M0L5valueS788;
        moonbit_decref(_M0L14_2acurr__entryS790);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS790);
      }
      _M0L3pslS2523 = _M0L14_2acurr__entryS790->$2;
      if (_M0L3pslS778 > _M0L3pslS2523) {
        int32_t _M0L4sizeS2524 = _M0L4selfS781->$1;
        int32_t _M0L8grow__atS2525 = _M0L4selfS781->$4;
        int32_t _M0L7_2abindS791;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS792;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS793;
        if (_M0L4sizeS2524 >= _M0L8grow__atS2525) {
          int32_t _M0L14capacity__maskS2527;
          int32_t _M0L6_2atmpS2526;
          moonbit_decref(_M0L14_2acurr__entryS790);
          moonbit_incref(_M0L4selfS781);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS781);
          _M0L14capacity__maskS2527 = _M0L4selfS781->$3;
          _M0L6_2atmpS2526 = _M0L4hashS783 & _M0L14capacity__maskS2527;
          _M0L3pslS778 = 0;
          _M0L3idxS779 = _M0L6_2atmpS2526;
          continue;
        }
        moonbit_incref(_M0L4selfS781);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS781, _M0L3idxS779, _M0L14_2acurr__entryS790);
        _M0L7_2abindS791 = _M0L4selfS781->$6;
        _M0L7_2abindS792 = 0;
        _M0L5entryS793
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS793)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS793->$0 = _M0L7_2abindS791;
        _M0L5entryS793->$1 = _M0L7_2abindS792;
        _M0L5entryS793->$2 = _M0L3pslS778;
        _M0L5entryS793->$3 = _M0L4hashS783;
        _M0L5entryS793->$4 = _M0L3keyS787;
        _M0L5entryS793->$5 = _M0L5valueS788;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS781, _M0L3idxS779, _M0L5entryS793);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS790);
      }
      _M0L6_2atmpS2528 = _M0L3pslS778 + 1;
      _M0L6_2atmpS2530 = _M0L3idxS779 + 1;
      _M0L14capacity__maskS2531 = _M0L4selfS781->$3;
      _M0L6_2atmpS2529 = _M0L6_2atmpS2530 & _M0L14capacity__maskS2531;
      _M0L3pslS778 = _M0L6_2atmpS2528;
      _M0L3idxS779 = _M0L6_2atmpS2529;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS797,
  moonbit_string_t _M0L3keyS803,
  void* _M0L5valueS804,
  int32_t _M0L4hashS799
) {
  int32_t _M0L14capacity__maskS2552;
  int32_t _M0L6_2atmpS2551;
  int32_t _M0L3pslS794;
  int32_t _M0L3idxS795;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2552 = _M0L4selfS797->$3;
  _M0L6_2atmpS2551 = _M0L4hashS799 & _M0L14capacity__maskS2552;
  _M0L3pslS794 = 0;
  _M0L3idxS795 = _M0L6_2atmpS2551;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4039 = _M0L4selfS797->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2550 = _M0L8_2afieldS4039;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS4038;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS796;
    if (
      _M0L3idxS795 < 0
      || _M0L3idxS795 >= Moonbit_array_length(_M0L7entriesS2550)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4038
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2550[_M0L3idxS795];
    _M0L7_2abindS796 = _M0L6_2atmpS4038;
    if (_M0L7_2abindS796 == 0) {
      int32_t _M0L4sizeS2535 = _M0L4selfS797->$1;
      int32_t _M0L8grow__atS2536 = _M0L4selfS797->$4;
      int32_t _M0L7_2abindS800;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS801;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS802;
      if (_M0L4sizeS2535 >= _M0L8grow__atS2536) {
        int32_t _M0L14capacity__maskS2538;
        int32_t _M0L6_2atmpS2537;
        moonbit_incref(_M0L4selfS797);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS797);
        _M0L14capacity__maskS2538 = _M0L4selfS797->$3;
        _M0L6_2atmpS2537 = _M0L4hashS799 & _M0L14capacity__maskS2538;
        _M0L3pslS794 = 0;
        _M0L3idxS795 = _M0L6_2atmpS2537;
        continue;
      }
      _M0L7_2abindS800 = _M0L4selfS797->$6;
      _M0L7_2abindS801 = 0;
      _M0L5entryS802
      = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
      Moonbit_object_header(_M0L5entryS802)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
      _M0L5entryS802->$0 = _M0L7_2abindS800;
      _M0L5entryS802->$1 = _M0L7_2abindS801;
      _M0L5entryS802->$2 = _M0L3pslS794;
      _M0L5entryS802->$3 = _M0L4hashS799;
      _M0L5entryS802->$4 = _M0L3keyS803;
      _M0L5entryS802->$5 = _M0L5valueS804;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS797, _M0L3idxS795, _M0L5entryS802);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS805 = _M0L7_2abindS796;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS806 =
        _M0L7_2aSomeS805;
      int32_t _M0L4hashS2540 = _M0L14_2acurr__entryS806->$3;
      int32_t _if__result_4527;
      int32_t _M0L3pslS2541;
      int32_t _M0L6_2atmpS2546;
      int32_t _M0L6_2atmpS2548;
      int32_t _M0L14capacity__maskS2549;
      int32_t _M0L6_2atmpS2547;
      if (_M0L4hashS2540 == _M0L4hashS799) {
        moonbit_string_t _M0L8_2afieldS4037 = _M0L14_2acurr__entryS806->$4;
        moonbit_string_t _M0L3keyS2539 = _M0L8_2afieldS4037;
        int32_t _M0L6_2atmpS4036;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4036
        = moonbit_val_array_equal(_M0L3keyS2539, _M0L3keyS803);
        _if__result_4527 = _M0L6_2atmpS4036;
      } else {
        _if__result_4527 = 0;
      }
      if (_if__result_4527) {
        void* _M0L6_2aoldS4035;
        moonbit_incref(_M0L14_2acurr__entryS806);
        moonbit_decref(_M0L3keyS803);
        moonbit_decref(_M0L4selfS797);
        _M0L6_2aoldS4035 = _M0L14_2acurr__entryS806->$5;
        moonbit_decref(_M0L6_2aoldS4035);
        _M0L14_2acurr__entryS806->$5 = _M0L5valueS804;
        moonbit_decref(_M0L14_2acurr__entryS806);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS806);
      }
      _M0L3pslS2541 = _M0L14_2acurr__entryS806->$2;
      if (_M0L3pslS794 > _M0L3pslS2541) {
        int32_t _M0L4sizeS2542 = _M0L4selfS797->$1;
        int32_t _M0L8grow__atS2543 = _M0L4selfS797->$4;
        int32_t _M0L7_2abindS807;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS808;
        struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS809;
        if (_M0L4sizeS2542 >= _M0L8grow__atS2543) {
          int32_t _M0L14capacity__maskS2545;
          int32_t _M0L6_2atmpS2544;
          moonbit_decref(_M0L14_2acurr__entryS806);
          moonbit_incref(_M0L4selfS797);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB4JsonE(_M0L4selfS797);
          _M0L14capacity__maskS2545 = _M0L4selfS797->$3;
          _M0L6_2atmpS2544 = _M0L4hashS799 & _M0L14capacity__maskS2545;
          _M0L3pslS794 = 0;
          _M0L3idxS795 = _M0L6_2atmpS2544;
          continue;
        }
        moonbit_incref(_M0L4selfS797);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB4JsonE(_M0L4selfS797, _M0L3idxS795, _M0L14_2acurr__entryS806);
        _M0L7_2abindS807 = _M0L4selfS797->$6;
        _M0L7_2abindS808 = 0;
        _M0L5entryS809
        = (struct _M0TPB5EntryGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB4JsonE));
        Moonbit_object_header(_M0L5entryS809)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB4JsonE, $1) >> 2, 3, 0);
        _M0L5entryS809->$0 = _M0L7_2abindS807;
        _M0L5entryS809->$1 = _M0L7_2abindS808;
        _M0L5entryS809->$2 = _M0L3pslS794;
        _M0L5entryS809->$3 = _M0L4hashS799;
        _M0L5entryS809->$4 = _M0L3keyS803;
        _M0L5entryS809->$5 = _M0L5valueS804;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(_M0L4selfS797, _M0L3idxS795, _M0L5entryS809);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS806);
      }
      _M0L6_2atmpS2546 = _M0L3pslS794 + 1;
      _M0L6_2atmpS2548 = _M0L3idxS795 + 1;
      _M0L14capacity__maskS2549 = _M0L4selfS797->$3;
      _M0L6_2atmpS2547 = _M0L6_2atmpS2548 & _M0L14capacity__maskS2549;
      _M0L3pslS794 = _M0L6_2atmpS2546;
      _M0L3idxS795 = _M0L6_2atmpS2547;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS736,
  int32_t _M0L3idxS741,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS740
) {
  int32_t _M0L3pslS2466;
  int32_t _M0L6_2atmpS2462;
  int32_t _M0L6_2atmpS2464;
  int32_t _M0L14capacity__maskS2465;
  int32_t _M0L6_2atmpS2463;
  int32_t _M0L3pslS732;
  int32_t _M0L3idxS733;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS734;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2466 = _M0L5entryS740->$2;
  _M0L6_2atmpS2462 = _M0L3pslS2466 + 1;
  _M0L6_2atmpS2464 = _M0L3idxS741 + 1;
  _M0L14capacity__maskS2465 = _M0L4selfS736->$3;
  _M0L6_2atmpS2463 = _M0L6_2atmpS2464 & _M0L14capacity__maskS2465;
  _M0L3pslS732 = _M0L6_2atmpS2462;
  _M0L3idxS733 = _M0L6_2atmpS2463;
  _M0L5entryS734 = _M0L5entryS740;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4041 =
      _M0L4selfS736->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2461 =
      _M0L8_2afieldS4041;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4040;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS735;
    if (
      _M0L3idxS733 < 0
      || _M0L3idxS733 >= Moonbit_array_length(_M0L7entriesS2461)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4040
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2461[
        _M0L3idxS733
      ];
    _M0L7_2abindS735 = _M0L6_2atmpS4040;
    if (_M0L7_2abindS735 == 0) {
      _M0L5entryS734->$2 = _M0L3pslS732;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS736, _M0L5entryS734, _M0L3idxS733);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS738 =
        _M0L7_2abindS735;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS739 =
        _M0L7_2aSomeS738;
      int32_t _M0L3pslS2451 = _M0L14_2acurr__entryS739->$2;
      if (_M0L3pslS732 > _M0L3pslS2451) {
        int32_t _M0L3pslS2456;
        int32_t _M0L6_2atmpS2452;
        int32_t _M0L6_2atmpS2454;
        int32_t _M0L14capacity__maskS2455;
        int32_t _M0L6_2atmpS2453;
        _M0L5entryS734->$2 = _M0L3pslS732;
        moonbit_incref(_M0L14_2acurr__entryS739);
        moonbit_incref(_M0L4selfS736);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS736, _M0L5entryS734, _M0L3idxS733);
        _M0L3pslS2456 = _M0L14_2acurr__entryS739->$2;
        _M0L6_2atmpS2452 = _M0L3pslS2456 + 1;
        _M0L6_2atmpS2454 = _M0L3idxS733 + 1;
        _M0L14capacity__maskS2455 = _M0L4selfS736->$3;
        _M0L6_2atmpS2453 = _M0L6_2atmpS2454 & _M0L14capacity__maskS2455;
        _M0L3pslS732 = _M0L6_2atmpS2452;
        _M0L3idxS733 = _M0L6_2atmpS2453;
        _M0L5entryS734 = _M0L14_2acurr__entryS739;
        continue;
      } else {
        int32_t _M0L6_2atmpS2457 = _M0L3pslS732 + 1;
        int32_t _M0L6_2atmpS2459 = _M0L3idxS733 + 1;
        int32_t _M0L14capacity__maskS2460 = _M0L4selfS736->$3;
        int32_t _M0L6_2atmpS2458 =
          _M0L6_2atmpS2459 & _M0L14capacity__maskS2460;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_4529 =
          _M0L5entryS734;
        _M0L3pslS732 = _M0L6_2atmpS2457;
        _M0L3idxS733 = _M0L6_2atmpS2458;
        _M0L5entryS734 = _tmp_4529;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS746,
  int32_t _M0L3idxS751,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS750
) {
  int32_t _M0L3pslS2482;
  int32_t _M0L6_2atmpS2478;
  int32_t _M0L6_2atmpS2480;
  int32_t _M0L14capacity__maskS2481;
  int32_t _M0L6_2atmpS2479;
  int32_t _M0L3pslS742;
  int32_t _M0L3idxS743;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS744;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2482 = _M0L5entryS750->$2;
  _M0L6_2atmpS2478 = _M0L3pslS2482 + 1;
  _M0L6_2atmpS2480 = _M0L3idxS751 + 1;
  _M0L14capacity__maskS2481 = _M0L4selfS746->$3;
  _M0L6_2atmpS2479 = _M0L6_2atmpS2480 & _M0L14capacity__maskS2481;
  _M0L3pslS742 = _M0L6_2atmpS2478;
  _M0L3idxS743 = _M0L6_2atmpS2479;
  _M0L5entryS744 = _M0L5entryS750;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4043 =
      _M0L4selfS746->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2477 =
      _M0L8_2afieldS4043;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4042;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS745;
    if (
      _M0L3idxS743 < 0
      || _M0L3idxS743 >= Moonbit_array_length(_M0L7entriesS2477)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4042
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2477[
        _M0L3idxS743
      ];
    _M0L7_2abindS745 = _M0L6_2atmpS4042;
    if (_M0L7_2abindS745 == 0) {
      _M0L5entryS744->$2 = _M0L3pslS742;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS746, _M0L5entryS744, _M0L3idxS743);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS748 =
        _M0L7_2abindS745;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS749 =
        _M0L7_2aSomeS748;
      int32_t _M0L3pslS2467 = _M0L14_2acurr__entryS749->$2;
      if (_M0L3pslS742 > _M0L3pslS2467) {
        int32_t _M0L3pslS2472;
        int32_t _M0L6_2atmpS2468;
        int32_t _M0L6_2atmpS2470;
        int32_t _M0L14capacity__maskS2471;
        int32_t _M0L6_2atmpS2469;
        _M0L5entryS744->$2 = _M0L3pslS742;
        moonbit_incref(_M0L14_2acurr__entryS749);
        moonbit_incref(_M0L4selfS746);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS746, _M0L5entryS744, _M0L3idxS743);
        _M0L3pslS2472 = _M0L14_2acurr__entryS749->$2;
        _M0L6_2atmpS2468 = _M0L3pslS2472 + 1;
        _M0L6_2atmpS2470 = _M0L3idxS743 + 1;
        _M0L14capacity__maskS2471 = _M0L4selfS746->$3;
        _M0L6_2atmpS2469 = _M0L6_2atmpS2470 & _M0L14capacity__maskS2471;
        _M0L3pslS742 = _M0L6_2atmpS2468;
        _M0L3idxS743 = _M0L6_2atmpS2469;
        _M0L5entryS744 = _M0L14_2acurr__entryS749;
        continue;
      } else {
        int32_t _M0L6_2atmpS2473 = _M0L3pslS742 + 1;
        int32_t _M0L6_2atmpS2475 = _M0L3idxS743 + 1;
        int32_t _M0L14capacity__maskS2476 = _M0L4selfS746->$3;
        int32_t _M0L6_2atmpS2474 =
          _M0L6_2atmpS2475 & _M0L14capacity__maskS2476;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_4531 =
          _M0L5entryS744;
        _M0L3pslS742 = _M0L6_2atmpS2473;
        _M0L3idxS743 = _M0L6_2atmpS2474;
        _M0L5entryS744 = _tmp_4531;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS756,
  int32_t _M0L3idxS761,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS760
) {
  int32_t _M0L3pslS2498;
  int32_t _M0L6_2atmpS2494;
  int32_t _M0L6_2atmpS2496;
  int32_t _M0L14capacity__maskS2497;
  int32_t _M0L6_2atmpS2495;
  int32_t _M0L3pslS752;
  int32_t _M0L3idxS753;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS754;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2498 = _M0L5entryS760->$2;
  _M0L6_2atmpS2494 = _M0L3pslS2498 + 1;
  _M0L6_2atmpS2496 = _M0L3idxS761 + 1;
  _M0L14capacity__maskS2497 = _M0L4selfS756->$3;
  _M0L6_2atmpS2495 = _M0L6_2atmpS2496 & _M0L14capacity__maskS2497;
  _M0L3pslS752 = _M0L6_2atmpS2494;
  _M0L3idxS753 = _M0L6_2atmpS2495;
  _M0L5entryS754 = _M0L5entryS760;
  while (1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4045 = _M0L4selfS756->$0;
    struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2493 = _M0L8_2afieldS4045;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS4044;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS755;
    if (
      _M0L3idxS753 < 0
      || _M0L3idxS753 >= Moonbit_array_length(_M0L7entriesS2493)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4044
    = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2493[_M0L3idxS753];
    _M0L7_2abindS755 = _M0L6_2atmpS4044;
    if (_M0L7_2abindS755 == 0) {
      _M0L5entryS754->$2 = _M0L3pslS752;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS756, _M0L5entryS754, _M0L3idxS753);
      break;
    } else {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS758 = _M0L7_2abindS755;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L14_2acurr__entryS759 =
        _M0L7_2aSomeS758;
      int32_t _M0L3pslS2483 = _M0L14_2acurr__entryS759->$2;
      if (_M0L3pslS752 > _M0L3pslS2483) {
        int32_t _M0L3pslS2488;
        int32_t _M0L6_2atmpS2484;
        int32_t _M0L6_2atmpS2486;
        int32_t _M0L14capacity__maskS2487;
        int32_t _M0L6_2atmpS2485;
        _M0L5entryS754->$2 = _M0L3pslS752;
        moonbit_incref(_M0L14_2acurr__entryS759);
        moonbit_incref(_M0L4selfS756);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB4JsonE(_M0L4selfS756, _M0L5entryS754, _M0L3idxS753);
        _M0L3pslS2488 = _M0L14_2acurr__entryS759->$2;
        _M0L6_2atmpS2484 = _M0L3pslS2488 + 1;
        _M0L6_2atmpS2486 = _M0L3idxS753 + 1;
        _M0L14capacity__maskS2487 = _M0L4selfS756->$3;
        _M0L6_2atmpS2485 = _M0L6_2atmpS2486 & _M0L14capacity__maskS2487;
        _M0L3pslS752 = _M0L6_2atmpS2484;
        _M0L3idxS753 = _M0L6_2atmpS2485;
        _M0L5entryS754 = _M0L14_2acurr__entryS759;
        continue;
      } else {
        int32_t _M0L6_2atmpS2489 = _M0L3pslS752 + 1;
        int32_t _M0L6_2atmpS2491 = _M0L3idxS753 + 1;
        int32_t _M0L14capacity__maskS2492 = _M0L4selfS756->$3;
        int32_t _M0L6_2atmpS2490 =
          _M0L6_2atmpS2491 & _M0L14capacity__maskS2492;
        struct _M0TPB5EntryGsRPB4JsonE* _tmp_4533 = _M0L5entryS754;
        _M0L3pslS752 = _M0L6_2atmpS2489;
        _M0L3idxS753 = _M0L6_2atmpS2490;
        _M0L5entryS754 = _tmp_4533;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS714,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS716,
  int32_t _M0L8new__idxS715
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4048;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2445;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2446;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4047;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4046;
  int32_t _M0L6_2acntS4341;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS717;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4048 = _M0L4selfS714->$0;
  _M0L7entriesS2445 = _M0L8_2afieldS4048;
  moonbit_incref(_M0L5entryS716);
  _M0L6_2atmpS2446 = _M0L5entryS716;
  if (
    _M0L8new__idxS715 < 0
    || _M0L8new__idxS715 >= Moonbit_array_length(_M0L7entriesS2445)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4047
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2445[
      _M0L8new__idxS715
    ];
  if (_M0L6_2aoldS4047) {
    moonbit_decref(_M0L6_2aoldS4047);
  }
  _M0L7entriesS2445[_M0L8new__idxS715] = _M0L6_2atmpS2446;
  _M0L8_2afieldS4046 = _M0L5entryS716->$1;
  _M0L6_2acntS4341 = Moonbit_object_header(_M0L5entryS716)->rc;
  if (_M0L6_2acntS4341 > 1) {
    int32_t _M0L11_2anew__cntS4344 = _M0L6_2acntS4341 - 1;
    Moonbit_object_header(_M0L5entryS716)->rc = _M0L11_2anew__cntS4344;
    if (_M0L8_2afieldS4046) {
      moonbit_incref(_M0L8_2afieldS4046);
    }
  } else if (_M0L6_2acntS4341 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4343 =
      _M0L5entryS716->$5;
    moonbit_string_t _M0L8_2afieldS4342;
    moonbit_decref(_M0L8_2afieldS4343);
    _M0L8_2afieldS4342 = _M0L5entryS716->$4;
    moonbit_decref(_M0L8_2afieldS4342);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS716);
  }
  _M0L7_2abindS717 = _M0L8_2afieldS4046;
  if (_M0L7_2abindS717 == 0) {
    if (_M0L7_2abindS717) {
      moonbit_decref(_M0L7_2abindS717);
    }
    _M0L4selfS714->$6 = _M0L8new__idxS715;
    moonbit_decref(_M0L4selfS714);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS718;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS719;
    moonbit_decref(_M0L4selfS714);
    _M0L7_2aSomeS718 = _M0L7_2abindS717;
    _M0L7_2anextS719 = _M0L7_2aSomeS718;
    _M0L7_2anextS719->$0 = _M0L8new__idxS715;
    moonbit_decref(_M0L7_2anextS719);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS720,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS722,
  int32_t _M0L8new__idxS721
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4051;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2447;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2448;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4050;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4049;
  int32_t _M0L6_2acntS4345;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS723;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4051 = _M0L4selfS720->$0;
  _M0L7entriesS2447 = _M0L8_2afieldS4051;
  moonbit_incref(_M0L5entryS722);
  _M0L6_2atmpS2448 = _M0L5entryS722;
  if (
    _M0L8new__idxS721 < 0
    || _M0L8new__idxS721 >= Moonbit_array_length(_M0L7entriesS2447)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4050
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2447[
      _M0L8new__idxS721
    ];
  if (_M0L6_2aoldS4050) {
    moonbit_decref(_M0L6_2aoldS4050);
  }
  _M0L7entriesS2447[_M0L8new__idxS721] = _M0L6_2atmpS2448;
  _M0L8_2afieldS4049 = _M0L5entryS722->$1;
  _M0L6_2acntS4345 = Moonbit_object_header(_M0L5entryS722)->rc;
  if (_M0L6_2acntS4345 > 1) {
    int32_t _M0L11_2anew__cntS4347 = _M0L6_2acntS4345 - 1;
    Moonbit_object_header(_M0L5entryS722)->rc = _M0L11_2anew__cntS4347;
    if (_M0L8_2afieldS4049) {
      moonbit_incref(_M0L8_2afieldS4049);
    }
  } else if (_M0L6_2acntS4345 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4346 =
      _M0L5entryS722->$5;
    moonbit_decref(_M0L8_2afieldS4346);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS722);
  }
  _M0L7_2abindS723 = _M0L8_2afieldS4049;
  if (_M0L7_2abindS723 == 0) {
    if (_M0L7_2abindS723) {
      moonbit_decref(_M0L7_2abindS723);
    }
    _M0L4selfS720->$6 = _M0L8new__idxS721;
    moonbit_decref(_M0L4selfS720);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS724;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS725;
    moonbit_decref(_M0L4selfS720);
    _M0L7_2aSomeS724 = _M0L7_2abindS723;
    _M0L7_2anextS725 = _M0L7_2aSomeS724;
    _M0L7_2anextS725->$0 = _M0L8new__idxS721;
    moonbit_decref(_M0L7_2anextS725);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS726,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS728,
  int32_t _M0L8new__idxS727
) {
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4054;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2449;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2450;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4053;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS4052;
  int32_t _M0L6_2acntS4348;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS729;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4054 = _M0L4selfS726->$0;
  _M0L7entriesS2449 = _M0L8_2afieldS4054;
  moonbit_incref(_M0L5entryS728);
  _M0L6_2atmpS2450 = _M0L5entryS728;
  if (
    _M0L8new__idxS727 < 0
    || _M0L8new__idxS727 >= Moonbit_array_length(_M0L7entriesS2449)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4053
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2449[_M0L8new__idxS727];
  if (_M0L6_2aoldS4053) {
    moonbit_decref(_M0L6_2aoldS4053);
  }
  _M0L7entriesS2449[_M0L8new__idxS727] = _M0L6_2atmpS2450;
  _M0L8_2afieldS4052 = _M0L5entryS728->$1;
  _M0L6_2acntS4348 = Moonbit_object_header(_M0L5entryS728)->rc;
  if (_M0L6_2acntS4348 > 1) {
    int32_t _M0L11_2anew__cntS4351 = _M0L6_2acntS4348 - 1;
    Moonbit_object_header(_M0L5entryS728)->rc = _M0L11_2anew__cntS4351;
    if (_M0L8_2afieldS4052) {
      moonbit_incref(_M0L8_2afieldS4052);
    }
  } else if (_M0L6_2acntS4348 == 1) {
    void* _M0L8_2afieldS4350 = _M0L5entryS728->$5;
    moonbit_string_t _M0L8_2afieldS4349;
    moonbit_decref(_M0L8_2afieldS4350);
    _M0L8_2afieldS4349 = _M0L5entryS728->$4;
    moonbit_decref(_M0L8_2afieldS4349);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS728);
  }
  _M0L7_2abindS729 = _M0L8_2afieldS4052;
  if (_M0L7_2abindS729 == 0) {
    if (_M0L7_2abindS729) {
      moonbit_decref(_M0L7_2abindS729);
    }
    _M0L4selfS726->$6 = _M0L8new__idxS727;
    moonbit_decref(_M0L4selfS726);
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS730;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS731;
    moonbit_decref(_M0L4selfS726);
    _M0L7_2aSomeS730 = _M0L7_2abindS729;
    _M0L7_2anextS731 = _M0L7_2aSomeS730;
    _M0L7_2anextS731->$0 = _M0L8new__idxS727;
    moonbit_decref(_M0L7_2anextS731);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS703,
  int32_t _M0L3idxS705,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS704
) {
  int32_t _M0L7_2abindS702;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4056;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2423;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2424;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4055;
  int32_t _M0L4sizeS2426;
  int32_t _M0L6_2atmpS2425;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS702 = _M0L4selfS703->$6;
  switch (_M0L7_2abindS702) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2418;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4057;
      moonbit_incref(_M0L5entryS704);
      _M0L6_2atmpS2418 = _M0L5entryS704;
      _M0L6_2aoldS4057 = _M0L4selfS703->$5;
      if (_M0L6_2aoldS4057) {
        moonbit_decref(_M0L6_2aoldS4057);
      }
      _M0L4selfS703->$5 = _M0L6_2atmpS2418;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4060 =
        _M0L4selfS703->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2422 =
        _M0L8_2afieldS4060;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4059;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2421;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2419;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2420;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4058;
      if (
        _M0L7_2abindS702 < 0
        || _M0L7_2abindS702 >= Moonbit_array_length(_M0L7entriesS2422)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4059
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2422[
          _M0L7_2abindS702
        ];
      _M0L6_2atmpS2421 = _M0L6_2atmpS4059;
      if (_M0L6_2atmpS2421) {
        moonbit_incref(_M0L6_2atmpS2421);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2419
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2421);
      moonbit_incref(_M0L5entryS704);
      _M0L6_2atmpS2420 = _M0L5entryS704;
      _M0L6_2aoldS4058 = _M0L6_2atmpS2419->$1;
      if (_M0L6_2aoldS4058) {
        moonbit_decref(_M0L6_2aoldS4058);
      }
      _M0L6_2atmpS2419->$1 = _M0L6_2atmpS2420;
      moonbit_decref(_M0L6_2atmpS2419);
      break;
    }
  }
  _M0L4selfS703->$6 = _M0L3idxS705;
  _M0L8_2afieldS4056 = _M0L4selfS703->$0;
  _M0L7entriesS2423 = _M0L8_2afieldS4056;
  _M0L6_2atmpS2424 = _M0L5entryS704;
  if (
    _M0L3idxS705 < 0
    || _M0L3idxS705 >= Moonbit_array_length(_M0L7entriesS2423)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4055
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2423[
      _M0L3idxS705
    ];
  if (_M0L6_2aoldS4055) {
    moonbit_decref(_M0L6_2aoldS4055);
  }
  _M0L7entriesS2423[_M0L3idxS705] = _M0L6_2atmpS2424;
  _M0L4sizeS2426 = _M0L4selfS703->$1;
  _M0L6_2atmpS2425 = _M0L4sizeS2426 + 1;
  _M0L4selfS703->$1 = _M0L6_2atmpS2425;
  moonbit_decref(_M0L4selfS703);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS707,
  int32_t _M0L3idxS709,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS708
) {
  int32_t _M0L7_2abindS706;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4062;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2432;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2433;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4061;
  int32_t _M0L4sizeS2435;
  int32_t _M0L6_2atmpS2434;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS706 = _M0L4selfS707->$6;
  switch (_M0L7_2abindS706) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2427;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4063;
      moonbit_incref(_M0L5entryS708);
      _M0L6_2atmpS2427 = _M0L5entryS708;
      _M0L6_2aoldS4063 = _M0L4selfS707->$5;
      if (_M0L6_2aoldS4063) {
        moonbit_decref(_M0L6_2aoldS4063);
      }
      _M0L4selfS707->$5 = _M0L6_2atmpS2427;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4066 =
        _M0L4selfS707->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2431 =
        _M0L8_2afieldS4066;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4065;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2430;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2428;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2429;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4064;
      if (
        _M0L7_2abindS706 < 0
        || _M0L7_2abindS706 >= Moonbit_array_length(_M0L7entriesS2431)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4065
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2431[
          _M0L7_2abindS706
        ];
      _M0L6_2atmpS2430 = _M0L6_2atmpS4065;
      if (_M0L6_2atmpS2430) {
        moonbit_incref(_M0L6_2atmpS2430);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2428
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2430);
      moonbit_incref(_M0L5entryS708);
      _M0L6_2atmpS2429 = _M0L5entryS708;
      _M0L6_2aoldS4064 = _M0L6_2atmpS2428->$1;
      if (_M0L6_2aoldS4064) {
        moonbit_decref(_M0L6_2aoldS4064);
      }
      _M0L6_2atmpS2428->$1 = _M0L6_2atmpS2429;
      moonbit_decref(_M0L6_2atmpS2428);
      break;
    }
  }
  _M0L4selfS707->$6 = _M0L3idxS709;
  _M0L8_2afieldS4062 = _M0L4selfS707->$0;
  _M0L7entriesS2432 = _M0L8_2afieldS4062;
  _M0L6_2atmpS2433 = _M0L5entryS708;
  if (
    _M0L3idxS709 < 0
    || _M0L3idxS709 >= Moonbit_array_length(_M0L7entriesS2432)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4061
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2432[
      _M0L3idxS709
    ];
  if (_M0L6_2aoldS4061) {
    moonbit_decref(_M0L6_2aoldS4061);
  }
  _M0L7entriesS2432[_M0L3idxS709] = _M0L6_2atmpS2433;
  _M0L4sizeS2435 = _M0L4selfS707->$1;
  _M0L6_2atmpS2434 = _M0L4sizeS2435 + 1;
  _M0L4selfS707->$1 = _M0L6_2atmpS2434;
  moonbit_decref(_M0L4selfS707);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS711,
  int32_t _M0L3idxS713,
  struct _M0TPB5EntryGsRPB4JsonE* _M0L5entryS712
) {
  int32_t _M0L7_2abindS710;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4068;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2441;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2442;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4067;
  int32_t _M0L4sizeS2444;
  int32_t _M0L6_2atmpS2443;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS710 = _M0L4selfS711->$6;
  switch (_M0L7_2abindS710) {
    case -1: {
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2436;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4069;
      moonbit_incref(_M0L5entryS712);
      _M0L6_2atmpS2436 = _M0L5entryS712;
      _M0L6_2aoldS4069 = _M0L4selfS711->$5;
      if (_M0L6_2aoldS4069) {
        moonbit_decref(_M0L6_2aoldS4069);
      }
      _M0L4selfS711->$5 = _M0L6_2atmpS2436;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4072 = _M0L4selfS711->$0;
      struct _M0TPB5EntryGsRPB4JsonE** _M0L7entriesS2440 = _M0L8_2afieldS4072;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS4071;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2439;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2437;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2438;
      struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS4070;
      if (
        _M0L7_2abindS710 < 0
        || _M0L7_2abindS710 >= Moonbit_array_length(_M0L7entriesS2440)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4071
      = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2440[_M0L7_2abindS710];
      _M0L6_2atmpS2439 = _M0L6_2atmpS4071;
      if (_M0L6_2atmpS2439) {
        moonbit_incref(_M0L6_2atmpS2439);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2437
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(_M0L6_2atmpS2439);
      moonbit_incref(_M0L5entryS712);
      _M0L6_2atmpS2438 = _M0L5entryS712;
      _M0L6_2aoldS4070 = _M0L6_2atmpS2437->$1;
      if (_M0L6_2aoldS4070) {
        moonbit_decref(_M0L6_2aoldS4070);
      }
      _M0L6_2atmpS2437->$1 = _M0L6_2atmpS2438;
      moonbit_decref(_M0L6_2atmpS2437);
      break;
    }
  }
  _M0L4selfS711->$6 = _M0L3idxS713;
  _M0L8_2afieldS4068 = _M0L4selfS711->$0;
  _M0L7entriesS2441 = _M0L8_2afieldS4068;
  _M0L6_2atmpS2442 = _M0L5entryS712;
  if (
    _M0L3idxS713 < 0
    || _M0L3idxS713 >= Moonbit_array_length(_M0L7entriesS2441)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4067
  = (struct _M0TPB5EntryGsRPB4JsonE*)_M0L7entriesS2441[_M0L3idxS713];
  if (_M0L6_2aoldS4067) {
    moonbit_decref(_M0L6_2aoldS4067);
  }
  _M0L7entriesS2441[_M0L3idxS713] = _M0L6_2atmpS2442;
  _M0L4sizeS2444 = _M0L4selfS711->$1;
  _M0L6_2atmpS2443 = _M0L4sizeS2444 + 1;
  _M0L4selfS711->$1 = _M0L6_2atmpS2443;
  moonbit_decref(_M0L4selfS711);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS685
) {
  int32_t _M0L8capacityS684;
  int32_t _M0L7_2abindS686;
  int32_t _M0L7_2abindS687;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2415;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS688;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS689;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_4534;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS684
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS685);
  _M0L7_2abindS686 = _M0L8capacityS684 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS687 = _M0FPB21calc__grow__threshold(_M0L8capacityS684);
  _M0L6_2atmpS2415 = 0;
  _M0L7_2abindS688
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS684, _M0L6_2atmpS2415);
  _M0L7_2abindS689 = 0;
  _block_4534
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_4534)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_4534->$0 = _M0L7_2abindS688;
  _block_4534->$1 = 0;
  _block_4534->$2 = _M0L8capacityS684;
  _block_4534->$3 = _M0L7_2abindS686;
  _block_4534->$4 = _M0L7_2abindS687;
  _block_4534->$5 = _M0L7_2abindS689;
  _block_4534->$6 = -1;
  return _block_4534;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS691
) {
  int32_t _M0L8capacityS690;
  int32_t _M0L7_2abindS692;
  int32_t _M0L7_2abindS693;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2416;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS694;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS695;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4535;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS690
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS691);
  _M0L7_2abindS692 = _M0L8capacityS690 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS693 = _M0FPB21calc__grow__threshold(_M0L8capacityS690);
  _M0L6_2atmpS2416 = 0;
  _M0L7_2abindS694
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS690, _M0L6_2atmpS2416);
  _M0L7_2abindS695 = 0;
  _block_4535
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4535)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4535->$0 = _M0L7_2abindS694;
  _block_4535->$1 = 0;
  _block_4535->$2 = _M0L8capacityS690;
  _block_4535->$3 = _M0L7_2abindS692;
  _block_4535->$4 = _M0L7_2abindS693;
  _block_4535->$5 = _M0L7_2abindS695;
  _block_4535->$6 = -1;
  return _block_4535;
}

struct _M0TPB3MapGsRPB4JsonE* _M0MPB3Map11new_2einnerGsRPB4JsonE(
  int32_t _M0L8capacityS697
) {
  int32_t _M0L8capacityS696;
  int32_t _M0L7_2abindS698;
  int32_t _M0L7_2abindS699;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2atmpS2417;
  struct _M0TPB5EntryGsRPB4JsonE** _M0L7_2abindS700;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS701;
  struct _M0TPB3MapGsRPB4JsonE* _block_4536;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS696
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS697);
  _M0L7_2abindS698 = _M0L8capacityS696 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS699 = _M0FPB21calc__grow__threshold(_M0L8capacityS696);
  _M0L6_2atmpS2417 = 0;
  _M0L7_2abindS700
  = (struct _M0TPB5EntryGsRPB4JsonE**)moonbit_make_ref_array(_M0L8capacityS696, _M0L6_2atmpS2417);
  _M0L7_2abindS701 = 0;
  _block_4536
  = (struct _M0TPB3MapGsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB4JsonE));
  Moonbit_object_header(_block_4536)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB4JsonE, $0) >> 2, 2, 0);
  _block_4536->$0 = _M0L7_2abindS700;
  _block_4536->$1 = 0;
  _block_4536->$2 = _M0L8capacityS696;
  _block_4536->$3 = _M0L7_2abindS698;
  _block_4536->$4 = _M0L7_2abindS699;
  _block_4536->$5 = _M0L7_2abindS701;
  _block_4536->$6 = -1;
  return _block_4536;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS683) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS683 >= 0) {
    int32_t _M0L6_2atmpS2414;
    int32_t _M0L6_2atmpS2413;
    int32_t _M0L6_2atmpS2412;
    int32_t _M0L6_2atmpS2411;
    if (_M0L4selfS683 <= 1) {
      return 1;
    }
    if (_M0L4selfS683 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2414 = _M0L4selfS683 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2413 = moonbit_clz32(_M0L6_2atmpS2414);
    _M0L6_2atmpS2412 = _M0L6_2atmpS2413 - 1;
    _M0L6_2atmpS2411 = 2147483647 >> (_M0L6_2atmpS2412 & 31);
    return _M0L6_2atmpS2411 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS682) {
  int32_t _M0L6_2atmpS2410;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2410 = _M0L8capacityS682 * 13;
  return _M0L6_2atmpS2410 / 16;
}

int32_t _M0MPC15array5Array3setGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L4selfS675,
  int32_t _M0L5indexS676,
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L5valueS677
) {
  int32_t _M0L3lenS674;
  int32_t _if__result_4537;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS674 = _M0L4selfS675->$1;
  if (_M0L5indexS676 >= 0) {
    _if__result_4537 = _M0L5indexS676 < _M0L3lenS674;
  } else {
    _if__result_4537 = 0;
  }
  if (_if__result_4537) {
    struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L6_2atmpS2408;
    struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2aoldS4073;
    #line 265 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS2408
    = _M0MPC15array5Array6bufferGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(_M0L4selfS675);
    if (
      _M0L5indexS676 < 0
      || _M0L5indexS676 >= Moonbit_array_length(_M0L6_2atmpS2408)
    ) {
      #line 265 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4073
    = (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*)_M0L6_2atmpS2408[
        _M0L5indexS676
      ];
    if (_M0L6_2aoldS4073) {
      moonbit_decref(_M0L6_2aoldS4073);
    }
    _M0L6_2atmpS2408[_M0L5indexS676] = _M0L5valueS677;
    moonbit_decref(_M0L6_2atmpS2408);
  } else {
    if (_M0L5valueS677) {
      moonbit_decref(_M0L5valueS677);
    }
    moonbit_decref(_M0L4selfS675);
    #line 264 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPC15array5Array3setGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS679,
  int32_t _M0L5indexS680,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L5valueS681
) {
  int32_t _M0L3lenS678;
  int32_t _if__result_4538;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS678 = _M0L4selfS679->$1;
  if (_M0L5indexS680 >= 0) {
    _if__result_4538 = _M0L5indexS680 < _M0L3lenS678;
  } else {
    _if__result_4538 = 0;
  }
  if (_if__result_4538) {
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L6_2atmpS2409;
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2aoldS4074;
    #line 265 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS2409
    = _M0MPC15array5Array6bufferGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L4selfS679);
    if (
      _M0L5indexS680 < 0
      || _M0L5indexS680 >= Moonbit_array_length(_M0L6_2atmpS2409)
    ) {
      #line 265 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS4074
    = (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*)_M0L6_2atmpS2409[
        _M0L5indexS680
      ];
    if (_M0L6_2aoldS4074) {
      moonbit_decref(_M0L6_2aoldS4074);
    }
    _M0L6_2atmpS2409[_M0L5indexS680] = _M0L5valueS681;
    moonbit_decref(_M0L6_2atmpS2409);
  } else {
    if (_M0L5valueS681) {
      moonbit_decref(_M0L5valueS681);
    }
    moonbit_decref(_M0L4selfS679);
    #line 264 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
  return 0;
}

moonbit_string_t _M0MPC16option6Option3mapGRPB13StringBuildersE(
  struct _M0TPB13StringBuilder* _M0L4selfS666,
  struct _M0TWRPB13StringBuilderEs* _M0L1fS669
) {
  #line 131 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS666 == 0) {
    moonbit_decref(_M0L1fS669);
    if (_M0L4selfS666) {
      moonbit_decref(_M0L4selfS666);
    }
    return 0;
  } else {
    struct _M0TPB13StringBuilder* _M0L7_2aSomeS667 = _M0L4selfS666;
    struct _M0TPB13StringBuilder* _M0L4_2atS668 = _M0L7_2aSomeS667;
    moonbit_string_t _M0L6_2atmpS2406;
    #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    _M0L6_2atmpS2406 = _M0L1fS669->code(_M0L1fS669, _M0L4_2atS668);
    return _M0L6_2atmpS2406;
  }
}

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MPC16option6Option3mapGRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L4selfS670,
  struct _M0TWRP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderERP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L1fS673
) {
  #line 131 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS670 == 0) {
    moonbit_decref(_M0L1fS673);
    if (_M0L4selfS670) {
      moonbit_decref(_M0L4selfS670);
    }
    return 0;
  } else {
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L7_2aSomeS671 =
      _M0L4selfS670;
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L4_2atS672 =
      _M0L7_2aSomeS671;
    struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2atmpS2407;
    #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    _M0L6_2atmpS2407 = _M0L1fS673->code(_M0L1fS673, _M0L4_2atS672);
    return _M0L6_2atmpS2407;
  }
}

moonbit_string_t _M0MPC16option6Option10unwrap__orGsE(
  moonbit_string_t _M0L4selfS663,
  moonbit_string_t _M0L7defaultS664
) {
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS663 == 0) {
    if (_M0L4selfS663) {
      moonbit_decref(_M0L4selfS663);
    }
    return _M0L7defaultS664;
  } else {
    moonbit_string_t _M0L7_2aSomeS665;
    moonbit_decref(_M0L7defaultS664);
    _M0L7_2aSomeS665 = _M0L4selfS663;
    return _M0L7_2aSomeS665;
  }
}

moonbit_string_t _M0MPC16option6Option6unwrapGsE(
  moonbit_string_t _M0L4selfS653
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS653 == 0) {
    if (_M0L4selfS653) {
      moonbit_decref(_M0L4selfS653);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    moonbit_string_t _M0L7_2aSomeS654 = _M0L4selfS653;
    return _M0L7_2aSomeS654;
  }
}

int32_t _M0MPC16option6Option6unwrapGiE(int64_t _M0L4selfS655) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS655 == 4294967296ll) {
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    int64_t _M0L7_2aSomeS656 = _M0L4selfS655;
    return (int32_t)_M0L7_2aSomeS656;
  }
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS657
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS657 == 0) {
    if (_M0L4selfS657) {
      moonbit_decref(_M0L4selfS657);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS658 =
      _M0L4selfS657;
    return _M0L7_2aSomeS658;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS659
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS659 == 0) {
    if (_M0L4selfS659) {
      moonbit_decref(_M0L4selfS659);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS660 =
      _M0L4selfS659;
    return _M0L7_2aSomeS660;
  }
}

struct _M0TPB5EntryGsRPB4JsonE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB4JsonEE(
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4selfS661
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS661 == 0) {
    if (_M0L4selfS661) {
      moonbit_decref(_M0L4selfS661);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS662 = _M0L4selfS661;
    return _M0L7_2aSomeS662;
  }
}

struct _M0TPB5ArrayGRPB4JsonE* _M0MPC15array5Array12make__uninitGRPB4JsonE(
  int32_t _M0L3lenS652
) {
  void** _M0L6_2atmpS2405;
  struct _M0TPB5ArrayGRPB4JsonE* _block_4539;
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2405
  = (void**)moonbit_make_ref_array(_M0L3lenS652, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _block_4539
  = (struct _M0TPB5ArrayGRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPB4JsonE));
  Moonbit_object_header(_block_4539)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPB4JsonE, $0) >> 2, 1, 0);
  _block_4539->$0 = _M0L6_2atmpS2405;
  _block_4539->$1 = _M0L3lenS652;
  return _block_4539;
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS651
) {
  moonbit_string_t* _M0L6_2atmpS2404;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2404 = _M0L4selfS651;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2404);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS647,
  int32_t _M0L5indexS648
) {
  uint64_t* _M0L6_2atmpS2402;
  uint64_t _M0L6_2atmpS4075;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2402 = _M0L4selfS647;
  if (
    _M0L5indexS648 < 0
    || _M0L5indexS648 >= Moonbit_array_length(_M0L6_2atmpS2402)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4075 = (uint64_t)_M0L6_2atmpS2402[_M0L5indexS648];
  moonbit_decref(_M0L6_2atmpS2402);
  return _M0L6_2atmpS4075;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS649,
  int32_t _M0L5indexS650
) {
  uint32_t* _M0L6_2atmpS2403;
  uint32_t _M0L6_2atmpS4076;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2403 = _M0L4selfS649;
  if (
    _M0L5indexS650 < 0
    || _M0L5indexS650 >= Moonbit_array_length(_M0L6_2atmpS2403)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4076 = (uint32_t)_M0L6_2atmpS2403[_M0L5indexS650];
  moonbit_decref(_M0L6_2atmpS2403);
  return _M0L6_2atmpS4076;
}

struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0MPC15array5Array4iterGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS646
) {
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L8_2afieldS4078;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L3bufS2400;
  int32_t _M0L8_2afieldS4077;
  int32_t _M0L6_2acntS4352;
  int32_t _M0L3lenS2401;
  struct _M0TPB9ArrayViewGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE _M0L6_2atmpS2399;
  #line 1651 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS4078 = _M0L4selfS646->$0;
  _M0L3bufS2400 = _M0L8_2afieldS4078;
  _M0L8_2afieldS4077 = _M0L4selfS646->$1;
  _M0L6_2acntS4352 = Moonbit_object_header(_M0L4selfS646)->rc;
  if (_M0L6_2acntS4352 > 1) {
    int32_t _M0L11_2anew__cntS4353 = _M0L6_2acntS4352 - 1;
    Moonbit_object_header(_M0L4selfS646)->rc = _M0L11_2anew__cntS4353;
    moonbit_incref(_M0L3bufS2400);
  } else if (_M0L6_2acntS4352 == 1) {
    #line 1653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_free(_M0L4selfS646);
  }
  _M0L3lenS2401 = _M0L8_2afieldS4077;
  _M0L6_2atmpS2399
  = (struct _M0TPB9ArrayViewGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE){
    0, _M0L3lenS2401, _M0L3bufS2400
  };
  #line 1653 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  return _M0MPC15array9ArrayView4iterGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L6_2atmpS2399);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS645
) {
  moonbit_string_t* _M0L6_2atmpS2397;
  int32_t _M0L6_2atmpS4079;
  int32_t _M0L6_2atmpS2398;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2396;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS645);
  _M0L6_2atmpS2397 = _M0L4selfS645;
  _M0L6_2atmpS4079 = Moonbit_array_length(_M0L4selfS645);
  moonbit_decref(_M0L4selfS645);
  _M0L6_2atmpS2398 = _M0L6_2atmpS4079;
  _M0L6_2atmpS2396
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2398, _M0L6_2atmpS2397
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2396);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS640
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS639;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2373__l570__* _closure_4540;
  struct _M0TWEOs* _M0L6_2atmpS2372;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS639
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS639)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS639->$0 = 0;
  _closure_4540
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2373__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2373__l570__));
  Moonbit_object_header(_closure_4540)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2373__l570__, $0_0) >> 2, 2, 0);
  _closure_4540->code = &_M0MPC15array9ArrayView4iterGsEC2373l570;
  _closure_4540->$0_0 = _M0L4selfS640.$0;
  _closure_4540->$0_1 = _M0L4selfS640.$1;
  _closure_4540->$0_2 = _M0L4selfS640.$2;
  _closure_4540->$1 = _M0L1iS639;
  _M0L6_2atmpS2372 = (struct _M0TWEOs*)_closure_4540;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2372);
}

struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0MPC15array9ArrayView4iterGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB9ArrayViewGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE _M0L4selfS643
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS642;
  struct _M0R125ArrayView_3a_3aiter_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_5d_7c_2eanon__u2385__l570__* _closure_4541;
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L6_2atmpS2384;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS642
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS642)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS642->$0 = 0;
  _closure_4541
  = (struct _M0R125ArrayView_3a_3aiter_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_5d_7c_2eanon__u2385__l570__*)moonbit_malloc(sizeof(struct _M0R125ArrayView_3a_3aiter_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_5d_7c_2eanon__u2385__l570__));
  Moonbit_object_header(_closure_4541)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R125ArrayView_3a_3aiter_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_5d_7c_2eanon__u2385__l570__, $0_0) >> 2, 2, 0);
  _closure_4541->code
  = &_M0MPC15array9ArrayView4iterGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEC2385l570;
  _closure_4541->$0_0 = _M0L4selfS643.$0;
  _closure_4541->$0_1 = _M0L4selfS643.$1;
  _closure_4541->$0_2 = _M0L4selfS643.$2;
  _closure_4541->$1 = _M0L1iS642;
  _M0L6_2atmpS2384
  = (struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE*)_closure_4541;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L6_2atmpS2384);
}

void* _M0MPC15array9ArrayView4iterGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEC2385l570(
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L6_2aenvS2386
) {
  struct _M0R125ArrayView_3a_3aiter_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_5d_7c_2eanon__u2385__l570__* _M0L14_2acasted__envS2387;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4084;
  struct _M0TPC13ref3RefGiE* _M0L1iS642;
  struct _M0TPB9ArrayViewGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE _M0L8_2afieldS4083;
  int32_t _M0L6_2acntS4354;
  struct _M0TPB9ArrayViewGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE _M0L4selfS643;
  int32_t _M0L3valS2388;
  int32_t _M0L6_2atmpS2389;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2387
  = (struct _M0R125ArrayView_3a_3aiter_7c_5bclawteam_2fclawteam_2finternal_2fopenai_2fChatCompletionChoiceBuilder_3f_5d_7c_2eanon__u2385__l570__*)_M0L6_2aenvS2386;
  _M0L8_2afieldS4084 = _M0L14_2acasted__envS2387->$1;
  _M0L1iS642 = _M0L8_2afieldS4084;
  _M0L8_2afieldS4083
  = (struct _M0TPB9ArrayViewGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE){
    _M0L14_2acasted__envS2387->$0_1,
      _M0L14_2acasted__envS2387->$0_2,
      _M0L14_2acasted__envS2387->$0_0
  };
  _M0L6_2acntS4354 = Moonbit_object_header(_M0L14_2acasted__envS2387)->rc;
  if (_M0L6_2acntS4354 > 1) {
    int32_t _M0L11_2anew__cntS4355 = _M0L6_2acntS4354 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2387)->rc
    = _M0L11_2anew__cntS4355;
    moonbit_incref(_M0L1iS642);
    moonbit_incref(_M0L8_2afieldS4083.$0);
  } else if (_M0L6_2acntS4354 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2387);
  }
  _M0L4selfS643 = _M0L8_2afieldS4083;
  _M0L3valS2388 = _M0L1iS642->$0;
  moonbit_incref(_M0L4selfS643.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2389
  = _M0MPC15array9ArrayView6lengthGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L4selfS643);
  if (_M0L3valS2388 < _M0L6_2atmpS2389) {
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L8_2afieldS4082 =
      _M0L4selfS643.$0;
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L3bufS2392 =
      _M0L8_2afieldS4082;
    int32_t _M0L8_2afieldS4081 = _M0L4selfS643.$1;
    int32_t _M0L5startS2394 = _M0L8_2afieldS4081;
    int32_t _M0L3valS2395 = _M0L1iS642->$0;
    int32_t _M0L6_2atmpS2393 = _M0L5startS2394 + _M0L3valS2395;
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2atmpS4080 =
      (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*)_M0L3bufS2392[
        _M0L6_2atmpS2393
      ];
    struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L4elemS644;
    int32_t _M0L3valS2391;
    int32_t _M0L6_2atmpS2390;
    void* _block_4542;
    if (_M0L6_2atmpS4080) {
      moonbit_incref(_M0L6_2atmpS4080);
    }
    moonbit_decref(_M0L3bufS2392);
    _M0L4elemS644 = _M0L6_2atmpS4080;
    _M0L3valS2391 = _M0L1iS642->$0;
    _M0L6_2atmpS2390 = _M0L3valS2391 + 1;
    _M0L1iS642->$0 = _M0L6_2atmpS2390;
    moonbit_decref(_M0L1iS642);
    _block_4542
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some));
    Moonbit_object_header(_block_4542)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some, $0) >> 2, 1, 1);
    ((struct _M0DTPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE4Some*)_block_4542)->$0
    = _M0L4elemS644;
    return _block_4542;
  } else {
    moonbit_decref(_M0L4selfS643.$0);
    moonbit_decref(_M0L1iS642);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2373l570(
  struct _M0TWEOs* _M0L6_2aenvS2374
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2373__l570__* _M0L14_2acasted__envS2375;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4089;
  struct _M0TPC13ref3RefGiE* _M0L1iS639;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS4088;
  int32_t _M0L6_2acntS4356;
  struct _M0TPB9ArrayViewGsE _M0L4selfS640;
  int32_t _M0L3valS2376;
  int32_t _M0L6_2atmpS2377;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2375
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2373__l570__*)_M0L6_2aenvS2374;
  _M0L8_2afieldS4089 = _M0L14_2acasted__envS2375->$1;
  _M0L1iS639 = _M0L8_2afieldS4089;
  _M0L8_2afieldS4088
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2375->$0_1,
      _M0L14_2acasted__envS2375->$0_2,
      _M0L14_2acasted__envS2375->$0_0
  };
  _M0L6_2acntS4356 = Moonbit_object_header(_M0L14_2acasted__envS2375)->rc;
  if (_M0L6_2acntS4356 > 1) {
    int32_t _M0L11_2anew__cntS4357 = _M0L6_2acntS4356 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2375)->rc
    = _M0L11_2anew__cntS4357;
    moonbit_incref(_M0L1iS639);
    moonbit_incref(_M0L8_2afieldS4088.$0);
  } else if (_M0L6_2acntS4356 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2375);
  }
  _M0L4selfS640 = _M0L8_2afieldS4088;
  _M0L3valS2376 = _M0L1iS639->$0;
  moonbit_incref(_M0L4selfS640.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2377 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS640);
  if (_M0L3valS2376 < _M0L6_2atmpS2377) {
    moonbit_string_t* _M0L8_2afieldS4087 = _M0L4selfS640.$0;
    moonbit_string_t* _M0L3bufS2380 = _M0L8_2afieldS4087;
    int32_t _M0L8_2afieldS4086 = _M0L4selfS640.$1;
    int32_t _M0L5startS2382 = _M0L8_2afieldS4086;
    int32_t _M0L3valS2383 = _M0L1iS639->$0;
    int32_t _M0L6_2atmpS2381 = _M0L5startS2382 + _M0L3valS2383;
    moonbit_string_t _M0L6_2atmpS4085 =
      (moonbit_string_t)_M0L3bufS2380[_M0L6_2atmpS2381];
    moonbit_string_t _M0L4elemS641;
    int32_t _M0L3valS2379;
    int32_t _M0L6_2atmpS2378;
    moonbit_incref(_M0L6_2atmpS4085);
    moonbit_decref(_M0L3bufS2380);
    _M0L4elemS641 = _M0L6_2atmpS4085;
    _M0L3valS2379 = _M0L1iS639->$0;
    _M0L6_2atmpS2378 = _M0L3valS2379 + 1;
    _M0L1iS639->$0 = _M0L6_2atmpS2378;
    moonbit_decref(_M0L1iS639);
    return _M0L4elemS641;
  } else {
    moonbit_decref(_M0L4selfS640.$0);
    moonbit_decref(_M0L1iS639);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS638
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS638;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS637,
  struct _M0TPB6Logger _M0L6loggerS636
) {
  moonbit_string_t _M0L6_2atmpS2371;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2371
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS637, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS636.$0->$method_0(_M0L6loggerS636.$1, _M0L6_2atmpS2371);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS635,
  struct _M0TPB6Logger _M0L6loggerS634
) {
  moonbit_string_t _M0L6_2atmpS2370;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2370 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS635, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS634.$0->$method_0(_M0L6loggerS634.$1, _M0L6_2atmpS2370);
  return 0;
}

struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0MPB4Iter9to__arrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L4selfS630
) {
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L6_2atmpS2369;
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0L6resultS628;
  #line 674 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L6_2atmpS2369
  = (struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice**)moonbit_empty_ref_array;
  _M0L6resultS628
  = (struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE));
  Moonbit_object_header(_M0L6resultS628)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE, $0) >> 2, 1, 0);
  _M0L6resultS628->$0 = _M0L6_2atmpS2369;
  _M0L6resultS628->$1 = 0;
  while (1) {
    struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L7_2abindS629;
    moonbit_incref(_M0L4selfS630);
    #line 677 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    _M0L7_2abindS629
    = _M0MPB4Iter4nextGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(_M0L4selfS630);
    if (_M0L7_2abindS629 == 0) {
      moonbit_decref(_M0L4selfS630);
      if (_M0L7_2abindS629) {
        moonbit_decref(_M0L7_2abindS629);
      }
    } else {
      struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L7_2aSomeS631 =
        _M0L7_2abindS629;
      struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L4_2axS632 =
        _M0L7_2aSomeS631;
      moonbit_incref(_M0L6resultS628);
      #line 678 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
      _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(_M0L6resultS628, _M0L4_2axS632);
      continue;
    }
    break;
  }
  return _M0L6resultS628;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS623) {
  int32_t _M0L3lenS622;
  struct _M0TPC13ref3RefGiE* _M0L5indexS624;
  struct _M0R38String_3a_3aiter_2eanon__u2353__l247__* _closure_4544;
  struct _M0TWEOc* _M0L6_2atmpS2352;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS622 = Moonbit_array_length(_M0L4selfS623);
  _M0L5indexS624
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS624)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS624->$0 = 0;
  _closure_4544
  = (struct _M0R38String_3a_3aiter_2eanon__u2353__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2353__l247__));
  Moonbit_object_header(_closure_4544)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2353__l247__, $0) >> 2, 2, 0);
  _closure_4544->code = &_M0MPC16string6String4iterC2353l247;
  _closure_4544->$0 = _M0L5indexS624;
  _closure_4544->$1 = _M0L4selfS623;
  _closure_4544->$2 = _M0L3lenS622;
  _M0L6_2atmpS2352 = (struct _M0TWEOc*)_closure_4544;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2352);
}

int32_t _M0MPC16string6String4iterC2353l247(
  struct _M0TWEOc* _M0L6_2aenvS2354
) {
  struct _M0R38String_3a_3aiter_2eanon__u2353__l247__* _M0L14_2acasted__envS2355;
  int32_t _M0L3lenS622;
  moonbit_string_t _M0L8_2afieldS4092;
  moonbit_string_t _M0L4selfS623;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4091;
  int32_t _M0L6_2acntS4358;
  struct _M0TPC13ref3RefGiE* _M0L5indexS624;
  int32_t _M0L3valS2356;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2355
  = (struct _M0R38String_3a_3aiter_2eanon__u2353__l247__*)_M0L6_2aenvS2354;
  _M0L3lenS622 = _M0L14_2acasted__envS2355->$2;
  _M0L8_2afieldS4092 = _M0L14_2acasted__envS2355->$1;
  _M0L4selfS623 = _M0L8_2afieldS4092;
  _M0L8_2afieldS4091 = _M0L14_2acasted__envS2355->$0;
  _M0L6_2acntS4358 = Moonbit_object_header(_M0L14_2acasted__envS2355)->rc;
  if (_M0L6_2acntS4358 > 1) {
    int32_t _M0L11_2anew__cntS4359 = _M0L6_2acntS4358 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2355)->rc
    = _M0L11_2anew__cntS4359;
    moonbit_incref(_M0L4selfS623);
    moonbit_incref(_M0L8_2afieldS4091);
  } else if (_M0L6_2acntS4358 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2355);
  }
  _M0L5indexS624 = _M0L8_2afieldS4091;
  _M0L3valS2356 = _M0L5indexS624->$0;
  if (_M0L3valS2356 < _M0L3lenS622) {
    int32_t _M0L3valS2368 = _M0L5indexS624->$0;
    int32_t _M0L2c1S625 = _M0L4selfS623[_M0L3valS2368];
    int32_t _if__result_4545;
    int32_t _M0L3valS2366;
    int32_t _M0L6_2atmpS2365;
    int32_t _M0L6_2atmpS2367;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S625)) {
      int32_t _M0L3valS2358 = _M0L5indexS624->$0;
      int32_t _M0L6_2atmpS2357 = _M0L3valS2358 + 1;
      _if__result_4545 = _M0L6_2atmpS2357 < _M0L3lenS622;
    } else {
      _if__result_4545 = 0;
    }
    if (_if__result_4545) {
      int32_t _M0L3valS2364 = _M0L5indexS624->$0;
      int32_t _M0L6_2atmpS2363 = _M0L3valS2364 + 1;
      int32_t _M0L6_2atmpS4090 = _M0L4selfS623[_M0L6_2atmpS2363];
      int32_t _M0L2c2S626;
      moonbit_decref(_M0L4selfS623);
      _M0L2c2S626 = _M0L6_2atmpS4090;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S626)) {
        int32_t _M0L6_2atmpS2361 = (int32_t)_M0L2c1S625;
        int32_t _M0L6_2atmpS2362 = (int32_t)_M0L2c2S626;
        int32_t _M0L1cS627;
        int32_t _M0L3valS2360;
        int32_t _M0L6_2atmpS2359;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS627
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2361, _M0L6_2atmpS2362);
        _M0L3valS2360 = _M0L5indexS624->$0;
        _M0L6_2atmpS2359 = _M0L3valS2360 + 2;
        _M0L5indexS624->$0 = _M0L6_2atmpS2359;
        moonbit_decref(_M0L5indexS624);
        return _M0L1cS627;
      }
    } else {
      moonbit_decref(_M0L4selfS623);
    }
    _M0L3valS2366 = _M0L5indexS624->$0;
    _M0L6_2atmpS2365 = _M0L3valS2366 + 1;
    _M0L5indexS624->$0 = _M0L6_2atmpS2365;
    moonbit_decref(_M0L5indexS624);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2367 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S625);
    return _M0L6_2atmpS2367;
  } else {
    moonbit_decref(_M0L5indexS624);
    moonbit_decref(_M0L4selfS623);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS601,
  moonbit_string_t _M0L5valueS603
) {
  int32_t _M0L3lenS2317;
  moonbit_string_t* _M0L6_2atmpS2319;
  int32_t _M0L6_2atmpS4095;
  int32_t _M0L6_2atmpS2318;
  int32_t _M0L6lengthS602;
  moonbit_string_t* _M0L8_2afieldS4094;
  moonbit_string_t* _M0L3bufS2320;
  moonbit_string_t _M0L6_2aoldS4093;
  int32_t _M0L6_2atmpS2321;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2317 = _M0L4selfS601->$1;
  moonbit_incref(_M0L4selfS601);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2319 = _M0MPC15array5Array6bufferGsE(_M0L4selfS601);
  _M0L6_2atmpS4095 = Moonbit_array_length(_M0L6_2atmpS2319);
  moonbit_decref(_M0L6_2atmpS2319);
  _M0L6_2atmpS2318 = _M0L6_2atmpS4095;
  if (_M0L3lenS2317 == _M0L6_2atmpS2318) {
    moonbit_incref(_M0L4selfS601);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS601);
  }
  _M0L6lengthS602 = _M0L4selfS601->$1;
  _M0L8_2afieldS4094 = _M0L4selfS601->$0;
  _M0L3bufS2320 = _M0L8_2afieldS4094;
  _M0L6_2aoldS4093 = (moonbit_string_t)_M0L3bufS2320[_M0L6lengthS602];
  moonbit_decref(_M0L6_2aoldS4093);
  _M0L3bufS2320[_M0L6lengthS602] = _M0L5valueS603;
  _M0L6_2atmpS2321 = _M0L6lengthS602 + 1;
  _M0L4selfS601->$1 = _M0L6_2atmpS2321;
  moonbit_decref(_M0L4selfS601);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS604,
  struct _M0TUsiE* _M0L5valueS606
) {
  int32_t _M0L3lenS2322;
  struct _M0TUsiE** _M0L6_2atmpS2324;
  int32_t _M0L6_2atmpS4098;
  int32_t _M0L6_2atmpS2323;
  int32_t _M0L6lengthS605;
  struct _M0TUsiE** _M0L8_2afieldS4097;
  struct _M0TUsiE** _M0L3bufS2325;
  struct _M0TUsiE* _M0L6_2aoldS4096;
  int32_t _M0L6_2atmpS2326;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2322 = _M0L4selfS604->$1;
  moonbit_incref(_M0L4selfS604);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2324 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS604);
  _M0L6_2atmpS4098 = Moonbit_array_length(_M0L6_2atmpS2324);
  moonbit_decref(_M0L6_2atmpS2324);
  _M0L6_2atmpS2323 = _M0L6_2atmpS4098;
  if (_M0L3lenS2322 == _M0L6_2atmpS2323) {
    moonbit_incref(_M0L4selfS604);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS604);
  }
  _M0L6lengthS605 = _M0L4selfS604->$1;
  _M0L8_2afieldS4097 = _M0L4selfS604->$0;
  _M0L3bufS2325 = _M0L8_2afieldS4097;
  _M0L6_2aoldS4096 = (struct _M0TUsiE*)_M0L3bufS2325[_M0L6lengthS605];
  if (_M0L6_2aoldS4096) {
    moonbit_decref(_M0L6_2aoldS4096);
  }
  _M0L3bufS2325[_M0L6lengthS605] = _M0L5valueS606;
  _M0L6_2atmpS2326 = _M0L6lengthS605 + 1;
  _M0L4selfS604->$1 = _M0L6_2atmpS2326;
  moonbit_decref(_M0L4selfS604);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS607,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L5valueS609
) {
  int32_t _M0L3lenS2327;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L6_2atmpS2329;
  int32_t _M0L6_2atmpS4101;
  int32_t _M0L6_2atmpS2328;
  int32_t _M0L6lengthS608;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L8_2afieldS4100;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L3bufS2330;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2aoldS4099;
  int32_t _M0L6_2atmpS2331;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2327 = _M0L4selfS607->$1;
  moonbit_incref(_M0L4selfS607);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2329
  = _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L4selfS607);
  _M0L6_2atmpS4101 = Moonbit_array_length(_M0L6_2atmpS2329);
  moonbit_decref(_M0L6_2atmpS2329);
  _M0L6_2atmpS2328 = _M0L6_2atmpS4101;
  if (_M0L3lenS2327 == _M0L6_2atmpS2328) {
    moonbit_incref(_M0L4selfS607);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L4selfS607);
  }
  _M0L6lengthS608 = _M0L4selfS607->$1;
  _M0L8_2afieldS4100 = _M0L4selfS607->$0;
  _M0L3bufS2330 = _M0L8_2afieldS4100;
  _M0L6_2aoldS4099
  = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)_M0L3bufS2330[
      _M0L6lengthS608
    ];
  if (_M0L6_2aoldS4099) {
    moonbit_decref(_M0L6_2aoldS4099);
  }
  _M0L3bufS2330[_M0L6lengthS608] = _M0L5valueS609;
  _M0L6_2atmpS2331 = _M0L6lengthS608 + 1;
  _M0L4selfS607->$1 = _M0L6_2atmpS2331;
  moonbit_decref(_M0L4selfS607);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS610,
  void* _M0L5valueS612
) {
  int32_t _M0L3lenS2332;
  void** _M0L6_2atmpS2334;
  int32_t _M0L6_2atmpS4104;
  int32_t _M0L6_2atmpS2333;
  int32_t _M0L6lengthS611;
  void** _M0L8_2afieldS4103;
  void** _M0L3bufS2335;
  void* _M0L6_2aoldS4102;
  int32_t _M0L6_2atmpS2336;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2332 = _M0L4selfS610->$1;
  moonbit_incref(_M0L4selfS610);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2334
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS610);
  _M0L6_2atmpS4104 = Moonbit_array_length(_M0L6_2atmpS2334);
  moonbit_decref(_M0L6_2atmpS2334);
  _M0L6_2atmpS2333 = _M0L6_2atmpS4104;
  if (_M0L3lenS2332 == _M0L6_2atmpS2333) {
    moonbit_incref(_M0L4selfS610);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS610);
  }
  _M0L6lengthS611 = _M0L4selfS610->$1;
  _M0L8_2afieldS4103 = _M0L4selfS610->$0;
  _M0L3bufS2335 = _M0L8_2afieldS4103;
  _M0L6_2aoldS4102 = (void*)_M0L3bufS2335[_M0L6lengthS611];
  moonbit_decref(_M0L6_2aoldS4102);
  _M0L3bufS2335[_M0L6lengthS611] = _M0L5valueS612;
  _M0L6_2atmpS2336 = _M0L6lengthS611 + 1;
  _M0L4selfS610->$1 = _M0L6_2atmpS2336;
  moonbit_decref(_M0L4selfS610);
  return 0;
}

int32_t _M0MPC15array5Array4pushGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L4selfS613,
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L5valueS615
) {
  int32_t _M0L3lenS2337;
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L6_2atmpS2339;
  int32_t _M0L6_2atmpS4107;
  int32_t _M0L6_2atmpS2338;
  int32_t _M0L6lengthS614;
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L8_2afieldS4106;
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L3bufS2340;
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2aoldS4105;
  int32_t _M0L6_2atmpS2341;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2337 = _M0L4selfS613->$1;
  moonbit_incref(_M0L4selfS613);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2339
  = _M0MPC15array5Array6bufferGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(_M0L4selfS613);
  _M0L6_2atmpS4107 = Moonbit_array_length(_M0L6_2atmpS2339);
  moonbit_decref(_M0L6_2atmpS2339);
  _M0L6_2atmpS2338 = _M0L6_2atmpS4107;
  if (_M0L3lenS2337 == _M0L6_2atmpS2338) {
    moonbit_incref(_M0L4selfS613);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(_M0L4selfS613);
  }
  _M0L6lengthS614 = _M0L4selfS613->$1;
  _M0L8_2afieldS4106 = _M0L4selfS613->$0;
  _M0L3bufS2340 = _M0L8_2afieldS4106;
  _M0L6_2aoldS4105
  = (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*)_M0L3bufS2340[
      _M0L6lengthS614
    ];
  if (_M0L6_2aoldS4105) {
    moonbit_decref(_M0L6_2aoldS4105);
  }
  _M0L3bufS2340[_M0L6lengthS614] = _M0L5valueS615;
  _M0L6_2atmpS2341 = _M0L6lengthS614 + 1;
  _M0L4selfS613->$1 = _M0L6_2atmpS2341;
  moonbit_decref(_M0L4selfS613);
  return 0;
}

int32_t _M0MPC15array5Array4pushGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS616,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L5valueS618
) {
  int32_t _M0L3lenS2342;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L6_2atmpS2344;
  int32_t _M0L6_2atmpS4110;
  int32_t _M0L6_2atmpS2343;
  int32_t _M0L6lengthS617;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L8_2afieldS4109;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L3bufS2345;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2aoldS4108;
  int32_t _M0L6_2atmpS2346;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2342 = _M0L4selfS616->$1;
  moonbit_incref(_M0L4selfS616);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2344
  = _M0MPC15array5Array6bufferGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L4selfS616);
  _M0L6_2atmpS4110 = Moonbit_array_length(_M0L6_2atmpS2344);
  moonbit_decref(_M0L6_2atmpS2344);
  _M0L6_2atmpS2343 = _M0L6_2atmpS4110;
  if (_M0L3lenS2342 == _M0L6_2atmpS2343) {
    moonbit_incref(_M0L4selfS616);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L4selfS616);
  }
  _M0L6lengthS617 = _M0L4selfS616->$1;
  _M0L8_2afieldS4109 = _M0L4selfS616->$0;
  _M0L3bufS2345 = _M0L8_2afieldS4109;
  _M0L6_2aoldS4108
  = (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*)_M0L3bufS2345[
      _M0L6lengthS617
    ];
  if (_M0L6_2aoldS4108) {
    moonbit_decref(_M0L6_2aoldS4108);
  }
  _M0L3bufS2345[_M0L6lengthS617] = _M0L5valueS618;
  _M0L6_2atmpS2346 = _M0L6lengthS617 + 1;
  _M0L4selfS616->$1 = _M0L6_2atmpS2346;
  moonbit_decref(_M0L4selfS616);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0L4selfS619,
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L5valueS621
) {
  int32_t _M0L3lenS2347;
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L6_2atmpS2349;
  int32_t _M0L6_2atmpS4113;
  int32_t _M0L6_2atmpS2348;
  int32_t _M0L6lengthS620;
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L8_2afieldS4112;
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L3bufS2350;
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2aoldS4111;
  int32_t _M0L6_2atmpS2351;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS2347 = _M0L4selfS619->$1;
  moonbit_incref(_M0L4selfS619);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS2349
  = _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(_M0L4selfS619);
  _M0L6_2atmpS4113 = Moonbit_array_length(_M0L6_2atmpS2349);
  moonbit_decref(_M0L6_2atmpS2349);
  _M0L6_2atmpS2348 = _M0L6_2atmpS4113;
  if (_M0L3lenS2347 == _M0L6_2atmpS2348) {
    moonbit_incref(_M0L4selfS619);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(_M0L4selfS619);
  }
  _M0L6lengthS620 = _M0L4selfS619->$1;
  _M0L8_2afieldS4112 = _M0L4selfS619->$0;
  _M0L3bufS2350 = _M0L8_2afieldS4112;
  _M0L6_2aoldS4111
  = (struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*)_M0L3bufS2350[
      _M0L6lengthS620
    ];
  if (_M0L6_2aoldS4111) {
    moonbit_decref(_M0L6_2aoldS4111);
  }
  _M0L3bufS2350[_M0L6lengthS620] = _M0L5valueS621;
  _M0L6_2atmpS2351 = _M0L6lengthS620 + 1;
  _M0L4selfS619->$1 = _M0L6_2atmpS2351;
  moonbit_decref(_M0L4selfS619);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS581) {
  int32_t _M0L8old__capS580;
  int32_t _M0L8new__capS582;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS580 = _M0L4selfS581->$1;
  if (_M0L8old__capS580 == 0) {
    _M0L8new__capS582 = 8;
  } else {
    _M0L8new__capS582 = _M0L8old__capS580 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS581, _M0L8new__capS582);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS584
) {
  int32_t _M0L8old__capS583;
  int32_t _M0L8new__capS585;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS583 = _M0L4selfS584->$1;
  if (_M0L8old__capS583 == 0) {
    _M0L8new__capS585 = 8;
  } else {
    _M0L8new__capS585 = _M0L8old__capS583 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS584, _M0L8new__capS585);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS587
) {
  int32_t _M0L8old__capS586;
  int32_t _M0L8new__capS588;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS586 = _M0L4selfS587->$1;
  if (_M0L8old__capS586 == 0) {
    _M0L8new__capS588 = 8;
  } else {
    _M0L8new__capS588 = _M0L8old__capS586 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L4selfS587, _M0L8new__capS588);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS590
) {
  int32_t _M0L8old__capS589;
  int32_t _M0L8new__capS591;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS589 = _M0L4selfS590->$1;
  if (_M0L8old__capS589 == 0) {
    _M0L8new__capS591 = 8;
  } else {
    _M0L8new__capS591 = _M0L8old__capS589 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS590, _M0L8new__capS591);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L4selfS593
) {
  int32_t _M0L8old__capS592;
  int32_t _M0L8new__capS594;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS592 = _M0L4selfS593->$1;
  if (_M0L8old__capS592 == 0) {
    _M0L8new__capS594 = 8;
  } else {
    _M0L8new__capS594 = _M0L8old__capS592 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(_M0L4selfS593, _M0L8new__capS594);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS596
) {
  int32_t _M0L8old__capS595;
  int32_t _M0L8new__capS597;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS595 = _M0L4selfS596->$1;
  if (_M0L8old__capS595 == 0) {
    _M0L8new__capS597 = 8;
  } else {
    _M0L8new__capS597 = _M0L8old__capS595 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L4selfS596, _M0L8new__capS597);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0L4selfS599
) {
  int32_t _M0L8old__capS598;
  int32_t _M0L8new__capS600;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS598 = _M0L4selfS599->$1;
  if (_M0L8old__capS598 == 0) {
    _M0L8new__capS600 = 8;
  } else {
    _M0L8new__capS600 = _M0L8old__capS598 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(_M0L4selfS599, _M0L8new__capS600);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS541,
  int32_t _M0L13new__capacityS539
) {
  moonbit_string_t* _M0L8new__bufS538;
  moonbit_string_t* _M0L8_2afieldS4115;
  moonbit_string_t* _M0L8old__bufS540;
  int32_t _M0L8old__capS542;
  int32_t _M0L9copy__lenS543;
  moonbit_string_t* _M0L6_2aoldS4114;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS538
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS539, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS4115 = _M0L4selfS541->$0;
  _M0L8old__bufS540 = _M0L8_2afieldS4115;
  _M0L8old__capS542 = Moonbit_array_length(_M0L8old__bufS540);
  if (_M0L8old__capS542 < _M0L13new__capacityS539) {
    _M0L9copy__lenS543 = _M0L8old__capS542;
  } else {
    _M0L9copy__lenS543 = _M0L13new__capacityS539;
  }
  moonbit_incref(_M0L8old__bufS540);
  moonbit_incref(_M0L8new__bufS538);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS538, 0, _M0L8old__bufS540, 0, _M0L9copy__lenS543);
  _M0L6_2aoldS4114 = _M0L4selfS541->$0;
  moonbit_decref(_M0L6_2aoldS4114);
  _M0L4selfS541->$0 = _M0L8new__bufS538;
  moonbit_decref(_M0L4selfS541);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS547,
  int32_t _M0L13new__capacityS545
) {
  struct _M0TUsiE** _M0L8new__bufS544;
  struct _M0TUsiE** _M0L8_2afieldS4117;
  struct _M0TUsiE** _M0L8old__bufS546;
  int32_t _M0L8old__capS548;
  int32_t _M0L9copy__lenS549;
  struct _M0TUsiE** _M0L6_2aoldS4116;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS544
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS545, 0);
  _M0L8_2afieldS4117 = _M0L4selfS547->$0;
  _M0L8old__bufS546 = _M0L8_2afieldS4117;
  _M0L8old__capS548 = Moonbit_array_length(_M0L8old__bufS546);
  if (_M0L8old__capS548 < _M0L13new__capacityS545) {
    _M0L9copy__lenS549 = _M0L8old__capS548;
  } else {
    _M0L9copy__lenS549 = _M0L13new__capacityS545;
  }
  moonbit_incref(_M0L8old__bufS546);
  moonbit_incref(_M0L8new__bufS544);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS544, 0, _M0L8old__bufS546, 0, _M0L9copy__lenS549);
  _M0L6_2aoldS4116 = _M0L4selfS547->$0;
  moonbit_decref(_M0L6_2aoldS4116);
  _M0L4selfS547->$0 = _M0L8new__bufS544;
  moonbit_decref(_M0L4selfS547);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS553,
  int32_t _M0L13new__capacityS551
) {
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L8new__bufS550;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L8_2afieldS4119;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L8old__bufS552;
  int32_t _M0L8old__capS554;
  int32_t _M0L9copy__lenS555;
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L6_2aoldS4118;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS550
  = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall**)moonbit_make_ref_array(_M0L13new__capacityS551, 0);
  _M0L8_2afieldS4119 = _M0L4selfS553->$0;
  _M0L8old__bufS552 = _M0L8_2afieldS4119;
  _M0L8old__capS554 = Moonbit_array_length(_M0L8old__bufS552);
  if (_M0L8old__capS554 < _M0L13new__capacityS551) {
    _M0L9copy__lenS555 = _M0L8old__capS554;
  } else {
    _M0L9copy__lenS555 = _M0L13new__capacityS551;
  }
  moonbit_incref(_M0L8old__bufS552);
  moonbit_incref(_M0L8new__bufS550);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(_M0L8new__bufS550, 0, _M0L8old__bufS552, 0, _M0L9copy__lenS555);
  _M0L6_2aoldS4118 = _M0L4selfS553->$0;
  moonbit_decref(_M0L6_2aoldS4118);
  _M0L4selfS553->$0 = _M0L8new__bufS550;
  moonbit_decref(_M0L4selfS553);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS559,
  int32_t _M0L13new__capacityS557
) {
  void** _M0L8new__bufS556;
  void** _M0L8_2afieldS4121;
  void** _M0L8old__bufS558;
  int32_t _M0L8old__capS560;
  int32_t _M0L9copy__lenS561;
  void** _M0L6_2aoldS4120;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS556
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS557, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS4121 = _M0L4selfS559->$0;
  _M0L8old__bufS558 = _M0L8_2afieldS4121;
  _M0L8old__capS560 = Moonbit_array_length(_M0L8old__bufS558);
  if (_M0L8old__capS560 < _M0L13new__capacityS557) {
    _M0L9copy__lenS561 = _M0L8old__capS560;
  } else {
    _M0L9copy__lenS561 = _M0L13new__capacityS557;
  }
  moonbit_incref(_M0L8old__bufS558);
  moonbit_incref(_M0L8new__bufS556);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS556, 0, _M0L8old__bufS558, 0, _M0L9copy__lenS561);
  _M0L6_2aoldS4120 = _M0L4selfS559->$0;
  moonbit_decref(_M0L6_2aoldS4120);
  _M0L4selfS559->$0 = _M0L8new__bufS556;
  moonbit_decref(_M0L4selfS559);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L4selfS565,
  int32_t _M0L13new__capacityS563
) {
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L8new__bufS562;
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L8_2afieldS4123;
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L8old__bufS564;
  int32_t _M0L8old__capS566;
  int32_t _M0L9copy__lenS567;
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L6_2aoldS4122;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS562
  = (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder**)moonbit_make_ref_array(_M0L13new__capacityS563, 0);
  _M0L8_2afieldS4123 = _M0L4selfS565->$0;
  _M0L8old__bufS564 = _M0L8_2afieldS4123;
  _M0L8old__capS566 = Moonbit_array_length(_M0L8old__bufS564);
  if (_M0L8old__capS566 < _M0L13new__capacityS563) {
    _M0L9copy__lenS567 = _M0L8old__capS566;
  } else {
    _M0L9copy__lenS567 = _M0L13new__capacityS563;
  }
  moonbit_incref(_M0L8old__bufS564);
  moonbit_incref(_M0L8new__bufS562);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(_M0L8new__bufS562, 0, _M0L8old__bufS564, 0, _M0L9copy__lenS567);
  _M0L6_2aoldS4122 = _M0L4selfS565->$0;
  moonbit_decref(_M0L6_2aoldS4122);
  _M0L4selfS565->$0 = _M0L8new__bufS562;
  moonbit_decref(_M0L4selfS565);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS571,
  int32_t _M0L13new__capacityS569
) {
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L8new__bufS568;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L8_2afieldS4125;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L8old__bufS570;
  int32_t _M0L8old__capS572;
  int32_t _M0L9copy__lenS573;
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L6_2aoldS4124;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS568
  = (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder**)moonbit_make_ref_array(_M0L13new__capacityS569, 0);
  _M0L8_2afieldS4125 = _M0L4selfS571->$0;
  _M0L8old__bufS570 = _M0L8_2afieldS4125;
  _M0L8old__capS572 = Moonbit_array_length(_M0L8old__bufS570);
  if (_M0L8old__capS572 < _M0L13new__capacityS569) {
    _M0L9copy__lenS573 = _M0L8old__capS572;
  } else {
    _M0L9copy__lenS573 = _M0L13new__capacityS569;
  }
  moonbit_incref(_M0L8old__bufS570);
  moonbit_incref(_M0L8new__bufS568);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(_M0L8new__bufS568, 0, _M0L8old__bufS570, 0, _M0L9copy__lenS573);
  _M0L6_2aoldS4124 = _M0L4selfS571->$0;
  moonbit_decref(_M0L6_2aoldS4124);
  _M0L4selfS571->$0 = _M0L8new__bufS568;
  moonbit_decref(_M0L4selfS571);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0L4selfS577,
  int32_t _M0L13new__capacityS575
) {
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L8new__bufS574;
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L8_2afieldS4127;
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L8old__bufS576;
  int32_t _M0L8old__capS578;
  int32_t _M0L9copy__lenS579;
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L6_2aoldS4126;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS574
  = (struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice**)moonbit_make_ref_array(_M0L13new__capacityS575, 0);
  _M0L8_2afieldS4127 = _M0L4selfS577->$0;
  _M0L8old__bufS576 = _M0L8_2afieldS4127;
  _M0L8old__capS578 = Moonbit_array_length(_M0L8old__bufS576);
  if (_M0L8old__capS578 < _M0L13new__capacityS575) {
    _M0L9copy__lenS579 = _M0L8old__capS578;
  } else {
    _M0L9copy__lenS579 = _M0L13new__capacityS575;
  }
  moonbit_incref(_M0L8old__bufS576);
  moonbit_incref(_M0L8new__bufS574);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(_M0L8new__bufS574, 0, _M0L8old__bufS576, 0, _M0L9copy__lenS579);
  _M0L6_2aoldS4126 = _M0L4selfS577->$0;
  moonbit_decref(_M0L6_2aoldS4126);
  _M0L4selfS577->$0 = _M0L8new__bufS574;
  moonbit_decref(_M0L4selfS577);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS537
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS537 == 0) {
    moonbit_string_t* _M0L6_2atmpS2315 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4546 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4546)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4546->$0 = _M0L6_2atmpS2315;
    _block_4546->$1 = 0;
    return _block_4546;
  } else {
    moonbit_string_t* _M0L6_2atmpS2316 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS537, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_4547 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4547)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4547->$0 = _M0L6_2atmpS2316;
    _block_4547->$1 = 0;
    return _block_4547;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS531,
  int32_t _M0L1nS530
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS530 <= 0) {
    moonbit_decref(_M0L4selfS531);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS530 == 1) {
    return _M0L4selfS531;
  } else {
    int32_t _M0L3lenS532 = Moonbit_array_length(_M0L4selfS531);
    int32_t _M0L6_2atmpS2314 = _M0L3lenS532 * _M0L1nS530;
    struct _M0TPB13StringBuilder* _M0L3bufS533;
    moonbit_string_t _M0L3strS534;
    int32_t _M0L2__S535;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS533 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2314);
    _M0L3strS534 = _M0L4selfS531;
    _M0L2__S535 = 0;
    while (1) {
      if (_M0L2__S535 < _M0L1nS530) {
        int32_t _M0L6_2atmpS2313;
        moonbit_incref(_M0L3strS534);
        moonbit_incref(_M0L3bufS533);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS533, _M0L3strS534);
        _M0L6_2atmpS2313 = _M0L2__S535 + 1;
        _M0L2__S535 = _M0L6_2atmpS2313;
        continue;
      } else {
        moonbit_decref(_M0L3strS534);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS533);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS528,
  struct _M0TPC16string10StringView _M0L3strS529
) {
  int32_t _M0L3lenS2301;
  int32_t _M0L6_2atmpS2303;
  int32_t _M0L6_2atmpS2302;
  int32_t _M0L6_2atmpS2300;
  moonbit_bytes_t _M0L8_2afieldS4128;
  moonbit_bytes_t _M0L4dataS2304;
  int32_t _M0L3lenS2305;
  moonbit_string_t _M0L6_2atmpS2306;
  int32_t _M0L6_2atmpS2307;
  int32_t _M0L6_2atmpS2308;
  int32_t _M0L3lenS2310;
  int32_t _M0L6_2atmpS2312;
  int32_t _M0L6_2atmpS2311;
  int32_t _M0L6_2atmpS2309;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS2301 = _M0L4selfS528->$1;
  moonbit_incref(_M0L3strS529.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2303 = _M0MPC16string10StringView6length(_M0L3strS529);
  _M0L6_2atmpS2302 = _M0L6_2atmpS2303 * 2;
  _M0L6_2atmpS2300 = _M0L3lenS2301 + _M0L6_2atmpS2302;
  moonbit_incref(_M0L4selfS528);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS528, _M0L6_2atmpS2300);
  _M0L8_2afieldS4128 = _M0L4selfS528->$0;
  _M0L4dataS2304 = _M0L8_2afieldS4128;
  _M0L3lenS2305 = _M0L4selfS528->$1;
  moonbit_incref(_M0L4dataS2304);
  moonbit_incref(_M0L3strS529.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2306 = _M0MPC16string10StringView4data(_M0L3strS529);
  moonbit_incref(_M0L3strS529.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2307 = _M0MPC16string10StringView13start__offset(_M0L3strS529);
  moonbit_incref(_M0L3strS529.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2308 = _M0MPC16string10StringView6length(_M0L3strS529);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS2304, _M0L3lenS2305, _M0L6_2atmpS2306, _M0L6_2atmpS2307, _M0L6_2atmpS2308);
  _M0L3lenS2310 = _M0L4selfS528->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS2312 = _M0MPC16string10StringView6length(_M0L3strS529);
  _M0L6_2atmpS2311 = _M0L6_2atmpS2312 * 2;
  _M0L6_2atmpS2309 = _M0L3lenS2310 + _M0L6_2atmpS2311;
  _M0L4selfS528->$1 = _M0L6_2atmpS2309;
  moonbit_decref(_M0L4selfS528);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS520,
  int32_t _M0L3lenS523,
  int32_t _M0L13start__offsetS527,
  int64_t _M0L11end__offsetS518
) {
  int32_t _M0L11end__offsetS517;
  int32_t _M0L5indexS521;
  int32_t _M0L5countS522;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS518 == 4294967296ll) {
    _M0L11end__offsetS517 = Moonbit_array_length(_M0L4selfS520);
  } else {
    int64_t _M0L7_2aSomeS519 = _M0L11end__offsetS518;
    _M0L11end__offsetS517 = (int32_t)_M0L7_2aSomeS519;
  }
  _M0L5indexS521 = _M0L13start__offsetS527;
  _M0L5countS522 = 0;
  while (1) {
    int32_t _if__result_4550;
    if (_M0L5indexS521 < _M0L11end__offsetS517) {
      _if__result_4550 = _M0L5countS522 < _M0L3lenS523;
    } else {
      _if__result_4550 = 0;
    }
    if (_if__result_4550) {
      int32_t _M0L2c1S524 = _M0L4selfS520[_M0L5indexS521];
      int32_t _if__result_4551;
      int32_t _M0L6_2atmpS2298;
      int32_t _M0L6_2atmpS2299;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S524)) {
        int32_t _M0L6_2atmpS2294 = _M0L5indexS521 + 1;
        _if__result_4551 = _M0L6_2atmpS2294 < _M0L11end__offsetS517;
      } else {
        _if__result_4551 = 0;
      }
      if (_if__result_4551) {
        int32_t _M0L6_2atmpS2297 = _M0L5indexS521 + 1;
        int32_t _M0L2c2S525 = _M0L4selfS520[_M0L6_2atmpS2297];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S525)) {
          int32_t _M0L6_2atmpS2295 = _M0L5indexS521 + 2;
          int32_t _M0L6_2atmpS2296 = _M0L5countS522 + 1;
          _M0L5indexS521 = _M0L6_2atmpS2295;
          _M0L5countS522 = _M0L6_2atmpS2296;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_113.data, (moonbit_string_t)moonbit_string_literal_114.data);
        }
      }
      _M0L6_2atmpS2298 = _M0L5indexS521 + 1;
      _M0L6_2atmpS2299 = _M0L5countS522 + 1;
      _M0L5indexS521 = _M0L6_2atmpS2298;
      _M0L5countS522 = _M0L6_2atmpS2299;
      continue;
    } else {
      moonbit_decref(_M0L4selfS520);
      return _M0L5countS522 >= _M0L3lenS523;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS512
) {
  int32_t _M0L3endS2284;
  int32_t _M0L8_2afieldS4129;
  int32_t _M0L5startS2285;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2284 = _M0L4selfS512.$2;
  _M0L8_2afieldS4129 = _M0L4selfS512.$1;
  moonbit_decref(_M0L4selfS512.$0);
  _M0L5startS2285 = _M0L8_2afieldS4129;
  return _M0L3endS2284 - _M0L5startS2285;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS513
) {
  int32_t _M0L3endS2286;
  int32_t _M0L8_2afieldS4130;
  int32_t _M0L5startS2287;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2286 = _M0L4selfS513.$2;
  _M0L8_2afieldS4130 = _M0L4selfS513.$1;
  moonbit_decref(_M0L4selfS513.$0);
  _M0L5startS2287 = _M0L8_2afieldS4130;
  return _M0L3endS2286 - _M0L5startS2287;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS514
) {
  int32_t _M0L3endS2288;
  int32_t _M0L8_2afieldS4131;
  int32_t _M0L5startS2289;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2288 = _M0L4selfS514.$2;
  _M0L8_2afieldS4131 = _M0L4selfS514.$1;
  moonbit_decref(_M0L4selfS514.$0);
  _M0L5startS2289 = _M0L8_2afieldS4131;
  return _M0L3endS2288 - _M0L5startS2289;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB4JsonEE(
  struct _M0TPB9ArrayViewGUsRPB4JsonEE _M0L4selfS515
) {
  int32_t _M0L3endS2290;
  int32_t _M0L8_2afieldS4132;
  int32_t _M0L5startS2291;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2290 = _M0L4selfS515.$2;
  _M0L8_2afieldS4132 = _M0L4selfS515.$1;
  moonbit_decref(_M0L4selfS515.$0);
  _M0L5startS2291 = _M0L8_2afieldS4132;
  return _M0L3endS2290 - _M0L5startS2291;
}

int32_t _M0MPC15array9ArrayView6lengthGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB9ArrayViewGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE _M0L4selfS516
) {
  int32_t _M0L3endS2292;
  int32_t _M0L8_2afieldS4133;
  int32_t _M0L5startS2293;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS2292 = _M0L4selfS516.$2;
  _M0L8_2afieldS4133 = _M0L4selfS516.$1;
  moonbit_decref(_M0L4selfS516.$0);
  _M0L5startS2293 = _M0L8_2afieldS4133;
  return _M0L3endS2292 - _M0L5startS2293;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS510,
  int64_t _M0L19start__offset_2eoptS508,
  int64_t _M0L11end__offsetS511
) {
  int32_t _M0L13start__offsetS507;
  if (_M0L19start__offset_2eoptS508 == 4294967296ll) {
    _M0L13start__offsetS507 = 0;
  } else {
    int64_t _M0L7_2aSomeS509 = _M0L19start__offset_2eoptS508;
    _M0L13start__offsetS507 = (int32_t)_M0L7_2aSomeS509;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS510, _M0L13start__offsetS507, _M0L11end__offsetS511);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS505,
  int32_t _M0L13start__offsetS506,
  int64_t _M0L11end__offsetS503
) {
  int32_t _M0L11end__offsetS502;
  int32_t _if__result_4552;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS503 == 4294967296ll) {
    _M0L11end__offsetS502 = Moonbit_array_length(_M0L4selfS505);
  } else {
    int64_t _M0L7_2aSomeS504 = _M0L11end__offsetS503;
    _M0L11end__offsetS502 = (int32_t)_M0L7_2aSomeS504;
  }
  if (_M0L13start__offsetS506 >= 0) {
    if (_M0L13start__offsetS506 <= _M0L11end__offsetS502) {
      int32_t _M0L6_2atmpS2283 = Moonbit_array_length(_M0L4selfS505);
      _if__result_4552 = _M0L11end__offsetS502 <= _M0L6_2atmpS2283;
    } else {
      _if__result_4552 = 0;
    }
  } else {
    _if__result_4552 = 0;
  }
  if (_if__result_4552) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS506,
                                                 _M0L11end__offsetS502,
                                                 _M0L4selfS505};
  } else {
    moonbit_decref(_M0L4selfS505);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_115.data, (moonbit_string_t)moonbit_string_literal_116.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS501
) {
  moonbit_string_t _M0L8_2afieldS4135;
  moonbit_string_t _M0L3strS2280;
  int32_t _M0L5startS2281;
  int32_t _M0L8_2afieldS4134;
  int32_t _M0L3endS2282;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4135 = _M0L4selfS501.$0;
  _M0L3strS2280 = _M0L8_2afieldS4135;
  _M0L5startS2281 = _M0L4selfS501.$1;
  _M0L8_2afieldS4134 = _M0L4selfS501.$2;
  _M0L3endS2282 = _M0L8_2afieldS4134;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS2280, _M0L5startS2281, _M0L3endS2282);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS499,
  struct _M0TPB6Logger _M0L6loggerS500
) {
  moonbit_string_t _M0L8_2afieldS4137;
  moonbit_string_t _M0L3strS2277;
  int32_t _M0L5startS2278;
  int32_t _M0L8_2afieldS4136;
  int32_t _M0L3endS2279;
  moonbit_string_t _M0L6substrS498;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4137 = _M0L4selfS499.$0;
  _M0L3strS2277 = _M0L8_2afieldS4137;
  _M0L5startS2278 = _M0L4selfS499.$1;
  _M0L8_2afieldS4136 = _M0L4selfS499.$2;
  _M0L3endS2279 = _M0L8_2afieldS4136;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS498
  = _M0MPC16string6String17unsafe__substring(_M0L3strS2277, _M0L5startS2278, _M0L3endS2279);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS498, _M0L6loggerS500);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS490,
  struct _M0TPB6Logger _M0L6loggerS488
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS489;
  int32_t _M0L3lenS491;
  int32_t _M0L1iS492;
  int32_t _M0L3segS493;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS488.$1) {
    moonbit_incref(_M0L6loggerS488.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS488.$0->$method_3(_M0L6loggerS488.$1, 34);
  moonbit_incref(_M0L4selfS490);
  if (_M0L6loggerS488.$1) {
    moonbit_incref(_M0L6loggerS488.$1);
  }
  _M0L6_2aenvS489
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS489)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS489->$0 = _M0L4selfS490;
  _M0L6_2aenvS489->$1_0 = _M0L6loggerS488.$0;
  _M0L6_2aenvS489->$1_1 = _M0L6loggerS488.$1;
  _M0L3lenS491 = Moonbit_array_length(_M0L4selfS490);
  _M0L1iS492 = 0;
  _M0L3segS493 = 0;
  _2afor_494:;
  while (1) {
    int32_t _M0L4codeS495;
    int32_t _M0L1cS497;
    int32_t _M0L6_2atmpS2261;
    int32_t _M0L6_2atmpS2262;
    int32_t _M0L6_2atmpS2263;
    int32_t _tmp_4556;
    int32_t _tmp_4557;
    if (_M0L1iS492 >= _M0L3lenS491) {
      moonbit_decref(_M0L4selfS490);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS489, _M0L3segS493, _M0L1iS492);
      break;
    }
    _M0L4codeS495 = _M0L4selfS490[_M0L1iS492];
    switch (_M0L4codeS495) {
      case 34: {
        _M0L1cS497 = _M0L4codeS495;
        goto join_496;
        break;
      }
      
      case 92: {
        _M0L1cS497 = _M0L4codeS495;
        goto join_496;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS2264;
        int32_t _M0L6_2atmpS2265;
        moonbit_incref(_M0L6_2aenvS489);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS489, _M0L3segS493, _M0L1iS492);
        if (_M0L6loggerS488.$1) {
          moonbit_incref(_M0L6loggerS488.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS488.$0->$method_0(_M0L6loggerS488.$1, (moonbit_string_t)moonbit_string_literal_97.data);
        _M0L6_2atmpS2264 = _M0L1iS492 + 1;
        _M0L6_2atmpS2265 = _M0L1iS492 + 1;
        _M0L1iS492 = _M0L6_2atmpS2264;
        _M0L3segS493 = _M0L6_2atmpS2265;
        goto _2afor_494;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS2266;
        int32_t _M0L6_2atmpS2267;
        moonbit_incref(_M0L6_2aenvS489);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS489, _M0L3segS493, _M0L1iS492);
        if (_M0L6loggerS488.$1) {
          moonbit_incref(_M0L6loggerS488.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS488.$0->$method_0(_M0L6loggerS488.$1, (moonbit_string_t)moonbit_string_literal_98.data);
        _M0L6_2atmpS2266 = _M0L1iS492 + 1;
        _M0L6_2atmpS2267 = _M0L1iS492 + 1;
        _M0L1iS492 = _M0L6_2atmpS2266;
        _M0L3segS493 = _M0L6_2atmpS2267;
        goto _2afor_494;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS2268;
        int32_t _M0L6_2atmpS2269;
        moonbit_incref(_M0L6_2aenvS489);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS489, _M0L3segS493, _M0L1iS492);
        if (_M0L6loggerS488.$1) {
          moonbit_incref(_M0L6loggerS488.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS488.$0->$method_0(_M0L6loggerS488.$1, (moonbit_string_t)moonbit_string_literal_99.data);
        _M0L6_2atmpS2268 = _M0L1iS492 + 1;
        _M0L6_2atmpS2269 = _M0L1iS492 + 1;
        _M0L1iS492 = _M0L6_2atmpS2268;
        _M0L3segS493 = _M0L6_2atmpS2269;
        goto _2afor_494;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS2270;
        int32_t _M0L6_2atmpS2271;
        moonbit_incref(_M0L6_2aenvS489);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS489, _M0L3segS493, _M0L1iS492);
        if (_M0L6loggerS488.$1) {
          moonbit_incref(_M0L6loggerS488.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS488.$0->$method_0(_M0L6loggerS488.$1, (moonbit_string_t)moonbit_string_literal_100.data);
        _M0L6_2atmpS2270 = _M0L1iS492 + 1;
        _M0L6_2atmpS2271 = _M0L1iS492 + 1;
        _M0L1iS492 = _M0L6_2atmpS2270;
        _M0L3segS493 = _M0L6_2atmpS2271;
        goto _2afor_494;
        break;
      }
      default: {
        if (_M0L4codeS495 < 32) {
          int32_t _M0L6_2atmpS2273;
          moonbit_string_t _M0L6_2atmpS2272;
          int32_t _M0L6_2atmpS2274;
          int32_t _M0L6_2atmpS2275;
          moonbit_incref(_M0L6_2aenvS489);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS489, _M0L3segS493, _M0L1iS492);
          if (_M0L6loggerS488.$1) {
            moonbit_incref(_M0L6loggerS488.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS488.$0->$method_0(_M0L6loggerS488.$1, (moonbit_string_t)moonbit_string_literal_117.data);
          _M0L6_2atmpS2273 = _M0L4codeS495 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS2272 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2273);
          if (_M0L6loggerS488.$1) {
            moonbit_incref(_M0L6loggerS488.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS488.$0->$method_0(_M0L6loggerS488.$1, _M0L6_2atmpS2272);
          if (_M0L6loggerS488.$1) {
            moonbit_incref(_M0L6loggerS488.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS488.$0->$method_3(_M0L6loggerS488.$1, 125);
          _M0L6_2atmpS2274 = _M0L1iS492 + 1;
          _M0L6_2atmpS2275 = _M0L1iS492 + 1;
          _M0L1iS492 = _M0L6_2atmpS2274;
          _M0L3segS493 = _M0L6_2atmpS2275;
          goto _2afor_494;
        } else {
          int32_t _M0L6_2atmpS2276 = _M0L1iS492 + 1;
          int32_t _tmp_4555 = _M0L3segS493;
          _M0L1iS492 = _M0L6_2atmpS2276;
          _M0L3segS493 = _tmp_4555;
          goto _2afor_494;
        }
        break;
      }
    }
    goto joinlet_4554;
    join_496:;
    moonbit_incref(_M0L6_2aenvS489);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS489, _M0L3segS493, _M0L1iS492);
    if (_M0L6loggerS488.$1) {
      moonbit_incref(_M0L6loggerS488.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS488.$0->$method_3(_M0L6loggerS488.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2261 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS497);
    if (_M0L6loggerS488.$1) {
      moonbit_incref(_M0L6loggerS488.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS488.$0->$method_3(_M0L6loggerS488.$1, _M0L6_2atmpS2261);
    _M0L6_2atmpS2262 = _M0L1iS492 + 1;
    _M0L6_2atmpS2263 = _M0L1iS492 + 1;
    _M0L1iS492 = _M0L6_2atmpS2262;
    _M0L3segS493 = _M0L6_2atmpS2263;
    continue;
    joinlet_4554:;
    _tmp_4556 = _M0L1iS492;
    _tmp_4557 = _M0L3segS493;
    _M0L1iS492 = _tmp_4556;
    _M0L3segS493 = _tmp_4557;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS488.$0->$method_3(_M0L6loggerS488.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS484,
  int32_t _M0L3segS487,
  int32_t _M0L1iS486
) {
  struct _M0TPB6Logger _M0L8_2afieldS4139;
  struct _M0TPB6Logger _M0L6loggerS483;
  moonbit_string_t _M0L8_2afieldS4138;
  int32_t _M0L6_2acntS4360;
  moonbit_string_t _M0L4selfS485;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS4139
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS484->$1_0, _M0L6_2aenvS484->$1_1
  };
  _M0L6loggerS483 = _M0L8_2afieldS4139;
  _M0L8_2afieldS4138 = _M0L6_2aenvS484->$0;
  _M0L6_2acntS4360 = Moonbit_object_header(_M0L6_2aenvS484)->rc;
  if (_M0L6_2acntS4360 > 1) {
    int32_t _M0L11_2anew__cntS4361 = _M0L6_2acntS4360 - 1;
    Moonbit_object_header(_M0L6_2aenvS484)->rc = _M0L11_2anew__cntS4361;
    if (_M0L6loggerS483.$1) {
      moonbit_incref(_M0L6loggerS483.$1);
    }
    moonbit_incref(_M0L8_2afieldS4138);
  } else if (_M0L6_2acntS4360 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS484);
  }
  _M0L4selfS485 = _M0L8_2afieldS4138;
  if (_M0L1iS486 > _M0L3segS487) {
    int32_t _M0L6_2atmpS2260 = _M0L1iS486 - _M0L3segS487;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS483.$0->$method_1(_M0L6loggerS483.$1, _M0L4selfS485, _M0L3segS487, _M0L6_2atmpS2260);
  } else {
    moonbit_decref(_M0L4selfS485);
    if (_M0L6loggerS483.$1) {
      moonbit_decref(_M0L6loggerS483.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS482) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS481;
  int32_t _M0L6_2atmpS2257;
  int32_t _M0L6_2atmpS2256;
  int32_t _M0L6_2atmpS2259;
  int32_t _M0L6_2atmpS2258;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS2255;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS481 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2257 = _M0IPC14byte4BytePB3Div3div(_M0L1bS482, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2256
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2257);
  moonbit_incref(_M0L7_2aselfS481);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS481, _M0L6_2atmpS2256);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2259 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS482, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2258
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS2259);
  moonbit_incref(_M0L7_2aselfS481);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS481, _M0L6_2atmpS2258);
  _M0L6_2atmpS2255 = _M0L7_2aselfS481;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS2255);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS480) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS480 < 10) {
    int32_t _M0L6_2atmpS2252;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2252 = _M0IPC14byte4BytePB3Add3add(_M0L1iS480, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2252);
  } else {
    int32_t _M0L6_2atmpS2254;
    int32_t _M0L6_2atmpS2253;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2254 = _M0IPC14byte4BytePB3Add3add(_M0L1iS480, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS2253 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS2254, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2253);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS478,
  int32_t _M0L4thatS479
) {
  int32_t _M0L6_2atmpS2250;
  int32_t _M0L6_2atmpS2251;
  int32_t _M0L6_2atmpS2249;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2250 = (int32_t)_M0L4selfS478;
  _M0L6_2atmpS2251 = (int32_t)_M0L4thatS479;
  _M0L6_2atmpS2249 = _M0L6_2atmpS2250 - _M0L6_2atmpS2251;
  return _M0L6_2atmpS2249 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS476,
  int32_t _M0L4thatS477
) {
  int32_t _M0L6_2atmpS2247;
  int32_t _M0L6_2atmpS2248;
  int32_t _M0L6_2atmpS2246;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2247 = (int32_t)_M0L4selfS476;
  _M0L6_2atmpS2248 = (int32_t)_M0L4thatS477;
  _M0L6_2atmpS2246 = _M0L6_2atmpS2247 % _M0L6_2atmpS2248;
  return _M0L6_2atmpS2246 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS474,
  int32_t _M0L4thatS475
) {
  int32_t _M0L6_2atmpS2244;
  int32_t _M0L6_2atmpS2245;
  int32_t _M0L6_2atmpS2243;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2244 = (int32_t)_M0L4selfS474;
  _M0L6_2atmpS2245 = (int32_t)_M0L4thatS475;
  _M0L6_2atmpS2243 = _M0L6_2atmpS2244 / _M0L6_2atmpS2245;
  return _M0L6_2atmpS2243 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS472,
  int32_t _M0L4thatS473
) {
  int32_t _M0L6_2atmpS2241;
  int32_t _M0L6_2atmpS2242;
  int32_t _M0L6_2atmpS2240;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2241 = (int32_t)_M0L4selfS472;
  _M0L6_2atmpS2242 = (int32_t)_M0L4thatS473;
  _M0L6_2atmpS2240 = _M0L6_2atmpS2241 + _M0L6_2atmpS2242;
  return _M0L6_2atmpS2240 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS469,
  int32_t _M0L5startS467,
  int32_t _M0L3endS468
) {
  int32_t _if__result_4558;
  int32_t _M0L3lenS470;
  int32_t _M0L6_2atmpS2238;
  int32_t _M0L6_2atmpS2239;
  moonbit_bytes_t _M0L5bytesS471;
  moonbit_bytes_t _M0L6_2atmpS2237;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS467 == 0) {
    int32_t _M0L6_2atmpS2236 = Moonbit_array_length(_M0L3strS469);
    _if__result_4558 = _M0L3endS468 == _M0L6_2atmpS2236;
  } else {
    _if__result_4558 = 0;
  }
  if (_if__result_4558) {
    return _M0L3strS469;
  }
  _M0L3lenS470 = _M0L3endS468 - _M0L5startS467;
  _M0L6_2atmpS2238 = _M0L3lenS470 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS2239 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS471
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS2238, _M0L6_2atmpS2239);
  moonbit_incref(_M0L5bytesS471);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS471, 0, _M0L3strS469, _M0L5startS467, _M0L3lenS470);
  _M0L6_2atmpS2237 = _M0L5bytesS471;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2237, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS463) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS463;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS464
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS464;
}

struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0MPB4Iter3newGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L1fS465
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS465;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS466) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS466;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS455,
  int32_t _M0L5radixS454
) {
  int32_t _if__result_4559;
  uint16_t* _M0L6bufferS456;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS454 < 2) {
    _if__result_4559 = 1;
  } else {
    _if__result_4559 = _M0L5radixS454 > 36;
  }
  if (_if__result_4559) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_118.data, (moonbit_string_t)moonbit_string_literal_119.data);
  }
  if (_M0L4selfS455 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_105.data;
  }
  switch (_M0L5radixS454) {
    case 10: {
      int32_t _M0L3lenS457;
      uint16_t* _M0L6bufferS458;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS457 = _M0FPB12dec__count64(_M0L4selfS455);
      _M0L6bufferS458 = (uint16_t*)moonbit_make_string(_M0L3lenS457, 0);
      moonbit_incref(_M0L6bufferS458);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS458, _M0L4selfS455, 0, _M0L3lenS457);
      _M0L6bufferS456 = _M0L6bufferS458;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS459;
      uint16_t* _M0L6bufferS460;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS459 = _M0FPB12hex__count64(_M0L4selfS455);
      _M0L6bufferS460 = (uint16_t*)moonbit_make_string(_M0L3lenS459, 0);
      moonbit_incref(_M0L6bufferS460);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS460, _M0L4selfS455, 0, _M0L3lenS459);
      _M0L6bufferS456 = _M0L6bufferS460;
      break;
    }
    default: {
      int32_t _M0L3lenS461;
      uint16_t* _M0L6bufferS462;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS461 = _M0FPB14radix__count64(_M0L4selfS455, _M0L5radixS454);
      _M0L6bufferS462 = (uint16_t*)moonbit_make_string(_M0L3lenS461, 0);
      moonbit_incref(_M0L6bufferS462);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS462, _M0L4selfS455, 0, _M0L3lenS461, _M0L5radixS454);
      _M0L6bufferS456 = _M0L6bufferS462;
      break;
    }
  }
  return _M0L6bufferS456;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS444,
  uint64_t _M0L3numS432,
  int32_t _M0L12digit__startS435,
  int32_t _M0L10total__lenS434
) {
  uint64_t _M0Lm3numS431;
  int32_t _M0Lm6offsetS433;
  uint64_t _M0L6_2atmpS2235;
  int32_t _M0Lm9remainingS446;
  int32_t _M0L6_2atmpS2216;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS431 = _M0L3numS432;
  _M0Lm6offsetS433 = _M0L10total__lenS434 - _M0L12digit__startS435;
  while (1) {
    uint64_t _M0L6_2atmpS2179 = _M0Lm3numS431;
    if (_M0L6_2atmpS2179 >= 10000ull) {
      uint64_t _M0L6_2atmpS2202 = _M0Lm3numS431;
      uint64_t _M0L1tS436 = _M0L6_2atmpS2202 / 10000ull;
      uint64_t _M0L6_2atmpS2201 = _M0Lm3numS431;
      uint64_t _M0L6_2atmpS2200 = _M0L6_2atmpS2201 % 10000ull;
      int32_t _M0L1rS437 = (int32_t)_M0L6_2atmpS2200;
      int32_t _M0L2d1S438;
      int32_t _M0L2d2S439;
      int32_t _M0L6_2atmpS2180;
      int32_t _M0L6_2atmpS2199;
      int32_t _M0L6_2atmpS2198;
      int32_t _M0L6d1__hiS440;
      int32_t _M0L6_2atmpS2197;
      int32_t _M0L6_2atmpS2196;
      int32_t _M0L6d1__loS441;
      int32_t _M0L6_2atmpS2195;
      int32_t _M0L6_2atmpS2194;
      int32_t _M0L6d2__hiS442;
      int32_t _M0L6_2atmpS2193;
      int32_t _M0L6_2atmpS2192;
      int32_t _M0L6d2__loS443;
      int32_t _M0L6_2atmpS2182;
      int32_t _M0L6_2atmpS2181;
      int32_t _M0L6_2atmpS2185;
      int32_t _M0L6_2atmpS2184;
      int32_t _M0L6_2atmpS2183;
      int32_t _M0L6_2atmpS2188;
      int32_t _M0L6_2atmpS2187;
      int32_t _M0L6_2atmpS2186;
      int32_t _M0L6_2atmpS2191;
      int32_t _M0L6_2atmpS2190;
      int32_t _M0L6_2atmpS2189;
      _M0Lm3numS431 = _M0L1tS436;
      _M0L2d1S438 = _M0L1rS437 / 100;
      _M0L2d2S439 = _M0L1rS437 % 100;
      _M0L6_2atmpS2180 = _M0Lm6offsetS433;
      _M0Lm6offsetS433 = _M0L6_2atmpS2180 - 4;
      _M0L6_2atmpS2199 = _M0L2d1S438 / 10;
      _M0L6_2atmpS2198 = 48 + _M0L6_2atmpS2199;
      _M0L6d1__hiS440 = (uint16_t)_M0L6_2atmpS2198;
      _M0L6_2atmpS2197 = _M0L2d1S438 % 10;
      _M0L6_2atmpS2196 = 48 + _M0L6_2atmpS2197;
      _M0L6d1__loS441 = (uint16_t)_M0L6_2atmpS2196;
      _M0L6_2atmpS2195 = _M0L2d2S439 / 10;
      _M0L6_2atmpS2194 = 48 + _M0L6_2atmpS2195;
      _M0L6d2__hiS442 = (uint16_t)_M0L6_2atmpS2194;
      _M0L6_2atmpS2193 = _M0L2d2S439 % 10;
      _M0L6_2atmpS2192 = 48 + _M0L6_2atmpS2193;
      _M0L6d2__loS443 = (uint16_t)_M0L6_2atmpS2192;
      _M0L6_2atmpS2182 = _M0Lm6offsetS433;
      _M0L6_2atmpS2181 = _M0L12digit__startS435 + _M0L6_2atmpS2182;
      _M0L6bufferS444[_M0L6_2atmpS2181] = _M0L6d1__hiS440;
      _M0L6_2atmpS2185 = _M0Lm6offsetS433;
      _M0L6_2atmpS2184 = _M0L12digit__startS435 + _M0L6_2atmpS2185;
      _M0L6_2atmpS2183 = _M0L6_2atmpS2184 + 1;
      _M0L6bufferS444[_M0L6_2atmpS2183] = _M0L6d1__loS441;
      _M0L6_2atmpS2188 = _M0Lm6offsetS433;
      _M0L6_2atmpS2187 = _M0L12digit__startS435 + _M0L6_2atmpS2188;
      _M0L6_2atmpS2186 = _M0L6_2atmpS2187 + 2;
      _M0L6bufferS444[_M0L6_2atmpS2186] = _M0L6d2__hiS442;
      _M0L6_2atmpS2191 = _M0Lm6offsetS433;
      _M0L6_2atmpS2190 = _M0L12digit__startS435 + _M0L6_2atmpS2191;
      _M0L6_2atmpS2189 = _M0L6_2atmpS2190 + 3;
      _M0L6bufferS444[_M0L6_2atmpS2189] = _M0L6d2__loS443;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2235 = _M0Lm3numS431;
  _M0Lm9remainingS446 = (int32_t)_M0L6_2atmpS2235;
  while (1) {
    int32_t _M0L6_2atmpS2203 = _M0Lm9remainingS446;
    if (_M0L6_2atmpS2203 >= 100) {
      int32_t _M0L6_2atmpS2215 = _M0Lm9remainingS446;
      int32_t _M0L1tS447 = _M0L6_2atmpS2215 / 100;
      int32_t _M0L6_2atmpS2214 = _M0Lm9remainingS446;
      int32_t _M0L1dS448 = _M0L6_2atmpS2214 % 100;
      int32_t _M0L6_2atmpS2204;
      int32_t _M0L6_2atmpS2213;
      int32_t _M0L6_2atmpS2212;
      int32_t _M0L5d__hiS449;
      int32_t _M0L6_2atmpS2211;
      int32_t _M0L6_2atmpS2210;
      int32_t _M0L5d__loS450;
      int32_t _M0L6_2atmpS2206;
      int32_t _M0L6_2atmpS2205;
      int32_t _M0L6_2atmpS2209;
      int32_t _M0L6_2atmpS2208;
      int32_t _M0L6_2atmpS2207;
      _M0Lm9remainingS446 = _M0L1tS447;
      _M0L6_2atmpS2204 = _M0Lm6offsetS433;
      _M0Lm6offsetS433 = _M0L6_2atmpS2204 - 2;
      _M0L6_2atmpS2213 = _M0L1dS448 / 10;
      _M0L6_2atmpS2212 = 48 + _M0L6_2atmpS2213;
      _M0L5d__hiS449 = (uint16_t)_M0L6_2atmpS2212;
      _M0L6_2atmpS2211 = _M0L1dS448 % 10;
      _M0L6_2atmpS2210 = 48 + _M0L6_2atmpS2211;
      _M0L5d__loS450 = (uint16_t)_M0L6_2atmpS2210;
      _M0L6_2atmpS2206 = _M0Lm6offsetS433;
      _M0L6_2atmpS2205 = _M0L12digit__startS435 + _M0L6_2atmpS2206;
      _M0L6bufferS444[_M0L6_2atmpS2205] = _M0L5d__hiS449;
      _M0L6_2atmpS2209 = _M0Lm6offsetS433;
      _M0L6_2atmpS2208 = _M0L12digit__startS435 + _M0L6_2atmpS2209;
      _M0L6_2atmpS2207 = _M0L6_2atmpS2208 + 1;
      _M0L6bufferS444[_M0L6_2atmpS2207] = _M0L5d__loS450;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2216 = _M0Lm9remainingS446;
  if (_M0L6_2atmpS2216 >= 10) {
    int32_t _M0L6_2atmpS2217 = _M0Lm6offsetS433;
    int32_t _M0L6_2atmpS2228;
    int32_t _M0L6_2atmpS2227;
    int32_t _M0L6_2atmpS2226;
    int32_t _M0L5d__hiS452;
    int32_t _M0L6_2atmpS2225;
    int32_t _M0L6_2atmpS2224;
    int32_t _M0L6_2atmpS2223;
    int32_t _M0L5d__loS453;
    int32_t _M0L6_2atmpS2219;
    int32_t _M0L6_2atmpS2218;
    int32_t _M0L6_2atmpS2222;
    int32_t _M0L6_2atmpS2221;
    int32_t _M0L6_2atmpS2220;
    _M0Lm6offsetS433 = _M0L6_2atmpS2217 - 2;
    _M0L6_2atmpS2228 = _M0Lm9remainingS446;
    _M0L6_2atmpS2227 = _M0L6_2atmpS2228 / 10;
    _M0L6_2atmpS2226 = 48 + _M0L6_2atmpS2227;
    _M0L5d__hiS452 = (uint16_t)_M0L6_2atmpS2226;
    _M0L6_2atmpS2225 = _M0Lm9remainingS446;
    _M0L6_2atmpS2224 = _M0L6_2atmpS2225 % 10;
    _M0L6_2atmpS2223 = 48 + _M0L6_2atmpS2224;
    _M0L5d__loS453 = (uint16_t)_M0L6_2atmpS2223;
    _M0L6_2atmpS2219 = _M0Lm6offsetS433;
    _M0L6_2atmpS2218 = _M0L12digit__startS435 + _M0L6_2atmpS2219;
    _M0L6bufferS444[_M0L6_2atmpS2218] = _M0L5d__hiS452;
    _M0L6_2atmpS2222 = _M0Lm6offsetS433;
    _M0L6_2atmpS2221 = _M0L12digit__startS435 + _M0L6_2atmpS2222;
    _M0L6_2atmpS2220 = _M0L6_2atmpS2221 + 1;
    _M0L6bufferS444[_M0L6_2atmpS2220] = _M0L5d__loS453;
    moonbit_decref(_M0L6bufferS444);
  } else {
    int32_t _M0L6_2atmpS2229 = _M0Lm6offsetS433;
    int32_t _M0L6_2atmpS2234;
    int32_t _M0L6_2atmpS2230;
    int32_t _M0L6_2atmpS2233;
    int32_t _M0L6_2atmpS2232;
    int32_t _M0L6_2atmpS2231;
    _M0Lm6offsetS433 = _M0L6_2atmpS2229 - 1;
    _M0L6_2atmpS2234 = _M0Lm6offsetS433;
    _M0L6_2atmpS2230 = _M0L12digit__startS435 + _M0L6_2atmpS2234;
    _M0L6_2atmpS2233 = _M0Lm9remainingS446;
    _M0L6_2atmpS2232 = 48 + _M0L6_2atmpS2233;
    _M0L6_2atmpS2231 = (uint16_t)_M0L6_2atmpS2232;
    _M0L6bufferS444[_M0L6_2atmpS2230] = _M0L6_2atmpS2231;
    moonbit_decref(_M0L6bufferS444);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS426,
  uint64_t _M0L3numS420,
  int32_t _M0L12digit__startS418,
  int32_t _M0L10total__lenS417,
  int32_t _M0L5radixS422
) {
  int32_t _M0Lm6offsetS416;
  uint64_t _M0Lm1nS419;
  uint64_t _M0L4baseS421;
  int32_t _M0L6_2atmpS2161;
  int32_t _M0L6_2atmpS2160;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS416 = _M0L10total__lenS417 - _M0L12digit__startS418;
  _M0Lm1nS419 = _M0L3numS420;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS421 = _M0MPC13int3Int10to__uint64(_M0L5radixS422);
  _M0L6_2atmpS2161 = _M0L5radixS422 - 1;
  _M0L6_2atmpS2160 = _M0L5radixS422 & _M0L6_2atmpS2161;
  if (_M0L6_2atmpS2160 == 0) {
    int32_t _M0L5shiftS423;
    uint64_t _M0L4maskS424;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS423 = moonbit_ctz32(_M0L5radixS422);
    _M0L4maskS424 = _M0L4baseS421 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS2162 = _M0Lm1nS419;
      if (_M0L6_2atmpS2162 > 0ull) {
        int32_t _M0L6_2atmpS2163 = _M0Lm6offsetS416;
        uint64_t _M0L6_2atmpS2169;
        uint64_t _M0L6_2atmpS2168;
        int32_t _M0L5digitS425;
        int32_t _M0L6_2atmpS2166;
        int32_t _M0L6_2atmpS2164;
        int32_t _M0L6_2atmpS2165;
        uint64_t _M0L6_2atmpS2167;
        _M0Lm6offsetS416 = _M0L6_2atmpS2163 - 1;
        _M0L6_2atmpS2169 = _M0Lm1nS419;
        _M0L6_2atmpS2168 = _M0L6_2atmpS2169 & _M0L4maskS424;
        _M0L5digitS425 = (int32_t)_M0L6_2atmpS2168;
        _M0L6_2atmpS2166 = _M0Lm6offsetS416;
        _M0L6_2atmpS2164 = _M0L12digit__startS418 + _M0L6_2atmpS2166;
        _M0L6_2atmpS2165
        = ((moonbit_string_t)moonbit_string_literal_120.data)[
          _M0L5digitS425
        ];
        _M0L6bufferS426[_M0L6_2atmpS2164] = _M0L6_2atmpS2165;
        _M0L6_2atmpS2167 = _M0Lm1nS419;
        _M0Lm1nS419 = _M0L6_2atmpS2167 >> (_M0L5shiftS423 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS426);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS2170 = _M0Lm1nS419;
      if (_M0L6_2atmpS2170 > 0ull) {
        int32_t _M0L6_2atmpS2171 = _M0Lm6offsetS416;
        uint64_t _M0L6_2atmpS2178;
        uint64_t _M0L1qS428;
        uint64_t _M0L6_2atmpS2176;
        uint64_t _M0L6_2atmpS2177;
        uint64_t _M0L6_2atmpS2175;
        int32_t _M0L5digitS429;
        int32_t _M0L6_2atmpS2174;
        int32_t _M0L6_2atmpS2172;
        int32_t _M0L6_2atmpS2173;
        _M0Lm6offsetS416 = _M0L6_2atmpS2171 - 1;
        _M0L6_2atmpS2178 = _M0Lm1nS419;
        _M0L1qS428 = _M0L6_2atmpS2178 / _M0L4baseS421;
        _M0L6_2atmpS2176 = _M0Lm1nS419;
        _M0L6_2atmpS2177 = _M0L1qS428 * _M0L4baseS421;
        _M0L6_2atmpS2175 = _M0L6_2atmpS2176 - _M0L6_2atmpS2177;
        _M0L5digitS429 = (int32_t)_M0L6_2atmpS2175;
        _M0L6_2atmpS2174 = _M0Lm6offsetS416;
        _M0L6_2atmpS2172 = _M0L12digit__startS418 + _M0L6_2atmpS2174;
        _M0L6_2atmpS2173
        = ((moonbit_string_t)moonbit_string_literal_120.data)[
          _M0L5digitS429
        ];
        _M0L6bufferS426[_M0L6_2atmpS2172] = _M0L6_2atmpS2173;
        _M0Lm1nS419 = _M0L1qS428;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS426);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS413,
  uint64_t _M0L3numS409,
  int32_t _M0L12digit__startS407,
  int32_t _M0L10total__lenS406
) {
  int32_t _M0Lm6offsetS405;
  uint64_t _M0Lm1nS408;
  int32_t _M0L6_2atmpS2156;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS405 = _M0L10total__lenS406 - _M0L12digit__startS407;
  _M0Lm1nS408 = _M0L3numS409;
  while (1) {
    int32_t _M0L6_2atmpS2144 = _M0Lm6offsetS405;
    if (_M0L6_2atmpS2144 >= 2) {
      int32_t _M0L6_2atmpS2145 = _M0Lm6offsetS405;
      uint64_t _M0L6_2atmpS2155;
      uint64_t _M0L6_2atmpS2154;
      int32_t _M0L9byte__valS410;
      int32_t _M0L2hiS411;
      int32_t _M0L2loS412;
      int32_t _M0L6_2atmpS2148;
      int32_t _M0L6_2atmpS2146;
      int32_t _M0L6_2atmpS2147;
      int32_t _M0L6_2atmpS2152;
      int32_t _M0L6_2atmpS2151;
      int32_t _M0L6_2atmpS2149;
      int32_t _M0L6_2atmpS2150;
      uint64_t _M0L6_2atmpS2153;
      _M0Lm6offsetS405 = _M0L6_2atmpS2145 - 2;
      _M0L6_2atmpS2155 = _M0Lm1nS408;
      _M0L6_2atmpS2154 = _M0L6_2atmpS2155 & 255ull;
      _M0L9byte__valS410 = (int32_t)_M0L6_2atmpS2154;
      _M0L2hiS411 = _M0L9byte__valS410 / 16;
      _M0L2loS412 = _M0L9byte__valS410 % 16;
      _M0L6_2atmpS2148 = _M0Lm6offsetS405;
      _M0L6_2atmpS2146 = _M0L12digit__startS407 + _M0L6_2atmpS2148;
      _M0L6_2atmpS2147
      = ((moonbit_string_t)moonbit_string_literal_120.data)[
        _M0L2hiS411
      ];
      _M0L6bufferS413[_M0L6_2atmpS2146] = _M0L6_2atmpS2147;
      _M0L6_2atmpS2152 = _M0Lm6offsetS405;
      _M0L6_2atmpS2151 = _M0L12digit__startS407 + _M0L6_2atmpS2152;
      _M0L6_2atmpS2149 = _M0L6_2atmpS2151 + 1;
      _M0L6_2atmpS2150
      = ((moonbit_string_t)moonbit_string_literal_120.data)[
        _M0L2loS412
      ];
      _M0L6bufferS413[_M0L6_2atmpS2149] = _M0L6_2atmpS2150;
      _M0L6_2atmpS2153 = _M0Lm1nS408;
      _M0Lm1nS408 = _M0L6_2atmpS2153 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2156 = _M0Lm6offsetS405;
  if (_M0L6_2atmpS2156 == 1) {
    uint64_t _M0L6_2atmpS2159 = _M0Lm1nS408;
    uint64_t _M0L6_2atmpS2158 = _M0L6_2atmpS2159 & 15ull;
    int32_t _M0L6nibbleS415 = (int32_t)_M0L6_2atmpS2158;
    int32_t _M0L6_2atmpS2157 =
      ((moonbit_string_t)moonbit_string_literal_120.data)[_M0L6nibbleS415];
    _M0L6bufferS413[_M0L12digit__startS407] = _M0L6_2atmpS2157;
    moonbit_decref(_M0L6bufferS413);
  } else {
    moonbit_decref(_M0L6bufferS413);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS399,
  int32_t _M0L5radixS402
) {
  uint64_t _M0Lm3numS400;
  uint64_t _M0L4baseS401;
  int32_t _M0Lm5countS403;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS399 == 0ull) {
    return 1;
  }
  _M0Lm3numS400 = _M0L5valueS399;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS401 = _M0MPC13int3Int10to__uint64(_M0L5radixS402);
  _M0Lm5countS403 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS2141 = _M0Lm3numS400;
    if (_M0L6_2atmpS2141 > 0ull) {
      int32_t _M0L6_2atmpS2142 = _M0Lm5countS403;
      uint64_t _M0L6_2atmpS2143;
      _M0Lm5countS403 = _M0L6_2atmpS2142 + 1;
      _M0L6_2atmpS2143 = _M0Lm3numS400;
      _M0Lm3numS400 = _M0L6_2atmpS2143 / _M0L4baseS401;
      continue;
    }
    break;
  }
  return _M0Lm5countS403;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS397) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS397 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS398;
    int32_t _M0L6_2atmpS2140;
    int32_t _M0L6_2atmpS2139;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS398 = moonbit_clz64(_M0L5valueS397);
    _M0L6_2atmpS2140 = 63 - _M0L14leading__zerosS398;
    _M0L6_2atmpS2139 = _M0L6_2atmpS2140 / 4;
    return _M0L6_2atmpS2139 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS396) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS396 >= 10000000000ull) {
    if (_M0L5valueS396 >= 100000000000000ull) {
      if (_M0L5valueS396 >= 10000000000000000ull) {
        if (_M0L5valueS396 >= 1000000000000000000ull) {
          if (_M0L5valueS396 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS396 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS396 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS396 >= 1000000000000ull) {
      if (_M0L5valueS396 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS396 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS396 >= 100000ull) {
    if (_M0L5valueS396 >= 10000000ull) {
      if (_M0L5valueS396 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS396 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS396 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS396 >= 1000ull) {
    if (_M0L5valueS396 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS396 >= 100ull) {
    return 3;
  } else if (_M0L5valueS396 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS380,
  int32_t _M0L5radixS379
) {
  int32_t _if__result_4566;
  int32_t _M0L12is__negativeS381;
  uint32_t _M0L3numS382;
  uint16_t* _M0L6bufferS383;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS379 < 2) {
    _if__result_4566 = 1;
  } else {
    _if__result_4566 = _M0L5radixS379 > 36;
  }
  if (_if__result_4566) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_118.data, (moonbit_string_t)moonbit_string_literal_121.data);
  }
  if (_M0L4selfS380 == 0) {
    return (moonbit_string_t)moonbit_string_literal_105.data;
  }
  _M0L12is__negativeS381 = _M0L4selfS380 < 0;
  if (_M0L12is__negativeS381) {
    int32_t _M0L6_2atmpS2138 = -_M0L4selfS380;
    _M0L3numS382 = *(uint32_t*)&_M0L6_2atmpS2138;
  } else {
    _M0L3numS382 = *(uint32_t*)&_M0L4selfS380;
  }
  switch (_M0L5radixS379) {
    case 10: {
      int32_t _M0L10digit__lenS384;
      int32_t _M0L6_2atmpS2135;
      int32_t _M0L10total__lenS385;
      uint16_t* _M0L6bufferS386;
      int32_t _M0L12digit__startS387;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS384 = _M0FPB12dec__count32(_M0L3numS382);
      if (_M0L12is__negativeS381) {
        _M0L6_2atmpS2135 = 1;
      } else {
        _M0L6_2atmpS2135 = 0;
      }
      _M0L10total__lenS385 = _M0L10digit__lenS384 + _M0L6_2atmpS2135;
      _M0L6bufferS386
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS385, 0);
      if (_M0L12is__negativeS381) {
        _M0L12digit__startS387 = 1;
      } else {
        _M0L12digit__startS387 = 0;
      }
      moonbit_incref(_M0L6bufferS386);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS386, _M0L3numS382, _M0L12digit__startS387, _M0L10total__lenS385);
      _M0L6bufferS383 = _M0L6bufferS386;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS388;
      int32_t _M0L6_2atmpS2136;
      int32_t _M0L10total__lenS389;
      uint16_t* _M0L6bufferS390;
      int32_t _M0L12digit__startS391;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS388 = _M0FPB12hex__count32(_M0L3numS382);
      if (_M0L12is__negativeS381) {
        _M0L6_2atmpS2136 = 1;
      } else {
        _M0L6_2atmpS2136 = 0;
      }
      _M0L10total__lenS389 = _M0L10digit__lenS388 + _M0L6_2atmpS2136;
      _M0L6bufferS390
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS389, 0);
      if (_M0L12is__negativeS381) {
        _M0L12digit__startS391 = 1;
      } else {
        _M0L12digit__startS391 = 0;
      }
      moonbit_incref(_M0L6bufferS390);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS390, _M0L3numS382, _M0L12digit__startS391, _M0L10total__lenS389);
      _M0L6bufferS383 = _M0L6bufferS390;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS392;
      int32_t _M0L6_2atmpS2137;
      int32_t _M0L10total__lenS393;
      uint16_t* _M0L6bufferS394;
      int32_t _M0L12digit__startS395;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS392
      = _M0FPB14radix__count32(_M0L3numS382, _M0L5radixS379);
      if (_M0L12is__negativeS381) {
        _M0L6_2atmpS2137 = 1;
      } else {
        _M0L6_2atmpS2137 = 0;
      }
      _M0L10total__lenS393 = _M0L10digit__lenS392 + _M0L6_2atmpS2137;
      _M0L6bufferS394
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS393, 0);
      if (_M0L12is__negativeS381) {
        _M0L12digit__startS395 = 1;
      } else {
        _M0L12digit__startS395 = 0;
      }
      moonbit_incref(_M0L6bufferS394);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS394, _M0L3numS382, _M0L12digit__startS395, _M0L10total__lenS393, _M0L5radixS379);
      _M0L6bufferS383 = _M0L6bufferS394;
      break;
    }
  }
  if (_M0L12is__negativeS381) {
    _M0L6bufferS383[0] = 45;
  }
  return _M0L6bufferS383;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS373,
  int32_t _M0L5radixS376
) {
  uint32_t _M0Lm3numS374;
  uint32_t _M0L4baseS375;
  int32_t _M0Lm5countS377;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS373 == 0u) {
    return 1;
  }
  _M0Lm3numS374 = _M0L5valueS373;
  _M0L4baseS375 = *(uint32_t*)&_M0L5radixS376;
  _M0Lm5countS377 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS2132 = _M0Lm3numS374;
    if (_M0L6_2atmpS2132 > 0u) {
      int32_t _M0L6_2atmpS2133 = _M0Lm5countS377;
      uint32_t _M0L6_2atmpS2134;
      _M0Lm5countS377 = _M0L6_2atmpS2133 + 1;
      _M0L6_2atmpS2134 = _M0Lm3numS374;
      _M0Lm3numS374 = _M0L6_2atmpS2134 / _M0L4baseS375;
      continue;
    }
    break;
  }
  return _M0Lm5countS377;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS371) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS371 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS372;
    int32_t _M0L6_2atmpS2131;
    int32_t _M0L6_2atmpS2130;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS372 = moonbit_clz32(_M0L5valueS371);
    _M0L6_2atmpS2131 = 31 - _M0L14leading__zerosS372;
    _M0L6_2atmpS2130 = _M0L6_2atmpS2131 / 4;
    return _M0L6_2atmpS2130 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS370) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS370 >= 100000u) {
    if (_M0L5valueS370 >= 10000000u) {
      if (_M0L5valueS370 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS370 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS370 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS370 >= 1000u) {
    if (_M0L5valueS370 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS370 >= 100u) {
    return 3;
  } else if (_M0L5valueS370 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS360,
  uint32_t _M0L3numS348,
  int32_t _M0L12digit__startS351,
  int32_t _M0L10total__lenS350
) {
  uint32_t _M0Lm3numS347;
  int32_t _M0Lm6offsetS349;
  uint32_t _M0L6_2atmpS2129;
  int32_t _M0Lm9remainingS362;
  int32_t _M0L6_2atmpS2110;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS347 = _M0L3numS348;
  _M0Lm6offsetS349 = _M0L10total__lenS350 - _M0L12digit__startS351;
  while (1) {
    uint32_t _M0L6_2atmpS2073 = _M0Lm3numS347;
    if (_M0L6_2atmpS2073 >= 10000u) {
      uint32_t _M0L6_2atmpS2096 = _M0Lm3numS347;
      uint32_t _M0L1tS352 = _M0L6_2atmpS2096 / 10000u;
      uint32_t _M0L6_2atmpS2095 = _M0Lm3numS347;
      uint32_t _M0L6_2atmpS2094 = _M0L6_2atmpS2095 % 10000u;
      int32_t _M0L1rS353 = *(int32_t*)&_M0L6_2atmpS2094;
      int32_t _M0L2d1S354;
      int32_t _M0L2d2S355;
      int32_t _M0L6_2atmpS2074;
      int32_t _M0L6_2atmpS2093;
      int32_t _M0L6_2atmpS2092;
      int32_t _M0L6d1__hiS356;
      int32_t _M0L6_2atmpS2091;
      int32_t _M0L6_2atmpS2090;
      int32_t _M0L6d1__loS357;
      int32_t _M0L6_2atmpS2089;
      int32_t _M0L6_2atmpS2088;
      int32_t _M0L6d2__hiS358;
      int32_t _M0L6_2atmpS2087;
      int32_t _M0L6_2atmpS2086;
      int32_t _M0L6d2__loS359;
      int32_t _M0L6_2atmpS2076;
      int32_t _M0L6_2atmpS2075;
      int32_t _M0L6_2atmpS2079;
      int32_t _M0L6_2atmpS2078;
      int32_t _M0L6_2atmpS2077;
      int32_t _M0L6_2atmpS2082;
      int32_t _M0L6_2atmpS2081;
      int32_t _M0L6_2atmpS2080;
      int32_t _M0L6_2atmpS2085;
      int32_t _M0L6_2atmpS2084;
      int32_t _M0L6_2atmpS2083;
      _M0Lm3numS347 = _M0L1tS352;
      _M0L2d1S354 = _M0L1rS353 / 100;
      _M0L2d2S355 = _M0L1rS353 % 100;
      _M0L6_2atmpS2074 = _M0Lm6offsetS349;
      _M0Lm6offsetS349 = _M0L6_2atmpS2074 - 4;
      _M0L6_2atmpS2093 = _M0L2d1S354 / 10;
      _M0L6_2atmpS2092 = 48 + _M0L6_2atmpS2093;
      _M0L6d1__hiS356 = (uint16_t)_M0L6_2atmpS2092;
      _M0L6_2atmpS2091 = _M0L2d1S354 % 10;
      _M0L6_2atmpS2090 = 48 + _M0L6_2atmpS2091;
      _M0L6d1__loS357 = (uint16_t)_M0L6_2atmpS2090;
      _M0L6_2atmpS2089 = _M0L2d2S355 / 10;
      _M0L6_2atmpS2088 = 48 + _M0L6_2atmpS2089;
      _M0L6d2__hiS358 = (uint16_t)_M0L6_2atmpS2088;
      _M0L6_2atmpS2087 = _M0L2d2S355 % 10;
      _M0L6_2atmpS2086 = 48 + _M0L6_2atmpS2087;
      _M0L6d2__loS359 = (uint16_t)_M0L6_2atmpS2086;
      _M0L6_2atmpS2076 = _M0Lm6offsetS349;
      _M0L6_2atmpS2075 = _M0L12digit__startS351 + _M0L6_2atmpS2076;
      _M0L6bufferS360[_M0L6_2atmpS2075] = _M0L6d1__hiS356;
      _M0L6_2atmpS2079 = _M0Lm6offsetS349;
      _M0L6_2atmpS2078 = _M0L12digit__startS351 + _M0L6_2atmpS2079;
      _M0L6_2atmpS2077 = _M0L6_2atmpS2078 + 1;
      _M0L6bufferS360[_M0L6_2atmpS2077] = _M0L6d1__loS357;
      _M0L6_2atmpS2082 = _M0Lm6offsetS349;
      _M0L6_2atmpS2081 = _M0L12digit__startS351 + _M0L6_2atmpS2082;
      _M0L6_2atmpS2080 = _M0L6_2atmpS2081 + 2;
      _M0L6bufferS360[_M0L6_2atmpS2080] = _M0L6d2__hiS358;
      _M0L6_2atmpS2085 = _M0Lm6offsetS349;
      _M0L6_2atmpS2084 = _M0L12digit__startS351 + _M0L6_2atmpS2085;
      _M0L6_2atmpS2083 = _M0L6_2atmpS2084 + 3;
      _M0L6bufferS360[_M0L6_2atmpS2083] = _M0L6d2__loS359;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2129 = _M0Lm3numS347;
  _M0Lm9remainingS362 = *(int32_t*)&_M0L6_2atmpS2129;
  while (1) {
    int32_t _M0L6_2atmpS2097 = _M0Lm9remainingS362;
    if (_M0L6_2atmpS2097 >= 100) {
      int32_t _M0L6_2atmpS2109 = _M0Lm9remainingS362;
      int32_t _M0L1tS363 = _M0L6_2atmpS2109 / 100;
      int32_t _M0L6_2atmpS2108 = _M0Lm9remainingS362;
      int32_t _M0L1dS364 = _M0L6_2atmpS2108 % 100;
      int32_t _M0L6_2atmpS2098;
      int32_t _M0L6_2atmpS2107;
      int32_t _M0L6_2atmpS2106;
      int32_t _M0L5d__hiS365;
      int32_t _M0L6_2atmpS2105;
      int32_t _M0L6_2atmpS2104;
      int32_t _M0L5d__loS366;
      int32_t _M0L6_2atmpS2100;
      int32_t _M0L6_2atmpS2099;
      int32_t _M0L6_2atmpS2103;
      int32_t _M0L6_2atmpS2102;
      int32_t _M0L6_2atmpS2101;
      _M0Lm9remainingS362 = _M0L1tS363;
      _M0L6_2atmpS2098 = _M0Lm6offsetS349;
      _M0Lm6offsetS349 = _M0L6_2atmpS2098 - 2;
      _M0L6_2atmpS2107 = _M0L1dS364 / 10;
      _M0L6_2atmpS2106 = 48 + _M0L6_2atmpS2107;
      _M0L5d__hiS365 = (uint16_t)_M0L6_2atmpS2106;
      _M0L6_2atmpS2105 = _M0L1dS364 % 10;
      _M0L6_2atmpS2104 = 48 + _M0L6_2atmpS2105;
      _M0L5d__loS366 = (uint16_t)_M0L6_2atmpS2104;
      _M0L6_2atmpS2100 = _M0Lm6offsetS349;
      _M0L6_2atmpS2099 = _M0L12digit__startS351 + _M0L6_2atmpS2100;
      _M0L6bufferS360[_M0L6_2atmpS2099] = _M0L5d__hiS365;
      _M0L6_2atmpS2103 = _M0Lm6offsetS349;
      _M0L6_2atmpS2102 = _M0L12digit__startS351 + _M0L6_2atmpS2103;
      _M0L6_2atmpS2101 = _M0L6_2atmpS2102 + 1;
      _M0L6bufferS360[_M0L6_2atmpS2101] = _M0L5d__loS366;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2110 = _M0Lm9remainingS362;
  if (_M0L6_2atmpS2110 >= 10) {
    int32_t _M0L6_2atmpS2111 = _M0Lm6offsetS349;
    int32_t _M0L6_2atmpS2122;
    int32_t _M0L6_2atmpS2121;
    int32_t _M0L6_2atmpS2120;
    int32_t _M0L5d__hiS368;
    int32_t _M0L6_2atmpS2119;
    int32_t _M0L6_2atmpS2118;
    int32_t _M0L6_2atmpS2117;
    int32_t _M0L5d__loS369;
    int32_t _M0L6_2atmpS2113;
    int32_t _M0L6_2atmpS2112;
    int32_t _M0L6_2atmpS2116;
    int32_t _M0L6_2atmpS2115;
    int32_t _M0L6_2atmpS2114;
    _M0Lm6offsetS349 = _M0L6_2atmpS2111 - 2;
    _M0L6_2atmpS2122 = _M0Lm9remainingS362;
    _M0L6_2atmpS2121 = _M0L6_2atmpS2122 / 10;
    _M0L6_2atmpS2120 = 48 + _M0L6_2atmpS2121;
    _M0L5d__hiS368 = (uint16_t)_M0L6_2atmpS2120;
    _M0L6_2atmpS2119 = _M0Lm9remainingS362;
    _M0L6_2atmpS2118 = _M0L6_2atmpS2119 % 10;
    _M0L6_2atmpS2117 = 48 + _M0L6_2atmpS2118;
    _M0L5d__loS369 = (uint16_t)_M0L6_2atmpS2117;
    _M0L6_2atmpS2113 = _M0Lm6offsetS349;
    _M0L6_2atmpS2112 = _M0L12digit__startS351 + _M0L6_2atmpS2113;
    _M0L6bufferS360[_M0L6_2atmpS2112] = _M0L5d__hiS368;
    _M0L6_2atmpS2116 = _M0Lm6offsetS349;
    _M0L6_2atmpS2115 = _M0L12digit__startS351 + _M0L6_2atmpS2116;
    _M0L6_2atmpS2114 = _M0L6_2atmpS2115 + 1;
    _M0L6bufferS360[_M0L6_2atmpS2114] = _M0L5d__loS369;
    moonbit_decref(_M0L6bufferS360);
  } else {
    int32_t _M0L6_2atmpS2123 = _M0Lm6offsetS349;
    int32_t _M0L6_2atmpS2128;
    int32_t _M0L6_2atmpS2124;
    int32_t _M0L6_2atmpS2127;
    int32_t _M0L6_2atmpS2126;
    int32_t _M0L6_2atmpS2125;
    _M0Lm6offsetS349 = _M0L6_2atmpS2123 - 1;
    _M0L6_2atmpS2128 = _M0Lm6offsetS349;
    _M0L6_2atmpS2124 = _M0L12digit__startS351 + _M0L6_2atmpS2128;
    _M0L6_2atmpS2127 = _M0Lm9remainingS362;
    _M0L6_2atmpS2126 = 48 + _M0L6_2atmpS2127;
    _M0L6_2atmpS2125 = (uint16_t)_M0L6_2atmpS2126;
    _M0L6bufferS360[_M0L6_2atmpS2124] = _M0L6_2atmpS2125;
    moonbit_decref(_M0L6bufferS360);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS342,
  uint32_t _M0L3numS336,
  int32_t _M0L12digit__startS334,
  int32_t _M0L10total__lenS333,
  int32_t _M0L5radixS338
) {
  int32_t _M0Lm6offsetS332;
  uint32_t _M0Lm1nS335;
  uint32_t _M0L4baseS337;
  int32_t _M0L6_2atmpS2055;
  int32_t _M0L6_2atmpS2054;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS332 = _M0L10total__lenS333 - _M0L12digit__startS334;
  _M0Lm1nS335 = _M0L3numS336;
  _M0L4baseS337 = *(uint32_t*)&_M0L5radixS338;
  _M0L6_2atmpS2055 = _M0L5radixS338 - 1;
  _M0L6_2atmpS2054 = _M0L5radixS338 & _M0L6_2atmpS2055;
  if (_M0L6_2atmpS2054 == 0) {
    int32_t _M0L5shiftS339;
    uint32_t _M0L4maskS340;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS339 = moonbit_ctz32(_M0L5radixS338);
    _M0L4maskS340 = _M0L4baseS337 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS2056 = _M0Lm1nS335;
      if (_M0L6_2atmpS2056 > 0u) {
        int32_t _M0L6_2atmpS2057 = _M0Lm6offsetS332;
        uint32_t _M0L6_2atmpS2063;
        uint32_t _M0L6_2atmpS2062;
        int32_t _M0L5digitS341;
        int32_t _M0L6_2atmpS2060;
        int32_t _M0L6_2atmpS2058;
        int32_t _M0L6_2atmpS2059;
        uint32_t _M0L6_2atmpS2061;
        _M0Lm6offsetS332 = _M0L6_2atmpS2057 - 1;
        _M0L6_2atmpS2063 = _M0Lm1nS335;
        _M0L6_2atmpS2062 = _M0L6_2atmpS2063 & _M0L4maskS340;
        _M0L5digitS341 = *(int32_t*)&_M0L6_2atmpS2062;
        _M0L6_2atmpS2060 = _M0Lm6offsetS332;
        _M0L6_2atmpS2058 = _M0L12digit__startS334 + _M0L6_2atmpS2060;
        _M0L6_2atmpS2059
        = ((moonbit_string_t)moonbit_string_literal_120.data)[
          _M0L5digitS341
        ];
        _M0L6bufferS342[_M0L6_2atmpS2058] = _M0L6_2atmpS2059;
        _M0L6_2atmpS2061 = _M0Lm1nS335;
        _M0Lm1nS335 = _M0L6_2atmpS2061 >> (_M0L5shiftS339 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS342);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS2064 = _M0Lm1nS335;
      if (_M0L6_2atmpS2064 > 0u) {
        int32_t _M0L6_2atmpS2065 = _M0Lm6offsetS332;
        uint32_t _M0L6_2atmpS2072;
        uint32_t _M0L1qS344;
        uint32_t _M0L6_2atmpS2070;
        uint32_t _M0L6_2atmpS2071;
        uint32_t _M0L6_2atmpS2069;
        int32_t _M0L5digitS345;
        int32_t _M0L6_2atmpS2068;
        int32_t _M0L6_2atmpS2066;
        int32_t _M0L6_2atmpS2067;
        _M0Lm6offsetS332 = _M0L6_2atmpS2065 - 1;
        _M0L6_2atmpS2072 = _M0Lm1nS335;
        _M0L1qS344 = _M0L6_2atmpS2072 / _M0L4baseS337;
        _M0L6_2atmpS2070 = _M0Lm1nS335;
        _M0L6_2atmpS2071 = _M0L1qS344 * _M0L4baseS337;
        _M0L6_2atmpS2069 = _M0L6_2atmpS2070 - _M0L6_2atmpS2071;
        _M0L5digitS345 = *(int32_t*)&_M0L6_2atmpS2069;
        _M0L6_2atmpS2068 = _M0Lm6offsetS332;
        _M0L6_2atmpS2066 = _M0L12digit__startS334 + _M0L6_2atmpS2068;
        _M0L6_2atmpS2067
        = ((moonbit_string_t)moonbit_string_literal_120.data)[
          _M0L5digitS345
        ];
        _M0L6bufferS342[_M0L6_2atmpS2066] = _M0L6_2atmpS2067;
        _M0Lm1nS335 = _M0L1qS344;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS342);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS329,
  uint32_t _M0L3numS325,
  int32_t _M0L12digit__startS323,
  int32_t _M0L10total__lenS322
) {
  int32_t _M0Lm6offsetS321;
  uint32_t _M0Lm1nS324;
  int32_t _M0L6_2atmpS2050;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS321 = _M0L10total__lenS322 - _M0L12digit__startS323;
  _M0Lm1nS324 = _M0L3numS325;
  while (1) {
    int32_t _M0L6_2atmpS2038 = _M0Lm6offsetS321;
    if (_M0L6_2atmpS2038 >= 2) {
      int32_t _M0L6_2atmpS2039 = _M0Lm6offsetS321;
      uint32_t _M0L6_2atmpS2049;
      uint32_t _M0L6_2atmpS2048;
      int32_t _M0L9byte__valS326;
      int32_t _M0L2hiS327;
      int32_t _M0L2loS328;
      int32_t _M0L6_2atmpS2042;
      int32_t _M0L6_2atmpS2040;
      int32_t _M0L6_2atmpS2041;
      int32_t _M0L6_2atmpS2046;
      int32_t _M0L6_2atmpS2045;
      int32_t _M0L6_2atmpS2043;
      int32_t _M0L6_2atmpS2044;
      uint32_t _M0L6_2atmpS2047;
      _M0Lm6offsetS321 = _M0L6_2atmpS2039 - 2;
      _M0L6_2atmpS2049 = _M0Lm1nS324;
      _M0L6_2atmpS2048 = _M0L6_2atmpS2049 & 255u;
      _M0L9byte__valS326 = *(int32_t*)&_M0L6_2atmpS2048;
      _M0L2hiS327 = _M0L9byte__valS326 / 16;
      _M0L2loS328 = _M0L9byte__valS326 % 16;
      _M0L6_2atmpS2042 = _M0Lm6offsetS321;
      _M0L6_2atmpS2040 = _M0L12digit__startS323 + _M0L6_2atmpS2042;
      _M0L6_2atmpS2041
      = ((moonbit_string_t)moonbit_string_literal_120.data)[
        _M0L2hiS327
      ];
      _M0L6bufferS329[_M0L6_2atmpS2040] = _M0L6_2atmpS2041;
      _M0L6_2atmpS2046 = _M0Lm6offsetS321;
      _M0L6_2atmpS2045 = _M0L12digit__startS323 + _M0L6_2atmpS2046;
      _M0L6_2atmpS2043 = _M0L6_2atmpS2045 + 1;
      _M0L6_2atmpS2044
      = ((moonbit_string_t)moonbit_string_literal_120.data)[
        _M0L2loS328
      ];
      _M0L6bufferS329[_M0L6_2atmpS2043] = _M0L6_2atmpS2044;
      _M0L6_2atmpS2047 = _M0Lm1nS324;
      _M0Lm1nS324 = _M0L6_2atmpS2047 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2050 = _M0Lm6offsetS321;
  if (_M0L6_2atmpS2050 == 1) {
    uint32_t _M0L6_2atmpS2053 = _M0Lm1nS324;
    uint32_t _M0L6_2atmpS2052 = _M0L6_2atmpS2053 & 15u;
    int32_t _M0L6nibbleS331 = *(int32_t*)&_M0L6_2atmpS2052;
    int32_t _M0L6_2atmpS2051 =
      ((moonbit_string_t)moonbit_string_literal_120.data)[_M0L6nibbleS331];
    _M0L6bufferS329[_M0L12digit__startS323] = _M0L6_2atmpS2051;
    moonbit_decref(_M0L6bufferS329);
  } else {
    moonbit_decref(_M0L6bufferS329);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS312) {
  struct _M0TWEOs* _M0L7_2afuncS311;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS311 = _M0L4selfS312;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS311->code(_M0L7_2afuncS311);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS314
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS313;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS313 = _M0L4selfS314;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS313->code(_M0L7_2afuncS313);
}

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0MPB4Iter4nextGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L4selfS316
) {
  struct _M0TWEORP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L7_2afuncS315;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS315 = _M0L4selfS316;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS315->code(_M0L7_2afuncS315);
}

void* _M0MPB4Iter4nextGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS318
) {
  struct _M0TWERPC16option6OptionGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L7_2afuncS317;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS317 = _M0L4selfS318;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS317->code(_M0L7_2afuncS317);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS320) {
  struct _M0TWEOc* _M0L7_2afuncS319;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS319 = _M0L4selfS320;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS319->code(_M0L7_2afuncS319);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS304
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS303;
  struct _M0TPB6Logger _M0L6_2atmpS2034;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS303 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS303);
  _M0L6_2atmpS2034
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS303
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS304, _M0L6_2atmpS2034);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS303);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS306
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS305;
  struct _M0TPB6Logger _M0L6_2atmpS2035;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS305 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS305);
  _M0L6_2atmpS2035
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS305
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS306, _M0L6_2atmpS2035);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS305);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS308
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS307;
  struct _M0TPB6Logger _M0L6_2atmpS2036;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS307 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS307);
  _M0L6_2atmpS2036
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS307
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS308, _M0L6_2atmpS2036);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS307);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS310
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS309;
  struct _M0TPB6Logger _M0L6_2atmpS2037;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS309 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS309);
  _M0L6_2atmpS2037
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS309
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS310, _M0L6_2atmpS2037);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS309);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS302
) {
  int32_t _M0L8_2afieldS4140;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4140 = _M0L4selfS302.$1;
  moonbit_decref(_M0L4selfS302.$0);
  return _M0L8_2afieldS4140;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS301
) {
  int32_t _M0L3endS2032;
  int32_t _M0L8_2afieldS4141;
  int32_t _M0L5startS2033;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS2032 = _M0L4selfS301.$2;
  _M0L8_2afieldS4141 = _M0L4selfS301.$1;
  moonbit_decref(_M0L4selfS301.$0);
  _M0L5startS2033 = _M0L8_2afieldS4141;
  return _M0L3endS2032 - _M0L5startS2033;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS300
) {
  moonbit_string_t _M0L8_2afieldS4142;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4142 = _M0L4selfS300.$0;
  return _M0L8_2afieldS4142;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS296,
  moonbit_string_t _M0L5valueS297,
  int32_t _M0L5startS298,
  int32_t _M0L3lenS299
) {
  int32_t _M0L6_2atmpS2031;
  int64_t _M0L6_2atmpS2030;
  struct _M0TPC16string10StringView _M0L6_2atmpS2029;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2031 = _M0L5startS298 + _M0L3lenS299;
  _M0L6_2atmpS2030 = (int64_t)_M0L6_2atmpS2031;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2029
  = _M0MPC16string6String11sub_2einner(_M0L5valueS297, _M0L5startS298, _M0L6_2atmpS2030);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS296, _M0L6_2atmpS2029);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS289,
  int32_t _M0L5startS295,
  int64_t _M0L3endS291
) {
  int32_t _M0L3lenS288;
  int32_t _M0L3endS290;
  int32_t _M0L5startS294;
  int32_t _if__result_4573;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS288 = Moonbit_array_length(_M0L4selfS289);
  if (_M0L3endS291 == 4294967296ll) {
    _M0L3endS290 = _M0L3lenS288;
  } else {
    int64_t _M0L7_2aSomeS292 = _M0L3endS291;
    int32_t _M0L6_2aendS293 = (int32_t)_M0L7_2aSomeS292;
    if (_M0L6_2aendS293 < 0) {
      _M0L3endS290 = _M0L3lenS288 + _M0L6_2aendS293;
    } else {
      _M0L3endS290 = _M0L6_2aendS293;
    }
  }
  if (_M0L5startS295 < 0) {
    _M0L5startS294 = _M0L3lenS288 + _M0L5startS295;
  } else {
    _M0L5startS294 = _M0L5startS295;
  }
  if (_M0L5startS294 >= 0) {
    if (_M0L5startS294 <= _M0L3endS290) {
      _if__result_4573 = _M0L3endS290 <= _M0L3lenS288;
    } else {
      _if__result_4573 = 0;
    }
  } else {
    _if__result_4573 = 0;
  }
  if (_if__result_4573) {
    if (_M0L5startS294 < _M0L3lenS288) {
      int32_t _M0L6_2atmpS2026 = _M0L4selfS289[_M0L5startS294];
      int32_t _M0L6_2atmpS2025;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS2025
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2026);
      if (!_M0L6_2atmpS2025) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS290 < _M0L3lenS288) {
      int32_t _M0L6_2atmpS2028 = _M0L4selfS289[_M0L3endS290];
      int32_t _M0L6_2atmpS2027;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS2027
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS2028);
      if (!_M0L6_2atmpS2027) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS294,
                                                 _M0L3endS290,
                                                 _M0L4selfS289};
  } else {
    moonbit_decref(_M0L4selfS289);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS285) {
  struct _M0TPB6Hasher* _M0L1hS284;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS284 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS284);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS284, _M0L4selfS285);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS284);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS287
) {
  struct _M0TPB6Hasher* _M0L1hS286;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS286 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS286);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS286, _M0L4selfS287);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS286);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS282) {
  int32_t _M0L4seedS281;
  if (_M0L10seed_2eoptS282 == 4294967296ll) {
    _M0L4seedS281 = 0;
  } else {
    int64_t _M0L7_2aSomeS283 = _M0L10seed_2eoptS282;
    _M0L4seedS281 = (int32_t)_M0L7_2aSomeS283;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS281);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS280) {
  uint32_t _M0L6_2atmpS2024;
  uint32_t _M0L6_2atmpS2023;
  struct _M0TPB6Hasher* _block_4574;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS2024 = *(uint32_t*)&_M0L4seedS280;
  _M0L6_2atmpS2023 = _M0L6_2atmpS2024 + 374761393u;
  _block_4574
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4574)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4574->$0 = _M0L6_2atmpS2023;
  return _block_4574;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS279) {
  uint32_t _M0L6_2atmpS2022;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS2022 = _M0MPB6Hasher9avalanche(_M0L4selfS279);
  return *(int32_t*)&_M0L6_2atmpS2022;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS278) {
  uint32_t _M0L8_2afieldS4143;
  uint32_t _M0Lm3accS277;
  uint32_t _M0L6_2atmpS2011;
  uint32_t _M0L6_2atmpS2013;
  uint32_t _M0L6_2atmpS2012;
  uint32_t _M0L6_2atmpS2014;
  uint32_t _M0L6_2atmpS2015;
  uint32_t _M0L6_2atmpS2017;
  uint32_t _M0L6_2atmpS2016;
  uint32_t _M0L6_2atmpS2018;
  uint32_t _M0L6_2atmpS2019;
  uint32_t _M0L6_2atmpS2021;
  uint32_t _M0L6_2atmpS2020;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS4143 = _M0L4selfS278->$0;
  moonbit_decref(_M0L4selfS278);
  _M0Lm3accS277 = _M0L8_2afieldS4143;
  _M0L6_2atmpS2011 = _M0Lm3accS277;
  _M0L6_2atmpS2013 = _M0Lm3accS277;
  _M0L6_2atmpS2012 = _M0L6_2atmpS2013 >> 15;
  _M0Lm3accS277 = _M0L6_2atmpS2011 ^ _M0L6_2atmpS2012;
  _M0L6_2atmpS2014 = _M0Lm3accS277;
  _M0Lm3accS277 = _M0L6_2atmpS2014 * 2246822519u;
  _M0L6_2atmpS2015 = _M0Lm3accS277;
  _M0L6_2atmpS2017 = _M0Lm3accS277;
  _M0L6_2atmpS2016 = _M0L6_2atmpS2017 >> 13;
  _M0Lm3accS277 = _M0L6_2atmpS2015 ^ _M0L6_2atmpS2016;
  _M0L6_2atmpS2018 = _M0Lm3accS277;
  _M0Lm3accS277 = _M0L6_2atmpS2018 * 3266489917u;
  _M0L6_2atmpS2019 = _M0Lm3accS277;
  _M0L6_2atmpS2021 = _M0Lm3accS277;
  _M0L6_2atmpS2020 = _M0L6_2atmpS2021 >> 16;
  _M0Lm3accS277 = _M0L6_2atmpS2019 ^ _M0L6_2atmpS2020;
  return _M0Lm3accS277;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS275,
  moonbit_string_t _M0L1yS276
) {
  int32_t _M0L6_2atmpS4144;
  int32_t _M0L6_2atmpS2010;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS4144 = moonbit_val_array_equal(_M0L1xS275, _M0L1yS276);
  moonbit_decref(_M0L1xS275);
  moonbit_decref(_M0L1yS276);
  _M0L6_2atmpS2010 = _M0L6_2atmpS4144;
  return !_M0L6_2atmpS2010;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS272,
  int32_t _M0L5valueS271
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS271, _M0L4selfS272);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS274,
  moonbit_string_t _M0L5valueS273
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS273, _M0L4selfS274);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS270) {
  int64_t _M0L6_2atmpS2009;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2009 = (int64_t)_M0L4selfS270;
  return *(uint64_t*)&_M0L6_2atmpS2009;
}

void* _M0MPC14json4Json6number(
  double _M0L6numberS268,
  moonbit_string_t _M0L4reprS269
) {
  void* _block_4575;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4575 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6Number));
  Moonbit_object_header(_block_4575)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6Number, $1) >> 2, 1, 3);
  ((struct _M0DTPB4Json6Number*)_block_4575)->$0 = _M0L6numberS268;
  ((struct _M0DTPB4Json6Number*)_block_4575)->$1 = _M0L4reprS269;
  return _block_4575;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS266,
  int32_t _M0L5valueS267
) {
  uint32_t _M0L6_2atmpS2008;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS2008 = *(uint32_t*)&_M0L5valueS267;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS266, _M0L6_2atmpS2008);
  return 0;
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS259
) {
  struct _M0TPB13StringBuilder* _M0L3bufS257;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS258;
  int32_t _M0L7_2abindS260;
  int32_t _M0L1iS261;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS257 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS258 = _M0L4selfS259;
  moonbit_incref(_M0L3bufS257);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS257, 91);
  _M0L7_2abindS260 = _M0L7_2aselfS258->$1;
  _M0L1iS261 = 0;
  while (1) {
    if (_M0L1iS261 < _M0L7_2abindS260) {
      int32_t _if__result_4577;
      moonbit_string_t* _M0L8_2afieldS4146;
      moonbit_string_t* _M0L3bufS2006;
      moonbit_string_t _M0L6_2atmpS4145;
      moonbit_string_t _M0L4itemS262;
      int32_t _M0L6_2atmpS2007;
      if (_M0L1iS261 != 0) {
        moonbit_incref(_M0L3bufS257);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS257, (moonbit_string_t)moonbit_string_literal_122.data);
      }
      if (_M0L1iS261 < 0) {
        _if__result_4577 = 1;
      } else {
        int32_t _M0L3lenS2005 = _M0L7_2aselfS258->$1;
        _if__result_4577 = _M0L1iS261 >= _M0L3lenS2005;
      }
      if (_if__result_4577) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS4146 = _M0L7_2aselfS258->$0;
      _M0L3bufS2006 = _M0L8_2afieldS4146;
      _M0L6_2atmpS4145 = (moonbit_string_t)_M0L3bufS2006[_M0L1iS261];
      _M0L4itemS262 = _M0L6_2atmpS4145;
      if (_M0L4itemS262 == 0) {
        moonbit_incref(_M0L3bufS257);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS257, (moonbit_string_t)moonbit_string_literal_83.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS263 = _M0L4itemS262;
        moonbit_string_t _M0L6_2alocS264 = _M0L7_2aSomeS263;
        moonbit_string_t _M0L6_2atmpS2004;
        moonbit_incref(_M0L6_2alocS264);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS2004
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS264);
        moonbit_incref(_M0L3bufS257);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS257, _M0L6_2atmpS2004);
      }
      _M0L6_2atmpS2007 = _M0L1iS261 + 1;
      _M0L1iS261 = _M0L6_2atmpS2007;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS258);
    }
    break;
  }
  moonbit_incref(_M0L3bufS257);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS257, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS257);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS256
) {
  moonbit_string_t _M0L6_2atmpS2003;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2002;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2003 = _M0L4selfS256;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2002 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2003);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS2002);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS255
) {
  struct _M0TPB13StringBuilder* _M0L2sbS254;
  struct _M0TPC16string10StringView _M0L8_2afieldS4159;
  struct _M0TPC16string10StringView _M0L3pkgS1987;
  moonbit_string_t _M0L6_2atmpS1986;
  moonbit_string_t _M0L6_2atmpS4158;
  moonbit_string_t _M0L6_2atmpS1985;
  moonbit_string_t _M0L6_2atmpS4157;
  moonbit_string_t _M0L6_2atmpS1984;
  struct _M0TPC16string10StringView _M0L8_2afieldS4156;
  struct _M0TPC16string10StringView _M0L8filenameS1988;
  struct _M0TPC16string10StringView _M0L8_2afieldS4155;
  struct _M0TPC16string10StringView _M0L11start__lineS1991;
  moonbit_string_t _M0L6_2atmpS1990;
  moonbit_string_t _M0L6_2atmpS4154;
  moonbit_string_t _M0L6_2atmpS1989;
  struct _M0TPC16string10StringView _M0L8_2afieldS4153;
  struct _M0TPC16string10StringView _M0L13start__columnS1994;
  moonbit_string_t _M0L6_2atmpS1993;
  moonbit_string_t _M0L6_2atmpS4152;
  moonbit_string_t _M0L6_2atmpS1992;
  struct _M0TPC16string10StringView _M0L8_2afieldS4151;
  struct _M0TPC16string10StringView _M0L9end__lineS1997;
  moonbit_string_t _M0L6_2atmpS1996;
  moonbit_string_t _M0L6_2atmpS4150;
  moonbit_string_t _M0L6_2atmpS1995;
  struct _M0TPC16string10StringView _M0L8_2afieldS4149;
  int32_t _M0L6_2acntS4362;
  struct _M0TPC16string10StringView _M0L11end__columnS2001;
  moonbit_string_t _M0L6_2atmpS2000;
  moonbit_string_t _M0L6_2atmpS4148;
  moonbit_string_t _M0L6_2atmpS1999;
  moonbit_string_t _M0L6_2atmpS4147;
  moonbit_string_t _M0L6_2atmpS1998;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS254 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS4159
  = (struct _M0TPC16string10StringView){
    _M0L4selfS255->$0_1, _M0L4selfS255->$0_2, _M0L4selfS255->$0_0
  };
  _M0L3pkgS1987 = _M0L8_2afieldS4159;
  moonbit_incref(_M0L3pkgS1987.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1986
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1987);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4158
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_123.data, _M0L6_2atmpS1986);
  moonbit_decref(_M0L6_2atmpS1986);
  _M0L6_2atmpS1985 = _M0L6_2atmpS4158;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4157
  = moonbit_add_string(_M0L6_2atmpS1985, (moonbit_string_t)moonbit_string_literal_124.data);
  moonbit_decref(_M0L6_2atmpS1985);
  _M0L6_2atmpS1984 = _M0L6_2atmpS4157;
  moonbit_incref(_M0L2sbS254);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS254, _M0L6_2atmpS1984);
  moonbit_incref(_M0L2sbS254);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS254, (moonbit_string_t)moonbit_string_literal_125.data);
  _M0L8_2afieldS4156
  = (struct _M0TPC16string10StringView){
    _M0L4selfS255->$1_1, _M0L4selfS255->$1_2, _M0L4selfS255->$1_0
  };
  _M0L8filenameS1988 = _M0L8_2afieldS4156;
  moonbit_incref(_M0L8filenameS1988.$0);
  moonbit_incref(_M0L2sbS254);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS254, _M0L8filenameS1988);
  _M0L8_2afieldS4155
  = (struct _M0TPC16string10StringView){
    _M0L4selfS255->$2_1, _M0L4selfS255->$2_2, _M0L4selfS255->$2_0
  };
  _M0L11start__lineS1991 = _M0L8_2afieldS4155;
  moonbit_incref(_M0L11start__lineS1991.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1990
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1991);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4154
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_126.data, _M0L6_2atmpS1990);
  moonbit_decref(_M0L6_2atmpS1990);
  _M0L6_2atmpS1989 = _M0L6_2atmpS4154;
  moonbit_incref(_M0L2sbS254);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS254, _M0L6_2atmpS1989);
  _M0L8_2afieldS4153
  = (struct _M0TPC16string10StringView){
    _M0L4selfS255->$3_1, _M0L4selfS255->$3_2, _M0L4selfS255->$3_0
  };
  _M0L13start__columnS1994 = _M0L8_2afieldS4153;
  moonbit_incref(_M0L13start__columnS1994.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1993
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1994);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4152
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_127.data, _M0L6_2atmpS1993);
  moonbit_decref(_M0L6_2atmpS1993);
  _M0L6_2atmpS1992 = _M0L6_2atmpS4152;
  moonbit_incref(_M0L2sbS254);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS254, _M0L6_2atmpS1992);
  _M0L8_2afieldS4151
  = (struct _M0TPC16string10StringView){
    _M0L4selfS255->$4_1, _M0L4selfS255->$4_2, _M0L4selfS255->$4_0
  };
  _M0L9end__lineS1997 = _M0L8_2afieldS4151;
  moonbit_incref(_M0L9end__lineS1997.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1996
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1997);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4150
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_128.data, _M0L6_2atmpS1996);
  moonbit_decref(_M0L6_2atmpS1996);
  _M0L6_2atmpS1995 = _M0L6_2atmpS4150;
  moonbit_incref(_M0L2sbS254);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS254, _M0L6_2atmpS1995);
  _M0L8_2afieldS4149
  = (struct _M0TPC16string10StringView){
    _M0L4selfS255->$5_1, _M0L4selfS255->$5_2, _M0L4selfS255->$5_0
  };
  _M0L6_2acntS4362 = Moonbit_object_header(_M0L4selfS255)->rc;
  if (_M0L6_2acntS4362 > 1) {
    int32_t _M0L11_2anew__cntS4368 = _M0L6_2acntS4362 - 1;
    Moonbit_object_header(_M0L4selfS255)->rc = _M0L11_2anew__cntS4368;
    moonbit_incref(_M0L8_2afieldS4149.$0);
  } else if (_M0L6_2acntS4362 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4367 =
      (struct _M0TPC16string10StringView){_M0L4selfS255->$4_1,
                                            _M0L4selfS255->$4_2,
                                            _M0L4selfS255->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4366;
    struct _M0TPC16string10StringView _M0L8_2afieldS4365;
    struct _M0TPC16string10StringView _M0L8_2afieldS4364;
    struct _M0TPC16string10StringView _M0L8_2afieldS4363;
    moonbit_decref(_M0L8_2afieldS4367.$0);
    _M0L8_2afieldS4366
    = (struct _M0TPC16string10StringView){
      _M0L4selfS255->$3_1, _M0L4selfS255->$3_2, _M0L4selfS255->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4366.$0);
    _M0L8_2afieldS4365
    = (struct _M0TPC16string10StringView){
      _M0L4selfS255->$2_1, _M0L4selfS255->$2_2, _M0L4selfS255->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4365.$0);
    _M0L8_2afieldS4364
    = (struct _M0TPC16string10StringView){
      _M0L4selfS255->$1_1, _M0L4selfS255->$1_2, _M0L4selfS255->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4364.$0);
    _M0L8_2afieldS4363
    = (struct _M0TPC16string10StringView){
      _M0L4selfS255->$0_1, _M0L4selfS255->$0_2, _M0L4selfS255->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4363.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS255);
  }
  _M0L11end__columnS2001 = _M0L8_2afieldS4149;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2000
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS2001);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4148
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_129.data, _M0L6_2atmpS2000);
  moonbit_decref(_M0L6_2atmpS2000);
  _M0L6_2atmpS1999 = _M0L6_2atmpS4148;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4147
  = moonbit_add_string(_M0L6_2atmpS1999, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1999);
  _M0L6_2atmpS1998 = _M0L6_2atmpS4147;
  moonbit_incref(_M0L2sbS254);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS254, _M0L6_2atmpS1998);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS254);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS252,
  moonbit_string_t _M0L3strS253
) {
  int32_t _M0L3lenS1974;
  int32_t _M0L6_2atmpS1976;
  int32_t _M0L6_2atmpS1975;
  int32_t _M0L6_2atmpS1973;
  moonbit_bytes_t _M0L8_2afieldS4161;
  moonbit_bytes_t _M0L4dataS1977;
  int32_t _M0L3lenS1978;
  int32_t _M0L6_2atmpS1979;
  int32_t _M0L3lenS1981;
  int32_t _M0L6_2atmpS4160;
  int32_t _M0L6_2atmpS1983;
  int32_t _M0L6_2atmpS1982;
  int32_t _M0L6_2atmpS1980;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1974 = _M0L4selfS252->$1;
  _M0L6_2atmpS1976 = Moonbit_array_length(_M0L3strS253);
  _M0L6_2atmpS1975 = _M0L6_2atmpS1976 * 2;
  _M0L6_2atmpS1973 = _M0L3lenS1974 + _M0L6_2atmpS1975;
  moonbit_incref(_M0L4selfS252);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS252, _M0L6_2atmpS1973);
  _M0L8_2afieldS4161 = _M0L4selfS252->$0;
  _M0L4dataS1977 = _M0L8_2afieldS4161;
  _M0L3lenS1978 = _M0L4selfS252->$1;
  _M0L6_2atmpS1979 = Moonbit_array_length(_M0L3strS253);
  moonbit_incref(_M0L4dataS1977);
  moonbit_incref(_M0L3strS253);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1977, _M0L3lenS1978, _M0L3strS253, 0, _M0L6_2atmpS1979);
  _M0L3lenS1981 = _M0L4selfS252->$1;
  _M0L6_2atmpS4160 = Moonbit_array_length(_M0L3strS253);
  moonbit_decref(_M0L3strS253);
  _M0L6_2atmpS1983 = _M0L6_2atmpS4160;
  _M0L6_2atmpS1982 = _M0L6_2atmpS1983 * 2;
  _M0L6_2atmpS1980 = _M0L3lenS1981 + _M0L6_2atmpS1982;
  _M0L4selfS252->$1 = _M0L6_2atmpS1980;
  moonbit_decref(_M0L4selfS252);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS244,
  int32_t _M0L13bytes__offsetS239,
  moonbit_string_t _M0L3strS246,
  int32_t _M0L11str__offsetS242,
  int32_t _M0L6lengthS240
) {
  int32_t _M0L6_2atmpS1972;
  int32_t _M0L6_2atmpS1971;
  int32_t _M0L2e1S238;
  int32_t _M0L6_2atmpS1970;
  int32_t _M0L2e2S241;
  int32_t _M0L4len1S243;
  int32_t _M0L4len2S245;
  int32_t _if__result_4578;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1972 = _M0L6lengthS240 * 2;
  _M0L6_2atmpS1971 = _M0L13bytes__offsetS239 + _M0L6_2atmpS1972;
  _M0L2e1S238 = _M0L6_2atmpS1971 - 1;
  _M0L6_2atmpS1970 = _M0L11str__offsetS242 + _M0L6lengthS240;
  _M0L2e2S241 = _M0L6_2atmpS1970 - 1;
  _M0L4len1S243 = Moonbit_array_length(_M0L4selfS244);
  _M0L4len2S245 = Moonbit_array_length(_M0L3strS246);
  if (_M0L6lengthS240 >= 0) {
    if (_M0L13bytes__offsetS239 >= 0) {
      if (_M0L2e1S238 < _M0L4len1S243) {
        if (_M0L11str__offsetS242 >= 0) {
          _if__result_4578 = _M0L2e2S241 < _M0L4len2S245;
        } else {
          _if__result_4578 = 0;
        }
      } else {
        _if__result_4578 = 0;
      }
    } else {
      _if__result_4578 = 0;
    }
  } else {
    _if__result_4578 = 0;
  }
  if (_if__result_4578) {
    int32_t _M0L16end__str__offsetS247 =
      _M0L11str__offsetS242 + _M0L6lengthS240;
    int32_t _M0L1iS248 = _M0L11str__offsetS242;
    int32_t _M0L1jS249 = _M0L13bytes__offsetS239;
    while (1) {
      if (_M0L1iS248 < _M0L16end__str__offsetS247) {
        int32_t _M0L6_2atmpS1967 = _M0L3strS246[_M0L1iS248];
        int32_t _M0L6_2atmpS1966 = (int32_t)_M0L6_2atmpS1967;
        uint32_t _M0L1cS250 = *(uint32_t*)&_M0L6_2atmpS1966;
        uint32_t _M0L6_2atmpS1962 = _M0L1cS250 & 255u;
        int32_t _M0L6_2atmpS1961;
        int32_t _M0L6_2atmpS1963;
        uint32_t _M0L6_2atmpS1965;
        int32_t _M0L6_2atmpS1964;
        int32_t _M0L6_2atmpS1968;
        int32_t _M0L6_2atmpS1969;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1961 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1962);
        if (
          _M0L1jS249 < 0 || _M0L1jS249 >= Moonbit_array_length(_M0L4selfS244)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS244[_M0L1jS249] = _M0L6_2atmpS1961;
        _M0L6_2atmpS1963 = _M0L1jS249 + 1;
        _M0L6_2atmpS1965 = _M0L1cS250 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1964 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1965);
        if (
          _M0L6_2atmpS1963 < 0
          || _M0L6_2atmpS1963 >= Moonbit_array_length(_M0L4selfS244)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS244[_M0L6_2atmpS1963] = _M0L6_2atmpS1964;
        _M0L6_2atmpS1968 = _M0L1iS248 + 1;
        _M0L6_2atmpS1969 = _M0L1jS249 + 2;
        _M0L1iS248 = _M0L6_2atmpS1968;
        _M0L1jS249 = _M0L6_2atmpS1969;
        continue;
      } else {
        moonbit_decref(_M0L3strS246);
        moonbit_decref(_M0L4selfS244);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS246);
    moonbit_decref(_M0L4selfS244);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGdE(
  struct _M0TPB13StringBuilder* _M0L4selfS235,
  double _M0L3objS234
) {
  struct _M0TPB6Logger _M0L6_2atmpS1959;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1959
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS235
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS234, _M0L6_2atmpS1959);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS237,
  struct _M0TPC16string10StringView _M0L3objS236
) {
  struct _M0TPB6Logger _M0L6_2atmpS1960;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1960
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS237
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS236, _M0L6_2atmpS1960);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS180
) {
  int32_t _M0L6_2atmpS1958;
  struct _M0TPC16string10StringView _M0L7_2abindS179;
  moonbit_string_t _M0L7_2adataS181;
  int32_t _M0L8_2astartS182;
  int32_t _M0L6_2atmpS1957;
  int32_t _M0L6_2aendS183;
  int32_t _M0Lm9_2acursorS184;
  int32_t _M0Lm13accept__stateS185;
  int32_t _M0Lm10match__endS186;
  int32_t _M0Lm20match__tag__saver__0S187;
  int32_t _M0Lm20match__tag__saver__1S188;
  int32_t _M0Lm20match__tag__saver__2S189;
  int32_t _M0Lm20match__tag__saver__3S190;
  int32_t _M0Lm20match__tag__saver__4S191;
  int32_t _M0Lm6tag__0S192;
  int32_t _M0Lm6tag__1S193;
  int32_t _M0Lm9tag__1__1S194;
  int32_t _M0Lm9tag__1__2S195;
  int32_t _M0Lm6tag__3S196;
  int32_t _M0Lm6tag__2S197;
  int32_t _M0Lm9tag__2__1S198;
  int32_t _M0Lm6tag__4S199;
  int32_t _M0L6_2atmpS1915;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1958 = Moonbit_array_length(_M0L4reprS180);
  _M0L7_2abindS179
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1958, _M0L4reprS180
  };
  moonbit_incref(_M0L7_2abindS179.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS181 = _M0MPC16string10StringView4data(_M0L7_2abindS179);
  moonbit_incref(_M0L7_2abindS179.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS182
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS179);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1957 = _M0MPC16string10StringView6length(_M0L7_2abindS179);
  _M0L6_2aendS183 = _M0L8_2astartS182 + _M0L6_2atmpS1957;
  _M0Lm9_2acursorS184 = _M0L8_2astartS182;
  _M0Lm13accept__stateS185 = -1;
  _M0Lm10match__endS186 = -1;
  _M0Lm20match__tag__saver__0S187 = -1;
  _M0Lm20match__tag__saver__1S188 = -1;
  _M0Lm20match__tag__saver__2S189 = -1;
  _M0Lm20match__tag__saver__3S190 = -1;
  _M0Lm20match__tag__saver__4S191 = -1;
  _M0Lm6tag__0S192 = -1;
  _M0Lm6tag__1S193 = -1;
  _M0Lm9tag__1__1S194 = -1;
  _M0Lm9tag__1__2S195 = -1;
  _M0Lm6tag__3S196 = -1;
  _M0Lm6tag__2S197 = -1;
  _M0Lm9tag__2__1S198 = -1;
  _M0Lm6tag__4S199 = -1;
  _M0L6_2atmpS1915 = _M0Lm9_2acursorS184;
  if (_M0L6_2atmpS1915 < _M0L6_2aendS183) {
    int32_t _M0L6_2atmpS1917 = _M0Lm9_2acursorS184;
    int32_t _M0L6_2atmpS1916;
    moonbit_incref(_M0L7_2adataS181);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1916
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1917);
    if (_M0L6_2atmpS1916 == 64) {
      int32_t _M0L6_2atmpS1918 = _M0Lm9_2acursorS184;
      _M0Lm9_2acursorS184 = _M0L6_2atmpS1918 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1919;
        _M0Lm6tag__0S192 = _M0Lm9_2acursorS184;
        _M0L6_2atmpS1919 = _M0Lm9_2acursorS184;
        if (_M0L6_2atmpS1919 < _M0L6_2aendS183) {
          int32_t _M0L6_2atmpS1956 = _M0Lm9_2acursorS184;
          int32_t _M0L10next__charS207;
          int32_t _M0L6_2atmpS1920;
          moonbit_incref(_M0L7_2adataS181);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS207
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1956);
          _M0L6_2atmpS1920 = _M0Lm9_2acursorS184;
          _M0Lm9_2acursorS184 = _M0L6_2atmpS1920 + 1;
          if (_M0L10next__charS207 == 58) {
            int32_t _M0L6_2atmpS1921 = _M0Lm9_2acursorS184;
            if (_M0L6_2atmpS1921 < _M0L6_2aendS183) {
              int32_t _M0L6_2atmpS1922 = _M0Lm9_2acursorS184;
              int32_t _M0L12dispatch__15S208;
              _M0Lm9_2acursorS184 = _M0L6_2atmpS1922 + 1;
              _M0L12dispatch__15S208 = 0;
              loop__label__15_211:;
              while (1) {
                int32_t _M0L6_2atmpS1923;
                switch (_M0L12dispatch__15S208) {
                  case 3: {
                    int32_t _M0L6_2atmpS1926;
                    _M0Lm9tag__1__2S195 = _M0Lm9tag__1__1S194;
                    _M0Lm9tag__1__1S194 = _M0Lm6tag__1S193;
                    _M0Lm6tag__1S193 = _M0Lm9_2acursorS184;
                    _M0L6_2atmpS1926 = _M0Lm9_2acursorS184;
                    if (_M0L6_2atmpS1926 < _M0L6_2aendS183) {
                      int32_t _M0L6_2atmpS1931 = _M0Lm9_2acursorS184;
                      int32_t _M0L10next__charS215;
                      int32_t _M0L6_2atmpS1927;
                      moonbit_incref(_M0L7_2adataS181);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS215
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1931);
                      _M0L6_2atmpS1927 = _M0Lm9_2acursorS184;
                      _M0Lm9_2acursorS184 = _M0L6_2atmpS1927 + 1;
                      if (_M0L10next__charS215 < 58) {
                        if (_M0L10next__charS215 < 48) {
                          goto join_214;
                        } else {
                          int32_t _M0L6_2atmpS1928;
                          _M0Lm6tag__1S193 = _M0Lm9_2acursorS184;
                          _M0Lm9tag__2__1S198 = _M0Lm6tag__2S197;
                          _M0Lm6tag__2S197 = _M0Lm9_2acursorS184;
                          _M0Lm6tag__3S196 = _M0Lm9_2acursorS184;
                          _M0L6_2atmpS1928 = _M0Lm9_2acursorS184;
                          if (_M0L6_2atmpS1928 < _M0L6_2aendS183) {
                            int32_t _M0L6_2atmpS1930 = _M0Lm9_2acursorS184;
                            int32_t _M0L10next__charS217;
                            int32_t _M0L6_2atmpS1929;
                            moonbit_incref(_M0L7_2adataS181);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS217
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1930);
                            _M0L6_2atmpS1929 = _M0Lm9_2acursorS184;
                            _M0Lm9_2acursorS184 = _M0L6_2atmpS1929 + 1;
                            if (_M0L10next__charS217 < 48) {
                              if (_M0L10next__charS217 == 45) {
                                goto join_209;
                              } else {
                                goto join_216;
                              }
                            } else if (_M0L10next__charS217 > 57) {
                              if (_M0L10next__charS217 < 59) {
                                _M0L12dispatch__15S208 = 3;
                                goto loop__label__15_211;
                              } else {
                                goto join_216;
                              }
                            } else {
                              _M0L12dispatch__15S208 = 6;
                              goto loop__label__15_211;
                            }
                            join_216:;
                            _M0L12dispatch__15S208 = 0;
                            goto loop__label__15_211;
                          } else {
                            goto join_200;
                          }
                        }
                      } else if (_M0L10next__charS215 > 58) {
                        goto join_214;
                      } else {
                        _M0L12dispatch__15S208 = 1;
                        goto loop__label__15_211;
                      }
                      join_214:;
                      _M0L12dispatch__15S208 = 0;
                      goto loop__label__15_211;
                    } else {
                      goto join_200;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1932;
                    _M0Lm6tag__1S193 = _M0Lm9_2acursorS184;
                    _M0Lm6tag__2S197 = _M0Lm9_2acursorS184;
                    _M0L6_2atmpS1932 = _M0Lm9_2acursorS184;
                    if (_M0L6_2atmpS1932 < _M0L6_2aendS183) {
                      int32_t _M0L6_2atmpS1934 = _M0Lm9_2acursorS184;
                      int32_t _M0L10next__charS219;
                      int32_t _M0L6_2atmpS1933;
                      moonbit_incref(_M0L7_2adataS181);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS219
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1934);
                      _M0L6_2atmpS1933 = _M0Lm9_2acursorS184;
                      _M0Lm9_2acursorS184 = _M0L6_2atmpS1933 + 1;
                      if (_M0L10next__charS219 < 58) {
                        if (_M0L10next__charS219 < 48) {
                          goto join_218;
                        } else {
                          _M0L12dispatch__15S208 = 2;
                          goto loop__label__15_211;
                        }
                      } else if (_M0L10next__charS219 > 58) {
                        goto join_218;
                      } else {
                        _M0L12dispatch__15S208 = 3;
                        goto loop__label__15_211;
                      }
                      join_218:;
                      _M0L12dispatch__15S208 = 0;
                      goto loop__label__15_211;
                    } else {
                      goto join_200;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1935;
                    _M0Lm6tag__1S193 = _M0Lm9_2acursorS184;
                    _M0L6_2atmpS1935 = _M0Lm9_2acursorS184;
                    if (_M0L6_2atmpS1935 < _M0L6_2aendS183) {
                      int32_t _M0L6_2atmpS1937 = _M0Lm9_2acursorS184;
                      int32_t _M0L10next__charS220;
                      int32_t _M0L6_2atmpS1936;
                      moonbit_incref(_M0L7_2adataS181);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS220
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1937);
                      _M0L6_2atmpS1936 = _M0Lm9_2acursorS184;
                      _M0Lm9_2acursorS184 = _M0L6_2atmpS1936 + 1;
                      if (_M0L10next__charS220 == 58) {
                        _M0L12dispatch__15S208 = 1;
                        goto loop__label__15_211;
                      } else {
                        _M0L12dispatch__15S208 = 0;
                        goto loop__label__15_211;
                      }
                    } else {
                      goto join_200;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1938;
                    _M0Lm6tag__1S193 = _M0Lm9_2acursorS184;
                    _M0Lm6tag__4S199 = _M0Lm9_2acursorS184;
                    _M0L6_2atmpS1938 = _M0Lm9_2acursorS184;
                    if (_M0L6_2atmpS1938 < _M0L6_2aendS183) {
                      int32_t _M0L6_2atmpS1946 = _M0Lm9_2acursorS184;
                      int32_t _M0L10next__charS222;
                      int32_t _M0L6_2atmpS1939;
                      moonbit_incref(_M0L7_2adataS181);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS222
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1946);
                      _M0L6_2atmpS1939 = _M0Lm9_2acursorS184;
                      _M0Lm9_2acursorS184 = _M0L6_2atmpS1939 + 1;
                      if (_M0L10next__charS222 < 58) {
                        if (_M0L10next__charS222 < 48) {
                          goto join_221;
                        } else {
                          _M0L12dispatch__15S208 = 4;
                          goto loop__label__15_211;
                        }
                      } else if (_M0L10next__charS222 > 58) {
                        goto join_221;
                      } else {
                        int32_t _M0L6_2atmpS1940;
                        _M0Lm9tag__1__2S195 = _M0Lm9tag__1__1S194;
                        _M0Lm9tag__1__1S194 = _M0Lm6tag__1S193;
                        _M0Lm6tag__1S193 = _M0Lm9_2acursorS184;
                        _M0L6_2atmpS1940 = _M0Lm9_2acursorS184;
                        if (_M0L6_2atmpS1940 < _M0L6_2aendS183) {
                          int32_t _M0L6_2atmpS1945 = _M0Lm9_2acursorS184;
                          int32_t _M0L10next__charS224;
                          int32_t _M0L6_2atmpS1941;
                          moonbit_incref(_M0L7_2adataS181);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS224
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1945);
                          _M0L6_2atmpS1941 = _M0Lm9_2acursorS184;
                          _M0Lm9_2acursorS184 = _M0L6_2atmpS1941 + 1;
                          if (_M0L10next__charS224 < 58) {
                            if (_M0L10next__charS224 < 48) {
                              goto join_223;
                            } else {
                              int32_t _M0L6_2atmpS1942;
                              _M0Lm6tag__1S193 = _M0Lm9_2acursorS184;
                              _M0Lm9tag__2__1S198 = _M0Lm6tag__2S197;
                              _M0Lm6tag__2S197 = _M0Lm9_2acursorS184;
                              _M0L6_2atmpS1942 = _M0Lm9_2acursorS184;
                              if (_M0L6_2atmpS1942 < _M0L6_2aendS183) {
                                int32_t _M0L6_2atmpS1944 =
                                  _M0Lm9_2acursorS184;
                                int32_t _M0L10next__charS226;
                                int32_t _M0L6_2atmpS1943;
                                moonbit_incref(_M0L7_2adataS181);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS226
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1944);
                                _M0L6_2atmpS1943 = _M0Lm9_2acursorS184;
                                _M0Lm9_2acursorS184 = _M0L6_2atmpS1943 + 1;
                                if (_M0L10next__charS226 < 58) {
                                  if (_M0L10next__charS226 < 48) {
                                    goto join_225;
                                  } else {
                                    _M0L12dispatch__15S208 = 5;
                                    goto loop__label__15_211;
                                  }
                                } else if (_M0L10next__charS226 > 58) {
                                  goto join_225;
                                } else {
                                  _M0L12dispatch__15S208 = 3;
                                  goto loop__label__15_211;
                                }
                                join_225:;
                                _M0L12dispatch__15S208 = 0;
                                goto loop__label__15_211;
                              } else {
                                goto join_213;
                              }
                            }
                          } else if (_M0L10next__charS224 > 58) {
                            goto join_223;
                          } else {
                            _M0L12dispatch__15S208 = 1;
                            goto loop__label__15_211;
                          }
                          join_223:;
                          _M0L12dispatch__15S208 = 0;
                          goto loop__label__15_211;
                        } else {
                          goto join_200;
                        }
                      }
                      join_221:;
                      _M0L12dispatch__15S208 = 0;
                      goto loop__label__15_211;
                    } else {
                      goto join_200;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1947;
                    _M0Lm6tag__1S193 = _M0Lm9_2acursorS184;
                    _M0Lm6tag__2S197 = _M0Lm9_2acursorS184;
                    _M0L6_2atmpS1947 = _M0Lm9_2acursorS184;
                    if (_M0L6_2atmpS1947 < _M0L6_2aendS183) {
                      int32_t _M0L6_2atmpS1949 = _M0Lm9_2acursorS184;
                      int32_t _M0L10next__charS228;
                      int32_t _M0L6_2atmpS1948;
                      moonbit_incref(_M0L7_2adataS181);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS228
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1949);
                      _M0L6_2atmpS1948 = _M0Lm9_2acursorS184;
                      _M0Lm9_2acursorS184 = _M0L6_2atmpS1948 + 1;
                      if (_M0L10next__charS228 < 58) {
                        if (_M0L10next__charS228 < 48) {
                          goto join_227;
                        } else {
                          _M0L12dispatch__15S208 = 5;
                          goto loop__label__15_211;
                        }
                      } else if (_M0L10next__charS228 > 58) {
                        goto join_227;
                      } else {
                        _M0L12dispatch__15S208 = 3;
                        goto loop__label__15_211;
                      }
                      join_227:;
                      _M0L12dispatch__15S208 = 0;
                      goto loop__label__15_211;
                    } else {
                      goto join_213;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1950;
                    _M0Lm6tag__1S193 = _M0Lm9_2acursorS184;
                    _M0Lm6tag__2S197 = _M0Lm9_2acursorS184;
                    _M0Lm6tag__3S196 = _M0Lm9_2acursorS184;
                    _M0L6_2atmpS1950 = _M0Lm9_2acursorS184;
                    if (_M0L6_2atmpS1950 < _M0L6_2aendS183) {
                      int32_t _M0L6_2atmpS1952 = _M0Lm9_2acursorS184;
                      int32_t _M0L10next__charS230;
                      int32_t _M0L6_2atmpS1951;
                      moonbit_incref(_M0L7_2adataS181);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS230
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1952);
                      _M0L6_2atmpS1951 = _M0Lm9_2acursorS184;
                      _M0Lm9_2acursorS184 = _M0L6_2atmpS1951 + 1;
                      if (_M0L10next__charS230 < 48) {
                        if (_M0L10next__charS230 == 45) {
                          goto join_209;
                        } else {
                          goto join_229;
                        }
                      } else if (_M0L10next__charS230 > 57) {
                        if (_M0L10next__charS230 < 59) {
                          _M0L12dispatch__15S208 = 3;
                          goto loop__label__15_211;
                        } else {
                          goto join_229;
                        }
                      } else {
                        _M0L12dispatch__15S208 = 6;
                        goto loop__label__15_211;
                      }
                      join_229:;
                      _M0L12dispatch__15S208 = 0;
                      goto loop__label__15_211;
                    } else {
                      goto join_200;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1953;
                    _M0Lm9tag__1__1S194 = _M0Lm6tag__1S193;
                    _M0Lm6tag__1S193 = _M0Lm9_2acursorS184;
                    _M0L6_2atmpS1953 = _M0Lm9_2acursorS184;
                    if (_M0L6_2atmpS1953 < _M0L6_2aendS183) {
                      int32_t _M0L6_2atmpS1955 = _M0Lm9_2acursorS184;
                      int32_t _M0L10next__charS232;
                      int32_t _M0L6_2atmpS1954;
                      moonbit_incref(_M0L7_2adataS181);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS232
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1955);
                      _M0L6_2atmpS1954 = _M0Lm9_2acursorS184;
                      _M0Lm9_2acursorS184 = _M0L6_2atmpS1954 + 1;
                      if (_M0L10next__charS232 < 58) {
                        if (_M0L10next__charS232 < 48) {
                          goto join_231;
                        } else {
                          _M0L12dispatch__15S208 = 2;
                          goto loop__label__15_211;
                        }
                      } else if (_M0L10next__charS232 > 58) {
                        goto join_231;
                      } else {
                        _M0L12dispatch__15S208 = 1;
                        goto loop__label__15_211;
                      }
                      join_231:;
                      _M0L12dispatch__15S208 = 0;
                      goto loop__label__15_211;
                    } else {
                      goto join_200;
                    }
                    break;
                  }
                  default: {
                    goto join_200;
                    break;
                  }
                }
                join_213:;
                _M0Lm6tag__1S193 = _M0Lm9tag__1__2S195;
                _M0Lm6tag__2S197 = _M0Lm9tag__2__1S198;
                _M0Lm20match__tag__saver__0S187 = _M0Lm6tag__0S192;
                _M0Lm20match__tag__saver__1S188 = _M0Lm6tag__1S193;
                _M0Lm20match__tag__saver__2S189 = _M0Lm6tag__2S197;
                _M0Lm20match__tag__saver__3S190 = _M0Lm6tag__3S196;
                _M0Lm20match__tag__saver__4S191 = _M0Lm6tag__4S199;
                _M0Lm13accept__stateS185 = 0;
                _M0Lm10match__endS186 = _M0Lm9_2acursorS184;
                goto join_200;
                join_209:;
                _M0Lm9tag__1__1S194 = _M0Lm9tag__1__2S195;
                _M0Lm6tag__1S193 = _M0Lm9_2acursorS184;
                _M0Lm6tag__2S197 = _M0Lm9tag__2__1S198;
                _M0L6_2atmpS1923 = _M0Lm9_2acursorS184;
                if (_M0L6_2atmpS1923 < _M0L6_2aendS183) {
                  int32_t _M0L6_2atmpS1925 = _M0Lm9_2acursorS184;
                  int32_t _M0L10next__charS212;
                  int32_t _M0L6_2atmpS1924;
                  moonbit_incref(_M0L7_2adataS181);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS212
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS181, _M0L6_2atmpS1925);
                  _M0L6_2atmpS1924 = _M0Lm9_2acursorS184;
                  _M0Lm9_2acursorS184 = _M0L6_2atmpS1924 + 1;
                  if (_M0L10next__charS212 < 58) {
                    if (_M0L10next__charS212 < 48) {
                      goto join_210;
                    } else {
                      _M0L12dispatch__15S208 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS212 > 58) {
                    goto join_210;
                  } else {
                    _M0L12dispatch__15S208 = 1;
                    continue;
                  }
                  join_210:;
                  _M0L12dispatch__15S208 = 0;
                  continue;
                } else {
                  goto join_200;
                }
                break;
              }
            } else {
              goto join_200;
            }
          } else {
            continue;
          }
        } else {
          goto join_200;
        }
        break;
      }
    } else {
      goto join_200;
    }
  } else {
    goto join_200;
  }
  join_200:;
  switch (_M0Lm13accept__stateS185) {
    case 0: {
      int32_t _M0L6_2atmpS1914 = _M0Lm20match__tag__saver__1S188;
      int32_t _M0L6_2atmpS1913 = _M0L6_2atmpS1914 + 1;
      int64_t _M0L6_2atmpS1910 = (int64_t)_M0L6_2atmpS1913;
      int32_t _M0L6_2atmpS1912 = _M0Lm20match__tag__saver__2S189;
      int64_t _M0L6_2atmpS1911 = (int64_t)_M0L6_2atmpS1912;
      struct _M0TPC16string10StringView _M0L11start__lineS201;
      int32_t _M0L6_2atmpS1909;
      int32_t _M0L6_2atmpS1908;
      int64_t _M0L6_2atmpS1905;
      int32_t _M0L6_2atmpS1907;
      int64_t _M0L6_2atmpS1906;
      struct _M0TPC16string10StringView _M0L13start__columnS202;
      int32_t _M0L6_2atmpS1904;
      int64_t _M0L6_2atmpS1901;
      int32_t _M0L6_2atmpS1903;
      int64_t _M0L6_2atmpS1902;
      struct _M0TPC16string10StringView _M0L3pkgS203;
      int32_t _M0L6_2atmpS1900;
      int32_t _M0L6_2atmpS1899;
      int64_t _M0L6_2atmpS1896;
      int32_t _M0L6_2atmpS1898;
      int64_t _M0L6_2atmpS1897;
      struct _M0TPC16string10StringView _M0L8filenameS204;
      int32_t _M0L6_2atmpS1895;
      int32_t _M0L6_2atmpS1894;
      int64_t _M0L6_2atmpS1891;
      int32_t _M0L6_2atmpS1893;
      int64_t _M0L6_2atmpS1892;
      struct _M0TPC16string10StringView _M0L9end__lineS205;
      int32_t _M0L6_2atmpS1890;
      int32_t _M0L6_2atmpS1889;
      int64_t _M0L6_2atmpS1886;
      int32_t _M0L6_2atmpS1888;
      int64_t _M0L6_2atmpS1887;
      struct _M0TPC16string10StringView _M0L11end__columnS206;
      struct _M0TPB13SourceLocRepr* _block_4595;
      moonbit_incref(_M0L7_2adataS181);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS201
      = _M0MPC16string6String4view(_M0L7_2adataS181, _M0L6_2atmpS1910, _M0L6_2atmpS1911);
      _M0L6_2atmpS1909 = _M0Lm20match__tag__saver__2S189;
      _M0L6_2atmpS1908 = _M0L6_2atmpS1909 + 1;
      _M0L6_2atmpS1905 = (int64_t)_M0L6_2atmpS1908;
      _M0L6_2atmpS1907 = _M0Lm20match__tag__saver__3S190;
      _M0L6_2atmpS1906 = (int64_t)_M0L6_2atmpS1907;
      moonbit_incref(_M0L7_2adataS181);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS202
      = _M0MPC16string6String4view(_M0L7_2adataS181, _M0L6_2atmpS1905, _M0L6_2atmpS1906);
      _M0L6_2atmpS1904 = _M0L8_2astartS182 + 1;
      _M0L6_2atmpS1901 = (int64_t)_M0L6_2atmpS1904;
      _M0L6_2atmpS1903 = _M0Lm20match__tag__saver__0S187;
      _M0L6_2atmpS1902 = (int64_t)_M0L6_2atmpS1903;
      moonbit_incref(_M0L7_2adataS181);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS203
      = _M0MPC16string6String4view(_M0L7_2adataS181, _M0L6_2atmpS1901, _M0L6_2atmpS1902);
      _M0L6_2atmpS1900 = _M0Lm20match__tag__saver__0S187;
      _M0L6_2atmpS1899 = _M0L6_2atmpS1900 + 1;
      _M0L6_2atmpS1896 = (int64_t)_M0L6_2atmpS1899;
      _M0L6_2atmpS1898 = _M0Lm20match__tag__saver__1S188;
      _M0L6_2atmpS1897 = (int64_t)_M0L6_2atmpS1898;
      moonbit_incref(_M0L7_2adataS181);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS204
      = _M0MPC16string6String4view(_M0L7_2adataS181, _M0L6_2atmpS1896, _M0L6_2atmpS1897);
      _M0L6_2atmpS1895 = _M0Lm20match__tag__saver__3S190;
      _M0L6_2atmpS1894 = _M0L6_2atmpS1895 + 1;
      _M0L6_2atmpS1891 = (int64_t)_M0L6_2atmpS1894;
      _M0L6_2atmpS1893 = _M0Lm20match__tag__saver__4S191;
      _M0L6_2atmpS1892 = (int64_t)_M0L6_2atmpS1893;
      moonbit_incref(_M0L7_2adataS181);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS205
      = _M0MPC16string6String4view(_M0L7_2adataS181, _M0L6_2atmpS1891, _M0L6_2atmpS1892);
      _M0L6_2atmpS1890 = _M0Lm20match__tag__saver__4S191;
      _M0L6_2atmpS1889 = _M0L6_2atmpS1890 + 1;
      _M0L6_2atmpS1886 = (int64_t)_M0L6_2atmpS1889;
      _M0L6_2atmpS1888 = _M0Lm10match__endS186;
      _M0L6_2atmpS1887 = (int64_t)_M0L6_2atmpS1888;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS206
      = _M0MPC16string6String4view(_M0L7_2adataS181, _M0L6_2atmpS1886, _M0L6_2atmpS1887);
      _block_4595
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4595)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_4595->$0_0 = _M0L3pkgS203.$0;
      _block_4595->$0_1 = _M0L3pkgS203.$1;
      _block_4595->$0_2 = _M0L3pkgS203.$2;
      _block_4595->$1_0 = _M0L8filenameS204.$0;
      _block_4595->$1_1 = _M0L8filenameS204.$1;
      _block_4595->$1_2 = _M0L8filenameS204.$2;
      _block_4595->$2_0 = _M0L11start__lineS201.$0;
      _block_4595->$2_1 = _M0L11start__lineS201.$1;
      _block_4595->$2_2 = _M0L11start__lineS201.$2;
      _block_4595->$3_0 = _M0L13start__columnS202.$0;
      _block_4595->$3_1 = _M0L13start__columnS202.$1;
      _block_4595->$3_2 = _M0L13start__columnS202.$2;
      _block_4595->$4_0 = _M0L9end__lineS205.$0;
      _block_4595->$4_1 = _M0L9end__lineS205.$1;
      _block_4595->$4_2 = _M0L9end__lineS205.$2;
      _block_4595->$5_0 = _M0L11end__columnS206.$0;
      _block_4595->$5_1 = _M0L11end__columnS206.$1;
      _block_4595->$5_2 = _M0L11end__columnS206.$2;
      return _block_4595;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS181);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS177,
  int32_t _M0L5indexS178
) {
  int32_t _M0L3lenS176;
  int32_t _if__result_4596;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS176 = _M0L4selfS177->$1;
  if (_M0L5indexS178 >= 0) {
    _if__result_4596 = _M0L5indexS178 < _M0L3lenS176;
  } else {
    _if__result_4596 = 0;
  }
  if (_if__result_4596) {
    moonbit_string_t* _M0L6_2atmpS1885;
    moonbit_string_t _M0L6_2atmpS4162;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1885 = _M0MPC15array5Array6bufferGsE(_M0L4selfS177);
    if (
      _M0L5indexS178 < 0
      || _M0L5indexS178 >= Moonbit_array_length(_M0L6_2atmpS1885)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4162 = (moonbit_string_t)_M0L6_2atmpS1885[_M0L5indexS178];
    moonbit_incref(_M0L6_2atmpS4162);
    moonbit_decref(_M0L6_2atmpS1885);
    return _M0L6_2atmpS4162;
  } else {
    moonbit_decref(_M0L4selfS177);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array5Array6lengthGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L4selfS174
) {
  int32_t _M0L8_2afieldS4163;
  #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4163 = _M0L4selfS174->$1;
  moonbit_decref(_M0L4selfS174);
  return _M0L8_2afieldS4163;
}

int32_t _M0MPC15array5Array6lengthGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS175
) {
  int32_t _M0L8_2afieldS4164;
  #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4164 = _M0L4selfS175->$1;
  moonbit_decref(_M0L4selfS175);
  return _M0L8_2afieldS4164;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS167
) {
  moonbit_string_t* _M0L8_2afieldS4165;
  int32_t _M0L6_2acntS4369;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4165 = _M0L4selfS167->$0;
  _M0L6_2acntS4369 = Moonbit_object_header(_M0L4selfS167)->rc;
  if (_M0L6_2acntS4369 > 1) {
    int32_t _M0L11_2anew__cntS4370 = _M0L6_2acntS4369 - 1;
    Moonbit_object_header(_M0L4selfS167)->rc = _M0L11_2anew__cntS4370;
    moonbit_incref(_M0L8_2afieldS4165);
  } else if (_M0L6_2acntS4369 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS167);
  }
  return _M0L8_2afieldS4165;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS168
) {
  struct _M0TUsiE** _M0L8_2afieldS4166;
  int32_t _M0L6_2acntS4371;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4166 = _M0L4selfS168->$0;
  _M0L6_2acntS4371 = Moonbit_object_header(_M0L4selfS168)->rc;
  if (_M0L6_2acntS4371 > 1) {
    int32_t _M0L11_2anew__cntS4372 = _M0L6_2acntS4371 - 1;
    Moonbit_object_header(_M0L4selfS168)->rc = _M0L11_2anew__cntS4372;
    moonbit_incref(_M0L8_2afieldS4166);
  } else if (_M0L6_2acntS4371 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS168);
  }
  return _M0L8_2afieldS4166;
}

struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0MPC15array5Array6bufferGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE* _M0L4selfS169
) {
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L8_2afieldS4167;
  int32_t _M0L6_2acntS4373;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4167 = _M0L4selfS169->$0;
  _M0L6_2acntS4373 = Moonbit_object_header(_M0L4selfS169)->rc;
  if (_M0L6_2acntS4373 > 1) {
    int32_t _M0L11_2anew__cntS4374 = _M0L6_2acntS4373 - 1;
    Moonbit_object_header(_M0L4selfS169)->rc = _M0L11_2anew__cntS4374;
    moonbit_incref(_M0L8_2afieldS4167);
  } else if (_M0L6_2acntS4373 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS169);
  }
  return _M0L8_2afieldS4167;
}

struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE* _M0L4selfS170
) {
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L8_2afieldS4168;
  int32_t _M0L6_2acntS4375;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4168 = _M0L4selfS170->$0;
  _M0L6_2acntS4375 = Moonbit_object_header(_M0L4selfS170)->rc;
  if (_M0L6_2acntS4375 > 1) {
    int32_t _M0L11_2anew__cntS4376 = _M0L6_2acntS4375 - 1;
    Moonbit_object_header(_M0L4selfS170)->rc = _M0L11_2anew__cntS4376;
    moonbit_incref(_M0L8_2afieldS4168);
  } else if (_M0L6_2acntS4375 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS170);
  }
  return _M0L8_2afieldS4168;
}

struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0MPC15array5Array6bufferGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TPB5ArrayGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE* _M0L4selfS171
) {
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L8_2afieldS4169;
  int32_t _M0L6_2acntS4377;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4169 = _M0L4selfS171->$0;
  _M0L6_2acntS4377 = Moonbit_object_header(_M0L4selfS171)->rc;
  if (_M0L6_2acntS4377 > 1) {
    int32_t _M0L11_2anew__cntS4378 = _M0L6_2acntS4377 - 1;
    Moonbit_object_header(_M0L4selfS171)->rc = _M0L11_2anew__cntS4378;
    moonbit_incref(_M0L8_2afieldS4169);
  } else if (_M0L6_2acntS4377 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS171);
  }
  return _M0L8_2afieldS4169;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS172
) {
  void** _M0L8_2afieldS4170;
  int32_t _M0L6_2acntS4379;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4170 = _M0L4selfS172->$0;
  _M0L6_2acntS4379 = Moonbit_object_header(_M0L4selfS172)->rc;
  if (_M0L6_2acntS4379 > 1) {
    int32_t _M0L11_2anew__cntS4380 = _M0L6_2acntS4379 - 1;
    Moonbit_object_header(_M0L4selfS172)->rc = _M0L11_2anew__cntS4380;
    moonbit_incref(_M0L8_2afieldS4170);
  } else if (_M0L6_2acntS4379 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS172);
  }
  return _M0L8_2afieldS4170;
}

struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0MPC15array5Array6bufferGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TPB5ArrayGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE* _M0L4selfS173
) {
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L8_2afieldS4171;
  int32_t _M0L6_2acntS4381;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4171 = _M0L4selfS173->$0;
  _M0L6_2acntS4381 = Moonbit_object_header(_M0L4selfS173)->rc;
  if (_M0L6_2acntS4381 > 1) {
    int32_t _M0L11_2anew__cntS4382 = _M0L6_2acntS4381 - 1;
    Moonbit_object_header(_M0L4selfS173)->rc = _M0L11_2anew__cntS4382;
    moonbit_incref(_M0L8_2afieldS4171);
  } else if (_M0L6_2acntS4381 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS173);
  }
  return _M0L8_2afieldS4171;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS166) {
  struct _M0TPB13StringBuilder* _M0L3bufS165;
  struct _M0TPB6Logger _M0L6_2atmpS1884;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS165 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS165);
  _M0L6_2atmpS1884
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS165
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS166, _M0L6_2atmpS1884);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS165);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS164) {
  int32_t _M0L6_2atmpS1883;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1883 = (int32_t)_M0L4selfS164;
  return _M0L6_2atmpS1883;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS162,
  int32_t _M0L8trailingS163
) {
  int32_t _M0L6_2atmpS1882;
  int32_t _M0L6_2atmpS1881;
  int32_t _M0L6_2atmpS1880;
  int32_t _M0L6_2atmpS1879;
  int32_t _M0L6_2atmpS1878;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1882 = _M0L7leadingS162 - 55296;
  _M0L6_2atmpS1881 = _M0L6_2atmpS1882 * 1024;
  _M0L6_2atmpS1880 = _M0L6_2atmpS1881 + _M0L8trailingS163;
  _M0L6_2atmpS1879 = _M0L6_2atmpS1880 - 56320;
  _M0L6_2atmpS1878 = _M0L6_2atmpS1879 + 65536;
  return _M0L6_2atmpS1878;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS161) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS161 >= 56320) {
    return _M0L4selfS161 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS160) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS160 >= 55296) {
    return _M0L4selfS160 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS157,
  int32_t _M0L2chS159
) {
  int32_t _M0L3lenS1873;
  int32_t _M0L6_2atmpS1872;
  moonbit_bytes_t _M0L8_2afieldS4172;
  moonbit_bytes_t _M0L4dataS1876;
  int32_t _M0L3lenS1877;
  int32_t _M0L3incS158;
  int32_t _M0L3lenS1875;
  int32_t _M0L6_2atmpS1874;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1873 = _M0L4selfS157->$1;
  _M0L6_2atmpS1872 = _M0L3lenS1873 + 4;
  moonbit_incref(_M0L4selfS157);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS157, _M0L6_2atmpS1872);
  _M0L8_2afieldS4172 = _M0L4selfS157->$0;
  _M0L4dataS1876 = _M0L8_2afieldS4172;
  _M0L3lenS1877 = _M0L4selfS157->$1;
  moonbit_incref(_M0L4dataS1876);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS158
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1876, _M0L3lenS1877, _M0L2chS159);
  _M0L3lenS1875 = _M0L4selfS157->$1;
  _M0L6_2atmpS1874 = _M0L3lenS1875 + _M0L3incS158;
  _M0L4selfS157->$1 = _M0L6_2atmpS1874;
  moonbit_decref(_M0L4selfS157);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS152,
  int32_t _M0L8requiredS153
) {
  moonbit_bytes_t _M0L8_2afieldS4176;
  moonbit_bytes_t _M0L4dataS1871;
  int32_t _M0L6_2atmpS4175;
  int32_t _M0L12current__lenS151;
  int32_t _M0Lm13enough__spaceS154;
  int32_t _M0L6_2atmpS1869;
  int32_t _M0L6_2atmpS1870;
  moonbit_bytes_t _M0L9new__dataS156;
  moonbit_bytes_t _M0L8_2afieldS4174;
  moonbit_bytes_t _M0L4dataS1867;
  int32_t _M0L3lenS1868;
  moonbit_bytes_t _M0L6_2aoldS4173;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4176 = _M0L4selfS152->$0;
  _M0L4dataS1871 = _M0L8_2afieldS4176;
  _M0L6_2atmpS4175 = Moonbit_array_length(_M0L4dataS1871);
  _M0L12current__lenS151 = _M0L6_2atmpS4175;
  if (_M0L8requiredS153 <= _M0L12current__lenS151) {
    moonbit_decref(_M0L4selfS152);
    return 0;
  }
  _M0Lm13enough__spaceS154 = _M0L12current__lenS151;
  while (1) {
    int32_t _M0L6_2atmpS1865 = _M0Lm13enough__spaceS154;
    if (_M0L6_2atmpS1865 < _M0L8requiredS153) {
      int32_t _M0L6_2atmpS1866 = _M0Lm13enough__spaceS154;
      _M0Lm13enough__spaceS154 = _M0L6_2atmpS1866 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1869 = _M0Lm13enough__spaceS154;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1870 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS156
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1869, _M0L6_2atmpS1870);
  _M0L8_2afieldS4174 = _M0L4selfS152->$0;
  _M0L4dataS1867 = _M0L8_2afieldS4174;
  _M0L3lenS1868 = _M0L4selfS152->$1;
  moonbit_incref(_M0L4dataS1867);
  moonbit_incref(_M0L9new__dataS156);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS156, 0, _M0L4dataS1867, 0, _M0L3lenS1868);
  _M0L6_2aoldS4173 = _M0L4selfS152->$0;
  moonbit_decref(_M0L6_2aoldS4173);
  _M0L4selfS152->$0 = _M0L9new__dataS156;
  moonbit_decref(_M0L4selfS152);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS146,
  int32_t _M0L6offsetS147,
  int32_t _M0L5valueS145
) {
  uint32_t _M0L4codeS144;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS144 = _M0MPC14char4Char8to__uint(_M0L5valueS145);
  if (_M0L4codeS144 < 65536u) {
    uint32_t _M0L6_2atmpS1848 = _M0L4codeS144 & 255u;
    int32_t _M0L6_2atmpS1847;
    int32_t _M0L6_2atmpS1849;
    uint32_t _M0L6_2atmpS1851;
    int32_t _M0L6_2atmpS1850;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1847 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1848);
    if (
      _M0L6offsetS147 < 0
      || _M0L6offsetS147 >= Moonbit_array_length(_M0L4selfS146)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS146[_M0L6offsetS147] = _M0L6_2atmpS1847;
    _M0L6_2atmpS1849 = _M0L6offsetS147 + 1;
    _M0L6_2atmpS1851 = _M0L4codeS144 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1850 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1851);
    if (
      _M0L6_2atmpS1849 < 0
      || _M0L6_2atmpS1849 >= Moonbit_array_length(_M0L4selfS146)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS146[_M0L6_2atmpS1849] = _M0L6_2atmpS1850;
    moonbit_decref(_M0L4selfS146);
    return 2;
  } else if (_M0L4codeS144 < 1114112u) {
    uint32_t _M0L2hiS148 = _M0L4codeS144 - 65536u;
    uint32_t _M0L6_2atmpS1864 = _M0L2hiS148 >> 10;
    uint32_t _M0L2loS149 = _M0L6_2atmpS1864 | 55296u;
    uint32_t _M0L6_2atmpS1863 = _M0L2hiS148 & 1023u;
    uint32_t _M0L2hiS150 = _M0L6_2atmpS1863 | 56320u;
    uint32_t _M0L6_2atmpS1853 = _M0L2loS149 & 255u;
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
      _M0L6offsetS147 < 0
      || _M0L6offsetS147 >= Moonbit_array_length(_M0L4selfS146)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS146[_M0L6offsetS147] = _M0L6_2atmpS1852;
    _M0L6_2atmpS1854 = _M0L6offsetS147 + 1;
    _M0L6_2atmpS1856 = _M0L2loS149 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1855 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1856);
    if (
      _M0L6_2atmpS1854 < 0
      || _M0L6_2atmpS1854 >= Moonbit_array_length(_M0L4selfS146)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS146[_M0L6_2atmpS1854] = _M0L6_2atmpS1855;
    _M0L6_2atmpS1857 = _M0L6offsetS147 + 2;
    _M0L6_2atmpS1859 = _M0L2hiS150 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1858 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1859);
    if (
      _M0L6_2atmpS1857 < 0
      || _M0L6_2atmpS1857 >= Moonbit_array_length(_M0L4selfS146)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS146[_M0L6_2atmpS1857] = _M0L6_2atmpS1858;
    _M0L6_2atmpS1860 = _M0L6offsetS147 + 3;
    _M0L6_2atmpS1862 = _M0L2hiS150 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1861 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1862);
    if (
      _M0L6_2atmpS1860 < 0
      || _M0L6_2atmpS1860 >= Moonbit_array_length(_M0L4selfS146)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS146[_M0L6_2atmpS1860] = _M0L6_2atmpS1861;
    moonbit_decref(_M0L4selfS146);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS146);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_130.data, (moonbit_string_t)moonbit_string_literal_131.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS143) {
  int32_t _M0L6_2atmpS1846;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1846 = *(int32_t*)&_M0L4selfS143;
  return _M0L6_2atmpS1846 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS142) {
  int32_t _M0L6_2atmpS1845;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1845 = _M0L4selfS142;
  return *(uint32_t*)&_M0L6_2atmpS1845;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS141
) {
  moonbit_bytes_t _M0L8_2afieldS4178;
  moonbit_bytes_t _M0L4dataS1844;
  moonbit_bytes_t _M0L6_2atmpS1841;
  int32_t _M0L8_2afieldS4177;
  int32_t _M0L3lenS1843;
  int64_t _M0L6_2atmpS1842;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4178 = _M0L4selfS141->$0;
  _M0L4dataS1844 = _M0L8_2afieldS4178;
  moonbit_incref(_M0L4dataS1844);
  _M0L6_2atmpS1841 = _M0L4dataS1844;
  _M0L8_2afieldS4177 = _M0L4selfS141->$1;
  moonbit_decref(_M0L4selfS141);
  _M0L3lenS1843 = _M0L8_2afieldS4177;
  _M0L6_2atmpS1842 = (int64_t)_M0L3lenS1843;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1841, 0, _M0L6_2atmpS1842);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS136,
  int32_t _M0L6offsetS140,
  int64_t _M0L6lengthS138
) {
  int32_t _M0L3lenS135;
  int32_t _M0L6lengthS137;
  int32_t _if__result_4598;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS135 = Moonbit_array_length(_M0L4selfS136);
  if (_M0L6lengthS138 == 4294967296ll) {
    _M0L6lengthS137 = _M0L3lenS135 - _M0L6offsetS140;
  } else {
    int64_t _M0L7_2aSomeS139 = _M0L6lengthS138;
    _M0L6lengthS137 = (int32_t)_M0L7_2aSomeS139;
  }
  if (_M0L6offsetS140 >= 0) {
    if (_M0L6lengthS137 >= 0) {
      int32_t _M0L6_2atmpS1840 = _M0L6offsetS140 + _M0L6lengthS137;
      _if__result_4598 = _M0L6_2atmpS1840 <= _M0L3lenS135;
    } else {
      _if__result_4598 = 0;
    }
  } else {
    _if__result_4598 = 0;
  }
  if (_if__result_4598) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS136, _M0L6offsetS140, _M0L6lengthS137);
  } else {
    moonbit_decref(_M0L4selfS136);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS133
) {
  int32_t _M0L7initialS132;
  moonbit_bytes_t _M0L4dataS134;
  struct _M0TPB13StringBuilder* _block_4599;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS133 < 1) {
    _M0L7initialS132 = 1;
  } else {
    _M0L7initialS132 = _M0L10size__hintS133;
  }
  _M0L4dataS134 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS132, 0);
  _block_4599
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4599)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4599->$0 = _M0L4dataS134;
  _block_4599->$1 = 0;
  return _block_4599;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS131) {
  int32_t _M0L6_2atmpS1839;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1839 = (int32_t)_M0L4selfS131;
  return _M0L6_2atmpS1839;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS96,
  int32_t _M0L11dst__offsetS97,
  moonbit_string_t* _M0L3srcS98,
  int32_t _M0L11src__offsetS99,
  int32_t _M0L3lenS100
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS96, _M0L11dst__offsetS97, _M0L3srcS98, _M0L11src__offsetS99, _M0L3lenS100);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS101,
  int32_t _M0L11dst__offsetS102,
  struct _M0TUsiE** _M0L3srcS103,
  int32_t _M0L11src__offsetS104,
  int32_t _M0L3lenS105
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS101, _M0L11dst__offsetS102, _M0L3srcS103, _M0L11src__offsetS104, _M0L3lenS105);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallE(
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L3dstS106,
  int32_t _M0L11dst__offsetS107,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L3srcS108,
  int32_t _M0L11src__offsetS109,
  int32_t _M0L3lenS110
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallEE(_M0L3dstS106, _M0L11dst__offsetS107, _M0L3srcS108, _M0L11src__offsetS109, _M0L3lenS110);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(
  void** _M0L3dstS111,
  int32_t _M0L11dst__offsetS112,
  void** _M0L3srcS113,
  int32_t _M0L11src__offsetS114,
  int32_t _M0L3lenS115
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(_M0L3dstS111, _M0L11dst__offsetS112, _M0L3srcS113, _M0L11src__offsetS114, _M0L3lenS115);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderE(
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L3dstS116,
  int32_t _M0L11dst__offsetS117,
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L3srcS118,
  int32_t _M0L11src__offsetS119,
  int32_t _M0L3lenS120
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderEE(_M0L3dstS116, _M0L11dst__offsetS117, _M0L3srcS118, _M0L11src__offsetS119, _M0L3lenS120);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderE(
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L3dstS121,
  int32_t _M0L11dst__offsetS122,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L3srcS123,
  int32_t _M0L11src__offsetS124,
  int32_t _M0L3lenS125
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEE(_M0L3dstS121, _M0L11dst__offsetS122, _M0L3srcS123, _M0L11src__offsetS124, _M0L3lenS125);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceE(
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L3dstS126,
  int32_t _M0L11dst__offsetS127,
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L3srcS128,
  int32_t _M0L11src__offsetS129,
  int32_t _M0L3lenS130
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceEE(_M0L3dstS126, _M0L11dst__offsetS127, _M0L3srcS128, _M0L11src__offsetS129, _M0L3lenS130);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS24,
  int32_t _M0L11dst__offsetS26,
  moonbit_bytes_t _M0L3srcS25,
  int32_t _M0L11src__offsetS27,
  int32_t _M0L3lenS29
) {
  int32_t _if__result_4600;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS24 == _M0L3srcS25) {
    _if__result_4600 = _M0L11dst__offsetS26 < _M0L11src__offsetS27;
  } else {
    _if__result_4600 = 0;
  }
  if (_if__result_4600) {
    int32_t _M0L1iS28 = 0;
    while (1) {
      if (_M0L1iS28 < _M0L3lenS29) {
        int32_t _M0L6_2atmpS1767 = _M0L11dst__offsetS26 + _M0L1iS28;
        int32_t _M0L6_2atmpS1769 = _M0L11src__offsetS27 + _M0L1iS28;
        int32_t _M0L6_2atmpS1768;
        int32_t _M0L6_2atmpS1770;
        if (
          _M0L6_2atmpS1769 < 0
          || _M0L6_2atmpS1769 >= Moonbit_array_length(_M0L3srcS25)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1768 = (int32_t)_M0L3srcS25[_M0L6_2atmpS1769];
        if (
          _M0L6_2atmpS1767 < 0
          || _M0L6_2atmpS1767 >= Moonbit_array_length(_M0L3dstS24)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS24[_M0L6_2atmpS1767] = _M0L6_2atmpS1768;
        _M0L6_2atmpS1770 = _M0L1iS28 + 1;
        _M0L1iS28 = _M0L6_2atmpS1770;
        continue;
      } else {
        moonbit_decref(_M0L3srcS25);
        moonbit_decref(_M0L3dstS24);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1775 = _M0L3lenS29 - 1;
    int32_t _M0L1iS31 = _M0L6_2atmpS1775;
    while (1) {
      if (_M0L1iS31 >= 0) {
        int32_t _M0L6_2atmpS1771 = _M0L11dst__offsetS26 + _M0L1iS31;
        int32_t _M0L6_2atmpS1773 = _M0L11src__offsetS27 + _M0L1iS31;
        int32_t _M0L6_2atmpS1772;
        int32_t _M0L6_2atmpS1774;
        if (
          _M0L6_2atmpS1773 < 0
          || _M0L6_2atmpS1773 >= Moonbit_array_length(_M0L3srcS25)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1772 = (int32_t)_M0L3srcS25[_M0L6_2atmpS1773];
        if (
          _M0L6_2atmpS1771 < 0
          || _M0L6_2atmpS1771 >= Moonbit_array_length(_M0L3dstS24)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS24[_M0L6_2atmpS1771] = _M0L6_2atmpS1772;
        _M0L6_2atmpS1774 = _M0L1iS31 - 1;
        _M0L1iS31 = _M0L6_2atmpS1774;
        continue;
      } else {
        moonbit_decref(_M0L3srcS25);
        moonbit_decref(_M0L3dstS24);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS33,
  int32_t _M0L11dst__offsetS35,
  moonbit_string_t* _M0L3srcS34,
  int32_t _M0L11src__offsetS36,
  int32_t _M0L3lenS38
) {
  int32_t _if__result_4603;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS33 == _M0L3srcS34) {
    _if__result_4603 = _M0L11dst__offsetS35 < _M0L11src__offsetS36;
  } else {
    _if__result_4603 = 0;
  }
  if (_if__result_4603) {
    int32_t _M0L1iS37 = 0;
    while (1) {
      if (_M0L1iS37 < _M0L3lenS38) {
        int32_t _M0L6_2atmpS1776 = _M0L11dst__offsetS35 + _M0L1iS37;
        int32_t _M0L6_2atmpS1778 = _M0L11src__offsetS36 + _M0L1iS37;
        moonbit_string_t _M0L6_2atmpS4180;
        moonbit_string_t _M0L6_2atmpS1777;
        moonbit_string_t _M0L6_2aoldS4179;
        int32_t _M0L6_2atmpS1779;
        if (
          _M0L6_2atmpS1778 < 0
          || _M0L6_2atmpS1778 >= Moonbit_array_length(_M0L3srcS34)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4180 = (moonbit_string_t)_M0L3srcS34[_M0L6_2atmpS1778];
        _M0L6_2atmpS1777 = _M0L6_2atmpS4180;
        if (
          _M0L6_2atmpS1776 < 0
          || _M0L6_2atmpS1776 >= Moonbit_array_length(_M0L3dstS33)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4179 = (moonbit_string_t)_M0L3dstS33[_M0L6_2atmpS1776];
        moonbit_incref(_M0L6_2atmpS1777);
        moonbit_decref(_M0L6_2aoldS4179);
        _M0L3dstS33[_M0L6_2atmpS1776] = _M0L6_2atmpS1777;
        _M0L6_2atmpS1779 = _M0L1iS37 + 1;
        _M0L1iS37 = _M0L6_2atmpS1779;
        continue;
      } else {
        moonbit_decref(_M0L3srcS34);
        moonbit_decref(_M0L3dstS33);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1784 = _M0L3lenS38 - 1;
    int32_t _M0L1iS40 = _M0L6_2atmpS1784;
    while (1) {
      if (_M0L1iS40 >= 0) {
        int32_t _M0L6_2atmpS1780 = _M0L11dst__offsetS35 + _M0L1iS40;
        int32_t _M0L6_2atmpS1782 = _M0L11src__offsetS36 + _M0L1iS40;
        moonbit_string_t _M0L6_2atmpS4182;
        moonbit_string_t _M0L6_2atmpS1781;
        moonbit_string_t _M0L6_2aoldS4181;
        int32_t _M0L6_2atmpS1783;
        if (
          _M0L6_2atmpS1782 < 0
          || _M0L6_2atmpS1782 >= Moonbit_array_length(_M0L3srcS34)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4182 = (moonbit_string_t)_M0L3srcS34[_M0L6_2atmpS1782];
        _M0L6_2atmpS1781 = _M0L6_2atmpS4182;
        if (
          _M0L6_2atmpS1780 < 0
          || _M0L6_2atmpS1780 >= Moonbit_array_length(_M0L3dstS33)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4181 = (moonbit_string_t)_M0L3dstS33[_M0L6_2atmpS1780];
        moonbit_incref(_M0L6_2atmpS1781);
        moonbit_decref(_M0L6_2aoldS4181);
        _M0L3dstS33[_M0L6_2atmpS1780] = _M0L6_2atmpS1781;
        _M0L6_2atmpS1783 = _M0L1iS40 - 1;
        _M0L1iS40 = _M0L6_2atmpS1783;
        continue;
      } else {
        moonbit_decref(_M0L3srcS34);
        moonbit_decref(_M0L3dstS33);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS42,
  int32_t _M0L11dst__offsetS44,
  struct _M0TUsiE** _M0L3srcS43,
  int32_t _M0L11src__offsetS45,
  int32_t _M0L3lenS47
) {
  int32_t _if__result_4606;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS42 == _M0L3srcS43) {
    _if__result_4606 = _M0L11dst__offsetS44 < _M0L11src__offsetS45;
  } else {
    _if__result_4606 = 0;
  }
  if (_if__result_4606) {
    int32_t _M0L1iS46 = 0;
    while (1) {
      if (_M0L1iS46 < _M0L3lenS47) {
        int32_t _M0L6_2atmpS1785 = _M0L11dst__offsetS44 + _M0L1iS46;
        int32_t _M0L6_2atmpS1787 = _M0L11src__offsetS45 + _M0L1iS46;
        struct _M0TUsiE* _M0L6_2atmpS4184;
        struct _M0TUsiE* _M0L6_2atmpS1786;
        struct _M0TUsiE* _M0L6_2aoldS4183;
        int32_t _M0L6_2atmpS1788;
        if (
          _M0L6_2atmpS1787 < 0
          || _M0L6_2atmpS1787 >= Moonbit_array_length(_M0L3srcS43)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4184 = (struct _M0TUsiE*)_M0L3srcS43[_M0L6_2atmpS1787];
        _M0L6_2atmpS1786 = _M0L6_2atmpS4184;
        if (
          _M0L6_2atmpS1785 < 0
          || _M0L6_2atmpS1785 >= Moonbit_array_length(_M0L3dstS42)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4183 = (struct _M0TUsiE*)_M0L3dstS42[_M0L6_2atmpS1785];
        if (_M0L6_2atmpS1786) {
          moonbit_incref(_M0L6_2atmpS1786);
        }
        if (_M0L6_2aoldS4183) {
          moonbit_decref(_M0L6_2aoldS4183);
        }
        _M0L3dstS42[_M0L6_2atmpS1785] = _M0L6_2atmpS1786;
        _M0L6_2atmpS1788 = _M0L1iS46 + 1;
        _M0L1iS46 = _M0L6_2atmpS1788;
        continue;
      } else {
        moonbit_decref(_M0L3srcS43);
        moonbit_decref(_M0L3dstS42);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1793 = _M0L3lenS47 - 1;
    int32_t _M0L1iS49 = _M0L6_2atmpS1793;
    while (1) {
      if (_M0L1iS49 >= 0) {
        int32_t _M0L6_2atmpS1789 = _M0L11dst__offsetS44 + _M0L1iS49;
        int32_t _M0L6_2atmpS1791 = _M0L11src__offsetS45 + _M0L1iS49;
        struct _M0TUsiE* _M0L6_2atmpS4186;
        struct _M0TUsiE* _M0L6_2atmpS1790;
        struct _M0TUsiE* _M0L6_2aoldS4185;
        int32_t _M0L6_2atmpS1792;
        if (
          _M0L6_2atmpS1791 < 0
          || _M0L6_2atmpS1791 >= Moonbit_array_length(_M0L3srcS43)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4186 = (struct _M0TUsiE*)_M0L3srcS43[_M0L6_2atmpS1791];
        _M0L6_2atmpS1790 = _M0L6_2atmpS4186;
        if (
          _M0L6_2atmpS1789 < 0
          || _M0L6_2atmpS1789 >= Moonbit_array_length(_M0L3dstS42)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4185 = (struct _M0TUsiE*)_M0L3dstS42[_M0L6_2atmpS1789];
        if (_M0L6_2atmpS1790) {
          moonbit_incref(_M0L6_2atmpS1790);
        }
        if (_M0L6_2aoldS4185) {
          moonbit_decref(_M0L6_2aoldS4185);
        }
        _M0L3dstS42[_M0L6_2atmpS1789] = _M0L6_2atmpS1790;
        _M0L6_2atmpS1792 = _M0L1iS49 - 1;
        _M0L1iS49 = _M0L6_2atmpS1792;
        continue;
      } else {
        moonbit_decref(_M0L3srcS43);
        moonbit_decref(_M0L3dstS42);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallEE(
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L3dstS51,
  int32_t _M0L11dst__offsetS53,
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall** _M0L3srcS52,
  int32_t _M0L11src__offsetS54,
  int32_t _M0L3lenS56
) {
  int32_t _if__result_4609;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS51 == _M0L3srcS52) {
    _if__result_4609 = _M0L11dst__offsetS53 < _M0L11src__offsetS54;
  } else {
    _if__result_4609 = 0;
  }
  if (_if__result_4609) {
    int32_t _M0L1iS55 = 0;
    while (1) {
      if (_M0L1iS55 < _M0L3lenS56) {
        int32_t _M0L6_2atmpS1794 = _M0L11dst__offsetS53 + _M0L1iS55;
        int32_t _M0L6_2atmpS1796 = _M0L11src__offsetS54 + _M0L1iS55;
        struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2atmpS4188;
        struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2atmpS1795;
        struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2aoldS4187;
        int32_t _M0L6_2atmpS1797;
        if (
          _M0L6_2atmpS1796 < 0
          || _M0L6_2atmpS1796 >= Moonbit_array_length(_M0L3srcS52)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4188
        = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)_M0L3srcS52[
            _M0L6_2atmpS1796
          ];
        _M0L6_2atmpS1795 = _M0L6_2atmpS4188;
        if (
          _M0L6_2atmpS1794 < 0
          || _M0L6_2atmpS1794 >= Moonbit_array_length(_M0L3dstS51)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4187
        = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)_M0L3dstS51[
            _M0L6_2atmpS1794
          ];
        if (_M0L6_2atmpS1795) {
          moonbit_incref(_M0L6_2atmpS1795);
        }
        if (_M0L6_2aoldS4187) {
          moonbit_decref(_M0L6_2aoldS4187);
        }
        _M0L3dstS51[_M0L6_2atmpS1794] = _M0L6_2atmpS1795;
        _M0L6_2atmpS1797 = _M0L1iS55 + 1;
        _M0L1iS55 = _M0L6_2atmpS1797;
        continue;
      } else {
        moonbit_decref(_M0L3srcS52);
        moonbit_decref(_M0L3dstS51);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1802 = _M0L3lenS56 - 1;
    int32_t _M0L1iS58 = _M0L6_2atmpS1802;
    while (1) {
      if (_M0L1iS58 >= 0) {
        int32_t _M0L6_2atmpS1798 = _M0L11dst__offsetS53 + _M0L1iS58;
        int32_t _M0L6_2atmpS1800 = _M0L11src__offsetS54 + _M0L1iS58;
        struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2atmpS4190;
        struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2atmpS1799;
        struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L6_2aoldS4189;
        int32_t _M0L6_2atmpS1801;
        if (
          _M0L6_2atmpS1800 < 0
          || _M0L6_2atmpS1800 >= Moonbit_array_length(_M0L3srcS52)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4190
        = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)_M0L3srcS52[
            _M0L6_2atmpS1800
          ];
        _M0L6_2atmpS1799 = _M0L6_2atmpS4190;
        if (
          _M0L6_2atmpS1798 < 0
          || _M0L6_2atmpS1798 >= Moonbit_array_length(_M0L3dstS51)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4189
        = (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)_M0L3dstS51[
            _M0L6_2atmpS1798
          ];
        if (_M0L6_2atmpS1799) {
          moonbit_incref(_M0L6_2atmpS1799);
        }
        if (_M0L6_2aoldS4189) {
          moonbit_decref(_M0L6_2aoldS4189);
        }
        _M0L3dstS51[_M0L6_2atmpS1798] = _M0L6_2atmpS1799;
        _M0L6_2atmpS1801 = _M0L1iS58 - 1;
        _M0L1iS58 = _M0L6_2atmpS1801;
        continue;
      } else {
        moonbit_decref(_M0L3srcS52);
        moonbit_decref(_M0L3dstS51);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRPC14json10WriteFrameEE(
  void** _M0L3dstS60,
  int32_t _M0L11dst__offsetS62,
  void** _M0L3srcS61,
  int32_t _M0L11src__offsetS63,
  int32_t _M0L3lenS65
) {
  int32_t _if__result_4612;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS60 == _M0L3srcS61) {
    _if__result_4612 = _M0L11dst__offsetS62 < _M0L11src__offsetS63;
  } else {
    _if__result_4612 = 0;
  }
  if (_if__result_4612) {
    int32_t _M0L1iS64 = 0;
    while (1) {
      if (_M0L1iS64 < _M0L3lenS65) {
        int32_t _M0L6_2atmpS1803 = _M0L11dst__offsetS62 + _M0L1iS64;
        int32_t _M0L6_2atmpS1805 = _M0L11src__offsetS63 + _M0L1iS64;
        void* _M0L6_2atmpS4192;
        void* _M0L6_2atmpS1804;
        void* _M0L6_2aoldS4191;
        int32_t _M0L6_2atmpS1806;
        if (
          _M0L6_2atmpS1805 < 0
          || _M0L6_2atmpS1805 >= Moonbit_array_length(_M0L3srcS61)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4192 = (void*)_M0L3srcS61[_M0L6_2atmpS1805];
        _M0L6_2atmpS1804 = _M0L6_2atmpS4192;
        if (
          _M0L6_2atmpS1803 < 0
          || _M0L6_2atmpS1803 >= Moonbit_array_length(_M0L3dstS60)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4191 = (void*)_M0L3dstS60[_M0L6_2atmpS1803];
        moonbit_incref(_M0L6_2atmpS1804);
        moonbit_decref(_M0L6_2aoldS4191);
        _M0L3dstS60[_M0L6_2atmpS1803] = _M0L6_2atmpS1804;
        _M0L6_2atmpS1806 = _M0L1iS64 + 1;
        _M0L1iS64 = _M0L6_2atmpS1806;
        continue;
      } else {
        moonbit_decref(_M0L3srcS61);
        moonbit_decref(_M0L3dstS60);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1811 = _M0L3lenS65 - 1;
    int32_t _M0L1iS67 = _M0L6_2atmpS1811;
    while (1) {
      if (_M0L1iS67 >= 0) {
        int32_t _M0L6_2atmpS1807 = _M0L11dst__offsetS62 + _M0L1iS67;
        int32_t _M0L6_2atmpS1809 = _M0L11src__offsetS63 + _M0L1iS67;
        void* _M0L6_2atmpS4194;
        void* _M0L6_2atmpS1808;
        void* _M0L6_2aoldS4193;
        int32_t _M0L6_2atmpS1810;
        if (
          _M0L6_2atmpS1809 < 0
          || _M0L6_2atmpS1809 >= Moonbit_array_length(_M0L3srcS61)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4194 = (void*)_M0L3srcS61[_M0L6_2atmpS1809];
        _M0L6_2atmpS1808 = _M0L6_2atmpS4194;
        if (
          _M0L6_2atmpS1807 < 0
          || _M0L6_2atmpS1807 >= Moonbit_array_length(_M0L3dstS60)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4193 = (void*)_M0L3dstS60[_M0L6_2atmpS1807];
        moonbit_incref(_M0L6_2atmpS1808);
        moonbit_decref(_M0L6_2aoldS4193);
        _M0L3dstS60[_M0L6_2atmpS1807] = _M0L6_2atmpS1808;
        _M0L6_2atmpS1810 = _M0L1iS67 - 1;
        _M0L1iS67 = _M0L6_2atmpS1810;
        continue;
      } else {
        moonbit_decref(_M0L3srcS61);
        moonbit_decref(_M0L3dstS60);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGORP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilderEE(
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L3dstS69,
  int32_t _M0L11dst__offsetS71,
  struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder** _M0L3srcS70,
  int32_t _M0L11src__offsetS72,
  int32_t _M0L3lenS74
) {
  int32_t _if__result_4615;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS69 == _M0L3srcS70) {
    _if__result_4615 = _M0L11dst__offsetS71 < _M0L11src__offsetS72;
  } else {
    _if__result_4615 = 0;
  }
  if (_if__result_4615) {
    int32_t _M0L1iS73 = 0;
    while (1) {
      if (_M0L1iS73 < _M0L3lenS74) {
        int32_t _M0L6_2atmpS1812 = _M0L11dst__offsetS71 + _M0L1iS73;
        int32_t _M0L6_2atmpS1814 = _M0L11src__offsetS72 + _M0L1iS73;
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2atmpS4196;
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2atmpS1813;
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2aoldS4195;
        int32_t _M0L6_2atmpS1815;
        if (
          _M0L6_2atmpS1814 < 0
          || _M0L6_2atmpS1814 >= Moonbit_array_length(_M0L3srcS70)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4196
        = (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*)_M0L3srcS70[
            _M0L6_2atmpS1814
          ];
        _M0L6_2atmpS1813 = _M0L6_2atmpS4196;
        if (
          _M0L6_2atmpS1812 < 0
          || _M0L6_2atmpS1812 >= Moonbit_array_length(_M0L3dstS69)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4195
        = (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*)_M0L3dstS69[
            _M0L6_2atmpS1812
          ];
        if (_M0L6_2atmpS1813) {
          moonbit_incref(_M0L6_2atmpS1813);
        }
        if (_M0L6_2aoldS4195) {
          moonbit_decref(_M0L6_2aoldS4195);
        }
        _M0L3dstS69[_M0L6_2atmpS1812] = _M0L6_2atmpS1813;
        _M0L6_2atmpS1815 = _M0L1iS73 + 1;
        _M0L1iS73 = _M0L6_2atmpS1815;
        continue;
      } else {
        moonbit_decref(_M0L3srcS70);
        moonbit_decref(_M0L3dstS69);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1820 = _M0L3lenS74 - 1;
    int32_t _M0L1iS76 = _M0L6_2atmpS1820;
    while (1) {
      if (_M0L1iS76 >= 0) {
        int32_t _M0L6_2atmpS1816 = _M0L11dst__offsetS71 + _M0L1iS76;
        int32_t _M0L6_2atmpS1818 = _M0L11src__offsetS72 + _M0L1iS76;
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2atmpS4198;
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2atmpS1817;
        struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder* _M0L6_2aoldS4197;
        int32_t _M0L6_2atmpS1819;
        if (
          _M0L6_2atmpS1818 < 0
          || _M0L6_2atmpS1818 >= Moonbit_array_length(_M0L3srcS70)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4198
        = (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*)_M0L3srcS70[
            _M0L6_2atmpS1818
          ];
        _M0L6_2atmpS1817 = _M0L6_2atmpS4198;
        if (
          _M0L6_2atmpS1816 < 0
          || _M0L6_2atmpS1816 >= Moonbit_array_length(_M0L3dstS69)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4197
        = (struct _M0TP48clawteam8clawteam8internal6openai36ChatCompletionMessageToolCallBuilder*)_M0L3dstS69[
            _M0L6_2atmpS1816
          ];
        if (_M0L6_2atmpS1817) {
          moonbit_incref(_M0L6_2atmpS1817);
        }
        if (_M0L6_2aoldS4197) {
          moonbit_decref(_M0L6_2aoldS4197);
        }
        _M0L3dstS69[_M0L6_2atmpS1816] = _M0L6_2atmpS1817;
        _M0L6_2atmpS1819 = _M0L1iS76 - 1;
        _M0L1iS76 = _M0L6_2atmpS1819;
        continue;
      } else {
        moonbit_decref(_M0L3srcS70);
        moonbit_decref(_M0L3dstS69);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGORP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilderEE(
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L3dstS78,
  int32_t _M0L11dst__offsetS80,
  struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder** _M0L3srcS79,
  int32_t _M0L11src__offsetS81,
  int32_t _M0L3lenS83
) {
  int32_t _if__result_4618;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS78 == _M0L3srcS79) {
    _if__result_4618 = _M0L11dst__offsetS80 < _M0L11src__offsetS81;
  } else {
    _if__result_4618 = 0;
  }
  if (_if__result_4618) {
    int32_t _M0L1iS82 = 0;
    while (1) {
      if (_M0L1iS82 < _M0L3lenS83) {
        int32_t _M0L6_2atmpS1821 = _M0L11dst__offsetS80 + _M0L1iS82;
        int32_t _M0L6_2atmpS1823 = _M0L11src__offsetS81 + _M0L1iS82;
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2atmpS4200;
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2atmpS1822;
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2aoldS4199;
        int32_t _M0L6_2atmpS1824;
        if (
          _M0L6_2atmpS1823 < 0
          || _M0L6_2atmpS1823 >= Moonbit_array_length(_M0L3srcS79)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4200
        = (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*)_M0L3srcS79[
            _M0L6_2atmpS1823
          ];
        _M0L6_2atmpS1822 = _M0L6_2atmpS4200;
        if (
          _M0L6_2atmpS1821 < 0
          || _M0L6_2atmpS1821 >= Moonbit_array_length(_M0L3dstS78)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4199
        = (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*)_M0L3dstS78[
            _M0L6_2atmpS1821
          ];
        if (_M0L6_2atmpS1822) {
          moonbit_incref(_M0L6_2atmpS1822);
        }
        if (_M0L6_2aoldS4199) {
          moonbit_decref(_M0L6_2aoldS4199);
        }
        _M0L3dstS78[_M0L6_2atmpS1821] = _M0L6_2atmpS1822;
        _M0L6_2atmpS1824 = _M0L1iS82 + 1;
        _M0L1iS82 = _M0L6_2atmpS1824;
        continue;
      } else {
        moonbit_decref(_M0L3srcS79);
        moonbit_decref(_M0L3dstS78);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1829 = _M0L3lenS83 - 1;
    int32_t _M0L1iS85 = _M0L6_2atmpS1829;
    while (1) {
      if (_M0L1iS85 >= 0) {
        int32_t _M0L6_2atmpS1825 = _M0L11dst__offsetS80 + _M0L1iS85;
        int32_t _M0L6_2atmpS1827 = _M0L11src__offsetS81 + _M0L1iS85;
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2atmpS4202;
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2atmpS1826;
        struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder* _M0L6_2aoldS4201;
        int32_t _M0L6_2atmpS1828;
        if (
          _M0L6_2atmpS1827 < 0
          || _M0L6_2atmpS1827 >= Moonbit_array_length(_M0L3srcS79)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4202
        = (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*)_M0L3srcS79[
            _M0L6_2atmpS1827
          ];
        _M0L6_2atmpS1826 = _M0L6_2atmpS4202;
        if (
          _M0L6_2atmpS1825 < 0
          || _M0L6_2atmpS1825 >= Moonbit_array_length(_M0L3dstS78)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4201
        = (struct _M0TP48clawteam8clawteam8internal6openai27ChatCompletionChoiceBuilder*)_M0L3dstS78[
            _M0L6_2atmpS1825
          ];
        if (_M0L6_2atmpS1826) {
          moonbit_incref(_M0L6_2atmpS1826);
        }
        if (_M0L6_2aoldS4201) {
          moonbit_decref(_M0L6_2aoldS4201);
        }
        _M0L3dstS78[_M0L6_2atmpS1825] = _M0L6_2atmpS1826;
        _M0L6_2atmpS1828 = _M0L1iS85 - 1;
        _M0L1iS85 = _M0L6_2atmpS1828;
        continue;
      } else {
        moonbit_decref(_M0L3srcS79);
        moonbit_decref(_M0L3dstS78);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGRP48clawteam8clawteam8internal6openai20ChatCompletionChoiceEE(
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L3dstS87,
  int32_t _M0L11dst__offsetS89,
  struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice** _M0L3srcS88,
  int32_t _M0L11src__offsetS90,
  int32_t _M0L3lenS92
) {
  int32_t _if__result_4621;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS87 == _M0L3srcS88) {
    _if__result_4621 = _M0L11dst__offsetS89 < _M0L11src__offsetS90;
  } else {
    _if__result_4621 = 0;
  }
  if (_if__result_4621) {
    int32_t _M0L1iS91 = 0;
    while (1) {
      if (_M0L1iS91 < _M0L3lenS92) {
        int32_t _M0L6_2atmpS1830 = _M0L11dst__offsetS89 + _M0L1iS91;
        int32_t _M0L6_2atmpS1832 = _M0L11src__offsetS90 + _M0L1iS91;
        struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2atmpS4204;
        struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2atmpS1831;
        struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2aoldS4203;
        int32_t _M0L6_2atmpS1833;
        if (
          _M0L6_2atmpS1832 < 0
          || _M0L6_2atmpS1832 >= Moonbit_array_length(_M0L3srcS88)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4204
        = (struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*)_M0L3srcS88[
            _M0L6_2atmpS1832
          ];
        _M0L6_2atmpS1831 = _M0L6_2atmpS4204;
        if (
          _M0L6_2atmpS1830 < 0
          || _M0L6_2atmpS1830 >= Moonbit_array_length(_M0L3dstS87)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4203
        = (struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*)_M0L3dstS87[
            _M0L6_2atmpS1830
          ];
        if (_M0L6_2atmpS1831) {
          moonbit_incref(_M0L6_2atmpS1831);
        }
        if (_M0L6_2aoldS4203) {
          moonbit_decref(_M0L6_2aoldS4203);
        }
        _M0L3dstS87[_M0L6_2atmpS1830] = _M0L6_2atmpS1831;
        _M0L6_2atmpS1833 = _M0L1iS91 + 1;
        _M0L1iS91 = _M0L6_2atmpS1833;
        continue;
      } else {
        moonbit_decref(_M0L3srcS88);
        moonbit_decref(_M0L3dstS87);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1838 = _M0L3lenS92 - 1;
    int32_t _M0L1iS94 = _M0L6_2atmpS1838;
    while (1) {
      if (_M0L1iS94 >= 0) {
        int32_t _M0L6_2atmpS1834 = _M0L11dst__offsetS89 + _M0L1iS94;
        int32_t _M0L6_2atmpS1836 = _M0L11src__offsetS90 + _M0L1iS94;
        struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2atmpS4206;
        struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2atmpS1835;
        struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice* _M0L6_2aoldS4205;
        int32_t _M0L6_2atmpS1837;
        if (
          _M0L6_2atmpS1836 < 0
          || _M0L6_2atmpS1836 >= Moonbit_array_length(_M0L3srcS88)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4206
        = (struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*)_M0L3srcS88[
            _M0L6_2atmpS1836
          ];
        _M0L6_2atmpS1835 = _M0L6_2atmpS4206;
        if (
          _M0L6_2atmpS1834 < 0
          || _M0L6_2atmpS1834 >= Moonbit_array_length(_M0L3dstS87)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4205
        = (struct _M0TP48clawteam8clawteam8internal6openai20ChatCompletionChoice*)_M0L3dstS87[
            _M0L6_2atmpS1834
          ];
        if (_M0L6_2atmpS1835) {
          moonbit_incref(_M0L6_2atmpS1835);
        }
        if (_M0L6_2aoldS4205) {
          moonbit_decref(_M0L6_2aoldS4205);
        }
        _M0L3dstS87[_M0L6_2atmpS1834] = _M0L6_2atmpS1835;
        _M0L6_2atmpS1837 = _M0L1iS94 - 1;
        _M0L1iS94 = _M0L6_2atmpS1837;
        continue;
      } else {
        moonbit_decref(_M0L3srcS88);
        moonbit_decref(_M0L3dstS87);
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
  moonbit_string_t _M0L6_2atmpS1756;
  moonbit_string_t _M0L6_2atmpS4209;
  moonbit_string_t _M0L6_2atmpS1754;
  moonbit_string_t _M0L6_2atmpS1755;
  moonbit_string_t _M0L6_2atmpS4208;
  moonbit_string_t _M0L6_2atmpS1753;
  moonbit_string_t _M0L6_2atmpS4207;
  moonbit_string_t _M0L6_2atmpS1752;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1756 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4209
  = moonbit_add_string(_M0L6_2atmpS1756, (moonbit_string_t)moonbit_string_literal_132.data);
  moonbit_decref(_M0L6_2atmpS1756);
  _M0L6_2atmpS1754 = _M0L6_2atmpS4209;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1755
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4208 = moonbit_add_string(_M0L6_2atmpS1754, _M0L6_2atmpS1755);
  moonbit_decref(_M0L6_2atmpS1754);
  moonbit_decref(_M0L6_2atmpS1755);
  _M0L6_2atmpS1753 = _M0L6_2atmpS4208;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4207
  = moonbit_add_string(_M0L6_2atmpS1753, (moonbit_string_t)moonbit_string_literal_84.data);
  moonbit_decref(_M0L6_2atmpS1753);
  _M0L6_2atmpS1752 = _M0L6_2atmpS4207;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1752);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS1761;
  moonbit_string_t _M0L6_2atmpS4212;
  moonbit_string_t _M0L6_2atmpS1759;
  moonbit_string_t _M0L6_2atmpS1760;
  moonbit_string_t _M0L6_2atmpS4211;
  moonbit_string_t _M0L6_2atmpS1758;
  moonbit_string_t _M0L6_2atmpS4210;
  moonbit_string_t _M0L6_2atmpS1757;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1761 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4212
  = moonbit_add_string(_M0L6_2atmpS1761, (moonbit_string_t)moonbit_string_literal_132.data);
  moonbit_decref(_M0L6_2atmpS1761);
  _M0L6_2atmpS1759 = _M0L6_2atmpS4212;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1760
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4211 = moonbit_add_string(_M0L6_2atmpS1759, _M0L6_2atmpS1760);
  moonbit_decref(_M0L6_2atmpS1759);
  moonbit_decref(_M0L6_2atmpS1760);
  _M0L6_2atmpS1758 = _M0L6_2atmpS4211;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4210
  = moonbit_add_string(_M0L6_2atmpS1758, (moonbit_string_t)moonbit_string_literal_84.data);
  moonbit_decref(_M0L6_2atmpS1758);
  _M0L6_2atmpS1757 = _M0L6_2atmpS4210;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1757);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS1766;
  moonbit_string_t _M0L6_2atmpS4215;
  moonbit_string_t _M0L6_2atmpS1764;
  moonbit_string_t _M0L6_2atmpS1765;
  moonbit_string_t _M0L6_2atmpS4214;
  moonbit_string_t _M0L6_2atmpS1763;
  moonbit_string_t _M0L6_2atmpS4213;
  moonbit_string_t _M0L6_2atmpS1762;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1766 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4215
  = moonbit_add_string(_M0L6_2atmpS1766, (moonbit_string_t)moonbit_string_literal_132.data);
  moonbit_decref(_M0L6_2atmpS1766);
  _M0L6_2atmpS1764 = _M0L6_2atmpS4215;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1765
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4214 = moonbit_add_string(_M0L6_2atmpS1764, _M0L6_2atmpS1765);
  moonbit_decref(_M0L6_2atmpS1764);
  moonbit_decref(_M0L6_2atmpS1765);
  _M0L6_2atmpS1763 = _M0L6_2atmpS4214;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4213
  = moonbit_add_string(_M0L6_2atmpS1763, (moonbit_string_t)moonbit_string_literal_84.data);
  moonbit_decref(_M0L6_2atmpS1763);
  _M0L6_2atmpS1762 = _M0L6_2atmpS4213;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1762);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS16,
  uint32_t _M0L5valueS17
) {
  uint32_t _M0L3accS1751;
  uint32_t _M0L6_2atmpS1750;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1751 = _M0L4selfS16->$0;
  _M0L6_2atmpS1750 = _M0L3accS1751 + 4u;
  _M0L4selfS16->$0 = _M0L6_2atmpS1750;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS16, _M0L5valueS17);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5inputS15
) {
  uint32_t _M0L3accS1748;
  uint32_t _M0L6_2atmpS1749;
  uint32_t _M0L6_2atmpS1747;
  uint32_t _M0L6_2atmpS1746;
  uint32_t _M0L6_2atmpS1745;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1748 = _M0L4selfS14->$0;
  _M0L6_2atmpS1749 = _M0L5inputS15 * 3266489917u;
  _M0L6_2atmpS1747 = _M0L3accS1748 + _M0L6_2atmpS1749;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1746 = _M0FPB4rotl(_M0L6_2atmpS1747, 17);
  _M0L6_2atmpS1745 = _M0L6_2atmpS1746 * 668265263u;
  _M0L4selfS14->$0 = _M0L6_2atmpS1745;
  moonbit_decref(_M0L4selfS14);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS12, int32_t _M0L1rS13) {
  uint32_t _M0L6_2atmpS1742;
  int32_t _M0L6_2atmpS1744;
  uint32_t _M0L6_2atmpS1743;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1742 = _M0L1xS12 << (_M0L1rS13 & 31);
  _M0L6_2atmpS1744 = 32 - _M0L1rS13;
  _M0L6_2atmpS1743 = _M0L1xS12 >> (_M0L6_2atmpS1744 & 31);
  return _M0L6_2atmpS1742 | _M0L6_2atmpS1743;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S8,
  struct _M0TPB6Logger _M0L10_2ax__4934S11
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS9;
  moonbit_string_t _M0L8_2afieldS4216;
  int32_t _M0L6_2acntS4383;
  moonbit_string_t _M0L15_2a_2aarg__4935S10;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS9
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S8;
  _M0L8_2afieldS4216 = _M0L10_2aFailureS9->$0;
  _M0L6_2acntS4383 = Moonbit_object_header(_M0L10_2aFailureS9)->rc;
  if (_M0L6_2acntS4383 > 1) {
    int32_t _M0L11_2anew__cntS4384 = _M0L6_2acntS4383 - 1;
    Moonbit_object_header(_M0L10_2aFailureS9)->rc = _M0L11_2anew__cntS4384;
    moonbit_incref(_M0L8_2afieldS4216);
  } else if (_M0L6_2acntS4383 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS9);
  }
  _M0L15_2a_2aarg__4935S10 = _M0L8_2afieldS4216;
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_133.data);
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S11, _M0L15_2a_2aarg__4935S10);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_134.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS7) {
  void* _block_4624;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4624 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4624)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4624)->$0 = _M0L4selfS7;
  return _block_4624;
}

void* _M0MPC14json4Json5array(struct _M0TPB5ArrayGRPB4JsonE* _M0L5arrayS6) {
  void* _block_4625;
  #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4625 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json5Array));
  Moonbit_object_header(_block_4625)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json5Array, $0) >> 2, 1, 5);
  ((struct _M0DTPB4Json5Array*)_block_4625)->$0 = _M0L5arrayS6;
  return _block_4625;
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1698) {
  switch (Moonbit_object_tag(_M0L4_2aeS1698)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1698);
      return (moonbit_string_t)moonbit_string_literal_135.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1698);
      return (moonbit_string_t)moonbit_string_literal_136.data;
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1698);
      return (moonbit_string_t)moonbit_string_literal_137.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1698);
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1698);
      return (moonbit_string_t)moonbit_string_literal_138.data;
      break;
    }
  }
}

void* _M0IP48clawteam8clawteam8internal6openai21ChatCompletionMessagePB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1721
) {
  struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage* _M0L7_2aselfS1720 =
    (struct _M0TP48clawteam8clawteam8internal6openai21ChatCompletionMessage*)_M0L11_2aobj__ptrS1721;
  return _M0IP48clawteam8clawteam8internal6openai21ChatCompletionMessagePB6ToJson8to__json(_M0L7_2aselfS1720);
}

void* _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1719
) {
  struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall* _M0L7_2aselfS1718 =
    (struct _M0TP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCall*)_M0L11_2aobj__ptrS1719;
  return _M0IP48clawteam8clawteam8internal6openai29ChatCompletionMessageToolCallPB6ToJson8to__json(_M0L7_2aselfS1718);
}

void* _M0IP48clawteam8clawteam8internal6openai14ChatCompletionPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1717
) {
  struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion* _M0L7_2aselfS1716 =
    (struct _M0TP48clawteam8clawteam8internal6openai14ChatCompletion*)_M0L11_2aobj__ptrS1717;
  return _M0IP48clawteam8clawteam8internal6openai14ChatCompletionPB6ToJson8to__json(_M0L7_2aselfS1716);
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1715,
  int32_t _M0L8_2aparamS1714
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1713 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1715;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1713, _M0L8_2aparamS1714);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1712,
  struct _M0TPC16string10StringView _M0L8_2aparamS1711
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1710 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1712;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1710, _M0L8_2aparamS1711);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1709,
  moonbit_string_t _M0L8_2aparamS1706,
  int32_t _M0L8_2aparamS1707,
  int32_t _M0L8_2aparamS1708
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1705 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1709;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1705, _M0L8_2aparamS1706, _M0L8_2aparamS1707, _M0L8_2aparamS1708);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1704,
  moonbit_string_t _M0L8_2aparamS1703
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1702 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1704;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1702, _M0L8_2aparamS1703);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1741 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1740;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1733;
  moonbit_string_t* _M0L6_2atmpS1739;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1738;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1734;
  moonbit_string_t* _M0L6_2atmpS1737;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1736;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1735;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1625;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1732;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1731;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1730;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1729;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1624;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1728;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1727;
  _M0L6_2atmpS1741[0] = (moonbit_string_t)moonbit_string_literal_139.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal6openai55____test__6275696c6465725f7762746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1740
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1740)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1740->$0
  = _M0FP48clawteam8clawteam8internal6openai55____test__6275696c6465725f7762746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1740->$1 = _M0L6_2atmpS1741;
  _M0L8_2atupleS1733
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1733)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1733->$0 = 0;
  _M0L8_2atupleS1733->$1 = _M0L8_2atupleS1740;
  _M0L6_2atmpS1739 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1739[0] = (moonbit_string_t)moonbit_string_literal_140.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal6openai55____test__6275696c6465725f7762746573742e6d6274__1_2eclo);
  _M0L8_2atupleS1738
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1738)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1738->$0
  = _M0FP48clawteam8clawteam8internal6openai55____test__6275696c6465725f7762746573742e6d6274__1_2eclo;
  _M0L8_2atupleS1738->$1 = _M0L6_2atmpS1739;
  _M0L8_2atupleS1734
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1734)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1734->$0 = 1;
  _M0L8_2atupleS1734->$1 = _M0L8_2atupleS1738;
  _M0L6_2atmpS1737 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1737[0] = (moonbit_string_t)moonbit_string_literal_141.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal6openai55____test__6275696c6465725f7762746573742e6d6274__2_2eclo);
  _M0L8_2atupleS1736
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1736)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1736->$0
  = _M0FP48clawteam8clawteam8internal6openai55____test__6275696c6465725f7762746573742e6d6274__2_2eclo;
  _M0L8_2atupleS1736->$1 = _M0L6_2atmpS1737;
  _M0L8_2atupleS1735
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1735)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1735->$0 = 2;
  _M0L8_2atupleS1735->$1 = _M0L8_2atupleS1736;
  _M0L7_2abindS1625
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1625[0] = _M0L8_2atupleS1733;
  _M0L7_2abindS1625[1] = _M0L8_2atupleS1734;
  _M0L7_2abindS1625[2] = _M0L8_2atupleS1735;
  _M0L6_2atmpS1732 = _M0L7_2abindS1625;
  _M0L6_2atmpS1731
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 3, _M0L6_2atmpS1732
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1730
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1731);
  _M0L8_2atupleS1729
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1729)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1729->$0 = (moonbit_string_t)moonbit_string_literal_142.data;
  _M0L8_2atupleS1729->$1 = _M0L6_2atmpS1730;
  _M0L7_2abindS1624
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1624[0] = _M0L8_2atupleS1729;
  _M0L6_2atmpS1728 = _M0L7_2abindS1624;
  _M0L6_2atmpS1727
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 1, _M0L6_2atmpS1728
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0FP48clawteam8clawteam8internal6openai48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1727);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1726;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1692;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1693;
  int32_t _M0L7_2abindS1694;
  int32_t _M0L2__S1695;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1726
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1692
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1692)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1692->$0 = _M0L6_2atmpS1726;
  _M0L12async__testsS1692->$1 = 0;
  #line 440 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS1693
  = _M0FP48clawteam8clawteam8internal6openai52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1694 = _M0L7_2abindS1693->$1;
  _M0L2__S1695 = 0;
  while (1) {
    if (_M0L2__S1695 < _M0L7_2abindS1694) {
      struct _M0TUsiE** _M0L8_2afieldS4220 = _M0L7_2abindS1693->$0;
      struct _M0TUsiE** _M0L3bufS1725 = _M0L8_2afieldS4220;
      struct _M0TUsiE* _M0L6_2atmpS4219 =
        (struct _M0TUsiE*)_M0L3bufS1725[_M0L2__S1695];
      struct _M0TUsiE* _M0L3argS1696 = _M0L6_2atmpS4219;
      moonbit_string_t _M0L8_2afieldS4218 = _M0L3argS1696->$0;
      moonbit_string_t _M0L6_2atmpS1722 = _M0L8_2afieldS4218;
      int32_t _M0L8_2afieldS4217 = _M0L3argS1696->$1;
      int32_t _M0L6_2atmpS1723 = _M0L8_2afieldS4217;
      int32_t _M0L6_2atmpS1724;
      moonbit_incref(_M0L6_2atmpS1722);
      moonbit_incref(_M0L12async__testsS1692);
      #line 441 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
      _M0FP48clawteam8clawteam8internal6openai44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1692, _M0L6_2atmpS1722, _M0L6_2atmpS1723);
      _M0L6_2atmpS1724 = _M0L2__S1695 + 1;
      _M0L2__S1695 = _M0L6_2atmpS1724;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1693);
    }
    break;
  }
  #line 443 "E:\\moonbit\\clawteam\\internal\\openai\\__generated_driver_for_whitebox_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal6openai28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal6openai34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1692);
  return 0;
}