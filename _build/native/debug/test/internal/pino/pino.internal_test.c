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
struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1170__l570__;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TUsRPB6LoggerE;

struct _M0TPB13StringBuilder;

struct _M0DTPC15error5Error107clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0R110_24clawteam_2fclawteam_2finternal_2fpino_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c687;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB6Logger;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal4pino33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal4pino33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1543__l429__;

struct _M0TPB13SourceLocRepr;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0TWRPC15error5ErrorEu;

struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

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

struct _M0DTPC16result6ResultGuRPB7FailureE3Err;

struct _M0TWEu;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16result6ResultGuRPB7FailureE2Ok;

struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1539__l430__;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1170__l570__ {
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

struct _M0DTPC15error5Error107clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  void* $1;
  
};

struct _M0R110_24clawteam_2fclawteam_2finternal_2fpino_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c687 {
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal4pino33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam8internal4pino33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
};

struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1543__l429__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
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

struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
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

struct _M0DTPC16result6ResultGuRPB7FailureE3Err {
  void* $0;
  
};

struct _M0TWEu {
  int32_t(* code)(struct _M0TWEu*);
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGuRPB7FailureE2Ok {
  int32_t $0;
  
};

struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1539__l430__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
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

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal4pino41____test__6c6576656c2e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__executeN17error__to__stringS696(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__executeN14handle__resultS687(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal4pino41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam8internal4pino41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testC1543l429(
  struct _M0TWEu*
);

int32_t _M0IP48clawteam8clawteam8internal4pino41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testC1539l430(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam8internal4pino45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS619(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS614(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS607(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S601(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal4pino28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal4pino34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal4pino31____test__6c6576656c2e6d6274__0(
  
);

int32_t _M0IP48clawteam8clawteam8internal4pino5LevelPB7Compare7compare(
  int32_t,
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

struct moonbit_result_0 _M0FPB12assert__true(
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

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1170l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

int32_t _M0IPC14bool4BoolPB4Show6output(int32_t, struct _M0TPB6Logger);

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

struct moonbit_result_0 _M0FPB4failGuE(moonbit_string_t, moonbit_string_t);

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

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGbE(int32_t);

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

int32_t _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal4pino5LevelE(
  int32_t,
  int32_t
);

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

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    96, 32, 105, 115, 32, 110, 111, 116, 32, 116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 112, 
    105, 110, 111, 58, 108, 101, 118, 101, 108, 46, 109, 98, 116, 58, 
    51, 49, 58, 51, 45, 51, 49, 58, 50, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 112, 
    105, 110, 111, 58, 108, 101, 118, 101, 108, 46, 109, 98, 116, 58, 
    50, 57, 58, 51, 45, 50, 57, 58, 50, 56, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 112, 
    105, 110, 111, 58, 108, 101, 118, 101, 108, 46, 109, 98, 116, 58, 
    51, 48, 58, 51, 45, 51, 48, 58, 50, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    32, 70, 65, 73, 76, 69, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_30 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_14 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 96, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[100]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 99), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 112, 105, 
    110, 111, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 
    68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 
    83, 107, 105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 
    105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 
    116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 112, 
    105, 110, 111, 58, 108, 101, 118, 101, 108, 46, 109, 98, 116, 58, 
    51, 50, 58, 51, 45, 51, 50, 58, 50, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    108, 101, 118, 101, 108, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_16 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    116, 114, 97, 110, 115, 112, 111, 114, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_37 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_31 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_35 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    108, 111, 103, 103, 101, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[60]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 59), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 112, 105, 110, 111, 
    34, 44, 32, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[98]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 97), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 112, 105, 
    110, 111, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 
    68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 
    74, 115, 69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 
    116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 
    101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_36 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 105, 110, 116, 101, 114, 110, 97, 108, 47, 112, 
    105, 110, 111, 58, 108, 101, 118, 101, 108, 46, 109, 98, 116, 58, 
    50, 56, 58, 51, 45, 50, 56, 58, 50, 57, 0
  };

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam8internal4pino41____test__6c6576656c2e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal4pino41____test__6c6576656c2e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__executeN17error__to__stringS696$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__executeN17error__to__stringS696
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam8internal4pino37____test__6c6576656c2e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam8internal4pino41____test__6c6576656c2e6d6274__0_2edyncall$closure.data;

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

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam8internal4pino48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal4pino41____test__6c6576656c2e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS1574
) {
  return _M0FP48clawteam8clawteam8internal4pino31____test__6c6576656c2e6d6274__0();
}

int32_t _M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS717,
  moonbit_string_t _M0L8filenameS692,
  int32_t _M0L5indexS695
) {
  struct _M0R110_24clawteam_2fclawteam_2finternal_2fpino_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c687* _closure_1799;
  struct _M0TWssbEu* _M0L14handle__resultS687;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS696;
  void* _M0L11_2atry__errS711;
  struct moonbit_result_0 _tmp_1801;
  int32_t _handle__error__result_1802;
  int32_t _M0L6_2atmpS1562;
  void* _M0L3errS712;
  moonbit_string_t _M0L4nameS714;
  struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS715;
  moonbit_string_t _M0L8_2afieldS1575;
  int32_t _M0L6_2acntS1742;
  moonbit_string_t _M0L7_2anameS716;
  #line 528 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS692);
  _closure_1799
  = (struct _M0R110_24clawteam_2fclawteam_2finternal_2fpino_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c687*)moonbit_malloc(sizeof(struct _M0R110_24clawteam_2fclawteam_2finternal_2fpino_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c687));
  Moonbit_object_header(_closure_1799)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R110_24clawteam_2fclawteam_2finternal_2fpino_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c687, $1) >> 2, 1, 0);
  _closure_1799->code
  = &_M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__executeN14handle__resultS687;
  _closure_1799->$0 = _M0L5indexS695;
  _closure_1799->$1 = _M0L8filenameS692;
  _M0L14handle__resultS687 = (struct _M0TWssbEu*)_closure_1799;
  _M0L17error__to__stringS696
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__executeN17error__to__stringS696$closure.data;
  moonbit_incref(_M0L12async__testsS717);
  moonbit_incref(_M0L17error__to__stringS696);
  moonbit_incref(_M0L8filenameS692);
  moonbit_incref(_M0L14handle__resultS687);
  #line 562 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _tmp_1801
  = _M0IP48clawteam8clawteam8internal4pino41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__test(_M0L12async__testsS717, _M0L8filenameS692, _M0L5indexS695, _M0L14handle__resultS687, _M0L17error__to__stringS696);
  if (_tmp_1801.tag) {
    int32_t const _M0L5_2aokS1571 = _tmp_1801.data.ok;
    _handle__error__result_1802 = _M0L5_2aokS1571;
  } else {
    void* const _M0L6_2aerrS1572 = _tmp_1801.data.err;
    moonbit_decref(_M0L12async__testsS717);
    moonbit_decref(_M0L17error__to__stringS696);
    moonbit_decref(_M0L8filenameS692);
    _M0L11_2atry__errS711 = _M0L6_2aerrS1572;
    goto join_710;
  }
  if (_handle__error__result_1802) {
    moonbit_decref(_M0L12async__testsS717);
    moonbit_decref(_M0L17error__to__stringS696);
    moonbit_decref(_M0L8filenameS692);
    _M0L6_2atmpS1562 = 1;
  } else {
    struct moonbit_result_0 _tmp_1803;
    int32_t _handle__error__result_1804;
    moonbit_incref(_M0L12async__testsS717);
    moonbit_incref(_M0L17error__to__stringS696);
    moonbit_incref(_M0L8filenameS692);
    moonbit_incref(_M0L14handle__resultS687);
    #line 565 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
    _tmp_1803
    = _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS717, _M0L8filenameS692, _M0L5indexS695, _M0L14handle__resultS687, _M0L17error__to__stringS696);
    if (_tmp_1803.tag) {
      int32_t const _M0L5_2aokS1569 = _tmp_1803.data.ok;
      _handle__error__result_1804 = _M0L5_2aokS1569;
    } else {
      void* const _M0L6_2aerrS1570 = _tmp_1803.data.err;
      moonbit_decref(_M0L12async__testsS717);
      moonbit_decref(_M0L17error__to__stringS696);
      moonbit_decref(_M0L8filenameS692);
      _M0L11_2atry__errS711 = _M0L6_2aerrS1570;
      goto join_710;
    }
    if (_handle__error__result_1804) {
      moonbit_decref(_M0L12async__testsS717);
      moonbit_decref(_M0L17error__to__stringS696);
      moonbit_decref(_M0L8filenameS692);
      _M0L6_2atmpS1562 = 1;
    } else {
      struct moonbit_result_0 _tmp_1805;
      int32_t _handle__error__result_1806;
      moonbit_incref(_M0L12async__testsS717);
      moonbit_incref(_M0L17error__to__stringS696);
      moonbit_incref(_M0L8filenameS692);
      moonbit_incref(_M0L14handle__resultS687);
      #line 568 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _tmp_1805
      = _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS717, _M0L8filenameS692, _M0L5indexS695, _M0L14handle__resultS687, _M0L17error__to__stringS696);
      if (_tmp_1805.tag) {
        int32_t const _M0L5_2aokS1567 = _tmp_1805.data.ok;
        _handle__error__result_1806 = _M0L5_2aokS1567;
      } else {
        void* const _M0L6_2aerrS1568 = _tmp_1805.data.err;
        moonbit_decref(_M0L12async__testsS717);
        moonbit_decref(_M0L17error__to__stringS696);
        moonbit_decref(_M0L8filenameS692);
        _M0L11_2atry__errS711 = _M0L6_2aerrS1568;
        goto join_710;
      }
      if (_handle__error__result_1806) {
        moonbit_decref(_M0L12async__testsS717);
        moonbit_decref(_M0L17error__to__stringS696);
        moonbit_decref(_M0L8filenameS692);
        _M0L6_2atmpS1562 = 1;
      } else {
        struct moonbit_result_0 _tmp_1807;
        int32_t _handle__error__result_1808;
        moonbit_incref(_M0L12async__testsS717);
        moonbit_incref(_M0L17error__to__stringS696);
        moonbit_incref(_M0L8filenameS692);
        moonbit_incref(_M0L14handle__resultS687);
        #line 571 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        _tmp_1807
        = _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS717, _M0L8filenameS692, _M0L5indexS695, _M0L14handle__resultS687, _M0L17error__to__stringS696);
        if (_tmp_1807.tag) {
          int32_t const _M0L5_2aokS1565 = _tmp_1807.data.ok;
          _handle__error__result_1808 = _M0L5_2aokS1565;
        } else {
          void* const _M0L6_2aerrS1566 = _tmp_1807.data.err;
          moonbit_decref(_M0L12async__testsS717);
          moonbit_decref(_M0L17error__to__stringS696);
          moonbit_decref(_M0L8filenameS692);
          _M0L11_2atry__errS711 = _M0L6_2aerrS1566;
          goto join_710;
        }
        if (_handle__error__result_1808) {
          moonbit_decref(_M0L12async__testsS717);
          moonbit_decref(_M0L17error__to__stringS696);
          moonbit_decref(_M0L8filenameS692);
          _M0L6_2atmpS1562 = 1;
        } else {
          struct moonbit_result_0 _tmp_1809;
          moonbit_incref(_M0L14handle__resultS687);
          #line 574 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
          _tmp_1809
          = _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS717, _M0L8filenameS692, _M0L5indexS695, _M0L14handle__resultS687, _M0L17error__to__stringS696);
          if (_tmp_1809.tag) {
            int32_t const _M0L5_2aokS1563 = _tmp_1809.data.ok;
            _M0L6_2atmpS1562 = _M0L5_2aokS1563;
          } else {
            void* const _M0L6_2aerrS1564 = _tmp_1809.data.err;
            _M0L11_2atry__errS711 = _M0L6_2aerrS1564;
            goto join_710;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS1562) {
    void* _M0L109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1573 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1573)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1573)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS711
    = _M0L109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1573;
    goto join_710;
  } else {
    moonbit_decref(_M0L14handle__resultS687);
  }
  goto joinlet_1800;
  join_710:;
  _M0L3errS712 = _M0L11_2atry__errS711;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS715
  = (struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS712;
  _M0L8_2afieldS1575 = _M0L36_2aMoonBitTestDriverInternalSkipTestS715->$0;
  _M0L6_2acntS1742
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS715)->rc;
  if (_M0L6_2acntS1742 > 1) {
    int32_t _M0L11_2anew__cntS1743 = _M0L6_2acntS1742 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS715)->rc
    = _M0L11_2anew__cntS1743;
    moonbit_incref(_M0L8_2afieldS1575);
  } else if (_M0L6_2acntS1742 == 1) {
    #line 581 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS715);
  }
  _M0L7_2anameS716 = _M0L8_2afieldS1575;
  _M0L4nameS714 = _M0L7_2anameS716;
  goto join_713;
  goto joinlet_1810;
  join_713:;
  #line 582 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__executeN14handle__resultS687(_M0L14handle__resultS687, _M0L4nameS714, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_1810:;
  joinlet_1800:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__executeN17error__to__stringS696(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS1561,
  void* _M0L3errS697
) {
  void* _M0L1eS699;
  moonbit_string_t _M0L1eS701;
  #line 551 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS1561);
  switch (Moonbit_object_tag(_M0L3errS697)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS702 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS697;
      moonbit_string_t _M0L8_2afieldS1576 = _M0L10_2aFailureS702->$0;
      int32_t _M0L6_2acntS1744 =
        Moonbit_object_header(_M0L10_2aFailureS702)->rc;
      moonbit_string_t _M0L4_2aeS703;
      if (_M0L6_2acntS1744 > 1) {
        int32_t _M0L11_2anew__cntS1745 = _M0L6_2acntS1744 - 1;
        Moonbit_object_header(_M0L10_2aFailureS702)->rc
        = _M0L11_2anew__cntS1745;
        moonbit_incref(_M0L8_2afieldS1576);
      } else if (_M0L6_2acntS1744 == 1) {
        #line 552 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS702);
      }
      _M0L4_2aeS703 = _M0L8_2afieldS1576;
      _M0L1eS701 = _M0L4_2aeS703;
      goto join_700;
      break;
    }
    
    case 2: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS704 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS697;
      moonbit_string_t _M0L8_2afieldS1577 = _M0L15_2aInspectErrorS704->$0;
      int32_t _M0L6_2acntS1746 =
        Moonbit_object_header(_M0L15_2aInspectErrorS704)->rc;
      moonbit_string_t _M0L4_2aeS705;
      if (_M0L6_2acntS1746 > 1) {
        int32_t _M0L11_2anew__cntS1747 = _M0L6_2acntS1746 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS704)->rc
        = _M0L11_2anew__cntS1747;
        moonbit_incref(_M0L8_2afieldS1577);
      } else if (_M0L6_2acntS1746 == 1) {
        #line 552 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS704);
      }
      _M0L4_2aeS705 = _M0L8_2afieldS1577;
      _M0L1eS701 = _M0L4_2aeS705;
      goto join_700;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS706 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS697;
      moonbit_string_t _M0L8_2afieldS1578 = _M0L16_2aSnapshotErrorS706->$0;
      int32_t _M0L6_2acntS1748 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS706)->rc;
      moonbit_string_t _M0L4_2aeS707;
      if (_M0L6_2acntS1748 > 1) {
        int32_t _M0L11_2anew__cntS1749 = _M0L6_2acntS1748 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS706)->rc
        = _M0L11_2anew__cntS1749;
        moonbit_incref(_M0L8_2afieldS1578);
      } else if (_M0L6_2acntS1748 == 1) {
        #line 552 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS706);
      }
      _M0L4_2aeS707 = _M0L8_2afieldS1578;
      _M0L1eS701 = _M0L4_2aeS707;
      goto join_700;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error107clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS708 =
        (struct _M0DTPC15error5Error107clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS697;
      moonbit_string_t _M0L8_2afieldS1579 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS708->$0;
      int32_t _M0L6_2acntS1750 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS708)->rc;
      moonbit_string_t _M0L4_2aeS709;
      if (_M0L6_2acntS1750 > 1) {
        int32_t _M0L11_2anew__cntS1751 = _M0L6_2acntS1750 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS708)->rc
        = _M0L11_2anew__cntS1751;
        moonbit_incref(_M0L8_2afieldS1579);
      } else if (_M0L6_2acntS1750 == 1) {
        #line 552 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS708);
      }
      _M0L4_2aeS709 = _M0L8_2afieldS1579;
      _M0L1eS701 = _M0L4_2aeS709;
      goto join_700;
      break;
    }
    default: {
      _M0L1eS699 = _M0L3errS697;
      goto join_698;
      break;
    }
  }
  join_700:;
  return _M0L1eS701;
  join_698:;
  #line 557 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS699);
}

int32_t _M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__executeN14handle__resultS687(
  struct _M0TWssbEu* _M0L6_2aenvS1547,
  moonbit_string_t _M0L8testnameS688,
  moonbit_string_t _M0L7messageS689,
  int32_t _M0L7skippedS690
) {
  struct _M0R110_24clawteam_2fclawteam_2finternal_2fpino_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c687* _M0L14_2acasted__envS1548;
  moonbit_string_t _M0L8_2afieldS1589;
  moonbit_string_t _M0L8filenameS692;
  int32_t _M0L8_2afieldS1588;
  int32_t _M0L6_2acntS1752;
  int32_t _M0L5indexS695;
  int32_t _if__result_1813;
  moonbit_string_t _M0L10file__nameS691;
  moonbit_string_t _M0L10test__nameS693;
  moonbit_string_t _M0L7messageS694;
  moonbit_string_t _M0L6_2atmpS1560;
  moonbit_string_t _M0L6_2atmpS1587;
  moonbit_string_t _M0L6_2atmpS1559;
  moonbit_string_t _M0L6_2atmpS1586;
  moonbit_string_t _M0L6_2atmpS1557;
  moonbit_string_t _M0L6_2atmpS1558;
  moonbit_string_t _M0L6_2atmpS1585;
  moonbit_string_t _M0L6_2atmpS1556;
  moonbit_string_t _M0L6_2atmpS1584;
  moonbit_string_t _M0L6_2atmpS1554;
  moonbit_string_t _M0L6_2atmpS1555;
  moonbit_string_t _M0L6_2atmpS1583;
  moonbit_string_t _M0L6_2atmpS1553;
  moonbit_string_t _M0L6_2atmpS1582;
  moonbit_string_t _M0L6_2atmpS1551;
  moonbit_string_t _M0L6_2atmpS1552;
  moonbit_string_t _M0L6_2atmpS1581;
  moonbit_string_t _M0L6_2atmpS1550;
  moonbit_string_t _M0L6_2atmpS1580;
  moonbit_string_t _M0L6_2atmpS1549;
  #line 535 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS1548
  = (struct _M0R110_24clawteam_2fclawteam_2finternal_2fpino_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c687*)_M0L6_2aenvS1547;
  _M0L8_2afieldS1589 = _M0L14_2acasted__envS1548->$1;
  _M0L8filenameS692 = _M0L8_2afieldS1589;
  _M0L8_2afieldS1588 = _M0L14_2acasted__envS1548->$0;
  _M0L6_2acntS1752 = Moonbit_object_header(_M0L14_2acasted__envS1548)->rc;
  if (_M0L6_2acntS1752 > 1) {
    int32_t _M0L11_2anew__cntS1753 = _M0L6_2acntS1752 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1548)->rc
    = _M0L11_2anew__cntS1753;
    moonbit_incref(_M0L8filenameS692);
  } else if (_M0L6_2acntS1752 == 1) {
    #line 535 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1548);
  }
  _M0L5indexS695 = _M0L8_2afieldS1588;
  if (!_M0L7skippedS690) {
    _if__result_1813 = 1;
  } else {
    _if__result_1813 = 0;
  }
  if (_if__result_1813) {
    
  }
  #line 541 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS691 = _M0MPC16string6String6escape(_M0L8filenameS692);
  #line 542 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS693 = _M0MPC16string6String6escape(_M0L8testnameS688);
  #line 543 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS694 = _M0MPC16string6String6escape(_M0L7messageS689);
  #line 544 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 546 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1560
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS691);
  #line 545 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1587
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS1560);
  moonbit_decref(_M0L6_2atmpS1560);
  _M0L6_2atmpS1559 = _M0L6_2atmpS1587;
  #line 545 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1586
  = moonbit_add_string(_M0L6_2atmpS1559, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS1559);
  _M0L6_2atmpS1557 = _M0L6_2atmpS1586;
  #line 546 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1558
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS695);
  #line 545 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1585 = moonbit_add_string(_M0L6_2atmpS1557, _M0L6_2atmpS1558);
  moonbit_decref(_M0L6_2atmpS1557);
  moonbit_decref(_M0L6_2atmpS1558);
  _M0L6_2atmpS1556 = _M0L6_2atmpS1585;
  #line 545 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1584
  = moonbit_add_string(_M0L6_2atmpS1556, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS1556);
  _M0L6_2atmpS1554 = _M0L6_2atmpS1584;
  #line 546 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1555
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS693);
  #line 545 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1583 = moonbit_add_string(_M0L6_2atmpS1554, _M0L6_2atmpS1555);
  moonbit_decref(_M0L6_2atmpS1554);
  moonbit_decref(_M0L6_2atmpS1555);
  _M0L6_2atmpS1553 = _M0L6_2atmpS1583;
  #line 545 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1582
  = moonbit_add_string(_M0L6_2atmpS1553, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS1553);
  _M0L6_2atmpS1551 = _M0L6_2atmpS1582;
  #line 546 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1552
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS694);
  #line 545 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1581 = moonbit_add_string(_M0L6_2atmpS1551, _M0L6_2atmpS1552);
  moonbit_decref(_M0L6_2atmpS1551);
  moonbit_decref(_M0L6_2atmpS1552);
  _M0L6_2atmpS1550 = _M0L6_2atmpS1581;
  #line 545 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1580
  = moonbit_add_string(_M0L6_2atmpS1550, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1550);
  _M0L6_2atmpS1549 = _M0L6_2atmpS1580;
  #line 545 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1549);
  #line 548 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam8internal4pino41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S686,
  moonbit_string_t _M0L8filenameS683,
  int32_t _M0L5indexS677,
  struct _M0TWssbEu* _M0L14handle__resultS673,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS675
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS653;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS682;
  struct _M0TWEuQRPC15error5Error* _M0L1fS655;
  moonbit_string_t* _M0L5attrsS656;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS676;
  moonbit_string_t _M0L4nameS659;
  moonbit_string_t _M0L4nameS657;
  int32_t _M0L6_2atmpS1546;
  struct _M0TWEOs* _M0L5_2aitS661;
  struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1543__l429__* _closure_1822;
  struct _M0TWEu* _M0L6_2atmpS1537;
  struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1539__l430__* _closure_1823;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS1538;
  struct moonbit_result_0 _result_1824;
  #line 409 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S686);
  moonbit_incref(_M0FP48clawteam8clawteam8internal4pino48moonbit__test__driver__internal__no__args__tests);
  #line 416 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS682
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam8internal4pino48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS683);
  if (_M0L7_2abindS682 == 0) {
    struct moonbit_result_0 _result_1815;
    if (_M0L7_2abindS682) {
      moonbit_decref(_M0L7_2abindS682);
    }
    moonbit_decref(_M0L17error__to__stringS675);
    moonbit_decref(_M0L14handle__resultS673);
    _result_1815.tag = 1;
    _result_1815.data.ok = 0;
    return _result_1815;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS684 =
      _M0L7_2abindS682;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS685 =
      _M0L7_2aSomeS684;
    _M0L10index__mapS653 = _M0L13_2aindex__mapS685;
    goto join_652;
  }
  join_652:;
  #line 418 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS676
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS653, _M0L5indexS677);
  if (_M0L7_2abindS676 == 0) {
    struct moonbit_result_0 _result_1817;
    if (_M0L7_2abindS676) {
      moonbit_decref(_M0L7_2abindS676);
    }
    moonbit_decref(_M0L17error__to__stringS675);
    moonbit_decref(_M0L14handle__resultS673);
    _result_1817.tag = 1;
    _result_1817.data.ok = 0;
    return _result_1817;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS678 = _M0L7_2abindS676;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS679 = _M0L7_2aSomeS678;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS1593 = _M0L4_2axS679->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS680 = _M0L8_2afieldS1593;
    moonbit_string_t* _M0L8_2afieldS1592 = _M0L4_2axS679->$1;
    int32_t _M0L6_2acntS1754 = Moonbit_object_header(_M0L4_2axS679)->rc;
    moonbit_string_t* _M0L8_2aattrsS681;
    if (_M0L6_2acntS1754 > 1) {
      int32_t _M0L11_2anew__cntS1755 = _M0L6_2acntS1754 - 1;
      Moonbit_object_header(_M0L4_2axS679)->rc = _M0L11_2anew__cntS1755;
      moonbit_incref(_M0L8_2afieldS1592);
      moonbit_incref(_M0L4_2afS680);
    } else if (_M0L6_2acntS1754 == 1) {
      #line 416 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS679);
    }
    _M0L8_2aattrsS681 = _M0L8_2afieldS1592;
    _M0L1fS655 = _M0L4_2afS680;
    _M0L5attrsS656 = _M0L8_2aattrsS681;
    goto join_654;
  }
  join_654:;
  _M0L6_2atmpS1546 = Moonbit_array_length(_M0L5attrsS656);
  if (_M0L6_2atmpS1546 >= 1) {
    moonbit_string_t _M0L6_2atmpS1591 = (moonbit_string_t)_M0L5attrsS656[0];
    moonbit_string_t _M0L7_2anameS660 = _M0L6_2atmpS1591;
    moonbit_incref(_M0L7_2anameS660);
    _M0L4nameS659 = _M0L7_2anameS660;
    goto join_658;
  } else {
    _M0L4nameS657 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_1818;
  join_658:;
  _M0L4nameS657 = _M0L4nameS659;
  joinlet_1818:;
  #line 419 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS661 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS656);
  while (1) {
    moonbit_string_t _M0L4attrS663;
    moonbit_string_t _M0L7_2abindS670;
    int32_t _M0L6_2atmpS1530;
    int64_t _M0L6_2atmpS1529;
    moonbit_incref(_M0L5_2aitS661);
    #line 421 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
    _M0L7_2abindS670 = _M0MPB4Iter4nextGsE(_M0L5_2aitS661);
    if (_M0L7_2abindS670 == 0) {
      if (_M0L7_2abindS670) {
        moonbit_decref(_M0L7_2abindS670);
      }
      moonbit_decref(_M0L5_2aitS661);
    } else {
      moonbit_string_t _M0L7_2aSomeS671 = _M0L7_2abindS670;
      moonbit_string_t _M0L7_2aattrS672 = _M0L7_2aSomeS671;
      _M0L4attrS663 = _M0L7_2aattrS672;
      goto join_662;
    }
    goto joinlet_1820;
    join_662:;
    _M0L6_2atmpS1530 = Moonbit_array_length(_M0L4attrS663);
    _M0L6_2atmpS1529 = (int64_t)_M0L6_2atmpS1530;
    moonbit_incref(_M0L4attrS663);
    #line 422 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS663, 5, 0, _M0L6_2atmpS1529)
    ) {
      int32_t _M0L6_2atmpS1536 = _M0L4attrS663[0];
      int32_t _M0L4_2axS664 = _M0L6_2atmpS1536;
      if (_M0L4_2axS664 == 112) {
        int32_t _M0L6_2atmpS1535 = _M0L4attrS663[1];
        int32_t _M0L4_2axS665 = _M0L6_2atmpS1535;
        if (_M0L4_2axS665 == 97) {
          int32_t _M0L6_2atmpS1534 = _M0L4attrS663[2];
          int32_t _M0L4_2axS666 = _M0L6_2atmpS1534;
          if (_M0L4_2axS666 == 110) {
            int32_t _M0L6_2atmpS1533 = _M0L4attrS663[3];
            int32_t _M0L4_2axS667 = _M0L6_2atmpS1533;
            if (_M0L4_2axS667 == 105) {
              int32_t _M0L6_2atmpS1590 = _M0L4attrS663[4];
              int32_t _M0L6_2atmpS1532;
              int32_t _M0L4_2axS668;
              moonbit_decref(_M0L4attrS663);
              _M0L6_2atmpS1532 = _M0L6_2atmpS1590;
              _M0L4_2axS668 = _M0L6_2atmpS1532;
              if (_M0L4_2axS668 == 99) {
                void* _M0L109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1531;
                struct moonbit_result_0 _result_1821;
                moonbit_decref(_M0L17error__to__stringS675);
                moonbit_decref(_M0L14handle__resultS673);
                moonbit_decref(_M0L5_2aitS661);
                moonbit_decref(_M0L1fS655);
                _M0L109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1531
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1531)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 1);
                ((struct _M0DTPC15error5Error109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1531)->$0
                = _M0L4nameS657;
                _result_1821.tag = 0;
                _result_1821.data.err
                = _M0L109clawteam_2fclawteam_2finternal_2fpino_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1531;
                return _result_1821;
              }
            } else {
              moonbit_decref(_M0L4attrS663);
            }
          } else {
            moonbit_decref(_M0L4attrS663);
          }
        } else {
          moonbit_decref(_M0L4attrS663);
        }
      } else {
        moonbit_decref(_M0L4attrS663);
      }
    } else {
      moonbit_decref(_M0L4attrS663);
    }
    continue;
    joinlet_1820:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS673);
  moonbit_incref(_M0L4nameS657);
  _closure_1822
  = (struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1543__l429__*)moonbit_malloc(sizeof(struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1543__l429__));
  Moonbit_object_header(_closure_1822)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1543__l429__, $0) >> 2, 2, 0);
  _closure_1822->code
  = &_M0IP48clawteam8clawteam8internal4pino41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testC1543l429;
  _closure_1822->$0 = _M0L14handle__resultS673;
  _closure_1822->$1 = _M0L4nameS657;
  _M0L6_2atmpS1537 = (struct _M0TWEu*)_closure_1822;
  _closure_1823
  = (struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1539__l430__*)moonbit_malloc(sizeof(struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1539__l430__));
  Moonbit_object_header(_closure_1823)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1539__l430__, $0) >> 2, 3, 0);
  _closure_1823->code
  = &_M0IP48clawteam8clawteam8internal4pino41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testC1539l430;
  _closure_1823->$0 = _M0L17error__to__stringS675;
  _closure_1823->$1 = _M0L14handle__resultS673;
  _closure_1823->$2 = _M0L4nameS657;
  _M0L6_2atmpS1538 = (struct _M0TWRPC15error5ErrorEu*)_closure_1823;
  #line 427 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal4pino45moonbit__test__driver__internal__catch__error(_M0L1fS655, _M0L6_2atmpS1537, _M0L6_2atmpS1538);
  _result_1824.tag = 1;
  _result_1824.data.ok = 1;
  return _result_1824;
}

