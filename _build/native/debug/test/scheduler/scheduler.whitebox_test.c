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

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB9ArrayViewGUssEE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TUsRPB6LoggerE;

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam9scheduler33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TPB13StringBuilder;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TPB5EntryGssE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TUssE;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0R104_24clawteam_2fclawteam_2fscheduler_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c817;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TP38clawteam8clawteam9scheduler13CliCallParams;

struct _M0TPB6Logger;

struct _M0TWEuQRPC15error5Error;

struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1915__l427__;

struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1911__l428__;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0TP38clawteam8clawteam9scheduler13CliCallResult;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0TPB3MapGssE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB4Show;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1455__l570__;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0TP38clawteam8clawteam9scheduler15DispatchContext;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0BTPB4Show;

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam9scheduler33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0KTPB4ShowS6String;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0TWEu;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC15error5Error101clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC15error5Error103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

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

struct _M0TPB9ArrayViewGUssEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUssE** $0;
  
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

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam9scheduler33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
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

struct _M0TPB5ArrayGORPB9SourceLocE {
  int32_t $1;
  moonbit_string_t* $0;
  
};

struct _M0TPB5EntryGssE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGssE* $1;
  moonbit_string_t $4;
  moonbit_string_t $5;
  
};

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error {
  struct moonbit_result_0(* code)(
    struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error*,
    struct _M0TWuEu*,
    struct _M0TWRPC15error5ErrorEu*
  );
  
};

struct _M0TUssE {
  moonbit_string_t $0;
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

struct _M0R104_24clawteam_2fclawteam_2fscheduler_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c817 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
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

struct _M0TP38clawteam8clawteam9scheduler13CliCallParams {
  moonbit_string_t $0;
  moonbit_string_t $1;
  moonbit_string_t $2;
  struct _M0TPB3MapGssE* $3;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1915__l427__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1911__l428__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0TP38clawteam8clawteam9scheduler13CliCallResult {
  int32_t $2;
  int32_t $3;
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
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

struct _M0TPB3MapGssE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGssE** $0;
  struct _M0TPB5EntryGssE* $5;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TPB4Show {
  struct _M0BTPB4Show* $0;
  void* $1;
  
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

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1455__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
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

struct _M0TP38clawteam8clawteam9scheduler15DispatchContext {
  int32_t $3;
  moonbit_string_t $0;
  moonbit_string_t $1;
  moonbit_string_t $2;
  struct _M0TPB3MapGssE* $4;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0BTPB4Show {
  int32_t(* $method_0)(void*, struct _M0TPB6Logger);
  moonbit_string_t(* $method_1)(void*);
  
};

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam9scheduler33MoonBitTestDriverInternalSkipTestE3Err {
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

struct _M0KTPB4ShowS6String {
  struct _M0BTPB4Show* $0;
  void* $1;
  
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

struct _M0DTPC15error5Error101clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
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

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__executeN17error__to__stringS826(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__executeN14handle__resultS817(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP38clawteam8clawteam9scheduler41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP38clawteam8clawteam9scheduler41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testC1915l427(
  struct _M0TWEu*
);

int32_t _M0IP38clawteam8clawteam9scheduler41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testC1911l428(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP38clawteam8clawteam9scheduler45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS751(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS746(
  int32_t
);

moonbit_string_t _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS739(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S733(
  int32_t,
  moonbit_string_t
);

#define _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP38clawteam8clawteam9scheduler28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam9scheduler34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler53____test__7363686564756c65725f7762746573742e6d6274__2(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler53____test__7363686564756c65725f7762746573742e6d6274__1(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler53____test__7363686564756c65725f7762746573742e6d6274__0(
  
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

struct _M0TPB3MapGssE* _M0MPB3Map11from__arrayGssE(
  struct _M0TPB9ArrayViewGUssEE
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

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE*,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE*);

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

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE*,
  moonbit_string_t,
  moonbit_string_t,
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

int32_t _M0MPB3Map10push__awayGssE(
  struct _M0TPB3MapGssE*,
  int32_t,
  struct _M0TPB5EntryGssE*
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

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE*,
  struct _M0TPB5EntryGssE*,
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

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE*,
  int32_t,
  struct _M0TPB5EntryGssE*
);

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t
);

struct _M0TPB3MapGssE* _M0MPB3Map11new_2einnerGssE(int32_t);

int32_t _M0MPC13int3Int20next__power__of__two(int32_t);

int32_t _M0FPB21calc__grow__threshold(int32_t);

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*
);

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE*
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1455l570(struct _M0TWEOs*);

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

int32_t _M0MPC15array9ArrayView6lengthGUssEE(struct _M0TPB9ArrayViewGUssEE);

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

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MPB6Hasher7combineGiE(struct _M0TPB6Hasher*, int32_t);

int32_t _M0MPB6Hasher7combineGsE(struct _M0TPB6Hasher*, moonbit_string_t);

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

moonbit_string_t _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*
);

int32_t _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
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
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 49, 50, 58, 51, 45, 49, 50, 58, 52, 
    53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 34, 44, 32, 34, 102, 
    105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 49, 51, 58, 52, 48, 45, 49, 51, 58, 
    54, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_39 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    72, 101, 108, 112, 32, 116, 104, 101, 32, 117, 115, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[94]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 93), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 46, 77, 
    111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 
    101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 
    114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 
    116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 
    108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[32]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 31), 
    67, 108, 105, 67, 97, 108, 108, 82, 101, 115, 117, 108, 116, 32, 
    105, 110, 116, 101, 114, 110, 97, 108, 32, 99, 114, 101, 97, 116, 
    105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[32]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 31), 
    67, 108, 105, 67, 97, 108, 108, 80, 97, 114, 97, 109, 115, 32, 105, 
    110, 116, 101, 114, 110, 97, 108, 32, 99, 114, 101, 97, 116, 105, 
    111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 51, 52, 58, 51, 45, 51, 52, 58, 53, 
    49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    119, 101, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 50, 51, 58, 51, 52, 45, 50, 51, 58, 
    52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[34]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 33), 
    68, 105, 115, 112, 97, 116, 99, 104, 67, 111, 110, 116, 101, 120, 
    116, 32, 105, 110, 116, 101, 114, 110, 97, 108, 32, 99, 114, 101, 
    97, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    117, 115, 101, 114, 45, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_64 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_62 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 51, 52, 58, 49, 49, 45, 51, 52, 58, 
    50, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_53 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 55, 48, 58, 53, 45, 55, 
    48, 58, 54, 57, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_43 =
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
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 51, 52, 58, 51, 57, 45, 51, 52, 58, 
    53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    89, 111, 117, 32, 97, 114, 101, 32, 97, 110, 32, 97, 115, 115, 105, 
    115, 116, 97, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 50, 51, 58, 51, 45, 50, 51, 58, 52, 
    51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    115, 101, 115, 115, 105, 111, 110, 45, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[61]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 60), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 49, 51, 58, 51, 45, 49, 51, 58, 54, 
    51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    111, 117, 116, 112, 117, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 50, 51, 58, 49, 49, 45, 50, 51, 58, 
    50, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 49, 50, 58, 51, 54, 45, 49, 50, 58, 
    52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 49, 51, 58, 49, 49, 45, 49, 51, 58, 
    51, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[96]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 95), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 46, 77, 
    111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 
    101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 
    84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 
    115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 
    97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_41 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[62]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 61), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 115, 99, 104, 101, 100, 117, 108, 101, 114, 58, 
    115, 99, 104, 101, 100, 117, 108, 101, 114, 95, 119, 98, 116, 101, 
    115, 116, 46, 109, 98, 116, 58, 49, 50, 58, 49, 49, 45, 49, 50, 58, 
    50, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 56, 49, 58, 57, 45, 56, 
    49, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    99, 108, 97, 117, 100, 101, 0
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

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__2_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__executeN17error__to__stringS826$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__executeN17error__to__stringS826
  };

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam9scheduler59____test__7363686564756c65725f7762746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__1_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam9scheduler59____test__7363686564756c65725f7762746573742e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__2_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam9scheduler59____test__7363686564756c65725f7762746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__0_2edyncall$closure.data;

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
} _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow,
       .$method_1 = _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow}
  };

struct _M0BTPB4Show* _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

moonbit_bytes_t _M0FPB14base64__encodeN6base64S1657 =
  (moonbit_bytes_t)moonbit_bytes_literal_0.data;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP38clawteam8clawteam9scheduler48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS1948
) {
  return _M0FP38clawteam8clawteam9scheduler53____test__7363686564756c65725f7762746573742e6d6274__0();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS1947
) {
  return _M0FP38clawteam8clawteam9scheduler53____test__7363686564756c65725f7762746573742e6d6274__2();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler63____test__7363686564756c65725f7762746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS1946
) {
  return _M0FP38clawteam8clawteam9scheduler53____test__7363686564756c65725f7762746573742e6d6274__1();
}

int32_t _M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS847,
  moonbit_string_t _M0L8filenameS822,
  int32_t _M0L5indexS825
) {
  struct _M0R104_24clawteam_2fclawteam_2fscheduler_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c817* _closure_2266;
  struct _M0TWssbEu* _M0L14handle__resultS817;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS826;
  void* _M0L11_2atry__errS841;
  struct moonbit_result_0 _tmp_2268;
  int32_t _handle__error__result_2269;
  int32_t _M0L6_2atmpS1934;
  void* _M0L3errS842;
  moonbit_string_t _M0L4nameS844;
  struct _M0DTPC15error5Error103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS845;
  moonbit_string_t _M0L8_2afieldS1949;
  int32_t _M0L6_2acntS2183;
  moonbit_string_t _M0L7_2anameS846;
  #line 526 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  moonbit_incref(_M0L8filenameS822);
  _closure_2266
  = (struct _M0R104_24clawteam_2fclawteam_2fscheduler_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c817*)moonbit_malloc(sizeof(struct _M0R104_24clawteam_2fclawteam_2fscheduler_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c817));
  Moonbit_object_header(_closure_2266)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R104_24clawteam_2fclawteam_2fscheduler_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c817, $1) >> 2, 1, 0);
  _closure_2266->code
  = &_M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__executeN14handle__resultS817;
  _closure_2266->$0 = _M0L5indexS825;
  _closure_2266->$1 = _M0L8filenameS822;
  _M0L14handle__resultS817 = (struct _M0TWssbEu*)_closure_2266;
  _M0L17error__to__stringS826
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__executeN17error__to__stringS826$closure.data;
  moonbit_incref(_M0L12async__testsS847);
  moonbit_incref(_M0L17error__to__stringS826);
  moonbit_incref(_M0L8filenameS822);
  moonbit_incref(_M0L14handle__resultS817);
  #line 560 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _tmp_2268
  = _M0IP38clawteam8clawteam9scheduler41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__test(_M0L12async__testsS847, _M0L8filenameS822, _M0L5indexS825, _M0L14handle__resultS817, _M0L17error__to__stringS826);
  if (_tmp_2268.tag) {
    int32_t const _M0L5_2aokS1943 = _tmp_2268.data.ok;
    _handle__error__result_2269 = _M0L5_2aokS1943;
  } else {
    void* const _M0L6_2aerrS1944 = _tmp_2268.data.err;
    moonbit_decref(_M0L12async__testsS847);
    moonbit_decref(_M0L17error__to__stringS826);
    moonbit_decref(_M0L8filenameS822);
    _M0L11_2atry__errS841 = _M0L6_2aerrS1944;
    goto join_840;
  }
  if (_handle__error__result_2269) {
    moonbit_decref(_M0L12async__testsS847);
    moonbit_decref(_M0L17error__to__stringS826);
    moonbit_decref(_M0L8filenameS822);
    _M0L6_2atmpS1934 = 1;
  } else {
    struct moonbit_result_0 _tmp_2270;
    int32_t _handle__error__result_2271;
    moonbit_incref(_M0L12async__testsS847);
    moonbit_incref(_M0L17error__to__stringS826);
    moonbit_incref(_M0L8filenameS822);
    moonbit_incref(_M0L14handle__resultS817);
    #line 563 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
    _tmp_2270
    = _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS847, _M0L8filenameS822, _M0L5indexS825, _M0L14handle__resultS817, _M0L17error__to__stringS826);
    if (_tmp_2270.tag) {
      int32_t const _M0L5_2aokS1941 = _tmp_2270.data.ok;
      _handle__error__result_2271 = _M0L5_2aokS1941;
    } else {
      void* const _M0L6_2aerrS1942 = _tmp_2270.data.err;
      moonbit_decref(_M0L12async__testsS847);
      moonbit_decref(_M0L17error__to__stringS826);
      moonbit_decref(_M0L8filenameS822);
      _M0L11_2atry__errS841 = _M0L6_2aerrS1942;
      goto join_840;
    }
    if (_handle__error__result_2271) {
      moonbit_decref(_M0L12async__testsS847);
      moonbit_decref(_M0L17error__to__stringS826);
      moonbit_decref(_M0L8filenameS822);
      _M0L6_2atmpS1934 = 1;
    } else {
      struct moonbit_result_0 _tmp_2272;
      int32_t _handle__error__result_2273;
      moonbit_incref(_M0L12async__testsS847);
      moonbit_incref(_M0L17error__to__stringS826);
      moonbit_incref(_M0L8filenameS822);
      moonbit_incref(_M0L14handle__resultS817);
      #line 566 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _tmp_2272
      = _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS847, _M0L8filenameS822, _M0L5indexS825, _M0L14handle__resultS817, _M0L17error__to__stringS826);
      if (_tmp_2272.tag) {
        int32_t const _M0L5_2aokS1939 = _tmp_2272.data.ok;
        _handle__error__result_2273 = _M0L5_2aokS1939;
      } else {
        void* const _M0L6_2aerrS1940 = _tmp_2272.data.err;
        moonbit_decref(_M0L12async__testsS847);
        moonbit_decref(_M0L17error__to__stringS826);
        moonbit_decref(_M0L8filenameS822);
        _M0L11_2atry__errS841 = _M0L6_2aerrS1940;
        goto join_840;
      }
      if (_handle__error__result_2273) {
        moonbit_decref(_M0L12async__testsS847);
        moonbit_decref(_M0L17error__to__stringS826);
        moonbit_decref(_M0L8filenameS822);
        _M0L6_2atmpS1934 = 1;
      } else {
        struct moonbit_result_0 _tmp_2274;
        int32_t _handle__error__result_2275;
        moonbit_incref(_M0L12async__testsS847);
        moonbit_incref(_M0L17error__to__stringS826);
        moonbit_incref(_M0L8filenameS822);
        moonbit_incref(_M0L14handle__resultS817);
        #line 569 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        _tmp_2274
        = _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS847, _M0L8filenameS822, _M0L5indexS825, _M0L14handle__resultS817, _M0L17error__to__stringS826);
        if (_tmp_2274.tag) {
          int32_t const _M0L5_2aokS1937 = _tmp_2274.data.ok;
          _handle__error__result_2275 = _M0L5_2aokS1937;
        } else {
          void* const _M0L6_2aerrS1938 = _tmp_2274.data.err;
          moonbit_decref(_M0L12async__testsS847);
          moonbit_decref(_M0L17error__to__stringS826);
          moonbit_decref(_M0L8filenameS822);
          _M0L11_2atry__errS841 = _M0L6_2aerrS1938;
          goto join_840;
        }
        if (_handle__error__result_2275) {
          moonbit_decref(_M0L12async__testsS847);
          moonbit_decref(_M0L17error__to__stringS826);
          moonbit_decref(_M0L8filenameS822);
          _M0L6_2atmpS1934 = 1;
        } else {
          struct moonbit_result_0 _tmp_2276;
          moonbit_incref(_M0L14handle__resultS817);
          #line 572 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
          _tmp_2276
          = _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS847, _M0L8filenameS822, _M0L5indexS825, _M0L14handle__resultS817, _M0L17error__to__stringS826);
          if (_tmp_2276.tag) {
            int32_t const _M0L5_2aokS1935 = _tmp_2276.data.ok;
            _M0L6_2atmpS1934 = _M0L5_2aokS1935;
          } else {
            void* const _M0L6_2aerrS1936 = _tmp_2276.data.err;
            _M0L11_2atry__errS841 = _M0L6_2aerrS1936;
            goto join_840;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS1934) {
    void* _M0L103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1945 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1945)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1945)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS841
    = _M0L103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1945;
    goto join_840;
  } else {
    moonbit_decref(_M0L14handle__resultS817);
  }
  goto joinlet_2267;
  join_840:;
  _M0L3errS842 = _M0L11_2atry__errS841;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS845
  = (struct _M0DTPC15error5Error103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS842;
  _M0L8_2afieldS1949 = _M0L36_2aMoonBitTestDriverInternalSkipTestS845->$0;
  _M0L6_2acntS2183
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS845)->rc;
  if (_M0L6_2acntS2183 > 1) {
    int32_t _M0L11_2anew__cntS2184 = _M0L6_2acntS2183 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS845)->rc
    = _M0L11_2anew__cntS2184;
    moonbit_incref(_M0L8_2afieldS1949);
  } else if (_M0L6_2acntS2183 == 1) {
    #line 579 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS845);
  }
  _M0L7_2anameS846 = _M0L8_2afieldS1949;
  _M0L4nameS844 = _M0L7_2anameS846;
  goto join_843;
  goto joinlet_2277;
  join_843:;
  #line 580 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__executeN14handle__resultS817(_M0L14handle__resultS817, _M0L4nameS844, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_2277:;
  joinlet_2267:;
  return 0;
}

moonbit_string_t _M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__executeN17error__to__stringS826(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS1933,
  void* _M0L3errS827
) {
  void* _M0L1eS829;
  moonbit_string_t _M0L1eS831;
  #line 549 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L6_2aenvS1933);
  switch (Moonbit_object_tag(_M0L3errS827)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS832 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS827;
      moonbit_string_t _M0L8_2afieldS1950 = _M0L10_2aFailureS832->$0;
      int32_t _M0L6_2acntS2185 =
        Moonbit_object_header(_M0L10_2aFailureS832)->rc;
      moonbit_string_t _M0L4_2aeS833;
      if (_M0L6_2acntS2185 > 1) {
        int32_t _M0L11_2anew__cntS2186 = _M0L6_2acntS2185 - 1;
        Moonbit_object_header(_M0L10_2aFailureS832)->rc
        = _M0L11_2anew__cntS2186;
        moonbit_incref(_M0L8_2afieldS1950);
      } else if (_M0L6_2acntS2185 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L10_2aFailureS832);
      }
      _M0L4_2aeS833 = _M0L8_2afieldS1950;
      _M0L1eS831 = _M0L4_2aeS833;
      goto join_830;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS834 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS827;
      moonbit_string_t _M0L8_2afieldS1951 = _M0L15_2aInspectErrorS834->$0;
      int32_t _M0L6_2acntS2187 =
        Moonbit_object_header(_M0L15_2aInspectErrorS834)->rc;
      moonbit_string_t _M0L4_2aeS835;
      if (_M0L6_2acntS2187 > 1) {
        int32_t _M0L11_2anew__cntS2188 = _M0L6_2acntS2187 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS834)->rc
        = _M0L11_2anew__cntS2188;
        moonbit_incref(_M0L8_2afieldS1951);
      } else if (_M0L6_2acntS2187 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS834);
      }
      _M0L4_2aeS835 = _M0L8_2afieldS1951;
      _M0L1eS831 = _M0L4_2aeS835;
      goto join_830;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS836 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS827;
      moonbit_string_t _M0L8_2afieldS1952 = _M0L16_2aSnapshotErrorS836->$0;
      int32_t _M0L6_2acntS2189 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS836)->rc;
      moonbit_string_t _M0L4_2aeS837;
      if (_M0L6_2acntS2189 > 1) {
        int32_t _M0L11_2anew__cntS2190 = _M0L6_2acntS2189 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS836)->rc
        = _M0L11_2anew__cntS2190;
        moonbit_incref(_M0L8_2afieldS1952);
      } else if (_M0L6_2acntS2189 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS836);
      }
      _M0L4_2aeS837 = _M0L8_2afieldS1952;
      _M0L1eS831 = _M0L4_2aeS837;
      goto join_830;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error101clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS838 =
        (struct _M0DTPC15error5Error101clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS827;
      moonbit_string_t _M0L8_2afieldS1953 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS838->$0;
      int32_t _M0L6_2acntS2191 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS838)->rc;
      moonbit_string_t _M0L4_2aeS839;
      if (_M0L6_2acntS2191 > 1) {
        int32_t _M0L11_2anew__cntS2192 = _M0L6_2acntS2191 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS838)->rc
        = _M0L11_2anew__cntS2192;
        moonbit_incref(_M0L8_2afieldS1953);
      } else if (_M0L6_2acntS2191 == 1) {
        #line 550 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS838);
      }
      _M0L4_2aeS839 = _M0L8_2afieldS1953;
      _M0L1eS831 = _M0L4_2aeS839;
      goto join_830;
      break;
    }
    default: {
      _M0L1eS829 = _M0L3errS827;
      goto join_828;
      break;
    }
  }
  join_830:;
  return _M0L1eS831;
  join_828:;
  #line 555 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS829);
}

int32_t _M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__executeN14handle__resultS817(
  struct _M0TWssbEu* _M0L6_2aenvS1919,
  moonbit_string_t _M0L8testnameS818,
  moonbit_string_t _M0L7messageS819,
  int32_t _M0L7skippedS820
) {
  struct _M0R104_24clawteam_2fclawteam_2fscheduler_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c817* _M0L14_2acasted__envS1920;
  moonbit_string_t _M0L8_2afieldS1963;
  moonbit_string_t _M0L8filenameS822;
  int32_t _M0L8_2afieldS1962;
  int32_t _M0L6_2acntS2193;
  int32_t _M0L5indexS825;
  int32_t _if__result_2280;
  moonbit_string_t _M0L10file__nameS821;
  moonbit_string_t _M0L10test__nameS823;
  moonbit_string_t _M0L7messageS824;
  moonbit_string_t _M0L6_2atmpS1932;
  moonbit_string_t _M0L6_2atmpS1961;
  moonbit_string_t _M0L6_2atmpS1931;
  moonbit_string_t _M0L6_2atmpS1960;
  moonbit_string_t _M0L6_2atmpS1929;
  moonbit_string_t _M0L6_2atmpS1930;
  moonbit_string_t _M0L6_2atmpS1959;
  moonbit_string_t _M0L6_2atmpS1928;
  moonbit_string_t _M0L6_2atmpS1958;
  moonbit_string_t _M0L6_2atmpS1926;
  moonbit_string_t _M0L6_2atmpS1927;
  moonbit_string_t _M0L6_2atmpS1957;
  moonbit_string_t _M0L6_2atmpS1925;
  moonbit_string_t _M0L6_2atmpS1956;
  moonbit_string_t _M0L6_2atmpS1923;
  moonbit_string_t _M0L6_2atmpS1924;
  moonbit_string_t _M0L6_2atmpS1955;
  moonbit_string_t _M0L6_2atmpS1922;
  moonbit_string_t _M0L6_2atmpS1954;
  moonbit_string_t _M0L6_2atmpS1921;
  #line 533 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS1920
  = (struct _M0R104_24clawteam_2fclawteam_2fscheduler_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c817*)_M0L6_2aenvS1919;
  _M0L8_2afieldS1963 = _M0L14_2acasted__envS1920->$1;
  _M0L8filenameS822 = _M0L8_2afieldS1963;
  _M0L8_2afieldS1962 = _M0L14_2acasted__envS1920->$0;
  _M0L6_2acntS2193 = Moonbit_object_header(_M0L14_2acasted__envS1920)->rc;
  if (_M0L6_2acntS2193 > 1) {
    int32_t _M0L11_2anew__cntS2194 = _M0L6_2acntS2193 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1920)->rc
    = _M0L11_2anew__cntS2194;
    moonbit_incref(_M0L8filenameS822);
  } else if (_M0L6_2acntS2193 == 1) {
    #line 533 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1920);
  }
  _M0L5indexS825 = _M0L8_2afieldS1962;
  if (!_M0L7skippedS820) {
    _if__result_2280 = 1;
  } else {
    _if__result_2280 = 0;
  }
  if (_if__result_2280) {
    
  }
  #line 539 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L10file__nameS821 = _M0MPC16string6String6escape(_M0L8filenameS822);
  #line 540 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L10test__nameS823 = _M0MPC16string6String6escape(_M0L8testnameS818);
  #line 541 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L7messageS824 = _M0MPC16string6String6escape(_M0L7messageS819);
  #line 542 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 544 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1932
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS821);
  #line 543 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1961
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS1932);
  moonbit_decref(_M0L6_2atmpS1932);
  _M0L6_2atmpS1931 = _M0L6_2atmpS1961;
  #line 543 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1960
  = moonbit_add_string(_M0L6_2atmpS1931, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS1931);
  _M0L6_2atmpS1929 = _M0L6_2atmpS1960;
  #line 544 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1930
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS825);
  #line 543 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1959 = moonbit_add_string(_M0L6_2atmpS1929, _M0L6_2atmpS1930);
  moonbit_decref(_M0L6_2atmpS1929);
  moonbit_decref(_M0L6_2atmpS1930);
  _M0L6_2atmpS1928 = _M0L6_2atmpS1959;
  #line 543 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1958
  = moonbit_add_string(_M0L6_2atmpS1928, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS1928);
  _M0L6_2atmpS1926 = _M0L6_2atmpS1958;
  #line 544 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1927
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS823);
  #line 543 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1957 = moonbit_add_string(_M0L6_2atmpS1926, _M0L6_2atmpS1927);
  moonbit_decref(_M0L6_2atmpS1926);
  moonbit_decref(_M0L6_2atmpS1927);
  _M0L6_2atmpS1925 = _M0L6_2atmpS1957;
  #line 543 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1956
  = moonbit_add_string(_M0L6_2atmpS1925, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS1925);
  _M0L6_2atmpS1923 = _M0L6_2atmpS1956;
  #line 544 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1924
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS824);
  #line 543 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1955 = moonbit_add_string(_M0L6_2atmpS1923, _M0L6_2atmpS1924);
  moonbit_decref(_M0L6_2atmpS1923);
  moonbit_decref(_M0L6_2atmpS1924);
  _M0L6_2atmpS1922 = _M0L6_2atmpS1955;
  #line 543 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1954
  = moonbit_add_string(_M0L6_2atmpS1922, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1922);
  _M0L6_2atmpS1921 = _M0L6_2atmpS1954;
  #line 543 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS1921);
  #line 546 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP38clawteam8clawteam9scheduler41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S816,
  moonbit_string_t _M0L8filenameS813,
  int32_t _M0L5indexS807,
  struct _M0TWssbEu* _M0L14handle__resultS803,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS805
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS783;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS812;
  struct _M0TWEuQRPC15error5Error* _M0L1fS785;
  moonbit_string_t* _M0L5attrsS786;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS806;
  moonbit_string_t _M0L4nameS789;
  moonbit_string_t _M0L4nameS787;
  int32_t _M0L6_2atmpS1918;
  struct _M0TWEOs* _M0L5_2aitS791;
  struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1915__l427__* _closure_2289;
  struct _M0TWEu* _M0L6_2atmpS1909;
  struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1911__l428__* _closure_2290;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS1910;
  struct moonbit_result_0 _result_2291;
  #line 407 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S816);
  moonbit_incref(_M0FP38clawteam8clawteam9scheduler48moonbit__test__driver__internal__no__args__tests);
  #line 414 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS812
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP38clawteam8clawteam9scheduler48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS813);
  if (_M0L7_2abindS812 == 0) {
    struct moonbit_result_0 _result_2282;
    if (_M0L7_2abindS812) {
      moonbit_decref(_M0L7_2abindS812);
    }
    moonbit_decref(_M0L17error__to__stringS805);
    moonbit_decref(_M0L14handle__resultS803);
    _result_2282.tag = 1;
    _result_2282.data.ok = 0;
    return _result_2282;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS814 =
      _M0L7_2abindS812;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS815 =
      _M0L7_2aSomeS814;
    _M0L10index__mapS783 = _M0L13_2aindex__mapS815;
    goto join_782;
  }
  join_782:;
  #line 416 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS806
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS783, _M0L5indexS807);
  if (_M0L7_2abindS806 == 0) {
    struct moonbit_result_0 _result_2284;
    if (_M0L7_2abindS806) {
      moonbit_decref(_M0L7_2abindS806);
    }
    moonbit_decref(_M0L17error__to__stringS805);
    moonbit_decref(_M0L14handle__resultS803);
    _result_2284.tag = 1;
    _result_2284.data.ok = 0;
    return _result_2284;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS808 = _M0L7_2abindS806;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS809 = _M0L7_2aSomeS808;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS1967 = _M0L4_2axS809->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS810 = _M0L8_2afieldS1967;
    moonbit_string_t* _M0L8_2afieldS1966 = _M0L4_2axS809->$1;
    int32_t _M0L6_2acntS2195 = Moonbit_object_header(_M0L4_2axS809)->rc;
    moonbit_string_t* _M0L8_2aattrsS811;
    if (_M0L6_2acntS2195 > 1) {
      int32_t _M0L11_2anew__cntS2196 = _M0L6_2acntS2195 - 1;
      Moonbit_object_header(_M0L4_2axS809)->rc = _M0L11_2anew__cntS2196;
      moonbit_incref(_M0L8_2afieldS1966);
      moonbit_incref(_M0L4_2afS810);
    } else if (_M0L6_2acntS2195 == 1) {
      #line 414 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      moonbit_free(_M0L4_2axS809);
    }
    _M0L8_2aattrsS811 = _M0L8_2afieldS1966;
    _M0L1fS785 = _M0L4_2afS810;
    _M0L5attrsS786 = _M0L8_2aattrsS811;
    goto join_784;
  }
  join_784:;
  _M0L6_2atmpS1918 = Moonbit_array_length(_M0L5attrsS786);
  if (_M0L6_2atmpS1918 >= 1) {
    moonbit_string_t _M0L6_2atmpS1965 = (moonbit_string_t)_M0L5attrsS786[0];
    moonbit_string_t _M0L7_2anameS790 = _M0L6_2atmpS1965;
    moonbit_incref(_M0L7_2anameS790);
    _M0L4nameS789 = _M0L7_2anameS790;
    goto join_788;
  } else {
    _M0L4nameS787 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_2285;
  join_788:;
  _M0L4nameS787 = _M0L4nameS789;
  joinlet_2285:;
  #line 417 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L5_2aitS791 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS786);
  while (1) {
    moonbit_string_t _M0L4attrS793;
    moonbit_string_t _M0L7_2abindS800;
    int32_t _M0L6_2atmpS1902;
    int64_t _M0L6_2atmpS1901;
    moonbit_incref(_M0L5_2aitS791);
    #line 419 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
    _M0L7_2abindS800 = _M0MPB4Iter4nextGsE(_M0L5_2aitS791);
    if (_M0L7_2abindS800 == 0) {
      if (_M0L7_2abindS800) {
        moonbit_decref(_M0L7_2abindS800);
      }
      moonbit_decref(_M0L5_2aitS791);
    } else {
      moonbit_string_t _M0L7_2aSomeS801 = _M0L7_2abindS800;
      moonbit_string_t _M0L7_2aattrS802 = _M0L7_2aSomeS801;
      _M0L4attrS793 = _M0L7_2aattrS802;
      goto join_792;
    }
    goto joinlet_2287;
    join_792:;
    _M0L6_2atmpS1902 = Moonbit_array_length(_M0L4attrS793);
    _M0L6_2atmpS1901 = (int64_t)_M0L6_2atmpS1902;
    moonbit_incref(_M0L4attrS793);
    #line 420 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS793, 5, 0, _M0L6_2atmpS1901)
    ) {
      int32_t _M0L6_2atmpS1908 = _M0L4attrS793[0];
      int32_t _M0L4_2axS794 = _M0L6_2atmpS1908;
      if (_M0L4_2axS794 == 112) {
        int32_t _M0L6_2atmpS1907 = _M0L4attrS793[1];
        int32_t _M0L4_2axS795 = _M0L6_2atmpS1907;
        if (_M0L4_2axS795 == 97) {
          int32_t _M0L6_2atmpS1906 = _M0L4attrS793[2];
          int32_t _M0L4_2axS796 = _M0L6_2atmpS1906;
          if (_M0L4_2axS796 == 110) {
            int32_t _M0L6_2atmpS1905 = _M0L4attrS793[3];
            int32_t _M0L4_2axS797 = _M0L6_2atmpS1905;
            if (_M0L4_2axS797 == 105) {
              int32_t _M0L6_2atmpS1964 = _M0L4attrS793[4];
              int32_t _M0L6_2atmpS1904;
              int32_t _M0L4_2axS798;
              moonbit_decref(_M0L4attrS793);
              _M0L6_2atmpS1904 = _M0L6_2atmpS1964;
              _M0L4_2axS798 = _M0L6_2atmpS1904;
              if (_M0L4_2axS798 == 99) {
                void* _M0L103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1903;
                struct moonbit_result_0 _result_2288;
                moonbit_decref(_M0L17error__to__stringS805);
                moonbit_decref(_M0L14handle__resultS803);
                moonbit_decref(_M0L5_2aitS791);
                moonbit_decref(_M0L1fS785);
                _M0L103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1903
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1903)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1903)->$0
                = _M0L4nameS787;
                _result_2288.tag = 0;
                _result_2288.data.err
                = _M0L103clawteam_2fclawteam_2fscheduler_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS1903;
                return _result_2288;
              }
            } else {
              moonbit_decref(_M0L4attrS793);
            }
          } else {
            moonbit_decref(_M0L4attrS793);
          }
        } else {
          moonbit_decref(_M0L4attrS793);
        }
      } else {
        moonbit_decref(_M0L4attrS793);
      }
    } else {
      moonbit_decref(_M0L4attrS793);
    }
    continue;
    joinlet_2287:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS803);
  moonbit_incref(_M0L4nameS787);
  _closure_2289
  = (struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1915__l427__*)moonbit_malloc(sizeof(struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1915__l427__));
  Moonbit_object_header(_closure_2289)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1915__l427__, $0) >> 2, 2, 0);
  _closure_2289->code
  = &_M0IP38clawteam8clawteam9scheduler41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testC1915l427;
  _closure_2289->$0 = _M0L14handle__resultS803;
  _closure_2289->$1 = _M0L4nameS787;
  _M0L6_2atmpS1909 = (struct _M0TWEu*)_closure_2289;
  _closure_2290
  = (struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1911__l428__*)moonbit_malloc(sizeof(struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1911__l428__));
  Moonbit_object_header(_closure_2290)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1911__l428__, $0) >> 2, 3, 0);
  _closure_2290->code
  = &_M0IP38clawteam8clawteam9scheduler41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testC1911l428;
  _closure_2290->$0 = _M0L17error__to__stringS805;
  _closure_2290->$1 = _M0L14handle__resultS803;
  _closure_2290->$2 = _M0L4nameS787;
  _M0L6_2atmpS1910 = (struct _M0TWRPC15error5ErrorEu*)_closure_2290;
  #line 425 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0FP38clawteam8clawteam9scheduler45moonbit__test__driver__internal__catch__error(_M0L1fS785, _M0L6_2atmpS1909, _M0L6_2atmpS1910);
  _result_2291.tag = 1;
  _result_2291.data.ok = 1;
  return _result_2291;
}

int32_t _M0IP38clawteam8clawteam9scheduler41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testC1915l427(
  struct _M0TWEu* _M0L6_2aenvS1916
) {
  struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1915__l427__* _M0L14_2acasted__envS1917;
  moonbit_string_t _M0L8_2afieldS1969;
  moonbit_string_t _M0L4nameS787;
  struct _M0TWssbEu* _M0L8_2afieldS1968;
  int32_t _M0L6_2acntS2197;
  struct _M0TWssbEu* _M0L14handle__resultS803;
  #line 427 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS1917
  = (struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1915__l427__*)_M0L6_2aenvS1916;
  _M0L8_2afieldS1969 = _M0L14_2acasted__envS1917->$1;
  _M0L4nameS787 = _M0L8_2afieldS1969;
  _M0L8_2afieldS1968 = _M0L14_2acasted__envS1917->$0;
  _M0L6_2acntS2197 = Moonbit_object_header(_M0L14_2acasted__envS1917)->rc;
  if (_M0L6_2acntS2197 > 1) {
    int32_t _M0L11_2anew__cntS2198 = _M0L6_2acntS2197 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1917)->rc
    = _M0L11_2anew__cntS2198;
    moonbit_incref(_M0L4nameS787);
    moonbit_incref(_M0L8_2afieldS1968);
  } else if (_M0L6_2acntS2197 == 1) {
    #line 427 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1917);
  }
  _M0L14handle__resultS803 = _M0L8_2afieldS1968;
  #line 427 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L14handle__resultS803->code(_M0L14handle__resultS803, _M0L4nameS787, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP38clawteam8clawteam9scheduler41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testC1911l428(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS1912,
  void* _M0L3errS804
) {
  struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1911__l428__* _M0L14_2acasted__envS1913;
  moonbit_string_t _M0L8_2afieldS1972;
  moonbit_string_t _M0L4nameS787;
  struct _M0TWssbEu* _M0L8_2afieldS1971;
  struct _M0TWssbEu* _M0L14handle__resultS803;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS1970;
  int32_t _M0L6_2acntS2199;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS805;
  moonbit_string_t _M0L6_2atmpS1914;
  #line 428 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L14_2acasted__envS1913
  = (struct _M0R179_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fscheduler_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u1911__l428__*)_M0L6_2aenvS1912;
  _M0L8_2afieldS1972 = _M0L14_2acasted__envS1913->$2;
  _M0L4nameS787 = _M0L8_2afieldS1972;
  _M0L8_2afieldS1971 = _M0L14_2acasted__envS1913->$1;
  _M0L14handle__resultS803 = _M0L8_2afieldS1971;
  _M0L8_2afieldS1970 = _M0L14_2acasted__envS1913->$0;
  _M0L6_2acntS2199 = Moonbit_object_header(_M0L14_2acasted__envS1913)->rc;
  if (_M0L6_2acntS2199 > 1) {
    int32_t _M0L11_2anew__cntS2200 = _M0L6_2acntS2199 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1913)->rc
    = _M0L11_2anew__cntS2200;
    moonbit_incref(_M0L4nameS787);
    moonbit_incref(_M0L14handle__resultS803);
    moonbit_incref(_M0L8_2afieldS1970);
  } else if (_M0L6_2acntS2199 == 1) {
    #line 428 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS1913);
  }
  _M0L17error__to__stringS805 = _M0L8_2afieldS1970;
  #line 428 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1914
  = _M0L17error__to__stringS805->code(_M0L17error__to__stringS805, _M0L3errS804);
  #line 428 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L14handle__resultS803->code(_M0L14handle__resultS803, _M0L4nameS787, _M0L6_2atmpS1914, 0);
  return 0;
}

int32_t _M0FP38clawteam8clawteam9scheduler45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS778,
  struct _M0TWEu* _M0L6on__okS779,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS776
) {
  void* _M0L11_2atry__errS774;
  struct moonbit_result_0 _tmp_2293;
  void* _M0L3errS775;
  #line 375 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _tmp_2293 = _M0L1fS778->code(_M0L1fS778);
  if (_tmp_2293.tag) {
    int32_t const _M0L5_2aokS1899 = _tmp_2293.data.ok;
    moonbit_decref(_M0L7on__errS776);
  } else {
    void* const _M0L6_2aerrS1900 = _tmp_2293.data.err;
    moonbit_decref(_M0L6on__okS779);
    _M0L11_2atry__errS774 = _M0L6_2aerrS1900;
    goto join_773;
  }
  #line 382 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6on__okS779->code(_M0L6on__okS779);
  goto joinlet_2292;
  join_773:;
  _M0L3errS775 = _M0L11_2atry__errS774;
  #line 383 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L7on__errS776->code(_M0L7on__errS776, _M0L3errS775);
  joinlet_2292:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S733;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS739;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS746;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS751;
  struct _M0TUsiE** _M0L6_2atmpS1898;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS758;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS759;
  moonbit_string_t _M0L6_2atmpS1897;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS760;
  int32_t _M0L7_2abindS761;
  int32_t _M0L2__S762;
  #line 193 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S733 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS739 = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS746
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS739;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS751 = 0;
  _M0L6_2atmpS1898 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS758
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS758)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS758->$0 = _M0L6_2atmpS1898;
  _M0L16file__and__indexS758->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L9cli__argsS759
  = _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS746(_M0L57moonbit__test__driver__internal__get__cli__args__internalS746);
  #line 284 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1897 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS759, 1);
  #line 283 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L10test__argsS760
  = _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS751(_M0L51moonbit__test__driver__internal__split__mbt__stringS751, _M0L6_2atmpS1897, 47);
  _M0L7_2abindS761 = _M0L10test__argsS760->$1;
  _M0L2__S762 = 0;
  while (1) {
    if (_M0L2__S762 < _M0L7_2abindS761) {
      moonbit_string_t* _M0L8_2afieldS1974 = _M0L10test__argsS760->$0;
      moonbit_string_t* _M0L3bufS1896 = _M0L8_2afieldS1974;
      moonbit_string_t _M0L6_2atmpS1973 =
        (moonbit_string_t)_M0L3bufS1896[_M0L2__S762];
      moonbit_string_t _M0L3argS763 = _M0L6_2atmpS1973;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS764;
      moonbit_string_t _M0L4fileS765;
      moonbit_string_t _M0L5rangeS766;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS767;
      moonbit_string_t _M0L6_2atmpS1894;
      int32_t _M0L5startS768;
      moonbit_string_t _M0L6_2atmpS1893;
      int32_t _M0L3endS769;
      int32_t _M0L1iS770;
      int32_t _M0L6_2atmpS1895;
      moonbit_incref(_M0L3argS763);
      #line 288 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _M0L16file__and__rangeS764
      = _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS751(_M0L51moonbit__test__driver__internal__split__mbt__stringS751, _M0L3argS763, 58);
      moonbit_incref(_M0L16file__and__rangeS764);
      #line 289 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _M0L4fileS765
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS764, 0);
      #line 290 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _M0L5rangeS766
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS764, 1);
      #line 291 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _M0L15start__and__endS767
      = _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS751(_M0L51moonbit__test__driver__internal__split__mbt__stringS751, _M0L5rangeS766, 45);
      moonbit_incref(_M0L15start__and__endS767);
      #line 294 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS1894
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS767, 0);
      #line 294 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _M0L5startS768
      = _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S733(_M0L45moonbit__test__driver__internal__parse__int__S733, _M0L6_2atmpS1894);
      #line 295 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS1893
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS767, 1);
      #line 295 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _M0L3endS769
      = _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S733(_M0L45moonbit__test__driver__internal__parse__int__S733, _M0L6_2atmpS1893);
      _M0L1iS770 = _M0L5startS768;
      while (1) {
        if (_M0L1iS770 < _M0L3endS769) {
          struct _M0TUsiE* _M0L8_2atupleS1891;
          int32_t _M0L6_2atmpS1892;
          moonbit_incref(_M0L4fileS765);
          _M0L8_2atupleS1891
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS1891)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS1891->$0 = _M0L4fileS765;
          _M0L8_2atupleS1891->$1 = _M0L1iS770;
          moonbit_incref(_M0L16file__and__indexS758);
          #line 297 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS758, _M0L8_2atupleS1891);
          _M0L6_2atmpS1892 = _M0L1iS770 + 1;
          _M0L1iS770 = _M0L6_2atmpS1892;
          continue;
        } else {
          moonbit_decref(_M0L4fileS765);
        }
        break;
      }
      _M0L6_2atmpS1895 = _M0L2__S762 + 1;
      _M0L2__S762 = _M0L6_2atmpS1895;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS760);
    }
    break;
  }
  return _M0L16file__and__indexS758;
}

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS751(
  int32_t _M0L6_2aenvS1872,
  moonbit_string_t _M0L1sS752,
  int32_t _M0L3sepS753
) {
  moonbit_string_t* _M0L6_2atmpS1890;
  struct _M0TPB5ArrayGsE* _M0L3resS754;
  struct _M0TPC13ref3RefGiE* _M0L1iS755;
  struct _M0TPC13ref3RefGiE* _M0L5startS756;
  int32_t _M0L3valS1885;
  int32_t _M0L6_2atmpS1886;
  #line 261 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS1890 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS754
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS754)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS754->$0 = _M0L6_2atmpS1890;
  _M0L3resS754->$1 = 0;
  _M0L1iS755
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS755)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS755->$0 = 0;
  _M0L5startS756
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS756)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS756->$0 = 0;
  while (1) {
    int32_t _M0L3valS1873 = _M0L1iS755->$0;
    int32_t _M0L6_2atmpS1874 = Moonbit_array_length(_M0L1sS752);
    if (_M0L3valS1873 < _M0L6_2atmpS1874) {
      int32_t _M0L3valS1877 = _M0L1iS755->$0;
      int32_t _M0L6_2atmpS1876;
      int32_t _M0L6_2atmpS1875;
      int32_t _M0L3valS1884;
      int32_t _M0L6_2atmpS1883;
      if (
        _M0L3valS1877 < 0
        || _M0L3valS1877 >= Moonbit_array_length(_M0L1sS752)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1876 = _M0L1sS752[_M0L3valS1877];
      _M0L6_2atmpS1875 = _M0L6_2atmpS1876;
      if (_M0L6_2atmpS1875 == _M0L3sepS753) {
        int32_t _M0L3valS1879 = _M0L5startS756->$0;
        int32_t _M0L3valS1880 = _M0L1iS755->$0;
        moonbit_string_t _M0L6_2atmpS1878;
        int32_t _M0L3valS1882;
        int32_t _M0L6_2atmpS1881;
        moonbit_incref(_M0L1sS752);
        #line 270 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        _M0L6_2atmpS1878
        = _M0MPC16string6String17unsafe__substring(_M0L1sS752, _M0L3valS1879, _M0L3valS1880);
        moonbit_incref(_M0L3resS754);
        #line 270 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS754, _M0L6_2atmpS1878);
        _M0L3valS1882 = _M0L1iS755->$0;
        _M0L6_2atmpS1881 = _M0L3valS1882 + 1;
        _M0L5startS756->$0 = _M0L6_2atmpS1881;
      }
      _M0L3valS1884 = _M0L1iS755->$0;
      _M0L6_2atmpS1883 = _M0L3valS1884 + 1;
      _M0L1iS755->$0 = _M0L6_2atmpS1883;
      continue;
    } else {
      moonbit_decref(_M0L1iS755);
    }
    break;
  }
  _M0L3valS1885 = _M0L5startS756->$0;
  _M0L6_2atmpS1886 = Moonbit_array_length(_M0L1sS752);
  if (_M0L3valS1885 < _M0L6_2atmpS1886) {
    int32_t _M0L8_2afieldS1975 = _M0L5startS756->$0;
    int32_t _M0L3valS1888;
    int32_t _M0L6_2atmpS1889;
    moonbit_string_t _M0L6_2atmpS1887;
    moonbit_decref(_M0L5startS756);
    _M0L3valS1888 = _M0L8_2afieldS1975;
    _M0L6_2atmpS1889 = Moonbit_array_length(_M0L1sS752);
    #line 276 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
    _M0L6_2atmpS1887
    = _M0MPC16string6String17unsafe__substring(_M0L1sS752, _M0L3valS1888, _M0L6_2atmpS1889);
    moonbit_incref(_M0L3resS754);
    #line 276 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS754, _M0L6_2atmpS1887);
  } else {
    moonbit_decref(_M0L5startS756);
    moonbit_decref(_M0L1sS752);
  }
  return _M0L3resS754;
}

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS746(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS739
) {
  moonbit_bytes_t* _M0L3tmpS747;
  int32_t _M0L6_2atmpS1871;
  struct _M0TPB5ArrayGsE* _M0L3resS748;
  int32_t _M0L1iS749;
  #line 250 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L3tmpS747
  = _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS1871 = Moonbit_array_length(_M0L3tmpS747);
  #line 254 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L3resS748 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS1871);
  _M0L1iS749 = 0;
  while (1) {
    int32_t _M0L6_2atmpS1867 = Moonbit_array_length(_M0L3tmpS747);
    if (_M0L1iS749 < _M0L6_2atmpS1867) {
      moonbit_bytes_t _M0L6_2atmpS1976;
      moonbit_bytes_t _M0L6_2atmpS1869;
      moonbit_string_t _M0L6_2atmpS1868;
      int32_t _M0L6_2atmpS1870;
      if (_M0L1iS749 < 0 || _M0L1iS749 >= Moonbit_array_length(_M0L3tmpS747)) {
        #line 256 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1976 = (moonbit_bytes_t)_M0L3tmpS747[_M0L1iS749];
      _M0L6_2atmpS1869 = _M0L6_2atmpS1976;
      moonbit_incref(_M0L6_2atmpS1869);
      #line 256 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _M0L6_2atmpS1868
      = _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS739(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS739, _M0L6_2atmpS1869);
      moonbit_incref(_M0L3resS748);
      #line 256 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS748, _M0L6_2atmpS1868);
      _M0L6_2atmpS1870 = _M0L1iS749 + 1;
      _M0L1iS749 = _M0L6_2atmpS1870;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS747);
    }
    break;
  }
  return _M0L3resS748;
}

moonbit_string_t _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS739(
  int32_t _M0L6_2aenvS1781,
  moonbit_bytes_t _M0L5bytesS740
) {
  struct _M0TPB13StringBuilder* _M0L3resS741;
  int32_t _M0L3lenS742;
  struct _M0TPC13ref3RefGiE* _M0L1iS743;
  #line 206 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L3resS741 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS742 = Moonbit_array_length(_M0L5bytesS740);
  _M0L1iS743
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS743)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS743->$0 = 0;
  while (1) {
    int32_t _M0L3valS1782 = _M0L1iS743->$0;
    if (_M0L3valS1782 < _M0L3lenS742) {
      int32_t _M0L3valS1866 = _M0L1iS743->$0;
      int32_t _M0L6_2atmpS1865;
      int32_t _M0L6_2atmpS1864;
      struct _M0TPC13ref3RefGiE* _M0L1cS744;
      int32_t _M0L3valS1783;
      if (
        _M0L3valS1866 < 0
        || _M0L3valS1866 >= Moonbit_array_length(_M0L5bytesS740)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1865 = _M0L5bytesS740[_M0L3valS1866];
      _M0L6_2atmpS1864 = (int32_t)_M0L6_2atmpS1865;
      _M0L1cS744
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS744)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS744->$0 = _M0L6_2atmpS1864;
      _M0L3valS1783 = _M0L1cS744->$0;
      if (_M0L3valS1783 < 128) {
        int32_t _M0L8_2afieldS1977 = _M0L1cS744->$0;
        int32_t _M0L3valS1785;
        int32_t _M0L6_2atmpS1784;
        int32_t _M0L3valS1787;
        int32_t _M0L6_2atmpS1786;
        moonbit_decref(_M0L1cS744);
        _M0L3valS1785 = _M0L8_2afieldS1977;
        _M0L6_2atmpS1784 = _M0L3valS1785;
        moonbit_incref(_M0L3resS741);
        #line 215 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS741, _M0L6_2atmpS1784);
        _M0L3valS1787 = _M0L1iS743->$0;
        _M0L6_2atmpS1786 = _M0L3valS1787 + 1;
        _M0L1iS743->$0 = _M0L6_2atmpS1786;
      } else {
        int32_t _M0L3valS1788 = _M0L1cS744->$0;
        if (_M0L3valS1788 < 224) {
          int32_t _M0L3valS1790 = _M0L1iS743->$0;
          int32_t _M0L6_2atmpS1789 = _M0L3valS1790 + 1;
          int32_t _M0L3valS1799;
          int32_t _M0L6_2atmpS1798;
          int32_t _M0L6_2atmpS1792;
          int32_t _M0L3valS1797;
          int32_t _M0L6_2atmpS1796;
          int32_t _M0L6_2atmpS1795;
          int32_t _M0L6_2atmpS1794;
          int32_t _M0L6_2atmpS1793;
          int32_t _M0L6_2atmpS1791;
          int32_t _M0L8_2afieldS1978;
          int32_t _M0L3valS1801;
          int32_t _M0L6_2atmpS1800;
          int32_t _M0L3valS1803;
          int32_t _M0L6_2atmpS1802;
          if (_M0L6_2atmpS1789 >= _M0L3lenS742) {
            moonbit_decref(_M0L1cS744);
            moonbit_decref(_M0L1iS743);
            moonbit_decref(_M0L5bytesS740);
            break;
          }
          _M0L3valS1799 = _M0L1cS744->$0;
          _M0L6_2atmpS1798 = _M0L3valS1799 & 31;
          _M0L6_2atmpS1792 = _M0L6_2atmpS1798 << 6;
          _M0L3valS1797 = _M0L1iS743->$0;
          _M0L6_2atmpS1796 = _M0L3valS1797 + 1;
          if (
            _M0L6_2atmpS1796 < 0
            || _M0L6_2atmpS1796 >= Moonbit_array_length(_M0L5bytesS740)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1795 = _M0L5bytesS740[_M0L6_2atmpS1796];
          _M0L6_2atmpS1794 = (int32_t)_M0L6_2atmpS1795;
          _M0L6_2atmpS1793 = _M0L6_2atmpS1794 & 63;
          _M0L6_2atmpS1791 = _M0L6_2atmpS1792 | _M0L6_2atmpS1793;
          _M0L1cS744->$0 = _M0L6_2atmpS1791;
          _M0L8_2afieldS1978 = _M0L1cS744->$0;
          moonbit_decref(_M0L1cS744);
          _M0L3valS1801 = _M0L8_2afieldS1978;
          _M0L6_2atmpS1800 = _M0L3valS1801;
          moonbit_incref(_M0L3resS741);
          #line 222 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS741, _M0L6_2atmpS1800);
          _M0L3valS1803 = _M0L1iS743->$0;
          _M0L6_2atmpS1802 = _M0L3valS1803 + 2;
          _M0L1iS743->$0 = _M0L6_2atmpS1802;
        } else {
          int32_t _M0L3valS1804 = _M0L1cS744->$0;
          if (_M0L3valS1804 < 240) {
            int32_t _M0L3valS1806 = _M0L1iS743->$0;
            int32_t _M0L6_2atmpS1805 = _M0L3valS1806 + 2;
            int32_t _M0L3valS1822;
            int32_t _M0L6_2atmpS1821;
            int32_t _M0L6_2atmpS1814;
            int32_t _M0L3valS1820;
            int32_t _M0L6_2atmpS1819;
            int32_t _M0L6_2atmpS1818;
            int32_t _M0L6_2atmpS1817;
            int32_t _M0L6_2atmpS1816;
            int32_t _M0L6_2atmpS1815;
            int32_t _M0L6_2atmpS1808;
            int32_t _M0L3valS1813;
            int32_t _M0L6_2atmpS1812;
            int32_t _M0L6_2atmpS1811;
            int32_t _M0L6_2atmpS1810;
            int32_t _M0L6_2atmpS1809;
            int32_t _M0L6_2atmpS1807;
            int32_t _M0L8_2afieldS1979;
            int32_t _M0L3valS1824;
            int32_t _M0L6_2atmpS1823;
            int32_t _M0L3valS1826;
            int32_t _M0L6_2atmpS1825;
            if (_M0L6_2atmpS1805 >= _M0L3lenS742) {
              moonbit_decref(_M0L1cS744);
              moonbit_decref(_M0L1iS743);
              moonbit_decref(_M0L5bytesS740);
              break;
            }
            _M0L3valS1822 = _M0L1cS744->$0;
            _M0L6_2atmpS1821 = _M0L3valS1822 & 15;
            _M0L6_2atmpS1814 = _M0L6_2atmpS1821 << 12;
            _M0L3valS1820 = _M0L1iS743->$0;
            _M0L6_2atmpS1819 = _M0L3valS1820 + 1;
            if (
              _M0L6_2atmpS1819 < 0
              || _M0L6_2atmpS1819 >= Moonbit_array_length(_M0L5bytesS740)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1818 = _M0L5bytesS740[_M0L6_2atmpS1819];
            _M0L6_2atmpS1817 = (int32_t)_M0L6_2atmpS1818;
            _M0L6_2atmpS1816 = _M0L6_2atmpS1817 & 63;
            _M0L6_2atmpS1815 = _M0L6_2atmpS1816 << 6;
            _M0L6_2atmpS1808 = _M0L6_2atmpS1814 | _M0L6_2atmpS1815;
            _M0L3valS1813 = _M0L1iS743->$0;
            _M0L6_2atmpS1812 = _M0L3valS1813 + 2;
            if (
              _M0L6_2atmpS1812 < 0
              || _M0L6_2atmpS1812 >= Moonbit_array_length(_M0L5bytesS740)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1811 = _M0L5bytesS740[_M0L6_2atmpS1812];
            _M0L6_2atmpS1810 = (int32_t)_M0L6_2atmpS1811;
            _M0L6_2atmpS1809 = _M0L6_2atmpS1810 & 63;
            _M0L6_2atmpS1807 = _M0L6_2atmpS1808 | _M0L6_2atmpS1809;
            _M0L1cS744->$0 = _M0L6_2atmpS1807;
            _M0L8_2afieldS1979 = _M0L1cS744->$0;
            moonbit_decref(_M0L1cS744);
            _M0L3valS1824 = _M0L8_2afieldS1979;
            _M0L6_2atmpS1823 = _M0L3valS1824;
            moonbit_incref(_M0L3resS741);
            #line 231 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS741, _M0L6_2atmpS1823);
            _M0L3valS1826 = _M0L1iS743->$0;
            _M0L6_2atmpS1825 = _M0L3valS1826 + 3;
            _M0L1iS743->$0 = _M0L6_2atmpS1825;
          } else {
            int32_t _M0L3valS1828 = _M0L1iS743->$0;
            int32_t _M0L6_2atmpS1827 = _M0L3valS1828 + 3;
            int32_t _M0L3valS1851;
            int32_t _M0L6_2atmpS1850;
            int32_t _M0L6_2atmpS1843;
            int32_t _M0L3valS1849;
            int32_t _M0L6_2atmpS1848;
            int32_t _M0L6_2atmpS1847;
            int32_t _M0L6_2atmpS1846;
            int32_t _M0L6_2atmpS1845;
            int32_t _M0L6_2atmpS1844;
            int32_t _M0L6_2atmpS1836;
            int32_t _M0L3valS1842;
            int32_t _M0L6_2atmpS1841;
            int32_t _M0L6_2atmpS1840;
            int32_t _M0L6_2atmpS1839;
            int32_t _M0L6_2atmpS1838;
            int32_t _M0L6_2atmpS1837;
            int32_t _M0L6_2atmpS1830;
            int32_t _M0L3valS1835;
            int32_t _M0L6_2atmpS1834;
            int32_t _M0L6_2atmpS1833;
            int32_t _M0L6_2atmpS1832;
            int32_t _M0L6_2atmpS1831;
            int32_t _M0L6_2atmpS1829;
            int32_t _M0L3valS1853;
            int32_t _M0L6_2atmpS1852;
            int32_t _M0L3valS1857;
            int32_t _M0L6_2atmpS1856;
            int32_t _M0L6_2atmpS1855;
            int32_t _M0L6_2atmpS1854;
            int32_t _M0L8_2afieldS1980;
            int32_t _M0L3valS1861;
            int32_t _M0L6_2atmpS1860;
            int32_t _M0L6_2atmpS1859;
            int32_t _M0L6_2atmpS1858;
            int32_t _M0L3valS1863;
            int32_t _M0L6_2atmpS1862;
            if (_M0L6_2atmpS1827 >= _M0L3lenS742) {
              moonbit_decref(_M0L1cS744);
              moonbit_decref(_M0L1iS743);
              moonbit_decref(_M0L5bytesS740);
              break;
            }
            _M0L3valS1851 = _M0L1cS744->$0;
            _M0L6_2atmpS1850 = _M0L3valS1851 & 7;
            _M0L6_2atmpS1843 = _M0L6_2atmpS1850 << 18;
            _M0L3valS1849 = _M0L1iS743->$0;
            _M0L6_2atmpS1848 = _M0L3valS1849 + 1;
            if (
              _M0L6_2atmpS1848 < 0
              || _M0L6_2atmpS1848 >= Moonbit_array_length(_M0L5bytesS740)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1847 = _M0L5bytesS740[_M0L6_2atmpS1848];
            _M0L6_2atmpS1846 = (int32_t)_M0L6_2atmpS1847;
            _M0L6_2atmpS1845 = _M0L6_2atmpS1846 & 63;
            _M0L6_2atmpS1844 = _M0L6_2atmpS1845 << 12;
            _M0L6_2atmpS1836 = _M0L6_2atmpS1843 | _M0L6_2atmpS1844;
            _M0L3valS1842 = _M0L1iS743->$0;
            _M0L6_2atmpS1841 = _M0L3valS1842 + 2;
            if (
              _M0L6_2atmpS1841 < 0
              || _M0L6_2atmpS1841 >= Moonbit_array_length(_M0L5bytesS740)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1840 = _M0L5bytesS740[_M0L6_2atmpS1841];
            _M0L6_2atmpS1839 = (int32_t)_M0L6_2atmpS1840;
            _M0L6_2atmpS1838 = _M0L6_2atmpS1839 & 63;
            _M0L6_2atmpS1837 = _M0L6_2atmpS1838 << 6;
            _M0L6_2atmpS1830 = _M0L6_2atmpS1836 | _M0L6_2atmpS1837;
            _M0L3valS1835 = _M0L1iS743->$0;
            _M0L6_2atmpS1834 = _M0L3valS1835 + 3;
            if (
              _M0L6_2atmpS1834 < 0
              || _M0L6_2atmpS1834 >= Moonbit_array_length(_M0L5bytesS740)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS1833 = _M0L5bytesS740[_M0L6_2atmpS1834];
            _M0L6_2atmpS1832 = (int32_t)_M0L6_2atmpS1833;
            _M0L6_2atmpS1831 = _M0L6_2atmpS1832 & 63;
            _M0L6_2atmpS1829 = _M0L6_2atmpS1830 | _M0L6_2atmpS1831;
            _M0L1cS744->$0 = _M0L6_2atmpS1829;
            _M0L3valS1853 = _M0L1cS744->$0;
            _M0L6_2atmpS1852 = _M0L3valS1853 - 65536;
            _M0L1cS744->$0 = _M0L6_2atmpS1852;
            _M0L3valS1857 = _M0L1cS744->$0;
            _M0L6_2atmpS1856 = _M0L3valS1857 >> 10;
            _M0L6_2atmpS1855 = _M0L6_2atmpS1856 + 55296;
            _M0L6_2atmpS1854 = _M0L6_2atmpS1855;
            moonbit_incref(_M0L3resS741);
            #line 242 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS741, _M0L6_2atmpS1854);
            _M0L8_2afieldS1980 = _M0L1cS744->$0;
            moonbit_decref(_M0L1cS744);
            _M0L3valS1861 = _M0L8_2afieldS1980;
            _M0L6_2atmpS1860 = _M0L3valS1861 & 1023;
            _M0L6_2atmpS1859 = _M0L6_2atmpS1860 + 56320;
            _M0L6_2atmpS1858 = _M0L6_2atmpS1859;
            moonbit_incref(_M0L3resS741);
            #line 243 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS741, _M0L6_2atmpS1858);
            _M0L3valS1863 = _M0L1iS743->$0;
            _M0L6_2atmpS1862 = _M0L3valS1863 + 4;
            _M0L1iS743->$0 = _M0L6_2atmpS1862;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS743);
      moonbit_decref(_M0L5bytesS740);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS741);
}

int32_t _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S733(
  int32_t _M0L6_2aenvS1774,
  moonbit_string_t _M0L1sS734
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS735;
  int32_t _M0L3lenS736;
  int32_t _M0L1iS737;
  int32_t _M0L8_2afieldS1981;
  #line 197 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L3resS735
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS735)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS735->$0 = 0;
  _M0L3lenS736 = Moonbit_array_length(_M0L1sS734);
  _M0L1iS737 = 0;
  while (1) {
    if (_M0L1iS737 < _M0L3lenS736) {
      int32_t _M0L3valS1779 = _M0L3resS735->$0;
      int32_t _M0L6_2atmpS1776 = _M0L3valS1779 * 10;
      int32_t _M0L6_2atmpS1778;
      int32_t _M0L6_2atmpS1777;
      int32_t _M0L6_2atmpS1775;
      int32_t _M0L6_2atmpS1780;
      if (_M0L1iS737 < 0 || _M0L1iS737 >= Moonbit_array_length(_M0L1sS734)) {
        #line 201 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1778 = _M0L1sS734[_M0L1iS737];
      _M0L6_2atmpS1777 = _M0L6_2atmpS1778 - 48;
      _M0L6_2atmpS1775 = _M0L6_2atmpS1776 + _M0L6_2atmpS1777;
      _M0L3resS735->$0 = _M0L6_2atmpS1775;
      _M0L6_2atmpS1780 = _M0L1iS737 + 1;
      _M0L1iS737 = _M0L6_2atmpS1780;
      continue;
    } else {
      moonbit_decref(_M0L1sS734);
    }
    break;
  }
  _M0L8_2afieldS1981 = _M0L3resS735->$0;
  moonbit_decref(_M0L3resS735);
  return _M0L8_2afieldS1981;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S713,
  moonbit_string_t _M0L12_2adiscard__S714,
  int32_t _M0L12_2adiscard__S715,
  struct _M0TWssbEu* _M0L12_2adiscard__S716,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S717
) {
  struct moonbit_result_0 _result_2300;
  #line 34 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S717);
  moonbit_decref(_M0L12_2adiscard__S716);
  moonbit_decref(_M0L12_2adiscard__S714);
  moonbit_decref(_M0L12_2adiscard__S713);
  _result_2300.tag = 1;
  _result_2300.data.ok = 0;
  return _result_2300;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S718,
  moonbit_string_t _M0L12_2adiscard__S719,
  int32_t _M0L12_2adiscard__S720,
  struct _M0TWssbEu* _M0L12_2adiscard__S721,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S722
) {
  struct moonbit_result_0 _result_2301;
  #line 34 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S722);
  moonbit_decref(_M0L12_2adiscard__S721);
  moonbit_decref(_M0L12_2adiscard__S719);
  moonbit_decref(_M0L12_2adiscard__S718);
  _result_2301.tag = 1;
  _result_2301.data.ok = 0;
  return _result_2301;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S723,
  moonbit_string_t _M0L12_2adiscard__S724,
  int32_t _M0L12_2adiscard__S725,
  struct _M0TWssbEu* _M0L12_2adiscard__S726,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S727
) {
  struct moonbit_result_0 _result_2302;
  #line 34 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S727);
  moonbit_decref(_M0L12_2adiscard__S726);
  moonbit_decref(_M0L12_2adiscard__S724);
  moonbit_decref(_M0L12_2adiscard__S723);
  _result_2302.tag = 1;
  _result_2302.data.ok = 0;
  return _result_2302;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam9scheduler21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam9scheduler50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S728,
  moonbit_string_t _M0L12_2adiscard__S729,
  int32_t _M0L12_2adiscard__S730,
  struct _M0TWssbEu* _M0L12_2adiscard__S731,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S732
) {
  struct moonbit_result_0 _result_2303;
  #line 34 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S732);
  moonbit_decref(_M0L12_2adiscard__S731);
  moonbit_decref(_M0L12_2adiscard__S729);
  moonbit_decref(_M0L12_2adiscard__S728);
  _result_2303.tag = 1;
  _result_2303.data.ok = 0;
  return _result_2303;
}

int32_t _M0IP016_24default__implP38clawteam8clawteam9scheduler28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam9scheduler34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S712
) {
  #line 12 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S712);
  return 0;
}

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler53____test__7363686564756c65725f7762746573742e6d6274__2(
  
) {
  moonbit_string_t _M0L6_2atmpS1769;
  moonbit_string_t _M0L6_2atmpS1770;
  struct _M0TUssE** _M0L7_2abindS711;
  struct _M0TUssE** _M0L6_2atmpS1773;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS1772;
  struct _M0TPB3MapGssE* _M0L6_2atmpS1771;
  struct _M0TP38clawteam8clawteam9scheduler15DispatchContext* _M0L7contextS710;
  moonbit_string_t _M0L8_2afieldS1982;
  int32_t _M0L6_2acntS2201;
  moonbit_string_t _M0L11session__idS1768;
  struct _M0TPB4Show _M0L6_2atmpS1761;
  moonbit_string_t _M0L6_2atmpS1764;
  moonbit_string_t _M0L6_2atmpS1765;
  moonbit_string_t _M0L6_2atmpS1766;
  moonbit_string_t _M0L6_2atmpS1767;
  moonbit_string_t* _M0L6_2atmpS1763;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1762;
  #line 26 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
  _M0L6_2atmpS1769 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS1770 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L7_2abindS711 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1773 = _M0L7_2abindS711;
  _M0L6_2atmpS1772 = (struct _M0TPB9ArrayViewGUssEE){0, 0, _M0L6_2atmpS1773};
  #line 32 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
  _M0L6_2atmpS1771 = _M0MPB3Map11from__arrayGssE(_M0L6_2atmpS1772);
  _M0L7contextS710
  = (struct _M0TP38clawteam8clawteam9scheduler15DispatchContext*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam9scheduler15DispatchContext));
  Moonbit_object_header(_M0L7contextS710)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam9scheduler15DispatchContext, $0) >> 2, 4, 0);
  _M0L7contextS710->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L7contextS710->$1 = _M0L6_2atmpS1769;
  _M0L7contextS710->$2 = _M0L6_2atmpS1770;
  _M0L7contextS710->$3 = 1700000000;
  _M0L7contextS710->$4 = _M0L6_2atmpS1771;
  _M0L8_2afieldS1982 = _M0L7contextS710->$0;
  _M0L6_2acntS2201 = Moonbit_object_header(_M0L7contextS710)->rc;
  if (_M0L6_2acntS2201 > 1) {
    int32_t _M0L11_2anew__cntS2205 = _M0L6_2acntS2201 - 1;
    Moonbit_object_header(_M0L7contextS710)->rc = _M0L11_2anew__cntS2205;
    moonbit_incref(_M0L8_2afieldS1982);
  } else if (_M0L6_2acntS2201 == 1) {
    struct _M0TPB3MapGssE* _M0L8_2afieldS2204 = _M0L7contextS710->$4;
    moonbit_string_t _M0L8_2afieldS2203;
    moonbit_string_t _M0L8_2afieldS2202;
    moonbit_decref(_M0L8_2afieldS2204);
    _M0L8_2afieldS2203 = _M0L7contextS710->$2;
    if (_M0L8_2afieldS2203) {
      moonbit_decref(_M0L8_2afieldS2203);
    }
    _M0L8_2afieldS2202 = _M0L7contextS710->$1;
    if (_M0L8_2afieldS2202) {
      moonbit_decref(_M0L8_2afieldS2202);
    }
    #line 34 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
    moonbit_free(_M0L7contextS710);
  }
  _M0L11session__idS1768 = _M0L8_2afieldS1982;
  _M0L6_2atmpS1761
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L11session__idS1768
  };
  _M0L6_2atmpS1764 = (moonbit_string_t)moonbit_string_literal_12.data;
  _M0L6_2atmpS1765 = (moonbit_string_t)moonbit_string_literal_13.data;
  _M0L6_2atmpS1766 = 0;
  _M0L6_2atmpS1767 = 0;
  _M0L6_2atmpS1763 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1763[0] = _M0L6_2atmpS1764;
  _M0L6_2atmpS1763[1] = _M0L6_2atmpS1765;
  _M0L6_2atmpS1763[2] = _M0L6_2atmpS1766;
  _M0L6_2atmpS1763[3] = _M0L6_2atmpS1767;
  _M0L6_2atmpS1762
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1762)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1762->$0 = _M0L6_2atmpS1763;
  _M0L6_2atmpS1762->$1 = 4;
  #line 34 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1761, (moonbit_string_t)moonbit_string_literal_11.data, (moonbit_string_t)moonbit_string_literal_14.data, _M0L6_2atmpS1762);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler53____test__7363686564756c65725f7762746573742e6d6274__1(
  
) {
  struct _M0TP38clawteam8clawteam9scheduler13CliCallResult* _M0L6resultS709;
  moonbit_string_t _M0L8_2afieldS1983;
  int32_t _M0L6_2acntS2206;
  moonbit_string_t _M0L6stdoutS1760;
  struct _M0TPB4Show _M0L6_2atmpS1753;
  moonbit_string_t _M0L6_2atmpS1756;
  moonbit_string_t _M0L6_2atmpS1757;
  moonbit_string_t _M0L6_2atmpS1758;
  moonbit_string_t _M0L6_2atmpS1759;
  moonbit_string_t* _M0L6_2atmpS1755;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1754;
  #line 16 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
  _M0L6resultS709
  = (struct _M0TP38clawteam8clawteam9scheduler13CliCallResult*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam9scheduler13CliCallResult));
  Moonbit_object_header(_M0L6resultS709)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam9scheduler13CliCallResult, $0) >> 2, 2, 0);
  _M0L6resultS709->$0 = (moonbit_string_t)moonbit_string_literal_15.data;
  _M0L6resultS709->$1 = (moonbit_string_t)moonbit_string_literal_0.data;
  _M0L6resultS709->$2 = 0;
  _M0L6resultS709->$3 = 100;
  _M0L8_2afieldS1983 = _M0L6resultS709->$0;
  _M0L6_2acntS2206 = Moonbit_object_header(_M0L6resultS709)->rc;
  if (_M0L6_2acntS2206 > 1) {
    int32_t _M0L11_2anew__cntS2208 = _M0L6_2acntS2206 - 1;
    Moonbit_object_header(_M0L6resultS709)->rc = _M0L11_2anew__cntS2208;
    moonbit_incref(_M0L8_2afieldS1983);
  } else if (_M0L6_2acntS2206 == 1) {
    moonbit_string_t _M0L8_2afieldS2207 = _M0L6resultS709->$1;
    moonbit_decref(_M0L8_2afieldS2207);
    #line 23 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
    moonbit_free(_M0L6resultS709);
  }
  _M0L6stdoutS1760 = _M0L8_2afieldS1983;
  _M0L6_2atmpS1753
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6stdoutS1760
  };
  _M0L6_2atmpS1756 = (moonbit_string_t)moonbit_string_literal_16.data;
  _M0L6_2atmpS1757 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS1758 = 0;
  _M0L6_2atmpS1759 = 0;
  _M0L6_2atmpS1755 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1755[0] = _M0L6_2atmpS1756;
  _M0L6_2atmpS1755[1] = _M0L6_2atmpS1757;
  _M0L6_2atmpS1755[2] = _M0L6_2atmpS1758;
  _M0L6_2atmpS1755[3] = _M0L6_2atmpS1759;
  _M0L6_2atmpS1754
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1754)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1754->$0 = _M0L6_2atmpS1755;
  _M0L6_2atmpS1754->$1 = 4;
  #line 23 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1753, (moonbit_string_t)moonbit_string_literal_15.data, (moonbit_string_t)moonbit_string_literal_18.data, _M0L6_2atmpS1754);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam9scheduler53____test__7363686564756c65725f7762746573742e6d6274__0(
  
) {
  struct _M0TUssE** _M0L7_2abindS708;
  struct _M0TUssE** _M0L6_2atmpS1752;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS1751;
  struct _M0TPB3MapGssE* _M0L6_2atmpS1750;
  struct _M0TP38clawteam8clawteam9scheduler13CliCallParams* _M0L6paramsS707;
  moonbit_string_t _M0L8_2afieldS1985;
  moonbit_string_t _M0L9cli__toolS1739;
  struct _M0TPB4Show _M0L6_2atmpS1732;
  moonbit_string_t _M0L6_2atmpS1735;
  moonbit_string_t _M0L6_2atmpS1736;
  moonbit_string_t _M0L6_2atmpS1737;
  moonbit_string_t _M0L6_2atmpS1738;
  moonbit_string_t* _M0L6_2atmpS1734;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1733;
  struct moonbit_result_0 _tmp_2304;
  moonbit_string_t _M0L8_2afieldS1984;
  int32_t _M0L6_2acntS2209;
  moonbit_string_t _M0L13agent__promptS1749;
  struct _M0TPB4Show _M0L6_2atmpS1742;
  moonbit_string_t _M0L6_2atmpS1745;
  moonbit_string_t _M0L6_2atmpS1746;
  moonbit_string_t _M0L6_2atmpS1747;
  moonbit_string_t _M0L6_2atmpS1748;
  moonbit_string_t* _M0L6_2atmpS1744;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS1743;
  #line 5 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
  _M0L7_2abindS708 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1752 = _M0L7_2abindS708;
  _M0L6_2atmpS1751 = (struct _M0TPB9ArrayViewGUssEE){0, 0, _M0L6_2atmpS1752};
  #line 10 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
  _M0L6_2atmpS1750 = _M0MPB3Map11from__arrayGssE(_M0L6_2atmpS1751);
  _M0L6paramsS707
  = (struct _M0TP38clawteam8clawteam9scheduler13CliCallParams*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam9scheduler13CliCallParams));
  Moonbit_object_header(_M0L6paramsS707)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam9scheduler13CliCallParams, $0) >> 2, 4, 0);
  _M0L6paramsS707->$0 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6paramsS707->$1 = (moonbit_string_t)moonbit_string_literal_20.data;
  _M0L6paramsS707->$2 = (moonbit_string_t)moonbit_string_literal_21.data;
  _M0L6paramsS707->$3 = _M0L6_2atmpS1750;
  _M0L8_2afieldS1985 = _M0L6paramsS707->$0;
  _M0L9cli__toolS1739 = _M0L8_2afieldS1985;
  moonbit_incref(_M0L9cli__toolS1739);
  _M0L6_2atmpS1732
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L9cli__toolS1739
  };
  _M0L6_2atmpS1735 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS1736 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS1737 = 0;
  _M0L6_2atmpS1738 = 0;
  _M0L6_2atmpS1734 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1734[0] = _M0L6_2atmpS1735;
  _M0L6_2atmpS1734[1] = _M0L6_2atmpS1736;
  _M0L6_2atmpS1734[2] = _M0L6_2atmpS1737;
  _M0L6_2atmpS1734[3] = _M0L6_2atmpS1738;
  _M0L6_2atmpS1733
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1733)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1733->$0 = _M0L6_2atmpS1734;
  _M0L6_2atmpS1733->$1 = 4;
  #line 12 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
  _tmp_2304
  = _M0FPB15inspect_2einner(_M0L6_2atmpS1732, (moonbit_string_t)moonbit_string_literal_19.data, (moonbit_string_t)moonbit_string_literal_24.data, _M0L6_2atmpS1733);
  if (_tmp_2304.tag) {
    int32_t const _M0L5_2aokS1740 = _tmp_2304.data.ok;
  } else {
    void* const _M0L6_2aerrS1741 = _tmp_2304.data.err;
    struct moonbit_result_0 _result_2305;
    moonbit_decref(_M0L6paramsS707);
    _result_2305.tag = 0;
    _result_2305.data.err = _M0L6_2aerrS1741;
    return _result_2305;
  }
  _M0L8_2afieldS1984 = _M0L6paramsS707->$1;
  _M0L6_2acntS2209 = Moonbit_object_header(_M0L6paramsS707)->rc;
  if (_M0L6_2acntS2209 > 1) {
    int32_t _M0L11_2anew__cntS2213 = _M0L6_2acntS2209 - 1;
    Moonbit_object_header(_M0L6paramsS707)->rc = _M0L11_2anew__cntS2213;
    moonbit_incref(_M0L8_2afieldS1984);
  } else if (_M0L6_2acntS2209 == 1) {
    struct _M0TPB3MapGssE* _M0L8_2afieldS2212 = _M0L6paramsS707->$3;
    moonbit_string_t _M0L8_2afieldS2211;
    moonbit_string_t _M0L8_2afieldS2210;
    moonbit_decref(_M0L8_2afieldS2212);
    _M0L8_2afieldS2211 = _M0L6paramsS707->$2;
    moonbit_decref(_M0L8_2afieldS2211);
    _M0L8_2afieldS2210 = _M0L6paramsS707->$0;
    moonbit_decref(_M0L8_2afieldS2210);
    #line 13 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
    moonbit_free(_M0L6paramsS707);
  }
  _M0L13agent__promptS1749 = _M0L8_2afieldS1984;
  _M0L6_2atmpS1742
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L13agent__promptS1749
  };
  _M0L6_2atmpS1745 = (moonbit_string_t)moonbit_string_literal_25.data;
  _M0L6_2atmpS1746 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS1747 = 0;
  _M0L6_2atmpS1748 = 0;
  _M0L6_2atmpS1744 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS1744[0] = _M0L6_2atmpS1745;
  _M0L6_2atmpS1744[1] = _M0L6_2atmpS1746;
  _M0L6_2atmpS1744[2] = _M0L6_2atmpS1747;
  _M0L6_2atmpS1744[3] = _M0L6_2atmpS1748;
  _M0L6_2atmpS1743
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS1743)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS1743->$0 = _M0L6_2atmpS1744;
  _M0L6_2atmpS1743->$1 = 4;
  #line 13 "E:\\moonbit\\clawteam\\scheduler\\scheduler_wbtest.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS1742, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_27.data, _M0L6_2atmpS1743);
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS705,
  struct _M0TPB6Logger _M0L6loggerS706
) {
  moonbit_string_t _M0L6_2atmpS1731;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1730;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1731 = _M0L4selfS705;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1730 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1731);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS1730, _M0L6loggerS706);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS682,
  struct _M0TPB6Logger _M0L6loggerS704
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS1994;
  struct _M0TPC16string10StringView _M0L3pkgS681;
  moonbit_string_t _M0L7_2adataS683;
  int32_t _M0L8_2astartS684;
  int32_t _M0L6_2atmpS1729;
  int32_t _M0L6_2aendS685;
  int32_t _M0Lm9_2acursorS686;
  int32_t _M0Lm13accept__stateS687;
  int32_t _M0Lm10match__endS688;
  int32_t _M0Lm20match__tag__saver__0S689;
  int32_t _M0Lm6tag__0S690;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS691;
  struct _M0TPC16string10StringView _M0L8_2afieldS1993;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS700;
  void* _M0L8_2afieldS1992;
  int32_t _M0L6_2acntS2214;
  void* _M0L16_2apackage__nameS701;
  struct _M0TPC16string10StringView _M0L8_2afieldS1990;
  struct _M0TPC16string10StringView _M0L8filenameS1706;
  struct _M0TPC16string10StringView _M0L8_2afieldS1989;
  struct _M0TPC16string10StringView _M0L11start__lineS1707;
  struct _M0TPC16string10StringView _M0L8_2afieldS1988;
  struct _M0TPC16string10StringView _M0L13start__columnS1708;
  struct _M0TPC16string10StringView _M0L8_2afieldS1987;
  struct _M0TPC16string10StringView _M0L9end__lineS1709;
  struct _M0TPC16string10StringView _M0L8_2afieldS1986;
  int32_t _M0L6_2acntS2218;
  struct _M0TPC16string10StringView _M0L11end__columnS1710;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS1994
  = (struct _M0TPC16string10StringView){
    _M0L4selfS682->$0_1, _M0L4selfS682->$0_2, _M0L4selfS682->$0_0
  };
  _M0L3pkgS681 = _M0L8_2afieldS1994;
  moonbit_incref(_M0L3pkgS681.$0);
  moonbit_incref(_M0L3pkgS681.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS683 = _M0MPC16string10StringView4data(_M0L3pkgS681);
  moonbit_incref(_M0L3pkgS681.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS684 = _M0MPC16string10StringView13start__offset(_M0L3pkgS681);
  moonbit_incref(_M0L3pkgS681.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1729 = _M0MPC16string10StringView6length(_M0L3pkgS681);
  _M0L6_2aendS685 = _M0L8_2astartS684 + _M0L6_2atmpS1729;
  _M0Lm9_2acursorS686 = _M0L8_2astartS684;
  _M0Lm13accept__stateS687 = -1;
  _M0Lm10match__endS688 = -1;
  _M0Lm20match__tag__saver__0S689 = -1;
  _M0Lm6tag__0S690 = -1;
  while (1) {
    int32_t _M0L6_2atmpS1721 = _M0Lm9_2acursorS686;
    if (_M0L6_2atmpS1721 < _M0L6_2aendS685) {
      int32_t _M0L6_2atmpS1728 = _M0Lm9_2acursorS686;
      int32_t _M0L10next__charS695;
      int32_t _M0L6_2atmpS1722;
      moonbit_incref(_M0L7_2adataS683);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS695
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS683, _M0L6_2atmpS1728);
      _M0L6_2atmpS1722 = _M0Lm9_2acursorS686;
      _M0Lm9_2acursorS686 = _M0L6_2atmpS1722 + 1;
      if (_M0L10next__charS695 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS1723;
          _M0Lm6tag__0S690 = _M0Lm9_2acursorS686;
          _M0L6_2atmpS1723 = _M0Lm9_2acursorS686;
          if (_M0L6_2atmpS1723 < _M0L6_2aendS685) {
            int32_t _M0L6_2atmpS1727 = _M0Lm9_2acursorS686;
            int32_t _M0L10next__charS696;
            int32_t _M0L6_2atmpS1724;
            moonbit_incref(_M0L7_2adataS683);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS696
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS683, _M0L6_2atmpS1727);
            _M0L6_2atmpS1724 = _M0Lm9_2acursorS686;
            _M0Lm9_2acursorS686 = _M0L6_2atmpS1724 + 1;
            if (_M0L10next__charS696 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS1725 = _M0Lm9_2acursorS686;
                if (_M0L6_2atmpS1725 < _M0L6_2aendS685) {
                  int32_t _M0L6_2atmpS1726 = _M0Lm9_2acursorS686;
                  _M0Lm9_2acursorS686 = _M0L6_2atmpS1726 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S689 = _M0Lm6tag__0S690;
                  _M0Lm13accept__stateS687 = 0;
                  _M0Lm10match__endS688 = _M0Lm9_2acursorS686;
                  goto join_692;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_692;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_692;
    }
    break;
  }
  goto joinlet_2306;
  join_692:;
  switch (_M0Lm13accept__stateS687) {
    case 0: {
      int32_t _M0L6_2atmpS1719;
      int32_t _M0L6_2atmpS1718;
      int64_t _M0L6_2atmpS1715;
      int32_t _M0L6_2atmpS1717;
      int64_t _M0L6_2atmpS1716;
      struct _M0TPC16string10StringView _M0L13package__nameS693;
      int64_t _M0L6_2atmpS1712;
      int32_t _M0L6_2atmpS1714;
      int64_t _M0L6_2atmpS1713;
      struct _M0TPC16string10StringView _M0L12module__nameS694;
      void* _M0L4SomeS1711;
      moonbit_decref(_M0L3pkgS681.$0);
      _M0L6_2atmpS1719 = _M0Lm20match__tag__saver__0S689;
      _M0L6_2atmpS1718 = _M0L6_2atmpS1719 + 1;
      _M0L6_2atmpS1715 = (int64_t)_M0L6_2atmpS1718;
      _M0L6_2atmpS1717 = _M0Lm10match__endS688;
      _M0L6_2atmpS1716 = (int64_t)_M0L6_2atmpS1717;
      moonbit_incref(_M0L7_2adataS683);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS693
      = _M0MPC16string6String4view(_M0L7_2adataS683, _M0L6_2atmpS1715, _M0L6_2atmpS1716);
      _M0L6_2atmpS1712 = (int64_t)_M0L8_2astartS684;
      _M0L6_2atmpS1714 = _M0Lm20match__tag__saver__0S689;
      _M0L6_2atmpS1713 = (int64_t)_M0L6_2atmpS1714;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS694
      = _M0MPC16string6String4view(_M0L7_2adataS683, _M0L6_2atmpS1712, _M0L6_2atmpS1713);
      _M0L4SomeS1711
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS1711)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1711)->$0_0
      = _M0L13package__nameS693.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1711)->$0_1
      = _M0L13package__nameS693.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS1711)->$0_2
      = _M0L13package__nameS693.$2;
      _M0L7_2abindS691
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS691)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS691->$0_0 = _M0L12module__nameS694.$0;
      _M0L7_2abindS691->$0_1 = _M0L12module__nameS694.$1;
      _M0L7_2abindS691->$0_2 = _M0L12module__nameS694.$2;
      _M0L7_2abindS691->$1 = _M0L4SomeS1711;
      break;
    }
    default: {
      void* _M0L4NoneS1720;
      moonbit_decref(_M0L7_2adataS683);
      _M0L4NoneS1720
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS691
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS691)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS691->$0_0 = _M0L3pkgS681.$0;
      _M0L7_2abindS691->$0_1 = _M0L3pkgS681.$1;
      _M0L7_2abindS691->$0_2 = _M0L3pkgS681.$2;
      _M0L7_2abindS691->$1 = _M0L4NoneS1720;
      break;
    }
  }
  joinlet_2306:;
  _M0L8_2afieldS1993
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS691->$0_1, _M0L7_2abindS691->$0_2, _M0L7_2abindS691->$0_0
  };
  _M0L15_2amodule__nameS700 = _M0L8_2afieldS1993;
  _M0L8_2afieldS1992 = _M0L7_2abindS691->$1;
  _M0L6_2acntS2214 = Moonbit_object_header(_M0L7_2abindS691)->rc;
  if (_M0L6_2acntS2214 > 1) {
    int32_t _M0L11_2anew__cntS2215 = _M0L6_2acntS2214 - 1;
    Moonbit_object_header(_M0L7_2abindS691)->rc = _M0L11_2anew__cntS2215;
    moonbit_incref(_M0L8_2afieldS1992);
    moonbit_incref(_M0L15_2amodule__nameS700.$0);
  } else if (_M0L6_2acntS2214 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS691);
  }
  _M0L16_2apackage__nameS701 = _M0L8_2afieldS1992;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS701)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS702 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS701;
      struct _M0TPC16string10StringView _M0L8_2afieldS1991 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS702->$0_1,
                                              _M0L7_2aSomeS702->$0_2,
                                              _M0L7_2aSomeS702->$0_0};
      int32_t _M0L6_2acntS2216 = Moonbit_object_header(_M0L7_2aSomeS702)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS703;
      if (_M0L6_2acntS2216 > 1) {
        int32_t _M0L11_2anew__cntS2217 = _M0L6_2acntS2216 - 1;
        Moonbit_object_header(_M0L7_2aSomeS702)->rc = _M0L11_2anew__cntS2217;
        moonbit_incref(_M0L8_2afieldS1991.$0);
      } else if (_M0L6_2acntS2216 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS702);
      }
      _M0L12_2apkg__nameS703 = _M0L8_2afieldS1991;
      if (_M0L6loggerS704.$1) {
        moonbit_incref(_M0L6loggerS704.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS704.$0->$method_2(_M0L6loggerS704.$1, _M0L12_2apkg__nameS703);
      if (_M0L6loggerS704.$1) {
        moonbit_incref(_M0L6loggerS704.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS704.$0->$method_3(_M0L6loggerS704.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS701);
      break;
    }
  }
  _M0L8_2afieldS1990
  = (struct _M0TPC16string10StringView){
    _M0L4selfS682->$1_1, _M0L4selfS682->$1_2, _M0L4selfS682->$1_0
  };
  _M0L8filenameS1706 = _M0L8_2afieldS1990;
  moonbit_incref(_M0L8filenameS1706.$0);
  if (_M0L6loggerS704.$1) {
    moonbit_incref(_M0L6loggerS704.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS704.$0->$method_2(_M0L6loggerS704.$1, _M0L8filenameS1706);
  if (_M0L6loggerS704.$1) {
    moonbit_incref(_M0L6loggerS704.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS704.$0->$method_3(_M0L6loggerS704.$1, 58);
  _M0L8_2afieldS1989
  = (struct _M0TPC16string10StringView){
    _M0L4selfS682->$2_1, _M0L4selfS682->$2_2, _M0L4selfS682->$2_0
  };
  _M0L11start__lineS1707 = _M0L8_2afieldS1989;
  moonbit_incref(_M0L11start__lineS1707.$0);
  if (_M0L6loggerS704.$1) {
    moonbit_incref(_M0L6loggerS704.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS704.$0->$method_2(_M0L6loggerS704.$1, _M0L11start__lineS1707);
  if (_M0L6loggerS704.$1) {
    moonbit_incref(_M0L6loggerS704.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS704.$0->$method_3(_M0L6loggerS704.$1, 58);
  _M0L8_2afieldS1988
  = (struct _M0TPC16string10StringView){
    _M0L4selfS682->$3_1, _M0L4selfS682->$3_2, _M0L4selfS682->$3_0
  };
  _M0L13start__columnS1708 = _M0L8_2afieldS1988;
  moonbit_incref(_M0L13start__columnS1708.$0);
  if (_M0L6loggerS704.$1) {
    moonbit_incref(_M0L6loggerS704.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS704.$0->$method_2(_M0L6loggerS704.$1, _M0L13start__columnS1708);
  if (_M0L6loggerS704.$1) {
    moonbit_incref(_M0L6loggerS704.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS704.$0->$method_3(_M0L6loggerS704.$1, 45);
  _M0L8_2afieldS1987
  = (struct _M0TPC16string10StringView){
    _M0L4selfS682->$4_1, _M0L4selfS682->$4_2, _M0L4selfS682->$4_0
  };
  _M0L9end__lineS1709 = _M0L8_2afieldS1987;
  moonbit_incref(_M0L9end__lineS1709.$0);
  if (_M0L6loggerS704.$1) {
    moonbit_incref(_M0L6loggerS704.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS704.$0->$method_2(_M0L6loggerS704.$1, _M0L9end__lineS1709);
  if (_M0L6loggerS704.$1) {
    moonbit_incref(_M0L6loggerS704.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS704.$0->$method_3(_M0L6loggerS704.$1, 58);
  _M0L8_2afieldS1986
  = (struct _M0TPC16string10StringView){
    _M0L4selfS682->$5_1, _M0L4selfS682->$5_2, _M0L4selfS682->$5_0
  };
  _M0L6_2acntS2218 = Moonbit_object_header(_M0L4selfS682)->rc;
  if (_M0L6_2acntS2218 > 1) {
    int32_t _M0L11_2anew__cntS2224 = _M0L6_2acntS2218 - 1;
    Moonbit_object_header(_M0L4selfS682)->rc = _M0L11_2anew__cntS2224;
    moonbit_incref(_M0L8_2afieldS1986.$0);
  } else if (_M0L6_2acntS2218 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS2223 =
      (struct _M0TPC16string10StringView){_M0L4selfS682->$4_1,
                                            _M0L4selfS682->$4_2,
                                            _M0L4selfS682->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS2222;
    struct _M0TPC16string10StringView _M0L8_2afieldS2221;
    struct _M0TPC16string10StringView _M0L8_2afieldS2220;
    struct _M0TPC16string10StringView _M0L8_2afieldS2219;
    moonbit_decref(_M0L8_2afieldS2223.$0);
    _M0L8_2afieldS2222
    = (struct _M0TPC16string10StringView){
      _M0L4selfS682->$3_1, _M0L4selfS682->$3_2, _M0L4selfS682->$3_0
    };
    moonbit_decref(_M0L8_2afieldS2222.$0);
    _M0L8_2afieldS2221
    = (struct _M0TPC16string10StringView){
      _M0L4selfS682->$2_1, _M0L4selfS682->$2_2, _M0L4selfS682->$2_0
    };
    moonbit_decref(_M0L8_2afieldS2221.$0);
    _M0L8_2afieldS2220
    = (struct _M0TPC16string10StringView){
      _M0L4selfS682->$1_1, _M0L4selfS682->$1_2, _M0L4selfS682->$1_0
    };
    moonbit_decref(_M0L8_2afieldS2220.$0);
    _M0L8_2afieldS2219
    = (struct _M0TPC16string10StringView){
      _M0L4selfS682->$0_1, _M0L4selfS682->$0_2, _M0L4selfS682->$0_0
    };
    moonbit_decref(_M0L8_2afieldS2219.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS682);
  }
  _M0L11end__columnS1710 = _M0L8_2afieldS1986;
  if (_M0L6loggerS704.$1) {
    moonbit_incref(_M0L6loggerS704.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS704.$0->$method_2(_M0L6loggerS704.$1, _M0L11end__columnS1710);
  if (_M0L6loggerS704.$1) {
    moonbit_incref(_M0L6loggerS704.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS704.$0->$method_3(_M0L6loggerS704.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS704.$0->$method_2(_M0L6loggerS704.$1, _M0L15_2amodule__nameS700);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS680) {
  moonbit_string_t _M0L6_2atmpS1705;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS1705 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS680);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS1705);
  moonbit_decref(_M0L6_2atmpS1705);
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS679,
  struct _M0TPB6Hasher* _M0L6hasherS678
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS678, _M0L4selfS679);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS677,
  struct _M0TPB6Hasher* _M0L6hasherS676
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS676, _M0L4selfS677);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS674,
  moonbit_string_t _M0L5valueS672
) {
  int32_t _M0L7_2abindS671;
  int32_t _M0L1iS673;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS671 = Moonbit_array_length(_M0L5valueS672);
  _M0L1iS673 = 0;
  while (1) {
    if (_M0L1iS673 < _M0L7_2abindS671) {
      int32_t _M0L6_2atmpS1703 = _M0L5valueS672[_M0L1iS673];
      int32_t _M0L6_2atmpS1702 = (int32_t)_M0L6_2atmpS1703;
      uint32_t _M0L6_2atmpS1701 = *(uint32_t*)&_M0L6_2atmpS1702;
      int32_t _M0L6_2atmpS1704;
      moonbit_incref(_M0L4selfS674);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS674, _M0L6_2atmpS1701);
      _M0L6_2atmpS1704 = _M0L1iS673 + 1;
      _M0L1iS673 = _M0L6_2atmpS1704;
      continue;
    } else {
      moonbit_decref(_M0L4selfS674);
      moonbit_decref(_M0L5valueS672);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS669,
  int32_t _M0L3idxS670
) {
  int32_t _M0L6_2atmpS1995;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1995 = _M0L4selfS669[_M0L3idxS670];
  moonbit_decref(_M0L4selfS669);
  return _M0L6_2atmpS1995;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS656,
  int32_t _M0L3keyS652
) {
  int32_t _M0L4hashS651;
  int32_t _M0L14capacity__maskS1686;
  int32_t _M0L6_2atmpS1685;
  int32_t _M0L1iS653;
  int32_t _M0L3idxS654;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS651 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS652);
  _M0L14capacity__maskS1686 = _M0L4selfS656->$3;
  _M0L6_2atmpS1685 = _M0L4hashS651 & _M0L14capacity__maskS1686;
  _M0L1iS653 = 0;
  _M0L3idxS654 = _M0L6_2atmpS1685;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS1999 =
      _M0L4selfS656->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1684 =
      _M0L8_2afieldS1999;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1998;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS655;
    if (
      _M0L3idxS654 < 0
      || _M0L3idxS654 >= Moonbit_array_length(_M0L7entriesS1684)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1998
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1684[
        _M0L3idxS654
      ];
    _M0L7_2abindS655 = _M0L6_2atmpS1998;
    if (_M0L7_2abindS655 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1673;
      if (_M0L7_2abindS655) {
        moonbit_incref(_M0L7_2abindS655);
      }
      moonbit_decref(_M0L4selfS656);
      if (_M0L7_2abindS655) {
        moonbit_decref(_M0L7_2abindS655);
      }
      _M0L6_2atmpS1673 = 0;
      return _M0L6_2atmpS1673;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS657 =
        _M0L7_2abindS655;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS658 =
        _M0L7_2aSomeS657;
      int32_t _M0L4hashS1675 = _M0L8_2aentryS658->$3;
      int32_t _if__result_2312;
      int32_t _M0L8_2afieldS1996;
      int32_t _M0L3pslS1678;
      int32_t _M0L6_2atmpS1680;
      int32_t _M0L6_2atmpS1682;
      int32_t _M0L14capacity__maskS1683;
      int32_t _M0L6_2atmpS1681;
      if (_M0L4hashS1675 == _M0L4hashS651) {
        int32_t _M0L3keyS1674 = _M0L8_2aentryS658->$4;
        _if__result_2312 = _M0L3keyS1674 == _M0L3keyS652;
      } else {
        _if__result_2312 = 0;
      }
      if (_if__result_2312) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS1997;
        int32_t _M0L6_2acntS2225;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS1677;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1676;
        moonbit_incref(_M0L8_2aentryS658);
        moonbit_decref(_M0L4selfS656);
        _M0L8_2afieldS1997 = _M0L8_2aentryS658->$5;
        _M0L6_2acntS2225 = Moonbit_object_header(_M0L8_2aentryS658)->rc;
        if (_M0L6_2acntS2225 > 1) {
          int32_t _M0L11_2anew__cntS2227 = _M0L6_2acntS2225 - 1;
          Moonbit_object_header(_M0L8_2aentryS658)->rc
          = _M0L11_2anew__cntS2227;
          moonbit_incref(_M0L8_2afieldS1997);
        } else if (_M0L6_2acntS2225 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2226 =
            _M0L8_2aentryS658->$1;
          if (_M0L8_2afieldS2226) {
            moonbit_decref(_M0L8_2afieldS2226);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS658);
        }
        _M0L5valueS1677 = _M0L8_2afieldS1997;
        _M0L6_2atmpS1676 = _M0L5valueS1677;
        return _M0L6_2atmpS1676;
      } else {
        moonbit_incref(_M0L8_2aentryS658);
      }
      _M0L8_2afieldS1996 = _M0L8_2aentryS658->$2;
      moonbit_decref(_M0L8_2aentryS658);
      _M0L3pslS1678 = _M0L8_2afieldS1996;
      if (_M0L1iS653 > _M0L3pslS1678) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1679;
        moonbit_decref(_M0L4selfS656);
        _M0L6_2atmpS1679 = 0;
        return _M0L6_2atmpS1679;
      }
      _M0L6_2atmpS1680 = _M0L1iS653 + 1;
      _M0L6_2atmpS1682 = _M0L3idxS654 + 1;
      _M0L14capacity__maskS1683 = _M0L4selfS656->$3;
      _M0L6_2atmpS1681 = _M0L6_2atmpS1682 & _M0L14capacity__maskS1683;
      _M0L1iS653 = _M0L6_2atmpS1680;
      _M0L3idxS654 = _M0L6_2atmpS1681;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS665,
  moonbit_string_t _M0L3keyS661
) {
  int32_t _M0L4hashS660;
  int32_t _M0L14capacity__maskS1700;
  int32_t _M0L6_2atmpS1699;
  int32_t _M0L1iS662;
  int32_t _M0L3idxS663;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS661);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS660 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS661);
  _M0L14capacity__maskS1700 = _M0L4selfS665->$3;
  _M0L6_2atmpS1699 = _M0L4hashS660 & _M0L14capacity__maskS1700;
  _M0L1iS662 = 0;
  _M0L3idxS663 = _M0L6_2atmpS1699;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2005 =
      _M0L4selfS665->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1698 =
      _M0L8_2afieldS2005;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2004;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS664;
    if (
      _M0L3idxS663 < 0
      || _M0L3idxS663 >= Moonbit_array_length(_M0L7entriesS1698)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2004
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1698[
        _M0L3idxS663
      ];
    _M0L7_2abindS664 = _M0L6_2atmpS2004;
    if (_M0L7_2abindS664 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1687;
      if (_M0L7_2abindS664) {
        moonbit_incref(_M0L7_2abindS664);
      }
      moonbit_decref(_M0L4selfS665);
      if (_M0L7_2abindS664) {
        moonbit_decref(_M0L7_2abindS664);
      }
      moonbit_decref(_M0L3keyS661);
      _M0L6_2atmpS1687 = 0;
      return _M0L6_2atmpS1687;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS666 =
        _M0L7_2abindS664;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS667 =
        _M0L7_2aSomeS666;
      int32_t _M0L4hashS1689 = _M0L8_2aentryS667->$3;
      int32_t _if__result_2314;
      int32_t _M0L8_2afieldS2000;
      int32_t _M0L3pslS1692;
      int32_t _M0L6_2atmpS1694;
      int32_t _M0L6_2atmpS1696;
      int32_t _M0L14capacity__maskS1697;
      int32_t _M0L6_2atmpS1695;
      if (_M0L4hashS1689 == _M0L4hashS660) {
        moonbit_string_t _M0L8_2afieldS2003 = _M0L8_2aentryS667->$4;
        moonbit_string_t _M0L3keyS1688 = _M0L8_2afieldS2003;
        int32_t _M0L6_2atmpS2002;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2002
        = moonbit_val_array_equal(_M0L3keyS1688, _M0L3keyS661);
        _if__result_2314 = _M0L6_2atmpS2002;
      } else {
        _if__result_2314 = 0;
      }
      if (_if__result_2314) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2001;
        int32_t _M0L6_2acntS2228;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS1691;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1690;
        moonbit_incref(_M0L8_2aentryS667);
        moonbit_decref(_M0L4selfS665);
        moonbit_decref(_M0L3keyS661);
        _M0L8_2afieldS2001 = _M0L8_2aentryS667->$5;
        _M0L6_2acntS2228 = Moonbit_object_header(_M0L8_2aentryS667)->rc;
        if (_M0L6_2acntS2228 > 1) {
          int32_t _M0L11_2anew__cntS2231 = _M0L6_2acntS2228 - 1;
          Moonbit_object_header(_M0L8_2aentryS667)->rc
          = _M0L11_2anew__cntS2231;
          moonbit_incref(_M0L8_2afieldS2001);
        } else if (_M0L6_2acntS2228 == 1) {
          moonbit_string_t _M0L8_2afieldS2230 = _M0L8_2aentryS667->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2229;
          moonbit_decref(_M0L8_2afieldS2230);
          _M0L8_2afieldS2229 = _M0L8_2aentryS667->$1;
          if (_M0L8_2afieldS2229) {
            moonbit_decref(_M0L8_2afieldS2229);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS667);
        }
        _M0L5valueS1691 = _M0L8_2afieldS2001;
        _M0L6_2atmpS1690 = _M0L5valueS1691;
        return _M0L6_2atmpS1690;
      } else {
        moonbit_incref(_M0L8_2aentryS667);
      }
      _M0L8_2afieldS2000 = _M0L8_2aentryS667->$2;
      moonbit_decref(_M0L8_2aentryS667);
      _M0L3pslS1692 = _M0L8_2afieldS2000;
      if (_M0L1iS662 > _M0L3pslS1692) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1693;
        moonbit_decref(_M0L4selfS665);
        moonbit_decref(_M0L3keyS661);
        _M0L6_2atmpS1693 = 0;
        return _M0L6_2atmpS1693;
      }
      _M0L6_2atmpS1694 = _M0L1iS662 + 1;
      _M0L6_2atmpS1696 = _M0L3idxS663 + 1;
      _M0L14capacity__maskS1697 = _M0L4selfS665->$3;
      _M0L6_2atmpS1695 = _M0L6_2atmpS1696 & _M0L14capacity__maskS1697;
      _M0L1iS662 = _M0L6_2atmpS1694;
      _M0L3idxS663 = _M0L6_2atmpS1695;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS628
) {
  int32_t _M0L6lengthS627;
  int32_t _M0Lm8capacityS629;
  int32_t _M0L6_2atmpS1638;
  int32_t _M0L6_2atmpS1637;
  int32_t _M0L6_2atmpS1648;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS630;
  int32_t _M0L3endS1646;
  int32_t _M0L5startS1647;
  int32_t _M0L7_2abindS631;
  int32_t _M0L2__S632;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS628.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS627
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS628);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS629 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS627);
  _M0L6_2atmpS1638 = _M0Lm8capacityS629;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1637 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1638);
  if (_M0L6lengthS627 > _M0L6_2atmpS1637) {
    int32_t _M0L6_2atmpS1639 = _M0Lm8capacityS629;
    _M0Lm8capacityS629 = _M0L6_2atmpS1639 * 2;
  }
  _M0L6_2atmpS1648 = _M0Lm8capacityS629;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS630
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1648);
  _M0L3endS1646 = _M0L3arrS628.$2;
  _M0L5startS1647 = _M0L3arrS628.$1;
  _M0L7_2abindS631 = _M0L3endS1646 - _M0L5startS1647;
  _M0L2__S632 = 0;
  while (1) {
    if (_M0L2__S632 < _M0L7_2abindS631) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2009 =
        _M0L3arrS628.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS1643 =
        _M0L8_2afieldS2009;
      int32_t _M0L5startS1645 = _M0L3arrS628.$1;
      int32_t _M0L6_2atmpS1644 = _M0L5startS1645 + _M0L2__S632;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2008 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS1643[
          _M0L6_2atmpS1644
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS633 =
        _M0L6_2atmpS2008;
      moonbit_string_t _M0L8_2afieldS2007 = _M0L1eS633->$0;
      moonbit_string_t _M0L6_2atmpS1640 = _M0L8_2afieldS2007;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2006 =
        _M0L1eS633->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1641 =
        _M0L8_2afieldS2006;
      int32_t _M0L6_2atmpS1642;
      moonbit_incref(_M0L6_2atmpS1641);
      moonbit_incref(_M0L6_2atmpS1640);
      moonbit_incref(_M0L1mS630);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS630, _M0L6_2atmpS1640, _M0L6_2atmpS1641);
      _M0L6_2atmpS1642 = _M0L2__S632 + 1;
      _M0L2__S632 = _M0L6_2atmpS1642;
      continue;
    } else {
      moonbit_decref(_M0L3arrS628.$0);
    }
    break;
  }
  return _M0L1mS630;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS636
) {
  int32_t _M0L6lengthS635;
  int32_t _M0Lm8capacityS637;
  int32_t _M0L6_2atmpS1650;
  int32_t _M0L6_2atmpS1649;
  int32_t _M0L6_2atmpS1660;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS638;
  int32_t _M0L3endS1658;
  int32_t _M0L5startS1659;
  int32_t _M0L7_2abindS639;
  int32_t _M0L2__S640;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS636.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS635
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS636);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS637 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS635);
  _M0L6_2atmpS1650 = _M0Lm8capacityS637;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1649 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1650);
  if (_M0L6lengthS635 > _M0L6_2atmpS1649) {
    int32_t _M0L6_2atmpS1651 = _M0Lm8capacityS637;
    _M0Lm8capacityS637 = _M0L6_2atmpS1651 * 2;
  }
  _M0L6_2atmpS1660 = _M0Lm8capacityS637;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS638
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1660);
  _M0L3endS1658 = _M0L3arrS636.$2;
  _M0L5startS1659 = _M0L3arrS636.$1;
  _M0L7_2abindS639 = _M0L3endS1658 - _M0L5startS1659;
  _M0L2__S640 = 0;
  while (1) {
    if (_M0L2__S640 < _M0L7_2abindS639) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2012 =
        _M0L3arrS636.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS1655 =
        _M0L8_2afieldS2012;
      int32_t _M0L5startS1657 = _M0L3arrS636.$1;
      int32_t _M0L6_2atmpS1656 = _M0L5startS1657 + _M0L2__S640;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2011 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS1655[
          _M0L6_2atmpS1656
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS641 = _M0L6_2atmpS2011;
      int32_t _M0L6_2atmpS1652 = _M0L1eS641->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2010 =
        _M0L1eS641->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS1653 =
        _M0L8_2afieldS2010;
      int32_t _M0L6_2atmpS1654;
      moonbit_incref(_M0L6_2atmpS1653);
      moonbit_incref(_M0L1mS638);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS638, _M0L6_2atmpS1652, _M0L6_2atmpS1653);
      _M0L6_2atmpS1654 = _M0L2__S640 + 1;
      _M0L2__S640 = _M0L6_2atmpS1654;
      continue;
    } else {
      moonbit_decref(_M0L3arrS636.$0);
    }
    break;
  }
  return _M0L1mS638;
}

struct _M0TPB3MapGssE* _M0MPB3Map11from__arrayGssE(
  struct _M0TPB9ArrayViewGUssEE _M0L3arrS644
) {
  int32_t _M0L6lengthS643;
  int32_t _M0Lm8capacityS645;
  int32_t _M0L6_2atmpS1662;
  int32_t _M0L6_2atmpS1661;
  int32_t _M0L6_2atmpS1672;
  struct _M0TPB3MapGssE* _M0L1mS646;
  int32_t _M0L3endS1670;
  int32_t _M0L5startS1671;
  int32_t _M0L7_2abindS647;
  int32_t _M0L2__S648;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS644.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS643 = _M0MPC15array9ArrayView6lengthGUssEE(_M0L3arrS644);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS645 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS643);
  _M0L6_2atmpS1662 = _M0Lm8capacityS645;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1661 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS1662);
  if (_M0L6lengthS643 > _M0L6_2atmpS1661) {
    int32_t _M0L6_2atmpS1663 = _M0Lm8capacityS645;
    _M0Lm8capacityS645 = _M0L6_2atmpS1663 * 2;
  }
  _M0L6_2atmpS1672 = _M0Lm8capacityS645;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS646 = _M0MPB3Map11new_2einnerGssE(_M0L6_2atmpS1672);
  _M0L3endS1670 = _M0L3arrS644.$2;
  _M0L5startS1671 = _M0L3arrS644.$1;
  _M0L7_2abindS647 = _M0L3endS1670 - _M0L5startS1671;
  _M0L2__S648 = 0;
  while (1) {
    if (_M0L2__S648 < _M0L7_2abindS647) {
      struct _M0TUssE** _M0L8_2afieldS2016 = _M0L3arrS644.$0;
      struct _M0TUssE** _M0L3bufS1667 = _M0L8_2afieldS2016;
      int32_t _M0L5startS1669 = _M0L3arrS644.$1;
      int32_t _M0L6_2atmpS1668 = _M0L5startS1669 + _M0L2__S648;
      struct _M0TUssE* _M0L6_2atmpS2015 =
        (struct _M0TUssE*)_M0L3bufS1667[_M0L6_2atmpS1668];
      struct _M0TUssE* _M0L1eS649 = _M0L6_2atmpS2015;
      moonbit_string_t _M0L8_2afieldS2014 = _M0L1eS649->$0;
      moonbit_string_t _M0L6_2atmpS1664 = _M0L8_2afieldS2014;
      moonbit_string_t _M0L8_2afieldS2013 = _M0L1eS649->$1;
      moonbit_string_t _M0L6_2atmpS1665 = _M0L8_2afieldS2013;
      int32_t _M0L6_2atmpS1666;
      moonbit_incref(_M0L6_2atmpS1665);
      moonbit_incref(_M0L6_2atmpS1664);
      moonbit_incref(_M0L1mS646);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGssE(_M0L1mS646, _M0L6_2atmpS1664, _M0L6_2atmpS1665);
      _M0L6_2atmpS1666 = _M0L2__S648 + 1;
      _M0L2__S648 = _M0L6_2atmpS1666;
      continue;
    } else {
      moonbit_decref(_M0L3arrS644.$0);
    }
    break;
  }
  return _M0L1mS646;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS618,
  moonbit_string_t _M0L3keyS619,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS620
) {
  int32_t _M0L6_2atmpS1634;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS619);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1634 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS619);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS618, _M0L3keyS619, _M0L5valueS620, _M0L6_2atmpS1634);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS621,
  int32_t _M0L3keyS622,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS623
) {
  int32_t _M0L6_2atmpS1635;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1635 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS622);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS621, _M0L3keyS622, _M0L5valueS623, _M0L6_2atmpS1635);
  return 0;
}

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE* _M0L4selfS624,
  moonbit_string_t _M0L3keyS625,
  moonbit_string_t _M0L5valueS626
) {
  int32_t _M0L6_2atmpS1636;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS625);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1636 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS625);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGssE(_M0L4selfS624, _M0L3keyS625, _M0L5valueS626, _M0L6_2atmpS1636);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS586
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2023;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS585;
  int32_t _M0L8capacityS1619;
  int32_t _M0L13new__capacityS587;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1614;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1613;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS2022;
  int32_t _M0L6_2atmpS1615;
  int32_t _M0L8capacityS1617;
  int32_t _M0L6_2atmpS1616;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1618;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2021;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS588;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2023 = _M0L4selfS586->$5;
  _M0L9old__headS585 = _M0L8_2afieldS2023;
  _M0L8capacityS1619 = _M0L4selfS586->$2;
  _M0L13new__capacityS587 = _M0L8capacityS1619 << 1;
  _M0L6_2atmpS1614 = 0;
  _M0L6_2atmpS1613
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS587, _M0L6_2atmpS1614);
  _M0L6_2aoldS2022 = _M0L4selfS586->$0;
  if (_M0L9old__headS585) {
    moonbit_incref(_M0L9old__headS585);
  }
  moonbit_decref(_M0L6_2aoldS2022);
  _M0L4selfS586->$0 = _M0L6_2atmpS1613;
  _M0L4selfS586->$2 = _M0L13new__capacityS587;
  _M0L6_2atmpS1615 = _M0L13new__capacityS587 - 1;
  _M0L4selfS586->$3 = _M0L6_2atmpS1615;
  _M0L8capacityS1617 = _M0L4selfS586->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1616 = _M0FPB21calc__grow__threshold(_M0L8capacityS1617);
  _M0L4selfS586->$4 = _M0L6_2atmpS1616;
  _M0L4selfS586->$1 = 0;
  _M0L6_2atmpS1618 = 0;
  _M0L6_2aoldS2021 = _M0L4selfS586->$5;
  if (_M0L6_2aoldS2021) {
    moonbit_decref(_M0L6_2aoldS2021);
  }
  _M0L4selfS586->$5 = _M0L6_2atmpS1618;
  _M0L4selfS586->$6 = -1;
  _M0L8_2aparamS588 = _M0L9old__headS585;
  while (1) {
    if (_M0L8_2aparamS588 == 0) {
      if (_M0L8_2aparamS588) {
        moonbit_decref(_M0L8_2aparamS588);
      }
      moonbit_decref(_M0L4selfS586);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS589 =
        _M0L8_2aparamS588;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS590 =
        _M0L7_2aSomeS589;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2020 =
        _M0L4_2axS590->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS591 =
        _M0L8_2afieldS2020;
      moonbit_string_t _M0L8_2afieldS2019 = _M0L4_2axS590->$4;
      moonbit_string_t _M0L6_2akeyS592 = _M0L8_2afieldS2019;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2018 =
        _M0L4_2axS590->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS593 =
        _M0L8_2afieldS2018;
      int32_t _M0L8_2afieldS2017 = _M0L4_2axS590->$3;
      int32_t _M0L6_2acntS2232 = Moonbit_object_header(_M0L4_2axS590)->rc;
      int32_t _M0L7_2ahashS594;
      if (_M0L6_2acntS2232 > 1) {
        int32_t _M0L11_2anew__cntS2233 = _M0L6_2acntS2232 - 1;
        Moonbit_object_header(_M0L4_2axS590)->rc = _M0L11_2anew__cntS2233;
        moonbit_incref(_M0L8_2avalueS593);
        moonbit_incref(_M0L6_2akeyS592);
        if (_M0L7_2anextS591) {
          moonbit_incref(_M0L7_2anextS591);
        }
      } else if (_M0L6_2acntS2232 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS590);
      }
      _M0L7_2ahashS594 = _M0L8_2afieldS2017;
      moonbit_incref(_M0L4selfS586);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS586, _M0L6_2akeyS592, _M0L8_2avalueS593, _M0L7_2ahashS594);
      _M0L8_2aparamS588 = _M0L7_2anextS591;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS597
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2029;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS596;
  int32_t _M0L8capacityS1626;
  int32_t _M0L13new__capacityS598;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1621;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1620;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS2028;
  int32_t _M0L6_2atmpS1622;
  int32_t _M0L8capacityS1624;
  int32_t _M0L6_2atmpS1623;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1625;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2027;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS599;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2029 = _M0L4selfS597->$5;
  _M0L9old__headS596 = _M0L8_2afieldS2029;
  _M0L8capacityS1626 = _M0L4selfS597->$2;
  _M0L13new__capacityS598 = _M0L8capacityS1626 << 1;
  _M0L6_2atmpS1621 = 0;
  _M0L6_2atmpS1620
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS598, _M0L6_2atmpS1621);
  _M0L6_2aoldS2028 = _M0L4selfS597->$0;
  if (_M0L9old__headS596) {
    moonbit_incref(_M0L9old__headS596);
  }
  moonbit_decref(_M0L6_2aoldS2028);
  _M0L4selfS597->$0 = _M0L6_2atmpS1620;
  _M0L4selfS597->$2 = _M0L13new__capacityS598;
  _M0L6_2atmpS1622 = _M0L13new__capacityS598 - 1;
  _M0L4selfS597->$3 = _M0L6_2atmpS1622;
  _M0L8capacityS1624 = _M0L4selfS597->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1623 = _M0FPB21calc__grow__threshold(_M0L8capacityS1624);
  _M0L4selfS597->$4 = _M0L6_2atmpS1623;
  _M0L4selfS597->$1 = 0;
  _M0L6_2atmpS1625 = 0;
  _M0L6_2aoldS2027 = _M0L4selfS597->$5;
  if (_M0L6_2aoldS2027) {
    moonbit_decref(_M0L6_2aoldS2027);
  }
  _M0L4selfS597->$5 = _M0L6_2atmpS1625;
  _M0L4selfS597->$6 = -1;
  _M0L8_2aparamS599 = _M0L9old__headS596;
  while (1) {
    if (_M0L8_2aparamS599 == 0) {
      if (_M0L8_2aparamS599) {
        moonbit_decref(_M0L8_2aparamS599);
      }
      moonbit_decref(_M0L4selfS597);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS600 =
        _M0L8_2aparamS599;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS601 =
        _M0L7_2aSomeS600;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2026 =
        _M0L4_2axS601->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS602 =
        _M0L8_2afieldS2026;
      int32_t _M0L6_2akeyS603 = _M0L4_2axS601->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2025 =
        _M0L4_2axS601->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS604 =
        _M0L8_2afieldS2025;
      int32_t _M0L8_2afieldS2024 = _M0L4_2axS601->$3;
      int32_t _M0L6_2acntS2234 = Moonbit_object_header(_M0L4_2axS601)->rc;
      int32_t _M0L7_2ahashS605;
      if (_M0L6_2acntS2234 > 1) {
        int32_t _M0L11_2anew__cntS2235 = _M0L6_2acntS2234 - 1;
        Moonbit_object_header(_M0L4_2axS601)->rc = _M0L11_2anew__cntS2235;
        moonbit_incref(_M0L8_2avalueS604);
        if (_M0L7_2anextS602) {
          moonbit_incref(_M0L7_2anextS602);
        }
      } else if (_M0L6_2acntS2234 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS601);
      }
      _M0L7_2ahashS605 = _M0L8_2afieldS2024;
      moonbit_incref(_M0L4selfS597);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS597, _M0L6_2akeyS603, _M0L8_2avalueS604, _M0L7_2ahashS605);
      _M0L8_2aparamS599 = _M0L7_2anextS602;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE* _M0L4selfS608) {
  struct _M0TPB5EntryGssE* _M0L8_2afieldS2036;
  struct _M0TPB5EntryGssE* _M0L9old__headS607;
  int32_t _M0L8capacityS1633;
  int32_t _M0L13new__capacityS609;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS1628;
  struct _M0TPB5EntryGssE** _M0L6_2atmpS1627;
  struct _M0TPB5EntryGssE** _M0L6_2aoldS2035;
  int32_t _M0L6_2atmpS1629;
  int32_t _M0L8capacityS1631;
  int32_t _M0L6_2atmpS1630;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS1632;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS2034;
  struct _M0TPB5EntryGssE* _M0L8_2aparamS610;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2036 = _M0L4selfS608->$5;
  _M0L9old__headS607 = _M0L8_2afieldS2036;
  _M0L8capacityS1633 = _M0L4selfS608->$2;
  _M0L13new__capacityS609 = _M0L8capacityS1633 << 1;
  _M0L6_2atmpS1628 = 0;
  _M0L6_2atmpS1627
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L13new__capacityS609, _M0L6_2atmpS1628);
  _M0L6_2aoldS2035 = _M0L4selfS608->$0;
  if (_M0L9old__headS607) {
    moonbit_incref(_M0L9old__headS607);
  }
  moonbit_decref(_M0L6_2aoldS2035);
  _M0L4selfS608->$0 = _M0L6_2atmpS1627;
  _M0L4selfS608->$2 = _M0L13new__capacityS609;
  _M0L6_2atmpS1629 = _M0L13new__capacityS609 - 1;
  _M0L4selfS608->$3 = _M0L6_2atmpS1629;
  _M0L8capacityS1631 = _M0L4selfS608->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1630 = _M0FPB21calc__grow__threshold(_M0L8capacityS1631);
  _M0L4selfS608->$4 = _M0L6_2atmpS1630;
  _M0L4selfS608->$1 = 0;
  _M0L6_2atmpS1632 = 0;
  _M0L6_2aoldS2034 = _M0L4selfS608->$5;
  if (_M0L6_2aoldS2034) {
    moonbit_decref(_M0L6_2aoldS2034);
  }
  _M0L4selfS608->$5 = _M0L6_2atmpS1632;
  _M0L4selfS608->$6 = -1;
  _M0L8_2aparamS610 = _M0L9old__headS607;
  while (1) {
    if (_M0L8_2aparamS610 == 0) {
      if (_M0L8_2aparamS610) {
        moonbit_decref(_M0L8_2aparamS610);
      }
      moonbit_decref(_M0L4selfS608);
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS611 = _M0L8_2aparamS610;
      struct _M0TPB5EntryGssE* _M0L4_2axS612 = _M0L7_2aSomeS611;
      struct _M0TPB5EntryGssE* _M0L8_2afieldS2033 = _M0L4_2axS612->$1;
      struct _M0TPB5EntryGssE* _M0L7_2anextS613 = _M0L8_2afieldS2033;
      moonbit_string_t _M0L8_2afieldS2032 = _M0L4_2axS612->$4;
      moonbit_string_t _M0L6_2akeyS614 = _M0L8_2afieldS2032;
      moonbit_string_t _M0L8_2afieldS2031 = _M0L4_2axS612->$5;
      moonbit_string_t _M0L8_2avalueS615 = _M0L8_2afieldS2031;
      int32_t _M0L8_2afieldS2030 = _M0L4_2axS612->$3;
      int32_t _M0L6_2acntS2236 = Moonbit_object_header(_M0L4_2axS612)->rc;
      int32_t _M0L7_2ahashS616;
      if (_M0L6_2acntS2236 > 1) {
        int32_t _M0L11_2anew__cntS2237 = _M0L6_2acntS2236 - 1;
        Moonbit_object_header(_M0L4_2axS612)->rc = _M0L11_2anew__cntS2237;
        moonbit_incref(_M0L8_2avalueS615);
        moonbit_incref(_M0L6_2akeyS614);
        if (_M0L7_2anextS613) {
          moonbit_incref(_M0L7_2anextS613);
        }
      } else if (_M0L6_2acntS2236 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS612);
      }
      _M0L7_2ahashS616 = _M0L8_2afieldS2030;
      moonbit_incref(_M0L4selfS608);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGssE(_M0L4selfS608, _M0L6_2akeyS614, _M0L8_2avalueS615, _M0L7_2ahashS616);
      _M0L8_2aparamS610 = _M0L7_2anextS613;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS540,
  moonbit_string_t _M0L3keyS546,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS547,
  int32_t _M0L4hashS542
) {
  int32_t _M0L14capacity__maskS1576;
  int32_t _M0L6_2atmpS1575;
  int32_t _M0L3pslS537;
  int32_t _M0L3idxS538;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1576 = _M0L4selfS540->$3;
  _M0L6_2atmpS1575 = _M0L4hashS542 & _M0L14capacity__maskS1576;
  _M0L3pslS537 = 0;
  _M0L3idxS538 = _M0L6_2atmpS1575;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2041 =
      _M0L4selfS540->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1574 =
      _M0L8_2afieldS2041;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2040;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS539;
    if (
      _M0L3idxS538 < 0
      || _M0L3idxS538 >= Moonbit_array_length(_M0L7entriesS1574)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2040
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1574[
        _M0L3idxS538
      ];
    _M0L7_2abindS539 = _M0L6_2atmpS2040;
    if (_M0L7_2abindS539 == 0) {
      int32_t _M0L4sizeS1559 = _M0L4selfS540->$1;
      int32_t _M0L8grow__atS1560 = _M0L4selfS540->$4;
      int32_t _M0L7_2abindS543;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS544;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS545;
      if (_M0L4sizeS1559 >= _M0L8grow__atS1560) {
        int32_t _M0L14capacity__maskS1562;
        int32_t _M0L6_2atmpS1561;
        moonbit_incref(_M0L4selfS540);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS540);
        _M0L14capacity__maskS1562 = _M0L4selfS540->$3;
        _M0L6_2atmpS1561 = _M0L4hashS542 & _M0L14capacity__maskS1562;
        _M0L3pslS537 = 0;
        _M0L3idxS538 = _M0L6_2atmpS1561;
        continue;
      }
      _M0L7_2abindS543 = _M0L4selfS540->$6;
      _M0L7_2abindS544 = 0;
      _M0L5entryS545
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS545)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS545->$0 = _M0L7_2abindS543;
      _M0L5entryS545->$1 = _M0L7_2abindS544;
      _M0L5entryS545->$2 = _M0L3pslS537;
      _M0L5entryS545->$3 = _M0L4hashS542;
      _M0L5entryS545->$4 = _M0L3keyS546;
      _M0L5entryS545->$5 = _M0L5valueS547;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS540, _M0L3idxS538, _M0L5entryS545);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS548 =
        _M0L7_2abindS539;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS549 =
        _M0L7_2aSomeS548;
      int32_t _M0L4hashS1564 = _M0L14_2acurr__entryS549->$3;
      int32_t _if__result_2322;
      int32_t _M0L3pslS1565;
      int32_t _M0L6_2atmpS1570;
      int32_t _M0L6_2atmpS1572;
      int32_t _M0L14capacity__maskS1573;
      int32_t _M0L6_2atmpS1571;
      if (_M0L4hashS1564 == _M0L4hashS542) {
        moonbit_string_t _M0L8_2afieldS2039 = _M0L14_2acurr__entryS549->$4;
        moonbit_string_t _M0L3keyS1563 = _M0L8_2afieldS2039;
        int32_t _M0L6_2atmpS2038;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2038
        = moonbit_val_array_equal(_M0L3keyS1563, _M0L3keyS546);
        _if__result_2322 = _M0L6_2atmpS2038;
      } else {
        _if__result_2322 = 0;
      }
      if (_if__result_2322) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2037;
        moonbit_incref(_M0L14_2acurr__entryS549);
        moonbit_decref(_M0L3keyS546);
        moonbit_decref(_M0L4selfS540);
        _M0L6_2aoldS2037 = _M0L14_2acurr__entryS549->$5;
        moonbit_decref(_M0L6_2aoldS2037);
        _M0L14_2acurr__entryS549->$5 = _M0L5valueS547;
        moonbit_decref(_M0L14_2acurr__entryS549);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS549);
      }
      _M0L3pslS1565 = _M0L14_2acurr__entryS549->$2;
      if (_M0L3pslS537 > _M0L3pslS1565) {
        int32_t _M0L4sizeS1566 = _M0L4selfS540->$1;
        int32_t _M0L8grow__atS1567 = _M0L4selfS540->$4;
        int32_t _M0L7_2abindS550;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS551;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS552;
        if (_M0L4sizeS1566 >= _M0L8grow__atS1567) {
          int32_t _M0L14capacity__maskS1569;
          int32_t _M0L6_2atmpS1568;
          moonbit_decref(_M0L14_2acurr__entryS549);
          moonbit_incref(_M0L4selfS540);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS540);
          _M0L14capacity__maskS1569 = _M0L4selfS540->$3;
          _M0L6_2atmpS1568 = _M0L4hashS542 & _M0L14capacity__maskS1569;
          _M0L3pslS537 = 0;
          _M0L3idxS538 = _M0L6_2atmpS1568;
          continue;
        }
        moonbit_incref(_M0L4selfS540);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS540, _M0L3idxS538, _M0L14_2acurr__entryS549);
        _M0L7_2abindS550 = _M0L4selfS540->$6;
        _M0L7_2abindS551 = 0;
        _M0L5entryS552
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS552)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS552->$0 = _M0L7_2abindS550;
        _M0L5entryS552->$1 = _M0L7_2abindS551;
        _M0L5entryS552->$2 = _M0L3pslS537;
        _M0L5entryS552->$3 = _M0L4hashS542;
        _M0L5entryS552->$4 = _M0L3keyS546;
        _M0L5entryS552->$5 = _M0L5valueS547;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS540, _M0L3idxS538, _M0L5entryS552);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS549);
      }
      _M0L6_2atmpS1570 = _M0L3pslS537 + 1;
      _M0L6_2atmpS1572 = _M0L3idxS538 + 1;
      _M0L14capacity__maskS1573 = _M0L4selfS540->$3;
      _M0L6_2atmpS1571 = _M0L6_2atmpS1572 & _M0L14capacity__maskS1573;
      _M0L3pslS537 = _M0L6_2atmpS1570;
      _M0L3idxS538 = _M0L6_2atmpS1571;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS556,
  int32_t _M0L3keyS562,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS563,
  int32_t _M0L4hashS558
) {
  int32_t _M0L14capacity__maskS1594;
  int32_t _M0L6_2atmpS1593;
  int32_t _M0L3pslS553;
  int32_t _M0L3idxS554;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1594 = _M0L4selfS556->$3;
  _M0L6_2atmpS1593 = _M0L4hashS558 & _M0L14capacity__maskS1594;
  _M0L3pslS553 = 0;
  _M0L3idxS554 = _M0L6_2atmpS1593;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2044 =
      _M0L4selfS556->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1592 =
      _M0L8_2afieldS2044;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2043;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS555;
    if (
      _M0L3idxS554 < 0
      || _M0L3idxS554 >= Moonbit_array_length(_M0L7entriesS1592)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2043
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1592[
        _M0L3idxS554
      ];
    _M0L7_2abindS555 = _M0L6_2atmpS2043;
    if (_M0L7_2abindS555 == 0) {
      int32_t _M0L4sizeS1577 = _M0L4selfS556->$1;
      int32_t _M0L8grow__atS1578 = _M0L4selfS556->$4;
      int32_t _M0L7_2abindS559;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS560;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS561;
      if (_M0L4sizeS1577 >= _M0L8grow__atS1578) {
        int32_t _M0L14capacity__maskS1580;
        int32_t _M0L6_2atmpS1579;
        moonbit_incref(_M0L4selfS556);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS556);
        _M0L14capacity__maskS1580 = _M0L4selfS556->$3;
        _M0L6_2atmpS1579 = _M0L4hashS558 & _M0L14capacity__maskS1580;
        _M0L3pslS553 = 0;
        _M0L3idxS554 = _M0L6_2atmpS1579;
        continue;
      }
      _M0L7_2abindS559 = _M0L4selfS556->$6;
      _M0L7_2abindS560 = 0;
      _M0L5entryS561
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS561)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS561->$0 = _M0L7_2abindS559;
      _M0L5entryS561->$1 = _M0L7_2abindS560;
      _M0L5entryS561->$2 = _M0L3pslS553;
      _M0L5entryS561->$3 = _M0L4hashS558;
      _M0L5entryS561->$4 = _M0L3keyS562;
      _M0L5entryS561->$5 = _M0L5valueS563;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS556, _M0L3idxS554, _M0L5entryS561);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS564 =
        _M0L7_2abindS555;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS565 =
        _M0L7_2aSomeS564;
      int32_t _M0L4hashS1582 = _M0L14_2acurr__entryS565->$3;
      int32_t _if__result_2324;
      int32_t _M0L3pslS1583;
      int32_t _M0L6_2atmpS1588;
      int32_t _M0L6_2atmpS1590;
      int32_t _M0L14capacity__maskS1591;
      int32_t _M0L6_2atmpS1589;
      if (_M0L4hashS1582 == _M0L4hashS558) {
        int32_t _M0L3keyS1581 = _M0L14_2acurr__entryS565->$4;
        _if__result_2324 = _M0L3keyS1581 == _M0L3keyS562;
      } else {
        _if__result_2324 = 0;
      }
      if (_if__result_2324) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS2042;
        moonbit_incref(_M0L14_2acurr__entryS565);
        moonbit_decref(_M0L4selfS556);
        _M0L6_2aoldS2042 = _M0L14_2acurr__entryS565->$5;
        moonbit_decref(_M0L6_2aoldS2042);
        _M0L14_2acurr__entryS565->$5 = _M0L5valueS563;
        moonbit_decref(_M0L14_2acurr__entryS565);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS565);
      }
      _M0L3pslS1583 = _M0L14_2acurr__entryS565->$2;
      if (_M0L3pslS553 > _M0L3pslS1583) {
        int32_t _M0L4sizeS1584 = _M0L4selfS556->$1;
        int32_t _M0L8grow__atS1585 = _M0L4selfS556->$4;
        int32_t _M0L7_2abindS566;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS567;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS568;
        if (_M0L4sizeS1584 >= _M0L8grow__atS1585) {
          int32_t _M0L14capacity__maskS1587;
          int32_t _M0L6_2atmpS1586;
          moonbit_decref(_M0L14_2acurr__entryS565);
          moonbit_incref(_M0L4selfS556);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS556);
          _M0L14capacity__maskS1587 = _M0L4selfS556->$3;
          _M0L6_2atmpS1586 = _M0L4hashS558 & _M0L14capacity__maskS1587;
          _M0L3pslS553 = 0;
          _M0L3idxS554 = _M0L6_2atmpS1586;
          continue;
        }
        moonbit_incref(_M0L4selfS556);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS556, _M0L3idxS554, _M0L14_2acurr__entryS565);
        _M0L7_2abindS566 = _M0L4selfS556->$6;
        _M0L7_2abindS567 = 0;
        _M0L5entryS568
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS568)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS568->$0 = _M0L7_2abindS566;
        _M0L5entryS568->$1 = _M0L7_2abindS567;
        _M0L5entryS568->$2 = _M0L3pslS553;
        _M0L5entryS568->$3 = _M0L4hashS558;
        _M0L5entryS568->$4 = _M0L3keyS562;
        _M0L5entryS568->$5 = _M0L5valueS563;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS556, _M0L3idxS554, _M0L5entryS568);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS565);
      }
      _M0L6_2atmpS1588 = _M0L3pslS553 + 1;
      _M0L6_2atmpS1590 = _M0L3idxS554 + 1;
      _M0L14capacity__maskS1591 = _M0L4selfS556->$3;
      _M0L6_2atmpS1589 = _M0L6_2atmpS1590 & _M0L14capacity__maskS1591;
      _M0L3pslS553 = _M0L6_2atmpS1588;
      _M0L3idxS554 = _M0L6_2atmpS1589;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE* _M0L4selfS572,
  moonbit_string_t _M0L3keyS578,
  moonbit_string_t _M0L5valueS579,
  int32_t _M0L4hashS574
) {
  int32_t _M0L14capacity__maskS1612;
  int32_t _M0L6_2atmpS1611;
  int32_t _M0L3pslS569;
  int32_t _M0L3idxS570;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS1612 = _M0L4selfS572->$3;
  _M0L6_2atmpS1611 = _M0L4hashS574 & _M0L14capacity__maskS1612;
  _M0L3pslS569 = 0;
  _M0L3idxS570 = _M0L6_2atmpS1611;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L8_2afieldS2049 = _M0L4selfS572->$0;
    struct _M0TPB5EntryGssE** _M0L7entriesS1610 = _M0L8_2afieldS2049;
    struct _M0TPB5EntryGssE* _M0L6_2atmpS2048;
    struct _M0TPB5EntryGssE* _M0L7_2abindS571;
    if (
      _M0L3idxS570 < 0
      || _M0L3idxS570 >= Moonbit_array_length(_M0L7entriesS1610)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2048
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS1610[_M0L3idxS570];
    _M0L7_2abindS571 = _M0L6_2atmpS2048;
    if (_M0L7_2abindS571 == 0) {
      int32_t _M0L4sizeS1595 = _M0L4selfS572->$1;
      int32_t _M0L8grow__atS1596 = _M0L4selfS572->$4;
      int32_t _M0L7_2abindS575;
      struct _M0TPB5EntryGssE* _M0L7_2abindS576;
      struct _M0TPB5EntryGssE* _M0L5entryS577;
      if (_M0L4sizeS1595 >= _M0L8grow__atS1596) {
        int32_t _M0L14capacity__maskS1598;
        int32_t _M0L6_2atmpS1597;
        moonbit_incref(_M0L4selfS572);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGssE(_M0L4selfS572);
        _M0L14capacity__maskS1598 = _M0L4selfS572->$3;
        _M0L6_2atmpS1597 = _M0L4hashS574 & _M0L14capacity__maskS1598;
        _M0L3pslS569 = 0;
        _M0L3idxS570 = _M0L6_2atmpS1597;
        continue;
      }
      _M0L7_2abindS575 = _M0L4selfS572->$6;
      _M0L7_2abindS576 = 0;
      _M0L5entryS577
      = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
      Moonbit_object_header(_M0L5entryS577)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGssE, $1) >> 2, 3, 0);
      _M0L5entryS577->$0 = _M0L7_2abindS575;
      _M0L5entryS577->$1 = _M0L7_2abindS576;
      _M0L5entryS577->$2 = _M0L3pslS569;
      _M0L5entryS577->$3 = _M0L4hashS574;
      _M0L5entryS577->$4 = _M0L3keyS578;
      _M0L5entryS577->$5 = _M0L5valueS579;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS572, _M0L3idxS570, _M0L5entryS577);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS580 = _M0L7_2abindS571;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS581 = _M0L7_2aSomeS580;
      int32_t _M0L4hashS1600 = _M0L14_2acurr__entryS581->$3;
      int32_t _if__result_2326;
      int32_t _M0L3pslS1601;
      int32_t _M0L6_2atmpS1606;
      int32_t _M0L6_2atmpS1608;
      int32_t _M0L14capacity__maskS1609;
      int32_t _M0L6_2atmpS1607;
      if (_M0L4hashS1600 == _M0L4hashS574) {
        moonbit_string_t _M0L8_2afieldS2047 = _M0L14_2acurr__entryS581->$4;
        moonbit_string_t _M0L3keyS1599 = _M0L8_2afieldS2047;
        int32_t _M0L6_2atmpS2046;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2046
        = moonbit_val_array_equal(_M0L3keyS1599, _M0L3keyS578);
        _if__result_2326 = _M0L6_2atmpS2046;
      } else {
        _if__result_2326 = 0;
      }
      if (_if__result_2326) {
        moonbit_string_t _M0L6_2aoldS2045;
        moonbit_incref(_M0L14_2acurr__entryS581);
        moonbit_decref(_M0L3keyS578);
        moonbit_decref(_M0L4selfS572);
        _M0L6_2aoldS2045 = _M0L14_2acurr__entryS581->$5;
        moonbit_decref(_M0L6_2aoldS2045);
        _M0L14_2acurr__entryS581->$5 = _M0L5valueS579;
        moonbit_decref(_M0L14_2acurr__entryS581);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS581);
      }
      _M0L3pslS1601 = _M0L14_2acurr__entryS581->$2;
      if (_M0L3pslS569 > _M0L3pslS1601) {
        int32_t _M0L4sizeS1602 = _M0L4selfS572->$1;
        int32_t _M0L8grow__atS1603 = _M0L4selfS572->$4;
        int32_t _M0L7_2abindS582;
        struct _M0TPB5EntryGssE* _M0L7_2abindS583;
        struct _M0TPB5EntryGssE* _M0L5entryS584;
        if (_M0L4sizeS1602 >= _M0L8grow__atS1603) {
          int32_t _M0L14capacity__maskS1605;
          int32_t _M0L6_2atmpS1604;
          moonbit_decref(_M0L14_2acurr__entryS581);
          moonbit_incref(_M0L4selfS572);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGssE(_M0L4selfS572);
          _M0L14capacity__maskS1605 = _M0L4selfS572->$3;
          _M0L6_2atmpS1604 = _M0L4hashS574 & _M0L14capacity__maskS1605;
          _M0L3pslS569 = 0;
          _M0L3idxS570 = _M0L6_2atmpS1604;
          continue;
        }
        moonbit_incref(_M0L4selfS572);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS572, _M0L3idxS570, _M0L14_2acurr__entryS581);
        _M0L7_2abindS582 = _M0L4selfS572->$6;
        _M0L7_2abindS583 = 0;
        _M0L5entryS584
        = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
        Moonbit_object_header(_M0L5entryS584)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGssE, $1) >> 2, 3, 0);
        _M0L5entryS584->$0 = _M0L7_2abindS582;
        _M0L5entryS584->$1 = _M0L7_2abindS583;
        _M0L5entryS584->$2 = _M0L3pslS569;
        _M0L5entryS584->$3 = _M0L4hashS574;
        _M0L5entryS584->$4 = _M0L3keyS578;
        _M0L5entryS584->$5 = _M0L5valueS579;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS572, _M0L3idxS570, _M0L5entryS584);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS581);
      }
      _M0L6_2atmpS1606 = _M0L3pslS569 + 1;
      _M0L6_2atmpS1608 = _M0L3idxS570 + 1;
      _M0L14capacity__maskS1609 = _M0L4selfS572->$3;
      _M0L6_2atmpS1607 = _M0L6_2atmpS1608 & _M0L14capacity__maskS1609;
      _M0L3pslS569 = _M0L6_2atmpS1606;
      _M0L3idxS570 = _M0L6_2atmpS1607;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS511,
  int32_t _M0L3idxS516,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS515
) {
  int32_t _M0L3pslS1526;
  int32_t _M0L6_2atmpS1522;
  int32_t _M0L6_2atmpS1524;
  int32_t _M0L14capacity__maskS1525;
  int32_t _M0L6_2atmpS1523;
  int32_t _M0L3pslS507;
  int32_t _M0L3idxS508;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS509;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1526 = _M0L5entryS515->$2;
  _M0L6_2atmpS1522 = _M0L3pslS1526 + 1;
  _M0L6_2atmpS1524 = _M0L3idxS516 + 1;
  _M0L14capacity__maskS1525 = _M0L4selfS511->$3;
  _M0L6_2atmpS1523 = _M0L6_2atmpS1524 & _M0L14capacity__maskS1525;
  _M0L3pslS507 = _M0L6_2atmpS1522;
  _M0L3idxS508 = _M0L6_2atmpS1523;
  _M0L5entryS509 = _M0L5entryS515;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2051 =
      _M0L4selfS511->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1521 =
      _M0L8_2afieldS2051;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2050;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS510;
    if (
      _M0L3idxS508 < 0
      || _M0L3idxS508 >= Moonbit_array_length(_M0L7entriesS1521)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2050
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1521[
        _M0L3idxS508
      ];
    _M0L7_2abindS510 = _M0L6_2atmpS2050;
    if (_M0L7_2abindS510 == 0) {
      _M0L5entryS509->$2 = _M0L3pslS507;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS511, _M0L5entryS509, _M0L3idxS508);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS513 =
        _M0L7_2abindS510;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS514 =
        _M0L7_2aSomeS513;
      int32_t _M0L3pslS1511 = _M0L14_2acurr__entryS514->$2;
      if (_M0L3pslS507 > _M0L3pslS1511) {
        int32_t _M0L3pslS1516;
        int32_t _M0L6_2atmpS1512;
        int32_t _M0L6_2atmpS1514;
        int32_t _M0L14capacity__maskS1515;
        int32_t _M0L6_2atmpS1513;
        _M0L5entryS509->$2 = _M0L3pslS507;
        moonbit_incref(_M0L14_2acurr__entryS514);
        moonbit_incref(_M0L4selfS511);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS511, _M0L5entryS509, _M0L3idxS508);
        _M0L3pslS1516 = _M0L14_2acurr__entryS514->$2;
        _M0L6_2atmpS1512 = _M0L3pslS1516 + 1;
        _M0L6_2atmpS1514 = _M0L3idxS508 + 1;
        _M0L14capacity__maskS1515 = _M0L4selfS511->$3;
        _M0L6_2atmpS1513 = _M0L6_2atmpS1514 & _M0L14capacity__maskS1515;
        _M0L3pslS507 = _M0L6_2atmpS1512;
        _M0L3idxS508 = _M0L6_2atmpS1513;
        _M0L5entryS509 = _M0L14_2acurr__entryS514;
        continue;
      } else {
        int32_t _M0L6_2atmpS1517 = _M0L3pslS507 + 1;
        int32_t _M0L6_2atmpS1519 = _M0L3idxS508 + 1;
        int32_t _M0L14capacity__maskS1520 = _M0L4selfS511->$3;
        int32_t _M0L6_2atmpS1518 =
          _M0L6_2atmpS1519 & _M0L14capacity__maskS1520;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_2328 =
          _M0L5entryS509;
        _M0L3pslS507 = _M0L6_2atmpS1517;
        _M0L3idxS508 = _M0L6_2atmpS1518;
        _M0L5entryS509 = _tmp_2328;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS521,
  int32_t _M0L3idxS526,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS525
) {
  int32_t _M0L3pslS1542;
  int32_t _M0L6_2atmpS1538;
  int32_t _M0L6_2atmpS1540;
  int32_t _M0L14capacity__maskS1541;
  int32_t _M0L6_2atmpS1539;
  int32_t _M0L3pslS517;
  int32_t _M0L3idxS518;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS519;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1542 = _M0L5entryS525->$2;
  _M0L6_2atmpS1538 = _M0L3pslS1542 + 1;
  _M0L6_2atmpS1540 = _M0L3idxS526 + 1;
  _M0L14capacity__maskS1541 = _M0L4selfS521->$3;
  _M0L6_2atmpS1539 = _M0L6_2atmpS1540 & _M0L14capacity__maskS1541;
  _M0L3pslS517 = _M0L6_2atmpS1538;
  _M0L3idxS518 = _M0L6_2atmpS1539;
  _M0L5entryS519 = _M0L5entryS525;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2053 =
      _M0L4selfS521->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1537 =
      _M0L8_2afieldS2053;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2052;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS520;
    if (
      _M0L3idxS518 < 0
      || _M0L3idxS518 >= Moonbit_array_length(_M0L7entriesS1537)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2052
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1537[
        _M0L3idxS518
      ];
    _M0L7_2abindS520 = _M0L6_2atmpS2052;
    if (_M0L7_2abindS520 == 0) {
      _M0L5entryS519->$2 = _M0L3pslS517;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS521, _M0L5entryS519, _M0L3idxS518);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS523 =
        _M0L7_2abindS520;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS524 =
        _M0L7_2aSomeS523;
      int32_t _M0L3pslS1527 = _M0L14_2acurr__entryS524->$2;
      if (_M0L3pslS517 > _M0L3pslS1527) {
        int32_t _M0L3pslS1532;
        int32_t _M0L6_2atmpS1528;
        int32_t _M0L6_2atmpS1530;
        int32_t _M0L14capacity__maskS1531;
        int32_t _M0L6_2atmpS1529;
        _M0L5entryS519->$2 = _M0L3pslS517;
        moonbit_incref(_M0L14_2acurr__entryS524);
        moonbit_incref(_M0L4selfS521);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS521, _M0L5entryS519, _M0L3idxS518);
        _M0L3pslS1532 = _M0L14_2acurr__entryS524->$2;
        _M0L6_2atmpS1528 = _M0L3pslS1532 + 1;
        _M0L6_2atmpS1530 = _M0L3idxS518 + 1;
        _M0L14capacity__maskS1531 = _M0L4selfS521->$3;
        _M0L6_2atmpS1529 = _M0L6_2atmpS1530 & _M0L14capacity__maskS1531;
        _M0L3pslS517 = _M0L6_2atmpS1528;
        _M0L3idxS518 = _M0L6_2atmpS1529;
        _M0L5entryS519 = _M0L14_2acurr__entryS524;
        continue;
      } else {
        int32_t _M0L6_2atmpS1533 = _M0L3pslS517 + 1;
        int32_t _M0L6_2atmpS1535 = _M0L3idxS518 + 1;
        int32_t _M0L14capacity__maskS1536 = _M0L4selfS521->$3;
        int32_t _M0L6_2atmpS1534 =
          _M0L6_2atmpS1535 & _M0L14capacity__maskS1536;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_2330 =
          _M0L5entryS519;
        _M0L3pslS517 = _M0L6_2atmpS1533;
        _M0L3idxS518 = _M0L6_2atmpS1534;
        _M0L5entryS519 = _tmp_2330;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGssE(
  struct _M0TPB3MapGssE* _M0L4selfS531,
  int32_t _M0L3idxS536,
  struct _M0TPB5EntryGssE* _M0L5entryS535
) {
  int32_t _M0L3pslS1558;
  int32_t _M0L6_2atmpS1554;
  int32_t _M0L6_2atmpS1556;
  int32_t _M0L14capacity__maskS1557;
  int32_t _M0L6_2atmpS1555;
  int32_t _M0L3pslS527;
  int32_t _M0L3idxS528;
  struct _M0TPB5EntryGssE* _M0L5entryS529;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1558 = _M0L5entryS535->$2;
  _M0L6_2atmpS1554 = _M0L3pslS1558 + 1;
  _M0L6_2atmpS1556 = _M0L3idxS536 + 1;
  _M0L14capacity__maskS1557 = _M0L4selfS531->$3;
  _M0L6_2atmpS1555 = _M0L6_2atmpS1556 & _M0L14capacity__maskS1557;
  _M0L3pslS527 = _M0L6_2atmpS1554;
  _M0L3idxS528 = _M0L6_2atmpS1555;
  _M0L5entryS529 = _M0L5entryS535;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L8_2afieldS2055 = _M0L4selfS531->$0;
    struct _M0TPB5EntryGssE** _M0L7entriesS1553 = _M0L8_2afieldS2055;
    struct _M0TPB5EntryGssE* _M0L6_2atmpS2054;
    struct _M0TPB5EntryGssE* _M0L7_2abindS530;
    if (
      _M0L3idxS528 < 0
      || _M0L3idxS528 >= Moonbit_array_length(_M0L7entriesS1553)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2054
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS1553[_M0L3idxS528];
    _M0L7_2abindS530 = _M0L6_2atmpS2054;
    if (_M0L7_2abindS530 == 0) {
      _M0L5entryS529->$2 = _M0L3pslS527;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGssE(_M0L4selfS531, _M0L5entryS529, _M0L3idxS528);
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS533 = _M0L7_2abindS530;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS534 = _M0L7_2aSomeS533;
      int32_t _M0L3pslS1543 = _M0L14_2acurr__entryS534->$2;
      if (_M0L3pslS527 > _M0L3pslS1543) {
        int32_t _M0L3pslS1548;
        int32_t _M0L6_2atmpS1544;
        int32_t _M0L6_2atmpS1546;
        int32_t _M0L14capacity__maskS1547;
        int32_t _M0L6_2atmpS1545;
        _M0L5entryS529->$2 = _M0L3pslS527;
        moonbit_incref(_M0L14_2acurr__entryS534);
        moonbit_incref(_M0L4selfS531);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGssE(_M0L4selfS531, _M0L5entryS529, _M0L3idxS528);
        _M0L3pslS1548 = _M0L14_2acurr__entryS534->$2;
        _M0L6_2atmpS1544 = _M0L3pslS1548 + 1;
        _M0L6_2atmpS1546 = _M0L3idxS528 + 1;
        _M0L14capacity__maskS1547 = _M0L4selfS531->$3;
        _M0L6_2atmpS1545 = _M0L6_2atmpS1546 & _M0L14capacity__maskS1547;
        _M0L3pslS527 = _M0L6_2atmpS1544;
        _M0L3idxS528 = _M0L6_2atmpS1545;
        _M0L5entryS529 = _M0L14_2acurr__entryS534;
        continue;
      } else {
        int32_t _M0L6_2atmpS1549 = _M0L3pslS527 + 1;
        int32_t _M0L6_2atmpS1551 = _M0L3idxS528 + 1;
        int32_t _M0L14capacity__maskS1552 = _M0L4selfS531->$3;
        int32_t _M0L6_2atmpS1550 =
          _M0L6_2atmpS1551 & _M0L14capacity__maskS1552;
        struct _M0TPB5EntryGssE* _tmp_2332 = _M0L5entryS529;
        _M0L3pslS527 = _M0L6_2atmpS1549;
        _M0L3idxS528 = _M0L6_2atmpS1550;
        _M0L5entryS529 = _tmp_2332;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS489,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS491,
  int32_t _M0L8new__idxS490
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2058;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1505;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1506;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2057;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS2056;
  int32_t _M0L6_2acntS2238;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS492;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2058 = _M0L4selfS489->$0;
  _M0L7entriesS1505 = _M0L8_2afieldS2058;
  moonbit_incref(_M0L5entryS491);
  _M0L6_2atmpS1506 = _M0L5entryS491;
  if (
    _M0L8new__idxS490 < 0
    || _M0L8new__idxS490 >= Moonbit_array_length(_M0L7entriesS1505)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2057
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1505[
      _M0L8new__idxS490
    ];
  if (_M0L6_2aoldS2057) {
    moonbit_decref(_M0L6_2aoldS2057);
  }
  _M0L7entriesS1505[_M0L8new__idxS490] = _M0L6_2atmpS1506;
  _M0L8_2afieldS2056 = _M0L5entryS491->$1;
  _M0L6_2acntS2238 = Moonbit_object_header(_M0L5entryS491)->rc;
  if (_M0L6_2acntS2238 > 1) {
    int32_t _M0L11_2anew__cntS2241 = _M0L6_2acntS2238 - 1;
    Moonbit_object_header(_M0L5entryS491)->rc = _M0L11_2anew__cntS2241;
    if (_M0L8_2afieldS2056) {
      moonbit_incref(_M0L8_2afieldS2056);
    }
  } else if (_M0L6_2acntS2238 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2240 =
      _M0L5entryS491->$5;
    moonbit_string_t _M0L8_2afieldS2239;
    moonbit_decref(_M0L8_2afieldS2240);
    _M0L8_2afieldS2239 = _M0L5entryS491->$4;
    moonbit_decref(_M0L8_2afieldS2239);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS491);
  }
  _M0L7_2abindS492 = _M0L8_2afieldS2056;
  if (_M0L7_2abindS492 == 0) {
    if (_M0L7_2abindS492) {
      moonbit_decref(_M0L7_2abindS492);
    }
    _M0L4selfS489->$6 = _M0L8new__idxS490;
    moonbit_decref(_M0L4selfS489);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS493;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS494;
    moonbit_decref(_M0L4selfS489);
    _M0L7_2aSomeS493 = _M0L7_2abindS492;
    _M0L7_2anextS494 = _M0L7_2aSomeS493;
    _M0L7_2anextS494->$0 = _M0L8new__idxS490;
    moonbit_decref(_M0L7_2anextS494);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS495,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS497,
  int32_t _M0L8new__idxS496
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2061;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1507;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1508;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2060;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2059;
  int32_t _M0L6_2acntS2242;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS498;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2061 = _M0L4selfS495->$0;
  _M0L7entriesS1507 = _M0L8_2afieldS2061;
  moonbit_incref(_M0L5entryS497);
  _M0L6_2atmpS1508 = _M0L5entryS497;
  if (
    _M0L8new__idxS496 < 0
    || _M0L8new__idxS496 >= Moonbit_array_length(_M0L7entriesS1507)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2060
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1507[
      _M0L8new__idxS496
    ];
  if (_M0L6_2aoldS2060) {
    moonbit_decref(_M0L6_2aoldS2060);
  }
  _M0L7entriesS1507[_M0L8new__idxS496] = _M0L6_2atmpS1508;
  _M0L8_2afieldS2059 = _M0L5entryS497->$1;
  _M0L6_2acntS2242 = Moonbit_object_header(_M0L5entryS497)->rc;
  if (_M0L6_2acntS2242 > 1) {
    int32_t _M0L11_2anew__cntS2244 = _M0L6_2acntS2242 - 1;
    Moonbit_object_header(_M0L5entryS497)->rc = _M0L11_2anew__cntS2244;
    if (_M0L8_2afieldS2059) {
      moonbit_incref(_M0L8_2afieldS2059);
    }
  } else if (_M0L6_2acntS2242 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2243 =
      _M0L5entryS497->$5;
    moonbit_decref(_M0L8_2afieldS2243);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS497);
  }
  _M0L7_2abindS498 = _M0L8_2afieldS2059;
  if (_M0L7_2abindS498 == 0) {
    if (_M0L7_2abindS498) {
      moonbit_decref(_M0L7_2abindS498);
    }
    _M0L4selfS495->$6 = _M0L8new__idxS496;
    moonbit_decref(_M0L4selfS495);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS499;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS500;
    moonbit_decref(_M0L4selfS495);
    _M0L7_2aSomeS499 = _M0L7_2abindS498;
    _M0L7_2anextS500 = _M0L7_2aSomeS499;
    _M0L7_2anextS500->$0 = _M0L8new__idxS496;
    moonbit_decref(_M0L7_2anextS500);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS501,
  struct _M0TPB5EntryGssE* _M0L5entryS503,
  int32_t _M0L8new__idxS502
) {
  struct _M0TPB5EntryGssE** _M0L8_2afieldS2064;
  struct _M0TPB5EntryGssE** _M0L7entriesS1509;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS1510;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS2063;
  struct _M0TPB5EntryGssE* _M0L8_2afieldS2062;
  int32_t _M0L6_2acntS2245;
  struct _M0TPB5EntryGssE* _M0L7_2abindS504;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS2064 = _M0L4selfS501->$0;
  _M0L7entriesS1509 = _M0L8_2afieldS2064;
  moonbit_incref(_M0L5entryS503);
  _M0L6_2atmpS1510 = _M0L5entryS503;
  if (
    _M0L8new__idxS502 < 0
    || _M0L8new__idxS502 >= Moonbit_array_length(_M0L7entriesS1509)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2063
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS1509[_M0L8new__idxS502];
  if (_M0L6_2aoldS2063) {
    moonbit_decref(_M0L6_2aoldS2063);
  }
  _M0L7entriesS1509[_M0L8new__idxS502] = _M0L6_2atmpS1510;
  _M0L8_2afieldS2062 = _M0L5entryS503->$1;
  _M0L6_2acntS2245 = Moonbit_object_header(_M0L5entryS503)->rc;
  if (_M0L6_2acntS2245 > 1) {
    int32_t _M0L11_2anew__cntS2248 = _M0L6_2acntS2245 - 1;
    Moonbit_object_header(_M0L5entryS503)->rc = _M0L11_2anew__cntS2248;
    if (_M0L8_2afieldS2062) {
      moonbit_incref(_M0L8_2afieldS2062);
    }
  } else if (_M0L6_2acntS2245 == 1) {
    moonbit_string_t _M0L8_2afieldS2247 = _M0L5entryS503->$5;
    moonbit_string_t _M0L8_2afieldS2246;
    moonbit_decref(_M0L8_2afieldS2247);
    _M0L8_2afieldS2246 = _M0L5entryS503->$4;
    moonbit_decref(_M0L8_2afieldS2246);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS503);
  }
  _M0L7_2abindS504 = _M0L8_2afieldS2062;
  if (_M0L7_2abindS504 == 0) {
    if (_M0L7_2abindS504) {
      moonbit_decref(_M0L7_2abindS504);
    }
    _M0L4selfS501->$6 = _M0L8new__idxS502;
    moonbit_decref(_M0L4selfS501);
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS505;
    struct _M0TPB5EntryGssE* _M0L7_2anextS506;
    moonbit_decref(_M0L4selfS501);
    _M0L7_2aSomeS505 = _M0L7_2abindS504;
    _M0L7_2anextS506 = _M0L7_2aSomeS505;
    _M0L7_2anextS506->$0 = _M0L8new__idxS502;
    moonbit_decref(_M0L7_2anextS506);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS478,
  int32_t _M0L3idxS480,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS479
) {
  int32_t _M0L7_2abindS477;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2066;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1483;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1484;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2065;
  int32_t _M0L4sizeS1486;
  int32_t _M0L6_2atmpS1485;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS477 = _M0L4selfS478->$6;
  switch (_M0L7_2abindS477) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1478;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2067;
      moonbit_incref(_M0L5entryS479);
      _M0L6_2atmpS1478 = _M0L5entryS479;
      _M0L6_2aoldS2067 = _M0L4selfS478->$5;
      if (_M0L6_2aoldS2067) {
        moonbit_decref(_M0L6_2aoldS2067);
      }
      _M0L4selfS478->$5 = _M0L6_2atmpS1478;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2070 =
        _M0L4selfS478->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1482 =
        _M0L8_2afieldS2070;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2069;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1481;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1479;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1480;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS2068;
      if (
        _M0L7_2abindS477 < 0
        || _M0L7_2abindS477 >= Moonbit_array_length(_M0L7entriesS1482)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2069
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1482[
          _M0L7_2abindS477
        ];
      _M0L6_2atmpS1481 = _M0L6_2atmpS2069;
      if (_M0L6_2atmpS1481) {
        moonbit_incref(_M0L6_2atmpS1481);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1479
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1481);
      moonbit_incref(_M0L5entryS479);
      _M0L6_2atmpS1480 = _M0L5entryS479;
      _M0L6_2aoldS2068 = _M0L6_2atmpS1479->$1;
      if (_M0L6_2aoldS2068) {
        moonbit_decref(_M0L6_2aoldS2068);
      }
      _M0L6_2atmpS1479->$1 = _M0L6_2atmpS1480;
      moonbit_decref(_M0L6_2atmpS1479);
      break;
    }
  }
  _M0L4selfS478->$6 = _M0L3idxS480;
  _M0L8_2afieldS2066 = _M0L4selfS478->$0;
  _M0L7entriesS1483 = _M0L8_2afieldS2066;
  _M0L6_2atmpS1484 = _M0L5entryS479;
  if (
    _M0L3idxS480 < 0
    || _M0L3idxS480 >= Moonbit_array_length(_M0L7entriesS1483)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2065
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1483[
      _M0L3idxS480
    ];
  if (_M0L6_2aoldS2065) {
    moonbit_decref(_M0L6_2aoldS2065);
  }
  _M0L7entriesS1483[_M0L3idxS480] = _M0L6_2atmpS1484;
  _M0L4sizeS1486 = _M0L4selfS478->$1;
  _M0L6_2atmpS1485 = _M0L4sizeS1486 + 1;
  _M0L4selfS478->$1 = _M0L6_2atmpS1485;
  moonbit_decref(_M0L4selfS478);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS482,
  int32_t _M0L3idxS484,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS483
) {
  int32_t _M0L7_2abindS481;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2072;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1492;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1493;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2071;
  int32_t _M0L4sizeS1495;
  int32_t _M0L6_2atmpS1494;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS481 = _M0L4selfS482->$6;
  switch (_M0L7_2abindS481) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1487;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2073;
      moonbit_incref(_M0L5entryS483);
      _M0L6_2atmpS1487 = _M0L5entryS483;
      _M0L6_2aoldS2073 = _M0L4selfS482->$5;
      if (_M0L6_2aoldS2073) {
        moonbit_decref(_M0L6_2aoldS2073);
      }
      _M0L4selfS482->$5 = _M0L6_2atmpS1487;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2076 =
        _M0L4selfS482->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1491 =
        _M0L8_2afieldS2076;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2075;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1490;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1488;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1489;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS2074;
      if (
        _M0L7_2abindS481 < 0
        || _M0L7_2abindS481 >= Moonbit_array_length(_M0L7entriesS1491)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2075
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1491[
          _M0L7_2abindS481
        ];
      _M0L6_2atmpS1490 = _M0L6_2atmpS2075;
      if (_M0L6_2atmpS1490) {
        moonbit_incref(_M0L6_2atmpS1490);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1488
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1490);
      moonbit_incref(_M0L5entryS483);
      _M0L6_2atmpS1489 = _M0L5entryS483;
      _M0L6_2aoldS2074 = _M0L6_2atmpS1488->$1;
      if (_M0L6_2aoldS2074) {
        moonbit_decref(_M0L6_2aoldS2074);
      }
      _M0L6_2atmpS1488->$1 = _M0L6_2atmpS1489;
      moonbit_decref(_M0L6_2atmpS1488);
      break;
    }
  }
  _M0L4selfS482->$6 = _M0L3idxS484;
  _M0L8_2afieldS2072 = _M0L4selfS482->$0;
  _M0L7entriesS1492 = _M0L8_2afieldS2072;
  _M0L6_2atmpS1493 = _M0L5entryS483;
  if (
    _M0L3idxS484 < 0
    || _M0L3idxS484 >= Moonbit_array_length(_M0L7entriesS1492)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2071
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1492[
      _M0L3idxS484
    ];
  if (_M0L6_2aoldS2071) {
    moonbit_decref(_M0L6_2aoldS2071);
  }
  _M0L7entriesS1492[_M0L3idxS484] = _M0L6_2atmpS1493;
  _M0L4sizeS1495 = _M0L4selfS482->$1;
  _M0L6_2atmpS1494 = _M0L4sizeS1495 + 1;
  _M0L4selfS482->$1 = _M0L6_2atmpS1494;
  moonbit_decref(_M0L4selfS482);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE* _M0L4selfS486,
  int32_t _M0L3idxS488,
  struct _M0TPB5EntryGssE* _M0L5entryS487
) {
  int32_t _M0L7_2abindS485;
  struct _M0TPB5EntryGssE** _M0L8_2afieldS2078;
  struct _M0TPB5EntryGssE** _M0L7entriesS1501;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS1502;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS2077;
  int32_t _M0L4sizeS1504;
  int32_t _M0L6_2atmpS1503;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS485 = _M0L4selfS486->$6;
  switch (_M0L7_2abindS485) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L6_2atmpS1496;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS2079;
      moonbit_incref(_M0L5entryS487);
      _M0L6_2atmpS1496 = _M0L5entryS487;
      _M0L6_2aoldS2079 = _M0L4selfS486->$5;
      if (_M0L6_2aoldS2079) {
        moonbit_decref(_M0L6_2aoldS2079);
      }
      _M0L4selfS486->$5 = _M0L6_2atmpS1496;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L8_2afieldS2082 = _M0L4selfS486->$0;
      struct _M0TPB5EntryGssE** _M0L7entriesS1500 = _M0L8_2afieldS2082;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS2081;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS1499;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS1497;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS1498;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS2080;
      if (
        _M0L7_2abindS485 < 0
        || _M0L7_2abindS485 >= Moonbit_array_length(_M0L7entriesS1500)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2081
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS1500[_M0L7_2abindS485];
      _M0L6_2atmpS1499 = _M0L6_2atmpS2081;
      if (_M0L6_2atmpS1499) {
        moonbit_incref(_M0L6_2atmpS1499);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1497
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS1499);
      moonbit_incref(_M0L5entryS487);
      _M0L6_2atmpS1498 = _M0L5entryS487;
      _M0L6_2aoldS2080 = _M0L6_2atmpS1497->$1;
      if (_M0L6_2aoldS2080) {
        moonbit_decref(_M0L6_2aoldS2080);
      }
      _M0L6_2atmpS1497->$1 = _M0L6_2atmpS1498;
      moonbit_decref(_M0L6_2atmpS1497);
      break;
    }
  }
  _M0L4selfS486->$6 = _M0L3idxS488;
  _M0L8_2afieldS2078 = _M0L4selfS486->$0;
  _M0L7entriesS1501 = _M0L8_2afieldS2078;
  _M0L6_2atmpS1502 = _M0L5entryS487;
  if (
    _M0L3idxS488 < 0
    || _M0L3idxS488 >= Moonbit_array_length(_M0L7entriesS1501)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS2077
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS1501[_M0L3idxS488];
  if (_M0L6_2aoldS2077) {
    moonbit_decref(_M0L6_2aoldS2077);
  }
  _M0L7entriesS1501[_M0L3idxS488] = _M0L6_2atmpS1502;
  _M0L4sizeS1504 = _M0L4selfS486->$1;
  _M0L6_2atmpS1503 = _M0L4sizeS1504 + 1;
  _M0L4selfS486->$1 = _M0L6_2atmpS1503;
  moonbit_decref(_M0L4selfS486);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS460
) {
  int32_t _M0L8capacityS459;
  int32_t _M0L7_2abindS461;
  int32_t _M0L7_2abindS462;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1475;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS463;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS464;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_2333;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS459
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS460);
  _M0L7_2abindS461 = _M0L8capacityS459 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS462 = _M0FPB21calc__grow__threshold(_M0L8capacityS459);
  _M0L6_2atmpS1475 = 0;
  _M0L7_2abindS463
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS459, _M0L6_2atmpS1475);
  _M0L7_2abindS464 = 0;
  _block_2333
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_2333)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_2333->$0 = _M0L7_2abindS463;
  _block_2333->$1 = 0;
  _block_2333->$2 = _M0L8capacityS459;
  _block_2333->$3 = _M0L7_2abindS461;
  _block_2333->$4 = _M0L7_2abindS462;
  _block_2333->$5 = _M0L7_2abindS464;
  _block_2333->$6 = -1;
  return _block_2333;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS466
) {
  int32_t _M0L8capacityS465;
  int32_t _M0L7_2abindS467;
  int32_t _M0L7_2abindS468;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1476;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS469;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS470;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_2334;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS465
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS466);
  _M0L7_2abindS467 = _M0L8capacityS465 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS468 = _M0FPB21calc__grow__threshold(_M0L8capacityS465);
  _M0L6_2atmpS1476 = 0;
  _M0L7_2abindS469
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS465, _M0L6_2atmpS1476);
  _M0L7_2abindS470 = 0;
  _block_2334
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_2334)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_2334->$0 = _M0L7_2abindS469;
  _block_2334->$1 = 0;
  _block_2334->$2 = _M0L8capacityS465;
  _block_2334->$3 = _M0L7_2abindS467;
  _block_2334->$4 = _M0L7_2abindS468;
  _block_2334->$5 = _M0L7_2abindS470;
  _block_2334->$6 = -1;
  return _block_2334;
}

struct _M0TPB3MapGssE* _M0MPB3Map11new_2einnerGssE(int32_t _M0L8capacityS472) {
  int32_t _M0L8capacityS471;
  int32_t _M0L7_2abindS473;
  int32_t _M0L7_2abindS474;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS1477;
  struct _M0TPB5EntryGssE** _M0L7_2abindS475;
  struct _M0TPB5EntryGssE* _M0L7_2abindS476;
  struct _M0TPB3MapGssE* _block_2335;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS471
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS472);
  _M0L7_2abindS473 = _M0L8capacityS471 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS474 = _M0FPB21calc__grow__threshold(_M0L8capacityS471);
  _M0L6_2atmpS1477 = 0;
  _M0L7_2abindS475
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L8capacityS471, _M0L6_2atmpS1477);
  _M0L7_2abindS476 = 0;
  _block_2335
  = (struct _M0TPB3MapGssE*)moonbit_malloc(sizeof(struct _M0TPB3MapGssE));
  Moonbit_object_header(_block_2335)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGssE, $0) >> 2, 2, 0);
  _block_2335->$0 = _M0L7_2abindS475;
  _block_2335->$1 = 0;
  _block_2335->$2 = _M0L8capacityS471;
  _block_2335->$3 = _M0L7_2abindS473;
  _block_2335->$4 = _M0L7_2abindS474;
  _block_2335->$5 = _M0L7_2abindS476;
  _block_2335->$6 = -1;
  return _block_2335;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS458) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS458 >= 0) {
    int32_t _M0L6_2atmpS1474;
    int32_t _M0L6_2atmpS1473;
    int32_t _M0L6_2atmpS1472;
    int32_t _M0L6_2atmpS1471;
    if (_M0L4selfS458 <= 1) {
      return 1;
    }
    if (_M0L4selfS458 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1474 = _M0L4selfS458 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS1473 = moonbit_clz32(_M0L6_2atmpS1474);
    _M0L6_2atmpS1472 = _M0L6_2atmpS1473 - 1;
    _M0L6_2atmpS1471 = 2147483647 >> (_M0L6_2atmpS1472 & 31);
    return _M0L6_2atmpS1471 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS457) {
  int32_t _M0L6_2atmpS1470;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1470 = _M0L8capacityS457 * 13;
  return _M0L6_2atmpS1470 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS451
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS451 == 0) {
    if (_M0L4selfS451) {
      moonbit_decref(_M0L4selfS451);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS452 =
      _M0L4selfS451;
    return _M0L7_2aSomeS452;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS453
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS453 == 0) {
    if (_M0L4selfS453) {
      moonbit_decref(_M0L4selfS453);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS454 =
      _M0L4selfS453;
    return _M0L7_2aSomeS454;
  }
}

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE* _M0L4selfS455
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS455 == 0) {
    if (_M0L4selfS455) {
      moonbit_decref(_M0L4selfS455);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS456 = _M0L4selfS455;
    return _M0L7_2aSomeS456;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS450
) {
  moonbit_string_t* _M0L6_2atmpS1469;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1469 = _M0L4selfS450;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1469);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS449
) {
  moonbit_string_t* _M0L6_2atmpS1467;
  int32_t _M0L6_2atmpS2083;
  int32_t _M0L6_2atmpS1468;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1466;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS449);
  _M0L6_2atmpS1467 = _M0L4selfS449;
  _M0L6_2atmpS2083 = Moonbit_array_length(_M0L4selfS449);
  moonbit_decref(_M0L4selfS449);
  _M0L6_2atmpS1468 = _M0L6_2atmpS2083;
  _M0L6_2atmpS1466
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1468, _M0L6_2atmpS1467
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1466);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS447
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS446;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1455__l570__* _closure_2336;
  struct _M0TWEOs* _M0L6_2atmpS1454;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS446
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS446)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS446->$0 = 0;
  _closure_2336
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1455__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1455__l570__));
  Moonbit_object_header(_closure_2336)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1455__l570__, $0_0) >> 2, 2, 0);
  _closure_2336->code = &_M0MPC15array9ArrayView4iterGsEC1455l570;
  _closure_2336->$0_0 = _M0L4selfS447.$0;
  _closure_2336->$0_1 = _M0L4selfS447.$1;
  _closure_2336->$0_2 = _M0L4selfS447.$2;
  _closure_2336->$1 = _M0L1iS446;
  _M0L6_2atmpS1454 = (struct _M0TWEOs*)_closure_2336;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1454);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1455l570(
  struct _M0TWEOs* _M0L6_2aenvS1456
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1455__l570__* _M0L14_2acasted__envS1457;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS2088;
  struct _M0TPC13ref3RefGiE* _M0L1iS446;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS2087;
  int32_t _M0L6_2acntS2249;
  struct _M0TPB9ArrayViewGsE _M0L4selfS447;
  int32_t _M0L3valS1458;
  int32_t _M0L6_2atmpS1459;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1457
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1455__l570__*)_M0L6_2aenvS1456;
  _M0L8_2afieldS2088 = _M0L14_2acasted__envS1457->$1;
  _M0L1iS446 = _M0L8_2afieldS2088;
  _M0L8_2afieldS2087
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1457->$0_1,
      _M0L14_2acasted__envS1457->$0_2,
      _M0L14_2acasted__envS1457->$0_0
  };
  _M0L6_2acntS2249 = Moonbit_object_header(_M0L14_2acasted__envS1457)->rc;
  if (_M0L6_2acntS2249 > 1) {
    int32_t _M0L11_2anew__cntS2250 = _M0L6_2acntS2249 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1457)->rc
    = _M0L11_2anew__cntS2250;
    moonbit_incref(_M0L1iS446);
    moonbit_incref(_M0L8_2afieldS2087.$0);
  } else if (_M0L6_2acntS2249 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1457);
  }
  _M0L4selfS447 = _M0L8_2afieldS2087;
  _M0L3valS1458 = _M0L1iS446->$0;
  moonbit_incref(_M0L4selfS447.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1459 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS447);
  if (_M0L3valS1458 < _M0L6_2atmpS1459) {
    moonbit_string_t* _M0L8_2afieldS2086 = _M0L4selfS447.$0;
    moonbit_string_t* _M0L3bufS1462 = _M0L8_2afieldS2086;
    int32_t _M0L8_2afieldS2085 = _M0L4selfS447.$1;
    int32_t _M0L5startS1464 = _M0L8_2afieldS2085;
    int32_t _M0L3valS1465 = _M0L1iS446->$0;
    int32_t _M0L6_2atmpS1463 = _M0L5startS1464 + _M0L3valS1465;
    moonbit_string_t _M0L6_2atmpS2084 =
      (moonbit_string_t)_M0L3bufS1462[_M0L6_2atmpS1463];
    moonbit_string_t _M0L4elemS448;
    int32_t _M0L3valS1461;
    int32_t _M0L6_2atmpS1460;
    moonbit_incref(_M0L6_2atmpS2084);
    moonbit_decref(_M0L3bufS1462);
    _M0L4elemS448 = _M0L6_2atmpS2084;
    _M0L3valS1461 = _M0L1iS446->$0;
    _M0L6_2atmpS1460 = _M0L3valS1461 + 1;
    _M0L1iS446->$0 = _M0L6_2atmpS1460;
    moonbit_decref(_M0L1iS446);
    return _M0L4elemS448;
  } else {
    moonbit_decref(_M0L4selfS447.$0);
    moonbit_decref(_M0L1iS446);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS445
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS445;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS444,
  struct _M0TPB6Logger _M0L6loggerS443
) {
  moonbit_string_t _M0L6_2atmpS1453;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1453 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS444, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS443.$0->$method_0(_M0L6loggerS443.$1, _M0L6_2atmpS1453);
  return 0;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS437,
  moonbit_string_t _M0L5valueS439
) {
  int32_t _M0L3lenS1443;
  moonbit_string_t* _M0L6_2atmpS1445;
  int32_t _M0L6_2atmpS2091;
  int32_t _M0L6_2atmpS1444;
  int32_t _M0L6lengthS438;
  moonbit_string_t* _M0L8_2afieldS2090;
  moonbit_string_t* _M0L3bufS1446;
  moonbit_string_t _M0L6_2aoldS2089;
  int32_t _M0L6_2atmpS1447;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1443 = _M0L4selfS437->$1;
  moonbit_incref(_M0L4selfS437);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1445 = _M0MPC15array5Array6bufferGsE(_M0L4selfS437);
  _M0L6_2atmpS2091 = Moonbit_array_length(_M0L6_2atmpS1445);
  moonbit_decref(_M0L6_2atmpS1445);
  _M0L6_2atmpS1444 = _M0L6_2atmpS2091;
  if (_M0L3lenS1443 == _M0L6_2atmpS1444) {
    moonbit_incref(_M0L4selfS437);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS437);
  }
  _M0L6lengthS438 = _M0L4selfS437->$1;
  _M0L8_2afieldS2090 = _M0L4selfS437->$0;
  _M0L3bufS1446 = _M0L8_2afieldS2090;
  _M0L6_2aoldS2089 = (moonbit_string_t)_M0L3bufS1446[_M0L6lengthS438];
  moonbit_decref(_M0L6_2aoldS2089);
  _M0L3bufS1446[_M0L6lengthS438] = _M0L5valueS439;
  _M0L6_2atmpS1447 = _M0L6lengthS438 + 1;
  _M0L4selfS437->$1 = _M0L6_2atmpS1447;
  moonbit_decref(_M0L4selfS437);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS440,
  struct _M0TUsiE* _M0L5valueS442
) {
  int32_t _M0L3lenS1448;
  struct _M0TUsiE** _M0L6_2atmpS1450;
  int32_t _M0L6_2atmpS2094;
  int32_t _M0L6_2atmpS1449;
  int32_t _M0L6lengthS441;
  struct _M0TUsiE** _M0L8_2afieldS2093;
  struct _M0TUsiE** _M0L3bufS1451;
  struct _M0TUsiE* _M0L6_2aoldS2092;
  int32_t _M0L6_2atmpS1452;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1448 = _M0L4selfS440->$1;
  moonbit_incref(_M0L4selfS440);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1450 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS440);
  _M0L6_2atmpS2094 = Moonbit_array_length(_M0L6_2atmpS1450);
  moonbit_decref(_M0L6_2atmpS1450);
  _M0L6_2atmpS1449 = _M0L6_2atmpS2094;
  if (_M0L3lenS1448 == _M0L6_2atmpS1449) {
    moonbit_incref(_M0L4selfS440);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS440);
  }
  _M0L6lengthS441 = _M0L4selfS440->$1;
  _M0L8_2afieldS2093 = _M0L4selfS440->$0;
  _M0L3bufS1451 = _M0L8_2afieldS2093;
  _M0L6_2aoldS2092 = (struct _M0TUsiE*)_M0L3bufS1451[_M0L6lengthS441];
  if (_M0L6_2aoldS2092) {
    moonbit_decref(_M0L6_2aoldS2092);
  }
  _M0L3bufS1451[_M0L6lengthS441] = _M0L5valueS442;
  _M0L6_2atmpS1452 = _M0L6lengthS441 + 1;
  _M0L4selfS440->$1 = _M0L6_2atmpS1452;
  moonbit_decref(_M0L4selfS440);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS432) {
  int32_t _M0L8old__capS431;
  int32_t _M0L8new__capS433;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS431 = _M0L4selfS432->$1;
  if (_M0L8old__capS431 == 0) {
    _M0L8new__capS433 = 8;
  } else {
    _M0L8new__capS433 = _M0L8old__capS431 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS432, _M0L8new__capS433);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS435
) {
  int32_t _M0L8old__capS434;
  int32_t _M0L8new__capS436;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS434 = _M0L4selfS435->$1;
  if (_M0L8old__capS434 == 0) {
    _M0L8new__capS436 = 8;
  } else {
    _M0L8new__capS436 = _M0L8old__capS434 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS435, _M0L8new__capS436);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS422,
  int32_t _M0L13new__capacityS420
) {
  moonbit_string_t* _M0L8new__bufS419;
  moonbit_string_t* _M0L8_2afieldS2096;
  moonbit_string_t* _M0L8old__bufS421;
  int32_t _M0L8old__capS423;
  int32_t _M0L9copy__lenS424;
  moonbit_string_t* _M0L6_2aoldS2095;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS419
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS420, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS2096 = _M0L4selfS422->$0;
  _M0L8old__bufS421 = _M0L8_2afieldS2096;
  _M0L8old__capS423 = Moonbit_array_length(_M0L8old__bufS421);
  if (_M0L8old__capS423 < _M0L13new__capacityS420) {
    _M0L9copy__lenS424 = _M0L8old__capS423;
  } else {
    _M0L9copy__lenS424 = _M0L13new__capacityS420;
  }
  moonbit_incref(_M0L8old__bufS421);
  moonbit_incref(_M0L8new__bufS419);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS419, 0, _M0L8old__bufS421, 0, _M0L9copy__lenS424);
  _M0L6_2aoldS2095 = _M0L4selfS422->$0;
  moonbit_decref(_M0L6_2aoldS2095);
  _M0L4selfS422->$0 = _M0L8new__bufS419;
  moonbit_decref(_M0L4selfS422);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS428,
  int32_t _M0L13new__capacityS426
) {
  struct _M0TUsiE** _M0L8new__bufS425;
  struct _M0TUsiE** _M0L8_2afieldS2098;
  struct _M0TUsiE** _M0L8old__bufS427;
  int32_t _M0L8old__capS429;
  int32_t _M0L9copy__lenS430;
  struct _M0TUsiE** _M0L6_2aoldS2097;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS425
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS426, 0);
  _M0L8_2afieldS2098 = _M0L4selfS428->$0;
  _M0L8old__bufS427 = _M0L8_2afieldS2098;
  _M0L8old__capS429 = Moonbit_array_length(_M0L8old__bufS427);
  if (_M0L8old__capS429 < _M0L13new__capacityS426) {
    _M0L9copy__lenS430 = _M0L8old__capS429;
  } else {
    _M0L9copy__lenS430 = _M0L13new__capacityS426;
  }
  moonbit_incref(_M0L8old__bufS427);
  moonbit_incref(_M0L8new__bufS425);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS425, 0, _M0L8old__bufS427, 0, _M0L9copy__lenS430);
  _M0L6_2aoldS2097 = _M0L4selfS428->$0;
  moonbit_decref(_M0L6_2aoldS2097);
  _M0L4selfS428->$0 = _M0L8new__bufS425;
  moonbit_decref(_M0L4selfS428);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS418
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS418 == 0) {
    moonbit_string_t* _M0L6_2atmpS1441 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_2337 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2337)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2337->$0 = _M0L6_2atmpS1441;
    _block_2337->$1 = 0;
    return _block_2337;
  } else {
    moonbit_string_t* _M0L6_2atmpS1442 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS418, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_2338 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_2338)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_2338->$0 = _M0L6_2atmpS1442;
    _block_2338->$1 = 0;
    return _block_2338;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS416,
  struct _M0TPC16string10StringView _M0L3strS417
) {
  int32_t _M0L3lenS1429;
  int32_t _M0L6_2atmpS1431;
  int32_t _M0L6_2atmpS1430;
  int32_t _M0L6_2atmpS1428;
  moonbit_bytes_t _M0L8_2afieldS2099;
  moonbit_bytes_t _M0L4dataS1432;
  int32_t _M0L3lenS1433;
  moonbit_string_t _M0L6_2atmpS1434;
  int32_t _M0L6_2atmpS1435;
  int32_t _M0L6_2atmpS1436;
  int32_t _M0L3lenS1438;
  int32_t _M0L6_2atmpS1440;
  int32_t _M0L6_2atmpS1439;
  int32_t _M0L6_2atmpS1437;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1429 = _M0L4selfS416->$1;
  moonbit_incref(_M0L3strS417.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1431 = _M0MPC16string10StringView6length(_M0L3strS417);
  _M0L6_2atmpS1430 = _M0L6_2atmpS1431 * 2;
  _M0L6_2atmpS1428 = _M0L3lenS1429 + _M0L6_2atmpS1430;
  moonbit_incref(_M0L4selfS416);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS416, _M0L6_2atmpS1428);
  _M0L8_2afieldS2099 = _M0L4selfS416->$0;
  _M0L4dataS1432 = _M0L8_2afieldS2099;
  _M0L3lenS1433 = _M0L4selfS416->$1;
  moonbit_incref(_M0L4dataS1432);
  moonbit_incref(_M0L3strS417.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1434 = _M0MPC16string10StringView4data(_M0L3strS417);
  moonbit_incref(_M0L3strS417.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1435 = _M0MPC16string10StringView13start__offset(_M0L3strS417);
  moonbit_incref(_M0L3strS417.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1436 = _M0MPC16string10StringView6length(_M0L3strS417);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1432, _M0L3lenS1433, _M0L6_2atmpS1434, _M0L6_2atmpS1435, _M0L6_2atmpS1436);
  _M0L3lenS1438 = _M0L4selfS416->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1440 = _M0MPC16string10StringView6length(_M0L3strS417);
  _M0L6_2atmpS1439 = _M0L6_2atmpS1440 * 2;
  _M0L6_2atmpS1437 = _M0L3lenS1438 + _M0L6_2atmpS1439;
  _M0L4selfS416->$1 = _M0L6_2atmpS1437;
  moonbit_decref(_M0L4selfS416);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS408,
  int32_t _M0L3lenS411,
  int32_t _M0L13start__offsetS415,
  int64_t _M0L11end__offsetS406
) {
  int32_t _M0L11end__offsetS405;
  int32_t _M0L5indexS409;
  int32_t _M0L5countS410;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS406 == 4294967296ll) {
    _M0L11end__offsetS405 = Moonbit_array_length(_M0L4selfS408);
  } else {
    int64_t _M0L7_2aSomeS407 = _M0L11end__offsetS406;
    _M0L11end__offsetS405 = (int32_t)_M0L7_2aSomeS407;
  }
  _M0L5indexS409 = _M0L13start__offsetS415;
  _M0L5countS410 = 0;
  while (1) {
    int32_t _if__result_2340;
    if (_M0L5indexS409 < _M0L11end__offsetS405) {
      _if__result_2340 = _M0L5countS410 < _M0L3lenS411;
    } else {
      _if__result_2340 = 0;
    }
    if (_if__result_2340) {
      int32_t _M0L2c1S412 = _M0L4selfS408[_M0L5indexS409];
      int32_t _if__result_2341;
      int32_t _M0L6_2atmpS1426;
      int32_t _M0L6_2atmpS1427;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S412)) {
        int32_t _M0L6_2atmpS1422 = _M0L5indexS409 + 1;
        _if__result_2341 = _M0L6_2atmpS1422 < _M0L11end__offsetS405;
      } else {
        _if__result_2341 = 0;
      }
      if (_if__result_2341) {
        int32_t _M0L6_2atmpS1425 = _M0L5indexS409 + 1;
        int32_t _M0L2c2S413 = _M0L4selfS408[_M0L6_2atmpS1425];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S413)) {
          int32_t _M0L6_2atmpS1423 = _M0L5indexS409 + 2;
          int32_t _M0L6_2atmpS1424 = _M0L5countS410 + 1;
          _M0L5indexS409 = _M0L6_2atmpS1423;
          _M0L5countS410 = _M0L6_2atmpS1424;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_28.data, (moonbit_string_t)moonbit_string_literal_29.data);
        }
      }
      _M0L6_2atmpS1426 = _M0L5indexS409 + 1;
      _M0L6_2atmpS1427 = _M0L5countS410 + 1;
      _M0L5indexS409 = _M0L6_2atmpS1426;
      _M0L5countS410 = _M0L6_2atmpS1427;
      continue;
    } else {
      moonbit_decref(_M0L4selfS408);
      return _M0L5countS410 >= _M0L3lenS411;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS401
) {
  int32_t _M0L3endS1414;
  int32_t _M0L8_2afieldS2100;
  int32_t _M0L5startS1415;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1414 = _M0L4selfS401.$2;
  _M0L8_2afieldS2100 = _M0L4selfS401.$1;
  moonbit_decref(_M0L4selfS401.$0);
  _M0L5startS1415 = _M0L8_2afieldS2100;
  return _M0L3endS1414 - _M0L5startS1415;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS402
) {
  int32_t _M0L3endS1416;
  int32_t _M0L8_2afieldS2101;
  int32_t _M0L5startS1417;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1416 = _M0L4selfS402.$2;
  _M0L8_2afieldS2101 = _M0L4selfS402.$1;
  moonbit_decref(_M0L4selfS402.$0);
  _M0L5startS1417 = _M0L8_2afieldS2101;
  return _M0L3endS1416 - _M0L5startS1417;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS403
) {
  int32_t _M0L3endS1418;
  int32_t _M0L8_2afieldS2102;
  int32_t _M0L5startS1419;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1418 = _M0L4selfS403.$2;
  _M0L8_2afieldS2102 = _M0L4selfS403.$1;
  moonbit_decref(_M0L4selfS403.$0);
  _M0L5startS1419 = _M0L8_2afieldS2102;
  return _M0L3endS1418 - _M0L5startS1419;
}

int32_t _M0MPC15array9ArrayView6lengthGUssEE(
  struct _M0TPB9ArrayViewGUssEE _M0L4selfS404
) {
  int32_t _M0L3endS1420;
  int32_t _M0L8_2afieldS2103;
  int32_t _M0L5startS1421;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1420 = _M0L4selfS404.$2;
  _M0L8_2afieldS2103 = _M0L4selfS404.$1;
  moonbit_decref(_M0L4selfS404.$0);
  _M0L5startS1421 = _M0L8_2afieldS2103;
  return _M0L3endS1420 - _M0L5startS1421;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS399,
  int64_t _M0L19start__offset_2eoptS397,
  int64_t _M0L11end__offsetS400
) {
  int32_t _M0L13start__offsetS396;
  if (_M0L19start__offset_2eoptS397 == 4294967296ll) {
    _M0L13start__offsetS396 = 0;
  } else {
    int64_t _M0L7_2aSomeS398 = _M0L19start__offset_2eoptS397;
    _M0L13start__offsetS396 = (int32_t)_M0L7_2aSomeS398;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS399, _M0L13start__offsetS396, _M0L11end__offsetS400);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS394,
  int32_t _M0L13start__offsetS395,
  int64_t _M0L11end__offsetS392
) {
  int32_t _M0L11end__offsetS391;
  int32_t _if__result_2342;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS392 == 4294967296ll) {
    _M0L11end__offsetS391 = Moonbit_array_length(_M0L4selfS394);
  } else {
    int64_t _M0L7_2aSomeS393 = _M0L11end__offsetS392;
    _M0L11end__offsetS391 = (int32_t)_M0L7_2aSomeS393;
  }
  if (_M0L13start__offsetS395 >= 0) {
    if (_M0L13start__offsetS395 <= _M0L11end__offsetS391) {
      int32_t _M0L6_2atmpS1413 = Moonbit_array_length(_M0L4selfS394);
      _if__result_2342 = _M0L11end__offsetS391 <= _M0L6_2atmpS1413;
    } else {
      _if__result_2342 = 0;
    }
  } else {
    _if__result_2342 = 0;
  }
  if (_if__result_2342) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS395,
                                                 _M0L11end__offsetS391,
                                                 _M0L4selfS394};
  } else {
    moonbit_decref(_M0L4selfS394);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_30.data, (moonbit_string_t)moonbit_string_literal_31.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS390
) {
  moonbit_string_t _M0L8_2afieldS2105;
  moonbit_string_t _M0L3strS1410;
  int32_t _M0L5startS1411;
  int32_t _M0L8_2afieldS2104;
  int32_t _M0L3endS1412;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2105 = _M0L4selfS390.$0;
  _M0L3strS1410 = _M0L8_2afieldS2105;
  _M0L5startS1411 = _M0L4selfS390.$1;
  _M0L8_2afieldS2104 = _M0L4selfS390.$2;
  _M0L3endS1412 = _M0L8_2afieldS2104;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1410, _M0L5startS1411, _M0L3endS1412);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS388,
  struct _M0TPB6Logger _M0L6loggerS389
) {
  moonbit_string_t _M0L8_2afieldS2107;
  moonbit_string_t _M0L3strS1407;
  int32_t _M0L5startS1408;
  int32_t _M0L8_2afieldS2106;
  int32_t _M0L3endS1409;
  moonbit_string_t _M0L6substrS387;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2107 = _M0L4selfS388.$0;
  _M0L3strS1407 = _M0L8_2afieldS2107;
  _M0L5startS1408 = _M0L4selfS388.$1;
  _M0L8_2afieldS2106 = _M0L4selfS388.$2;
  _M0L3endS1409 = _M0L8_2afieldS2106;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS387
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1407, _M0L5startS1408, _M0L3endS1409);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS387, _M0L6loggerS389);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS379,
  struct _M0TPB6Logger _M0L6loggerS377
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS378;
  int32_t _M0L3lenS380;
  int32_t _M0L1iS381;
  int32_t _M0L3segS382;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS377.$1) {
    moonbit_incref(_M0L6loggerS377.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS377.$0->$method_3(_M0L6loggerS377.$1, 34);
  moonbit_incref(_M0L4selfS379);
  if (_M0L6loggerS377.$1) {
    moonbit_incref(_M0L6loggerS377.$1);
  }
  _M0L6_2aenvS378
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS378)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS378->$0 = _M0L4selfS379;
  _M0L6_2aenvS378->$1_0 = _M0L6loggerS377.$0;
  _M0L6_2aenvS378->$1_1 = _M0L6loggerS377.$1;
  _M0L3lenS380 = Moonbit_array_length(_M0L4selfS379);
  _M0L1iS381 = 0;
  _M0L3segS382 = 0;
  _2afor_383:;
  while (1) {
    int32_t _M0L4codeS384;
    int32_t _M0L1cS386;
    int32_t _M0L6_2atmpS1391;
    int32_t _M0L6_2atmpS1392;
    int32_t _M0L6_2atmpS1393;
    int32_t _tmp_2346;
    int32_t _tmp_2347;
    if (_M0L1iS381 >= _M0L3lenS380) {
      moonbit_decref(_M0L4selfS379);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
      break;
    }
    _M0L4codeS384 = _M0L4selfS379[_M0L1iS381];
    switch (_M0L4codeS384) {
      case 34: {
        _M0L1cS386 = _M0L4codeS384;
        goto join_385;
        break;
      }
      
      case 92: {
        _M0L1cS386 = _M0L4codeS384;
        goto join_385;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1394;
        int32_t _M0L6_2atmpS1395;
        moonbit_incref(_M0L6_2aenvS378);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
        if (_M0L6loggerS377.$1) {
          moonbit_incref(_M0L6loggerS377.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, (moonbit_string_t)moonbit_string_literal_32.data);
        _M0L6_2atmpS1394 = _M0L1iS381 + 1;
        _M0L6_2atmpS1395 = _M0L1iS381 + 1;
        _M0L1iS381 = _M0L6_2atmpS1394;
        _M0L3segS382 = _M0L6_2atmpS1395;
        goto _2afor_383;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1396;
        int32_t _M0L6_2atmpS1397;
        moonbit_incref(_M0L6_2aenvS378);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
        if (_M0L6loggerS377.$1) {
          moonbit_incref(_M0L6loggerS377.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, (moonbit_string_t)moonbit_string_literal_33.data);
        _M0L6_2atmpS1396 = _M0L1iS381 + 1;
        _M0L6_2atmpS1397 = _M0L1iS381 + 1;
        _M0L1iS381 = _M0L6_2atmpS1396;
        _M0L3segS382 = _M0L6_2atmpS1397;
        goto _2afor_383;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1398;
        int32_t _M0L6_2atmpS1399;
        moonbit_incref(_M0L6_2aenvS378);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
        if (_M0L6loggerS377.$1) {
          moonbit_incref(_M0L6loggerS377.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, (moonbit_string_t)moonbit_string_literal_34.data);
        _M0L6_2atmpS1398 = _M0L1iS381 + 1;
        _M0L6_2atmpS1399 = _M0L1iS381 + 1;
        _M0L1iS381 = _M0L6_2atmpS1398;
        _M0L3segS382 = _M0L6_2atmpS1399;
        goto _2afor_383;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1400;
        int32_t _M0L6_2atmpS1401;
        moonbit_incref(_M0L6_2aenvS378);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
        if (_M0L6loggerS377.$1) {
          moonbit_incref(_M0L6loggerS377.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, (moonbit_string_t)moonbit_string_literal_35.data);
        _M0L6_2atmpS1400 = _M0L1iS381 + 1;
        _M0L6_2atmpS1401 = _M0L1iS381 + 1;
        _M0L1iS381 = _M0L6_2atmpS1400;
        _M0L3segS382 = _M0L6_2atmpS1401;
        goto _2afor_383;
        break;
      }
      default: {
        if (_M0L4codeS384 < 32) {
          int32_t _M0L6_2atmpS1403;
          moonbit_string_t _M0L6_2atmpS1402;
          int32_t _M0L6_2atmpS1404;
          int32_t _M0L6_2atmpS1405;
          moonbit_incref(_M0L6_2aenvS378);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
          if (_M0L6loggerS377.$1) {
            moonbit_incref(_M0L6loggerS377.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, (moonbit_string_t)moonbit_string_literal_36.data);
          _M0L6_2atmpS1403 = _M0L4codeS384 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1402 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1403);
          if (_M0L6loggerS377.$1) {
            moonbit_incref(_M0L6loggerS377.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS377.$0->$method_0(_M0L6loggerS377.$1, _M0L6_2atmpS1402);
          if (_M0L6loggerS377.$1) {
            moonbit_incref(_M0L6loggerS377.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS377.$0->$method_3(_M0L6loggerS377.$1, 125);
          _M0L6_2atmpS1404 = _M0L1iS381 + 1;
          _M0L6_2atmpS1405 = _M0L1iS381 + 1;
          _M0L1iS381 = _M0L6_2atmpS1404;
          _M0L3segS382 = _M0L6_2atmpS1405;
          goto _2afor_383;
        } else {
          int32_t _M0L6_2atmpS1406 = _M0L1iS381 + 1;
          int32_t _tmp_2345 = _M0L3segS382;
          _M0L1iS381 = _M0L6_2atmpS1406;
          _M0L3segS382 = _tmp_2345;
          goto _2afor_383;
        }
        break;
      }
    }
    goto joinlet_2344;
    join_385:;
    moonbit_incref(_M0L6_2aenvS378);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS378, _M0L3segS382, _M0L1iS381);
    if (_M0L6loggerS377.$1) {
      moonbit_incref(_M0L6loggerS377.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS377.$0->$method_3(_M0L6loggerS377.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1391 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS386);
    if (_M0L6loggerS377.$1) {
      moonbit_incref(_M0L6loggerS377.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS377.$0->$method_3(_M0L6loggerS377.$1, _M0L6_2atmpS1391);
    _M0L6_2atmpS1392 = _M0L1iS381 + 1;
    _M0L6_2atmpS1393 = _M0L1iS381 + 1;
    _M0L1iS381 = _M0L6_2atmpS1392;
    _M0L3segS382 = _M0L6_2atmpS1393;
    continue;
    joinlet_2344:;
    _tmp_2346 = _M0L1iS381;
    _tmp_2347 = _M0L3segS382;
    _M0L1iS381 = _tmp_2346;
    _M0L3segS382 = _tmp_2347;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS377.$0->$method_3(_M0L6loggerS377.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS373,
  int32_t _M0L3segS376,
  int32_t _M0L1iS375
) {
  struct _M0TPB6Logger _M0L8_2afieldS2109;
  struct _M0TPB6Logger _M0L6loggerS372;
  moonbit_string_t _M0L8_2afieldS2108;
  int32_t _M0L6_2acntS2251;
  moonbit_string_t _M0L4selfS374;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS2109
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS373->$1_0, _M0L6_2aenvS373->$1_1
  };
  _M0L6loggerS372 = _M0L8_2afieldS2109;
  _M0L8_2afieldS2108 = _M0L6_2aenvS373->$0;
  _M0L6_2acntS2251 = Moonbit_object_header(_M0L6_2aenvS373)->rc;
  if (_M0L6_2acntS2251 > 1) {
    int32_t _M0L11_2anew__cntS2252 = _M0L6_2acntS2251 - 1;
    Moonbit_object_header(_M0L6_2aenvS373)->rc = _M0L11_2anew__cntS2252;
    if (_M0L6loggerS372.$1) {
      moonbit_incref(_M0L6loggerS372.$1);
    }
    moonbit_incref(_M0L8_2afieldS2108);
  } else if (_M0L6_2acntS2251 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS373);
  }
  _M0L4selfS374 = _M0L8_2afieldS2108;
  if (_M0L1iS375 > _M0L3segS376) {
    int32_t _M0L6_2atmpS1390 = _M0L1iS375 - _M0L3segS376;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS372.$0->$method_1(_M0L6loggerS372.$1, _M0L4selfS374, _M0L3segS376, _M0L6_2atmpS1390);
  } else {
    moonbit_decref(_M0L4selfS374);
    if (_M0L6loggerS372.$1) {
      moonbit_decref(_M0L6loggerS372.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS371) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS370;
  int32_t _M0L6_2atmpS1387;
  int32_t _M0L6_2atmpS1386;
  int32_t _M0L6_2atmpS1389;
  int32_t _M0L6_2atmpS1388;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1385;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS370 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1387 = _M0IPC14byte4BytePB3Div3div(_M0L1bS371, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1386
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1387);
  moonbit_incref(_M0L7_2aselfS370);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS370, _M0L6_2atmpS1386);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1389 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS371, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1388
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1389);
  moonbit_incref(_M0L7_2aselfS370);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS370, _M0L6_2atmpS1388);
  _M0L6_2atmpS1385 = _M0L7_2aselfS370;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1385);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS369) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS369 < 10) {
    int32_t _M0L6_2atmpS1382;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1382 = _M0IPC14byte4BytePB3Add3add(_M0L1iS369, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1382);
  } else {
    int32_t _M0L6_2atmpS1384;
    int32_t _M0L6_2atmpS1383;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1384 = _M0IPC14byte4BytePB3Add3add(_M0L1iS369, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1383 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1384, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1383);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS367,
  int32_t _M0L4thatS368
) {
  int32_t _M0L6_2atmpS1380;
  int32_t _M0L6_2atmpS1381;
  int32_t _M0L6_2atmpS1379;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1380 = (int32_t)_M0L4selfS367;
  _M0L6_2atmpS1381 = (int32_t)_M0L4thatS368;
  _M0L6_2atmpS1379 = _M0L6_2atmpS1380 - _M0L6_2atmpS1381;
  return _M0L6_2atmpS1379 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS365,
  int32_t _M0L4thatS366
) {
  int32_t _M0L6_2atmpS1377;
  int32_t _M0L6_2atmpS1378;
  int32_t _M0L6_2atmpS1376;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1377 = (int32_t)_M0L4selfS365;
  _M0L6_2atmpS1378 = (int32_t)_M0L4thatS366;
  _M0L6_2atmpS1376 = _M0L6_2atmpS1377 % _M0L6_2atmpS1378;
  return _M0L6_2atmpS1376 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS363,
  int32_t _M0L4thatS364
) {
  int32_t _M0L6_2atmpS1374;
  int32_t _M0L6_2atmpS1375;
  int32_t _M0L6_2atmpS1373;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1374 = (int32_t)_M0L4selfS363;
  _M0L6_2atmpS1375 = (int32_t)_M0L4thatS364;
  _M0L6_2atmpS1373 = _M0L6_2atmpS1374 / _M0L6_2atmpS1375;
  return _M0L6_2atmpS1373 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS361,
  int32_t _M0L4thatS362
) {
  int32_t _M0L6_2atmpS1371;
  int32_t _M0L6_2atmpS1372;
  int32_t _M0L6_2atmpS1370;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1371 = (int32_t)_M0L4selfS361;
  _M0L6_2atmpS1372 = (int32_t)_M0L4thatS362;
  _M0L6_2atmpS1370 = _M0L6_2atmpS1371 + _M0L6_2atmpS1372;
  return _M0L6_2atmpS1370 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS358,
  int32_t _M0L5startS356,
  int32_t _M0L3endS357
) {
  int32_t _if__result_2348;
  int32_t _M0L3lenS359;
  int32_t _M0L6_2atmpS1368;
  int32_t _M0L6_2atmpS1369;
  moonbit_bytes_t _M0L5bytesS360;
  moonbit_bytes_t _M0L6_2atmpS1367;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS356 == 0) {
    int32_t _M0L6_2atmpS1366 = Moonbit_array_length(_M0L3strS358);
    _if__result_2348 = _M0L3endS357 == _M0L6_2atmpS1366;
  } else {
    _if__result_2348 = 0;
  }
  if (_if__result_2348) {
    return _M0L3strS358;
  }
  _M0L3lenS359 = _M0L3endS357 - _M0L5startS356;
  _M0L6_2atmpS1368 = _M0L3lenS359 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1369 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS360
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1368, _M0L6_2atmpS1369);
  moonbit_incref(_M0L5bytesS360);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS360, 0, _M0L3strS358, _M0L5startS356, _M0L3lenS359);
  _M0L6_2atmpS1367 = _M0L5bytesS360;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1367, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS355) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS355;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS339,
  int32_t _M0L5radixS338
) {
  int32_t _if__result_2349;
  int32_t _M0L12is__negativeS340;
  uint32_t _M0L3numS341;
  uint16_t* _M0L6bufferS342;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS338 < 2) {
    _if__result_2349 = 1;
  } else {
    _if__result_2349 = _M0L5radixS338 > 36;
  }
  if (_if__result_2349) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_37.data, (moonbit_string_t)moonbit_string_literal_38.data);
  }
  if (_M0L4selfS339 == 0) {
    return (moonbit_string_t)moonbit_string_literal_39.data;
  }
  _M0L12is__negativeS340 = _M0L4selfS339 < 0;
  if (_M0L12is__negativeS340) {
    int32_t _M0L6_2atmpS1365 = -_M0L4selfS339;
    _M0L3numS341 = *(uint32_t*)&_M0L6_2atmpS1365;
  } else {
    _M0L3numS341 = *(uint32_t*)&_M0L4selfS339;
  }
  switch (_M0L5radixS338) {
    case 10: {
      int32_t _M0L10digit__lenS343;
      int32_t _M0L6_2atmpS1362;
      int32_t _M0L10total__lenS344;
      uint16_t* _M0L6bufferS345;
      int32_t _M0L12digit__startS346;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS343 = _M0FPB12dec__count32(_M0L3numS341);
      if (_M0L12is__negativeS340) {
        _M0L6_2atmpS1362 = 1;
      } else {
        _M0L6_2atmpS1362 = 0;
      }
      _M0L10total__lenS344 = _M0L10digit__lenS343 + _M0L6_2atmpS1362;
      _M0L6bufferS345
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS344, 0);
      if (_M0L12is__negativeS340) {
        _M0L12digit__startS346 = 1;
      } else {
        _M0L12digit__startS346 = 0;
      }
      moonbit_incref(_M0L6bufferS345);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS345, _M0L3numS341, _M0L12digit__startS346, _M0L10total__lenS344);
      _M0L6bufferS342 = _M0L6bufferS345;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS347;
      int32_t _M0L6_2atmpS1363;
      int32_t _M0L10total__lenS348;
      uint16_t* _M0L6bufferS349;
      int32_t _M0L12digit__startS350;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS347 = _M0FPB12hex__count32(_M0L3numS341);
      if (_M0L12is__negativeS340) {
        _M0L6_2atmpS1363 = 1;
      } else {
        _M0L6_2atmpS1363 = 0;
      }
      _M0L10total__lenS348 = _M0L10digit__lenS347 + _M0L6_2atmpS1363;
      _M0L6bufferS349
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS348, 0);
      if (_M0L12is__negativeS340) {
        _M0L12digit__startS350 = 1;
      } else {
        _M0L12digit__startS350 = 0;
      }
      moonbit_incref(_M0L6bufferS349);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS349, _M0L3numS341, _M0L12digit__startS350, _M0L10total__lenS348);
      _M0L6bufferS342 = _M0L6bufferS349;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS351;
      int32_t _M0L6_2atmpS1364;
      int32_t _M0L10total__lenS352;
      uint16_t* _M0L6bufferS353;
      int32_t _M0L12digit__startS354;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS351
      = _M0FPB14radix__count32(_M0L3numS341, _M0L5radixS338);
      if (_M0L12is__negativeS340) {
        _M0L6_2atmpS1364 = 1;
      } else {
        _M0L6_2atmpS1364 = 0;
      }
      _M0L10total__lenS352 = _M0L10digit__lenS351 + _M0L6_2atmpS1364;
      _M0L6bufferS353
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS352, 0);
      if (_M0L12is__negativeS340) {
        _M0L12digit__startS354 = 1;
      } else {
        _M0L12digit__startS354 = 0;
      }
      moonbit_incref(_M0L6bufferS353);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS353, _M0L3numS341, _M0L12digit__startS354, _M0L10total__lenS352, _M0L5radixS338);
      _M0L6bufferS342 = _M0L6bufferS353;
      break;
    }
  }
  if (_M0L12is__negativeS340) {
    _M0L6bufferS342[0] = 45;
  }
  return _M0L6bufferS342;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS332,
  int32_t _M0L5radixS335
) {
  uint32_t _M0Lm3numS333;
  uint32_t _M0L4baseS334;
  int32_t _M0Lm5countS336;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS332 == 0u) {
    return 1;
  }
  _M0Lm3numS333 = _M0L5valueS332;
  _M0L4baseS334 = *(uint32_t*)&_M0L5radixS335;
  _M0Lm5countS336 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1359 = _M0Lm3numS333;
    if (_M0L6_2atmpS1359 > 0u) {
      int32_t _M0L6_2atmpS1360 = _M0Lm5countS336;
      uint32_t _M0L6_2atmpS1361;
      _M0Lm5countS336 = _M0L6_2atmpS1360 + 1;
      _M0L6_2atmpS1361 = _M0Lm3numS333;
      _M0Lm3numS333 = _M0L6_2atmpS1361 / _M0L4baseS334;
      continue;
    }
    break;
  }
  return _M0Lm5countS336;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS330) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS330 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS331;
    int32_t _M0L6_2atmpS1358;
    int32_t _M0L6_2atmpS1357;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS331 = moonbit_clz32(_M0L5valueS330);
    _M0L6_2atmpS1358 = 31 - _M0L14leading__zerosS331;
    _M0L6_2atmpS1357 = _M0L6_2atmpS1358 / 4;
    return _M0L6_2atmpS1357 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS329) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS329 >= 100000u) {
    if (_M0L5valueS329 >= 10000000u) {
      if (_M0L5valueS329 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS329 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS329 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS329 >= 1000u) {
    if (_M0L5valueS329 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS329 >= 100u) {
    return 3;
  } else if (_M0L5valueS329 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS319,
  uint32_t _M0L3numS307,
  int32_t _M0L12digit__startS310,
  int32_t _M0L10total__lenS309
) {
  uint32_t _M0Lm3numS306;
  int32_t _M0Lm6offsetS308;
  uint32_t _M0L6_2atmpS1356;
  int32_t _M0Lm9remainingS321;
  int32_t _M0L6_2atmpS1337;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS306 = _M0L3numS307;
  _M0Lm6offsetS308 = _M0L10total__lenS309 - _M0L12digit__startS310;
  while (1) {
    uint32_t _M0L6_2atmpS1300 = _M0Lm3numS306;
    if (_M0L6_2atmpS1300 >= 10000u) {
      uint32_t _M0L6_2atmpS1323 = _M0Lm3numS306;
      uint32_t _M0L1tS311 = _M0L6_2atmpS1323 / 10000u;
      uint32_t _M0L6_2atmpS1322 = _M0Lm3numS306;
      uint32_t _M0L6_2atmpS1321 = _M0L6_2atmpS1322 % 10000u;
      int32_t _M0L1rS312 = *(int32_t*)&_M0L6_2atmpS1321;
      int32_t _M0L2d1S313;
      int32_t _M0L2d2S314;
      int32_t _M0L6_2atmpS1301;
      int32_t _M0L6_2atmpS1320;
      int32_t _M0L6_2atmpS1319;
      int32_t _M0L6d1__hiS315;
      int32_t _M0L6_2atmpS1318;
      int32_t _M0L6_2atmpS1317;
      int32_t _M0L6d1__loS316;
      int32_t _M0L6_2atmpS1316;
      int32_t _M0L6_2atmpS1315;
      int32_t _M0L6d2__hiS317;
      int32_t _M0L6_2atmpS1314;
      int32_t _M0L6_2atmpS1313;
      int32_t _M0L6d2__loS318;
      int32_t _M0L6_2atmpS1303;
      int32_t _M0L6_2atmpS1302;
      int32_t _M0L6_2atmpS1306;
      int32_t _M0L6_2atmpS1305;
      int32_t _M0L6_2atmpS1304;
      int32_t _M0L6_2atmpS1309;
      int32_t _M0L6_2atmpS1308;
      int32_t _M0L6_2atmpS1307;
      int32_t _M0L6_2atmpS1312;
      int32_t _M0L6_2atmpS1311;
      int32_t _M0L6_2atmpS1310;
      _M0Lm3numS306 = _M0L1tS311;
      _M0L2d1S313 = _M0L1rS312 / 100;
      _M0L2d2S314 = _M0L1rS312 % 100;
      _M0L6_2atmpS1301 = _M0Lm6offsetS308;
      _M0Lm6offsetS308 = _M0L6_2atmpS1301 - 4;
      _M0L6_2atmpS1320 = _M0L2d1S313 / 10;
      _M0L6_2atmpS1319 = 48 + _M0L6_2atmpS1320;
      _M0L6d1__hiS315 = (uint16_t)_M0L6_2atmpS1319;
      _M0L6_2atmpS1318 = _M0L2d1S313 % 10;
      _M0L6_2atmpS1317 = 48 + _M0L6_2atmpS1318;
      _M0L6d1__loS316 = (uint16_t)_M0L6_2atmpS1317;
      _M0L6_2atmpS1316 = _M0L2d2S314 / 10;
      _M0L6_2atmpS1315 = 48 + _M0L6_2atmpS1316;
      _M0L6d2__hiS317 = (uint16_t)_M0L6_2atmpS1315;
      _M0L6_2atmpS1314 = _M0L2d2S314 % 10;
      _M0L6_2atmpS1313 = 48 + _M0L6_2atmpS1314;
      _M0L6d2__loS318 = (uint16_t)_M0L6_2atmpS1313;
      _M0L6_2atmpS1303 = _M0Lm6offsetS308;
      _M0L6_2atmpS1302 = _M0L12digit__startS310 + _M0L6_2atmpS1303;
      _M0L6bufferS319[_M0L6_2atmpS1302] = _M0L6d1__hiS315;
      _M0L6_2atmpS1306 = _M0Lm6offsetS308;
      _M0L6_2atmpS1305 = _M0L12digit__startS310 + _M0L6_2atmpS1306;
      _M0L6_2atmpS1304 = _M0L6_2atmpS1305 + 1;
      _M0L6bufferS319[_M0L6_2atmpS1304] = _M0L6d1__loS316;
      _M0L6_2atmpS1309 = _M0Lm6offsetS308;
      _M0L6_2atmpS1308 = _M0L12digit__startS310 + _M0L6_2atmpS1309;
      _M0L6_2atmpS1307 = _M0L6_2atmpS1308 + 2;
      _M0L6bufferS319[_M0L6_2atmpS1307] = _M0L6d2__hiS317;
      _M0L6_2atmpS1312 = _M0Lm6offsetS308;
      _M0L6_2atmpS1311 = _M0L12digit__startS310 + _M0L6_2atmpS1312;
      _M0L6_2atmpS1310 = _M0L6_2atmpS1311 + 3;
      _M0L6bufferS319[_M0L6_2atmpS1310] = _M0L6d2__loS318;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1356 = _M0Lm3numS306;
  _M0Lm9remainingS321 = *(int32_t*)&_M0L6_2atmpS1356;
  while (1) {
    int32_t _M0L6_2atmpS1324 = _M0Lm9remainingS321;
    if (_M0L6_2atmpS1324 >= 100) {
      int32_t _M0L6_2atmpS1336 = _M0Lm9remainingS321;
      int32_t _M0L1tS322 = _M0L6_2atmpS1336 / 100;
      int32_t _M0L6_2atmpS1335 = _M0Lm9remainingS321;
      int32_t _M0L1dS323 = _M0L6_2atmpS1335 % 100;
      int32_t _M0L6_2atmpS1325;
      int32_t _M0L6_2atmpS1334;
      int32_t _M0L6_2atmpS1333;
      int32_t _M0L5d__hiS324;
      int32_t _M0L6_2atmpS1332;
      int32_t _M0L6_2atmpS1331;
      int32_t _M0L5d__loS325;
      int32_t _M0L6_2atmpS1327;
      int32_t _M0L6_2atmpS1326;
      int32_t _M0L6_2atmpS1330;
      int32_t _M0L6_2atmpS1329;
      int32_t _M0L6_2atmpS1328;
      _M0Lm9remainingS321 = _M0L1tS322;
      _M0L6_2atmpS1325 = _M0Lm6offsetS308;
      _M0Lm6offsetS308 = _M0L6_2atmpS1325 - 2;
      _M0L6_2atmpS1334 = _M0L1dS323 / 10;
      _M0L6_2atmpS1333 = 48 + _M0L6_2atmpS1334;
      _M0L5d__hiS324 = (uint16_t)_M0L6_2atmpS1333;
      _M0L6_2atmpS1332 = _M0L1dS323 % 10;
      _M0L6_2atmpS1331 = 48 + _M0L6_2atmpS1332;
      _M0L5d__loS325 = (uint16_t)_M0L6_2atmpS1331;
      _M0L6_2atmpS1327 = _M0Lm6offsetS308;
      _M0L6_2atmpS1326 = _M0L12digit__startS310 + _M0L6_2atmpS1327;
      _M0L6bufferS319[_M0L6_2atmpS1326] = _M0L5d__hiS324;
      _M0L6_2atmpS1330 = _M0Lm6offsetS308;
      _M0L6_2atmpS1329 = _M0L12digit__startS310 + _M0L6_2atmpS1330;
      _M0L6_2atmpS1328 = _M0L6_2atmpS1329 + 1;
      _M0L6bufferS319[_M0L6_2atmpS1328] = _M0L5d__loS325;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1337 = _M0Lm9remainingS321;
  if (_M0L6_2atmpS1337 >= 10) {
    int32_t _M0L6_2atmpS1338 = _M0Lm6offsetS308;
    int32_t _M0L6_2atmpS1349;
    int32_t _M0L6_2atmpS1348;
    int32_t _M0L6_2atmpS1347;
    int32_t _M0L5d__hiS327;
    int32_t _M0L6_2atmpS1346;
    int32_t _M0L6_2atmpS1345;
    int32_t _M0L6_2atmpS1344;
    int32_t _M0L5d__loS328;
    int32_t _M0L6_2atmpS1340;
    int32_t _M0L6_2atmpS1339;
    int32_t _M0L6_2atmpS1343;
    int32_t _M0L6_2atmpS1342;
    int32_t _M0L6_2atmpS1341;
    _M0Lm6offsetS308 = _M0L6_2atmpS1338 - 2;
    _M0L6_2atmpS1349 = _M0Lm9remainingS321;
    _M0L6_2atmpS1348 = _M0L6_2atmpS1349 / 10;
    _M0L6_2atmpS1347 = 48 + _M0L6_2atmpS1348;
    _M0L5d__hiS327 = (uint16_t)_M0L6_2atmpS1347;
    _M0L6_2atmpS1346 = _M0Lm9remainingS321;
    _M0L6_2atmpS1345 = _M0L6_2atmpS1346 % 10;
    _M0L6_2atmpS1344 = 48 + _M0L6_2atmpS1345;
    _M0L5d__loS328 = (uint16_t)_M0L6_2atmpS1344;
    _M0L6_2atmpS1340 = _M0Lm6offsetS308;
    _M0L6_2atmpS1339 = _M0L12digit__startS310 + _M0L6_2atmpS1340;
    _M0L6bufferS319[_M0L6_2atmpS1339] = _M0L5d__hiS327;
    _M0L6_2atmpS1343 = _M0Lm6offsetS308;
    _M0L6_2atmpS1342 = _M0L12digit__startS310 + _M0L6_2atmpS1343;
    _M0L6_2atmpS1341 = _M0L6_2atmpS1342 + 1;
    _M0L6bufferS319[_M0L6_2atmpS1341] = _M0L5d__loS328;
    moonbit_decref(_M0L6bufferS319);
  } else {
    int32_t _M0L6_2atmpS1350 = _M0Lm6offsetS308;
    int32_t _M0L6_2atmpS1355;
    int32_t _M0L6_2atmpS1351;
    int32_t _M0L6_2atmpS1354;
    int32_t _M0L6_2atmpS1353;
    int32_t _M0L6_2atmpS1352;
    _M0Lm6offsetS308 = _M0L6_2atmpS1350 - 1;
    _M0L6_2atmpS1355 = _M0Lm6offsetS308;
    _M0L6_2atmpS1351 = _M0L12digit__startS310 + _M0L6_2atmpS1355;
    _M0L6_2atmpS1354 = _M0Lm9remainingS321;
    _M0L6_2atmpS1353 = 48 + _M0L6_2atmpS1354;
    _M0L6_2atmpS1352 = (uint16_t)_M0L6_2atmpS1353;
    _M0L6bufferS319[_M0L6_2atmpS1351] = _M0L6_2atmpS1352;
    moonbit_decref(_M0L6bufferS319);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS301,
  uint32_t _M0L3numS295,
  int32_t _M0L12digit__startS293,
  int32_t _M0L10total__lenS292,
  int32_t _M0L5radixS297
) {
  int32_t _M0Lm6offsetS291;
  uint32_t _M0Lm1nS294;
  uint32_t _M0L4baseS296;
  int32_t _M0L6_2atmpS1282;
  int32_t _M0L6_2atmpS1281;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS291 = _M0L10total__lenS292 - _M0L12digit__startS293;
  _M0Lm1nS294 = _M0L3numS295;
  _M0L4baseS296 = *(uint32_t*)&_M0L5radixS297;
  _M0L6_2atmpS1282 = _M0L5radixS297 - 1;
  _M0L6_2atmpS1281 = _M0L5radixS297 & _M0L6_2atmpS1282;
  if (_M0L6_2atmpS1281 == 0) {
    int32_t _M0L5shiftS298;
    uint32_t _M0L4maskS299;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS298 = moonbit_ctz32(_M0L5radixS297);
    _M0L4maskS299 = _M0L4baseS296 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1283 = _M0Lm1nS294;
      if (_M0L6_2atmpS1283 > 0u) {
        int32_t _M0L6_2atmpS1284 = _M0Lm6offsetS291;
        uint32_t _M0L6_2atmpS1290;
        uint32_t _M0L6_2atmpS1289;
        int32_t _M0L5digitS300;
        int32_t _M0L6_2atmpS1287;
        int32_t _M0L6_2atmpS1285;
        int32_t _M0L6_2atmpS1286;
        uint32_t _M0L6_2atmpS1288;
        _M0Lm6offsetS291 = _M0L6_2atmpS1284 - 1;
        _M0L6_2atmpS1290 = _M0Lm1nS294;
        _M0L6_2atmpS1289 = _M0L6_2atmpS1290 & _M0L4maskS299;
        _M0L5digitS300 = *(int32_t*)&_M0L6_2atmpS1289;
        _M0L6_2atmpS1287 = _M0Lm6offsetS291;
        _M0L6_2atmpS1285 = _M0L12digit__startS293 + _M0L6_2atmpS1287;
        _M0L6_2atmpS1286
        = ((moonbit_string_t)moonbit_string_literal_40.data)[
          _M0L5digitS300
        ];
        _M0L6bufferS301[_M0L6_2atmpS1285] = _M0L6_2atmpS1286;
        _M0L6_2atmpS1288 = _M0Lm1nS294;
        _M0Lm1nS294 = _M0L6_2atmpS1288 >> (_M0L5shiftS298 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS301);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1291 = _M0Lm1nS294;
      if (_M0L6_2atmpS1291 > 0u) {
        int32_t _M0L6_2atmpS1292 = _M0Lm6offsetS291;
        uint32_t _M0L6_2atmpS1299;
        uint32_t _M0L1qS303;
        uint32_t _M0L6_2atmpS1297;
        uint32_t _M0L6_2atmpS1298;
        uint32_t _M0L6_2atmpS1296;
        int32_t _M0L5digitS304;
        int32_t _M0L6_2atmpS1295;
        int32_t _M0L6_2atmpS1293;
        int32_t _M0L6_2atmpS1294;
        _M0Lm6offsetS291 = _M0L6_2atmpS1292 - 1;
        _M0L6_2atmpS1299 = _M0Lm1nS294;
        _M0L1qS303 = _M0L6_2atmpS1299 / _M0L4baseS296;
        _M0L6_2atmpS1297 = _M0Lm1nS294;
        _M0L6_2atmpS1298 = _M0L1qS303 * _M0L4baseS296;
        _M0L6_2atmpS1296 = _M0L6_2atmpS1297 - _M0L6_2atmpS1298;
        _M0L5digitS304 = *(int32_t*)&_M0L6_2atmpS1296;
        _M0L6_2atmpS1295 = _M0Lm6offsetS291;
        _M0L6_2atmpS1293 = _M0L12digit__startS293 + _M0L6_2atmpS1295;
        _M0L6_2atmpS1294
        = ((moonbit_string_t)moonbit_string_literal_40.data)[
          _M0L5digitS304
        ];
        _M0L6bufferS301[_M0L6_2atmpS1293] = _M0L6_2atmpS1294;
        _M0Lm1nS294 = _M0L1qS303;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS301);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS288,
  uint32_t _M0L3numS284,
  int32_t _M0L12digit__startS282,
  int32_t _M0L10total__lenS281
) {
  int32_t _M0Lm6offsetS280;
  uint32_t _M0Lm1nS283;
  int32_t _M0L6_2atmpS1277;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS280 = _M0L10total__lenS281 - _M0L12digit__startS282;
  _M0Lm1nS283 = _M0L3numS284;
  while (1) {
    int32_t _M0L6_2atmpS1265 = _M0Lm6offsetS280;
    if (_M0L6_2atmpS1265 >= 2) {
      int32_t _M0L6_2atmpS1266 = _M0Lm6offsetS280;
      uint32_t _M0L6_2atmpS1276;
      uint32_t _M0L6_2atmpS1275;
      int32_t _M0L9byte__valS285;
      int32_t _M0L2hiS286;
      int32_t _M0L2loS287;
      int32_t _M0L6_2atmpS1269;
      int32_t _M0L6_2atmpS1267;
      int32_t _M0L6_2atmpS1268;
      int32_t _M0L6_2atmpS1273;
      int32_t _M0L6_2atmpS1272;
      int32_t _M0L6_2atmpS1270;
      int32_t _M0L6_2atmpS1271;
      uint32_t _M0L6_2atmpS1274;
      _M0Lm6offsetS280 = _M0L6_2atmpS1266 - 2;
      _M0L6_2atmpS1276 = _M0Lm1nS283;
      _M0L6_2atmpS1275 = _M0L6_2atmpS1276 & 255u;
      _M0L9byte__valS285 = *(int32_t*)&_M0L6_2atmpS1275;
      _M0L2hiS286 = _M0L9byte__valS285 / 16;
      _M0L2loS287 = _M0L9byte__valS285 % 16;
      _M0L6_2atmpS1269 = _M0Lm6offsetS280;
      _M0L6_2atmpS1267 = _M0L12digit__startS282 + _M0L6_2atmpS1269;
      _M0L6_2atmpS1268
      = ((moonbit_string_t)moonbit_string_literal_40.data)[
        _M0L2hiS286
      ];
      _M0L6bufferS288[_M0L6_2atmpS1267] = _M0L6_2atmpS1268;
      _M0L6_2atmpS1273 = _M0Lm6offsetS280;
      _M0L6_2atmpS1272 = _M0L12digit__startS282 + _M0L6_2atmpS1273;
      _M0L6_2atmpS1270 = _M0L6_2atmpS1272 + 1;
      _M0L6_2atmpS1271
      = ((moonbit_string_t)moonbit_string_literal_40.data)[
        _M0L2loS287
      ];
      _M0L6bufferS288[_M0L6_2atmpS1270] = _M0L6_2atmpS1271;
      _M0L6_2atmpS1274 = _M0Lm1nS283;
      _M0Lm1nS283 = _M0L6_2atmpS1274 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1277 = _M0Lm6offsetS280;
  if (_M0L6_2atmpS1277 == 1) {
    uint32_t _M0L6_2atmpS1280 = _M0Lm1nS283;
    uint32_t _M0L6_2atmpS1279 = _M0L6_2atmpS1280 & 15u;
    int32_t _M0L6nibbleS290 = *(int32_t*)&_M0L6_2atmpS1279;
    int32_t _M0L6_2atmpS1278 =
      ((moonbit_string_t)moonbit_string_literal_40.data)[_M0L6nibbleS290];
    _M0L6bufferS288[_M0L12digit__startS282] = _M0L6_2atmpS1278;
    moonbit_decref(_M0L6bufferS288);
  } else {
    moonbit_decref(_M0L6bufferS288);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS279) {
  struct _M0TWEOs* _M0L7_2afuncS278;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS278 = _M0L4selfS279;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS278->code(_M0L7_2afuncS278);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS273
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS272;
  struct _M0TPB6Logger _M0L6_2atmpS1262;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS272 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS272);
  _M0L6_2atmpS1262
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS272
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS273, _M0L6_2atmpS1262);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS272);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS275
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS274;
  struct _M0TPB6Logger _M0L6_2atmpS1263;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS274 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS274);
  _M0L6_2atmpS1263
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS274
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS275, _M0L6_2atmpS1263);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS274);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS277
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS276;
  struct _M0TPB6Logger _M0L6_2atmpS1264;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS276 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS276);
  _M0L6_2atmpS1264
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS276
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS277, _M0L6_2atmpS1264);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS276);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS271
) {
  int32_t _M0L8_2afieldS2110;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2110 = _M0L4selfS271.$1;
  moonbit_decref(_M0L4selfS271.$0);
  return _M0L8_2afieldS2110;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS270
) {
  int32_t _M0L3endS1260;
  int32_t _M0L8_2afieldS2111;
  int32_t _M0L5startS1261;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1260 = _M0L4selfS270.$2;
  _M0L8_2afieldS2111 = _M0L4selfS270.$1;
  moonbit_decref(_M0L4selfS270.$0);
  _M0L5startS1261 = _M0L8_2afieldS2111;
  return _M0L3endS1260 - _M0L5startS1261;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS269
) {
  moonbit_string_t _M0L8_2afieldS2112;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS2112 = _M0L4selfS269.$0;
  return _M0L8_2afieldS2112;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS265,
  moonbit_string_t _M0L5valueS266,
  int32_t _M0L5startS267,
  int32_t _M0L3lenS268
) {
  int32_t _M0L6_2atmpS1259;
  int64_t _M0L6_2atmpS1258;
  struct _M0TPC16string10StringView _M0L6_2atmpS1257;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1259 = _M0L5startS267 + _M0L3lenS268;
  _M0L6_2atmpS1258 = (int64_t)_M0L6_2atmpS1259;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1257
  = _M0MPC16string6String11sub_2einner(_M0L5valueS266, _M0L5startS267, _M0L6_2atmpS1258);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS265, _M0L6_2atmpS1257);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS258,
  int32_t _M0L5startS264,
  int64_t _M0L3endS260
) {
  int32_t _M0L3lenS257;
  int32_t _M0L3endS259;
  int32_t _M0L5startS263;
  int32_t _if__result_2356;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS257 = Moonbit_array_length(_M0L4selfS258);
  if (_M0L3endS260 == 4294967296ll) {
    _M0L3endS259 = _M0L3lenS257;
  } else {
    int64_t _M0L7_2aSomeS261 = _M0L3endS260;
    int32_t _M0L6_2aendS262 = (int32_t)_M0L7_2aSomeS261;
    if (_M0L6_2aendS262 < 0) {
      _M0L3endS259 = _M0L3lenS257 + _M0L6_2aendS262;
    } else {
      _M0L3endS259 = _M0L6_2aendS262;
    }
  }
  if (_M0L5startS264 < 0) {
    _M0L5startS263 = _M0L3lenS257 + _M0L5startS264;
  } else {
    _M0L5startS263 = _M0L5startS264;
  }
  if (_M0L5startS263 >= 0) {
    if (_M0L5startS263 <= _M0L3endS259) {
      _if__result_2356 = _M0L3endS259 <= _M0L3lenS257;
    } else {
      _if__result_2356 = 0;
    }
  } else {
    _if__result_2356 = 0;
  }
  if (_if__result_2356) {
    if (_M0L5startS263 < _M0L3lenS257) {
      int32_t _M0L6_2atmpS1254 = _M0L4selfS258[_M0L5startS263];
      int32_t _M0L6_2atmpS1253;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1253
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1254);
      if (!_M0L6_2atmpS1253) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS259 < _M0L3lenS257) {
      int32_t _M0L6_2atmpS1256 = _M0L4selfS258[_M0L3endS259];
      int32_t _M0L6_2atmpS1255;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1255
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1256);
      if (!_M0L6_2atmpS1255) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS263,
                                                 _M0L3endS259,
                                                 _M0L4selfS258};
  } else {
    moonbit_decref(_M0L4selfS258);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS254) {
  struct _M0TPB6Hasher* _M0L1hS253;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS253 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS253);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS253, _M0L4selfS254);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS253);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS256
) {
  struct _M0TPB6Hasher* _M0L1hS255;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS255 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS255);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS255, _M0L4selfS256);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS255);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS251) {
  int32_t _M0L4seedS250;
  if (_M0L10seed_2eoptS251 == 4294967296ll) {
    _M0L4seedS250 = 0;
  } else {
    int64_t _M0L7_2aSomeS252 = _M0L10seed_2eoptS251;
    _M0L4seedS250 = (int32_t)_M0L7_2aSomeS252;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS250);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS249) {
  uint32_t _M0L6_2atmpS1252;
  uint32_t _M0L6_2atmpS1251;
  struct _M0TPB6Hasher* _block_2357;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1252 = *(uint32_t*)&_M0L4seedS249;
  _M0L6_2atmpS1251 = _M0L6_2atmpS1252 + 374761393u;
  _block_2357
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_2357)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_2357->$0 = _M0L6_2atmpS1251;
  return _block_2357;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS248) {
  uint32_t _M0L6_2atmpS1250;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1250 = _M0MPB6Hasher9avalanche(_M0L4selfS248);
  return *(int32_t*)&_M0L6_2atmpS1250;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS247) {
  uint32_t _M0L8_2afieldS2113;
  uint32_t _M0Lm3accS246;
  uint32_t _M0L6_2atmpS1239;
  uint32_t _M0L6_2atmpS1241;
  uint32_t _M0L6_2atmpS1240;
  uint32_t _M0L6_2atmpS1242;
  uint32_t _M0L6_2atmpS1243;
  uint32_t _M0L6_2atmpS1245;
  uint32_t _M0L6_2atmpS1244;
  uint32_t _M0L6_2atmpS1246;
  uint32_t _M0L6_2atmpS1247;
  uint32_t _M0L6_2atmpS1249;
  uint32_t _M0L6_2atmpS1248;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS2113 = _M0L4selfS247->$0;
  moonbit_decref(_M0L4selfS247);
  _M0Lm3accS246 = _M0L8_2afieldS2113;
  _M0L6_2atmpS1239 = _M0Lm3accS246;
  _M0L6_2atmpS1241 = _M0Lm3accS246;
  _M0L6_2atmpS1240 = _M0L6_2atmpS1241 >> 15;
  _M0Lm3accS246 = _M0L6_2atmpS1239 ^ _M0L6_2atmpS1240;
  _M0L6_2atmpS1242 = _M0Lm3accS246;
  _M0Lm3accS246 = _M0L6_2atmpS1242 * 2246822519u;
  _M0L6_2atmpS1243 = _M0Lm3accS246;
  _M0L6_2atmpS1245 = _M0Lm3accS246;
  _M0L6_2atmpS1244 = _M0L6_2atmpS1245 >> 13;
  _M0Lm3accS246 = _M0L6_2atmpS1243 ^ _M0L6_2atmpS1244;
  _M0L6_2atmpS1246 = _M0Lm3accS246;
  _M0Lm3accS246 = _M0L6_2atmpS1246 * 3266489917u;
  _M0L6_2atmpS1247 = _M0Lm3accS246;
  _M0L6_2atmpS1249 = _M0Lm3accS246;
  _M0L6_2atmpS1248 = _M0L6_2atmpS1249 >> 16;
  _M0Lm3accS246 = _M0L6_2atmpS1247 ^ _M0L6_2atmpS1248;
  return _M0Lm3accS246;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS244,
  moonbit_string_t _M0L1yS245
) {
  int32_t _M0L6_2atmpS2114;
  int32_t _M0L6_2atmpS1238;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS2114 = moonbit_val_array_equal(_M0L1xS244, _M0L1yS245);
  moonbit_decref(_M0L1xS244);
  moonbit_decref(_M0L1yS245);
  _M0L6_2atmpS1238 = _M0L6_2atmpS2114;
  return !_M0L6_2atmpS1238;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS241,
  int32_t _M0L5valueS240
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS240, _M0L4selfS241);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS243,
  moonbit_string_t _M0L5valueS242
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS242, _M0L4selfS243);
  return 0;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS238,
  int32_t _M0L5valueS239
) {
  uint32_t _M0L6_2atmpS1237;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1237 = *(uint32_t*)&_M0L5valueS239;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS238, _M0L6_2atmpS1237);
  return 0;
}

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show _M0L3objS228,
  moonbit_string_t _M0L7contentS229,
  moonbit_string_t _M0L3locS231,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS233
) {
  moonbit_string_t _M0L6actualS227;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6actualS227 = _M0L3objS228.$0->$method_1(_M0L3objS228.$1);
  moonbit_incref(_M0L7contentS229);
  moonbit_incref(_M0L6actualS227);
  #line 192 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS227, _M0L7contentS229)
  ) {
    moonbit_string_t _M0L3locS230;
    moonbit_string_t _M0L9args__locS232;
    moonbit_string_t _M0L15expect__escapedS234;
    moonbit_string_t _M0L15actual__escapedS235;
    moonbit_string_t _M0L6_2atmpS1235;
    moonbit_string_t _M0L6_2atmpS1234;
    moonbit_string_t _M0L6_2atmpS2130;
    moonbit_string_t _M0L6_2atmpS1233;
    moonbit_string_t _M0L6_2atmpS2129;
    moonbit_string_t _M0L14expect__base64S236;
    moonbit_string_t _M0L6_2atmpS1232;
    moonbit_string_t _M0L6_2atmpS1231;
    moonbit_string_t _M0L6_2atmpS2128;
    moonbit_string_t _M0L6_2atmpS1230;
    moonbit_string_t _M0L6_2atmpS2127;
    moonbit_string_t _M0L14actual__base64S237;
    moonbit_string_t _M0L6_2atmpS1229;
    moonbit_string_t _M0L6_2atmpS2126;
    moonbit_string_t _M0L6_2atmpS1228;
    moonbit_string_t _M0L6_2atmpS2125;
    moonbit_string_t _M0L6_2atmpS1226;
    moonbit_string_t _M0L6_2atmpS1227;
    moonbit_string_t _M0L6_2atmpS2124;
    moonbit_string_t _M0L6_2atmpS1225;
    moonbit_string_t _M0L6_2atmpS2123;
    moonbit_string_t _M0L6_2atmpS1223;
    moonbit_string_t _M0L6_2atmpS1224;
    moonbit_string_t _M0L6_2atmpS2122;
    moonbit_string_t _M0L6_2atmpS1222;
    moonbit_string_t _M0L6_2atmpS2121;
    moonbit_string_t _M0L6_2atmpS1220;
    moonbit_string_t _M0L6_2atmpS1221;
    moonbit_string_t _M0L6_2atmpS2120;
    moonbit_string_t _M0L6_2atmpS1219;
    moonbit_string_t _M0L6_2atmpS2119;
    moonbit_string_t _M0L6_2atmpS1217;
    moonbit_string_t _M0L6_2atmpS1218;
    moonbit_string_t _M0L6_2atmpS2118;
    moonbit_string_t _M0L6_2atmpS1216;
    moonbit_string_t _M0L6_2atmpS2117;
    moonbit_string_t _M0L6_2atmpS1214;
    moonbit_string_t _M0L6_2atmpS1215;
    moonbit_string_t _M0L6_2atmpS2116;
    moonbit_string_t _M0L6_2atmpS1213;
    moonbit_string_t _M0L6_2atmpS2115;
    moonbit_string_t _M0L6_2atmpS1212;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1211;
    struct moonbit_result_0 _result_2358;
    #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L3locS230 = _M0MPB9SourceLoc16to__json__string(_M0L3locS231);
    #line 194 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L9args__locS232 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS233);
    moonbit_incref(_M0L7contentS229);
    #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L15expect__escapedS234
    = _M0MPC16string6String6escape(_M0L7contentS229);
    moonbit_incref(_M0L6actualS227);
    #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L15actual__escapedS235 = _M0MPC16string6String6escape(_M0L6actualS227);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1235
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS229);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1234
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1235);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2130
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_41.data, _M0L6_2atmpS1234);
    moonbit_decref(_M0L6_2atmpS1234);
    _M0L6_2atmpS1233 = _M0L6_2atmpS2130;
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2129
    = moonbit_add_string(_M0L6_2atmpS1233, (moonbit_string_t)moonbit_string_literal_41.data);
    moonbit_decref(_M0L6_2atmpS1233);
    _M0L14expect__base64S236 = _M0L6_2atmpS2129;
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1232
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS227);
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1231
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1232);
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2128
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_41.data, _M0L6_2atmpS1231);
    moonbit_decref(_M0L6_2atmpS1231);
    _M0L6_2atmpS1230 = _M0L6_2atmpS2128;
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2127
    = moonbit_add_string(_M0L6_2atmpS1230, (moonbit_string_t)moonbit_string_literal_41.data);
    moonbit_decref(_M0L6_2atmpS1230);
    _M0L14actual__base64S237 = _M0L6_2atmpS2127;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1229 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS230);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2126
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_42.data, _M0L6_2atmpS1229);
    moonbit_decref(_M0L6_2atmpS1229);
    _M0L6_2atmpS1228 = _M0L6_2atmpS2126;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2125
    = moonbit_add_string(_M0L6_2atmpS1228, (moonbit_string_t)moonbit_string_literal_43.data);
    moonbit_decref(_M0L6_2atmpS1228);
    _M0L6_2atmpS1226 = _M0L6_2atmpS2125;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1227
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS232);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2124 = moonbit_add_string(_M0L6_2atmpS1226, _M0L6_2atmpS1227);
    moonbit_decref(_M0L6_2atmpS1226);
    moonbit_decref(_M0L6_2atmpS1227);
    _M0L6_2atmpS1225 = _M0L6_2atmpS2124;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2123
    = moonbit_add_string(_M0L6_2atmpS1225, (moonbit_string_t)moonbit_string_literal_44.data);
    moonbit_decref(_M0L6_2atmpS1225);
    _M0L6_2atmpS1223 = _M0L6_2atmpS2123;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1224
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS234);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2122 = moonbit_add_string(_M0L6_2atmpS1223, _M0L6_2atmpS1224);
    moonbit_decref(_M0L6_2atmpS1223);
    moonbit_decref(_M0L6_2atmpS1224);
    _M0L6_2atmpS1222 = _M0L6_2atmpS2122;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2121
    = moonbit_add_string(_M0L6_2atmpS1222, (moonbit_string_t)moonbit_string_literal_45.data);
    moonbit_decref(_M0L6_2atmpS1222);
    _M0L6_2atmpS1220 = _M0L6_2atmpS2121;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1221
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS235);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2120 = moonbit_add_string(_M0L6_2atmpS1220, _M0L6_2atmpS1221);
    moonbit_decref(_M0L6_2atmpS1220);
    moonbit_decref(_M0L6_2atmpS1221);
    _M0L6_2atmpS1219 = _M0L6_2atmpS2120;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2119
    = moonbit_add_string(_M0L6_2atmpS1219, (moonbit_string_t)moonbit_string_literal_46.data);
    moonbit_decref(_M0L6_2atmpS1219);
    _M0L6_2atmpS1217 = _M0L6_2atmpS2119;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1218
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S236);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2118 = moonbit_add_string(_M0L6_2atmpS1217, _M0L6_2atmpS1218);
    moonbit_decref(_M0L6_2atmpS1217);
    moonbit_decref(_M0L6_2atmpS1218);
    _M0L6_2atmpS1216 = _M0L6_2atmpS2118;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2117
    = moonbit_add_string(_M0L6_2atmpS1216, (moonbit_string_t)moonbit_string_literal_47.data);
    moonbit_decref(_M0L6_2atmpS1216);
    _M0L6_2atmpS1214 = _M0L6_2atmpS2117;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1215
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S237);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2116 = moonbit_add_string(_M0L6_2atmpS1214, _M0L6_2atmpS1215);
    moonbit_decref(_M0L6_2atmpS1214);
    moonbit_decref(_M0L6_2atmpS1215);
    _M0L6_2atmpS1213 = _M0L6_2atmpS2116;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS2115
    = moonbit_add_string(_M0L6_2atmpS1213, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS1213);
    _M0L6_2atmpS1212 = _M0L6_2atmpS2115;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1211
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1211)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1211)->$0
    = _M0L6_2atmpS1212;
    _result_2358.tag = 0;
    _result_2358.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1211;
    return _result_2358;
  } else {
    int32_t _M0L6_2atmpS1236;
    struct moonbit_result_0 _result_2359;
    moonbit_decref(_M0L9args__locS233);
    moonbit_decref(_M0L3locS231);
    moonbit_decref(_M0L7contentS229);
    moonbit_decref(_M0L6actualS227);
    _M0L6_2atmpS1236 = 0;
    _result_2359.tag = 1;
    _result_2359.data.ok = _M0L6_2atmpS1236;
    return _result_2359;
  }
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS220
) {
  struct _M0TPB13StringBuilder* _M0L3bufS218;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS219;
  int32_t _M0L7_2abindS221;
  int32_t _M0L1iS222;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS218 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS219 = _M0L4selfS220;
  moonbit_incref(_M0L3bufS218);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS218, 91);
  _M0L7_2abindS221 = _M0L7_2aselfS219->$1;
  _M0L1iS222 = 0;
  while (1) {
    if (_M0L1iS222 < _M0L7_2abindS221) {
      int32_t _if__result_2361;
      moonbit_string_t* _M0L8_2afieldS2132;
      moonbit_string_t* _M0L3bufS1209;
      moonbit_string_t _M0L6_2atmpS2131;
      moonbit_string_t _M0L4itemS223;
      int32_t _M0L6_2atmpS1210;
      if (_M0L1iS222 != 0) {
        moonbit_incref(_M0L3bufS218);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS218, (moonbit_string_t)moonbit_string_literal_48.data);
      }
      if (_M0L1iS222 < 0) {
        _if__result_2361 = 1;
      } else {
        int32_t _M0L3lenS1208 = _M0L7_2aselfS219->$1;
        _if__result_2361 = _M0L1iS222 >= _M0L3lenS1208;
      }
      if (_if__result_2361) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS2132 = _M0L7_2aselfS219->$0;
      _M0L3bufS1209 = _M0L8_2afieldS2132;
      _M0L6_2atmpS2131 = (moonbit_string_t)_M0L3bufS1209[_M0L1iS222];
      _M0L4itemS223 = _M0L6_2atmpS2131;
      if (_M0L4itemS223 == 0) {
        moonbit_incref(_M0L3bufS218);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS218, (moonbit_string_t)moonbit_string_literal_49.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS224 = _M0L4itemS223;
        moonbit_string_t _M0L6_2alocS225 = _M0L7_2aSomeS224;
        moonbit_string_t _M0L6_2atmpS1207;
        moonbit_incref(_M0L6_2alocS225);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1207
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS225);
        moonbit_incref(_M0L3bufS218);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS218, _M0L6_2atmpS1207);
      }
      _M0L6_2atmpS1210 = _M0L1iS222 + 1;
      _M0L1iS222 = _M0L6_2atmpS1210;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS219);
    }
    break;
  }
  moonbit_incref(_M0L3bufS218);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS218, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS218);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS217
) {
  moonbit_string_t _M0L6_2atmpS1206;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1205;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1206 = _M0L4selfS217;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1205 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1206);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1205);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS216
) {
  struct _M0TPB13StringBuilder* _M0L2sbS215;
  struct _M0TPC16string10StringView _M0L8_2afieldS2145;
  struct _M0TPC16string10StringView _M0L3pkgS1190;
  moonbit_string_t _M0L6_2atmpS1189;
  moonbit_string_t _M0L6_2atmpS2144;
  moonbit_string_t _M0L6_2atmpS1188;
  moonbit_string_t _M0L6_2atmpS2143;
  moonbit_string_t _M0L6_2atmpS1187;
  struct _M0TPC16string10StringView _M0L8_2afieldS2142;
  struct _M0TPC16string10StringView _M0L8filenameS1191;
  struct _M0TPC16string10StringView _M0L8_2afieldS2141;
  struct _M0TPC16string10StringView _M0L11start__lineS1194;
  moonbit_string_t _M0L6_2atmpS1193;
  moonbit_string_t _M0L6_2atmpS2140;
  moonbit_string_t _M0L6_2atmpS1192;
  struct _M0TPC16string10StringView _M0L8_2afieldS2139;
  struct _M0TPC16string10StringView _M0L13start__columnS1197;
  moonbit_string_t _M0L6_2atmpS1196;
  moonbit_string_t _M0L6_2atmpS2138;
  moonbit_string_t _M0L6_2atmpS1195;
  struct _M0TPC16string10StringView _M0L8_2afieldS2137;
  struct _M0TPC16string10StringView _M0L9end__lineS1200;
  moonbit_string_t _M0L6_2atmpS1199;
  moonbit_string_t _M0L6_2atmpS2136;
  moonbit_string_t _M0L6_2atmpS1198;
  struct _M0TPC16string10StringView _M0L8_2afieldS2135;
  int32_t _M0L6_2acntS2253;
  struct _M0TPC16string10StringView _M0L11end__columnS1204;
  moonbit_string_t _M0L6_2atmpS1203;
  moonbit_string_t _M0L6_2atmpS2134;
  moonbit_string_t _M0L6_2atmpS1202;
  moonbit_string_t _M0L6_2atmpS2133;
  moonbit_string_t _M0L6_2atmpS1201;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS215 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS2145
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$0_1, _M0L4selfS216->$0_2, _M0L4selfS216->$0_0
  };
  _M0L3pkgS1190 = _M0L8_2afieldS2145;
  moonbit_incref(_M0L3pkgS1190.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1189
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1190);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2144
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_50.data, _M0L6_2atmpS1189);
  moonbit_decref(_M0L6_2atmpS1189);
  _M0L6_2atmpS1188 = _M0L6_2atmpS2144;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2143
  = moonbit_add_string(_M0L6_2atmpS1188, (moonbit_string_t)moonbit_string_literal_41.data);
  moonbit_decref(_M0L6_2atmpS1188);
  _M0L6_2atmpS1187 = _M0L6_2atmpS2143;
  moonbit_incref(_M0L2sbS215);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, _M0L6_2atmpS1187);
  moonbit_incref(_M0L2sbS215);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, (moonbit_string_t)moonbit_string_literal_51.data);
  _M0L8_2afieldS2142
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$1_1, _M0L4selfS216->$1_2, _M0L4selfS216->$1_0
  };
  _M0L8filenameS1191 = _M0L8_2afieldS2142;
  moonbit_incref(_M0L8filenameS1191.$0);
  moonbit_incref(_M0L2sbS215);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS215, _M0L8filenameS1191);
  _M0L8_2afieldS2141
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$2_1, _M0L4selfS216->$2_2, _M0L4selfS216->$2_0
  };
  _M0L11start__lineS1194 = _M0L8_2afieldS2141;
  moonbit_incref(_M0L11start__lineS1194.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1193
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1194);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2140
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_52.data, _M0L6_2atmpS1193);
  moonbit_decref(_M0L6_2atmpS1193);
  _M0L6_2atmpS1192 = _M0L6_2atmpS2140;
  moonbit_incref(_M0L2sbS215);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, _M0L6_2atmpS1192);
  _M0L8_2afieldS2139
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$3_1, _M0L4selfS216->$3_2, _M0L4selfS216->$3_0
  };
  _M0L13start__columnS1197 = _M0L8_2afieldS2139;
  moonbit_incref(_M0L13start__columnS1197.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1196
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1197);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2138
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_53.data, _M0L6_2atmpS1196);
  moonbit_decref(_M0L6_2atmpS1196);
  _M0L6_2atmpS1195 = _M0L6_2atmpS2138;
  moonbit_incref(_M0L2sbS215);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, _M0L6_2atmpS1195);
  _M0L8_2afieldS2137
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$4_1, _M0L4selfS216->$4_2, _M0L4selfS216->$4_0
  };
  _M0L9end__lineS1200 = _M0L8_2afieldS2137;
  moonbit_incref(_M0L9end__lineS1200.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1199
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1200);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2136
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_54.data, _M0L6_2atmpS1199);
  moonbit_decref(_M0L6_2atmpS1199);
  _M0L6_2atmpS1198 = _M0L6_2atmpS2136;
  moonbit_incref(_M0L2sbS215);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, _M0L6_2atmpS1198);
  _M0L8_2afieldS2135
  = (struct _M0TPC16string10StringView){
    _M0L4selfS216->$5_1, _M0L4selfS216->$5_2, _M0L4selfS216->$5_0
  };
  _M0L6_2acntS2253 = Moonbit_object_header(_M0L4selfS216)->rc;
  if (_M0L6_2acntS2253 > 1) {
    int32_t _M0L11_2anew__cntS2259 = _M0L6_2acntS2253 - 1;
    Moonbit_object_header(_M0L4selfS216)->rc = _M0L11_2anew__cntS2259;
    moonbit_incref(_M0L8_2afieldS2135.$0);
  } else if (_M0L6_2acntS2253 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS2258 =
      (struct _M0TPC16string10StringView){_M0L4selfS216->$4_1,
                                            _M0L4selfS216->$4_2,
                                            _M0L4selfS216->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS2257;
    struct _M0TPC16string10StringView _M0L8_2afieldS2256;
    struct _M0TPC16string10StringView _M0L8_2afieldS2255;
    struct _M0TPC16string10StringView _M0L8_2afieldS2254;
    moonbit_decref(_M0L8_2afieldS2258.$0);
    _M0L8_2afieldS2257
    = (struct _M0TPC16string10StringView){
      _M0L4selfS216->$3_1, _M0L4selfS216->$3_2, _M0L4selfS216->$3_0
    };
    moonbit_decref(_M0L8_2afieldS2257.$0);
    _M0L8_2afieldS2256
    = (struct _M0TPC16string10StringView){
      _M0L4selfS216->$2_1, _M0L4selfS216->$2_2, _M0L4selfS216->$2_0
    };
    moonbit_decref(_M0L8_2afieldS2256.$0);
    _M0L8_2afieldS2255
    = (struct _M0TPC16string10StringView){
      _M0L4selfS216->$1_1, _M0L4selfS216->$1_2, _M0L4selfS216->$1_0
    };
    moonbit_decref(_M0L8_2afieldS2255.$0);
    _M0L8_2afieldS2254
    = (struct _M0TPC16string10StringView){
      _M0L4selfS216->$0_1, _M0L4selfS216->$0_2, _M0L4selfS216->$0_0
    };
    moonbit_decref(_M0L8_2afieldS2254.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS216);
  }
  _M0L11end__columnS1204 = _M0L8_2afieldS2135;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1203
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1204);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2134
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_55.data, _M0L6_2atmpS1203);
  moonbit_decref(_M0L6_2atmpS1203);
  _M0L6_2atmpS1202 = _M0L6_2atmpS2134;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2133
  = moonbit_add_string(_M0L6_2atmpS1202, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1202);
  _M0L6_2atmpS1201 = _M0L6_2atmpS2133;
  moonbit_incref(_M0L2sbS215);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS215, _M0L6_2atmpS1201);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS215);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS213,
  moonbit_string_t _M0L3strS214
) {
  int32_t _M0L3lenS1177;
  int32_t _M0L6_2atmpS1179;
  int32_t _M0L6_2atmpS1178;
  int32_t _M0L6_2atmpS1176;
  moonbit_bytes_t _M0L8_2afieldS2147;
  moonbit_bytes_t _M0L4dataS1180;
  int32_t _M0L3lenS1181;
  int32_t _M0L6_2atmpS1182;
  int32_t _M0L3lenS1184;
  int32_t _M0L6_2atmpS2146;
  int32_t _M0L6_2atmpS1186;
  int32_t _M0L6_2atmpS1185;
  int32_t _M0L6_2atmpS1183;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1177 = _M0L4selfS213->$1;
  _M0L6_2atmpS1179 = Moonbit_array_length(_M0L3strS214);
  _M0L6_2atmpS1178 = _M0L6_2atmpS1179 * 2;
  _M0L6_2atmpS1176 = _M0L3lenS1177 + _M0L6_2atmpS1178;
  moonbit_incref(_M0L4selfS213);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS213, _M0L6_2atmpS1176);
  _M0L8_2afieldS2147 = _M0L4selfS213->$0;
  _M0L4dataS1180 = _M0L8_2afieldS2147;
  _M0L3lenS1181 = _M0L4selfS213->$1;
  _M0L6_2atmpS1182 = Moonbit_array_length(_M0L3strS214);
  moonbit_incref(_M0L4dataS1180);
  moonbit_incref(_M0L3strS214);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1180, _M0L3lenS1181, _M0L3strS214, 0, _M0L6_2atmpS1182);
  _M0L3lenS1184 = _M0L4selfS213->$1;
  _M0L6_2atmpS2146 = Moonbit_array_length(_M0L3strS214);
  moonbit_decref(_M0L3strS214);
  _M0L6_2atmpS1186 = _M0L6_2atmpS2146;
  _M0L6_2atmpS1185 = _M0L6_2atmpS1186 * 2;
  _M0L6_2atmpS1183 = _M0L3lenS1184 + _M0L6_2atmpS1185;
  _M0L4selfS213->$1 = _M0L6_2atmpS1183;
  moonbit_decref(_M0L4selfS213);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS205,
  int32_t _M0L13bytes__offsetS200,
  moonbit_string_t _M0L3strS207,
  int32_t _M0L11str__offsetS203,
  int32_t _M0L6lengthS201
) {
  int32_t _M0L6_2atmpS1175;
  int32_t _M0L6_2atmpS1174;
  int32_t _M0L2e1S199;
  int32_t _M0L6_2atmpS1173;
  int32_t _M0L2e2S202;
  int32_t _M0L4len1S204;
  int32_t _M0L4len2S206;
  int32_t _if__result_2362;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1175 = _M0L6lengthS201 * 2;
  _M0L6_2atmpS1174 = _M0L13bytes__offsetS200 + _M0L6_2atmpS1175;
  _M0L2e1S199 = _M0L6_2atmpS1174 - 1;
  _M0L6_2atmpS1173 = _M0L11str__offsetS203 + _M0L6lengthS201;
  _M0L2e2S202 = _M0L6_2atmpS1173 - 1;
  _M0L4len1S204 = Moonbit_array_length(_M0L4selfS205);
  _M0L4len2S206 = Moonbit_array_length(_M0L3strS207);
  if (_M0L6lengthS201 >= 0) {
    if (_M0L13bytes__offsetS200 >= 0) {
      if (_M0L2e1S199 < _M0L4len1S204) {
        if (_M0L11str__offsetS203 >= 0) {
          _if__result_2362 = _M0L2e2S202 < _M0L4len2S206;
        } else {
          _if__result_2362 = 0;
        }
      } else {
        _if__result_2362 = 0;
      }
    } else {
      _if__result_2362 = 0;
    }
  } else {
    _if__result_2362 = 0;
  }
  if (_if__result_2362) {
    int32_t _M0L16end__str__offsetS208 =
      _M0L11str__offsetS203 + _M0L6lengthS201;
    int32_t _M0L1iS209 = _M0L11str__offsetS203;
    int32_t _M0L1jS210 = _M0L13bytes__offsetS200;
    while (1) {
      if (_M0L1iS209 < _M0L16end__str__offsetS208) {
        int32_t _M0L6_2atmpS1170 = _M0L3strS207[_M0L1iS209];
        int32_t _M0L6_2atmpS1169 = (int32_t)_M0L6_2atmpS1170;
        uint32_t _M0L1cS211 = *(uint32_t*)&_M0L6_2atmpS1169;
        uint32_t _M0L6_2atmpS1165 = _M0L1cS211 & 255u;
        int32_t _M0L6_2atmpS1164;
        int32_t _M0L6_2atmpS1166;
        uint32_t _M0L6_2atmpS1168;
        int32_t _M0L6_2atmpS1167;
        int32_t _M0L6_2atmpS1171;
        int32_t _M0L6_2atmpS1172;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1164 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1165);
        if (
          _M0L1jS210 < 0 || _M0L1jS210 >= Moonbit_array_length(_M0L4selfS205)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS205[_M0L1jS210] = _M0L6_2atmpS1164;
        _M0L6_2atmpS1166 = _M0L1jS210 + 1;
        _M0L6_2atmpS1168 = _M0L1cS211 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1167 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1168);
        if (
          _M0L6_2atmpS1166 < 0
          || _M0L6_2atmpS1166 >= Moonbit_array_length(_M0L4selfS205)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS205[_M0L6_2atmpS1166] = _M0L6_2atmpS1167;
        _M0L6_2atmpS1171 = _M0L1iS209 + 1;
        _M0L6_2atmpS1172 = _M0L1jS210 + 2;
        _M0L1iS209 = _M0L6_2atmpS1171;
        _M0L1jS210 = _M0L6_2atmpS1172;
        continue;
      } else {
        moonbit_decref(_M0L3strS207);
        moonbit_decref(_M0L4selfS205);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS207);
    moonbit_decref(_M0L4selfS205);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS198,
  struct _M0TPC16string10StringView _M0L3objS197
) {
  struct _M0TPB6Logger _M0L6_2atmpS1163;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1163
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS198
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS197, _M0L6_2atmpS1163);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS143
) {
  int32_t _M0L6_2atmpS1162;
  struct _M0TPC16string10StringView _M0L7_2abindS142;
  moonbit_string_t _M0L7_2adataS144;
  int32_t _M0L8_2astartS145;
  int32_t _M0L6_2atmpS1161;
  int32_t _M0L6_2aendS146;
  int32_t _M0Lm9_2acursorS147;
  int32_t _M0Lm13accept__stateS148;
  int32_t _M0Lm10match__endS149;
  int32_t _M0Lm20match__tag__saver__0S150;
  int32_t _M0Lm20match__tag__saver__1S151;
  int32_t _M0Lm20match__tag__saver__2S152;
  int32_t _M0Lm20match__tag__saver__3S153;
  int32_t _M0Lm20match__tag__saver__4S154;
  int32_t _M0Lm6tag__0S155;
  int32_t _M0Lm6tag__1S156;
  int32_t _M0Lm9tag__1__1S157;
  int32_t _M0Lm9tag__1__2S158;
  int32_t _M0Lm6tag__3S159;
  int32_t _M0Lm6tag__2S160;
  int32_t _M0Lm9tag__2__1S161;
  int32_t _M0Lm6tag__4S162;
  int32_t _M0L6_2atmpS1119;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1162 = Moonbit_array_length(_M0L4reprS143);
  _M0L7_2abindS142
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1162, _M0L4reprS143
  };
  moonbit_incref(_M0L7_2abindS142.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS144 = _M0MPC16string10StringView4data(_M0L7_2abindS142);
  moonbit_incref(_M0L7_2abindS142.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS145
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS142);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1161 = _M0MPC16string10StringView6length(_M0L7_2abindS142);
  _M0L6_2aendS146 = _M0L8_2astartS145 + _M0L6_2atmpS1161;
  _M0Lm9_2acursorS147 = _M0L8_2astartS145;
  _M0Lm13accept__stateS148 = -1;
  _M0Lm10match__endS149 = -1;
  _M0Lm20match__tag__saver__0S150 = -1;
  _M0Lm20match__tag__saver__1S151 = -1;
  _M0Lm20match__tag__saver__2S152 = -1;
  _M0Lm20match__tag__saver__3S153 = -1;
  _M0Lm20match__tag__saver__4S154 = -1;
  _M0Lm6tag__0S155 = -1;
  _M0Lm6tag__1S156 = -1;
  _M0Lm9tag__1__1S157 = -1;
  _M0Lm9tag__1__2S158 = -1;
  _M0Lm6tag__3S159 = -1;
  _M0Lm6tag__2S160 = -1;
  _M0Lm9tag__2__1S161 = -1;
  _M0Lm6tag__4S162 = -1;
  _M0L6_2atmpS1119 = _M0Lm9_2acursorS147;
  if (_M0L6_2atmpS1119 < _M0L6_2aendS146) {
    int32_t _M0L6_2atmpS1121 = _M0Lm9_2acursorS147;
    int32_t _M0L6_2atmpS1120;
    moonbit_incref(_M0L7_2adataS144);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1120
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1121);
    if (_M0L6_2atmpS1120 == 64) {
      int32_t _M0L6_2atmpS1122 = _M0Lm9_2acursorS147;
      _M0Lm9_2acursorS147 = _M0L6_2atmpS1122 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1123;
        _M0Lm6tag__0S155 = _M0Lm9_2acursorS147;
        _M0L6_2atmpS1123 = _M0Lm9_2acursorS147;
        if (_M0L6_2atmpS1123 < _M0L6_2aendS146) {
          int32_t _M0L6_2atmpS1160 = _M0Lm9_2acursorS147;
          int32_t _M0L10next__charS170;
          int32_t _M0L6_2atmpS1124;
          moonbit_incref(_M0L7_2adataS144);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS170
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1160);
          _M0L6_2atmpS1124 = _M0Lm9_2acursorS147;
          _M0Lm9_2acursorS147 = _M0L6_2atmpS1124 + 1;
          if (_M0L10next__charS170 == 58) {
            int32_t _M0L6_2atmpS1125 = _M0Lm9_2acursorS147;
            if (_M0L6_2atmpS1125 < _M0L6_2aendS146) {
              int32_t _M0L6_2atmpS1126 = _M0Lm9_2acursorS147;
              int32_t _M0L12dispatch__15S171;
              _M0Lm9_2acursorS147 = _M0L6_2atmpS1126 + 1;
              _M0L12dispatch__15S171 = 0;
              loop__label__15_174:;
              while (1) {
                int32_t _M0L6_2atmpS1127;
                switch (_M0L12dispatch__15S171) {
                  case 3: {
                    int32_t _M0L6_2atmpS1130;
                    _M0Lm9tag__1__2S158 = _M0Lm9tag__1__1S157;
                    _M0Lm9tag__1__1S157 = _M0Lm6tag__1S156;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1130 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1130 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1135 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS178;
                      int32_t _M0L6_2atmpS1131;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS178
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1135);
                      _M0L6_2atmpS1131 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1131 + 1;
                      if (_M0L10next__charS178 < 58) {
                        if (_M0L10next__charS178 < 48) {
                          goto join_177;
                        } else {
                          int32_t _M0L6_2atmpS1132;
                          _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                          _M0Lm9tag__2__1S161 = _M0Lm6tag__2S160;
                          _M0Lm6tag__2S160 = _M0Lm9_2acursorS147;
                          _M0Lm6tag__3S159 = _M0Lm9_2acursorS147;
                          _M0L6_2atmpS1132 = _M0Lm9_2acursorS147;
                          if (_M0L6_2atmpS1132 < _M0L6_2aendS146) {
                            int32_t _M0L6_2atmpS1134 = _M0Lm9_2acursorS147;
                            int32_t _M0L10next__charS180;
                            int32_t _M0L6_2atmpS1133;
                            moonbit_incref(_M0L7_2adataS144);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS180
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1134);
                            _M0L6_2atmpS1133 = _M0Lm9_2acursorS147;
                            _M0Lm9_2acursorS147 = _M0L6_2atmpS1133 + 1;
                            if (_M0L10next__charS180 < 48) {
                              if (_M0L10next__charS180 == 45) {
                                goto join_172;
                              } else {
                                goto join_179;
                              }
                            } else if (_M0L10next__charS180 > 57) {
                              if (_M0L10next__charS180 < 59) {
                                _M0L12dispatch__15S171 = 3;
                                goto loop__label__15_174;
                              } else {
                                goto join_179;
                              }
                            } else {
                              _M0L12dispatch__15S171 = 6;
                              goto loop__label__15_174;
                            }
                            join_179:;
                            _M0L12dispatch__15S171 = 0;
                            goto loop__label__15_174;
                          } else {
                            goto join_163;
                          }
                        }
                      } else if (_M0L10next__charS178 > 58) {
                        goto join_177;
                      } else {
                        _M0L12dispatch__15S171 = 1;
                        goto loop__label__15_174;
                      }
                      join_177:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1136;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0Lm6tag__2S160 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1136 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1136 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1138 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS182;
                      int32_t _M0L6_2atmpS1137;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS182
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1138);
                      _M0L6_2atmpS1137 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1137 + 1;
                      if (_M0L10next__charS182 < 58) {
                        if (_M0L10next__charS182 < 48) {
                          goto join_181;
                        } else {
                          _M0L12dispatch__15S171 = 2;
                          goto loop__label__15_174;
                        }
                      } else if (_M0L10next__charS182 > 58) {
                        goto join_181;
                      } else {
                        _M0L12dispatch__15S171 = 3;
                        goto loop__label__15_174;
                      }
                      join_181:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1139;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1139 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1139 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1141 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS183;
                      int32_t _M0L6_2atmpS1140;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS183
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1141);
                      _M0L6_2atmpS1140 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1140 + 1;
                      if (_M0L10next__charS183 == 58) {
                        _M0L12dispatch__15S171 = 1;
                        goto loop__label__15_174;
                      } else {
                        _M0L12dispatch__15S171 = 0;
                        goto loop__label__15_174;
                      }
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1142;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0Lm6tag__4S162 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1142 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1142 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1150 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS185;
                      int32_t _M0L6_2atmpS1143;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS185
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1150);
                      _M0L6_2atmpS1143 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1143 + 1;
                      if (_M0L10next__charS185 < 58) {
                        if (_M0L10next__charS185 < 48) {
                          goto join_184;
                        } else {
                          _M0L12dispatch__15S171 = 4;
                          goto loop__label__15_174;
                        }
                      } else if (_M0L10next__charS185 > 58) {
                        goto join_184;
                      } else {
                        int32_t _M0L6_2atmpS1144;
                        _M0Lm9tag__1__2S158 = _M0Lm9tag__1__1S157;
                        _M0Lm9tag__1__1S157 = _M0Lm6tag__1S156;
                        _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                        _M0L6_2atmpS1144 = _M0Lm9_2acursorS147;
                        if (_M0L6_2atmpS1144 < _M0L6_2aendS146) {
                          int32_t _M0L6_2atmpS1149 = _M0Lm9_2acursorS147;
                          int32_t _M0L10next__charS187;
                          int32_t _M0L6_2atmpS1145;
                          moonbit_incref(_M0L7_2adataS144);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS187
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1149);
                          _M0L6_2atmpS1145 = _M0Lm9_2acursorS147;
                          _M0Lm9_2acursorS147 = _M0L6_2atmpS1145 + 1;
                          if (_M0L10next__charS187 < 58) {
                            if (_M0L10next__charS187 < 48) {
                              goto join_186;
                            } else {
                              int32_t _M0L6_2atmpS1146;
                              _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                              _M0Lm9tag__2__1S161 = _M0Lm6tag__2S160;
                              _M0Lm6tag__2S160 = _M0Lm9_2acursorS147;
                              _M0L6_2atmpS1146 = _M0Lm9_2acursorS147;
                              if (_M0L6_2atmpS1146 < _M0L6_2aendS146) {
                                int32_t _M0L6_2atmpS1148 =
                                  _M0Lm9_2acursorS147;
                                int32_t _M0L10next__charS189;
                                int32_t _M0L6_2atmpS1147;
                                moonbit_incref(_M0L7_2adataS144);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS189
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1148);
                                _M0L6_2atmpS1147 = _M0Lm9_2acursorS147;
                                _M0Lm9_2acursorS147 = _M0L6_2atmpS1147 + 1;
                                if (_M0L10next__charS189 < 58) {
                                  if (_M0L10next__charS189 < 48) {
                                    goto join_188;
                                  } else {
                                    _M0L12dispatch__15S171 = 5;
                                    goto loop__label__15_174;
                                  }
                                } else if (_M0L10next__charS189 > 58) {
                                  goto join_188;
                                } else {
                                  _M0L12dispatch__15S171 = 3;
                                  goto loop__label__15_174;
                                }
                                join_188:;
                                _M0L12dispatch__15S171 = 0;
                                goto loop__label__15_174;
                              } else {
                                goto join_176;
                              }
                            }
                          } else if (_M0L10next__charS187 > 58) {
                            goto join_186;
                          } else {
                            _M0L12dispatch__15S171 = 1;
                            goto loop__label__15_174;
                          }
                          join_186:;
                          _M0L12dispatch__15S171 = 0;
                          goto loop__label__15_174;
                        } else {
                          goto join_163;
                        }
                      }
                      join_184:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1151;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0Lm6tag__2S160 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1151 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1151 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1153 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS191;
                      int32_t _M0L6_2atmpS1152;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS191
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1153);
                      _M0L6_2atmpS1152 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1152 + 1;
                      if (_M0L10next__charS191 < 58) {
                        if (_M0L10next__charS191 < 48) {
                          goto join_190;
                        } else {
                          _M0L12dispatch__15S171 = 5;
                          goto loop__label__15_174;
                        }
                      } else if (_M0L10next__charS191 > 58) {
                        goto join_190;
                      } else {
                        _M0L12dispatch__15S171 = 3;
                        goto loop__label__15_174;
                      }
                      join_190:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_176;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1154;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0Lm6tag__2S160 = _M0Lm9_2acursorS147;
                    _M0Lm6tag__3S159 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1154 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1154 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1156 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS193;
                      int32_t _M0L6_2atmpS1155;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS193
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1156);
                      _M0L6_2atmpS1155 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1155 + 1;
                      if (_M0L10next__charS193 < 48) {
                        if (_M0L10next__charS193 == 45) {
                          goto join_172;
                        } else {
                          goto join_192;
                        }
                      } else if (_M0L10next__charS193 > 57) {
                        if (_M0L10next__charS193 < 59) {
                          _M0L12dispatch__15S171 = 3;
                          goto loop__label__15_174;
                        } else {
                          goto join_192;
                        }
                      } else {
                        _M0L12dispatch__15S171 = 6;
                        goto loop__label__15_174;
                      }
                      join_192:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1157;
                    _M0Lm9tag__1__1S157 = _M0Lm6tag__1S156;
                    _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                    _M0L6_2atmpS1157 = _M0Lm9_2acursorS147;
                    if (_M0L6_2atmpS1157 < _M0L6_2aendS146) {
                      int32_t _M0L6_2atmpS1159 = _M0Lm9_2acursorS147;
                      int32_t _M0L10next__charS195;
                      int32_t _M0L6_2atmpS1158;
                      moonbit_incref(_M0L7_2adataS144);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS195
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1159);
                      _M0L6_2atmpS1158 = _M0Lm9_2acursorS147;
                      _M0Lm9_2acursorS147 = _M0L6_2atmpS1158 + 1;
                      if (_M0L10next__charS195 < 58) {
                        if (_M0L10next__charS195 < 48) {
                          goto join_194;
                        } else {
                          _M0L12dispatch__15S171 = 2;
                          goto loop__label__15_174;
                        }
                      } else if (_M0L10next__charS195 > 58) {
                        goto join_194;
                      } else {
                        _M0L12dispatch__15S171 = 1;
                        goto loop__label__15_174;
                      }
                      join_194:;
                      _M0L12dispatch__15S171 = 0;
                      goto loop__label__15_174;
                    } else {
                      goto join_163;
                    }
                    break;
                  }
                  default: {
                    goto join_163;
                    break;
                  }
                }
                join_176:;
                _M0Lm6tag__1S156 = _M0Lm9tag__1__2S158;
                _M0Lm6tag__2S160 = _M0Lm9tag__2__1S161;
                _M0Lm20match__tag__saver__0S150 = _M0Lm6tag__0S155;
                _M0Lm20match__tag__saver__1S151 = _M0Lm6tag__1S156;
                _M0Lm20match__tag__saver__2S152 = _M0Lm6tag__2S160;
                _M0Lm20match__tag__saver__3S153 = _M0Lm6tag__3S159;
                _M0Lm20match__tag__saver__4S154 = _M0Lm6tag__4S162;
                _M0Lm13accept__stateS148 = 0;
                _M0Lm10match__endS149 = _M0Lm9_2acursorS147;
                goto join_163;
                join_172:;
                _M0Lm9tag__1__1S157 = _M0Lm9tag__1__2S158;
                _M0Lm6tag__1S156 = _M0Lm9_2acursorS147;
                _M0Lm6tag__2S160 = _M0Lm9tag__2__1S161;
                _M0L6_2atmpS1127 = _M0Lm9_2acursorS147;
                if (_M0L6_2atmpS1127 < _M0L6_2aendS146) {
                  int32_t _M0L6_2atmpS1129 = _M0Lm9_2acursorS147;
                  int32_t _M0L10next__charS175;
                  int32_t _M0L6_2atmpS1128;
                  moonbit_incref(_M0L7_2adataS144);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS175
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS144, _M0L6_2atmpS1129);
                  _M0L6_2atmpS1128 = _M0Lm9_2acursorS147;
                  _M0Lm9_2acursorS147 = _M0L6_2atmpS1128 + 1;
                  if (_M0L10next__charS175 < 58) {
                    if (_M0L10next__charS175 < 48) {
                      goto join_173;
                    } else {
                      _M0L12dispatch__15S171 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS175 > 58) {
                    goto join_173;
                  } else {
                    _M0L12dispatch__15S171 = 1;
                    continue;
                  }
                  join_173:;
                  _M0L12dispatch__15S171 = 0;
                  continue;
                } else {
                  goto join_163;
                }
                break;
              }
            } else {
              goto join_163;
            }
          } else {
            continue;
          }
        } else {
          goto join_163;
        }
        break;
      }
    } else {
      goto join_163;
    }
  } else {
    goto join_163;
  }
  join_163:;
  switch (_M0Lm13accept__stateS148) {
    case 0: {
      int32_t _M0L6_2atmpS1118 = _M0Lm20match__tag__saver__1S151;
      int32_t _M0L6_2atmpS1117 = _M0L6_2atmpS1118 + 1;
      int64_t _M0L6_2atmpS1114 = (int64_t)_M0L6_2atmpS1117;
      int32_t _M0L6_2atmpS1116 = _M0Lm20match__tag__saver__2S152;
      int64_t _M0L6_2atmpS1115 = (int64_t)_M0L6_2atmpS1116;
      struct _M0TPC16string10StringView _M0L11start__lineS164;
      int32_t _M0L6_2atmpS1113;
      int32_t _M0L6_2atmpS1112;
      int64_t _M0L6_2atmpS1109;
      int32_t _M0L6_2atmpS1111;
      int64_t _M0L6_2atmpS1110;
      struct _M0TPC16string10StringView _M0L13start__columnS165;
      int32_t _M0L6_2atmpS1108;
      int64_t _M0L6_2atmpS1105;
      int32_t _M0L6_2atmpS1107;
      int64_t _M0L6_2atmpS1106;
      struct _M0TPC16string10StringView _M0L3pkgS166;
      int32_t _M0L6_2atmpS1104;
      int32_t _M0L6_2atmpS1103;
      int64_t _M0L6_2atmpS1100;
      int32_t _M0L6_2atmpS1102;
      int64_t _M0L6_2atmpS1101;
      struct _M0TPC16string10StringView _M0L8filenameS167;
      int32_t _M0L6_2atmpS1099;
      int32_t _M0L6_2atmpS1098;
      int64_t _M0L6_2atmpS1095;
      int32_t _M0L6_2atmpS1097;
      int64_t _M0L6_2atmpS1096;
      struct _M0TPC16string10StringView _M0L9end__lineS168;
      int32_t _M0L6_2atmpS1094;
      int32_t _M0L6_2atmpS1093;
      int64_t _M0L6_2atmpS1090;
      int32_t _M0L6_2atmpS1092;
      int64_t _M0L6_2atmpS1091;
      struct _M0TPC16string10StringView _M0L11end__columnS169;
      struct _M0TPB13SourceLocRepr* _block_2379;
      moonbit_incref(_M0L7_2adataS144);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS164
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1114, _M0L6_2atmpS1115);
      _M0L6_2atmpS1113 = _M0Lm20match__tag__saver__2S152;
      _M0L6_2atmpS1112 = _M0L6_2atmpS1113 + 1;
      _M0L6_2atmpS1109 = (int64_t)_M0L6_2atmpS1112;
      _M0L6_2atmpS1111 = _M0Lm20match__tag__saver__3S153;
      _M0L6_2atmpS1110 = (int64_t)_M0L6_2atmpS1111;
      moonbit_incref(_M0L7_2adataS144);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS165
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1109, _M0L6_2atmpS1110);
      _M0L6_2atmpS1108 = _M0L8_2astartS145 + 1;
      _M0L6_2atmpS1105 = (int64_t)_M0L6_2atmpS1108;
      _M0L6_2atmpS1107 = _M0Lm20match__tag__saver__0S150;
      _M0L6_2atmpS1106 = (int64_t)_M0L6_2atmpS1107;
      moonbit_incref(_M0L7_2adataS144);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS166
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1105, _M0L6_2atmpS1106);
      _M0L6_2atmpS1104 = _M0Lm20match__tag__saver__0S150;
      _M0L6_2atmpS1103 = _M0L6_2atmpS1104 + 1;
      _M0L6_2atmpS1100 = (int64_t)_M0L6_2atmpS1103;
      _M0L6_2atmpS1102 = _M0Lm20match__tag__saver__1S151;
      _M0L6_2atmpS1101 = (int64_t)_M0L6_2atmpS1102;
      moonbit_incref(_M0L7_2adataS144);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS167
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1100, _M0L6_2atmpS1101);
      _M0L6_2atmpS1099 = _M0Lm20match__tag__saver__3S153;
      _M0L6_2atmpS1098 = _M0L6_2atmpS1099 + 1;
      _M0L6_2atmpS1095 = (int64_t)_M0L6_2atmpS1098;
      _M0L6_2atmpS1097 = _M0Lm20match__tag__saver__4S154;
      _M0L6_2atmpS1096 = (int64_t)_M0L6_2atmpS1097;
      moonbit_incref(_M0L7_2adataS144);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS168
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1095, _M0L6_2atmpS1096);
      _M0L6_2atmpS1094 = _M0Lm20match__tag__saver__4S154;
      _M0L6_2atmpS1093 = _M0L6_2atmpS1094 + 1;
      _M0L6_2atmpS1090 = (int64_t)_M0L6_2atmpS1093;
      _M0L6_2atmpS1092 = _M0Lm10match__endS149;
      _M0L6_2atmpS1091 = (int64_t)_M0L6_2atmpS1092;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS169
      = _M0MPC16string6String4view(_M0L7_2adataS144, _M0L6_2atmpS1090, _M0L6_2atmpS1091);
      _block_2379
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_2379)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_2379->$0_0 = _M0L3pkgS166.$0;
      _block_2379->$0_1 = _M0L3pkgS166.$1;
      _block_2379->$0_2 = _M0L3pkgS166.$2;
      _block_2379->$1_0 = _M0L8filenameS167.$0;
      _block_2379->$1_1 = _M0L8filenameS167.$1;
      _block_2379->$1_2 = _M0L8filenameS167.$2;
      _block_2379->$2_0 = _M0L11start__lineS164.$0;
      _block_2379->$2_1 = _M0L11start__lineS164.$1;
      _block_2379->$2_2 = _M0L11start__lineS164.$2;
      _block_2379->$3_0 = _M0L13start__columnS165.$0;
      _block_2379->$3_1 = _M0L13start__columnS165.$1;
      _block_2379->$3_2 = _M0L13start__columnS165.$2;
      _block_2379->$4_0 = _M0L9end__lineS168.$0;
      _block_2379->$4_1 = _M0L9end__lineS168.$1;
      _block_2379->$4_2 = _M0L9end__lineS168.$2;
      _block_2379->$5_0 = _M0L11end__columnS169.$0;
      _block_2379->$5_1 = _M0L11end__columnS169.$1;
      _block_2379->$5_2 = _M0L11end__columnS169.$2;
      return _block_2379;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS144);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS140,
  int32_t _M0L5indexS141
) {
  int32_t _M0L3lenS139;
  int32_t _if__result_2380;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS139 = _M0L4selfS140->$1;
  if (_M0L5indexS141 >= 0) {
    _if__result_2380 = _M0L5indexS141 < _M0L3lenS139;
  } else {
    _if__result_2380 = 0;
  }
  if (_if__result_2380) {
    moonbit_string_t* _M0L6_2atmpS1089;
    moonbit_string_t _M0L6_2atmpS2148;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1089 = _M0MPC15array5Array6bufferGsE(_M0L4selfS140);
    if (
      _M0L5indexS141 < 0
      || _M0L5indexS141 >= Moonbit_array_length(_M0L6_2atmpS1089)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2148 = (moonbit_string_t)_M0L6_2atmpS1089[_M0L5indexS141];
    moonbit_incref(_M0L6_2atmpS2148);
    moonbit_decref(_M0L6_2atmpS1089);
    return _M0L6_2atmpS2148;
  } else {
    moonbit_decref(_M0L4selfS140);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS137
) {
  moonbit_string_t* _M0L8_2afieldS2149;
  int32_t _M0L6_2acntS2260;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS2149 = _M0L4selfS137->$0;
  _M0L6_2acntS2260 = Moonbit_object_header(_M0L4selfS137)->rc;
  if (_M0L6_2acntS2260 > 1) {
    int32_t _M0L11_2anew__cntS2261 = _M0L6_2acntS2260 - 1;
    Moonbit_object_header(_M0L4selfS137)->rc = _M0L11_2anew__cntS2261;
    moonbit_incref(_M0L8_2afieldS2149);
  } else if (_M0L6_2acntS2260 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS137);
  }
  return _M0L8_2afieldS2149;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS138
) {
  struct _M0TUsiE** _M0L8_2afieldS2150;
  int32_t _M0L6_2acntS2262;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS2150 = _M0L4selfS138->$0;
  _M0L6_2acntS2262 = Moonbit_object_header(_M0L4selfS138)->rc;
  if (_M0L6_2acntS2262 > 1) {
    int32_t _M0L11_2anew__cntS2263 = _M0L6_2acntS2262 - 1;
    Moonbit_object_header(_M0L4selfS138)->rc = _M0L11_2anew__cntS2263;
    moonbit_incref(_M0L8_2afieldS2150);
  } else if (_M0L6_2acntS2262 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS138);
  }
  return _M0L8_2afieldS2150;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS136) {
  struct _M0TPB13StringBuilder* _M0L3bufS135;
  struct _M0TPB6Logger _M0L6_2atmpS1088;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS135 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS135);
  _M0L6_2atmpS1088
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS135
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS136, _M0L6_2atmpS1088);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS135);
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS129
) {
  int32_t _M0L17codepoint__lengthS128;
  int32_t _M0L6_2atmpS1087;
  moonbit_bytes_t _M0L4dataS130;
  int32_t _M0L1iS131;
  int32_t _M0L12utf16__indexS132;
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_incref(_M0L1sS129);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L17codepoint__lengthS128
  = _M0MPC16string6String20char__length_2einner(_M0L1sS129, 0, 4294967296ll);
  _M0L6_2atmpS1087 = _M0L17codepoint__lengthS128 * 4;
  _M0L4dataS130 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1087, 0);
  _M0L1iS131 = 0;
  _M0L12utf16__indexS132 = 0;
  while (1) {
    if (_M0L1iS131 < _M0L17codepoint__lengthS128) {
      int32_t _M0L6_2atmpS1084;
      int32_t _M0L1cS133;
      int32_t _M0L6_2atmpS1085;
      int32_t _M0L6_2atmpS1086;
      moonbit_incref(_M0L1sS129);
      #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1084
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS129, _M0L12utf16__indexS132);
      _M0L1cS133 = _M0L6_2atmpS1084;
      if (_M0L1cS133 > 65535) {
        int32_t _M0L6_2atmpS1052 = _M0L1iS131 * 4;
        int32_t _M0L6_2atmpS1054 = _M0L1cS133 & 255;
        int32_t _M0L6_2atmpS1053 = _M0L6_2atmpS1054 & 0xff;
        int32_t _M0L6_2atmpS1059;
        int32_t _M0L6_2atmpS1055;
        int32_t _M0L6_2atmpS1058;
        int32_t _M0L6_2atmpS1057;
        int32_t _M0L6_2atmpS1056;
        int32_t _M0L6_2atmpS1064;
        int32_t _M0L6_2atmpS1060;
        int32_t _M0L6_2atmpS1063;
        int32_t _M0L6_2atmpS1062;
        int32_t _M0L6_2atmpS1061;
        int32_t _M0L6_2atmpS1069;
        int32_t _M0L6_2atmpS1065;
        int32_t _M0L6_2atmpS1068;
        int32_t _M0L6_2atmpS1067;
        int32_t _M0L6_2atmpS1066;
        int32_t _M0L6_2atmpS1070;
        int32_t _M0L6_2atmpS1071;
        if (
          _M0L6_2atmpS1052 < 0
          || _M0L6_2atmpS1052 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1052] = _M0L6_2atmpS1053;
        _M0L6_2atmpS1059 = _M0L1iS131 * 4;
        _M0L6_2atmpS1055 = _M0L6_2atmpS1059 + 1;
        _M0L6_2atmpS1058 = _M0L1cS133 >> 8;
        _M0L6_2atmpS1057 = _M0L6_2atmpS1058 & 255;
        _M0L6_2atmpS1056 = _M0L6_2atmpS1057 & 0xff;
        if (
          _M0L6_2atmpS1055 < 0
          || _M0L6_2atmpS1055 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1055] = _M0L6_2atmpS1056;
        _M0L6_2atmpS1064 = _M0L1iS131 * 4;
        _M0L6_2atmpS1060 = _M0L6_2atmpS1064 + 2;
        _M0L6_2atmpS1063 = _M0L1cS133 >> 16;
        _M0L6_2atmpS1062 = _M0L6_2atmpS1063 & 255;
        _M0L6_2atmpS1061 = _M0L6_2atmpS1062 & 0xff;
        if (
          _M0L6_2atmpS1060 < 0
          || _M0L6_2atmpS1060 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1060] = _M0L6_2atmpS1061;
        _M0L6_2atmpS1069 = _M0L1iS131 * 4;
        _M0L6_2atmpS1065 = _M0L6_2atmpS1069 + 3;
        _M0L6_2atmpS1068 = _M0L1cS133 >> 24;
        _M0L6_2atmpS1067 = _M0L6_2atmpS1068 & 255;
        _M0L6_2atmpS1066 = _M0L6_2atmpS1067 & 0xff;
        if (
          _M0L6_2atmpS1065 < 0
          || _M0L6_2atmpS1065 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 114 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1065] = _M0L6_2atmpS1066;
        _M0L6_2atmpS1070 = _M0L1iS131 + 1;
        _M0L6_2atmpS1071 = _M0L12utf16__indexS132 + 2;
        _M0L1iS131 = _M0L6_2atmpS1070;
        _M0L12utf16__indexS132 = _M0L6_2atmpS1071;
        continue;
      } else {
        int32_t _M0L6_2atmpS1072 = _M0L1iS131 * 4;
        int32_t _M0L6_2atmpS1074 = _M0L1cS133 & 255;
        int32_t _M0L6_2atmpS1073 = _M0L6_2atmpS1074 & 0xff;
        int32_t _M0L6_2atmpS1079;
        int32_t _M0L6_2atmpS1075;
        int32_t _M0L6_2atmpS1078;
        int32_t _M0L6_2atmpS1077;
        int32_t _M0L6_2atmpS1076;
        int32_t _M0L6_2atmpS1081;
        int32_t _M0L6_2atmpS1080;
        int32_t _M0L6_2atmpS1083;
        int32_t _M0L6_2atmpS1082;
        if (
          _M0L6_2atmpS1072 < 0
          || _M0L6_2atmpS1072 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 117 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1072] = _M0L6_2atmpS1073;
        _M0L6_2atmpS1079 = _M0L1iS131 * 4;
        _M0L6_2atmpS1075 = _M0L6_2atmpS1079 + 1;
        _M0L6_2atmpS1078 = _M0L1cS133 >> 8;
        _M0L6_2atmpS1077 = _M0L6_2atmpS1078 & 255;
        _M0L6_2atmpS1076 = _M0L6_2atmpS1077 & 0xff;
        if (
          _M0L6_2atmpS1075 < 0
          || _M0L6_2atmpS1075 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1075] = _M0L6_2atmpS1076;
        _M0L6_2atmpS1081 = _M0L1iS131 * 4;
        _M0L6_2atmpS1080 = _M0L6_2atmpS1081 + 2;
        if (
          _M0L6_2atmpS1080 < 0
          || _M0L6_2atmpS1080 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1080] = 0;
        _M0L6_2atmpS1083 = _M0L1iS131 * 4;
        _M0L6_2atmpS1082 = _M0L6_2atmpS1083 + 3;
        if (
          _M0L6_2atmpS1082 < 0
          || _M0L6_2atmpS1082 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1082] = 0;
      }
      _M0L6_2atmpS1085 = _M0L1iS131 + 1;
      _M0L6_2atmpS1086 = _M0L12utf16__indexS132 + 1;
      _M0L1iS131 = _M0L6_2atmpS1085;
      _M0L12utf16__indexS132 = _M0L6_2atmpS1086;
      continue;
    } else {
      moonbit_decref(_M0L1sS129);
    }
    break;
  }
  #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  return _M0FPB14base64__encode(_M0L4dataS130);
}

int32_t _M0MPC16string6String16unsafe__char__at(
  moonbit_string_t _M0L4selfS125,
  int32_t _M0L5indexS126
) {
  int32_t _M0L2c1S124;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  _M0L2c1S124 = _M0L4selfS125[_M0L5indexS126];
  #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
  if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S124)) {
    int32_t _M0L6_2atmpS1051 = _M0L5indexS126 + 1;
    int32_t _M0L6_2atmpS2151 = _M0L4selfS125[_M0L6_2atmpS1051];
    int32_t _M0L2c2S127;
    int32_t _M0L6_2atmpS1049;
    int32_t _M0L6_2atmpS1050;
    moonbit_decref(_M0L4selfS125);
    _M0L2c2S127 = _M0L6_2atmpS2151;
    _M0L6_2atmpS1049 = (int32_t)_M0L2c1S124;
    _M0L6_2atmpS1050 = (int32_t)_M0L2c2S127;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1049, _M0L6_2atmpS1050);
  } else {
    moonbit_decref(_M0L4selfS125);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S124);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS123) {
  int32_t _M0L6_2atmpS1048;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1048 = (int32_t)_M0L4selfS123;
  return _M0L6_2atmpS1048;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS121,
  int32_t _M0L8trailingS122
) {
  int32_t _M0L6_2atmpS1047;
  int32_t _M0L6_2atmpS1046;
  int32_t _M0L6_2atmpS1045;
  int32_t _M0L6_2atmpS1044;
  int32_t _M0L6_2atmpS1043;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1047 = _M0L7leadingS121 - 55296;
  _M0L6_2atmpS1046 = _M0L6_2atmpS1047 * 1024;
  _M0L6_2atmpS1045 = _M0L6_2atmpS1046 + _M0L8trailingS122;
  _M0L6_2atmpS1044 = _M0L6_2atmpS1045 - 56320;
  _M0L6_2atmpS1043 = _M0L6_2atmpS1044 + 65536;
  return _M0L6_2atmpS1043;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS114,
  int32_t _M0L13start__offsetS115,
  int64_t _M0L11end__offsetS112
) {
  int32_t _M0L11end__offsetS111;
  int32_t _if__result_2382;
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS112 == 4294967296ll) {
    _M0L11end__offsetS111 = Moonbit_array_length(_M0L4selfS114);
  } else {
    int64_t _M0L7_2aSomeS113 = _M0L11end__offsetS112;
    _M0L11end__offsetS111 = (int32_t)_M0L7_2aSomeS113;
  }
  if (_M0L13start__offsetS115 >= 0) {
    if (_M0L13start__offsetS115 <= _M0L11end__offsetS111) {
      int32_t _M0L6_2atmpS1036 = Moonbit_array_length(_M0L4selfS114);
      _if__result_2382 = _M0L11end__offsetS111 <= _M0L6_2atmpS1036;
    } else {
      _if__result_2382 = 0;
    }
  } else {
    _if__result_2382 = 0;
  }
  if (_if__result_2382) {
    int32_t _M0L12utf16__indexS116 = _M0L13start__offsetS115;
    int32_t _M0L11char__countS117 = 0;
    while (1) {
      if (_M0L12utf16__indexS116 < _M0L11end__offsetS111) {
        int32_t _M0L2c1S118 = _M0L4selfS114[_M0L12utf16__indexS116];
        int32_t _if__result_2384;
        int32_t _M0L6_2atmpS1041;
        int32_t _M0L6_2atmpS1042;
        #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S118)) {
          int32_t _M0L6_2atmpS1037 = _M0L12utf16__indexS116 + 1;
          _if__result_2384 = _M0L6_2atmpS1037 < _M0L11end__offsetS111;
        } else {
          _if__result_2384 = 0;
        }
        if (_if__result_2384) {
          int32_t _M0L6_2atmpS1040 = _M0L12utf16__indexS116 + 1;
          int32_t _M0L2c2S119 = _M0L4selfS114[_M0L6_2atmpS1040];
          #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S119)) {
            int32_t _M0L6_2atmpS1038 = _M0L12utf16__indexS116 + 2;
            int32_t _M0L6_2atmpS1039 = _M0L11char__countS117 + 1;
            _M0L12utf16__indexS116 = _M0L6_2atmpS1038;
            _M0L11char__countS117 = _M0L6_2atmpS1039;
            continue;
          } else {
            #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
            _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_28.data, (moonbit_string_t)moonbit_string_literal_56.data);
          }
        }
        _M0L6_2atmpS1041 = _M0L12utf16__indexS116 + 1;
        _M0L6_2atmpS1042 = _M0L11char__countS117 + 1;
        _M0L12utf16__indexS116 = _M0L6_2atmpS1041;
        _M0L11char__countS117 = _M0L6_2atmpS1042;
        continue;
      } else {
        moonbit_decref(_M0L4selfS114);
        return _M0L11char__countS117;
      }
      break;
    }
  } else {
    moonbit_decref(_M0L4selfS114);
    #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_57.data, (moonbit_string_t)moonbit_string_literal_58.data);
  }
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS110) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS110 >= 56320) {
    return _M0L4selfS110 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS109) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS109 >= 55296) {
    return _M0L4selfS109 <= 56319;
  } else {
    return 0;
  }
}

moonbit_string_t _M0FPB14base64__encode(moonbit_bytes_t _M0L4dataS90) {
  struct _M0TPB13StringBuilder* _M0L3bufS88;
  int32_t _M0L3lenS89;
  int32_t _M0L3remS91;
  int32_t _M0L1iS92;
  #line 61 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L3bufS88 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS89 = Moonbit_array_length(_M0L4dataS90);
  _M0L3remS91 = _M0L3lenS89 % 3;
  _M0L1iS92 = 0;
  while (1) {
    int32_t _M0L6_2atmpS988 = _M0L3lenS89 - _M0L3remS91;
    if (_M0L1iS92 < _M0L6_2atmpS988) {
      int32_t _M0L6_2atmpS1010;
      int32_t _M0L2b0S93;
      int32_t _M0L6_2atmpS1009;
      int32_t _M0L6_2atmpS1008;
      int32_t _M0L2b1S94;
      int32_t _M0L6_2atmpS1007;
      int32_t _M0L6_2atmpS1006;
      int32_t _M0L2b2S95;
      int32_t _M0L6_2atmpS1005;
      int32_t _M0L6_2atmpS1004;
      int32_t _M0L2x0S96;
      int32_t _M0L6_2atmpS1003;
      int32_t _M0L6_2atmpS1000;
      int32_t _M0L6_2atmpS1002;
      int32_t _M0L6_2atmpS1001;
      int32_t _M0L6_2atmpS999;
      int32_t _M0L2x1S97;
      int32_t _M0L6_2atmpS998;
      int32_t _M0L6_2atmpS995;
      int32_t _M0L6_2atmpS997;
      int32_t _M0L6_2atmpS996;
      int32_t _M0L6_2atmpS994;
      int32_t _M0L2x2S98;
      int32_t _M0L6_2atmpS993;
      int32_t _M0L2x3S99;
      int32_t _M0L6_2atmpS989;
      int32_t _M0L6_2atmpS990;
      int32_t _M0L6_2atmpS991;
      int32_t _M0L6_2atmpS992;
      int32_t _M0L6_2atmpS1011;
      if (_M0L1iS92 < 0 || _M0L1iS92 >= Moonbit_array_length(_M0L4dataS90)) {
        #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1010 = (int32_t)_M0L4dataS90[_M0L1iS92];
      _M0L2b0S93 = (int32_t)_M0L6_2atmpS1010;
      _M0L6_2atmpS1009 = _M0L1iS92 + 1;
      if (
        _M0L6_2atmpS1009 < 0
        || _M0L6_2atmpS1009 >= Moonbit_array_length(_M0L4dataS90)
      ) {
        #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1008 = (int32_t)_M0L4dataS90[_M0L6_2atmpS1009];
      _M0L2b1S94 = (int32_t)_M0L6_2atmpS1008;
      _M0L6_2atmpS1007 = _M0L1iS92 + 2;
      if (
        _M0L6_2atmpS1007 < 0
        || _M0L6_2atmpS1007 >= Moonbit_array_length(_M0L4dataS90)
      ) {
        #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1006 = (int32_t)_M0L4dataS90[_M0L6_2atmpS1007];
      _M0L2b2S95 = (int32_t)_M0L6_2atmpS1006;
      _M0L6_2atmpS1005 = _M0L2b0S93 & 252;
      _M0L6_2atmpS1004 = _M0L6_2atmpS1005 >> 2;
      if (
        _M0L6_2atmpS1004 < 0
        || _M0L6_2atmpS1004
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x0S96 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1004];
      _M0L6_2atmpS1003 = _M0L2b0S93 & 3;
      _M0L6_2atmpS1000 = _M0L6_2atmpS1003 << 4;
      _M0L6_2atmpS1002 = _M0L2b1S94 & 240;
      _M0L6_2atmpS1001 = _M0L6_2atmpS1002 >> 4;
      _M0L6_2atmpS999 = _M0L6_2atmpS1000 | _M0L6_2atmpS1001;
      if (
        _M0L6_2atmpS999 < 0
        || _M0L6_2atmpS999
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x1S97 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS999];
      _M0L6_2atmpS998 = _M0L2b1S94 & 15;
      _M0L6_2atmpS995 = _M0L6_2atmpS998 << 2;
      _M0L6_2atmpS997 = _M0L2b2S95 & 192;
      _M0L6_2atmpS996 = _M0L6_2atmpS997 >> 6;
      _M0L6_2atmpS994 = _M0L6_2atmpS995 | _M0L6_2atmpS996;
      if (
        _M0L6_2atmpS994 < 0
        || _M0L6_2atmpS994
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x2S98 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS994];
      _M0L6_2atmpS993 = _M0L2b2S95 & 63;
      if (
        _M0L6_2atmpS993 < 0
        || _M0L6_2atmpS993
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x3S99 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS993];
      #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS989 = _M0MPC14byte4Byte8to__char(_M0L2x0S96);
      moonbit_incref(_M0L3bufS88);
      #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS989);
      #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS990 = _M0MPC14byte4Byte8to__char(_M0L2x1S97);
      moonbit_incref(_M0L3bufS88);
      #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS990);
      #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS991 = _M0MPC14byte4Byte8to__char(_M0L2x2S98);
      moonbit_incref(_M0L3bufS88);
      #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS991);
      #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS992 = _M0MPC14byte4Byte8to__char(_M0L2x3S99);
      moonbit_incref(_M0L3bufS88);
      #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS992);
      _M0L6_2atmpS1011 = _M0L1iS92 + 3;
      _M0L1iS92 = _M0L6_2atmpS1011;
      continue;
    }
    break;
  }
  if (_M0L3remS91 == 1) {
    int32_t _M0L6_2atmpS1019 = _M0L3lenS89 - 1;
    int32_t _M0L6_2atmpS2152;
    int32_t _M0L6_2atmpS1018;
    int32_t _M0L2b0S101;
    int32_t _M0L6_2atmpS1017;
    int32_t _M0L6_2atmpS1016;
    int32_t _M0L2x0S102;
    int32_t _M0L6_2atmpS1015;
    int32_t _M0L6_2atmpS1014;
    int32_t _M0L2x1S103;
    int32_t _M0L6_2atmpS1012;
    int32_t _M0L6_2atmpS1013;
    if (
      _M0L6_2atmpS1019 < 0
      || _M0L6_2atmpS1019 >= Moonbit_array_length(_M0L4dataS90)
    ) {
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2152 = (int32_t)_M0L4dataS90[_M0L6_2atmpS1019];
    moonbit_decref(_M0L4dataS90);
    _M0L6_2atmpS1018 = _M0L6_2atmpS2152;
    _M0L2b0S101 = (int32_t)_M0L6_2atmpS1018;
    _M0L6_2atmpS1017 = _M0L2b0S101 & 252;
    _M0L6_2atmpS1016 = _M0L6_2atmpS1017 >> 2;
    if (
      _M0L6_2atmpS1016 < 0
      || _M0L6_2atmpS1016
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x0S102 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1016];
    _M0L6_2atmpS1015 = _M0L2b0S101 & 3;
    _M0L6_2atmpS1014 = _M0L6_2atmpS1015 << 4;
    if (
      _M0L6_2atmpS1014 < 0
      || _M0L6_2atmpS1014
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x1S103 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1014];
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1012 = _M0MPC14byte4Byte8to__char(_M0L2x0S102);
    moonbit_incref(_M0L3bufS88);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1012);
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1013 = _M0MPC14byte4Byte8to__char(_M0L2x1S103);
    moonbit_incref(_M0L3bufS88);
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1013);
    moonbit_incref(_M0L3bufS88);
    #line 85 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, 61);
    moonbit_incref(_M0L3bufS88);
    #line 86 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, 61);
  } else if (_M0L3remS91 == 2) {
    int32_t _M0L6_2atmpS1035 = _M0L3lenS89 - 2;
    int32_t _M0L6_2atmpS1034;
    int32_t _M0L2b0S104;
    int32_t _M0L6_2atmpS1033;
    int32_t _M0L6_2atmpS2153;
    int32_t _M0L6_2atmpS1032;
    int32_t _M0L2b1S105;
    int32_t _M0L6_2atmpS1031;
    int32_t _M0L6_2atmpS1030;
    int32_t _M0L2x0S106;
    int32_t _M0L6_2atmpS1029;
    int32_t _M0L6_2atmpS1026;
    int32_t _M0L6_2atmpS1028;
    int32_t _M0L6_2atmpS1027;
    int32_t _M0L6_2atmpS1025;
    int32_t _M0L2x1S107;
    int32_t _M0L6_2atmpS1024;
    int32_t _M0L6_2atmpS1023;
    int32_t _M0L2x2S108;
    int32_t _M0L6_2atmpS1020;
    int32_t _M0L6_2atmpS1021;
    int32_t _M0L6_2atmpS1022;
    if (
      _M0L6_2atmpS1035 < 0
      || _M0L6_2atmpS1035 >= Moonbit_array_length(_M0L4dataS90)
    ) {
      #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1034 = (int32_t)_M0L4dataS90[_M0L6_2atmpS1035];
    _M0L2b0S104 = (int32_t)_M0L6_2atmpS1034;
    _M0L6_2atmpS1033 = _M0L3lenS89 - 1;
    if (
      _M0L6_2atmpS1033 < 0
      || _M0L6_2atmpS1033 >= Moonbit_array_length(_M0L4dataS90)
    ) {
      #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2153 = (int32_t)_M0L4dataS90[_M0L6_2atmpS1033];
    moonbit_decref(_M0L4dataS90);
    _M0L6_2atmpS1032 = _M0L6_2atmpS2153;
    _M0L2b1S105 = (int32_t)_M0L6_2atmpS1032;
    _M0L6_2atmpS1031 = _M0L2b0S104 & 252;
    _M0L6_2atmpS1030 = _M0L6_2atmpS1031 >> 2;
    if (
      _M0L6_2atmpS1030 < 0
      || _M0L6_2atmpS1030
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x0S106 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1030];
    _M0L6_2atmpS1029 = _M0L2b0S104 & 3;
    _M0L6_2atmpS1026 = _M0L6_2atmpS1029 << 4;
    _M0L6_2atmpS1028 = _M0L2b1S105 & 240;
    _M0L6_2atmpS1027 = _M0L6_2atmpS1028 >> 4;
    _M0L6_2atmpS1025 = _M0L6_2atmpS1026 | _M0L6_2atmpS1027;
    if (
      _M0L6_2atmpS1025 < 0
      || _M0L6_2atmpS1025
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x1S107 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1025];
    _M0L6_2atmpS1024 = _M0L2b1S105 & 15;
    _M0L6_2atmpS1023 = _M0L6_2atmpS1024 << 2;
    if (
      _M0L6_2atmpS1023 < 0
      || _M0L6_2atmpS1023
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x2S108 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1023];
    #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1020 = _M0MPC14byte4Byte8to__char(_M0L2x0S106);
    moonbit_incref(_M0L3bufS88);
    #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1020);
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1021 = _M0MPC14byte4Byte8to__char(_M0L2x1S107);
    moonbit_incref(_M0L3bufS88);
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1021);
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1022 = _M0MPC14byte4Byte8to__char(_M0L2x2S108);
    moonbit_incref(_M0L3bufS88);
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1022);
    moonbit_incref(_M0L3bufS88);
    #line 96 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, 61);
  } else {
    moonbit_decref(_M0L4dataS90);
  }
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS88);
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS85,
  int32_t _M0L2chS87
) {
  int32_t _M0L3lenS983;
  int32_t _M0L6_2atmpS982;
  moonbit_bytes_t _M0L8_2afieldS2154;
  moonbit_bytes_t _M0L4dataS986;
  int32_t _M0L3lenS987;
  int32_t _M0L3incS86;
  int32_t _M0L3lenS985;
  int32_t _M0L6_2atmpS984;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS983 = _M0L4selfS85->$1;
  _M0L6_2atmpS982 = _M0L3lenS983 + 4;
  moonbit_incref(_M0L4selfS85);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS85, _M0L6_2atmpS982);
  _M0L8_2afieldS2154 = _M0L4selfS85->$0;
  _M0L4dataS986 = _M0L8_2afieldS2154;
  _M0L3lenS987 = _M0L4selfS85->$1;
  moonbit_incref(_M0L4dataS986);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS86
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS986, _M0L3lenS987, _M0L2chS87);
  _M0L3lenS985 = _M0L4selfS85->$1;
  _M0L6_2atmpS984 = _M0L3lenS985 + _M0L3incS86;
  _M0L4selfS85->$1 = _M0L6_2atmpS984;
  moonbit_decref(_M0L4selfS85);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS80,
  int32_t _M0L8requiredS81
) {
  moonbit_bytes_t _M0L8_2afieldS2158;
  moonbit_bytes_t _M0L4dataS981;
  int32_t _M0L6_2atmpS2157;
  int32_t _M0L12current__lenS79;
  int32_t _M0Lm13enough__spaceS82;
  int32_t _M0L6_2atmpS979;
  int32_t _M0L6_2atmpS980;
  moonbit_bytes_t _M0L9new__dataS84;
  moonbit_bytes_t _M0L8_2afieldS2156;
  moonbit_bytes_t _M0L4dataS977;
  int32_t _M0L3lenS978;
  moonbit_bytes_t _M0L6_2aoldS2155;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS2158 = _M0L4selfS80->$0;
  _M0L4dataS981 = _M0L8_2afieldS2158;
  _M0L6_2atmpS2157 = Moonbit_array_length(_M0L4dataS981);
  _M0L12current__lenS79 = _M0L6_2atmpS2157;
  if (_M0L8requiredS81 <= _M0L12current__lenS79) {
    moonbit_decref(_M0L4selfS80);
    return 0;
  }
  _M0Lm13enough__spaceS82 = _M0L12current__lenS79;
  while (1) {
    int32_t _M0L6_2atmpS975 = _M0Lm13enough__spaceS82;
    if (_M0L6_2atmpS975 < _M0L8requiredS81) {
      int32_t _M0L6_2atmpS976 = _M0Lm13enough__spaceS82;
      _M0Lm13enough__spaceS82 = _M0L6_2atmpS976 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS979 = _M0Lm13enough__spaceS82;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS980 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS84
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS979, _M0L6_2atmpS980);
  _M0L8_2afieldS2156 = _M0L4selfS80->$0;
  _M0L4dataS977 = _M0L8_2afieldS2156;
  _M0L3lenS978 = _M0L4selfS80->$1;
  moonbit_incref(_M0L4dataS977);
  moonbit_incref(_M0L9new__dataS84);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS84, 0, _M0L4dataS977, 0, _M0L3lenS978);
  _M0L6_2aoldS2155 = _M0L4selfS80->$0;
  moonbit_decref(_M0L6_2aoldS2155);
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
    uint32_t _M0L6_2atmpS958 = _M0L4codeS72 & 255u;
    int32_t _M0L6_2atmpS957;
    int32_t _M0L6_2atmpS959;
    uint32_t _M0L6_2atmpS961;
    int32_t _M0L6_2atmpS960;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS957 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS958);
    if (
      _M0L6offsetS75 < 0
      || _M0L6offsetS75 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6offsetS75] = _M0L6_2atmpS957;
    _M0L6_2atmpS959 = _M0L6offsetS75 + 1;
    _M0L6_2atmpS961 = _M0L4codeS72 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS960 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS961);
    if (
      _M0L6_2atmpS959 < 0
      || _M0L6_2atmpS959 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS959] = _M0L6_2atmpS960;
    moonbit_decref(_M0L4selfS74);
    return 2;
  } else if (_M0L4codeS72 < 1114112u) {
    uint32_t _M0L2hiS76 = _M0L4codeS72 - 65536u;
    uint32_t _M0L6_2atmpS974 = _M0L2hiS76 >> 10;
    uint32_t _M0L2loS77 = _M0L6_2atmpS974 | 55296u;
    uint32_t _M0L6_2atmpS973 = _M0L2hiS76 & 1023u;
    uint32_t _M0L2hiS78 = _M0L6_2atmpS973 | 56320u;
    uint32_t _M0L6_2atmpS963 = _M0L2loS77 & 255u;
    int32_t _M0L6_2atmpS962;
    int32_t _M0L6_2atmpS964;
    uint32_t _M0L6_2atmpS966;
    int32_t _M0L6_2atmpS965;
    int32_t _M0L6_2atmpS967;
    uint32_t _M0L6_2atmpS969;
    int32_t _M0L6_2atmpS968;
    int32_t _M0L6_2atmpS970;
    uint32_t _M0L6_2atmpS972;
    int32_t _M0L6_2atmpS971;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS962 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS963);
    if (
      _M0L6offsetS75 < 0
      || _M0L6offsetS75 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6offsetS75] = _M0L6_2atmpS962;
    _M0L6_2atmpS964 = _M0L6offsetS75 + 1;
    _M0L6_2atmpS966 = _M0L2loS77 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS965 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS966);
    if (
      _M0L6_2atmpS964 < 0
      || _M0L6_2atmpS964 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS964] = _M0L6_2atmpS965;
    _M0L6_2atmpS967 = _M0L6offsetS75 + 2;
    _M0L6_2atmpS969 = _M0L2hiS78 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS968 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS969);
    if (
      _M0L6_2atmpS967 < 0
      || _M0L6_2atmpS967 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS967] = _M0L6_2atmpS968;
    _M0L6_2atmpS970 = _M0L6offsetS75 + 3;
    _M0L6_2atmpS972 = _M0L2hiS78 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS971 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS972);
    if (
      _M0L6_2atmpS970 < 0
      || _M0L6_2atmpS970 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS970] = _M0L6_2atmpS971;
    moonbit_decref(_M0L4selfS74);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS74);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_59.data, (moonbit_string_t)moonbit_string_literal_60.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS71) {
  int32_t _M0L6_2atmpS956;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS956 = *(int32_t*)&_M0L4selfS71;
  return _M0L6_2atmpS956 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS70) {
  int32_t _M0L6_2atmpS955;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS955 = _M0L4selfS70;
  return *(uint32_t*)&_M0L6_2atmpS955;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS69
) {
  moonbit_bytes_t _M0L8_2afieldS2160;
  moonbit_bytes_t _M0L4dataS954;
  moonbit_bytes_t _M0L6_2atmpS951;
  int32_t _M0L8_2afieldS2159;
  int32_t _M0L3lenS953;
  int64_t _M0L6_2atmpS952;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS2160 = _M0L4selfS69->$0;
  _M0L4dataS954 = _M0L8_2afieldS2160;
  moonbit_incref(_M0L4dataS954);
  _M0L6_2atmpS951 = _M0L4dataS954;
  _M0L8_2afieldS2159 = _M0L4selfS69->$1;
  moonbit_decref(_M0L4selfS69);
  _M0L3lenS953 = _M0L8_2afieldS2159;
  _M0L6_2atmpS952 = (int64_t)_M0L3lenS953;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS951, 0, _M0L6_2atmpS952);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS64,
  int32_t _M0L6offsetS68,
  int64_t _M0L6lengthS66
) {
  int32_t _M0L3lenS63;
  int32_t _M0L6lengthS65;
  int32_t _if__result_2387;
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
      int32_t _M0L6_2atmpS950 = _M0L6offsetS68 + _M0L6lengthS65;
      _if__result_2387 = _M0L6_2atmpS950 <= _M0L3lenS63;
    } else {
      _if__result_2387 = 0;
    }
  } else {
    _if__result_2387 = 0;
  }
  if (_if__result_2387) {
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
  struct _M0TPB13StringBuilder* _block_2388;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS61 < 1) {
    _M0L7initialS60 = 1;
  } else {
    _M0L7initialS60 = _M0L10size__hintS61;
  }
  _M0L4dataS62 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS60, 0);
  _block_2388
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_2388)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_2388->$0 = _M0L4dataS62;
  _block_2388->$1 = 0;
  return _block_2388;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS59) {
  int32_t _M0L6_2atmpS949;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS949 = (int32_t)_M0L4selfS59;
  return _M0L6_2atmpS949;
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
  int32_t _if__result_2389;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS22 == _M0L3srcS23) {
    _if__result_2389 = _M0L11dst__offsetS24 < _M0L11src__offsetS25;
  } else {
    _if__result_2389 = 0;
  }
  if (_if__result_2389) {
    int32_t _M0L1iS26 = 0;
    while (1) {
      if (_M0L1iS26 < _M0L3lenS27) {
        int32_t _M0L6_2atmpS922 = _M0L11dst__offsetS24 + _M0L1iS26;
        int32_t _M0L6_2atmpS924 = _M0L11src__offsetS25 + _M0L1iS26;
        int32_t _M0L6_2atmpS923;
        int32_t _M0L6_2atmpS925;
        if (
          _M0L6_2atmpS924 < 0
          || _M0L6_2atmpS924 >= Moonbit_array_length(_M0L3srcS23)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS923 = (int32_t)_M0L3srcS23[_M0L6_2atmpS924];
        if (
          _M0L6_2atmpS922 < 0
          || _M0L6_2atmpS922 >= Moonbit_array_length(_M0L3dstS22)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS22[_M0L6_2atmpS922] = _M0L6_2atmpS923;
        _M0L6_2atmpS925 = _M0L1iS26 + 1;
        _M0L1iS26 = _M0L6_2atmpS925;
        continue;
      } else {
        moonbit_decref(_M0L3srcS23);
        moonbit_decref(_M0L3dstS22);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS930 = _M0L3lenS27 - 1;
    int32_t _M0L1iS29 = _M0L6_2atmpS930;
    while (1) {
      if (_M0L1iS29 >= 0) {
        int32_t _M0L6_2atmpS926 = _M0L11dst__offsetS24 + _M0L1iS29;
        int32_t _M0L6_2atmpS928 = _M0L11src__offsetS25 + _M0L1iS29;
        int32_t _M0L6_2atmpS927;
        int32_t _M0L6_2atmpS929;
        if (
          _M0L6_2atmpS928 < 0
          || _M0L6_2atmpS928 >= Moonbit_array_length(_M0L3srcS23)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS927 = (int32_t)_M0L3srcS23[_M0L6_2atmpS928];
        if (
          _M0L6_2atmpS926 < 0
          || _M0L6_2atmpS926 >= Moonbit_array_length(_M0L3dstS22)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS22[_M0L6_2atmpS926] = _M0L6_2atmpS927;
        _M0L6_2atmpS929 = _M0L1iS29 - 1;
        _M0L1iS29 = _M0L6_2atmpS929;
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
  int32_t _if__result_2392;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS31 == _M0L3srcS32) {
    _if__result_2392 = _M0L11dst__offsetS33 < _M0L11src__offsetS34;
  } else {
    _if__result_2392 = 0;
  }
  if (_if__result_2392) {
    int32_t _M0L1iS35 = 0;
    while (1) {
      if (_M0L1iS35 < _M0L3lenS36) {
        int32_t _M0L6_2atmpS931 = _M0L11dst__offsetS33 + _M0L1iS35;
        int32_t _M0L6_2atmpS933 = _M0L11src__offsetS34 + _M0L1iS35;
        moonbit_string_t _M0L6_2atmpS2162;
        moonbit_string_t _M0L6_2atmpS932;
        moonbit_string_t _M0L6_2aoldS2161;
        int32_t _M0L6_2atmpS934;
        if (
          _M0L6_2atmpS933 < 0
          || _M0L6_2atmpS933 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2162 = (moonbit_string_t)_M0L3srcS32[_M0L6_2atmpS933];
        _M0L6_2atmpS932 = _M0L6_2atmpS2162;
        if (
          _M0L6_2atmpS931 < 0
          || _M0L6_2atmpS931 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2161 = (moonbit_string_t)_M0L3dstS31[_M0L6_2atmpS931];
        moonbit_incref(_M0L6_2atmpS932);
        moonbit_decref(_M0L6_2aoldS2161);
        _M0L3dstS31[_M0L6_2atmpS931] = _M0L6_2atmpS932;
        _M0L6_2atmpS934 = _M0L1iS35 + 1;
        _M0L1iS35 = _M0L6_2atmpS934;
        continue;
      } else {
        moonbit_decref(_M0L3srcS32);
        moonbit_decref(_M0L3dstS31);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS939 = _M0L3lenS36 - 1;
    int32_t _M0L1iS38 = _M0L6_2atmpS939;
    while (1) {
      if (_M0L1iS38 >= 0) {
        int32_t _M0L6_2atmpS935 = _M0L11dst__offsetS33 + _M0L1iS38;
        int32_t _M0L6_2atmpS937 = _M0L11src__offsetS34 + _M0L1iS38;
        moonbit_string_t _M0L6_2atmpS2164;
        moonbit_string_t _M0L6_2atmpS936;
        moonbit_string_t _M0L6_2aoldS2163;
        int32_t _M0L6_2atmpS938;
        if (
          _M0L6_2atmpS937 < 0
          || _M0L6_2atmpS937 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2164 = (moonbit_string_t)_M0L3srcS32[_M0L6_2atmpS937];
        _M0L6_2atmpS936 = _M0L6_2atmpS2164;
        if (
          _M0L6_2atmpS935 < 0
          || _M0L6_2atmpS935 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2163 = (moonbit_string_t)_M0L3dstS31[_M0L6_2atmpS935];
        moonbit_incref(_M0L6_2atmpS936);
        moonbit_decref(_M0L6_2aoldS2163);
        _M0L3dstS31[_M0L6_2atmpS935] = _M0L6_2atmpS936;
        _M0L6_2atmpS938 = _M0L1iS38 - 1;
        _M0L1iS38 = _M0L6_2atmpS938;
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
  int32_t _if__result_2395;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS40 == _M0L3srcS41) {
    _if__result_2395 = _M0L11dst__offsetS42 < _M0L11src__offsetS43;
  } else {
    _if__result_2395 = 0;
  }
  if (_if__result_2395) {
    int32_t _M0L1iS44 = 0;
    while (1) {
      if (_M0L1iS44 < _M0L3lenS45) {
        int32_t _M0L6_2atmpS940 = _M0L11dst__offsetS42 + _M0L1iS44;
        int32_t _M0L6_2atmpS942 = _M0L11src__offsetS43 + _M0L1iS44;
        struct _M0TUsiE* _M0L6_2atmpS2166;
        struct _M0TUsiE* _M0L6_2atmpS941;
        struct _M0TUsiE* _M0L6_2aoldS2165;
        int32_t _M0L6_2atmpS943;
        if (
          _M0L6_2atmpS942 < 0
          || _M0L6_2atmpS942 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2166 = (struct _M0TUsiE*)_M0L3srcS41[_M0L6_2atmpS942];
        _M0L6_2atmpS941 = _M0L6_2atmpS2166;
        if (
          _M0L6_2atmpS940 < 0
          || _M0L6_2atmpS940 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2165 = (struct _M0TUsiE*)_M0L3dstS40[_M0L6_2atmpS940];
        if (_M0L6_2atmpS941) {
          moonbit_incref(_M0L6_2atmpS941);
        }
        if (_M0L6_2aoldS2165) {
          moonbit_decref(_M0L6_2aoldS2165);
        }
        _M0L3dstS40[_M0L6_2atmpS940] = _M0L6_2atmpS941;
        _M0L6_2atmpS943 = _M0L1iS44 + 1;
        _M0L1iS44 = _M0L6_2atmpS943;
        continue;
      } else {
        moonbit_decref(_M0L3srcS41);
        moonbit_decref(_M0L3dstS40);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS948 = _M0L3lenS45 - 1;
    int32_t _M0L1iS47 = _M0L6_2atmpS948;
    while (1) {
      if (_M0L1iS47 >= 0) {
        int32_t _M0L6_2atmpS944 = _M0L11dst__offsetS42 + _M0L1iS47;
        int32_t _M0L6_2atmpS946 = _M0L11src__offsetS43 + _M0L1iS47;
        struct _M0TUsiE* _M0L6_2atmpS2168;
        struct _M0TUsiE* _M0L6_2atmpS945;
        struct _M0TUsiE* _M0L6_2aoldS2167;
        int32_t _M0L6_2atmpS947;
        if (
          _M0L6_2atmpS946 < 0
          || _M0L6_2atmpS946 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS2168 = (struct _M0TUsiE*)_M0L3srcS41[_M0L6_2atmpS946];
        _M0L6_2atmpS945 = _M0L6_2atmpS2168;
        if (
          _M0L6_2atmpS944 < 0
          || _M0L6_2atmpS944 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS2167 = (struct _M0TUsiE*)_M0L3dstS40[_M0L6_2atmpS944];
        if (_M0L6_2atmpS945) {
          moonbit_incref(_M0L6_2atmpS945);
        }
        if (_M0L6_2aoldS2167) {
          moonbit_decref(_M0L6_2aoldS2167);
        }
        _M0L3dstS40[_M0L6_2atmpS944] = _M0L6_2atmpS945;
        _M0L6_2atmpS947 = _M0L1iS47 - 1;
        _M0L1iS47 = _M0L6_2atmpS947;
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
  moonbit_string_t _M0L6_2atmpS911;
  moonbit_string_t _M0L6_2atmpS2171;
  moonbit_string_t _M0L6_2atmpS909;
  moonbit_string_t _M0L6_2atmpS910;
  moonbit_string_t _M0L6_2atmpS2170;
  moonbit_string_t _M0L6_2atmpS908;
  moonbit_string_t _M0L6_2atmpS2169;
  moonbit_string_t _M0L6_2atmpS907;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS911 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS16);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2171
  = moonbit_add_string(_M0L6_2atmpS911, (moonbit_string_t)moonbit_string_literal_61.data);
  moonbit_decref(_M0L6_2atmpS911);
  _M0L6_2atmpS909 = _M0L6_2atmpS2171;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS910
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS17);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2170 = moonbit_add_string(_M0L6_2atmpS909, _M0L6_2atmpS910);
  moonbit_decref(_M0L6_2atmpS909);
  moonbit_decref(_M0L6_2atmpS910);
  _M0L6_2atmpS908 = _M0L6_2atmpS2170;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2169
  = moonbit_add_string(_M0L6_2atmpS908, (moonbit_string_t)moonbit_string_literal_62.data);
  moonbit_decref(_M0L6_2atmpS908);
  _M0L6_2atmpS907 = _M0L6_2atmpS2169;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS907);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS18,
  moonbit_string_t _M0L3locS19
) {
  moonbit_string_t _M0L6_2atmpS916;
  moonbit_string_t _M0L6_2atmpS2174;
  moonbit_string_t _M0L6_2atmpS914;
  moonbit_string_t _M0L6_2atmpS915;
  moonbit_string_t _M0L6_2atmpS2173;
  moonbit_string_t _M0L6_2atmpS913;
  moonbit_string_t _M0L6_2atmpS2172;
  moonbit_string_t _M0L6_2atmpS912;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS916 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2174
  = moonbit_add_string(_M0L6_2atmpS916, (moonbit_string_t)moonbit_string_literal_61.data);
  moonbit_decref(_M0L6_2atmpS916);
  _M0L6_2atmpS914 = _M0L6_2atmpS2174;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS915
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2173 = moonbit_add_string(_M0L6_2atmpS914, _M0L6_2atmpS915);
  moonbit_decref(_M0L6_2atmpS914);
  moonbit_decref(_M0L6_2atmpS915);
  _M0L6_2atmpS913 = _M0L6_2atmpS2173;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2172
  = moonbit_add_string(_M0L6_2atmpS913, (moonbit_string_t)moonbit_string_literal_62.data);
  moonbit_decref(_M0L6_2atmpS913);
  _M0L6_2atmpS912 = _M0L6_2atmpS2172;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS912);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS921;
  moonbit_string_t _M0L6_2atmpS2177;
  moonbit_string_t _M0L6_2atmpS919;
  moonbit_string_t _M0L6_2atmpS920;
  moonbit_string_t _M0L6_2atmpS2176;
  moonbit_string_t _M0L6_2atmpS918;
  moonbit_string_t _M0L6_2atmpS2175;
  moonbit_string_t _M0L6_2atmpS917;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS921 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2177
  = moonbit_add_string(_M0L6_2atmpS921, (moonbit_string_t)moonbit_string_literal_61.data);
  moonbit_decref(_M0L6_2atmpS921);
  _M0L6_2atmpS919 = _M0L6_2atmpS2177;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS920
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2176 = moonbit_add_string(_M0L6_2atmpS919, _M0L6_2atmpS920);
  moonbit_decref(_M0L6_2atmpS919);
  moonbit_decref(_M0L6_2atmpS920);
  _M0L6_2atmpS918 = _M0L6_2atmpS2176;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2175
  = moonbit_add_string(_M0L6_2atmpS918, (moonbit_string_t)moonbit_string_literal_62.data);
  moonbit_decref(_M0L6_2atmpS918);
  _M0L6_2atmpS917 = _M0L6_2atmpS2175;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS917);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5valueS15
) {
  uint32_t _M0L3accS906;
  uint32_t _M0L6_2atmpS905;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS906 = _M0L4selfS14->$0;
  _M0L6_2atmpS905 = _M0L3accS906 + 4u;
  _M0L4selfS14->$0 = _M0L6_2atmpS905;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS14, _M0L5valueS15);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS12,
  uint32_t _M0L5inputS13
) {
  uint32_t _M0L3accS903;
  uint32_t _M0L6_2atmpS904;
  uint32_t _M0L6_2atmpS902;
  uint32_t _M0L6_2atmpS901;
  uint32_t _M0L6_2atmpS900;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS903 = _M0L4selfS12->$0;
  _M0L6_2atmpS904 = _M0L5inputS13 * 3266489917u;
  _M0L6_2atmpS902 = _M0L3accS903 + _M0L6_2atmpS904;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS901 = _M0FPB4rotl(_M0L6_2atmpS902, 17);
  _M0L6_2atmpS900 = _M0L6_2atmpS901 * 668265263u;
  _M0L4selfS12->$0 = _M0L6_2atmpS900;
  moonbit_decref(_M0L4selfS12);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS10, int32_t _M0L1rS11) {
  uint32_t _M0L6_2atmpS897;
  int32_t _M0L6_2atmpS899;
  uint32_t _M0L6_2atmpS898;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS897 = _M0L1xS10 << (_M0L1rS11 & 31);
  _M0L6_2atmpS899 = 32 - _M0L1rS11;
  _M0L6_2atmpS898 = _M0L1xS10 >> (_M0L6_2atmpS899 & 31);
  return _M0L6_2atmpS897 | _M0L6_2atmpS898;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S6,
  struct _M0TPB6Logger _M0L10_2ax__4934S9
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS7;
  moonbit_string_t _M0L8_2afieldS2178;
  int32_t _M0L6_2acntS2264;
  moonbit_string_t _M0L15_2a_2aarg__4935S8;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS7
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S6;
  _M0L8_2afieldS2178 = _M0L10_2aFailureS7->$0;
  _M0L6_2acntS2264 = Moonbit_object_header(_M0L10_2aFailureS7)->rc;
  if (_M0L6_2acntS2264 > 1) {
    int32_t _M0L11_2anew__cntS2265 = _M0L6_2acntS2264 - 1;
    Moonbit_object_header(_M0L10_2aFailureS7)->rc = _M0L11_2anew__cntS2265;
    moonbit_incref(_M0L8_2afieldS2178);
  } else if (_M0L6_2acntS2264 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS7);
  }
  _M0L15_2a_2aarg__4935S8 = _M0L8_2afieldS2178;
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_63.data);
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S9, _M0L15_2a_2aarg__4935S8);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_64.data);
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS854) {
  switch (Moonbit_object_tag(_M0L4_2aeS854)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS854);
      return (moonbit_string_t)moonbit_string_literal_65.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS854);
      return (moonbit_string_t)moonbit_string_literal_66.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS854);
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS854);
      return (moonbit_string_t)moonbit_string_literal_67.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS854);
      return (moonbit_string_t)moonbit_string_literal_68.data;
      break;
    }
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS876
) {
  moonbit_string_t _M0L7_2aselfS875 = (moonbit_string_t)_M0L11_2aobj__ptrS876;
  return _M0IPC16string6StringPB4Show10to__string(_M0L7_2aselfS875);
}

int32_t _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS874,
  struct _M0TPB6Logger _M0L8_2aparamS873
) {
  moonbit_string_t _M0L7_2aselfS872 = (moonbit_string_t)_M0L11_2aobj__ptrS874;
  _M0IPC16string6StringPB4Show6output(_M0L7_2aselfS872, _M0L8_2aparamS873);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS871,
  int32_t _M0L8_2aparamS870
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS869 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS871;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS869, _M0L8_2aparamS870);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS868,
  struct _M0TPC16string10StringView _M0L8_2aparamS867
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS866 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS868;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS866, _M0L8_2aparamS867);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS865,
  moonbit_string_t _M0L8_2aparamS862,
  int32_t _M0L8_2aparamS863,
  int32_t _M0L8_2aparamS864
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS861 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS865;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS861, _M0L8_2aparamS862, _M0L8_2aparamS863, _M0L8_2aparamS864);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS860,
  moonbit_string_t _M0L8_2aparamS859
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS858 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS860;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS858, _M0L8_2aparamS859);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS896 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS895;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS888;
  moonbit_string_t* _M0L6_2atmpS894;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS893;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS889;
  moonbit_string_t* _M0L6_2atmpS892;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS891;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS890;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS781;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS887;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS886;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS885;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS884;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS780;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS883;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS882;
  _M0L6_2atmpS896[0] = (moonbit_string_t)moonbit_string_literal_69.data;
  moonbit_incref(_M0FP38clawteam8clawteam9scheduler59____test__7363686564756c65725f7762746573742e6d6274__0_2eclo);
  _M0L8_2atupleS895
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS895)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS895->$0
  = _M0FP38clawteam8clawteam9scheduler59____test__7363686564756c65725f7762746573742e6d6274__0_2eclo;
  _M0L8_2atupleS895->$1 = _M0L6_2atmpS896;
  _M0L8_2atupleS888
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS888)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS888->$0 = 0;
  _M0L8_2atupleS888->$1 = _M0L8_2atupleS895;
  _M0L6_2atmpS894 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS894[0] = (moonbit_string_t)moonbit_string_literal_70.data;
  moonbit_incref(_M0FP38clawteam8clawteam9scheduler59____test__7363686564756c65725f7762746573742e6d6274__1_2eclo);
  _M0L8_2atupleS893
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS893)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS893->$0
  = _M0FP38clawteam8clawteam9scheduler59____test__7363686564756c65725f7762746573742e6d6274__1_2eclo;
  _M0L8_2atupleS893->$1 = _M0L6_2atmpS894;
  _M0L8_2atupleS889
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS889)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS889->$0 = 1;
  _M0L8_2atupleS889->$1 = _M0L8_2atupleS893;
  _M0L6_2atmpS892 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS892[0] = (moonbit_string_t)moonbit_string_literal_71.data;
  moonbit_incref(_M0FP38clawteam8clawteam9scheduler59____test__7363686564756c65725f7762746573742e6d6274__2_2eclo);
  _M0L8_2atupleS891
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS891)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS891->$0
  = _M0FP38clawteam8clawteam9scheduler59____test__7363686564756c65725f7762746573742e6d6274__2_2eclo;
  _M0L8_2atupleS891->$1 = _M0L6_2atmpS892;
  _M0L8_2atupleS890
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS890)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS890->$0 = 2;
  _M0L8_2atupleS890->$1 = _M0L8_2atupleS891;
  _M0L7_2abindS781
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS781[0] = _M0L8_2atupleS888;
  _M0L7_2abindS781[1] = _M0L8_2atupleS889;
  _M0L7_2abindS781[2] = _M0L8_2atupleS890;
  _M0L6_2atmpS887 = _M0L7_2abindS781;
  _M0L6_2atmpS886
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 3, _M0L6_2atmpS887
  };
  #line 398 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L6_2atmpS885
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS886);
  _M0L8_2atupleS884
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS884)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS884->$0 = (moonbit_string_t)moonbit_string_literal_72.data;
  _M0L8_2atupleS884->$1 = _M0L6_2atmpS885;
  _M0L7_2abindS780
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS780[0] = _M0L8_2atupleS884;
  _M0L6_2atmpS883 = _M0L7_2abindS780;
  _M0L6_2atmpS882
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 1, _M0L6_2atmpS883
  };
  #line 397 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0FP38clawteam8clawteam9scheduler48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS882);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS881;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS848;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS849;
  int32_t _M0L7_2abindS850;
  int32_t _M0L2__S851;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS881
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS848
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS848)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS848->$0 = _M0L6_2atmpS881;
  _M0L12async__testsS848->$1 = 0;
  #line 440 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0L7_2abindS849
  = _M0FP38clawteam8clawteam9scheduler52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS850 = _M0L7_2abindS849->$1;
  _M0L2__S851 = 0;
  while (1) {
    if (_M0L2__S851 < _M0L7_2abindS850) {
      struct _M0TUsiE** _M0L8_2afieldS2182 = _M0L7_2abindS849->$0;
      struct _M0TUsiE** _M0L3bufS880 = _M0L8_2afieldS2182;
      struct _M0TUsiE* _M0L6_2atmpS2181 =
        (struct _M0TUsiE*)_M0L3bufS880[_M0L2__S851];
      struct _M0TUsiE* _M0L3argS852 = _M0L6_2atmpS2181;
      moonbit_string_t _M0L8_2afieldS2180 = _M0L3argS852->$0;
      moonbit_string_t _M0L6_2atmpS877 = _M0L8_2afieldS2180;
      int32_t _M0L8_2afieldS2179 = _M0L3argS852->$1;
      int32_t _M0L6_2atmpS878 = _M0L8_2afieldS2179;
      int32_t _M0L6_2atmpS879;
      moonbit_incref(_M0L6_2atmpS877);
      moonbit_incref(_M0L12async__testsS848);
      #line 441 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
      _M0FP38clawteam8clawteam9scheduler44moonbit__test__driver__internal__do__execute(_M0L12async__testsS848, _M0L6_2atmpS877, _M0L6_2atmpS878);
      _M0L6_2atmpS879 = _M0L2__S851 + 1;
      _M0L2__S851 = _M0L6_2atmpS879;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS849);
    }
    break;
  }
  #line 443 "E:\\moonbit\\clawteam\\scheduler\\__generated_driver_for_whitebox_test.mbt"
  _M0IP016_24default__implP38clawteam8clawteam9scheduler28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam9scheduler34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS848);
  return 0;
}