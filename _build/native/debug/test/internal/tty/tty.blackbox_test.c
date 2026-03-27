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

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2204__l591__;

struct _M0DTPC14json10WriteFrame6Object;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRP48clawteam8clawteam8internal5errno5ErrnoE3Err;

struct _M0DTPB4Json5Array;

struct _M0TWssbEu;

struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0Y4Bool;

struct _M0TUsRPB6LoggerE;

struct _M0TWEOc;

struct _M0TPB13StringBuilder;

struct _M0TPB5ArrayGRPC14json10WriteFrameE;

struct _M0TPB17FloatingDecimal64;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal19tty__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0R38String_3a_3aiter_2eanon__u2001__l247__;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB5ArrayGRPB4JsonE;

struct _M0TPB6Logger;

struct _M0TP48clawteam8clawteam8internal3tty4Size;

struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3532__l427__;

struct _M0TWEuQRPC15error5Error;

struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3528__l428__;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l570__;

struct _M0TPB19MulShiftAll64Result;

struct _M0TWEOUsRPB4JsonE;

struct _M0R126_24clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPB4Json6Number;

struct _M0DTPC14json10WriteFrame5Array;

struct _M0BTPB6ToJson;

struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRPC15error5ErrorE2Ok;

struct _M0TPB6ToJson;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPB4Json6String;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TUsRPB4JsonE;

struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0DTPB4Json6Object;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0KTPB6ToJsonS4Bool;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TPC15bytes9BytesView;

struct _M0TWsRPB4JsonEORPB4Json;

struct _M0TWuEu;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal19tty__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPC16string10StringView;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB5EntryGsRPB4JsonE;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno;

struct _M0TPB3MapGsRPB4JsonE;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRP48clawteam8clawteam8internal5errno5ErrnoE2Ok;

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRPC15error5ErrorE3Err;

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

struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2204__l591__ {
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

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRP48clawteam8clawteam8internal5errno5ErrnoE3Err {
  void* $0;
  
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

struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
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

struct _M0Y4Bool {
  int32_t $0;
  
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal19tty__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0R38String_3a_3aiter_2eanon__u2001__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
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

struct _M0TPB5ArrayGRPB4JsonE {
  int32_t $1;
  void** $0;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TP48clawteam8clawteam8internal3tty4Size {
  int32_t $0;
  int32_t $1;
  
};

struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3532__l427__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3528__l428__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TPB19MulShiftAll64Result {
  uint64_t $0;
  uint64_t $1;
  uint64_t $2;
  
};

struct _M0TWEOUsRPB4JsonE {
  struct _M0TUsRPB4JsonE*(* code)(struct _M0TWEOUsRPB4JsonE*);
  
};

struct _M0R126_24clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362 {
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

struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRPC15error5ErrorE2Ok {
  struct _M0TP48clawteam8clawteam8internal3tty4Size* $0;
  
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

struct _M0KTPB6ToJsonS4Bool {
  struct _M0BTPB6ToJson* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPC15bytes9BytesView {
  int32_t $1;
  int32_t $2;
  moonbit_bytes_t $0;
  
};

struct _M0TWsRPB4JsonEORPB4Json {
  void*(* code)(struct _M0TWsRPB4JsonEORPB4Json*, moonbit_string_t, void*);
  
};

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal19tty__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno {
  int32_t $0;
  
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

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRP48clawteam8clawteam8internal5errno5ErrnoE2Ok {
  struct _M0TP48clawteam8clawteam8internal3tty4Size* $0;
  
};

struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRPC15error5ErrorE3Err {
  void* $0;
  
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
  union { struct _M0TP48clawteam8clawteam8internal3tty4Size* ok; void* err; 
  } data;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal19tty__blackbox__test47____test__7474795f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1371(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1362(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal19tty__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal19tty__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testC3532l427(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam8internal19tty__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testC3528l428(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1295(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1290(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1283(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1277(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal19tty__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal19tty__blackbox__test37____test__7474795f746573742e6d6274__0(
  
);

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3tty12window__size();

#define _M0FP48clawteam8clawteam8internal3tty19tty__get__win__size moonbit_moonclaw_tty_get_win_size

int32_t _M0IP48clawteam8clawteam8internal5errno5ErrnoPB4Show6output(
  void*,
  struct _M0TPB6Logger
);

#define _M0FP48clawteam8clawteam8internal5errno15errno__strerror moonbit_moonclaw_errno_strerror

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

moonbit_string_t _M0FPC28encoding4utf821decode__lossy_2einner(
  struct _M0TPC15bytes9BytesView,
  int32_t
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

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15bytes9BytesView6length(struct _M0TPC15bytes9BytesView);

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

void* _M0IPC14bool4BoolPB6ToJson8to__json(int32_t);

void* _M0MPC14json4Json7boolean(int32_t);

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE*
);

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2204l591(
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

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2020l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC16uint646UInt64PB4Show6output(uint64_t, struct _M0TPB6Logger);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC2001l247(struct _M0TWEOc*);

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

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRP48clawteam8clawteam8internal5errno5ErrnoE(
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

struct _M0TPC15bytes9BytesView _M0FPB5abortGRPC15bytes9BytesViewE(
  moonbit_string_t,
  moonbit_string_t
);

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

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
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

void* _M0IPC14bool4BoolPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void*
);

int32_t moonbit_moonclaw_c_load_byte(void*, int32_t);

int32_t moonbit_moonclaw_tty_get_win_size(int32_t*);

int32_t moonbit_moonclaw_c_is_null(void*);

void* moonbit_moonclaw_errno_strerror(int32_t);

uint64_t moonbit_moonclaw_c_strlen(void*);

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
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    92, 117, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    123, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 116, 116, 121, 34, 
    44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    73, 110, 102, 105, 110, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    78, 97, 78, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    10, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 51, 51, 
    58, 53, 45, 54, 51, 51, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_44 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 100, 111, 
    117, 98, 108, 101, 95, 114, 121, 117, 95, 110, 111, 110, 106, 115, 
    46, 109, 98, 116, 58, 49, 49, 54, 58, 51, 45, 49, 49, 54, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_22 =
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
} const moonbit_string_literal_71 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 56, 
    48, 58, 53, 45, 49, 56, 48, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_32 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 32, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_23 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 116, 
    116, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 116, 116, 121, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 58, 54, 49, 45, 51, 58, 54, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    10, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    73, 108, 108, 101, 103, 97, 108, 65, 114, 103, 117, 109, 101, 110, 
    116, 69, 120, 99, 101, 112, 116, 105, 111, 110, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    119, 105, 110, 100, 111, 119, 115, 95, 115, 105, 122, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_64 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    116, 116, 121, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_14 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 102, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_48 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 45, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    10, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[111]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 110), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 116, 116, 
    121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 
    46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 
    105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 
    69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 
    101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 
    110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    69, 114, 114, 110, 111, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    10, 32, 32, 32, 32, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    44, 32, 34, 109, 111, 100, 101, 34, 58, 32, 34, 106, 115, 111, 110, 
    34, 125, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    10, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[68]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 67), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 116, 
    116, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 116, 116, 121, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 58, 51, 45, 51, 58, 54, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    10, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    116, 116, 121, 95, 116, 101, 115, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 116, 
    116, 121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 
    116, 58, 116, 116, 121, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    58, 51, 58, 49, 54, 45, 51, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_61 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 47, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 92, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[113]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 112), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 116, 116, 
    121, 95, 98, 108, 97, 99, 107, 98, 111, 120, 95, 116, 101, 115, 116, 
    46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 
    105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 
    105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 
    84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 
    114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    10, 32, 32, 32, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_19 =
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

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal19tty__blackbox__test47____test__7474795f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal19tty__blackbox__test47____test__7474795f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1371$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1371
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal19tty__blackbox__test43____test__7474795f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal19tty__blackbox__test47____test__7474795f746573742e6d6274__0_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB6ToJson data; 
} _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB6ToJson) >> 2, 0, 0),
    {.$method_0 = _M0IPC14bool4BoolPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson}
  };

struct _M0BTPB6ToJson* _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id =
  &_M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id$object.data;

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
} _M0FPB30ryu__to__string_2erecord_2f900$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0),
    {.$0 = 0ull, .$1 = 0}
  };

struct _M0TPB17FloatingDecimal64* _M0FPB30ryu__to__string_2erecord_2f900 =
  &_M0FPB30ryu__to__string_2erecord_2f900$object.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal19tty__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal19tty__blackbox__test47____test__7474795f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3563
) {
  return _M0FP48clawteam8clawteam8internal19tty__blackbox__test37____test__7474795f746573742e6d6274__0();
}

int32_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1392,
  moonbit_string_t _M0L8filenameS1367,
  int32_t _M0L5indexS1370
) {
  struct _M0R126_24clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362* _closure_4216;
  struct _M0TWssbEu* _M0L14handle__resultS1362;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1371;
  void* _M0L11_2atry__errS1386;
  struct moonbit_result_0 _tmp_4218;
  int32_t _handle__error__result_4219;
  int32_t _M0L6_2atmpS3551;
  void* _M0L3errS1387;
  moonbit_string_t _M0L4nameS1389;
  struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1390;
  moonbit_string_t _M0L8_2afieldS3564;
  int32_t _M0L6_2acntS4133;
  moonbit_string_t _M0L7_2anameS1391;
  #line 526 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS1367);
  _closure_4216
  = (struct _M0R126_24clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362*)moonbit_malloc(sizeof(struct _M0R126_24clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362));
  Moonbit_object_header(_closure_4216)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R126_24clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362, $1) >> 2, 1, 0);
  _closure_4216->code
  = &_M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1362;
  _closure_4216->$0 = _M0L5indexS1370;
  _closure_4216->$1 = _M0L8filenameS1367;
  _M0L14handle__resultS1362 = (struct _M0TWssbEu*)_closure_4216;
  _M0L17error__to__stringS1371
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1371$closure.data;
  moonbit_incref(_M0L12async__testsS1392);
  moonbit_incref(_M0L17error__to__stringS1371);
  moonbit_incref(_M0L8filenameS1367);
  moonbit_incref(_M0L14handle__resultS1362);
  #line 560 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _tmp_4218
  = _M0IP48clawteam8clawteam8internal19tty__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS1392, _M0L8filenameS1367, _M0L5indexS1370, _M0L14handle__resultS1362, _M0L17error__to__stringS1371);
  if (_tmp_4218.tag) {
    int32_t const _M0L5_2aokS3560 = _tmp_4218.data.ok;
    _handle__error__result_4219 = _M0L5_2aokS3560;
  } else {
    void* const _M0L6_2aerrS3561 = _tmp_4218.data.err;
    moonbit_decref(_M0L12async__testsS1392);
    moonbit_decref(_M0L17error__to__stringS1371);
    moonbit_decref(_M0L8filenameS1367);
    _M0L11_2atry__errS1386 = _M0L6_2aerrS3561;
    goto join_1385;
  }
  if (_handle__error__result_4219) {
    moonbit_decref(_M0L12async__testsS1392);
    moonbit_decref(_M0L17error__to__stringS1371);
    moonbit_decref(_M0L8filenameS1367);
    _M0L6_2atmpS3551 = 1;
  } else {
    struct moonbit_result_0 _tmp_4220;
    int32_t _handle__error__result_4221;
    moonbit_incref(_M0L12async__testsS1392);
    moonbit_incref(_M0L17error__to__stringS1371);
    moonbit_incref(_M0L8filenameS1367);
    moonbit_incref(_M0L14handle__resultS1362);
    #line 563 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
    _tmp_4220
    = _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1392, _M0L8filenameS1367, _M0L5indexS1370, _M0L14handle__resultS1362, _M0L17error__to__stringS1371);
    if (_tmp_4220.tag) {
      int32_t const _M0L5_2aokS3558 = _tmp_4220.data.ok;
      _handle__error__result_4221 = _M0L5_2aokS3558;
    } else {
      void* const _M0L6_2aerrS3559 = _tmp_4220.data.err;
      moonbit_decref(_M0L12async__testsS1392);
      moonbit_decref(_M0L17error__to__stringS1371);
      moonbit_decref(_M0L8filenameS1367);
      _M0L11_2atry__errS1386 = _M0L6_2aerrS3559;
      goto join_1385;
    }
    if (_handle__error__result_4221) {
      moonbit_decref(_M0L12async__testsS1392);
      moonbit_decref(_M0L17error__to__stringS1371);
      moonbit_decref(_M0L8filenameS1367);
      _M0L6_2atmpS3551 = 1;
    } else {
      struct moonbit_result_0 _tmp_4222;
      int32_t _handle__error__result_4223;
      moonbit_incref(_M0L12async__testsS1392);
      moonbit_incref(_M0L17error__to__stringS1371);
      moonbit_incref(_M0L8filenameS1367);
      moonbit_incref(_M0L14handle__resultS1362);
      #line 566 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _tmp_4222
      = _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1392, _M0L8filenameS1367, _M0L5indexS1370, _M0L14handle__resultS1362, _M0L17error__to__stringS1371);
      if (_tmp_4222.tag) {
        int32_t const _M0L5_2aokS3556 = _tmp_4222.data.ok;
        _handle__error__result_4223 = _M0L5_2aokS3556;
      } else {
        void* const _M0L6_2aerrS3557 = _tmp_4222.data.err;
        moonbit_decref(_M0L12async__testsS1392);
        moonbit_decref(_M0L17error__to__stringS1371);
        moonbit_decref(_M0L8filenameS1367);
        _M0L11_2atry__errS1386 = _M0L6_2aerrS3557;
        goto join_1385;
      }
      if (_handle__error__result_4223) {
        moonbit_decref(_M0L12async__testsS1392);
        moonbit_decref(_M0L17error__to__stringS1371);
        moonbit_decref(_M0L8filenameS1367);
        _M0L6_2atmpS3551 = 1;
      } else {
        struct moonbit_result_0 _tmp_4224;
        int32_t _handle__error__result_4225;
        moonbit_incref(_M0L12async__testsS1392);
        moonbit_incref(_M0L17error__to__stringS1371);
        moonbit_incref(_M0L8filenameS1367);
        moonbit_incref(_M0L14handle__resultS1362);
        #line 569 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        _tmp_4224
        = _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1392, _M0L8filenameS1367, _M0L5indexS1370, _M0L14handle__resultS1362, _M0L17error__to__stringS1371);
        if (_tmp_4224.tag) {
          int32_t const _M0L5_2aokS3554 = _tmp_4224.data.ok;
          _handle__error__result_4225 = _M0L5_2aokS3554;
        } else {
          void* const _M0L6_2aerrS3555 = _tmp_4224.data.err;
          moonbit_decref(_M0L12async__testsS1392);
          moonbit_decref(_M0L17error__to__stringS1371);
          moonbit_decref(_M0L8filenameS1367);
          _M0L11_2atry__errS1386 = _M0L6_2aerrS3555;
          goto join_1385;
        }
        if (_handle__error__result_4225) {
          moonbit_decref(_M0L12async__testsS1392);
          moonbit_decref(_M0L17error__to__stringS1371);
          moonbit_decref(_M0L8filenameS1367);
          _M0L6_2atmpS3551 = 1;
        } else {
          struct moonbit_result_0 _tmp_4226;
          moonbit_incref(_M0L14handle__resultS1362);
          #line 572 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
          _tmp_4226
          = _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1392, _M0L8filenameS1367, _M0L5indexS1370, _M0L14handle__resultS1362, _M0L17error__to__stringS1371);
          if (_tmp_4226.tag) {
            int32_t const _M0L5_2aokS3552 = _tmp_4226.data.ok;
            _M0L6_2atmpS3551 = _M0L5_2aokS3552;
          } else {
            void* const _M0L6_2aerrS3553 = _tmp_4226.data.err;
            _M0L11_2atry__errS1386 = _M0L6_2aerrS3553;
            goto join_1385;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3551) {
    void* _M0L124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3562 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3562)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 3);
    ((struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3562)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1386
    = _M0L124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3562;
    goto join_1385;
  } else {
    moonbit_decref(_M0L14handle__resultS1362);
  }
  goto joinlet_4217;
  join_1385:;
  _M0L3errS1387 = _M0L11_2atry__errS1386;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1390
  = (struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1387;
  _M0L8_2afieldS3564 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1390->$0;
  _M0L6_2acntS4133
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1390)->rc;
  if (_M0L6_2acntS4133 > 1) {
    int32_t _M0L11_2anew__cntS4134 = _M0L6_2acntS4133 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1390)->rc
    = _M0L11_2anew__cntS4134;
    moonbit_incref(_M0L8_2afieldS3564);
  } else if (_M0L6_2acntS4133 == 1) {
    #line 579 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1390);
  }
  _M0L7_2anameS1391 = _M0L8_2afieldS3564;
  _M0L4nameS1389 = _M0L7_2anameS1391;
  goto join_1388;
  goto joinlet_4227;
  join_1388:;
  #line 580 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1362(_M0L14handle__resultS1362, _M0L4nameS1389, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_4227:;
  joinlet_4217:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1371(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3550,
  void* _M0L3errS1372
) {
  void* _M0L1eS1374;
  moonbit_string_t _M0L1eS1376;
  #line 549 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS3550);
  switch (Moonbit_object_tag(_M0L3errS1372)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1377 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1372;
      moonbit_string_t _M0L8_2afieldS3565 = _M0L10_2aFailureS1377->$0;
      int32_t _M0L6_2acntS4135 =
        Moonbit_object_header(_M0L10_2aFailureS1377)->rc;
      moonbit_string_t _M0L4_2aeS1378;
      if (_M0L6_2acntS4135 > 1) {
        int32_t _M0L11_2anew__cntS4136 = _M0L6_2acntS4135 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1377)->rc
        = _M0L11_2anew__cntS4136;
        moonbit_incref(_M0L8_2afieldS3565);
      } else if (_M0L6_2acntS4135 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1377);
      }
      _M0L4_2aeS1378 = _M0L8_2afieldS3565;
      _M0L1eS1376 = _M0L4_2aeS1378;
      goto join_1375;
      break;
    }
    
    case 2: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1379 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1372;
      moonbit_string_t _M0L8_2afieldS3566 = _M0L15_2aInspectErrorS1379->$0;
      int32_t _M0L6_2acntS4137 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1379)->rc;
      moonbit_string_t _M0L4_2aeS1380;
      if (_M0L6_2acntS4137 > 1) {
        int32_t _M0L11_2anew__cntS4138 = _M0L6_2acntS4137 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1379)->rc
        = _M0L11_2anew__cntS4138;
        moonbit_incref(_M0L8_2afieldS3566);
      } else if (_M0L6_2acntS4137 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1379);
      }
      _M0L4_2aeS1380 = _M0L8_2afieldS3566;
      _M0L1eS1376 = _M0L4_2aeS1380;
      goto join_1375;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1381 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1372;
      moonbit_string_t _M0L8_2afieldS3567 = _M0L16_2aSnapshotErrorS1381->$0;
      int32_t _M0L6_2acntS4139 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1381)->rc;
      moonbit_string_t _M0L4_2aeS1382;
      if (_M0L6_2acntS4139 > 1) {
        int32_t _M0L11_2anew__cntS4140 = _M0L6_2acntS4139 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1381)->rc
        = _M0L11_2anew__cntS4140;
        moonbit_incref(_M0L8_2afieldS3567);
      } else if (_M0L6_2acntS4139 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1381);
      }
      _M0L4_2aeS1382 = _M0L8_2afieldS3567;
      _M0L1eS1376 = _M0L4_2aeS1382;
      goto join_1375;
      break;
    }
    
    case 5: {
      struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1383 =
        (struct _M0DTPC15error5Error122clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1372;
      moonbit_string_t _M0L8_2afieldS3568 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1383->$0;
      int32_t _M0L6_2acntS4141 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1383)->rc;
      moonbit_string_t _M0L4_2aeS1384;
      if (_M0L6_2acntS4141 > 1) {
        int32_t _M0L11_2anew__cntS4142 = _M0L6_2acntS4141 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1383)->rc
        = _M0L11_2anew__cntS4142;
        moonbit_incref(_M0L8_2afieldS3568);
      } else if (_M0L6_2acntS4141 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1383);
      }
      _M0L4_2aeS1384 = _M0L8_2afieldS3568;
      _M0L1eS1376 = _M0L4_2aeS1384;
      goto join_1375;
      break;
    }
    default: {
      _M0L1eS1374 = _M0L3errS1372;
      goto join_1373;
      break;
    }
  }
  join_1375:;
  return _M0L1eS1376;
  join_1373:;
  #line 555 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1374);
}

int32_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1362(
  struct _M0TWssbEu* _M0L6_2aenvS3536,
  moonbit_string_t _M0L8testnameS1363,
  moonbit_string_t _M0L7messageS1364,
  int32_t _M0L7skippedS1365
) {
  struct _M0R126_24clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362* _M0L14_2acasted__envS3537;
  moonbit_string_t _M0L8_2afieldS3578;
  moonbit_string_t _M0L8filenameS1367;
  int32_t _M0L8_2afieldS3577;
  int32_t _M0L6_2acntS4143;
  int32_t _M0L5indexS1370;
  int32_t _if__result_4230;
  moonbit_string_t _M0L10file__nameS1366;
  moonbit_string_t _M0L10test__nameS1368;
  moonbit_string_t _M0L7messageS1369;
  moonbit_string_t _M0L6_2atmpS3549;
  moonbit_string_t _M0L6_2atmpS3576;
  moonbit_string_t _M0L6_2atmpS3548;
  moonbit_string_t _M0L6_2atmpS3575;
  moonbit_string_t _M0L6_2atmpS3546;
  moonbit_string_t _M0L6_2atmpS3547;
  moonbit_string_t _M0L6_2atmpS3574;
  moonbit_string_t _M0L6_2atmpS3545;
  moonbit_string_t _M0L6_2atmpS3573;
  moonbit_string_t _M0L6_2atmpS3543;
  moonbit_string_t _M0L6_2atmpS3544;
  moonbit_string_t _M0L6_2atmpS3572;
  moonbit_string_t _M0L6_2atmpS3542;
  moonbit_string_t _M0L6_2atmpS3571;
  moonbit_string_t _M0L6_2atmpS3540;
  moonbit_string_t _M0L6_2atmpS3541;
  moonbit_string_t _M0L6_2atmpS3570;
  moonbit_string_t _M0L6_2atmpS3539;
  moonbit_string_t _M0L6_2atmpS3569;
  moonbit_string_t _M0L6_2atmpS3538;
  #line 533 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3537
  = (struct _M0R126_24clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362*)_M0L6_2aenvS3536;
  _M0L8_2afieldS3578 = _M0L14_2acasted__envS3537->$1;
  _M0L8filenameS1367 = _M0L8_2afieldS3578;
  _M0L8_2afieldS3577 = _M0L14_2acasted__envS3537->$0;
  _M0L6_2acntS4143 = Moonbit_object_header(_M0L14_2acasted__envS3537)->rc;
  if (_M0L6_2acntS4143 > 1) {
    int32_t _M0L11_2anew__cntS4144 = _M0L6_2acntS4143 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3537)->rc
    = _M0L11_2anew__cntS4144;
    moonbit_incref(_M0L8filenameS1367);
  } else if (_M0L6_2acntS4143 == 1) {
    #line 533 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3537);
  }
  _M0L5indexS1370 = _M0L8_2afieldS3577;
  if (!_M0L7skippedS1365) {
    _if__result_4230 = 1;
  } else {
    _if__result_4230 = 0;
  }
  if (_if__result_4230) {
    
  }
  #line 539 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS1366 = _M0MPC16string6String6escape(_M0L8filenameS1367);
  #line 540 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS1368 = _M0MPC16string6String6escape(_M0L8testnameS1363);
  #line 541 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS1369 = _M0MPC16string6String6escape(_M0L7messageS1364);
  #line 542 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 544 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3549
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1366);
  #line 543 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3576
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3549);
  moonbit_decref(_M0L6_2atmpS3549);
  _M0L6_2atmpS3548 = _M0L6_2atmpS3576;
  #line 543 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3575
  = moonbit_add_string(_M0L6_2atmpS3548, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3548);
  _M0L6_2atmpS3546 = _M0L6_2atmpS3575;
  #line 544 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3547
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1370);
  #line 543 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3574 = moonbit_add_string(_M0L6_2atmpS3546, _M0L6_2atmpS3547);
  moonbit_decref(_M0L6_2atmpS3546);
  moonbit_decref(_M0L6_2atmpS3547);
  _M0L6_2atmpS3545 = _M0L6_2atmpS3574;
  #line 543 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3573
  = moonbit_add_string(_M0L6_2atmpS3545, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3545);
  _M0L6_2atmpS3543 = _M0L6_2atmpS3573;
  #line 544 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3544
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1368);
  #line 543 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3572 = moonbit_add_string(_M0L6_2atmpS3543, _M0L6_2atmpS3544);
  moonbit_decref(_M0L6_2atmpS3543);
  moonbit_decref(_M0L6_2atmpS3544);
  _M0L6_2atmpS3542 = _M0L6_2atmpS3572;
  #line 543 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3571
  = moonbit_add_string(_M0L6_2atmpS3542, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3542);
  _M0L6_2atmpS3540 = _M0L6_2atmpS3571;
  #line 544 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3541
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1369);
  #line 543 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3570 = moonbit_add_string(_M0L6_2atmpS3540, _M0L6_2atmpS3541);
  moonbit_decref(_M0L6_2atmpS3540);
  moonbit_decref(_M0L6_2atmpS3541);
  _M0L6_2atmpS3539 = _M0L6_2atmpS3570;
  #line 543 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3569
  = moonbit_add_string(_M0L6_2atmpS3539, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3539);
  _M0L6_2atmpS3538 = _M0L6_2atmpS3569;
  #line 543 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3538);
  #line 546 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal19tty__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1361,
  moonbit_string_t _M0L8filenameS1358,
  int32_t _M0L5indexS1352,
  struct _M0TWssbEu* _M0L14handle__resultS1348,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1350
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1328;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1357;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1330;
  moonbit_string_t* _M0L5attrsS1331;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1351;
  moonbit_string_t _M0L4nameS1334;
  moonbit_string_t _M0L4nameS1332;
  int32_t _M0L6_2atmpS3535;
  struct _M0TWEOs* _M0L5_2aitS1336;
  struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3532__l427__* _closure_4239;
  struct _M0TWEOc* _M0L6_2atmpS3526;
  struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3528__l428__* _closure_4240;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3527;
  struct moonbit_result_0 _result_4241;
  #line 407 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1361);
  moonbit_incref(_M0FP48clawteam8clawteam8internal19tty__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 414 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1357
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal19tty__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1358);
  if (_M0L7_2abindS1357 == 0) {
    struct moonbit_result_0 _result_4232;
    if (_M0L7_2abindS1357) {
      moonbit_decref(_M0L7_2abindS1357);
    }
    moonbit_decref(_M0L17error__to__stringS1350);
    moonbit_decref(_M0L14handle__resultS1348);
    _result_4232.tag = 1;
    _result_4232.data.ok = 0;
    return _result_4232;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1359 =
      _M0L7_2abindS1357;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1360 =
      _M0L7_2aSomeS1359;
    _M0L10index__mapS1328 = _M0L13_2aindex__mapS1360;
    goto join_1327;
  }
  join_1327:;
  #line 416 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1351
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1328, _M0L5indexS1352);
  if (_M0L7_2abindS1351 == 0) {
    struct moonbit_result_0 _result_4234;
    if (_M0L7_2abindS1351) {
      moonbit_decref(_M0L7_2abindS1351);
    }
    moonbit_decref(_M0L17error__to__stringS1350);
    moonbit_decref(_M0L14handle__resultS1348);
    _result_4234.tag = 1;
    _result_4234.data.ok = 0;
    return _result_4234;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1353 =
      _M0L7_2abindS1351;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1354 = _M0L7_2aSomeS1353;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3582 = _M0L4_2axS1354->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1355 = _M0L8_2afieldS3582;
    moonbit_string_t* _M0L8_2afieldS3581 = _M0L4_2axS1354->$1;
    int32_t _M0L6_2acntS4145 = Moonbit_object_header(_M0L4_2axS1354)->rc;
    moonbit_string_t* _M0L8_2aattrsS1356;
    if (_M0L6_2acntS4145 > 1) {
      int32_t _M0L11_2anew__cntS4146 = _M0L6_2acntS4145 - 1;
      Moonbit_object_header(_M0L4_2axS1354)->rc = _M0L11_2anew__cntS4146;
      moonbit_incref(_M0L8_2afieldS3581);
      moonbit_incref(_M0L4_2afS1355);
    } else if (_M0L6_2acntS4145 == 1) {
      #line 414 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS1354);
    }
    _M0L8_2aattrsS1356 = _M0L8_2afieldS3581;
    _M0L1fS1330 = _M0L4_2afS1355;
    _M0L5attrsS1331 = _M0L8_2aattrsS1356;
    goto join_1329;
  }
  join_1329:;
  _M0L6_2atmpS3535 = Moonbit_array_length(_M0L5attrsS1331);
  if (_M0L6_2atmpS3535 >= 1) {
    moonbit_string_t _M0L6_2atmpS3580 = (moonbit_string_t)_M0L5attrsS1331[0];
    moonbit_string_t _M0L7_2anameS1335 = _M0L6_2atmpS3580;
    moonbit_incref(_M0L7_2anameS1335);
    _M0L4nameS1334 = _M0L7_2anameS1335;
    goto join_1333;
  } else {
    _M0L4nameS1332 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_4235;
  join_1333:;
  _M0L4nameS1332 = _M0L4nameS1334;
  joinlet_4235:;
  #line 417 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS1336 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1331);
  while (1) {
    moonbit_string_t _M0L4attrS1338;
    moonbit_string_t _M0L7_2abindS1345;
    int32_t _M0L6_2atmpS3519;
    int64_t _M0L6_2atmpS3518;
    moonbit_incref(_M0L5_2aitS1336);
    #line 419 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS1345 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1336);
    if (_M0L7_2abindS1345 == 0) {
      if (_M0L7_2abindS1345) {
        moonbit_decref(_M0L7_2abindS1345);
      }
      moonbit_decref(_M0L5_2aitS1336);
    } else {
      moonbit_string_t _M0L7_2aSomeS1346 = _M0L7_2abindS1345;
      moonbit_string_t _M0L7_2aattrS1347 = _M0L7_2aSomeS1346;
      _M0L4attrS1338 = _M0L7_2aattrS1347;
      goto join_1337;
    }
    goto joinlet_4237;
    join_1337:;
    _M0L6_2atmpS3519 = Moonbit_array_length(_M0L4attrS1338);
    _M0L6_2atmpS3518 = (int64_t)_M0L6_2atmpS3519;
    moonbit_incref(_M0L4attrS1338);
    #line 420 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1338, 5, 0, _M0L6_2atmpS3518)
    ) {
      int32_t _M0L6_2atmpS3525 = _M0L4attrS1338[0];
      int32_t _M0L4_2axS1339 = _M0L6_2atmpS3525;
      if (_M0L4_2axS1339 == 112) {
        int32_t _M0L6_2atmpS3524 = _M0L4attrS1338[1];
        int32_t _M0L4_2axS1340 = _M0L6_2atmpS3524;
        if (_M0L4_2axS1340 == 97) {
          int32_t _M0L6_2atmpS3523 = _M0L4attrS1338[2];
          int32_t _M0L4_2axS1341 = _M0L6_2atmpS3523;
          if (_M0L4_2axS1341 == 110) {
            int32_t _M0L6_2atmpS3522 = _M0L4attrS1338[3];
            int32_t _M0L4_2axS1342 = _M0L6_2atmpS3522;
            if (_M0L4_2axS1342 == 105) {
              int32_t _M0L6_2atmpS3579 = _M0L4attrS1338[4];
              int32_t _M0L6_2atmpS3521;
              int32_t _M0L4_2axS1343;
              moonbit_decref(_M0L4attrS1338);
              _M0L6_2atmpS3521 = _M0L6_2atmpS3579;
              _M0L4_2axS1343 = _M0L6_2atmpS3521;
              if (_M0L4_2axS1343 == 99) {
                void* _M0L124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3520;
                struct moonbit_result_0 _result_4238;
                moonbit_decref(_M0L17error__to__stringS1350);
                moonbit_decref(_M0L14handle__resultS1348);
                moonbit_decref(_M0L5_2aitS1336);
                moonbit_decref(_M0L1fS1330);
                _M0L124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3520
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3520)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 3);
                ((struct _M0DTPC15error5Error124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3520)->$0
                = _M0L4nameS1332;
                _result_4238.tag = 0;
                _result_4238.data.err
                = _M0L124clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3520;
                return _result_4238;
              }
            } else {
              moonbit_decref(_M0L4attrS1338);
            }
          } else {
            moonbit_decref(_M0L4attrS1338);
          }
        } else {
          moonbit_decref(_M0L4attrS1338);
        }
      } else {
        moonbit_decref(_M0L4attrS1338);
      }
    } else {
      moonbit_decref(_M0L4attrS1338);
    }
    continue;
    joinlet_4237:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1348);
  moonbit_incref(_M0L4nameS1332);
  _closure_4239
  = (struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3532__l427__*)moonbit_malloc(sizeof(struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3532__l427__));
  Moonbit_object_header(_closure_4239)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3532__l427__, $0) >> 2, 2, 0);
  _closure_4239->code
  = &_M0IP48clawteam8clawteam8internal19tty__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testC3532l427;
  _closure_4239->$0 = _M0L14handle__resultS1348;
  _closure_4239->$1 = _M0L4nameS1332;
  _M0L6_2atmpS3526 = (struct _M0TWEOc*)_closure_4239;
  _closure_4240
  = (struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3528__l428__*)moonbit_malloc(sizeof(struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3528__l428__));
  Moonbit_object_header(_closure_4240)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3528__l428__, $0) >> 2, 3, 0);
  _closure_4240->code
  = &_M0IP48clawteam8clawteam8internal19tty__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testC3528l428;
  _closure_4240->$0 = _M0L17error__to__stringS1350;
  _closure_4240->$1 = _M0L14handle__resultS1348;
  _closure_4240->$2 = _M0L4nameS1332;
  _M0L6_2atmpS3527 = (struct _M0TWRPC15error5ErrorEu*)_closure_4240;
  #line 425 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal19tty__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS1330, _M0L6_2atmpS3526, _M0L6_2atmpS3527);
  _result_4241.tag = 1;
  _result_4241.data.ok = 1;
  return _result_4241;
}

int32_t _M0IP48clawteam8clawteam8internal19tty__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testC3532l427(
  struct _M0TWEOc* _M0L6_2aenvS3533
) {
  struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3532__l427__* _M0L14_2acasted__envS3534;
  moonbit_string_t _M0L8_2afieldS3584;
  moonbit_string_t _M0L4nameS1332;
  struct _M0TWssbEu* _M0L8_2afieldS3583;
  int32_t _M0L6_2acntS4147;
  struct _M0TWssbEu* _M0L14handle__resultS1348;
  #line 427 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3534
  = (struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3532__l427__*)_M0L6_2aenvS3533;
  _M0L8_2afieldS3584 = _M0L14_2acasted__envS3534->$1;
  _M0L4nameS1332 = _M0L8_2afieldS3584;
  _M0L8_2afieldS3583 = _M0L14_2acasted__envS3534->$0;
  _M0L6_2acntS4147 = Moonbit_object_header(_M0L14_2acasted__envS3534)->rc;
  if (_M0L6_2acntS4147 > 1) {
    int32_t _M0L11_2anew__cntS4148 = _M0L6_2acntS4147 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3534)->rc
    = _M0L11_2anew__cntS4148;
    moonbit_incref(_M0L4nameS1332);
    moonbit_incref(_M0L8_2afieldS3583);
  } else if (_M0L6_2acntS4147 == 1) {
    #line 427 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3534);
  }
  _M0L14handle__resultS1348 = _M0L8_2afieldS3583;
  #line 427 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1348->code(_M0L14handle__resultS1348, _M0L4nameS1332, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal19tty__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testC3528l428(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3529,
  void* _M0L3errS1349
) {
  struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3528__l428__* _M0L14_2acasted__envS3530;
  moonbit_string_t _M0L8_2afieldS3587;
  moonbit_string_t _M0L4nameS1332;
  struct _M0TWssbEu* _M0L8_2afieldS3586;
  struct _M0TWssbEu* _M0L14handle__resultS1348;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3585;
  int32_t _M0L6_2acntS4149;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1350;
  moonbit_string_t _M0L6_2atmpS3531;
  #line 428 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS3530
  = (struct _M0R221_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2ftty__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3528__l428__*)_M0L6_2aenvS3529;
  _M0L8_2afieldS3587 = _M0L14_2acasted__envS3530->$2;
  _M0L4nameS1332 = _M0L8_2afieldS3587;
  _M0L8_2afieldS3586 = _M0L14_2acasted__envS3530->$1;
  _M0L14handle__resultS1348 = _M0L8_2afieldS3586;
  _M0L8_2afieldS3585 = _M0L14_2acasted__envS3530->$0;
  _M0L6_2acntS4149 = Moonbit_object_header(_M0L14_2acasted__envS3530)->rc;
  if (_M0L6_2acntS4149 > 1) {
    int32_t _M0L11_2anew__cntS4150 = _M0L6_2acntS4149 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3530)->rc
    = _M0L11_2anew__cntS4150;
    moonbit_incref(_M0L4nameS1332);
    moonbit_incref(_M0L14handle__resultS1348);
    moonbit_incref(_M0L8_2afieldS3585);
  } else if (_M0L6_2acntS4149 == 1) {
    #line 428 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3530);
  }
  _M0L17error__to__stringS1350 = _M0L8_2afieldS3585;
  #line 428 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3531
  = _M0L17error__to__stringS1350->code(_M0L17error__to__stringS1350, _M0L3errS1349);
  #line 428 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1348->code(_M0L14handle__resultS1348, _M0L4nameS1332, _M0L6_2atmpS3531, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1322,
  struct _M0TWEOc* _M0L6on__okS1323,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1320
) {
  void* _M0L11_2atry__errS1318;
  struct moonbit_result_0 _tmp_4243;
  void* _M0L3errS1319;
  #line 375 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _tmp_4243 = _M0L1fS1322->code(_M0L1fS1322);
  if (_tmp_4243.tag) {
    int32_t const _M0L5_2aokS3516 = _tmp_4243.data.ok;
    moonbit_decref(_M0L7on__errS1320);
  } else {
    void* const _M0L6_2aerrS3517 = _tmp_4243.data.err;
    moonbit_decref(_M0L6on__okS1323);
    _M0L11_2atry__errS1318 = _M0L6_2aerrS3517;
    goto join_1317;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS1323->code(_M0L6on__okS1323);
  goto joinlet_4242;
  join_1317:;
  _M0L3errS1319 = _M0L11_2atry__errS1318;
  #line 383 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS1320->code(_M0L7on__errS1320, _M0L3errS1319);
  joinlet_4242:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1277;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1283;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1290;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1295;
  struct _M0TUsiE** _M0L6_2atmpS3515;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1302;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1303;
  moonbit_string_t _M0L6_2atmpS3514;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1304;
  int32_t _M0L7_2abindS1305;
  int32_t _M0L2__S1306;
  #line 193 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1277 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1283
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1290
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1283;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1295 = 0;
  _M0L6_2atmpS3515 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1302
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1302)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1302->$0 = _M0L6_2atmpS3515;
  _M0L16file__and__indexS1302->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS1303
  = _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1290(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1290);
  #line 284 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3514 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1303, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS1304
  = _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1295(_M0L51moonbit__test__driver__internal__split__mbt__stringS1295, _M0L6_2atmpS3514, 47);
  _M0L7_2abindS1305 = _M0L10test__argsS1304->$1;
  _M0L2__S1306 = 0;
  while (1) {
    if (_M0L2__S1306 < _M0L7_2abindS1305) {
      moonbit_string_t* _M0L8_2afieldS3589 = _M0L10test__argsS1304->$0;
      moonbit_string_t* _M0L3bufS3513 = _M0L8_2afieldS3589;
      moonbit_string_t _M0L6_2atmpS3588 =
        (moonbit_string_t)_M0L3bufS3513[_M0L2__S1306];
      moonbit_string_t _M0L3argS1307 = _M0L6_2atmpS3588;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1308;
      moonbit_string_t _M0L4fileS1309;
      moonbit_string_t _M0L5rangeS1310;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1311;
      moonbit_string_t _M0L6_2atmpS3511;
      int32_t _M0L5startS1312;
      moonbit_string_t _M0L6_2atmpS3510;
      int32_t _M0L3endS1313;
      int32_t _M0L1iS1314;
      int32_t _M0L6_2atmpS3512;
      moonbit_incref(_M0L3argS1307);
      #line 288 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS1308
      = _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1295(_M0L51moonbit__test__driver__internal__split__mbt__stringS1295, _M0L3argS1307, 58);
      moonbit_incref(_M0L16file__and__rangeS1308);
      #line 289 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS1309
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1308, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS1310
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1308, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS1311
      = _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1295(_M0L51moonbit__test__driver__internal__split__mbt__stringS1295, _M0L5rangeS1310, 45);
      moonbit_incref(_M0L15start__and__endS1311);
      #line 294 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3511
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1311, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _M0L5startS1312
      = _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1277(_M0L45moonbit__test__driver__internal__parse__int__S1277, _M0L6_2atmpS3511);
      #line 295 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3510
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1311, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _M0L3endS1313
      = _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1277(_M0L45moonbit__test__driver__internal__parse__int__S1277, _M0L6_2atmpS3510);
      _M0L1iS1314 = _M0L5startS1312;
      while (1) {
        if (_M0L1iS1314 < _M0L3endS1313) {
          struct _M0TUsiE* _M0L8_2atupleS3508;
          int32_t _M0L6_2atmpS3509;
          moonbit_incref(_M0L4fileS1309);
          _M0L8_2atupleS3508
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3508)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3508->$0 = _M0L4fileS1309;
          _M0L8_2atupleS3508->$1 = _M0L1iS1314;
          moonbit_incref(_M0L16file__and__indexS1302);
          #line 297 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1302, _M0L8_2atupleS3508);
          _M0L6_2atmpS3509 = _M0L1iS1314 + 1;
          _M0L1iS1314 = _M0L6_2atmpS3509;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1309);
        }
        break;
      }
      _M0L6_2atmpS3512 = _M0L2__S1306 + 1;
      _M0L2__S1306 = _M0L6_2atmpS3512;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1304);
    }
    break;
  }
  return _M0L16file__and__indexS1302;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1295(
  int32_t _M0L6_2aenvS3489,
  moonbit_string_t _M0L1sS1296,
  int32_t _M0L3sepS1297
) {
  moonbit_string_t* _M0L6_2atmpS3507;
  struct _M0TPB5ArrayGsE* _M0L3resS1298;
  struct _M0TPC13ref3RefGiE* _M0L1iS1299;
  struct _M0TPC13ref3RefGiE* _M0L5startS1300;
  int32_t _M0L3valS3502;
  int32_t _M0L6_2atmpS3503;
  #line 261 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS3507 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1298
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1298)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1298->$0 = _M0L6_2atmpS3507;
  _M0L3resS1298->$1 = 0;
  _M0L1iS1299
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1299)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1299->$0 = 0;
  _M0L5startS1300
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1300)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1300->$0 = 0;
  while (1) {
    int32_t _M0L3valS3490 = _M0L1iS1299->$0;
    int32_t _M0L6_2atmpS3491 = Moonbit_array_length(_M0L1sS1296);
    if (_M0L3valS3490 < _M0L6_2atmpS3491) {
      int32_t _M0L3valS3494 = _M0L1iS1299->$0;
      int32_t _M0L6_2atmpS3493;
      int32_t _M0L6_2atmpS3492;
      int32_t _M0L3valS3501;
      int32_t _M0L6_2atmpS3500;
      if (
        _M0L3valS3494 < 0
        || _M0L3valS3494 >= Moonbit_array_length(_M0L1sS1296)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3493 = _M0L1sS1296[_M0L3valS3494];
      _M0L6_2atmpS3492 = _M0L6_2atmpS3493;
      if (_M0L6_2atmpS3492 == _M0L3sepS1297) {
        int32_t _M0L3valS3496 = _M0L5startS1300->$0;
        int32_t _M0L3valS3497 = _M0L1iS1299->$0;
        moonbit_string_t _M0L6_2atmpS3495;
        int32_t _M0L3valS3499;
        int32_t _M0L6_2atmpS3498;
        moonbit_incref(_M0L1sS1296);
        #line 270 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS3495
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1296, _M0L3valS3496, _M0L3valS3497);
        moonbit_incref(_M0L3resS1298);
        #line 270 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1298, _M0L6_2atmpS3495);
        _M0L3valS3499 = _M0L1iS1299->$0;
        _M0L6_2atmpS3498 = _M0L3valS3499 + 1;
        _M0L5startS1300->$0 = _M0L6_2atmpS3498;
      }
      _M0L3valS3501 = _M0L1iS1299->$0;
      _M0L6_2atmpS3500 = _M0L3valS3501 + 1;
      _M0L1iS1299->$0 = _M0L6_2atmpS3500;
      continue;
    } else {
      moonbit_decref(_M0L1iS1299);
    }
    break;
  }
  _M0L3valS3502 = _M0L5startS1300->$0;
  _M0L6_2atmpS3503 = Moonbit_array_length(_M0L1sS1296);
  if (_M0L3valS3502 < _M0L6_2atmpS3503) {
    int32_t _M0L8_2afieldS3590 = _M0L5startS1300->$0;
    int32_t _M0L3valS3505;
    int32_t _M0L6_2atmpS3506;
    moonbit_string_t _M0L6_2atmpS3504;
    moonbit_decref(_M0L5startS1300);
    _M0L3valS3505 = _M0L8_2afieldS3590;
    _M0L6_2atmpS3506 = Moonbit_array_length(_M0L1sS1296);
    #line 276 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS3504
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1296, _M0L3valS3505, _M0L6_2atmpS3506);
    moonbit_incref(_M0L3resS1298);
    #line 276 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1298, _M0L6_2atmpS3504);
  } else {
    moonbit_decref(_M0L5startS1300);
    moonbit_decref(_M0L1sS1296);
  }
  return _M0L3resS1298;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1290(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1283
) {
  moonbit_bytes_t* _M0L3tmpS1291;
  int32_t _M0L6_2atmpS3488;
  struct _M0TPB5ArrayGsE* _M0L3resS1292;
  int32_t _M0L1iS1293;
  #line 250 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS1291
  = _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3488 = Moonbit_array_length(_M0L3tmpS1291);
  #line 254 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1292 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3488);
  _M0L1iS1293 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3484 = Moonbit_array_length(_M0L3tmpS1291);
    if (_M0L1iS1293 < _M0L6_2atmpS3484) {
      moonbit_bytes_t _M0L6_2atmpS3591;
      moonbit_bytes_t _M0L6_2atmpS3486;
      moonbit_string_t _M0L6_2atmpS3485;
      int32_t _M0L6_2atmpS3487;
      if (
        _M0L1iS1293 < 0 || _M0L1iS1293 >= Moonbit_array_length(_M0L3tmpS1291)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3591 = (moonbit_bytes_t)_M0L3tmpS1291[_M0L1iS1293];
      _M0L6_2atmpS3486 = _M0L6_2atmpS3591;
      moonbit_incref(_M0L6_2atmpS3486);
      #line 256 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS3485
      = _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1283(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1283, _M0L6_2atmpS3486);
      moonbit_incref(_M0L3resS1292);
      #line 256 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1292, _M0L6_2atmpS3485);
      _M0L6_2atmpS3487 = _M0L1iS1293 + 1;
      _M0L1iS1293 = _M0L6_2atmpS3487;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1291);
    }
    break;
  }
  return _M0L3resS1292;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1283(
  int32_t _M0L6_2aenvS3398,
  moonbit_bytes_t _M0L5bytesS1284
) {
  struct _M0TPB13StringBuilder* _M0L3resS1285;
  int32_t _M0L3lenS1286;
  struct _M0TPC13ref3RefGiE* _M0L1iS1287;
  #line 206 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1285 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1286 = Moonbit_array_length(_M0L5bytesS1284);
  _M0L1iS1287
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1287)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1287->$0 = 0;
  while (1) {
    int32_t _M0L3valS3399 = _M0L1iS1287->$0;
    if (_M0L3valS3399 < _M0L3lenS1286) {
      int32_t _M0L3valS3483 = _M0L1iS1287->$0;
      int32_t _M0L6_2atmpS3482;
      int32_t _M0L6_2atmpS3481;
      struct _M0TPC13ref3RefGiE* _M0L1cS1288;
      int32_t _M0L3valS3400;
      if (
        _M0L3valS3483 < 0
        || _M0L3valS3483 >= Moonbit_array_length(_M0L5bytesS1284)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3482 = _M0L5bytesS1284[_M0L3valS3483];
      _M0L6_2atmpS3481 = (int32_t)_M0L6_2atmpS3482;
      _M0L1cS1288
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1288)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1288->$0 = _M0L6_2atmpS3481;
      _M0L3valS3400 = _M0L1cS1288->$0;
      if (_M0L3valS3400 < 128) {
        int32_t _M0L8_2afieldS3592 = _M0L1cS1288->$0;
        int32_t _M0L3valS3402;
        int32_t _M0L6_2atmpS3401;
        int32_t _M0L3valS3404;
        int32_t _M0L6_2atmpS3403;
        moonbit_decref(_M0L1cS1288);
        _M0L3valS3402 = _M0L8_2afieldS3592;
        _M0L6_2atmpS3401 = _M0L3valS3402;
        moonbit_incref(_M0L3resS1285);
        #line 215 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1285, _M0L6_2atmpS3401);
        _M0L3valS3404 = _M0L1iS1287->$0;
        _M0L6_2atmpS3403 = _M0L3valS3404 + 1;
        _M0L1iS1287->$0 = _M0L6_2atmpS3403;
      } else {
        int32_t _M0L3valS3405 = _M0L1cS1288->$0;
        if (_M0L3valS3405 < 224) {
          int32_t _M0L3valS3407 = _M0L1iS1287->$0;
          int32_t _M0L6_2atmpS3406 = _M0L3valS3407 + 1;
          int32_t _M0L3valS3416;
          int32_t _M0L6_2atmpS3415;
          int32_t _M0L6_2atmpS3409;
          int32_t _M0L3valS3414;
          int32_t _M0L6_2atmpS3413;
          int32_t _M0L6_2atmpS3412;
          int32_t _M0L6_2atmpS3411;
          int32_t _M0L6_2atmpS3410;
          int32_t _M0L6_2atmpS3408;
          int32_t _M0L8_2afieldS3593;
          int32_t _M0L3valS3418;
          int32_t _M0L6_2atmpS3417;
          int32_t _M0L3valS3420;
          int32_t _M0L6_2atmpS3419;
          if (_M0L6_2atmpS3406 >= _M0L3lenS1286) {
            moonbit_decref(_M0L1cS1288);
            moonbit_decref(_M0L1iS1287);
            moonbit_decref(_M0L5bytesS1284);
            break;
          }
          _M0L3valS3416 = _M0L1cS1288->$0;
          _M0L6_2atmpS3415 = _M0L3valS3416 & 31;
          _M0L6_2atmpS3409 = _M0L6_2atmpS3415 << 6;
          _M0L3valS3414 = _M0L1iS1287->$0;
          _M0L6_2atmpS3413 = _M0L3valS3414 + 1;
          if (
            _M0L6_2atmpS3413 < 0
            || _M0L6_2atmpS3413 >= Moonbit_array_length(_M0L5bytesS1284)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3412 = _M0L5bytesS1284[_M0L6_2atmpS3413];
          _M0L6_2atmpS3411 = (int32_t)_M0L6_2atmpS3412;
          _M0L6_2atmpS3410 = _M0L6_2atmpS3411 & 63;
          _M0L6_2atmpS3408 = _M0L6_2atmpS3409 | _M0L6_2atmpS3410;
          _M0L1cS1288->$0 = _M0L6_2atmpS3408;
          _M0L8_2afieldS3593 = _M0L1cS1288->$0;
          moonbit_decref(_M0L1cS1288);
          _M0L3valS3418 = _M0L8_2afieldS3593;
          _M0L6_2atmpS3417 = _M0L3valS3418;
          moonbit_incref(_M0L3resS1285);
          #line 222 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1285, _M0L6_2atmpS3417);
          _M0L3valS3420 = _M0L1iS1287->$0;
          _M0L6_2atmpS3419 = _M0L3valS3420 + 2;
          _M0L1iS1287->$0 = _M0L6_2atmpS3419;
        } else {
          int32_t _M0L3valS3421 = _M0L1cS1288->$0;
          if (_M0L3valS3421 < 240) {
            int32_t _M0L3valS3423 = _M0L1iS1287->$0;
            int32_t _M0L6_2atmpS3422 = _M0L3valS3423 + 2;
            int32_t _M0L3valS3439;
            int32_t _M0L6_2atmpS3438;
            int32_t _M0L6_2atmpS3431;
            int32_t _M0L3valS3437;
            int32_t _M0L6_2atmpS3436;
            int32_t _M0L6_2atmpS3435;
            int32_t _M0L6_2atmpS3434;
            int32_t _M0L6_2atmpS3433;
            int32_t _M0L6_2atmpS3432;
            int32_t _M0L6_2atmpS3425;
            int32_t _M0L3valS3430;
            int32_t _M0L6_2atmpS3429;
            int32_t _M0L6_2atmpS3428;
            int32_t _M0L6_2atmpS3427;
            int32_t _M0L6_2atmpS3426;
            int32_t _M0L6_2atmpS3424;
            int32_t _M0L8_2afieldS3594;
            int32_t _M0L3valS3441;
            int32_t _M0L6_2atmpS3440;
            int32_t _M0L3valS3443;
            int32_t _M0L6_2atmpS3442;
            if (_M0L6_2atmpS3422 >= _M0L3lenS1286) {
              moonbit_decref(_M0L1cS1288);
              moonbit_decref(_M0L1iS1287);
              moonbit_decref(_M0L5bytesS1284);
              break;
            }
            _M0L3valS3439 = _M0L1cS1288->$0;
            _M0L6_2atmpS3438 = _M0L3valS3439 & 15;
            _M0L6_2atmpS3431 = _M0L6_2atmpS3438 << 12;
            _M0L3valS3437 = _M0L1iS1287->$0;
            _M0L6_2atmpS3436 = _M0L3valS3437 + 1;
            if (
              _M0L6_2atmpS3436 < 0
              || _M0L6_2atmpS3436 >= Moonbit_array_length(_M0L5bytesS1284)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3435 = _M0L5bytesS1284[_M0L6_2atmpS3436];
            _M0L6_2atmpS3434 = (int32_t)_M0L6_2atmpS3435;
            _M0L6_2atmpS3433 = _M0L6_2atmpS3434 & 63;
            _M0L6_2atmpS3432 = _M0L6_2atmpS3433 << 6;
            _M0L6_2atmpS3425 = _M0L6_2atmpS3431 | _M0L6_2atmpS3432;
            _M0L3valS3430 = _M0L1iS1287->$0;
            _M0L6_2atmpS3429 = _M0L3valS3430 + 2;
            if (
              _M0L6_2atmpS3429 < 0
              || _M0L6_2atmpS3429 >= Moonbit_array_length(_M0L5bytesS1284)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3428 = _M0L5bytesS1284[_M0L6_2atmpS3429];
            _M0L6_2atmpS3427 = (int32_t)_M0L6_2atmpS3428;
            _M0L6_2atmpS3426 = _M0L6_2atmpS3427 & 63;
            _M0L6_2atmpS3424 = _M0L6_2atmpS3425 | _M0L6_2atmpS3426;
            _M0L1cS1288->$0 = _M0L6_2atmpS3424;
            _M0L8_2afieldS3594 = _M0L1cS1288->$0;
            moonbit_decref(_M0L1cS1288);
            _M0L3valS3441 = _M0L8_2afieldS3594;
            _M0L6_2atmpS3440 = _M0L3valS3441;
            moonbit_incref(_M0L3resS1285);
            #line 231 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1285, _M0L6_2atmpS3440);
            _M0L3valS3443 = _M0L1iS1287->$0;
            _M0L6_2atmpS3442 = _M0L3valS3443 + 3;
            _M0L1iS1287->$0 = _M0L6_2atmpS3442;
          } else {
            int32_t _M0L3valS3445 = _M0L1iS1287->$0;
            int32_t _M0L6_2atmpS3444 = _M0L3valS3445 + 3;
            int32_t _M0L3valS3468;
            int32_t _M0L6_2atmpS3467;
            int32_t _M0L6_2atmpS3460;
            int32_t _M0L3valS3466;
            int32_t _M0L6_2atmpS3465;
            int32_t _M0L6_2atmpS3464;
            int32_t _M0L6_2atmpS3463;
            int32_t _M0L6_2atmpS3462;
            int32_t _M0L6_2atmpS3461;
            int32_t _M0L6_2atmpS3453;
            int32_t _M0L3valS3459;
            int32_t _M0L6_2atmpS3458;
            int32_t _M0L6_2atmpS3457;
            int32_t _M0L6_2atmpS3456;
            int32_t _M0L6_2atmpS3455;
            int32_t _M0L6_2atmpS3454;
            int32_t _M0L6_2atmpS3447;
            int32_t _M0L3valS3452;
            int32_t _M0L6_2atmpS3451;
            int32_t _M0L6_2atmpS3450;
            int32_t _M0L6_2atmpS3449;
            int32_t _M0L6_2atmpS3448;
            int32_t _M0L6_2atmpS3446;
            int32_t _M0L3valS3470;
            int32_t _M0L6_2atmpS3469;
            int32_t _M0L3valS3474;
            int32_t _M0L6_2atmpS3473;
            int32_t _M0L6_2atmpS3472;
            int32_t _M0L6_2atmpS3471;
            int32_t _M0L8_2afieldS3595;
            int32_t _M0L3valS3478;
            int32_t _M0L6_2atmpS3477;
            int32_t _M0L6_2atmpS3476;
            int32_t _M0L6_2atmpS3475;
            int32_t _M0L3valS3480;
            int32_t _M0L6_2atmpS3479;
            if (_M0L6_2atmpS3444 >= _M0L3lenS1286) {
              moonbit_decref(_M0L1cS1288);
              moonbit_decref(_M0L1iS1287);
              moonbit_decref(_M0L5bytesS1284);
              break;
            }
            _M0L3valS3468 = _M0L1cS1288->$0;
            _M0L6_2atmpS3467 = _M0L3valS3468 & 7;
            _M0L6_2atmpS3460 = _M0L6_2atmpS3467 << 18;
            _M0L3valS3466 = _M0L1iS1287->$0;
            _M0L6_2atmpS3465 = _M0L3valS3466 + 1;
            if (
              _M0L6_2atmpS3465 < 0
              || _M0L6_2atmpS3465 >= Moonbit_array_length(_M0L5bytesS1284)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3464 = _M0L5bytesS1284[_M0L6_2atmpS3465];
            _M0L6_2atmpS3463 = (int32_t)_M0L6_2atmpS3464;
            _M0L6_2atmpS3462 = _M0L6_2atmpS3463 & 63;
            _M0L6_2atmpS3461 = _M0L6_2atmpS3462 << 12;
            _M0L6_2atmpS3453 = _M0L6_2atmpS3460 | _M0L6_2atmpS3461;
            _M0L3valS3459 = _M0L1iS1287->$0;
            _M0L6_2atmpS3458 = _M0L3valS3459 + 2;
            if (
              _M0L6_2atmpS3458 < 0
              || _M0L6_2atmpS3458 >= Moonbit_array_length(_M0L5bytesS1284)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3457 = _M0L5bytesS1284[_M0L6_2atmpS3458];
            _M0L6_2atmpS3456 = (int32_t)_M0L6_2atmpS3457;
            _M0L6_2atmpS3455 = _M0L6_2atmpS3456 & 63;
            _M0L6_2atmpS3454 = _M0L6_2atmpS3455 << 6;
            _M0L6_2atmpS3447 = _M0L6_2atmpS3453 | _M0L6_2atmpS3454;
            _M0L3valS3452 = _M0L1iS1287->$0;
            _M0L6_2atmpS3451 = _M0L3valS3452 + 3;
            if (
              _M0L6_2atmpS3451 < 0
              || _M0L6_2atmpS3451 >= Moonbit_array_length(_M0L5bytesS1284)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3450 = _M0L5bytesS1284[_M0L6_2atmpS3451];
            _M0L6_2atmpS3449 = (int32_t)_M0L6_2atmpS3450;
            _M0L6_2atmpS3448 = _M0L6_2atmpS3449 & 63;
            _M0L6_2atmpS3446 = _M0L6_2atmpS3447 | _M0L6_2atmpS3448;
            _M0L1cS1288->$0 = _M0L6_2atmpS3446;
            _M0L3valS3470 = _M0L1cS1288->$0;
            _M0L6_2atmpS3469 = _M0L3valS3470 - 65536;
            _M0L1cS1288->$0 = _M0L6_2atmpS3469;
            _M0L3valS3474 = _M0L1cS1288->$0;
            _M0L6_2atmpS3473 = _M0L3valS3474 >> 10;
            _M0L6_2atmpS3472 = _M0L6_2atmpS3473 + 55296;
            _M0L6_2atmpS3471 = _M0L6_2atmpS3472;
            moonbit_incref(_M0L3resS1285);
            #line 242 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1285, _M0L6_2atmpS3471);
            _M0L8_2afieldS3595 = _M0L1cS1288->$0;
            moonbit_decref(_M0L1cS1288);
            _M0L3valS3478 = _M0L8_2afieldS3595;
            _M0L6_2atmpS3477 = _M0L3valS3478 & 1023;
            _M0L6_2atmpS3476 = _M0L6_2atmpS3477 + 56320;
            _M0L6_2atmpS3475 = _M0L6_2atmpS3476;
            moonbit_incref(_M0L3resS1285);
            #line 243 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1285, _M0L6_2atmpS3475);
            _M0L3valS3480 = _M0L1iS1287->$0;
            _M0L6_2atmpS3479 = _M0L3valS3480 + 4;
            _M0L1iS1287->$0 = _M0L6_2atmpS3479;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1287);
      moonbit_decref(_M0L5bytesS1284);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1285);
}

int32_t _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1277(
  int32_t _M0L6_2aenvS3391,
  moonbit_string_t _M0L1sS1278
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1279;
  int32_t _M0L3lenS1280;
  int32_t _M0L1iS1281;
  int32_t _M0L8_2afieldS3596;
  #line 197 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1279
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1279)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1279->$0 = 0;
  _M0L3lenS1280 = Moonbit_array_length(_M0L1sS1278);
  _M0L1iS1281 = 0;
  while (1) {
    if (_M0L1iS1281 < _M0L3lenS1280) {
      int32_t _M0L3valS3396 = _M0L3resS1279->$0;
      int32_t _M0L6_2atmpS3393 = _M0L3valS3396 * 10;
      int32_t _M0L6_2atmpS3395;
      int32_t _M0L6_2atmpS3394;
      int32_t _M0L6_2atmpS3392;
      int32_t _M0L6_2atmpS3397;
      if (
        _M0L1iS1281 < 0 || _M0L1iS1281 >= Moonbit_array_length(_M0L1sS1278)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3395 = _M0L1sS1278[_M0L1iS1281];
      _M0L6_2atmpS3394 = _M0L6_2atmpS3395 - 48;
      _M0L6_2atmpS3392 = _M0L6_2atmpS3393 + _M0L6_2atmpS3394;
      _M0L3resS1279->$0 = _M0L6_2atmpS3392;
      _M0L6_2atmpS3397 = _M0L1iS1281 + 1;
      _M0L1iS1281 = _M0L6_2atmpS3397;
      continue;
    } else {
      moonbit_decref(_M0L1sS1278);
    }
    break;
  }
  _M0L8_2afieldS3596 = _M0L3resS1279->$0;
  moonbit_decref(_M0L3resS1279);
  return _M0L8_2afieldS3596;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1257,
  moonbit_string_t _M0L12_2adiscard__S1258,
  int32_t _M0L12_2adiscard__S1259,
  struct _M0TWssbEu* _M0L12_2adiscard__S1260,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1261
) {
  struct moonbit_result_0 _result_4250;
  #line 34 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1261);
  moonbit_decref(_M0L12_2adiscard__S1260);
  moonbit_decref(_M0L12_2adiscard__S1258);
  moonbit_decref(_M0L12_2adiscard__S1257);
  _result_4250.tag = 1;
  _result_4250.data.ok = 0;
  return _result_4250;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1262,
  moonbit_string_t _M0L12_2adiscard__S1263,
  int32_t _M0L12_2adiscard__S1264,
  struct _M0TWssbEu* _M0L12_2adiscard__S1265,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1266
) {
  struct moonbit_result_0 _result_4251;
  #line 34 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1266);
  moonbit_decref(_M0L12_2adiscard__S1265);
  moonbit_decref(_M0L12_2adiscard__S1263);
  moonbit_decref(_M0L12_2adiscard__S1262);
  _result_4251.tag = 1;
  _result_4251.data.ok = 0;
  return _result_4251;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1267,
  moonbit_string_t _M0L12_2adiscard__S1268,
  int32_t _M0L12_2adiscard__S1269,
  struct _M0TWssbEu* _M0L12_2adiscard__S1270,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1271
) {
  struct moonbit_result_0 _result_4252;
  #line 34 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1271);
  moonbit_decref(_M0L12_2adiscard__S1270);
  moonbit_decref(_M0L12_2adiscard__S1268);
  moonbit_decref(_M0L12_2adiscard__S1267);
  _result_4252.tag = 1;
  _result_4252.data.ok = 0;
  return _result_4252;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal19tty__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1272,
  moonbit_string_t _M0L12_2adiscard__S1273,
  int32_t _M0L12_2adiscard__S1274,
  struct _M0TWssbEu* _M0L12_2adiscard__S1275,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1276
) {
  struct moonbit_result_0 _result_4253;
  #line 34 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1276);
  moonbit_decref(_M0L12_2adiscard__S1275);
  moonbit_decref(_M0L12_2adiscard__S1273);
  moonbit_decref(_M0L12_2adiscard__S1272);
  _result_4253.tag = 1;
  _result_4253.data.ok = 0;
  return _result_4253;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal19tty__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1256
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1256);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal19tty__blackbox__test37____test__7474795f746573742e6d6274__0(
  
) {
  void* _M0L11_2atry__errS1255;
  void* _M0L7_2abindS1253;
  struct moonbit_result_1 _tmp_4255;
  struct _M0TP48clawteam8clawteam8internal3tty4Size* _M0L6_2atmpS3388;
  int32_t _M0L6_2atmpS3386;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS3387;
  struct _M0TPB6ToJson _M0L6_2atmpS3377;
  void* _M0L6_2atmpS3385;
  void* _M0L6_2atmpS3378;
  moonbit_string_t _M0L6_2atmpS3381;
  moonbit_string_t _M0L6_2atmpS3382;
  moonbit_string_t _M0L6_2atmpS3383;
  moonbit_string_t _M0L6_2atmpS3384;
  moonbit_string_t* _M0L6_2atmpS3380;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS3379;
  #line 2 "E:\\moonbit\\clawteam\\internal\\tty\\tty_test.mbt"
  #line 3 "E:\\moonbit\\clawteam\\internal\\tty\\tty_test.mbt"
  _tmp_4255 = _M0FP48clawteam8clawteam8internal3tty12window__size();
  if (_tmp_4255.tag) {
    struct _M0TP48clawteam8clawteam8internal3tty4Size* const _M0L5_2aokS3389 =
      _tmp_4255.data.ok;
    _M0L6_2atmpS3388 = _M0L5_2aokS3389;
  } else {
    void* const _M0L6_2aerrS3390 = _tmp_4255.data.err;
    _M0L11_2atry__errS1255 = _M0L6_2aerrS3390;
    goto join_1254;
  }
  _M0L7_2abindS1253
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRPC15error5ErrorE2Ok));
  Moonbit_object_header(_M0L7_2abindS1253)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRPC15error5ErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRPC15error5ErrorE2Ok*)_M0L7_2abindS1253)->$0
  = _M0L6_2atmpS3388;
  goto joinlet_4254;
  join_1254:;
  _M0L7_2abindS1253
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRPC15error5ErrorE3Err));
  Moonbit_object_header(_M0L7_2abindS1253)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRPC15error5ErrorE3Err, $0) >> 2, 1, 0);
  ((struct _M0DTPC16result6ResultGRP48clawteam8clawteam8internal3tty4SizeRPC15error5ErrorE3Err*)_M0L7_2abindS1253)->$0
  = _M0L11_2atry__errS1255;
  joinlet_4254:;
  switch (Moonbit_object_tag(_M0L7_2abindS1253)) {
    case 0: {
      moonbit_decref(_M0L7_2abindS1253);
      _M0L6_2atmpS3386 = 1;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2abindS1253);
      _M0L6_2atmpS3386 = 0;
      break;
    }
  }
  _M0L14_2aboxed__selfS3387
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS3387)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS3387->$0 = _M0L6_2atmpS3386;
  _M0L6_2atmpS3377
  = (struct _M0TPB6ToJson){
    _M0FP079Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eToJson_2estatic__method__table__id,
      _M0L14_2aboxed__selfS3387
  };
  #line 3 "E:\\moonbit\\clawteam\\internal\\tty\\tty_test.mbt"
  _M0L6_2atmpS3385 = _M0MPC14json4Json7boolean(1);
  _M0L6_2atmpS3378 = _M0L6_2atmpS3385;
  _M0L6_2atmpS3381 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS3382 = (moonbit_string_t)moonbit_string_literal_10.data;
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
  #line 3 "E:\\moonbit\\clawteam\\internal\\tty\\tty_test.mbt"
  return _M0FPC14json13json__inspect(_M0L6_2atmpS3377, _M0L6_2atmpS3378, (moonbit_string_t)moonbit_string_literal_11.data, _M0L6_2atmpS3379);
}

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal3tty12window__size() {
  int32_t* _M0L3bufS1251;
  int32_t _M0L6resultS1252;
  int32_t _M0L6_2atmpS3375;
  int32_t _M0L6_2atmpS3597;
  int32_t _M0L6_2atmpS3376;
  struct _M0TP48clawteam8clawteam8internal3tty4Size* _M0L6_2atmpS3374;
  struct moonbit_result_1 _result_4257;
  #line 32 "E:\\moonbit\\clawteam\\internal\\tty\\tty.mbt"
  _M0L3bufS1251 = (int32_t*)moonbit_make_int32_array_raw(2);
  _M0L3bufS1251[0] = 0;
  _M0L3bufS1251[1] = 0;
  #line 34 "E:\\moonbit\\clawteam\\internal\\tty\\tty.mbt"
  _M0L6resultS1252
  = _M0FP48clawteam8clawteam8internal3tty19tty__get__win__size(_M0L3bufS1251);
  if (_M0L6resultS1252 != 0) {
    void* _M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3373;
    struct moonbit_result_1 _result_4256;
    moonbit_decref(_M0L3bufS1251);
    _M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3373
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno));
    Moonbit_object_header(_M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3373)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno) >> 2, 0, 1);
    ((struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno*)_M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3373)->$0
    = _M0L6resultS1252;
    _result_4256.tag = 0;
    _result_4256.data.err
    = _M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3373;
    return _result_4256;
  }
  if (0 < 0 || 0 >= Moonbit_array_length(_M0L3bufS1251)) {
    #line 38 "E:\\moonbit\\clawteam\\internal\\tty\\tty.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3375 = (int32_t)_M0L3bufS1251[0];
  if (1 < 0 || 1 >= Moonbit_array_length(_M0L3bufS1251)) {
    #line 38 "E:\\moonbit\\clawteam\\internal\\tty\\tty.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3597 = (int32_t)_M0L3bufS1251[1];
  moonbit_decref(_M0L3bufS1251);
  _M0L6_2atmpS3376 = _M0L6_2atmpS3597;
  _M0L6_2atmpS3374
  = (struct _M0TP48clawteam8clawteam8internal3tty4Size*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam8internal3tty4Size));
  Moonbit_object_header(_M0L6_2atmpS3374)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP48clawteam8clawteam8internal3tty4Size) >> 2, 0, 0);
  _M0L6_2atmpS3374->$0 = _M0L6_2atmpS3375;
  _M0L6_2atmpS3374->$1 = _M0L6_2atmpS3376;
  _result_4257.tag = 1;
  _result_4257.data.ok = _M0L6_2atmpS3374;
  return _result_4257;
}

int32_t _M0IP48clawteam8clawteam8internal5errno5ErrnoPB4Show6output(
  void* _M0L4selfS1248,
  struct _M0TPB6Logger _M0L6loggerS1241
) {
  int32_t _M0L6errnumS1239;
  struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno* _M0L8_2aErrnoS1249;
  int32_t _M0L8_2afieldS3599;
  int32_t _M0L9_2aerrnumS1250;
  void* _M0L6c__strS1240;
  #line 28 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
  _M0L8_2aErrnoS1249
  = (struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno*)_M0L4selfS1248;
  _M0L8_2afieldS3599 = _M0L8_2aErrnoS1249->$0;
  moonbit_decref(_M0L8_2aErrnoS1249);
  _M0L9_2aerrnumS1250 = _M0L8_2afieldS3599;
  _M0L6errnumS1239 = _M0L9_2aerrnumS1250;
  goto join_1238;
  goto joinlet_4258;
  join_1238:;
  #line 30 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
  _M0L6c__strS1240
  = _M0FP48clawteam8clawteam8internal5errno15errno__strerror(_M0L6errnumS1239);
  #line 31 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
  if (
    _M0MP48clawteam8clawteam8internal1c7Pointer8is__nullGyE(_M0L6c__strS1240)
  ) {
    moonbit_string_t _M0L6_2atmpS3366;
    moonbit_string_t _M0L6_2atmpS3598;
    moonbit_string_t _M0L6_2atmpS3365;
    #line 32 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3366
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6errnumS1239);
    #line 32 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3598
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_12.data, _M0L6_2atmpS3366);
    moonbit_decref(_M0L6_2atmpS3366);
    _M0L6_2atmpS3365 = _M0L6_2atmpS3598;
    #line 32 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6loggerS1241.$0->$method_0(_M0L6loggerS1241.$1, _M0L6_2atmpS3365);
  } else {
    uint64_t _M0L6_2atmpS3372;
    int32_t _M0L6c__lenS1242;
    moonbit_bytes_t _M0L3bufS1243;
    int32_t _M0L1iS1244;
    moonbit_bytes_t _M0L7_2abindS1247;
    int32_t _M0L6_2atmpS3371;
    int64_t _M0L6_2atmpS3370;
    struct _M0TPC15bytes9BytesView _M0L6_2atmpS3369;
    moonbit_string_t _M0L3strS1246;
    #line 34 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3372
    = _M0FP48clawteam8clawteam8internal1c6strlen(_M0L6c__strS1240);
    _M0L6c__lenS1242 = (int32_t)_M0L6_2atmpS3372;
    _M0L3bufS1243 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6c__lenS1242, 0);
    _M0L1iS1244 = 0;
    while (1) {
      if (_M0L1iS1244 < _M0L6c__lenS1242) {
        int32_t _M0L6_2atmpS3367;
        int32_t _M0L6_2atmpS3368;
        #line 37 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
        _M0L6_2atmpS3367
        = _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(_M0L6c__strS1240, _M0L1iS1244);
        if (
          _M0L1iS1244 < 0
          || _M0L1iS1244 >= Moonbit_array_length(_M0L3bufS1243)
        ) {
          #line 37 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
          moonbit_panic();
        }
        _M0L3bufS1243[_M0L1iS1244] = _M0L6_2atmpS3367;
        _M0L6_2atmpS3368 = _M0L1iS1244 + 1;
        _M0L1iS1244 = _M0L6_2atmpS3368;
        continue;
      }
      break;
    }
    _M0L7_2abindS1247 = _M0L3bufS1243;
    _M0L6_2atmpS3371 = Moonbit_array_length(_M0L7_2abindS1247);
    _M0L6_2atmpS3370 = (int64_t)_M0L6_2atmpS3371;
    #line 39 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3369
    = _M0MPC15bytes5Bytes12view_2einner(_M0L7_2abindS1247, 0, _M0L6_2atmpS3370);
    #line 39 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L3strS1246
    = _M0FPC28encoding4utf821decode__lossy_2einner(_M0L6_2atmpS3369, 0);
    #line 40 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6loggerS1241.$0->$method_0(_M0L6loggerS1241.$1, _M0L3strS1246);
  }
  joinlet_4258:;
  return 0;
}

int32_t _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(
  void* _M0L7pointerS1236,
  int32_t _M0L6offsetS1237
) {
  #line 145 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 146 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0FP48clawteam8clawteam8internal1c22moonbit__c__load__byte(_M0L7pointerS1236, _M0L6offsetS1237);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(
  void* _M0L4selfS1234,
  int32_t _M0L5indexS1235
) {
  #line 53 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 54 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(_M0L4selfS1234, _M0L5indexS1235);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer8is__nullGyE(
  void* _M0L4selfS1233
) {
  void* _M0L6_2atmpS3364;
  #line 24 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  _M0L6_2atmpS3364 = _M0L4selfS1233;
  #line 25 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0MP48clawteam8clawteam8internal1c7Pointer10__is__null(_M0L6_2atmpS3364);
}

moonbit_string_t _M0FPC28encoding4utf821decode__lossy_2einner(
  struct _M0TPC15bytes9BytesView _M0L5bytesS1025,
  int32_t _M0L11ignore__bomS1026
) {
  struct _M0TPC15bytes9BytesView _M0L5bytesS1023;
  int32_t _M0L6_2atmpS3348;
  int32_t _M0L6_2atmpS3347;
  moonbit_bytes_t _M0L1tS1031;
  int32_t _M0L4tlenS1032;
  int32_t _M0L11_2aparam__0S1033;
  struct _M0TPC15bytes9BytesView _M0L11_2aparam__1S1034;
  moonbit_bytes_t _M0L6_2atmpS2666;
  int64_t _M0L6_2atmpS2667;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  if (_M0L11ignore__bomS1026) {
    int32_t _M0L3endS3350 = _M0L5bytesS1025.$2;
    int32_t _M0L5startS3351 = _M0L5bytesS1025.$1;
    int32_t _M0L6_2atmpS3349 = _M0L3endS3350 - _M0L5startS3351;
    if (_M0L6_2atmpS3349 >= 3) {
      moonbit_bytes_t _M0L8_2afieldS3921 = _M0L5bytesS1025.$0;
      moonbit_bytes_t _M0L5bytesS3362 = _M0L8_2afieldS3921;
      int32_t _M0L5startS3363 = _M0L5bytesS1025.$1;
      int32_t _M0L6_2atmpS3920 = _M0L5bytesS3362[_M0L5startS3363];
      int32_t _M0L4_2axS1028 = _M0L6_2atmpS3920;
      if (_M0L4_2axS1028 == 239) {
        moonbit_bytes_t _M0L8_2afieldS3919 = _M0L5bytesS1025.$0;
        moonbit_bytes_t _M0L5bytesS3359 = _M0L8_2afieldS3919;
        int32_t _M0L5startS3361 = _M0L5bytesS1025.$1;
        int32_t _M0L6_2atmpS3360 = _M0L5startS3361 + 1;
        int32_t _M0L6_2atmpS3918 = _M0L5bytesS3359[_M0L6_2atmpS3360];
        int32_t _M0L4_2axS1029 = _M0L6_2atmpS3918;
        if (_M0L4_2axS1029 == 187) {
          moonbit_bytes_t _M0L8_2afieldS3917 = _M0L5bytesS1025.$0;
          moonbit_bytes_t _M0L5bytesS3356 = _M0L8_2afieldS3917;
          int32_t _M0L5startS3358 = _M0L5bytesS1025.$1;
          int32_t _M0L6_2atmpS3357 = _M0L5startS3358 + 2;
          int32_t _M0L6_2atmpS3916 = _M0L5bytesS3356[_M0L6_2atmpS3357];
          int32_t _M0L4_2axS1030 = _M0L6_2atmpS3916;
          if (_M0L4_2axS1030 == 191) {
            moonbit_bytes_t _M0L8_2afieldS3915 = _M0L5bytesS1025.$0;
            moonbit_bytes_t _M0L5bytesS3352 = _M0L8_2afieldS3915;
            int32_t _M0L5startS3355 = _M0L5bytesS1025.$1;
            int32_t _M0L6_2atmpS3353 = _M0L5startS3355 + 3;
            int32_t _M0L8_2afieldS3914 = _M0L5bytesS1025.$2;
            int32_t _M0L3endS3354 = _M0L8_2afieldS3914;
            _M0L5bytesS1023
            = (struct _M0TPC15bytes9BytesView){
              _M0L6_2atmpS3353, _M0L3endS3354, _M0L5bytesS3352
            };
          } else {
            goto join_1027;
          }
        } else {
          goto join_1027;
        }
      } else {
        goto join_1027;
      }
    } else {
      goto join_1027;
    }
    goto joinlet_4261;
    join_1027:;
    goto join_1024;
    joinlet_4261:;
  } else {
    goto join_1024;
  }
  goto joinlet_4260;
  join_1024:;
  _M0L5bytesS1023 = _M0L5bytesS1025;
  joinlet_4260:;
  moonbit_incref(_M0L5bytesS1023.$0);
  #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  _M0L6_2atmpS3348 = _M0MPC15bytes9BytesView6length(_M0L5bytesS1023);
  _M0L6_2atmpS3347 = _M0L6_2atmpS3348 * 2;
  _M0L1tS1031 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS3347, 0);
  _M0L11_2aparam__0S1033 = 0;
  _M0L11_2aparam__1S1034 = _M0L5bytesS1023;
  while (1) {
    int32_t _M0L4tlenS1036;
    struct _M0TPC15bytes9BytesView _M0L4restS1037;
    struct _M0TPC15bytes9BytesView _M0L4restS1040;
    int32_t _M0L4tlenS1041;
    struct _M0TPC15bytes9BytesView _M0L4restS1043;
    int32_t _M0L4tlenS1044;
    struct _M0TPC15bytes9BytesView _M0L4restS1046;
    int32_t _M0L4tlenS1047;
    int32_t _M0L4tlenS1049;
    int32_t _M0L2b0S1050;
    int32_t _M0L2b1S1051;
    int32_t _M0L2b2S1052;
    int32_t _M0L2b3S1053;
    struct _M0TPC15bytes9BytesView _M0L4restS1054;
    int32_t _M0L4tlenS1060;
    int32_t _M0L2b0S1061;
    int32_t _M0L2b1S1062;
    int32_t _M0L2b2S1063;
    struct _M0TPC15bytes9BytesView _M0L4restS1064;
    int32_t _M0L4tlenS1067;
    struct _M0TPC15bytes9BytesView _M0L4restS1068;
    int32_t _M0L2b0S1069;
    int32_t _M0L2b1S1070;
    int32_t _M0L4tlenS1073;
    struct _M0TPC15bytes9BytesView _M0L4restS1074;
    int32_t _M0L1bS1075;
    int32_t _M0L3endS2727 = _M0L11_2aparam__1S1034.$2;
    int32_t _M0L5startS2728 = _M0L11_2aparam__1S1034.$1;
    int32_t _M0L6_2atmpS2726 = _M0L3endS2727 - _M0L5startS2728;
    int32_t _M0L6_2atmpS2725;
    int32_t _M0L6_2atmpS2724;
    int32_t _M0L6_2atmpS2723;
    int32_t _M0L6_2atmpS2720;
    int32_t _M0L6_2atmpS2722;
    int32_t _M0L6_2atmpS2721;
    int32_t _M0L2chS1071;
    int32_t _M0L6_2atmpS2715;
    int32_t _M0L6_2atmpS2716;
    int32_t _M0L6_2atmpS2718;
    int32_t _M0L6_2atmpS2717;
    int32_t _M0L6_2atmpS2719;
    int32_t _M0L6_2atmpS2714;
    int32_t _M0L6_2atmpS2713;
    int32_t _M0L6_2atmpS2709;
    int32_t _M0L6_2atmpS2712;
    int32_t _M0L6_2atmpS2711;
    int32_t _M0L6_2atmpS2710;
    int32_t _M0L6_2atmpS2706;
    int32_t _M0L6_2atmpS2708;
    int32_t _M0L6_2atmpS2707;
    int32_t _M0L2chS1065;
    int32_t _M0L6_2atmpS2701;
    int32_t _M0L6_2atmpS2702;
    int32_t _M0L6_2atmpS2704;
    int32_t _M0L6_2atmpS2703;
    int32_t _M0L6_2atmpS2705;
    int32_t _M0L6_2atmpS2700;
    int32_t _M0L6_2atmpS2699;
    int32_t _M0L6_2atmpS2695;
    int32_t _M0L6_2atmpS2698;
    int32_t _M0L6_2atmpS2697;
    int32_t _M0L6_2atmpS2696;
    int32_t _M0L6_2atmpS2691;
    int32_t _M0L6_2atmpS2694;
    int32_t _M0L6_2atmpS2693;
    int32_t _M0L6_2atmpS2692;
    int32_t _M0L6_2atmpS2688;
    int32_t _M0L6_2atmpS2690;
    int32_t _M0L6_2atmpS2689;
    int32_t _M0L2chS1055;
    int32_t _M0L3chmS1056;
    int32_t _M0L6_2atmpS2687;
    int32_t _M0L3ch1S1057;
    int32_t _M0L6_2atmpS2686;
    int32_t _M0L3ch2S1058;
    int32_t _M0L6_2atmpS2676;
    int32_t _M0L6_2atmpS2677;
    int32_t _M0L6_2atmpS2679;
    int32_t _M0L6_2atmpS2678;
    int32_t _M0L6_2atmpS2680;
    int32_t _M0L6_2atmpS2681;
    int32_t _M0L6_2atmpS2682;
    int32_t _M0L6_2atmpS2684;
    int32_t _M0L6_2atmpS2683;
    int32_t _M0L6_2atmpS2685;
    int32_t _M0L6_2atmpS2674;
    int32_t _M0L6_2atmpS2675;
    int32_t _M0L6_2atmpS2672;
    int32_t _M0L6_2atmpS2673;
    int32_t _M0L6_2atmpS2670;
    int32_t _M0L6_2atmpS2671;
    int32_t _M0L6_2atmpS2668;
    int32_t _M0L6_2atmpS2669;
    if (_M0L6_2atmpS2726 == 0) {
      moonbit_decref(_M0L11_2aparam__1S1034.$0);
      _M0L4tlenS1032 = _M0L11_2aparam__0S1033;
    } else {
      int32_t _M0L3endS2730 = _M0L11_2aparam__1S1034.$2;
      int32_t _M0L5startS2731 = _M0L11_2aparam__1S1034.$1;
      int32_t _M0L6_2atmpS2729 = _M0L3endS2730 - _M0L5startS2731;
      if (_M0L6_2atmpS2729 >= 8) {
        moonbit_bytes_t _M0L8_2afieldS3721 = _M0L11_2aparam__1S1034.$0;
        moonbit_bytes_t _M0L5bytesS2955 = _M0L8_2afieldS3721;
        int32_t _M0L5startS2956 = _M0L11_2aparam__1S1034.$1;
        int32_t _M0L6_2atmpS3720 = _M0L5bytesS2955[_M0L5startS2956];
        int32_t _M0L4_2axS1076 = _M0L6_2atmpS3720;
        if (_M0L4_2axS1076 <= 127) {
          moonbit_bytes_t _M0L8_2afieldS3629 = _M0L11_2aparam__1S1034.$0;
          moonbit_bytes_t _M0L5bytesS2952 = _M0L8_2afieldS3629;
          int32_t _M0L5startS2954 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS2953 = _M0L5startS2954 + 1;
          int32_t _M0L6_2atmpS3628 = _M0L5bytesS2952[_M0L6_2atmpS2953];
          int32_t _M0L4_2axS1077 = _M0L6_2atmpS3628;
          if (_M0L4_2axS1077 <= 127) {
            moonbit_bytes_t _M0L8_2afieldS3625 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2949 = _M0L8_2afieldS3625;
            int32_t _M0L5startS2951 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2950 = _M0L5startS2951 + 2;
            int32_t _M0L6_2atmpS3624 = _M0L5bytesS2949[_M0L6_2atmpS2950];
            int32_t _M0L4_2axS1078 = _M0L6_2atmpS3624;
            if (_M0L4_2axS1078 <= 127) {
              moonbit_bytes_t _M0L8_2afieldS3621 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2946 = _M0L8_2afieldS3621;
              int32_t _M0L5startS2948 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2947 = _M0L5startS2948 + 3;
              int32_t _M0L6_2atmpS3620 = _M0L5bytesS2946[_M0L6_2atmpS2947];
              int32_t _M0L4_2axS1079 = _M0L6_2atmpS3620;
              if (_M0L4_2axS1079 <= 127) {
                moonbit_bytes_t _M0L8_2afieldS3617 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS2943 = _M0L8_2afieldS3617;
                int32_t _M0L5startS2945 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS2944 = _M0L5startS2945 + 4;
                int32_t _M0L6_2atmpS3616 = _M0L5bytesS2943[_M0L6_2atmpS2944];
                int32_t _M0L4_2axS1080 = _M0L6_2atmpS3616;
                if (_M0L4_2axS1080 <= 127) {
                  moonbit_bytes_t _M0L8_2afieldS3613 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS2940 = _M0L8_2afieldS3613;
                  int32_t _M0L5startS2942 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS2941 = _M0L5startS2942 + 5;
                  int32_t _M0L6_2atmpS3612 =
                    _M0L5bytesS2940[_M0L6_2atmpS2941];
                  int32_t _M0L4_2axS1081 = _M0L6_2atmpS3612;
                  if (_M0L4_2axS1081 <= 127) {
                    moonbit_bytes_t _M0L8_2afieldS3609 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS2937 = _M0L8_2afieldS3609;
                    int32_t _M0L5startS2939 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS2938 = _M0L5startS2939 + 6;
                    int32_t _M0L6_2atmpS3608 =
                      _M0L5bytesS2937[_M0L6_2atmpS2938];
                    int32_t _M0L4_2axS1082 = _M0L6_2atmpS3608;
                    if (_M0L4_2axS1082 <= 127) {
                      moonbit_bytes_t _M0L8_2afieldS3605 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS2934 = _M0L8_2afieldS3605;
                      int32_t _M0L5startS2936 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS2935 = _M0L5startS2936 + 7;
                      int32_t _M0L6_2atmpS3604 =
                        _M0L5bytesS2934[_M0L6_2atmpS2935];
                      int32_t _M0L4_2axS1083 = _M0L6_2atmpS3604;
                      if (_M0L4_2axS1083 <= 127) {
                        moonbit_bytes_t _M0L8_2afieldS3601 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS2930 = _M0L8_2afieldS3601;
                        int32_t _M0L5startS2933 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS2931 = _M0L5startS2933 + 8;
                        int32_t _M0L8_2afieldS3600 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS2932 = _M0L8_2afieldS3600;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1084 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2931,
                                                             _M0L3endS2932,
                                                             _M0L5bytesS2930};
                        int32_t _M0L6_2atmpS2922;
                        int32_t _M0L6_2atmpS2923;
                        int32_t _M0L6_2atmpS2924;
                        int32_t _M0L6_2atmpS2925;
                        int32_t _M0L6_2atmpS2926;
                        int32_t _M0L6_2atmpS2927;
                        int32_t _M0L6_2atmpS2928;
                        int32_t _M0L6_2atmpS2929;
                        _M0L1tS1031[_M0L11_2aparam__0S1033] = _M0L4_2axS1076;
                        _M0L6_2atmpS2922 = _M0L11_2aparam__0S1033 + 2;
                        _M0L1tS1031[_M0L6_2atmpS2922] = _M0L4_2axS1077;
                        _M0L6_2atmpS2923 = _M0L11_2aparam__0S1033 + 4;
                        _M0L1tS1031[_M0L6_2atmpS2923] = _M0L4_2axS1078;
                        _M0L6_2atmpS2924 = _M0L11_2aparam__0S1033 + 6;
                        _M0L1tS1031[_M0L6_2atmpS2924] = _M0L4_2axS1079;
                        _M0L6_2atmpS2925 = _M0L11_2aparam__0S1033 + 8;
                        _M0L1tS1031[_M0L6_2atmpS2925] = _M0L4_2axS1080;
                        _M0L6_2atmpS2926 = _M0L11_2aparam__0S1033 + 10;
                        _M0L1tS1031[_M0L6_2atmpS2926] = _M0L4_2axS1081;
                        _M0L6_2atmpS2927 = _M0L11_2aparam__0S1033 + 12;
                        _M0L1tS1031[_M0L6_2atmpS2927] = _M0L4_2axS1082;
                        _M0L6_2atmpS2928 = _M0L11_2aparam__0S1033 + 14;
                        _M0L1tS1031[_M0L6_2atmpS2928] = _M0L4_2axS1083;
                        _M0L6_2atmpS2929 = _M0L11_2aparam__0S1033 + 16;
                        _M0L11_2aparam__0S1033 = _M0L6_2atmpS2929;
                        _M0L11_2aparam__1S1034 = _M0L4_2axS1084;
                        continue;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3603 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS2918 = _M0L8_2afieldS3603;
                        int32_t _M0L5startS2921 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS2919 = _M0L5startS2921 + 1;
                        int32_t _M0L8_2afieldS3602 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS2920 = _M0L8_2afieldS3602;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1085 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2919,
                                                             _M0L3endS2920,
                                                             _M0L5bytesS2918};
                        _M0L4tlenS1073 = _M0L11_2aparam__0S1033;
                        _M0L4restS1074 = _M0L4_2axS1085;
                        _M0L1bS1075 = _M0L4_2axS1076;
                        goto join_1072;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3607 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS2914 = _M0L8_2afieldS3607;
                      int32_t _M0L5startS2917 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS2915 = _M0L5startS2917 + 1;
                      int32_t _M0L8_2afieldS3606 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS2916 = _M0L8_2afieldS3606;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1086 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2915,
                                                           _M0L3endS2916,
                                                           _M0L5bytesS2914};
                      _M0L4tlenS1073 = _M0L11_2aparam__0S1033;
                      _M0L4restS1074 = _M0L4_2axS1086;
                      _M0L1bS1075 = _M0L4_2axS1076;
                      goto join_1072;
                    }
                  } else {
                    moonbit_bytes_t _M0L8_2afieldS3611 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS2910 = _M0L8_2afieldS3611;
                    int32_t _M0L5startS2913 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS2911 = _M0L5startS2913 + 1;
                    int32_t _M0L8_2afieldS3610 = _M0L11_2aparam__1S1034.$2;
                    int32_t _M0L3endS2912 = _M0L8_2afieldS3610;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS1087 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2911,
                                                         _M0L3endS2912,
                                                         _M0L5bytesS2910};
                    _M0L4tlenS1073 = _M0L11_2aparam__0S1033;
                    _M0L4restS1074 = _M0L4_2axS1087;
                    _M0L1bS1075 = _M0L4_2axS1076;
                    goto join_1072;
                  }
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3615 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS2906 = _M0L8_2afieldS3615;
                  int32_t _M0L5startS2909 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS2907 = _M0L5startS2909 + 1;
                  int32_t _M0L8_2afieldS3614 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS2908 = _M0L8_2afieldS3614;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1088 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2907,
                                                       _M0L3endS2908,
                                                       _M0L5bytesS2906};
                  _M0L4tlenS1073 = _M0L11_2aparam__0S1033;
                  _M0L4restS1074 = _M0L4_2axS1088;
                  _M0L1bS1075 = _M0L4_2axS1076;
                  goto join_1072;
                }
              } else {
                moonbit_bytes_t _M0L8_2afieldS3619 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS2902 = _M0L8_2afieldS3619;
                int32_t _M0L5startS2905 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS2903 = _M0L5startS2905 + 1;
                int32_t _M0L8_2afieldS3618 = _M0L11_2aparam__1S1034.$2;
                int32_t _M0L3endS2904 = _M0L8_2afieldS3618;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1089 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2903,
                                                     _M0L3endS2904,
                                                     _M0L5bytesS2902};
                _M0L4tlenS1073 = _M0L11_2aparam__0S1033;
                _M0L4restS1074 = _M0L4_2axS1089;
                _M0L1bS1075 = _M0L4_2axS1076;
                goto join_1072;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3623 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2898 = _M0L8_2afieldS3623;
              int32_t _M0L5startS2901 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2899 = _M0L5startS2901 + 1;
              int32_t _M0L8_2afieldS3622 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2900 = _M0L8_2afieldS3622;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1090 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2899,
                                                   _M0L3endS2900,
                                                   _M0L5bytesS2898};
              _M0L4tlenS1073 = _M0L11_2aparam__0S1033;
              _M0L4restS1074 = _M0L4_2axS1090;
              _M0L1bS1075 = _M0L4_2axS1076;
              goto join_1072;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3627 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2894 = _M0L8_2afieldS3627;
            int32_t _M0L5startS2897 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2895 = _M0L5startS2897 + 1;
            int32_t _M0L8_2afieldS3626 = _M0L11_2aparam__1S1034.$2;
            int32_t _M0L3endS2896 = _M0L8_2afieldS3626;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1091 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2895,
                                                 _M0L3endS2896,
                                                 _M0L5bytesS2894};
            _M0L4tlenS1073 = _M0L11_2aparam__0S1033;
            _M0L4restS1074 = _M0L4_2axS1091;
            _M0L1bS1075 = _M0L4_2axS1076;
            goto join_1072;
          }
        } else if (_M0L4_2axS1076 >= 194 && _M0L4_2axS1076 <= 223) {
          moonbit_bytes_t _M0L8_2afieldS3635 = _M0L11_2aparam__1S1034.$0;
          moonbit_bytes_t _M0L5bytesS2891 = _M0L8_2afieldS3635;
          int32_t _M0L5startS2893 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS2892 = _M0L5startS2893 + 1;
          int32_t _M0L6_2atmpS3634 = _M0L5bytesS2891[_M0L6_2atmpS2892];
          int32_t _M0L4_2axS1092 = _M0L6_2atmpS3634;
          if (_M0L4_2axS1092 >= 128 && _M0L4_2axS1092 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3631 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2887 = _M0L8_2afieldS3631;
            int32_t _M0L5startS2890 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2888 = _M0L5startS2890 + 2;
            int32_t _M0L8_2afieldS3630 = _M0L11_2aparam__1S1034.$2;
            int32_t _M0L3endS2889 = _M0L8_2afieldS3630;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1093 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2888,
                                                 _M0L3endS2889,
                                                 _M0L5bytesS2887};
            _M0L4tlenS1067 = _M0L11_2aparam__0S1033;
            _M0L4restS1068 = _M0L4_2axS1093;
            _M0L2b0S1069 = _M0L4_2axS1076;
            _M0L2b1S1070 = _M0L4_2axS1092;
            goto join_1066;
          } else {
            moonbit_bytes_t _M0L8_2afieldS3633 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2883 = _M0L8_2afieldS3633;
            int32_t _M0L5startS2886 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2884 = _M0L5startS2886 + 1;
            int32_t _M0L8_2afieldS3632 = _M0L11_2aparam__1S1034.$2;
            int32_t _M0L3endS2885 = _M0L8_2afieldS3632;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1094 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2884,
                                                 _M0L3endS2885,
                                                 _M0L5bytesS2883};
            _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
            _M0L4restS1037 = _M0L4_2axS1094;
            goto join_1035;
          }
        } else if (_M0L4_2axS1076 == 224) {
          moonbit_bytes_t _M0L8_2afieldS3645 = _M0L11_2aparam__1S1034.$0;
          moonbit_bytes_t _M0L5bytesS2880 = _M0L8_2afieldS3645;
          int32_t _M0L5startS2882 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS2881 = _M0L5startS2882 + 1;
          int32_t _M0L6_2atmpS3644 = _M0L5bytesS2880[_M0L6_2atmpS2881];
          int32_t _M0L4_2axS1095 = _M0L6_2atmpS3644;
          if (_M0L4_2axS1095 >= 160 && _M0L4_2axS1095 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3641 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2877 = _M0L8_2afieldS3641;
            int32_t _M0L5startS2879 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2878 = _M0L5startS2879 + 2;
            int32_t _M0L6_2atmpS3640 = _M0L5bytesS2877[_M0L6_2atmpS2878];
            int32_t _M0L4_2axS1096 = _M0L6_2atmpS3640;
            if (_M0L4_2axS1096 >= 128 && _M0L4_2axS1096 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3637 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2873 = _M0L8_2afieldS3637;
              int32_t _M0L5startS2876 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2874 = _M0L5startS2876 + 3;
              int32_t _M0L8_2afieldS3636 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2875 = _M0L8_2afieldS3636;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1097 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2874,
                                                   _M0L3endS2875,
                                                   _M0L5bytesS2873};
              _M0L4tlenS1060 = _M0L11_2aparam__0S1033;
              _M0L2b0S1061 = _M0L4_2axS1076;
              _M0L2b1S1062 = _M0L4_2axS1095;
              _M0L2b2S1063 = _M0L4_2axS1096;
              _M0L4restS1064 = _M0L4_2axS1097;
              goto join_1059;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3639 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2869 = _M0L8_2afieldS3639;
              int32_t _M0L5startS2872 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2870 = _M0L5startS2872 + 2;
              int32_t _M0L8_2afieldS3638 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2871 = _M0L8_2afieldS3638;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1098 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2870,
                                                   _M0L3endS2871,
                                                   _M0L5bytesS2869};
              _M0L4restS1046 = _M0L4_2axS1098;
              _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
              goto join_1045;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3643 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2865 = _M0L8_2afieldS3643;
            int32_t _M0L5startS2868 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2866 = _M0L5startS2868 + 1;
            int32_t _M0L8_2afieldS3642 = _M0L11_2aparam__1S1034.$2;
            int32_t _M0L3endS2867 = _M0L8_2afieldS3642;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1099 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2866,
                                                 _M0L3endS2867,
                                                 _M0L5bytesS2865};
            _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
            _M0L4restS1037 = _M0L4_2axS1099;
            goto join_1035;
          }
        } else if (_M0L4_2axS1076 >= 225 && _M0L4_2axS1076 <= 236) {
          moonbit_bytes_t _M0L8_2afieldS3655 = _M0L11_2aparam__1S1034.$0;
          moonbit_bytes_t _M0L5bytesS2862 = _M0L8_2afieldS3655;
          int32_t _M0L5startS2864 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS2863 = _M0L5startS2864 + 1;
          int32_t _M0L6_2atmpS3654 = _M0L5bytesS2862[_M0L6_2atmpS2863];
          int32_t _M0L4_2axS1100 = _M0L6_2atmpS3654;
          if (_M0L4_2axS1100 >= 128 && _M0L4_2axS1100 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3651 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2859 = _M0L8_2afieldS3651;
            int32_t _M0L5startS2861 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2860 = _M0L5startS2861 + 2;
            int32_t _M0L6_2atmpS3650 = _M0L5bytesS2859[_M0L6_2atmpS2860];
            int32_t _M0L4_2axS1101 = _M0L6_2atmpS3650;
            if (_M0L4_2axS1101 >= 128 && _M0L4_2axS1101 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3647 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2855 = _M0L8_2afieldS3647;
              int32_t _M0L5startS2858 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2856 = _M0L5startS2858 + 3;
              int32_t _M0L8_2afieldS3646 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2857 = _M0L8_2afieldS3646;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1102 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2856,
                                                   _M0L3endS2857,
                                                   _M0L5bytesS2855};
              _M0L4tlenS1060 = _M0L11_2aparam__0S1033;
              _M0L2b0S1061 = _M0L4_2axS1076;
              _M0L2b1S1062 = _M0L4_2axS1100;
              _M0L2b2S1063 = _M0L4_2axS1101;
              _M0L4restS1064 = _M0L4_2axS1102;
              goto join_1059;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3649 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2851 = _M0L8_2afieldS3649;
              int32_t _M0L5startS2854 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2852 = _M0L5startS2854 + 2;
              int32_t _M0L8_2afieldS3648 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2853 = _M0L8_2afieldS3648;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1103 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2852,
                                                   _M0L3endS2853,
                                                   _M0L5bytesS2851};
              _M0L4restS1046 = _M0L4_2axS1103;
              _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
              goto join_1045;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3653 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2847 = _M0L8_2afieldS3653;
            int32_t _M0L5startS2850 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2848 = _M0L5startS2850 + 1;
            int32_t _M0L8_2afieldS3652 = _M0L11_2aparam__1S1034.$2;
            int32_t _M0L3endS2849 = _M0L8_2afieldS3652;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1104 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2848,
                                                 _M0L3endS2849,
                                                 _M0L5bytesS2847};
            _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
            _M0L4restS1037 = _M0L4_2axS1104;
            goto join_1035;
          }
        } else if (_M0L4_2axS1076 == 237) {
          moonbit_bytes_t _M0L8_2afieldS3665 = _M0L11_2aparam__1S1034.$0;
          moonbit_bytes_t _M0L5bytesS2844 = _M0L8_2afieldS3665;
          int32_t _M0L5startS2846 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS2845 = _M0L5startS2846 + 1;
          int32_t _M0L6_2atmpS3664 = _M0L5bytesS2844[_M0L6_2atmpS2845];
          int32_t _M0L4_2axS1105 = _M0L6_2atmpS3664;
          if (_M0L4_2axS1105 >= 128 && _M0L4_2axS1105 <= 159) {
            moonbit_bytes_t _M0L8_2afieldS3661 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2841 = _M0L8_2afieldS3661;
            int32_t _M0L5startS2843 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2842 = _M0L5startS2843 + 2;
            int32_t _M0L6_2atmpS3660 = _M0L5bytesS2841[_M0L6_2atmpS2842];
            int32_t _M0L4_2axS1106 = _M0L6_2atmpS3660;
            if (_M0L4_2axS1106 >= 128 && _M0L4_2axS1106 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3657 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2837 = _M0L8_2afieldS3657;
              int32_t _M0L5startS2840 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2838 = _M0L5startS2840 + 3;
              int32_t _M0L8_2afieldS3656 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2839 = _M0L8_2afieldS3656;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1107 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2838,
                                                   _M0L3endS2839,
                                                   _M0L5bytesS2837};
              _M0L4tlenS1060 = _M0L11_2aparam__0S1033;
              _M0L2b0S1061 = _M0L4_2axS1076;
              _M0L2b1S1062 = _M0L4_2axS1105;
              _M0L2b2S1063 = _M0L4_2axS1106;
              _M0L4restS1064 = _M0L4_2axS1107;
              goto join_1059;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3659 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2833 = _M0L8_2afieldS3659;
              int32_t _M0L5startS2836 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2834 = _M0L5startS2836 + 2;
              int32_t _M0L8_2afieldS3658 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2835 = _M0L8_2afieldS3658;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1108 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2834,
                                                   _M0L3endS2835,
                                                   _M0L5bytesS2833};
              _M0L4restS1046 = _M0L4_2axS1108;
              _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
              goto join_1045;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3663 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2829 = _M0L8_2afieldS3663;
            int32_t _M0L5startS2832 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2830 = _M0L5startS2832 + 1;
            int32_t _M0L8_2afieldS3662 = _M0L11_2aparam__1S1034.$2;
            int32_t _M0L3endS2831 = _M0L8_2afieldS3662;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1109 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2830,
                                                 _M0L3endS2831,
                                                 _M0L5bytesS2829};
            _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
            _M0L4restS1037 = _M0L4_2axS1109;
            goto join_1035;
          }
        } else if (_M0L4_2axS1076 >= 238 && _M0L4_2axS1076 <= 239) {
          moonbit_bytes_t _M0L8_2afieldS3675 = _M0L11_2aparam__1S1034.$0;
          moonbit_bytes_t _M0L5bytesS2826 = _M0L8_2afieldS3675;
          int32_t _M0L5startS2828 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS2827 = _M0L5startS2828 + 1;
          int32_t _M0L6_2atmpS3674 = _M0L5bytesS2826[_M0L6_2atmpS2827];
          int32_t _M0L4_2axS1110 = _M0L6_2atmpS3674;
          if (_M0L4_2axS1110 >= 128 && _M0L4_2axS1110 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3671 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2823 = _M0L8_2afieldS3671;
            int32_t _M0L5startS2825 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2824 = _M0L5startS2825 + 2;
            int32_t _M0L6_2atmpS3670 = _M0L5bytesS2823[_M0L6_2atmpS2824];
            int32_t _M0L4_2axS1111 = _M0L6_2atmpS3670;
            if (_M0L4_2axS1111 >= 128 && _M0L4_2axS1111 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3667 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2819 = _M0L8_2afieldS3667;
              int32_t _M0L5startS2822 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2820 = _M0L5startS2822 + 3;
              int32_t _M0L8_2afieldS3666 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2821 = _M0L8_2afieldS3666;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1112 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2820,
                                                   _M0L3endS2821,
                                                   _M0L5bytesS2819};
              _M0L4tlenS1060 = _M0L11_2aparam__0S1033;
              _M0L2b0S1061 = _M0L4_2axS1076;
              _M0L2b1S1062 = _M0L4_2axS1110;
              _M0L2b2S1063 = _M0L4_2axS1111;
              _M0L4restS1064 = _M0L4_2axS1112;
              goto join_1059;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3669 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2815 = _M0L8_2afieldS3669;
              int32_t _M0L5startS2818 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2816 = _M0L5startS2818 + 2;
              int32_t _M0L8_2afieldS3668 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2817 = _M0L8_2afieldS3668;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1113 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2816,
                                                   _M0L3endS2817,
                                                   _M0L5bytesS2815};
              _M0L4restS1046 = _M0L4_2axS1113;
              _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
              goto join_1045;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3673 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2811 = _M0L8_2afieldS3673;
            int32_t _M0L5startS2814 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2812 = _M0L5startS2814 + 1;
            int32_t _M0L8_2afieldS3672 = _M0L11_2aparam__1S1034.$2;
            int32_t _M0L3endS2813 = _M0L8_2afieldS3672;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1114 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2812,
                                                 _M0L3endS2813,
                                                 _M0L5bytesS2811};
            _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
            _M0L4restS1037 = _M0L4_2axS1114;
            goto join_1035;
          }
        } else if (_M0L4_2axS1076 == 240) {
          moonbit_bytes_t _M0L8_2afieldS3689 = _M0L11_2aparam__1S1034.$0;
          moonbit_bytes_t _M0L5bytesS2808 = _M0L8_2afieldS3689;
          int32_t _M0L5startS2810 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS2809 = _M0L5startS2810 + 1;
          int32_t _M0L6_2atmpS3688 = _M0L5bytesS2808[_M0L6_2atmpS2809];
          int32_t _M0L4_2axS1115 = _M0L6_2atmpS3688;
          if (_M0L4_2axS1115 >= 144 && _M0L4_2axS1115 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3685 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2805 = _M0L8_2afieldS3685;
            int32_t _M0L5startS2807 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2806 = _M0L5startS2807 + 2;
            int32_t _M0L6_2atmpS3684 = _M0L5bytesS2805[_M0L6_2atmpS2806];
            int32_t _M0L4_2axS1116 = _M0L6_2atmpS3684;
            if (_M0L4_2axS1116 >= 128 && _M0L4_2axS1116 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3681 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2802 = _M0L8_2afieldS3681;
              int32_t _M0L5startS2804 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2803 = _M0L5startS2804 + 3;
              int32_t _M0L6_2atmpS3680 = _M0L5bytesS2802[_M0L6_2atmpS2803];
              int32_t _M0L4_2axS1117 = _M0L6_2atmpS3680;
              if (_M0L4_2axS1117 >= 128 && _M0L4_2axS1117 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3677 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS2798 = _M0L8_2afieldS3677;
                int32_t _M0L5startS2801 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS2799 = _M0L5startS2801 + 4;
                int32_t _M0L8_2afieldS3676 = _M0L11_2aparam__1S1034.$2;
                int32_t _M0L3endS2800 = _M0L8_2afieldS3676;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1118 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2799,
                                                     _M0L3endS2800,
                                                     _M0L5bytesS2798};
                _M0L4tlenS1049 = _M0L11_2aparam__0S1033;
                _M0L2b0S1050 = _M0L4_2axS1076;
                _M0L2b1S1051 = _M0L4_2axS1115;
                _M0L2b2S1052 = _M0L4_2axS1116;
                _M0L2b3S1053 = _M0L4_2axS1117;
                _M0L4restS1054 = _M0L4_2axS1118;
                goto join_1048;
              } else {
                moonbit_bytes_t _M0L8_2afieldS3679 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS2794 = _M0L8_2afieldS3679;
                int32_t _M0L5startS2797 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS2795 = _M0L5startS2797 + 3;
                int32_t _M0L8_2afieldS3678 = _M0L11_2aparam__1S1034.$2;
                int32_t _M0L3endS2796 = _M0L8_2afieldS3678;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1119 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2795,
                                                     _M0L3endS2796,
                                                     _M0L5bytesS2794};
                _M0L4restS1043 = _M0L4_2axS1119;
                _M0L4tlenS1044 = _M0L11_2aparam__0S1033;
                goto join_1042;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3683 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2790 = _M0L8_2afieldS3683;
              int32_t _M0L5startS2793 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2791 = _M0L5startS2793 + 2;
              int32_t _M0L8_2afieldS3682 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2792 = _M0L8_2afieldS3682;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1120 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2791,
                                                   _M0L3endS2792,
                                                   _M0L5bytesS2790};
              _M0L4restS1040 = _M0L4_2axS1120;
              _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
              goto join_1039;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3687 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2786 = _M0L8_2afieldS3687;
            int32_t _M0L5startS2789 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2787 = _M0L5startS2789 + 1;
            int32_t _M0L8_2afieldS3686 = _M0L11_2aparam__1S1034.$2;
            int32_t _M0L3endS2788 = _M0L8_2afieldS3686;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1121 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2787,
                                                 _M0L3endS2788,
                                                 _M0L5bytesS2786};
            _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
            _M0L4restS1037 = _M0L4_2axS1121;
            goto join_1035;
          }
        } else if (_M0L4_2axS1076 >= 241 && _M0L4_2axS1076 <= 243) {
          moonbit_bytes_t _M0L8_2afieldS3703 = _M0L11_2aparam__1S1034.$0;
          moonbit_bytes_t _M0L5bytesS2783 = _M0L8_2afieldS3703;
          int32_t _M0L5startS2785 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS2784 = _M0L5startS2785 + 1;
          int32_t _M0L6_2atmpS3702 = _M0L5bytesS2783[_M0L6_2atmpS2784];
          int32_t _M0L4_2axS1122 = _M0L6_2atmpS3702;
          if (_M0L4_2axS1122 >= 128 && _M0L4_2axS1122 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3699 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2780 = _M0L8_2afieldS3699;
            int32_t _M0L5startS2782 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2781 = _M0L5startS2782 + 2;
            int32_t _M0L6_2atmpS3698 = _M0L5bytesS2780[_M0L6_2atmpS2781];
            int32_t _M0L4_2axS1123 = _M0L6_2atmpS3698;
            if (_M0L4_2axS1123 >= 128 && _M0L4_2axS1123 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3695 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2777 = _M0L8_2afieldS3695;
              int32_t _M0L5startS2779 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2778 = _M0L5startS2779 + 3;
              int32_t _M0L6_2atmpS3694 = _M0L5bytesS2777[_M0L6_2atmpS2778];
              int32_t _M0L4_2axS1124 = _M0L6_2atmpS3694;
              if (_M0L4_2axS1124 >= 128 && _M0L4_2axS1124 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3691 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS2773 = _M0L8_2afieldS3691;
                int32_t _M0L5startS2776 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS2774 = _M0L5startS2776 + 4;
                int32_t _M0L8_2afieldS3690 = _M0L11_2aparam__1S1034.$2;
                int32_t _M0L3endS2775 = _M0L8_2afieldS3690;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1125 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2774,
                                                     _M0L3endS2775,
                                                     _M0L5bytesS2773};
                _M0L4tlenS1049 = _M0L11_2aparam__0S1033;
                _M0L2b0S1050 = _M0L4_2axS1076;
                _M0L2b1S1051 = _M0L4_2axS1122;
                _M0L2b2S1052 = _M0L4_2axS1123;
                _M0L2b3S1053 = _M0L4_2axS1124;
                _M0L4restS1054 = _M0L4_2axS1125;
                goto join_1048;
              } else {
                moonbit_bytes_t _M0L8_2afieldS3693 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS2769 = _M0L8_2afieldS3693;
                int32_t _M0L5startS2772 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS2770 = _M0L5startS2772 + 3;
                int32_t _M0L8_2afieldS3692 = _M0L11_2aparam__1S1034.$2;
                int32_t _M0L3endS2771 = _M0L8_2afieldS3692;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1126 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2770,
                                                     _M0L3endS2771,
                                                     _M0L5bytesS2769};
                _M0L4restS1043 = _M0L4_2axS1126;
                _M0L4tlenS1044 = _M0L11_2aparam__0S1033;
                goto join_1042;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3697 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2765 = _M0L8_2afieldS3697;
              int32_t _M0L5startS2768 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2766 = _M0L5startS2768 + 2;
              int32_t _M0L8_2afieldS3696 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2767 = _M0L8_2afieldS3696;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1127 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2766,
                                                   _M0L3endS2767,
                                                   _M0L5bytesS2765};
              _M0L4restS1040 = _M0L4_2axS1127;
              _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
              goto join_1039;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3701 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2761 = _M0L8_2afieldS3701;
            int32_t _M0L5startS2764 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2762 = _M0L5startS2764 + 1;
            int32_t _M0L8_2afieldS3700 = _M0L11_2aparam__1S1034.$2;
            int32_t _M0L3endS2763 = _M0L8_2afieldS3700;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1128 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2762,
                                                 _M0L3endS2763,
                                                 _M0L5bytesS2761};
            _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
            _M0L4restS1037 = _M0L4_2axS1128;
            goto join_1035;
          }
        } else if (_M0L4_2axS1076 == 244) {
          moonbit_bytes_t _M0L8_2afieldS3717 = _M0L11_2aparam__1S1034.$0;
          moonbit_bytes_t _M0L5bytesS2758 = _M0L8_2afieldS3717;
          int32_t _M0L5startS2760 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS2759 = _M0L5startS2760 + 1;
          int32_t _M0L6_2atmpS3716 = _M0L5bytesS2758[_M0L6_2atmpS2759];
          int32_t _M0L4_2axS1129 = _M0L6_2atmpS3716;
          if (_M0L4_2axS1129 >= 128 && _M0L4_2axS1129 <= 143) {
            moonbit_bytes_t _M0L8_2afieldS3713 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2755 = _M0L8_2afieldS3713;
            int32_t _M0L5startS2757 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2756 = _M0L5startS2757 + 2;
            int32_t _M0L6_2atmpS3712 = _M0L5bytesS2755[_M0L6_2atmpS2756];
            int32_t _M0L4_2axS1130 = _M0L6_2atmpS3712;
            if (_M0L4_2axS1130 >= 128 && _M0L4_2axS1130 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3709 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2752 = _M0L8_2afieldS3709;
              int32_t _M0L5startS2754 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2753 = _M0L5startS2754 + 3;
              int32_t _M0L6_2atmpS3708 = _M0L5bytesS2752[_M0L6_2atmpS2753];
              int32_t _M0L4_2axS1131 = _M0L6_2atmpS3708;
              if (_M0L4_2axS1131 >= 128 && _M0L4_2axS1131 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3705 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS2748 = _M0L8_2afieldS3705;
                int32_t _M0L5startS2751 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS2749 = _M0L5startS2751 + 4;
                int32_t _M0L8_2afieldS3704 = _M0L11_2aparam__1S1034.$2;
                int32_t _M0L3endS2750 = _M0L8_2afieldS3704;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1132 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2749,
                                                     _M0L3endS2750,
                                                     _M0L5bytesS2748};
                _M0L4tlenS1049 = _M0L11_2aparam__0S1033;
                _M0L2b0S1050 = _M0L4_2axS1076;
                _M0L2b1S1051 = _M0L4_2axS1129;
                _M0L2b2S1052 = _M0L4_2axS1130;
                _M0L2b3S1053 = _M0L4_2axS1131;
                _M0L4restS1054 = _M0L4_2axS1132;
                goto join_1048;
              } else {
                moonbit_bytes_t _M0L8_2afieldS3707 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS2744 = _M0L8_2afieldS3707;
                int32_t _M0L5startS2747 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS2745 = _M0L5startS2747 + 3;
                int32_t _M0L8_2afieldS3706 = _M0L11_2aparam__1S1034.$2;
                int32_t _M0L3endS2746 = _M0L8_2afieldS3706;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1133 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2745,
                                                     _M0L3endS2746,
                                                     _M0L5bytesS2744};
                _M0L4restS1043 = _M0L4_2axS1133;
                _M0L4tlenS1044 = _M0L11_2aparam__0S1033;
                goto join_1042;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3711 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS2740 = _M0L8_2afieldS3711;
              int32_t _M0L5startS2743 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2741 = _M0L5startS2743 + 2;
              int32_t _M0L8_2afieldS3710 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L3endS2742 = _M0L8_2afieldS3710;
              struct _M0TPC15bytes9BytesView _M0L4_2axS1134 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2741,
                                                   _M0L3endS2742,
                                                   _M0L5bytesS2740};
              _M0L4restS1040 = _M0L4_2axS1134;
              _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
              goto join_1039;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3715 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS2736 = _M0L8_2afieldS3715;
            int32_t _M0L5startS2739 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS2737 = _M0L5startS2739 + 1;
            int32_t _M0L8_2afieldS3714 = _M0L11_2aparam__1S1034.$2;
            int32_t _M0L3endS2738 = _M0L8_2afieldS3714;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1135 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2737,
                                                 _M0L3endS2738,
                                                 _M0L5bytesS2736};
            _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
            _M0L4restS1037 = _M0L4_2axS1135;
            goto join_1035;
          }
        } else {
          moonbit_bytes_t _M0L8_2afieldS3719 = _M0L11_2aparam__1S1034.$0;
          moonbit_bytes_t _M0L5bytesS2732 = _M0L8_2afieldS3719;
          int32_t _M0L5startS2735 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS2733 = _M0L5startS2735 + 1;
          int32_t _M0L8_2afieldS3718 = _M0L11_2aparam__1S1034.$2;
          int32_t _M0L3endS2734 = _M0L8_2afieldS3718;
          struct _M0TPC15bytes9BytesView _M0L4_2axS1136 =
            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2733,
                                               _M0L3endS2734,
                                               _M0L5bytesS2732};
          _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
          _M0L4restS1037 = _M0L4_2axS1136;
          goto join_1035;
        }
      } else {
        moonbit_bytes_t _M0L8_2afieldS3913 = _M0L11_2aparam__1S1034.$0;
        moonbit_bytes_t _M0L5bytesS3345 = _M0L8_2afieldS3913;
        int32_t _M0L5startS3346 = _M0L11_2aparam__1S1034.$1;
        int32_t _M0L6_2atmpS3912 = _M0L5bytesS3345[_M0L5startS3346];
        int32_t _M0L4_2axS1137 = _M0L6_2atmpS3912;
        if (_M0L4_2axS1137 >= 0 && _M0L4_2axS1137 <= 127) {
          moonbit_bytes_t _M0L8_2afieldS3723 = _M0L11_2aparam__1S1034.$0;
          moonbit_bytes_t _M0L5bytesS3341 = _M0L8_2afieldS3723;
          int32_t _M0L5startS3344 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS3342 = _M0L5startS3344 + 1;
          int32_t _M0L8_2afieldS3722 = _M0L11_2aparam__1S1034.$2;
          int32_t _M0L3endS3343 = _M0L8_2afieldS3722;
          struct _M0TPC15bytes9BytesView _M0L4_2axS1138 =
            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3342,
                                               _M0L3endS3343,
                                               _M0L5bytesS3341};
          _M0L4tlenS1073 = _M0L11_2aparam__0S1033;
          _M0L4restS1074 = _M0L4_2axS1138;
          _M0L1bS1075 = _M0L4_2axS1137;
          goto join_1072;
        } else {
          int32_t _M0L3endS2958 = _M0L11_2aparam__1S1034.$2;
          int32_t _M0L5startS2959 = _M0L11_2aparam__1S1034.$1;
          int32_t _M0L6_2atmpS2957 = _M0L3endS2958 - _M0L5startS2959;
          if (_M0L6_2atmpS2957 >= 2) {
            if (_M0L4_2axS1137 >= 194 && _M0L4_2axS1137 <= 223) {
              moonbit_bytes_t _M0L8_2afieldS3733 = _M0L11_2aparam__1S1034.$0;
              moonbit_bytes_t _M0L5bytesS3334 = _M0L8_2afieldS3733;
              int32_t _M0L5startS3336 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS3335 = _M0L5startS3336 + 1;
              int32_t _M0L6_2atmpS3732 = _M0L5bytesS3334[_M0L6_2atmpS3335];
              int32_t _M0L4_2axS1139 = _M0L6_2atmpS3732;
              if (_M0L4_2axS1139 >= 128 && _M0L4_2axS1139 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3725 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS3330 = _M0L8_2afieldS3725;
                int32_t _M0L5startS3333 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS3331 = _M0L5startS3333 + 2;
                int32_t _M0L8_2afieldS3724 = _M0L11_2aparam__1S1034.$2;
                int32_t _M0L3endS3332 = _M0L8_2afieldS3724;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1140 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3331,
                                                     _M0L3endS3332,
                                                     _M0L5bytesS3330};
                _M0L4tlenS1067 = _M0L11_2aparam__0S1033;
                _M0L4restS1068 = _M0L4_2axS1140;
                _M0L2b0S1069 = _M0L4_2axS1137;
                _M0L2b1S1070 = _M0L4_2axS1139;
                goto join_1066;
              } else {
                int32_t _M0L3endS3313 = _M0L11_2aparam__1S1034.$2;
                int32_t _M0L5startS3314 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS3312 = _M0L3endS3313 - _M0L5startS3314;
                if (_M0L6_2atmpS3312 >= 3) {
                  int32_t _M0L3endS3316 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L5startS3317 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3315 = _M0L3endS3316 - _M0L5startS3317;
                  if (_M0L6_2atmpS3315 >= 4) {
                    moonbit_bytes_t _M0L8_2afieldS3727 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS3318 = _M0L8_2afieldS3727;
                    int32_t _M0L5startS3321 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3319 = _M0L5startS3321 + 1;
                    int32_t _M0L8_2afieldS3726 = _M0L11_2aparam__1S1034.$2;
                    int32_t _M0L3endS3320 = _M0L8_2afieldS3726;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS1141 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3319,
                                                         _M0L3endS3320,
                                                         _M0L5bytesS3318};
                    _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                    _M0L4restS1037 = _M0L4_2axS1141;
                    goto join_1035;
                  } else {
                    moonbit_bytes_t _M0L8_2afieldS3729 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS3322 = _M0L8_2afieldS3729;
                    int32_t _M0L5startS3325 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3323 = _M0L5startS3325 + 1;
                    int32_t _M0L8_2afieldS3728 = _M0L11_2aparam__1S1034.$2;
                    int32_t _M0L3endS3324 = _M0L8_2afieldS3728;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS1142 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3323,
                                                         _M0L3endS3324,
                                                         _M0L5bytesS3322};
                    _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                    _M0L4restS1037 = _M0L4_2axS1142;
                    goto join_1035;
                  }
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3731 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3326 = _M0L8_2afieldS3731;
                  int32_t _M0L5startS3329 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3327 = _M0L5startS3329 + 1;
                  int32_t _M0L8_2afieldS3730 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3328 = _M0L8_2afieldS3730;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1143 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3327,
                                                       _M0L3endS3328,
                                                       _M0L5bytesS3326};
                  _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                  _M0L4restS1037 = _M0L4_2axS1143;
                  goto join_1035;
                }
              }
            } else {
              int32_t _M0L3endS2961 = _M0L11_2aparam__1S1034.$2;
              int32_t _M0L5startS2962 = _M0L11_2aparam__1S1034.$1;
              int32_t _M0L6_2atmpS2960 = _M0L3endS2961 - _M0L5startS2962;
              if (_M0L6_2atmpS2960 >= 3) {
                if (_M0L4_2axS1137 == 224) {
                  moonbit_bytes_t _M0L8_2afieldS3747 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3228 = _M0L8_2afieldS3747;
                  int32_t _M0L5startS3230 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3229 = _M0L5startS3230 + 1;
                  int32_t _M0L6_2atmpS3746 =
                    _M0L5bytesS3228[_M0L6_2atmpS3229];
                  int32_t _M0L4_2axS1144 = _M0L6_2atmpS3746;
                  if (_M0L4_2axS1144 >= 160 && _M0L4_2axS1144 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS3741 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS3225 = _M0L8_2afieldS3741;
                    int32_t _M0L5startS3227 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3226 = _M0L5startS3227 + 2;
                    int32_t _M0L6_2atmpS3740 =
                      _M0L5bytesS3225[_M0L6_2atmpS3226];
                    int32_t _M0L4_2axS1145 = _M0L6_2atmpS3740;
                    if (_M0L4_2axS1145 >= 128 && _M0L4_2axS1145 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3735 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3221 = _M0L8_2afieldS3735;
                      int32_t _M0L5startS3224 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3222 = _M0L5startS3224 + 3;
                      int32_t _M0L8_2afieldS3734 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3223 = _M0L8_2afieldS3734;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1146 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3222,
                                                           _M0L3endS3223,
                                                           _M0L5bytesS3221};
                      _M0L4tlenS1060 = _M0L11_2aparam__0S1033;
                      _M0L2b0S1061 = _M0L4_2axS1137;
                      _M0L2b1S1062 = _M0L4_2axS1144;
                      _M0L2b2S1063 = _M0L4_2axS1145;
                      _M0L4restS1064 = _M0L4_2axS1146;
                      goto join_1059;
                    } else {
                      int32_t _M0L3endS3211 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L5startS3212 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3210 =
                        _M0L3endS3211 - _M0L5startS3212;
                      if (_M0L6_2atmpS3210 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS3737 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3213 = _M0L8_2afieldS3737;
                        int32_t _M0L5startS3216 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3214 = _M0L5startS3216 + 2;
                        int32_t _M0L8_2afieldS3736 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3215 = _M0L8_2afieldS3736;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1147 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3214,
                                                             _M0L3endS3215,
                                                             _M0L5bytesS3213};
                        _M0L4restS1046 = _M0L4_2axS1147;
                        _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                        goto join_1045;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3739 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3217 = _M0L8_2afieldS3739;
                        int32_t _M0L5startS3220 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3218 = _M0L5startS3220 + 2;
                        int32_t _M0L8_2afieldS3738 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3219 = _M0L8_2afieldS3738;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1148 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3218,
                                                             _M0L3endS3219,
                                                             _M0L5bytesS3217};
                        _M0L4restS1046 = _M0L4_2axS1148;
                        _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                        goto join_1045;
                      }
                    }
                  } else {
                    int32_t _M0L3endS3200 = _M0L11_2aparam__1S1034.$2;
                    int32_t _M0L5startS3201 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3199 =
                      _M0L3endS3200 - _M0L5startS3201;
                    if (_M0L6_2atmpS3199 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS3743 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3202 = _M0L8_2afieldS3743;
                      int32_t _M0L5startS3205 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3203 = _M0L5startS3205 + 1;
                      int32_t _M0L8_2afieldS3742 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3204 = _M0L8_2afieldS3742;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1149 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3203,
                                                           _M0L3endS3204,
                                                           _M0L5bytesS3202};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1149;
                      goto join_1035;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3745 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3206 = _M0L8_2afieldS3745;
                      int32_t _M0L5startS3209 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3207 = _M0L5startS3209 + 1;
                      int32_t _M0L8_2afieldS3744 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3208 = _M0L8_2afieldS3744;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1150 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3207,
                                                           _M0L3endS3208,
                                                           _M0L5bytesS3206};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1150;
                      goto join_1035;
                    }
                  }
                } else if (_M0L4_2axS1137 >= 225 && _M0L4_2axS1137 <= 236) {
                  moonbit_bytes_t _M0L8_2afieldS3761 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3196 = _M0L8_2afieldS3761;
                  int32_t _M0L5startS3198 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3197 = _M0L5startS3198 + 1;
                  int32_t _M0L6_2atmpS3760 =
                    _M0L5bytesS3196[_M0L6_2atmpS3197];
                  int32_t _M0L4_2axS1151 = _M0L6_2atmpS3760;
                  if (_M0L4_2axS1151 >= 128 && _M0L4_2axS1151 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS3755 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS3193 = _M0L8_2afieldS3755;
                    int32_t _M0L5startS3195 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3194 = _M0L5startS3195 + 2;
                    int32_t _M0L6_2atmpS3754 =
                      _M0L5bytesS3193[_M0L6_2atmpS3194];
                    int32_t _M0L4_2axS1152 = _M0L6_2atmpS3754;
                    if (_M0L4_2axS1152 >= 128 && _M0L4_2axS1152 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3749 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3189 = _M0L8_2afieldS3749;
                      int32_t _M0L5startS3192 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3190 = _M0L5startS3192 + 3;
                      int32_t _M0L8_2afieldS3748 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3191 = _M0L8_2afieldS3748;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1153 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3190,
                                                           _M0L3endS3191,
                                                           _M0L5bytesS3189};
                      _M0L4tlenS1060 = _M0L11_2aparam__0S1033;
                      _M0L2b0S1061 = _M0L4_2axS1137;
                      _M0L2b1S1062 = _M0L4_2axS1151;
                      _M0L2b2S1063 = _M0L4_2axS1152;
                      _M0L4restS1064 = _M0L4_2axS1153;
                      goto join_1059;
                    } else {
                      int32_t _M0L3endS3179 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L5startS3180 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3178 =
                        _M0L3endS3179 - _M0L5startS3180;
                      if (_M0L6_2atmpS3178 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS3751 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3181 = _M0L8_2afieldS3751;
                        int32_t _M0L5startS3184 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3182 = _M0L5startS3184 + 2;
                        int32_t _M0L8_2afieldS3750 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3183 = _M0L8_2afieldS3750;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1154 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3182,
                                                             _M0L3endS3183,
                                                             _M0L5bytesS3181};
                        _M0L4restS1046 = _M0L4_2axS1154;
                        _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                        goto join_1045;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3753 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3185 = _M0L8_2afieldS3753;
                        int32_t _M0L5startS3188 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3186 = _M0L5startS3188 + 2;
                        int32_t _M0L8_2afieldS3752 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3187 = _M0L8_2afieldS3752;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1155 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3186,
                                                             _M0L3endS3187,
                                                             _M0L5bytesS3185};
                        _M0L4restS1046 = _M0L4_2axS1155;
                        _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                        goto join_1045;
                      }
                    }
                  } else {
                    int32_t _M0L3endS3168 = _M0L11_2aparam__1S1034.$2;
                    int32_t _M0L5startS3169 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3167 =
                      _M0L3endS3168 - _M0L5startS3169;
                    if (_M0L6_2atmpS3167 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS3757 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3170 = _M0L8_2afieldS3757;
                      int32_t _M0L5startS3173 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3171 = _M0L5startS3173 + 1;
                      int32_t _M0L8_2afieldS3756 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3172 = _M0L8_2afieldS3756;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1156 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3171,
                                                           _M0L3endS3172,
                                                           _M0L5bytesS3170};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1156;
                      goto join_1035;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3759 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3174 = _M0L8_2afieldS3759;
                      int32_t _M0L5startS3177 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3175 = _M0L5startS3177 + 1;
                      int32_t _M0L8_2afieldS3758 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3176 = _M0L8_2afieldS3758;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1157 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3175,
                                                           _M0L3endS3176,
                                                           _M0L5bytesS3174};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1157;
                      goto join_1035;
                    }
                  }
                } else if (_M0L4_2axS1137 == 237) {
                  moonbit_bytes_t _M0L8_2afieldS3775 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3164 = _M0L8_2afieldS3775;
                  int32_t _M0L5startS3166 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3165 = _M0L5startS3166 + 1;
                  int32_t _M0L6_2atmpS3774 =
                    _M0L5bytesS3164[_M0L6_2atmpS3165];
                  int32_t _M0L4_2axS1158 = _M0L6_2atmpS3774;
                  if (_M0L4_2axS1158 >= 128 && _M0L4_2axS1158 <= 159) {
                    moonbit_bytes_t _M0L8_2afieldS3769 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS3161 = _M0L8_2afieldS3769;
                    int32_t _M0L5startS3163 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3162 = _M0L5startS3163 + 2;
                    int32_t _M0L6_2atmpS3768 =
                      _M0L5bytesS3161[_M0L6_2atmpS3162];
                    int32_t _M0L4_2axS1159 = _M0L6_2atmpS3768;
                    if (_M0L4_2axS1159 >= 128 && _M0L4_2axS1159 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3763 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3157 = _M0L8_2afieldS3763;
                      int32_t _M0L5startS3160 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3158 = _M0L5startS3160 + 3;
                      int32_t _M0L8_2afieldS3762 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3159 = _M0L8_2afieldS3762;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1160 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3158,
                                                           _M0L3endS3159,
                                                           _M0L5bytesS3157};
                      _M0L4tlenS1060 = _M0L11_2aparam__0S1033;
                      _M0L2b0S1061 = _M0L4_2axS1137;
                      _M0L2b1S1062 = _M0L4_2axS1158;
                      _M0L2b2S1063 = _M0L4_2axS1159;
                      _M0L4restS1064 = _M0L4_2axS1160;
                      goto join_1059;
                    } else {
                      int32_t _M0L3endS3147 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L5startS3148 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3146 =
                        _M0L3endS3147 - _M0L5startS3148;
                      if (_M0L6_2atmpS3146 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS3765 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3149 = _M0L8_2afieldS3765;
                        int32_t _M0L5startS3152 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3150 = _M0L5startS3152 + 2;
                        int32_t _M0L8_2afieldS3764 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3151 = _M0L8_2afieldS3764;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1161 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3150,
                                                             _M0L3endS3151,
                                                             _M0L5bytesS3149};
                        _M0L4restS1046 = _M0L4_2axS1161;
                        _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                        goto join_1045;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3767 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3153 = _M0L8_2afieldS3767;
                        int32_t _M0L5startS3156 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3154 = _M0L5startS3156 + 2;
                        int32_t _M0L8_2afieldS3766 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3155 = _M0L8_2afieldS3766;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1162 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3154,
                                                             _M0L3endS3155,
                                                             _M0L5bytesS3153};
                        _M0L4restS1046 = _M0L4_2axS1162;
                        _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                        goto join_1045;
                      }
                    }
                  } else {
                    int32_t _M0L3endS3136 = _M0L11_2aparam__1S1034.$2;
                    int32_t _M0L5startS3137 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3135 =
                      _M0L3endS3136 - _M0L5startS3137;
                    if (_M0L6_2atmpS3135 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS3771 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3138 = _M0L8_2afieldS3771;
                      int32_t _M0L5startS3141 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3139 = _M0L5startS3141 + 1;
                      int32_t _M0L8_2afieldS3770 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3140 = _M0L8_2afieldS3770;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1163 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3139,
                                                           _M0L3endS3140,
                                                           _M0L5bytesS3138};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1163;
                      goto join_1035;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3773 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3142 = _M0L8_2afieldS3773;
                      int32_t _M0L5startS3145 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3143 = _M0L5startS3145 + 1;
                      int32_t _M0L8_2afieldS3772 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3144 = _M0L8_2afieldS3772;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1164 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3143,
                                                           _M0L3endS3144,
                                                           _M0L5bytesS3142};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1164;
                      goto join_1035;
                    }
                  }
                } else if (_M0L4_2axS1137 >= 238 && _M0L4_2axS1137 <= 239) {
                  moonbit_bytes_t _M0L8_2afieldS3789 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3132 = _M0L8_2afieldS3789;
                  int32_t _M0L5startS3134 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3133 = _M0L5startS3134 + 1;
                  int32_t _M0L6_2atmpS3788 =
                    _M0L5bytesS3132[_M0L6_2atmpS3133];
                  int32_t _M0L4_2axS1165 = _M0L6_2atmpS3788;
                  if (_M0L4_2axS1165 >= 128 && _M0L4_2axS1165 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS3783 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS3129 = _M0L8_2afieldS3783;
                    int32_t _M0L5startS3131 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3130 = _M0L5startS3131 + 2;
                    int32_t _M0L6_2atmpS3782 =
                      _M0L5bytesS3129[_M0L6_2atmpS3130];
                    int32_t _M0L4_2axS1166 = _M0L6_2atmpS3782;
                    if (_M0L4_2axS1166 >= 128 && _M0L4_2axS1166 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3777 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3125 = _M0L8_2afieldS3777;
                      int32_t _M0L5startS3128 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3126 = _M0L5startS3128 + 3;
                      int32_t _M0L8_2afieldS3776 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3127 = _M0L8_2afieldS3776;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1167 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3126,
                                                           _M0L3endS3127,
                                                           _M0L5bytesS3125};
                      _M0L4tlenS1060 = _M0L11_2aparam__0S1033;
                      _M0L2b0S1061 = _M0L4_2axS1137;
                      _M0L2b1S1062 = _M0L4_2axS1165;
                      _M0L2b2S1063 = _M0L4_2axS1166;
                      _M0L4restS1064 = _M0L4_2axS1167;
                      goto join_1059;
                    } else {
                      int32_t _M0L3endS3115 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L5startS3116 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3114 =
                        _M0L3endS3115 - _M0L5startS3116;
                      if (_M0L6_2atmpS3114 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS3779 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3117 = _M0L8_2afieldS3779;
                        int32_t _M0L5startS3120 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3118 = _M0L5startS3120 + 2;
                        int32_t _M0L8_2afieldS3778 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3119 = _M0L8_2afieldS3778;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1168 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3118,
                                                             _M0L3endS3119,
                                                             _M0L5bytesS3117};
                        _M0L4restS1046 = _M0L4_2axS1168;
                        _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                        goto join_1045;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3781 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3121 = _M0L8_2afieldS3781;
                        int32_t _M0L5startS3124 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3122 = _M0L5startS3124 + 2;
                        int32_t _M0L8_2afieldS3780 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3123 = _M0L8_2afieldS3780;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1169 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3122,
                                                             _M0L3endS3123,
                                                             _M0L5bytesS3121};
                        _M0L4restS1046 = _M0L4_2axS1169;
                        _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                        goto join_1045;
                      }
                    }
                  } else {
                    int32_t _M0L3endS3104 = _M0L11_2aparam__1S1034.$2;
                    int32_t _M0L5startS3105 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3103 =
                      _M0L3endS3104 - _M0L5startS3105;
                    if (_M0L6_2atmpS3103 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS3785 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3106 = _M0L8_2afieldS3785;
                      int32_t _M0L5startS3109 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3107 = _M0L5startS3109 + 1;
                      int32_t _M0L8_2afieldS3784 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3108 = _M0L8_2afieldS3784;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1170 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3107,
                                                           _M0L3endS3108,
                                                           _M0L5bytesS3106};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1170;
                      goto join_1035;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3787 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3110 = _M0L8_2afieldS3787;
                      int32_t _M0L5startS3113 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3111 = _M0L5startS3113 + 1;
                      int32_t _M0L8_2afieldS3786 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3112 = _M0L8_2afieldS3786;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1171 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3111,
                                                           _M0L3endS3112,
                                                           _M0L5bytesS3110};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1171;
                      goto join_1035;
                    }
                  }
                } else {
                  int32_t _M0L3endS2964 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L5startS2965 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS2963 = _M0L3endS2964 - _M0L5startS2965;
                  if (_M0L6_2atmpS2963 >= 4) {
                    if (_M0L4_2axS1137 == 240) {
                      moonbit_bytes_t _M0L8_2afieldS3803 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3042 = _M0L8_2afieldS3803;
                      int32_t _M0L5startS3044 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3043 = _M0L5startS3044 + 1;
                      int32_t _M0L6_2atmpS3802 =
                        _M0L5bytesS3042[_M0L6_2atmpS3043];
                      int32_t _M0L4_2axS1172 = _M0L6_2atmpS3802;
                      if (_M0L4_2axS1172 >= 144 && _M0L4_2axS1172 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS3799 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3039 = _M0L8_2afieldS3799;
                        int32_t _M0L5startS3041 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3040 = _M0L5startS3041 + 2;
                        int32_t _M0L6_2atmpS3798 =
                          _M0L5bytesS3039[_M0L6_2atmpS3040];
                        int32_t _M0L4_2axS1173 = _M0L6_2atmpS3798;
                        if (_M0L4_2axS1173 >= 128 && _M0L4_2axS1173 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS3795 =
                            _M0L11_2aparam__1S1034.$0;
                          moonbit_bytes_t _M0L5bytesS3036 =
                            _M0L8_2afieldS3795;
                          int32_t _M0L5startS3038 = _M0L11_2aparam__1S1034.$1;
                          int32_t _M0L6_2atmpS3037 = _M0L5startS3038 + 3;
                          int32_t _M0L6_2atmpS3794 =
                            _M0L5bytesS3036[_M0L6_2atmpS3037];
                          int32_t _M0L4_2axS1174 = _M0L6_2atmpS3794;
                          if (_M0L4_2axS1174 >= 128 && _M0L4_2axS1174 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS3791 =
                              _M0L11_2aparam__1S1034.$0;
                            moonbit_bytes_t _M0L5bytesS3032 =
                              _M0L8_2afieldS3791;
                            int32_t _M0L5startS3035 =
                              _M0L11_2aparam__1S1034.$1;
                            int32_t _M0L6_2atmpS3033 = _M0L5startS3035 + 4;
                            int32_t _M0L8_2afieldS3790 =
                              _M0L11_2aparam__1S1034.$2;
                            int32_t _M0L3endS3034 = _M0L8_2afieldS3790;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1175 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3033,
                                                                 _M0L3endS3034,
                                                                 _M0L5bytesS3032};
                            _M0L4tlenS1049 = _M0L11_2aparam__0S1033;
                            _M0L2b0S1050 = _M0L4_2axS1137;
                            _M0L2b1S1051 = _M0L4_2axS1172;
                            _M0L2b2S1052 = _M0L4_2axS1173;
                            _M0L2b3S1053 = _M0L4_2axS1174;
                            _M0L4restS1054 = _M0L4_2axS1175;
                            goto join_1048;
                          } else {
                            moonbit_bytes_t _M0L8_2afieldS3793 =
                              _M0L11_2aparam__1S1034.$0;
                            moonbit_bytes_t _M0L5bytesS3028 =
                              _M0L8_2afieldS3793;
                            int32_t _M0L5startS3031 =
                              _M0L11_2aparam__1S1034.$1;
                            int32_t _M0L6_2atmpS3029 = _M0L5startS3031 + 3;
                            int32_t _M0L8_2afieldS3792 =
                              _M0L11_2aparam__1S1034.$2;
                            int32_t _M0L3endS3030 = _M0L8_2afieldS3792;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1176 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3029,
                                                                 _M0L3endS3030,
                                                                 _M0L5bytesS3028};
                            _M0L4restS1043 = _M0L4_2axS1176;
                            _M0L4tlenS1044 = _M0L11_2aparam__0S1033;
                            goto join_1042;
                          }
                        } else {
                          moonbit_bytes_t _M0L8_2afieldS3797 =
                            _M0L11_2aparam__1S1034.$0;
                          moonbit_bytes_t _M0L5bytesS3024 =
                            _M0L8_2afieldS3797;
                          int32_t _M0L5startS3027 = _M0L11_2aparam__1S1034.$1;
                          int32_t _M0L6_2atmpS3025 = _M0L5startS3027 + 2;
                          int32_t _M0L8_2afieldS3796 =
                            _M0L11_2aparam__1S1034.$2;
                          int32_t _M0L3endS3026 = _M0L8_2afieldS3796;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS1177 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3025,
                                                               _M0L3endS3026,
                                                               _M0L5bytesS3024};
                          _M0L4restS1040 = _M0L4_2axS1177;
                          _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
                          goto join_1039;
                        }
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3801 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3020 = _M0L8_2afieldS3801;
                        int32_t _M0L5startS3023 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3021 = _M0L5startS3023 + 1;
                        int32_t _M0L8_2afieldS3800 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3022 = _M0L8_2afieldS3800;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1178 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3021,
                                                             _M0L3endS3022,
                                                             _M0L5bytesS3020};
                        _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                        _M0L4restS1037 = _M0L4_2axS1178;
                        goto join_1035;
                      }
                    } else if (
                             _M0L4_2axS1137 >= 241 && _M0L4_2axS1137 <= 243
                           ) {
                      moonbit_bytes_t _M0L8_2afieldS3817 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3017 = _M0L8_2afieldS3817;
                      int32_t _M0L5startS3019 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3018 = _M0L5startS3019 + 1;
                      int32_t _M0L6_2atmpS3816 =
                        _M0L5bytesS3017[_M0L6_2atmpS3018];
                      int32_t _M0L4_2axS1179 = _M0L6_2atmpS3816;
                      if (_M0L4_2axS1179 >= 128 && _M0L4_2axS1179 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS3813 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3014 = _M0L8_2afieldS3813;
                        int32_t _M0L5startS3016 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3015 = _M0L5startS3016 + 2;
                        int32_t _M0L6_2atmpS3812 =
                          _M0L5bytesS3014[_M0L6_2atmpS3015];
                        int32_t _M0L4_2axS1180 = _M0L6_2atmpS3812;
                        if (_M0L4_2axS1180 >= 128 && _M0L4_2axS1180 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS3809 =
                            _M0L11_2aparam__1S1034.$0;
                          moonbit_bytes_t _M0L5bytesS3011 =
                            _M0L8_2afieldS3809;
                          int32_t _M0L5startS3013 = _M0L11_2aparam__1S1034.$1;
                          int32_t _M0L6_2atmpS3012 = _M0L5startS3013 + 3;
                          int32_t _M0L6_2atmpS3808 =
                            _M0L5bytesS3011[_M0L6_2atmpS3012];
                          int32_t _M0L4_2axS1181 = _M0L6_2atmpS3808;
                          if (_M0L4_2axS1181 >= 128 && _M0L4_2axS1181 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS3805 =
                              _M0L11_2aparam__1S1034.$0;
                            moonbit_bytes_t _M0L5bytesS3007 =
                              _M0L8_2afieldS3805;
                            int32_t _M0L5startS3010 =
                              _M0L11_2aparam__1S1034.$1;
                            int32_t _M0L6_2atmpS3008 = _M0L5startS3010 + 4;
                            int32_t _M0L8_2afieldS3804 =
                              _M0L11_2aparam__1S1034.$2;
                            int32_t _M0L3endS3009 = _M0L8_2afieldS3804;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1182 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3008,
                                                                 _M0L3endS3009,
                                                                 _M0L5bytesS3007};
                            _M0L4tlenS1049 = _M0L11_2aparam__0S1033;
                            _M0L2b0S1050 = _M0L4_2axS1137;
                            _M0L2b1S1051 = _M0L4_2axS1179;
                            _M0L2b2S1052 = _M0L4_2axS1180;
                            _M0L2b3S1053 = _M0L4_2axS1181;
                            _M0L4restS1054 = _M0L4_2axS1182;
                            goto join_1048;
                          } else {
                            moonbit_bytes_t _M0L8_2afieldS3807 =
                              _M0L11_2aparam__1S1034.$0;
                            moonbit_bytes_t _M0L5bytesS3003 =
                              _M0L8_2afieldS3807;
                            int32_t _M0L5startS3006 =
                              _M0L11_2aparam__1S1034.$1;
                            int32_t _M0L6_2atmpS3004 = _M0L5startS3006 + 3;
                            int32_t _M0L8_2afieldS3806 =
                              _M0L11_2aparam__1S1034.$2;
                            int32_t _M0L3endS3005 = _M0L8_2afieldS3806;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1183 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3004,
                                                                 _M0L3endS3005,
                                                                 _M0L5bytesS3003};
                            _M0L4restS1043 = _M0L4_2axS1183;
                            _M0L4tlenS1044 = _M0L11_2aparam__0S1033;
                            goto join_1042;
                          }
                        } else {
                          moonbit_bytes_t _M0L8_2afieldS3811 =
                            _M0L11_2aparam__1S1034.$0;
                          moonbit_bytes_t _M0L5bytesS2999 =
                            _M0L8_2afieldS3811;
                          int32_t _M0L5startS3002 = _M0L11_2aparam__1S1034.$1;
                          int32_t _M0L6_2atmpS3000 = _M0L5startS3002 + 2;
                          int32_t _M0L8_2afieldS3810 =
                            _M0L11_2aparam__1S1034.$2;
                          int32_t _M0L3endS3001 = _M0L8_2afieldS3810;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS1184 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3000,
                                                               _M0L3endS3001,
                                                               _M0L5bytesS2999};
                          _M0L4restS1040 = _M0L4_2axS1184;
                          _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
                          goto join_1039;
                        }
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3815 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS2995 = _M0L8_2afieldS3815;
                        int32_t _M0L5startS2998 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS2996 = _M0L5startS2998 + 1;
                        int32_t _M0L8_2afieldS3814 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS2997 = _M0L8_2afieldS3814;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1185 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2996,
                                                             _M0L3endS2997,
                                                             _M0L5bytesS2995};
                        _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                        _M0L4restS1037 = _M0L4_2axS1185;
                        goto join_1035;
                      }
                    } else if (_M0L4_2axS1137 == 244) {
                      moonbit_bytes_t _M0L8_2afieldS3831 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS2992 = _M0L8_2afieldS3831;
                      int32_t _M0L5startS2994 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS2993 = _M0L5startS2994 + 1;
                      int32_t _M0L6_2atmpS3830 =
                        _M0L5bytesS2992[_M0L6_2atmpS2993];
                      int32_t _M0L4_2axS1186 = _M0L6_2atmpS3830;
                      if (_M0L4_2axS1186 >= 128 && _M0L4_2axS1186 <= 143) {
                        moonbit_bytes_t _M0L8_2afieldS3827 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS2989 = _M0L8_2afieldS3827;
                        int32_t _M0L5startS2991 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS2990 = _M0L5startS2991 + 2;
                        int32_t _M0L6_2atmpS3826 =
                          _M0L5bytesS2989[_M0L6_2atmpS2990];
                        int32_t _M0L4_2axS1187 = _M0L6_2atmpS3826;
                        if (_M0L4_2axS1187 >= 128 && _M0L4_2axS1187 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS3823 =
                            _M0L11_2aparam__1S1034.$0;
                          moonbit_bytes_t _M0L5bytesS2986 =
                            _M0L8_2afieldS3823;
                          int32_t _M0L5startS2988 = _M0L11_2aparam__1S1034.$1;
                          int32_t _M0L6_2atmpS2987 = _M0L5startS2988 + 3;
                          int32_t _M0L6_2atmpS3822 =
                            _M0L5bytesS2986[_M0L6_2atmpS2987];
                          int32_t _M0L4_2axS1188 = _M0L6_2atmpS3822;
                          if (_M0L4_2axS1188 >= 128 && _M0L4_2axS1188 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS3819 =
                              _M0L11_2aparam__1S1034.$0;
                            moonbit_bytes_t _M0L5bytesS2982 =
                              _M0L8_2afieldS3819;
                            int32_t _M0L5startS2985 =
                              _M0L11_2aparam__1S1034.$1;
                            int32_t _M0L6_2atmpS2983 = _M0L5startS2985 + 4;
                            int32_t _M0L8_2afieldS3818 =
                              _M0L11_2aparam__1S1034.$2;
                            int32_t _M0L3endS2984 = _M0L8_2afieldS3818;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1189 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2983,
                                                                 _M0L3endS2984,
                                                                 _M0L5bytesS2982};
                            _M0L4tlenS1049 = _M0L11_2aparam__0S1033;
                            _M0L2b0S1050 = _M0L4_2axS1137;
                            _M0L2b1S1051 = _M0L4_2axS1186;
                            _M0L2b2S1052 = _M0L4_2axS1187;
                            _M0L2b3S1053 = _M0L4_2axS1188;
                            _M0L4restS1054 = _M0L4_2axS1189;
                            goto join_1048;
                          } else {
                            moonbit_bytes_t _M0L8_2afieldS3821 =
                              _M0L11_2aparam__1S1034.$0;
                            moonbit_bytes_t _M0L5bytesS2978 =
                              _M0L8_2afieldS3821;
                            int32_t _M0L5startS2981 =
                              _M0L11_2aparam__1S1034.$1;
                            int32_t _M0L6_2atmpS2979 = _M0L5startS2981 + 3;
                            int32_t _M0L8_2afieldS3820 =
                              _M0L11_2aparam__1S1034.$2;
                            int32_t _M0L3endS2980 = _M0L8_2afieldS3820;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS1190 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2979,
                                                                 _M0L3endS2980,
                                                                 _M0L5bytesS2978};
                            _M0L4restS1043 = _M0L4_2axS1190;
                            _M0L4tlenS1044 = _M0L11_2aparam__0S1033;
                            goto join_1042;
                          }
                        } else {
                          moonbit_bytes_t _M0L8_2afieldS3825 =
                            _M0L11_2aparam__1S1034.$0;
                          moonbit_bytes_t _M0L5bytesS2974 =
                            _M0L8_2afieldS3825;
                          int32_t _M0L5startS2977 = _M0L11_2aparam__1S1034.$1;
                          int32_t _M0L6_2atmpS2975 = _M0L5startS2977 + 2;
                          int32_t _M0L8_2afieldS3824 =
                            _M0L11_2aparam__1S1034.$2;
                          int32_t _M0L3endS2976 = _M0L8_2afieldS3824;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS1191 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2975,
                                                               _M0L3endS2976,
                                                               _M0L5bytesS2974};
                          _M0L4restS1040 = _M0L4_2axS1191;
                          _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
                          goto join_1039;
                        }
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3829 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS2970 = _M0L8_2afieldS3829;
                        int32_t _M0L5startS2973 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS2971 = _M0L5startS2973 + 1;
                        int32_t _M0L8_2afieldS3828 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS2972 = _M0L8_2afieldS3828;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1192 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2971,
                                                             _M0L3endS2972,
                                                             _M0L5bytesS2970};
                        _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                        _M0L4restS1037 = _M0L4_2axS1192;
                        goto join_1035;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3833 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS2966 = _M0L8_2afieldS3833;
                      int32_t _M0L5startS2969 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS2967 = _M0L5startS2969 + 1;
                      int32_t _M0L8_2afieldS3832 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS2968 = _M0L8_2afieldS3832;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1193 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2967,
                                                           _M0L3endS2968,
                                                           _M0L5bytesS2966};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1193;
                      goto join_1035;
                    }
                  } else if (_M0L4_2axS1137 == 240) {
                    moonbit_bytes_t _M0L8_2afieldS3843 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS3100 = _M0L8_2afieldS3843;
                    int32_t _M0L5startS3102 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3101 = _M0L5startS3102 + 1;
                    int32_t _M0L6_2atmpS3842 =
                      _M0L5bytesS3100[_M0L6_2atmpS3101];
                    int32_t _M0L4_2axS1194 = _M0L6_2atmpS3842;
                    if (_M0L4_2axS1194 >= 144 && _M0L4_2axS1194 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3839 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3097 = _M0L8_2afieldS3839;
                      int32_t _M0L5startS3099 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3098 = _M0L5startS3099 + 2;
                      int32_t _M0L6_2atmpS3838 =
                        _M0L5bytesS3097[_M0L6_2atmpS3098];
                      int32_t _M0L4_2axS1195 = _M0L6_2atmpS3838;
                      if (_M0L4_2axS1195 >= 128 && _M0L4_2axS1195 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS3835 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3093 = _M0L8_2afieldS3835;
                        int32_t _M0L5startS3096 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3094 = _M0L5startS3096 + 3;
                        int32_t _M0L8_2afieldS3834 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3095 = _M0L8_2afieldS3834;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1196 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3094,
                                                             _M0L3endS3095,
                                                             _M0L5bytesS3093};
                        _M0L4restS1043 = _M0L4_2axS1196;
                        _M0L4tlenS1044 = _M0L11_2aparam__0S1033;
                        goto join_1042;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3837 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3089 = _M0L8_2afieldS3837;
                        int32_t _M0L5startS3092 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3090 = _M0L5startS3092 + 2;
                        int32_t _M0L8_2afieldS3836 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3091 = _M0L8_2afieldS3836;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1197 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3090,
                                                             _M0L3endS3091,
                                                             _M0L5bytesS3089};
                        _M0L4restS1040 = _M0L4_2axS1197;
                        _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
                        goto join_1039;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3841 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3085 = _M0L8_2afieldS3841;
                      int32_t _M0L5startS3088 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3086 = _M0L5startS3088 + 1;
                      int32_t _M0L8_2afieldS3840 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3087 = _M0L8_2afieldS3840;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1198 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3086,
                                                           _M0L3endS3087,
                                                           _M0L5bytesS3085};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1198;
                      goto join_1035;
                    }
                  } else if (_M0L4_2axS1137 >= 241 && _M0L4_2axS1137 <= 243) {
                    moonbit_bytes_t _M0L8_2afieldS3853 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS3082 = _M0L8_2afieldS3853;
                    int32_t _M0L5startS3084 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3083 = _M0L5startS3084 + 1;
                    int32_t _M0L6_2atmpS3852 =
                      _M0L5bytesS3082[_M0L6_2atmpS3083];
                    int32_t _M0L4_2axS1199 = _M0L6_2atmpS3852;
                    if (_M0L4_2axS1199 >= 128 && _M0L4_2axS1199 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3849 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3079 = _M0L8_2afieldS3849;
                      int32_t _M0L5startS3081 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3080 = _M0L5startS3081 + 2;
                      int32_t _M0L6_2atmpS3848 =
                        _M0L5bytesS3079[_M0L6_2atmpS3080];
                      int32_t _M0L4_2axS1200 = _M0L6_2atmpS3848;
                      if (_M0L4_2axS1200 >= 128 && _M0L4_2axS1200 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS3845 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3075 = _M0L8_2afieldS3845;
                        int32_t _M0L5startS3078 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3076 = _M0L5startS3078 + 3;
                        int32_t _M0L8_2afieldS3844 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3077 = _M0L8_2afieldS3844;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1201 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3076,
                                                             _M0L3endS3077,
                                                             _M0L5bytesS3075};
                        _M0L4restS1043 = _M0L4_2axS1201;
                        _M0L4tlenS1044 = _M0L11_2aparam__0S1033;
                        goto join_1042;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3847 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3071 = _M0L8_2afieldS3847;
                        int32_t _M0L5startS3074 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3072 = _M0L5startS3074 + 2;
                        int32_t _M0L8_2afieldS3846 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3073 = _M0L8_2afieldS3846;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1202 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3072,
                                                             _M0L3endS3073,
                                                             _M0L5bytesS3071};
                        _M0L4restS1040 = _M0L4_2axS1202;
                        _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
                        goto join_1039;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3851 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3067 = _M0L8_2afieldS3851;
                      int32_t _M0L5startS3070 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3068 = _M0L5startS3070 + 1;
                      int32_t _M0L8_2afieldS3850 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3069 = _M0L8_2afieldS3850;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1203 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3068,
                                                           _M0L3endS3069,
                                                           _M0L5bytesS3067};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1203;
                      goto join_1035;
                    }
                  } else if (_M0L4_2axS1137 == 244) {
                    moonbit_bytes_t _M0L8_2afieldS3863 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS3064 = _M0L8_2afieldS3863;
                    int32_t _M0L5startS3066 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3065 = _M0L5startS3066 + 1;
                    int32_t _M0L6_2atmpS3862 =
                      _M0L5bytesS3064[_M0L6_2atmpS3065];
                    int32_t _M0L4_2axS1204 = _M0L6_2atmpS3862;
                    if (_M0L4_2axS1204 >= 128 && _M0L4_2axS1204 <= 143) {
                      moonbit_bytes_t _M0L8_2afieldS3859 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3061 = _M0L8_2afieldS3859;
                      int32_t _M0L5startS3063 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3062 = _M0L5startS3063 + 2;
                      int32_t _M0L6_2atmpS3858 =
                        _M0L5bytesS3061[_M0L6_2atmpS3062];
                      int32_t _M0L4_2axS1205 = _M0L6_2atmpS3858;
                      if (_M0L4_2axS1205 >= 128 && _M0L4_2axS1205 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS3855 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3057 = _M0L8_2afieldS3855;
                        int32_t _M0L5startS3060 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3058 = _M0L5startS3060 + 3;
                        int32_t _M0L8_2afieldS3854 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3059 = _M0L8_2afieldS3854;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1206 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3058,
                                                             _M0L3endS3059,
                                                             _M0L5bytesS3057};
                        _M0L4restS1043 = _M0L4_2axS1206;
                        _M0L4tlenS1044 = _M0L11_2aparam__0S1033;
                        goto join_1042;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3857 =
                          _M0L11_2aparam__1S1034.$0;
                        moonbit_bytes_t _M0L5bytesS3053 = _M0L8_2afieldS3857;
                        int32_t _M0L5startS3056 = _M0L11_2aparam__1S1034.$1;
                        int32_t _M0L6_2atmpS3054 = _M0L5startS3056 + 2;
                        int32_t _M0L8_2afieldS3856 =
                          _M0L11_2aparam__1S1034.$2;
                        int32_t _M0L3endS3055 = _M0L8_2afieldS3856;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS1207 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3054,
                                                             _M0L3endS3055,
                                                             _M0L5bytesS3053};
                        _M0L4restS1040 = _M0L4_2axS1207;
                        _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
                        goto join_1039;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3861 =
                        _M0L11_2aparam__1S1034.$0;
                      moonbit_bytes_t _M0L5bytesS3049 = _M0L8_2afieldS3861;
                      int32_t _M0L5startS3052 = _M0L11_2aparam__1S1034.$1;
                      int32_t _M0L6_2atmpS3050 = _M0L5startS3052 + 1;
                      int32_t _M0L8_2afieldS3860 = _M0L11_2aparam__1S1034.$2;
                      int32_t _M0L3endS3051 = _M0L8_2afieldS3860;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS1208 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3050,
                                                           _M0L3endS3051,
                                                           _M0L5bytesS3049};
                      _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                      _M0L4restS1037 = _M0L4_2axS1208;
                      goto join_1035;
                    }
                  } else {
                    moonbit_bytes_t _M0L8_2afieldS3865 =
                      _M0L11_2aparam__1S1034.$0;
                    moonbit_bytes_t _M0L5bytesS3045 = _M0L8_2afieldS3865;
                    int32_t _M0L5startS3048 = _M0L11_2aparam__1S1034.$1;
                    int32_t _M0L6_2atmpS3046 = _M0L5startS3048 + 1;
                    int32_t _M0L8_2afieldS3864 = _M0L11_2aparam__1S1034.$2;
                    int32_t _M0L3endS3047 = _M0L8_2afieldS3864;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS1209 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3046,
                                                         _M0L3endS3047,
                                                         _M0L5bytesS3045};
                    _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                    _M0L4restS1037 = _M0L4_2axS1209;
                    goto join_1035;
                  }
                }
              } else if (_M0L4_2axS1137 == 224) {
                moonbit_bytes_t _M0L8_2afieldS3871 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS3309 = _M0L8_2afieldS3871;
                int32_t _M0L5startS3311 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS3310 = _M0L5startS3311 + 1;
                int32_t _M0L6_2atmpS3870 = _M0L5bytesS3309[_M0L6_2atmpS3310];
                int32_t _M0L4_2axS1210 = _M0L6_2atmpS3870;
                if (_M0L4_2axS1210 >= 160 && _M0L4_2axS1210 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS3867 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3305 = _M0L8_2afieldS3867;
                  int32_t _M0L5startS3308 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3306 = _M0L5startS3308 + 2;
                  int32_t _M0L8_2afieldS3866 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3307 = _M0L8_2afieldS3866;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1211 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3306,
                                                       _M0L3endS3307,
                                                       _M0L5bytesS3305};
                  _M0L4restS1046 = _M0L4_2axS1211;
                  _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                  goto join_1045;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3869 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3301 = _M0L8_2afieldS3869;
                  int32_t _M0L5startS3304 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3302 = _M0L5startS3304 + 1;
                  int32_t _M0L8_2afieldS3868 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3303 = _M0L8_2afieldS3868;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1212 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3302,
                                                       _M0L3endS3303,
                                                       _M0L5bytesS3301};
                  _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                  _M0L4restS1037 = _M0L4_2axS1212;
                  goto join_1035;
                }
              } else if (_M0L4_2axS1137 >= 225 && _M0L4_2axS1137 <= 236) {
                moonbit_bytes_t _M0L8_2afieldS3877 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS3298 = _M0L8_2afieldS3877;
                int32_t _M0L5startS3300 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS3299 = _M0L5startS3300 + 1;
                int32_t _M0L6_2atmpS3876 = _M0L5bytesS3298[_M0L6_2atmpS3299];
                int32_t _M0L4_2axS1213 = _M0L6_2atmpS3876;
                if (_M0L4_2axS1213 >= 128 && _M0L4_2axS1213 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS3873 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3294 = _M0L8_2afieldS3873;
                  int32_t _M0L5startS3297 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3295 = _M0L5startS3297 + 2;
                  int32_t _M0L8_2afieldS3872 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3296 = _M0L8_2afieldS3872;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1214 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3295,
                                                       _M0L3endS3296,
                                                       _M0L5bytesS3294};
                  _M0L4restS1046 = _M0L4_2axS1214;
                  _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                  goto join_1045;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3875 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3290 = _M0L8_2afieldS3875;
                  int32_t _M0L5startS3293 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3291 = _M0L5startS3293 + 1;
                  int32_t _M0L8_2afieldS3874 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3292 = _M0L8_2afieldS3874;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1215 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3291,
                                                       _M0L3endS3292,
                                                       _M0L5bytesS3290};
                  _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                  _M0L4restS1037 = _M0L4_2axS1215;
                  goto join_1035;
                }
              } else if (_M0L4_2axS1137 == 237) {
                moonbit_bytes_t _M0L8_2afieldS3883 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS3287 = _M0L8_2afieldS3883;
                int32_t _M0L5startS3289 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS3288 = _M0L5startS3289 + 1;
                int32_t _M0L6_2atmpS3882 = _M0L5bytesS3287[_M0L6_2atmpS3288];
                int32_t _M0L4_2axS1216 = _M0L6_2atmpS3882;
                if (_M0L4_2axS1216 >= 128 && _M0L4_2axS1216 <= 159) {
                  moonbit_bytes_t _M0L8_2afieldS3879 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3283 = _M0L8_2afieldS3879;
                  int32_t _M0L5startS3286 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3284 = _M0L5startS3286 + 2;
                  int32_t _M0L8_2afieldS3878 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3285 = _M0L8_2afieldS3878;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1217 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3284,
                                                       _M0L3endS3285,
                                                       _M0L5bytesS3283};
                  _M0L4restS1046 = _M0L4_2axS1217;
                  _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                  goto join_1045;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3881 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3279 = _M0L8_2afieldS3881;
                  int32_t _M0L5startS3282 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3280 = _M0L5startS3282 + 1;
                  int32_t _M0L8_2afieldS3880 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3281 = _M0L8_2afieldS3880;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1218 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3280,
                                                       _M0L3endS3281,
                                                       _M0L5bytesS3279};
                  _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                  _M0L4restS1037 = _M0L4_2axS1218;
                  goto join_1035;
                }
              } else if (_M0L4_2axS1137 >= 238 && _M0L4_2axS1137 <= 239) {
                moonbit_bytes_t _M0L8_2afieldS3889 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS3276 = _M0L8_2afieldS3889;
                int32_t _M0L5startS3278 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS3277 = _M0L5startS3278 + 1;
                int32_t _M0L6_2atmpS3888 = _M0L5bytesS3276[_M0L6_2atmpS3277];
                int32_t _M0L4_2axS1219 = _M0L6_2atmpS3888;
                if (_M0L4_2axS1219 >= 128 && _M0L4_2axS1219 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS3885 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3272 = _M0L8_2afieldS3885;
                  int32_t _M0L5startS3275 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3273 = _M0L5startS3275 + 2;
                  int32_t _M0L8_2afieldS3884 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3274 = _M0L8_2afieldS3884;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1220 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3273,
                                                       _M0L3endS3274,
                                                       _M0L5bytesS3272};
                  _M0L4restS1046 = _M0L4_2axS1220;
                  _M0L4tlenS1047 = _M0L11_2aparam__0S1033;
                  goto join_1045;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3887 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3268 = _M0L8_2afieldS3887;
                  int32_t _M0L5startS3271 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3269 = _M0L5startS3271 + 1;
                  int32_t _M0L8_2afieldS3886 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3270 = _M0L8_2afieldS3886;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1221 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3269,
                                                       _M0L3endS3270,
                                                       _M0L5bytesS3268};
                  _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                  _M0L4restS1037 = _M0L4_2axS1221;
                  goto join_1035;
                }
              } else if (_M0L4_2axS1137 == 240) {
                moonbit_bytes_t _M0L8_2afieldS3895 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS3265 = _M0L8_2afieldS3895;
                int32_t _M0L5startS3267 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS3266 = _M0L5startS3267 + 1;
                int32_t _M0L6_2atmpS3894 = _M0L5bytesS3265[_M0L6_2atmpS3266];
                int32_t _M0L4_2axS1222 = _M0L6_2atmpS3894;
                if (_M0L4_2axS1222 >= 144 && _M0L4_2axS1222 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS3891 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3261 = _M0L8_2afieldS3891;
                  int32_t _M0L5startS3264 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3262 = _M0L5startS3264 + 2;
                  int32_t _M0L8_2afieldS3890 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3263 = _M0L8_2afieldS3890;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1223 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3262,
                                                       _M0L3endS3263,
                                                       _M0L5bytesS3261};
                  _M0L4restS1040 = _M0L4_2axS1223;
                  _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
                  goto join_1039;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3893 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3257 = _M0L8_2afieldS3893;
                  int32_t _M0L5startS3260 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3258 = _M0L5startS3260 + 1;
                  int32_t _M0L8_2afieldS3892 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3259 = _M0L8_2afieldS3892;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1224 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3258,
                                                       _M0L3endS3259,
                                                       _M0L5bytesS3257};
                  _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                  _M0L4restS1037 = _M0L4_2axS1224;
                  goto join_1035;
                }
              } else if (_M0L4_2axS1137 >= 241 && _M0L4_2axS1137 <= 243) {
                moonbit_bytes_t _M0L8_2afieldS3901 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS3254 = _M0L8_2afieldS3901;
                int32_t _M0L5startS3256 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS3255 = _M0L5startS3256 + 1;
                int32_t _M0L6_2atmpS3900 = _M0L5bytesS3254[_M0L6_2atmpS3255];
                int32_t _M0L4_2axS1225 = _M0L6_2atmpS3900;
                if (_M0L4_2axS1225 >= 128 && _M0L4_2axS1225 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS3897 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3250 = _M0L8_2afieldS3897;
                  int32_t _M0L5startS3253 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3251 = _M0L5startS3253 + 2;
                  int32_t _M0L8_2afieldS3896 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3252 = _M0L8_2afieldS3896;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1226 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3251,
                                                       _M0L3endS3252,
                                                       _M0L5bytesS3250};
                  _M0L4restS1040 = _M0L4_2axS1226;
                  _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
                  goto join_1039;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3899 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3246 = _M0L8_2afieldS3899;
                  int32_t _M0L5startS3249 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3247 = _M0L5startS3249 + 1;
                  int32_t _M0L8_2afieldS3898 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3248 = _M0L8_2afieldS3898;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1227 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3247,
                                                       _M0L3endS3248,
                                                       _M0L5bytesS3246};
                  _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                  _M0L4restS1037 = _M0L4_2axS1227;
                  goto join_1035;
                }
              } else if (_M0L4_2axS1137 == 244) {
                moonbit_bytes_t _M0L8_2afieldS3907 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS3243 = _M0L8_2afieldS3907;
                int32_t _M0L5startS3245 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS3244 = _M0L5startS3245 + 1;
                int32_t _M0L6_2atmpS3906 = _M0L5bytesS3243[_M0L6_2atmpS3244];
                int32_t _M0L4_2axS1228 = _M0L6_2atmpS3906;
                if (_M0L4_2axS1228 >= 128 && _M0L4_2axS1228 <= 143) {
                  moonbit_bytes_t _M0L8_2afieldS3903 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3239 = _M0L8_2afieldS3903;
                  int32_t _M0L5startS3242 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3240 = _M0L5startS3242 + 2;
                  int32_t _M0L8_2afieldS3902 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3241 = _M0L8_2afieldS3902;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1229 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3240,
                                                       _M0L3endS3241,
                                                       _M0L5bytesS3239};
                  _M0L4restS1040 = _M0L4_2axS1229;
                  _M0L4tlenS1041 = _M0L11_2aparam__0S1033;
                  goto join_1039;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3905 =
                    _M0L11_2aparam__1S1034.$0;
                  moonbit_bytes_t _M0L5bytesS3235 = _M0L8_2afieldS3905;
                  int32_t _M0L5startS3238 = _M0L11_2aparam__1S1034.$1;
                  int32_t _M0L6_2atmpS3236 = _M0L5startS3238 + 1;
                  int32_t _M0L8_2afieldS3904 = _M0L11_2aparam__1S1034.$2;
                  int32_t _M0L3endS3237 = _M0L8_2afieldS3904;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS1230 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3236,
                                                       _M0L3endS3237,
                                                       _M0L5bytesS3235};
                  _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                  _M0L4restS1037 = _M0L4_2axS1230;
                  goto join_1035;
                }
              } else {
                moonbit_bytes_t _M0L8_2afieldS3909 =
                  _M0L11_2aparam__1S1034.$0;
                moonbit_bytes_t _M0L5bytesS3231 = _M0L8_2afieldS3909;
                int32_t _M0L5startS3234 = _M0L11_2aparam__1S1034.$1;
                int32_t _M0L6_2atmpS3232 = _M0L5startS3234 + 1;
                int32_t _M0L8_2afieldS3908 = _M0L11_2aparam__1S1034.$2;
                int32_t _M0L3endS3233 = _M0L8_2afieldS3908;
                struct _M0TPC15bytes9BytesView _M0L4_2axS1231 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3232,
                                                     _M0L3endS3233,
                                                     _M0L5bytesS3231};
                _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
                _M0L4restS1037 = _M0L4_2axS1231;
                goto join_1035;
              }
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3911 = _M0L11_2aparam__1S1034.$0;
            moonbit_bytes_t _M0L5bytesS3337 = _M0L8_2afieldS3911;
            int32_t _M0L5startS3340 = _M0L11_2aparam__1S1034.$1;
            int32_t _M0L6_2atmpS3338 = _M0L5startS3340 + 1;
            int32_t _M0L8_2afieldS3910 = _M0L11_2aparam__1S1034.$2;
            int32_t _M0L3endS3339 = _M0L8_2afieldS3910;
            struct _M0TPC15bytes9BytesView _M0L4_2axS1232 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3338,
                                                 _M0L3endS3339,
                                                 _M0L5bytesS3337};
            _M0L4tlenS1036 = _M0L11_2aparam__0S1033;
            _M0L4restS1037 = _M0L4_2axS1232;
            goto join_1035;
          }
        }
      }
    }
    goto joinlet_4270;
    join_1072:;
    _M0L1tS1031[_M0L4tlenS1073] = _M0L1bS1075;
    _M0L6_2atmpS2725 = _M0L4tlenS1073 + 2;
    _M0L11_2aparam__0S1033 = _M0L6_2atmpS2725;
    _M0L11_2aparam__1S1034 = _M0L4restS1074;
    continue;
    joinlet_4270:;
    goto joinlet_4269;
    join_1066:;
    _M0L6_2atmpS2724 = (int32_t)_M0L2b0S1069;
    _M0L6_2atmpS2723 = _M0L6_2atmpS2724 & 31;
    _M0L6_2atmpS2720 = _M0L6_2atmpS2723 << 6;
    _M0L6_2atmpS2722 = (int32_t)_M0L2b1S1070;
    _M0L6_2atmpS2721 = _M0L6_2atmpS2722 & 63;
    _M0L2chS1071 = _M0L6_2atmpS2720 | _M0L6_2atmpS2721;
    _M0L6_2atmpS2715 = _M0L2chS1071 & 0xff;
    _M0L1tS1031[_M0L4tlenS1067] = _M0L6_2atmpS2715;
    _M0L6_2atmpS2716 = _M0L4tlenS1067 + 1;
    _M0L6_2atmpS2718 = _M0L2chS1071 >> 8;
    _M0L6_2atmpS2717 = _M0L6_2atmpS2718 & 0xff;
    _M0L1tS1031[_M0L6_2atmpS2716] = _M0L6_2atmpS2717;
    _M0L6_2atmpS2719 = _M0L4tlenS1067 + 2;
    _M0L11_2aparam__0S1033 = _M0L6_2atmpS2719;
    _M0L11_2aparam__1S1034 = _M0L4restS1068;
    continue;
    joinlet_4269:;
    goto joinlet_4268;
    join_1059:;
    _M0L6_2atmpS2714 = (int32_t)_M0L2b0S1061;
    _M0L6_2atmpS2713 = _M0L6_2atmpS2714 & 15;
    _M0L6_2atmpS2709 = _M0L6_2atmpS2713 << 12;
    _M0L6_2atmpS2712 = (int32_t)_M0L2b1S1062;
    _M0L6_2atmpS2711 = _M0L6_2atmpS2712 & 63;
    _M0L6_2atmpS2710 = _M0L6_2atmpS2711 << 6;
    _M0L6_2atmpS2706 = _M0L6_2atmpS2709 | _M0L6_2atmpS2710;
    _M0L6_2atmpS2708 = (int32_t)_M0L2b2S1063;
    _M0L6_2atmpS2707 = _M0L6_2atmpS2708 & 63;
    _M0L2chS1065 = _M0L6_2atmpS2706 | _M0L6_2atmpS2707;
    _M0L6_2atmpS2701 = _M0L2chS1065 & 0xff;
    _M0L1tS1031[_M0L4tlenS1060] = _M0L6_2atmpS2701;
    _M0L6_2atmpS2702 = _M0L4tlenS1060 + 1;
    _M0L6_2atmpS2704 = _M0L2chS1065 >> 8;
    _M0L6_2atmpS2703 = _M0L6_2atmpS2704 & 0xff;
    _M0L1tS1031[_M0L6_2atmpS2702] = _M0L6_2atmpS2703;
    _M0L6_2atmpS2705 = _M0L4tlenS1060 + 2;
    _M0L11_2aparam__0S1033 = _M0L6_2atmpS2705;
    _M0L11_2aparam__1S1034 = _M0L4restS1064;
    continue;
    joinlet_4268:;
    goto joinlet_4267;
    join_1048:;
    _M0L6_2atmpS2700 = (int32_t)_M0L2b0S1050;
    _M0L6_2atmpS2699 = _M0L6_2atmpS2700 & 7;
    _M0L6_2atmpS2695 = _M0L6_2atmpS2699 << 18;
    _M0L6_2atmpS2698 = (int32_t)_M0L2b1S1051;
    _M0L6_2atmpS2697 = _M0L6_2atmpS2698 & 63;
    _M0L6_2atmpS2696 = _M0L6_2atmpS2697 << 12;
    _M0L6_2atmpS2691 = _M0L6_2atmpS2695 | _M0L6_2atmpS2696;
    _M0L6_2atmpS2694 = (int32_t)_M0L2b2S1052;
    _M0L6_2atmpS2693 = _M0L6_2atmpS2694 & 63;
    _M0L6_2atmpS2692 = _M0L6_2atmpS2693 << 6;
    _M0L6_2atmpS2688 = _M0L6_2atmpS2691 | _M0L6_2atmpS2692;
    _M0L6_2atmpS2690 = (int32_t)_M0L2b3S1053;
    _M0L6_2atmpS2689 = _M0L6_2atmpS2690 & 63;
    _M0L2chS1055 = _M0L6_2atmpS2688 | _M0L6_2atmpS2689;
    _M0L3chmS1056 = _M0L2chS1055 - 65536;
    _M0L6_2atmpS2687 = _M0L3chmS1056 >> 10;
    _M0L3ch1S1057 = _M0L6_2atmpS2687 + 55296;
    _M0L6_2atmpS2686 = _M0L3chmS1056 & 1023;
    _M0L3ch2S1058 = _M0L6_2atmpS2686 + 56320;
    _M0L6_2atmpS2676 = _M0L3ch1S1057 & 0xff;
    _M0L1tS1031[_M0L4tlenS1049] = _M0L6_2atmpS2676;
    _M0L6_2atmpS2677 = _M0L4tlenS1049 + 1;
    _M0L6_2atmpS2679 = _M0L3ch1S1057 >> 8;
    _M0L6_2atmpS2678 = _M0L6_2atmpS2679 & 0xff;
    _M0L1tS1031[_M0L6_2atmpS2677] = _M0L6_2atmpS2678;
    _M0L6_2atmpS2680 = _M0L4tlenS1049 + 2;
    _M0L6_2atmpS2681 = _M0L3ch2S1058 & 0xff;
    _M0L1tS1031[_M0L6_2atmpS2680] = _M0L6_2atmpS2681;
    _M0L6_2atmpS2682 = _M0L4tlenS1049 + 3;
    _M0L6_2atmpS2684 = _M0L3ch2S1058 >> 8;
    _M0L6_2atmpS2683 = _M0L6_2atmpS2684 & 0xff;
    _M0L1tS1031[_M0L6_2atmpS2682] = _M0L6_2atmpS2683;
    _M0L6_2atmpS2685 = _M0L4tlenS1049 + 4;
    _M0L11_2aparam__0S1033 = _M0L6_2atmpS2685;
    _M0L11_2aparam__1S1034 = _M0L4restS1054;
    continue;
    joinlet_4267:;
    goto joinlet_4266;
    join_1045:;
    _M0L1tS1031[_M0L4tlenS1047] = 253;
    _M0L6_2atmpS2674 = _M0L4tlenS1047 + 1;
    _M0L1tS1031[_M0L6_2atmpS2674] = 255;
    _M0L6_2atmpS2675 = _M0L4tlenS1047 + 2;
    _M0L11_2aparam__0S1033 = _M0L6_2atmpS2675;
    _M0L11_2aparam__1S1034 = _M0L4restS1046;
    continue;
    joinlet_4266:;
    goto joinlet_4265;
    join_1042:;
    _M0L1tS1031[_M0L4tlenS1044] = 253;
    _M0L6_2atmpS2672 = _M0L4tlenS1044 + 1;
    _M0L1tS1031[_M0L6_2atmpS2672] = 255;
    _M0L6_2atmpS2673 = _M0L4tlenS1044 + 2;
    _M0L11_2aparam__0S1033 = _M0L6_2atmpS2673;
    _M0L11_2aparam__1S1034 = _M0L4restS1043;
    continue;
    joinlet_4265:;
    goto joinlet_4264;
    join_1039:;
    _M0L1tS1031[_M0L4tlenS1041] = 253;
    _M0L6_2atmpS2670 = _M0L4tlenS1041 + 1;
    _M0L1tS1031[_M0L6_2atmpS2670] = 255;
    _M0L6_2atmpS2671 = _M0L4tlenS1041 + 2;
    _M0L11_2aparam__0S1033 = _M0L6_2atmpS2671;
    _M0L11_2aparam__1S1034 = _M0L4restS1040;
    continue;
    joinlet_4264:;
    goto joinlet_4263;
    join_1035:;
    _M0L1tS1031[_M0L4tlenS1036] = 253;
    _M0L6_2atmpS2668 = _M0L4tlenS1036 + 1;
    _M0L1tS1031[_M0L6_2atmpS2668] = 255;
    _M0L6_2atmpS2669 = _M0L4tlenS1036 + 2;
    _M0L11_2aparam__0S1033 = _M0L6_2atmpS2669;
    _M0L11_2aparam__1S1034 = _M0L4restS1037;
    continue;
    joinlet_4263:;
    break;
  }
  _M0L6_2atmpS2666 = _M0L1tS1031;
  _M0L6_2atmpS2667 = (int64_t)_M0L4tlenS1032;
  #line 259 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2666, 0, _M0L6_2atmpS2667);
}

struct moonbit_result_0 _M0FPC14json13json__inspect(
  struct _M0TPB6ToJson _M0L3objS1018,
  void* _M0L7contentS1020,
  moonbit_string_t _M0L3locS1014,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS1016
) {
  moonbit_string_t _M0L3locS1013;
  moonbit_string_t _M0L9args__locS1015;
  void* _M0L6_2atmpS2664;
  struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2665;
  moonbit_string_t _M0L6actualS1017;
  moonbit_string_t _M0L4wantS1019;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 491 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3locS1013 = _M0MPB9SourceLoc16to__json__string(_M0L3locS1014);
  #line 492 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L9args__locS1015 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS1016);
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2664 = _M0L3objS1018.$0->$method_0(_M0L3objS1018.$1);
  _M0L6_2atmpS2665 = 0;
  #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6actualS1017
  = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2664, 0, 0, _M0L6_2atmpS2665);
  if (_M0L7contentS1020 == 0) {
    void* _M0L6_2atmpS2661;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2662;
    if (_M0L7contentS1020) {
      moonbit_decref(_M0L7contentS1020);
    }
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2661
    = _M0IPC16string6StringPB6ToJson8to__json((moonbit_string_t)moonbit_string_literal_0.data);
    _M0L6_2atmpS2662 = 0;
    #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1019
    = _M0MPC14json4Json17stringify_2einner(_M0L6_2atmpS2661, 0, 0, _M0L6_2atmpS2662);
  } else {
    void* _M0L7_2aSomeS1021 = _M0L7contentS1020;
    void* _M0L4_2axS1022 = _M0L7_2aSomeS1021;
    struct _M0TWsRPB4JsonEORPB4Json* _M0L6_2atmpS2663 = 0;
    #line 496 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L4wantS1019
    = _M0MPC14json4Json17stringify_2einner(_M0L4_2axS1022, 0, 0, _M0L6_2atmpS2663);
  }
  moonbit_incref(_M0L4wantS1019);
  moonbit_incref(_M0L6actualS1017);
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS1017, _M0L4wantS1019)
  ) {
    moonbit_string_t _M0L6_2atmpS2659;
    moonbit_string_t _M0L6_2atmpS3929;
    moonbit_string_t _M0L6_2atmpS2658;
    moonbit_string_t _M0L6_2atmpS3928;
    moonbit_string_t _M0L6_2atmpS2656;
    moonbit_string_t _M0L6_2atmpS2657;
    moonbit_string_t _M0L6_2atmpS3927;
    moonbit_string_t _M0L6_2atmpS2655;
    moonbit_string_t _M0L6_2atmpS3926;
    moonbit_string_t _M0L6_2atmpS2652;
    moonbit_string_t _M0L6_2atmpS2654;
    moonbit_string_t _M0L6_2atmpS2653;
    moonbit_string_t _M0L6_2atmpS3925;
    moonbit_string_t _M0L6_2atmpS2651;
    moonbit_string_t _M0L6_2atmpS3924;
    moonbit_string_t _M0L6_2atmpS2648;
    moonbit_string_t _M0L6_2atmpS2650;
    moonbit_string_t _M0L6_2atmpS2649;
    moonbit_string_t _M0L6_2atmpS3923;
    moonbit_string_t _M0L6_2atmpS2647;
    moonbit_string_t _M0L6_2atmpS3922;
    moonbit_string_t _M0L6_2atmpS2646;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2645;
    struct moonbit_result_0 _result_4271;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2659
    = _M0IPC16string6StringPB4Show10to__string(_M0L3locS1013);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3929
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_13.data, _M0L6_2atmpS2659);
    moonbit_decref(_M0L6_2atmpS2659);
    _M0L6_2atmpS2658 = _M0L6_2atmpS3929;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3928
    = moonbit_add_string(_M0L6_2atmpS2658, (moonbit_string_t)moonbit_string_literal_14.data);
    moonbit_decref(_M0L6_2atmpS2658);
    _M0L6_2atmpS2656 = _M0L6_2atmpS3928;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2657
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS1015);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3927 = moonbit_add_string(_M0L6_2atmpS2656, _M0L6_2atmpS2657);
    moonbit_decref(_M0L6_2atmpS2656);
    moonbit_decref(_M0L6_2atmpS2657);
    _M0L6_2atmpS2655 = _M0L6_2atmpS3927;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3926
    = moonbit_add_string(_M0L6_2atmpS2655, (moonbit_string_t)moonbit_string_literal_15.data);
    moonbit_decref(_M0L6_2atmpS2655);
    _M0L6_2atmpS2652 = _M0L6_2atmpS3926;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2654 = _M0MPC16string6String6escape(_M0L4wantS1019);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2653
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2654);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3925 = moonbit_add_string(_M0L6_2atmpS2652, _M0L6_2atmpS2653);
    moonbit_decref(_M0L6_2atmpS2652);
    moonbit_decref(_M0L6_2atmpS2653);
    _M0L6_2atmpS2651 = _M0L6_2atmpS3925;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3924
    = moonbit_add_string(_M0L6_2atmpS2651, (moonbit_string_t)moonbit_string_literal_16.data);
    moonbit_decref(_M0L6_2atmpS2651);
    _M0L6_2atmpS2648 = _M0L6_2atmpS3924;
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2650 = _M0MPC16string6String6escape(_M0L6actualS1017);
    #line 500 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS2649
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS2650);
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3923 = moonbit_add_string(_M0L6_2atmpS2648, _M0L6_2atmpS2649);
    moonbit_decref(_M0L6_2atmpS2648);
    moonbit_decref(_M0L6_2atmpS2649);
    _M0L6_2atmpS2647 = _M0L6_2atmpS3923;
    #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L6_2atmpS3922
    = moonbit_add_string(_M0L6_2atmpS2647, (moonbit_string_t)moonbit_string_literal_17.data);
    moonbit_decref(_M0L6_2atmpS2647);
    _M0L6_2atmpS2646 = _M0L6_2atmpS3922;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2645
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2645)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2645)->$0
    = _M0L6_2atmpS2646;
    _result_4271.tag = 0;
    _result_4271.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS2645;
    return _result_4271;
  } else {
    int32_t _M0L6_2atmpS2660;
    struct moonbit_result_0 _result_4272;
    moonbit_decref(_M0L4wantS1019);
    moonbit_decref(_M0L6actualS1017);
    moonbit_decref(_M0L9args__locS1015);
    moonbit_decref(_M0L3locS1013);
    _M0L6_2atmpS2660 = 0;
    _result_4272.tag = 1;
    _result_4272.data.ok = _M0L6_2atmpS2660;
    return _result_4272;
  }
}

moonbit_string_t _M0MPC14json4Json17stringify_2einner(
  void* _M0L4selfS1012,
  int32_t _M0L13escape__slashS984,
  int32_t _M0L6indentS979,
  struct _M0TWsRPB4JsonEORPB4Json* _M0L8replacerS1005
) {
  struct _M0TPB13StringBuilder* _M0L3bufS971;
  void** _M0L6_2atmpS2644;
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L5stackS972;
  int32_t _M0Lm5depthS973;
  void* _M0L6_2atmpS2643;
  void* _M0L8_2aparamS974;
  #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  #line 279 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS971 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L6_2atmpS2644 = (void**)moonbit_empty_ref_array;
  _M0L5stackS972
  = (struct _M0TPB5ArrayGRPC14json10WriteFrameE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRPC14json10WriteFrameE));
  Moonbit_object_header(_M0L5stackS972)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRPC14json10WriteFrameE, $0) >> 2, 1, 0);
  _M0L5stackS972->$0 = _M0L6_2atmpS2644;
  _M0L5stackS972->$1 = 0;
  _M0Lm5depthS973 = 0;
  _M0L6_2atmpS2643 = _M0L4selfS1012;
  _M0L8_2aparamS974 = _M0L6_2atmpS2643;
  _2aloop_990:;
  while (1) {
    if (_M0L8_2aparamS974 == 0) {
      int32_t _M0L3lenS2605;
      if (_M0L8_2aparamS974) {
        moonbit_decref(_M0L8_2aparamS974);
      }
      _M0L3lenS2605 = _M0L5stackS972->$1;
      if (_M0L3lenS2605 == 0) {
        if (_M0L8replacerS1005) {
          moonbit_decref(_M0L8replacerS1005);
        }
        moonbit_decref(_M0L5stackS972);
        break;
      } else {
        void** _M0L8_2afieldS3937 = _M0L5stackS972->$0;
        void** _M0L3bufS2629 = _M0L8_2afieldS3937;
        int32_t _M0L3lenS2631 = _M0L5stackS972->$1;
        int32_t _M0L6_2atmpS2630 = _M0L3lenS2631 - 1;
        void* _M0L6_2atmpS3936 = (void*)_M0L3bufS2629[_M0L6_2atmpS2630];
        void* _M0L4_2axS991 = _M0L6_2atmpS3936;
        switch (Moonbit_object_tag(_M0L4_2axS991)) {
          case 0: {
            struct _M0DTPC14json10WriteFrame5Array* _M0L8_2aArrayS992 =
              (struct _M0DTPC14json10WriteFrame5Array*)_M0L4_2axS991;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3932 =
              _M0L8_2aArrayS992->$0;
            struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS993 =
              _M0L8_2afieldS3932;
            int32_t _M0L4_2aiS994 = _M0L8_2aArrayS992->$1;
            int32_t _M0L3lenS2617 = _M0L6_2aarrS993->$1;
            if (_M0L4_2aiS994 < _M0L3lenS2617) {
              int32_t _if__result_4274;
              void** _M0L8_2afieldS3931;
              void** _M0L3bufS2623;
              void* _M0L6_2atmpS3930;
              void* _M0L7elementS995;
              int32_t _M0L6_2atmpS2618;
              void* _M0L6_2atmpS2621;
              if (_M0L4_2aiS994 < 0) {
                _if__result_4274 = 1;
              } else {
                int32_t _M0L3lenS2622 = _M0L6_2aarrS993->$1;
                _if__result_4274 = _M0L4_2aiS994 >= _M0L3lenS2622;
              }
              if (_if__result_4274) {
                #line 328 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_panic();
              }
              _M0L8_2afieldS3931 = _M0L6_2aarrS993->$0;
              _M0L3bufS2623 = _M0L8_2afieldS3931;
              _M0L6_2atmpS3930 = (void*)_M0L3bufS2623[_M0L4_2aiS994];
              _M0L7elementS995 = _M0L6_2atmpS3930;
              _M0L6_2atmpS2618 = _M0L4_2aiS994 + 1;
              _M0L8_2aArrayS992->$1 = _M0L6_2atmpS2618;
              if (_M0L4_2aiS994 > 0) {
                int32_t _M0L6_2atmpS2620;
                moonbit_string_t _M0L6_2atmpS2619;
                moonbit_incref(_M0L7elementS995);
                moonbit_incref(_M0L3bufS971);
                #line 331 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 44);
                _M0L6_2atmpS2620 = _M0Lm5depthS973;
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2619
                = _M0FPC14json11indent__str(_M0L6_2atmpS2620, _M0L6indentS979);
                moonbit_incref(_M0L3bufS971);
                #line 332 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, _M0L6_2atmpS2619);
              } else {
                moonbit_incref(_M0L7elementS995);
              }
              _M0L6_2atmpS2621 = _M0L7elementS995;
              _M0L8_2aparamS974 = _M0L6_2atmpS2621;
              goto _2aloop_990;
            } else {
              int32_t _M0L6_2atmpS2624 = _M0Lm5depthS973;
              void* _M0L6_2atmpS2625;
              int32_t _M0L6_2atmpS2627;
              moonbit_string_t _M0L6_2atmpS2626;
              void* _M0L6_2atmpS2628;
              _M0Lm5depthS973 = _M0L6_2atmpS2624 - 1;
              moonbit_incref(_M0L5stackS972);
              #line 337 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2625
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS972);
              if (_M0L6_2atmpS2625) {
                moonbit_decref(_M0L6_2atmpS2625);
              }
              _M0L6_2atmpS2627 = _M0Lm5depthS973;
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2626
              = _M0FPC14json11indent__str(_M0L6_2atmpS2627, _M0L6indentS979);
              moonbit_incref(_M0L3bufS971);
              #line 338 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, _M0L6_2atmpS2626);
              moonbit_incref(_M0L3bufS971);
              #line 339 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 93);
              _M0L6_2atmpS2628 = 0;
              _M0L8_2aparamS974 = _M0L6_2atmpS2628;
              goto _2aloop_990;
            }
            break;
          }
          default: {
            struct _M0DTPC14json10WriteFrame6Object* _M0L9_2aObjectS996 =
              (struct _M0DTPC14json10WriteFrame6Object*)_M0L4_2axS991;
            struct _M0TWEOUsRPB4JsonE* _M0L8_2afieldS3935 =
              _M0L9_2aObjectS996->$0;
            struct _M0TWEOUsRPB4JsonE* _M0L11_2aiteratorS997 =
              _M0L8_2afieldS3935;
            int32_t _M0L8_2afirstS998 = _M0L9_2aObjectS996->$1;
            struct _M0TUsRPB4JsonE* _M0L7_2abindS999;
            moonbit_incref(_M0L11_2aiteratorS997);
            moonbit_incref(_M0L9_2aObjectS996);
            #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L7_2abindS999
            = _M0MPB4Iter4nextGUsRPB4JsonEE(_M0L11_2aiteratorS997);
            if (_M0L7_2abindS999 == 0) {
              int32_t _M0L6_2atmpS2606;
              void* _M0L6_2atmpS2607;
              int32_t _M0L6_2atmpS2609;
              moonbit_string_t _M0L6_2atmpS2608;
              void* _M0L6_2atmpS2610;
              if (_M0L7_2abindS999) {
                moonbit_decref(_M0L7_2abindS999);
              }
              moonbit_decref(_M0L9_2aObjectS996);
              _M0L6_2atmpS2606 = _M0Lm5depthS973;
              _M0Lm5depthS973 = _M0L6_2atmpS2606 - 1;
              moonbit_incref(_M0L5stackS972);
              #line 369 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2607
              = _M0MPC15array5Array3popGRPC14json10WriteFrameE(_M0L5stackS972);
              if (_M0L6_2atmpS2607) {
                moonbit_decref(_M0L6_2atmpS2607);
              }
              _M0L6_2atmpS2609 = _M0Lm5depthS973;
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2608
              = _M0FPC14json11indent__str(_M0L6_2atmpS2609, _M0L6indentS979);
              moonbit_incref(_M0L3bufS971);
              #line 370 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, _M0L6_2atmpS2608);
              moonbit_incref(_M0L3bufS971);
              #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 125);
              _M0L6_2atmpS2610 = 0;
              _M0L8_2aparamS974 = _M0L6_2atmpS2610;
              goto _2aloop_990;
            } else {
              struct _M0TUsRPB4JsonE* _M0L7_2aSomeS1000 = _M0L7_2abindS999;
              struct _M0TUsRPB4JsonE* _M0L4_2axS1001 = _M0L7_2aSomeS1000;
              moonbit_string_t _M0L8_2afieldS3934 = _M0L4_2axS1001->$0;
              moonbit_string_t _M0L4_2akS1002 = _M0L8_2afieldS3934;
              void* _M0L8_2afieldS3933 = _M0L4_2axS1001->$1;
              int32_t _M0L6_2acntS4151 =
                Moonbit_object_header(_M0L4_2axS1001)->rc;
              void* _M0L4_2avS1003;
              void* _M0Lm2v2S1004;
              moonbit_string_t _M0L6_2atmpS2614;
              void* _M0L6_2atmpS2616;
              void* _M0L6_2atmpS2615;
              if (_M0L6_2acntS4151 > 1) {
                int32_t _M0L11_2anew__cntS4152 = _M0L6_2acntS4151 - 1;
                Moonbit_object_header(_M0L4_2axS1001)->rc
                = _M0L11_2anew__cntS4152;
                moonbit_incref(_M0L8_2afieldS3933);
                moonbit_incref(_M0L4_2akS1002);
              } else if (_M0L6_2acntS4151 == 1) {
                #line 343 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                moonbit_free(_M0L4_2axS1001);
              }
              _M0L4_2avS1003 = _M0L8_2afieldS3933;
              _M0Lm2v2S1004 = _M0L4_2avS1003;
              if (_M0L8replacerS1005 == 0) {
                moonbit_incref(_M0Lm2v2S1004);
                moonbit_decref(_M0L4_2avS1003);
              } else {
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2aSomeS1006 =
                  _M0L8replacerS1005;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L11_2areplacerS1007 =
                  _M0L7_2aSomeS1006;
                struct _M0TWsRPB4JsonEORPB4Json* _M0L7_2afuncS1008 =
                  _M0L11_2areplacerS1007;
                void* _M0L7_2abindS1009;
                moonbit_incref(_M0L7_2afuncS1008);
                moonbit_incref(_M0L4_2akS1002);
                #line 347 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L7_2abindS1009
                = _M0L7_2afuncS1008->code(_M0L7_2afuncS1008, _M0L4_2akS1002, _M0L4_2avS1003);
                if (_M0L7_2abindS1009 == 0) {
                  void* _M0L6_2atmpS2611;
                  if (_M0L7_2abindS1009) {
                    moonbit_decref(_M0L7_2abindS1009);
                  }
                  moonbit_decref(_M0L4_2akS1002);
                  moonbit_decref(_M0L9_2aObjectS996);
                  _M0L6_2atmpS2611 = 0;
                  _M0L8_2aparamS974 = _M0L6_2atmpS2611;
                  goto _2aloop_990;
                } else {
                  void* _M0L7_2aSomeS1010 = _M0L7_2abindS1009;
                  void* _M0L4_2avS1011 = _M0L7_2aSomeS1010;
                  _M0Lm2v2S1004 = _M0L4_2avS1011;
                }
              }
              if (!_M0L8_2afirstS998) {
                int32_t _M0L6_2atmpS2613;
                moonbit_string_t _M0L6_2atmpS2612;
                moonbit_incref(_M0L3bufS971);
                #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 44);
                _M0L6_2atmpS2613 = _M0Lm5depthS973;
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0L6_2atmpS2612
                = _M0FPC14json11indent__str(_M0L6_2atmpS2613, _M0L6indentS979);
                moonbit_incref(_M0L3bufS971);
                #line 355 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, _M0L6_2atmpS2612);
              }
              moonbit_incref(_M0L3bufS971);
              #line 357 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 34);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0L6_2atmpS2614
              = _M0FPC14json6escape(_M0L4_2akS1002, _M0L13escape__slashS984);
              moonbit_incref(_M0L3bufS971);
              #line 358 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, _M0L6_2atmpS2614);
              moonbit_incref(_M0L3bufS971);
              #line 359 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 34);
              moonbit_incref(_M0L3bufS971);
              #line 360 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
              _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 58);
              if (_M0L6indentS979 > 0) {
                moonbit_incref(_M0L3bufS971);
                #line 362 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
                _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 32);
              }
              _M0L9_2aObjectS996->$1 = 0;
              moonbit_decref(_M0L9_2aObjectS996);
              _M0L6_2atmpS2616 = _M0Lm2v2S1004;
              _M0L6_2atmpS2615 = _M0L6_2atmpS2616;
              _M0L8_2aparamS974 = _M0L6_2atmpS2615;
              goto _2aloop_990;
            }
            break;
          }
        }
      }
    } else {
      void* _M0L7_2aSomeS975 = _M0L8_2aparamS974;
      void* _M0L8_2avalueS976 = _M0L7_2aSomeS975;
      void* _M0L6_2atmpS2642;
      switch (Moonbit_object_tag(_M0L8_2avalueS976)) {
        case 6: {
          struct _M0DTPB4Json6Object* _M0L9_2aObjectS977 =
            (struct _M0DTPB4Json6Object*)_M0L8_2avalueS976;
          struct _M0TPB3MapGsRPB4JsonE* _M0L8_2afieldS3938 =
            _M0L9_2aObjectS977->$0;
          int32_t _M0L6_2acntS4153 =
            Moonbit_object_header(_M0L9_2aObjectS977)->rc;
          struct _M0TPB3MapGsRPB4JsonE* _M0L10_2amembersS978;
          if (_M0L6_2acntS4153 > 1) {
            int32_t _M0L11_2anew__cntS4154 = _M0L6_2acntS4153 - 1;
            Moonbit_object_header(_M0L9_2aObjectS977)->rc
            = _M0L11_2anew__cntS4154;
            moonbit_incref(_M0L8_2afieldS3938);
          } else if (_M0L6_2acntS4153 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aObjectS977);
          }
          _M0L10_2amembersS978 = _M0L8_2afieldS3938;
          moonbit_incref(_M0L10_2amembersS978);
          #line 288 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPB3Map9is__emptyGsRPB4JsonE(_M0L10_2amembersS978)) {
            moonbit_decref(_M0L10_2amembersS978);
            moonbit_incref(_M0L3bufS971);
            #line 289 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, (moonbit_string_t)moonbit_string_literal_18.data);
          } else {
            int32_t _M0L6_2atmpS2637 = _M0Lm5depthS973;
            int32_t _M0L6_2atmpS2639;
            moonbit_string_t _M0L6_2atmpS2638;
            struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2641;
            void* _M0L6ObjectS2640;
            _M0Lm5depthS973 = _M0L6_2atmpS2637 + 1;
            moonbit_incref(_M0L3bufS971);
            #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 123);
            _M0L6_2atmpS2639 = _M0Lm5depthS973;
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2638
            = _M0FPC14json11indent__str(_M0L6_2atmpS2639, _M0L6indentS979);
            moonbit_incref(_M0L3bufS971);
            #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, _M0L6_2atmpS2638);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2641
            = _M0MPB3Map4iterGsRPB4JsonE(_M0L10_2amembersS978);
            _M0L6ObjectS2640
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame6Object));
            Moonbit_object_header(_M0L6ObjectS2640)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame6Object, $0) >> 2, 1, 1);
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2640)->$0
            = _M0L6_2atmpS2641;
            ((struct _M0DTPC14json10WriteFrame6Object*)_M0L6ObjectS2640)->$1
            = 1;
            moonbit_incref(_M0L5stackS972);
            #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS972, _M0L6ObjectS2640);
          }
          break;
        }
        
        case 5: {
          struct _M0DTPB4Json5Array* _M0L8_2aArrayS980 =
            (struct _M0DTPB4Json5Array*)_M0L8_2avalueS976;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L8_2afieldS3939 =
            _M0L8_2aArrayS980->$0;
          int32_t _M0L6_2acntS4155 =
            Moonbit_object_header(_M0L8_2aArrayS980)->rc;
          struct _M0TPB5ArrayGRPB4JsonE* _M0L6_2aarrS981;
          if (_M0L6_2acntS4155 > 1) {
            int32_t _M0L11_2anew__cntS4156 = _M0L6_2acntS4155 - 1;
            Moonbit_object_header(_M0L8_2aArrayS980)->rc
            = _M0L11_2anew__cntS4156;
            moonbit_incref(_M0L8_2afieldS3939);
          } else if (_M0L6_2acntS4155 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L8_2aArrayS980);
          }
          _M0L6_2aarrS981 = _M0L8_2afieldS3939;
          moonbit_incref(_M0L6_2aarrS981);
          #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          if (_M0MPC15array5Array9is__emptyGRPB4JsonE(_M0L6_2aarrS981)) {
            moonbit_decref(_M0L6_2aarrS981);
            moonbit_incref(_M0L3bufS971);
            #line 299 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, (moonbit_string_t)moonbit_string_literal_19.data);
          } else {
            int32_t _M0L6_2atmpS2633 = _M0Lm5depthS973;
            int32_t _M0L6_2atmpS2635;
            moonbit_string_t _M0L6_2atmpS2634;
            void* _M0L5ArrayS2636;
            _M0Lm5depthS973 = _M0L6_2atmpS2633 + 1;
            moonbit_incref(_M0L3bufS971);
            #line 302 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 91);
            _M0L6_2atmpS2635 = _M0Lm5depthS973;
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0L6_2atmpS2634
            = _M0FPC14json11indent__str(_M0L6_2atmpS2635, _M0L6indentS979);
            moonbit_incref(_M0L3bufS971);
            #line 303 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, _M0L6_2atmpS2634);
            _M0L5ArrayS2636
            = (void*)moonbit_malloc(sizeof(struct _M0DTPC14json10WriteFrame5Array));
            Moonbit_object_header(_M0L5ArrayS2636)->meta
            = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC14json10WriteFrame5Array, $0) >> 2, 1, 0);
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2636)->$0
            = _M0L6_2aarrS981;
            ((struct _M0DTPC14json10WriteFrame5Array*)_M0L5ArrayS2636)->$1
            = 0;
            moonbit_incref(_M0L5stackS972);
            #line 304 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPC15array5Array4pushGRPC14json10WriteFrameE(_M0L5stackS972, _M0L5ArrayS2636);
          }
          break;
        }
        
        case 4: {
          struct _M0DTPB4Json6String* _M0L9_2aStringS982 =
            (struct _M0DTPB4Json6String*)_M0L8_2avalueS976;
          moonbit_string_t _M0L8_2afieldS3940 = _M0L9_2aStringS982->$0;
          int32_t _M0L6_2acntS4157 =
            Moonbit_object_header(_M0L9_2aStringS982)->rc;
          moonbit_string_t _M0L4_2asS983;
          moonbit_string_t _M0L6_2atmpS2632;
          if (_M0L6_2acntS4157 > 1) {
            int32_t _M0L11_2anew__cntS4158 = _M0L6_2acntS4157 - 1;
            Moonbit_object_header(_M0L9_2aStringS982)->rc
            = _M0L11_2anew__cntS4158;
            moonbit_incref(_M0L8_2afieldS3940);
          } else if (_M0L6_2acntS4157 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aStringS982);
          }
          _M0L4_2asS983 = _M0L8_2afieldS3940;
          moonbit_incref(_M0L3bufS971);
          #line 307 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 34);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2632
          = _M0FPC14json6escape(_M0L4_2asS983, _M0L13escape__slashS984);
          moonbit_incref(_M0L3bufS971);
          #line 308 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, _M0L6_2atmpS2632);
          moonbit_incref(_M0L3bufS971);
          #line 309 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS971, 34);
          break;
        }
        
        case 3: {
          struct _M0DTPB4Json6Number* _M0L9_2aNumberS985 =
            (struct _M0DTPB4Json6Number*)_M0L8_2avalueS976;
          double _M0L4_2anS986 = _M0L9_2aNumberS985->$0;
          moonbit_string_t _M0L8_2afieldS3941 = _M0L9_2aNumberS985->$1;
          int32_t _M0L6_2acntS4159 =
            Moonbit_object_header(_M0L9_2aNumberS985)->rc;
          moonbit_string_t _M0L7_2areprS987;
          if (_M0L6_2acntS4159 > 1) {
            int32_t _M0L11_2anew__cntS4160 = _M0L6_2acntS4159 - 1;
            Moonbit_object_header(_M0L9_2aNumberS985)->rc
            = _M0L11_2anew__cntS4160;
            if (_M0L8_2afieldS3941) {
              moonbit_incref(_M0L8_2afieldS3941);
            }
          } else if (_M0L6_2acntS4159 == 1) {
            #line 284 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            moonbit_free(_M0L9_2aNumberS985);
          }
          _M0L7_2areprS987 = _M0L8_2afieldS3941;
          if (_M0L7_2areprS987 == 0) {
            if (_M0L7_2areprS987) {
              moonbit_decref(_M0L7_2areprS987);
            }
            moonbit_incref(_M0L3bufS971);
            #line 313 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0MPB13StringBuilder13write__objectGdE(_M0L3bufS971, _M0L4_2anS986);
          } else {
            moonbit_string_t _M0L7_2aSomeS988 = _M0L7_2areprS987;
            moonbit_string_t _M0L4_2arS989 = _M0L7_2aSomeS988;
            moonbit_incref(_M0L3bufS971);
            #line 314 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
            _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, _M0L4_2arS989);
          }
          break;
        }
        
        case 1: {
          moonbit_incref(_M0L3bufS971);
          #line 316 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, (moonbit_string_t)moonbit_string_literal_20.data);
          break;
        }
        
        case 2: {
          moonbit_incref(_M0L3bufS971);
          #line 317 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, (moonbit_string_t)moonbit_string_literal_21.data);
          break;
        }
        default: {
          moonbit_decref(_M0L8_2avalueS976);
          moonbit_incref(_M0L3bufS971);
          #line 318 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS971, (moonbit_string_t)moonbit_string_literal_22.data);
          break;
        }
      }
      _M0L6_2atmpS2642 = 0;
      _M0L8_2aparamS974 = _M0L6_2atmpS2642;
      continue;
    }
    break;
  }
  #line 377 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS971);
}

moonbit_string_t _M0FPC14json11indent__str(
  int32_t _M0L5levelS970,
  int32_t _M0L6indentS968
) {
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  if (_M0L6indentS968 == 0) {
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else {
    int32_t _M0L6spacesS969 = _M0L6indentS968 * _M0L5levelS970;
    switch (_M0L6spacesS969) {
      case 0: {
        return (moonbit_string_t)moonbit_string_literal_23.data;
        break;
      }
      
      case 1: {
        return (moonbit_string_t)moonbit_string_literal_24.data;
        break;
      }
      
      case 2: {
        return (moonbit_string_t)moonbit_string_literal_25.data;
        break;
      }
      
      case 3: {
        return (moonbit_string_t)moonbit_string_literal_26.data;
        break;
      }
      
      case 4: {
        return (moonbit_string_t)moonbit_string_literal_27.data;
        break;
      }
      
      case 5: {
        return (moonbit_string_t)moonbit_string_literal_28.data;
        break;
      }
      
      case 6: {
        return (moonbit_string_t)moonbit_string_literal_29.data;
        break;
      }
      
      case 7: {
        return (moonbit_string_t)moonbit_string_literal_30.data;
        break;
      }
      
      case 8: {
        return (moonbit_string_t)moonbit_string_literal_31.data;
        break;
      }
      default: {
        moonbit_string_t _M0L6_2atmpS2604;
        moonbit_string_t _M0L6_2atmpS3942;
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS2604
        = _M0MPC16string6String6repeat((moonbit_string_t)moonbit_string_literal_32.data, _M0L6spacesS969);
        #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0L6_2atmpS3942
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_23.data, _M0L6_2atmpS2604);
        moonbit_decref(_M0L6_2atmpS2604);
        return _M0L6_2atmpS3942;
        break;
      }
    }
  }
}

moonbit_string_t _M0FPC14json6escape(
  moonbit_string_t _M0L3strS960,
  int32_t _M0L13escape__slashS965
) {
  int32_t _M0L6_2atmpS2603;
  struct _M0TPB13StringBuilder* _M0L3bufS959;
  struct _M0TWEOc* _M0L5_2aitS961;
  #line 381 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L6_2atmpS2603 = Moonbit_array_length(_M0L3strS960);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L3bufS959 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2603);
  #line 382 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  _M0L5_2aitS961 = _M0MPC16string6String4iter(_M0L3strS960);
  while (1) {
    int32_t _M0L7_2abindS962;
    moonbit_incref(_M0L5_2aitS961);
    #line 383 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
    _M0L7_2abindS962 = _M0MPB4Iter4nextGcE(_M0L5_2aitS961);
    if (_M0L7_2abindS962 == -1) {
      moonbit_decref(_M0L5_2aitS961);
    } else {
      int32_t _M0L7_2aSomeS963 = _M0L7_2abindS962;
      int32_t _M0L4_2acS964 = _M0L7_2aSomeS963;
      if (_M0L4_2acS964 == 34) {
        moonbit_incref(_M0L3bufS959);
        #line 385 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS959, (moonbit_string_t)moonbit_string_literal_33.data);
      } else if (_M0L4_2acS964 == 92) {
        moonbit_incref(_M0L3bufS959);
        #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS959, (moonbit_string_t)moonbit_string_literal_34.data);
      } else if (_M0L4_2acS964 == 47) {
        if (_M0L13escape__slashS965) {
          moonbit_incref(_M0L3bufS959);
          #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS959, (moonbit_string_t)moonbit_string_literal_35.data);
        } else {
          moonbit_incref(_M0L3bufS959);
          #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS959, _M0L4_2acS964);
        }
      } else if (_M0L4_2acS964 == 10) {
        moonbit_incref(_M0L3bufS959);
        #line 393 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS959, (moonbit_string_t)moonbit_string_literal_36.data);
      } else if (_M0L4_2acS964 == 13) {
        moonbit_incref(_M0L3bufS959);
        #line 394 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS959, (moonbit_string_t)moonbit_string_literal_37.data);
      } else if (_M0L4_2acS964 == 8) {
        moonbit_incref(_M0L3bufS959);
        #line 395 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS959, (moonbit_string_t)moonbit_string_literal_38.data);
      } else if (_M0L4_2acS964 == 9) {
        moonbit_incref(_M0L3bufS959);
        #line 396 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS959, (moonbit_string_t)moonbit_string_literal_39.data);
      } else {
        int32_t _M0L4codeS966 = _M0L4_2acS964;
        if (_M0L4codeS966 == 12) {
          moonbit_incref(_M0L3bufS959);
          #line 400 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS959, (moonbit_string_t)moonbit_string_literal_40.data);
        } else if (_M0L4codeS966 < 32) {
          int32_t _M0L6_2atmpS2602;
          moonbit_string_t _M0L6_2atmpS2601;
          moonbit_incref(_M0L3bufS959);
          #line 402 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS959, (moonbit_string_t)moonbit_string_literal_41.data);
          _M0L6_2atmpS2602 = _M0L4codeS966 & 0xff;
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0L6_2atmpS2601 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS2602);
          moonbit_incref(_M0L3bufS959);
          #line 403 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS959, _M0L6_2atmpS2601);
        } else {
          moonbit_incref(_M0L3bufS959);
          #line 405 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS959, _M0L4_2acS964);
        }
      }
      continue;
    }
    break;
  }
  #line 410 "C:\\Users\\Administrator\\.moon\\lib\\core\\json\\json.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS959);
}

int32_t _M0MPC15array5Array9is__emptyGRPB4JsonE(
  struct _M0TPB5ArrayGRPB4JsonE* _M0L4selfS958
) {
  int32_t _M0L8_2afieldS3943;
  int32_t _M0L3lenS2600;
  #line 717 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L8_2afieldS3943 = _M0L4selfS958->$1;
  moonbit_decref(_M0L4selfS958);
  _M0L3lenS2600 = _M0L8_2afieldS3943;
  return _M0L3lenS2600 == 0;
}

void* _M0MPC15array5Array3popGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS955
) {
  int32_t _M0L3lenS954;
  #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS954 = _M0L4selfS955->$1;
  if (_M0L3lenS954 == 0) {
    moonbit_decref(_M0L4selfS955);
    return 0;
  } else {
    int32_t _M0L5indexS956 = _M0L3lenS954 - 1;
    void** _M0L8_2afieldS3947 = _M0L4selfS955->$0;
    void** _M0L3bufS2599 = _M0L8_2afieldS3947;
    void* _M0L6_2atmpS3946 = (void*)_M0L3bufS2599[_M0L5indexS956];
    void* _M0L1vS957 = _M0L6_2atmpS3946;
    void** _M0L8_2afieldS3945 = _M0L4selfS955->$0;
    void** _M0L3bufS2598 = _M0L8_2afieldS3945;
    void* _M0L6_2aoldS3944;
    if (
      _M0L5indexS956 < 0
      || _M0L5indexS956 >= Moonbit_array_length(_M0L3bufS2598)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6_2aoldS3944 = (void*)_M0L3bufS2598[_M0L5indexS956];
    moonbit_incref(_M0L1vS957);
    moonbit_decref(_M0L6_2aoldS3944);
    if (
      _M0L5indexS956 < 0
      || _M0L5indexS956 >= Moonbit_array_length(_M0L3bufS2598)
    ) {
      #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
      moonbit_panic();
    }
    _M0L3bufS2598[_M0L5indexS956]
    = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
    _M0L4selfS955->$1 = _M0L5indexS956;
    moonbit_decref(_M0L4selfS955);
    return _M0L1vS957;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS952,
  struct _M0TPB6Logger _M0L6loggerS953
) {
  moonbit_string_t _M0L6_2atmpS2597;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2596;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2597 = _M0L4selfS952;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2596 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2597);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2596, _M0L6loggerS953);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS929,
  struct _M0TPB6Logger _M0L6loggerS951
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS3956;
  struct _M0TPC16string10StringView _M0L3pkgS928;
  moonbit_string_t _M0L7_2adataS930;
  int32_t _M0L8_2astartS931;
  int32_t _M0L6_2atmpS2595;
  int32_t _M0L6_2aendS932;
  int32_t _M0Lm9_2acursorS933;
  int32_t _M0Lm13accept__stateS934;
  int32_t _M0Lm10match__endS935;
  int32_t _M0Lm20match__tag__saver__0S936;
  int32_t _M0Lm6tag__0S937;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS938;
  struct _M0TPC16string10StringView _M0L8_2afieldS3955;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS947;
  void* _M0L8_2afieldS3954;
  int32_t _M0L6_2acntS4161;
  void* _M0L16_2apackage__nameS948;
  struct _M0TPC16string10StringView _M0L8_2afieldS3952;
  struct _M0TPC16string10StringView _M0L8filenameS2572;
  struct _M0TPC16string10StringView _M0L8_2afieldS3951;
  struct _M0TPC16string10StringView _M0L11start__lineS2573;
  struct _M0TPC16string10StringView _M0L8_2afieldS3950;
  struct _M0TPC16string10StringView _M0L13start__columnS2574;
  struct _M0TPC16string10StringView _M0L8_2afieldS3949;
  struct _M0TPC16string10StringView _M0L9end__lineS2575;
  struct _M0TPC16string10StringView _M0L8_2afieldS3948;
  int32_t _M0L6_2acntS4165;
  struct _M0TPC16string10StringView _M0L11end__columnS2576;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS3956
  = (struct _M0TPC16string10StringView){
    _M0L4selfS929->$0_1, _M0L4selfS929->$0_2, _M0L4selfS929->$0_0
  };
  _M0L3pkgS928 = _M0L8_2afieldS3956;
  moonbit_incref(_M0L3pkgS928.$0);
  moonbit_incref(_M0L3pkgS928.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS930 = _M0MPC16string10StringView4data(_M0L3pkgS928);
  moonbit_incref(_M0L3pkgS928.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS931 = _M0MPC16string10StringView13start__offset(_M0L3pkgS928);
  moonbit_incref(_M0L3pkgS928.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2595 = _M0MPC16string10StringView6length(_M0L3pkgS928);
  _M0L6_2aendS932 = _M0L8_2astartS931 + _M0L6_2atmpS2595;
  _M0Lm9_2acursorS933 = _M0L8_2astartS931;
  _M0Lm13accept__stateS934 = -1;
  _M0Lm10match__endS935 = -1;
  _M0Lm20match__tag__saver__0S936 = -1;
  _M0Lm6tag__0S937 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2587 = _M0Lm9_2acursorS933;
    if (_M0L6_2atmpS2587 < _M0L6_2aendS932) {
      int32_t _M0L6_2atmpS2594 = _M0Lm9_2acursorS933;
      int32_t _M0L10next__charS942;
      int32_t _M0L6_2atmpS2588;
      moonbit_incref(_M0L7_2adataS930);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS942
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS930, _M0L6_2atmpS2594);
      _M0L6_2atmpS2588 = _M0Lm9_2acursorS933;
      _M0Lm9_2acursorS933 = _M0L6_2atmpS2588 + 1;
      if (_M0L10next__charS942 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2589;
          _M0Lm6tag__0S937 = _M0Lm9_2acursorS933;
          _M0L6_2atmpS2589 = _M0Lm9_2acursorS933;
          if (_M0L6_2atmpS2589 < _M0L6_2aendS932) {
            int32_t _M0L6_2atmpS2593 = _M0Lm9_2acursorS933;
            int32_t _M0L10next__charS943;
            int32_t _M0L6_2atmpS2590;
            moonbit_incref(_M0L7_2adataS930);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS943
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS930, _M0L6_2atmpS2593);
            _M0L6_2atmpS2590 = _M0Lm9_2acursorS933;
            _M0Lm9_2acursorS933 = _M0L6_2atmpS2590 + 1;
            if (_M0L10next__charS943 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2591 = _M0Lm9_2acursorS933;
                if (_M0L6_2atmpS2591 < _M0L6_2aendS932) {
                  int32_t _M0L6_2atmpS2592 = _M0Lm9_2acursorS933;
                  _M0Lm9_2acursorS933 = _M0L6_2atmpS2592 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S936 = _M0Lm6tag__0S937;
                  _M0Lm13accept__stateS934 = 0;
                  _M0Lm10match__endS935 = _M0Lm9_2acursorS933;
                  goto join_939;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_939;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_939;
    }
    break;
  }
  goto joinlet_4276;
  join_939:;
  switch (_M0Lm13accept__stateS934) {
    case 0: {
      int32_t _M0L6_2atmpS2585;
      int32_t _M0L6_2atmpS2584;
      int64_t _M0L6_2atmpS2581;
      int32_t _M0L6_2atmpS2583;
      int64_t _M0L6_2atmpS2582;
      struct _M0TPC16string10StringView _M0L13package__nameS940;
      int64_t _M0L6_2atmpS2578;
      int32_t _M0L6_2atmpS2580;
      int64_t _M0L6_2atmpS2579;
      struct _M0TPC16string10StringView _M0L12module__nameS941;
      void* _M0L4SomeS2577;
      moonbit_decref(_M0L3pkgS928.$0);
      _M0L6_2atmpS2585 = _M0Lm20match__tag__saver__0S936;
      _M0L6_2atmpS2584 = _M0L6_2atmpS2585 + 1;
      _M0L6_2atmpS2581 = (int64_t)_M0L6_2atmpS2584;
      _M0L6_2atmpS2583 = _M0Lm10match__endS935;
      _M0L6_2atmpS2582 = (int64_t)_M0L6_2atmpS2583;
      moonbit_incref(_M0L7_2adataS930);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS940
      = _M0MPC16string6String4view(_M0L7_2adataS930, _M0L6_2atmpS2581, _M0L6_2atmpS2582);
      _M0L6_2atmpS2578 = (int64_t)_M0L8_2astartS931;
      _M0L6_2atmpS2580 = _M0Lm20match__tag__saver__0S936;
      _M0L6_2atmpS2579 = (int64_t)_M0L6_2atmpS2580;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS941
      = _M0MPC16string6String4view(_M0L7_2adataS930, _M0L6_2atmpS2578, _M0L6_2atmpS2579);
      _M0L4SomeS2577
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2577)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2577)->$0_0
      = _M0L13package__nameS940.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2577)->$0_1
      = _M0L13package__nameS940.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2577)->$0_2
      = _M0L13package__nameS940.$2;
      _M0L7_2abindS938
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS938)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS938->$0_0 = _M0L12module__nameS941.$0;
      _M0L7_2abindS938->$0_1 = _M0L12module__nameS941.$1;
      _M0L7_2abindS938->$0_2 = _M0L12module__nameS941.$2;
      _M0L7_2abindS938->$1 = _M0L4SomeS2577;
      break;
    }
    default: {
      void* _M0L4NoneS2586;
      moonbit_decref(_M0L7_2adataS930);
      _M0L4NoneS2586
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS938
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS938)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS938->$0_0 = _M0L3pkgS928.$0;
      _M0L7_2abindS938->$0_1 = _M0L3pkgS928.$1;
      _M0L7_2abindS938->$0_2 = _M0L3pkgS928.$2;
      _M0L7_2abindS938->$1 = _M0L4NoneS2586;
      break;
    }
  }
  joinlet_4276:;
  _M0L8_2afieldS3955
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS938->$0_1, _M0L7_2abindS938->$0_2, _M0L7_2abindS938->$0_0
  };
  _M0L15_2amodule__nameS947 = _M0L8_2afieldS3955;
  _M0L8_2afieldS3954 = _M0L7_2abindS938->$1;
  _M0L6_2acntS4161 = Moonbit_object_header(_M0L7_2abindS938)->rc;
  if (_M0L6_2acntS4161 > 1) {
    int32_t _M0L11_2anew__cntS4162 = _M0L6_2acntS4161 - 1;
    Moonbit_object_header(_M0L7_2abindS938)->rc = _M0L11_2anew__cntS4162;
    moonbit_incref(_M0L8_2afieldS3954);
    moonbit_incref(_M0L15_2amodule__nameS947.$0);
  } else if (_M0L6_2acntS4161 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS938);
  }
  _M0L16_2apackage__nameS948 = _M0L8_2afieldS3954;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS948)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS949 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS948;
      struct _M0TPC16string10StringView _M0L8_2afieldS3953 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS949->$0_1,
                                              _M0L7_2aSomeS949->$0_2,
                                              _M0L7_2aSomeS949->$0_0};
      int32_t _M0L6_2acntS4163 = Moonbit_object_header(_M0L7_2aSomeS949)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS950;
      if (_M0L6_2acntS4163 > 1) {
        int32_t _M0L11_2anew__cntS4164 = _M0L6_2acntS4163 - 1;
        Moonbit_object_header(_M0L7_2aSomeS949)->rc = _M0L11_2anew__cntS4164;
        moonbit_incref(_M0L8_2afieldS3953.$0);
      } else if (_M0L6_2acntS4163 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS949);
      }
      _M0L12_2apkg__nameS950 = _M0L8_2afieldS3953;
      if (_M0L6loggerS951.$1) {
        moonbit_incref(_M0L6loggerS951.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS951.$0->$method_2(_M0L6loggerS951.$1, _M0L12_2apkg__nameS950);
      if (_M0L6loggerS951.$1) {
        moonbit_incref(_M0L6loggerS951.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS951.$0->$method_3(_M0L6loggerS951.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS948);
      break;
    }
  }
  _M0L8_2afieldS3952
  = (struct _M0TPC16string10StringView){
    _M0L4selfS929->$1_1, _M0L4selfS929->$1_2, _M0L4selfS929->$1_0
  };
  _M0L8filenameS2572 = _M0L8_2afieldS3952;
  moonbit_incref(_M0L8filenameS2572.$0);
  if (_M0L6loggerS951.$1) {
    moonbit_incref(_M0L6loggerS951.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS951.$0->$method_2(_M0L6loggerS951.$1, _M0L8filenameS2572);
  if (_M0L6loggerS951.$1) {
    moonbit_incref(_M0L6loggerS951.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS951.$0->$method_3(_M0L6loggerS951.$1, 58);
  _M0L8_2afieldS3951
  = (struct _M0TPC16string10StringView){
    _M0L4selfS929->$2_1, _M0L4selfS929->$2_2, _M0L4selfS929->$2_0
  };
  _M0L11start__lineS2573 = _M0L8_2afieldS3951;
  moonbit_incref(_M0L11start__lineS2573.$0);
  if (_M0L6loggerS951.$1) {
    moonbit_incref(_M0L6loggerS951.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS951.$0->$method_2(_M0L6loggerS951.$1, _M0L11start__lineS2573);
  if (_M0L6loggerS951.$1) {
    moonbit_incref(_M0L6loggerS951.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS951.$0->$method_3(_M0L6loggerS951.$1, 58);
  _M0L8_2afieldS3950
  = (struct _M0TPC16string10StringView){
    _M0L4selfS929->$3_1, _M0L4selfS929->$3_2, _M0L4selfS929->$3_0
  };
  _M0L13start__columnS2574 = _M0L8_2afieldS3950;
  moonbit_incref(_M0L13start__columnS2574.$0);
  if (_M0L6loggerS951.$1) {
    moonbit_incref(_M0L6loggerS951.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS951.$0->$method_2(_M0L6loggerS951.$1, _M0L13start__columnS2574);
  if (_M0L6loggerS951.$1) {
    moonbit_incref(_M0L6loggerS951.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS951.$0->$method_3(_M0L6loggerS951.$1, 45);
  _M0L8_2afieldS3949
  = (struct _M0TPC16string10StringView){
    _M0L4selfS929->$4_1, _M0L4selfS929->$4_2, _M0L4selfS929->$4_0
  };
  _M0L9end__lineS2575 = _M0L8_2afieldS3949;
  moonbit_incref(_M0L9end__lineS2575.$0);
  if (_M0L6loggerS951.$1) {
    moonbit_incref(_M0L6loggerS951.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS951.$0->$method_2(_M0L6loggerS951.$1, _M0L9end__lineS2575);
  if (_M0L6loggerS951.$1) {
    moonbit_incref(_M0L6loggerS951.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS951.$0->$method_3(_M0L6loggerS951.$1, 58);
  _M0L8_2afieldS3948
  = (struct _M0TPC16string10StringView){
    _M0L4selfS929->$5_1, _M0L4selfS929->$5_2, _M0L4selfS929->$5_0
  };
  _M0L6_2acntS4165 = Moonbit_object_header(_M0L4selfS929)->rc;
  if (_M0L6_2acntS4165 > 1) {
    int32_t _M0L11_2anew__cntS4171 = _M0L6_2acntS4165 - 1;
    Moonbit_object_header(_M0L4selfS929)->rc = _M0L11_2anew__cntS4171;
    moonbit_incref(_M0L8_2afieldS3948.$0);
  } else if (_M0L6_2acntS4165 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4170 =
      (struct _M0TPC16string10StringView){_M0L4selfS929->$4_1,
                                            _M0L4selfS929->$4_2,
                                            _M0L4selfS929->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4169;
    struct _M0TPC16string10StringView _M0L8_2afieldS4168;
    struct _M0TPC16string10StringView _M0L8_2afieldS4167;
    struct _M0TPC16string10StringView _M0L8_2afieldS4166;
    moonbit_decref(_M0L8_2afieldS4170.$0);
    _M0L8_2afieldS4169
    = (struct _M0TPC16string10StringView){
      _M0L4selfS929->$3_1, _M0L4selfS929->$3_2, _M0L4selfS929->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4169.$0);
    _M0L8_2afieldS4168
    = (struct _M0TPC16string10StringView){
      _M0L4selfS929->$2_1, _M0L4selfS929->$2_2, _M0L4selfS929->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4168.$0);
    _M0L8_2afieldS4167
    = (struct _M0TPC16string10StringView){
      _M0L4selfS929->$1_1, _M0L4selfS929->$1_2, _M0L4selfS929->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4167.$0);
    _M0L8_2afieldS4166
    = (struct _M0TPC16string10StringView){
      _M0L4selfS929->$0_1, _M0L4selfS929->$0_2, _M0L4selfS929->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4166.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS929);
  }
  _M0L11end__columnS2576 = _M0L8_2afieldS3948;
  if (_M0L6loggerS951.$1) {
    moonbit_incref(_M0L6loggerS951.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS951.$0->$method_2(_M0L6loggerS951.$1, _M0L11end__columnS2576);
  if (_M0L6loggerS951.$1) {
    moonbit_incref(_M0L6loggerS951.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS951.$0->$method_3(_M0L6loggerS951.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS951.$0->$method_2(_M0L6loggerS951.$1, _M0L15_2amodule__nameS947);
  return 0;
}

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t _M0L4selfS920,
  int32_t _M0L5startS926,
  int64_t _M0L3endS922
) {
  int32_t _M0L3lenS919;
  int32_t _M0L3endS921;
  int32_t _M0L5startS925;
  int32_t _if__result_4280;
  #line 170 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L3lenS919 = Moonbit_array_length(_M0L4selfS920);
  if (_M0L3endS922 == 4294967296ll) {
    _M0L3endS921 = _M0L3lenS919;
  } else {
    int64_t _M0L7_2aSomeS923 = _M0L3endS922;
    int32_t _M0L6_2aendS924 = (int32_t)_M0L7_2aSomeS923;
    if (_M0L6_2aendS924 < 0) {
      _M0L3endS921 = _M0L3lenS919 + _M0L6_2aendS924;
    } else {
      _M0L3endS921 = _M0L6_2aendS924;
    }
  }
  if (_M0L5startS926 < 0) {
    _M0L5startS925 = _M0L3lenS919 + _M0L5startS926;
  } else {
    _M0L5startS925 = _M0L5startS926;
  }
  if (_M0L5startS925 >= 0) {
    if (_M0L5startS925 <= _M0L3endS921) {
      _if__result_4280 = _M0L3endS921 <= _M0L3lenS919;
    } else {
      _if__result_4280 = 0;
    }
  } else {
    _if__result_4280 = 0;
  }
  if (_if__result_4280) {
    int32_t _M0L7_2abindS927 = _M0L3endS921 - _M0L5startS925;
    int32_t _M0L6_2atmpS2571 = _M0L5startS925 + _M0L7_2abindS927;
    return (struct _M0TPC15bytes9BytesView){_M0L5startS925,
                                              _M0L6_2atmpS2571,
                                              _M0L4selfS920};
  } else {
    moonbit_decref(_M0L4selfS920);
    #line 180 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
    return _M0FPB5abortGRPC15bytes9BytesViewE((moonbit_string_t)moonbit_string_literal_42.data, (moonbit_string_t)moonbit_string_literal_43.data);
  }
}

int32_t _M0MPC15bytes9BytesView6length(
  struct _M0TPC15bytes9BytesView _M0L4selfS918
) {
  int32_t _M0L3endS2569;
  int32_t _M0L8_2afieldS3957;
  int32_t _M0L5startS2570;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L3endS2569 = _M0L4selfS918.$2;
  _M0L8_2afieldS3957 = _M0L4selfS918.$1;
  moonbit_decref(_M0L4selfS918.$0);
  _M0L5startS2570 = _M0L8_2afieldS3957;
  return _M0L3endS2569 - _M0L5startS2570;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS917) {
  moonbit_string_t _M0L6_2atmpS2568;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2568 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS917);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2568);
  moonbit_decref(_M0L6_2atmpS2568);
  return 0;
}

int32_t _M0IPC16double6DoublePB4Show6output(
  double _M0L4selfS916,
  struct _M0TPB6Logger _M0L6loggerS915
) {
  moonbit_string_t _M0L6_2atmpS2567;
  #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6_2atmpS2567 = _M0MPC16double6Double10to__string(_M0L4selfS916);
  #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  _M0L6loggerS915.$0->$method_0(_M0L6loggerS915.$1, _M0L6_2atmpS2567);
  return 0;
}

moonbit_string_t _M0MPC16double6Double10to__string(double _M0L4selfS914) {
  #line 282 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double.mbt"
  return _M0FPB15ryu__to__string(_M0L4selfS914);
}

moonbit_string_t _M0FPB15ryu__to__string(double _M0L3valS901) {
  uint64_t _M0L4bitsS902;
  uint64_t _M0L6_2atmpS2566;
  uint64_t _M0L6_2atmpS2565;
  int32_t _M0L8ieeeSignS903;
  uint64_t _M0L12ieeeMantissaS904;
  uint64_t _M0L6_2atmpS2564;
  uint64_t _M0L6_2atmpS2563;
  int32_t _M0L12ieeeExponentS905;
  int32_t _if__result_4281;
  struct _M0TPB17FloatingDecimal64* _M0Lm1vS906;
  struct _M0TPB17FloatingDecimal64* _M0L5smallS907;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2562;
  #line 659 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L3valS901 == 0x0p+0) {
    return (moonbit_string_t)moonbit_string_literal_44.data;
  }
  _M0L4bitsS902 = *(int64_t*)&_M0L3valS901;
  _M0L6_2atmpS2566 = _M0L4bitsS902 >> 63;
  _M0L6_2atmpS2565 = _M0L6_2atmpS2566 & 1ull;
  _M0L8ieeeSignS903 = _M0L6_2atmpS2565 != 0ull;
  _M0L12ieeeMantissaS904 = _M0L4bitsS902 & 4503599627370495ull;
  _M0L6_2atmpS2564 = _M0L4bitsS902 >> 52;
  _M0L6_2atmpS2563 = _M0L6_2atmpS2564 & 2047ull;
  _M0L12ieeeExponentS905 = (int32_t)_M0L6_2atmpS2563;
  if (_M0L12ieeeExponentS905 == 2047) {
    _if__result_4281 = 1;
  } else if (_M0L12ieeeExponentS905 == 0) {
    _if__result_4281 = _M0L12ieeeMantissaS904 == 0ull;
  } else {
    _if__result_4281 = 0;
  }
  if (_if__result_4281) {
    int32_t _M0L6_2atmpS2551 = _M0L12ieeeExponentS905 != 0;
    int32_t _M0L6_2atmpS2552 = _M0L12ieeeMantissaS904 != 0ull;
    #line 676 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB18copy__special__str(_M0L8ieeeSignS903, _M0L6_2atmpS2551, _M0L6_2atmpS2552);
  }
  _M0Lm1vS906 = _M0FPB30ryu__to__string_2erecord_2f900;
  #line 679 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L5smallS907
  = _M0FPB15d2d__small__int(_M0L12ieeeMantissaS904, _M0L12ieeeExponentS905);
  if (_M0L5smallS907 == 0) {
    uint32_t _M0L6_2atmpS2553;
    if (_M0L5smallS907) {
      moonbit_decref(_M0L5smallS907);
    }
    _M0L6_2atmpS2553 = *(uint32_t*)&_M0L12ieeeExponentS905;
    #line 693 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm1vS906 = _M0FPB3d2d(_M0L12ieeeMantissaS904, _M0L6_2atmpS2553);
  } else {
    struct _M0TPB17FloatingDecimal64* _M0L7_2aSomeS908 = _M0L5smallS907;
    struct _M0TPB17FloatingDecimal64* _M0L4_2afS909 = _M0L7_2aSomeS908;
    struct _M0TPB17FloatingDecimal64* _M0Lm1xS910 = _M0L4_2afS909;
    while (1) {
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2561 = _M0Lm1xS910;
      uint64_t _M0L8_2afieldS3960 = _M0L6_2atmpS2561->$0;
      uint64_t _M0L8mantissaS2560 = _M0L8_2afieldS3960;
      uint64_t _M0L1qS911 = _M0L8mantissaS2560 / 10ull;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2559 = _M0Lm1xS910;
      uint64_t _M0L8_2afieldS3959 = _M0L6_2atmpS2559->$0;
      uint64_t _M0L8mantissaS2557 = _M0L8_2afieldS3959;
      uint64_t _M0L6_2atmpS2558 = 10ull * _M0L1qS911;
      uint64_t _M0L1rS912 = _M0L8mantissaS2557 - _M0L6_2atmpS2558;
      struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2556;
      int32_t _M0L8_2afieldS3958;
      int32_t _M0L8exponentS2555;
      int32_t _M0L6_2atmpS2554;
      if (_M0L1rS912 != 0ull) {
        break;
      }
      _M0L6_2atmpS2556 = _M0Lm1xS910;
      _M0L8_2afieldS3958 = _M0L6_2atmpS2556->$1;
      moonbit_decref(_M0L6_2atmpS2556);
      _M0L8exponentS2555 = _M0L8_2afieldS3958;
      _M0L6_2atmpS2554 = _M0L8exponentS2555 + 1;
      _M0Lm1xS910
      = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
      Moonbit_object_header(_M0Lm1xS910)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
      _M0Lm1xS910->$0 = _M0L1qS911;
      _M0Lm1xS910->$1 = _M0L6_2atmpS2554;
      continue;
      break;
    }
    _M0Lm1vS906 = _M0Lm1xS910;
  }
  _M0L6_2atmpS2562 = _M0Lm1vS906;
  #line 695 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB9to__chars(_M0L6_2atmpS2562, _M0L8ieeeSignS903);
}

struct _M0TPB17FloatingDecimal64* _M0FPB15d2d__small__int(
  uint64_t _M0L12ieeeMantissaS895,
  int32_t _M0L12ieeeExponentS897
) {
  uint64_t _M0L2m2S894;
  int32_t _M0L6_2atmpS2550;
  int32_t _M0L2e2S896;
  int32_t _M0L6_2atmpS2549;
  uint64_t _M0L6_2atmpS2548;
  uint64_t _M0L4maskS898;
  uint64_t _M0L8fractionS899;
  int32_t _M0L6_2atmpS2547;
  uint64_t _M0L6_2atmpS2546;
  struct _M0TPB17FloatingDecimal64* _M0L6_2atmpS2545;
  #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2m2S894 = 4503599627370496ull | _M0L12ieeeMantissaS895;
  _M0L6_2atmpS2550 = _M0L12ieeeExponentS897 - 1023;
  _M0L2e2S896 = _M0L6_2atmpS2550 - 52;
  if (_M0L2e2S896 > 0) {
    return 0;
  }
  if (_M0L2e2S896 < -52) {
    return 0;
  }
  _M0L6_2atmpS2549 = -_M0L2e2S896;
  _M0L6_2atmpS2548 = 1ull << (_M0L6_2atmpS2549 & 63);
  _M0L4maskS898 = _M0L6_2atmpS2548 - 1ull;
  _M0L8fractionS899 = _M0L2m2S894 & _M0L4maskS898;
  if (_M0L8fractionS899 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2547 = -_M0L2e2S896;
  _M0L6_2atmpS2546 = _M0L2m2S894 >> (_M0L6_2atmpS2547 & 63);
  _M0L6_2atmpS2545
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_M0L6_2atmpS2545)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _M0L6_2atmpS2545->$0 = _M0L6_2atmpS2546;
  _M0L6_2atmpS2545->$1 = 0;
  return _M0L6_2atmpS2545;
}

moonbit_string_t _M0FPB9to__chars(
  struct _M0TPB17FloatingDecimal64* _M0L1vS868,
  int32_t _M0L4signS866
) {
  int32_t _M0L6_2atmpS2544;
  moonbit_bytes_t _M0L6resultS864;
  int32_t _M0Lm5indexS865;
  uint64_t _M0Lm6outputS867;
  uint64_t _M0L6_2atmpS2543;
  int32_t _M0L7olengthS869;
  int32_t _M0L8_2afieldS3961;
  int32_t _M0L8exponentS2542;
  int32_t _M0L6_2atmpS2541;
  int32_t _M0Lm3expS870;
  int32_t _M0L6_2atmpS2540;
  int32_t _M0L6_2atmpS2538;
  int32_t _M0L18scientificNotationS871;
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2544 = _M0IPC14byte4BytePB7Default7default();
  _M0L6resultS864 = (moonbit_bytes_t)moonbit_make_bytes(25, _M0L6_2atmpS2544);
  _M0Lm5indexS865 = 0;
  if (_M0L4signS866) {
    int32_t _M0L6_2atmpS2413 = _M0Lm5indexS865;
    int32_t _M0L6_2atmpS2414;
    if (
      _M0L6_2atmpS2413 < 0
      || _M0L6_2atmpS2413 >= Moonbit_array_length(_M0L6resultS864)
    ) {
      #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS864[_M0L6_2atmpS2413] = 45;
    _M0L6_2atmpS2414 = _M0Lm5indexS865;
    _M0Lm5indexS865 = _M0L6_2atmpS2414 + 1;
  }
  _M0Lm6outputS867 = _M0L1vS868->$0;
  _M0L6_2atmpS2543 = _M0Lm6outputS867;
  #line 540 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7olengthS869 = _M0FPB17decimal__length17(_M0L6_2atmpS2543);
  _M0L8_2afieldS3961 = _M0L1vS868->$1;
  moonbit_decref(_M0L1vS868);
  _M0L8exponentS2542 = _M0L8_2afieldS3961;
  _M0L6_2atmpS2541 = _M0L8exponentS2542 + _M0L7olengthS869;
  _M0Lm3expS870 = _M0L6_2atmpS2541 - 1;
  _M0L6_2atmpS2540 = _M0Lm3expS870;
  if (_M0L6_2atmpS2540 >= -6) {
    int32_t _M0L6_2atmpS2539 = _M0Lm3expS870;
    _M0L6_2atmpS2538 = _M0L6_2atmpS2539 < 21;
  } else {
    _M0L6_2atmpS2538 = 0;
  }
  _M0L18scientificNotationS871 = !_M0L6_2atmpS2538;
  if (_M0L18scientificNotationS871) {
    int32_t _M0L7_2abindS872 = _M0L7olengthS869 - 1;
    int32_t _M0L1iS873 = 0;
    int32_t _M0L6_2atmpS2424;
    uint64_t _M0L6_2atmpS2429;
    int32_t _M0L6_2atmpS2428;
    int32_t _M0L6_2atmpS2427;
    int32_t _M0L6_2atmpS2426;
    int32_t _M0L6_2atmpS2425;
    int32_t _M0L6_2atmpS2433;
    int32_t _M0L6_2atmpS2434;
    int32_t _M0L6_2atmpS2435;
    int32_t _M0L6_2atmpS2436;
    int32_t _M0L6_2atmpS2437;
    int32_t _M0L6_2atmpS2443;
    int32_t _M0L6_2atmpS2476;
    while (1) {
      if (_M0L1iS873 < _M0L7_2abindS872) {
        uint64_t _M0L6_2atmpS2422 = _M0Lm6outputS867;
        uint64_t _M0L1cS874 = _M0L6_2atmpS2422 % 10ull;
        uint64_t _M0L6_2atmpS2415 = _M0Lm6outputS867;
        int32_t _M0L6_2atmpS2421;
        int32_t _M0L6_2atmpS2420;
        int32_t _M0L6_2atmpS2416;
        int32_t _M0L6_2atmpS2419;
        int32_t _M0L6_2atmpS2418;
        int32_t _M0L6_2atmpS2417;
        int32_t _M0L6_2atmpS2423;
        _M0Lm6outputS867 = _M0L6_2atmpS2415 / 10ull;
        _M0L6_2atmpS2421 = _M0Lm5indexS865;
        _M0L6_2atmpS2420 = _M0L6_2atmpS2421 + _M0L7olengthS869;
        _M0L6_2atmpS2416 = _M0L6_2atmpS2420 - _M0L1iS873;
        _M0L6_2atmpS2419 = (int32_t)_M0L1cS874;
        _M0L6_2atmpS2418 = 48 + _M0L6_2atmpS2419;
        _M0L6_2atmpS2417 = _M0L6_2atmpS2418 & 0xff;
        if (
          _M0L6_2atmpS2416 < 0
          || _M0L6_2atmpS2416 >= Moonbit_array_length(_M0L6resultS864)
        ) {
          #line 549 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS864[_M0L6_2atmpS2416] = _M0L6_2atmpS2417;
        _M0L6_2atmpS2423 = _M0L1iS873 + 1;
        _M0L1iS873 = _M0L6_2atmpS2423;
        continue;
      }
      break;
    }
    _M0L6_2atmpS2424 = _M0Lm5indexS865;
    _M0L6_2atmpS2429 = _M0Lm6outputS867;
    _M0L6_2atmpS2428 = (int32_t)_M0L6_2atmpS2429;
    _M0L6_2atmpS2427 = _M0L6_2atmpS2428 % 10;
    _M0L6_2atmpS2426 = 48 + _M0L6_2atmpS2427;
    _M0L6_2atmpS2425 = _M0L6_2atmpS2426 & 0xff;
    if (
      _M0L6_2atmpS2424 < 0
      || _M0L6_2atmpS2424 >= Moonbit_array_length(_M0L6resultS864)
    ) {
      #line 551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS864[_M0L6_2atmpS2424] = _M0L6_2atmpS2425;
    if (_M0L7olengthS869 > 1) {
      int32_t _M0L6_2atmpS2431 = _M0Lm5indexS865;
      int32_t _M0L6_2atmpS2430 = _M0L6_2atmpS2431 + 1;
      if (
        _M0L6_2atmpS2430 < 0
        || _M0L6_2atmpS2430 >= Moonbit_array_length(_M0L6resultS864)
      ) {
        #line 553 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS864[_M0L6_2atmpS2430] = 46;
    } else {
      int32_t _M0L6_2atmpS2432 = _M0Lm5indexS865;
      _M0Lm5indexS865 = _M0L6_2atmpS2432 - 1;
    }
    _M0L6_2atmpS2433 = _M0Lm5indexS865;
    _M0L6_2atmpS2434 = _M0L7olengthS869 + 1;
    _M0Lm5indexS865 = _M0L6_2atmpS2433 + _M0L6_2atmpS2434;
    _M0L6_2atmpS2435 = _M0Lm5indexS865;
    if (
      _M0L6_2atmpS2435 < 0
      || _M0L6_2atmpS2435 >= Moonbit_array_length(_M0L6resultS864)
    ) {
      #line 561 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      moonbit_panic();
    }
    _M0L6resultS864[_M0L6_2atmpS2435] = 101;
    _M0L6_2atmpS2436 = _M0Lm5indexS865;
    _M0Lm5indexS865 = _M0L6_2atmpS2436 + 1;
    _M0L6_2atmpS2437 = _M0Lm3expS870;
    if (_M0L6_2atmpS2437 < 0) {
      int32_t _M0L6_2atmpS2438 = _M0Lm5indexS865;
      int32_t _M0L6_2atmpS2439;
      int32_t _M0L6_2atmpS2440;
      if (
        _M0L6_2atmpS2438 < 0
        || _M0L6_2atmpS2438 >= Moonbit_array_length(_M0L6resultS864)
      ) {
        #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS864[_M0L6_2atmpS2438] = 45;
      _M0L6_2atmpS2439 = _M0Lm5indexS865;
      _M0Lm5indexS865 = _M0L6_2atmpS2439 + 1;
      _M0L6_2atmpS2440 = _M0Lm3expS870;
      _M0Lm3expS870 = -_M0L6_2atmpS2440;
    } else {
      int32_t _M0L6_2atmpS2441 = _M0Lm5indexS865;
      int32_t _M0L6_2atmpS2442;
      if (
        _M0L6_2atmpS2441 < 0
        || _M0L6_2atmpS2441 >= Moonbit_array_length(_M0L6resultS864)
      ) {
        #line 568 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS864[_M0L6_2atmpS2441] = 43;
      _M0L6_2atmpS2442 = _M0Lm5indexS865;
      _M0Lm5indexS865 = _M0L6_2atmpS2442 + 1;
    }
    _M0L6_2atmpS2443 = _M0Lm3expS870;
    if (_M0L6_2atmpS2443 >= 100) {
      int32_t _M0L6_2atmpS2459 = _M0Lm3expS870;
      int32_t _M0L1aS876 = _M0L6_2atmpS2459 / 100;
      int32_t _M0L6_2atmpS2458 = _M0Lm3expS870;
      int32_t _M0L6_2atmpS2457 = _M0L6_2atmpS2458 / 10;
      int32_t _M0L1bS877 = _M0L6_2atmpS2457 % 10;
      int32_t _M0L6_2atmpS2456 = _M0Lm3expS870;
      int32_t _M0L1cS878 = _M0L6_2atmpS2456 % 10;
      int32_t _M0L6_2atmpS2444 = _M0Lm5indexS865;
      int32_t _M0L6_2atmpS2446 = 48 + _M0L1aS876;
      int32_t _M0L6_2atmpS2445 = _M0L6_2atmpS2446 & 0xff;
      int32_t _M0L6_2atmpS2450;
      int32_t _M0L6_2atmpS2447;
      int32_t _M0L6_2atmpS2449;
      int32_t _M0L6_2atmpS2448;
      int32_t _M0L6_2atmpS2454;
      int32_t _M0L6_2atmpS2451;
      int32_t _M0L6_2atmpS2453;
      int32_t _M0L6_2atmpS2452;
      int32_t _M0L6_2atmpS2455;
      if (
        _M0L6_2atmpS2444 < 0
        || _M0L6_2atmpS2444 >= Moonbit_array_length(_M0L6resultS864)
      ) {
        #line 575 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS864[_M0L6_2atmpS2444] = _M0L6_2atmpS2445;
      _M0L6_2atmpS2450 = _M0Lm5indexS865;
      _M0L6_2atmpS2447 = _M0L6_2atmpS2450 + 1;
      _M0L6_2atmpS2449 = 48 + _M0L1bS877;
      _M0L6_2atmpS2448 = _M0L6_2atmpS2449 & 0xff;
      if (
        _M0L6_2atmpS2447 < 0
        || _M0L6_2atmpS2447 >= Moonbit_array_length(_M0L6resultS864)
      ) {
        #line 576 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS864[_M0L6_2atmpS2447] = _M0L6_2atmpS2448;
      _M0L6_2atmpS2454 = _M0Lm5indexS865;
      _M0L6_2atmpS2451 = _M0L6_2atmpS2454 + 2;
      _M0L6_2atmpS2453 = 48 + _M0L1cS878;
      _M0L6_2atmpS2452 = _M0L6_2atmpS2453 & 0xff;
      if (
        _M0L6_2atmpS2451 < 0
        || _M0L6_2atmpS2451 >= Moonbit_array_length(_M0L6resultS864)
      ) {
        #line 577 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS864[_M0L6_2atmpS2451] = _M0L6_2atmpS2452;
      _M0L6_2atmpS2455 = _M0Lm5indexS865;
      _M0Lm5indexS865 = _M0L6_2atmpS2455 + 3;
    } else {
      int32_t _M0L6_2atmpS2460 = _M0Lm3expS870;
      if (_M0L6_2atmpS2460 >= 10) {
        int32_t _M0L6_2atmpS2470 = _M0Lm3expS870;
        int32_t _M0L1aS879 = _M0L6_2atmpS2470 / 10;
        int32_t _M0L6_2atmpS2469 = _M0Lm3expS870;
        int32_t _M0L1bS880 = _M0L6_2atmpS2469 % 10;
        int32_t _M0L6_2atmpS2461 = _M0Lm5indexS865;
        int32_t _M0L6_2atmpS2463 = 48 + _M0L1aS879;
        int32_t _M0L6_2atmpS2462 = _M0L6_2atmpS2463 & 0xff;
        int32_t _M0L6_2atmpS2467;
        int32_t _M0L6_2atmpS2464;
        int32_t _M0L6_2atmpS2466;
        int32_t _M0L6_2atmpS2465;
        int32_t _M0L6_2atmpS2468;
        if (
          _M0L6_2atmpS2461 < 0
          || _M0L6_2atmpS2461 >= Moonbit_array_length(_M0L6resultS864)
        ) {
          #line 582 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS864[_M0L6_2atmpS2461] = _M0L6_2atmpS2462;
        _M0L6_2atmpS2467 = _M0Lm5indexS865;
        _M0L6_2atmpS2464 = _M0L6_2atmpS2467 + 1;
        _M0L6_2atmpS2466 = 48 + _M0L1bS880;
        _M0L6_2atmpS2465 = _M0L6_2atmpS2466 & 0xff;
        if (
          _M0L6_2atmpS2464 < 0
          || _M0L6_2atmpS2464 >= Moonbit_array_length(_M0L6resultS864)
        ) {
          #line 583 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS864[_M0L6_2atmpS2464] = _M0L6_2atmpS2465;
        _M0L6_2atmpS2468 = _M0Lm5indexS865;
        _M0Lm5indexS865 = _M0L6_2atmpS2468 + 2;
      } else {
        int32_t _M0L6_2atmpS2471 = _M0Lm5indexS865;
        int32_t _M0L6_2atmpS2474 = _M0Lm3expS870;
        int32_t _M0L6_2atmpS2473 = 48 + _M0L6_2atmpS2474;
        int32_t _M0L6_2atmpS2472 = _M0L6_2atmpS2473 & 0xff;
        int32_t _M0L6_2atmpS2475;
        if (
          _M0L6_2atmpS2471 < 0
          || _M0L6_2atmpS2471 >= Moonbit_array_length(_M0L6resultS864)
        ) {
          #line 586 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
          moonbit_panic();
        }
        _M0L6resultS864[_M0L6_2atmpS2471] = _M0L6_2atmpS2472;
        _M0L6_2atmpS2475 = _M0Lm5indexS865;
        _M0Lm5indexS865 = _M0L6_2atmpS2475 + 1;
      }
    }
    _M0L6_2atmpS2476 = _M0Lm5indexS865;
    #line 589 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS864, 0, _M0L6_2atmpS2476);
  } else {
    int32_t _M0L6_2atmpS2477 = _M0Lm3expS870;
    int32_t _M0L6_2atmpS2537;
    if (_M0L6_2atmpS2477 < 0) {
      int32_t _M0L6_2atmpS2478 = _M0Lm5indexS865;
      int32_t _M0L6_2atmpS2479;
      int32_t _M0L6_2atmpS2480;
      int32_t _M0L6_2atmpS2481;
      int32_t _M0L1iS881;
      int32_t _M0L7currentS883;
      int32_t _M0L1iS884;
      if (
        _M0L6_2atmpS2478 < 0
        || _M0L6_2atmpS2478 >= Moonbit_array_length(_M0L6resultS864)
      ) {
        #line 594 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS864[_M0L6_2atmpS2478] = 48;
      _M0L6_2atmpS2479 = _M0Lm5indexS865;
      _M0Lm5indexS865 = _M0L6_2atmpS2479 + 1;
      _M0L6_2atmpS2480 = _M0Lm5indexS865;
      if (
        _M0L6_2atmpS2480 < 0
        || _M0L6_2atmpS2480 >= Moonbit_array_length(_M0L6resultS864)
      ) {
        #line 596 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6resultS864[_M0L6_2atmpS2480] = 46;
      _M0L6_2atmpS2481 = _M0Lm5indexS865;
      _M0Lm5indexS865 = _M0L6_2atmpS2481 + 1;
      _M0L1iS881 = -1;
      while (1) {
        int32_t _M0L6_2atmpS2482 = _M0Lm3expS870;
        if (_M0L1iS881 > _M0L6_2atmpS2482) {
          int32_t _M0L6_2atmpS2483 = _M0Lm5indexS865;
          int32_t _M0L6_2atmpS2484;
          int32_t _M0L6_2atmpS2485;
          if (
            _M0L6_2atmpS2483 < 0
            || _M0L6_2atmpS2483 >= Moonbit_array_length(_M0L6resultS864)
          ) {
            #line 599 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS864[_M0L6_2atmpS2483] = 48;
          _M0L6_2atmpS2484 = _M0Lm5indexS865;
          _M0Lm5indexS865 = _M0L6_2atmpS2484 + 1;
          _M0L6_2atmpS2485 = _M0L1iS881 - 1;
          _M0L1iS881 = _M0L6_2atmpS2485;
          continue;
        }
        break;
      }
      _M0L7currentS883 = _M0Lm5indexS865;
      _M0L1iS884 = 0;
      while (1) {
        if (_M0L1iS884 < _M0L7olengthS869) {
          int32_t _M0L6_2atmpS2493 = _M0L7currentS883 + _M0L7olengthS869;
          int32_t _M0L6_2atmpS2492 = _M0L6_2atmpS2493 - _M0L1iS884;
          int32_t _M0L6_2atmpS2486 = _M0L6_2atmpS2492 - 1;
          uint64_t _M0L6_2atmpS2491 = _M0Lm6outputS867;
          uint64_t _M0L6_2atmpS2490 = _M0L6_2atmpS2491 % 10ull;
          int32_t _M0L6_2atmpS2489 = (int32_t)_M0L6_2atmpS2490;
          int32_t _M0L6_2atmpS2488 = 48 + _M0L6_2atmpS2489;
          int32_t _M0L6_2atmpS2487 = _M0L6_2atmpS2488 & 0xff;
          uint64_t _M0L6_2atmpS2494;
          int32_t _M0L6_2atmpS2495;
          int32_t _M0L6_2atmpS2496;
          if (
            _M0L6_2atmpS2486 < 0
            || _M0L6_2atmpS2486 >= Moonbit_array_length(_M0L6resultS864)
          ) {
            #line 604 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
            moonbit_panic();
          }
          _M0L6resultS864[_M0L6_2atmpS2486] = _M0L6_2atmpS2487;
          _M0L6_2atmpS2494 = _M0Lm6outputS867;
          _M0Lm6outputS867 = _M0L6_2atmpS2494 / 10ull;
          _M0L6_2atmpS2495 = _M0Lm5indexS865;
          _M0Lm5indexS865 = _M0L6_2atmpS2495 + 1;
          _M0L6_2atmpS2496 = _M0L1iS884 + 1;
          _M0L1iS884 = _M0L6_2atmpS2496;
          continue;
        }
        break;
      }
    } else {
      int32_t _M0L6_2atmpS2498 = _M0Lm3expS870;
      int32_t _M0L6_2atmpS2497 = _M0L6_2atmpS2498 + 1;
      if (_M0L6_2atmpS2497 >= _M0L7olengthS869) {
        int32_t _M0L1iS886 = 0;
        int32_t _M0L6_2atmpS2510;
        int32_t _M0L6_2atmpS2514;
        int32_t _M0L7_2abindS888;
        int32_t _M0L2__S889;
        while (1) {
          if (_M0L1iS886 < _M0L7olengthS869) {
            int32_t _M0L6_2atmpS2507 = _M0Lm5indexS865;
            int32_t _M0L6_2atmpS2506 = _M0L6_2atmpS2507 + _M0L7olengthS869;
            int32_t _M0L6_2atmpS2505 = _M0L6_2atmpS2506 - _M0L1iS886;
            int32_t _M0L6_2atmpS2499 = _M0L6_2atmpS2505 - 1;
            uint64_t _M0L6_2atmpS2504 = _M0Lm6outputS867;
            uint64_t _M0L6_2atmpS2503 = _M0L6_2atmpS2504 % 10ull;
            int32_t _M0L6_2atmpS2502 = (int32_t)_M0L6_2atmpS2503;
            int32_t _M0L6_2atmpS2501 = 48 + _M0L6_2atmpS2502;
            int32_t _M0L6_2atmpS2500 = _M0L6_2atmpS2501 & 0xff;
            uint64_t _M0L6_2atmpS2508;
            int32_t _M0L6_2atmpS2509;
            if (
              _M0L6_2atmpS2499 < 0
              || _M0L6_2atmpS2499 >= Moonbit_array_length(_M0L6resultS864)
            ) {
              #line 611 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS864[_M0L6_2atmpS2499] = _M0L6_2atmpS2500;
            _M0L6_2atmpS2508 = _M0Lm6outputS867;
            _M0Lm6outputS867 = _M0L6_2atmpS2508 / 10ull;
            _M0L6_2atmpS2509 = _M0L1iS886 + 1;
            _M0L1iS886 = _M0L6_2atmpS2509;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2510 = _M0Lm5indexS865;
        _M0Lm5indexS865 = _M0L6_2atmpS2510 + _M0L7olengthS869;
        _M0L6_2atmpS2514 = _M0Lm3expS870;
        _M0L7_2abindS888 = _M0L6_2atmpS2514 + 1;
        _M0L2__S889 = _M0L7olengthS869;
        while (1) {
          if (_M0L2__S889 < _M0L7_2abindS888) {
            int32_t _M0L6_2atmpS2511 = _M0Lm5indexS865;
            int32_t _M0L6_2atmpS2512;
            int32_t _M0L6_2atmpS2513;
            if (
              _M0L6_2atmpS2511 < 0
              || _M0L6_2atmpS2511 >= Moonbit_array_length(_M0L6resultS864)
            ) {
              #line 616 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS864[_M0L6_2atmpS2511] = 48;
            _M0L6_2atmpS2512 = _M0Lm5indexS865;
            _M0Lm5indexS865 = _M0L6_2atmpS2512 + 1;
            _M0L6_2atmpS2513 = _M0L2__S889 + 1;
            _M0L2__S889 = _M0L6_2atmpS2513;
            continue;
          }
          break;
        }
      } else {
        int32_t _M0L6_2atmpS2536 = _M0Lm5indexS865;
        int32_t _M0Lm7currentS891 = _M0L6_2atmpS2536 + 1;
        int32_t _M0L1iS892 = 0;
        int32_t _M0L6_2atmpS2534;
        int32_t _M0L6_2atmpS2535;
        while (1) {
          if (_M0L1iS892 < _M0L7olengthS869) {
            int32_t _M0L6_2atmpS2517 = _M0L7olengthS869 - _M0L1iS892;
            int32_t _M0L6_2atmpS2515 = _M0L6_2atmpS2517 - 1;
            int32_t _M0L6_2atmpS2516 = _M0Lm3expS870;
            int32_t _M0L6_2atmpS2531;
            int32_t _M0L6_2atmpS2530;
            int32_t _M0L6_2atmpS2529;
            int32_t _M0L6_2atmpS2523;
            uint64_t _M0L6_2atmpS2528;
            uint64_t _M0L6_2atmpS2527;
            int32_t _M0L6_2atmpS2526;
            int32_t _M0L6_2atmpS2525;
            int32_t _M0L6_2atmpS2524;
            uint64_t _M0L6_2atmpS2532;
            int32_t _M0L6_2atmpS2533;
            if (_M0L6_2atmpS2515 == _M0L6_2atmpS2516) {
              int32_t _M0L6_2atmpS2521 = _M0Lm7currentS891;
              int32_t _M0L6_2atmpS2520 = _M0L6_2atmpS2521 + _M0L7olengthS869;
              int32_t _M0L6_2atmpS2519 = _M0L6_2atmpS2520 - _M0L1iS892;
              int32_t _M0L6_2atmpS2518 = _M0L6_2atmpS2519 - 1;
              int32_t _M0L6_2atmpS2522;
              if (
                _M0L6_2atmpS2518 < 0
                || _M0L6_2atmpS2518 >= Moonbit_array_length(_M0L6resultS864)
              ) {
                #line 624 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
                moonbit_panic();
              }
              _M0L6resultS864[_M0L6_2atmpS2518] = 46;
              _M0L6_2atmpS2522 = _M0Lm7currentS891;
              _M0Lm7currentS891 = _M0L6_2atmpS2522 - 1;
            }
            _M0L6_2atmpS2531 = _M0Lm7currentS891;
            _M0L6_2atmpS2530 = _M0L6_2atmpS2531 + _M0L7olengthS869;
            _M0L6_2atmpS2529 = _M0L6_2atmpS2530 - _M0L1iS892;
            _M0L6_2atmpS2523 = _M0L6_2atmpS2529 - 1;
            _M0L6_2atmpS2528 = _M0Lm6outputS867;
            _M0L6_2atmpS2527 = _M0L6_2atmpS2528 % 10ull;
            _M0L6_2atmpS2526 = (int32_t)_M0L6_2atmpS2527;
            _M0L6_2atmpS2525 = 48 + _M0L6_2atmpS2526;
            _M0L6_2atmpS2524 = _M0L6_2atmpS2525 & 0xff;
            if (
              _M0L6_2atmpS2523 < 0
              || _M0L6_2atmpS2523 >= Moonbit_array_length(_M0L6resultS864)
            ) {
              #line 627 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
              moonbit_panic();
            }
            _M0L6resultS864[_M0L6_2atmpS2523] = _M0L6_2atmpS2524;
            _M0L6_2atmpS2532 = _M0Lm6outputS867;
            _M0Lm6outputS867 = _M0L6_2atmpS2532 / 10ull;
            _M0L6_2atmpS2533 = _M0L1iS892 + 1;
            _M0L1iS892 = _M0L6_2atmpS2533;
            continue;
          }
          break;
        }
        _M0L6_2atmpS2534 = _M0Lm5indexS865;
        _M0L6_2atmpS2535 = _M0L7olengthS869 + 1;
        _M0Lm5indexS865 = _M0L6_2atmpS2534 + _M0L6_2atmpS2535;
      }
    }
    _M0L6_2atmpS2537 = _M0Lm5indexS865;
    #line 632 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    return _M0FPB19string__from__bytes(_M0L6resultS864, 0, _M0L6_2atmpS2537);
  }
}

struct _M0TPB17FloatingDecimal64* _M0FPB3d2d(
  uint64_t _M0L12ieeeMantissaS810,
  uint32_t _M0L12ieeeExponentS809
) {
  int32_t _M0Lm2e2S807;
  uint64_t _M0Lm2m2S808;
  uint64_t _M0L6_2atmpS2412;
  uint64_t _M0L6_2atmpS2411;
  int32_t _M0L4evenS811;
  uint64_t _M0L6_2atmpS2410;
  uint64_t _M0L2mvS812;
  int32_t _M0L7mmShiftS813;
  uint64_t _M0Lm2vrS814;
  uint64_t _M0Lm2vpS815;
  uint64_t _M0Lm2vmS816;
  int32_t _M0Lm3e10S817;
  int32_t _M0Lm17vmIsTrailingZerosS818;
  int32_t _M0Lm17vrIsTrailingZerosS819;
  int32_t _M0L6_2atmpS2312;
  int32_t _M0Lm7removedS838;
  int32_t _M0Lm16lastRemovedDigitS839;
  uint64_t _M0Lm6outputS840;
  int32_t _M0L6_2atmpS2408;
  int32_t _M0L6_2atmpS2409;
  int32_t _M0L3expS863;
  uint64_t _M0L6_2atmpS2407;
  struct _M0TPB17FloatingDecimal64* _block_4294;
  #line 348 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0Lm2e2S807 = 0;
  _M0Lm2m2S808 = 0ull;
  if (_M0L12ieeeExponentS809 == 0u) {
    _M0Lm2e2S807 = -1076;
    _M0Lm2m2S808 = _M0L12ieeeMantissaS810;
  } else {
    int32_t _M0L6_2atmpS2311 = *(int32_t*)&_M0L12ieeeExponentS809;
    int32_t _M0L6_2atmpS2310 = _M0L6_2atmpS2311 - 1023;
    int32_t _M0L6_2atmpS2309 = _M0L6_2atmpS2310 - 52;
    _M0Lm2e2S807 = _M0L6_2atmpS2309 - 2;
    _M0Lm2m2S808 = 4503599627370496ull | _M0L12ieeeMantissaS810;
  }
  _M0L6_2atmpS2412 = _M0Lm2m2S808;
  _M0L6_2atmpS2411 = _M0L6_2atmpS2412 & 1ull;
  _M0L4evenS811 = _M0L6_2atmpS2411 == 0ull;
  _M0L6_2atmpS2410 = _M0Lm2m2S808;
  _M0L2mvS812 = 4ull * _M0L6_2atmpS2410;
  if (_M0L12ieeeMantissaS810 != 0ull) {
    _M0L7mmShiftS813 = 1;
  } else {
    _M0L7mmShiftS813 = _M0L12ieeeExponentS809 <= 1u;
  }
  _M0Lm2vrS814 = 0ull;
  _M0Lm2vpS815 = 0ull;
  _M0Lm2vmS816 = 0ull;
  _M0Lm3e10S817 = 0;
  _M0Lm17vmIsTrailingZerosS818 = 0;
  _M0Lm17vrIsTrailingZerosS819 = 0;
  _M0L6_2atmpS2312 = _M0Lm2e2S807;
  if (_M0L6_2atmpS2312 >= 0) {
    int32_t _M0L6_2atmpS2334 = _M0Lm2e2S807;
    int32_t _M0L6_2atmpS2330;
    int32_t _M0L6_2atmpS2333;
    int32_t _M0L6_2atmpS2332;
    int32_t _M0L6_2atmpS2331;
    int32_t _M0L1qS820;
    int32_t _M0L6_2atmpS2329;
    int32_t _M0L6_2atmpS2328;
    int32_t _M0L1kS821;
    int32_t _M0L6_2atmpS2327;
    int32_t _M0L6_2atmpS2326;
    int32_t _M0L6_2atmpS2325;
    int32_t _M0L1iS822;
    struct _M0TPB8Pow5Pair _M0L4pow5S823;
    uint64_t _M0L6_2atmpS2324;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS824;
    uint64_t _M0L8_2avrOutS825;
    uint64_t _M0L8_2avpOutS826;
    uint64_t _M0L8_2avmOutS827;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2330 = _M0FPB9log10Pow2(_M0L6_2atmpS2334);
    _M0L6_2atmpS2333 = _M0Lm2e2S807;
    _M0L6_2atmpS2332 = _M0L6_2atmpS2333 > 3;
    #line 384 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2331 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2332);
    _M0L1qS820 = _M0L6_2atmpS2330 - _M0L6_2atmpS2331;
    _M0Lm3e10S817 = _M0L1qS820;
    #line 386 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2329 = _M0FPB8pow5bits(_M0L1qS820);
    _M0L6_2atmpS2328 = 125 + _M0L6_2atmpS2329;
    _M0L1kS821 = _M0L6_2atmpS2328 - 1;
    _M0L6_2atmpS2327 = _M0Lm2e2S807;
    _M0L6_2atmpS2326 = -_M0L6_2atmpS2327;
    _M0L6_2atmpS2325 = _M0L6_2atmpS2326 + _M0L1qS820;
    _M0L1iS822 = _M0L6_2atmpS2325 + _M0L1kS821;
    #line 388 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S823 = _M0FPB22double__computeInvPow5(_M0L1qS820);
    _M0L6_2atmpS2324 = _M0Lm2m2S808;
    #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS824
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2324, _M0L4pow5S823, _M0L1iS822, _M0L7mmShiftS813);
    _M0L8_2avrOutS825 = _M0L7_2abindS824.$0;
    _M0L8_2avpOutS826 = _M0L7_2abindS824.$1;
    _M0L8_2avmOutS827 = _M0L7_2abindS824.$2;
    _M0Lm2vrS814 = _M0L8_2avrOutS825;
    _M0Lm2vpS815 = _M0L8_2avpOutS826;
    _M0Lm2vmS816 = _M0L8_2avmOutS827;
    if (_M0L1qS820 <= 21) {
      int32_t _M0L6_2atmpS2320 = (int32_t)_M0L2mvS812;
      uint64_t _M0L6_2atmpS2323 = _M0L2mvS812 / 5ull;
      int32_t _M0L6_2atmpS2322 = (int32_t)_M0L6_2atmpS2323;
      int32_t _M0L6_2atmpS2321 = 5 * _M0L6_2atmpS2322;
      int32_t _M0L6mvMod5S828 = _M0L6_2atmpS2320 - _M0L6_2atmpS2321;
      if (_M0L6mvMod5S828 == 0) {
        #line 401 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vrIsTrailingZerosS819
        = _M0FPB18multipleOfPowerOf5(_M0L2mvS812, _M0L1qS820);
      } else if (_M0L4evenS811) {
        uint64_t _M0L6_2atmpS2314 = _M0L2mvS812 - 1ull;
        uint64_t _M0L6_2atmpS2315;
        uint64_t _M0L6_2atmpS2313;
        #line 407 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2315 = _M0MPC14bool4Bool10to__uint64(_M0L7mmShiftS813);
        _M0L6_2atmpS2313 = _M0L6_2atmpS2314 - _M0L6_2atmpS2315;
        #line 406 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0Lm17vmIsTrailingZerosS818
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2313, _M0L1qS820);
      } else {
        uint64_t _M0L6_2atmpS2316 = _M0Lm2vpS815;
        uint64_t _M0L6_2atmpS2319 = _M0L2mvS812 + 2ull;
        int32_t _M0L6_2atmpS2318;
        uint64_t _M0L6_2atmpS2317;
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2318
        = _M0FPB18multipleOfPowerOf5(_M0L6_2atmpS2319, _M0L1qS820);
        #line 411 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2317 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2318);
        _M0Lm2vpS815 = _M0L6_2atmpS2316 - _M0L6_2atmpS2317;
      }
    }
  } else {
    int32_t _M0L6_2atmpS2348 = _M0Lm2e2S807;
    int32_t _M0L6_2atmpS2347 = -_M0L6_2atmpS2348;
    int32_t _M0L6_2atmpS2342;
    int32_t _M0L6_2atmpS2346;
    int32_t _M0L6_2atmpS2345;
    int32_t _M0L6_2atmpS2344;
    int32_t _M0L6_2atmpS2343;
    int32_t _M0L1qS829;
    int32_t _M0L6_2atmpS2335;
    int32_t _M0L6_2atmpS2341;
    int32_t _M0L6_2atmpS2340;
    int32_t _M0L1iS830;
    int32_t _M0L6_2atmpS2339;
    int32_t _M0L1kS831;
    int32_t _M0L1jS832;
    struct _M0TPB8Pow5Pair _M0L4pow5S833;
    uint64_t _M0L6_2atmpS2338;
    struct _M0TPB19MulShiftAll64Result _M0L7_2abindS834;
    uint64_t _M0L8_2avrOutS835;
    uint64_t _M0L8_2avpOutS836;
    uint64_t _M0L8_2avmOutS837;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2342 = _M0FPB9log10Pow5(_M0L6_2atmpS2347);
    _M0L6_2atmpS2346 = _M0Lm2e2S807;
    _M0L6_2atmpS2345 = -_M0L6_2atmpS2346;
    _M0L6_2atmpS2344 = _M0L6_2atmpS2345 > 1;
    #line 416 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2343 = _M0MPC14bool4Bool7to__int(_M0L6_2atmpS2344);
    _M0L1qS829 = _M0L6_2atmpS2342 - _M0L6_2atmpS2343;
    _M0L6_2atmpS2335 = _M0Lm2e2S807;
    _M0Lm3e10S817 = _M0L1qS829 + _M0L6_2atmpS2335;
    _M0L6_2atmpS2341 = _M0Lm2e2S807;
    _M0L6_2atmpS2340 = -_M0L6_2atmpS2341;
    _M0L1iS830 = _M0L6_2atmpS2340 - _M0L1qS829;
    #line 419 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2339 = _M0FPB8pow5bits(_M0L1iS830);
    _M0L1kS831 = _M0L6_2atmpS2339 - 125;
    _M0L1jS832 = _M0L1qS829 - _M0L1kS831;
    #line 421 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L4pow5S833 = _M0FPB19double__computePow5(_M0L1iS830);
    _M0L6_2atmpS2338 = _M0Lm2m2S808;
    #line 422 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L7_2abindS834
    = _M0FPB13mulShiftAll64(_M0L6_2atmpS2338, _M0L4pow5S833, _M0L1jS832, _M0L7mmShiftS813);
    _M0L8_2avrOutS835 = _M0L7_2abindS834.$0;
    _M0L8_2avpOutS836 = _M0L7_2abindS834.$1;
    _M0L8_2avmOutS837 = _M0L7_2abindS834.$2;
    _M0Lm2vrS814 = _M0L8_2avrOutS835;
    _M0Lm2vpS815 = _M0L8_2avpOutS836;
    _M0Lm2vmS816 = _M0L8_2avmOutS837;
    if (_M0L1qS829 <= 1) {
      _M0Lm17vrIsTrailingZerosS819 = 1;
      if (_M0L4evenS811) {
        int32_t _M0L6_2atmpS2336;
        #line 433 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        _M0L6_2atmpS2336 = _M0MPC14bool4Bool7to__int(_M0L7mmShiftS813);
        _M0Lm17vmIsTrailingZerosS818 = _M0L6_2atmpS2336 == 1;
      } else {
        uint64_t _M0L6_2atmpS2337 = _M0Lm2vpS815;
        _M0Lm2vpS815 = _M0L6_2atmpS2337 - 1ull;
      }
    } else if (_M0L1qS829 < 63) {
      #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0Lm17vrIsTrailingZerosS819
      = _M0FPB18multipleOfPowerOf2(_M0L2mvS812, _M0L1qS829);
    }
  }
  _M0Lm7removedS838 = 0;
  _M0Lm16lastRemovedDigitS839 = 0;
  _M0Lm6outputS840 = 0ull;
  if (_M0Lm17vmIsTrailingZerosS818 || _M0Lm17vrIsTrailingZerosS819) {
    int32_t _if__result_4291;
    uint64_t _M0L6_2atmpS2378;
    uint64_t _M0L6_2atmpS2384;
    uint64_t _M0L6_2atmpS2385;
    int32_t _if__result_4292;
    int32_t _M0L6_2atmpS2381;
    int64_t _M0L6_2atmpS2380;
    uint64_t _M0L6_2atmpS2379;
    while (1) {
      uint64_t _M0L6_2atmpS2361 = _M0Lm2vpS815;
      uint64_t _M0L7vpDiv10S841 = _M0L6_2atmpS2361 / 10ull;
      uint64_t _M0L6_2atmpS2360 = _M0Lm2vmS816;
      uint64_t _M0L7vmDiv10S842 = _M0L6_2atmpS2360 / 10ull;
      uint64_t _M0L6_2atmpS2359;
      int32_t _M0L6_2atmpS2356;
      int32_t _M0L6_2atmpS2358;
      int32_t _M0L6_2atmpS2357;
      int32_t _M0L7vmMod10S844;
      uint64_t _M0L6_2atmpS2355;
      uint64_t _M0L7vrDiv10S845;
      uint64_t _M0L6_2atmpS2354;
      int32_t _M0L6_2atmpS2351;
      int32_t _M0L6_2atmpS2353;
      int32_t _M0L6_2atmpS2352;
      int32_t _M0L7vrMod10S846;
      int32_t _M0L6_2atmpS2350;
      if (_M0L7vpDiv10S841 <= _M0L7vmDiv10S842) {
        break;
      }
      _M0L6_2atmpS2359 = _M0Lm2vmS816;
      _M0L6_2atmpS2356 = (int32_t)_M0L6_2atmpS2359;
      _M0L6_2atmpS2358 = (int32_t)_M0L7vmDiv10S842;
      _M0L6_2atmpS2357 = 10 * _M0L6_2atmpS2358;
      _M0L7vmMod10S844 = _M0L6_2atmpS2356 - _M0L6_2atmpS2357;
      _M0L6_2atmpS2355 = _M0Lm2vrS814;
      _M0L7vrDiv10S845 = _M0L6_2atmpS2355 / 10ull;
      _M0L6_2atmpS2354 = _M0Lm2vrS814;
      _M0L6_2atmpS2351 = (int32_t)_M0L6_2atmpS2354;
      _M0L6_2atmpS2353 = (int32_t)_M0L7vrDiv10S845;
      _M0L6_2atmpS2352 = 10 * _M0L6_2atmpS2353;
      _M0L7vrMod10S846 = _M0L6_2atmpS2351 - _M0L6_2atmpS2352;
      if (_M0Lm17vmIsTrailingZerosS818) {
        _M0Lm17vmIsTrailingZerosS818 = _M0L7vmMod10S844 == 0;
      } else {
        _M0Lm17vmIsTrailingZerosS818 = 0;
      }
      if (_M0Lm17vrIsTrailingZerosS819) {
        int32_t _M0L6_2atmpS2349 = _M0Lm16lastRemovedDigitS839;
        _M0Lm17vrIsTrailingZerosS819 = _M0L6_2atmpS2349 == 0;
      } else {
        _M0Lm17vrIsTrailingZerosS819 = 0;
      }
      _M0Lm16lastRemovedDigitS839 = _M0L7vrMod10S846;
      _M0Lm2vrS814 = _M0L7vrDiv10S845;
      _M0Lm2vpS815 = _M0L7vpDiv10S841;
      _M0Lm2vmS816 = _M0L7vmDiv10S842;
      _M0L6_2atmpS2350 = _M0Lm7removedS838;
      _M0Lm7removedS838 = _M0L6_2atmpS2350 + 1;
      continue;
      break;
    }
    if (_M0Lm17vmIsTrailingZerosS818) {
      while (1) {
        uint64_t _M0L6_2atmpS2374 = _M0Lm2vmS816;
        uint64_t _M0L7vmDiv10S847 = _M0L6_2atmpS2374 / 10ull;
        uint64_t _M0L6_2atmpS2373 = _M0Lm2vmS816;
        int32_t _M0L6_2atmpS2370 = (int32_t)_M0L6_2atmpS2373;
        int32_t _M0L6_2atmpS2372 = (int32_t)_M0L7vmDiv10S847;
        int32_t _M0L6_2atmpS2371 = 10 * _M0L6_2atmpS2372;
        int32_t _M0L7vmMod10S848 = _M0L6_2atmpS2370 - _M0L6_2atmpS2371;
        uint64_t _M0L6_2atmpS2369;
        uint64_t _M0L7vpDiv10S850;
        uint64_t _M0L6_2atmpS2368;
        uint64_t _M0L7vrDiv10S851;
        uint64_t _M0L6_2atmpS2367;
        int32_t _M0L6_2atmpS2364;
        int32_t _M0L6_2atmpS2366;
        int32_t _M0L6_2atmpS2365;
        int32_t _M0L7vrMod10S852;
        int32_t _M0L6_2atmpS2363;
        if (_M0L7vmMod10S848 != 0) {
          break;
        }
        _M0L6_2atmpS2369 = _M0Lm2vpS815;
        _M0L7vpDiv10S850 = _M0L6_2atmpS2369 / 10ull;
        _M0L6_2atmpS2368 = _M0Lm2vrS814;
        _M0L7vrDiv10S851 = _M0L6_2atmpS2368 / 10ull;
        _M0L6_2atmpS2367 = _M0Lm2vrS814;
        _M0L6_2atmpS2364 = (int32_t)_M0L6_2atmpS2367;
        _M0L6_2atmpS2366 = (int32_t)_M0L7vrDiv10S851;
        _M0L6_2atmpS2365 = 10 * _M0L6_2atmpS2366;
        _M0L7vrMod10S852 = _M0L6_2atmpS2364 - _M0L6_2atmpS2365;
        if (_M0Lm17vrIsTrailingZerosS819) {
          int32_t _M0L6_2atmpS2362 = _M0Lm16lastRemovedDigitS839;
          _M0Lm17vrIsTrailingZerosS819 = _M0L6_2atmpS2362 == 0;
        } else {
          _M0Lm17vrIsTrailingZerosS819 = 0;
        }
        _M0Lm16lastRemovedDigitS839 = _M0L7vrMod10S852;
        _M0Lm2vrS814 = _M0L7vrDiv10S851;
        _M0Lm2vpS815 = _M0L7vpDiv10S850;
        _M0Lm2vmS816 = _M0L7vmDiv10S847;
        _M0L6_2atmpS2363 = _M0Lm7removedS838;
        _M0Lm7removedS838 = _M0L6_2atmpS2363 + 1;
        continue;
        break;
      }
    }
    if (_M0Lm17vrIsTrailingZerosS819) {
      int32_t _M0L6_2atmpS2377 = _M0Lm16lastRemovedDigitS839;
      if (_M0L6_2atmpS2377 == 5) {
        uint64_t _M0L6_2atmpS2376 = _M0Lm2vrS814;
        uint64_t _M0L6_2atmpS2375 = _M0L6_2atmpS2376 % 2ull;
        _if__result_4291 = _M0L6_2atmpS2375 == 0ull;
      } else {
        _if__result_4291 = 0;
      }
    } else {
      _if__result_4291 = 0;
    }
    if (_if__result_4291) {
      _M0Lm16lastRemovedDigitS839 = 4;
    }
    _M0L6_2atmpS2378 = _M0Lm2vrS814;
    _M0L6_2atmpS2384 = _M0Lm2vrS814;
    _M0L6_2atmpS2385 = _M0Lm2vmS816;
    if (_M0L6_2atmpS2384 == _M0L6_2atmpS2385) {
      if (!_M0L4evenS811) {
        _if__result_4292 = 1;
      } else {
        int32_t _M0L6_2atmpS2383 = _M0Lm17vmIsTrailingZerosS818;
        _if__result_4292 = !_M0L6_2atmpS2383;
      }
    } else {
      _if__result_4292 = 0;
    }
    if (_if__result_4292) {
      _M0L6_2atmpS2381 = 1;
    } else {
      int32_t _M0L6_2atmpS2382 = _M0Lm16lastRemovedDigitS839;
      _M0L6_2atmpS2381 = _M0L6_2atmpS2382 >= 5;
    }
    #line 488 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2380 = _M0MPC14bool4Bool9to__int64(_M0L6_2atmpS2381);
    _M0L6_2atmpS2379 = *(uint64_t*)&_M0L6_2atmpS2380;
    _M0Lm6outputS840 = _M0L6_2atmpS2378 + _M0L6_2atmpS2379;
  } else {
    int32_t _M0Lm7roundUpS853 = 0;
    uint64_t _M0L6_2atmpS2406 = _M0Lm2vpS815;
    uint64_t _M0L8vpDiv100S854 = _M0L6_2atmpS2406 / 100ull;
    uint64_t _M0L6_2atmpS2405 = _M0Lm2vmS816;
    uint64_t _M0L8vmDiv100S855 = _M0L6_2atmpS2405 / 100ull;
    uint64_t _M0L6_2atmpS2400;
    uint64_t _M0L6_2atmpS2403;
    uint64_t _M0L6_2atmpS2404;
    int32_t _M0L6_2atmpS2402;
    uint64_t _M0L6_2atmpS2401;
    if (_M0L8vpDiv100S854 > _M0L8vmDiv100S855) {
      uint64_t _M0L6_2atmpS2391 = _M0Lm2vrS814;
      uint64_t _M0L8vrDiv100S856 = _M0L6_2atmpS2391 / 100ull;
      uint64_t _M0L6_2atmpS2390 = _M0Lm2vrS814;
      int32_t _M0L6_2atmpS2387 = (int32_t)_M0L6_2atmpS2390;
      int32_t _M0L6_2atmpS2389 = (int32_t)_M0L8vrDiv100S856;
      int32_t _M0L6_2atmpS2388 = 100 * _M0L6_2atmpS2389;
      int32_t _M0L8vrMod100S857 = _M0L6_2atmpS2387 - _M0L6_2atmpS2388;
      int32_t _M0L6_2atmpS2386;
      _M0Lm7roundUpS853 = _M0L8vrMod100S857 >= 50;
      _M0Lm2vrS814 = _M0L8vrDiv100S856;
      _M0Lm2vpS815 = _M0L8vpDiv100S854;
      _M0Lm2vmS816 = _M0L8vmDiv100S855;
      _M0L6_2atmpS2386 = _M0Lm7removedS838;
      _M0Lm7removedS838 = _M0L6_2atmpS2386 + 2;
    }
    while (1) {
      uint64_t _M0L6_2atmpS2399 = _M0Lm2vpS815;
      uint64_t _M0L7vpDiv10S858 = _M0L6_2atmpS2399 / 10ull;
      uint64_t _M0L6_2atmpS2398 = _M0Lm2vmS816;
      uint64_t _M0L7vmDiv10S859 = _M0L6_2atmpS2398 / 10ull;
      uint64_t _M0L6_2atmpS2397;
      uint64_t _M0L7vrDiv10S861;
      uint64_t _M0L6_2atmpS2396;
      int32_t _M0L6_2atmpS2393;
      int32_t _M0L6_2atmpS2395;
      int32_t _M0L6_2atmpS2394;
      int32_t _M0L7vrMod10S862;
      int32_t _M0L6_2atmpS2392;
      if (_M0L7vpDiv10S858 <= _M0L7vmDiv10S859) {
        break;
      }
      _M0L6_2atmpS2397 = _M0Lm2vrS814;
      _M0L7vrDiv10S861 = _M0L6_2atmpS2397 / 10ull;
      _M0L6_2atmpS2396 = _M0Lm2vrS814;
      _M0L6_2atmpS2393 = (int32_t)_M0L6_2atmpS2396;
      _M0L6_2atmpS2395 = (int32_t)_M0L7vrDiv10S861;
      _M0L6_2atmpS2394 = 10 * _M0L6_2atmpS2395;
      _M0L7vrMod10S862 = _M0L6_2atmpS2393 - _M0L6_2atmpS2394;
      _M0Lm7roundUpS853 = _M0L7vrMod10S862 >= 5;
      _M0Lm2vrS814 = _M0L7vrDiv10S861;
      _M0Lm2vpS815 = _M0L7vpDiv10S858;
      _M0Lm2vmS816 = _M0L7vmDiv10S859;
      _M0L6_2atmpS2392 = _M0Lm7removedS838;
      _M0Lm7removedS838 = _M0L6_2atmpS2392 + 1;
      continue;
      break;
    }
    _M0L6_2atmpS2400 = _M0Lm2vrS814;
    _M0L6_2atmpS2403 = _M0Lm2vrS814;
    _M0L6_2atmpS2404 = _M0Lm2vmS816;
    _M0L6_2atmpS2402
    = _M0L6_2atmpS2403 == _M0L6_2atmpS2404 || _M0Lm7roundUpS853;
    #line 523 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS2401 = _M0MPC14bool4Bool10to__uint64(_M0L6_2atmpS2402);
    _M0Lm6outputS840 = _M0L6_2atmpS2400 + _M0L6_2atmpS2401;
  }
  _M0L6_2atmpS2408 = _M0Lm3e10S817;
  _M0L6_2atmpS2409 = _M0Lm7removedS838;
  _M0L3expS863 = _M0L6_2atmpS2408 + _M0L6_2atmpS2409;
  _M0L6_2atmpS2407 = _M0Lm6outputS840;
  _block_4294
  = (struct _M0TPB17FloatingDecimal64*)moonbit_malloc(sizeof(struct _M0TPB17FloatingDecimal64));
  Moonbit_object_header(_block_4294)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB17FloatingDecimal64) >> 2, 0, 0);
  _block_4294->$0 = _M0L6_2atmpS2407;
  _block_4294->$1 = _M0L3expS863;
  return _block_4294;
}

uint64_t _M0MPC14bool4Bool10to__uint64(int32_t _M0L4selfS806) {
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS806) {
    return 1ull;
  } else {
    return 0ull;
  }
}

int64_t _M0MPC14bool4Bool9to__int64(int32_t _M0L4selfS805) {
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS805) {
    return 1ll;
  } else {
    return 0ll;
  }
}

int32_t _M0MPC14bool4Bool7to__int(int32_t _M0L4selfS804) {
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bool.mbt"
  if (_M0L4selfS804) {
    return 1;
  } else {
    return 0;
  }
}

int32_t _M0FPB17decimal__length17(uint64_t _M0L1vS803) {
  #line 281 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L1vS803 >= 10000000000000000ull) {
    return 17;
  }
  if (_M0L1vS803 >= 1000000000000000ull) {
    return 16;
  }
  if (_M0L1vS803 >= 100000000000000ull) {
    return 15;
  }
  if (_M0L1vS803 >= 10000000000000ull) {
    return 14;
  }
  if (_M0L1vS803 >= 1000000000000ull) {
    return 13;
  }
  if (_M0L1vS803 >= 100000000000ull) {
    return 12;
  }
  if (_M0L1vS803 >= 10000000000ull) {
    return 11;
  }
  if (_M0L1vS803 >= 1000000000ull) {
    return 10;
  }
  if (_M0L1vS803 >= 100000000ull) {
    return 9;
  }
  if (_M0L1vS803 >= 10000000ull) {
    return 8;
  }
  if (_M0L1vS803 >= 1000000ull) {
    return 7;
  }
  if (_M0L1vS803 >= 100000ull) {
    return 6;
  }
  if (_M0L1vS803 >= 10000ull) {
    return 5;
  }
  if (_M0L1vS803 >= 1000ull) {
    return 4;
  }
  if (_M0L1vS803 >= 100ull) {
    return 3;
  }
  if (_M0L1vS803 >= 10ull) {
    return 2;
  }
  return 1;
}

struct _M0TPB8Pow5Pair _M0FPB22double__computeInvPow5(int32_t _M0L1iS786) {
  int32_t _M0L6_2atmpS2308;
  int32_t _M0L6_2atmpS2307;
  int32_t _M0L4baseS785;
  int32_t _M0L5base2S787;
  int32_t _M0L6offsetS788;
  int32_t _M0L6_2atmpS2306;
  uint64_t _M0L4mul0S789;
  int32_t _M0L6_2atmpS2305;
  int32_t _M0L6_2atmpS2304;
  uint64_t _M0L4mul1S790;
  uint64_t _M0L1mS791;
  struct _M0TPB7Umul128 _M0L7_2abindS792;
  uint64_t _M0L7_2alow1S793;
  uint64_t _M0L8_2ahigh1S794;
  struct _M0TPB7Umul128 _M0L7_2abindS795;
  uint64_t _M0L7_2alow0S796;
  uint64_t _M0L8_2ahigh0S797;
  uint64_t _M0L3sumS798;
  uint64_t _M0Lm5high1S799;
  int32_t _M0L6_2atmpS2302;
  int32_t _M0L6_2atmpS2303;
  int32_t _M0L5deltaS800;
  uint64_t _M0L6_2atmpS2301;
  uint64_t _M0L6_2atmpS2293;
  int32_t _M0L6_2atmpS2300;
  uint32_t _M0L6_2atmpS2297;
  int32_t _M0L6_2atmpS2299;
  int32_t _M0L6_2atmpS2298;
  uint32_t _M0L6_2atmpS2296;
  uint32_t _M0L6_2atmpS2295;
  uint64_t _M0L6_2atmpS2294;
  uint64_t _M0L1aS801;
  uint64_t _M0L6_2atmpS2292;
  uint64_t _M0L1bS802;
  #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2308 = _M0L1iS786 + 26;
  _M0L6_2atmpS2307 = _M0L6_2atmpS2308 - 1;
  _M0L4baseS785 = _M0L6_2atmpS2307 / 26;
  _M0L5base2S787 = _M0L4baseS785 * 26;
  _M0L6offsetS788 = _M0L5base2S787 - _M0L1iS786;
  _M0L6_2atmpS2306 = _M0L4baseS785 * 2;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S789
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2306);
  _M0L6_2atmpS2305 = _M0L4baseS785 * 2;
  _M0L6_2atmpS2304 = _M0L6_2atmpS2305 + 1;
  moonbit_incref(_M0FPB26gDOUBLE__POW5__INV__SPLIT2);
  #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S790
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB26gDOUBLE__POW5__INV__SPLIT2, _M0L6_2atmpS2304);
  if (_M0L6offsetS788 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S789, _M0L4mul1S790};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS791
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS788);
  #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS792 = _M0FPB7umul128(_M0L1mS791, _M0L4mul1S790);
  _M0L7_2alow1S793 = _M0L7_2abindS792.$0;
  _M0L8_2ahigh1S794 = _M0L7_2abindS792.$1;
  #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS795 = _M0FPB7umul128(_M0L1mS791, _M0L4mul0S789);
  _M0L7_2alow0S796 = _M0L7_2abindS795.$0;
  _M0L8_2ahigh0S797 = _M0L7_2abindS795.$1;
  _M0L3sumS798 = _M0L8_2ahigh0S797 + _M0L7_2alow1S793;
  _M0Lm5high1S799 = _M0L8_2ahigh1S794;
  if (_M0L3sumS798 < _M0L8_2ahigh0S797) {
    uint64_t _M0L6_2atmpS2291 = _M0Lm5high1S799;
    _M0Lm5high1S799 = _M0L6_2atmpS2291 + 1ull;
  }
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2302 = _M0FPB8pow5bits(_M0L5base2S787);
  #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2303 = _M0FPB8pow5bits(_M0L1iS786);
  _M0L5deltaS800 = _M0L6_2atmpS2302 - _M0L6_2atmpS2303;
  #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2301
  = _M0FPB13shiftright128(_M0L7_2alow0S796, _M0L3sumS798, _M0L5deltaS800);
  _M0L6_2atmpS2293 = _M0L6_2atmpS2301 + 1ull;
  _M0L6_2atmpS2300 = _M0L1iS786 / 16;
  moonbit_incref(_M0FPB19gPOW5__INV__OFFSETS);
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2297
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB19gPOW5__INV__OFFSETS, _M0L6_2atmpS2300);
  _M0L6_2atmpS2299 = _M0L1iS786 % 16;
  _M0L6_2atmpS2298 = _M0L6_2atmpS2299 << 1;
  _M0L6_2atmpS2296 = _M0L6_2atmpS2297 >> (_M0L6_2atmpS2298 & 31);
  _M0L6_2atmpS2295 = _M0L6_2atmpS2296 & 3u;
  #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2294 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2295);
  _M0L1aS801 = _M0L6_2atmpS2293 + _M0L6_2atmpS2294;
  _M0L6_2atmpS2292 = _M0Lm5high1S799;
  #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS802
  = _M0FPB13shiftright128(_M0L3sumS798, _M0L6_2atmpS2292, _M0L5deltaS800);
  return (struct _M0TPB8Pow5Pair){_M0L1aS801, _M0L1bS802};
}

struct _M0TPB8Pow5Pair _M0FPB19double__computePow5(int32_t _M0L1iS768) {
  int32_t _M0L4baseS767;
  int32_t _M0L5base2S769;
  int32_t _M0L6offsetS770;
  int32_t _M0L6_2atmpS2290;
  uint64_t _M0L4mul0S771;
  int32_t _M0L6_2atmpS2289;
  int32_t _M0L6_2atmpS2288;
  uint64_t _M0L4mul1S772;
  uint64_t _M0L1mS773;
  struct _M0TPB7Umul128 _M0L7_2abindS774;
  uint64_t _M0L7_2alow1S775;
  uint64_t _M0L8_2ahigh1S776;
  struct _M0TPB7Umul128 _M0L7_2abindS777;
  uint64_t _M0L7_2alow0S778;
  uint64_t _M0L8_2ahigh0S779;
  uint64_t _M0L3sumS780;
  uint64_t _M0Lm5high1S781;
  int32_t _M0L6_2atmpS2286;
  int32_t _M0L6_2atmpS2287;
  int32_t _M0L5deltaS782;
  uint64_t _M0L6_2atmpS2278;
  int32_t _M0L6_2atmpS2285;
  uint32_t _M0L6_2atmpS2282;
  int32_t _M0L6_2atmpS2284;
  int32_t _M0L6_2atmpS2283;
  uint32_t _M0L6_2atmpS2281;
  uint32_t _M0L6_2atmpS2280;
  uint64_t _M0L6_2atmpS2279;
  uint64_t _M0L1aS783;
  uint64_t _M0L6_2atmpS2277;
  uint64_t _M0L1bS784;
  #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4baseS767 = _M0L1iS768 / 26;
  _M0L5base2S769 = _M0L4baseS767 * 26;
  _M0L6offsetS770 = _M0L1iS768 - _M0L5base2S769;
  _M0L6_2atmpS2290 = _M0L4baseS767 * 2;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul0S771
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2290);
  _M0L6_2atmpS2289 = _M0L4baseS767 * 2;
  _M0L6_2atmpS2288 = _M0L6_2atmpS2289 + 1;
  moonbit_incref(_M0FPB21gDOUBLE__POW5__SPLIT2);
  #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L4mul1S772
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB21gDOUBLE__POW5__SPLIT2, _M0L6_2atmpS2288);
  if (_M0L6offsetS770 == 0) {
    return (struct _M0TPB8Pow5Pair){_M0L4mul0S771, _M0L4mul1S772};
  }
  moonbit_incref(_M0FPB20gDOUBLE__POW5__TABLE);
  #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1mS773
  = _M0MPC15array13ReadOnlyArray2atGmE(_M0FPB20gDOUBLE__POW5__TABLE, _M0L6offsetS770);
  #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS774 = _M0FPB7umul128(_M0L1mS773, _M0L4mul1S772);
  _M0L7_2alow1S775 = _M0L7_2abindS774.$0;
  _M0L8_2ahigh1S776 = _M0L7_2abindS774.$1;
  #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS777 = _M0FPB7umul128(_M0L1mS773, _M0L4mul0S771);
  _M0L7_2alow0S778 = _M0L7_2abindS777.$0;
  _M0L8_2ahigh0S779 = _M0L7_2abindS777.$1;
  _M0L3sumS780 = _M0L8_2ahigh0S779 + _M0L7_2alow1S775;
  _M0Lm5high1S781 = _M0L8_2ahigh1S776;
  if (_M0L3sumS780 < _M0L8_2ahigh0S779) {
    uint64_t _M0L6_2atmpS2276 = _M0Lm5high1S781;
    _M0Lm5high1S781 = _M0L6_2atmpS2276 + 1ull;
  }
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2286 = _M0FPB8pow5bits(_M0L1iS768);
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2287 = _M0FPB8pow5bits(_M0L5base2S769);
  _M0L5deltaS782 = _M0L6_2atmpS2286 - _M0L6_2atmpS2287;
  #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2278
  = _M0FPB13shiftright128(_M0L7_2alow0S778, _M0L3sumS780, _M0L5deltaS782);
  _M0L6_2atmpS2285 = _M0L1iS768 / 16;
  moonbit_incref(_M0FPB14gPOW5__OFFSETS);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2282
  = _M0MPC15array13ReadOnlyArray2atGjE(_M0FPB14gPOW5__OFFSETS, _M0L6_2atmpS2285);
  _M0L6_2atmpS2284 = _M0L1iS768 % 16;
  _M0L6_2atmpS2283 = _M0L6_2atmpS2284 << 1;
  _M0L6_2atmpS2281 = _M0L6_2atmpS2282 >> (_M0L6_2atmpS2283 & 31);
  _M0L6_2atmpS2280 = _M0L6_2atmpS2281 & 3u;
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2279 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS2280);
  _M0L1aS783 = _M0L6_2atmpS2278 + _M0L6_2atmpS2279;
  _M0L6_2atmpS2277 = _M0Lm5high1S781;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L1bS784
  = _M0FPB13shiftright128(_M0L3sumS780, _M0L6_2atmpS2277, _M0L5deltaS782);
  return (struct _M0TPB8Pow5Pair){_M0L1aS783, _M0L1bS784};
}

struct _M0TPB19MulShiftAll64Result _M0FPB13mulShiftAll64(
  uint64_t _M0L1mS741,
  struct _M0TPB8Pow5Pair _M0L3mulS738,
  int32_t _M0L1jS754,
  int32_t _M0L7mmShiftS756
) {
  uint64_t _M0L7_2amul0S737;
  uint64_t _M0L7_2amul1S739;
  uint64_t _M0L1mS740;
  struct _M0TPB7Umul128 _M0L7_2abindS742;
  uint64_t _M0L5_2aloS743;
  uint64_t _M0L6_2atmpS744;
  struct _M0TPB7Umul128 _M0L7_2abindS745;
  uint64_t _M0L6_2alo2S746;
  uint64_t _M0L6_2ahi2S747;
  uint64_t _M0L3midS748;
  uint64_t _M0L6_2atmpS2275;
  uint64_t _M0L2hiS749;
  uint64_t _M0L3lo2S750;
  uint64_t _M0L6_2atmpS2273;
  uint64_t _M0L6_2atmpS2274;
  uint64_t _M0L4mid2S751;
  uint64_t _M0L6_2atmpS2272;
  uint64_t _M0L3hi2S752;
  int32_t _M0L6_2atmpS2271;
  int32_t _M0L6_2atmpS2270;
  uint64_t _M0L2vpS753;
  uint64_t _M0Lm2vmS755;
  int32_t _M0L6_2atmpS2269;
  int32_t _M0L6_2atmpS2268;
  uint64_t _M0L2vrS766;
  uint64_t _M0L6_2atmpS2267;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2amul0S737 = _M0L3mulS738.$0;
  _M0L7_2amul1S739 = _M0L3mulS738.$1;
  _M0L1mS740 = _M0L1mS741 << 1;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS742 = _M0FPB7umul128(_M0L1mS740, _M0L7_2amul0S737);
  _M0L5_2aloS743 = _M0L7_2abindS742.$0;
  _M0L6_2atmpS744 = _M0L7_2abindS742.$1;
  #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L7_2abindS745 = _M0FPB7umul128(_M0L1mS740, _M0L7_2amul1S739);
  _M0L6_2alo2S746 = _M0L7_2abindS745.$0;
  _M0L6_2ahi2S747 = _M0L7_2abindS745.$1;
  _M0L3midS748 = _M0L6_2atmpS744 + _M0L6_2alo2S746;
  if (_M0L3midS748 < _M0L6_2atmpS744) {
    _M0L6_2atmpS2275 = 1ull;
  } else {
    _M0L6_2atmpS2275 = 0ull;
  }
  _M0L2hiS749 = _M0L6_2ahi2S747 + _M0L6_2atmpS2275;
  _M0L3lo2S750 = _M0L5_2aloS743 + _M0L7_2amul0S737;
  _M0L6_2atmpS2273 = _M0L3midS748 + _M0L7_2amul1S739;
  if (_M0L3lo2S750 < _M0L5_2aloS743) {
    _M0L6_2atmpS2274 = 1ull;
  } else {
    _M0L6_2atmpS2274 = 0ull;
  }
  _M0L4mid2S751 = _M0L6_2atmpS2273 + _M0L6_2atmpS2274;
  if (_M0L4mid2S751 < _M0L3midS748) {
    _M0L6_2atmpS2272 = 1ull;
  } else {
    _M0L6_2atmpS2272 = 0ull;
  }
  _M0L3hi2S752 = _M0L2hiS749 + _M0L6_2atmpS2272;
  _M0L6_2atmpS2271 = _M0L1jS754 - 64;
  _M0L6_2atmpS2270 = _M0L6_2atmpS2271 - 1;
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vpS753
  = _M0FPB13shiftright128(_M0L4mid2S751, _M0L3hi2S752, _M0L6_2atmpS2270);
  _M0Lm2vmS755 = 0ull;
  if (_M0L7mmShiftS756) {
    uint64_t _M0L3lo3S757 = _M0L5_2aloS743 - _M0L7_2amul0S737;
    uint64_t _M0L6_2atmpS2257 = _M0L3midS748 - _M0L7_2amul1S739;
    uint64_t _M0L6_2atmpS2258;
    uint64_t _M0L4mid3S758;
    uint64_t _M0L6_2atmpS2256;
    uint64_t _M0L3hi3S759;
    int32_t _M0L6_2atmpS2255;
    int32_t _M0L6_2atmpS2254;
    if (_M0L5_2aloS743 < _M0L3lo3S757) {
      _M0L6_2atmpS2258 = 1ull;
    } else {
      _M0L6_2atmpS2258 = 0ull;
    }
    _M0L4mid3S758 = _M0L6_2atmpS2257 - _M0L6_2atmpS2258;
    if (_M0L3midS748 < _M0L4mid3S758) {
      _M0L6_2atmpS2256 = 1ull;
    } else {
      _M0L6_2atmpS2256 = 0ull;
    }
    _M0L3hi3S759 = _M0L2hiS749 - _M0L6_2atmpS2256;
    _M0L6_2atmpS2255 = _M0L1jS754 - 64;
    _M0L6_2atmpS2254 = _M0L6_2atmpS2255 - 1;
    #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS755
    = _M0FPB13shiftright128(_M0L4mid3S758, _M0L3hi3S759, _M0L6_2atmpS2254);
  } else {
    uint64_t _M0L3lo3S760 = _M0L5_2aloS743 + _M0L5_2aloS743;
    uint64_t _M0L6_2atmpS2265 = _M0L3midS748 + _M0L3midS748;
    uint64_t _M0L6_2atmpS2266;
    uint64_t _M0L4mid3S761;
    uint64_t _M0L6_2atmpS2263;
    uint64_t _M0L6_2atmpS2264;
    uint64_t _M0L3hi3S762;
    uint64_t _M0L3lo4S763;
    uint64_t _M0L6_2atmpS2261;
    uint64_t _M0L6_2atmpS2262;
    uint64_t _M0L4mid4S764;
    uint64_t _M0L6_2atmpS2260;
    uint64_t _M0L3hi4S765;
    int32_t _M0L6_2atmpS2259;
    if (_M0L3lo3S760 < _M0L5_2aloS743) {
      _M0L6_2atmpS2266 = 1ull;
    } else {
      _M0L6_2atmpS2266 = 0ull;
    }
    _M0L4mid3S761 = _M0L6_2atmpS2265 + _M0L6_2atmpS2266;
    _M0L6_2atmpS2263 = _M0L2hiS749 + _M0L2hiS749;
    if (_M0L4mid3S761 < _M0L3midS748) {
      _M0L6_2atmpS2264 = 1ull;
    } else {
      _M0L6_2atmpS2264 = 0ull;
    }
    _M0L3hi3S762 = _M0L6_2atmpS2263 + _M0L6_2atmpS2264;
    _M0L3lo4S763 = _M0L3lo3S760 - _M0L7_2amul0S737;
    _M0L6_2atmpS2261 = _M0L4mid3S761 - _M0L7_2amul1S739;
    if (_M0L3lo3S760 < _M0L3lo4S763) {
      _M0L6_2atmpS2262 = 1ull;
    } else {
      _M0L6_2atmpS2262 = 0ull;
    }
    _M0L4mid4S764 = _M0L6_2atmpS2261 - _M0L6_2atmpS2262;
    if (_M0L4mid3S761 < _M0L4mid4S764) {
      _M0L6_2atmpS2260 = 1ull;
    } else {
      _M0L6_2atmpS2260 = 0ull;
    }
    _M0L3hi4S765 = _M0L3hi3S762 - _M0L6_2atmpS2260;
    _M0L6_2atmpS2259 = _M0L1jS754 - 64;
    #line 159 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0Lm2vmS755
    = _M0FPB13shiftright128(_M0L4mid4S764, _M0L3hi4S765, _M0L6_2atmpS2259);
  }
  _M0L6_2atmpS2269 = _M0L1jS754 - 64;
  _M0L6_2atmpS2268 = _M0L6_2atmpS2269 - 1;
  #line 161 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L2vrS766
  = _M0FPB13shiftright128(_M0L3midS748, _M0L2hiS749, _M0L6_2atmpS2268);
  _M0L6_2atmpS2267 = _M0Lm2vmS755;
  return (struct _M0TPB19MulShiftAll64Result){_M0L2vrS766,
                                                _M0L2vpS753,
                                                _M0L6_2atmpS2267};
}

int32_t _M0FPB18multipleOfPowerOf2(
  uint64_t _M0L5valueS735,
  int32_t _M0L1pS736
) {
  uint64_t _M0L6_2atmpS2253;
  uint64_t _M0L6_2atmpS2252;
  uint64_t _M0L6_2atmpS2251;
  #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2253 = 1ull << (_M0L1pS736 & 63);
  _M0L6_2atmpS2252 = _M0L6_2atmpS2253 - 1ull;
  _M0L6_2atmpS2251 = _M0L5valueS735 & _M0L6_2atmpS2252;
  return _M0L6_2atmpS2251 == 0ull;
}

int32_t _M0FPB18multipleOfPowerOf5(
  uint64_t _M0L5valueS733,
  int32_t _M0L1pS734
) {
  int32_t _M0L6_2atmpS2250;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2250 = _M0FPB10pow5Factor(_M0L5valueS733);
  return _M0L6_2atmpS2250 >= _M0L1pS734;
}

int32_t _M0FPB10pow5Factor(uint64_t _M0L5valueS729) {
  uint64_t _M0L6_2atmpS2238;
  uint64_t _M0L6_2atmpS2239;
  uint64_t _M0L6_2atmpS2240;
  uint64_t _M0L6_2atmpS2241;
  int32_t _M0Lm5countS730;
  uint64_t _M0Lm5valueS731;
  uint64_t _M0L6_2atmpS2249;
  moonbit_string_t _M0L6_2atmpS2248;
  moonbit_string_t _M0L6_2atmpS3962;
  moonbit_string_t _M0L6_2atmpS2247;
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2238 = _M0L5valueS729 % 5ull;
  if (_M0L6_2atmpS2238 != 0ull) {
    return 0;
  }
  _M0L6_2atmpS2239 = _M0L5valueS729 % 25ull;
  if (_M0L6_2atmpS2239 != 0ull) {
    return 1;
  }
  _M0L6_2atmpS2240 = _M0L5valueS729 % 125ull;
  if (_M0L6_2atmpS2240 != 0ull) {
    return 2;
  }
  _M0L6_2atmpS2241 = _M0L5valueS729 % 625ull;
  if (_M0L6_2atmpS2241 != 0ull) {
    return 3;
  }
  _M0Lm5countS730 = 4;
  _M0Lm5valueS731 = _M0L5valueS729 / 625ull;
  while (1) {
    uint64_t _M0L6_2atmpS2242 = _M0Lm5valueS731;
    if (_M0L6_2atmpS2242 > 0ull) {
      uint64_t _M0L6_2atmpS2244 = _M0Lm5valueS731;
      uint64_t _M0L6_2atmpS2243 = _M0L6_2atmpS2244 % 5ull;
      uint64_t _M0L6_2atmpS2245;
      int32_t _M0L6_2atmpS2246;
      if (_M0L6_2atmpS2243 != 0ull) {
        return _M0Lm5countS730;
      }
      _M0L6_2atmpS2245 = _M0Lm5valueS731;
      _M0Lm5valueS731 = _M0L6_2atmpS2245 / 5ull;
      _M0L6_2atmpS2246 = _M0Lm5countS730;
      _M0Lm5countS730 = _M0L6_2atmpS2246 + 1;
      continue;
    }
    break;
  }
  _M0L6_2atmpS2249 = _M0Lm5valueS731;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2248
  = _M0IP016_24default__implPB4Show10to__stringGmE(_M0L6_2atmpS2249);
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3962
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_45.data, _M0L6_2atmpS2248);
  moonbit_decref(_M0L6_2atmpS2248);
  _M0L6_2atmpS2247 = _M0L6_2atmpS3962;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0FPB5abortGiE(_M0L6_2atmpS2247, (moonbit_string_t)moonbit_string_literal_46.data);
}

uint64_t _M0FPB13shiftright128(
  uint64_t _M0L2loS728,
  uint64_t _M0L2hiS726,
  int32_t _M0L4distS727
) {
  int32_t _M0L6_2atmpS2237;
  uint64_t _M0L6_2atmpS2235;
  uint64_t _M0L6_2atmpS2236;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2237 = 64 - _M0L4distS727;
  _M0L6_2atmpS2235 = _M0L2hiS726 << (_M0L6_2atmpS2237 & 63);
  _M0L6_2atmpS2236 = _M0L2loS728 >> (_M0L4distS727 & 63);
  return _M0L6_2atmpS2235 | _M0L6_2atmpS2236;
}

struct _M0TPB7Umul128 _M0FPB7umul128(
  uint64_t _M0L1aS716,
  uint64_t _M0L1bS719
) {
  uint64_t _M0L3aLoS715;
  uint64_t _M0L3aHiS717;
  uint64_t _M0L3bLoS718;
  uint64_t _M0L3bHiS720;
  uint64_t _M0L1xS721;
  uint64_t _M0L6_2atmpS2233;
  uint64_t _M0L6_2atmpS2234;
  uint64_t _M0L1yS722;
  uint64_t _M0L6_2atmpS2231;
  uint64_t _M0L6_2atmpS2232;
  uint64_t _M0L1zS723;
  uint64_t _M0L6_2atmpS2229;
  uint64_t _M0L6_2atmpS2230;
  uint64_t _M0L6_2atmpS2227;
  uint64_t _M0L6_2atmpS2228;
  uint64_t _M0L1wS724;
  uint64_t _M0L2loS725;
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3aLoS715 = _M0L1aS716 & 4294967295ull;
  _M0L3aHiS717 = _M0L1aS716 >> 32;
  _M0L3bLoS718 = _M0L1bS719 & 4294967295ull;
  _M0L3bHiS720 = _M0L1bS719 >> 32;
  _M0L1xS721 = _M0L3aLoS715 * _M0L3bLoS718;
  _M0L6_2atmpS2233 = _M0L3aHiS717 * _M0L3bLoS718;
  _M0L6_2atmpS2234 = _M0L1xS721 >> 32;
  _M0L1yS722 = _M0L6_2atmpS2233 + _M0L6_2atmpS2234;
  _M0L6_2atmpS2231 = _M0L3aLoS715 * _M0L3bHiS720;
  _M0L6_2atmpS2232 = _M0L1yS722 & 4294967295ull;
  _M0L1zS723 = _M0L6_2atmpS2231 + _M0L6_2atmpS2232;
  _M0L6_2atmpS2229 = _M0L3aHiS717 * _M0L3bHiS720;
  _M0L6_2atmpS2230 = _M0L1yS722 >> 32;
  _M0L6_2atmpS2227 = _M0L6_2atmpS2229 + _M0L6_2atmpS2230;
  _M0L6_2atmpS2228 = _M0L1zS723 >> 32;
  _M0L1wS724 = _M0L6_2atmpS2227 + _M0L6_2atmpS2228;
  _M0L2loS725 = _M0L1aS716 * _M0L1bS719;
  return (struct _M0TPB7Umul128){_M0L2loS725, _M0L1wS724};
}

moonbit_string_t _M0FPB19string__from__bytes(
  moonbit_bytes_t _M0L5bytesS710,
  int32_t _M0L4fromS714,
  int32_t _M0L2toS712
) {
  int32_t _M0L6_2atmpS2226;
  struct _M0TPB13StringBuilder* _M0L3bufS709;
  int32_t _M0L1iS711;
  #line 52 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2226 = Moonbit_array_length(_M0L5bytesS710);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L3bufS709 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS2226);
  _M0L1iS711 = _M0L4fromS714;
  while (1) {
    if (_M0L1iS711 < _M0L2toS712) {
      int32_t _M0L6_2atmpS2224;
      int32_t _M0L6_2atmpS2223;
      int32_t _M0L6_2atmpS2225;
      if (
        _M0L1iS711 < 0 || _M0L1iS711 >= Moonbit_array_length(_M0L5bytesS710)
      ) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2224 = (int32_t)_M0L5bytesS710[_M0L1iS711];
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0L6_2atmpS2223 = _M0MPC14byte4Byte8to__char(_M0L6_2atmpS2224);
      moonbit_incref(_M0L3bufS709);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS709, _M0L6_2atmpS2223);
      _M0L6_2atmpS2225 = _M0L1iS711 + 1;
      _M0L1iS711 = _M0L6_2atmpS2225;
      continue;
    } else {
      moonbit_decref(_M0L5bytesS710);
    }
    break;
  }
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS709);
}

int32_t _M0FPB9log10Pow2(int32_t _M0L1eS708) {
  int32_t _M0L6_2atmpS2222;
  uint32_t _M0L6_2atmpS2221;
  uint32_t _M0L6_2atmpS2220;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2222 = _M0L1eS708 * 78913;
  _M0L6_2atmpS2221 = *(uint32_t*)&_M0L6_2atmpS2222;
  _M0L6_2atmpS2220 = _M0L6_2atmpS2221 >> 18;
  return *(int32_t*)&_M0L6_2atmpS2220;
}

int32_t _M0FPB9log10Pow5(int32_t _M0L1eS707) {
  int32_t _M0L6_2atmpS2219;
  uint32_t _M0L6_2atmpS2218;
  uint32_t _M0L6_2atmpS2217;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2219 = _M0L1eS707 * 732923;
  _M0L6_2atmpS2218 = *(uint32_t*)&_M0L6_2atmpS2219;
  _M0L6_2atmpS2217 = _M0L6_2atmpS2218 >> 20;
  return *(int32_t*)&_M0L6_2atmpS2217;
}

moonbit_string_t _M0FPB18copy__special__str(
  int32_t _M0L4signS705,
  int32_t _M0L8exponentS706,
  int32_t _M0L8mantissaS703
) {
  moonbit_string_t _M0L1sS704;
  moonbit_string_t _M0L6_2atmpS3963;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  if (_M0L8mantissaS703) {
    return (moonbit_string_t)moonbit_string_literal_47.data;
  }
  if (_M0L4signS705) {
    _M0L1sS704 = (moonbit_string_t)moonbit_string_literal_48.data;
  } else {
    _M0L1sS704 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  if (_M0L8exponentS706) {
    moonbit_string_t _M0L6_2atmpS3964;
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
    _M0L6_2atmpS3964
    = moonbit_add_string(_M0L1sS704, (moonbit_string_t)moonbit_string_literal_49.data);
    moonbit_decref(_M0L1sS704);
    return _M0L6_2atmpS3964;
  }
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS3963
  = moonbit_add_string(_M0L1sS704, (moonbit_string_t)moonbit_string_literal_50.data);
  moonbit_decref(_M0L1sS704);
  return _M0L6_2atmpS3963;
}

int32_t _M0FPB8pow5bits(int32_t _M0L1eS702) {
  int32_t _M0L6_2atmpS2216;
  uint32_t _M0L6_2atmpS2215;
  uint32_t _M0L6_2atmpS2214;
  int32_t _M0L6_2atmpS2213;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\double_ryu_nonjs.mbt"
  _M0L6_2atmpS2216 = _M0L1eS702 * 1217359;
  _M0L6_2atmpS2215 = *(uint32_t*)&_M0L6_2atmpS2216;
  _M0L6_2atmpS2214 = _M0L6_2atmpS2215 >> 19;
  _M0L6_2atmpS2213 = *(int32_t*)&_M0L6_2atmpS2214;
  return _M0L6_2atmpS2213 + 1;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS701,
  struct _M0TPB6Hasher* _M0L6hasherS700
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS700, _M0L4selfS701);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS699,
  struct _M0TPB6Hasher* _M0L6hasherS698
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS698, _M0L4selfS699);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS696,
  moonbit_string_t _M0L5valueS694
) {
  int32_t _M0L7_2abindS693;
  int32_t _M0L1iS695;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS693 = Moonbit_array_length(_M0L5valueS694);
  _M0L1iS695 = 0;
  while (1) {
    if (_M0L1iS695 < _M0L7_2abindS693) {
      int32_t _M0L6_2atmpS2211 = _M0L5valueS694[_M0L1iS695];
      int32_t _M0L6_2atmpS2210 = (int32_t)_M0L6_2atmpS2211;
      uint32_t _M0L6_2atmpS2209 = *(uint32_t*)&_M0L6_2atmpS2210;
      int32_t _M0L6_2atmpS2212;
      moonbit_incref(_M0L4selfS696);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS696, _M0L6_2atmpS2209);
      _M0L6_2atmpS2212 = _M0L1iS695 + 1;
      _M0L1iS695 = _M0L6_2atmpS2212;
      continue;
    } else {
      moonbit_decref(_M0L4selfS696);
      moonbit_decref(_M0L5valueS694);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS691,
  int32_t _M0L3idxS692
) {
  int32_t _M0L6_2atmpS3965;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3965 = _M0L4selfS691[_M0L3idxS692];
  moonbit_decref(_M0L4selfS691);
  return _M0L6_2atmpS3965;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS690) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS690;
}

void* _M0IPC14bool4BoolPB6ToJson8to__json(int32_t _M0L4selfS689) {
  #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L4selfS689) {
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json7boolean(1);
  } else {
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
    return _M0MPC14json4Json7boolean(0);
  }
}

void* _M0MPC14json4Json7boolean(int32_t _M0L7booleanS688) {
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  if (_M0L7booleanS688) {
    return (struct moonbit_object*)&moonbit_constant_constructor_1 + 1;
  } else {
    return (struct moonbit_object*)&moonbit_constant_constructor_2 + 1;
  }
}

struct _M0TWEOUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS681
) {
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3966;
  int32_t _M0L6_2acntS4172;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L4headS2208;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS680;
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2204__l591__* _closure_4298;
  struct _M0TWEOUsRPB4JsonE* _M0L6_2atmpS2203;
  #line 588 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3966 = _M0L4selfS681->$5;
  _M0L6_2acntS4172 = Moonbit_object_header(_M0L4selfS681)->rc;
  if (_M0L6_2acntS4172 > 1) {
    int32_t _M0L11_2anew__cntS4174 = _M0L6_2acntS4172 - 1;
    Moonbit_object_header(_M0L4selfS681)->rc = _M0L11_2anew__cntS4174;
    if (_M0L8_2afieldS3966) {
      moonbit_incref(_M0L8_2afieldS3966);
    }
  } else if (_M0L6_2acntS4172 == 1) {
    struct _M0TPB5EntryGsRPB4JsonE** _M0L8_2afieldS4173 = _M0L4selfS681->$0;
    moonbit_decref(_M0L8_2afieldS4173);
    #line 590 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L4selfS681);
  }
  _M0L4headS2208 = _M0L8_2afieldS3966;
  _M0L11curr__entryS680
  = (struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE));
  Moonbit_object_header(_M0L11curr__entryS680)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE, $0) >> 2, 1, 0);
  _M0L11curr__entryS680->$0 = _M0L4headS2208;
  _closure_4298
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2204__l591__*)moonbit_malloc(sizeof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2204__l591__));
  Moonbit_object_header(_closure_4298)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2204__l591__, $0) >> 2, 1, 0);
  _closure_4298->code = &_M0MPB3Map4iterGsRPB4JsonEC2204l591;
  _closure_4298->$0 = _M0L11curr__entryS680;
  _M0L6_2atmpS2203 = (struct _M0TWEOUsRPB4JsonE*)_closure_4298;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  return _M0MPB4Iter3newGUsRPB4JsonEE(_M0L6_2atmpS2203);
}

struct _M0TUsRPB4JsonE* _M0MPB3Map4iterGsRPB4JsonEC2204l591(
  struct _M0TWEOUsRPB4JsonE* _M0L6_2aenvS2205
) {
  struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2204__l591__* _M0L14_2acasted__envS2206;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L8_2afieldS3972;
  int32_t _M0L6_2acntS4175;
  struct _M0TPC13ref3RefGORPB5EntryGsRPB4JsonEE* _M0L11curr__entryS680;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3971;
  struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2abindS682;
  #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14_2acasted__envS2206
  = (struct _M0R94Map_3a_3aiter_7c_5bString_2c_20moonbitlang_2fcore_2fbuiltin_2fJson_5d_7c_2eanon__u2204__l591__*)_M0L6_2aenvS2205;
  _M0L8_2afieldS3972 = _M0L14_2acasted__envS2206->$0;
  _M0L6_2acntS4175 = Moonbit_object_header(_M0L14_2acasted__envS2206)->rc;
  if (_M0L6_2acntS4175 > 1) {
    int32_t _M0L11_2anew__cntS4176 = _M0L6_2acntS4175 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2206)->rc
    = _M0L11_2anew__cntS4176;
    moonbit_incref(_M0L8_2afieldS3972);
  } else if (_M0L6_2acntS4175 == 1) {
    #line 591 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L14_2acasted__envS2206);
  }
  _M0L11curr__entryS680 = _M0L8_2afieldS3972;
  _M0L8_2afieldS3971 = _M0L11curr__entryS680->$0;
  _M0L7_2abindS682 = _M0L8_2afieldS3971;
  if (_M0L7_2abindS682 == 0) {
    moonbit_decref(_M0L11curr__entryS680);
    return 0;
  } else {
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2aSomeS683 = _M0L7_2abindS682;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L4_2axS684 = _M0L7_2aSomeS683;
    moonbit_string_t _M0L8_2afieldS3970 = _M0L4_2axS684->$4;
    moonbit_string_t _M0L6_2akeyS685 = _M0L8_2afieldS3970;
    void* _M0L8_2afieldS3969 = _M0L4_2axS684->$5;
    void* _M0L8_2avalueS686 = _M0L8_2afieldS3969;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L8_2afieldS3968 = _M0L4_2axS684->$1;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L7_2anextS687 = _M0L8_2afieldS3968;
    struct _M0TPB5EntryGsRPB4JsonE* _M0L6_2aoldS3967 =
      _M0L11curr__entryS680->$0;
    struct _M0TUsRPB4JsonE* _M0L8_2atupleS2207;
    if (_M0L7_2anextS687) {
      moonbit_incref(_M0L7_2anextS687);
    }
    moonbit_incref(_M0L8_2avalueS686);
    moonbit_incref(_M0L6_2akeyS685);
    if (_M0L6_2aoldS3967) {
      moonbit_decref(_M0L6_2aoldS3967);
    }
    _M0L11curr__entryS680->$0 = _M0L7_2anextS687;
    moonbit_decref(_M0L11curr__entryS680);
    _M0L8_2atupleS2207
    = (struct _M0TUsRPB4JsonE*)moonbit_malloc(sizeof(struct _M0TUsRPB4JsonE));
    Moonbit_object_header(_M0L8_2atupleS2207)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB4JsonE, $0) >> 2, 2, 0);
    _M0L8_2atupleS2207->$0 = _M0L6_2akeyS685;
    _M0L8_2atupleS2207->$1 = _M0L8_2avalueS686;
    return _M0L8_2atupleS2207;
  }
}

int32_t _M0MPB3Map9is__emptyGsRPB4JsonE(
  struct _M0TPB3MapGsRPB4JsonE* _M0L4selfS679
) {
  int32_t _M0L8_2afieldS3973;
  int32_t _M0L4sizeS2202;
  #line 541 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3973 = _M0L4selfS679->$1;
  moonbit_decref(_M0L4selfS679);
  _M0L4sizeS2202 = _M0L8_2afieldS3973;
  return _M0L4sizeS2202 == 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS666,
  int32_t _M0L3keyS662
) {
  int32_t _M0L4hashS661;
  int32_t _M0L14capacity__maskS2187;
  int32_t _M0L6_2atmpS2186;
  int32_t _M0L1iS663;
  int32_t _M0L3idxS664;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS661 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS662);
  _M0L14capacity__maskS2187 = _M0L4selfS666->$3;
  _M0L6_2atmpS2186 = _M0L4hashS661 & _M0L14capacity__maskS2187;
  _M0L1iS663 = 0;
  _M0L3idxS664 = _M0L6_2atmpS2186;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3977 =
      _M0L4selfS666->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2185 =
      _M0L8_2afieldS3977;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3976;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS665;
    if (
      _M0L3idxS664 < 0
      || _M0L3idxS664 >= Moonbit_array_length(_M0L7entriesS2185)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3976
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2185[
        _M0L3idxS664
      ];
    _M0L7_2abindS665 = _M0L6_2atmpS3976;
    if (_M0L7_2abindS665 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2174;
      if (_M0L7_2abindS665) {
        moonbit_incref(_M0L7_2abindS665);
      }
      moonbit_decref(_M0L4selfS666);
      if (_M0L7_2abindS665) {
        moonbit_decref(_M0L7_2abindS665);
      }
      _M0L6_2atmpS2174 = 0;
      return _M0L6_2atmpS2174;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS667 =
        _M0L7_2abindS665;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS668 =
        _M0L7_2aSomeS667;
      int32_t _M0L4hashS2176 = _M0L8_2aentryS668->$3;
      int32_t _if__result_4300;
      int32_t _M0L8_2afieldS3974;
      int32_t _M0L3pslS2179;
      int32_t _M0L6_2atmpS2181;
      int32_t _M0L6_2atmpS2183;
      int32_t _M0L14capacity__maskS2184;
      int32_t _M0L6_2atmpS2182;
      if (_M0L4hashS2176 == _M0L4hashS661) {
        int32_t _M0L3keyS2175 = _M0L8_2aentryS668->$4;
        _if__result_4300 = _M0L3keyS2175 == _M0L3keyS662;
      } else {
        _if__result_4300 = 0;
      }
      if (_if__result_4300) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3975;
        int32_t _M0L6_2acntS4177;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2178;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2177;
        moonbit_incref(_M0L8_2aentryS668);
        moonbit_decref(_M0L4selfS666);
        _M0L8_2afieldS3975 = _M0L8_2aentryS668->$5;
        _M0L6_2acntS4177 = Moonbit_object_header(_M0L8_2aentryS668)->rc;
        if (_M0L6_2acntS4177 > 1) {
          int32_t _M0L11_2anew__cntS4179 = _M0L6_2acntS4177 - 1;
          Moonbit_object_header(_M0L8_2aentryS668)->rc
          = _M0L11_2anew__cntS4179;
          moonbit_incref(_M0L8_2afieldS3975);
        } else if (_M0L6_2acntS4177 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4178 =
            _M0L8_2aentryS668->$1;
          if (_M0L8_2afieldS4178) {
            moonbit_decref(_M0L8_2afieldS4178);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS668);
        }
        _M0L5valueS2178 = _M0L8_2afieldS3975;
        _M0L6_2atmpS2177 = _M0L5valueS2178;
        return _M0L6_2atmpS2177;
      } else {
        moonbit_incref(_M0L8_2aentryS668);
      }
      _M0L8_2afieldS3974 = _M0L8_2aentryS668->$2;
      moonbit_decref(_M0L8_2aentryS668);
      _M0L3pslS2179 = _M0L8_2afieldS3974;
      if (_M0L1iS663 > _M0L3pslS2179) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2180;
        moonbit_decref(_M0L4selfS666);
        _M0L6_2atmpS2180 = 0;
        return _M0L6_2atmpS2180;
      }
      _M0L6_2atmpS2181 = _M0L1iS663 + 1;
      _M0L6_2atmpS2183 = _M0L3idxS664 + 1;
      _M0L14capacity__maskS2184 = _M0L4selfS666->$3;
      _M0L6_2atmpS2182 = _M0L6_2atmpS2183 & _M0L14capacity__maskS2184;
      _M0L1iS663 = _M0L6_2atmpS2181;
      _M0L3idxS664 = _M0L6_2atmpS2182;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS675,
  moonbit_string_t _M0L3keyS671
) {
  int32_t _M0L4hashS670;
  int32_t _M0L14capacity__maskS2201;
  int32_t _M0L6_2atmpS2200;
  int32_t _M0L1iS672;
  int32_t _M0L3idxS673;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS671);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS670 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS671);
  _M0L14capacity__maskS2201 = _M0L4selfS675->$3;
  _M0L6_2atmpS2200 = _M0L4hashS670 & _M0L14capacity__maskS2201;
  _M0L1iS672 = 0;
  _M0L3idxS673 = _M0L6_2atmpS2200;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3983 =
      _M0L4selfS675->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2199 =
      _M0L8_2afieldS3983;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3982;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS674;
    if (
      _M0L3idxS673 < 0
      || _M0L3idxS673 >= Moonbit_array_length(_M0L7entriesS2199)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3982
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2199[
        _M0L3idxS673
      ];
    _M0L7_2abindS674 = _M0L6_2atmpS3982;
    if (_M0L7_2abindS674 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2188;
      if (_M0L7_2abindS674) {
        moonbit_incref(_M0L7_2abindS674);
      }
      moonbit_decref(_M0L4selfS675);
      if (_M0L7_2abindS674) {
        moonbit_decref(_M0L7_2abindS674);
      }
      moonbit_decref(_M0L3keyS671);
      _M0L6_2atmpS2188 = 0;
      return _M0L6_2atmpS2188;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS676 =
        _M0L7_2abindS674;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS677 =
        _M0L7_2aSomeS676;
      int32_t _M0L4hashS2190 = _M0L8_2aentryS677->$3;
      int32_t _if__result_4302;
      int32_t _M0L8_2afieldS3978;
      int32_t _M0L3pslS2193;
      int32_t _M0L6_2atmpS2195;
      int32_t _M0L6_2atmpS2197;
      int32_t _M0L14capacity__maskS2198;
      int32_t _M0L6_2atmpS2196;
      if (_M0L4hashS2190 == _M0L4hashS670) {
        moonbit_string_t _M0L8_2afieldS3981 = _M0L8_2aentryS677->$4;
        moonbit_string_t _M0L3keyS2189 = _M0L8_2afieldS3981;
        int32_t _M0L6_2atmpS3980;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3980
        = moonbit_val_array_equal(_M0L3keyS2189, _M0L3keyS671);
        _if__result_4302 = _M0L6_2atmpS3980;
      } else {
        _if__result_4302 = 0;
      }
      if (_if__result_4302) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3979;
        int32_t _M0L6_2acntS4180;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2192;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2191;
        moonbit_incref(_M0L8_2aentryS677);
        moonbit_decref(_M0L4selfS675);
        moonbit_decref(_M0L3keyS671);
        _M0L8_2afieldS3979 = _M0L8_2aentryS677->$5;
        _M0L6_2acntS4180 = Moonbit_object_header(_M0L8_2aentryS677)->rc;
        if (_M0L6_2acntS4180 > 1) {
          int32_t _M0L11_2anew__cntS4183 = _M0L6_2acntS4180 - 1;
          Moonbit_object_header(_M0L8_2aentryS677)->rc
          = _M0L11_2anew__cntS4183;
          moonbit_incref(_M0L8_2afieldS3979);
        } else if (_M0L6_2acntS4180 == 1) {
          moonbit_string_t _M0L8_2afieldS4182 = _M0L8_2aentryS677->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4181;
          moonbit_decref(_M0L8_2afieldS4182);
          _M0L8_2afieldS4181 = _M0L8_2aentryS677->$1;
          if (_M0L8_2afieldS4181) {
            moonbit_decref(_M0L8_2afieldS4181);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS677);
        }
        _M0L5valueS2192 = _M0L8_2afieldS3979;
        _M0L6_2atmpS2191 = _M0L5valueS2192;
        return _M0L6_2atmpS2191;
      } else {
        moonbit_incref(_M0L8_2aentryS677);
      }
      _M0L8_2afieldS3978 = _M0L8_2aentryS677->$2;
      moonbit_decref(_M0L8_2aentryS677);
      _M0L3pslS2193 = _M0L8_2afieldS3978;
      if (_M0L1iS672 > _M0L3pslS2193) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2194;
        moonbit_decref(_M0L4selfS675);
        moonbit_decref(_M0L3keyS671);
        _M0L6_2atmpS2194 = 0;
        return _M0L6_2atmpS2194;
      }
      _M0L6_2atmpS2195 = _M0L1iS672 + 1;
      _M0L6_2atmpS2197 = _M0L3idxS673 + 1;
      _M0L14capacity__maskS2198 = _M0L4selfS675->$3;
      _M0L6_2atmpS2196 = _M0L6_2atmpS2197 & _M0L14capacity__maskS2198;
      _M0L1iS672 = _M0L6_2atmpS2195;
      _M0L3idxS673 = _M0L6_2atmpS2196;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS646
) {
  int32_t _M0L6lengthS645;
  int32_t _M0Lm8capacityS647;
  int32_t _M0L6_2atmpS2151;
  int32_t _M0L6_2atmpS2150;
  int32_t _M0L6_2atmpS2161;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS648;
  int32_t _M0L3endS2159;
  int32_t _M0L5startS2160;
  int32_t _M0L7_2abindS649;
  int32_t _M0L2__S650;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS646.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS645
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS646);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS647 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS645);
  _M0L6_2atmpS2151 = _M0Lm8capacityS647;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2150 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2151);
  if (_M0L6lengthS645 > _M0L6_2atmpS2150) {
    int32_t _M0L6_2atmpS2152 = _M0Lm8capacityS647;
    _M0Lm8capacityS647 = _M0L6_2atmpS2152 * 2;
  }
  _M0L6_2atmpS2161 = _M0Lm8capacityS647;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS648
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2161);
  _M0L3endS2159 = _M0L3arrS646.$2;
  _M0L5startS2160 = _M0L3arrS646.$1;
  _M0L7_2abindS649 = _M0L3endS2159 - _M0L5startS2160;
  _M0L2__S650 = 0;
  while (1) {
    if (_M0L2__S650 < _M0L7_2abindS649) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3987 =
        _M0L3arrS646.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2156 =
        _M0L8_2afieldS3987;
      int32_t _M0L5startS2158 = _M0L3arrS646.$1;
      int32_t _M0L6_2atmpS2157 = _M0L5startS2158 + _M0L2__S650;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3986 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2156[
          _M0L6_2atmpS2157
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS651 =
        _M0L6_2atmpS3986;
      moonbit_string_t _M0L8_2afieldS3985 = _M0L1eS651->$0;
      moonbit_string_t _M0L6_2atmpS2153 = _M0L8_2afieldS3985;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3984 =
        _M0L1eS651->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2154 =
        _M0L8_2afieldS3984;
      int32_t _M0L6_2atmpS2155;
      moonbit_incref(_M0L6_2atmpS2154);
      moonbit_incref(_M0L6_2atmpS2153);
      moonbit_incref(_M0L1mS648);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS648, _M0L6_2atmpS2153, _M0L6_2atmpS2154);
      _M0L6_2atmpS2155 = _M0L2__S650 + 1;
      _M0L2__S650 = _M0L6_2atmpS2155;
      continue;
    } else {
      moonbit_decref(_M0L3arrS646.$0);
    }
    break;
  }
  return _M0L1mS648;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS654
) {
  int32_t _M0L6lengthS653;
  int32_t _M0Lm8capacityS655;
  int32_t _M0L6_2atmpS2163;
  int32_t _M0L6_2atmpS2162;
  int32_t _M0L6_2atmpS2173;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS656;
  int32_t _M0L3endS2171;
  int32_t _M0L5startS2172;
  int32_t _M0L7_2abindS657;
  int32_t _M0L2__S658;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS654.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS653
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS654);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS655 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS653);
  _M0L6_2atmpS2163 = _M0Lm8capacityS655;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2162 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2163);
  if (_M0L6lengthS653 > _M0L6_2atmpS2162) {
    int32_t _M0L6_2atmpS2164 = _M0Lm8capacityS655;
    _M0Lm8capacityS655 = _M0L6_2atmpS2164 * 2;
  }
  _M0L6_2atmpS2173 = _M0Lm8capacityS655;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS656
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2173);
  _M0L3endS2171 = _M0L3arrS654.$2;
  _M0L5startS2172 = _M0L3arrS654.$1;
  _M0L7_2abindS657 = _M0L3endS2171 - _M0L5startS2172;
  _M0L2__S658 = 0;
  while (1) {
    if (_M0L2__S658 < _M0L7_2abindS657) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3990 =
        _M0L3arrS654.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2168 =
        _M0L8_2afieldS3990;
      int32_t _M0L5startS2170 = _M0L3arrS654.$1;
      int32_t _M0L6_2atmpS2169 = _M0L5startS2170 + _M0L2__S658;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3989 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2168[
          _M0L6_2atmpS2169
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS659 = _M0L6_2atmpS3989;
      int32_t _M0L6_2atmpS2165 = _M0L1eS659->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3988 =
        _M0L1eS659->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2166 =
        _M0L8_2afieldS3988;
      int32_t _M0L6_2atmpS2167;
      moonbit_incref(_M0L6_2atmpS2166);
      moonbit_incref(_M0L1mS656);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS656, _M0L6_2atmpS2165, _M0L6_2atmpS2166);
      _M0L6_2atmpS2167 = _M0L2__S658 + 1;
      _M0L2__S658 = _M0L6_2atmpS2167;
      continue;
    } else {
      moonbit_decref(_M0L3arrS654.$0);
    }
    break;
  }
  return _M0L1mS656;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS639,
  moonbit_string_t _M0L3keyS640,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS641
) {
  int32_t _M0L6_2atmpS2148;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS640);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2148 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS640);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS639, _M0L3keyS640, _M0L5valueS641, _M0L6_2atmpS2148);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS642,
  int32_t _M0L3keyS643,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS644
) {
  int32_t _M0L6_2atmpS2149;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2149 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS643);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS642, _M0L3keyS643, _M0L5valueS644, _M0L6_2atmpS2149);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS618
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3997;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS617;
  int32_t _M0L8capacityS2140;
  int32_t _M0L13new__capacityS619;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2135;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2134;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3996;
  int32_t _M0L6_2atmpS2136;
  int32_t _M0L8capacityS2138;
  int32_t _M0L6_2atmpS2137;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2139;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3995;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS620;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3997 = _M0L4selfS618->$5;
  _M0L9old__headS617 = _M0L8_2afieldS3997;
  _M0L8capacityS2140 = _M0L4selfS618->$2;
  _M0L13new__capacityS619 = _M0L8capacityS2140 << 1;
  _M0L6_2atmpS2135 = 0;
  _M0L6_2atmpS2134
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS619, _M0L6_2atmpS2135);
  _M0L6_2aoldS3996 = _M0L4selfS618->$0;
  if (_M0L9old__headS617) {
    moonbit_incref(_M0L9old__headS617);
  }
  moonbit_decref(_M0L6_2aoldS3996);
  _M0L4selfS618->$0 = _M0L6_2atmpS2134;
  _M0L4selfS618->$2 = _M0L13new__capacityS619;
  _M0L6_2atmpS2136 = _M0L13new__capacityS619 - 1;
  _M0L4selfS618->$3 = _M0L6_2atmpS2136;
  _M0L8capacityS2138 = _M0L4selfS618->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2137 = _M0FPB21calc__grow__threshold(_M0L8capacityS2138);
  _M0L4selfS618->$4 = _M0L6_2atmpS2137;
  _M0L4selfS618->$1 = 0;
  _M0L6_2atmpS2139 = 0;
  _M0L6_2aoldS3995 = _M0L4selfS618->$5;
  if (_M0L6_2aoldS3995) {
    moonbit_decref(_M0L6_2aoldS3995);
  }
  _M0L4selfS618->$5 = _M0L6_2atmpS2139;
  _M0L4selfS618->$6 = -1;
  _M0L8_2aparamS620 = _M0L9old__headS617;
  while (1) {
    if (_M0L8_2aparamS620 == 0) {
      if (_M0L8_2aparamS620) {
        moonbit_decref(_M0L8_2aparamS620);
      }
      moonbit_decref(_M0L4selfS618);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS621 =
        _M0L8_2aparamS620;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS622 =
        _M0L7_2aSomeS621;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3994 =
        _M0L4_2axS622->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS623 =
        _M0L8_2afieldS3994;
      moonbit_string_t _M0L8_2afieldS3993 = _M0L4_2axS622->$4;
      moonbit_string_t _M0L6_2akeyS624 = _M0L8_2afieldS3993;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3992 =
        _M0L4_2axS622->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS625 =
        _M0L8_2afieldS3992;
      int32_t _M0L8_2afieldS3991 = _M0L4_2axS622->$3;
      int32_t _M0L6_2acntS4184 = Moonbit_object_header(_M0L4_2axS622)->rc;
      int32_t _M0L7_2ahashS626;
      if (_M0L6_2acntS4184 > 1) {
        int32_t _M0L11_2anew__cntS4185 = _M0L6_2acntS4184 - 1;
        Moonbit_object_header(_M0L4_2axS622)->rc = _M0L11_2anew__cntS4185;
        moonbit_incref(_M0L8_2avalueS625);
        moonbit_incref(_M0L6_2akeyS624);
        if (_M0L7_2anextS623) {
          moonbit_incref(_M0L7_2anextS623);
        }
      } else if (_M0L6_2acntS4184 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS622);
      }
      _M0L7_2ahashS626 = _M0L8_2afieldS3991;
      moonbit_incref(_M0L4selfS618);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS618, _M0L6_2akeyS624, _M0L8_2avalueS625, _M0L7_2ahashS626);
      _M0L8_2aparamS620 = _M0L7_2anextS623;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS629
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4003;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS628;
  int32_t _M0L8capacityS2147;
  int32_t _M0L13new__capacityS630;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2142;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2141;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS4002;
  int32_t _M0L6_2atmpS2143;
  int32_t _M0L8capacityS2145;
  int32_t _M0L6_2atmpS2144;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2146;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4001;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS631;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4003 = _M0L4selfS629->$5;
  _M0L9old__headS628 = _M0L8_2afieldS4003;
  _M0L8capacityS2147 = _M0L4selfS629->$2;
  _M0L13new__capacityS630 = _M0L8capacityS2147 << 1;
  _M0L6_2atmpS2142 = 0;
  _M0L6_2atmpS2141
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS630, _M0L6_2atmpS2142);
  _M0L6_2aoldS4002 = _M0L4selfS629->$0;
  if (_M0L9old__headS628) {
    moonbit_incref(_M0L9old__headS628);
  }
  moonbit_decref(_M0L6_2aoldS4002);
  _M0L4selfS629->$0 = _M0L6_2atmpS2141;
  _M0L4selfS629->$2 = _M0L13new__capacityS630;
  _M0L6_2atmpS2143 = _M0L13new__capacityS630 - 1;
  _M0L4selfS629->$3 = _M0L6_2atmpS2143;
  _M0L8capacityS2145 = _M0L4selfS629->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2144 = _M0FPB21calc__grow__threshold(_M0L8capacityS2145);
  _M0L4selfS629->$4 = _M0L6_2atmpS2144;
  _M0L4selfS629->$1 = 0;
  _M0L6_2atmpS2146 = 0;
  _M0L6_2aoldS4001 = _M0L4selfS629->$5;
  if (_M0L6_2aoldS4001) {
    moonbit_decref(_M0L6_2aoldS4001);
  }
  _M0L4selfS629->$5 = _M0L6_2atmpS2146;
  _M0L4selfS629->$6 = -1;
  _M0L8_2aparamS631 = _M0L9old__headS628;
  while (1) {
    if (_M0L8_2aparamS631 == 0) {
      if (_M0L8_2aparamS631) {
        moonbit_decref(_M0L8_2aparamS631);
      }
      moonbit_decref(_M0L4selfS629);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS632 =
        _M0L8_2aparamS631;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS633 =
        _M0L7_2aSomeS632;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4000 =
        _M0L4_2axS633->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS634 =
        _M0L8_2afieldS4000;
      int32_t _M0L6_2akeyS635 = _M0L4_2axS633->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3999 =
        _M0L4_2axS633->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS636 =
        _M0L8_2afieldS3999;
      int32_t _M0L8_2afieldS3998 = _M0L4_2axS633->$3;
      int32_t _M0L6_2acntS4186 = Moonbit_object_header(_M0L4_2axS633)->rc;
      int32_t _M0L7_2ahashS637;
      if (_M0L6_2acntS4186 > 1) {
        int32_t _M0L11_2anew__cntS4187 = _M0L6_2acntS4186 - 1;
        Moonbit_object_header(_M0L4_2axS633)->rc = _M0L11_2anew__cntS4187;
        moonbit_incref(_M0L8_2avalueS636);
        if (_M0L7_2anextS634) {
          moonbit_incref(_M0L7_2anextS634);
        }
      } else if (_M0L6_2acntS4186 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS633);
      }
      _M0L7_2ahashS637 = _M0L8_2afieldS3998;
      moonbit_incref(_M0L4selfS629);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS629, _M0L6_2akeyS635, _M0L8_2avalueS636, _M0L7_2ahashS637);
      _M0L8_2aparamS631 = _M0L7_2anextS634;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS588,
  moonbit_string_t _M0L3keyS594,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS595,
  int32_t _M0L4hashS590
) {
  int32_t _M0L14capacity__maskS2115;
  int32_t _M0L6_2atmpS2114;
  int32_t _M0L3pslS585;
  int32_t _M0L3idxS586;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2115 = _M0L4selfS588->$3;
  _M0L6_2atmpS2114 = _M0L4hashS590 & _M0L14capacity__maskS2115;
  _M0L3pslS585 = 0;
  _M0L3idxS586 = _M0L6_2atmpS2114;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4008 =
      _M0L4selfS588->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2113 =
      _M0L8_2afieldS4008;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4007;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS587;
    if (
      _M0L3idxS586 < 0
      || _M0L3idxS586 >= Moonbit_array_length(_M0L7entriesS2113)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4007
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2113[
        _M0L3idxS586
      ];
    _M0L7_2abindS587 = _M0L6_2atmpS4007;
    if (_M0L7_2abindS587 == 0) {
      int32_t _M0L4sizeS2098 = _M0L4selfS588->$1;
      int32_t _M0L8grow__atS2099 = _M0L4selfS588->$4;
      int32_t _M0L7_2abindS591;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS592;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS593;
      if (_M0L4sizeS2098 >= _M0L8grow__atS2099) {
        int32_t _M0L14capacity__maskS2101;
        int32_t _M0L6_2atmpS2100;
        moonbit_incref(_M0L4selfS588);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS588);
        _M0L14capacity__maskS2101 = _M0L4selfS588->$3;
        _M0L6_2atmpS2100 = _M0L4hashS590 & _M0L14capacity__maskS2101;
        _M0L3pslS585 = 0;
        _M0L3idxS586 = _M0L6_2atmpS2100;
        continue;
      }
      _M0L7_2abindS591 = _M0L4selfS588->$6;
      _M0L7_2abindS592 = 0;
      _M0L5entryS593
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS593)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS593->$0 = _M0L7_2abindS591;
      _M0L5entryS593->$1 = _M0L7_2abindS592;
      _M0L5entryS593->$2 = _M0L3pslS585;
      _M0L5entryS593->$3 = _M0L4hashS590;
      _M0L5entryS593->$4 = _M0L3keyS594;
      _M0L5entryS593->$5 = _M0L5valueS595;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS588, _M0L3idxS586, _M0L5entryS593);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS596 =
        _M0L7_2abindS587;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS597 =
        _M0L7_2aSomeS596;
      int32_t _M0L4hashS2103 = _M0L14_2acurr__entryS597->$3;
      int32_t _if__result_4308;
      int32_t _M0L3pslS2104;
      int32_t _M0L6_2atmpS2109;
      int32_t _M0L6_2atmpS2111;
      int32_t _M0L14capacity__maskS2112;
      int32_t _M0L6_2atmpS2110;
      if (_M0L4hashS2103 == _M0L4hashS590) {
        moonbit_string_t _M0L8_2afieldS4006 = _M0L14_2acurr__entryS597->$4;
        moonbit_string_t _M0L3keyS2102 = _M0L8_2afieldS4006;
        int32_t _M0L6_2atmpS4005;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4005
        = moonbit_val_array_equal(_M0L3keyS2102, _M0L3keyS594);
        _if__result_4308 = _M0L6_2atmpS4005;
      } else {
        _if__result_4308 = 0;
      }
      if (_if__result_4308) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4004;
        moonbit_incref(_M0L14_2acurr__entryS597);
        moonbit_decref(_M0L3keyS594);
        moonbit_decref(_M0L4selfS588);
        _M0L6_2aoldS4004 = _M0L14_2acurr__entryS597->$5;
        moonbit_decref(_M0L6_2aoldS4004);
        _M0L14_2acurr__entryS597->$5 = _M0L5valueS595;
        moonbit_decref(_M0L14_2acurr__entryS597);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS597);
      }
      _M0L3pslS2104 = _M0L14_2acurr__entryS597->$2;
      if (_M0L3pslS585 > _M0L3pslS2104) {
        int32_t _M0L4sizeS2105 = _M0L4selfS588->$1;
        int32_t _M0L8grow__atS2106 = _M0L4selfS588->$4;
        int32_t _M0L7_2abindS598;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS599;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS600;
        if (_M0L4sizeS2105 >= _M0L8grow__atS2106) {
          int32_t _M0L14capacity__maskS2108;
          int32_t _M0L6_2atmpS2107;
          moonbit_decref(_M0L14_2acurr__entryS597);
          moonbit_incref(_M0L4selfS588);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS588);
          _M0L14capacity__maskS2108 = _M0L4selfS588->$3;
          _M0L6_2atmpS2107 = _M0L4hashS590 & _M0L14capacity__maskS2108;
          _M0L3pslS585 = 0;
          _M0L3idxS586 = _M0L6_2atmpS2107;
          continue;
        }
        moonbit_incref(_M0L4selfS588);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS588, _M0L3idxS586, _M0L14_2acurr__entryS597);
        _M0L7_2abindS598 = _M0L4selfS588->$6;
        _M0L7_2abindS599 = 0;
        _M0L5entryS600
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS600)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS600->$0 = _M0L7_2abindS598;
        _M0L5entryS600->$1 = _M0L7_2abindS599;
        _M0L5entryS600->$2 = _M0L3pslS585;
        _M0L5entryS600->$3 = _M0L4hashS590;
        _M0L5entryS600->$4 = _M0L3keyS594;
        _M0L5entryS600->$5 = _M0L5valueS595;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS588, _M0L3idxS586, _M0L5entryS600);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS597);
      }
      _M0L6_2atmpS2109 = _M0L3pslS585 + 1;
      _M0L6_2atmpS2111 = _M0L3idxS586 + 1;
      _M0L14capacity__maskS2112 = _M0L4selfS588->$3;
      _M0L6_2atmpS2110 = _M0L6_2atmpS2111 & _M0L14capacity__maskS2112;
      _M0L3pslS585 = _M0L6_2atmpS2109;
      _M0L3idxS586 = _M0L6_2atmpS2110;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS604,
  int32_t _M0L3keyS610,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS611,
  int32_t _M0L4hashS606
) {
  int32_t _M0L14capacity__maskS2133;
  int32_t _M0L6_2atmpS2132;
  int32_t _M0L3pslS601;
  int32_t _M0L3idxS602;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2133 = _M0L4selfS604->$3;
  _M0L6_2atmpS2132 = _M0L4hashS606 & _M0L14capacity__maskS2133;
  _M0L3pslS601 = 0;
  _M0L3idxS602 = _M0L6_2atmpS2132;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4011 =
      _M0L4selfS604->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2131 =
      _M0L8_2afieldS4011;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4010;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS603;
    if (
      _M0L3idxS602 < 0
      || _M0L3idxS602 >= Moonbit_array_length(_M0L7entriesS2131)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4010
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2131[
        _M0L3idxS602
      ];
    _M0L7_2abindS603 = _M0L6_2atmpS4010;
    if (_M0L7_2abindS603 == 0) {
      int32_t _M0L4sizeS2116 = _M0L4selfS604->$1;
      int32_t _M0L8grow__atS2117 = _M0L4selfS604->$4;
      int32_t _M0L7_2abindS607;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS608;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS609;
      if (_M0L4sizeS2116 >= _M0L8grow__atS2117) {
        int32_t _M0L14capacity__maskS2119;
        int32_t _M0L6_2atmpS2118;
        moonbit_incref(_M0L4selfS604);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS604);
        _M0L14capacity__maskS2119 = _M0L4selfS604->$3;
        _M0L6_2atmpS2118 = _M0L4hashS606 & _M0L14capacity__maskS2119;
        _M0L3pslS601 = 0;
        _M0L3idxS602 = _M0L6_2atmpS2118;
        continue;
      }
      _M0L7_2abindS607 = _M0L4selfS604->$6;
      _M0L7_2abindS608 = 0;
      _M0L5entryS609
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS609)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS609->$0 = _M0L7_2abindS607;
      _M0L5entryS609->$1 = _M0L7_2abindS608;
      _M0L5entryS609->$2 = _M0L3pslS601;
      _M0L5entryS609->$3 = _M0L4hashS606;
      _M0L5entryS609->$4 = _M0L3keyS610;
      _M0L5entryS609->$5 = _M0L5valueS611;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS604, _M0L3idxS602, _M0L5entryS609);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS612 =
        _M0L7_2abindS603;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS613 =
        _M0L7_2aSomeS612;
      int32_t _M0L4hashS2121 = _M0L14_2acurr__entryS613->$3;
      int32_t _if__result_4310;
      int32_t _M0L3pslS2122;
      int32_t _M0L6_2atmpS2127;
      int32_t _M0L6_2atmpS2129;
      int32_t _M0L14capacity__maskS2130;
      int32_t _M0L6_2atmpS2128;
      if (_M0L4hashS2121 == _M0L4hashS606) {
        int32_t _M0L3keyS2120 = _M0L14_2acurr__entryS613->$4;
        _if__result_4310 = _M0L3keyS2120 == _M0L3keyS610;
      } else {
        _if__result_4310 = 0;
      }
      if (_if__result_4310) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS4009;
        moonbit_incref(_M0L14_2acurr__entryS613);
        moonbit_decref(_M0L4selfS604);
        _M0L6_2aoldS4009 = _M0L14_2acurr__entryS613->$5;
        moonbit_decref(_M0L6_2aoldS4009);
        _M0L14_2acurr__entryS613->$5 = _M0L5valueS611;
        moonbit_decref(_M0L14_2acurr__entryS613);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS613);
      }
      _M0L3pslS2122 = _M0L14_2acurr__entryS613->$2;
      if (_M0L3pslS601 > _M0L3pslS2122) {
        int32_t _M0L4sizeS2123 = _M0L4selfS604->$1;
        int32_t _M0L8grow__atS2124 = _M0L4selfS604->$4;
        int32_t _M0L7_2abindS614;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS615;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS616;
        if (_M0L4sizeS2123 >= _M0L8grow__atS2124) {
          int32_t _M0L14capacity__maskS2126;
          int32_t _M0L6_2atmpS2125;
          moonbit_decref(_M0L14_2acurr__entryS613);
          moonbit_incref(_M0L4selfS604);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS604);
          _M0L14capacity__maskS2126 = _M0L4selfS604->$3;
          _M0L6_2atmpS2125 = _M0L4hashS606 & _M0L14capacity__maskS2126;
          _M0L3pslS601 = 0;
          _M0L3idxS602 = _M0L6_2atmpS2125;
          continue;
        }
        moonbit_incref(_M0L4selfS604);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS604, _M0L3idxS602, _M0L14_2acurr__entryS613);
        _M0L7_2abindS614 = _M0L4selfS604->$6;
        _M0L7_2abindS615 = 0;
        _M0L5entryS616
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS616)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS616->$0 = _M0L7_2abindS614;
        _M0L5entryS616->$1 = _M0L7_2abindS615;
        _M0L5entryS616->$2 = _M0L3pslS601;
        _M0L5entryS616->$3 = _M0L4hashS606;
        _M0L5entryS616->$4 = _M0L3keyS610;
        _M0L5entryS616->$5 = _M0L5valueS611;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS604, _M0L3idxS602, _M0L5entryS616);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS613);
      }
      _M0L6_2atmpS2127 = _M0L3pslS601 + 1;
      _M0L6_2atmpS2129 = _M0L3idxS602 + 1;
      _M0L14capacity__maskS2130 = _M0L4selfS604->$3;
      _M0L6_2atmpS2128 = _M0L6_2atmpS2129 & _M0L14capacity__maskS2130;
      _M0L3pslS601 = _M0L6_2atmpS2127;
      _M0L3idxS602 = _M0L6_2atmpS2128;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS569,
  int32_t _M0L3idxS574,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS573
) {
  int32_t _M0L3pslS2081;
  int32_t _M0L6_2atmpS2077;
  int32_t _M0L6_2atmpS2079;
  int32_t _M0L14capacity__maskS2080;
  int32_t _M0L6_2atmpS2078;
  int32_t _M0L3pslS565;
  int32_t _M0L3idxS566;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS567;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2081 = _M0L5entryS573->$2;
  _M0L6_2atmpS2077 = _M0L3pslS2081 + 1;
  _M0L6_2atmpS2079 = _M0L3idxS574 + 1;
  _M0L14capacity__maskS2080 = _M0L4selfS569->$3;
  _M0L6_2atmpS2078 = _M0L6_2atmpS2079 & _M0L14capacity__maskS2080;
  _M0L3pslS565 = _M0L6_2atmpS2077;
  _M0L3idxS566 = _M0L6_2atmpS2078;
  _M0L5entryS567 = _M0L5entryS573;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4013 =
      _M0L4selfS569->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2076 =
      _M0L8_2afieldS4013;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4012;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS568;
    if (
      _M0L3idxS566 < 0
      || _M0L3idxS566 >= Moonbit_array_length(_M0L7entriesS2076)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4012
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2076[
        _M0L3idxS566
      ];
    _M0L7_2abindS568 = _M0L6_2atmpS4012;
    if (_M0L7_2abindS568 == 0) {
      _M0L5entryS567->$2 = _M0L3pslS565;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS569, _M0L5entryS567, _M0L3idxS566);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS571 =
        _M0L7_2abindS568;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS572 =
        _M0L7_2aSomeS571;
      int32_t _M0L3pslS2066 = _M0L14_2acurr__entryS572->$2;
      if (_M0L3pslS565 > _M0L3pslS2066) {
        int32_t _M0L3pslS2071;
        int32_t _M0L6_2atmpS2067;
        int32_t _M0L6_2atmpS2069;
        int32_t _M0L14capacity__maskS2070;
        int32_t _M0L6_2atmpS2068;
        _M0L5entryS567->$2 = _M0L3pslS565;
        moonbit_incref(_M0L14_2acurr__entryS572);
        moonbit_incref(_M0L4selfS569);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS569, _M0L5entryS567, _M0L3idxS566);
        _M0L3pslS2071 = _M0L14_2acurr__entryS572->$2;
        _M0L6_2atmpS2067 = _M0L3pslS2071 + 1;
        _M0L6_2atmpS2069 = _M0L3idxS566 + 1;
        _M0L14capacity__maskS2070 = _M0L4selfS569->$3;
        _M0L6_2atmpS2068 = _M0L6_2atmpS2069 & _M0L14capacity__maskS2070;
        _M0L3pslS565 = _M0L6_2atmpS2067;
        _M0L3idxS566 = _M0L6_2atmpS2068;
        _M0L5entryS567 = _M0L14_2acurr__entryS572;
        continue;
      } else {
        int32_t _M0L6_2atmpS2072 = _M0L3pslS565 + 1;
        int32_t _M0L6_2atmpS2074 = _M0L3idxS566 + 1;
        int32_t _M0L14capacity__maskS2075 = _M0L4selfS569->$3;
        int32_t _M0L6_2atmpS2073 =
          _M0L6_2atmpS2074 & _M0L14capacity__maskS2075;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_4312 =
          _M0L5entryS567;
        _M0L3pslS565 = _M0L6_2atmpS2072;
        _M0L3idxS566 = _M0L6_2atmpS2073;
        _M0L5entryS567 = _tmp_4312;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS579,
  int32_t _M0L3idxS584,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS583
) {
  int32_t _M0L3pslS2097;
  int32_t _M0L6_2atmpS2093;
  int32_t _M0L6_2atmpS2095;
  int32_t _M0L14capacity__maskS2096;
  int32_t _M0L6_2atmpS2094;
  int32_t _M0L3pslS575;
  int32_t _M0L3idxS576;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS577;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2097 = _M0L5entryS583->$2;
  _M0L6_2atmpS2093 = _M0L3pslS2097 + 1;
  _M0L6_2atmpS2095 = _M0L3idxS584 + 1;
  _M0L14capacity__maskS2096 = _M0L4selfS579->$3;
  _M0L6_2atmpS2094 = _M0L6_2atmpS2095 & _M0L14capacity__maskS2096;
  _M0L3pslS575 = _M0L6_2atmpS2093;
  _M0L3idxS576 = _M0L6_2atmpS2094;
  _M0L5entryS577 = _M0L5entryS583;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4015 =
      _M0L4selfS579->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2092 =
      _M0L8_2afieldS4015;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4014;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS578;
    if (
      _M0L3idxS576 < 0
      || _M0L3idxS576 >= Moonbit_array_length(_M0L7entriesS2092)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4014
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2092[
        _M0L3idxS576
      ];
    _M0L7_2abindS578 = _M0L6_2atmpS4014;
    if (_M0L7_2abindS578 == 0) {
      _M0L5entryS577->$2 = _M0L3pslS575;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS579, _M0L5entryS577, _M0L3idxS576);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS581 =
        _M0L7_2abindS578;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS582 =
        _M0L7_2aSomeS581;
      int32_t _M0L3pslS2082 = _M0L14_2acurr__entryS582->$2;
      if (_M0L3pslS575 > _M0L3pslS2082) {
        int32_t _M0L3pslS2087;
        int32_t _M0L6_2atmpS2083;
        int32_t _M0L6_2atmpS2085;
        int32_t _M0L14capacity__maskS2086;
        int32_t _M0L6_2atmpS2084;
        _M0L5entryS577->$2 = _M0L3pslS575;
        moonbit_incref(_M0L14_2acurr__entryS582);
        moonbit_incref(_M0L4selfS579);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS579, _M0L5entryS577, _M0L3idxS576);
        _M0L3pslS2087 = _M0L14_2acurr__entryS582->$2;
        _M0L6_2atmpS2083 = _M0L3pslS2087 + 1;
        _M0L6_2atmpS2085 = _M0L3idxS576 + 1;
        _M0L14capacity__maskS2086 = _M0L4selfS579->$3;
        _M0L6_2atmpS2084 = _M0L6_2atmpS2085 & _M0L14capacity__maskS2086;
        _M0L3pslS575 = _M0L6_2atmpS2083;
        _M0L3idxS576 = _M0L6_2atmpS2084;
        _M0L5entryS577 = _M0L14_2acurr__entryS582;
        continue;
      } else {
        int32_t _M0L6_2atmpS2088 = _M0L3pslS575 + 1;
        int32_t _M0L6_2atmpS2090 = _M0L3idxS576 + 1;
        int32_t _M0L14capacity__maskS2091 = _M0L4selfS579->$3;
        int32_t _M0L6_2atmpS2089 =
          _M0L6_2atmpS2090 & _M0L14capacity__maskS2091;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_4314 =
          _M0L5entryS577;
        _M0L3pslS575 = _M0L6_2atmpS2088;
        _M0L3idxS576 = _M0L6_2atmpS2089;
        _M0L5entryS577 = _tmp_4314;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS553,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS555,
  int32_t _M0L8new__idxS554
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4018;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2062;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2063;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4017;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4016;
  int32_t _M0L6_2acntS4188;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS556;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4018 = _M0L4selfS553->$0;
  _M0L7entriesS2062 = _M0L8_2afieldS4018;
  moonbit_incref(_M0L5entryS555);
  _M0L6_2atmpS2063 = _M0L5entryS555;
  if (
    _M0L8new__idxS554 < 0
    || _M0L8new__idxS554 >= Moonbit_array_length(_M0L7entriesS2062)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4017
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2062[
      _M0L8new__idxS554
    ];
  if (_M0L6_2aoldS4017) {
    moonbit_decref(_M0L6_2aoldS4017);
  }
  _M0L7entriesS2062[_M0L8new__idxS554] = _M0L6_2atmpS2063;
  _M0L8_2afieldS4016 = _M0L5entryS555->$1;
  _M0L6_2acntS4188 = Moonbit_object_header(_M0L5entryS555)->rc;
  if (_M0L6_2acntS4188 > 1) {
    int32_t _M0L11_2anew__cntS4191 = _M0L6_2acntS4188 - 1;
    Moonbit_object_header(_M0L5entryS555)->rc = _M0L11_2anew__cntS4191;
    if (_M0L8_2afieldS4016) {
      moonbit_incref(_M0L8_2afieldS4016);
    }
  } else if (_M0L6_2acntS4188 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4190 =
      _M0L5entryS555->$5;
    moonbit_string_t _M0L8_2afieldS4189;
    moonbit_decref(_M0L8_2afieldS4190);
    _M0L8_2afieldS4189 = _M0L5entryS555->$4;
    moonbit_decref(_M0L8_2afieldS4189);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS555);
  }
  _M0L7_2abindS556 = _M0L8_2afieldS4016;
  if (_M0L7_2abindS556 == 0) {
    if (_M0L7_2abindS556) {
      moonbit_decref(_M0L7_2abindS556);
    }
    _M0L4selfS553->$6 = _M0L8new__idxS554;
    moonbit_decref(_M0L4selfS553);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS557;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS558;
    moonbit_decref(_M0L4selfS553);
    _M0L7_2aSomeS557 = _M0L7_2abindS556;
    _M0L7_2anextS558 = _M0L7_2aSomeS557;
    _M0L7_2anextS558->$0 = _M0L8new__idxS554;
    moonbit_decref(_M0L7_2anextS558);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS559,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS561,
  int32_t _M0L8new__idxS560
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4021;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2064;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2065;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4020;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4019;
  int32_t _M0L6_2acntS4192;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS562;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4021 = _M0L4selfS559->$0;
  _M0L7entriesS2064 = _M0L8_2afieldS4021;
  moonbit_incref(_M0L5entryS561);
  _M0L6_2atmpS2065 = _M0L5entryS561;
  if (
    _M0L8new__idxS560 < 0
    || _M0L8new__idxS560 >= Moonbit_array_length(_M0L7entriesS2064)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4020
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2064[
      _M0L8new__idxS560
    ];
  if (_M0L6_2aoldS4020) {
    moonbit_decref(_M0L6_2aoldS4020);
  }
  _M0L7entriesS2064[_M0L8new__idxS560] = _M0L6_2atmpS2065;
  _M0L8_2afieldS4019 = _M0L5entryS561->$1;
  _M0L6_2acntS4192 = Moonbit_object_header(_M0L5entryS561)->rc;
  if (_M0L6_2acntS4192 > 1) {
    int32_t _M0L11_2anew__cntS4194 = _M0L6_2acntS4192 - 1;
    Moonbit_object_header(_M0L5entryS561)->rc = _M0L11_2anew__cntS4194;
    if (_M0L8_2afieldS4019) {
      moonbit_incref(_M0L8_2afieldS4019);
    }
  } else if (_M0L6_2acntS4192 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4193 =
      _M0L5entryS561->$5;
    moonbit_decref(_M0L8_2afieldS4193);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS561);
  }
  _M0L7_2abindS562 = _M0L8_2afieldS4019;
  if (_M0L7_2abindS562 == 0) {
    if (_M0L7_2abindS562) {
      moonbit_decref(_M0L7_2abindS562);
    }
    _M0L4selfS559->$6 = _M0L8new__idxS560;
    moonbit_decref(_M0L4selfS559);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS563;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS564;
    moonbit_decref(_M0L4selfS559);
    _M0L7_2aSomeS563 = _M0L7_2abindS562;
    _M0L7_2anextS564 = _M0L7_2aSomeS563;
    _M0L7_2anextS564->$0 = _M0L8new__idxS560;
    moonbit_decref(_M0L7_2anextS564);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS546,
  int32_t _M0L3idxS548,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS547
) {
  int32_t _M0L7_2abindS545;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4023;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2049;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2050;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4022;
  int32_t _M0L4sizeS2052;
  int32_t _M0L6_2atmpS2051;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS545 = _M0L4selfS546->$6;
  switch (_M0L7_2abindS545) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2044;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4024;
      moonbit_incref(_M0L5entryS547);
      _M0L6_2atmpS2044 = _M0L5entryS547;
      _M0L6_2aoldS4024 = _M0L4selfS546->$5;
      if (_M0L6_2aoldS4024) {
        moonbit_decref(_M0L6_2aoldS4024);
      }
      _M0L4selfS546->$5 = _M0L6_2atmpS2044;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4027 =
        _M0L4selfS546->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2048 =
        _M0L8_2afieldS4027;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4026;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2047;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2045;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2046;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4025;
      if (
        _M0L7_2abindS545 < 0
        || _M0L7_2abindS545 >= Moonbit_array_length(_M0L7entriesS2048)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4026
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2048[
          _M0L7_2abindS545
        ];
      _M0L6_2atmpS2047 = _M0L6_2atmpS4026;
      if (_M0L6_2atmpS2047) {
        moonbit_incref(_M0L6_2atmpS2047);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2045
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2047);
      moonbit_incref(_M0L5entryS547);
      _M0L6_2atmpS2046 = _M0L5entryS547;
      _M0L6_2aoldS4025 = _M0L6_2atmpS2045->$1;
      if (_M0L6_2aoldS4025) {
        moonbit_decref(_M0L6_2aoldS4025);
      }
      _M0L6_2atmpS2045->$1 = _M0L6_2atmpS2046;
      moonbit_decref(_M0L6_2atmpS2045);
      break;
    }
  }
  _M0L4selfS546->$6 = _M0L3idxS548;
  _M0L8_2afieldS4023 = _M0L4selfS546->$0;
  _M0L7entriesS2049 = _M0L8_2afieldS4023;
  _M0L6_2atmpS2050 = _M0L5entryS547;
  if (
    _M0L3idxS548 < 0
    || _M0L3idxS548 >= Moonbit_array_length(_M0L7entriesS2049)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4022
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2049[
      _M0L3idxS548
    ];
  if (_M0L6_2aoldS4022) {
    moonbit_decref(_M0L6_2aoldS4022);
  }
  _M0L7entriesS2049[_M0L3idxS548] = _M0L6_2atmpS2050;
  _M0L4sizeS2052 = _M0L4selfS546->$1;
  _M0L6_2atmpS2051 = _M0L4sizeS2052 + 1;
  _M0L4selfS546->$1 = _M0L6_2atmpS2051;
  moonbit_decref(_M0L4selfS546);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS550,
  int32_t _M0L3idxS552,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS551
) {
  int32_t _M0L7_2abindS549;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4029;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2058;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2059;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4028;
  int32_t _M0L4sizeS2061;
  int32_t _M0L6_2atmpS2060;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS549 = _M0L4selfS550->$6;
  switch (_M0L7_2abindS549) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2053;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4030;
      moonbit_incref(_M0L5entryS551);
      _M0L6_2atmpS2053 = _M0L5entryS551;
      _M0L6_2aoldS4030 = _M0L4selfS550->$5;
      if (_M0L6_2aoldS4030) {
        moonbit_decref(_M0L6_2aoldS4030);
      }
      _M0L4selfS550->$5 = _M0L6_2atmpS2053;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4033 =
        _M0L4selfS550->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2057 =
        _M0L8_2afieldS4033;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4032;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2056;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2054;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2055;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4031;
      if (
        _M0L7_2abindS549 < 0
        || _M0L7_2abindS549 >= Moonbit_array_length(_M0L7entriesS2057)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4032
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2057[
          _M0L7_2abindS549
        ];
      _M0L6_2atmpS2056 = _M0L6_2atmpS4032;
      if (_M0L6_2atmpS2056) {
        moonbit_incref(_M0L6_2atmpS2056);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2054
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2056);
      moonbit_incref(_M0L5entryS551);
      _M0L6_2atmpS2055 = _M0L5entryS551;
      _M0L6_2aoldS4031 = _M0L6_2atmpS2054->$1;
      if (_M0L6_2aoldS4031) {
        moonbit_decref(_M0L6_2aoldS4031);
      }
      _M0L6_2atmpS2054->$1 = _M0L6_2atmpS2055;
      moonbit_decref(_M0L6_2atmpS2054);
      break;
    }
  }
  _M0L4selfS550->$6 = _M0L3idxS552;
  _M0L8_2afieldS4029 = _M0L4selfS550->$0;
  _M0L7entriesS2058 = _M0L8_2afieldS4029;
  _M0L6_2atmpS2059 = _M0L5entryS551;
  if (
    _M0L3idxS552 < 0
    || _M0L3idxS552 >= Moonbit_array_length(_M0L7entriesS2058)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4028
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2058[
      _M0L3idxS552
    ];
  if (_M0L6_2aoldS4028) {
    moonbit_decref(_M0L6_2aoldS4028);
  }
  _M0L7entriesS2058[_M0L3idxS552] = _M0L6_2atmpS2059;
  _M0L4sizeS2061 = _M0L4selfS550->$1;
  _M0L6_2atmpS2060 = _M0L4sizeS2061 + 1;
  _M0L4selfS550->$1 = _M0L6_2atmpS2060;
  moonbit_decref(_M0L4selfS550);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS534
) {
  int32_t _M0L8capacityS533;
  int32_t _M0L7_2abindS535;
  int32_t _M0L7_2abindS536;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2042;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS537;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS538;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_4315;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS533
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS534);
  _M0L7_2abindS535 = _M0L8capacityS533 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS536 = _M0FPB21calc__grow__threshold(_M0L8capacityS533);
  _M0L6_2atmpS2042 = 0;
  _M0L7_2abindS537
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS533, _M0L6_2atmpS2042);
  _M0L7_2abindS538 = 0;
  _block_4315
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_4315)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_4315->$0 = _M0L7_2abindS537;
  _block_4315->$1 = 0;
  _block_4315->$2 = _M0L8capacityS533;
  _block_4315->$3 = _M0L7_2abindS535;
  _block_4315->$4 = _M0L7_2abindS536;
  _block_4315->$5 = _M0L7_2abindS538;
  _block_4315->$6 = -1;
  return _block_4315;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS540
) {
  int32_t _M0L8capacityS539;
  int32_t _M0L7_2abindS541;
  int32_t _M0L7_2abindS542;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2043;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS543;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS544;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4316;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS539
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS540);
  _M0L7_2abindS541 = _M0L8capacityS539 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS542 = _M0FPB21calc__grow__threshold(_M0L8capacityS539);
  _M0L6_2atmpS2043 = 0;
  _M0L7_2abindS543
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS539, _M0L6_2atmpS2043);
  _M0L7_2abindS544 = 0;
  _block_4316
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4316)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4316->$0 = _M0L7_2abindS543;
  _block_4316->$1 = 0;
  _block_4316->$2 = _M0L8capacityS539;
  _block_4316->$3 = _M0L7_2abindS541;
  _block_4316->$4 = _M0L7_2abindS542;
  _block_4316->$5 = _M0L7_2abindS544;
  _block_4316->$6 = -1;
  return _block_4316;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS532) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS532 >= 0) {
    int32_t _M0L6_2atmpS2041;
    int32_t _M0L6_2atmpS2040;
    int32_t _M0L6_2atmpS2039;
    int32_t _M0L6_2atmpS2038;
    if (_M0L4selfS532 <= 1) {
      return 1;
    }
    if (_M0L4selfS532 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2041 = _M0L4selfS532 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2040 = moonbit_clz32(_M0L6_2atmpS2041);
    _M0L6_2atmpS2039 = _M0L6_2atmpS2040 - 1;
    _M0L6_2atmpS2038 = 2147483647 >> (_M0L6_2atmpS2039 & 31);
    return _M0L6_2atmpS2038 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS531) {
  int32_t _M0L6_2atmpS2037;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2037 = _M0L8capacityS531 * 13;
  return _M0L6_2atmpS2037 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS527
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS527 == 0) {
    if (_M0L4selfS527) {
      moonbit_decref(_M0L4selfS527);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS528 =
      _M0L4selfS527;
    return _M0L7_2aSomeS528;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS529
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS529 == 0) {
    if (_M0L4selfS529) {
      moonbit_decref(_M0L4selfS529);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS530 =
      _M0L4selfS529;
    return _M0L7_2aSomeS530;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS526
) {
  moonbit_string_t* _M0L6_2atmpS2036;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2036 = _M0L4selfS526;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS2036);
}

uint64_t _M0MPC15array13ReadOnlyArray2atGmE(
  uint64_t* _M0L4selfS522,
  int32_t _M0L5indexS523
) {
  uint64_t* _M0L6_2atmpS2034;
  uint64_t _M0L6_2atmpS4034;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2034 = _M0L4selfS522;
  if (
    _M0L5indexS523 < 0
    || _M0L5indexS523 >= Moonbit_array_length(_M0L6_2atmpS2034)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4034 = (uint64_t)_M0L6_2atmpS2034[_M0L5indexS523];
  moonbit_decref(_M0L6_2atmpS2034);
  return _M0L6_2atmpS4034;
}

uint32_t _M0MPC15array13ReadOnlyArray2atGjE(
  uint32_t* _M0L4selfS524,
  int32_t _M0L5indexS525
) {
  uint32_t* _M0L6_2atmpS2035;
  uint32_t _M0L6_2atmpS4035;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS2035 = _M0L4selfS524;
  if (
    _M0L5indexS525 < 0
    || _M0L5indexS525 >= Moonbit_array_length(_M0L6_2atmpS2035)
  ) {
    #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4035 = (uint32_t)_M0L6_2atmpS2035[_M0L5indexS525];
  moonbit_decref(_M0L6_2atmpS2035);
  return _M0L6_2atmpS4035;
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS521
) {
  moonbit_string_t* _M0L6_2atmpS2032;
  int32_t _M0L6_2atmpS4036;
  int32_t _M0L6_2atmpS2033;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS2031;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS521);
  _M0L6_2atmpS2032 = _M0L4selfS521;
  _M0L6_2atmpS4036 = Moonbit_array_length(_M0L4selfS521);
  moonbit_decref(_M0L4selfS521);
  _M0L6_2atmpS2033 = _M0L6_2atmpS4036;
  _M0L6_2atmpS2031
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS2033, _M0L6_2atmpS2032
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS2031);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS519
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS518;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l570__* _closure_4317;
  struct _M0TWEOs* _M0L6_2atmpS2019;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS518
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS518)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS518->$0 = 0;
  _closure_4317
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l570__));
  Moonbit_object_header(_closure_4317)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l570__, $0_0) >> 2, 2, 0);
  _closure_4317->code = &_M0MPC15array9ArrayView4iterGsEC2020l570;
  _closure_4317->$0_0 = _M0L4selfS519.$0;
  _closure_4317->$0_1 = _M0L4selfS519.$1;
  _closure_4317->$0_2 = _M0L4selfS519.$2;
  _closure_4317->$1 = _M0L1iS518;
  _M0L6_2atmpS2019 = (struct _M0TWEOs*)_closure_4317;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS2019);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC2020l570(
  struct _M0TWEOs* _M0L6_2aenvS2021
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l570__* _M0L14_2acasted__envS2022;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4041;
  struct _M0TPC13ref3RefGiE* _M0L1iS518;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS4040;
  int32_t _M0L6_2acntS4195;
  struct _M0TPB9ArrayViewGsE _M0L4selfS519;
  int32_t _M0L3valS2023;
  int32_t _M0L6_2atmpS2024;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS2022
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u2020__l570__*)_M0L6_2aenvS2021;
  _M0L8_2afieldS4041 = _M0L14_2acasted__envS2022->$1;
  _M0L1iS518 = _M0L8_2afieldS4041;
  _M0L8_2afieldS4040
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS2022->$0_1,
      _M0L14_2acasted__envS2022->$0_2,
      _M0L14_2acasted__envS2022->$0_0
  };
  _M0L6_2acntS4195 = Moonbit_object_header(_M0L14_2acasted__envS2022)->rc;
  if (_M0L6_2acntS4195 > 1) {
    int32_t _M0L11_2anew__cntS4196 = _M0L6_2acntS4195 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2022)->rc
    = _M0L11_2anew__cntS4196;
    moonbit_incref(_M0L1iS518);
    moonbit_incref(_M0L8_2afieldS4040.$0);
  } else if (_M0L6_2acntS4195 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS2022);
  }
  _M0L4selfS519 = _M0L8_2afieldS4040;
  _M0L3valS2023 = _M0L1iS518->$0;
  moonbit_incref(_M0L4selfS519.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS2024 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS519);
  if (_M0L3valS2023 < _M0L6_2atmpS2024) {
    moonbit_string_t* _M0L8_2afieldS4039 = _M0L4selfS519.$0;
    moonbit_string_t* _M0L3bufS2027 = _M0L8_2afieldS4039;
    int32_t _M0L8_2afieldS4038 = _M0L4selfS519.$1;
    int32_t _M0L5startS2029 = _M0L8_2afieldS4038;
    int32_t _M0L3valS2030 = _M0L1iS518->$0;
    int32_t _M0L6_2atmpS2028 = _M0L5startS2029 + _M0L3valS2030;
    moonbit_string_t _M0L6_2atmpS4037 =
      (moonbit_string_t)_M0L3bufS2027[_M0L6_2atmpS2028];
    moonbit_string_t _M0L4elemS520;
    int32_t _M0L3valS2026;
    int32_t _M0L6_2atmpS2025;
    moonbit_incref(_M0L6_2atmpS4037);
    moonbit_decref(_M0L3bufS2027);
    _M0L4elemS520 = _M0L6_2atmpS4037;
    _M0L3valS2026 = _M0L1iS518->$0;
    _M0L6_2atmpS2025 = _M0L3valS2026 + 1;
    _M0L1iS518->$0 = _M0L6_2atmpS2025;
    moonbit_decref(_M0L1iS518);
    return _M0L4elemS520;
  } else {
    moonbit_decref(_M0L4selfS519.$0);
    moonbit_decref(_M0L1iS518);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS517
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS517;
}

int32_t _M0IPC16uint646UInt64PB4Show6output(
  uint64_t _M0L4selfS516,
  struct _M0TPB6Logger _M0L6loggerS515
) {
  moonbit_string_t _M0L6_2atmpS2018;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2018
  = _M0MPC16uint646UInt6418to__string_2einner(_M0L4selfS516, 10);
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS515.$0->$method_0(_M0L6loggerS515.$1, _M0L6_2atmpS2018);
  return 0;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS514,
  struct _M0TPB6Logger _M0L6loggerS513
) {
  moonbit_string_t _M0L6_2atmpS2017;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS2017 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS514, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS513.$0->$method_0(_M0L6loggerS513.$1, _M0L6_2atmpS2017);
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS508) {
  int32_t _M0L3lenS507;
  struct _M0TPC13ref3RefGiE* _M0L5indexS509;
  struct _M0R38String_3a_3aiter_2eanon__u2001__l247__* _closure_4318;
  struct _M0TWEOc* _M0L6_2atmpS2000;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS507 = Moonbit_array_length(_M0L4selfS508);
  _M0L5indexS509
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS509)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS509->$0 = 0;
  _closure_4318
  = (struct _M0R38String_3a_3aiter_2eanon__u2001__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u2001__l247__));
  Moonbit_object_header(_closure_4318)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u2001__l247__, $0) >> 2, 2, 0);
  _closure_4318->code = &_M0MPC16string6String4iterC2001l247;
  _closure_4318->$0 = _M0L5indexS509;
  _closure_4318->$1 = _M0L4selfS508;
  _closure_4318->$2 = _M0L3lenS507;
  _M0L6_2atmpS2000 = (struct _M0TWEOc*)_closure_4318;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS2000);
}

int32_t _M0MPC16string6String4iterC2001l247(
  struct _M0TWEOc* _M0L6_2aenvS2002
) {
  struct _M0R38String_3a_3aiter_2eanon__u2001__l247__* _M0L14_2acasted__envS2003;
  int32_t _M0L3lenS507;
  moonbit_string_t _M0L8_2afieldS4044;
  moonbit_string_t _M0L4selfS508;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4043;
  int32_t _M0L6_2acntS4197;
  struct _M0TPC13ref3RefGiE* _M0L5indexS509;
  int32_t _M0L3valS2004;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS2003
  = (struct _M0R38String_3a_3aiter_2eanon__u2001__l247__*)_M0L6_2aenvS2002;
  _M0L3lenS507 = _M0L14_2acasted__envS2003->$2;
  _M0L8_2afieldS4044 = _M0L14_2acasted__envS2003->$1;
  _M0L4selfS508 = _M0L8_2afieldS4044;
  _M0L8_2afieldS4043 = _M0L14_2acasted__envS2003->$0;
  _M0L6_2acntS4197 = Moonbit_object_header(_M0L14_2acasted__envS2003)->rc;
  if (_M0L6_2acntS4197 > 1) {
    int32_t _M0L11_2anew__cntS4198 = _M0L6_2acntS4197 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2003)->rc
    = _M0L11_2anew__cntS4198;
    moonbit_incref(_M0L4selfS508);
    moonbit_incref(_M0L8_2afieldS4043);
  } else if (_M0L6_2acntS4197 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS2003);
  }
  _M0L5indexS509 = _M0L8_2afieldS4043;
  _M0L3valS2004 = _M0L5indexS509->$0;
  if (_M0L3valS2004 < _M0L3lenS507) {
    int32_t _M0L3valS2016 = _M0L5indexS509->$0;
    int32_t _M0L2c1S510 = _M0L4selfS508[_M0L3valS2016];
    int32_t _if__result_4319;
    int32_t _M0L3valS2014;
    int32_t _M0L6_2atmpS2013;
    int32_t _M0L6_2atmpS2015;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S510)) {
      int32_t _M0L3valS2006 = _M0L5indexS509->$0;
      int32_t _M0L6_2atmpS2005 = _M0L3valS2006 + 1;
      _if__result_4319 = _M0L6_2atmpS2005 < _M0L3lenS507;
    } else {
      _if__result_4319 = 0;
    }
    if (_if__result_4319) {
      int32_t _M0L3valS2012 = _M0L5indexS509->$0;
      int32_t _M0L6_2atmpS2011 = _M0L3valS2012 + 1;
      int32_t _M0L6_2atmpS4042 = _M0L4selfS508[_M0L6_2atmpS2011];
      int32_t _M0L2c2S511;
      moonbit_decref(_M0L4selfS508);
      _M0L2c2S511 = _M0L6_2atmpS4042;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S511)) {
        int32_t _M0L6_2atmpS2009 = (int32_t)_M0L2c1S510;
        int32_t _M0L6_2atmpS2010 = (int32_t)_M0L2c2S511;
        int32_t _M0L1cS512;
        int32_t _M0L3valS2008;
        int32_t _M0L6_2atmpS2007;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS512
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS2009, _M0L6_2atmpS2010);
        _M0L3valS2008 = _M0L5indexS509->$0;
        _M0L6_2atmpS2007 = _M0L3valS2008 + 2;
        _M0L5indexS509->$0 = _M0L6_2atmpS2007;
        moonbit_decref(_M0L5indexS509);
        return _M0L1cS512;
      }
    } else {
      moonbit_decref(_M0L4selfS508);
    }
    _M0L3valS2014 = _M0L5indexS509->$0;
    _M0L6_2atmpS2013 = _M0L3valS2014 + 1;
    _M0L5indexS509->$0 = _M0L6_2atmpS2013;
    moonbit_decref(_M0L5indexS509);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS2015 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S510);
    return _M0L6_2atmpS2015;
  } else {
    moonbit_decref(_M0L5indexS509);
    moonbit_decref(_M0L4selfS508);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS498,
  moonbit_string_t _M0L5valueS500
) {
  int32_t _M0L3lenS1985;
  moonbit_string_t* _M0L6_2atmpS1987;
  int32_t _M0L6_2atmpS4047;
  int32_t _M0L6_2atmpS1986;
  int32_t _M0L6lengthS499;
  moonbit_string_t* _M0L8_2afieldS4046;
  moonbit_string_t* _M0L3bufS1988;
  moonbit_string_t _M0L6_2aoldS4045;
  int32_t _M0L6_2atmpS1989;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1985 = _M0L4selfS498->$1;
  moonbit_incref(_M0L4selfS498);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1987 = _M0MPC15array5Array6bufferGsE(_M0L4selfS498);
  _M0L6_2atmpS4047 = Moonbit_array_length(_M0L6_2atmpS1987);
  moonbit_decref(_M0L6_2atmpS1987);
  _M0L6_2atmpS1986 = _M0L6_2atmpS4047;
  if (_M0L3lenS1985 == _M0L6_2atmpS1986) {
    moonbit_incref(_M0L4selfS498);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS498);
  }
  _M0L6lengthS499 = _M0L4selfS498->$1;
  _M0L8_2afieldS4046 = _M0L4selfS498->$0;
  _M0L3bufS1988 = _M0L8_2afieldS4046;
  _M0L6_2aoldS4045 = (moonbit_string_t)_M0L3bufS1988[_M0L6lengthS499];
  moonbit_decref(_M0L6_2aoldS4045);
  _M0L3bufS1988[_M0L6lengthS499] = _M0L5valueS500;
  _M0L6_2atmpS1989 = _M0L6lengthS499 + 1;
  _M0L4selfS498->$1 = _M0L6_2atmpS1989;
  moonbit_decref(_M0L4selfS498);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS501,
  struct _M0TUsiE* _M0L5valueS503
) {
  int32_t _M0L3lenS1990;
  struct _M0TUsiE** _M0L6_2atmpS1992;
  int32_t _M0L6_2atmpS4050;
  int32_t _M0L6_2atmpS1991;
  int32_t _M0L6lengthS502;
  struct _M0TUsiE** _M0L8_2afieldS4049;
  struct _M0TUsiE** _M0L3bufS1993;
  struct _M0TUsiE* _M0L6_2aoldS4048;
  int32_t _M0L6_2atmpS1994;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1990 = _M0L4selfS501->$1;
  moonbit_incref(_M0L4selfS501);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1992 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS501);
  _M0L6_2atmpS4050 = Moonbit_array_length(_M0L6_2atmpS1992);
  moonbit_decref(_M0L6_2atmpS1992);
  _M0L6_2atmpS1991 = _M0L6_2atmpS4050;
  if (_M0L3lenS1990 == _M0L6_2atmpS1991) {
    moonbit_incref(_M0L4selfS501);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS501);
  }
  _M0L6lengthS502 = _M0L4selfS501->$1;
  _M0L8_2afieldS4049 = _M0L4selfS501->$0;
  _M0L3bufS1993 = _M0L8_2afieldS4049;
  _M0L6_2aoldS4048 = (struct _M0TUsiE*)_M0L3bufS1993[_M0L6lengthS502];
  if (_M0L6_2aoldS4048) {
    moonbit_decref(_M0L6_2aoldS4048);
  }
  _M0L3bufS1993[_M0L6lengthS502] = _M0L5valueS503;
  _M0L6_2atmpS1994 = _M0L6lengthS502 + 1;
  _M0L4selfS501->$1 = _M0L6_2atmpS1994;
  moonbit_decref(_M0L4selfS501);
  return 0;
}

int32_t _M0MPC15array5Array4pushGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS504,
  void* _M0L5valueS506
) {
  int32_t _M0L3lenS1995;
  void** _M0L6_2atmpS1997;
  int32_t _M0L6_2atmpS4053;
  int32_t _M0L6_2atmpS1996;
  int32_t _M0L6lengthS505;
  void** _M0L8_2afieldS4052;
  void** _M0L3bufS1998;
  void* _M0L6_2aoldS4051;
  int32_t _M0L6_2atmpS1999;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1995 = _M0L4selfS504->$1;
  moonbit_incref(_M0L4selfS504);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1997
  = _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(_M0L4selfS504);
  _M0L6_2atmpS4053 = Moonbit_array_length(_M0L6_2atmpS1997);
  moonbit_decref(_M0L6_2atmpS1997);
  _M0L6_2atmpS1996 = _M0L6_2atmpS4053;
  if (_M0L3lenS1995 == _M0L6_2atmpS1996) {
    moonbit_incref(_M0L4selfS504);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(_M0L4selfS504);
  }
  _M0L6lengthS505 = _M0L4selfS504->$1;
  _M0L8_2afieldS4052 = _M0L4selfS504->$0;
  _M0L3bufS1998 = _M0L8_2afieldS4052;
  _M0L6_2aoldS4051 = (void*)_M0L3bufS1998[_M0L6lengthS505];
  moonbit_decref(_M0L6_2aoldS4051);
  _M0L3bufS1998[_M0L6lengthS505] = _M0L5valueS506;
  _M0L6_2atmpS1999 = _M0L6lengthS505 + 1;
  _M0L4selfS504->$1 = _M0L6_2atmpS1999;
  moonbit_decref(_M0L4selfS504);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS490) {
  int32_t _M0L8old__capS489;
  int32_t _M0L8new__capS491;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS489 = _M0L4selfS490->$1;
  if (_M0L8old__capS489 == 0) {
    _M0L8new__capS491 = 8;
  } else {
    _M0L8new__capS491 = _M0L8old__capS489 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS490, _M0L8new__capS491);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS493
) {
  int32_t _M0L8old__capS492;
  int32_t _M0L8new__capS494;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS492 = _M0L4selfS493->$1;
  if (_M0L8old__capS492 == 0) {
    _M0L8new__capS494 = 8;
  } else {
    _M0L8new__capS494 = _M0L8old__capS492 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS493, _M0L8new__capS494);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS496
) {
  int32_t _M0L8old__capS495;
  int32_t _M0L8new__capS497;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS495 = _M0L4selfS496->$1;
  if (_M0L8old__capS495 == 0) {
    _M0L8new__capS497 = 8;
  } else {
    _M0L8new__capS497 = _M0L8old__capS495 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(_M0L4selfS496, _M0L8new__capS497);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS474,
  int32_t _M0L13new__capacityS472
) {
  moonbit_string_t* _M0L8new__bufS471;
  moonbit_string_t* _M0L8_2afieldS4055;
  moonbit_string_t* _M0L8old__bufS473;
  int32_t _M0L8old__capS475;
  int32_t _M0L9copy__lenS476;
  moonbit_string_t* _M0L6_2aoldS4054;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS471
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS472, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS4055 = _M0L4selfS474->$0;
  _M0L8old__bufS473 = _M0L8_2afieldS4055;
  _M0L8old__capS475 = Moonbit_array_length(_M0L8old__bufS473);
  if (_M0L8old__capS475 < _M0L13new__capacityS472) {
    _M0L9copy__lenS476 = _M0L8old__capS475;
  } else {
    _M0L9copy__lenS476 = _M0L13new__capacityS472;
  }
  moonbit_incref(_M0L8old__bufS473);
  moonbit_incref(_M0L8new__bufS471);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS471, 0, _M0L8old__bufS473, 0, _M0L9copy__lenS476);
  _M0L6_2aoldS4054 = _M0L4selfS474->$0;
  moonbit_decref(_M0L6_2aoldS4054);
  _M0L4selfS474->$0 = _M0L8new__bufS471;
  moonbit_decref(_M0L4selfS474);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS480,
  int32_t _M0L13new__capacityS478
) {
  struct _M0TUsiE** _M0L8new__bufS477;
  struct _M0TUsiE** _M0L8_2afieldS4057;
  struct _M0TUsiE** _M0L8old__bufS479;
  int32_t _M0L8old__capS481;
  int32_t _M0L9copy__lenS482;
  struct _M0TUsiE** _M0L6_2aoldS4056;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS477
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS478, 0);
  _M0L8_2afieldS4057 = _M0L4selfS480->$0;
  _M0L8old__bufS479 = _M0L8_2afieldS4057;
  _M0L8old__capS481 = Moonbit_array_length(_M0L8old__bufS479);
  if (_M0L8old__capS481 < _M0L13new__capacityS478) {
    _M0L9copy__lenS482 = _M0L8old__capS481;
  } else {
    _M0L9copy__lenS482 = _M0L13new__capacityS478;
  }
  moonbit_incref(_M0L8old__bufS479);
  moonbit_incref(_M0L8new__bufS477);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS477, 0, _M0L8old__bufS479, 0, _M0L9copy__lenS482);
  _M0L6_2aoldS4056 = _M0L4selfS480->$0;
  moonbit_decref(_M0L6_2aoldS4056);
  _M0L4selfS480->$0 = _M0L8new__bufS477;
  moonbit_decref(_M0L4selfS480);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS486,
  int32_t _M0L13new__capacityS484
) {
  void** _M0L8new__bufS483;
  void** _M0L8_2afieldS4059;
  void** _M0L8old__bufS485;
  int32_t _M0L8old__capS487;
  int32_t _M0L9copy__lenS488;
  void** _M0L6_2aoldS4058;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS483
  = (void**)moonbit_make_ref_array(_M0L13new__capacityS484, (struct moonbit_object*)&moonbit_constant_constructor_0 + 1);
  _M0L8_2afieldS4059 = _M0L4selfS486->$0;
  _M0L8old__bufS485 = _M0L8_2afieldS4059;
  _M0L8old__capS487 = Moonbit_array_length(_M0L8old__bufS485);
  if (_M0L8old__capS487 < _M0L13new__capacityS484) {
    _M0L9copy__lenS488 = _M0L8old__capS487;
  } else {
    _M0L9copy__lenS488 = _M0L13new__capacityS484;
  }
  moonbit_incref(_M0L8old__bufS485);
  moonbit_incref(_M0L8new__bufS483);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGRPC14json10WriteFrameE(_M0L8new__bufS483, 0, _M0L8old__bufS485, 0, _M0L9copy__lenS488);
  _M0L6_2aoldS4058 = _M0L4selfS486->$0;
  moonbit_decref(_M0L6_2aoldS4058);
  _M0L4selfS486->$0 = _M0L8new__bufS483;
  moonbit_decref(_M0L4selfS486);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS470
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS470 == 0) {
    moonbit_string_t* _M0L6_2atmpS1983 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4320 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4320)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4320->$0 = _M0L6_2atmpS1983;
    _block_4320->$1 = 0;
    return _block_4320;
  } else {
    moonbit_string_t* _M0L6_2atmpS1984 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS470, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_4321 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4321)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4321->$0 = _M0L6_2atmpS1984;
    _block_4321->$1 = 0;
    return _block_4321;
  }
}

moonbit_string_t _M0MPC16string6String6repeat(
  moonbit_string_t _M0L4selfS464,
  int32_t _M0L1nS463
) {
  #line 988 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  if (_M0L1nS463 <= 0) {
    moonbit_decref(_M0L4selfS464);
    return (moonbit_string_t)moonbit_string_literal_0.data;
  } else if (_M0L1nS463 == 1) {
    return _M0L4selfS464;
  } else {
    int32_t _M0L3lenS465 = Moonbit_array_length(_M0L4selfS464);
    int32_t _M0L6_2atmpS1982 = _M0L3lenS465 * _M0L1nS463;
    struct _M0TPB13StringBuilder* _M0L3bufS466;
    moonbit_string_t _M0L3strS467;
    int32_t _M0L2__S468;
    #line 994 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    _M0L3bufS466 = _M0MPB13StringBuilder11new_2einner(_M0L6_2atmpS1982);
    _M0L3strS467 = _M0L4selfS464;
    _M0L2__S468 = 0;
    while (1) {
      if (_M0L2__S468 < _M0L1nS463) {
        int32_t _M0L6_2atmpS1981;
        moonbit_incref(_M0L3strS467);
        moonbit_incref(_M0L3bufS466);
        #line 997 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS466, _M0L3strS467);
        _M0L6_2atmpS1981 = _M0L2__S468 + 1;
        _M0L2__S468 = _M0L6_2atmpS1981;
        continue;
      } else {
        moonbit_decref(_M0L3strS467);
      }
      break;
    }
    #line 999 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0MPB13StringBuilder10to__string(_M0L3bufS466);
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS461,
  struct _M0TPC16string10StringView _M0L3strS462
) {
  int32_t _M0L3lenS1969;
  int32_t _M0L6_2atmpS1971;
  int32_t _M0L6_2atmpS1970;
  int32_t _M0L6_2atmpS1968;
  moonbit_bytes_t _M0L8_2afieldS4060;
  moonbit_bytes_t _M0L4dataS1972;
  int32_t _M0L3lenS1973;
  moonbit_string_t _M0L6_2atmpS1974;
  int32_t _M0L6_2atmpS1975;
  int32_t _M0L6_2atmpS1976;
  int32_t _M0L3lenS1978;
  int32_t _M0L6_2atmpS1980;
  int32_t _M0L6_2atmpS1979;
  int32_t _M0L6_2atmpS1977;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1969 = _M0L4selfS461->$1;
  moonbit_incref(_M0L3strS462.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1971 = _M0MPC16string10StringView6length(_M0L3strS462);
  _M0L6_2atmpS1970 = _M0L6_2atmpS1971 * 2;
  _M0L6_2atmpS1968 = _M0L3lenS1969 + _M0L6_2atmpS1970;
  moonbit_incref(_M0L4selfS461);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS461, _M0L6_2atmpS1968);
  _M0L8_2afieldS4060 = _M0L4selfS461->$0;
  _M0L4dataS1972 = _M0L8_2afieldS4060;
  _M0L3lenS1973 = _M0L4selfS461->$1;
  moonbit_incref(_M0L4dataS1972);
  moonbit_incref(_M0L3strS462.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1974 = _M0MPC16string10StringView4data(_M0L3strS462);
  moonbit_incref(_M0L3strS462.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1975 = _M0MPC16string10StringView13start__offset(_M0L3strS462);
  moonbit_incref(_M0L3strS462.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1976 = _M0MPC16string10StringView6length(_M0L3strS462);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1972, _M0L3lenS1973, _M0L6_2atmpS1974, _M0L6_2atmpS1975, _M0L6_2atmpS1976);
  _M0L3lenS1978 = _M0L4selfS461->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1980 = _M0MPC16string10StringView6length(_M0L3strS462);
  _M0L6_2atmpS1979 = _M0L6_2atmpS1980 * 2;
  _M0L6_2atmpS1977 = _M0L3lenS1978 + _M0L6_2atmpS1979;
  _M0L4selfS461->$1 = _M0L6_2atmpS1977;
  moonbit_decref(_M0L4selfS461);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS453,
  int32_t _M0L3lenS456,
  int32_t _M0L13start__offsetS460,
  int64_t _M0L11end__offsetS451
) {
  int32_t _M0L11end__offsetS450;
  int32_t _M0L5indexS454;
  int32_t _M0L5countS455;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS451 == 4294967296ll) {
    _M0L11end__offsetS450 = Moonbit_array_length(_M0L4selfS453);
  } else {
    int64_t _M0L7_2aSomeS452 = _M0L11end__offsetS451;
    _M0L11end__offsetS450 = (int32_t)_M0L7_2aSomeS452;
  }
  _M0L5indexS454 = _M0L13start__offsetS460;
  _M0L5countS455 = 0;
  while (1) {
    int32_t _if__result_4324;
    if (_M0L5indexS454 < _M0L11end__offsetS450) {
      _if__result_4324 = _M0L5countS455 < _M0L3lenS456;
    } else {
      _if__result_4324 = 0;
    }
    if (_if__result_4324) {
      int32_t _M0L2c1S457 = _M0L4selfS453[_M0L5indexS454];
      int32_t _if__result_4325;
      int32_t _M0L6_2atmpS1966;
      int32_t _M0L6_2atmpS1967;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S457)) {
        int32_t _M0L6_2atmpS1962 = _M0L5indexS454 + 1;
        _if__result_4325 = _M0L6_2atmpS1962 < _M0L11end__offsetS450;
      } else {
        _if__result_4325 = 0;
      }
      if (_if__result_4325) {
        int32_t _M0L6_2atmpS1965 = _M0L5indexS454 + 1;
        int32_t _M0L2c2S458 = _M0L4selfS453[_M0L6_2atmpS1965];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S458)) {
          int32_t _M0L6_2atmpS1963 = _M0L5indexS454 + 2;
          int32_t _M0L6_2atmpS1964 = _M0L5countS455 + 1;
          _M0L5indexS454 = _M0L6_2atmpS1963;
          _M0L5countS455 = _M0L6_2atmpS1964;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_51.data, (moonbit_string_t)moonbit_string_literal_52.data);
        }
      }
      _M0L6_2atmpS1966 = _M0L5indexS454 + 1;
      _M0L6_2atmpS1967 = _M0L5countS455 + 1;
      _M0L5indexS454 = _M0L6_2atmpS1966;
      _M0L5countS455 = _M0L6_2atmpS1967;
      continue;
    } else {
      moonbit_decref(_M0L4selfS453);
      return _M0L5countS455 >= _M0L3lenS456;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS447
) {
  int32_t _M0L3endS1956;
  int32_t _M0L8_2afieldS4061;
  int32_t _M0L5startS1957;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1956 = _M0L4selfS447.$2;
  _M0L8_2afieldS4061 = _M0L4selfS447.$1;
  moonbit_decref(_M0L4selfS447.$0);
  _M0L5startS1957 = _M0L8_2afieldS4061;
  return _M0L3endS1956 - _M0L5startS1957;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS448
) {
  int32_t _M0L3endS1958;
  int32_t _M0L8_2afieldS4062;
  int32_t _M0L5startS1959;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1958 = _M0L4selfS448.$2;
  _M0L8_2afieldS4062 = _M0L4selfS448.$1;
  moonbit_decref(_M0L4selfS448.$0);
  _M0L5startS1959 = _M0L8_2afieldS4062;
  return _M0L3endS1958 - _M0L5startS1959;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS449
) {
  int32_t _M0L3endS1960;
  int32_t _M0L8_2afieldS4063;
  int32_t _M0L5startS1961;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1960 = _M0L4selfS449.$2;
  _M0L8_2afieldS4063 = _M0L4selfS449.$1;
  moonbit_decref(_M0L4selfS449.$0);
  _M0L5startS1961 = _M0L8_2afieldS4063;
  return _M0L3endS1960 - _M0L5startS1961;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS445,
  int64_t _M0L19start__offset_2eoptS443,
  int64_t _M0L11end__offsetS446
) {
  int32_t _M0L13start__offsetS442;
  if (_M0L19start__offset_2eoptS443 == 4294967296ll) {
    _M0L13start__offsetS442 = 0;
  } else {
    int64_t _M0L7_2aSomeS444 = _M0L19start__offset_2eoptS443;
    _M0L13start__offsetS442 = (int32_t)_M0L7_2aSomeS444;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS445, _M0L13start__offsetS442, _M0L11end__offsetS446);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS440,
  int32_t _M0L13start__offsetS441,
  int64_t _M0L11end__offsetS438
) {
  int32_t _M0L11end__offsetS437;
  int32_t _if__result_4326;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS438 == 4294967296ll) {
    _M0L11end__offsetS437 = Moonbit_array_length(_M0L4selfS440);
  } else {
    int64_t _M0L7_2aSomeS439 = _M0L11end__offsetS438;
    _M0L11end__offsetS437 = (int32_t)_M0L7_2aSomeS439;
  }
  if (_M0L13start__offsetS441 >= 0) {
    if (_M0L13start__offsetS441 <= _M0L11end__offsetS437) {
      int32_t _M0L6_2atmpS1955 = Moonbit_array_length(_M0L4selfS440);
      _if__result_4326 = _M0L11end__offsetS437 <= _M0L6_2atmpS1955;
    } else {
      _if__result_4326 = 0;
    }
  } else {
    _if__result_4326 = 0;
  }
  if (_if__result_4326) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS441,
                                                 _M0L11end__offsetS437,
                                                 _M0L4selfS440};
  } else {
    moonbit_decref(_M0L4selfS440);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_42.data, (moonbit_string_t)moonbit_string_literal_53.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS436
) {
  moonbit_string_t _M0L8_2afieldS4065;
  moonbit_string_t _M0L3strS1952;
  int32_t _M0L5startS1953;
  int32_t _M0L8_2afieldS4064;
  int32_t _M0L3endS1954;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4065 = _M0L4selfS436.$0;
  _M0L3strS1952 = _M0L8_2afieldS4065;
  _M0L5startS1953 = _M0L4selfS436.$1;
  _M0L8_2afieldS4064 = _M0L4selfS436.$2;
  _M0L3endS1954 = _M0L8_2afieldS4064;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1952, _M0L5startS1953, _M0L3endS1954);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS434,
  struct _M0TPB6Logger _M0L6loggerS435
) {
  moonbit_string_t _M0L8_2afieldS4067;
  moonbit_string_t _M0L3strS1949;
  int32_t _M0L5startS1950;
  int32_t _M0L8_2afieldS4066;
  int32_t _M0L3endS1951;
  moonbit_string_t _M0L6substrS433;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4067 = _M0L4selfS434.$0;
  _M0L3strS1949 = _M0L8_2afieldS4067;
  _M0L5startS1950 = _M0L4selfS434.$1;
  _M0L8_2afieldS4066 = _M0L4selfS434.$2;
  _M0L3endS1951 = _M0L8_2afieldS4066;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS433
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1949, _M0L5startS1950, _M0L3endS1951);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS433, _M0L6loggerS435);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS425,
  struct _M0TPB6Logger _M0L6loggerS423
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS424;
  int32_t _M0L3lenS426;
  int32_t _M0L1iS427;
  int32_t _M0L3segS428;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS423.$1) {
    moonbit_incref(_M0L6loggerS423.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS423.$0->$method_3(_M0L6loggerS423.$1, 34);
  moonbit_incref(_M0L4selfS425);
  if (_M0L6loggerS423.$1) {
    moonbit_incref(_M0L6loggerS423.$1);
  }
  _M0L6_2aenvS424
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS424)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS424->$0 = _M0L4selfS425;
  _M0L6_2aenvS424->$1_0 = _M0L6loggerS423.$0;
  _M0L6_2aenvS424->$1_1 = _M0L6loggerS423.$1;
  _M0L3lenS426 = Moonbit_array_length(_M0L4selfS425);
  _M0L1iS427 = 0;
  _M0L3segS428 = 0;
  _2afor_429:;
  while (1) {
    int32_t _M0L4codeS430;
    int32_t _M0L1cS432;
    int32_t _M0L6_2atmpS1933;
    int32_t _M0L6_2atmpS1934;
    int32_t _M0L6_2atmpS1935;
    int32_t _tmp_4330;
    int32_t _tmp_4331;
    if (_M0L1iS427 >= _M0L3lenS426) {
      moonbit_decref(_M0L4selfS425);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS424, _M0L3segS428, _M0L1iS427);
      break;
    }
    _M0L4codeS430 = _M0L4selfS425[_M0L1iS427];
    switch (_M0L4codeS430) {
      case 34: {
        _M0L1cS432 = _M0L4codeS430;
        goto join_431;
        break;
      }
      
      case 92: {
        _M0L1cS432 = _M0L4codeS430;
        goto join_431;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1936;
        int32_t _M0L6_2atmpS1937;
        moonbit_incref(_M0L6_2aenvS424);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS424, _M0L3segS428, _M0L1iS427);
        if (_M0L6loggerS423.$1) {
          moonbit_incref(_M0L6loggerS423.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS423.$0->$method_0(_M0L6loggerS423.$1, (moonbit_string_t)moonbit_string_literal_36.data);
        _M0L6_2atmpS1936 = _M0L1iS427 + 1;
        _M0L6_2atmpS1937 = _M0L1iS427 + 1;
        _M0L1iS427 = _M0L6_2atmpS1936;
        _M0L3segS428 = _M0L6_2atmpS1937;
        goto _2afor_429;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1938;
        int32_t _M0L6_2atmpS1939;
        moonbit_incref(_M0L6_2aenvS424);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS424, _M0L3segS428, _M0L1iS427);
        if (_M0L6loggerS423.$1) {
          moonbit_incref(_M0L6loggerS423.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS423.$0->$method_0(_M0L6loggerS423.$1, (moonbit_string_t)moonbit_string_literal_37.data);
        _M0L6_2atmpS1938 = _M0L1iS427 + 1;
        _M0L6_2atmpS1939 = _M0L1iS427 + 1;
        _M0L1iS427 = _M0L6_2atmpS1938;
        _M0L3segS428 = _M0L6_2atmpS1939;
        goto _2afor_429;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1940;
        int32_t _M0L6_2atmpS1941;
        moonbit_incref(_M0L6_2aenvS424);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS424, _M0L3segS428, _M0L1iS427);
        if (_M0L6loggerS423.$1) {
          moonbit_incref(_M0L6loggerS423.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS423.$0->$method_0(_M0L6loggerS423.$1, (moonbit_string_t)moonbit_string_literal_38.data);
        _M0L6_2atmpS1940 = _M0L1iS427 + 1;
        _M0L6_2atmpS1941 = _M0L1iS427 + 1;
        _M0L1iS427 = _M0L6_2atmpS1940;
        _M0L3segS428 = _M0L6_2atmpS1941;
        goto _2afor_429;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1942;
        int32_t _M0L6_2atmpS1943;
        moonbit_incref(_M0L6_2aenvS424);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS424, _M0L3segS428, _M0L1iS427);
        if (_M0L6loggerS423.$1) {
          moonbit_incref(_M0L6loggerS423.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS423.$0->$method_0(_M0L6loggerS423.$1, (moonbit_string_t)moonbit_string_literal_39.data);
        _M0L6_2atmpS1942 = _M0L1iS427 + 1;
        _M0L6_2atmpS1943 = _M0L1iS427 + 1;
        _M0L1iS427 = _M0L6_2atmpS1942;
        _M0L3segS428 = _M0L6_2atmpS1943;
        goto _2afor_429;
        break;
      }
      default: {
        if (_M0L4codeS430 < 32) {
          int32_t _M0L6_2atmpS1945;
          moonbit_string_t _M0L6_2atmpS1944;
          int32_t _M0L6_2atmpS1946;
          int32_t _M0L6_2atmpS1947;
          moonbit_incref(_M0L6_2aenvS424);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS424, _M0L3segS428, _M0L1iS427);
          if (_M0L6loggerS423.$1) {
            moonbit_incref(_M0L6loggerS423.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS423.$0->$method_0(_M0L6loggerS423.$1, (moonbit_string_t)moonbit_string_literal_54.data);
          _M0L6_2atmpS1945 = _M0L4codeS430 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1944 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1945);
          if (_M0L6loggerS423.$1) {
            moonbit_incref(_M0L6loggerS423.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS423.$0->$method_0(_M0L6loggerS423.$1, _M0L6_2atmpS1944);
          if (_M0L6loggerS423.$1) {
            moonbit_incref(_M0L6loggerS423.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS423.$0->$method_3(_M0L6loggerS423.$1, 125);
          _M0L6_2atmpS1946 = _M0L1iS427 + 1;
          _M0L6_2atmpS1947 = _M0L1iS427 + 1;
          _M0L1iS427 = _M0L6_2atmpS1946;
          _M0L3segS428 = _M0L6_2atmpS1947;
          goto _2afor_429;
        } else {
          int32_t _M0L6_2atmpS1948 = _M0L1iS427 + 1;
          int32_t _tmp_4329 = _M0L3segS428;
          _M0L1iS427 = _M0L6_2atmpS1948;
          _M0L3segS428 = _tmp_4329;
          goto _2afor_429;
        }
        break;
      }
    }
    goto joinlet_4328;
    join_431:;
    moonbit_incref(_M0L6_2aenvS424);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS424, _M0L3segS428, _M0L1iS427);
    if (_M0L6loggerS423.$1) {
      moonbit_incref(_M0L6loggerS423.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS423.$0->$method_3(_M0L6loggerS423.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1933 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS432);
    if (_M0L6loggerS423.$1) {
      moonbit_incref(_M0L6loggerS423.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS423.$0->$method_3(_M0L6loggerS423.$1, _M0L6_2atmpS1933);
    _M0L6_2atmpS1934 = _M0L1iS427 + 1;
    _M0L6_2atmpS1935 = _M0L1iS427 + 1;
    _M0L1iS427 = _M0L6_2atmpS1934;
    _M0L3segS428 = _M0L6_2atmpS1935;
    continue;
    joinlet_4328:;
    _tmp_4330 = _M0L1iS427;
    _tmp_4331 = _M0L3segS428;
    _M0L1iS427 = _tmp_4330;
    _M0L3segS428 = _tmp_4331;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS423.$0->$method_3(_M0L6loggerS423.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS419,
  int32_t _M0L3segS422,
  int32_t _M0L1iS421
) {
  struct _M0TPB6Logger _M0L8_2afieldS4069;
  struct _M0TPB6Logger _M0L6loggerS418;
  moonbit_string_t _M0L8_2afieldS4068;
  int32_t _M0L6_2acntS4199;
  moonbit_string_t _M0L4selfS420;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS4069
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS419->$1_0, _M0L6_2aenvS419->$1_1
  };
  _M0L6loggerS418 = _M0L8_2afieldS4069;
  _M0L8_2afieldS4068 = _M0L6_2aenvS419->$0;
  _M0L6_2acntS4199 = Moonbit_object_header(_M0L6_2aenvS419)->rc;
  if (_M0L6_2acntS4199 > 1) {
    int32_t _M0L11_2anew__cntS4200 = _M0L6_2acntS4199 - 1;
    Moonbit_object_header(_M0L6_2aenvS419)->rc = _M0L11_2anew__cntS4200;
    if (_M0L6loggerS418.$1) {
      moonbit_incref(_M0L6loggerS418.$1);
    }
    moonbit_incref(_M0L8_2afieldS4068);
  } else if (_M0L6_2acntS4199 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS419);
  }
  _M0L4selfS420 = _M0L8_2afieldS4068;
  if (_M0L1iS421 > _M0L3segS422) {
    int32_t _M0L6_2atmpS1932 = _M0L1iS421 - _M0L3segS422;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS418.$0->$method_1(_M0L6loggerS418.$1, _M0L4selfS420, _M0L3segS422, _M0L6_2atmpS1932);
  } else {
    moonbit_decref(_M0L4selfS420);
    if (_M0L6loggerS418.$1) {
      moonbit_decref(_M0L6loggerS418.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS417) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS416;
  int32_t _M0L6_2atmpS1929;
  int32_t _M0L6_2atmpS1928;
  int32_t _M0L6_2atmpS1931;
  int32_t _M0L6_2atmpS1930;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1927;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS416 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1929 = _M0IPC14byte4BytePB3Div3div(_M0L1bS417, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1928
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1929);
  moonbit_incref(_M0L7_2aselfS416);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS416, _M0L6_2atmpS1928);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1931 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS417, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1930
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1931);
  moonbit_incref(_M0L7_2aselfS416);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS416, _M0L6_2atmpS1930);
  _M0L6_2atmpS1927 = _M0L7_2aselfS416;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1927);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS415) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS415 < 10) {
    int32_t _M0L6_2atmpS1924;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1924 = _M0IPC14byte4BytePB3Add3add(_M0L1iS415, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1924);
  } else {
    int32_t _M0L6_2atmpS1926;
    int32_t _M0L6_2atmpS1925;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1926 = _M0IPC14byte4BytePB3Add3add(_M0L1iS415, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1925 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1926, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1925);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS413,
  int32_t _M0L4thatS414
) {
  int32_t _M0L6_2atmpS1922;
  int32_t _M0L6_2atmpS1923;
  int32_t _M0L6_2atmpS1921;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1922 = (int32_t)_M0L4selfS413;
  _M0L6_2atmpS1923 = (int32_t)_M0L4thatS414;
  _M0L6_2atmpS1921 = _M0L6_2atmpS1922 - _M0L6_2atmpS1923;
  return _M0L6_2atmpS1921 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS411,
  int32_t _M0L4thatS412
) {
  int32_t _M0L6_2atmpS1919;
  int32_t _M0L6_2atmpS1920;
  int32_t _M0L6_2atmpS1918;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1919 = (int32_t)_M0L4selfS411;
  _M0L6_2atmpS1920 = (int32_t)_M0L4thatS412;
  _M0L6_2atmpS1918 = _M0L6_2atmpS1919 % _M0L6_2atmpS1920;
  return _M0L6_2atmpS1918 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS409,
  int32_t _M0L4thatS410
) {
  int32_t _M0L6_2atmpS1916;
  int32_t _M0L6_2atmpS1917;
  int32_t _M0L6_2atmpS1915;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1916 = (int32_t)_M0L4selfS409;
  _M0L6_2atmpS1917 = (int32_t)_M0L4thatS410;
  _M0L6_2atmpS1915 = _M0L6_2atmpS1916 / _M0L6_2atmpS1917;
  return _M0L6_2atmpS1915 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS407,
  int32_t _M0L4thatS408
) {
  int32_t _M0L6_2atmpS1913;
  int32_t _M0L6_2atmpS1914;
  int32_t _M0L6_2atmpS1912;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1913 = (int32_t)_M0L4selfS407;
  _M0L6_2atmpS1914 = (int32_t)_M0L4thatS408;
  _M0L6_2atmpS1912 = _M0L6_2atmpS1913 + _M0L6_2atmpS1914;
  return _M0L6_2atmpS1912 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS404,
  int32_t _M0L5startS402,
  int32_t _M0L3endS403
) {
  int32_t _if__result_4332;
  int32_t _M0L3lenS405;
  int32_t _M0L6_2atmpS1910;
  int32_t _M0L6_2atmpS1911;
  moonbit_bytes_t _M0L5bytesS406;
  moonbit_bytes_t _M0L6_2atmpS1909;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS402 == 0) {
    int32_t _M0L6_2atmpS1908 = Moonbit_array_length(_M0L3strS404);
    _if__result_4332 = _M0L3endS403 == _M0L6_2atmpS1908;
  } else {
    _if__result_4332 = 0;
  }
  if (_if__result_4332) {
    return _M0L3strS404;
  }
  _M0L3lenS405 = _M0L3endS403 - _M0L5startS402;
  _M0L6_2atmpS1910 = _M0L3lenS405 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1911 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS406
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1910, _M0L6_2atmpS1911);
  moonbit_incref(_M0L5bytesS406);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS406, 0, _M0L3strS404, _M0L5startS402, _M0L3lenS405);
  _M0L6_2atmpS1909 = _M0L5bytesS406;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1909, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS399) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS399;
}

struct _M0TWEOUsRPB4JsonE* _M0MPB4Iter3newGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L1fS400
) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS400;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS401) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS401;
}

moonbit_string_t _M0MPC16uint646UInt6418to__string_2einner(
  uint64_t _M0L4selfS391,
  int32_t _M0L5radixS390
) {
  int32_t _if__result_4333;
  uint16_t* _M0L6bufferS392;
  #line 629 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS390 < 2) {
    _if__result_4333 = 1;
  } else {
    _if__result_4333 = _M0L5radixS390 > 36;
  }
  if (_if__result_4333) {
    #line 633 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_55.data, (moonbit_string_t)moonbit_string_literal_56.data);
  }
  if (_M0L4selfS391 == 0ull) {
    return (moonbit_string_t)moonbit_string_literal_44.data;
  }
  switch (_M0L5radixS390) {
    case 10: {
      int32_t _M0L3lenS393;
      uint16_t* _M0L6bufferS394;
      #line 644 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS393 = _M0FPB12dec__count64(_M0L4selfS391);
      _M0L6bufferS394 = (uint16_t*)moonbit_make_string(_M0L3lenS393, 0);
      moonbit_incref(_M0L6bufferS394);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__dec(_M0L6bufferS394, _M0L4selfS391, 0, _M0L3lenS393);
      _M0L6bufferS392 = _M0L6bufferS394;
      break;
    }
    
    case 16: {
      int32_t _M0L3lenS395;
      uint16_t* _M0L6bufferS396;
      #line 650 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS395 = _M0FPB12hex__count64(_M0L4selfS391);
      _M0L6bufferS396 = (uint16_t*)moonbit_make_string(_M0L3lenS395, 0);
      moonbit_incref(_M0L6bufferS396);
      #line 652 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB22int64__to__string__hex(_M0L6bufferS396, _M0L4selfS391, 0, _M0L3lenS395);
      _M0L6bufferS392 = _M0L6bufferS396;
      break;
    }
    default: {
      int32_t _M0L3lenS397;
      uint16_t* _M0L6bufferS398;
      #line 656 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L3lenS397 = _M0FPB14radix__count64(_M0L4selfS391, _M0L5radixS390);
      _M0L6bufferS398 = (uint16_t*)moonbit_make_string(_M0L3lenS397, 0);
      moonbit_incref(_M0L6bufferS398);
      #line 658 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB26int64__to__string__generic(_M0L6bufferS398, _M0L4selfS391, 0, _M0L3lenS397, _M0L5radixS390);
      _M0L6bufferS392 = _M0L6bufferS398;
      break;
    }
  }
  return _M0L6bufferS392;
}

int32_t _M0FPB22int64__to__string__dec(
  uint16_t* _M0L6bufferS380,
  uint64_t _M0L3numS368,
  int32_t _M0L12digit__startS371,
  int32_t _M0L10total__lenS370
) {
  uint64_t _M0Lm3numS367;
  int32_t _M0Lm6offsetS369;
  uint64_t _M0L6_2atmpS1907;
  int32_t _M0Lm9remainingS382;
  int32_t _M0L6_2atmpS1888;
  #line 512 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS367 = _M0L3numS368;
  _M0Lm6offsetS369 = _M0L10total__lenS370 - _M0L12digit__startS371;
  while (1) {
    uint64_t _M0L6_2atmpS1851 = _M0Lm3numS367;
    if (_M0L6_2atmpS1851 >= 10000ull) {
      uint64_t _M0L6_2atmpS1874 = _M0Lm3numS367;
      uint64_t _M0L1tS372 = _M0L6_2atmpS1874 / 10000ull;
      uint64_t _M0L6_2atmpS1873 = _M0Lm3numS367;
      uint64_t _M0L6_2atmpS1872 = _M0L6_2atmpS1873 % 10000ull;
      int32_t _M0L1rS373 = (int32_t)_M0L6_2atmpS1872;
      int32_t _M0L2d1S374;
      int32_t _M0L2d2S375;
      int32_t _M0L6_2atmpS1852;
      int32_t _M0L6_2atmpS1871;
      int32_t _M0L6_2atmpS1870;
      int32_t _M0L6d1__hiS376;
      int32_t _M0L6_2atmpS1869;
      int32_t _M0L6_2atmpS1868;
      int32_t _M0L6d1__loS377;
      int32_t _M0L6_2atmpS1867;
      int32_t _M0L6_2atmpS1866;
      int32_t _M0L6d2__hiS378;
      int32_t _M0L6_2atmpS1865;
      int32_t _M0L6_2atmpS1864;
      int32_t _M0L6d2__loS379;
      int32_t _M0L6_2atmpS1854;
      int32_t _M0L6_2atmpS1853;
      int32_t _M0L6_2atmpS1857;
      int32_t _M0L6_2atmpS1856;
      int32_t _M0L6_2atmpS1855;
      int32_t _M0L6_2atmpS1860;
      int32_t _M0L6_2atmpS1859;
      int32_t _M0L6_2atmpS1858;
      int32_t _M0L6_2atmpS1863;
      int32_t _M0L6_2atmpS1862;
      int32_t _M0L6_2atmpS1861;
      _M0Lm3numS367 = _M0L1tS372;
      _M0L2d1S374 = _M0L1rS373 / 100;
      _M0L2d2S375 = _M0L1rS373 % 100;
      _M0L6_2atmpS1852 = _M0Lm6offsetS369;
      _M0Lm6offsetS369 = _M0L6_2atmpS1852 - 4;
      _M0L6_2atmpS1871 = _M0L2d1S374 / 10;
      _M0L6_2atmpS1870 = 48 + _M0L6_2atmpS1871;
      _M0L6d1__hiS376 = (uint16_t)_M0L6_2atmpS1870;
      _M0L6_2atmpS1869 = _M0L2d1S374 % 10;
      _M0L6_2atmpS1868 = 48 + _M0L6_2atmpS1869;
      _M0L6d1__loS377 = (uint16_t)_M0L6_2atmpS1868;
      _M0L6_2atmpS1867 = _M0L2d2S375 / 10;
      _M0L6_2atmpS1866 = 48 + _M0L6_2atmpS1867;
      _M0L6d2__hiS378 = (uint16_t)_M0L6_2atmpS1866;
      _M0L6_2atmpS1865 = _M0L2d2S375 % 10;
      _M0L6_2atmpS1864 = 48 + _M0L6_2atmpS1865;
      _M0L6d2__loS379 = (uint16_t)_M0L6_2atmpS1864;
      _M0L6_2atmpS1854 = _M0Lm6offsetS369;
      _M0L6_2atmpS1853 = _M0L12digit__startS371 + _M0L6_2atmpS1854;
      _M0L6bufferS380[_M0L6_2atmpS1853] = _M0L6d1__hiS376;
      _M0L6_2atmpS1857 = _M0Lm6offsetS369;
      _M0L6_2atmpS1856 = _M0L12digit__startS371 + _M0L6_2atmpS1857;
      _M0L6_2atmpS1855 = _M0L6_2atmpS1856 + 1;
      _M0L6bufferS380[_M0L6_2atmpS1855] = _M0L6d1__loS377;
      _M0L6_2atmpS1860 = _M0Lm6offsetS369;
      _M0L6_2atmpS1859 = _M0L12digit__startS371 + _M0L6_2atmpS1860;
      _M0L6_2atmpS1858 = _M0L6_2atmpS1859 + 2;
      _M0L6bufferS380[_M0L6_2atmpS1858] = _M0L6d2__hiS378;
      _M0L6_2atmpS1863 = _M0Lm6offsetS369;
      _M0L6_2atmpS1862 = _M0L12digit__startS371 + _M0L6_2atmpS1863;
      _M0L6_2atmpS1861 = _M0L6_2atmpS1862 + 3;
      _M0L6bufferS380[_M0L6_2atmpS1861] = _M0L6d2__loS379;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1907 = _M0Lm3numS367;
  _M0Lm9remainingS382 = (int32_t)_M0L6_2atmpS1907;
  while (1) {
    int32_t _M0L6_2atmpS1875 = _M0Lm9remainingS382;
    if (_M0L6_2atmpS1875 >= 100) {
      int32_t _M0L6_2atmpS1887 = _M0Lm9remainingS382;
      int32_t _M0L1tS383 = _M0L6_2atmpS1887 / 100;
      int32_t _M0L6_2atmpS1886 = _M0Lm9remainingS382;
      int32_t _M0L1dS384 = _M0L6_2atmpS1886 % 100;
      int32_t _M0L6_2atmpS1876;
      int32_t _M0L6_2atmpS1885;
      int32_t _M0L6_2atmpS1884;
      int32_t _M0L5d__hiS385;
      int32_t _M0L6_2atmpS1883;
      int32_t _M0L6_2atmpS1882;
      int32_t _M0L5d__loS386;
      int32_t _M0L6_2atmpS1878;
      int32_t _M0L6_2atmpS1877;
      int32_t _M0L6_2atmpS1881;
      int32_t _M0L6_2atmpS1880;
      int32_t _M0L6_2atmpS1879;
      _M0Lm9remainingS382 = _M0L1tS383;
      _M0L6_2atmpS1876 = _M0Lm6offsetS369;
      _M0Lm6offsetS369 = _M0L6_2atmpS1876 - 2;
      _M0L6_2atmpS1885 = _M0L1dS384 / 10;
      _M0L6_2atmpS1884 = 48 + _M0L6_2atmpS1885;
      _M0L5d__hiS385 = (uint16_t)_M0L6_2atmpS1884;
      _M0L6_2atmpS1883 = _M0L1dS384 % 10;
      _M0L6_2atmpS1882 = 48 + _M0L6_2atmpS1883;
      _M0L5d__loS386 = (uint16_t)_M0L6_2atmpS1882;
      _M0L6_2atmpS1878 = _M0Lm6offsetS369;
      _M0L6_2atmpS1877 = _M0L12digit__startS371 + _M0L6_2atmpS1878;
      _M0L6bufferS380[_M0L6_2atmpS1877] = _M0L5d__hiS385;
      _M0L6_2atmpS1881 = _M0Lm6offsetS369;
      _M0L6_2atmpS1880 = _M0L12digit__startS371 + _M0L6_2atmpS1881;
      _M0L6_2atmpS1879 = _M0L6_2atmpS1880 + 1;
      _M0L6bufferS380[_M0L6_2atmpS1879] = _M0L5d__loS386;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1888 = _M0Lm9remainingS382;
  if (_M0L6_2atmpS1888 >= 10) {
    int32_t _M0L6_2atmpS1889 = _M0Lm6offsetS369;
    int32_t _M0L6_2atmpS1900;
    int32_t _M0L6_2atmpS1899;
    int32_t _M0L6_2atmpS1898;
    int32_t _M0L5d__hiS388;
    int32_t _M0L6_2atmpS1897;
    int32_t _M0L6_2atmpS1896;
    int32_t _M0L6_2atmpS1895;
    int32_t _M0L5d__loS389;
    int32_t _M0L6_2atmpS1891;
    int32_t _M0L6_2atmpS1890;
    int32_t _M0L6_2atmpS1894;
    int32_t _M0L6_2atmpS1893;
    int32_t _M0L6_2atmpS1892;
    _M0Lm6offsetS369 = _M0L6_2atmpS1889 - 2;
    _M0L6_2atmpS1900 = _M0Lm9remainingS382;
    _M0L6_2atmpS1899 = _M0L6_2atmpS1900 / 10;
    _M0L6_2atmpS1898 = 48 + _M0L6_2atmpS1899;
    _M0L5d__hiS388 = (uint16_t)_M0L6_2atmpS1898;
    _M0L6_2atmpS1897 = _M0Lm9remainingS382;
    _M0L6_2atmpS1896 = _M0L6_2atmpS1897 % 10;
    _M0L6_2atmpS1895 = 48 + _M0L6_2atmpS1896;
    _M0L5d__loS389 = (uint16_t)_M0L6_2atmpS1895;
    _M0L6_2atmpS1891 = _M0Lm6offsetS369;
    _M0L6_2atmpS1890 = _M0L12digit__startS371 + _M0L6_2atmpS1891;
    _M0L6bufferS380[_M0L6_2atmpS1890] = _M0L5d__hiS388;
    _M0L6_2atmpS1894 = _M0Lm6offsetS369;
    _M0L6_2atmpS1893 = _M0L12digit__startS371 + _M0L6_2atmpS1894;
    _M0L6_2atmpS1892 = _M0L6_2atmpS1893 + 1;
    _M0L6bufferS380[_M0L6_2atmpS1892] = _M0L5d__loS389;
    moonbit_decref(_M0L6bufferS380);
  } else {
    int32_t _M0L6_2atmpS1901 = _M0Lm6offsetS369;
    int32_t _M0L6_2atmpS1906;
    int32_t _M0L6_2atmpS1902;
    int32_t _M0L6_2atmpS1905;
    int32_t _M0L6_2atmpS1904;
    int32_t _M0L6_2atmpS1903;
    _M0Lm6offsetS369 = _M0L6_2atmpS1901 - 1;
    _M0L6_2atmpS1906 = _M0Lm6offsetS369;
    _M0L6_2atmpS1902 = _M0L12digit__startS371 + _M0L6_2atmpS1906;
    _M0L6_2atmpS1905 = _M0Lm9remainingS382;
    _M0L6_2atmpS1904 = 48 + _M0L6_2atmpS1905;
    _M0L6_2atmpS1903 = (uint16_t)_M0L6_2atmpS1904;
    _M0L6bufferS380[_M0L6_2atmpS1902] = _M0L6_2atmpS1903;
    moonbit_decref(_M0L6bufferS380);
  }
  return 0;
}

int32_t _M0FPB26int64__to__string__generic(
  uint16_t* _M0L6bufferS362,
  uint64_t _M0L3numS356,
  int32_t _M0L12digit__startS354,
  int32_t _M0L10total__lenS353,
  int32_t _M0L5radixS358
) {
  int32_t _M0Lm6offsetS352;
  uint64_t _M0Lm1nS355;
  uint64_t _M0L4baseS357;
  int32_t _M0L6_2atmpS1833;
  int32_t _M0L6_2atmpS1832;
  #line 477 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS352 = _M0L10total__lenS353 - _M0L12digit__startS354;
  _M0Lm1nS355 = _M0L3numS356;
  #line 487 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS357 = _M0MPC13int3Int10to__uint64(_M0L5radixS358);
  _M0L6_2atmpS1833 = _M0L5radixS358 - 1;
  _M0L6_2atmpS1832 = _M0L5radixS358 & _M0L6_2atmpS1833;
  if (_M0L6_2atmpS1832 == 0) {
    int32_t _M0L5shiftS359;
    uint64_t _M0L4maskS360;
    #line 490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS359 = moonbit_ctz32(_M0L5radixS358);
    _M0L4maskS360 = _M0L4baseS357 - 1ull;
    while (1) {
      uint64_t _M0L6_2atmpS1834 = _M0Lm1nS355;
      if (_M0L6_2atmpS1834 > 0ull) {
        int32_t _M0L6_2atmpS1835 = _M0Lm6offsetS352;
        uint64_t _M0L6_2atmpS1841;
        uint64_t _M0L6_2atmpS1840;
        int32_t _M0L5digitS361;
        int32_t _M0L6_2atmpS1838;
        int32_t _M0L6_2atmpS1836;
        int32_t _M0L6_2atmpS1837;
        uint64_t _M0L6_2atmpS1839;
        _M0Lm6offsetS352 = _M0L6_2atmpS1835 - 1;
        _M0L6_2atmpS1841 = _M0Lm1nS355;
        _M0L6_2atmpS1840 = _M0L6_2atmpS1841 & _M0L4maskS360;
        _M0L5digitS361 = (int32_t)_M0L6_2atmpS1840;
        _M0L6_2atmpS1838 = _M0Lm6offsetS352;
        _M0L6_2atmpS1836 = _M0L12digit__startS354 + _M0L6_2atmpS1838;
        _M0L6_2atmpS1837
        = ((moonbit_string_t)moonbit_string_literal_57.data)[
          _M0L5digitS361
        ];
        _M0L6bufferS362[_M0L6_2atmpS1836] = _M0L6_2atmpS1837;
        _M0L6_2atmpS1839 = _M0Lm1nS355;
        _M0Lm1nS355 = _M0L6_2atmpS1839 >> (_M0L5shiftS359 & 63);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS362);
      }
      break;
    }
  } else {
    while (1) {
      uint64_t _M0L6_2atmpS1842 = _M0Lm1nS355;
      if (_M0L6_2atmpS1842 > 0ull) {
        int32_t _M0L6_2atmpS1843 = _M0Lm6offsetS352;
        uint64_t _M0L6_2atmpS1850;
        uint64_t _M0L1qS364;
        uint64_t _M0L6_2atmpS1848;
        uint64_t _M0L6_2atmpS1849;
        uint64_t _M0L6_2atmpS1847;
        int32_t _M0L5digitS365;
        int32_t _M0L6_2atmpS1846;
        int32_t _M0L6_2atmpS1844;
        int32_t _M0L6_2atmpS1845;
        _M0Lm6offsetS352 = _M0L6_2atmpS1843 - 1;
        _M0L6_2atmpS1850 = _M0Lm1nS355;
        _M0L1qS364 = _M0L6_2atmpS1850 / _M0L4baseS357;
        _M0L6_2atmpS1848 = _M0Lm1nS355;
        _M0L6_2atmpS1849 = _M0L1qS364 * _M0L4baseS357;
        _M0L6_2atmpS1847 = _M0L6_2atmpS1848 - _M0L6_2atmpS1849;
        _M0L5digitS365 = (int32_t)_M0L6_2atmpS1847;
        _M0L6_2atmpS1846 = _M0Lm6offsetS352;
        _M0L6_2atmpS1844 = _M0L12digit__startS354 + _M0L6_2atmpS1846;
        _M0L6_2atmpS1845
        = ((moonbit_string_t)moonbit_string_literal_57.data)[
          _M0L5digitS365
        ];
        _M0L6bufferS362[_M0L6_2atmpS1844] = _M0L6_2atmpS1845;
        _M0Lm1nS355 = _M0L1qS364;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS362);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB22int64__to__string__hex(
  uint16_t* _M0L6bufferS349,
  uint64_t _M0L3numS345,
  int32_t _M0L12digit__startS343,
  int32_t _M0L10total__lenS342
) {
  int32_t _M0Lm6offsetS341;
  uint64_t _M0Lm1nS344;
  int32_t _M0L6_2atmpS1828;
  #line 447 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS341 = _M0L10total__lenS342 - _M0L12digit__startS343;
  _M0Lm1nS344 = _M0L3numS345;
  while (1) {
    int32_t _M0L6_2atmpS1816 = _M0Lm6offsetS341;
    if (_M0L6_2atmpS1816 >= 2) {
      int32_t _M0L6_2atmpS1817 = _M0Lm6offsetS341;
      uint64_t _M0L6_2atmpS1827;
      uint64_t _M0L6_2atmpS1826;
      int32_t _M0L9byte__valS346;
      int32_t _M0L2hiS347;
      int32_t _M0L2loS348;
      int32_t _M0L6_2atmpS1820;
      int32_t _M0L6_2atmpS1818;
      int32_t _M0L6_2atmpS1819;
      int32_t _M0L6_2atmpS1824;
      int32_t _M0L6_2atmpS1823;
      int32_t _M0L6_2atmpS1821;
      int32_t _M0L6_2atmpS1822;
      uint64_t _M0L6_2atmpS1825;
      _M0Lm6offsetS341 = _M0L6_2atmpS1817 - 2;
      _M0L6_2atmpS1827 = _M0Lm1nS344;
      _M0L6_2atmpS1826 = _M0L6_2atmpS1827 & 255ull;
      _M0L9byte__valS346 = (int32_t)_M0L6_2atmpS1826;
      _M0L2hiS347 = _M0L9byte__valS346 / 16;
      _M0L2loS348 = _M0L9byte__valS346 % 16;
      _M0L6_2atmpS1820 = _M0Lm6offsetS341;
      _M0L6_2atmpS1818 = _M0L12digit__startS343 + _M0L6_2atmpS1820;
      _M0L6_2atmpS1819
      = ((moonbit_string_t)moonbit_string_literal_57.data)[
        _M0L2hiS347
      ];
      _M0L6bufferS349[_M0L6_2atmpS1818] = _M0L6_2atmpS1819;
      _M0L6_2atmpS1824 = _M0Lm6offsetS341;
      _M0L6_2atmpS1823 = _M0L12digit__startS343 + _M0L6_2atmpS1824;
      _M0L6_2atmpS1821 = _M0L6_2atmpS1823 + 1;
      _M0L6_2atmpS1822
      = ((moonbit_string_t)moonbit_string_literal_57.data)[
        _M0L2loS348
      ];
      _M0L6bufferS349[_M0L6_2atmpS1821] = _M0L6_2atmpS1822;
      _M0L6_2atmpS1825 = _M0Lm1nS344;
      _M0Lm1nS344 = _M0L6_2atmpS1825 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1828 = _M0Lm6offsetS341;
  if (_M0L6_2atmpS1828 == 1) {
    uint64_t _M0L6_2atmpS1831 = _M0Lm1nS344;
    uint64_t _M0L6_2atmpS1830 = _M0L6_2atmpS1831 & 15ull;
    int32_t _M0L6nibbleS351 = (int32_t)_M0L6_2atmpS1830;
    int32_t _M0L6_2atmpS1829 =
      ((moonbit_string_t)moonbit_string_literal_57.data)[_M0L6nibbleS351];
    _M0L6bufferS349[_M0L12digit__startS343] = _M0L6_2atmpS1829;
    moonbit_decref(_M0L6bufferS349);
  } else {
    moonbit_decref(_M0L6bufferS349);
  }
  return 0;
}

int32_t _M0FPB14radix__count64(
  uint64_t _M0L5valueS335,
  int32_t _M0L5radixS338
) {
  uint64_t _M0Lm3numS336;
  uint64_t _M0L4baseS337;
  int32_t _M0Lm5countS339;
  #line 430 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS335 == 0ull) {
    return 1;
  }
  _M0Lm3numS336 = _M0L5valueS335;
  #line 436 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0L4baseS337 = _M0MPC13int3Int10to__uint64(_M0L5radixS338);
  _M0Lm5countS339 = 0;
  while (1) {
    uint64_t _M0L6_2atmpS1813 = _M0Lm3numS336;
    if (_M0L6_2atmpS1813 > 0ull) {
      int32_t _M0L6_2atmpS1814 = _M0Lm5countS339;
      uint64_t _M0L6_2atmpS1815;
      _M0Lm5countS339 = _M0L6_2atmpS1814 + 1;
      _M0L6_2atmpS1815 = _M0Lm3numS336;
      _M0Lm3numS336 = _M0L6_2atmpS1815 / _M0L4baseS337;
      continue;
    }
    break;
  }
  return _M0Lm5countS339;
}

int32_t _M0FPB12hex__count64(uint64_t _M0L5valueS333) {
  #line 418 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS333 == 0ull) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS334;
    int32_t _M0L6_2atmpS1812;
    int32_t _M0L6_2atmpS1811;
    #line 423 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS334 = moonbit_clz64(_M0L5valueS333);
    _M0L6_2atmpS1812 = 63 - _M0L14leading__zerosS334;
    _M0L6_2atmpS1811 = _M0L6_2atmpS1812 / 4;
    return _M0L6_2atmpS1811 + 1;
  }
}

int32_t _M0FPB12dec__count64(uint64_t _M0L5valueS332) {
  #line 354 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS332 >= 10000000000ull) {
    if (_M0L5valueS332 >= 100000000000000ull) {
      if (_M0L5valueS332 >= 10000000000000000ull) {
        if (_M0L5valueS332 >= 1000000000000000000ull) {
          if (_M0L5valueS332 >= 10000000000000000000ull) {
            return 20;
          } else {
            return 19;
          }
        } else if (_M0L5valueS332 >= 100000000000000000ull) {
          return 18;
        } else {
          return 17;
        }
      } else if (_M0L5valueS332 >= 1000000000000000ull) {
        return 16;
      } else {
        return 15;
      }
    } else if (_M0L5valueS332 >= 1000000000000ull) {
      if (_M0L5valueS332 >= 10000000000000ull) {
        return 14;
      } else {
        return 13;
      }
    } else if (_M0L5valueS332 >= 100000000000ull) {
      return 12;
    } else {
      return 11;
    }
  } else if (_M0L5valueS332 >= 100000ull) {
    if (_M0L5valueS332 >= 10000000ull) {
      if (_M0L5valueS332 >= 1000000000ull) {
        return 10;
      } else if (_M0L5valueS332 >= 100000000ull) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS332 >= 1000000ull) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS332 >= 1000ull) {
    if (_M0L5valueS332 >= 10000ull) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS332 >= 100ull) {
    return 3;
  } else if (_M0L5valueS332 >= 10ull) {
    return 2;
  } else {
    return 1;
  }
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS316,
  int32_t _M0L5radixS315
) {
  int32_t _if__result_4340;
  int32_t _M0L12is__negativeS317;
  uint32_t _M0L3numS318;
  uint16_t* _M0L6bufferS319;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS315 < 2) {
    _if__result_4340 = 1;
  } else {
    _if__result_4340 = _M0L5radixS315 > 36;
  }
  if (_if__result_4340) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_55.data, (moonbit_string_t)moonbit_string_literal_58.data);
  }
  if (_M0L4selfS316 == 0) {
    return (moonbit_string_t)moonbit_string_literal_44.data;
  }
  _M0L12is__negativeS317 = _M0L4selfS316 < 0;
  if (_M0L12is__negativeS317) {
    int32_t _M0L6_2atmpS1810 = -_M0L4selfS316;
    _M0L3numS318 = *(uint32_t*)&_M0L6_2atmpS1810;
  } else {
    _M0L3numS318 = *(uint32_t*)&_M0L4selfS316;
  }
  switch (_M0L5radixS315) {
    case 10: {
      int32_t _M0L10digit__lenS320;
      int32_t _M0L6_2atmpS1807;
      int32_t _M0L10total__lenS321;
      uint16_t* _M0L6bufferS322;
      int32_t _M0L12digit__startS323;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS320 = _M0FPB12dec__count32(_M0L3numS318);
      if (_M0L12is__negativeS317) {
        _M0L6_2atmpS1807 = 1;
      } else {
        _M0L6_2atmpS1807 = 0;
      }
      _M0L10total__lenS321 = _M0L10digit__lenS320 + _M0L6_2atmpS1807;
      _M0L6bufferS322
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS321, 0);
      if (_M0L12is__negativeS317) {
        _M0L12digit__startS323 = 1;
      } else {
        _M0L12digit__startS323 = 0;
      }
      moonbit_incref(_M0L6bufferS322);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS322, _M0L3numS318, _M0L12digit__startS323, _M0L10total__lenS321);
      _M0L6bufferS319 = _M0L6bufferS322;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS324;
      int32_t _M0L6_2atmpS1808;
      int32_t _M0L10total__lenS325;
      uint16_t* _M0L6bufferS326;
      int32_t _M0L12digit__startS327;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS324 = _M0FPB12hex__count32(_M0L3numS318);
      if (_M0L12is__negativeS317) {
        _M0L6_2atmpS1808 = 1;
      } else {
        _M0L6_2atmpS1808 = 0;
      }
      _M0L10total__lenS325 = _M0L10digit__lenS324 + _M0L6_2atmpS1808;
      _M0L6bufferS326
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS325, 0);
      if (_M0L12is__negativeS317) {
        _M0L12digit__startS327 = 1;
      } else {
        _M0L12digit__startS327 = 0;
      }
      moonbit_incref(_M0L6bufferS326);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS326, _M0L3numS318, _M0L12digit__startS327, _M0L10total__lenS325);
      _M0L6bufferS319 = _M0L6bufferS326;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS328;
      int32_t _M0L6_2atmpS1809;
      int32_t _M0L10total__lenS329;
      uint16_t* _M0L6bufferS330;
      int32_t _M0L12digit__startS331;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS328
      = _M0FPB14radix__count32(_M0L3numS318, _M0L5radixS315);
      if (_M0L12is__negativeS317) {
        _M0L6_2atmpS1809 = 1;
      } else {
        _M0L6_2atmpS1809 = 0;
      }
      _M0L10total__lenS329 = _M0L10digit__lenS328 + _M0L6_2atmpS1809;
      _M0L6bufferS330
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS329, 0);
      if (_M0L12is__negativeS317) {
        _M0L12digit__startS331 = 1;
      } else {
        _M0L12digit__startS331 = 0;
      }
      moonbit_incref(_M0L6bufferS330);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS330, _M0L3numS318, _M0L12digit__startS331, _M0L10total__lenS329, _M0L5radixS315);
      _M0L6bufferS319 = _M0L6bufferS330;
      break;
    }
  }
  if (_M0L12is__negativeS317) {
    _M0L6bufferS319[0] = 45;
  }
  return _M0L6bufferS319;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS309,
  int32_t _M0L5radixS312
) {
  uint32_t _M0Lm3numS310;
  uint32_t _M0L4baseS311;
  int32_t _M0Lm5countS313;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS309 == 0u) {
    return 1;
  }
  _M0Lm3numS310 = _M0L5valueS309;
  _M0L4baseS311 = *(uint32_t*)&_M0L5radixS312;
  _M0Lm5countS313 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1804 = _M0Lm3numS310;
    if (_M0L6_2atmpS1804 > 0u) {
      int32_t _M0L6_2atmpS1805 = _M0Lm5countS313;
      uint32_t _M0L6_2atmpS1806;
      _M0Lm5countS313 = _M0L6_2atmpS1805 + 1;
      _M0L6_2atmpS1806 = _M0Lm3numS310;
      _M0Lm3numS310 = _M0L6_2atmpS1806 / _M0L4baseS311;
      continue;
    }
    break;
  }
  return _M0Lm5countS313;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS307) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS307 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS308;
    int32_t _M0L6_2atmpS1803;
    int32_t _M0L6_2atmpS1802;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS308 = moonbit_clz32(_M0L5valueS307);
    _M0L6_2atmpS1803 = 31 - _M0L14leading__zerosS308;
    _M0L6_2atmpS1802 = _M0L6_2atmpS1803 / 4;
    return _M0L6_2atmpS1802 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS306) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS306 >= 100000u) {
    if (_M0L5valueS306 >= 10000000u) {
      if (_M0L5valueS306 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS306 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS306 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS306 >= 1000u) {
    if (_M0L5valueS306 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS306 >= 100u) {
    return 3;
  } else if (_M0L5valueS306 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS296,
  uint32_t _M0L3numS284,
  int32_t _M0L12digit__startS287,
  int32_t _M0L10total__lenS286
) {
  uint32_t _M0Lm3numS283;
  int32_t _M0Lm6offsetS285;
  uint32_t _M0L6_2atmpS1801;
  int32_t _M0Lm9remainingS298;
  int32_t _M0L6_2atmpS1782;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS283 = _M0L3numS284;
  _M0Lm6offsetS285 = _M0L10total__lenS286 - _M0L12digit__startS287;
  while (1) {
    uint32_t _M0L6_2atmpS1745 = _M0Lm3numS283;
    if (_M0L6_2atmpS1745 >= 10000u) {
      uint32_t _M0L6_2atmpS1768 = _M0Lm3numS283;
      uint32_t _M0L1tS288 = _M0L6_2atmpS1768 / 10000u;
      uint32_t _M0L6_2atmpS1767 = _M0Lm3numS283;
      uint32_t _M0L6_2atmpS1766 = _M0L6_2atmpS1767 % 10000u;
      int32_t _M0L1rS289 = *(int32_t*)&_M0L6_2atmpS1766;
      int32_t _M0L2d1S290;
      int32_t _M0L2d2S291;
      int32_t _M0L6_2atmpS1746;
      int32_t _M0L6_2atmpS1765;
      int32_t _M0L6_2atmpS1764;
      int32_t _M0L6d1__hiS292;
      int32_t _M0L6_2atmpS1763;
      int32_t _M0L6_2atmpS1762;
      int32_t _M0L6d1__loS293;
      int32_t _M0L6_2atmpS1761;
      int32_t _M0L6_2atmpS1760;
      int32_t _M0L6d2__hiS294;
      int32_t _M0L6_2atmpS1759;
      int32_t _M0L6_2atmpS1758;
      int32_t _M0L6d2__loS295;
      int32_t _M0L6_2atmpS1748;
      int32_t _M0L6_2atmpS1747;
      int32_t _M0L6_2atmpS1751;
      int32_t _M0L6_2atmpS1750;
      int32_t _M0L6_2atmpS1749;
      int32_t _M0L6_2atmpS1754;
      int32_t _M0L6_2atmpS1753;
      int32_t _M0L6_2atmpS1752;
      int32_t _M0L6_2atmpS1757;
      int32_t _M0L6_2atmpS1756;
      int32_t _M0L6_2atmpS1755;
      _M0Lm3numS283 = _M0L1tS288;
      _M0L2d1S290 = _M0L1rS289 / 100;
      _M0L2d2S291 = _M0L1rS289 % 100;
      _M0L6_2atmpS1746 = _M0Lm6offsetS285;
      _M0Lm6offsetS285 = _M0L6_2atmpS1746 - 4;
      _M0L6_2atmpS1765 = _M0L2d1S290 / 10;
      _M0L6_2atmpS1764 = 48 + _M0L6_2atmpS1765;
      _M0L6d1__hiS292 = (uint16_t)_M0L6_2atmpS1764;
      _M0L6_2atmpS1763 = _M0L2d1S290 % 10;
      _M0L6_2atmpS1762 = 48 + _M0L6_2atmpS1763;
      _M0L6d1__loS293 = (uint16_t)_M0L6_2atmpS1762;
      _M0L6_2atmpS1761 = _M0L2d2S291 / 10;
      _M0L6_2atmpS1760 = 48 + _M0L6_2atmpS1761;
      _M0L6d2__hiS294 = (uint16_t)_M0L6_2atmpS1760;
      _M0L6_2atmpS1759 = _M0L2d2S291 % 10;
      _M0L6_2atmpS1758 = 48 + _M0L6_2atmpS1759;
      _M0L6d2__loS295 = (uint16_t)_M0L6_2atmpS1758;
      _M0L6_2atmpS1748 = _M0Lm6offsetS285;
      _M0L6_2atmpS1747 = _M0L12digit__startS287 + _M0L6_2atmpS1748;
      _M0L6bufferS296[_M0L6_2atmpS1747] = _M0L6d1__hiS292;
      _M0L6_2atmpS1751 = _M0Lm6offsetS285;
      _M0L6_2atmpS1750 = _M0L12digit__startS287 + _M0L6_2atmpS1751;
      _M0L6_2atmpS1749 = _M0L6_2atmpS1750 + 1;
      _M0L6bufferS296[_M0L6_2atmpS1749] = _M0L6d1__loS293;
      _M0L6_2atmpS1754 = _M0Lm6offsetS285;
      _M0L6_2atmpS1753 = _M0L12digit__startS287 + _M0L6_2atmpS1754;
      _M0L6_2atmpS1752 = _M0L6_2atmpS1753 + 2;
      _M0L6bufferS296[_M0L6_2atmpS1752] = _M0L6d2__hiS294;
      _M0L6_2atmpS1757 = _M0Lm6offsetS285;
      _M0L6_2atmpS1756 = _M0L12digit__startS287 + _M0L6_2atmpS1757;
      _M0L6_2atmpS1755 = _M0L6_2atmpS1756 + 3;
      _M0L6bufferS296[_M0L6_2atmpS1755] = _M0L6d2__loS295;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1801 = _M0Lm3numS283;
  _M0Lm9remainingS298 = *(int32_t*)&_M0L6_2atmpS1801;
  while (1) {
    int32_t _M0L6_2atmpS1769 = _M0Lm9remainingS298;
    if (_M0L6_2atmpS1769 >= 100) {
      int32_t _M0L6_2atmpS1781 = _M0Lm9remainingS298;
      int32_t _M0L1tS299 = _M0L6_2atmpS1781 / 100;
      int32_t _M0L6_2atmpS1780 = _M0Lm9remainingS298;
      int32_t _M0L1dS300 = _M0L6_2atmpS1780 % 100;
      int32_t _M0L6_2atmpS1770;
      int32_t _M0L6_2atmpS1779;
      int32_t _M0L6_2atmpS1778;
      int32_t _M0L5d__hiS301;
      int32_t _M0L6_2atmpS1777;
      int32_t _M0L6_2atmpS1776;
      int32_t _M0L5d__loS302;
      int32_t _M0L6_2atmpS1772;
      int32_t _M0L6_2atmpS1771;
      int32_t _M0L6_2atmpS1775;
      int32_t _M0L6_2atmpS1774;
      int32_t _M0L6_2atmpS1773;
      _M0Lm9remainingS298 = _M0L1tS299;
      _M0L6_2atmpS1770 = _M0Lm6offsetS285;
      _M0Lm6offsetS285 = _M0L6_2atmpS1770 - 2;
      _M0L6_2atmpS1779 = _M0L1dS300 / 10;
      _M0L6_2atmpS1778 = 48 + _M0L6_2atmpS1779;
      _M0L5d__hiS301 = (uint16_t)_M0L6_2atmpS1778;
      _M0L6_2atmpS1777 = _M0L1dS300 % 10;
      _M0L6_2atmpS1776 = 48 + _M0L6_2atmpS1777;
      _M0L5d__loS302 = (uint16_t)_M0L6_2atmpS1776;
      _M0L6_2atmpS1772 = _M0Lm6offsetS285;
      _M0L6_2atmpS1771 = _M0L12digit__startS287 + _M0L6_2atmpS1772;
      _M0L6bufferS296[_M0L6_2atmpS1771] = _M0L5d__hiS301;
      _M0L6_2atmpS1775 = _M0Lm6offsetS285;
      _M0L6_2atmpS1774 = _M0L12digit__startS287 + _M0L6_2atmpS1775;
      _M0L6_2atmpS1773 = _M0L6_2atmpS1774 + 1;
      _M0L6bufferS296[_M0L6_2atmpS1773] = _M0L5d__loS302;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1782 = _M0Lm9remainingS298;
  if (_M0L6_2atmpS1782 >= 10) {
    int32_t _M0L6_2atmpS1783 = _M0Lm6offsetS285;
    int32_t _M0L6_2atmpS1794;
    int32_t _M0L6_2atmpS1793;
    int32_t _M0L6_2atmpS1792;
    int32_t _M0L5d__hiS304;
    int32_t _M0L6_2atmpS1791;
    int32_t _M0L6_2atmpS1790;
    int32_t _M0L6_2atmpS1789;
    int32_t _M0L5d__loS305;
    int32_t _M0L6_2atmpS1785;
    int32_t _M0L6_2atmpS1784;
    int32_t _M0L6_2atmpS1788;
    int32_t _M0L6_2atmpS1787;
    int32_t _M0L6_2atmpS1786;
    _M0Lm6offsetS285 = _M0L6_2atmpS1783 - 2;
    _M0L6_2atmpS1794 = _M0Lm9remainingS298;
    _M0L6_2atmpS1793 = _M0L6_2atmpS1794 / 10;
    _M0L6_2atmpS1792 = 48 + _M0L6_2atmpS1793;
    _M0L5d__hiS304 = (uint16_t)_M0L6_2atmpS1792;
    _M0L6_2atmpS1791 = _M0Lm9remainingS298;
    _M0L6_2atmpS1790 = _M0L6_2atmpS1791 % 10;
    _M0L6_2atmpS1789 = 48 + _M0L6_2atmpS1790;
    _M0L5d__loS305 = (uint16_t)_M0L6_2atmpS1789;
    _M0L6_2atmpS1785 = _M0Lm6offsetS285;
    _M0L6_2atmpS1784 = _M0L12digit__startS287 + _M0L6_2atmpS1785;
    _M0L6bufferS296[_M0L6_2atmpS1784] = _M0L5d__hiS304;
    _M0L6_2atmpS1788 = _M0Lm6offsetS285;
    _M0L6_2atmpS1787 = _M0L12digit__startS287 + _M0L6_2atmpS1788;
    _M0L6_2atmpS1786 = _M0L6_2atmpS1787 + 1;
    _M0L6bufferS296[_M0L6_2atmpS1786] = _M0L5d__loS305;
    moonbit_decref(_M0L6bufferS296);
  } else {
    int32_t _M0L6_2atmpS1795 = _M0Lm6offsetS285;
    int32_t _M0L6_2atmpS1800;
    int32_t _M0L6_2atmpS1796;
    int32_t _M0L6_2atmpS1799;
    int32_t _M0L6_2atmpS1798;
    int32_t _M0L6_2atmpS1797;
    _M0Lm6offsetS285 = _M0L6_2atmpS1795 - 1;
    _M0L6_2atmpS1800 = _M0Lm6offsetS285;
    _M0L6_2atmpS1796 = _M0L12digit__startS287 + _M0L6_2atmpS1800;
    _M0L6_2atmpS1799 = _M0Lm9remainingS298;
    _M0L6_2atmpS1798 = 48 + _M0L6_2atmpS1799;
    _M0L6_2atmpS1797 = (uint16_t)_M0L6_2atmpS1798;
    _M0L6bufferS296[_M0L6_2atmpS1796] = _M0L6_2atmpS1797;
    moonbit_decref(_M0L6bufferS296);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS278,
  uint32_t _M0L3numS272,
  int32_t _M0L12digit__startS270,
  int32_t _M0L10total__lenS269,
  int32_t _M0L5radixS274
) {
  int32_t _M0Lm6offsetS268;
  uint32_t _M0Lm1nS271;
  uint32_t _M0L4baseS273;
  int32_t _M0L6_2atmpS1727;
  int32_t _M0L6_2atmpS1726;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS268 = _M0L10total__lenS269 - _M0L12digit__startS270;
  _M0Lm1nS271 = _M0L3numS272;
  _M0L4baseS273 = *(uint32_t*)&_M0L5radixS274;
  _M0L6_2atmpS1727 = _M0L5radixS274 - 1;
  _M0L6_2atmpS1726 = _M0L5radixS274 & _M0L6_2atmpS1727;
  if (_M0L6_2atmpS1726 == 0) {
    int32_t _M0L5shiftS275;
    uint32_t _M0L4maskS276;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS275 = moonbit_ctz32(_M0L5radixS274);
    _M0L4maskS276 = _M0L4baseS273 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1728 = _M0Lm1nS271;
      if (_M0L6_2atmpS1728 > 0u) {
        int32_t _M0L6_2atmpS1729 = _M0Lm6offsetS268;
        uint32_t _M0L6_2atmpS1735;
        uint32_t _M0L6_2atmpS1734;
        int32_t _M0L5digitS277;
        int32_t _M0L6_2atmpS1732;
        int32_t _M0L6_2atmpS1730;
        int32_t _M0L6_2atmpS1731;
        uint32_t _M0L6_2atmpS1733;
        _M0Lm6offsetS268 = _M0L6_2atmpS1729 - 1;
        _M0L6_2atmpS1735 = _M0Lm1nS271;
        _M0L6_2atmpS1734 = _M0L6_2atmpS1735 & _M0L4maskS276;
        _M0L5digitS277 = *(int32_t*)&_M0L6_2atmpS1734;
        _M0L6_2atmpS1732 = _M0Lm6offsetS268;
        _M0L6_2atmpS1730 = _M0L12digit__startS270 + _M0L6_2atmpS1732;
        _M0L6_2atmpS1731
        = ((moonbit_string_t)moonbit_string_literal_57.data)[
          _M0L5digitS277
        ];
        _M0L6bufferS278[_M0L6_2atmpS1730] = _M0L6_2atmpS1731;
        _M0L6_2atmpS1733 = _M0Lm1nS271;
        _M0Lm1nS271 = _M0L6_2atmpS1733 >> (_M0L5shiftS275 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS278);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1736 = _M0Lm1nS271;
      if (_M0L6_2atmpS1736 > 0u) {
        int32_t _M0L6_2atmpS1737 = _M0Lm6offsetS268;
        uint32_t _M0L6_2atmpS1744;
        uint32_t _M0L1qS280;
        uint32_t _M0L6_2atmpS1742;
        uint32_t _M0L6_2atmpS1743;
        uint32_t _M0L6_2atmpS1741;
        int32_t _M0L5digitS281;
        int32_t _M0L6_2atmpS1740;
        int32_t _M0L6_2atmpS1738;
        int32_t _M0L6_2atmpS1739;
        _M0Lm6offsetS268 = _M0L6_2atmpS1737 - 1;
        _M0L6_2atmpS1744 = _M0Lm1nS271;
        _M0L1qS280 = _M0L6_2atmpS1744 / _M0L4baseS273;
        _M0L6_2atmpS1742 = _M0Lm1nS271;
        _M0L6_2atmpS1743 = _M0L1qS280 * _M0L4baseS273;
        _M0L6_2atmpS1741 = _M0L6_2atmpS1742 - _M0L6_2atmpS1743;
        _M0L5digitS281 = *(int32_t*)&_M0L6_2atmpS1741;
        _M0L6_2atmpS1740 = _M0Lm6offsetS268;
        _M0L6_2atmpS1738 = _M0L12digit__startS270 + _M0L6_2atmpS1740;
        _M0L6_2atmpS1739
        = ((moonbit_string_t)moonbit_string_literal_57.data)[
          _M0L5digitS281
        ];
        _M0L6bufferS278[_M0L6_2atmpS1738] = _M0L6_2atmpS1739;
        _M0Lm1nS271 = _M0L1qS280;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS278);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS265,
  uint32_t _M0L3numS261,
  int32_t _M0L12digit__startS259,
  int32_t _M0L10total__lenS258
) {
  int32_t _M0Lm6offsetS257;
  uint32_t _M0Lm1nS260;
  int32_t _M0L6_2atmpS1722;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS257 = _M0L10total__lenS258 - _M0L12digit__startS259;
  _M0Lm1nS260 = _M0L3numS261;
  while (1) {
    int32_t _M0L6_2atmpS1710 = _M0Lm6offsetS257;
    if (_M0L6_2atmpS1710 >= 2) {
      int32_t _M0L6_2atmpS1711 = _M0Lm6offsetS257;
      uint32_t _M0L6_2atmpS1721;
      uint32_t _M0L6_2atmpS1720;
      int32_t _M0L9byte__valS262;
      int32_t _M0L2hiS263;
      int32_t _M0L2loS264;
      int32_t _M0L6_2atmpS1714;
      int32_t _M0L6_2atmpS1712;
      int32_t _M0L6_2atmpS1713;
      int32_t _M0L6_2atmpS1718;
      int32_t _M0L6_2atmpS1717;
      int32_t _M0L6_2atmpS1715;
      int32_t _M0L6_2atmpS1716;
      uint32_t _M0L6_2atmpS1719;
      _M0Lm6offsetS257 = _M0L6_2atmpS1711 - 2;
      _M0L6_2atmpS1721 = _M0Lm1nS260;
      _M0L6_2atmpS1720 = _M0L6_2atmpS1721 & 255u;
      _M0L9byte__valS262 = *(int32_t*)&_M0L6_2atmpS1720;
      _M0L2hiS263 = _M0L9byte__valS262 / 16;
      _M0L2loS264 = _M0L9byte__valS262 % 16;
      _M0L6_2atmpS1714 = _M0Lm6offsetS257;
      _M0L6_2atmpS1712 = _M0L12digit__startS259 + _M0L6_2atmpS1714;
      _M0L6_2atmpS1713
      = ((moonbit_string_t)moonbit_string_literal_57.data)[
        _M0L2hiS263
      ];
      _M0L6bufferS265[_M0L6_2atmpS1712] = _M0L6_2atmpS1713;
      _M0L6_2atmpS1718 = _M0Lm6offsetS257;
      _M0L6_2atmpS1717 = _M0L12digit__startS259 + _M0L6_2atmpS1718;
      _M0L6_2atmpS1715 = _M0L6_2atmpS1717 + 1;
      _M0L6_2atmpS1716
      = ((moonbit_string_t)moonbit_string_literal_57.data)[
        _M0L2loS264
      ];
      _M0L6bufferS265[_M0L6_2atmpS1715] = _M0L6_2atmpS1716;
      _M0L6_2atmpS1719 = _M0Lm1nS260;
      _M0Lm1nS260 = _M0L6_2atmpS1719 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1722 = _M0Lm6offsetS257;
  if (_M0L6_2atmpS1722 == 1) {
    uint32_t _M0L6_2atmpS1725 = _M0Lm1nS260;
    uint32_t _M0L6_2atmpS1724 = _M0L6_2atmpS1725 & 15u;
    int32_t _M0L6nibbleS267 = *(int32_t*)&_M0L6_2atmpS1724;
    int32_t _M0L6_2atmpS1723 =
      ((moonbit_string_t)moonbit_string_literal_57.data)[_M0L6nibbleS267];
    _M0L6bufferS265[_M0L12digit__startS259] = _M0L6_2atmpS1723;
    moonbit_decref(_M0L6bufferS265);
  } else {
    moonbit_decref(_M0L6bufferS265);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS252) {
  struct _M0TWEOs* _M0L7_2afuncS251;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS251 = _M0L4selfS252;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS251->code(_M0L7_2afuncS251);
}

struct _M0TUsRPB4JsonE* _M0MPB4Iter4nextGUsRPB4JsonEE(
  struct _M0TWEOUsRPB4JsonE* _M0L4selfS254
) {
  struct _M0TWEOUsRPB4JsonE* _M0L7_2afuncS253;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS253 = _M0L4selfS254;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS253->code(_M0L7_2afuncS253);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS256) {
  struct _M0TWEOc* _M0L7_2afuncS255;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS255 = _M0L4selfS256;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS255->code(_M0L7_2afuncS255);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS242
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS241;
  struct _M0TPB6Logger _M0L6_2atmpS1705;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS241 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS241);
  _M0L6_2atmpS1705
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS241
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS242, _M0L6_2atmpS1705);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS241);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS244
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS243;
  struct _M0TPB6Logger _M0L6_2atmpS1706;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS243 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS243);
  _M0L6_2atmpS1706
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS243
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS244, _M0L6_2atmpS1706);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS243);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGmE(
  uint64_t _M0L4selfS246
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS245;
  struct _M0TPB6Logger _M0L6_2atmpS1707;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS245 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS245);
  _M0L6_2atmpS1707
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS245
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16uint646UInt64PB4Show6output(_M0L4selfS246, _M0L6_2atmpS1707);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS245);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRP48clawteam8clawteam8internal5errno5ErrnoE(
  void* _M0L4selfS248
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS247;
  struct _M0TPB6Logger _M0L6_2atmpS1708;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS247 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS247);
  _M0L6_2atmpS1708
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS247
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IP48clawteam8clawteam8internal5errno5ErrnoPB4Show6output(_M0L4selfS248, _M0L6_2atmpS1708);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS247);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS250
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS249;
  struct _M0TPB6Logger _M0L6_2atmpS1709;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS249 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS249);
  _M0L6_2atmpS1709
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS249
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS250, _M0L6_2atmpS1709);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS249);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS240
) {
  int32_t _M0L8_2afieldS4070;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4070 = _M0L4selfS240.$1;
  moonbit_decref(_M0L4selfS240.$0);
  return _M0L8_2afieldS4070;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS239
) {
  int32_t _M0L3endS1703;
  int32_t _M0L8_2afieldS4071;
  int32_t _M0L5startS1704;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1703 = _M0L4selfS239.$2;
  _M0L8_2afieldS4071 = _M0L4selfS239.$1;
  moonbit_decref(_M0L4selfS239.$0);
  _M0L5startS1704 = _M0L8_2afieldS4071;
  return _M0L3endS1703 - _M0L5startS1704;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS238
) {
  moonbit_string_t _M0L8_2afieldS4072;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4072 = _M0L4selfS238.$0;
  return _M0L8_2afieldS4072;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS234,
  moonbit_string_t _M0L5valueS235,
  int32_t _M0L5startS236,
  int32_t _M0L3lenS237
) {
  int32_t _M0L6_2atmpS1702;
  int64_t _M0L6_2atmpS1701;
  struct _M0TPC16string10StringView _M0L6_2atmpS1700;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1702 = _M0L5startS236 + _M0L3lenS237;
  _M0L6_2atmpS1701 = (int64_t)_M0L6_2atmpS1702;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1700
  = _M0MPC16string6String11sub_2einner(_M0L5valueS235, _M0L5startS236, _M0L6_2atmpS1701);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS234, _M0L6_2atmpS1700);
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
  int32_t _if__result_4347;
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
      _if__result_4347 = _M0L3endS228 <= _M0L3lenS226;
    } else {
      _if__result_4347 = 0;
    }
  } else {
    _if__result_4347 = 0;
  }
  if (_if__result_4347) {
    if (_M0L5startS232 < _M0L3lenS226) {
      int32_t _M0L6_2atmpS1697 = _M0L4selfS227[_M0L5startS232];
      int32_t _M0L6_2atmpS1696;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1696
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1697);
      if (!_M0L6_2atmpS1696) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS228 < _M0L3lenS226) {
      int32_t _M0L6_2atmpS1699 = _M0L4selfS227[_M0L3endS228];
      int32_t _M0L6_2atmpS1698;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1698
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1699);
      if (!_M0L6_2atmpS1698) {
        
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
  uint32_t _M0L6_2atmpS1695;
  uint32_t _M0L6_2atmpS1694;
  struct _M0TPB6Hasher* _block_4348;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1695 = *(uint32_t*)&_M0L4seedS218;
  _M0L6_2atmpS1694 = _M0L6_2atmpS1695 + 374761393u;
  _block_4348
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4348)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4348->$0 = _M0L6_2atmpS1694;
  return _block_4348;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS217) {
  uint32_t _M0L6_2atmpS1693;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1693 = _M0MPB6Hasher9avalanche(_M0L4selfS217);
  return *(int32_t*)&_M0L6_2atmpS1693;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS216) {
  uint32_t _M0L8_2afieldS4073;
  uint32_t _M0Lm3accS215;
  uint32_t _M0L6_2atmpS1682;
  uint32_t _M0L6_2atmpS1684;
  uint32_t _M0L6_2atmpS1683;
  uint32_t _M0L6_2atmpS1685;
  uint32_t _M0L6_2atmpS1686;
  uint32_t _M0L6_2atmpS1688;
  uint32_t _M0L6_2atmpS1687;
  uint32_t _M0L6_2atmpS1689;
  uint32_t _M0L6_2atmpS1690;
  uint32_t _M0L6_2atmpS1692;
  uint32_t _M0L6_2atmpS1691;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS4073 = _M0L4selfS216->$0;
  moonbit_decref(_M0L4selfS216);
  _M0Lm3accS215 = _M0L8_2afieldS4073;
  _M0L6_2atmpS1682 = _M0Lm3accS215;
  _M0L6_2atmpS1684 = _M0Lm3accS215;
  _M0L6_2atmpS1683 = _M0L6_2atmpS1684 >> 15;
  _M0Lm3accS215 = _M0L6_2atmpS1682 ^ _M0L6_2atmpS1683;
  _M0L6_2atmpS1685 = _M0Lm3accS215;
  _M0Lm3accS215 = _M0L6_2atmpS1685 * 2246822519u;
  _M0L6_2atmpS1686 = _M0Lm3accS215;
  _M0L6_2atmpS1688 = _M0Lm3accS215;
  _M0L6_2atmpS1687 = _M0L6_2atmpS1688 >> 13;
  _M0Lm3accS215 = _M0L6_2atmpS1686 ^ _M0L6_2atmpS1687;
  _M0L6_2atmpS1689 = _M0Lm3accS215;
  _M0Lm3accS215 = _M0L6_2atmpS1689 * 3266489917u;
  _M0L6_2atmpS1690 = _M0Lm3accS215;
  _M0L6_2atmpS1692 = _M0Lm3accS215;
  _M0L6_2atmpS1691 = _M0L6_2atmpS1692 >> 16;
  _M0Lm3accS215 = _M0L6_2atmpS1690 ^ _M0L6_2atmpS1691;
  return _M0Lm3accS215;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS213,
  moonbit_string_t _M0L1yS214
) {
  int32_t _M0L6_2atmpS4074;
  int32_t _M0L6_2atmpS1681;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS4074 = moonbit_val_array_equal(_M0L1xS213, _M0L1yS214);
  moonbit_decref(_M0L1xS213);
  moonbit_decref(_M0L1yS214);
  _M0L6_2atmpS1681 = _M0L6_2atmpS4074;
  return !_M0L6_2atmpS1681;
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
  int64_t _M0L6_2atmpS1680;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1680 = (int64_t)_M0L4selfS208;
  return *(uint64_t*)&_M0L6_2atmpS1680;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS206,
  int32_t _M0L5valueS207
) {
  uint32_t _M0L6_2atmpS1679;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1679 = *(uint32_t*)&_M0L5valueS207;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS206, _M0L6_2atmpS1679);
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
      int32_t _if__result_4350;
      moonbit_string_t* _M0L8_2afieldS4076;
      moonbit_string_t* _M0L3bufS1677;
      moonbit_string_t _M0L6_2atmpS4075;
      moonbit_string_t _M0L4itemS202;
      int32_t _M0L6_2atmpS1678;
      if (_M0L1iS201 != 0) {
        moonbit_incref(_M0L3bufS197);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS197, (moonbit_string_t)moonbit_string_literal_59.data);
      }
      if (_M0L1iS201 < 0) {
        _if__result_4350 = 1;
      } else {
        int32_t _M0L3lenS1676 = _M0L7_2aselfS198->$1;
        _if__result_4350 = _M0L1iS201 >= _M0L3lenS1676;
      }
      if (_if__result_4350) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS4076 = _M0L7_2aselfS198->$0;
      _M0L3bufS1677 = _M0L8_2afieldS4076;
      _M0L6_2atmpS4075 = (moonbit_string_t)_M0L3bufS1677[_M0L1iS201];
      _M0L4itemS202 = _M0L6_2atmpS4075;
      if (_M0L4itemS202 == 0) {
        moonbit_incref(_M0L3bufS197);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS197, (moonbit_string_t)moonbit_string_literal_22.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS203 = _M0L4itemS202;
        moonbit_string_t _M0L6_2alocS204 = _M0L7_2aSomeS203;
        moonbit_string_t _M0L6_2atmpS1675;
        moonbit_incref(_M0L6_2alocS204);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1675
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS204);
        moonbit_incref(_M0L3bufS197);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS197, _M0L6_2atmpS1675);
      }
      _M0L6_2atmpS1678 = _M0L1iS201 + 1;
      _M0L1iS201 = _M0L6_2atmpS1678;
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
  moonbit_string_t _M0L6_2atmpS1674;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1673;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1674 = _M0L4selfS196;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1673 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1674);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1673);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS195
) {
  struct _M0TPB13StringBuilder* _M0L2sbS194;
  struct _M0TPC16string10StringView _M0L8_2afieldS4089;
  struct _M0TPC16string10StringView _M0L3pkgS1658;
  moonbit_string_t _M0L6_2atmpS1657;
  moonbit_string_t _M0L6_2atmpS4088;
  moonbit_string_t _M0L6_2atmpS1656;
  moonbit_string_t _M0L6_2atmpS4087;
  moonbit_string_t _M0L6_2atmpS1655;
  struct _M0TPC16string10StringView _M0L8_2afieldS4086;
  struct _M0TPC16string10StringView _M0L8filenameS1659;
  struct _M0TPC16string10StringView _M0L8_2afieldS4085;
  struct _M0TPC16string10StringView _M0L11start__lineS1662;
  moonbit_string_t _M0L6_2atmpS1661;
  moonbit_string_t _M0L6_2atmpS4084;
  moonbit_string_t _M0L6_2atmpS1660;
  struct _M0TPC16string10StringView _M0L8_2afieldS4083;
  struct _M0TPC16string10StringView _M0L13start__columnS1665;
  moonbit_string_t _M0L6_2atmpS1664;
  moonbit_string_t _M0L6_2atmpS4082;
  moonbit_string_t _M0L6_2atmpS1663;
  struct _M0TPC16string10StringView _M0L8_2afieldS4081;
  struct _M0TPC16string10StringView _M0L9end__lineS1668;
  moonbit_string_t _M0L6_2atmpS1667;
  moonbit_string_t _M0L6_2atmpS4080;
  moonbit_string_t _M0L6_2atmpS1666;
  struct _M0TPC16string10StringView _M0L8_2afieldS4079;
  int32_t _M0L6_2acntS4201;
  struct _M0TPC16string10StringView _M0L11end__columnS1672;
  moonbit_string_t _M0L6_2atmpS1671;
  moonbit_string_t _M0L6_2atmpS4078;
  moonbit_string_t _M0L6_2atmpS1670;
  moonbit_string_t _M0L6_2atmpS4077;
  moonbit_string_t _M0L6_2atmpS1669;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS194 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS4089
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$0_1, _M0L4selfS195->$0_2, _M0L4selfS195->$0_0
  };
  _M0L3pkgS1658 = _M0L8_2afieldS4089;
  moonbit_incref(_M0L3pkgS1658.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1657
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1658);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4088
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_60.data, _M0L6_2atmpS1657);
  moonbit_decref(_M0L6_2atmpS1657);
  _M0L6_2atmpS1656 = _M0L6_2atmpS4088;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4087
  = moonbit_add_string(_M0L6_2atmpS1656, (moonbit_string_t)moonbit_string_literal_61.data);
  moonbit_decref(_M0L6_2atmpS1656);
  _M0L6_2atmpS1655 = _M0L6_2atmpS4087;
  moonbit_incref(_M0L2sbS194);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1655);
  moonbit_incref(_M0L2sbS194);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, (moonbit_string_t)moonbit_string_literal_62.data);
  _M0L8_2afieldS4086
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$1_1, _M0L4selfS195->$1_2, _M0L4selfS195->$1_0
  };
  _M0L8filenameS1659 = _M0L8_2afieldS4086;
  moonbit_incref(_M0L8filenameS1659.$0);
  moonbit_incref(_M0L2sbS194);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS194, _M0L8filenameS1659);
  _M0L8_2afieldS4085
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$2_1, _M0L4selfS195->$2_2, _M0L4selfS195->$2_0
  };
  _M0L11start__lineS1662 = _M0L8_2afieldS4085;
  moonbit_incref(_M0L11start__lineS1662.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1661
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1662);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4084
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_63.data, _M0L6_2atmpS1661);
  moonbit_decref(_M0L6_2atmpS1661);
  _M0L6_2atmpS1660 = _M0L6_2atmpS4084;
  moonbit_incref(_M0L2sbS194);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1660);
  _M0L8_2afieldS4083
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$3_1, _M0L4selfS195->$3_2, _M0L4selfS195->$3_0
  };
  _M0L13start__columnS1665 = _M0L8_2afieldS4083;
  moonbit_incref(_M0L13start__columnS1665.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1664
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1665);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4082
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_64.data, _M0L6_2atmpS1664);
  moonbit_decref(_M0L6_2atmpS1664);
  _M0L6_2atmpS1663 = _M0L6_2atmpS4082;
  moonbit_incref(_M0L2sbS194);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1663);
  _M0L8_2afieldS4081
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$4_1, _M0L4selfS195->$4_2, _M0L4selfS195->$4_0
  };
  _M0L9end__lineS1668 = _M0L8_2afieldS4081;
  moonbit_incref(_M0L9end__lineS1668.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1667
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1668);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4080
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_65.data, _M0L6_2atmpS1667);
  moonbit_decref(_M0L6_2atmpS1667);
  _M0L6_2atmpS1666 = _M0L6_2atmpS4080;
  moonbit_incref(_M0L2sbS194);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1666);
  _M0L8_2afieldS4079
  = (struct _M0TPC16string10StringView){
    _M0L4selfS195->$5_1, _M0L4selfS195->$5_2, _M0L4selfS195->$5_0
  };
  _M0L6_2acntS4201 = Moonbit_object_header(_M0L4selfS195)->rc;
  if (_M0L6_2acntS4201 > 1) {
    int32_t _M0L11_2anew__cntS4207 = _M0L6_2acntS4201 - 1;
    Moonbit_object_header(_M0L4selfS195)->rc = _M0L11_2anew__cntS4207;
    moonbit_incref(_M0L8_2afieldS4079.$0);
  } else if (_M0L6_2acntS4201 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4206 =
      (struct _M0TPC16string10StringView){_M0L4selfS195->$4_1,
                                            _M0L4selfS195->$4_2,
                                            _M0L4selfS195->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4205;
    struct _M0TPC16string10StringView _M0L8_2afieldS4204;
    struct _M0TPC16string10StringView _M0L8_2afieldS4203;
    struct _M0TPC16string10StringView _M0L8_2afieldS4202;
    moonbit_decref(_M0L8_2afieldS4206.$0);
    _M0L8_2afieldS4205
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$3_1, _M0L4selfS195->$3_2, _M0L4selfS195->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4205.$0);
    _M0L8_2afieldS4204
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$2_1, _M0L4selfS195->$2_2, _M0L4selfS195->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4204.$0);
    _M0L8_2afieldS4203
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$1_1, _M0L4selfS195->$1_2, _M0L4selfS195->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4203.$0);
    _M0L8_2afieldS4202
    = (struct _M0TPC16string10StringView){
      _M0L4selfS195->$0_1, _M0L4selfS195->$0_2, _M0L4selfS195->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4202.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS195);
  }
  _M0L11end__columnS1672 = _M0L8_2afieldS4079;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1671
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1672);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4078
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_66.data, _M0L6_2atmpS1671);
  moonbit_decref(_M0L6_2atmpS1671);
  _M0L6_2atmpS1670 = _M0L6_2atmpS4078;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS4077
  = moonbit_add_string(_M0L6_2atmpS1670, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1670);
  _M0L6_2atmpS1669 = _M0L6_2atmpS4077;
  moonbit_incref(_M0L2sbS194);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS194, _M0L6_2atmpS1669);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS194);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS192,
  moonbit_string_t _M0L3strS193
) {
  int32_t _M0L3lenS1645;
  int32_t _M0L6_2atmpS1647;
  int32_t _M0L6_2atmpS1646;
  int32_t _M0L6_2atmpS1644;
  moonbit_bytes_t _M0L8_2afieldS4091;
  moonbit_bytes_t _M0L4dataS1648;
  int32_t _M0L3lenS1649;
  int32_t _M0L6_2atmpS1650;
  int32_t _M0L3lenS1652;
  int32_t _M0L6_2atmpS4090;
  int32_t _M0L6_2atmpS1654;
  int32_t _M0L6_2atmpS1653;
  int32_t _M0L6_2atmpS1651;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1645 = _M0L4selfS192->$1;
  _M0L6_2atmpS1647 = Moonbit_array_length(_M0L3strS193);
  _M0L6_2atmpS1646 = _M0L6_2atmpS1647 * 2;
  _M0L6_2atmpS1644 = _M0L3lenS1645 + _M0L6_2atmpS1646;
  moonbit_incref(_M0L4selfS192);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS192, _M0L6_2atmpS1644);
  _M0L8_2afieldS4091 = _M0L4selfS192->$0;
  _M0L4dataS1648 = _M0L8_2afieldS4091;
  _M0L3lenS1649 = _M0L4selfS192->$1;
  _M0L6_2atmpS1650 = Moonbit_array_length(_M0L3strS193);
  moonbit_incref(_M0L4dataS1648);
  moonbit_incref(_M0L3strS193);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1648, _M0L3lenS1649, _M0L3strS193, 0, _M0L6_2atmpS1650);
  _M0L3lenS1652 = _M0L4selfS192->$1;
  _M0L6_2atmpS4090 = Moonbit_array_length(_M0L3strS193);
  moonbit_decref(_M0L3strS193);
  _M0L6_2atmpS1654 = _M0L6_2atmpS4090;
  _M0L6_2atmpS1653 = _M0L6_2atmpS1654 * 2;
  _M0L6_2atmpS1651 = _M0L3lenS1652 + _M0L6_2atmpS1653;
  _M0L4selfS192->$1 = _M0L6_2atmpS1651;
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
  int32_t _M0L6_2atmpS1643;
  int32_t _M0L6_2atmpS1642;
  int32_t _M0L2e1S178;
  int32_t _M0L6_2atmpS1641;
  int32_t _M0L2e2S181;
  int32_t _M0L4len1S183;
  int32_t _M0L4len2S185;
  int32_t _if__result_4351;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1643 = _M0L6lengthS180 * 2;
  _M0L6_2atmpS1642 = _M0L13bytes__offsetS179 + _M0L6_2atmpS1643;
  _M0L2e1S178 = _M0L6_2atmpS1642 - 1;
  _M0L6_2atmpS1641 = _M0L11str__offsetS182 + _M0L6lengthS180;
  _M0L2e2S181 = _M0L6_2atmpS1641 - 1;
  _M0L4len1S183 = Moonbit_array_length(_M0L4selfS184);
  _M0L4len2S185 = Moonbit_array_length(_M0L3strS186);
  if (_M0L6lengthS180 >= 0) {
    if (_M0L13bytes__offsetS179 >= 0) {
      if (_M0L2e1S178 < _M0L4len1S183) {
        if (_M0L11str__offsetS182 >= 0) {
          _if__result_4351 = _M0L2e2S181 < _M0L4len2S185;
        } else {
          _if__result_4351 = 0;
        }
      } else {
        _if__result_4351 = 0;
      }
    } else {
      _if__result_4351 = 0;
    }
  } else {
    _if__result_4351 = 0;
  }
  if (_if__result_4351) {
    int32_t _M0L16end__str__offsetS187 =
      _M0L11str__offsetS182 + _M0L6lengthS180;
    int32_t _M0L1iS188 = _M0L11str__offsetS182;
    int32_t _M0L1jS189 = _M0L13bytes__offsetS179;
    while (1) {
      if (_M0L1iS188 < _M0L16end__str__offsetS187) {
        int32_t _M0L6_2atmpS1638 = _M0L3strS186[_M0L1iS188];
        int32_t _M0L6_2atmpS1637 = (int32_t)_M0L6_2atmpS1638;
        uint32_t _M0L1cS190 = *(uint32_t*)&_M0L6_2atmpS1637;
        uint32_t _M0L6_2atmpS1633 = _M0L1cS190 & 255u;
        int32_t _M0L6_2atmpS1632;
        int32_t _M0L6_2atmpS1634;
        uint32_t _M0L6_2atmpS1636;
        int32_t _M0L6_2atmpS1635;
        int32_t _M0L6_2atmpS1639;
        int32_t _M0L6_2atmpS1640;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1632 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1633);
        if (
          _M0L1jS189 < 0 || _M0L1jS189 >= Moonbit_array_length(_M0L4selfS184)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS184[_M0L1jS189] = _M0L6_2atmpS1632;
        _M0L6_2atmpS1634 = _M0L1jS189 + 1;
        _M0L6_2atmpS1636 = _M0L1cS190 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1635 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1636);
        if (
          _M0L6_2atmpS1634 < 0
          || _M0L6_2atmpS1634 >= Moonbit_array_length(_M0L4selfS184)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS184[_M0L6_2atmpS1634] = _M0L6_2atmpS1635;
        _M0L6_2atmpS1639 = _M0L1iS188 + 1;
        _M0L6_2atmpS1640 = _M0L1jS189 + 2;
        _M0L1iS188 = _M0L6_2atmpS1639;
        _M0L1jS189 = _M0L6_2atmpS1640;
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
  struct _M0TPB6Logger _M0L6_2atmpS1630;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1630
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS175
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16double6DoublePB4Show6output(_M0L3objS174, _M0L6_2atmpS1630);
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS177,
  struct _M0TPC16string10StringView _M0L3objS176
) {
  struct _M0TPB6Logger _M0L6_2atmpS1631;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1631
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS177
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS176, _M0L6_2atmpS1631);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS120
) {
  int32_t _M0L6_2atmpS1629;
  struct _M0TPC16string10StringView _M0L7_2abindS119;
  moonbit_string_t _M0L7_2adataS121;
  int32_t _M0L8_2astartS122;
  int32_t _M0L6_2atmpS1628;
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
  int32_t _M0L6_2atmpS1586;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1629 = Moonbit_array_length(_M0L4reprS120);
  _M0L7_2abindS119
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1629, _M0L4reprS120
  };
  moonbit_incref(_M0L7_2abindS119.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS121 = _M0MPC16string10StringView4data(_M0L7_2abindS119);
  moonbit_incref(_M0L7_2abindS119.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS122
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS119);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1628 = _M0MPC16string10StringView6length(_M0L7_2abindS119);
  _M0L6_2aendS123 = _M0L8_2astartS122 + _M0L6_2atmpS1628;
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
  _M0L6_2atmpS1586 = _M0Lm9_2acursorS124;
  if (_M0L6_2atmpS1586 < _M0L6_2aendS123) {
    int32_t _M0L6_2atmpS1588 = _M0Lm9_2acursorS124;
    int32_t _M0L6_2atmpS1587;
    moonbit_incref(_M0L7_2adataS121);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1587
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1588);
    if (_M0L6_2atmpS1587 == 64) {
      int32_t _M0L6_2atmpS1589 = _M0Lm9_2acursorS124;
      _M0Lm9_2acursorS124 = _M0L6_2atmpS1589 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1590;
        _M0Lm6tag__0S132 = _M0Lm9_2acursorS124;
        _M0L6_2atmpS1590 = _M0Lm9_2acursorS124;
        if (_M0L6_2atmpS1590 < _M0L6_2aendS123) {
          int32_t _M0L6_2atmpS1627 = _M0Lm9_2acursorS124;
          int32_t _M0L10next__charS147;
          int32_t _M0L6_2atmpS1591;
          moonbit_incref(_M0L7_2adataS121);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS147
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1627);
          _M0L6_2atmpS1591 = _M0Lm9_2acursorS124;
          _M0Lm9_2acursorS124 = _M0L6_2atmpS1591 + 1;
          if (_M0L10next__charS147 == 58) {
            int32_t _M0L6_2atmpS1592 = _M0Lm9_2acursorS124;
            if (_M0L6_2atmpS1592 < _M0L6_2aendS123) {
              int32_t _M0L6_2atmpS1593 = _M0Lm9_2acursorS124;
              int32_t _M0L12dispatch__15S148;
              _M0Lm9_2acursorS124 = _M0L6_2atmpS1593 + 1;
              _M0L12dispatch__15S148 = 0;
              loop__label__15_151:;
              while (1) {
                int32_t _M0L6_2atmpS1594;
                switch (_M0L12dispatch__15S148) {
                  case 3: {
                    int32_t _M0L6_2atmpS1597;
                    _M0Lm9tag__1__2S135 = _M0Lm9tag__1__1S134;
                    _M0Lm9tag__1__1S134 = _M0Lm6tag__1S133;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1597 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1597 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1602 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS155;
                      int32_t _M0L6_2atmpS1598;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS155
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1602);
                      _M0L6_2atmpS1598 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1598 + 1;
                      if (_M0L10next__charS155 < 58) {
                        if (_M0L10next__charS155 < 48) {
                          goto join_154;
                        } else {
                          int32_t _M0L6_2atmpS1599;
                          _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                          _M0Lm9tag__2__1S138 = _M0Lm6tag__2S137;
                          _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                          _M0Lm6tag__3S136 = _M0Lm9_2acursorS124;
                          _M0L6_2atmpS1599 = _M0Lm9_2acursorS124;
                          if (_M0L6_2atmpS1599 < _M0L6_2aendS123) {
                            int32_t _M0L6_2atmpS1601 = _M0Lm9_2acursorS124;
                            int32_t _M0L10next__charS157;
                            int32_t _M0L6_2atmpS1600;
                            moonbit_incref(_M0L7_2adataS121);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS157
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1601);
                            _M0L6_2atmpS1600 = _M0Lm9_2acursorS124;
                            _M0Lm9_2acursorS124 = _M0L6_2atmpS1600 + 1;
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
                    int32_t _M0L6_2atmpS1603;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1603 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1603 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1605 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS159;
                      int32_t _M0L6_2atmpS1604;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS159
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1605);
                      _M0L6_2atmpS1604 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1604 + 1;
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
                    int32_t _M0L6_2atmpS1606;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1606 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1606 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1608 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS160;
                      int32_t _M0L6_2atmpS1607;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS160
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1608);
                      _M0L6_2atmpS1607 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1607 + 1;
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
                    int32_t _M0L6_2atmpS1609;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__4S139 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1609 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1609 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1617 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS162;
                      int32_t _M0L6_2atmpS1610;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS162
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1617);
                      _M0L6_2atmpS1610 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1610 + 1;
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
                        int32_t _M0L6_2atmpS1611;
                        _M0Lm9tag__1__2S135 = _M0Lm9tag__1__1S134;
                        _M0Lm9tag__1__1S134 = _M0Lm6tag__1S133;
                        _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                        _M0L6_2atmpS1611 = _M0Lm9_2acursorS124;
                        if (_M0L6_2atmpS1611 < _M0L6_2aendS123) {
                          int32_t _M0L6_2atmpS1616 = _M0Lm9_2acursorS124;
                          int32_t _M0L10next__charS164;
                          int32_t _M0L6_2atmpS1612;
                          moonbit_incref(_M0L7_2adataS121);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS164
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1616);
                          _M0L6_2atmpS1612 = _M0Lm9_2acursorS124;
                          _M0Lm9_2acursorS124 = _M0L6_2atmpS1612 + 1;
                          if (_M0L10next__charS164 < 58) {
                            if (_M0L10next__charS164 < 48) {
                              goto join_163;
                            } else {
                              int32_t _M0L6_2atmpS1613;
                              _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                              _M0Lm9tag__2__1S138 = _M0Lm6tag__2S137;
                              _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                              _M0L6_2atmpS1613 = _M0Lm9_2acursorS124;
                              if (_M0L6_2atmpS1613 < _M0L6_2aendS123) {
                                int32_t _M0L6_2atmpS1615 =
                                  _M0Lm9_2acursorS124;
                                int32_t _M0L10next__charS166;
                                int32_t _M0L6_2atmpS1614;
                                moonbit_incref(_M0L7_2adataS121);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS166
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1615);
                                _M0L6_2atmpS1614 = _M0Lm9_2acursorS124;
                                _M0Lm9_2acursorS124 = _M0L6_2atmpS1614 + 1;
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
                    int32_t _M0L6_2atmpS1618;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1618 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1618 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1620 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS168;
                      int32_t _M0L6_2atmpS1619;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS168
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1620);
                      _M0L6_2atmpS1619 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1619 + 1;
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
                    int32_t _M0L6_2atmpS1621;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__2S137 = _M0Lm9_2acursorS124;
                    _M0Lm6tag__3S136 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1621 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1621 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1623 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS170;
                      int32_t _M0L6_2atmpS1622;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS170
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1623);
                      _M0L6_2atmpS1622 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1622 + 1;
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
                    int32_t _M0L6_2atmpS1624;
                    _M0Lm9tag__1__1S134 = _M0Lm6tag__1S133;
                    _M0Lm6tag__1S133 = _M0Lm9_2acursorS124;
                    _M0L6_2atmpS1624 = _M0Lm9_2acursorS124;
                    if (_M0L6_2atmpS1624 < _M0L6_2aendS123) {
                      int32_t _M0L6_2atmpS1626 = _M0Lm9_2acursorS124;
                      int32_t _M0L10next__charS172;
                      int32_t _M0L6_2atmpS1625;
                      moonbit_incref(_M0L7_2adataS121);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS172
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1626);
                      _M0L6_2atmpS1625 = _M0Lm9_2acursorS124;
                      _M0Lm9_2acursorS124 = _M0L6_2atmpS1625 + 1;
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
                _M0L6_2atmpS1594 = _M0Lm9_2acursorS124;
                if (_M0L6_2atmpS1594 < _M0L6_2aendS123) {
                  int32_t _M0L6_2atmpS1596 = _M0Lm9_2acursorS124;
                  int32_t _M0L10next__charS152;
                  int32_t _M0L6_2atmpS1595;
                  moonbit_incref(_M0L7_2adataS121);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS152
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS121, _M0L6_2atmpS1596);
                  _M0L6_2atmpS1595 = _M0Lm9_2acursorS124;
                  _M0Lm9_2acursorS124 = _M0L6_2atmpS1595 + 1;
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
      int32_t _M0L6_2atmpS1585 = _M0Lm20match__tag__saver__1S128;
      int32_t _M0L6_2atmpS1584 = _M0L6_2atmpS1585 + 1;
      int64_t _M0L6_2atmpS1581 = (int64_t)_M0L6_2atmpS1584;
      int32_t _M0L6_2atmpS1583 = _M0Lm20match__tag__saver__2S129;
      int64_t _M0L6_2atmpS1582 = (int64_t)_M0L6_2atmpS1583;
      struct _M0TPC16string10StringView _M0L11start__lineS141;
      int32_t _M0L6_2atmpS1580;
      int32_t _M0L6_2atmpS1579;
      int64_t _M0L6_2atmpS1576;
      int32_t _M0L6_2atmpS1578;
      int64_t _M0L6_2atmpS1577;
      struct _M0TPC16string10StringView _M0L13start__columnS142;
      int32_t _M0L6_2atmpS1575;
      int64_t _M0L6_2atmpS1572;
      int32_t _M0L6_2atmpS1574;
      int64_t _M0L6_2atmpS1573;
      struct _M0TPC16string10StringView _M0L3pkgS143;
      int32_t _M0L6_2atmpS1571;
      int32_t _M0L6_2atmpS1570;
      int64_t _M0L6_2atmpS1567;
      int32_t _M0L6_2atmpS1569;
      int64_t _M0L6_2atmpS1568;
      struct _M0TPC16string10StringView _M0L8filenameS144;
      int32_t _M0L6_2atmpS1566;
      int32_t _M0L6_2atmpS1565;
      int64_t _M0L6_2atmpS1562;
      int32_t _M0L6_2atmpS1564;
      int64_t _M0L6_2atmpS1563;
      struct _M0TPC16string10StringView _M0L9end__lineS145;
      int32_t _M0L6_2atmpS1561;
      int32_t _M0L6_2atmpS1560;
      int64_t _M0L6_2atmpS1557;
      int32_t _M0L6_2atmpS1559;
      int64_t _M0L6_2atmpS1558;
      struct _M0TPC16string10StringView _M0L11end__columnS146;
      struct _M0TPB13SourceLocRepr* _block_4368;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS141
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1581, _M0L6_2atmpS1582);
      _M0L6_2atmpS1580 = _M0Lm20match__tag__saver__2S129;
      _M0L6_2atmpS1579 = _M0L6_2atmpS1580 + 1;
      _M0L6_2atmpS1576 = (int64_t)_M0L6_2atmpS1579;
      _M0L6_2atmpS1578 = _M0Lm20match__tag__saver__3S130;
      _M0L6_2atmpS1577 = (int64_t)_M0L6_2atmpS1578;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS142
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1576, _M0L6_2atmpS1577);
      _M0L6_2atmpS1575 = _M0L8_2astartS122 + 1;
      _M0L6_2atmpS1572 = (int64_t)_M0L6_2atmpS1575;
      _M0L6_2atmpS1574 = _M0Lm20match__tag__saver__0S127;
      _M0L6_2atmpS1573 = (int64_t)_M0L6_2atmpS1574;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS143
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1572, _M0L6_2atmpS1573);
      _M0L6_2atmpS1571 = _M0Lm20match__tag__saver__0S127;
      _M0L6_2atmpS1570 = _M0L6_2atmpS1571 + 1;
      _M0L6_2atmpS1567 = (int64_t)_M0L6_2atmpS1570;
      _M0L6_2atmpS1569 = _M0Lm20match__tag__saver__1S128;
      _M0L6_2atmpS1568 = (int64_t)_M0L6_2atmpS1569;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS144
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1567, _M0L6_2atmpS1568);
      _M0L6_2atmpS1566 = _M0Lm20match__tag__saver__3S130;
      _M0L6_2atmpS1565 = _M0L6_2atmpS1566 + 1;
      _M0L6_2atmpS1562 = (int64_t)_M0L6_2atmpS1565;
      _M0L6_2atmpS1564 = _M0Lm20match__tag__saver__4S131;
      _M0L6_2atmpS1563 = (int64_t)_M0L6_2atmpS1564;
      moonbit_incref(_M0L7_2adataS121);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS145
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1562, _M0L6_2atmpS1563);
      _M0L6_2atmpS1561 = _M0Lm20match__tag__saver__4S131;
      _M0L6_2atmpS1560 = _M0L6_2atmpS1561 + 1;
      _M0L6_2atmpS1557 = (int64_t)_M0L6_2atmpS1560;
      _M0L6_2atmpS1559 = _M0Lm10match__endS126;
      _M0L6_2atmpS1558 = (int64_t)_M0L6_2atmpS1559;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS146
      = _M0MPC16string6String4view(_M0L7_2adataS121, _M0L6_2atmpS1557, _M0L6_2atmpS1558);
      _block_4368
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4368)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_4368->$0_0 = _M0L3pkgS143.$0;
      _block_4368->$0_1 = _M0L3pkgS143.$1;
      _block_4368->$0_2 = _M0L3pkgS143.$2;
      _block_4368->$1_0 = _M0L8filenameS144.$0;
      _block_4368->$1_1 = _M0L8filenameS144.$1;
      _block_4368->$1_2 = _M0L8filenameS144.$2;
      _block_4368->$2_0 = _M0L11start__lineS141.$0;
      _block_4368->$2_1 = _M0L11start__lineS141.$1;
      _block_4368->$2_2 = _M0L11start__lineS141.$2;
      _block_4368->$3_0 = _M0L13start__columnS142.$0;
      _block_4368->$3_1 = _M0L13start__columnS142.$1;
      _block_4368->$3_2 = _M0L13start__columnS142.$2;
      _block_4368->$4_0 = _M0L9end__lineS145.$0;
      _block_4368->$4_1 = _M0L9end__lineS145.$1;
      _block_4368->$4_2 = _M0L9end__lineS145.$2;
      _block_4368->$5_0 = _M0L11end__columnS146.$0;
      _block_4368->$5_1 = _M0L11end__columnS146.$1;
      _block_4368->$5_2 = _M0L11end__columnS146.$2;
      return _block_4368;
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
  int32_t _if__result_4369;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS116 = _M0L4selfS117->$1;
  if (_M0L5indexS118 >= 0) {
    _if__result_4369 = _M0L5indexS118 < _M0L3lenS116;
  } else {
    _if__result_4369 = 0;
  }
  if (_if__result_4369) {
    moonbit_string_t* _M0L6_2atmpS1556;
    moonbit_string_t _M0L6_2atmpS4092;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1556 = _M0MPC15array5Array6bufferGsE(_M0L4selfS117);
    if (
      _M0L5indexS118 < 0
      || _M0L5indexS118 >= Moonbit_array_length(_M0L6_2atmpS1556)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4092 = (moonbit_string_t)_M0L6_2atmpS1556[_M0L5indexS118];
    moonbit_incref(_M0L6_2atmpS4092);
    moonbit_decref(_M0L6_2atmpS1556);
    return _M0L6_2atmpS4092;
  } else {
    moonbit_decref(_M0L4selfS117);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS113
) {
  moonbit_string_t* _M0L8_2afieldS4093;
  int32_t _M0L6_2acntS4208;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4093 = _M0L4selfS113->$0;
  _M0L6_2acntS4208 = Moonbit_object_header(_M0L4selfS113)->rc;
  if (_M0L6_2acntS4208 > 1) {
    int32_t _M0L11_2anew__cntS4209 = _M0L6_2acntS4208 - 1;
    Moonbit_object_header(_M0L4selfS113)->rc = _M0L11_2anew__cntS4209;
    moonbit_incref(_M0L8_2afieldS4093);
  } else if (_M0L6_2acntS4208 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS113);
  }
  return _M0L8_2afieldS4093;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS114
) {
  struct _M0TUsiE** _M0L8_2afieldS4094;
  int32_t _M0L6_2acntS4210;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4094 = _M0L4selfS114->$0;
  _M0L6_2acntS4210 = Moonbit_object_header(_M0L4selfS114)->rc;
  if (_M0L6_2acntS4210 > 1) {
    int32_t _M0L11_2anew__cntS4211 = _M0L6_2acntS4210 - 1;
    Moonbit_object_header(_M0L4selfS114)->rc = _M0L11_2anew__cntS4211;
    moonbit_incref(_M0L8_2afieldS4094);
  } else if (_M0L6_2acntS4210 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS114);
  }
  return _M0L8_2afieldS4094;
}

void** _M0MPC15array5Array6bufferGRPC14json10WriteFrameE(
  struct _M0TPB5ArrayGRPC14json10WriteFrameE* _M0L4selfS115
) {
  void** _M0L8_2afieldS4095;
  int32_t _M0L6_2acntS4212;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4095 = _M0L4selfS115->$0;
  _M0L6_2acntS4212 = Moonbit_object_header(_M0L4selfS115)->rc;
  if (_M0L6_2acntS4212 > 1) {
    int32_t _M0L11_2anew__cntS4213 = _M0L6_2acntS4212 - 1;
    Moonbit_object_header(_M0L4selfS115)->rc = _M0L11_2anew__cntS4213;
    moonbit_incref(_M0L8_2afieldS4095);
  } else if (_M0L6_2acntS4212 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS115);
  }
  return _M0L8_2afieldS4095;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS112) {
  struct _M0TPB13StringBuilder* _M0L3bufS111;
  struct _M0TPB6Logger _M0L6_2atmpS1555;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS111 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS111);
  _M0L6_2atmpS1555
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS111
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS112, _M0L6_2atmpS1555);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS111);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS110) {
  int32_t _M0L6_2atmpS1554;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1554 = (int32_t)_M0L4selfS110;
  return _M0L6_2atmpS1554;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS108,
  int32_t _M0L8trailingS109
) {
  int32_t _M0L6_2atmpS1553;
  int32_t _M0L6_2atmpS1552;
  int32_t _M0L6_2atmpS1551;
  int32_t _M0L6_2atmpS1550;
  int32_t _M0L6_2atmpS1549;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1553 = _M0L7leadingS108 - 55296;
  _M0L6_2atmpS1552 = _M0L6_2atmpS1553 * 1024;
  _M0L6_2atmpS1551 = _M0L6_2atmpS1552 + _M0L8trailingS109;
  _M0L6_2atmpS1550 = _M0L6_2atmpS1551 - 56320;
  _M0L6_2atmpS1549 = _M0L6_2atmpS1550 + 65536;
  return _M0L6_2atmpS1549;
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
  int32_t _M0L3lenS1544;
  int32_t _M0L6_2atmpS1543;
  moonbit_bytes_t _M0L8_2afieldS4096;
  moonbit_bytes_t _M0L4dataS1547;
  int32_t _M0L3lenS1548;
  int32_t _M0L3incS104;
  int32_t _M0L3lenS1546;
  int32_t _M0L6_2atmpS1545;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1544 = _M0L4selfS103->$1;
  _M0L6_2atmpS1543 = _M0L3lenS1544 + 4;
  moonbit_incref(_M0L4selfS103);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS103, _M0L6_2atmpS1543);
  _M0L8_2afieldS4096 = _M0L4selfS103->$0;
  _M0L4dataS1547 = _M0L8_2afieldS4096;
  _M0L3lenS1548 = _M0L4selfS103->$1;
  moonbit_incref(_M0L4dataS1547);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS104
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1547, _M0L3lenS1548, _M0L2chS105);
  _M0L3lenS1546 = _M0L4selfS103->$1;
  _M0L6_2atmpS1545 = _M0L3lenS1546 + _M0L3incS104;
  _M0L4selfS103->$1 = _M0L6_2atmpS1545;
  moonbit_decref(_M0L4selfS103);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS98,
  int32_t _M0L8requiredS99
) {
  moonbit_bytes_t _M0L8_2afieldS4100;
  moonbit_bytes_t _M0L4dataS1542;
  int32_t _M0L6_2atmpS4099;
  int32_t _M0L12current__lenS97;
  int32_t _M0Lm13enough__spaceS100;
  int32_t _M0L6_2atmpS1540;
  int32_t _M0L6_2atmpS1541;
  moonbit_bytes_t _M0L9new__dataS102;
  moonbit_bytes_t _M0L8_2afieldS4098;
  moonbit_bytes_t _M0L4dataS1538;
  int32_t _M0L3lenS1539;
  moonbit_bytes_t _M0L6_2aoldS4097;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4100 = _M0L4selfS98->$0;
  _M0L4dataS1542 = _M0L8_2afieldS4100;
  _M0L6_2atmpS4099 = Moonbit_array_length(_M0L4dataS1542);
  _M0L12current__lenS97 = _M0L6_2atmpS4099;
  if (_M0L8requiredS99 <= _M0L12current__lenS97) {
    moonbit_decref(_M0L4selfS98);
    return 0;
  }
  _M0Lm13enough__spaceS100 = _M0L12current__lenS97;
  while (1) {
    int32_t _M0L6_2atmpS1536 = _M0Lm13enough__spaceS100;
    if (_M0L6_2atmpS1536 < _M0L8requiredS99) {
      int32_t _M0L6_2atmpS1537 = _M0Lm13enough__spaceS100;
      _M0Lm13enough__spaceS100 = _M0L6_2atmpS1537 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1540 = _M0Lm13enough__spaceS100;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1541 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS102
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1540, _M0L6_2atmpS1541);
  _M0L8_2afieldS4098 = _M0L4selfS98->$0;
  _M0L4dataS1538 = _M0L8_2afieldS4098;
  _M0L3lenS1539 = _M0L4selfS98->$1;
  moonbit_incref(_M0L4dataS1538);
  moonbit_incref(_M0L9new__dataS102);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS102, 0, _M0L4dataS1538, 0, _M0L3lenS1539);
  _M0L6_2aoldS4097 = _M0L4selfS98->$0;
  moonbit_decref(_M0L6_2aoldS4097);
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
    uint32_t _M0L6_2atmpS1519 = _M0L4codeS90 & 255u;
    int32_t _M0L6_2atmpS1518;
    int32_t _M0L6_2atmpS1520;
    uint32_t _M0L6_2atmpS1522;
    int32_t _M0L6_2atmpS1521;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1518 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1519);
    if (
      _M0L6offsetS93 < 0
      || _M0L6offsetS93 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6offsetS93] = _M0L6_2atmpS1518;
    _M0L6_2atmpS1520 = _M0L6offsetS93 + 1;
    _M0L6_2atmpS1522 = _M0L4codeS90 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1521 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1522);
    if (
      _M0L6_2atmpS1520 < 0
      || _M0L6_2atmpS1520 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1520] = _M0L6_2atmpS1521;
    moonbit_decref(_M0L4selfS92);
    return 2;
  } else if (_M0L4codeS90 < 1114112u) {
    uint32_t _M0L2hiS94 = _M0L4codeS90 - 65536u;
    uint32_t _M0L6_2atmpS1535 = _M0L2hiS94 >> 10;
    uint32_t _M0L2loS95 = _M0L6_2atmpS1535 | 55296u;
    uint32_t _M0L6_2atmpS1534 = _M0L2hiS94 & 1023u;
    uint32_t _M0L2hiS96 = _M0L6_2atmpS1534 | 56320u;
    uint32_t _M0L6_2atmpS1524 = _M0L2loS95 & 255u;
    int32_t _M0L6_2atmpS1523;
    int32_t _M0L6_2atmpS1525;
    uint32_t _M0L6_2atmpS1527;
    int32_t _M0L6_2atmpS1526;
    int32_t _M0L6_2atmpS1528;
    uint32_t _M0L6_2atmpS1530;
    int32_t _M0L6_2atmpS1529;
    int32_t _M0L6_2atmpS1531;
    uint32_t _M0L6_2atmpS1533;
    int32_t _M0L6_2atmpS1532;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1523 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1524);
    if (
      _M0L6offsetS93 < 0
      || _M0L6offsetS93 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6offsetS93] = _M0L6_2atmpS1523;
    _M0L6_2atmpS1525 = _M0L6offsetS93 + 1;
    _M0L6_2atmpS1527 = _M0L2loS95 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1526 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1527);
    if (
      _M0L6_2atmpS1525 < 0
      || _M0L6_2atmpS1525 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1525] = _M0L6_2atmpS1526;
    _M0L6_2atmpS1528 = _M0L6offsetS93 + 2;
    _M0L6_2atmpS1530 = _M0L2hiS96 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1529 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1530);
    if (
      _M0L6_2atmpS1528 < 0
      || _M0L6_2atmpS1528 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1528] = _M0L6_2atmpS1529;
    _M0L6_2atmpS1531 = _M0L6offsetS93 + 3;
    _M0L6_2atmpS1533 = _M0L2hiS96 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1532 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1533);
    if (
      _M0L6_2atmpS1531 < 0
      || _M0L6_2atmpS1531 >= Moonbit_array_length(_M0L4selfS92)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS92[_M0L6_2atmpS1531] = _M0L6_2atmpS1532;
    moonbit_decref(_M0L4selfS92);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS92);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_67.data, (moonbit_string_t)moonbit_string_literal_68.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS89) {
  int32_t _M0L6_2atmpS1517;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1517 = *(int32_t*)&_M0L4selfS89;
  return _M0L6_2atmpS1517 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS88) {
  int32_t _M0L6_2atmpS1516;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1516 = _M0L4selfS88;
  return *(uint32_t*)&_M0L6_2atmpS1516;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS87
) {
  moonbit_bytes_t _M0L8_2afieldS4102;
  moonbit_bytes_t _M0L4dataS1515;
  moonbit_bytes_t _M0L6_2atmpS1512;
  int32_t _M0L8_2afieldS4101;
  int32_t _M0L3lenS1514;
  int64_t _M0L6_2atmpS1513;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4102 = _M0L4selfS87->$0;
  _M0L4dataS1515 = _M0L8_2afieldS4102;
  moonbit_incref(_M0L4dataS1515);
  _M0L6_2atmpS1512 = _M0L4dataS1515;
  _M0L8_2afieldS4101 = _M0L4selfS87->$1;
  moonbit_decref(_M0L4selfS87);
  _M0L3lenS1514 = _M0L8_2afieldS4101;
  _M0L6_2atmpS1513 = (int64_t)_M0L3lenS1514;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1512, 0, _M0L6_2atmpS1513);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS82,
  int32_t _M0L6offsetS86,
  int64_t _M0L6lengthS84
) {
  int32_t _M0L3lenS81;
  int32_t _M0L6lengthS83;
  int32_t _if__result_4371;
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
      int32_t _M0L6_2atmpS1511 = _M0L6offsetS86 + _M0L6lengthS83;
      _if__result_4371 = _M0L6_2atmpS1511 <= _M0L3lenS81;
    } else {
      _if__result_4371 = 0;
    }
  } else {
    _if__result_4371 = 0;
  }
  if (_if__result_4371) {
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
  struct _M0TPB13StringBuilder* _block_4372;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS79 < 1) {
    _M0L7initialS78 = 1;
  } else {
    _M0L7initialS78 = _M0L10size__hintS79;
  }
  _M0L4dataS80 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS78, 0);
  _block_4372
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4372)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4372->$0 = _M0L4dataS80;
  _block_4372->$1 = 0;
  return _block_4372;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS77) {
  int32_t _M0L6_2atmpS1510;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1510 = (int32_t)_M0L4selfS77;
  return _M0L6_2atmpS1510;
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
  int32_t _if__result_4373;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS26 == _M0L3srcS27) {
    _if__result_4373 = _M0L11dst__offsetS28 < _M0L11src__offsetS29;
  } else {
    _if__result_4373 = 0;
  }
  if (_if__result_4373) {
    int32_t _M0L1iS30 = 0;
    while (1) {
      if (_M0L1iS30 < _M0L3lenS31) {
        int32_t _M0L6_2atmpS1474 = _M0L11dst__offsetS28 + _M0L1iS30;
        int32_t _M0L6_2atmpS1476 = _M0L11src__offsetS29 + _M0L1iS30;
        int32_t _M0L6_2atmpS1475;
        int32_t _M0L6_2atmpS1477;
        if (
          _M0L6_2atmpS1476 < 0
          || _M0L6_2atmpS1476 >= Moonbit_array_length(_M0L3srcS27)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1475 = (int32_t)_M0L3srcS27[_M0L6_2atmpS1476];
        if (
          _M0L6_2atmpS1474 < 0
          || _M0L6_2atmpS1474 >= Moonbit_array_length(_M0L3dstS26)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS26[_M0L6_2atmpS1474] = _M0L6_2atmpS1475;
        _M0L6_2atmpS1477 = _M0L1iS30 + 1;
        _M0L1iS30 = _M0L6_2atmpS1477;
        continue;
      } else {
        moonbit_decref(_M0L3srcS27);
        moonbit_decref(_M0L3dstS26);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1482 = _M0L3lenS31 - 1;
    int32_t _M0L1iS33 = _M0L6_2atmpS1482;
    while (1) {
      if (_M0L1iS33 >= 0) {
        int32_t _M0L6_2atmpS1478 = _M0L11dst__offsetS28 + _M0L1iS33;
        int32_t _M0L6_2atmpS1480 = _M0L11src__offsetS29 + _M0L1iS33;
        int32_t _M0L6_2atmpS1479;
        int32_t _M0L6_2atmpS1481;
        if (
          _M0L6_2atmpS1480 < 0
          || _M0L6_2atmpS1480 >= Moonbit_array_length(_M0L3srcS27)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1479 = (int32_t)_M0L3srcS27[_M0L6_2atmpS1480];
        if (
          _M0L6_2atmpS1478 < 0
          || _M0L6_2atmpS1478 >= Moonbit_array_length(_M0L3dstS26)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS26[_M0L6_2atmpS1478] = _M0L6_2atmpS1479;
        _M0L6_2atmpS1481 = _M0L1iS33 - 1;
        _M0L1iS33 = _M0L6_2atmpS1481;
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
  int32_t _if__result_4376;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS35 == _M0L3srcS36) {
    _if__result_4376 = _M0L11dst__offsetS37 < _M0L11src__offsetS38;
  } else {
    _if__result_4376 = 0;
  }
  if (_if__result_4376) {
    int32_t _M0L1iS39 = 0;
    while (1) {
      if (_M0L1iS39 < _M0L3lenS40) {
        int32_t _M0L6_2atmpS1483 = _M0L11dst__offsetS37 + _M0L1iS39;
        int32_t _M0L6_2atmpS1485 = _M0L11src__offsetS38 + _M0L1iS39;
        moonbit_string_t _M0L6_2atmpS4104;
        moonbit_string_t _M0L6_2atmpS1484;
        moonbit_string_t _M0L6_2aoldS4103;
        int32_t _M0L6_2atmpS1486;
        if (
          _M0L6_2atmpS1485 < 0
          || _M0L6_2atmpS1485 >= Moonbit_array_length(_M0L3srcS36)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4104 = (moonbit_string_t)_M0L3srcS36[_M0L6_2atmpS1485];
        _M0L6_2atmpS1484 = _M0L6_2atmpS4104;
        if (
          _M0L6_2atmpS1483 < 0
          || _M0L6_2atmpS1483 >= Moonbit_array_length(_M0L3dstS35)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4103 = (moonbit_string_t)_M0L3dstS35[_M0L6_2atmpS1483];
        moonbit_incref(_M0L6_2atmpS1484);
        moonbit_decref(_M0L6_2aoldS4103);
        _M0L3dstS35[_M0L6_2atmpS1483] = _M0L6_2atmpS1484;
        _M0L6_2atmpS1486 = _M0L1iS39 + 1;
        _M0L1iS39 = _M0L6_2atmpS1486;
        continue;
      } else {
        moonbit_decref(_M0L3srcS36);
        moonbit_decref(_M0L3dstS35);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1491 = _M0L3lenS40 - 1;
    int32_t _M0L1iS42 = _M0L6_2atmpS1491;
    while (1) {
      if (_M0L1iS42 >= 0) {
        int32_t _M0L6_2atmpS1487 = _M0L11dst__offsetS37 + _M0L1iS42;
        int32_t _M0L6_2atmpS1489 = _M0L11src__offsetS38 + _M0L1iS42;
        moonbit_string_t _M0L6_2atmpS4106;
        moonbit_string_t _M0L6_2atmpS1488;
        moonbit_string_t _M0L6_2aoldS4105;
        int32_t _M0L6_2atmpS1490;
        if (
          _M0L6_2atmpS1489 < 0
          || _M0L6_2atmpS1489 >= Moonbit_array_length(_M0L3srcS36)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4106 = (moonbit_string_t)_M0L3srcS36[_M0L6_2atmpS1489];
        _M0L6_2atmpS1488 = _M0L6_2atmpS4106;
        if (
          _M0L6_2atmpS1487 < 0
          || _M0L6_2atmpS1487 >= Moonbit_array_length(_M0L3dstS35)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4105 = (moonbit_string_t)_M0L3dstS35[_M0L6_2atmpS1487];
        moonbit_incref(_M0L6_2atmpS1488);
        moonbit_decref(_M0L6_2aoldS4105);
        _M0L3dstS35[_M0L6_2atmpS1487] = _M0L6_2atmpS1488;
        _M0L6_2atmpS1490 = _M0L1iS42 - 1;
        _M0L1iS42 = _M0L6_2atmpS1490;
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
  int32_t _if__result_4379;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS44 == _M0L3srcS45) {
    _if__result_4379 = _M0L11dst__offsetS46 < _M0L11src__offsetS47;
  } else {
    _if__result_4379 = 0;
  }
  if (_if__result_4379) {
    int32_t _M0L1iS48 = 0;
    while (1) {
      if (_M0L1iS48 < _M0L3lenS49) {
        int32_t _M0L6_2atmpS1492 = _M0L11dst__offsetS46 + _M0L1iS48;
        int32_t _M0L6_2atmpS1494 = _M0L11src__offsetS47 + _M0L1iS48;
        struct _M0TUsiE* _M0L6_2atmpS4108;
        struct _M0TUsiE* _M0L6_2atmpS1493;
        struct _M0TUsiE* _M0L6_2aoldS4107;
        int32_t _M0L6_2atmpS1495;
        if (
          _M0L6_2atmpS1494 < 0
          || _M0L6_2atmpS1494 >= Moonbit_array_length(_M0L3srcS45)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4108 = (struct _M0TUsiE*)_M0L3srcS45[_M0L6_2atmpS1494];
        _M0L6_2atmpS1493 = _M0L6_2atmpS4108;
        if (
          _M0L6_2atmpS1492 < 0
          || _M0L6_2atmpS1492 >= Moonbit_array_length(_M0L3dstS44)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4107 = (struct _M0TUsiE*)_M0L3dstS44[_M0L6_2atmpS1492];
        if (_M0L6_2atmpS1493) {
          moonbit_incref(_M0L6_2atmpS1493);
        }
        if (_M0L6_2aoldS4107) {
          moonbit_decref(_M0L6_2aoldS4107);
        }
        _M0L3dstS44[_M0L6_2atmpS1492] = _M0L6_2atmpS1493;
        _M0L6_2atmpS1495 = _M0L1iS48 + 1;
        _M0L1iS48 = _M0L6_2atmpS1495;
        continue;
      } else {
        moonbit_decref(_M0L3srcS45);
        moonbit_decref(_M0L3dstS44);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1500 = _M0L3lenS49 - 1;
    int32_t _M0L1iS51 = _M0L6_2atmpS1500;
    while (1) {
      if (_M0L1iS51 >= 0) {
        int32_t _M0L6_2atmpS1496 = _M0L11dst__offsetS46 + _M0L1iS51;
        int32_t _M0L6_2atmpS1498 = _M0L11src__offsetS47 + _M0L1iS51;
        struct _M0TUsiE* _M0L6_2atmpS4110;
        struct _M0TUsiE* _M0L6_2atmpS1497;
        struct _M0TUsiE* _M0L6_2aoldS4109;
        int32_t _M0L6_2atmpS1499;
        if (
          _M0L6_2atmpS1498 < 0
          || _M0L6_2atmpS1498 >= Moonbit_array_length(_M0L3srcS45)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4110 = (struct _M0TUsiE*)_M0L3srcS45[_M0L6_2atmpS1498];
        _M0L6_2atmpS1497 = _M0L6_2atmpS4110;
        if (
          _M0L6_2atmpS1496 < 0
          || _M0L6_2atmpS1496 >= Moonbit_array_length(_M0L3dstS44)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4109 = (struct _M0TUsiE*)_M0L3dstS44[_M0L6_2atmpS1496];
        if (_M0L6_2atmpS1497) {
          moonbit_incref(_M0L6_2atmpS1497);
        }
        if (_M0L6_2aoldS4109) {
          moonbit_decref(_M0L6_2aoldS4109);
        }
        _M0L3dstS44[_M0L6_2atmpS1496] = _M0L6_2atmpS1497;
        _M0L6_2atmpS1499 = _M0L1iS51 - 1;
        _M0L1iS51 = _M0L6_2atmpS1499;
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
  int32_t _if__result_4382;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS53 == _M0L3srcS54) {
    _if__result_4382 = _M0L11dst__offsetS55 < _M0L11src__offsetS56;
  } else {
    _if__result_4382 = 0;
  }
  if (_if__result_4382) {
    int32_t _M0L1iS57 = 0;
    while (1) {
      if (_M0L1iS57 < _M0L3lenS58) {
        int32_t _M0L6_2atmpS1501 = _M0L11dst__offsetS55 + _M0L1iS57;
        int32_t _M0L6_2atmpS1503 = _M0L11src__offsetS56 + _M0L1iS57;
        void* _M0L6_2atmpS4112;
        void* _M0L6_2atmpS1502;
        void* _M0L6_2aoldS4111;
        int32_t _M0L6_2atmpS1504;
        if (
          _M0L6_2atmpS1503 < 0
          || _M0L6_2atmpS1503 >= Moonbit_array_length(_M0L3srcS54)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4112 = (void*)_M0L3srcS54[_M0L6_2atmpS1503];
        _M0L6_2atmpS1502 = _M0L6_2atmpS4112;
        if (
          _M0L6_2atmpS1501 < 0
          || _M0L6_2atmpS1501 >= Moonbit_array_length(_M0L3dstS53)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4111 = (void*)_M0L3dstS53[_M0L6_2atmpS1501];
        moonbit_incref(_M0L6_2atmpS1502);
        moonbit_decref(_M0L6_2aoldS4111);
        _M0L3dstS53[_M0L6_2atmpS1501] = _M0L6_2atmpS1502;
        _M0L6_2atmpS1504 = _M0L1iS57 + 1;
        _M0L1iS57 = _M0L6_2atmpS1504;
        continue;
      } else {
        moonbit_decref(_M0L3srcS54);
        moonbit_decref(_M0L3dstS53);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1509 = _M0L3lenS58 - 1;
    int32_t _M0L1iS60 = _M0L6_2atmpS1509;
    while (1) {
      if (_M0L1iS60 >= 0) {
        int32_t _M0L6_2atmpS1505 = _M0L11dst__offsetS55 + _M0L1iS60;
        int32_t _M0L6_2atmpS1507 = _M0L11src__offsetS56 + _M0L1iS60;
        void* _M0L6_2atmpS4114;
        void* _M0L6_2atmpS1506;
        void* _M0L6_2aoldS4113;
        int32_t _M0L6_2atmpS1508;
        if (
          _M0L6_2atmpS1507 < 0
          || _M0L6_2atmpS1507 >= Moonbit_array_length(_M0L3srcS54)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4114 = (void*)_M0L3srcS54[_M0L6_2atmpS1507];
        _M0L6_2atmpS1506 = _M0L6_2atmpS4114;
        if (
          _M0L6_2atmpS1505 < 0
          || _M0L6_2atmpS1505 >= Moonbit_array_length(_M0L3dstS53)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4113 = (void*)_M0L3dstS53[_M0L6_2atmpS1505];
        moonbit_incref(_M0L6_2atmpS1506);
        moonbit_decref(_M0L6_2aoldS4113);
        _M0L3dstS53[_M0L6_2atmpS1505] = _M0L6_2atmpS1506;
        _M0L6_2atmpS1508 = _M0L1iS60 - 1;
        _M0L1iS60 = _M0L6_2atmpS1508;
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
  moonbit_string_t _M0L6_2atmpS1458;
  moonbit_string_t _M0L6_2atmpS4117;
  moonbit_string_t _M0L6_2atmpS1456;
  moonbit_string_t _M0L6_2atmpS1457;
  moonbit_string_t _M0L6_2atmpS4116;
  moonbit_string_t _M0L6_2atmpS1455;
  moonbit_string_t _M0L6_2atmpS4115;
  moonbit_string_t _M0L6_2atmpS1454;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1458 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4117
  = moonbit_add_string(_M0L6_2atmpS1458, (moonbit_string_t)moonbit_string_literal_69.data);
  moonbit_decref(_M0L6_2atmpS1458);
  _M0L6_2atmpS1456 = _M0L6_2atmpS4117;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1457
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4116 = moonbit_add_string(_M0L6_2atmpS1456, _M0L6_2atmpS1457);
  moonbit_decref(_M0L6_2atmpS1456);
  moonbit_decref(_M0L6_2atmpS1457);
  _M0L6_2atmpS1455 = _M0L6_2atmpS4116;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4115
  = moonbit_add_string(_M0L6_2atmpS1455, (moonbit_string_t)moonbit_string_literal_23.data);
  moonbit_decref(_M0L6_2atmpS1455);
  _M0L6_2atmpS1454 = _M0L6_2atmpS4115;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1454);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS1463;
  moonbit_string_t _M0L6_2atmpS4120;
  moonbit_string_t _M0L6_2atmpS1461;
  moonbit_string_t _M0L6_2atmpS1462;
  moonbit_string_t _M0L6_2atmpS4119;
  moonbit_string_t _M0L6_2atmpS1460;
  moonbit_string_t _M0L6_2atmpS4118;
  moonbit_string_t _M0L6_2atmpS1459;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1463 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4120
  = moonbit_add_string(_M0L6_2atmpS1463, (moonbit_string_t)moonbit_string_literal_69.data);
  moonbit_decref(_M0L6_2atmpS1463);
  _M0L6_2atmpS1461 = _M0L6_2atmpS4120;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1462
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4119 = moonbit_add_string(_M0L6_2atmpS1461, _M0L6_2atmpS1462);
  moonbit_decref(_M0L6_2atmpS1461);
  moonbit_decref(_M0L6_2atmpS1462);
  _M0L6_2atmpS1460 = _M0L6_2atmpS4119;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4118
  = moonbit_add_string(_M0L6_2atmpS1460, (moonbit_string_t)moonbit_string_literal_23.data);
  moonbit_decref(_M0L6_2atmpS1460);
  _M0L6_2atmpS1459 = _M0L6_2atmpS4118;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1459);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS22,
  moonbit_string_t _M0L3locS23
) {
  moonbit_string_t _M0L6_2atmpS1468;
  moonbit_string_t _M0L6_2atmpS4123;
  moonbit_string_t _M0L6_2atmpS1466;
  moonbit_string_t _M0L6_2atmpS1467;
  moonbit_string_t _M0L6_2atmpS4122;
  moonbit_string_t _M0L6_2atmpS1465;
  moonbit_string_t _M0L6_2atmpS4121;
  moonbit_string_t _M0L6_2atmpS1464;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1468 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4123
  = moonbit_add_string(_M0L6_2atmpS1468, (moonbit_string_t)moonbit_string_literal_69.data);
  moonbit_decref(_M0L6_2atmpS1468);
  _M0L6_2atmpS1466 = _M0L6_2atmpS4123;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1467
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4122 = moonbit_add_string(_M0L6_2atmpS1466, _M0L6_2atmpS1467);
  moonbit_decref(_M0L6_2atmpS1466);
  moonbit_decref(_M0L6_2atmpS1467);
  _M0L6_2atmpS1465 = _M0L6_2atmpS4122;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4121
  = moonbit_add_string(_M0L6_2atmpS1465, (moonbit_string_t)moonbit_string_literal_23.data);
  moonbit_decref(_M0L6_2atmpS1465);
  _M0L6_2atmpS1464 = _M0L6_2atmpS4121;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1464);
}

struct _M0TPC15bytes9BytesView _M0FPB5abortGRPC15bytes9BytesViewE(
  moonbit_string_t _M0L6stringS24,
  moonbit_string_t _M0L3locS25
) {
  moonbit_string_t _M0L6_2atmpS1473;
  moonbit_string_t _M0L6_2atmpS4126;
  moonbit_string_t _M0L6_2atmpS1471;
  moonbit_string_t _M0L6_2atmpS1472;
  moonbit_string_t _M0L6_2atmpS4125;
  moonbit_string_t _M0L6_2atmpS1470;
  moonbit_string_t _M0L6_2atmpS4124;
  moonbit_string_t _M0L6_2atmpS1469;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1473 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4126
  = moonbit_add_string(_M0L6_2atmpS1473, (moonbit_string_t)moonbit_string_literal_69.data);
  moonbit_decref(_M0L6_2atmpS1473);
  _M0L6_2atmpS1471 = _M0L6_2atmpS4126;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1472
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4125 = moonbit_add_string(_M0L6_2atmpS1471, _M0L6_2atmpS1472);
  moonbit_decref(_M0L6_2atmpS1471);
  moonbit_decref(_M0L6_2atmpS1472);
  _M0L6_2atmpS1470 = _M0L6_2atmpS4125;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4124
  = moonbit_add_string(_M0L6_2atmpS1470, (moonbit_string_t)moonbit_string_literal_23.data);
  moonbit_decref(_M0L6_2atmpS1470);
  _M0L6_2atmpS1469 = _M0L6_2atmpS4124;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC15bytes9BytesViewE(_M0L6_2atmpS1469);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS16,
  uint32_t _M0L5valueS17
) {
  uint32_t _M0L3accS1453;
  uint32_t _M0L6_2atmpS1452;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1453 = _M0L4selfS16->$0;
  _M0L6_2atmpS1452 = _M0L3accS1453 + 4u;
  _M0L4selfS16->$0 = _M0L6_2atmpS1452;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS16, _M0L5valueS17);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5inputS15
) {
  uint32_t _M0L3accS1450;
  uint32_t _M0L6_2atmpS1451;
  uint32_t _M0L6_2atmpS1449;
  uint32_t _M0L6_2atmpS1448;
  uint32_t _M0L6_2atmpS1447;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1450 = _M0L4selfS14->$0;
  _M0L6_2atmpS1451 = _M0L5inputS15 * 3266489917u;
  _M0L6_2atmpS1449 = _M0L3accS1450 + _M0L6_2atmpS1451;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1448 = _M0FPB4rotl(_M0L6_2atmpS1449, 17);
  _M0L6_2atmpS1447 = _M0L6_2atmpS1448 * 668265263u;
  _M0L4selfS14->$0 = _M0L6_2atmpS1447;
  moonbit_decref(_M0L4selfS14);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS12, int32_t _M0L1rS13) {
  uint32_t _M0L6_2atmpS1444;
  int32_t _M0L6_2atmpS1446;
  uint32_t _M0L6_2atmpS1445;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1444 = _M0L1xS12 << (_M0L1rS13 & 31);
  _M0L6_2atmpS1446 = 32 - _M0L1rS13;
  _M0L6_2atmpS1445 = _M0L1xS12 >> (_M0L6_2atmpS1446 & 31);
  return _M0L6_2atmpS1444 | _M0L6_2atmpS1445;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S8,
  struct _M0TPB6Logger _M0L10_2ax__4934S11
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS9;
  moonbit_string_t _M0L8_2afieldS4127;
  int32_t _M0L6_2acntS4214;
  moonbit_string_t _M0L15_2a_2aarg__4935S10;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS9
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S8;
  _M0L8_2afieldS4127 = _M0L10_2aFailureS9->$0;
  _M0L6_2acntS4214 = Moonbit_object_header(_M0L10_2aFailureS9)->rc;
  if (_M0L6_2acntS4214 > 1) {
    int32_t _M0L11_2anew__cntS4215 = _M0L6_2acntS4214 - 1;
    Moonbit_object_header(_M0L10_2aFailureS9)->rc = _M0L11_2anew__cntS4215;
    moonbit_incref(_M0L8_2afieldS4127);
  } else if (_M0L6_2acntS4214 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS9);
  }
  _M0L15_2a_2aarg__4935S10 = _M0L8_2afieldS4127;
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_70.data);
  if (_M0L10_2ax__4934S11.$1) {
    moonbit_incref(_M0L10_2ax__4934S11.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S11, _M0L15_2a_2aarg__4935S10);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S11.$0->$method_0(_M0L10_2ax__4934S11.$1, (moonbit_string_t)moonbit_string_literal_71.data);
  return 0;
}

void* _M0IPC16string6StringPB6ToJson8to__json(moonbit_string_t _M0L4selfS7) {
  void* _block_4385;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\json.mbt"
  _block_4385 = (void*)moonbit_malloc(sizeof(struct _M0DTPB4Json6String));
  Moonbit_object_header(_block_4385)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPB4Json6String, $0) >> 2, 1, 4);
  ((struct _M0DTPB4Json6String*)_block_4385)->$0 = _M0L4selfS7;
  return _block_4385;
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

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
  moonbit_string_t _M0L3msgS4
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS4);
  moonbit_decref(_M0L3msgS4);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1399) {
  switch (Moonbit_object_tag(_M0L4_2aeS1399)) {
    case 5: {
      moonbit_decref(_M0L4_2aeS1399);
      return (moonbit_string_t)moonbit_string_literal_72.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1399);
      return (moonbit_string_t)moonbit_string_literal_73.data;
      break;
    }
    
    case 1: {
      return _M0IP016_24default__implPB4Show10to__stringGRP48clawteam8clawteam8internal5errno5ErrnoE(_M0L4_2aeS1399);
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1399);
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1399);
      return (moonbit_string_t)moonbit_string_literal_74.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1399);
      return (moonbit_string_t)moonbit_string_literal_75.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1425,
  int32_t _M0L8_2aparamS1424
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1423 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1425;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1423, _M0L8_2aparamS1424);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1422,
  struct _M0TPC16string10StringView _M0L8_2aparamS1421
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1420 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1422;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1420, _M0L8_2aparamS1421);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1419,
  moonbit_string_t _M0L8_2aparamS1416,
  int32_t _M0L8_2aparamS1417,
  int32_t _M0L8_2aparamS1418
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1415 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1419;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1415, _M0L8_2aparamS1416, _M0L8_2aparamS1417, _M0L8_2aparamS1418);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1414,
  moonbit_string_t _M0L8_2aparamS1413
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1412 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1414;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1412, _M0L8_2aparamS1413);
  return 0;
}

void* _M0IPC14bool4BoolPB6ToJson64to__json_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eToJson(
  void* _M0L11_2aobj__ptrS1410
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1411 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS1410;
  int32_t _M0L8_2afieldS4128 = _M0L14_2aboxed__selfS1411->$0;
  int32_t _M0L7_2aselfS1409;
  moonbit_decref(_M0L14_2aboxed__selfS1411);
  _M0L7_2aselfS1409 = _M0L8_2afieldS4128;
  return _M0IPC14bool4BoolPB6ToJson8to__json(_M0L7_2aselfS1409);
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1443 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1442;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1441;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1325;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1440;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1439;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1438;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1433;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1326;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1437;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1436;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1435;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1434;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1324;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1432;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1431;
  _M0L6_2atmpS1443[0] = (moonbit_string_t)moonbit_string_literal_76.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal19tty__blackbox__test43____test__7474795f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1442
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1442)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1442->$0
  = _M0FP48clawteam8clawteam8internal19tty__blackbox__test43____test__7474795f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1442->$1 = _M0L6_2atmpS1443;
  _M0L8_2atupleS1441
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1441)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1441->$0 = 0;
  _M0L8_2atupleS1441->$1 = _M0L8_2atupleS1442;
  _M0L7_2abindS1325
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1325[0] = _M0L8_2atupleS1441;
  _M0L6_2atmpS1440 = _M0L7_2abindS1325;
  _M0L6_2atmpS1439
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1440
  };
  #line 398 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1438
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1439);
  _M0L8_2atupleS1433
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1433)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1433->$0 = (moonbit_string_t)moonbit_string_literal_77.data;
  _M0L8_2atupleS1433->$1 = _M0L6_2atmpS1438;
  _M0L7_2abindS1326
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1437 = _M0L7_2abindS1326;
  _M0L6_2atmpS1436
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1437
  };
  #line 401 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1435
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1436);
  _M0L8_2atupleS1434
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1434)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1434->$0 = (moonbit_string_t)moonbit_string_literal_78.data;
  _M0L8_2atupleS1434->$1 = _M0L6_2atmpS1435;
  _M0L7_2abindS1324
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(2);
  _M0L7_2abindS1324[0] = _M0L8_2atupleS1433;
  _M0L7_2abindS1324[1] = _M0L8_2atupleS1434;
  _M0L6_2atmpS1432 = _M0L7_2abindS1324;
  _M0L6_2atmpS1431
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 2, _M0L6_2atmpS1432
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0FP48clawteam8clawteam8internal19tty__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1431);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1430;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1393;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1394;
  int32_t _M0L7_2abindS1395;
  int32_t _M0L2__S1396;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1430
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1393
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1393)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1393->$0 = _M0L6_2atmpS1430;
  _M0L12async__testsS1393->$1 = 0;
  #line 440 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1394
  = _M0FP48clawteam8clawteam8internal19tty__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1395 = _M0L7_2abindS1394->$1;
  _M0L2__S1396 = 0;
  while (1) {
    if (_M0L2__S1396 < _M0L7_2abindS1395) {
      struct _M0TUsiE** _M0L8_2afieldS4132 = _M0L7_2abindS1394->$0;
      struct _M0TUsiE** _M0L3bufS1429 = _M0L8_2afieldS4132;
      struct _M0TUsiE* _M0L6_2atmpS4131 =
        (struct _M0TUsiE*)_M0L3bufS1429[_M0L2__S1396];
      struct _M0TUsiE* _M0L3argS1397 = _M0L6_2atmpS4131;
      moonbit_string_t _M0L8_2afieldS4130 = _M0L3argS1397->$0;
      moonbit_string_t _M0L6_2atmpS1426 = _M0L8_2afieldS4130;
      int32_t _M0L8_2afieldS4129 = _M0L3argS1397->$1;
      int32_t _M0L6_2atmpS1427 = _M0L8_2afieldS4129;
      int32_t _M0L6_2atmpS1428;
      moonbit_incref(_M0L6_2atmpS1426);
      moonbit_incref(_M0L12async__testsS1393);
      #line 441 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
      _M0FP48clawteam8clawteam8internal19tty__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1393, _M0L6_2atmpS1426, _M0L6_2atmpS1427);
      _M0L6_2atmpS1428 = _M0L2__S1396 + 1;
      _M0L2__S1396 = _M0L6_2atmpS1428;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1394);
    }
    break;
  }
  #line 443 "E:\\moonbit\\clawteam\\internal\\tty\\__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal19tty__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal19tty__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1393);
  return 0;
}