int32_t _M0IP48clawteam8clawteam8internal4pino41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testC1543l429(
  struct _M0TWEu* _M0L6_2aenvS1544
) {
  struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1543__l429__* _M0L14_2acasted__envS1545;
  moonbit_string_t _M0L8_2afieldS1595;
  moonbit_string_t _M0L4nameS657;
  struct _M0TWssbEu* _M0L8_2afieldS1594;
  int32_t _M0L6_2acntS1756;
  struct _M0TWssbEu* _M0L14handle__resultS673;
  #line 429 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS1545
  = (struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1543__l429__*)_M0L6_2aenvS1544;
  _M0L8_2afieldS1595 = _M0L14_2acasted__envS1545->$1;
  _M0L4nameS657 = _M0L8_2afieldS1595;
  _M0L8_2afieldS1594 = _M0L14_2acasted__envS1545->$0;
  _M0L6_2acntS1756 = Moonbit_object_header(_M0L14_2acasted__envS1545)->rc;
  if (_M0L6_2acntS1756 > 1) {
    int32_t _M0L11_2anew__cntS1757 = _M0L6_2acntS1756 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1545)->rc
    = _M0L11_2anew__cntS1757;
    moonbit_incref(_M0L4nameS657);
    moonbit_incref(_M0L8_2afieldS1594);
  } else if (_M0L6_2acntS1756 == 1) {
    #line 429 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1545);
  }
  _M0L14handle__resultS673 = _M0L8_2afieldS1594;
  #line 429 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS673->code(_M0L14handle__resultS673, _M0L4nameS657, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam8internal4pino41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testC1539l430(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS1540,
  void* _M0L3errS674
) {
  struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1539__l430__* _M0L14_2acasted__envS1541;
  moonbit_string_t _M0L8_2afieldS1598;
  moonbit_string_t _M0L4nameS657;
  struct _M0TWssbEu* _M0L8_2afieldS1597;
  struct _M0TWssbEu* _M0L14handle__resultS673;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS1596;
  int32_t _M0L6_2acntS1758;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS675;
  moonbit_string_t _M0L6_2atmpS1542;
  #line 430 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS1541
  = (struct _M0R191_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2finternal_2fpino_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1539__l430__*)_M0L6_2aenvS1540;
  _M0L8_2afieldS1598 = _M0L14_2acasted__envS1541->$2;
  _M0L4nameS657 = _M0L8_2afieldS1598;
  _M0L8_2afieldS1597 = _M0L14_2acasted__envS1541->$1;
  _M0L14handle__resultS673 = _M0L8_2afieldS1597;
  _M0L8_2afieldS1596 = _M0L14_2acasted__envS1541->$0;
  _M0L6_2acntS1758 = Moonbit_object_header(_M0L14_2acasted__envS1541)->rc;
  if (_M0L6_2acntS1758 > 1) {
    int32_t _M0L11_2anew__cntS1759 = _M0L6_2acntS1758 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1541)->rc
    = _M0L11_2anew__cntS1759;
    moonbit_incref(_M0L4nameS657);
    moonbit_incref(_M0L14handle__resultS673);
    moonbit_incref(_M0L8_2afieldS1596);
  } else if (_M0L6_2acntS1758 == 1) {
    #line 430 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1541);
  }
  _M0L17error__to__stringS675 = _M0L8_2afieldS1596;
  #line 430 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1542
  = _M0L17error__to__stringS675->code(_M0L17error__to__stringS675, _M0L3errS674);
  #line 430 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS673->code(_M0L14handle__resultS673, _M0L4nameS657, _M0L6_2atmpS1542, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam8internal4pino45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS646,
  struct _M0TWEu* _M0L6on__okS647,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS644
) {
  void* _M0L11_2atry__errS642;
  struct moonbit_result_0 _tmp_1826;
  void* _M0L3errS643;
  #line 375 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _tmp_1826 = _M0L1fS646->code(_M0L1fS646);
  if (_tmp_1826.tag) {
    int32_t const _M0L5_2aokS1527 = _tmp_1826.data.ok;
    moonbit_decref(_M0L7on__errS644);
  } else {
    void* const _M0L6_2aerrS1528 = _tmp_1826.data.err;
    moonbit_decref(_M0L6on__okS647);
    _M0L11_2atry__errS642 = _M0L6_2aerrS1528;
    goto join_641;
  }
  #line 382 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS647->code(_M0L6on__okS647);
  goto joinlet_1825;
  join_641:;
  _M0L3errS643 = _M0L11_2atry__errS642;
  #line 383 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS644->code(_M0L7on__errS644, _M0L3errS643);
  joinlet_1825:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S601;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS607;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS614;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS619;
  struct _M0TUsiE** _M0L6_2atmpS1526;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS626;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS627;
  moonbit_string_t _M0L6_2atmpS1525;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS628;
  int32_t _M0L7_2abindS629;
  int32_t _M0L2__S630;
  #line 193 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S601 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS607 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS614
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS607;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS619 = 0;
  _M0L6_2atmpS1526 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS626
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS626)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS626->$0 = _M0L6_2atmpS1526;
  _M0L16file__and__indexS626->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS627
  = _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS614(_M0L57moonbit__test__driver__internal__get__cli__args__internalS614);
  #line 284 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1525 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS627, 1);
  #line 283 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS628
  = _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS619(_M0L51moonbit__test__driver__internal__split__mbt__stringS619, _M0L6_2atmpS1525, 47);
  _M0L7_2abindS629 = _M0L10test__argsS628->$1;
  _M0L2__S630 = 0;
  while (1) {
    if (_M0L2__S630 < _M0L7_2abindS629) {
      moonbit_string_t* _M0L8_2afieldS1600 = _M0L10test__argsS628->$0;
      moonbit_string_t* _M0L3bufS1524 = _M0L8_2afieldS1600;
      moonbit_string_t _M0L6_2atmpS1599 =
        (moonbit_string_t)_M0L3bufS1524[_M0L2__S630];
      moonbit_string_t _M0L3argS631 = _M0L6_2atmpS1599;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS632;
      moonbit_string_t _M0L4fileS633;
      moonbit_string_t _M0L5rangeS634;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS635;
      moonbit_string_t _M0L6_2atmpS1522;
      int32_t _M0L5startS636;
      moonbit_string_t _M0L6_2atmpS1521;
      int32_t _M0L3endS637;
      int32_t _M0L1iS638;
      int32_t _M0L6_2atmpS1523;
      moonbit_incref(_M0L3argS631);
      #line 288 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS632
      = _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS619(_M0L51moonbit__test__driver__internal__split__mbt__stringS619, _M0L3argS631, 58);
      moonbit_incref(_M0L16file__and__rangeS632);
      #line 289 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS633
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS632, 0);
      #line 290 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS634
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS632, 1);
      #line 291 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS635
      = _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS619(_M0L51moonbit__test__driver__internal__split__mbt__stringS619, _M0L5rangeS634, 45);
      moonbit_incref(_M0L15start__and__endS635);
      #line 294 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS1522
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS635, 0);
      #line 294 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _M0L5startS636
      = _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S601(_M0L45moonbit__test__driver__internal__parse__int__S601, _M0L6_2atmpS1522);
      #line 295 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS1521
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS635, 1);
      #line 295 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _M0L3endS637
      = _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S601(_M0L45moonbit__test__driver__internal__parse__int__S601, _M0L6_2atmpS1521);
      _M0L1iS638 = _M0L5startS636;
      while (1) {
        if (_M0L1iS638 < _M0L3endS637) {
          struct _M0TUsiE* _M0L8_2atupleS1519;
          int32_t _M0L6_2atmpS1520;
          moonbit_incref(_M0L4fileS633);
          _M0L8_2atupleS1519
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS1519)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS1519->$0 = _M0L4fileS633;
          _M0L8_2atupleS1519->$1 = _M0L1iS638;
          moonbit_incref(_M0L16file__and__indexS626);
          #line 297 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS626, _M0L8_2atupleS1519);
          _M0L6_2atmpS1520 = _M0L1iS638 + 1;
          _M0L1iS638 = _M0L6_2atmpS1520;
          continue;
        } else {
          moonbit_decref(_M0L4fileS633);
        }
        break;
      }
      _M0L6_2atmpS1523 = _M0L2__S630 + 1;
      _M0L2__S630 = _M0L6_2atmpS1523;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS628);
    }
    break;
  }
  return _M0L16file__and__indexS626;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS619(
  int32_t _M0L6_2aenvS1500,
  moonbit_string_t _M0L1sS620,
  int32_t _M0L3sepS621
) {
  moonbit_string_t* _M0L6_2atmpS1518;
  struct _M0TPB5ArrayGsE* _M0L3resS622;
  struct _M0TPC13ref3RefGiE* _M0L1iS623;
  struct _M0TPC13ref3RefGiE* _M0L5startS624;
  int32_t _M0L3valS1513;
  int32_t _M0L6_2atmpS1514;
  #line 261 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1518 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS622
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS622)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS622->$0 = _M0L6_2atmpS1518;
  _M0L3resS622->$1 = 0;
  _M0L1iS623
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS623)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS623->$0 = 0;
  _M0L5startS624
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS624)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS624->$0 = 0;
  while (1) {
    int32_t _M0L3valS1501 = _M0L1iS623->$0;
    int32_t _M0L6_2atmpS1502 = Moonbit_array_length(_M0L1sS620);
    if (_M0L3valS1501 < _M0L6_2atmpS1502) {
      int32_t _M0L3valS1505 = _M0L1iS623->$0;
      int32_t _M0L6_2atmpS1504;
      int32_t _M0L6_2atmpS1503;
      int32_t _M0L3valS1512;
      int32_t _M0L6_2atmpS1511;
      if (
        _M0L3valS1505 < 0
        || _M0L3valS1505 >= Moonbit_array_length(_M0L1sS620)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1504 = _M0L1sS620[_M0L3valS1505];
      _M0L6_2atmpS1503 = _M0L6_2atmpS1504;
      if (_M0L6_2atmpS1503 == _M0L3sepS621) {
        int32_t _M0L3valS1507 = _M0L5startS624->$0;
        int32_t _M0L3valS1508 = _M0L1iS623->$0;
        moonbit_string_t _M0L6_2atmpS1506;
        int32_t _M0L3valS1510;
        int32_t _M0L6_2atmpS1509;
        moonbit_incref(_M0L1sS620);
        #line 270 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS1506
        = _M0MPC16string6String17unsafe__substring(_M0L1sS620, _M0L3valS1507, _M0L3valS1508);
        moonbit_incref(_M0L3resS622);
        #line 270 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS622, _M0L6_2atmpS1506);
        _M0L3valS1510 = _M0L1iS623->$0;
        _M0L6_2atmpS1509 = _M0L3valS1510 + 1;
        _M0L5startS624->$0 = _M0L6_2atmpS1509;
      }
      _M0L3valS1512 = _M0L1iS623->$0;
      _M0L6_2atmpS1511 = _M0L3valS1512 + 1;
      _M0L1iS623->$0 = _M0L6_2atmpS1511;
      continue;
    } else {
      moonbit_decref(_M0L1iS623);
    }
    break;
  }
  _M0L3valS1513 = _M0L5startS624->$0;
  _M0L6_2atmpS1514 = Moonbit_array_length(_M0L1sS620);
  if (_M0L3valS1513 < _M0L6_2atmpS1514) {
    int32_t _M0L8_2afieldS1601 = _M0L5startS624->$0;
    int32_t _M0L3valS1516;
    int32_t _M0L6_2atmpS1517;
    moonbit_string_t _M0L6_2atmpS1515;
    moonbit_decref(_M0L5startS624);
    _M0L3valS1516 = _M0L8_2afieldS1601;
    _M0L6_2atmpS1517 = Moonbit_array_length(_M0L1sS620);
    #line 276 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS1515
    = _M0MPC16string6String17unsafe__substring(_M0L1sS620, _M0L3valS1516, _M0L6_2atmpS1517);
    moonbit_incref(_M0L3resS622);
    #line 276 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS622, _M0L6_2atmpS1515);
  } else {
    moonbit_decref(_M0L5startS624);
    moonbit_decref(_M0L1sS620);
  }
  return _M0L3resS622;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS614(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS607
) {
  moonbit_bytes_t* _M0L3tmpS615;
  int32_t _M0L6_2atmpS1499;
  struct _M0TPB5ArrayGsE* _M0L3resS616;
  int32_t _M0L1iS617;
  #line 250 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS615
  = _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS1499 = Moonbit_array_length(_M0L3tmpS615);
  #line 254 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L3resS616 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS1499);
  _M0L1iS617 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1495 = Moonbit_array_length(_M0L3tmpS615);
    if (_M0L1iS617 < _M0L6_2atmpS1495) {
      moonbit_bytes_t _M0L6_2atmpS1602;
      moonbit_bytes_t _M0L6_2atmpS1497;
      moonbit_string_t _M0L6_2atmpS1496;
      int32_t _M0L6_2atmpS1498;
      if (_M0L1iS617 < 0 || _M0L1iS617 >= Moonbit_array_length(_M0L3tmpS615)) {
        #line 256 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1602 = (moonbit_bytes_t)_M0L3tmpS615[_M0L1iS617];
      _M0L6_2atmpS1497 = _M0L6_2atmpS1602;
      moonbit_incref(_M0L6_2atmpS1497);
      #line 256 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS1496
      = _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS607(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS607, _M0L6_2atmpS1497);
      moonbit_incref(_M0L3resS616);
      #line 256 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS616, _M0L6_2atmpS1496);
      _M0L6_2atmpS1498 = _M0L1iS617 + 1;
      _M0L1iS617 = _M0L6_2atmpS1498;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS615);
    }
    break;
  }
  return _M0L3resS616;
}

moonbit_string_t _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS607(
  int32_t _M0L6_2aenvS1409,
  moonbit_bytes_t _M0L5bytesS608
) {
  struct _M0TPB13StringBuilder* _M0L3resS609;
  int32_t _M0L3lenS610;
  struct _M0TPC13ref3RefGiE* _M0L1iS611;
  #line 206 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L3resS609 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS610 = Moonbit_array_length(_M0L5bytesS608);
  _M0L1iS611
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS611)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS611->$0 = 0;
  while (1) {
    int32_t _M0L3valS1410 = _M0L1iS611->$0;
    if (_M0L3valS1410 < _M0L3lenS610) {
      int32_t _M0L3valS1494 = _M0L1iS611->$0;
      int32_t _M0L6_2atmpS1493;
      int32_t _M0L6_2atmpS1492;
      struct _M0TPC13ref3RefGiE* _M0L1cS612;
      int32_t _M0L3valS1411;
      if (
        _M0L3valS1494 < 0
        || _M0L3valS1494 >= Moonbit_array_length(_M0L5bytesS608)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1493 = _M0L5bytesS608[_M0L3valS1494];
      _M0L6_2atmpS1492 = (int32_t)_M0L6_2atmpS1493;
      _M0L1cS612
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS612)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS612->$0 = _M0L6_2atmpS1492;
      _M0L3valS1411 = _M0L1cS612->$0;
      if (_M0L3valS1411 < 128) {
        int32_t _M0L8_2afieldS1603 = _M0L1cS612->$0;
        int32_t _M0L3valS1413;
        int32_t _M0L6_2atmpS1412;
        int32_t _M0L3valS1415;
        int32_t _M0L6_2atmpS1414;
        moonbit_decref(_M0L1cS612);
        _M0L3valS1413 = _M0L8_2afieldS1603;
        _M0L6_2atmpS1412 = _M0L3valS1413;
        moonbit_incref(_M0L3resS609);
        #line 215 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS609, _M0L6_2atmpS1412);
        _M0L3valS1415 = _M0L1iS611->$0;
        _M0L6_2atmpS1414 = _M0L3valS1415 + 1;
        _M0L1iS611->$0 = _M0L6_2atmpS1414;
      } else {
        int32_t _M0L3valS1416 = _M0L1cS612->$0;
        if (_M0L3valS1416 < 224) {
          int32_t _M0L3valS1418 = _M0L1iS611->$0;
          int32_t _M0L6_2atmpS1417 = _M0L3valS1418 + 1;
          int32_t _M0L3valS1427;
          int32_t _M0L6_2atmpS1426;
          int32_t _M0L6_2atmpS1420;
          int32_t _M0L3valS1425;
          int32_t _M0L6_2atmpS1424;
          int32_t _M0L6_2atmpS1423;
          int32_t _M0L6_2atmpS1422;
          int32_t _M0L6_2atmpS1421;
          int32_t _M0L6_2atmpS1419;
          int32_t _M0L8_2afieldS1604;
          int32_t _M0L3valS1429;
          int32_t _M0L6_2atmpS1428;
          int32_t _M0L3valS1431;
          int32_t _M0L6_2atmpS1430;
          if (_M0L6_2atmpS1417 >= _M0L3lenS610) {
            moonbit_decref(_M0L1cS612);
            moonbit_decref(_M0L1iS611);
            moonbit_decref(_M0L5bytesS608);
            break;
          }
          _M0L3valS1427 = _M0L1cS612->$0;
          _M0L6_2atmpS1426 = _M0L3valS1427 & 31;
          _M0L6_2atmpS1420 = _M0L6_2atmpS1426 << 6;
          _M0L3valS1425 = _M0L1iS611->$0;
          _M0L6_2atmpS1424 = _M0L3valS1425 + 1;
          if (
            _M0L6_2atmpS1424 < 0
            || _M0L6_2atmpS1424 >= Moonbit_array_length(_M0L5bytesS608)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1423 = _M0L5bytesS608[_M0L6_2atmpS1424];
          _M0L6_2atmpS1422 = (int32_t)_M0L6_2atmpS1423;
          _M0L6_2atmpS1421 = _M0L6_2atmpS1422 & 63;
          _M0L6_2atmpS1419 = _M0L6_2atmpS1420 | _M0L6_2atmpS1421;
          _M0L1cS612->$0 = _M0L6_2atmpS1419;
          _M0L8_2afieldS1604 = _M0L1cS612->$0;
          moonbit_decref(_M0L1cS612);
          _M0L3valS1429 = _M0L8_2afieldS1604;
          _M0L6_2atmpS1428 = _M0L3valS1429;
          moonbit_incref(_M0L3resS609);
          #line 222 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS609, _M0L6_2atmpS1428);
          _M0L3valS1431 = _M0L1iS611->$0;
          _M0L6_2atmpS1430 = _M0L3valS1431 + 2;
          _M0L1iS611->$0 = _M0L6_2atmpS1430;
        } else {
          int32_t _M0L3valS1432 = _M0L1cS612->$0;
          if (_M0L3valS1432 < 240) {
            int32_t _M0L3valS1434 = _M0L1iS611->$0;
            int32_t _M0L6_2atmpS1433 = _M0L3valS1434 + 2;
            int32_t _M0L3valS1450;
            int32_t _M0L6_2atmpS1449;
            int32_t _M0L6_2atmpS1442;
            int32_t _M0L3valS1448;
            int32_t _M0L6_2atmpS1447;
            int32_t _M0L6_2atmpS1446;
            int32_t _M0L6_2atmpS1445;
            int32_t _M0L6_2atmpS1444;
            int32_t _M0L6_2atmpS1443;
            int32_t _M0L6_2atmpS1436;
            int32_t _M0L3valS1441;
            int32_t _M0L6_2atmpS1440;
            int32_t _M0L6_2atmpS1439;
            int32_t _M0L6_2atmpS1438;
            int32_t _M0L6_2atmpS1437;
            int32_t _M0L6_2atmpS1435;
            int32_t _M0L8_2afieldS1605;
            int32_t _M0L3valS1452;
            int32_t _M0L6_2atmpS1451;
            int32_t _M0L3valS1454;
            int32_t _M0L6_2atmpS1453;
            if (_M0L6_2atmpS1433 >= _M0L3lenS610) {
              moonbit_decref(_M0L1cS612);
              moonbit_decref(_M0L1iS611);
              moonbit_decref(_M0L5bytesS608);
              break;
            }
            _M0L3valS1450 = _M0L1cS612->$0;
            _M0L6_2atmpS1449 = _M0L3valS1450 & 15;
            _M0L6_2atmpS1442 = _M0L6_2atmpS1449 << 12;
            _M0L3valS1448 = _M0L1iS611->$0;
            _M0L6_2atmpS1447 = _M0L3valS1448 + 1;
            if (
              _M0L6_2atmpS1447 < 0
              || _M0L6_2atmpS1447 >= Moonbit_array_length(_M0L5bytesS608)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1446 = _M0L5bytesS608[_M0L6_2atmpS1447];
            _M0L6_2atmpS1445 = (int32_t)_M0L6_2atmpS1446;
            _M0L6_2atmpS1444 = _M0L6_2atmpS1445 & 63;
            _M0L6_2atmpS1443 = _M0L6_2atmpS1444 << 6;
            _M0L6_2atmpS1436 = _M0L6_2atmpS1442 | _M0L6_2atmpS1443;
            _M0L3valS1441 = _M0L1iS611->$0;
            _M0L6_2atmpS1440 = _M0L3valS1441 + 2;
            if (
              _M0L6_2atmpS1440 < 0
              || _M0L6_2atmpS1440 >= Moonbit_array_length(_M0L5bytesS608)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1439 = _M0L5bytesS608[_M0L6_2atmpS1440];
            _M0L6_2atmpS1438 = (int32_t)_M0L6_2atmpS1439;
            _M0L6_2atmpS1437 = _M0L6_2atmpS1438 & 63;
            _M0L6_2atmpS1435 = _M0L6_2atmpS1436 | _M0L6_2atmpS1437;
            _M0L1cS612->$0 = _M0L6_2atmpS1435;
            _M0L8_2afieldS1605 = _M0L1cS612->$0;
            moonbit_decref(_M0L1cS612);
            _M0L3valS1452 = _M0L8_2afieldS1605;
            _M0L6_2atmpS1451 = _M0L3valS1452;
            moonbit_incref(_M0L3resS609);
            #line 231 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS609, _M0L6_2atmpS1451);
            _M0L3valS1454 = _M0L1iS611->$0;
            _M0L6_2atmpS1453 = _M0L3valS1454 + 3;
            _M0L1iS611->$0 = _M0L6_2atmpS1453;
          } else {
            int32_t _M0L3valS1456 = _M0L1iS611->$0;
            int32_t _M0L6_2atmpS1455 = _M0L3valS1456 + 3;
            int32_t _M0L3valS1479;
            int32_t _M0L6_2atmpS1478;
            int32_t _M0L6_2atmpS1471;
            int32_t _M0L3valS1477;
            int32_t _M0L6_2atmpS1476;
            int32_t _M0L6_2atmpS1475;
            int32_t _M0L6_2atmpS1474;
            int32_t _M0L6_2atmpS1473;
            int32_t _M0L6_2atmpS1472;
            int32_t _M0L6_2atmpS1464;
            int32_t _M0L3valS1470;
            int32_t _M0L6_2atmpS1469;
            int32_t _M0L6_2atmpS1468;
            int32_t _M0L6_2atmpS1467;
            int32_t _M0L6_2atmpS1466;
            int32_t _M0L6_2atmpS1465;
            int32_t _M0L6_2atmpS1458;
            int32_t _M0L3valS1463;
            int32_t _M0L6_2atmpS1462;
            int32_t _M0L6_2atmpS1461;
            int32_t _M0L6_2atmpS1460;
            int32_t _M0L6_2atmpS1459;
            int32_t _M0L6_2atmpS1457;
            int32_t _M0L3valS1481;
            int32_t _M0L6_2atmpS1480;
            int32_t _M0L3valS1485;
            int32_t _M0L6_2atmpS1484;
            int32_t _M0L6_2atmpS1483;
            int32_t _M0L6_2atmpS1482;
            int32_t _M0L8_2afieldS1606;
            int32_t _M0L3valS1489;
            int32_t _M0L6_2atmpS1488;
            int32_t _M0L6_2atmpS1487;
            int32_t _M0L6_2atmpS1486;
            int32_t _M0L3valS1491;
            int32_t _M0L6_2atmpS1490;
            if (_M0L6_2atmpS1455 >= _M0L3lenS610) {
              moonbit_decref(_M0L1cS612);
              moonbit_decref(_M0L1iS611);
              moonbit_decref(_M0L5bytesS608);
              break;
            }
            _M0L3valS1479 = _M0L1cS612->$0;
            _M0L6_2atmpS1478 = _M0L3valS1479 & 7;
            _M0L6_2atmpS1471 = _M0L6_2atmpS1478 << 18;
            _M0L3valS1477 = _M0L1iS611->$0;
            _M0L6_2atmpS1476 = _M0L3valS1477 + 1;
            if (
              _M0L6_2atmpS1476 < 0
              || _M0L6_2atmpS1476 >= Moonbit_array_length(_M0L5bytesS608)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1475 = _M0L5bytesS608[_M0L6_2atmpS1476];
            _M0L6_2atmpS1474 = (int32_t)_M0L6_2atmpS1475;
            _M0L6_2atmpS1473 = _M0L6_2atmpS1474 & 63;
            _M0L6_2atmpS1472 = _M0L6_2atmpS1473 << 12;
            _M0L6_2atmpS1464 = _M0L6_2atmpS1471 | _M0L6_2atmpS1472;
            _M0L3valS1470 = _M0L1iS611->$0;
            _M0L6_2atmpS1469 = _M0L3valS1470 + 2;
            if (
              _M0L6_2atmpS1469 < 0
              || _M0L6_2atmpS1469 >= Moonbit_array_length(_M0L5bytesS608)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1468 = _M0L5bytesS608[_M0L6_2atmpS1469];
            _M0L6_2atmpS1467 = (int32_t)_M0L6_2atmpS1468;
            _M0L6_2atmpS1466 = _M0L6_2atmpS1467 & 63;
            _M0L6_2atmpS1465 = _M0L6_2atmpS1466 << 6;
            _M0L6_2atmpS1458 = _M0L6_2atmpS1464 | _M0L6_2atmpS1465;
            _M0L3valS1463 = _M0L1iS611->$0;
            _M0L6_2atmpS1462 = _M0L3valS1463 + 3;
            if (
              _M0L6_2atmpS1462 < 0
              || _M0L6_2atmpS1462 >= Moonbit_array_length(_M0L5bytesS608)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1461 = _M0L5bytesS608[_M0L6_2atmpS1462];
            _M0L6_2atmpS1460 = (int32_t)_M0L6_2atmpS1461;
            _M0L6_2atmpS1459 = _M0L6_2atmpS1460 & 63;
            _M0L6_2atmpS1457 = _M0L6_2atmpS1458 | _M0L6_2atmpS1459;
            _M0L1cS612->$0 = _M0L6_2atmpS1457;
            _M0L3valS1481 = _M0L1cS612->$0;
            _M0L6_2atmpS1480 = _M0L3valS1481 - 65536;
            _M0L1cS612->$0 = _M0L6_2atmpS1480;
            _M0L3valS1485 = _M0L1cS612->$0;
            _M0L6_2atmpS1484 = _M0L3valS1485 >> 10;
            _M0L6_2atmpS1483 = _M0L6_2atmpS1484 + 55296;
            _M0L6_2atmpS1482 = _M0L6_2atmpS1483;
            moonbit_incref(_M0L3resS609);
            #line 242 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS609, _M0L6_2atmpS1482);
            _M0L8_2afieldS1606 = _M0L1cS612->$0;
            moonbit_decref(_M0L1cS612);
            _M0L3valS1489 = _M0L8_2afieldS1606;
            _M0L6_2atmpS1488 = _M0L3valS1489 & 1023;
            _M0L6_2atmpS1487 = _M0L6_2atmpS1488 + 56320;
            _M0L6_2atmpS1486 = _M0L6_2atmpS1487;
            moonbit_incref(_M0L3resS609);
            #line 243 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS609, _M0L6_2atmpS1486);
            _M0L3valS1491 = _M0L1iS611->$0;
            _M0L6_2atmpS1490 = _M0L3valS1491 + 4;
            _M0L1iS611->$0 = _M0L6_2atmpS1490;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS611);
      moonbit_decref(_M0L5bytesS608);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS609);
}

int32_t _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S601(
  int32_t _M0L6_2aenvS1402,
  moonbit_string_t _M0L1sS602
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS603;
  int32_t _M0L3lenS604;
  int32_t _M0L1iS605;
  int32_t _M0L8_2afieldS1607;
  #line 197 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L3resS603
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS603)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS603->$0 = 0;
  _M0L3lenS604 = Moonbit_array_length(_M0L1sS602);
  _M0L1iS605 = 0;
  while (1) {
    if (_M0L1iS605 < _M0L3lenS604) {
      int32_t _M0L3valS1407 = _M0L3resS603->$0;
      int32_t _M0L6_2atmpS1404 = _M0L3valS1407 * 10;
      int32_t _M0L6_2atmpS1406;
      int32_t _M0L6_2atmpS1405;
      int32_t _M0L6_2atmpS1403;
      int32_t _M0L6_2atmpS1408;
      if (_M0L1iS605 < 0 || _M0L1iS605 >= Moonbit_array_length(_M0L1sS602)) {
        #line 201 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1406 = _M0L1sS602[_M0L1iS605];
      _M0L6_2atmpS1405 = _M0L6_2atmpS1406 - 48;
      _M0L6_2atmpS1403 = _M0L6_2atmpS1404 + _M0L6_2atmpS1405;
      _M0L3resS603->$0 = _M0L6_2atmpS1403;
      _M0L6_2atmpS1408 = _M0L1iS605 + 1;
      _M0L1iS605 = _M0L6_2atmpS1408;
      continue;
    } else {
      moonbit_decref(_M0L1sS602);
    }
    break;
  }
  _M0L8_2afieldS1607 = _M0L3resS603->$0;
  moonbit_decref(_M0L3resS603);
  return _M0L8_2afieldS1607;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S581,
  moonbit_string_t _M0L12_2adiscard__S582,
  int32_t _M0L12_2adiscard__S583,
  struct _M0TWssbEu* _M0L12_2adiscard__S584,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S585
) {
  struct moonbit_result_0 _result_1833;
  #line 34 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S585);
  moonbit_decref(_M0L12_2adiscard__S584);
  moonbit_decref(_M0L12_2adiscard__S582);
  moonbit_decref(_M0L12_2adiscard__S581);
  _result_1833.tag = 1;
  _result_1833.data.ok = 0;
  return _result_1833;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S586,
  moonbit_string_t _M0L12_2adiscard__S587,
  int32_t _M0L12_2adiscard__S588,
  struct _M0TWssbEu* _M0L12_2adiscard__S589,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S590
) {
  struct moonbit_result_0 _result_1834;
  #line 34 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S590);
  moonbit_decref(_M0L12_2adiscard__S589);
  moonbit_decref(_M0L12_2adiscard__S587);
  moonbit_decref(_M0L12_2adiscard__S586);
  _result_1834.tag = 1;
  _result_1834.data.ok = 0;
  return _result_1834;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S591,
  moonbit_string_t _M0L12_2adiscard__S592,
  int32_t _M0L12_2adiscard__S593,
  struct _M0TWssbEu* _M0L12_2adiscard__S594,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S595
) {
  struct moonbit_result_0 _result_1835;
  #line 34 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S595);
  moonbit_decref(_M0L12_2adiscard__S594);
  moonbit_decref(_M0L12_2adiscard__S592);
  moonbit_decref(_M0L12_2adiscard__S591);
  _result_1835.tag = 1;
  _result_1835.data.ok = 0;
  return _result_1835;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam8internal4pino21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam8internal4pino50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S596,
  moonbit_string_t _M0L12_2adiscard__S597,
  int32_t _M0L12_2adiscard__S598,
  struct _M0TWssbEu* _M0L12_2adiscard__S599,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S600
) {
  struct moonbit_result_0 _result_1836;
  #line 34 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S600);
  moonbit_decref(_M0L12_2adiscard__S599);
  moonbit_decref(_M0L12_2adiscard__S597);
  moonbit_decref(_M0L12_2adiscard__S596);
  _result_1836.tag = 1;
  _result_1836.data.ok = 0;
  return _result_1836;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam8internal4pino28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal4pino34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S580
) {
  #line 12 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S580);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam8internal4pino31____test__6c6576656c2e6d6274__0(
  
) {
  int32_t _M0L6_2atmpS1384;
  moonbit_string_t _M0L6_2atmpS1385;
  struct moonbit_result_0 _tmp_1837;
  int32_t _M0L6_2atmpS1388;
  moonbit_string_t _M0L6_2atmpS1389;
  struct moonbit_result_0 _tmp_1839;
  int32_t _M0L6_2atmpS1392;
  moonbit_string_t _M0L6_2atmpS1393;
  struct moonbit_result_0 _tmp_1841;
  int32_t _M0L6_2atmpS1396;
  moonbit_string_t _M0L6_2atmpS1397;
  struct moonbit_result_0 _tmp_1843;
  int32_t _M0L6_2atmpS1400;
  moonbit_string_t _M0L6_2atmpS1401;
  #line 27 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  #line 28 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  _M0L6_2atmpS1384
  = _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal4pino5LevelE(0, 1);
  _M0L6_2atmpS1385 = 0;
  #line 28 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  _tmp_1837
  = _M0FPB12assert__true(_M0L6_2atmpS1384, _M0L6_2atmpS1385, (moonbit_string_t)moonbit_string_literal_9.data);
  if (_tmp_1837.tag) {
    int32_t const _M0L5_2aokS1386 = _tmp_1837.data.ok;
  } else {
    void* const _M0L6_2aerrS1387 = _tmp_1837.data.err;
    struct moonbit_result_0 _result_1838;
    _result_1838.tag = 0;
    _result_1838.data.err = _M0L6_2aerrS1387;
    return _result_1838;
  }
  #line 29 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  _M0L6_2atmpS1388
  = _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal4pino5LevelE(1, 2);
  _M0L6_2atmpS1389 = 0;
  #line 29 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  _tmp_1839
  = _M0FPB12assert__true(_M0L6_2atmpS1388, _M0L6_2atmpS1389, (moonbit_string_t)moonbit_string_literal_10.data);
  if (_tmp_1839.tag) {
    int32_t const _M0L5_2aokS1390 = _tmp_1839.data.ok;
  } else {
    void* const _M0L6_2aerrS1391 = _tmp_1839.data.err;
    struct moonbit_result_0 _result_1840;
    _result_1840.tag = 0;
    _result_1840.data.err = _M0L6_2aerrS1391;
    return _result_1840;
  }
  #line 30 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  _M0L6_2atmpS1392
  = _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal4pino5LevelE(2, 3);
  _M0L6_2atmpS1393 = 0;
  #line 30 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  _tmp_1841
  = _M0FPB12assert__true(_M0L6_2atmpS1392, _M0L6_2atmpS1393, (moonbit_string_t)moonbit_string_literal_11.data);
  if (_tmp_1841.tag) {
    int32_t const _M0L5_2aokS1394 = _tmp_1841.data.ok;
  } else {
    void* const _M0L6_2aerrS1395 = _tmp_1841.data.err;
    struct moonbit_result_0 _result_1842;
    _result_1842.tag = 0;
    _result_1842.data.err = _M0L6_2aerrS1395;
    return _result_1842;
  }
  #line 31 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  _M0L6_2atmpS1396
  = _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal4pino5LevelE(3, 4);
  _M0L6_2atmpS1397 = 0;
  #line 31 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  _tmp_1843
  = _M0FPB12assert__true(_M0L6_2atmpS1396, _M0L6_2atmpS1397, (moonbit_string_t)moonbit_string_literal_12.data);
  if (_tmp_1843.tag) {
    int32_t const _M0L5_2aokS1398 = _tmp_1843.data.ok;
  } else {
    void* const _M0L6_2aerrS1399 = _tmp_1843.data.err;
    struct moonbit_result_0 _result_1844;
    _result_1844.tag = 0;
    _result_1844.data.err = _M0L6_2aerrS1399;
    return _result_1844;
  }
  #line 32 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  _M0L6_2atmpS1400
  = _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal4pino5LevelE(4, 5);
  _M0L6_2atmpS1401 = 0;
  #line 32 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  return _M0FPB12assert__true(_M0L6_2atmpS1400, _M0L6_2atmpS1401, (moonbit_string_t)moonbit_string_literal_13.data);
}

int32_t _M0IP48clawteam8clawteam8internal4pino5LevelPB7Compare7compare(
  int32_t _M0L9_2ax__148S578,
  int32_t _M0L9_2ax__149S579
) {
  #line 17 "E:\\moonbit\\clawteam\\internal\\pino\\level.mbt"
  switch (_M0L9_2ax__148S578) {
    case 0: {
      switch (_M0L9_2ax__149S579) {
        case 0: {
          return 0;
          break;
        }
        default: {
          return -1;
          break;
        }
      }
      break;
    }
    
    case 1: {
      switch (_M0L9_2ax__149S579) {
        case 0: {
          return 1;
          break;
        }
        
        case 1: {
          return 0;
          break;
        }
        default: {
          return -1;
          break;
        }
      }
      break;
    }
    
    case 2: {
      switch (_M0L9_2ax__149S579) {
        case 0: {
          return 1;
          break;
        }
        
        case 1: {
          return 1;
          break;
        }
        
        case 2: {
          return 0;
          break;
        }
        default: {
          return -1;
          break;
        }
      }
      break;
    }
    
    case 3: {
      switch (_M0L9_2ax__149S579) {
        case 0: {
          return 1;
          break;
        }
        
        case 1: {
          return 1;
          break;
        }
        
        case 2: {
          return 1;
          break;
        }
        
        case 3: {
          return 0;
          break;
        }
        default: {
          return -1;
          break;
        }
      }
      break;
    }
    
    case 4: {
      switch (_M0L9_2ax__149S579) {
        case 0: {
          return 1;
          break;
        }
        
        case 1: {
          return 1;
          break;
        }
        
        case 2: {
          return 1;
          break;
        }
        
        case 3: {
          return 1;
          break;
        }
        
        case 4: {
          return 0;
          break;
        }
        default: {
          return -1;
          break;
        }
      }
      break;
    }
    default: {
      switch (_M0L9_2ax__149S579) {
        case 0: {
          return 1;
          break;
        }
        
        case 1: {
          return 1;
          break;
        }
        
        case 2: {
          return 1;
          break;
        }
        
        case 3: {
          return 1;
          break;
        }
        
        case 4: {
          return 1;
          break;
        }
        default: {
          return 0;
          break;
        }
      }
      break;
    }
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS576,
  struct _M0TPB6Logger _M0L6loggerS577
) {
  moonbit_string_t _M0L6_2atmpS1383;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1382;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1383 = _M0L4selfS576;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1382 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1383);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS1382, _M0L6loggerS577);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS553,
  struct _M0TPB6Logger _M0L6loggerS575
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS1616;
  struct _M0TPC16string10StringView _M0L3pkgS552;
  moonbit_string_t _M0L7_2adataS554;
  int32_t _M0L8_2astartS555;
  int32_t _M0L6_2atmpS1381;
  int32_t _M0L6_2aendS556;
  int32_t _M0Lm9_2acursorS557;
  int32_t _M0Lm13accept__stateS558;
  int32_t _M0Lm10match__endS559;
  int32_t _M0Lm20match__tag__saver__0S560;
  int32_t _M0Lm6tag__0S561;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS562;
  struct _M0TPC16string10StringView _M0L8_2afieldS1615;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS571;
  void* _M0L8_2afieldS1614;
  int32_t _M0L6_2acntS1760;
  void* _M0L16_2apackage__nameS572;
  struct _M0TPC16string10StringView _M0L8_2afieldS1612;
  struct _M0TPC16string10StringView _M0L8filenameS1358;
  struct _M0TPC16string10StringView _M0L8_2afieldS1611;
  struct _M0TPC16string10StringView _M0L11start__lineS1359;
  struct _M0TPC16string10StringView _M0L8_2afieldS1610;
  struct _M0TPC16string10StringView _M0L13start__columnS1360;
  struct _M0TPC16string10StringView _M0L8_2afieldS1609;
  struct _M0TPC16string10StringView _M0L9end__lineS1361;
  struct _M0TPC16string10StringView _M0L8_2afieldS1608;
  int32_t _M0L6_2acntS1764;
  struct _M0TPC16string10StringView _M0L11end__columnS1362;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS1616
  = (struct _M0TPC16string10StringView){
    _M0L4selfS553->$0_1, _M0L4selfS553->$0_2, _M0L4selfS553->$0_0
  };
  _M0L3pkgS552 = _M0L8_2afieldS1616;
  moonbit_incref(_M0L3pkgS552.$0);
  moonbit_incref(_M0L3pkgS552.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS554 = _M0MPC16string10StringView4data(_M0L3pkgS552);
  moonbit_incref(_M0L3pkgS552.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS555 = _M0MPC16string10StringView13start__offset(_M0L3pkgS552);
  moonbit_incref(_M0L3pkgS552.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1381 = _M0MPC16string10StringView6length(_M0L3pkgS552);
  _M0L6_2aendS556 = _M0L8_2astartS555 + _M0L6_2atmpS1381;
  _M0Lm9_2acursorS557 = _M0L8_2astartS555;
  _M0Lm13accept__stateS558 = -1;
  _M0Lm10match__endS559 = -1;
  _M0Lm20match__tag__saver__0S560 = -1;
  _M0Lm6tag__0S561 = -1;
  while (1) {
    int32_t _M0L6_2atmpS1373 = _M0Lm9_2acursorS557;
    if (_M0L6_2atmpS1373 < _M0L6_2aendS556) {
      int32_t _M0L6_2atmpS1380 = _M0Lm9_2acursorS557;
      int32_t _M0L10next__charS566;
      int32_t _M0L6_2atmpS1374;
      moonbit_incref(_M0L7_2adataS554);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS566
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS554, _M0L6_2atmpS1380);
      _M0L6_2atmpS1374 = _M0Lm9_2acursorS557;
      _M0Lm9_2acursorS557 = _M0L6_2atmpS1374 + 1;
      if (_M0L10next__charS566 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS1375;
          _M0Lm6tag__0S561 = _M0Lm9_2acursorS557;
          _M0L6_2atmpS1375 = _M0Lm9_2acursorS557;
          if (_M0L6_2atmpS1375 < _M0L6_2aendS556) {
            int32_t _M0L6_2atmpS1379 = _M0Lm9_2acursorS557;
            int32_t _M0L10next__charS567;
            int32_t _M0L6_2atmpS1376;
            moonbit_incref(_M0L7_2adataS554);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS567
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS554, _M0L6_2atmpS1379);
            _M0L6_2atmpS1376 = _M0Lm9_2acursorS557;
            _M0Lm9_2acursorS557 = _M0L6_2atmpS1376 + 1;
            if (_M0L10next__charS567 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS1377 = _M0Lm9_2acursorS557;
                if (_M0L6_2atmpS1377 < _M0L6_2aendS556) {
                  int32_t _M0L6_2atmpS1378 = _M0Lm9_2acursorS557;
                  _M0Lm9_2acursorS557 = _M0L6_2atmpS1378 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S560 = _M0Lm6tag__0S561;
                  _M0Lm13accept__stateS558 = 0;
                  _M0Lm10match__endS559 = _M0Lm9_2acursorS557;
                  goto join_563;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_563;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_563;
    }
    break;
  }
  goto joinlet_1845;
  join_563:;
  switch (_M0Lm13accept__stateS558) {
    case 0: {
      int32_t _M0L6_2atmpS1371;
      int32_t _M0L6_2atmpS1370;
      int64_t _M0L6_2atmpS1367;
      int32_t _M0L6_2atmpS1369;
      int64_t _M0L6_2atmpS1368;
      struct _M0TPC16string10StringView _M0L13package__nameS564;
      int64_t _M0L6_2atmpS1364;
      int32_t _M0L6_2atmpS1366;
      int64_t _M0L6_2atmpS1365;
      struct _M0TPC16string10StringView _M0L12module__nameS565;
      void* _M0L4SomeS1363;
      moonbit_decref(_M0L3pkgS552.$0);
      _M0L6_2atmpS1371 = _M0Lm20match__tag__saver__0S560;
      _M0L6_2atmpS1370 = _M0L6_2atmpS1371 + 1;
      _M0L6_2atmpS1367 = (int64_t)_M0L6_2atmpS1370;
      _M0L6_2atmpS1369 = _M0Lm10match__endS559;
      _M0L6_2atmpS1368 = (int64_t)_M0L6_2atmpS1369;
      moonbit_incref(_M0L7_2adataS554);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS564
      = _M0MPC16string6String4view(_M0L7_2adataS554, _M0L6_2atmpS1367, _M0L6_2atmpS1368);
      _M0L6_2atmpS1364 = (int64_t)_M0L8_2astartS555;
      _M0L6_2atmpS1366 = _M0Lm20match__tag__saver__0S560;
      _M0L6_2atmpS1365 = (int64_t)_M0L6_2atmpS1366;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS565
      = _M0MPC16string6String4view(_M0L7_2adataS554, _M0L6_2atmpS1364, _M0L6_2atmpS1365);
      _M0L4SomeS1363
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS1363)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1363)->$0_0
      = _M0L13package__nameS564.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1363)->$0_1
      = _M0L13package__nameS564.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1363)->$0_2
      = _M0L13package__nameS564.$2;
      _M0L7_2abindS562
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS562)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS562->$0_0 = _M0L12module__nameS565.$0;
      _M0L7_2abindS562->$0_1 = _M0L12module__nameS565.$1;
      _M0L7_2abindS562->$0_2 = _M0L12module__nameS565.$2;
      _M0L7_2abindS562->$1 = _M0L4SomeS1363;
      break;
    }
    default: {
      void* _M0L4NoneS1372;
      moonbit_decref(_M0L7_2adataS554);
      _M0L4NoneS1372
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS562
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS562)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS562->$0_0 = _M0L3pkgS552.$0;
      _M0L7_2abindS562->$0_1 = _M0L3pkgS552.$1;
      _M0L7_2abindS562->$0_2 = _M0L3pkgS552.$2;
      _M0L7_2abindS562->$1 = _M0L4NoneS1372;
      break;
    }
  }
  joinlet_1845:;
  _M0L8_2afieldS1615
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS562->$0_1, _M0L7_2abindS562->$0_2, _M0L7_2abindS562->$0_0
  };
  _M0L15_2amodule__nameS571 = _M0L8_2afieldS1615;
  _M0L8_2afieldS1614 = _M0L7_2abindS562->$1;
  _M0L6_2acntS1760 = Moonbit_object_header(_M0L7_2abindS562)->rc;
  if (_M0L6_2acntS1760 > 1) {
    int32_t _M0L11_2anew__cntS1761 = _M0L6_2acntS1760 - 1;
    Moonbit_object_header(_M0L7_2abindS562)->rc = _M0L11_2anew__cntS1761;
    moonbit_incref(_M0L8_2afieldS1614);
    moonbit_incref(_M0L15_2amodule__nameS571.$0);
  } else if (_M0L6_2acntS1760 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS562);
  }
  _M0L16_2apackage__nameS572 = _M0L8_2afieldS1614;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS572)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS573 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS572;
      struct _M0TPC16string10StringView _M0L8_2afieldS1613 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS573->$0_1,
                                              _M0L7_2aSomeS573->$0_2,
                                              _M0L7_2aSomeS573->$0_0};
      int32_t _M0L6_2acntS1762 = Moonbit_object_header(_M0L7_2aSomeS573)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS574;
      if (_M0L6_2acntS1762 > 1) {
        int32_t _M0L11_2anew__cntS1763 = _M0L6_2acntS1762 - 1;
        Moonbit_object_header(_M0L7_2aSomeS573)->rc = _M0L11_2anew__cntS1763;
        moonbit_incref(_M0L8_2afieldS1613.$0);
      } else if (_M0L6_2acntS1762 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS573);
      }
      _M0L12_2apkg__nameS574 = _M0L8_2afieldS1613;
      if (_M0L6loggerS575.$1) {
        moonbit_incref(_M0L6loggerS575.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS575.$0->$method_2(_M0L6loggerS575.$1, _M0L12_2apkg__nameS574);
      if (_M0L6loggerS575.$1) {
        moonbit_incref(_M0L6loggerS575.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS575.$0->$method_3(_M0L6loggerS575.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS572);
      break;
    }
  }
  _M0L8_2afieldS1612
  = (struct _M0TPC16string10StringView){
    _M0L4selfS553->$1_1, _M0L4selfS553->$1_2, _M0L4selfS553->$1_0
  };
  _M0L8filenameS1358 = _M0L8_2afieldS1612;
  moonbit_incref(_M0L8filenameS1358.$0);
  if (_M0L6loggerS575.$1) {
    moonbit_incref(_M0L6loggerS575.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS575.$0->$method_2(_M0L6loggerS575.$1, _M0L8filenameS1358);
  if (_M0L6loggerS575.$1) {
    moonbit_incref(_M0L6loggerS575.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS575.$0->$method_3(_M0L6loggerS575.$1, 58);
  _M0L8_2afieldS1611
  = (struct _M0TPC16string10StringView){
    _M0L4selfS553->$2_1, _M0L4selfS553->$2_2, _M0L4selfS553->$2_0
  };
  _M0L11start__lineS1359 = _M0L8_2afieldS1611;
  moonbit_incref(_M0L11start__lineS1359.$0);
  if (_M0L6loggerS575.$1) {
    moonbit_incref(_M0L6loggerS575.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS575.$0->$method_2(_M0L6loggerS575.$1, _M0L11start__lineS1359);
  if (_M0L6loggerS575.$1) {
    moonbit_incref(_M0L6loggerS575.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS575.$0->$method_3(_M0L6loggerS575.$1, 58);
  _M0L8_2afieldS1610
  = (struct _M0TPC16string10StringView){
    _M0L4selfS553->$3_1, _M0L4selfS553->$3_2, _M0L4selfS553->$3_0
  };
  _M0L13start__columnS1360 = _M0L8_2afieldS1610;
  moonbit_incref(_M0L13start__columnS1360.$0);
  if (_M0L6loggerS575.$1) {
    moonbit_incref(_M0L6loggerS575.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS575.$0->$method_2(_M0L6loggerS575.$1, _M0L13start__columnS1360);
  if (_M0L6loggerS575.$1) {
    moonbit_incref(_M0L6loggerS575.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS575.$0->$method_3(_M0L6loggerS575.$1, 45);
  _M0L8_2afieldS1609
  = (struct _M0TPC16string10StringView){
    _M0L4selfS553->$4_1, _M0L4selfS553->$4_2, _M0L4selfS553->$4_0
  };
  _M0L9end__lineS1361 = _M0L8_2afieldS1609;
  moonbit_incref(_M0L9end__lineS1361.$0);
  if (_M0L6loggerS575.$1) {
    moonbit_incref(_M0L6loggerS575.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS575.$0->$method_2(_M0L6loggerS575.$1, _M0L9end__lineS1361);
  if (_M0L6loggerS575.$1) {
    moonbit_incref(_M0L6loggerS575.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS575.$0->$method_3(_M0L6loggerS575.$1, 58);
  _M0L8_2afieldS1608
  = (struct _M0TPC16string10StringView){
    _M0L4selfS553->$5_1, _M0L4selfS553->$5_2, _M0L4selfS553->$5_0
  };
  _M0L6_2acntS1764 = Moonbit_object_header(_M0L4selfS553)->rc;
  if (_M0L6_2acntS1764 > 1) {
    int32_t _M0L11_2anew__cntS1770 = _M0L6_2acntS1764 - 1;
    Moonbit_object_header(_M0L4selfS553)->rc = _M0L11_2anew__cntS1770;
    moonbit_incref(_M0L8_2afieldS1608.$0);
  } else if (_M0L6_2acntS1764 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS1769 =
      (struct _M0TPC16string10StringView){_M0L4selfS553->$4_1,
                                            _M0L4selfS553->$4_2,
                                            _M0L4selfS553->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS1768;
    struct _M0TPC16string10StringView _M0L8_2afieldS1767;
    struct _M0TPC16string10StringView _M0L8_2afieldS1766;
    struct _M0TPC16string10StringView _M0L8_2afieldS1765;
    moonbit_decref(_M0L8_2afieldS1769.$0);
    _M0L8_2afieldS1768
    = (struct _M0TPC16string10StringView){
      _M0L4selfS553->$3_1, _M0L4selfS553->$3_2, _M0L4selfS553->$3_0
    };
    moonbit_decref(_M0L8_2afieldS1768.$0);
    _M0L8_2afieldS1767
    = (struct _M0TPC16string10StringView){
      _M0L4selfS553->$2_1, _M0L4selfS553->$2_2, _M0L4selfS553->$2_0
    };
    moonbit_decref(_M0L8_2afieldS1767.$0);
    _M0L8_2afieldS1766
    = (struct _M0TPC16string10StringView){
      _M0L4selfS553->$1_1, _M0L4selfS553->$1_2, _M0L4selfS553->$1_0
    };
    moonbit_decref(_M0L8_2afieldS1766.$0);
    _M0L8_2afieldS1765
    = (struct _M0TPC16string10StringView){
      _M0L4selfS553->$0_1, _M0L4selfS553->$0_2, _M0L4selfS553->$0_0
    };
    moonbit_decref(_M0L8_2afieldS1765.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS553);
  }
  _M0L11end__columnS1362 = _M0L8_2afieldS1608;
  if (_M0L6loggerS575.$1) {
    moonbit_incref(_M0L6loggerS575.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS575.$0->$method_2(_M0L6loggerS575.$1, _M0L11end__columnS1362);
  if (_M0L6loggerS575.$1) {
    moonbit_incref(_M0L6loggerS575.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS575.$0->$method_3(_M0L6loggerS575.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS575.$0->$method_2(_M0L6loggerS575.$1, _M0L15_2amodule__nameS571);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS551) {
  moonbit_string_t _M0L6_2atmpS1357;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS1357 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS551);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS1357);
  moonbit_decref(_M0L6_2atmpS1357);
  return 0;
}

struct moonbit_result_0 _M0FPB12assert__true(
  int32_t _M0L1xS546,
  moonbit_string_t _M0L3msgS548,
  moonbit_string_t _M0L3locS550
) {
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  if (!_M0L1xS546) {
    moonbit_string_t _M0L9fail__msgS547;
    if (_M0L3msgS548 == 0) {
      moonbit_string_t _M0L6_2atmpS1355;
      moonbit_string_t _M0L6_2atmpS1618;
      moonbit_string_t _M0L6_2atmpS1354;
      moonbit_string_t _M0L6_2atmpS1617;
      if (_M0L3msgS548) {
        moonbit_decref(_M0L3msgS548);
      }
      #line 127 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS1355
      = _M0IP016_24default__implPB4Show10to__stringGbE(_M0L1xS546);
      #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS1618
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_14.data, _M0L6_2atmpS1355);
      moonbit_decref(_M0L6_2atmpS1355);
      _M0L6_2atmpS1354 = _M0L6_2atmpS1618;
      #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS1617
      = moonbit_add_string(_M0L6_2atmpS1354, (moonbit_string_t)moonbit_string_literal_15.data);
      moonbit_decref(_M0L6_2atmpS1354);
      _M0L9fail__msgS547 = _M0L6_2atmpS1617;
    } else {
      moonbit_string_t _M0L7_2aSomeS549 = _M0L3msgS548;
      _M0L9fail__msgS547 = _M0L7_2aSomeS549;
    }
    #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS547, _M0L3locS550);
  } else {
    int32_t _M0L6_2atmpS1356;
    struct moonbit_result_0 _result_1849;
    moonbit_decref(_M0L3locS550);
    if (_M0L3msgS548) {
      moonbit_decref(_M0L3msgS548);
    }
    _M0L6_2atmpS1356 = 0;
    _result_1849.tag = 1;
    _result_1849.data.ok = _M0L6_2atmpS1356;
    return _result_1849;
  }
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS545,
  struct _M0TPB6Hasher* _M0L6hasherS544
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS544, _M0L4selfS545);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS543,
  struct _M0TPB6Hasher* _M0L6hasherS542
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS542, _M0L4selfS543);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS540,
  moonbit_string_t _M0L5valueS538
) {
  int32_t _M0L7_2abindS537;
  int32_t _M0L1iS539;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS537 = Moonbit_array_length(_M0L5valueS538);
  _M0L1iS539 = 0;
  while (1) {
    if (_M0L1iS539 < _M0L7_2abindS537) {
      int32_t _M0L6_2atmpS1352 = _M0L5valueS538[_M0L1iS539];
      int32_t _M0L6_2atmpS1351 = (int32_t)_M0L6_2atmpS1352;
      uint32_t _M0L6_2atmpS1350 = *(uint32_t*)&_M0L6_2atmpS1351;
      int32_t _M0L6_2atmpS1353;
      moonbit_incref(_M0L4selfS540);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS540, _M0L6_2atmpS1350);
      _M0L6_2atmpS1353 = _M0L1iS539 + 1;
      _M0L1iS539 = _M0L6_2atmpS1353;
      continue;
    } else {
      moonbit_decref(_M0L4selfS540);
      moonbit_decref(_M0L5valueS538);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS535,
  int32_t _M0L3idxS536
) {
  int32_t _M0L6_2atmpS1619;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1619 = _M0L4selfS535[_M0L3idxS536];
  moonbit_decref(_M0L4selfS535);
  return _M0L6_2atmpS1619;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS522,
  int32_t _M0L3keyS518
) {
  int32_t _M0L4hashS517;
  int32_t _M0L14capacity__maskS1335;
  int32_t _M0L6_2atmpS1334;
  int32_t _M0L1iS519;
  int32_t _M0L3idxS520;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS517 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS518);
  _M0L14capacity__maskS1335 = _M0L4selfS522->$3;
  _M0L6_2atmpS1334 = _M0L4hashS517 & _M0L14capacity__maskS1335;
  _M0L1iS519 = 0;
  _M0L3idxS520 = _M0L6_2atmpS1334;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1623 =
      _M0L4selfS522->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1333 =
      _M0L8_2afieldS1623;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1622;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS521;
    if (
      _M0L3idxS520 < 0
      || _M0L3idxS520 >= Moonbit_array_length(_M0L7entriesS1333)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1622
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1333[
        _M0L3idxS520
      ];
    _M0L7_2abindS521 = _M0L6_2atmpS1622;
    if (_M0L7_2abindS521 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1322;
      if (_M0L7_2abindS521) {
        moonbit_incref(_M0L7_2abindS521);
      }
      moonbit_decref(_M0L4selfS522);
      if (_M0L7_2abindS521) {
        moonbit_decref(_M0L7_2abindS521);
      }
      _M0L6_2atmpS1322 = 0;
      return _M0L6_2atmpS1322;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS523 =
        _M0L7_2abindS521;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS524 =
        _M0L7_2aSomeS523;
      int32_t _M0L4hashS1324 = _M0L8_2aentryS524->$3;
      int32_t _if__result_1852;
      int32_t _M0L8_2afieldS1620;
      int32_t _M0L3pslS1327;
      int32_t _M0L6_2atmpS1329;
      int32_t _M0L6_2atmpS1331;
      int32_t _M0L14capacity__maskS1332;
      int32_t _M0L6_2atmpS1330;
      if (_M0L4hashS1324 == _M0L4hashS517) {
        int32_t _M0L3keyS1323 = _M0L8_2aentryS524->$4;
        _if__result_1852 = _M0L3keyS1323 == _M0L3keyS518;
      } else {
        _if__result_1852 = 0;
      }
      if (_if__result_1852) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1621;
        int32_t _M0L6_2acntS1771;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1326;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1325;
        moonbit_incref(_M0L8_2aentryS524);
        moonbit_decref(_M0L4selfS522);
        _M0L8_2afieldS1621 = _M0L8_2aentryS524->$5;
        _M0L6_2acntS1771 = Moonbit_object_header(_M0L8_2aentryS524)->rc;
        if (_M0L6_2acntS1771 > 1) {
          int32_t _M0L11_2anew__cntS1773 = _M0L6_2acntS1771 - 1;
          Moonbit_object_header(_M0L8_2aentryS524)->rc
          = _M0L11_2anew__cntS1773;
          moonbit_incref(_M0L8_2afieldS1621);
        } else if (_M0L6_2acntS1771 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1772 =
            _M0L8_2aentryS524->$1;
          if (_M0L8_2afieldS1772) {
            moonbit_decref(_M0L8_2afieldS1772);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS524);
        }
        _M0L5valueS1326 = _M0L8_2afieldS1621;
        _M0L6_2atmpS1325 = _M0L5valueS1326;
        return _M0L6_2atmpS1325;
      } else {
        moonbit_incref(_M0L8_2aentryS524);
      }
      _M0L8_2afieldS1620 = _M0L8_2aentryS524->$2;
      moonbit_decref(_M0L8_2aentryS524);
      _M0L3pslS1327 = _M0L8_2afieldS1620;
      if (_M0L1iS519 > _M0L3pslS1327) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1328;
        moonbit_decref(_M0L4selfS522);
        _M0L6_2atmpS1328 = 0;
        return _M0L6_2atmpS1328;
      }
      _M0L6_2atmpS1329 = _M0L1iS519 + 1;
      _M0L6_2atmpS1331 = _M0L3idxS520 + 1;
      _M0L14capacity__maskS1332 = _M0L4selfS522->$3;
      _M0L6_2atmpS1330 = _M0L6_2atmpS1331 & _M0L14capacity__maskS1332;
      _M0L1iS519 = _M0L6_2atmpS1329;
      _M0L3idxS520 = _M0L6_2atmpS1330;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS531,
  moonbit_string_t _M0L3keyS527
) {
  int32_t _M0L4hashS526;
  int32_t _M0L14capacity__maskS1349;
  int32_t _M0L6_2atmpS1348;
  int32_t _M0L1iS528;
  int32_t _M0L3idxS529;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS527);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS526 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS527);
  _M0L14capacity__maskS1349 = _M0L4selfS531->$3;
  _M0L6_2atmpS1348 = _M0L4hashS526 & _M0L14capacity__maskS1349;
  _M0L1iS528 = 0;
  _M0L3idxS529 = _M0L6_2atmpS1348;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1629 =
      _M0L4selfS531->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1347 =
      _M0L8_2afieldS1629;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1628;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS530;
    if (
      _M0L3idxS529 < 0
      || _M0L3idxS529 >= Moonbit_array_length(_M0L7entriesS1347)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1628
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1347[
        _M0L3idxS529
      ];
    _M0L7_2abindS530 = _M0L6_2atmpS1628;
    if (_M0L7_2abindS530 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1336;
      if (_M0L7_2abindS530) {
        moonbit_incref(_M0L7_2abindS530);
      }
      moonbit_decref(_M0L4selfS531);
      if (_M0L7_2abindS530) {
        moonbit_decref(_M0L7_2abindS530);
      }
      moonbit_decref(_M0L3keyS527);
      _M0L6_2atmpS1336 = 0;
      return _M0L6_2atmpS1336;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS532 =
        _M0L7_2abindS530;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS533 =
        _M0L7_2aSomeS532;
      int32_t _M0L4hashS1338 = _M0L8_2aentryS533->$3;
      int32_t _if__result_1854;
      int32_t _M0L8_2afieldS1624;
      int32_t _M0L3pslS1341;
      int32_t _M0L6_2atmpS1343;
      int32_t _M0L6_2atmpS1345;
      int32_t _M0L14capacity__maskS1346;
      int32_t _M0L6_2atmpS1344;
      if (_M0L4hashS1338 == _M0L4hashS526) {
        moonbit_string_t _M0L8_2afieldS1627 = _M0L8_2aentryS533->$4;
        moonbit_string_t _M0L3keyS1337 = _M0L8_2afieldS1627;
        int32_t _M0L6_2atmpS1626;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS1626
        = moonbit_val_array_equal(_M0L3keyS1337, _M0L3keyS527);
        _if__result_1854 = _M0L6_2atmpS1626;
      } else {
        _if__result_1854 = 0;
      }
      if (_if__result_1854) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1625;
        int32_t _M0L6_2acntS1774;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS1340;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1339;
        moonbit_incref(_M0L8_2aentryS533);
        moonbit_decref(_M0L4selfS531);
        moonbit_decref(_M0L3keyS527);
        _M0L8_2afieldS1625 = _M0L8_2aentryS533->$5;
        _M0L6_2acntS1774 = Moonbit_object_header(_M0L8_2aentryS533)->rc;
        if (_M0L6_2acntS1774 > 1) {
          int32_t _M0L11_2anew__cntS1777 = _M0L6_2acntS1774 - 1;
          Moonbit_object_header(_M0L8_2aentryS533)->rc
          = _M0L11_2anew__cntS1777;
          moonbit_incref(_M0L8_2afieldS1625);
        } else if (_M0L6_2acntS1774 == 1) {
          moonbit_string_t _M0L8_2afieldS1776 = _M0L8_2aentryS533->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1775;
          moonbit_decref(_M0L8_2afieldS1776);
          _M0L8_2afieldS1775 = _M0L8_2aentryS533->$1;
          if (_M0L8_2afieldS1775) {
            moonbit_decref(_M0L8_2afieldS1775);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS533);
        }
        _M0L5valueS1340 = _M0L8_2afieldS1625;
        _M0L6_2atmpS1339 = _M0L5valueS1340;
        return _M0L6_2atmpS1339;
      } else {
        moonbit_incref(_M0L8_2aentryS533);
      }
      _M0L8_2afieldS1624 = _M0L8_2aentryS533->$2;
      moonbit_decref(_M0L8_2aentryS533);
      _M0L3pslS1341 = _M0L8_2afieldS1624;
      if (_M0L1iS528 > _M0L3pslS1341) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1342;
        moonbit_decref(_M0L4selfS531);
        moonbit_decref(_M0L3keyS527);
        _M0L6_2atmpS1342 = 0;
        return _M0L6_2atmpS1342;
      }
      _M0L6_2atmpS1343 = _M0L1iS528 + 1;
      _M0L6_2atmpS1345 = _M0L3idxS529 + 1;
      _M0L14capacity__maskS1346 = _M0L4selfS531->$3;
      _M0L6_2atmpS1344 = _M0L6_2atmpS1345 & _M0L14capacity__maskS1346;
      _M0L1iS528 = _M0L6_2atmpS1343;
      _M0L3idxS529 = _M0L6_2atmpS1344;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS502
) {
  int32_t _M0L6lengthS501;
  int32_t _M0Lm8capacityS503;
  int32_t _M0L6_2atmpS1299;
  int32_t _M0L6_2atmpS1298;
  int32_t _M0L6_2atmpS1309;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS504;
  int32_t _M0L3endS1307;
  int32_t _M0L5startS1308;
  int32_t _M0L7_2abindS505;
  int32_t _M0L2__S506;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS502.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS501
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS502);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS503 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS501);
  _M0L6_2atmpS1299 = _M0Lm8capacityS503;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1298 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1299);
  if (_M0L6lengthS501 > _M0L6_2atmpS1298) {
    int32_t _M0L6_2atmpS1300 = _M0Lm8capacityS503;
    _M0Lm8capacityS503 = _M0L6_2atmpS1300 * 2;
  }
  _M0L6_2atmpS1309 = _M0Lm8capacityS503;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS504
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1309);
  _M0L3endS1307 = _M0L3arrS502.$2;
  _M0L5startS1308 = _M0L3arrS502.$1;
  _M0L7_2abindS505 = _M0L3endS1307 - _M0L5startS1308;
  _M0L2__S506 = 0;
  while (1) {
    if (_M0L2__S506 < _M0L7_2abindS505) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1633 =
        _M0L3arrS502.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1304 =
        _M0L8_2afieldS1633;
      int32_t _M0L5startS1306 = _M0L3arrS502.$1;
      int32_t _M0L6_2atmpS1305 = _M0L5startS1306 + _M0L2__S506;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1632 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1304[
          _M0L6_2atmpS1305
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS507 =
        _M0L6_2atmpS1632;
      moonbit_string_t _M0L8_2afieldS1631 = _M0L1eS507->$0;
      moonbit_string_t _M0L6_2atmpS1301 = _M0L8_2afieldS1631;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1630 =
        _M0L1eS507->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1302 =
        _M0L8_2afieldS1630;
      int32_t _M0L6_2atmpS1303;
      moonbit_incref(_M0L6_2atmpS1302);
      moonbit_incref(_M0L6_2atmpS1301);
      moonbit_incref(_M0L1mS504);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS504, _M0L6_2atmpS1301, _M0L6_2atmpS1302);
      _M0L6_2atmpS1303 = _M0L2__S506 + 1;
      _M0L2__S506 = _M0L6_2atmpS1303;
      continue;
    } else {
      moonbit_decref(_M0L3arrS502.$0);
    }
    break;
  }
  return _M0L1mS504;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS510
) {
  int32_t _M0L6lengthS509;
  int32_t _M0Lm8capacityS511;
  int32_t _M0L6_2atmpS1311;
  int32_t _M0L6_2atmpS1310;
  int32_t _M0L6_2atmpS1321;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS512;
  int32_t _M0L3endS1319;
  int32_t _M0L5startS1320;
  int32_t _M0L7_2abindS513;
  int32_t _M0L2__S514;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS510.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS509
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS510);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS511 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS509);
  _M0L6_2atmpS1311 = _M0Lm8capacityS511;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1310 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1311);
  if (_M0L6lengthS509 > _M0L6_2atmpS1310) {
    int32_t _M0L6_2atmpS1312 = _M0Lm8capacityS511;
    _M0Lm8capacityS511 = _M0L6_2atmpS1312 * 2;
  }
  _M0L6_2atmpS1321 = _M0Lm8capacityS511;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS512
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1321);
  _M0L3endS1319 = _M0L3arrS510.$2;
  _M0L5startS1320 = _M0L3arrS510.$1;
  _M0L7_2abindS513 = _M0L3endS1319 - _M0L5startS1320;
  _M0L2__S514 = 0;
  while (1) {
    if (_M0L2__S514 < _M0L7_2abindS513) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1636 =
        _M0L3arrS510.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1316 =
        _M0L8_2afieldS1636;
      int32_t _M0L5startS1318 = _M0L3arrS510.$1;
      int32_t _M0L6_2atmpS1317 = _M0L5startS1318 + _M0L2__S514;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1635 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1316[
          _M0L6_2atmpS1317
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS515 = _M0L6_2atmpS1635;
      int32_t _M0L6_2atmpS1313 = _M0L1eS515->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1634 =
        _M0L1eS515->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1314 =
        _M0L8_2afieldS1634;
      int32_t _M0L6_2atmpS1315;
      moonbit_incref(_M0L6_2atmpS1314);
      moonbit_incref(_M0L1mS512);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS512, _M0L6_2atmpS1313, _M0L6_2atmpS1314);
      _M0L6_2atmpS1315 = _M0L2__S514 + 1;
      _M0L2__S514 = _M0L6_2atmpS1315;
      continue;
    } else {
      moonbit_decref(_M0L3arrS510.$0);
    }
    break;
  }
  return _M0L1mS512;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS495,
  moonbit_string_t _M0L3keyS496,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS497
) {
  int32_t _M0L6_2atmpS1296;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS496);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1296 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS496);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS495, _M0L3keyS496, _M0L5valueS497, _M0L6_2atmpS1296);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS498,
  int32_t _M0L3keyS499,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS500
) {
  int32_t _M0L6_2atmpS1297;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1297 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS499);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS498, _M0L3keyS499, _M0L5valueS500, _M0L6_2atmpS1297);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS474
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1643;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS473;
  int32_t _M0L8capacityS1288;
  int32_t _M0L13new__capacityS475;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1283;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1282;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS1642;
  int32_t _M0L6_2atmpS1284;
  int32_t _M0L8capacityS1286;
  int32_t _M0L6_2atmpS1285;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1287;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1641;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS476;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1643 = _M0L4selfS474->$5;
  _M0L9old__headS473 = _M0L8_2afieldS1643;
  _M0L8capacityS1288 = _M0L4selfS474->$2;
  _M0L13new__capacityS475 = _M0L8capacityS1288 << 1;
  _M0L6_2atmpS1283 = 0;
  _M0L6_2atmpS1282
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS475, _M0L6_2atmpS1283);
  _M0L6_2aoldS1642 = _M0L4selfS474->$0;
  if (_M0L9old__headS473) {
    moonbit_incref(_M0L9old__headS473);
  }
  moonbit_decref(_M0L6_2aoldS1642);
  _M0L4selfS474->$0 = _M0L6_2atmpS1282;
  _M0L4selfS474->$2 = _M0L13new__capacityS475;
  _M0L6_2atmpS1284 = _M0L13new__capacityS475 - 1;
  _M0L4selfS474->$3 = _M0L6_2atmpS1284;
  _M0L8capacityS1286 = _M0L4selfS474->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1285 = _M0FPB21calc__grow__threshold(_M0L8capacityS1286);
  _M0L4selfS474->$4 = _M0L6_2atmpS1285;
  _M0L4selfS474->$1 = 0;
  _M0L6_2atmpS1287 = 0;
  _M0L6_2aoldS1641 = _M0L4selfS474->$5;
  if (_M0L6_2aoldS1641) {
    moonbit_decref(_M0L6_2aoldS1641);
  }
  _M0L4selfS474->$5 = _M0L6_2atmpS1287;
  _M0L4selfS474->$6 = -1;
  _M0L8_2aparamS476 = _M0L9old__headS473;
  while (1) {
    if (_M0L8_2aparamS476 == 0) {
      if (_M0L8_2aparamS476) {
        moonbit_decref(_M0L8_2aparamS476);
      }
      moonbit_decref(_M0L4selfS474);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS477 =
        _M0L8_2aparamS476;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS478 =
        _M0L7_2aSomeS477;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1640 =
        _M0L4_2axS478->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS479 =
        _M0L8_2afieldS1640;
      moonbit_string_t _M0L8_2afieldS1639 = _M0L4_2axS478->$4;
      moonbit_string_t _M0L6_2akeyS480 = _M0L8_2afieldS1639;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1638 =
        _M0L4_2axS478->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS481 =
        _M0L8_2afieldS1638;
      int32_t _M0L8_2afieldS1637 = _M0L4_2axS478->$3;
      int32_t _M0L6_2acntS1778 = Moonbit_object_header(_M0L4_2axS478)->rc;
      int32_t _M0L7_2ahashS482;
      if (_M0L6_2acntS1778 > 1) {
        int32_t _M0L11_2anew__cntS1779 = _M0L6_2acntS1778 - 1;
        Moonbit_object_header(_M0L4_2axS478)->rc = _M0L11_2anew__cntS1779;
        moonbit_incref(_M0L8_2avalueS481);
        moonbit_incref(_M0L6_2akeyS480);
        if (_M0L7_2anextS479) {
          moonbit_incref(_M0L7_2anextS479);
        }
      } else if (_M0L6_2acntS1778 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS478);
      }
      _M0L7_2ahashS482 = _M0L8_2afieldS1637;
      moonbit_incref(_M0L4selfS474);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS474, _M0L6_2akeyS480, _M0L8_2avalueS481, _M0L7_2ahashS482);
      _M0L8_2aparamS476 = _M0L7_2anextS479;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS485
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1649;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS484;
  int32_t _M0L8capacityS1295;
  int32_t _M0L13new__capacityS486;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1290;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1289;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS1648;
  int32_t _M0L6_2atmpS1291;
  int32_t _M0L8capacityS1293;
  int32_t _M0L6_2atmpS1292;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1294;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1647;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS487;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1649 = _M0L4selfS485->$5;
  _M0L9old__headS484 = _M0L8_2afieldS1649;
  _M0L8capacityS1295 = _M0L4selfS485->$2;
  _M0L13new__capacityS486 = _M0L8capacityS1295 << 1;
  _M0L6_2atmpS1290 = 0;
  _M0L6_2atmpS1289
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS486, _M0L6_2atmpS1290);
  _M0L6_2aoldS1648 = _M0L4selfS485->$0;
  if (_M0L9old__headS484) {
    moonbit_incref(_M0L9old__headS484);
  }
  moonbit_decref(_M0L6_2aoldS1648);
  _M0L4selfS485->$0 = _M0L6_2atmpS1289;
  _M0L4selfS485->$2 = _M0L13new__capacityS486;
  _M0L6_2atmpS1291 = _M0L13new__capacityS486 - 1;
  _M0L4selfS485->$3 = _M0L6_2atmpS1291;
  _M0L8capacityS1293 = _M0L4selfS485->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1292 = _M0FPB21calc__grow__threshold(_M0L8capacityS1293);
  _M0L4selfS485->$4 = _M0L6_2atmpS1292;
  _M0L4selfS485->$1 = 0;
  _M0L6_2atmpS1294 = 0;
  _M0L6_2aoldS1647 = _M0L4selfS485->$5;
  if (_M0L6_2aoldS1647) {
    moonbit_decref(_M0L6_2aoldS1647);
  }
  _M0L4selfS485->$5 = _M0L6_2atmpS1294;
  _M0L4selfS485->$6 = -1;
  _M0L8_2aparamS487 = _M0L9old__headS484;
  while (1) {
    if (_M0L8_2aparamS487 == 0) {
      if (_M0L8_2aparamS487) {
        moonbit_decref(_M0L8_2aparamS487);
      }
      moonbit_decref(_M0L4selfS485);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS488 =
        _M0L8_2aparamS487;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS489 =
        _M0L7_2aSomeS488;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1646 =
        _M0L4_2axS489->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS490 =
        _M0L8_2afieldS1646;
      int32_t _M0L6_2akeyS491 = _M0L4_2axS489->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1645 =
        _M0L4_2axS489->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS492 =
        _M0L8_2afieldS1645;
      int32_t _M0L8_2afieldS1644 = _M0L4_2axS489->$3;
      int32_t _M0L6_2acntS1780 = Moonbit_object_header(_M0L4_2axS489)->rc;
      int32_t _M0L7_2ahashS493;
      if (_M0L6_2acntS1780 > 1) {
        int32_t _M0L11_2anew__cntS1781 = _M0L6_2acntS1780 - 1;
        Moonbit_object_header(_M0L4_2axS489)->rc = _M0L11_2anew__cntS1781;
        moonbit_incref(_M0L8_2avalueS492);
        if (_M0L7_2anextS490) {
          moonbit_incref(_M0L7_2anextS490);
        }
      } else if (_M0L6_2acntS1780 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS489);
      }
      _M0L7_2ahashS493 = _M0L8_2afieldS1644;
      moonbit_incref(_M0L4selfS485);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS485, _M0L6_2akeyS491, _M0L8_2avalueS492, _M0L7_2ahashS493);
      _M0L8_2aparamS487 = _M0L7_2anextS490;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS444,
  moonbit_string_t _M0L3keyS450,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS451,
  int32_t _M0L4hashS446
) {
  int32_t _M0L14capacity__maskS1263;
  int32_t _M0L6_2atmpS1262;
  int32_t _M0L3pslS441;
  int32_t _M0L3idxS442;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1263 = _M0L4selfS444->$3;
  _M0L6_2atmpS1262 = _M0L4hashS446 & _M0L14capacity__maskS1263;
  _M0L3pslS441 = 0;
  _M0L3idxS442 = _M0L6_2atmpS1262;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1654 =
      _M0L4selfS444->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1261 =
      _M0L8_2afieldS1654;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1653;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS443;
    if (
      _M0L3idxS442 < 0
      || _M0L3idxS442 >= Moonbit_array_length(_M0L7entriesS1261)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1653
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1261[
        _M0L3idxS442
      ];
    _M0L7_2abindS443 = _M0L6_2atmpS1653;
    if (_M0L7_2abindS443 == 0) {
      int32_t _M0L4sizeS1246 = _M0L4selfS444->$1;
      int32_t _M0L8grow__atS1247 = _M0L4selfS444->$4;
      int32_t _M0L7_2abindS447;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS448;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS449;
      if (_M0L4sizeS1246 >= _M0L8grow__atS1247) {
        int32_t _M0L14capacity__maskS1249;
        int32_t _M0L6_2atmpS1248;
        moonbit_incref(_M0L4selfS444);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS444);
        _M0L14capacity__maskS1249 = _M0L4selfS444->$3;
        _M0L6_2atmpS1248 = _M0L4hashS446 & _M0L14capacity__maskS1249;
        _M0L3pslS441 = 0;
        _M0L3idxS442 = _M0L6_2atmpS1248;
        continue;
      }
      _M0L7_2abindS447 = _M0L4selfS444->$6;
      _M0L7_2abindS448 = 0;
      _M0L5entryS449
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS449)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS449->$0 = _M0L7_2abindS447;
      _M0L5entryS449->$1 = _M0L7_2abindS448;
      _M0L5entryS449->$2 = _M0L3pslS441;
      _M0L5entryS449->$3 = _M0L4hashS446;
      _M0L5entryS449->$4 = _M0L3keyS450;
      _M0L5entryS449->$5 = _M0L5valueS451;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS444, _M0L3idxS442, _M0L5entryS449);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS452 =
        _M0L7_2abindS443;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS453 =
        _M0L7_2aSomeS452;
      int32_t _M0L4hashS1251 = _M0L14_2acurr__entryS453->$3;
      int32_t _if__result_1860;
      int32_t _M0L3pslS1252;
      int32_t _M0L6_2atmpS1257;
      int32_t _M0L6_2atmpS1259;
      int32_t _M0L14capacity__maskS1260;
      int32_t _M0L6_2atmpS1258;
      if (_M0L4hashS1251 == _M0L4hashS446) {
        moonbit_string_t _M0L8_2afieldS1652 = _M0L14_2acurr__entryS453->$4;
        moonbit_string_t _M0L3keyS1250 = _M0L8_2afieldS1652;
        int32_t _M0L6_2atmpS1651;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS1651
        = moonbit_val_array_equal(_M0L3keyS1250, _M0L3keyS450);
        _if__result_1860 = _M0L6_2atmpS1651;
      } else {
        _if__result_1860 = 0;
      }
      if (_if__result_1860) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1650;
        moonbit_incref(_M0L14_2acurr__entryS453);
        moonbit_decref(_M0L3keyS450);
        moonbit_decref(_M0L4selfS444);
        _M0L6_2aoldS1650 = _M0L14_2acurr__entryS453->$5;
        moonbit_decref(_M0L6_2aoldS1650);
        _M0L14_2acurr__entryS453->$5 = _M0L5valueS451;
        moonbit_decref(_M0L14_2acurr__entryS453);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS453);
      }
      _M0L3pslS1252 = _M0L14_2acurr__entryS453->$2;
      if (_M0L3pslS441 > _M0L3pslS1252) {
        int32_t _M0L4sizeS1253 = _M0L4selfS444->$1;
        int32_t _M0L8grow__atS1254 = _M0L4selfS444->$4;
        int32_t _M0L7_2abindS454;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS455;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS456;
        if (_M0L4sizeS1253 >= _M0L8grow__atS1254) {
          int32_t _M0L14capacity__maskS1256;
          int32_t _M0L6_2atmpS1255;
          moonbit_decref(_M0L14_2acurr__entryS453);
          moonbit_incref(_M0L4selfS444);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS444);
          _M0L14capacity__maskS1256 = _M0L4selfS444->$3;
          _M0L6_2atmpS1255 = _M0L4hashS446 & _M0L14capacity__maskS1256;
          _M0L3pslS441 = 0;
          _M0L3idxS442 = _M0L6_2atmpS1255;
          continue;
        }
        moonbit_incref(_M0L4selfS444);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS444, _M0L3idxS442, _M0L14_2acurr__entryS453);
        _M0L7_2abindS454 = _M0L4selfS444->$6;
        _M0L7_2abindS455 = 0;
        _M0L5entryS456
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS456)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS456->$0 = _M0L7_2abindS454;
        _M0L5entryS456->$1 = _M0L7_2abindS455;
        _M0L5entryS456->$2 = _M0L3pslS441;
        _M0L5entryS456->$3 = _M0L4hashS446;
        _M0L5entryS456->$4 = _M0L3keyS450;
        _M0L5entryS456->$5 = _M0L5valueS451;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS444, _M0L3idxS442, _M0L5entryS456);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS453);
      }
      _M0L6_2atmpS1257 = _M0L3pslS441 + 1;
      _M0L6_2atmpS1259 = _M0L3idxS442 + 1;
      _M0L14capacity__maskS1260 = _M0L4selfS444->$3;
      _M0L6_2atmpS1258 = _M0L6_2atmpS1259 & _M0L14capacity__maskS1260;
      _M0L3pslS441 = _M0L6_2atmpS1257;
      _M0L3idxS442 = _M0L6_2atmpS1258;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS460,
  int32_t _M0L3keyS466,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS467,
  int32_t _M0L4hashS462
) {
  int32_t _M0L14capacity__maskS1281;
  int32_t _M0L6_2atmpS1280;
  int32_t _M0L3pslS457;
  int32_t _M0L3idxS458;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1281 = _M0L4selfS460->$3;
  _M0L6_2atmpS1280 = _M0L4hashS462 & _M0L14capacity__maskS1281;
  _M0L3pslS457 = 0;
  _M0L3idxS458 = _M0L6_2atmpS1280;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1657 =
      _M0L4selfS460->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1279 =
      _M0L8_2afieldS1657;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1656;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS459;
    if (
      _M0L3idxS458 < 0
      || _M0L3idxS458 >= Moonbit_array_length(_M0L7entriesS1279)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1656
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1279[
        _M0L3idxS458
      ];
    _M0L7_2abindS459 = _M0L6_2atmpS1656;
    if (_M0L7_2abindS459 == 0) {
      int32_t _M0L4sizeS1264 = _M0L4selfS460->$1;
      int32_t _M0L8grow__atS1265 = _M0L4selfS460->$4;
      int32_t _M0L7_2abindS463;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS464;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS465;
      if (_M0L4sizeS1264 >= _M0L8grow__atS1265) {
        int32_t _M0L14capacity__maskS1267;
        int32_t _M0L6_2atmpS1266;
        moonbit_incref(_M0L4selfS460);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS460);
        _M0L14capacity__maskS1267 = _M0L4selfS460->$3;
        _M0L6_2atmpS1266 = _M0L4hashS462 & _M0L14capacity__maskS1267;
        _M0L3pslS457 = 0;
        _M0L3idxS458 = _M0L6_2atmpS1266;
        continue;
      }
      _M0L7_2abindS463 = _M0L4selfS460->$6;
      _M0L7_2abindS464 = 0;
      _M0L5entryS465
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS465)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS465->$0 = _M0L7_2abindS463;
      _M0L5entryS465->$1 = _M0L7_2abindS464;
      _M0L5entryS465->$2 = _M0L3pslS457;
      _M0L5entryS465->$3 = _M0L4hashS462;
      _M0L5entryS465->$4 = _M0L3keyS466;
      _M0L5entryS465->$5 = _M0L5valueS467;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS460, _M0L3idxS458, _M0L5entryS465);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS468 =
        _M0L7_2abindS459;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS469 =
        _M0L7_2aSomeS468;
      int32_t _M0L4hashS1269 = _M0L14_2acurr__entryS469->$3;
      int32_t _if__result_1862;
      int32_t _M0L3pslS1270;
      int32_t _M0L6_2atmpS1275;
      int32_t _M0L6_2atmpS1277;
      int32_t _M0L14capacity__maskS1278;
      int32_t _M0L6_2atmpS1276;
      if (_M0L4hashS1269 == _M0L4hashS462) {
        int32_t _M0L3keyS1268 = _M0L14_2acurr__entryS469->$4;
        _if__result_1862 = _M0L3keyS1268 == _M0L3keyS466;
      } else {
        _if__result_1862 = 0;
      }
      if (_if__result_1862) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS1655;
        moonbit_incref(_M0L14_2acurr__entryS469);
        moonbit_decref(_M0L4selfS460);
        _M0L6_2aoldS1655 = _M0L14_2acurr__entryS469->$5;
        moonbit_decref(_M0L6_2aoldS1655);
        _M0L14_2acurr__entryS469->$5 = _M0L5valueS467;
        moonbit_decref(_M0L14_2acurr__entryS469);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS469);
      }
      _M0L3pslS1270 = _M0L14_2acurr__entryS469->$2;
      if (_M0L3pslS457 > _M0L3pslS1270) {
        int32_t _M0L4sizeS1271 = _M0L4selfS460->$1;
        int32_t _M0L8grow__atS1272 = _M0L4selfS460->$4;
        int32_t _M0L7_2abindS470;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS471;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS472;
        if (_M0L4sizeS1271 >= _M0L8grow__atS1272) {
          int32_t _M0L14capacity__maskS1274;
          int32_t _M0L6_2atmpS1273;
          moonbit_decref(_M0L14_2acurr__entryS469);
          moonbit_incref(_M0L4selfS460);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS460);
          _M0L14capacity__maskS1274 = _M0L4selfS460->$3;
          _M0L6_2atmpS1273 = _M0L4hashS462 & _M0L14capacity__maskS1274;
          _M0L3pslS457 = 0;
          _M0L3idxS458 = _M0L6_2atmpS1273;
          continue;
        }
        moonbit_incref(_M0L4selfS460);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS460, _M0L3idxS458, _M0L14_2acurr__entryS469);
        _M0L7_2abindS470 = _M0L4selfS460->$6;
        _M0L7_2abindS471 = 0;
        _M0L5entryS472
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS472)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS472->$0 = _M0L7_2abindS470;
        _M0L5entryS472->$1 = _M0L7_2abindS471;
        _M0L5entryS472->$2 = _M0L3pslS457;
        _M0L5entryS472->$3 = _M0L4hashS462;
        _M0L5entryS472->$4 = _M0L3keyS466;
        _M0L5entryS472->$5 = _M0L5valueS467;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS460, _M0L3idxS458, _M0L5entryS472);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS469);
      }
      _M0L6_2atmpS1275 = _M0L3pslS457 + 1;
      _M0L6_2atmpS1277 = _M0L3idxS458 + 1;
      _M0L14capacity__maskS1278 = _M0L4selfS460->$3;
      _M0L6_2atmpS1276 = _M0L6_2atmpS1277 & _M0L14capacity__maskS1278;
      _M0L3pslS457 = _M0L6_2atmpS1275;
      _M0L3idxS458 = _M0L6_2atmpS1276;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS425,
  int32_t _M0L3idxS430,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS429
) {
  int32_t _M0L3pslS1229;
  int32_t _M0L6_2atmpS1225;
  int32_t _M0L6_2atmpS1227;
  int32_t _M0L14capacity__maskS1228;
  int32_t _M0L6_2atmpS1226;
  int32_t _M0L3pslS421;
  int32_t _M0L3idxS422;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS423;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1229 = _M0L5entryS429->$2;
  _M0L6_2atmpS1225 = _M0L3pslS1229 + 1;
  _M0L6_2atmpS1227 = _M0L3idxS430 + 1;
  _M0L14capacity__maskS1228 = _M0L4selfS425->$3;
  _M0L6_2atmpS1226 = _M0L6_2atmpS1227 & _M0L14capacity__maskS1228;
  _M0L3pslS421 = _M0L6_2atmpS1225;
  _M0L3idxS422 = _M0L6_2atmpS1226;
  _M0L5entryS423 = _M0L5entryS429;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1659 =
      _M0L4selfS425->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1224 =
      _M0L8_2afieldS1659;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1658;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS424;
    if (
      _M0L3idxS422 < 0
      || _M0L3idxS422 >= Moonbit_array_length(_M0L7entriesS1224)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1658
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1224[
        _M0L3idxS422
      ];
    _M0L7_2abindS424 = _M0L6_2atmpS1658;
    if (_M0L7_2abindS424 == 0) {
      _M0L5entryS423->$2 = _M0L3pslS421;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS425, _M0L5entryS423, _M0L3idxS422);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS427 =
        _M0L7_2abindS424;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS428 =
        _M0L7_2aSomeS427;
      int32_t _M0L3pslS1214 = _M0L14_2acurr__entryS428->$2;
      if (_M0L3pslS421 > _M0L3pslS1214) {
        int32_t _M0L3pslS1219;
        int32_t _M0L6_2atmpS1215;
        int32_t _M0L6_2atmpS1217;
        int32_t _M0L14capacity__maskS1218;
        int32_t _M0L6_2atmpS1216;
        _M0L5entryS423->$2 = _M0L3pslS421;
        moonbit_incref(_M0L14_2acurr__entryS428);
        moonbit_incref(_M0L4selfS425);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS425, _M0L5entryS423, _M0L3idxS422);
        _M0L3pslS1219 = _M0L14_2acurr__entryS428->$2;
        _M0L6_2atmpS1215 = _M0L3pslS1219 + 1;
        _M0L6_2atmpS1217 = _M0L3idxS422 + 1;
        _M0L14capacity__maskS1218 = _M0L4selfS425->$3;
        _M0L6_2atmpS1216 = _M0L6_2atmpS1217 & _M0L14capacity__maskS1218;
        _M0L3pslS421 = _M0L6_2atmpS1215;
        _M0L3idxS422 = _M0L6_2atmpS1216;
        _M0L5entryS423 = _M0L14_2acurr__entryS428;
        continue;
      } else {
        int32_t _M0L6_2atmpS1220 = _M0L3pslS421 + 1;
        int32_t _M0L6_2atmpS1222 = _M0L3idxS422 + 1;
        int32_t _M0L14capacity__maskS1223 = _M0L4selfS425->$3;
        int32_t _M0L6_2atmpS1221 =
          _M0L6_2atmpS1222 & _M0L14capacity__maskS1223;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_1864 =
          _M0L5entryS423;
        _M0L3pslS421 = _M0L6_2atmpS1220;
        _M0L3idxS422 = _M0L6_2atmpS1221;
        _M0L5entryS423 = _tmp_1864;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS435,
  int32_t _M0L3idxS440,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS439
) {
  int32_t _M0L3pslS1245;
  int32_t _M0L6_2atmpS1241;
  int32_t _M0L6_2atmpS1243;
  int32_t _M0L14capacity__maskS1244;
  int32_t _M0L6_2atmpS1242;
  int32_t _M0L3pslS431;
  int32_t _M0L3idxS432;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS433;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1245 = _M0L5entryS439->$2;
  _M0L6_2atmpS1241 = _M0L3pslS1245 + 1;
  _M0L6_2atmpS1243 = _M0L3idxS440 + 1;
  _M0L14capacity__maskS1244 = _M0L4selfS435->$3;
  _M0L6_2atmpS1242 = _M0L6_2atmpS1243 & _M0L14capacity__maskS1244;
  _M0L3pslS431 = _M0L6_2atmpS1241;
  _M0L3idxS432 = _M0L6_2atmpS1242;
  _M0L5entryS433 = _M0L5entryS439;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1661 =
      _M0L4selfS435->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1240 =
      _M0L8_2afieldS1661;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1660;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS434;
    if (
      _M0L3idxS432 < 0
      || _M0L3idxS432 >= Moonbit_array_length(_M0L7entriesS1240)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1660
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1240[
        _M0L3idxS432
      ];
    _M0L7_2abindS434 = _M0L6_2atmpS1660;
    if (_M0L7_2abindS434 == 0) {
      _M0L5entryS433->$2 = _M0L3pslS431;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS435, _M0L5entryS433, _M0L3idxS432);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS437 =
        _M0L7_2abindS434;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS438 =
        _M0L7_2aSomeS437;
      int32_t _M0L3pslS1230 = _M0L14_2acurr__entryS438->$2;
      if (_M0L3pslS431 > _M0L3pslS1230) {
        int32_t _M0L3pslS1235;
        int32_t _M0L6_2atmpS1231;
        int32_t _M0L6_2atmpS1233;
        int32_t _M0L14capacity__maskS1234;
        int32_t _M0L6_2atmpS1232;
        _M0L5entryS433->$2 = _M0L3pslS431;
        moonbit_incref(_M0L14_2acurr__entryS438);
        moonbit_incref(_M0L4selfS435);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS435, _M0L5entryS433, _M0L3idxS432);
        _M0L3pslS1235 = _M0L14_2acurr__entryS438->$2;
        _M0L6_2atmpS1231 = _M0L3pslS1235 + 1;
        _M0L6_2atmpS1233 = _M0L3idxS432 + 1;
        _M0L14capacity__maskS1234 = _M0L4selfS435->$3;
        _M0L6_2atmpS1232 = _M0L6_2atmpS1233 & _M0L14capacity__maskS1234;
        _M0L3pslS431 = _M0L6_2atmpS1231;
        _M0L3idxS432 = _M0L6_2atmpS1232;
        _M0L5entryS433 = _M0L14_2acurr__entryS438;
        continue;
      } else {
        int32_t _M0L6_2atmpS1236 = _M0L3pslS431 + 1;
        int32_t _M0L6_2atmpS1238 = _M0L3idxS432 + 1;
        int32_t _M0L14capacity__maskS1239 = _M0L4selfS435->$3;
        int32_t _M0L6_2atmpS1237 =
          _M0L6_2atmpS1238 & _M0L14capacity__maskS1239;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_1866 =
          _M0L5entryS433;
        _M0L3pslS431 = _M0L6_2atmpS1236;
        _M0L3idxS432 = _M0L6_2atmpS1237;
        _M0L5entryS433 = _tmp_1866;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS409,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS411,
  int32_t _M0L8new__idxS410
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1664;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1210;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1211;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1663;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS1662;
  int32_t _M0L6_2acntS1782;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS412;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1664 = _M0L4selfS409->$0;
  _M0L7entriesS1210 = _M0L8_2afieldS1664;
  moonbit_incref(_M0L5entryS411);
  _M0L6_2atmpS1211 = _M0L5entryS411;
  if (
    _M0L8new__idxS410 < 0
    || _M0L8new__idxS410 >= Moonbit_array_length(_M0L7entriesS1210)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1663
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1210[
      _M0L8new__idxS410
    ];
  if (_M0L6_2aoldS1663) {
    moonbit_decref(_M0L6_2aoldS1663);
  }
  _M0L7entriesS1210[_M0L8new__idxS410] = _M0L6_2atmpS1211;
  _M0L8_2afieldS1662 = _M0L5entryS411->$1;
  _M0L6_2acntS1782 = Moonbit_object_header(_M0L5entryS411)->rc;
  if (_M0L6_2acntS1782 > 1) {
    int32_t _M0L11_2anew__cntS1785 = _M0L6_2acntS1782 - 1;
    Moonbit_object_header(_M0L5entryS411)->rc = _M0L11_2anew__cntS1785;
    if (_M0L8_2afieldS1662) {
      moonbit_incref(_M0L8_2afieldS1662);
    }
  } else if (_M0L6_2acntS1782 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1784 =
      _M0L5entryS411->$5;
    moonbit_string_t _M0L8_2afieldS1783;
    moonbit_decref(_M0L8_2afieldS1784);
    _M0L8_2afieldS1783 = _M0L5entryS411->$4;
    moonbit_decref(_M0L8_2afieldS1783);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS411);
  }
  _M0L7_2abindS412 = _M0L8_2afieldS1662;
  if (_M0L7_2abindS412 == 0) {
    if (_M0L7_2abindS412) {
      moonbit_decref(_M0L7_2abindS412);
    }
    _M0L4selfS409->$6 = _M0L8new__idxS410;
    moonbit_decref(_M0L4selfS409);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS413;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS414;
    moonbit_decref(_M0L4selfS409);
    _M0L7_2aSomeS413 = _M0L7_2abindS412;
    _M0L7_2anextS414 = _M0L7_2aSomeS413;
    _M0L7_2anextS414->$0 = _M0L8new__idxS410;
    moonbit_decref(_M0L7_2anextS414);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS415,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS417,
  int32_t _M0L8new__idxS416
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1667;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1212;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1213;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1666;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS1665;
  int32_t _M0L6_2acntS1786;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS418;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS1667 = _M0L4selfS415->$0;
  _M0L7entriesS1212 = _M0L8_2afieldS1667;
  moonbit_incref(_M0L5entryS417);
  _M0L6_2atmpS1213 = _M0L5entryS417;
  if (
    _M0L8new__idxS416 < 0
    || _M0L8new__idxS416 >= Moonbit_array_length(_M0L7entriesS1212)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1666
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1212[
      _M0L8new__idxS416
    ];
  if (_M0L6_2aoldS1666) {
    moonbit_decref(_M0L6_2aoldS1666);
  }
  _M0L7entriesS1212[_M0L8new__idxS416] = _M0L6_2atmpS1213;
  _M0L8_2afieldS1665 = _M0L5entryS417->$1;
  _M0L6_2acntS1786 = Moonbit_object_header(_M0L5entryS417)->rc;
  if (_M0L6_2acntS1786 > 1) {
    int32_t _M0L11_2anew__cntS1788 = _M0L6_2acntS1786 - 1;
    Moonbit_object_header(_M0L5entryS417)->rc = _M0L11_2anew__cntS1788;
    if (_M0L8_2afieldS1665) {
      moonbit_incref(_M0L8_2afieldS1665);
    }
  } else if (_M0L6_2acntS1786 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1787 =
      _M0L5entryS417->$5;
    moonbit_decref(_M0L8_2afieldS1787);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS417);
  }
  _M0L7_2abindS418 = _M0L8_2afieldS1665;
  if (_M0L7_2abindS418 == 0) {
    if (_M0L7_2abindS418) {
      moonbit_decref(_M0L7_2abindS418);
    }
    _M0L4selfS415->$6 = _M0L8new__idxS416;
    moonbit_decref(_M0L4selfS415);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS419;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS420;
    moonbit_decref(_M0L4selfS415);
    _M0L7_2aSomeS419 = _M0L7_2abindS418;
    _M0L7_2anextS420 = _M0L7_2aSomeS419;
    _M0L7_2anextS420->$0 = _M0L8new__idxS416;
    moonbit_decref(_M0L7_2anextS420);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS402,
  int32_t _M0L3idxS404,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS403
) {
  int32_t _M0L7_2abindS401;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1669;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1197;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1198;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1668;
  int32_t _M0L4sizeS1200;
  int32_t _M0L6_2atmpS1199;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS401 = _M0L4selfS402->$6;
  switch (_M0L7_2abindS401) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1192;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1670;
      moonbit_incref(_M0L5entryS403);
      _M0L6_2atmpS1192 = _M0L5entryS403;
      _M0L6_2aoldS1670 = _M0L4selfS402->$5;
      if (_M0L6_2aoldS1670) {
        moonbit_decref(_M0L6_2aoldS1670);
      }
      _M0L4selfS402->$5 = _M0L6_2atmpS1192;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS1673 =
        _M0L4selfS402->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1196 =
        _M0L8_2afieldS1673;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1672;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1195;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1193;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1194;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS1671;
      if (
        _M0L7_2abindS401 < 0
        || _M0L7_2abindS401 >= Moonbit_array_length(_M0L7entriesS1196)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1672
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1196[
          _M0L7_2abindS401
        ];
      _M0L6_2atmpS1195 = _M0L6_2atmpS1672;
      if (_M0L6_2atmpS1195) {
        moonbit_incref(_M0L6_2atmpS1195);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1193
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1195);
      moonbit_incref(_M0L5entryS403);
      _M0L6_2atmpS1194 = _M0L5entryS403;
      _M0L6_2aoldS1671 = _M0L6_2atmpS1193->$1;
      if (_M0L6_2aoldS1671) {
        moonbit_decref(_M0L6_2aoldS1671);
      }
      _M0L6_2atmpS1193->$1 = _M0L6_2atmpS1194;
      moonbit_decref(_M0L6_2atmpS1193);
      break;
    }
  }
  _M0L4selfS402->$6 = _M0L3idxS404;
  _M0L8_2afieldS1669 = _M0L4selfS402->$0;
  _M0L7entriesS1197 = _M0L8_2afieldS1669;
  _M0L6_2atmpS1198 = _M0L5entryS403;
  if (
    _M0L3idxS404 < 0
    || _M0L3idxS404 >= Moonbit_array_length(_M0L7entriesS1197)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1668
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1197[
      _M0L3idxS404
    ];
  if (_M0L6_2aoldS1668) {
    moonbit_decref(_M0L6_2aoldS1668);
  }
  _M0L7entriesS1197[_M0L3idxS404] = _M0L6_2atmpS1198;
  _M0L4sizeS1200 = _M0L4selfS402->$1;
  _M0L6_2atmpS1199 = _M0L4sizeS1200 + 1;
  _M0L4selfS402->$1 = _M0L6_2atmpS1199;
  moonbit_decref(_M0L4selfS402);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS406,
  int32_t _M0L3idxS408,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS407
) {
  int32_t _M0L7_2abindS405;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1675;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1206;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1207;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1674;
  int32_t _M0L4sizeS1209;
  int32_t _M0L6_2atmpS1208;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS405 = _M0L4selfS406->$6;
  switch (_M0L7_2abindS405) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1201;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1676;
      moonbit_incref(_M0L5entryS407);
      _M0L6_2atmpS1201 = _M0L5entryS407;
      _M0L6_2aoldS1676 = _M0L4selfS406->$5;
      if (_M0L6_2aoldS1676) {
        moonbit_decref(_M0L6_2aoldS1676);
      }
      _M0L4selfS406->$5 = _M0L6_2atmpS1201;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1679 =
        _M0L4selfS406->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1205 =
        _M0L8_2afieldS1679;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1678;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1204;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1202;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1203;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS1677;
      if (
        _M0L7_2abindS405 < 0
        || _M0L7_2abindS405 >= Moonbit_array_length(_M0L7entriesS1205)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1678
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1205[
          _M0L7_2abindS405
        ];
      _M0L6_2atmpS1204 = _M0L6_2atmpS1678;
      if (_M0L6_2atmpS1204) {
        moonbit_incref(_M0L6_2atmpS1204);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1202
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1204);
      moonbit_incref(_M0L5entryS407);
      _M0L6_2atmpS1203 = _M0L5entryS407;
      _M0L6_2aoldS1677 = _M0L6_2atmpS1202->$1;
      if (_M0L6_2aoldS1677) {
        moonbit_decref(_M0L6_2aoldS1677);
      }
      _M0L6_2atmpS1202->$1 = _M0L6_2atmpS1203;
      moonbit_decref(_M0L6_2atmpS1202);
      break;
    }
  }
  _M0L4selfS406->$6 = _M0L3idxS408;
  _M0L8_2afieldS1675 = _M0L4selfS406->$0;
  _M0L7entriesS1206 = _M0L8_2afieldS1675;
  _M0L6_2atmpS1207 = _M0L5entryS407;
  if (
    _M0L3idxS408 < 0
    || _M0L3idxS408 >= Moonbit_array_length(_M0L7entriesS1206)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS1674
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1206[
      _M0L3idxS408
    ];
  if (_M0L6_2aoldS1674) {
    moonbit_decref(_M0L6_2aoldS1674);
  }
  _M0L7entriesS1206[_M0L3idxS408] = _M0L6_2atmpS1207;
  _M0L4sizeS1209 = _M0L4selfS406->$1;
  _M0L6_2atmpS1208 = _M0L4sizeS1209 + 1;
  _M0L4selfS406->$1 = _M0L6_2atmpS1208;
  moonbit_decref(_M0L4selfS406);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS390
) {
  int32_t _M0L8capacityS389;
  int32_t _M0L7_2abindS391;
  int32_t _M0L7_2abindS392;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1190;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS393;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS394;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_1867;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS389
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS390);
  _M0L7_2abindS391 = _M0L8capacityS389 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS392 = _M0FPB21calc__grow__threshold(_M0L8capacityS389);
  _M0L6_2atmpS1190 = 0;
  _M0L7_2abindS393
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS389, _M0L6_2atmpS1190);
  _M0L7_2abindS394 = 0;
  _block_1867
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_1867)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_1867->$0 = _M0L7_2abindS393;
  _block_1867->$1 = 0;
  _block_1867->$2 = _M0L8capacityS389;
  _block_1867->$3 = _M0L7_2abindS391;
  _block_1867->$4 = _M0L7_2abindS392;
  _block_1867->$5 = _M0L7_2abindS394;
  _block_1867->$6 = -1;
  return _block_1867;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS396
) {
  int32_t _M0L8capacityS395;
  int32_t _M0L7_2abindS397;
  int32_t _M0L7_2abindS398;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1191;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS399;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS400;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_1868;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS395
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS396);
  _M0L7_2abindS397 = _M0L8capacityS395 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS398 = _M0FPB21calc__grow__threshold(_M0L8capacityS395);
  _M0L6_2atmpS1191 = 0;
  _M0L7_2abindS399
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS395, _M0L6_2atmpS1191);
  _M0L7_2abindS400 = 0;
  _block_1868
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_1868)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_1868->$0 = _M0L7_2abindS399;
  _block_1868->$1 = 0;
  _block_1868->$2 = _M0L8capacityS395;
  _block_1868->$3 = _M0L7_2abindS397;
  _block_1868->$4 = _M0L7_2abindS398;
  _block_1868->$5 = _M0L7_2abindS400;
  _block_1868->$6 = -1;
  return _block_1868;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS388) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS388 >= 0) {
    int32_t _M0L6_2atmpS1189;
    int32_t _M0L6_2atmpS1188;
    int32_t _M0L6_2atmpS1187;
    int32_t _M0L6_2atmpS1186;
    if (_M0L4selfS388 <= 1) {
      return 1;
    }
    if (_M0L4selfS388 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1189 = _M0L4selfS388 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS1188 = moonbit_clz32(_M0L6_2atmpS1189);
    _M0L6_2atmpS1187 = _M0L6_2atmpS1188 - 1;
    _M0L6_2atmpS1186 = 2147483647 >> (_M0L6_2atmpS1187 & 31);
    return _M0L6_2atmpS1186 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS387) {
  int32_t _M0L6_2atmpS1185;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1185 = _M0L8capacityS387 * 13;
  return _M0L6_2atmpS1185 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS383
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS383 == 0) {
    if (_M0L4selfS383) {
      moonbit_decref(_M0L4selfS383);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS384 =
      _M0L4selfS383;
    return _M0L7_2aSomeS384;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS385
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS385 == 0) {
    if (_M0L4selfS385) {
      moonbit_decref(_M0L4selfS385);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS386 =
      _M0L4selfS385;
    return _M0L7_2aSomeS386;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS382
) {
  moonbit_string_t* _M0L6_2atmpS1184;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1184 = _M0L4selfS382;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1184);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS381
) {
  moonbit_string_t* _M0L6_2atmpS1182;
  int32_t _M0L6_2atmpS1680;
  int32_t _M0L6_2atmpS1183;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1181;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS381);
  _M0L6_2atmpS1182 = _M0L4selfS381;
  _M0L6_2atmpS1680 = Moonbit_array_length(_M0L4selfS381);
  moonbit_decref(_M0L4selfS381);
  _M0L6_2atmpS1183 = _M0L6_2atmpS1680;
  _M0L6_2atmpS1181
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1183, _M0L6_2atmpS1182
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1181);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS379
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS378;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1170__l570__* _closure_1869;
  struct _M0TWEOs* _M0L6_2atmpS1169;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS378
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS378)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS378->$0 = 0;
  _closure_1869
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1170__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1170__l570__));
  Moonbit_object_header(_closure_1869)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1170__l570__, $0_0) >> 2, 2, 0);
  _closure_1869->code = &_M0MPC15array9ArrayView4iterGsEC1170l570;
  _closure_1869->$0_0 = _M0L4selfS379.$0;
  _closure_1869->$0_1 = _M0L4selfS379.$1;
  _closure_1869->$0_2 = _M0L4selfS379.$2;
  _closure_1869->$1 = _M0L1iS378;
  _M0L6_2atmpS1169 = (struct _M0TWEOs*)_closure_1869;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1169);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1170l570(
  struct _M0TWEOs* _M0L6_2aenvS1171
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1170__l570__* _M0L14_2acasted__envS1172;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS1685;
  struct _M0TPC13ref3RefGiE* _M0L1iS378;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS1684;
  int32_t _M0L6_2acntS1789;
  struct _M0TPB9ArrayViewGsE _M0L4selfS379;
  int32_t _M0L3valS1173;
  int32_t _M0L6_2atmpS1174;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1172
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1170__l570__*)_M0L6_2aenvS1171;
  _M0L8_2afieldS1685 = _M0L14_2acasted__envS1172->$1;
  _M0L1iS378 = _M0L8_2afieldS1685;
  _M0L8_2afieldS1684
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1172->$0_1,
      _M0L14_2acasted__envS1172->$0_2,
      _M0L14_2acasted__envS1172->$0_0
  };
  _M0L6_2acntS1789 = Moonbit_object_header(_M0L14_2acasted__envS1172)->rc;
  if (_M0L6_2acntS1789 > 1) {
    int32_t _M0L11_2anew__cntS1790 = _M0L6_2acntS1789 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1172)->rc
    = _M0L11_2anew__cntS1790;
    moonbit_incref(_M0L1iS378);
    moonbit_incref(_M0L8_2afieldS1684.$0);
  } else if (_M0L6_2acntS1789 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1172);
  }
  _M0L4selfS379 = _M0L8_2afieldS1684;
  _M0L3valS1173 = _M0L1iS378->$0;
  moonbit_incref(_M0L4selfS379.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1174 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS379);
  if (_M0L3valS1173 < _M0L6_2atmpS1174) {
    moonbit_string_t* _M0L8_2afieldS1683 = _M0L4selfS379.$0;
    moonbit_string_t* _M0L3bufS1177 = _M0L8_2afieldS1683;
    int32_t _M0L8_2afieldS1682 = _M0L4selfS379.$1;
    int32_t _M0L5startS1179 = _M0L8_2afieldS1682;
    int32_t _M0L3valS1180 = _M0L1iS378->$0;
    int32_t _M0L6_2atmpS1178 = _M0L5startS1179 + _M0L3valS1180;
    moonbit_string_t _M0L6_2atmpS1681 =
      (moonbit_string_t)_M0L3bufS1177[_M0L6_2atmpS1178];
    moonbit_string_t _M0L4elemS380;
    int32_t _M0L3valS1176;
    int32_t _M0L6_2atmpS1175;
    moonbit_incref(_M0L6_2atmpS1681);
    moonbit_decref(_M0L3bufS1177);
    _M0L4elemS380 = _M0L6_2atmpS1681;
    _M0L3valS1176 = _M0L1iS378->$0;
    _M0L6_2atmpS1175 = _M0L3valS1176 + 1;
    _M0L1iS378->$0 = _M0L6_2atmpS1175;
    moonbit_decref(_M0L1iS378);
    return _M0L4elemS380;
  } else {
    moonbit_decref(_M0L4selfS379.$0);
    moonbit_decref(_M0L1iS378);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS377
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS377;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS376,
  struct _M0TPB6Logger _M0L6loggerS375
) {
  moonbit_string_t _M0L6_2atmpS1168;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1168 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS376, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS375.$0->$method_0(_M0L6loggerS375.$1, _M0L6_2atmpS1168);
  return 0;
}

int32_t _M0IPC14bool4BoolPB4Show6output(
  int32_t _M0L4selfS373,
  struct _M0TPB6Logger _M0L6loggerS374
) {
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L4selfS373) {
    #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS374.$0->$method_0(_M0L6loggerS374.$1, (moonbit_string_t)moonbit_string_literal_16.data);
  } else {
    #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS374.$0->$method_0(_M0L6loggerS374.$1, (moonbit_string_t)moonbit_string_literal_17.data);
  }
  return 0;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS367,
  moonbit_string_t _M0L5valueS369
) {
  int32_t _M0L3lenS1158;
  moonbit_string_t* _M0L6_2atmpS1160;
  int32_t _M0L6_2atmpS1688;
  int32_t _M0L6_2atmpS1159;
  int32_t _M0L6lengthS368;
  moonbit_string_t* _M0L8_2afieldS1687;
  moonbit_string_t* _M0L3bufS1161;
  moonbit_string_t _M0L6_2aoldS1686;
  int32_t _M0L6_2atmpS1162;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1158 = _M0L4selfS367->$1;
  moonbit_incref(_M0L4selfS367);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1160 = _M0MPC15array5Array6bufferGsE(_M0L4selfS367);
  _M0L6_2atmpS1688 = Moonbit_array_length(_M0L6_2atmpS1160);
  moonbit_decref(_M0L6_2atmpS1160);
  _M0L6_2atmpS1159 = _M0L6_2atmpS1688;
  if (_M0L3lenS1158 == _M0L6_2atmpS1159) {
    moonbit_incref(_M0L4selfS367);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS367);
  }
  _M0L6lengthS368 = _M0L4selfS367->$1;
  _M0L8_2afieldS1687 = _M0L4selfS367->$0;
  _M0L3bufS1161 = _M0L8_2afieldS1687;
  _M0L6_2aoldS1686 = (moonbit_string_t)_M0L3bufS1161[_M0L6lengthS368];
  moonbit_decref(_M0L6_2aoldS1686);
  _M0L3bufS1161[_M0L6lengthS368] = _M0L5valueS369;
  _M0L6_2atmpS1162 = _M0L6lengthS368 + 1;
  _M0L4selfS367->$1 = _M0L6_2atmpS1162;
  moonbit_decref(_M0L4selfS367);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS370,
  struct _M0TUsiE* _M0L5valueS372
) {
  int32_t _M0L3lenS1163;
  struct _M0TUsiE** _M0L6_2atmpS1165;
  int32_t _M0L6_2atmpS1691;
  int32_t _M0L6_2atmpS1164;
  int32_t _M0L6lengthS371;
  struct _M0TUsiE** _M0L8_2afieldS1690;
  struct _M0TUsiE** _M0L3bufS1166;
  struct _M0TUsiE* _M0L6_2aoldS1689;
  int32_t _M0L6_2atmpS1167;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1163 = _M0L4selfS370->$1;
  moonbit_incref(_M0L4selfS370);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1165 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS370);
  _M0L6_2atmpS1691 = Moonbit_array_length(_M0L6_2atmpS1165);
  moonbit_decref(_M0L6_2atmpS1165);
  _M0L6_2atmpS1164 = _M0L6_2atmpS1691;
  if (_M0L3lenS1163 == _M0L6_2atmpS1164) {
    moonbit_incref(_M0L4selfS370);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS370);
  }
  _M0L6lengthS371 = _M0L4selfS370->$1;
  _M0L8_2afieldS1690 = _M0L4selfS370->$0;
  _M0L3bufS1166 = _M0L8_2afieldS1690;
  _M0L6_2aoldS1689 = (struct _M0TUsiE*)_M0L3bufS1166[_M0L6lengthS371];
  if (_M0L6_2aoldS1689) {
    moonbit_decref(_M0L6_2aoldS1689);
  }
  _M0L3bufS1166[_M0L6lengthS371] = _M0L5valueS372;
  _M0L6_2atmpS1167 = _M0L6lengthS371 + 1;
  _M0L4selfS370->$1 = _M0L6_2atmpS1167;
  moonbit_decref(_M0L4selfS370);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS362) {
  int32_t _M0L8old__capS361;
  int32_t _M0L8new__capS363;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS361 = _M0L4selfS362->$1;
  if (_M0L8old__capS361 == 0) {
    _M0L8new__capS363 = 8;
  } else {
    _M0L8new__capS363 = _M0L8old__capS361 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS362, _M0L8new__capS363);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS365
) {
  int32_t _M0L8old__capS364;
  int32_t _M0L8new__capS366;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS364 = _M0L4selfS365->$1;
  if (_M0L8old__capS364 == 0) {
    _M0L8new__capS366 = 8;
  } else {
    _M0L8new__capS366 = _M0L8old__capS364 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS365, _M0L8new__capS366);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS352,
  int32_t _M0L13new__capacityS350
) {
  moonbit_string_t* _M0L8new__bufS349;
  moonbit_string_t* _M0L8_2afieldS1693;
  moonbit_string_t* _M0L8old__bufS351;
  int32_t _M0L8old__capS353;
  int32_t _M0L9copy__lenS354;
  moonbit_string_t* _M0L6_2aoldS1692;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS349
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS350, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS1693 = _M0L4selfS352->$0;
  _M0L8old__bufS351 = _M0L8_2afieldS1693;
  _M0L8old__capS353 = Moonbit_array_length(_M0L8old__bufS351);
  if (_M0L8old__capS353 < _M0L13new__capacityS350) {
    _M0L9copy__lenS354 = _M0L8old__capS353;
  } else {
    _M0L9copy__lenS354 = _M0L13new__capacityS350;
  }
  moonbit_incref(_M0L8old__bufS351);
  moonbit_incref(_M0L8new__bufS349);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS349, 0, _M0L8old__bufS351, 0, _M0L9copy__lenS354);
  _M0L6_2aoldS1692 = _M0L4selfS352->$0;
  moonbit_decref(_M0L6_2aoldS1692);
  _M0L4selfS352->$0 = _M0L8new__bufS349;
  moonbit_decref(_M0L4selfS352);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS358,
  int32_t _M0L13new__capacityS356
) {
  struct _M0TUsiE** _M0L8new__bufS355;
  struct _M0TUsiE** _M0L8_2afieldS1695;
  struct _M0TUsiE** _M0L8old__bufS357;
  int32_t _M0L8old__capS359;
  int32_t _M0L9copy__lenS360;
  struct _M0TUsiE** _M0L6_2aoldS1694;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS355
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS356, 0);
  _M0L8_2afieldS1695 = _M0L4selfS358->$0;
  _M0L8old__bufS357 = _M0L8_2afieldS1695;
  _M0L8old__capS359 = Moonbit_array_length(_M0L8old__bufS357);
  if (_M0L8old__capS359 < _M0L13new__capacityS356) {
    _M0L9copy__lenS360 = _M0L8old__capS359;
  } else {
    _M0L9copy__lenS360 = _M0L13new__capacityS356;
  }
  moonbit_incref(_M0L8old__bufS357);
  moonbit_incref(_M0L8new__bufS355);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS355, 0, _M0L8old__bufS357, 0, _M0L9copy__lenS360);
  _M0L6_2aoldS1694 = _M0L4selfS358->$0;
  moonbit_decref(_M0L6_2aoldS1694);
  _M0L4selfS358->$0 = _M0L8new__bufS355;
  moonbit_decref(_M0L4selfS358);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS348
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS348 == 0) {
    moonbit_string_t* _M0L6_2atmpS1156 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_1870 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_1870)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_1870->$0 = _M0L6_2atmpS1156;
    _block_1870->$1 = 0;
    return _block_1870;
  } else {
    moonbit_string_t* _M0L6_2atmpS1157 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS348, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_1871 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_1871)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_1871->$0 = _M0L6_2atmpS1157;
    _block_1871->$1 = 0;
    return _block_1871;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS346,
  struct _M0TPC16string10StringView _M0L3strS347
) {
  int32_t _M0L3lenS1144;
  int32_t _M0L6_2atmpS1146;
  int32_t _M0L6_2atmpS1145;
  int32_t _M0L6_2atmpS1143;
  moonbit_bytes_t _M0L8_2afieldS1696;
  moonbit_bytes_t _M0L4dataS1147;
  int32_t _M0L3lenS1148;
  moonbit_string_t _M0L6_2atmpS1149;
  int32_t _M0L6_2atmpS1150;
  int32_t _M0L6_2atmpS1151;
  int32_t _M0L3lenS1153;
  int32_t _M0L6_2atmpS1155;
  int32_t _M0L6_2atmpS1154;
  int32_t _M0L6_2atmpS1152;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1144 = _M0L4selfS346->$1;
  moonbit_incref(_M0L3strS347.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1146 = _M0MPC16string10StringView6length(_M0L3strS347);
  _M0L6_2atmpS1145 = _M0L6_2atmpS1146 * 2;
  _M0L6_2atmpS1143 = _M0L3lenS1144 + _M0L6_2atmpS1145;
  moonbit_incref(_M0L4selfS346);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS346, _M0L6_2atmpS1143);
  _M0L8_2afieldS1696 = _M0L4selfS346->$0;
  _M0L4dataS1147 = _M0L8_2afieldS1696;
  _M0L3lenS1148 = _M0L4selfS346->$1;
  moonbit_incref(_M0L4dataS1147);
  moonbit_incref(_M0L3strS347.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1149 = _M0MPC16string10StringView4data(_M0L3strS347);
  moonbit_incref(_M0L3strS347.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1150 = _M0MPC16string10StringView13start__offset(_M0L3strS347);
  moonbit_incref(_M0L3strS347.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1151 = _M0MPC16string10StringView6length(_M0L3strS347);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1147, _M0L3lenS1148, _M0L6_2atmpS1149, _M0L6_2atmpS1150, _M0L6_2atmpS1151);
  _M0L3lenS1153 = _M0L4selfS346->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1155 = _M0MPC16string10StringView6length(_M0L3strS347);
  _M0L6_2atmpS1154 = _M0L6_2atmpS1155 * 2;
  _M0L6_2atmpS1152 = _M0L3lenS1153 + _M0L6_2atmpS1154;
  _M0L4selfS346->$1 = _M0L6_2atmpS1152;
  moonbit_decref(_M0L4selfS346);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS338,
  int32_t _M0L3lenS341,
  int32_t _M0L13start__offsetS345,
  int64_t _M0L11end__offsetS336
) {
  int32_t _M0L11end__offsetS335;
  int32_t _M0L5indexS339;
  int32_t _M0L5countS340;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS336 == 4294967296ll) {
    _M0L11end__offsetS335 = Moonbit_array_length(_M0L4selfS338);
  } else {
    int64_t _M0L7_2aSomeS337 = _M0L11end__offsetS336;
    _M0L11end__offsetS335 = (int32_t)_M0L7_2aSomeS337;
  }
  _M0L5indexS339 = _M0L13start__offsetS345;
  _M0L5countS340 = 0;
  while (1) {
    int32_t _if__result_1873;
    if (_M0L5indexS339 < _M0L11end__offsetS335) {
      _if__result_1873 = _M0L5countS340 < _M0L3lenS341;
    } else {
      _if__result_1873 = 0;
    }
    if (_if__result_1873) {
      int32_t _M0L2c1S342 = _M0L4selfS338[_M0L5indexS339];
      int32_t _if__result_1874;
      int32_t _M0L6_2atmpS1141;
      int32_t _M0L6_2atmpS1142;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S342)) {
        int32_t _M0L6_2atmpS1137 = _M0L5indexS339 + 1;
        _if__result_1874 = _M0L6_2atmpS1137 < _M0L11end__offsetS335;
      } else {
        _if__result_1874 = 0;
      }
      if (_if__result_1874) {
        int32_t _M0L6_2atmpS1140 = _M0L5indexS339 + 1;
        int32_t _M0L2c2S343 = _M0L4selfS338[_M0L6_2atmpS1140];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S343)) {
          int32_t _M0L6_2atmpS1138 = _M0L5indexS339 + 2;
          int32_t _M0L6_2atmpS1139 = _M0L5countS340 + 1;
          _M0L5indexS339 = _M0L6_2atmpS1138;
          _M0L5countS340 = _M0L6_2atmpS1139;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_18.data, (moonbit_string_t)moonbit_string_literal_19.data);
        }
      }
      _M0L6_2atmpS1141 = _M0L5indexS339 + 1;
      _M0L6_2atmpS1142 = _M0L5countS340 + 1;
      _M0L5indexS339 = _M0L6_2atmpS1141;
      _M0L5countS340 = _M0L6_2atmpS1142;
      continue;
    } else {
      moonbit_decref(_M0L4selfS338);
      return _M0L5countS340 >= _M0L3lenS341;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS332
) {
  int32_t _M0L3endS1131;
  int32_t _M0L8_2afieldS1697;
  int32_t _M0L5startS1132;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1131 = _M0L4selfS332.$2;
  _M0L8_2afieldS1697 = _M0L4selfS332.$1;
  moonbit_decref(_M0L4selfS332.$0);
  _M0L5startS1132 = _M0L8_2afieldS1697;
  return _M0L3endS1131 - _M0L5startS1132;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS333
) {
  int32_t _M0L3endS1133;
  int32_t _M0L8_2afieldS1698;
  int32_t _M0L5startS1134;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1133 = _M0L4selfS333.$2;
  _M0L8_2afieldS1698 = _M0L4selfS333.$1;
  moonbit_decref(_M0L4selfS333.$0);
  _M0L5startS1134 = _M0L8_2afieldS1698;
  return _M0L3endS1133 - _M0L5startS1134;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS334
) {
  int32_t _M0L3endS1135;
  int32_t _M0L8_2afieldS1699;
  int32_t _M0L5startS1136;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1135 = _M0L4selfS334.$2;
  _M0L8_2afieldS1699 = _M0L4selfS334.$1;
  moonbit_decref(_M0L4selfS334.$0);
  _M0L5startS1136 = _M0L8_2afieldS1699;
  return _M0L3endS1135 - _M0L5startS1136;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS330,
  int64_t _M0L19start__offset_2eoptS328,
  int64_t _M0L11end__offsetS331
) {
  int32_t _M0L13start__offsetS327;
  if (_M0L19start__offset_2eoptS328 == 4294967296ll) {
    _M0L13start__offsetS327 = 0;
  } else {
    int64_t _M0L7_2aSomeS329 = _M0L19start__offset_2eoptS328;
    _M0L13start__offsetS327 = (int32_t)_M0L7_2aSomeS329;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS330, _M0L13start__offsetS327, _M0L11end__offsetS331);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS325,
  int32_t _M0L13start__offsetS326,
  int64_t _M0L11end__offsetS323
) {
  int32_t _M0L11end__offsetS322;
  int32_t _if__result_1875;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS323 == 4294967296ll) {
    _M0L11end__offsetS322 = Moonbit_array_length(_M0L4selfS325);
  } else {
    int64_t _M0L7_2aSomeS324 = _M0L11end__offsetS323;
    _M0L11end__offsetS322 = (int32_t)_M0L7_2aSomeS324;
  }
  if (_M0L13start__offsetS326 >= 0) {
    if (_M0L13start__offsetS326 <= _M0L11end__offsetS322) {
      int32_t _M0L6_2atmpS1130 = Moonbit_array_length(_M0L4selfS325);
      _if__result_1875 = _M0L11end__offsetS322 <= _M0L6_2atmpS1130;
    } else {
      _if__result_1875 = 0;
    }
  } else {
    _if__result_1875 = 0;
  }
  if (_if__result_1875) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS326,
                                                 _M0L11end__offsetS322,
                                                 _M0L4selfS325};
  } else {
    moonbit_decref(_M0L4selfS325);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data);
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS314,
  struct _M0TPB6Logger _M0L6loggerS312
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS313;
  int32_t _M0L3lenS315;
  int32_t _M0L1iS316;
  int32_t _M0L3segS317;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS312.$1) {
    moonbit_incref(_M0L6loggerS312.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS312.$0->$method_3(_M0L6loggerS312.$1, 34);
  moonbit_incref(_M0L4selfS314);
  if (_M0L6loggerS312.$1) {
    moonbit_incref(_M0L6loggerS312.$1);
  }
  _M0L6_2aenvS313
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS313)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS313->$0 = _M0L4selfS314;
  _M0L6_2aenvS313->$1_0 = _M0L6loggerS312.$0;
  _M0L6_2aenvS313->$1_1 = _M0L6loggerS312.$1;
  _M0L3lenS315 = Moonbit_array_length(_M0L4selfS314);
  _M0L1iS316 = 0;
  _M0L3segS317 = 0;
  _2afor_318:;
  while (1) {
    int32_t _M0L4codeS319;
    int32_t _M0L1cS321;
    int32_t _M0L6_2atmpS1114;
    int32_t _M0L6_2atmpS1115;
    int32_t _M0L6_2atmpS1116;
    int32_t _tmp_1879;
    int32_t _tmp_1880;
    if (_M0L1iS316 >= _M0L3lenS315) {
      moonbit_decref(_M0L4selfS314);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS313, _M0L3segS317, _M0L1iS316);
      break;
    }
    _M0L4codeS319 = _M0L4selfS314[_M0L1iS316];
    switch (_M0L4codeS319) {
      case 34: {
        _M0L1cS321 = _M0L4codeS319;
        goto join_320;
        break;
      }
      
      case 92: {
        _M0L1cS321 = _M0L4codeS319;
        goto join_320;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1117;
        int32_t _M0L6_2atmpS1118;
        moonbit_incref(_M0L6_2aenvS313);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS313, _M0L3segS317, _M0L1iS316);
        if (_M0L6loggerS312.$1) {
          moonbit_incref(_M0L6loggerS312.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS312.$0->$method_0(_M0L6loggerS312.$1, (moonbit_string_t)moonbit_string_literal_22.data);
        _M0L6_2atmpS1117 = _M0L1iS316 + 1;
        _M0L6_2atmpS1118 = _M0L1iS316 + 1;
        _M0L1iS316 = _M0L6_2atmpS1117;
        _M0L3segS317 = _M0L6_2atmpS1118;
        goto _2afor_318;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1119;
        int32_t _M0L6_2atmpS1120;
        moonbit_incref(_M0L6_2aenvS313);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS313, _M0L3segS317, _M0L1iS316);
        if (_M0L6loggerS312.$1) {
          moonbit_incref(_M0L6loggerS312.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS312.$0->$method_0(_M0L6loggerS312.$1, (moonbit_string_t)moonbit_string_literal_23.data);
        _M0L6_2atmpS1119 = _M0L1iS316 + 1;
        _M0L6_2atmpS1120 = _M0L1iS316 + 1;
        _M0L1iS316 = _M0L6_2atmpS1119;
        _M0L3segS317 = _M0L6_2atmpS1120;
        goto _2afor_318;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1121;
        int32_t _M0L6_2atmpS1122;
        moonbit_incref(_M0L6_2aenvS313);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS313, _M0L3segS317, _M0L1iS316);
        if (_M0L6loggerS312.$1) {
          moonbit_incref(_M0L6loggerS312.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS312.$0->$method_0(_M0L6loggerS312.$1, (moonbit_string_t)moonbit_string_literal_24.data);
        _M0L6_2atmpS1121 = _M0L1iS316 + 1;
        _M0L6_2atmpS1122 = _M0L1iS316 + 1;
        _M0L1iS316 = _M0L6_2atmpS1121;
        _M0L3segS317 = _M0L6_2atmpS1122;
        goto _2afor_318;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1123;
        int32_t _M0L6_2atmpS1124;
        moonbit_incref(_M0L6_2aenvS313);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS313, _M0L3segS317, _M0L1iS316);
        if (_M0L6loggerS312.$1) {
          moonbit_incref(_M0L6loggerS312.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS312.$0->$method_0(_M0L6loggerS312.$1, (moonbit_string_t)moonbit_string_literal_25.data);
        _M0L6_2atmpS1123 = _M0L1iS316 + 1;
        _M0L6_2atmpS1124 = _M0L1iS316 + 1;
        _M0L1iS316 = _M0L6_2atmpS1123;
        _M0L3segS317 = _M0L6_2atmpS1124;
        goto _2afor_318;
        break;
      }
      default: {
        if (_M0L4codeS319 < 32) {
          int32_t _M0L6_2atmpS1126;
          moonbit_string_t _M0L6_2atmpS1125;
          int32_t _M0L6_2atmpS1127;
          int32_t _M0L6_2atmpS1128;
          moonbit_incref(_M0L6_2aenvS313);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS313, _M0L3segS317, _M0L1iS316);
          if (_M0L6loggerS312.$1) {
            moonbit_incref(_M0L6loggerS312.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS312.$0->$method_0(_M0L6loggerS312.$1, (moonbit_string_t)moonbit_string_literal_26.data);
          _M0L6_2atmpS1126 = _M0L4codeS319 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1125 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1126);
          if (_M0L6loggerS312.$1) {
            moonbit_incref(_M0L6loggerS312.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS312.$0->$method_0(_M0L6loggerS312.$1, _M0L6_2atmpS1125);
          if (_M0L6loggerS312.$1) {
            moonbit_incref(_M0L6loggerS312.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS312.$0->$method_3(_M0L6loggerS312.$1, 125);
          _M0L6_2atmpS1127 = _M0L1iS316 + 1;
          _M0L6_2atmpS1128 = _M0L1iS316 + 1;
          _M0L1iS316 = _M0L6_2atmpS1127;
          _M0L3segS317 = _M0L6_2atmpS1128;
          goto _2afor_318;
        } else {
          int32_t _M0L6_2atmpS1129 = _M0L1iS316 + 1;
          int32_t _tmp_1878 = _M0L3segS317;
          _M0L1iS316 = _M0L6_2atmpS1129;
          _M0L3segS317 = _tmp_1878;
          goto _2afor_318;
        }
        break;
      }
    }
    goto joinlet_1877;
    join_320:;
    moonbit_incref(_M0L6_2aenvS313);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS313, _M0L3segS317, _M0L1iS316);
    if (_M0L6loggerS312.$1) {
      moonbit_incref(_M0L6loggerS312.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS312.$0->$method_3(_M0L6loggerS312.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1114 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS321);
    if (_M0L6loggerS312.$1) {
      moonbit_incref(_M0L6loggerS312.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS312.$0->$method_3(_M0L6loggerS312.$1, _M0L6_2atmpS1114);
    _M0L6_2atmpS1115 = _M0L1iS316 + 1;
    _M0L6_2atmpS1116 = _M0L1iS316 + 1;
    _M0L1iS316 = _M0L6_2atmpS1115;
    _M0L3segS317 = _M0L6_2atmpS1116;
    continue;
    joinlet_1877:;
    _tmp_1879 = _M0L1iS316;
    _tmp_1880 = _M0L3segS317;
    _M0L1iS316 = _tmp_1879;
    _M0L3segS317 = _tmp_1880;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS312.$0->$method_3(_M0L6loggerS312.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS308,
  int32_t _M0L3segS311,
  int32_t _M0L1iS310
) {
  struct _M0TPB6Logger _M0L8_2afieldS1701;
  struct _M0TPB6Logger _M0L6loggerS307;
  moonbit_string_t _M0L8_2afieldS1700;
  int32_t _M0L6_2acntS1791;
  moonbit_string_t _M0L4selfS309;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS1701
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS308->$1_0, _M0L6_2aenvS308->$1_1
  };
  _M0L6loggerS307 = _M0L8_2afieldS1701;
  _M0L8_2afieldS1700 = _M0L6_2aenvS308->$0;
  _M0L6_2acntS1791 = Moonbit_object_header(_M0L6_2aenvS308)->rc;
  if (_M0L6_2acntS1791 > 1) {
    int32_t _M0L11_2anew__cntS1792 = _M0L6_2acntS1791 - 1;
    Moonbit_object_header(_M0L6_2aenvS308)->rc = _M0L11_2anew__cntS1792;
    if (_M0L6loggerS307.$1) {
      moonbit_incref(_M0L6loggerS307.$1);
    }
    moonbit_incref(_M0L8_2afieldS1700);
  } else if (_M0L6_2acntS1791 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS308);
  }
  _M0L4selfS309 = _M0L8_2afieldS1700;
  if (_M0L1iS310 > _M0L3segS311) {
    int32_t _M0L6_2atmpS1113 = _M0L1iS310 - _M0L3segS311;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS307.$0->$method_1(_M0L6loggerS307.$1, _M0L4selfS309, _M0L3segS311, _M0L6_2atmpS1113);
  } else {
    moonbit_decref(_M0L4selfS309);
    if (_M0L6loggerS307.$1) {
      moonbit_decref(_M0L6loggerS307.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS306) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS305;
  int32_t _M0L6_2atmpS1110;
  int32_t _M0L6_2atmpS1109;
  int32_t _M0L6_2atmpS1112;
  int32_t _M0L6_2atmpS1111;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1108;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS305 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1110 = _M0IPC14byte4BytePB3Div3div(_M0L1bS306, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1109
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1110);
  moonbit_incref(_M0L7_2aselfS305);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS305, _M0L6_2atmpS1109);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1112 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS306, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1111
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1112);
  moonbit_incref(_M0L7_2aselfS305);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS305, _M0L6_2atmpS1111);
  _M0L6_2atmpS1108 = _M0L7_2aselfS305;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1108);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS304) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS304 < 10) {
    int32_t _M0L6_2atmpS1105;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1105 = _M0IPC14byte4BytePB3Add3add(_M0L1iS304, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1105);
  } else {
    int32_t _M0L6_2atmpS1107;
    int32_t _M0L6_2atmpS1106;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1107 = _M0IPC14byte4BytePB3Add3add(_M0L1iS304, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1106 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1107, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1106);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS302,
  int32_t _M0L4thatS303
) {
  int32_t _M0L6_2atmpS1103;
  int32_t _M0L6_2atmpS1104;
  int32_t _M0L6_2atmpS1102;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1103 = (int32_t)_M0L4selfS302;
  _M0L6_2atmpS1104 = (int32_t)_M0L4thatS303;
  _M0L6_2atmpS1102 = _M0L6_2atmpS1103 - _M0L6_2atmpS1104;
  return _M0L6_2atmpS1102 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS300,
  int32_t _M0L4thatS301
) {
  int32_t _M0L6_2atmpS1100;
  int32_t _M0L6_2atmpS1101;
  int32_t _M0L6_2atmpS1099;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1100 = (int32_t)_M0L4selfS300;
  _M0L6_2atmpS1101 = (int32_t)_M0L4thatS301;
  _M0L6_2atmpS1099 = _M0L6_2atmpS1100 % _M0L6_2atmpS1101;
  return _M0L6_2atmpS1099 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS298,
  int32_t _M0L4thatS299
) {
  int32_t _M0L6_2atmpS1097;
  int32_t _M0L6_2atmpS1098;
  int32_t _M0L6_2atmpS1096;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1097 = (int32_t)_M0L4selfS298;
  _M0L6_2atmpS1098 = (int32_t)_M0L4thatS299;
  _M0L6_2atmpS1096 = _M0L6_2atmpS1097 / _M0L6_2atmpS1098;
  return _M0L6_2atmpS1096 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS296,
  int32_t _M0L4thatS297
) {
  int32_t _M0L6_2atmpS1094;
  int32_t _M0L6_2atmpS1095;
  int32_t _M0L6_2atmpS1093;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1094 = (int32_t)_M0L4selfS296;
  _M0L6_2atmpS1095 = (int32_t)_M0L4thatS297;
  _M0L6_2atmpS1093 = _M0L6_2atmpS1094 + _M0L6_2atmpS1095;
  return _M0L6_2atmpS1093 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS293,
  int32_t _M0L5startS291,
  int32_t _M0L3endS292
) {
  int32_t _if__result_1881;
  int32_t _M0L3lenS294;
  int32_t _M0L6_2atmpS1091;
  int32_t _M0L6_2atmpS1092;
  moonbit_bytes_t _M0L5bytesS295;
  moonbit_bytes_t _M0L6_2atmpS1090;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS291 == 0) {
    int32_t _M0L6_2atmpS1089 = Moonbit_array_length(_M0L3strS293);
    _if__result_1881 = _M0L3endS292 == _M0L6_2atmpS1089;
  } else {
    _if__result_1881 = 0;
  }
  if (_if__result_1881) {
    return _M0L3strS293;
  }
  _M0L3lenS294 = _M0L3endS292 - _M0L5startS291;
  _M0L6_2atmpS1091 = _M0L3lenS294 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1092 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS295
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1091, _M0L6_2atmpS1092);
  moonbit_incref(_M0L5bytesS295);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS295, 0, _M0L3strS293, _M0L5startS291, _M0L3lenS294);
  _M0L6_2atmpS1090 = _M0L5bytesS295;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1090, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS290) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS290;
}

struct moonbit_result_0 _M0FPB4failGuE(
  moonbit_string_t _M0L3msgS289,
  moonbit_string_t _M0L3locS288
) {
  moonbit_string_t _M0L6_2atmpS1088;
  moonbit_string_t _M0L6_2atmpS1703;
  moonbit_string_t _M0L6_2atmpS1086;
  moonbit_string_t _M0L6_2atmpS1087;
  moonbit_string_t _M0L6_2atmpS1702;
  moonbit_string_t _M0L6_2atmpS1085;
  void* _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1084;
  struct moonbit_result_0 _result_1882;
  #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS1088
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS288);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS1703
  = moonbit_add_string(_M0L6_2atmpS1088, (moonbit_string_t)moonbit_string_literal_27.data);
  moonbit_decref(_M0L6_2atmpS1088);
  _M0L6_2atmpS1086 = _M0L6_2atmpS1703;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS1087 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS289);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS1702 = moonbit_add_string(_M0L6_2atmpS1086, _M0L6_2atmpS1087);
  moonbit_decref(_M0L6_2atmpS1086);
  moonbit_decref(_M0L6_2atmpS1087);
  _M0L6_2atmpS1085 = _M0L6_2atmpS1702;
  _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1084
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure));
  Moonbit_object_header(_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1084)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure, $0) >> 2, 1, 0);
  ((struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1084)->$0
  = _M0L6_2atmpS1085;
  _result_1882.tag = 0;
  _result_1882.data.err
  = _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1084;
  return _result_1882;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS272,
  int32_t _M0L5radixS271
) {
  int32_t _if__result_1883;
  int32_t _M0L12is__negativeS273;
  uint32_t _M0L3numS274;
  uint16_t* _M0L6bufferS275;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS271 < 2) {
    _if__result_1883 = 1;
  } else {
    _if__result_1883 = _M0L5radixS271 > 36;
  }
  if (_if__result_1883) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_28.data, (moonbit_string_t)moonbit_string_literal_29.data);
  }
  if (_M0L4selfS272 == 0) {
    return (moonbit_string_t)moonbit_string_literal_30.data;
  }
  _M0L12is__negativeS273 = _M0L4selfS272 < 0;
  if (_M0L12is__negativeS273) {
    int32_t _M0L6_2atmpS1083 = -_M0L4selfS272;
    _M0L3numS274 = *(uint32_t*)&_M0L6_2atmpS1083;
  } else {
    _M0L3numS274 = *(uint32_t*)&_M0L4selfS272;
  }
  switch (_M0L5radixS271) {
    case 10: {
      int32_t _M0L10digit__lenS276;
      int32_t _M0L6_2atmpS1080;
      int32_t _M0L10total__lenS277;
      uint16_t* _M0L6bufferS278;
      int32_t _M0L12digit__startS279;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS276 = _M0FPB12dec__count32(_M0L3numS274);
      if (_M0L12is__negativeS273) {
        _M0L6_2atmpS1080 = 1;
      } else {
        _M0L6_2atmpS1080 = 0;
      }
      _M0L10total__lenS277 = _M0L10digit__lenS276 + _M0L6_2atmpS1080;
      _M0L6bufferS278
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS277, 0);
      if (_M0L12is__negativeS273) {
        _M0L12digit__startS279 = 1;
      } else {
        _M0L12digit__startS279 = 0;
      }
      moonbit_incref(_M0L6bufferS278);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS278, _M0L3numS274, _M0L12digit__startS279, _M0L10total__lenS277);
      _M0L6bufferS275 = _M0L6bufferS278;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS280;
      int32_t _M0L6_2atmpS1081;
      int32_t _M0L10total__lenS281;
      uint16_t* _M0L6bufferS282;
      int32_t _M0L12digit__startS283;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS280 = _M0FPB12hex__count32(_M0L3numS274);
      if (_M0L12is__negativeS273) {
        _M0L6_2atmpS1081 = 1;
      } else {
        _M0L6_2atmpS1081 = 0;
      }
      _M0L10total__lenS281 = _M0L10digit__lenS280 + _M0L6_2atmpS1081;
      _M0L6bufferS282
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS281, 0);
      if (_M0L12is__negativeS273) {
        _M0L12digit__startS283 = 1;
      } else {
        _M0L12digit__startS283 = 0;
      }
      moonbit_incref(_M0L6bufferS282);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS282, _M0L3numS274, _M0L12digit__startS283, _M0L10total__lenS281);
      _M0L6bufferS275 = _M0L6bufferS282;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS284;
      int32_t _M0L6_2atmpS1082;
      int32_t _M0L10total__lenS285;
      uint16_t* _M0L6bufferS286;
      int32_t _M0L12digit__startS287;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS284
      = _M0FPB14radix__count32(_M0L3numS274, _M0L5radixS271);
      if (_M0L12is__negativeS273) {
        _M0L6_2atmpS1082 = 1;
      } else {
        _M0L6_2atmpS1082 = 0;
      }
      _M0L10total__lenS285 = _M0L10digit__lenS284 + _M0L6_2atmpS1082;
      _M0L6bufferS286
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS285, 0);
      if (_M0L12is__negativeS273) {
        _M0L12digit__startS287 = 1;
      } else {
        _M0L12digit__startS287 = 0;
      }
      moonbit_incref(_M0L6bufferS286);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS286, _M0L3numS274, _M0L12digit__startS287, _M0L10total__lenS285, _M0L5radixS271);
      _M0L6bufferS275 = _M0L6bufferS286;
      break;
    }
  }
  if (_M0L12is__negativeS273) {
    _M0L6bufferS275[0] = 45;
  }
  return _M0L6bufferS275;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS265,
  int32_t _M0L5radixS268
) {
  uint32_t _M0Lm3numS266;
  uint32_t _M0L4baseS267;
  int32_t _M0Lm5countS269;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS265 == 0u) {
    return 1;
  }
  _M0Lm3numS266 = _M0L5valueS265;
  _M0L4baseS267 = *(uint32_t*)&_M0L5radixS268;
  _M0Lm5countS269 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1077 = _M0Lm3numS266;
    if (_M0L6_2atmpS1077 > 0u) {
      int32_t _M0L6_2atmpS1078 = _M0Lm5countS269;
      uint32_t _M0L6_2atmpS1079;
      _M0Lm5countS269 = _M0L6_2atmpS1078 + 1;
      _M0L6_2atmpS1079 = _M0Lm3numS266;
      _M0Lm3numS266 = _M0L6_2atmpS1079 / _M0L4baseS267;
      continue;
    }
    break;
  }
  return _M0Lm5countS269;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS263) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS263 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS264;
    int32_t _M0L6_2atmpS1076;
    int32_t _M0L6_2atmpS1075;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS264 = moonbit_clz32(_M0L5valueS263);
    _M0L6_2atmpS1076 = 31 - _M0L14leading__zerosS264;
    _M0L6_2atmpS1075 = _M0L6_2atmpS1076 / 4;
    return _M0L6_2atmpS1075 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS262) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS262 >= 100000u) {
    if (_M0L5valueS262 >= 10000000u) {
      if (_M0L5valueS262 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS262 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS262 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS262 >= 1000u) {
    if (_M0L5valueS262 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS262 >= 100u) {
    return 3;
  } else if (_M0L5valueS262 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS252,
  uint32_t _M0L3numS240,
  int32_t _M0L12digit__startS243,
  int32_t _M0L10total__lenS242
) {
  uint32_t _M0Lm3numS239;
  int32_t _M0Lm6offsetS241;
  uint32_t _M0L6_2atmpS1074;
  int32_t _M0Lm9remainingS254;
  int32_t _M0L6_2atmpS1055;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS239 = _M0L3numS240;
  _M0Lm6offsetS241 = _M0L10total__lenS242 - _M0L12digit__startS243;
  while (1) {
    uint32_t _M0L6_2atmpS1018 = _M0Lm3numS239;
    if (_M0L6_2atmpS1018 >= 10000u) {
      uint32_t _M0L6_2atmpS1041 = _M0Lm3numS239;
      uint32_t _M0L1tS244 = _M0L6_2atmpS1041 / 10000u;
      uint32_t _M0L6_2atmpS1040 = _M0Lm3numS239;
      uint32_t _M0L6_2atmpS1039 = _M0L6_2atmpS1040 % 10000u;
      int32_t _M0L1rS245 = *(int32_t*)&_M0L6_2atmpS1039;
      int32_t _M0L2d1S246;
      int32_t _M0L2d2S247;
      int32_t _M0L6_2atmpS1019;
      int32_t _M0L6_2atmpS1038;
      int32_t _M0L6_2atmpS1037;
      int32_t _M0L6d1__hiS248;
      int32_t _M0L6_2atmpS1036;
      int32_t _M0L6_2atmpS1035;
      int32_t _M0L6d1__loS249;
      int32_t _M0L6_2atmpS1034;
      int32_t _M0L6_2atmpS1033;
      int32_t _M0L6d2__hiS250;
      int32_t _M0L6_2atmpS1032;
      int32_t _M0L6_2atmpS1031;
      int32_t _M0L6d2__loS251;
      int32_t _M0L6_2atmpS1021;
      int32_t _M0L6_2atmpS1020;
      int32_t _M0L6_2atmpS1024;
      int32_t _M0L6_2atmpS1023;
      int32_t _M0L6_2atmpS1022;
      int32_t _M0L6_2atmpS1027;
      int32_t _M0L6_2atmpS1026;
      int32_t _M0L6_2atmpS1025;
      int32_t _M0L6_2atmpS1030;
      int32_t _M0L6_2atmpS1029;
      int32_t _M0L6_2atmpS1028;
      _M0Lm3numS239 = _M0L1tS244;
      _M0L2d1S246 = _M0L1rS245 / 100;
      _M0L2d2S247 = _M0L1rS245 % 100;
      _M0L6_2atmpS1019 = _M0Lm6offsetS241;
      _M0Lm6offsetS241 = _M0L6_2atmpS1019 - 4;
      _M0L6_2atmpS1038 = _M0L2d1S246 / 10;
      _M0L6_2atmpS1037 = 48 + _M0L6_2atmpS1038;
      _M0L6d1__hiS248 = (uint16_t)_M0L6_2atmpS1037;
      _M0L6_2atmpS1036 = _M0L2d1S246 % 10;
      _M0L6_2atmpS1035 = 48 + _M0L6_2atmpS1036;
      _M0L6d1__loS249 = (uint16_t)_M0L6_2atmpS1035;
      _M0L6_2atmpS1034 = _M0L2d2S247 / 10;
      _M0L6_2atmpS1033 = 48 + _M0L6_2atmpS1034;
      _M0L6d2__hiS250 = (uint16_t)_M0L6_2atmpS1033;
      _M0L6_2atmpS1032 = _M0L2d2S247 % 10;
      _M0L6_2atmpS1031 = 48 + _M0L6_2atmpS1032;
      _M0L6d2__loS251 = (uint16_t)_M0L6_2atmpS1031;
      _M0L6_2atmpS1021 = _M0Lm6offsetS241;
      _M0L6_2atmpS1020 = _M0L12digit__startS243 + _M0L6_2atmpS1021;
      _M0L6bufferS252[_M0L6_2atmpS1020] = _M0L6d1__hiS248;
      _M0L6_2atmpS1024 = _M0Lm6offsetS241;
      _M0L6_2atmpS1023 = _M0L12digit__startS243 + _M0L6_2atmpS1024;
      _M0L6_2atmpS1022 = _M0L6_2atmpS1023 + 1;
      _M0L6bufferS252[_M0L6_2atmpS1022] = _M0L6d1__loS249;
      _M0L6_2atmpS1027 = _M0Lm6offsetS241;
      _M0L6_2atmpS1026 = _M0L12digit__startS243 + _M0L6_2atmpS1027;
      _M0L6_2atmpS1025 = _M0L6_2atmpS1026 + 2;
      _M0L6bufferS252[_M0L6_2atmpS1025] = _M0L6d2__hiS250;
      _M0L6_2atmpS1030 = _M0Lm6offsetS241;
      _M0L6_2atmpS1029 = _M0L12digit__startS243 + _M0L6_2atmpS1030;
      _M0L6_2atmpS1028 = _M0L6_2atmpS1029 + 3;
      _M0L6bufferS252[_M0L6_2atmpS1028] = _M0L6d2__loS251;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1074 = _M0Lm3numS239;
  _M0Lm9remainingS254 = *(int32_t*)&_M0L6_2atmpS1074;
  while (1) {
    int32_t _M0L6_2atmpS1042 = _M0Lm9remainingS254;
    if (_M0L6_2atmpS1042 >= 100) {
      int32_t _M0L6_2atmpS1054 = _M0Lm9remainingS254;
      int32_t _M0L1tS255 = _M0L6_2atmpS1054 / 100;
      int32_t _M0L6_2atmpS1053 = _M0Lm9remainingS254;
      int32_t _M0L1dS256 = _M0L6_2atmpS1053 % 100;
      int32_t _M0L6_2atmpS1043;
      int32_t _M0L6_2atmpS1052;
      int32_t _M0L6_2atmpS1051;
      int32_t _M0L5d__hiS257;
      int32_t _M0L6_2atmpS1050;
      int32_t _M0L6_2atmpS1049;
      int32_t _M0L5d__loS258;
      int32_t _M0L6_2atmpS1045;
      int32_t _M0L6_2atmpS1044;
      int32_t _M0L6_2atmpS1048;
      int32_t _M0L6_2atmpS1047;
      int32_t _M0L6_2atmpS1046;
      _M0Lm9remainingS254 = _M0L1tS255;
      _M0L6_2atmpS1043 = _M0Lm6offsetS241;
      _M0Lm6offsetS241 = _M0L6_2atmpS1043 - 2;
      _M0L6_2atmpS1052 = _M0L1dS256 / 10;
      _M0L6_2atmpS1051 = 48 + _M0L6_2atmpS1052;
      _M0L5d__hiS257 = (uint16_t)_M0L6_2atmpS1051;
      _M0L6_2atmpS1050 = _M0L1dS256 % 10;
      _M0L6_2atmpS1049 = 48 + _M0L6_2atmpS1050;
      _M0L5d__loS258 = (uint16_t)_M0L6_2atmpS1049;
      _M0L6_2atmpS1045 = _M0Lm6offsetS241;
      _M0L6_2atmpS1044 = _M0L12digit__startS243 + _M0L6_2atmpS1045;
      _M0L6bufferS252[_M0L6_2atmpS1044] = _M0L5d__hiS257;
      _M0L6_2atmpS1048 = _M0Lm6offsetS241;
      _M0L6_2atmpS1047 = _M0L12digit__startS243 + _M0L6_2atmpS1048;
      _M0L6_2atmpS1046 = _M0L6_2atmpS1047 + 1;
      _M0L6bufferS252[_M0L6_2atmpS1046] = _M0L5d__loS258;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1055 = _M0Lm9remainingS254;
  if (_M0L6_2atmpS1055 >= 10) {
    int32_t _M0L6_2atmpS1056 = _M0Lm6offsetS241;
    int32_t _M0L6_2atmpS1067;
    int32_t _M0L6_2atmpS1066;
    int32_t _M0L6_2atmpS1065;
    int32_t _M0L5d__hiS260;
    int32_t _M0L6_2atmpS1064;
    int32_t _M0L6_2atmpS1063;
    int32_t _M0L6_2atmpS1062;
    int32_t _M0L5d__loS261;
    int32_t _M0L6_2atmpS1058;
    int32_t _M0L6_2atmpS1057;
    int32_t _M0L6_2atmpS1061;
    int32_t _M0L6_2atmpS1060;
    int32_t _M0L6_2atmpS1059;
    _M0Lm6offsetS241 = _M0L6_2atmpS1056 - 2;
    _M0L6_2atmpS1067 = _M0Lm9remainingS254;
    _M0L6_2atmpS1066 = _M0L6_2atmpS1067 / 10;
    _M0L6_2atmpS1065 = 48 + _M0L6_2atmpS1066;
    _M0L5d__hiS260 = (uint16_t)_M0L6_2atmpS1065;
    _M0L6_2atmpS1064 = _M0Lm9remainingS254;
    _M0L6_2atmpS1063 = _M0L6_2atmpS1064 % 10;
    _M0L6_2atmpS1062 = 48 + _M0L6_2atmpS1063;
    _M0L5d__loS261 = (uint16_t)_M0L6_2atmpS1062;
    _M0L6_2atmpS1058 = _M0Lm6offsetS241;
    _M0L6_2atmpS1057 = _M0L12digit__startS243 + _M0L6_2atmpS1058;
    _M0L6bufferS252[_M0L6_2atmpS1057] = _M0L5d__hiS260;
    _M0L6_2atmpS1061 = _M0Lm6offsetS241;
    _M0L6_2atmpS1060 = _M0L12digit__startS243 + _M0L6_2atmpS1061;
    _M0L6_2atmpS1059 = _M0L6_2atmpS1060 + 1;
    _M0L6bufferS252[_M0L6_2atmpS1059] = _M0L5d__loS261;
    moonbit_decref(_M0L6bufferS252);
  } else {
    int32_t _M0L6_2atmpS1068 = _M0Lm6offsetS241;
    int32_t _M0L6_2atmpS1073;
    int32_t _M0L6_2atmpS1069;
    int32_t _M0L6_2atmpS1072;
    int32_t _M0L6_2atmpS1071;
    int32_t _M0L6_2atmpS1070;
    _M0Lm6offsetS241 = _M0L6_2atmpS1068 - 1;
    _M0L6_2atmpS1073 = _M0Lm6offsetS241;
    _M0L6_2atmpS1069 = _M0L12digit__startS243 + _M0L6_2atmpS1073;
    _M0L6_2atmpS1072 = _M0Lm9remainingS254;
    _M0L6_2atmpS1071 = 48 + _M0L6_2atmpS1072;
    _M0L6_2atmpS1070 = (uint16_t)_M0L6_2atmpS1071;
    _M0L6bufferS252[_M0L6_2atmpS1069] = _M0L6_2atmpS1070;
    moonbit_decref(_M0L6bufferS252);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS234,
  uint32_t _M0L3numS228,
  int32_t _M0L12digit__startS226,
  int32_t _M0L10total__lenS225,
  int32_t _M0L5radixS230
) {
  int32_t _M0Lm6offsetS224;
  uint32_t _M0Lm1nS227;
  uint32_t _M0L4baseS229;
  int32_t _M0L6_2atmpS1000;
  int32_t _M0L6_2atmpS999;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS224 = _M0L10total__lenS225 - _M0L12digit__startS226;
  _M0Lm1nS227 = _M0L3numS228;
  _M0L4baseS229 = *(uint32_t*)&_M0L5radixS230;
  _M0L6_2atmpS1000 = _M0L5radixS230 - 1;
  _M0L6_2atmpS999 = _M0L5radixS230 & _M0L6_2atmpS1000;
  if (_M0L6_2atmpS999 == 0) {
    int32_t _M0L5shiftS231;
    uint32_t _M0L4maskS232;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS231 = moonbit_ctz32(_M0L5radixS230);
    _M0L4maskS232 = _M0L4baseS229 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1001 = _M0Lm1nS227;
      if (_M0L6_2atmpS1001 > 0u) {
        int32_t _M0L6_2atmpS1002 = _M0Lm6offsetS224;
        uint32_t _M0L6_2atmpS1008;
        uint32_t _M0L6_2atmpS1007;
        int32_t _M0L5digitS233;
        int32_t _M0L6_2atmpS1005;
        int32_t _M0L6_2atmpS1003;
        int32_t _M0L6_2atmpS1004;
        uint32_t _M0L6_2atmpS1006;
        _M0Lm6offsetS224 = _M0L6_2atmpS1002 - 1;
        _M0L6_2atmpS1008 = _M0Lm1nS227;
        _M0L6_2atmpS1007 = _M0L6_2atmpS1008 & _M0L4maskS232;
        _M0L5digitS233 = *(int32_t*)&_M0L6_2atmpS1007;
        _M0L6_2atmpS1005 = _M0Lm6offsetS224;
        _M0L6_2atmpS1003 = _M0L12digit__startS226 + _M0L6_2atmpS1005;
        _M0L6_2atmpS1004
        = ((moonbit_string_t)moonbit_string_literal_31.data)[
          _M0L5digitS233
        ];
        _M0L6bufferS234[_M0L6_2atmpS1003] = _M0L6_2atmpS1004;
        _M0L6_2atmpS1006 = _M0Lm1nS227;
        _M0Lm1nS227 = _M0L6_2atmpS1006 >> (_M0L5shiftS231 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS234);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1009 = _M0Lm1nS227;
      if (_M0L6_2atmpS1009 > 0u) {
        int32_t _M0L6_2atmpS1010 = _M0Lm6offsetS224;
        uint32_t _M0L6_2atmpS1017;
        uint32_t _M0L1qS236;
        uint32_t _M0L6_2atmpS1015;
        uint32_t _M0L6_2atmpS1016;
        uint32_t _M0L6_2atmpS1014;
        int32_t _M0L5digitS237;
        int32_t _M0L6_2atmpS1013;
        int32_t _M0L6_2atmpS1011;
        int32_t _M0L6_2atmpS1012;
        _M0Lm6offsetS224 = _M0L6_2atmpS1010 - 1;
        _M0L6_2atmpS1017 = _M0Lm1nS227;
        _M0L1qS236 = _M0L6_2atmpS1017 / _M0L4baseS229;
        _M0L6_2atmpS1015 = _M0Lm1nS227;
        _M0L6_2atmpS1016 = _M0L1qS236 * _M0L4baseS229;
        _M0L6_2atmpS1014 = _M0L6_2atmpS1015 - _M0L6_2atmpS1016;
        _M0L5digitS237 = *(int32_t*)&_M0L6_2atmpS1014;
        _M0L6_2atmpS1013 = _M0Lm6offsetS224;
        _M0L6_2atmpS1011 = _M0L12digit__startS226 + _M0L6_2atmpS1013;
        _M0L6_2atmpS1012
        = ((moonbit_string_t)moonbit_string_literal_31.data)[
          _M0L5digitS237
        ];
        _M0L6bufferS234[_M0L6_2atmpS1011] = _M0L6_2atmpS1012;
        _M0Lm1nS227 = _M0L1qS236;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS234);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS221,
  uint32_t _M0L3numS217,
  int32_t _M0L12digit__startS215,
  int32_t _M0L10total__lenS214
) {
  int32_t _M0Lm6offsetS213;
  uint32_t _M0Lm1nS216;
  int32_t _M0L6_2atmpS995;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS213 = _M0L10total__lenS214 - _M0L12digit__startS215;
  _M0Lm1nS216 = _M0L3numS217;
  while (1) {
    int32_t _M0L6_2atmpS983 = _M0Lm6offsetS213;
    if (_M0L6_2atmpS983 >= 2) {
      int32_t _M0L6_2atmpS984 = _M0Lm6offsetS213;
      uint32_t _M0L6_2atmpS994;
      uint32_t _M0L6_2atmpS993;
      int32_t _M0L9byte__valS218;
      int32_t _M0L2hiS219;
      int32_t _M0L2loS220;
      int32_t _M0L6_2atmpS987;
      int32_t _M0L6_2atmpS985;
      int32_t _M0L6_2atmpS986;
      int32_t _M0L6_2atmpS991;
      int32_t _M0L6_2atmpS990;
      int32_t _M0L6_2atmpS988;
      int32_t _M0L6_2atmpS989;
      uint32_t _M0L6_2atmpS992;
      _M0Lm6offsetS213 = _M0L6_2atmpS984 - 2;
      _M0L6_2atmpS994 = _M0Lm1nS216;
      _M0L6_2atmpS993 = _M0L6_2atmpS994 & 255u;
      _M0L9byte__valS218 = *(int32_t*)&_M0L6_2atmpS993;
      _M0L2hiS219 = _M0L9byte__valS218 / 16;
      _M0L2loS220 = _M0L9byte__valS218 % 16;
      _M0L6_2atmpS987 = _M0Lm6offsetS213;
      _M0L6_2atmpS985 = _M0L12digit__startS215 + _M0L6_2atmpS987;
      _M0L6_2atmpS986
      = ((moonbit_string_t)moonbit_string_literal_31.data)[
        _M0L2hiS219
      ];
      _M0L6bufferS221[_M0L6_2atmpS985] = _M0L6_2atmpS986;
      _M0L6_2atmpS991 = _M0Lm6offsetS213;
      _M0L6_2atmpS990 = _M0L12digit__startS215 + _M0L6_2atmpS991;
      _M0L6_2atmpS988 = _M0L6_2atmpS990 + 1;
      _M0L6_2atmpS989
      = ((moonbit_string_t)moonbit_string_literal_31.data)[
        _M0L2loS220
      ];
      _M0L6bufferS221[_M0L6_2atmpS988] = _M0L6_2atmpS989;
      _M0L6_2atmpS992 = _M0Lm1nS216;
      _M0Lm1nS216 = _M0L6_2atmpS992 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS995 = _M0Lm6offsetS213;
  if (_M0L6_2atmpS995 == 1) {
    uint32_t _M0L6_2atmpS998 = _M0Lm1nS216;
    uint32_t _M0L6_2atmpS997 = _M0L6_2atmpS998 & 15u;
    int32_t _M0L6nibbleS223 = *(int32_t*)&_M0L6_2atmpS997;
    int32_t _M0L6_2atmpS996 =
      ((moonbit_string_t)moonbit_string_literal_31.data)[_M0L6nibbleS223];
    _M0L6bufferS221[_M0L12digit__startS215] = _M0L6_2atmpS996;
    moonbit_decref(_M0L6bufferS221);
  } else {
    moonbit_decref(_M0L6bufferS221);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS212) {
  struct _M0TWEOs* _M0L7_2afuncS211;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS211 = _M0L4selfS212;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS211->code(_M0L7_2afuncS211);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS204
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS203;
  struct _M0TPB6Logger _M0L6_2atmpS979;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS203 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS203);
  _M0L6_2atmpS979
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS203
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS204, _M0L6_2atmpS979);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS203);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS206
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS205;
  struct _M0TPB6Logger _M0L6_2atmpS980;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS205 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS205);
  _M0L6_2atmpS980
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS205
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS206, _M0L6_2atmpS980);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS205);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGbE(
  int32_t _M0L4selfS208
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS207;
  struct _M0TPB6Logger _M0L6_2atmpS981;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS207 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS207);
  _M0L6_2atmpS981
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS207
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC14bool4BoolPB4Show6output(_M0L4selfS208, _M0L6_2atmpS981);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS207);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS210
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS209;
  struct _M0TPB6Logger _M0L6_2atmpS982;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS209 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS209);
  _M0L6_2atmpS982
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS209
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS210, _M0L6_2atmpS982);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS209);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS202
) {
  int32_t _M0L8_2afieldS1704;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS1704 = _M0L4selfS202.$1;
  moonbit_decref(_M0L4selfS202.$0);
  return _M0L8_2afieldS1704;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS201
) {
  int32_t _M0L3endS977;
  int32_t _M0L8_2afieldS1705;
  int32_t _M0L5startS978;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS977 = _M0L4selfS201.$2;
  _M0L8_2afieldS1705 = _M0L4selfS201.$1;
  moonbit_decref(_M0L4selfS201.$0);
  _M0L5startS978 = _M0L8_2afieldS1705;
  return _M0L3endS977 - _M0L5startS978;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS200
) {
  moonbit_string_t _M0L8_2afieldS1706;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS1706 = _M0L4selfS200.$0;
  return _M0L8_2afieldS1706;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS196,
  moonbit_string_t _M0L5valueS197,
  int32_t _M0L5startS198,
  int32_t _M0L3lenS199
) {
  int32_t _M0L6_2atmpS976;
  int64_t _M0L6_2atmpS975;
  struct _M0TPC16string10StringView _M0L6_2atmpS974;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS976 = _M0L5startS198 + _M0L3lenS199;
  _M0L6_2atmpS975 = (int64_t)_M0L6_2atmpS976;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS974
  = _M0MPC16string6String11sub_2einner(_M0L5valueS197, _M0L5startS198, _M0L6_2atmpS975);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS196, _M0L6_2atmpS974);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS189,
  int32_t _M0L5startS195,
  int64_t _M0L3endS191
) {
  int32_t _M0L3lenS188;
  int32_t _M0L3endS190;
  int32_t _M0L5startS194;
  int32_t _if__result_1890;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS188 = Moonbit_array_length(_M0L4selfS189);
  if (_M0L3endS191 == 4294967296ll) {
    _M0L3endS190 = _M0L3lenS188;
  } else {
    int64_t _M0L7_2aSomeS192 = _M0L3endS191;
    int32_t _M0L6_2aendS193 = (int32_t)_M0L7_2aSomeS192;
    if (_M0L6_2aendS193 < 0) {
      _M0L3endS190 = _M0L3lenS188 + _M0L6_2aendS193;
    } else {
      _M0L3endS190 = _M0L6_2aendS193;
    }
  }
  if (_M0L5startS195 < 0) {
    _M0L5startS194 = _M0L3lenS188 + _M0L5startS195;
  } else {
    _M0L5startS194 = _M0L5startS195;
  }
  if (_M0L5startS194 >= 0) {
    if (_M0L5startS194 <= _M0L3endS190) {
      _if__result_1890 = _M0L3endS190 <= _M0L3lenS188;
    } else {
      _if__result_1890 = 0;
    }
  } else {
    _if__result_1890 = 0;
  }
  if (_if__result_1890) {
    if (_M0L5startS194 < _M0L3lenS188) {
      int32_t _M0L6_2atmpS971 = _M0L4selfS189[_M0L5startS194];
      int32_t _M0L6_2atmpS970;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS970
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS971);
      if (!_M0L6_2atmpS970) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS190 < _M0L3lenS188) {
      int32_t _M0L6_2atmpS973 = _M0L4selfS189[_M0L3endS190];
      int32_t _M0L6_2atmpS972;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS972
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS973);
      if (!_M0L6_2atmpS972) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS194,
                                                 _M0L3endS190,
                                                 _M0L4selfS189};
  } else {
    moonbit_decref(_M0L4selfS189);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS185) {
  struct _M0TPB6Hasher* _M0L1hS184;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS184 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS184);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS184, _M0L4selfS185);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS184);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS187
) {
  struct _M0TPB6Hasher* _M0L1hS186;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS186 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS186);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS186, _M0L4selfS187);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS186);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS182) {
  int32_t _M0L4seedS181;
  if (_M0L10seed_2eoptS182 == 4294967296ll) {
    _M0L4seedS181 = 0;
  } else {
    int64_t _M0L7_2aSomeS183 = _M0L10seed_2eoptS182;
    _M0L4seedS181 = (int32_t)_M0L7_2aSomeS183;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS181);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS180) {
  uint32_t _M0L6_2atmpS969;
  uint32_t _M0L6_2atmpS968;
  struct _M0TPB6Hasher* _block_1891;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS969 = *(uint32_t*)&_M0L4seedS180;
  _M0L6_2atmpS968 = _M0L6_2atmpS969 + 374761393u;
  _block_1891
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_1891)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_1891->$0 = _M0L6_2atmpS968;
  return _block_1891;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS179) {
  uint32_t _M0L6_2atmpS967;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS967 = _M0MPB6Hasher9avalanche(_M0L4selfS179);
  return *(int32_t*)&_M0L6_2atmpS967;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS178) {
  uint32_t _M0L8_2afieldS1707;
  uint32_t _M0Lm3accS177;
  uint32_t _M0L6_2atmpS956;
  uint32_t _M0L6_2atmpS958;
  uint32_t _M0L6_2atmpS957;
  uint32_t _M0L6_2atmpS959;
  uint32_t _M0L6_2atmpS960;
  uint32_t _M0L6_2atmpS962;
  uint32_t _M0L6_2atmpS961;
  uint32_t _M0L6_2atmpS963;
  uint32_t _M0L6_2atmpS964;
  uint32_t _M0L6_2atmpS966;
  uint32_t _M0L6_2atmpS965;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS1707 = _M0L4selfS178->$0;
  moonbit_decref(_M0L4selfS178);
  _M0Lm3accS177 = _M0L8_2afieldS1707;
  _M0L6_2atmpS956 = _M0Lm3accS177;
  _M0L6_2atmpS958 = _M0Lm3accS177;
  _M0L6_2atmpS957 = _M0L6_2atmpS958 >> 15;
  _M0Lm3accS177 = _M0L6_2atmpS956 ^ _M0L6_2atmpS957;
  _M0L6_2atmpS959 = _M0Lm3accS177;
  _M0Lm3accS177 = _M0L6_2atmpS959 * 2246822519u;
  _M0L6_2atmpS960 = _M0Lm3accS177;
  _M0L6_2atmpS962 = _M0Lm3accS177;
  _M0L6_2atmpS961 = _M0L6_2atmpS962 >> 13;
  _M0Lm3accS177 = _M0L6_2atmpS960 ^ _M0L6_2atmpS961;
  _M0L6_2atmpS963 = _M0Lm3accS177;
  _M0Lm3accS177 = _M0L6_2atmpS963 * 3266489917u;
  _M0L6_2atmpS964 = _M0Lm3accS177;
  _M0L6_2atmpS966 = _M0Lm3accS177;
  _M0L6_2atmpS965 = _M0L6_2atmpS966 >> 16;
  _M0Lm3accS177 = _M0L6_2atmpS964 ^ _M0L6_2atmpS965;
  return _M0Lm3accS177;
}

int32_t _M0IP016_24default__implPB7Compare6op__ltGRP48clawteam8clawteam8internal4pino5LevelE(
  int32_t _M0L1xS175,
  int32_t _M0L1yS176
) {
  int32_t _M0L6_2atmpS955;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS955
  = _M0IP48clawteam8clawteam8internal4pino5LevelPB7Compare7compare(_M0L1xS175, _M0L1yS176);
  return _M0L6_2atmpS955 < 0;
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
  uint32_t _M0L6_2atmpS954;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS954 = *(uint32_t*)&_M0L5valueS170;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS169, _M0L6_2atmpS954);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS167,
  moonbit_string_t _M0L3strS168
) {
  int32_t _M0L3lenS944;
  int32_t _M0L6_2atmpS946;
  int32_t _M0L6_2atmpS945;
  int32_t _M0L6_2atmpS943;
  moonbit_bytes_t _M0L8_2afieldS1709;
  moonbit_bytes_t _M0L4dataS947;
  int32_t _M0L3lenS948;
  int32_t _M0L6_2atmpS949;
  int32_t _M0L3lenS951;
  int32_t _M0L6_2atmpS1708;
  int32_t _M0L6_2atmpS953;
  int32_t _M0L6_2atmpS952;
  int32_t _M0L6_2atmpS950;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS944 = _M0L4selfS167->$1;
  _M0L6_2atmpS946 = Moonbit_array_length(_M0L3strS168);
  _M0L6_2atmpS945 = _M0L6_2atmpS946 * 2;
  _M0L6_2atmpS943 = _M0L3lenS944 + _M0L6_2atmpS945;
  moonbit_incref(_M0L4selfS167);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS167, _M0L6_2atmpS943);
  _M0L8_2afieldS1709 = _M0L4selfS167->$0;
  _M0L4dataS947 = _M0L8_2afieldS1709;
  _M0L3lenS948 = _M0L4selfS167->$1;
  _M0L6_2atmpS949 = Moonbit_array_length(_M0L3strS168);
  moonbit_incref(_M0L4dataS947);
  moonbit_incref(_M0L3strS168);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS947, _M0L3lenS948, _M0L3strS168, 0, _M0L6_2atmpS949);
  _M0L3lenS951 = _M0L4selfS167->$1;
  _M0L6_2atmpS1708 = Moonbit_array_length(_M0L3strS168);
  moonbit_decref(_M0L3strS168);
  _M0L6_2atmpS953 = _M0L6_2atmpS1708;
  _M0L6_2atmpS952 = _M0L6_2atmpS953 * 2;
  _M0L6_2atmpS950 = _M0L3lenS951 + _M0L6_2atmpS952;
  _M0L4selfS167->$1 = _M0L6_2atmpS950;
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
  int32_t _M0L6_2atmpS942;
  int32_t _M0L6_2atmpS941;
  int32_t _M0L2e1S153;
  int32_t _M0L6_2atmpS940;
  int32_t _M0L2e2S156;
  int32_t _M0L4len1S158;
  int32_t _M0L4len2S160;
  int32_t _if__result_1892;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS942 = _M0L6lengthS155 * 2;
  _M0L6_2atmpS941 = _M0L13bytes__offsetS154 + _M0L6_2atmpS942;
  _M0L2e1S153 = _M0L6_2atmpS941 - 1;
  _M0L6_2atmpS940 = _M0L11str__offsetS157 + _M0L6lengthS155;
  _M0L2e2S156 = _M0L6_2atmpS940 - 1;
  _M0L4len1S158 = Moonbit_array_length(_M0L4selfS159);
  _M0L4len2S160 = Moonbit_array_length(_M0L3strS161);
  if (_M0L6lengthS155 >= 0) {
    if (_M0L13bytes__offsetS154 >= 0) {
      if (_M0L2e1S153 < _M0L4len1S158) {
        if (_M0L11str__offsetS157 >= 0) {
          _if__result_1892 = _M0L2e2S156 < _M0L4len2S160;
        } else {
          _if__result_1892 = 0;
        }
      } else {
        _if__result_1892 = 0;
      }
    } else {
      _if__result_1892 = 0;
    }
  } else {
    _if__result_1892 = 0;
  }
  if (_if__result_1892) {
    int32_t _M0L16end__str__offsetS162 =
      _M0L11str__offsetS157 + _M0L6lengthS155;
    int32_t _M0L1iS163 = _M0L11str__offsetS157;
    int32_t _M0L1jS164 = _M0L13bytes__offsetS154;
    while (1) {
      if (_M0L1iS163 < _M0L16end__str__offsetS162) {
        int32_t _M0L6_2atmpS937 = _M0L3strS161[_M0L1iS163];
        int32_t _M0L6_2atmpS936 = (int32_t)_M0L6_2atmpS937;
        uint32_t _M0L1cS165 = *(uint32_t*)&_M0L6_2atmpS936;
        uint32_t _M0L6_2atmpS932 = _M0L1cS165 & 255u;
        int32_t _M0L6_2atmpS931;
        int32_t _M0L6_2atmpS933;
        uint32_t _M0L6_2atmpS935;
        int32_t _M0L6_2atmpS934;
        int32_t _M0L6_2atmpS938;
        int32_t _M0L6_2atmpS939;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS931 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS932);
        if (
          _M0L1jS164 < 0 || _M0L1jS164 >= Moonbit_array_length(_M0L4selfS159)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS159[_M0L1jS164] = _M0L6_2atmpS931;
        _M0L6_2atmpS933 = _M0L1jS164 + 1;
        _M0L6_2atmpS935 = _M0L1cS165 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS934 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS935);
        if (
          _M0L6_2atmpS933 < 0
          || _M0L6_2atmpS933 >= Moonbit_array_length(_M0L4selfS159)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS159[_M0L6_2atmpS933] = _M0L6_2atmpS934;
        _M0L6_2atmpS938 = _M0L1iS163 + 1;
        _M0L6_2atmpS939 = _M0L1jS164 + 2;
        _M0L1iS163 = _M0L6_2atmpS938;
        _M0L1jS164 = _M0L6_2atmpS939;
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
  int32_t _M0L6_2atmpS930;
  struct _M0TPC16string10StringView _M0L7_2abindS98;
  moonbit_string_t _M0L7_2adataS100;
  int32_t _M0L8_2astartS101;
  int32_t _M0L6_2atmpS929;
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
  int32_t _M0L6_2atmpS887;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS930 = Moonbit_array_length(_M0L4reprS99);
  _M0L7_2abindS98
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS930, _M0L4reprS99
  };
  moonbit_incref(_M0L7_2abindS98.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS100 = _M0MPC16string10StringView4data(_M0L7_2abindS98);
  moonbit_incref(_M0L7_2abindS98.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS101
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS98);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS929 = _M0MPC16string10StringView6length(_M0L7_2abindS98);
  _M0L6_2aendS102 = _M0L8_2astartS101 + _M0L6_2atmpS929;
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
  _M0L6_2atmpS887 = _M0Lm9_2acursorS103;
  if (_M0L6_2atmpS887 < _M0L6_2aendS102) {
    int32_t _M0L6_2atmpS889 = _M0Lm9_2acursorS103;
    int32_t _M0L6_2atmpS888;
    moonbit_incref(_M0L7_2adataS100);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS888
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS889);
    if (_M0L6_2atmpS888 == 64) {
      int32_t _M0L6_2atmpS890 = _M0Lm9_2acursorS103;
      _M0Lm9_2acursorS103 = _M0L6_2atmpS890 + 1;
      while (1) {
        int32_t _M0L6_2atmpS891;
        _M0Lm6tag__0S111 = _M0Lm9_2acursorS103;
        _M0L6_2atmpS891 = _M0Lm9_2acursorS103;
        if (_M0L6_2atmpS891 < _M0L6_2aendS102) {
          int32_t _M0L6_2atmpS928 = _M0Lm9_2acursorS103;
          int32_t _M0L10next__charS126;
          int32_t _M0L6_2atmpS892;
          moonbit_incref(_M0L7_2adataS100);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS126
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS928);
          _M0L6_2atmpS892 = _M0Lm9_2acursorS103;
          _M0Lm9_2acursorS103 = _M0L6_2atmpS892 + 1;
          if (_M0L10next__charS126 == 58) {
            int32_t _M0L6_2atmpS893 = _M0Lm9_2acursorS103;
            if (_M0L6_2atmpS893 < _M0L6_2aendS102) {
              int32_t _M0L6_2atmpS894 = _M0Lm9_2acursorS103;
              int32_t _M0L12dispatch__15S127;
              _M0Lm9_2acursorS103 = _M0L6_2atmpS894 + 1;
              _M0L12dispatch__15S127 = 0;
              loop__label__15_130:;
              while (1) {
                int32_t _M0L6_2atmpS895;
                switch (_M0L12dispatch__15S127) {
                  case 3: {
                    int32_t _M0L6_2atmpS898;
                    _M0Lm9tag__1__2S114 = _M0Lm9tag__1__1S113;
                    _M0Lm9tag__1__1S113 = _M0Lm6tag__1S112;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS898 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS898 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS903 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS134;
                      int32_t _M0L6_2atmpS899;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS134
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS903);
                      _M0L6_2atmpS899 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS899 + 1;
                      if (_M0L10next__charS134 < 58) {
                        if (_M0L10next__charS134 < 48) {
                          goto join_133;
                        } else {
                          int32_t _M0L6_2atmpS900;
                          _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                          _M0Lm9tag__2__1S117 = _M0Lm6tag__2S116;
                          _M0Lm6tag__2S116 = _M0Lm9_2acursorS103;
                          _M0Lm6tag__3S115 = _M0Lm9_2acursorS103;
                          _M0L6_2atmpS900 = _M0Lm9_2acursorS103;
                          if (_M0L6_2atmpS900 < _M0L6_2aendS102) {
                            int32_t _M0L6_2atmpS902 = _M0Lm9_2acursorS103;
                            int32_t _M0L10next__charS136;
                            int32_t _M0L6_2atmpS901;
                            moonbit_incref(_M0L7_2adataS100);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS136
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS902);
                            _M0L6_2atmpS901 = _M0Lm9_2acursorS103;
                            _M0Lm9_2acursorS103 = _M0L6_2atmpS901 + 1;
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
                    int32_t _M0L6_2atmpS904;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0Lm6tag__2S116 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS904 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS904 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS906 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS138;
                      int32_t _M0L6_2atmpS905;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS138
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS906);
                      _M0L6_2atmpS905 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS905 + 1;
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
                    int32_t _M0L6_2atmpS907;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS907 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS907 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS909 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS139;
                      int32_t _M0L6_2atmpS908;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS139
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS909);
                      _M0L6_2atmpS908 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS908 + 1;
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
                    int32_t _M0L6_2atmpS910;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0Lm6tag__4S118 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS910 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS910 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS918 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS141;
                      int32_t _M0L6_2atmpS911;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS141
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS918);
                      _M0L6_2atmpS911 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS911 + 1;
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
                        int32_t _M0L6_2atmpS912;
                        _M0Lm9tag__1__2S114 = _M0Lm9tag__1__1S113;
                        _M0Lm9tag__1__1S113 = _M0Lm6tag__1S112;
                        _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                        _M0L6_2atmpS912 = _M0Lm9_2acursorS103;
                        if (_M0L6_2atmpS912 < _M0L6_2aendS102) {
                          int32_t _M0L6_2atmpS917 = _M0Lm9_2acursorS103;
                          int32_t _M0L10next__charS143;
                          int32_t _M0L6_2atmpS913;
                          moonbit_incref(_M0L7_2adataS100);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS143
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS917);
                          _M0L6_2atmpS913 = _M0Lm9_2acursorS103;
                          _M0Lm9_2acursorS103 = _M0L6_2atmpS913 + 1;
                          if (_M0L10next__charS143 < 58) {
                            if (_M0L10next__charS143 < 48) {
                              goto join_142;
                            } else {
                              int32_t _M0L6_2atmpS914;
                              _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                              _M0Lm9tag__2__1S117 = _M0Lm6tag__2S116;
                              _M0Lm6tag__2S116 = _M0Lm9_2acursorS103;
                              _M0L6_2atmpS914 = _M0Lm9_2acursorS103;
                              if (_M0L6_2atmpS914 < _M0L6_2aendS102) {
                                int32_t _M0L6_2atmpS916 = _M0Lm9_2acursorS103;
                                int32_t _M0L10next__charS145;
                                int32_t _M0L6_2atmpS915;
                                moonbit_incref(_M0L7_2adataS100);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS145
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS916);
                                _M0L6_2atmpS915 = _M0Lm9_2acursorS103;
                                _M0Lm9_2acursorS103 = _M0L6_2atmpS915 + 1;
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
                    int32_t _M0L6_2atmpS919;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0Lm6tag__2S116 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS919 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS919 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS921 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS147;
                      int32_t _M0L6_2atmpS920;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS147
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS921);
                      _M0L6_2atmpS920 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS920 + 1;
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
                    int32_t _M0L6_2atmpS922;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0Lm6tag__2S116 = _M0Lm9_2acursorS103;
                    _M0Lm6tag__3S115 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS922 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS922 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS924 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS149;
                      int32_t _M0L6_2atmpS923;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS149
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS924);
                      _M0L6_2atmpS923 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS923 + 1;
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
                    int32_t _M0L6_2atmpS925;
                    _M0Lm9tag__1__1S113 = _M0Lm6tag__1S112;
                    _M0Lm6tag__1S112 = _M0Lm9_2acursorS103;
                    _M0L6_2atmpS925 = _M0Lm9_2acursorS103;
                    if (_M0L6_2atmpS925 < _M0L6_2aendS102) {
                      int32_t _M0L6_2atmpS927 = _M0Lm9_2acursorS103;
                      int32_t _M0L10next__charS151;
                      int32_t _M0L6_2atmpS926;
                      moonbit_incref(_M0L7_2adataS100);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS151
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS927);
                      _M0L6_2atmpS926 = _M0Lm9_2acursorS103;
                      _M0Lm9_2acursorS103 = _M0L6_2atmpS926 + 1;
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
                _M0L6_2atmpS895 = _M0Lm9_2acursorS103;
                if (_M0L6_2atmpS895 < _M0L6_2aendS102) {
                  int32_t _M0L6_2atmpS897 = _M0Lm9_2acursorS103;
                  int32_t _M0L10next__charS131;
                  int32_t _M0L6_2atmpS896;
                  moonbit_incref(_M0L7_2adataS100);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS131
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS100, _M0L6_2atmpS897);
                  _M0L6_2atmpS896 = _M0Lm9_2acursorS103;
                  _M0Lm9_2acursorS103 = _M0L6_2atmpS896 + 1;
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
      int32_t _M0L6_2atmpS886 = _M0Lm20match__tag__saver__1S107;
      int32_t _M0L6_2atmpS885 = _M0L6_2atmpS886 + 1;
      int64_t _M0L6_2atmpS882 = (int64_t)_M0L6_2atmpS885;
      int32_t _M0L6_2atmpS884 = _M0Lm20match__tag__saver__2S108;
      int64_t _M0L6_2atmpS883 = (int64_t)_M0L6_2atmpS884;
      struct _M0TPC16string10StringView _M0L11start__lineS120;
      int32_t _M0L6_2atmpS881;
      int32_t _M0L6_2atmpS880;
      int64_t _M0L6_2atmpS877;
      int32_t _M0L6_2atmpS879;
      int64_t _M0L6_2atmpS878;
      struct _M0TPC16string10StringView _M0L13start__columnS121;
      int32_t _M0L6_2atmpS876;
      int64_t _M0L6_2atmpS873;
      int32_t _M0L6_2atmpS875;
      int64_t _M0L6_2atmpS874;
      struct _M0TPC16string10StringView _M0L3pkgS122;
      int32_t _M0L6_2atmpS872;
      int32_t _M0L6_2atmpS871;
      int64_t _M0L6_2atmpS868;
      int32_t _M0L6_2atmpS870;
      int64_t _M0L6_2atmpS869;
      struct _M0TPC16string10StringView _M0L8filenameS123;
      int32_t _M0L6_2atmpS867;
      int32_t _M0L6_2atmpS866;
      int64_t _M0L6_2atmpS863;
      int32_t _M0L6_2atmpS865;
      int64_t _M0L6_2atmpS864;
      struct _M0TPC16string10StringView _M0L9end__lineS124;
      int32_t _M0L6_2atmpS862;
      int32_t _M0L6_2atmpS861;
      int64_t _M0L6_2atmpS858;
      int32_t _M0L6_2atmpS860;
      int64_t _M0L6_2atmpS859;
      struct _M0TPC16string10StringView _M0L11end__columnS125;
      struct _M0TPB13SourceLocRepr* _block_1909;
      moonbit_incref(_M0L7_2adataS100);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS120
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS882, _M0L6_2atmpS883);
      _M0L6_2atmpS881 = _M0Lm20match__tag__saver__2S108;
      _M0L6_2atmpS880 = _M0L6_2atmpS881 + 1;
      _M0L6_2atmpS877 = (int64_t)_M0L6_2atmpS880;
      _M0L6_2atmpS879 = _M0Lm20match__tag__saver__3S109;
      _M0L6_2atmpS878 = (int64_t)_M0L6_2atmpS879;
      moonbit_incref(_M0L7_2adataS100);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS121
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS877, _M0L6_2atmpS878);
      _M0L6_2atmpS876 = _M0L8_2astartS101 + 1;
      _M0L6_2atmpS873 = (int64_t)_M0L6_2atmpS876;
      _M0L6_2atmpS875 = _M0Lm20match__tag__saver__0S106;
      _M0L6_2atmpS874 = (int64_t)_M0L6_2atmpS875;
      moonbit_incref(_M0L7_2adataS100);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS122
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS873, _M0L6_2atmpS874);
      _M0L6_2atmpS872 = _M0Lm20match__tag__saver__0S106;
      _M0L6_2atmpS871 = _M0L6_2atmpS872 + 1;
      _M0L6_2atmpS868 = (int64_t)_M0L6_2atmpS871;
      _M0L6_2atmpS870 = _M0Lm20match__tag__saver__1S107;
      _M0L6_2atmpS869 = (int64_t)_M0L6_2atmpS870;
      moonbit_incref(_M0L7_2adataS100);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS123
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS868, _M0L6_2atmpS869);
      _M0L6_2atmpS867 = _M0Lm20match__tag__saver__3S109;
      _M0L6_2atmpS866 = _M0L6_2atmpS867 + 1;
      _M0L6_2atmpS863 = (int64_t)_M0L6_2atmpS866;
      _M0L6_2atmpS865 = _M0Lm20match__tag__saver__4S110;
      _M0L6_2atmpS864 = (int64_t)_M0L6_2atmpS865;
      moonbit_incref(_M0L7_2adataS100);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS124
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS863, _M0L6_2atmpS864);
      _M0L6_2atmpS862 = _M0Lm20match__tag__saver__4S110;
      _M0L6_2atmpS861 = _M0L6_2atmpS862 + 1;
      _M0L6_2atmpS858 = (int64_t)_M0L6_2atmpS861;
      _M0L6_2atmpS860 = _M0Lm10match__endS105;
      _M0L6_2atmpS859 = (int64_t)_M0L6_2atmpS860;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS125
      = _M0MPC16string6String4view(_M0L7_2adataS100, _M0L6_2atmpS858, _M0L6_2atmpS859);
      _block_1909
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_1909)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_1909->$0_0 = _M0L3pkgS122.$0;
      _block_1909->$0_1 = _M0L3pkgS122.$1;
      _block_1909->$0_2 = _M0L3pkgS122.$2;
      _block_1909->$1_0 = _M0L8filenameS123.$0;
      _block_1909->$1_1 = _M0L8filenameS123.$1;
      _block_1909->$1_2 = _M0L8filenameS123.$2;
      _block_1909->$2_0 = _M0L11start__lineS120.$0;
      _block_1909->$2_1 = _M0L11start__lineS120.$1;
      _block_1909->$2_2 = _M0L11start__lineS120.$2;
      _block_1909->$3_0 = _M0L13start__columnS121.$0;
      _block_1909->$3_1 = _M0L13start__columnS121.$1;
      _block_1909->$3_2 = _M0L13start__columnS121.$2;
      _block_1909->$4_0 = _M0L9end__lineS124.$0;
      _block_1909->$4_1 = _M0L9end__lineS124.$1;
      _block_1909->$4_2 = _M0L9end__lineS124.$2;
      _block_1909->$5_0 = _M0L11end__columnS125.$0;
      _block_1909->$5_1 = _M0L11end__columnS125.$1;
      _block_1909->$5_2 = _M0L11end__columnS125.$2;
      return _block_1909;
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
  int32_t _if__result_1910;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS95 = _M0L4selfS96->$1;
  if (_M0L5indexS97 >= 0) {
    _if__result_1910 = _M0L5indexS97 < _M0L3lenS95;
  } else {
    _if__result_1910 = 0;
  }
  if (_if__result_1910) {
    moonbit_string_t* _M0L6_2atmpS857;
    moonbit_string_t _M0L6_2atmpS1710;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS857 = _M0MPC15array5Array6bufferGsE(_M0L4selfS96);
    if (
      _M0L5indexS97 < 0
      || _M0L5indexS97 >= Moonbit_array_length(_M0L6_2atmpS857)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1710 = (moonbit_string_t)_M0L6_2atmpS857[_M0L5indexS97];
    moonbit_incref(_M0L6_2atmpS1710);
    moonbit_decref(_M0L6_2atmpS857);
    return _M0L6_2atmpS1710;
  } else {
    moonbit_decref(_M0L4selfS96);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS93
) {
  moonbit_string_t* _M0L8_2afieldS1711;
  int32_t _M0L6_2acntS1793;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS1711 = _M0L4selfS93->$0;
  _M0L6_2acntS1793 = Moonbit_object_header(_M0L4selfS93)->rc;
  if (_M0L6_2acntS1793 > 1) {
    int32_t _M0L11_2anew__cntS1794 = _M0L6_2acntS1793 - 1;
    Moonbit_object_header(_M0L4selfS93)->rc = _M0L11_2anew__cntS1794;
    moonbit_incref(_M0L8_2afieldS1711);
  } else if (_M0L6_2acntS1793 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS93);
  }
  return _M0L8_2afieldS1711;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS94
) {
  struct _M0TUsiE** _M0L8_2afieldS1712;
  int32_t _M0L6_2acntS1795;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS1712 = _M0L4selfS94->$0;
  _M0L6_2acntS1795 = Moonbit_object_header(_M0L4selfS94)->rc;
  if (_M0L6_2acntS1795 > 1) {
    int32_t _M0L11_2anew__cntS1796 = _M0L6_2acntS1795 - 1;
    Moonbit_object_header(_M0L4selfS94)->rc = _M0L11_2anew__cntS1796;
    moonbit_incref(_M0L8_2afieldS1712);
  } else if (_M0L6_2acntS1795 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS94);
  }
  return _M0L8_2afieldS1712;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS92) {
  struct _M0TPB13StringBuilder* _M0L3bufS91;
  struct _M0TPB6Logger _M0L6_2atmpS856;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS91 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS91);
  _M0L6_2atmpS856
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS91
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS92, _M0L6_2atmpS856);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS91);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS90) {
  int32_t _M0L6_2atmpS855;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS855 = (int32_t)_M0L4selfS90;
  return _M0L6_2atmpS855;
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
  int32_t _M0L3lenS850;
  int32_t _M0L6_2atmpS849;
  moonbit_bytes_t _M0L8_2afieldS1713;
  moonbit_bytes_t _M0L4dataS853;
  int32_t _M0L3lenS854;
  int32_t _M0L3incS86;
  int32_t _M0L3lenS852;
  int32_t _M0L6_2atmpS851;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS850 = _M0L4selfS85->$1;
  _M0L6_2atmpS849 = _M0L3lenS850 + 4;
  moonbit_incref(_M0L4selfS85);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS85, _M0L6_2atmpS849);
  _M0L8_2afieldS1713 = _M0L4selfS85->$0;
  _M0L4dataS853 = _M0L8_2afieldS1713;
  _M0L3lenS854 = _M0L4selfS85->$1;
  moonbit_incref(_M0L4dataS853);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS86
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS853, _M0L3lenS854, _M0L2chS87);
  _M0L3lenS852 = _M0L4selfS85->$1;
  _M0L6_2atmpS851 = _M0L3lenS852 + _M0L3incS86;
  _M0L4selfS85->$1 = _M0L6_2atmpS851;
  moonbit_decref(_M0L4selfS85);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS80,
  int32_t _M0L8requiredS81
) {
  moonbit_bytes_t _M0L8_2afieldS1717;
  moonbit_bytes_t _M0L4dataS848;
  int32_t _M0L6_2atmpS1716;
  int32_t _M0L12current__lenS79;
  int32_t _M0Lm13enough__spaceS82;
  int32_t _M0L6_2atmpS846;
  int32_t _M0L6_2atmpS847;
  moonbit_bytes_t _M0L9new__dataS84;
  moonbit_bytes_t _M0L8_2afieldS1715;
  moonbit_bytes_t _M0L4dataS844;
  int32_t _M0L3lenS845;
  moonbit_bytes_t _M0L6_2aoldS1714;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS1717 = _M0L4selfS80->$0;
  _M0L4dataS848 = _M0L8_2afieldS1717;
  _M0L6_2atmpS1716 = Moonbit_array_length(_M0L4dataS848);
  _M0L12current__lenS79 = _M0L6_2atmpS1716;
  if (_M0L8requiredS81 <= _M0L12current__lenS79) {
    moonbit_decref(_M0L4selfS80);
    return 0;
  }
  _M0Lm13enough__spaceS82 = _M0L12current__lenS79;
  while (1) {
    int32_t _M0L6_2atmpS842 = _M0Lm13enough__spaceS82;
    if (_M0L6_2atmpS842 < _M0L8requiredS81) {
      int32_t _M0L6_2atmpS843 = _M0Lm13enough__spaceS82;
      _M0Lm13enough__spaceS82 = _M0L6_2atmpS843 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS846 = _M0Lm13enough__spaceS82;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS847 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS84
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS846, _M0L6_2atmpS847);
  _M0L8_2afieldS1715 = _M0L4selfS80->$0;
  _M0L4dataS844 = _M0L8_2afieldS1715;
  _M0L3lenS845 = _M0L4selfS80->$1;
  moonbit_incref(_M0L4dataS844);
  moonbit_incref(_M0L9new__dataS84);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS84, 0, _M0L4dataS844, 0, _M0L3lenS845);
  _M0L6_2aoldS1714 = _M0L4selfS80->$0;
  moonbit_decref(_M0L6_2aoldS1714);
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
    uint32_t _M0L6_2atmpS825 = _M0L4codeS72 & 255u;
    int32_t _M0L6_2atmpS824;
    int32_t _M0L6_2atmpS826;
    uint32_t _M0L6_2atmpS828;
    int32_t _M0L6_2atmpS827;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS824 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS825);
    if (
      _M0L6offsetS75 < 0
      || _M0L6offsetS75 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6offsetS75] = _M0L6_2atmpS824;
    _M0L6_2atmpS826 = _M0L6offsetS75 + 1;
    _M0L6_2atmpS828 = _M0L4codeS72 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS827 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS828);
    if (
      _M0L6_2atmpS826 < 0
      || _M0L6_2atmpS826 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS826] = _M0L6_2atmpS827;
    moonbit_decref(_M0L4selfS74);
    return 2;
  } else if (_M0L4codeS72 < 1114112u) {
    uint32_t _M0L2hiS76 = _M0L4codeS72 - 65536u;
    uint32_t _M0L6_2atmpS841 = _M0L2hiS76 >> 10;
    uint32_t _M0L2loS77 = _M0L6_2atmpS841 | 55296u;
    uint32_t _M0L6_2atmpS840 = _M0L2hiS76 & 1023u;
    uint32_t _M0L2hiS78 = _M0L6_2atmpS840 | 56320u;
    uint32_t _M0L6_2atmpS830 = _M0L2loS77 & 255u;
    int32_t _M0L6_2atmpS829;
    int32_t _M0L6_2atmpS831;
    uint32_t _M0L6_2atmpS833;
    int32_t _M0L6_2atmpS832;
    int32_t _M0L6_2atmpS834;
    uint32_t _M0L6_2atmpS836;
    int32_t _M0L6_2atmpS835;
    int32_t _M0L6_2atmpS837;
    uint32_t _M0L6_2atmpS839;
    int32_t _M0L6_2atmpS838;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS829 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS830);
    if (
      _M0L6offsetS75 < 0
      || _M0L6offsetS75 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6offsetS75] = _M0L6_2atmpS829;
    _M0L6_2atmpS831 = _M0L6offsetS75 + 1;
    _M0L6_2atmpS833 = _M0L2loS77 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS832 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS833);
    if (
      _M0L6_2atmpS831 < 0
      || _M0L6_2atmpS831 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS831] = _M0L6_2atmpS832;
    _M0L6_2atmpS834 = _M0L6offsetS75 + 2;
    _M0L6_2atmpS836 = _M0L2hiS78 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS835 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS836);
    if (
      _M0L6_2atmpS834 < 0
      || _M0L6_2atmpS834 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS834] = _M0L6_2atmpS835;
    _M0L6_2atmpS837 = _M0L6offsetS75 + 3;
    _M0L6_2atmpS839 = _M0L2hiS78 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS838 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS839);
    if (
      _M0L6_2atmpS837 < 0
      || _M0L6_2atmpS837 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS837] = _M0L6_2atmpS838;
    moonbit_decref(_M0L4selfS74);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS74);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_32.data, (moonbit_string_t)moonbit_string_literal_33.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS71) {
  int32_t _M0L6_2atmpS823;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS823 = *(int32_t*)&_M0L4selfS71;
  return _M0L6_2atmpS823 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS70) {
  int32_t _M0L6_2atmpS822;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS822 = _M0L4selfS70;
  return *(uint32_t*)&_M0L6_2atmpS822;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS69
) {
  moonbit_bytes_t _M0L8_2afieldS1719;
  moonbit_bytes_t _M0L4dataS821;
  moonbit_bytes_t _M0L6_2atmpS818;
  int32_t _M0L8_2afieldS1718;
  int32_t _M0L3lenS820;
  int64_t _M0L6_2atmpS819;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS1719 = _M0L4selfS69->$0;
  _M0L4dataS821 = _M0L8_2afieldS1719;
  moonbit_incref(_M0L4dataS821);
  _M0L6_2atmpS818 = _M0L4dataS821;
  _M0L8_2afieldS1718 = _M0L4selfS69->$1;
  moonbit_decref(_M0L4selfS69);
  _M0L3lenS820 = _M0L8_2afieldS1718;
  _M0L6_2atmpS819 = (int64_t)_M0L3lenS820;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS818, 0, _M0L6_2atmpS819);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS64,
  int32_t _M0L6offsetS68,
  int64_t _M0L6lengthS66
) {
  int32_t _M0L3lenS63;
  int32_t _M0L6lengthS65;
  int32_t _if__result_1912;
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
      int32_t _M0L6_2atmpS817 = _M0L6offsetS68 + _M0L6lengthS65;
      _if__result_1912 = _M0L6_2atmpS817 <= _M0L3lenS63;
    } else {
      _if__result_1912 = 0;
    }
  } else {
    _if__result_1912 = 0;
  }
  if (_if__result_1912) {
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
  struct _M0TPB13StringBuilder* _block_1913;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS61 < 1) {
    _M0L7initialS60 = 1;
  } else {
    _M0L7initialS60 = _M0L10size__hintS61;
  }
  _M0L4dataS62 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS60, 0);
  _block_1913
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_1913)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_1913->$0 = _M0L4dataS62;
  _block_1913->$1 = 0;
  return _block_1913;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS59) {
  int32_t _M0L6_2atmpS816;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS816 = (int32_t)_M0L4selfS59;
  return _M0L6_2atmpS816;
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
  int32_t _if__result_1914;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS22 == _M0L3srcS23) {
    _if__result_1914 = _M0L11dst__offsetS24 < _M0L11src__offsetS25;
  } else {
    _if__result_1914 = 0;
  }
  if (_if__result_1914) {
    int32_t _M0L1iS26 = 0;
    while (1) {
      if (_M0L1iS26 < _M0L3lenS27) {
        int32_t _M0L6_2atmpS789 = _M0L11dst__offsetS24 + _M0L1iS26;
        int32_t _M0L6_2atmpS791 = _M0L11src__offsetS25 + _M0L1iS26;
        int32_t _M0L6_2atmpS790;
        int32_t _M0L6_2atmpS792;
        if (
          _M0L6_2atmpS791 < 0
          || _M0L6_2atmpS791 >= Moonbit_array_length(_M0L3srcS23)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS790 = (int32_t)_M0L3srcS23[_M0L6_2atmpS791];
        if (
          _M0L6_2atmpS789 < 0
          || _M0L6_2atmpS789 >= Moonbit_array_length(_M0L3dstS22)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS22[_M0L6_2atmpS789] = _M0L6_2atmpS790;
        _M0L6_2atmpS792 = _M0L1iS26 + 1;
        _M0L1iS26 = _M0L6_2atmpS792;
        continue;
      } else {
        moonbit_decref(_M0L3srcS23);
        moonbit_decref(_M0L3dstS22);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS797 = _M0L3lenS27 - 1;
    int32_t _M0L1iS29 = _M0L6_2atmpS797;
    while (1) {
      if (_M0L1iS29 >= 0) {
        int32_t _M0L6_2atmpS793 = _M0L11dst__offsetS24 + _M0L1iS29;
        int32_t _M0L6_2atmpS795 = _M0L11src__offsetS25 + _M0L1iS29;
        int32_t _M0L6_2atmpS794;
        int32_t _M0L6_2atmpS796;
        if (
          _M0L6_2atmpS795 < 0
          || _M0L6_2atmpS795 >= Moonbit_array_length(_M0L3srcS23)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS794 = (int32_t)_M0L3srcS23[_M0L6_2atmpS795];
        if (
          _M0L6_2atmpS793 < 0
          || _M0L6_2atmpS793 >= Moonbit_array_length(_M0L3dstS22)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS22[_M0L6_2atmpS793] = _M0L6_2atmpS794;
        _M0L6_2atmpS796 = _M0L1iS29 - 1;
        _M0L1iS29 = _M0L6_2atmpS796;
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
  int32_t _if__result_1917;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS31 == _M0L3srcS32) {
    _if__result_1917 = _M0L11dst__offsetS33 < _M0L11src__offsetS34;
  } else {
    _if__result_1917 = 0;
  }
  if (_if__result_1917) {
    int32_t _M0L1iS35 = 0;
    while (1) {
      if (_M0L1iS35 < _M0L3lenS36) {
        int32_t _M0L6_2atmpS798 = _M0L11dst__offsetS33 + _M0L1iS35;
        int32_t _M0L6_2atmpS800 = _M0L11src__offsetS34 + _M0L1iS35;
        moonbit_string_t _M0L6_2atmpS1721;
        moonbit_string_t _M0L6_2atmpS799;
        moonbit_string_t _M0L6_2aoldS1720;
        int32_t _M0L6_2atmpS801;
        if (
          _M0L6_2atmpS800 < 0
          || _M0L6_2atmpS800 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1721 = (moonbit_string_t)_M0L3srcS32[_M0L6_2atmpS800];
        _M0L6_2atmpS799 = _M0L6_2atmpS1721;
        if (
          _M0L6_2atmpS798 < 0
          || _M0L6_2atmpS798 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1720 = (moonbit_string_t)_M0L3dstS31[_M0L6_2atmpS798];
        moonbit_incref(_M0L6_2atmpS799);
        moonbit_decref(_M0L6_2aoldS1720);
        _M0L3dstS31[_M0L6_2atmpS798] = _M0L6_2atmpS799;
        _M0L6_2atmpS801 = _M0L1iS35 + 1;
        _M0L1iS35 = _M0L6_2atmpS801;
        continue;
      } else {
        moonbit_decref(_M0L3srcS32);
        moonbit_decref(_M0L3dstS31);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS806 = _M0L3lenS36 - 1;
    int32_t _M0L1iS38 = _M0L6_2atmpS806;
    while (1) {
      if (_M0L1iS38 >= 0) {
        int32_t _M0L6_2atmpS802 = _M0L11dst__offsetS33 + _M0L1iS38;
        int32_t _M0L6_2atmpS804 = _M0L11src__offsetS34 + _M0L1iS38;
        moonbit_string_t _M0L6_2atmpS1723;
        moonbit_string_t _M0L6_2atmpS803;
        moonbit_string_t _M0L6_2aoldS1722;
        int32_t _M0L6_2atmpS805;
        if (
          _M0L6_2atmpS804 < 0
          || _M0L6_2atmpS804 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1723 = (moonbit_string_t)_M0L3srcS32[_M0L6_2atmpS804];
        _M0L6_2atmpS803 = _M0L6_2atmpS1723;
        if (
          _M0L6_2atmpS802 < 0
          || _M0L6_2atmpS802 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1722 = (moonbit_string_t)_M0L3dstS31[_M0L6_2atmpS802];
        moonbit_incref(_M0L6_2atmpS803);
        moonbit_decref(_M0L6_2aoldS1722);
        _M0L3dstS31[_M0L6_2atmpS802] = _M0L6_2atmpS803;
        _M0L6_2atmpS805 = _M0L1iS38 - 1;
        _M0L1iS38 = _M0L6_2atmpS805;
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
  int32_t _if__result_1920;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS40 == _M0L3srcS41) {
    _if__result_1920 = _M0L11dst__offsetS42 < _M0L11src__offsetS43;
  } else {
    _if__result_1920 = 0;
  }
  if (_if__result_1920) {
    int32_t _M0L1iS44 = 0;
    while (1) {
      if (_M0L1iS44 < _M0L3lenS45) {
        int32_t _M0L6_2atmpS807 = _M0L11dst__offsetS42 + _M0L1iS44;
        int32_t _M0L6_2atmpS809 = _M0L11src__offsetS43 + _M0L1iS44;
        struct _M0TUsiE* _M0L6_2atmpS1725;
        struct _M0TUsiE* _M0L6_2atmpS808;
        struct _M0TUsiE* _M0L6_2aoldS1724;
        int32_t _M0L6_2atmpS810;
        if (
          _M0L6_2atmpS809 < 0
          || _M0L6_2atmpS809 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1725 = (struct _M0TUsiE*)_M0L3srcS41[_M0L6_2atmpS809];
        _M0L6_2atmpS808 = _M0L6_2atmpS1725;
        if (
          _M0L6_2atmpS807 < 0
          || _M0L6_2atmpS807 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1724 = (struct _M0TUsiE*)_M0L3dstS40[_M0L6_2atmpS807];
        if (_M0L6_2atmpS808) {
          moonbit_incref(_M0L6_2atmpS808);
        }
        if (_M0L6_2aoldS1724) {
          moonbit_decref(_M0L6_2aoldS1724);
        }
        _M0L3dstS40[_M0L6_2atmpS807] = _M0L6_2atmpS808;
        _M0L6_2atmpS810 = _M0L1iS44 + 1;
        _M0L1iS44 = _M0L6_2atmpS810;
        continue;
      } else {
        moonbit_decref(_M0L3srcS41);
        moonbit_decref(_M0L3dstS40);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS815 = _M0L3lenS45 - 1;
    int32_t _M0L1iS47 = _M0L6_2atmpS815;
    while (1) {
      if (_M0L1iS47 >= 0) {
        int32_t _M0L6_2atmpS811 = _M0L11dst__offsetS42 + _M0L1iS47;
        int32_t _M0L6_2atmpS813 = _M0L11src__offsetS43 + _M0L1iS47;
        struct _M0TUsiE* _M0L6_2atmpS1727;
        struct _M0TUsiE* _M0L6_2atmpS812;
        struct _M0TUsiE* _M0L6_2aoldS1726;
        int32_t _M0L6_2atmpS814;
        if (
          _M0L6_2atmpS813 < 0
          || _M0L6_2atmpS813 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1727 = (struct _M0TUsiE*)_M0L3srcS41[_M0L6_2atmpS813];
        _M0L6_2atmpS812 = _M0L6_2atmpS1727;
        if (
          _M0L6_2atmpS811 < 0
          || _M0L6_2atmpS811 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS1726 = (struct _M0TUsiE*)_M0L3dstS40[_M0L6_2atmpS811];
        if (_M0L6_2atmpS812) {
          moonbit_incref(_M0L6_2atmpS812);
        }
        if (_M0L6_2aoldS1726) {
          moonbit_decref(_M0L6_2aoldS1726);
        }
        _M0L3dstS40[_M0L6_2atmpS811] = _M0L6_2atmpS812;
        _M0L6_2atmpS814 = _M0L1iS47 - 1;
        _M0L1iS47 = _M0L6_2atmpS814;
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
  moonbit_string_t _M0L6_2atmpS778;
  moonbit_string_t _M0L6_2atmpS1730;
  moonbit_string_t _M0L6_2atmpS776;
  moonbit_string_t _M0L6_2atmpS777;
  moonbit_string_t _M0L6_2atmpS1729;
  moonbit_string_t _M0L6_2atmpS775;
  moonbit_string_t _M0L6_2atmpS1728;
  moonbit_string_t _M0L6_2atmpS774;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS778 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS16);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1730
  = moonbit_add_string(_M0L6_2atmpS778, (moonbit_string_t)moonbit_string_literal_34.data);
  moonbit_decref(_M0L6_2atmpS778);
  _M0L6_2atmpS776 = _M0L6_2atmpS1730;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS777
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS17);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1729 = moonbit_add_string(_M0L6_2atmpS776, _M0L6_2atmpS777);
  moonbit_decref(_M0L6_2atmpS776);
  moonbit_decref(_M0L6_2atmpS777);
  _M0L6_2atmpS775 = _M0L6_2atmpS1729;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1728
  = moonbit_add_string(_M0L6_2atmpS775, (moonbit_string_t)moonbit_string_literal_35.data);
  moonbit_decref(_M0L6_2atmpS775);
  _M0L6_2atmpS774 = _M0L6_2atmpS1728;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS774);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS18,
  moonbit_string_t _M0L3locS19
) {
  moonbit_string_t _M0L6_2atmpS783;
  moonbit_string_t _M0L6_2atmpS1733;
  moonbit_string_t _M0L6_2atmpS781;
  moonbit_string_t _M0L6_2atmpS782;
  moonbit_string_t _M0L6_2atmpS1732;
  moonbit_string_t _M0L6_2atmpS780;
  moonbit_string_t _M0L6_2atmpS1731;
  moonbit_string_t _M0L6_2atmpS779;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS783 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1733
  = moonbit_add_string(_M0L6_2atmpS783, (moonbit_string_t)moonbit_string_literal_34.data);
  moonbit_decref(_M0L6_2atmpS783);
  _M0L6_2atmpS781 = _M0L6_2atmpS1733;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS782
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1732 = moonbit_add_string(_M0L6_2atmpS781, _M0L6_2atmpS782);
  moonbit_decref(_M0L6_2atmpS781);
  moonbit_decref(_M0L6_2atmpS782);
  _M0L6_2atmpS780 = _M0L6_2atmpS1732;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1731
  = moonbit_add_string(_M0L6_2atmpS780, (moonbit_string_t)moonbit_string_literal_35.data);
  moonbit_decref(_M0L6_2atmpS780);
  _M0L6_2atmpS779 = _M0L6_2atmpS1731;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS779);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS788;
  moonbit_string_t _M0L6_2atmpS1736;
  moonbit_string_t _M0L6_2atmpS786;
  moonbit_string_t _M0L6_2atmpS787;
  moonbit_string_t _M0L6_2atmpS1735;
  moonbit_string_t _M0L6_2atmpS785;
  moonbit_string_t _M0L6_2atmpS1734;
  moonbit_string_t _M0L6_2atmpS784;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS788 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1736
  = moonbit_add_string(_M0L6_2atmpS788, (moonbit_string_t)moonbit_string_literal_34.data);
  moonbit_decref(_M0L6_2atmpS788);
  _M0L6_2atmpS786 = _M0L6_2atmpS1736;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS787
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1735 = moonbit_add_string(_M0L6_2atmpS786, _M0L6_2atmpS787);
  moonbit_decref(_M0L6_2atmpS786);
  moonbit_decref(_M0L6_2atmpS787);
  _M0L6_2atmpS785 = _M0L6_2atmpS1735;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1734
  = moonbit_add_string(_M0L6_2atmpS785, (moonbit_string_t)moonbit_string_literal_35.data);
  moonbit_decref(_M0L6_2atmpS785);
  _M0L6_2atmpS784 = _M0L6_2atmpS1734;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS784);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5valueS15
) {
  uint32_t _M0L3accS773;
  uint32_t _M0L6_2atmpS772;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS773 = _M0L4selfS14->$0;
  _M0L6_2atmpS772 = _M0L3accS773 + 4u;
  _M0L4selfS14->$0 = _M0L6_2atmpS772;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS14, _M0L5valueS15);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS12,
  uint32_t _M0L5inputS13
) {
  uint32_t _M0L3accS770;
  uint32_t _M0L6_2atmpS771;
  uint32_t _M0L6_2atmpS769;
  uint32_t _M0L6_2atmpS768;
  uint32_t _M0L6_2atmpS767;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS770 = _M0L4selfS12->$0;
  _M0L6_2atmpS771 = _M0L5inputS13 * 3266489917u;
  _M0L6_2atmpS769 = _M0L3accS770 + _M0L6_2atmpS771;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS768 = _M0FPB4rotl(_M0L6_2atmpS769, 17);
  _M0L6_2atmpS767 = _M0L6_2atmpS768 * 668265263u;
  _M0L4selfS12->$0 = _M0L6_2atmpS767;
  moonbit_decref(_M0L4selfS12);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS10, int32_t _M0L1rS11) {
  uint32_t _M0L6_2atmpS764;
  int32_t _M0L6_2atmpS766;
  uint32_t _M0L6_2atmpS765;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS764 = _M0L1xS10 << (_M0L1rS11 & 31);
  _M0L6_2atmpS766 = 32 - _M0L1rS11;
  _M0L6_2atmpS765 = _M0L1xS10 >> (_M0L6_2atmpS766 & 31);
  return _M0L6_2atmpS764 | _M0L6_2atmpS765;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S6,
  struct _M0TPB6Logger _M0L10_2ax__4934S9
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS7;
  moonbit_string_t _M0L8_2afieldS1737;
  int32_t _M0L6_2acntS1797;
  moonbit_string_t _M0L15_2a_2aarg__4935S8;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS7
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S6;
  _M0L8_2afieldS1737 = _M0L10_2aFailureS7->$0;
  _M0L6_2acntS1797 = Moonbit_object_header(_M0L10_2aFailureS7)->rc;
  if (_M0L6_2acntS1797 > 1) {
    int32_t _M0L11_2anew__cntS1798 = _M0L6_2acntS1797 - 1;
    Moonbit_object_header(_M0L10_2aFailureS7)->rc = _M0L11_2anew__cntS1798;
    moonbit_incref(_M0L8_2afieldS1737);
  } else if (_M0L6_2acntS1797 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS7);
  }
  _M0L15_2a_2aarg__4935S8 = _M0L8_2afieldS1737;
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_36.data);
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S9, _M0L15_2a_2aarg__4935S8);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_37.data);
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS724) {
  switch (Moonbit_object_tag(_M0L4_2aeS724)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS724);
      return (moonbit_string_t)moonbit_string_literal_38.data;
      break;
    }
    
    case 1: {
      moonbit_decref(_M0L4_2aeS724);
      return (moonbit_string_t)moonbit_string_literal_39.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS724);
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS724);
      return (moonbit_string_t)moonbit_string_literal_40.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS724);
      return (moonbit_string_t)moonbit_string_literal_41.data;
      break;
    }
  }
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS741,
  int32_t _M0L8_2aparamS740
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS739 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS741;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS739, _M0L8_2aparamS740);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS738,
  struct _M0TPC16string10StringView _M0L8_2aparamS737
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS736 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS738;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS736, _M0L8_2aparamS737);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS735,
  moonbit_string_t _M0L8_2aparamS732,
  int32_t _M0L8_2aparamS733,
  int32_t _M0L8_2aparamS734
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS731 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS735;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS731, _M0L8_2aparamS732, _M0L8_2aparamS733, _M0L8_2aparamS734);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS730,
  moonbit_string_t _M0L8_2aparamS729
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS728 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS730;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS728, _M0L8_2aparamS729);
  return 0;
}

void moonbit_init() {
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS649 =
    (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS763 = _M0L7_2abindS649;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS762 =
    (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){0,
                                                             0,
                                                             _M0L6_2atmpS763};
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS761;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS749;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS650;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS760;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS759;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS758;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS750;
  moonbit_string_t* _M0L6_2atmpS757;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS756;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS755;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS651;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS754;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS753;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS752;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS751;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS648;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS748;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS747;
  #line 398 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS761
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS762);
  _M0L8_2atupleS749
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS749)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS749->$0 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L8_2atupleS749->$1 = _M0L6_2atmpS761;
  _M0L7_2abindS650
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS760 = _M0L7_2abindS650;
  _M0L6_2atmpS759
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS760
  };
  #line 400 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS758
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS759);
  _M0L8_2atupleS750
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS750)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS750->$0 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L8_2atupleS750->$1 = _M0L6_2atmpS758;
  _M0L6_2atmpS757 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS757[0] = (moonbit_string_t)moonbit_string_literal_0.data;
  moonbit_incref(_M0FP48clawteam8clawteam8internal4pino37____test__6c6576656c2e6d6274__0_2eclo);
  _M0L8_2atupleS756
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS756)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS756->$0
  = _M0FP48clawteam8clawteam8internal4pino37____test__6c6576656c2e6d6274__0_2eclo;
  _M0L8_2atupleS756->$1 = _M0L6_2atmpS757;
  _M0L8_2atupleS755
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS755)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS755->$0 = 0;
  _M0L8_2atupleS755->$1 = _M0L8_2atupleS756;
  _M0L7_2abindS651
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS651[0] = _M0L8_2atupleS755;
  _M0L6_2atmpS754 = _M0L7_2abindS651;
  _M0L6_2atmpS753
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS754
  };
  #line 402 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS752
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS753);
  _M0L8_2atupleS751
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS751)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS751->$0 = (moonbit_string_t)moonbit_string_literal_44.data;
  _M0L8_2atupleS751->$1 = _M0L6_2atmpS752;
  _M0L7_2abindS648
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS648[0] = _M0L8_2atupleS749;
  _M0L7_2abindS648[1] = _M0L8_2atupleS750;
  _M0L7_2abindS648[2] = _M0L8_2atupleS751;
  _M0L6_2atmpS748 = _M0L7_2abindS648;
  _M0L6_2atmpS747
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 3, _M0L6_2atmpS748
  };
  #line 397 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam8internal4pino48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS747);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS746;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS718;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS719;
  int32_t _M0L7_2abindS720;
  int32_t _M0L2__S721;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS746
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS718
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS718)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS718->$0 = _M0L6_2atmpS746;
  _M0L12async__testsS718->$1 = 0;
  #line 442 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS719
  = _M0FP48clawteam8clawteam8internal4pino52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS720 = _M0L7_2abindS719->$1;
  _M0L2__S721 = 0;
  while (1) {
    if (_M0L2__S721 < _M0L7_2abindS720) {
      struct _M0TUsiE** _M0L8_2afieldS1741 = _M0L7_2abindS719->$0;
      struct _M0TUsiE** _M0L3bufS745 = _M0L8_2afieldS1741;
      struct _M0TUsiE* _M0L6_2atmpS1740 =
        (struct _M0TUsiE*)_M0L3bufS745[_M0L2__S721];
      struct _M0TUsiE* _M0L3argS722 = _M0L6_2atmpS1740;
      moonbit_string_t _M0L8_2afieldS1739 = _M0L3argS722->$0;
      moonbit_string_t _M0L6_2atmpS742 = _M0L8_2afieldS1739;
      int32_t _M0L8_2afieldS1738 = _M0L3argS722->$1;
      int32_t _M0L6_2atmpS743 = _M0L8_2afieldS1738;
      int32_t _M0L6_2atmpS744;
      moonbit_incref(_M0L6_2atmpS742);
      moonbit_incref(_M0L12async__testsS718);
      #line 443 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam8internal4pino44moonbit__test__driver__internal__do__execute(_M0L12async__testsS718, _M0L6_2atmpS742, _M0L6_2atmpS743);
      _M0L6_2atmpS744 = _M0L2__S721 + 1;
      _M0L2__S721 = _M0L6_2atmpS744;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS719);
    }
    break;
  }
  #line 445 "E:\\moonbit\\clawteam\\internal\\pino\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam8internal4pino28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam8internal4pino34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS718);
  return 0;
}