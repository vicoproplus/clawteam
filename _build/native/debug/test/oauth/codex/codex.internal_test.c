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
struct _M0DTPC16result6ResultGjRPB7NoErrorE2Ok;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0R74_24moonbitlang_2fx_2fcodec_2fbase64_2eencode_2einner_2eanon__u3350__l113__;

struct _M0BTPC16random6Source;

struct _M0TPC36random8internal14random__source9Quadruple;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3734__l440__;

struct _M0DTPC15error5Error105clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0DTPC16result6ResultGzRP48clawteam8clawteam8internal5errno5ErrnoE2Ok;

struct _M0TPC16buffer6Buffer;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TUsRPB6LoggerE;

struct _M0TWEOc;

struct _M0TPB13StringBuilder;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0DTPC16option6OptionGmE4Some;

struct _M0TPC16random6Source;

struct _M0DTPC16result6ResultGzRPB7NoErrorE2Ok;

struct _M0TPC36random8internal14random__source7ChaCha8;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGUsiEE;

struct _M0R52Iter_3a_3atake_7c_5bUInt_5d_7c_2eanon__u2171__l481__;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TPB9ArrayViewGjE;

struct _M0TPC13ref3RefGjE;

struct _M0KTPC16random6SourceTPC36random8internal14random__source7ChaCha8;

struct _M0TPB6Logger;

struct _M0DTPC16result6ResultGAjRPB7NoErrorE2Ok;

struct _M0R44Bytes_3a_3afrom__array_2eanon__u2249__l455__;

struct _M0R38String_3a_3aiter_2eanon__u1939__l247__;

struct _M0R42StringView_3a_3aiter_2eanon__u1876__l198__;

struct _M0R76_24moonbitlang_2fx_2fcrypto_2earr__u32__to__u8be__into_2eanon__u3371__l117__;

struct _M0TP411moonbitlang1x5codec6base647Encoder;

struct _M0DTPC16result6ResultGzRPB7NoErrorE3Err;

struct _M0TWEuQRPC15error5Error;

struct _M0DTPC16result6ResultGRPC16random4RandRP48clawteam8clawteam8internal5errno5ErrnoE2Ok;

struct _M0DTPC16result6ResultGRPC16random4RandRP48clawteam8clawteam8internal5errno5ErrnoE3Err;

struct _M0TP311moonbitlang1x6crypto6SHA256;

struct _M0DTPC16result6ResultGAjRPB7NoErrorE3Err;

struct _M0DTPC16result6ResultGzRP48clawteam8clawteam8internal5errno5ErrnoE3Err;

struct _M0TWjEu;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0DTPC16result6ResultGuRPB7NoErrorE2Ok;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TWEOs;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0R98_40moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2eChaCha8_3a_3anew_2eanon__u3251__l29__;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0DTPC16result6ResultGyRPB7NoErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0R57ArrayView_3a_3aiter_7c_5bUInt_5d_7c_2eanon__u1969__l570__;

struct _M0TPB13SourceLocRepr;

struct _M0TWEOj;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC15error5Error107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0DTPC16result6ResultGuRPB7NoErrorE3Err;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1957__l570__;

struct _M0TWRPC15error5ErrorEu;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5oauth5codex33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0TP48clawteam8clawteam5oauth5codex9PkceCodes;

struct _M0TPB6Hasher;

struct _M0DTPC16result6ResultGjRPB7NoErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TPC15bytes9BytesView;

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5oauth5codex33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TWuEu;

struct _M0TPC16string10StringView;

struct _M0R109_24clawteam_2fclawteam_2foauth_2fcodex_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0TWiEj;

struct _M0TPC16random7UInt128;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB9ArrayViewGyE;

struct _M0TPB5ArrayGsE;

struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3738__l439__;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno;

struct _M0DTPC16result6ResultGuRPB7FailureE3Err;

struct _M0TPB9ArrayViewGsE;

struct _M0DTPC16result6ResultGuRPB7FailureE2Ok;

struct _M0R41BytesView_3a_3aiter_2eanon__u2237__l234__;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0DTPC16result6ResultGyRPB7NoErrorE2Ok;

struct _M0DTPC16result6ResultGjRPB7NoErrorE2Ok {
  uint32_t $0;
  
};

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $1;
  struct _M0TUWEuQRPC15error5ErrorNsE* $5;
  
};

struct _M0R74_24moonbitlang_2fx_2fcodec_2fbase64_2eencode_2einner_2eanon__u3350__l113__ {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  struct _M0TPB13StringBuilder* $0;
  
};

struct _M0BTPC16random6Source {
  uint64_t(* $method_0)(void*);
  
};

struct _M0TPC36random8internal14random__source9Quadruple {
  uint32_t $0;
  uint32_t $1;
  uint32_t $2;
  uint32_t $3;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3734__l440__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0DTPC15error5Error105clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGzRP48clawteam8clawteam8internal5errno5ErrnoE2Ok {
  moonbit_bytes_t $0;
  
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

struct _M0DTPC16option6OptionGmE4Some {
  uint64_t $0;
  
};

struct _M0TPC16random6Source {
  struct _M0BTPC16random6Source* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGzRPB7NoErrorE2Ok {
  moonbit_bytes_t $0;
  
};

struct _M0TPC36random8internal14random__source7ChaCha8 {
  uint32_t $2;
  uint32_t $3;
  uint32_t $4;
  uint32_t* $0;
  uint32_t* $1;
  
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

struct _M0R52Iter_3a_3atake_7c_5bUInt_5d_7c_2eanon__u2171__l481__ {
  int64_t(* code)(struct _M0TWEOj*);
  struct _M0TWEOj* $0;
  struct _M0TPC13ref3RefGiE* $1;
  
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

struct _M0TPB9ArrayViewGjE {
  int32_t $1;
  int32_t $2;
  uint32_t* $0;
  
};

struct _M0TPC13ref3RefGjE {
  uint32_t $0;
  
};

struct _M0KTPC16random6SourceTPC36random8internal14random__source7ChaCha8 {
  struct _M0BTPC16random6Source* $0;
  void* $1;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGAjRPB7NoErrorE2Ok {
  uint32_t* $0;
  
};

struct _M0R44Bytes_3a_3afrom__array_2eanon__u2249__l455__ {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_bytes_t $0_0;
  
};

struct _M0R38String_3a_3aiter_2eanon__u1939__l247__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $1;
  
};

struct _M0R42StringView_3a_3aiter_2eanon__u1876__l198__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $1;
  int32_t $2_1;
  int32_t $2_2;
  struct _M0TPC13ref3RefGiE* $0;
  moonbit_string_t $2_0;
  
};

struct _M0R76_24moonbitlang_2fx_2fcrypto_2earr__u32__to__u8be__into_2eanon__u3371__l117__ {
  int32_t(* code)(struct _M0TWjEu*, uint32_t);
  moonbit_bytes_t $0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TP411moonbitlang1x5codec6base647Encoder {
  int32_t $0;
  int32_t $1;
  
};

struct _M0DTPC16result6ResultGzRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTPC16result6ResultGRPC16random4RandRP48clawteam8clawteam8internal5errno5ErrnoE2Ok {
  struct _M0BTPC16random6Source* $0_0;
  void* $0_1;
  
};

struct _M0DTPC16result6ResultGRPC16random4RandRP48clawteam8clawteam8internal5errno5ErrnoE3Err {
  void* $0;
  
};

struct _M0TP311moonbitlang1x6crypto6SHA256 {
  uint64_t $1;
  int32_t $3;
  uint32_t* $0;
  moonbit_bytes_t $2;
  
};

struct _M0DTPC16result6ResultGAjRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGzRP48clawteam8clawteam8internal5errno5ErrnoE3Err {
  void* $0;
  
};

struct _M0TWjEu {
  int32_t(* code)(struct _M0TWjEu*, uint32_t);
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB7NoErrorE2Ok {
  int32_t $0;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0R98_40moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2eChaCha8_3a_3anew_2eanon__u3251__l29__ {
  uint32_t(* code)(struct _M0TWiEj*, int32_t);
  moonbit_bytes_t $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0DTPC16result6ResultGyRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** $0;
  
};

struct _M0R57ArrayView_3a_3aiter_7c_5bUInt_5d_7c_2eanon__u1969__l570__ {
  int64_t(* code)(struct _M0TWEOj*);
  int32_t $0_1;
  int32_t $0_2;
  uint32_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
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

struct _M0TWEOj {
  int64_t(* code)(struct _M0TWEOj*);
  
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

struct _M0DTPC15error5Error107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB7NoErrorE3Err {
  int32_t $0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1957__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5oauth5codex33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0TP48clawteam8clawteam5oauth5codex9PkceCodes {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0DTPC16result6ResultGjRPB7NoErrorE3Err {
  int32_t $0;
  
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

struct _M0TPC15bytes9BytesView {
  int32_t $1;
  int32_t $2;
  moonbit_bytes_t $0;
  
};

struct _M0DTPC16result6ResultGbRP48clawteam8clawteam5oauth5codex33MoonBitTestDriverInternalSkipTestE2Ok {
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

struct _M0R109_24clawteam_2fclawteam_2foauth_2fcodex_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TWiEj {
  uint32_t(* code)(struct _M0TWiEj*, int32_t);
  
};

struct _M0TPC16random7UInt128 {
  uint64_t $0;
  uint64_t $1;
  
};

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** $0;
  
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

struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3738__l439__ {
  int32_t(* code)(struct _M0TWEOc*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPB7FailureE3Err {
  void* $0;
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0DTPC16result6ResultGuRPB7FailureE2Ok {
  int32_t $0;
  
};

struct _M0R41BytesView_3a_3aiter_2eanon__u2237__l234__ {
  int32_t(* code)(struct _M0TWEOc*);
  int32_t $0_1;
  int32_t $0_2;
  int32_t $1;
  moonbit_bytes_t $0_0;
  struct _M0TPC13ref3RefGiE* $2;
  
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

struct _M0DTPC16result6ResultGyRPB7NoErrorE2Ok {
  int32_t $0;
  
};

struct moonbit_result_2 {
  int tag;
  union { moonbit_bytes_t ok; void* err;  } data;
  
};

struct moonbit_result_1 {
  int tag;
  union { struct _M0TPC16random6Source ok; void* err;  } data;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP48clawteam8clawteam5oauth5codex39____test__706b63652e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__executeN17error__to__stringS1371(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__executeN14handle__resultS1362(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP48clawteam8clawteam5oauth5codex41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP48clawteam8clawteam5oauth5codex41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testC3738l439(
  struct _M0TWEOc*
);

int32_t _M0IP48clawteam8clawteam5oauth5codex41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testC3734l440(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP48clawteam8clawteam5oauth5codex45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEOc*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1289(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1284(
  int32_t
);

moonbit_string_t _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1277(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1271(
  int32_t,
  moonbit_string_t
);

#define _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP48clawteam8clawteam5oauth5codex28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5oauth5codex34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP48clawteam8clawteam5oauth5codex29____test__706b63652e6d6274__0(
  
);

struct _M0TP48clawteam8clawteam5oauth5codex9PkceCodes* _M0FP48clawteam8clawteam5oauth5codex14generate__pkce(
  
);

moonbit_string_t _M0FP48clawteam8clawteam5oauth5codex14strip__padding(
  moonbit_string_t
);

moonbit_bytes_t _M0FP48clawteam8clawteam5oauth5codex23generate__random__bytes(
  int32_t
);

int32_t _M0FP48clawteam8clawteam5oauth5codex9gen__byte(
  struct _M0TPC16random6Source
);

int32_t _M0IPC15bytes5BytesP311moonbitlang1x6crypto10ByteSource8blit__to(
  moonbit_bytes_t,
  moonbit_bytes_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0IPC15bytes5BytesP311moonbitlang1x6crypto10ByteSource6length(
  moonbit_bytes_t
);

moonbit_bytes_t _M0FP311moonbitlang1x6crypto6sha256GzE(moonbit_bytes_t);

moonbit_bytes_t _M0MP311moonbitlang1x6crypto6SHA2568finalize(
  struct _M0TP311moonbitlang1x6crypto6SHA256*
);

int32_t _M0MP311moonbitlang1x6crypto6SHA25624__finalize__into_2einner(
  struct _M0TP311moonbitlang1x6crypto6SHA256*,
  moonbit_bytes_t,
  int32_t,
  int32_t
);

int32_t _M0MP311moonbitlang1x6crypto6SHA2566updateGzE(
  struct _M0TP311moonbitlang1x6crypto6SHA256*,
  moonbit_bytes_t
);

int32_t _M0MP311moonbitlang1x6crypto6SHA2569transform(
  moonbit_bytes_t,
  uint32_t*
);

struct _M0TP311moonbitlang1x6crypto6SHA256* _M0MP311moonbitlang1x6crypto6SHA2563new(
  uint32_t*
);

struct _M0TP311moonbitlang1x6crypto6SHA256* _M0MP311moonbitlang1x6crypto6SHA25611new_2einner(
  uint32_t*
);

uint32_t _M0MP311moonbitlang1x6crypto3SM35gg__1(uint32_t, uint32_t, uint32_t);

uint32_t _M0MP311moonbitlang1x6crypto3SM35ff__1(uint32_t, uint32_t, uint32_t);

uint32_t _M0FP311moonbitlang1x6crypto16rotate__right__u(uint32_t, int32_t);

int32_t _M0FP311moonbitlang1x6crypto24arr__u32__to__u8be__into(
  struct _M0TWEOj*,
  moonbit_bytes_t,
  int32_t
);

int32_t _M0FP311moonbitlang1x6crypto24arr__u32__to__u8be__intoC3371l117(
  struct _M0TWjEu*,
  uint32_t
);

uint32_t _M0FP311moonbitlang1x6crypto28bytes__u8__to__u32be_2einner(
  moonbit_bytes_t,
  int32_t
);

uint32_t _M0FP311moonbitlang1x6crypto6uint32(int32_t);

moonbit_string_t _M0FP411moonbitlang1x5codec6base6414encode_2einner(
  struct _M0TPC15bytes9BytesView,
  int32_t
);

int32_t _M0FP411moonbitlang1x5codec6base6414encode_2einnerC3350l113(
  struct _M0TWuEu*,
  int32_t
);

int32_t _M0MP411moonbitlang1x5codec6base647Encoder18encode__to_2einner(
  struct _M0TP411moonbitlang1x5codec6base647Encoder*,
  struct _M0TPC15bytes9BytesView,
  struct _M0TWuEu*,
  int32_t,
  int32_t
);

struct _M0TP411moonbitlang1x5codec6base647Encoder* _M0MP411moonbitlang1x5codec6base647Encoder3new(
  
);

int32_t _M0FP411moonbitlang1x5codec6base6415index__to__char(int32_t, int32_t);

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal4rand7chacha8();

struct moonbit_result_2 _M0FP48clawteam8clawteam8internal4rand5bytes(int32_t);

#define _M0FP48clawteam8clawteam8internal4rand11rand__bytes moonbit_moonclaw_rand_bytes

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

int32_t _M0MPC16random4Rand11int_2einner(
  struct _M0TPC16random6Source,
  int32_t
);

uint32_t _M0MPC16random4Rand12uint_2einner(
  struct _M0TPC16random6Source,
  uint32_t
);

uint64_t _M0MPC16random4Rand14uint64_2einner(
  struct _M0TPC16random6Source,
  uint64_t
);

struct _M0TPC16random7UInt128 _M0FPC16random7umul128(uint64_t, uint64_t);

uint64_t _M0MPC16random4Rand4next(struct _M0TPC16random6Source);

struct _M0TPC16random6Source _M0MPC16random4Rand15chacha8_2einner(
  moonbit_bytes_t
);

uint64_t _M0IPC36random8internal14random__source7ChaCha8PC16random6Source4next(
  struct _M0TPC36random8internal14random__source7ChaCha8*
);

int32_t _M0MPC36random8internal14random__source7ChaCha86refill(
  struct _M0TPC36random8internal14random__source7ChaCha8*
);

void* _M0MPC36random8internal14random__source7ChaCha84next(
  struct _M0TPC36random8internal14random__source7ChaCha8*
);

struct _M0TPC36random8internal14random__source7ChaCha8* _M0MPC36random8internal14random__source7ChaCha83new(
  moonbit_bytes_t
);

uint32_t _M0MPC36random8internal14random__source7ChaCha83newC3251l29(
  struct _M0TWiEj*,
  int32_t
);

int32_t _M0FPC36random8internal14random__source13chacha__block(
  uint32_t*,
  uint32_t*,
  uint32_t
);

struct _M0TPC36random8internal14random__source9Quadruple _M0FPC36random8internal14random__source13chacha__blockN2qrS30(
  struct _M0TPC36random8internal14random__source9Quadruple
);

int32_t _M0FPC36random8internal14random__source5setup(
  uint32_t*,
  uint32_t*,
  uint32_t
);

moonbit_string_t _M0FPC28encoding4utf821decode__lossy_2einner(
  struct _M0TPC15bytes9BytesView,
  int32_t
);

moonbit_bytes_t _M0FPC28encoding4utf814encode_2einner(
  struct _M0TPC16string10StringView,
  int32_t
);

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

uint32_t _M0MPC15bytes9BytesView25unsafe__extract__uint__le(
  struct _M0TPC15bytes9BytesView,
  int32_t,
  int32_t
);

uint32_t _M0MPC15bytes9BytesView21unsafe__extract__byte(
  struct _M0TPC15bytes9BytesView,
  int32_t,
  int32_t
);

int32_t _M0IPC14byte4BytePB3Shr3shr(int32_t, int32_t);

moonbit_bytes_t _M0MPC15bytes5Bytes11from__array(struct _M0TPB9ArrayViewGyE);

int32_t _M0MPC15bytes5Bytes11from__arrayC2249l455(struct _M0TWuEu*, int32_t);

int32_t _M0MPC15array10FixedArray17blit__from__bytes(
  moonbit_bytes_t,
  int32_t,
  moonbit_bytes_t,
  int32_t,
  int32_t
);

struct _M0TWEOc* _M0MPC15bytes9BytesView4iter(struct _M0TPC15bytes9BytesView);

int32_t _M0MPC15bytes9BytesView4iterC2237l234(struct _M0TWEOc*);

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15bytes9BytesView11unsafe__get(
  struct _M0TPC15bytes9BytesView,
  int32_t
);

int32_t _M0MPC15bytes9BytesView6length(struct _M0TPC15bytes9BytesView);

int32_t _M0FPB7printlnGsE(moonbit_string_t);

moonbit_bytes_t _M0MPC15bytes5Bytes5makei(int32_t, struct _M0TWuEu*);

uint32_t* _M0MPC15array10FixedArray4copyGjE(uint32_t*);

struct moonbit_result_0 _M0FPB12assert__true(
  int32_t,
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0MPC15array10FixedArray16blit__to_2einnerGyE(
  moonbit_bytes_t,
  moonbit_bytes_t,
  int32_t,
  int32_t,
  int32_t
);

int32_t _M0MPC15array10FixedArray16blit__to_2einnerGjE(
  uint32_t*,
  uint32_t*,
  int32_t,
  int32_t,
  int32_t
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

uint32_t _M0MPC14byte4Byte8to__uint(int32_t);

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t,
  int32_t
);

moonbit_bytes_t _M0MPC15bytes5Bytes4make(int32_t, int32_t);

struct _M0TWEOj* _M0MPB4Iter4takeGjE(struct _M0TWEOj*, int32_t);

int64_t _M0MPB4Iter4takeGjEC2171l481(struct _M0TWEOj*);

int32_t _M0MPB4Iter4eachGjE(struct _M0TWEOj*, struct _M0TWjEu*);

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t);

int32_t _M0MPC15array10FixedArray12fill_2einnerGyE(
  moonbit_bytes_t,
  int32_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15array10FixedArray15unchecked__fillGyE(
  moonbit_bytes_t,
  int32_t,
  int32_t,
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

struct _M0TPB9ArrayViewGyE _M0MPC15array10FixedArray12view_2einnerGyE(
  moonbit_bytes_t,
  int32_t,
  int64_t
);

int32_t _M0MPC15array9ArrayView2atGyE(struct _M0TPB9ArrayViewGyE, int32_t);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

uint32_t* _M0MPC15array10FixedArray5makeiGjE(int32_t, struct _M0TWiEj*);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOj* _M0MPC15array10FixedArray4iterGjE(uint32_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

struct _M0TWEOj* _M0MPC15array9ArrayView4iterGjE(struct _M0TPB9ArrayViewGjE);

int64_t _M0MPC15array9ArrayView4iterGjEC1969l570(struct _M0TWEOj*);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1957l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

int32_t _M0IPC14bool4BoolPB4Show6output(int32_t, struct _M0TPB6Logger);

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t);

int32_t _M0MPC16string6String4iterC1939l247(struct _M0TWEOc*);

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

int32_t _M0MPC15array9ArrayView6lengthGyE(struct _M0TPB9ArrayViewGyE);

int32_t _M0MPC15array9ArrayView6lengthGjE(struct _M0TPB9ArrayViewGjE);

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

int32_t _M0MPC16string10StringView4iterC1876l198(struct _M0TWEOc*);

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

struct _M0TWEOc* _M0MPB4Iter3newGyE(struct _M0TWEOc*);

struct _M0TWEOj* _M0MPB4Iter3newGjE(struct _M0TWEOj*);

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

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc*);

int32_t _M0MPB4Iter4nextGyE(struct _M0TWEOc*);

int64_t _M0MPB4Iter4nextGjE(struct _M0TWEOj*);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(int32_t);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t
);

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGbE(int32_t);

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

int32_t _M0MPC16uint646UInt648to__byte(uint64_t);

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

int32_t _M0MPC15array10FixedArray12unsafe__blitGjE(
  uint32_t*,
  int32_t,
  uint32_t*,
  int32_t,
  int32_t
);

int32_t _M0FPB5abortGiE(moonbit_string_t, moonbit_string_t);

int32_t _M0FPB5abortGuE(moonbit_string_t, moonbit_string_t);

struct _M0TPC16random6Source _M0FPB5abortGRPC16random4RandE(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPC15bytes9BytesView _M0FPB5abortGRPC15bytes9BytesViewE(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0FPB5abortGyE(moonbit_string_t, moonbit_string_t);

struct _M0TPB9ArrayViewGyE _M0FPB5abortGRPB9ArrayViewGyEE(
  moonbit_string_t,
  moonbit_string_t
);

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t,
  moonbit_string_t
);

uint32_t _M0FPB5abortGjE(moonbit_string_t, moonbit_string_t);

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

struct _M0TPC16random6Source _M0FPC15abort5abortGRPC16random4RandE(
  moonbit_string_t
);

struct _M0TPC15bytes9BytesView _M0FPC15abort5abortGRPC15bytes9BytesViewE(
  moonbit_string_t
);

int32_t _M0FPC15abort5abortGyE(moonbit_string_t);

struct _M0TPB9ArrayViewGyE _M0FPC15abort5abortGRPB9ArrayViewGyEE(
  moonbit_string_t
);

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t
);

uint32_t _M0FPC15abort5abortGjE(moonbit_string_t);

moonbit_string_t _M0FP15Error10to__string(void*);

uint64_t _M0IPC36random8internal14random__source7ChaCha8PC16random6Source59next_2edyncall__as___40moonbitlang_2fcore_2frandom_2eSource(
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

int32_t moonbit_moonclaw_c_load_byte(void*, int32_t);

int32_t moonbit_moonclaw_c_is_null(void*);

int32_t moonbit_moonclaw_rand_bytes(moonbit_bytes_t);

void* moonbit_moonclaw_errno_strerror(int32_t);

uint64_t moonbit_moonclaw_c_strlen(void*);

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    86, 105, 101, 119, 32, 105, 110, 100, 101, 120, 32, 111, 117, 116, 
    32, 111, 102, 32, 98, 111, 117, 110, 100, 115, 0
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

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 97, 114, 
    114, 97, 121, 118, 105, 101, 119, 46, 109, 98, 116, 58, 52, 53, 49, 
    58, 53, 45, 52, 53, 49, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[96]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 95), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 111, 97, 117, 116, 104, 47, 99, 111, 100, 101, 120, 
    46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 
    105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 
    69, 114, 114, 111, 114, 46, 77, 111, 111, 110, 66, 105, 116, 84, 
    101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 114, 
    110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 111, 97, 117, 116, 104, 47, 99, 111, 100, 101, 
    120, 58, 112, 107, 99, 101, 46, 109, 98, 116, 58, 50, 52, 58, 49, 
    48, 45, 50, 52, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 102, 102, 101, 114, 58, 98, 117, 102, 
    102, 101, 114, 46, 109, 98, 116, 58, 56, 49, 49, 58, 49, 48, 45, 
    56, 49, 49, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 111, 97, 117, 116, 104, 47, 99, 111, 100, 101, 
    120, 58, 112, 107, 99, 101, 46, 109, 98, 116, 58, 54, 48, 58, 51, 
    45, 54, 48, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[58]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 57), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 111, 97, 117, 116, 104, 47, 99, 111, 100, 101, 120, 34, 44, 32, 
    34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    32, 70, 65, 73, 76, 69, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    114, 101, 102, 114, 101, 115, 104, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_50 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    44, 32, 108, 101, 110, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 111, 97, 117, 116, 104, 47, 99, 111, 100, 101, 
    120, 58, 112, 107, 99, 101, 46, 109, 98, 116, 58, 54, 49, 58, 51, 
    45, 54, 49, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    112, 107, 99, 101, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    98, 111, 117, 110, 100, 115, 32, 99, 104, 101, 99, 107, 32, 102, 
    97, 105, 108, 101, 100, 58, 32, 100, 115, 116, 95, 111, 102, 102, 
    115, 101, 116, 32, 61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    105, 110, 115, 116, 114, 117, 99, 116, 105, 111, 110, 46, 109, 98, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_56 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[52]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 51), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 97, 114, 
    114, 97, 121, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 51, 53, 
    58, 53, 45, 49, 51, 55, 58, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 118, 105, 101, 119, 46, 109, 98, 116, 58, 49, 56, 
    48, 58, 53, 45, 49, 56, 48, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[85]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 84), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 114, 97, 110, 100, 111, 109, 47, 105, 110, 116, 
    101, 114, 110, 97, 108, 47, 114, 97, 110, 100, 111, 109, 95, 115, 
    111, 117, 114, 99, 101, 58, 114, 97, 110, 100, 111, 109, 95, 115, 
    111, 117, 114, 99, 101, 95, 99, 104, 97, 99, 104, 97, 46, 109, 98, 
    116, 58, 51, 50, 58, 49, 50, 45, 51, 50, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[43]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 42), 
    105, 110, 100, 101, 120, 32, 111, 117, 116, 32, 111, 102, 32, 98, 
    111, 117, 110, 100, 115, 58, 32, 116, 104, 101, 32, 108, 101, 110, 
    32, 105, 115, 32, 102, 114, 111, 109, 32, 48, 32, 116, 111, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_54 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 105, 
    116, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 54, 57, 
    49, 58, 49, 48, 45, 54, 57, 49, 58, 53, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 111, 97, 117, 116, 104, 47, 99, 111, 100, 101, 
    120, 58, 112, 107, 99, 101, 46, 109, 98, 116, 58, 54, 50, 58, 51, 
    45, 54, 50, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    44, 32, 115, 114, 99, 95, 111, 102, 102, 115, 101, 116, 32, 61, 32, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    108, 111, 103, 103, 101, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[98]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 97), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 111, 97, 117, 116, 104, 47, 99, 111, 100, 101, 120, 
    46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 
    105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 
    105, 112, 84, 101, 115, 116, 46, 77, 111, 111, 110, 66, 105, 116, 
    84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 116, 101, 
    114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    44, 32, 100, 115, 116, 46, 108, 101, 110, 103, 116, 104, 32, 61, 
    32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    32, 98, 117, 116, 32, 116, 104, 101, 32, 105, 110, 100, 101, 120, 
    32, 105, 115, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    111, 97, 117, 116, 104, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_55 =
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

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_2 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    45, 45, 45, 45, 45, 32, 66, 69, 71, 73, 78, 32, 77, 79, 79, 78, 32, 
    84, 69, 83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    96, 32, 105, 115, 32, 110, 111, 116, 32, 116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    69, 114, 114, 110, 111, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    101, 114, 114, 111, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[59]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 58), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 102, 105, 
    120, 101, 100, 97, 114, 114, 97, 121, 95, 98, 108, 111, 99, 107, 
    46, 109, 98, 116, 58, 49, 49, 53, 58, 53, 45, 49, 49, 55, 58, 54, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_24 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 96, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[47]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 46), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 114, 97, 110, 100, 111, 109, 58, 114, 97, 110, 
    100, 111, 109, 46, 109, 98, 116, 58, 52, 51, 58, 53, 45, 52, 51, 
    58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_49 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[14]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 13), 
    99, 111, 110, 115, 116, 97, 110, 116, 115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    103, 101, 110, 101, 114, 97, 116, 101, 95, 112, 107, 99, 101, 32, 
    112, 114, 111, 100, 117, 99, 101, 115, 32, 118, 97, 108, 105, 100, 
    32, 99, 111, 100, 101, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 32, 115, 101, 108, 102, 46, 108, 101, 110, 103, 116, 104, 32, 
    61, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[34]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 33), 
    70, 97, 105, 108, 101, 100, 32, 116, 111, 32, 99, 114, 101, 97, 116, 
    101, 32, 114, 97, 110, 100, 111, 109, 32, 103, 101, 110, 101, 114, 
    97, 116, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[27]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 26), 
    115, 101, 101, 100, 32, 109, 117, 115, 116, 32, 98, 101, 32, 51, 
    50, 32, 98, 121, 116, 101, 115, 32, 108, 111, 110, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[40]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 39), 
    73, 110, 118, 97, 108, 105, 100, 32, 98, 121, 116, 101, 32, 99, 111, 
    117, 110, 116, 32, 102, 111, 114, 32, 105, 110, 116, 51, 50, 32, 
    101, 120, 116, 114, 97, 99, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    99, 114, 101, 100, 101, 110, 116, 105, 97, 108, 115, 46, 109, 98, 
    116, 0
  };

struct { int32_t rc; uint32_t meta; uint8_t const data[1]; 
} const moonbit_bytes_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 0, 0), 0};

struct moonbit_object const moonbit_constant_constructor_0 =
  { -1, Moonbit_make_regular_object_header(2, 0, 0)};

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP48clawteam8clawteam5oauth5codex39____test__706b63652e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5oauth5codex39____test__706b63652e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__executeN17error__to__stringS1371$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__executeN17error__to__stringS1371
  };

struct _M0TWEuQRPC15error5Error* _M0FP48clawteam8clawteam5oauth5codex35____test__706b63652e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP48clawteam8clawteam5oauth5codex39____test__706b63652e6d6274__0_2edyncall$closure.data;

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

struct { int32_t rc; uint32_t meta; struct _M0BTPC16random6Source data; 
} _M0FP0139moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2fChaCha8_2eas___40moonbitlang_2fcore_2frandom_2eSource_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPC16random6Source) >> 2, 0, 0),
    {.$method_0 = _M0IPC36random8internal14random__source7ChaCha8PC16random6Source59next_2edyncall__as___40moonbitlang_2fcore_2frandom_2eSource}
  };

struct _M0BTPC16random6Source* _M0FP0139moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2fChaCha8_2eas___40moonbitlang_2fcore_2frandom_2eSource_2estatic__method__table__id =
  &_M0FP0139moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2fChaCha8_2eas___40moonbitlang_2fcore_2frandom_2eSource_2estatic__method__table__id$object.data;

uint32_t* _M0FP311moonbitlang1x6crypto9sha256__t;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP48clawteam8clawteam5oauth5codex48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP48clawteam8clawteam5oauth5codex39____test__706b63652e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS3769
) {
  return _M0FP48clawteam8clawteam5oauth5codex29____test__706b63652e6d6274__0();
}

int32_t _M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1392,
  moonbit_string_t _M0L8filenameS1367,
  int32_t _M0L5indexS1370
) {
  struct _M0R109_24clawteam_2fclawteam_2foauth_2fcodex_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362* _closure_4475;
  struct _M0TWssbEu* _M0L14handle__resultS1362;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1371;
  void* _M0L11_2atry__errS1386;
  struct moonbit_result_0 _tmp_4477;
  int32_t _handle__error__result_4478;
  int32_t _M0L6_2atmpS3757;
  void* _M0L3errS1387;
  moonbit_string_t _M0L4nameS1389;
  struct _M0DTPC15error5Error107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1390;
  moonbit_string_t _M0L8_2afieldS3770;
  int32_t _M0L6_2acntS4390;
  moonbit_string_t _M0L7_2anameS1391;
  #line 538 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  moonbit_incref(_M0L8filenameS1367);
  _closure_4475
  = (struct _M0R109_24clawteam_2fclawteam_2foauth_2fcodex_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362*)moonbit_malloc(sizeof(struct _M0R109_24clawteam_2fclawteam_2foauth_2fcodex_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362));
  Moonbit_object_header(_closure_4475)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R109_24clawteam_2fclawteam_2foauth_2fcodex_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362, $1) >> 2, 1, 0);
  _closure_4475->code
  = &_M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__executeN14handle__resultS1362;
  _closure_4475->$0 = _M0L5indexS1370;
  _closure_4475->$1 = _M0L8filenameS1367;
  _M0L14handle__resultS1362 = (struct _M0TWssbEu*)_closure_4475;
  _M0L17error__to__stringS1371
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__executeN17error__to__stringS1371$closure.data;
  moonbit_incref(_M0L12async__testsS1392);
  moonbit_incref(_M0L17error__to__stringS1371);
  moonbit_incref(_M0L8filenameS1367);
  moonbit_incref(_M0L14handle__resultS1362);
  #line 572 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _tmp_4477
  = _M0IP48clawteam8clawteam5oauth5codex41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__test(_M0L12async__testsS1392, _M0L8filenameS1367, _M0L5indexS1370, _M0L14handle__resultS1362, _M0L17error__to__stringS1371);
  if (_tmp_4477.tag) {
    int32_t const _M0L5_2aokS3766 = _tmp_4477.data.ok;
    _handle__error__result_4478 = _M0L5_2aokS3766;
  } else {
    void* const _M0L6_2aerrS3767 = _tmp_4477.data.err;
    moonbit_decref(_M0L12async__testsS1392);
    moonbit_decref(_M0L17error__to__stringS1371);
    moonbit_decref(_M0L8filenameS1367);
    _M0L11_2atry__errS1386 = _M0L6_2aerrS3767;
    goto join_1385;
  }
  if (_handle__error__result_4478) {
    moonbit_decref(_M0L12async__testsS1392);
    moonbit_decref(_M0L17error__to__stringS1371);
    moonbit_decref(_M0L8filenameS1367);
    _M0L6_2atmpS3757 = 1;
  } else {
    struct moonbit_result_0 _tmp_4479;
    int32_t _handle__error__result_4480;
    moonbit_incref(_M0L12async__testsS1392);
    moonbit_incref(_M0L17error__to__stringS1371);
    moonbit_incref(_M0L8filenameS1367);
    moonbit_incref(_M0L14handle__resultS1362);
    #line 575 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
    _tmp_4479
    = _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1392, _M0L8filenameS1367, _M0L5indexS1370, _M0L14handle__resultS1362, _M0L17error__to__stringS1371);
    if (_tmp_4479.tag) {
      int32_t const _M0L5_2aokS3764 = _tmp_4479.data.ok;
      _handle__error__result_4480 = _M0L5_2aokS3764;
    } else {
      void* const _M0L6_2aerrS3765 = _tmp_4479.data.err;
      moonbit_decref(_M0L12async__testsS1392);
      moonbit_decref(_M0L17error__to__stringS1371);
      moonbit_decref(_M0L8filenameS1367);
      _M0L11_2atry__errS1386 = _M0L6_2aerrS3765;
      goto join_1385;
    }
    if (_handle__error__result_4480) {
      moonbit_decref(_M0L12async__testsS1392);
      moonbit_decref(_M0L17error__to__stringS1371);
      moonbit_decref(_M0L8filenameS1367);
      _M0L6_2atmpS3757 = 1;
    } else {
      struct moonbit_result_0 _tmp_4481;
      int32_t _handle__error__result_4482;
      moonbit_incref(_M0L12async__testsS1392);
      moonbit_incref(_M0L17error__to__stringS1371);
      moonbit_incref(_M0L8filenameS1367);
      moonbit_incref(_M0L14handle__resultS1362);
      #line 578 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _tmp_4481
      = _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1392, _M0L8filenameS1367, _M0L5indexS1370, _M0L14handle__resultS1362, _M0L17error__to__stringS1371);
      if (_tmp_4481.tag) {
        int32_t const _M0L5_2aokS3762 = _tmp_4481.data.ok;
        _handle__error__result_4482 = _M0L5_2aokS3762;
      } else {
        void* const _M0L6_2aerrS3763 = _tmp_4481.data.err;
        moonbit_decref(_M0L12async__testsS1392);
        moonbit_decref(_M0L17error__to__stringS1371);
        moonbit_decref(_M0L8filenameS1367);
        _M0L11_2atry__errS1386 = _M0L6_2aerrS3763;
        goto join_1385;
      }
      if (_handle__error__result_4482) {
        moonbit_decref(_M0L12async__testsS1392);
        moonbit_decref(_M0L17error__to__stringS1371);
        moonbit_decref(_M0L8filenameS1367);
        _M0L6_2atmpS3757 = 1;
      } else {
        struct moonbit_result_0 _tmp_4483;
        int32_t _handle__error__result_4484;
        moonbit_incref(_M0L12async__testsS1392);
        moonbit_incref(_M0L17error__to__stringS1371);
        moonbit_incref(_M0L8filenameS1367);
        moonbit_incref(_M0L14handle__resultS1362);
        #line 581 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        _tmp_4483
        = _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1392, _M0L8filenameS1367, _M0L5indexS1370, _M0L14handle__resultS1362, _M0L17error__to__stringS1371);
        if (_tmp_4483.tag) {
          int32_t const _M0L5_2aokS3760 = _tmp_4483.data.ok;
          _handle__error__result_4484 = _M0L5_2aokS3760;
        } else {
          void* const _M0L6_2aerrS3761 = _tmp_4483.data.err;
          moonbit_decref(_M0L12async__testsS1392);
          moonbit_decref(_M0L17error__to__stringS1371);
          moonbit_decref(_M0L8filenameS1367);
          _M0L11_2atry__errS1386 = _M0L6_2aerrS3761;
          goto join_1385;
        }
        if (_handle__error__result_4484) {
          moonbit_decref(_M0L12async__testsS1392);
          moonbit_decref(_M0L17error__to__stringS1371);
          moonbit_decref(_M0L8filenameS1367);
          _M0L6_2atmpS3757 = 1;
        } else {
          struct moonbit_result_0 _tmp_4485;
          moonbit_incref(_M0L14handle__resultS1362);
          #line 584 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
          _tmp_4485
          = _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1392, _M0L8filenameS1367, _M0L5indexS1370, _M0L14handle__resultS1362, _M0L17error__to__stringS1371);
          if (_tmp_4485.tag) {
            int32_t const _M0L5_2aokS3758 = _tmp_4485.data.ok;
            _M0L6_2atmpS3757 = _M0L5_2aokS3758;
          } else {
            void* const _M0L6_2aerrS3759 = _tmp_4485.data.err;
            _M0L11_2atry__errS1386 = _M0L6_2aerrS3759;
            goto join_1385;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS3757) {
    void* _M0L107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3768 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3768)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3768)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1386
    = _M0L107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3768;
    goto join_1385;
  } else {
    moonbit_decref(_M0L14handle__resultS1362);
  }
  goto joinlet_4476;
  join_1385:;
  _M0L3errS1387 = _M0L11_2atry__errS1386;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1390
  = (struct _M0DTPC15error5Error107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1387;
  _M0L8_2afieldS3770 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1390->$0;
  _M0L6_2acntS4390
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1390)->rc;
  if (_M0L6_2acntS4390 > 1) {
    int32_t _M0L11_2anew__cntS4391 = _M0L6_2acntS4390 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1390)->rc
    = _M0L11_2anew__cntS4391;
    moonbit_incref(_M0L8_2afieldS3770);
  } else if (_M0L6_2acntS4390 == 1) {
    #line 591 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1390);
  }
  _M0L7_2anameS1391 = _M0L8_2afieldS3770;
  _M0L4nameS1389 = _M0L7_2anameS1391;
  goto join_1388;
  goto joinlet_4486;
  join_1388:;
  #line 592 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__executeN14handle__resultS1362(_M0L14handle__resultS1362, _M0L4nameS1389, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_4486:;
  joinlet_4476:;
  return 0;
}

moonbit_string_t _M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__executeN17error__to__stringS1371(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS3756,
  void* _M0L3errS1372
) {
  void* _M0L1eS1374;
  moonbit_string_t _M0L1eS1376;
  #line 561 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L6_2aenvS3756);
  switch (Moonbit_object_tag(_M0L3errS1372)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1377 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1372;
      moonbit_string_t _M0L8_2afieldS3771 = _M0L10_2aFailureS1377->$0;
      int32_t _M0L6_2acntS4392 =
        Moonbit_object_header(_M0L10_2aFailureS1377)->rc;
      moonbit_string_t _M0L4_2aeS1378;
      if (_M0L6_2acntS4392 > 1) {
        int32_t _M0L11_2anew__cntS4393 = _M0L6_2acntS4392 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1377)->rc
        = _M0L11_2anew__cntS4393;
        moonbit_incref(_M0L8_2afieldS3771);
      } else if (_M0L6_2acntS4392 == 1) {
        #line 562 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L10_2aFailureS1377);
      }
      _M0L4_2aeS1378 = _M0L8_2afieldS3771;
      _M0L1eS1376 = _M0L4_2aeS1378;
      goto join_1375;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1379 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1372;
      moonbit_string_t _M0L8_2afieldS3772 = _M0L15_2aInspectErrorS1379->$0;
      int32_t _M0L6_2acntS4394 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1379)->rc;
      moonbit_string_t _M0L4_2aeS1380;
      if (_M0L6_2acntS4394 > 1) {
        int32_t _M0L11_2anew__cntS4395 = _M0L6_2acntS4394 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1379)->rc
        = _M0L11_2anew__cntS4395;
        moonbit_incref(_M0L8_2afieldS3772);
      } else if (_M0L6_2acntS4394 == 1) {
        #line 562 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1379);
      }
      _M0L4_2aeS1380 = _M0L8_2afieldS3772;
      _M0L1eS1376 = _M0L4_2aeS1380;
      goto join_1375;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1381 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1372;
      moonbit_string_t _M0L8_2afieldS3773 = _M0L16_2aSnapshotErrorS1381->$0;
      int32_t _M0L6_2acntS4396 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1381)->rc;
      moonbit_string_t _M0L4_2aeS1382;
      if (_M0L6_2acntS4396 > 1) {
        int32_t _M0L11_2anew__cntS4397 = _M0L6_2acntS4396 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1381)->rc
        = _M0L11_2anew__cntS4397;
        moonbit_incref(_M0L8_2afieldS3773);
      } else if (_M0L6_2acntS4396 == 1) {
        #line 562 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1381);
      }
      _M0L4_2aeS1382 = _M0L8_2afieldS3773;
      _M0L1eS1376 = _M0L4_2aeS1382;
      goto join_1375;
      break;
    }
    
    case 5: {
      struct _M0DTPC15error5Error105clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1383 =
        (struct _M0DTPC15error5Error105clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1372;
      moonbit_string_t _M0L8_2afieldS3774 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1383->$0;
      int32_t _M0L6_2acntS4398 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1383)->rc;
      moonbit_string_t _M0L4_2aeS1384;
      if (_M0L6_2acntS4398 > 1) {
        int32_t _M0L11_2anew__cntS4399 = _M0L6_2acntS4398 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1383)->rc
        = _M0L11_2anew__cntS4399;
        moonbit_incref(_M0L8_2afieldS3774);
      } else if (_M0L6_2acntS4398 == 1) {
        #line 562 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1383);
      }
      _M0L4_2aeS1384 = _M0L8_2afieldS3774;
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
  #line 567 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1374);
}

int32_t _M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__executeN14handle__resultS1362(
  struct _M0TWssbEu* _M0L6_2aenvS3742,
  moonbit_string_t _M0L8testnameS1363,
  moonbit_string_t _M0L7messageS1364,
  int32_t _M0L7skippedS1365
) {
  struct _M0R109_24clawteam_2fclawteam_2foauth_2fcodex_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362* _M0L14_2acasted__envS3743;
  moonbit_string_t _M0L8_2afieldS3784;
  moonbit_string_t _M0L8filenameS1367;
  int32_t _M0L8_2afieldS3783;
  int32_t _M0L6_2acntS4400;
  int32_t _M0L5indexS1370;
  int32_t _if__result_4489;
  moonbit_string_t _M0L10file__nameS1366;
  moonbit_string_t _M0L10test__nameS1368;
  moonbit_string_t _M0L7messageS1369;
  moonbit_string_t _M0L6_2atmpS3755;
  moonbit_string_t _M0L6_2atmpS3782;
  moonbit_string_t _M0L6_2atmpS3754;
  moonbit_string_t _M0L6_2atmpS3781;
  moonbit_string_t _M0L6_2atmpS3752;
  moonbit_string_t _M0L6_2atmpS3753;
  moonbit_string_t _M0L6_2atmpS3780;
  moonbit_string_t _M0L6_2atmpS3751;
  moonbit_string_t _M0L6_2atmpS3779;
  moonbit_string_t _M0L6_2atmpS3749;
  moonbit_string_t _M0L6_2atmpS3750;
  moonbit_string_t _M0L6_2atmpS3778;
  moonbit_string_t _M0L6_2atmpS3748;
  moonbit_string_t _M0L6_2atmpS3777;
  moonbit_string_t _M0L6_2atmpS3746;
  moonbit_string_t _M0L6_2atmpS3747;
  moonbit_string_t _M0L6_2atmpS3776;
  moonbit_string_t _M0L6_2atmpS3745;
  moonbit_string_t _M0L6_2atmpS3775;
  moonbit_string_t _M0L6_2atmpS3744;
  #line 545 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3743
  = (struct _M0R109_24clawteam_2fclawteam_2foauth_2fcodex_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1362*)_M0L6_2aenvS3742;
  _M0L8_2afieldS3784 = _M0L14_2acasted__envS3743->$1;
  _M0L8filenameS1367 = _M0L8_2afieldS3784;
  _M0L8_2afieldS3783 = _M0L14_2acasted__envS3743->$0;
  _M0L6_2acntS4400 = Moonbit_object_header(_M0L14_2acasted__envS3743)->rc;
  if (_M0L6_2acntS4400 > 1) {
    int32_t _M0L11_2anew__cntS4401 = _M0L6_2acntS4400 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3743)->rc
    = _M0L11_2anew__cntS4401;
    moonbit_incref(_M0L8filenameS1367);
  } else if (_M0L6_2acntS4400 == 1) {
    #line 545 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3743);
  }
  _M0L5indexS1370 = _M0L8_2afieldS3783;
  if (!_M0L7skippedS1365) {
    _if__result_4489 = 1;
  } else {
    _if__result_4489 = 0;
  }
  if (_if__result_4489) {
    
  }
  #line 551 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L10file__nameS1366 = _M0MPC16string6String6escape(_M0L8filenameS1367);
  #line 552 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L10test__nameS1368 = _M0MPC16string6String6escape(_M0L8testnameS1363);
  #line 553 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L7messageS1369 = _M0MPC16string6String6escape(_M0L7messageS1364);
  #line 554 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 556 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3755
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1366);
  #line 555 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3782
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS3755);
  moonbit_decref(_M0L6_2atmpS3755);
  _M0L6_2atmpS3754 = _M0L6_2atmpS3782;
  #line 555 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3781
  = moonbit_add_string(_M0L6_2atmpS3754, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS3754);
  _M0L6_2atmpS3752 = _M0L6_2atmpS3781;
  #line 556 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3753
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1370);
  #line 555 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3780 = moonbit_add_string(_M0L6_2atmpS3752, _M0L6_2atmpS3753);
  moonbit_decref(_M0L6_2atmpS3752);
  moonbit_decref(_M0L6_2atmpS3753);
  _M0L6_2atmpS3751 = _M0L6_2atmpS3780;
  #line 555 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3779
  = moonbit_add_string(_M0L6_2atmpS3751, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS3751);
  _M0L6_2atmpS3749 = _M0L6_2atmpS3779;
  #line 556 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3750
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1368);
  #line 555 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3778 = moonbit_add_string(_M0L6_2atmpS3749, _M0L6_2atmpS3750);
  moonbit_decref(_M0L6_2atmpS3749);
  moonbit_decref(_M0L6_2atmpS3750);
  _M0L6_2atmpS3748 = _M0L6_2atmpS3778;
  #line 555 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3777
  = moonbit_add_string(_M0L6_2atmpS3748, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS3748);
  _M0L6_2atmpS3746 = _M0L6_2atmpS3777;
  #line 556 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3747
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1369);
  #line 555 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3776 = moonbit_add_string(_M0L6_2atmpS3746, _M0L6_2atmpS3747);
  moonbit_decref(_M0L6_2atmpS3746);
  moonbit_decref(_M0L6_2atmpS3747);
  _M0L6_2atmpS3745 = _M0L6_2atmpS3776;
  #line 555 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3775
  = moonbit_add_string(_M0L6_2atmpS3745, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS3745);
  _M0L6_2atmpS3744 = _M0L6_2atmpS3775;
  #line 555 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS3744);
  #line 558 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP48clawteam8clawteam5oauth5codex41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__test(
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
  int32_t _M0L6_2atmpS3741;
  struct _M0TWEOs* _M0L5_2aitS1336;
  struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3738__l439__* _closure_4498;
  struct _M0TWEOc* _M0L6_2atmpS3732;
  struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3734__l440__* _closure_4499;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS3733;
  struct moonbit_result_0 _result_4500;
  #line 419 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1361);
  moonbit_incref(_M0FP48clawteam8clawteam5oauth5codex48moonbit__test__driver__internal__no__args__tests);
  #line 426 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1357
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP48clawteam8clawteam5oauth5codex48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1358);
  if (_M0L7_2abindS1357 == 0) {
    struct moonbit_result_0 _result_4491;
    if (_M0L7_2abindS1357) {
      moonbit_decref(_M0L7_2abindS1357);
    }
    moonbit_decref(_M0L17error__to__stringS1350);
    moonbit_decref(_M0L14handle__resultS1348);
    _result_4491.tag = 1;
    _result_4491.data.ok = 0;
    return _result_4491;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1359 =
      _M0L7_2abindS1357;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1360 =
      _M0L7_2aSomeS1359;
    _M0L10index__mapS1328 = _M0L13_2aindex__mapS1360;
    goto join_1327;
  }
  join_1327:;
  #line 428 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1351
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1328, _M0L5indexS1352);
  if (_M0L7_2abindS1351 == 0) {
    struct moonbit_result_0 _result_4493;
    if (_M0L7_2abindS1351) {
      moonbit_decref(_M0L7_2abindS1351);
    }
    moonbit_decref(_M0L17error__to__stringS1350);
    moonbit_decref(_M0L14handle__resultS1348);
    _result_4493.tag = 1;
    _result_4493.data.ok = 0;
    return _result_4493;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1353 =
      _M0L7_2abindS1351;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1354 = _M0L7_2aSomeS1353;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS3788 = _M0L4_2axS1354->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1355 = _M0L8_2afieldS3788;
    moonbit_string_t* _M0L8_2afieldS3787 = _M0L4_2axS1354->$1;
    int32_t _M0L6_2acntS4402 = Moonbit_object_header(_M0L4_2axS1354)->rc;
    moonbit_string_t* _M0L8_2aattrsS1356;
    if (_M0L6_2acntS4402 > 1) {
      int32_t _M0L11_2anew__cntS4403 = _M0L6_2acntS4402 - 1;
      Moonbit_object_header(_M0L4_2axS1354)->rc = _M0L11_2anew__cntS4403;
      moonbit_incref(_M0L8_2afieldS3787);
      moonbit_incref(_M0L4_2afS1355);
    } else if (_M0L6_2acntS4402 == 1) {
      #line 426 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      moonbit_free(_M0L4_2axS1354);
    }
    _M0L8_2aattrsS1356 = _M0L8_2afieldS3787;
    _M0L1fS1330 = _M0L4_2afS1355;
    _M0L5attrsS1331 = _M0L8_2aattrsS1356;
    goto join_1329;
  }
  join_1329:;
  _M0L6_2atmpS3741 = Moonbit_array_length(_M0L5attrsS1331);
  if (_M0L6_2atmpS3741 >= 1) {
    moonbit_string_t _M0L6_2atmpS3786 = (moonbit_string_t)_M0L5attrsS1331[0];
    moonbit_string_t _M0L7_2anameS1335 = _M0L6_2atmpS3786;
    moonbit_incref(_M0L7_2anameS1335);
    _M0L4nameS1334 = _M0L7_2anameS1335;
    goto join_1333;
  } else {
    _M0L4nameS1332 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_4494;
  join_1333:;
  _M0L4nameS1332 = _M0L4nameS1334;
  joinlet_4494:;
  #line 429 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L5_2aitS1336 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1331);
  while (1) {
    moonbit_string_t _M0L4attrS1338;
    moonbit_string_t _M0L7_2abindS1345;
    int32_t _M0L6_2atmpS3725;
    int64_t _M0L6_2atmpS3724;
    moonbit_incref(_M0L5_2aitS1336);
    #line 431 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
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
    goto joinlet_4496;
    join_1337:;
    _M0L6_2atmpS3725 = Moonbit_array_length(_M0L4attrS1338);
    _M0L6_2atmpS3724 = (int64_t)_M0L6_2atmpS3725;
    moonbit_incref(_M0L4attrS1338);
    #line 432 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1338, 5, 0, _M0L6_2atmpS3724)
    ) {
      int32_t _M0L6_2atmpS3731 = _M0L4attrS1338[0];
      int32_t _M0L4_2axS1339 = _M0L6_2atmpS3731;
      if (_M0L4_2axS1339 == 112) {
        int32_t _M0L6_2atmpS3730 = _M0L4attrS1338[1];
        int32_t _M0L4_2axS1340 = _M0L6_2atmpS3730;
        if (_M0L4_2axS1340 == 97) {
          int32_t _M0L6_2atmpS3729 = _M0L4attrS1338[2];
          int32_t _M0L4_2axS1341 = _M0L6_2atmpS3729;
          if (_M0L4_2axS1341 == 110) {
            int32_t _M0L6_2atmpS3728 = _M0L4attrS1338[3];
            int32_t _M0L4_2axS1342 = _M0L6_2atmpS3728;
            if (_M0L4_2axS1342 == 105) {
              int32_t _M0L6_2atmpS3785 = _M0L4attrS1338[4];
              int32_t _M0L6_2atmpS3727;
              int32_t _M0L4_2axS1343;
              moonbit_decref(_M0L4attrS1338);
              _M0L6_2atmpS3727 = _M0L6_2atmpS3785;
              _M0L4_2axS1343 = _M0L6_2atmpS3727;
              if (_M0L4_2axS1343 == 99) {
                void* _M0L107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3726;
                struct moonbit_result_0 _result_4497;
                moonbit_decref(_M0L17error__to__stringS1350);
                moonbit_decref(_M0L14handle__resultS1348);
                moonbit_decref(_M0L5_2aitS1336);
                moonbit_decref(_M0L1fS1330);
                _M0L107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3726
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3726)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3726)->$0
                = _M0L4nameS1332;
                _result_4497.tag = 0;
                _result_4497.data.err
                = _M0L107clawteam_2fclawteam_2foauth_2fcodex_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS3726;
                return _result_4497;
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
    joinlet_4496:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1348);
  moonbit_incref(_M0L4nameS1332);
  _closure_4498
  = (struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3738__l439__*)moonbit_malloc(sizeof(struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3738__l439__));
  Moonbit_object_header(_closure_4498)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3738__l439__, $0) >> 2, 2, 0);
  _closure_4498->code
  = &_M0IP48clawteam8clawteam5oauth5codex41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testC3738l439;
  _closure_4498->$0 = _M0L14handle__resultS1348;
  _closure_4498->$1 = _M0L4nameS1332;
  _M0L6_2atmpS3732 = (struct _M0TWEOc*)_closure_4498;
  _closure_4499
  = (struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3734__l440__*)moonbit_malloc(sizeof(struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3734__l440__));
  Moonbit_object_header(_closure_4499)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3734__l440__, $0) >> 2, 3, 0);
  _closure_4499->code
  = &_M0IP48clawteam8clawteam5oauth5codex41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testC3734l440;
  _closure_4499->$0 = _M0L17error__to__stringS1350;
  _closure_4499->$1 = _M0L14handle__resultS1348;
  _closure_4499->$2 = _M0L4nameS1332;
  _M0L6_2atmpS3733 = (struct _M0TWRPC15error5ErrorEu*)_closure_4499;
  #line 437 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5oauth5codex45moonbit__test__driver__internal__catch__error(_M0L1fS1330, _M0L6_2atmpS3732, _M0L6_2atmpS3733);
  _result_4500.tag = 1;
  _result_4500.data.ok = 1;
  return _result_4500;
}

int32_t _M0IP48clawteam8clawteam5oauth5codex41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testC3738l439(
  struct _M0TWEOc* _M0L6_2aenvS3739
) {
  struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3738__l439__* _M0L14_2acasted__envS3740;
  moonbit_string_t _M0L8_2afieldS3790;
  moonbit_string_t _M0L4nameS1332;
  struct _M0TWssbEu* _M0L8_2afieldS3789;
  int32_t _M0L6_2acntS4404;
  struct _M0TWssbEu* _M0L14handle__resultS1348;
  #line 439 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3740
  = (struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3738__l439__*)_M0L6_2aenvS3739;
  _M0L8_2afieldS3790 = _M0L14_2acasted__envS3740->$1;
  _M0L4nameS1332 = _M0L8_2afieldS3790;
  _M0L8_2afieldS3789 = _M0L14_2acasted__envS3740->$0;
  _M0L6_2acntS4404 = Moonbit_object_header(_M0L14_2acasted__envS3740)->rc;
  if (_M0L6_2acntS4404 > 1) {
    int32_t _M0L11_2anew__cntS4405 = _M0L6_2acntS4404 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3740)->rc
    = _M0L11_2anew__cntS4405;
    moonbit_incref(_M0L4nameS1332);
    moonbit_incref(_M0L8_2afieldS3789);
  } else if (_M0L6_2acntS4404 == 1) {
    #line 439 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3740);
  }
  _M0L14handle__resultS1348 = _M0L8_2afieldS3789;
  #line 439 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1348->code(_M0L14handle__resultS1348, _M0L4nameS1332, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP48clawteam8clawteam5oauth5codex41MoonBit__Test__Driver__Internal__No__ArgsP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testC3734l440(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS3735,
  void* _M0L3errS1349
) {
  struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3734__l440__* _M0L14_2acasted__envS3736;
  moonbit_string_t _M0L8_2afieldS3793;
  moonbit_string_t _M0L4nameS1332;
  struct _M0TWssbEu* _M0L8_2afieldS3792;
  struct _M0TWssbEu* _M0L14handle__resultS1348;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS3791;
  int32_t _M0L6_2acntS4406;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1350;
  moonbit_string_t _M0L6_2atmpS3737;
  #line 440 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L14_2acasted__envS3736
  = (struct _M0R187_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2foauth_2fcodex_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u3734__l440__*)_M0L6_2aenvS3735;
  _M0L8_2afieldS3793 = _M0L14_2acasted__envS3736->$2;
  _M0L4nameS1332 = _M0L8_2afieldS3793;
  _M0L8_2afieldS3792 = _M0L14_2acasted__envS3736->$1;
  _M0L14handle__resultS1348 = _M0L8_2afieldS3792;
  _M0L8_2afieldS3791 = _M0L14_2acasted__envS3736->$0;
  _M0L6_2acntS4406 = Moonbit_object_header(_M0L14_2acasted__envS3736)->rc;
  if (_M0L6_2acntS4406 > 1) {
    int32_t _M0L11_2anew__cntS4407 = _M0L6_2acntS4406 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3736)->rc
    = _M0L11_2anew__cntS4407;
    moonbit_incref(_M0L4nameS1332);
    moonbit_incref(_M0L14handle__resultS1348);
    moonbit_incref(_M0L8_2afieldS3791);
  } else if (_M0L6_2acntS4406 == 1) {
    #line 440 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
    moonbit_free(_M0L14_2acasted__envS3736);
  }
  _M0L17error__to__stringS1350 = _M0L8_2afieldS3791;
  #line 440 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3737
  = _M0L17error__to__stringS1350->code(_M0L17error__to__stringS1350, _M0L3errS1349);
  #line 440 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L14handle__resultS1348->code(_M0L14handle__resultS1348, _M0L4nameS1332, _M0L6_2atmpS3737, 0);
  return 0;
}

int32_t _M0FP48clawteam8clawteam5oauth5codex45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1316,
  struct _M0TWEOc* _M0L6on__okS1317,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1314
) {
  void* _M0L11_2atry__errS1312;
  struct moonbit_result_0 _tmp_4502;
  void* _M0L3errS1313;
  #line 375 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _tmp_4502 = _M0L1fS1316->code(_M0L1fS1316);
  if (_tmp_4502.tag) {
    int32_t const _M0L5_2aokS3722 = _tmp_4502.data.ok;
    moonbit_decref(_M0L7on__errS1314);
  } else {
    void* const _M0L6_2aerrS3723 = _tmp_4502.data.err;
    moonbit_decref(_M0L6on__okS1317);
    _M0L11_2atry__errS1312 = _M0L6_2aerrS3723;
    goto join_1311;
  }
  #line 382 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6on__okS1317->code(_M0L6on__okS1317);
  goto joinlet_4501;
  join_1311:;
  _M0L3errS1313 = _M0L11_2atry__errS1312;
  #line 383 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L7on__errS1314->code(_M0L7on__errS1314, _M0L3errS1313);
  joinlet_4501:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1271;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1277;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1284;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1289;
  struct _M0TUsiE** _M0L6_2atmpS3721;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1296;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1297;
  moonbit_string_t _M0L6_2atmpS3720;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1298;
  int32_t _M0L7_2abindS1299;
  int32_t _M0L2__S1300;
  #line 193 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1271 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1277
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1284
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1277;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1289 = 0;
  _M0L6_2atmpS3721 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1296
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1296)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1296->$0 = _M0L6_2atmpS3721;
  _M0L16file__and__indexS1296->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L9cli__argsS1297
  = _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1284(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1284);
  #line 284 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3720 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1297, 1);
  #line 283 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L10test__argsS1298
  = _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1289(_M0L51moonbit__test__driver__internal__split__mbt__stringS1289, _M0L6_2atmpS3720, 47);
  _M0L7_2abindS1299 = _M0L10test__argsS1298->$1;
  _M0L2__S1300 = 0;
  while (1) {
    if (_M0L2__S1300 < _M0L7_2abindS1299) {
      moonbit_string_t* _M0L8_2afieldS3795 = _M0L10test__argsS1298->$0;
      moonbit_string_t* _M0L3bufS3719 = _M0L8_2afieldS3795;
      moonbit_string_t _M0L6_2atmpS3794 =
        (moonbit_string_t)_M0L3bufS3719[_M0L2__S1300];
      moonbit_string_t _M0L3argS1301 = _M0L6_2atmpS3794;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1302;
      moonbit_string_t _M0L4fileS1303;
      moonbit_string_t _M0L5rangeS1304;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1305;
      moonbit_string_t _M0L6_2atmpS3717;
      int32_t _M0L5startS1306;
      moonbit_string_t _M0L6_2atmpS3716;
      int32_t _M0L3endS1307;
      int32_t _M0L1iS1308;
      int32_t _M0L6_2atmpS3718;
      moonbit_incref(_M0L3argS1301);
      #line 288 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _M0L16file__and__rangeS1302
      = _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1289(_M0L51moonbit__test__driver__internal__split__mbt__stringS1289, _M0L3argS1301, 58);
      moonbit_incref(_M0L16file__and__rangeS1302);
      #line 289 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _M0L4fileS1303
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1302, 0);
      #line 290 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _M0L5rangeS1304
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1302, 1);
      #line 291 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _M0L15start__and__endS1305
      = _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1289(_M0L51moonbit__test__driver__internal__split__mbt__stringS1289, _M0L5rangeS1304, 45);
      moonbit_incref(_M0L15start__and__endS1305);
      #line 294 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3717
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1305, 0);
      #line 294 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _M0L5startS1306
      = _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1271(_M0L45moonbit__test__driver__internal__parse__int__S1271, _M0L6_2atmpS3717);
      #line 295 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3716
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1305, 1);
      #line 295 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _M0L3endS1307
      = _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1271(_M0L45moonbit__test__driver__internal__parse__int__S1271, _M0L6_2atmpS3716);
      _M0L1iS1308 = _M0L5startS1306;
      while (1) {
        if (_M0L1iS1308 < _M0L3endS1307) {
          struct _M0TUsiE* _M0L8_2atupleS3714;
          int32_t _M0L6_2atmpS3715;
          moonbit_incref(_M0L4fileS1303);
          _M0L8_2atupleS3714
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS3714)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS3714->$0 = _M0L4fileS1303;
          _M0L8_2atupleS3714->$1 = _M0L1iS1308;
          moonbit_incref(_M0L16file__and__indexS1296);
          #line 297 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1296, _M0L8_2atupleS3714);
          _M0L6_2atmpS3715 = _M0L1iS1308 + 1;
          _M0L1iS1308 = _M0L6_2atmpS3715;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1303);
        }
        break;
      }
      _M0L6_2atmpS3718 = _M0L2__S1300 + 1;
      _M0L2__S1300 = _M0L6_2atmpS3718;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1298);
    }
    break;
  }
  return _M0L16file__and__indexS1296;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1289(
  int32_t _M0L6_2aenvS3695,
  moonbit_string_t _M0L1sS1290,
  int32_t _M0L3sepS1291
) {
  moonbit_string_t* _M0L6_2atmpS3713;
  struct _M0TPB5ArrayGsE* _M0L3resS1292;
  struct _M0TPC13ref3RefGiE* _M0L1iS1293;
  struct _M0TPC13ref3RefGiE* _M0L5startS1294;
  int32_t _M0L3valS3708;
  int32_t _M0L6_2atmpS3709;
  #line 261 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS3713 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1292
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1292)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1292->$0 = _M0L6_2atmpS3713;
  _M0L3resS1292->$1 = 0;
  _M0L1iS1293
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1293)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1293->$0 = 0;
  _M0L5startS1294
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1294)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1294->$0 = 0;
  while (1) {
    int32_t _M0L3valS3696 = _M0L1iS1293->$0;
    int32_t _M0L6_2atmpS3697 = Moonbit_array_length(_M0L1sS1290);
    if (_M0L3valS3696 < _M0L6_2atmpS3697) {
      int32_t _M0L3valS3700 = _M0L1iS1293->$0;
      int32_t _M0L6_2atmpS3699;
      int32_t _M0L6_2atmpS3698;
      int32_t _M0L3valS3707;
      int32_t _M0L6_2atmpS3706;
      if (
        _M0L3valS3700 < 0
        || _M0L3valS3700 >= Moonbit_array_length(_M0L1sS1290)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3699 = _M0L1sS1290[_M0L3valS3700];
      _M0L6_2atmpS3698 = _M0L6_2atmpS3699;
      if (_M0L6_2atmpS3698 == _M0L3sepS1291) {
        int32_t _M0L3valS3702 = _M0L5startS1294->$0;
        int32_t _M0L3valS3703 = _M0L1iS1293->$0;
        moonbit_string_t _M0L6_2atmpS3701;
        int32_t _M0L3valS3705;
        int32_t _M0L6_2atmpS3704;
        moonbit_incref(_M0L1sS1290);
        #line 270 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        _M0L6_2atmpS3701
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1290, _M0L3valS3702, _M0L3valS3703);
        moonbit_incref(_M0L3resS1292);
        #line 270 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1292, _M0L6_2atmpS3701);
        _M0L3valS3705 = _M0L1iS1293->$0;
        _M0L6_2atmpS3704 = _M0L3valS3705 + 1;
        _M0L5startS1294->$0 = _M0L6_2atmpS3704;
      }
      _M0L3valS3707 = _M0L1iS1293->$0;
      _M0L6_2atmpS3706 = _M0L3valS3707 + 1;
      _M0L1iS1293->$0 = _M0L6_2atmpS3706;
      continue;
    } else {
      moonbit_decref(_M0L1iS1293);
    }
    break;
  }
  _M0L3valS3708 = _M0L5startS1294->$0;
  _M0L6_2atmpS3709 = Moonbit_array_length(_M0L1sS1290);
  if (_M0L3valS3708 < _M0L6_2atmpS3709) {
    int32_t _M0L8_2afieldS3796 = _M0L5startS1294->$0;
    int32_t _M0L3valS3711;
    int32_t _M0L6_2atmpS3712;
    moonbit_string_t _M0L6_2atmpS3710;
    moonbit_decref(_M0L5startS1294);
    _M0L3valS3711 = _M0L8_2afieldS3796;
    _M0L6_2atmpS3712 = Moonbit_array_length(_M0L1sS1290);
    #line 276 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
    _M0L6_2atmpS3710
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1290, _M0L3valS3711, _M0L6_2atmpS3712);
    moonbit_incref(_M0L3resS1292);
    #line 276 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1292, _M0L6_2atmpS3710);
  } else {
    moonbit_decref(_M0L5startS1294);
    moonbit_decref(_M0L1sS1290);
  }
  return _M0L3resS1292;
}

struct _M0TPB5ArrayGsE* _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1284(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1277
) {
  moonbit_bytes_t* _M0L3tmpS1285;
  int32_t _M0L6_2atmpS3694;
  struct _M0TPB5ArrayGsE* _M0L3resS1286;
  int32_t _M0L1iS1287;
  #line 250 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L3tmpS1285
  = _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS3694 = Moonbit_array_length(_M0L3tmpS1285);
  #line 254 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1286 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS3694);
  _M0L1iS1287 = 0;
  while (1) {
    int32_t _M0L6_2atmpS3690 = Moonbit_array_length(_M0L3tmpS1285);
    if (_M0L1iS1287 < _M0L6_2atmpS3690) {
      moonbit_bytes_t _M0L6_2atmpS3797;
      moonbit_bytes_t _M0L6_2atmpS3692;
      moonbit_string_t _M0L6_2atmpS3691;
      int32_t _M0L6_2atmpS3693;
      if (
        _M0L1iS1287 < 0 || _M0L1iS1287 >= Moonbit_array_length(_M0L3tmpS1285)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3797 = (moonbit_bytes_t)_M0L3tmpS1285[_M0L1iS1287];
      _M0L6_2atmpS3692 = _M0L6_2atmpS3797;
      moonbit_incref(_M0L6_2atmpS3692);
      #line 256 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _M0L6_2atmpS3691
      = _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1277(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1277, _M0L6_2atmpS3692);
      moonbit_incref(_M0L3resS1286);
      #line 256 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1286, _M0L6_2atmpS3691);
      _M0L6_2atmpS3693 = _M0L1iS1287 + 1;
      _M0L1iS1287 = _M0L6_2atmpS3693;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1285);
    }
    break;
  }
  return _M0L3resS1286;
}

moonbit_string_t _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1277(
  int32_t _M0L6_2aenvS3604,
  moonbit_bytes_t _M0L5bytesS1278
) {
  struct _M0TPB13StringBuilder* _M0L3resS1279;
  int32_t _M0L3lenS1280;
  struct _M0TPC13ref3RefGiE* _M0L1iS1281;
  #line 206 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1279 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1280 = Moonbit_array_length(_M0L5bytesS1278);
  _M0L1iS1281
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1281)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1281->$0 = 0;
  while (1) {
    int32_t _M0L3valS3605 = _M0L1iS1281->$0;
    if (_M0L3valS3605 < _M0L3lenS1280) {
      int32_t _M0L3valS3689 = _M0L1iS1281->$0;
      int32_t _M0L6_2atmpS3688;
      int32_t _M0L6_2atmpS3687;
      struct _M0TPC13ref3RefGiE* _M0L1cS1282;
      int32_t _M0L3valS3606;
      if (
        _M0L3valS3689 < 0
        || _M0L3valS3689 >= Moonbit_array_length(_M0L5bytesS1278)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3688 = _M0L5bytesS1278[_M0L3valS3689];
      _M0L6_2atmpS3687 = (int32_t)_M0L6_2atmpS3688;
      _M0L1cS1282
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1282)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1282->$0 = _M0L6_2atmpS3687;
      _M0L3valS3606 = _M0L1cS1282->$0;
      if (_M0L3valS3606 < 128) {
        int32_t _M0L8_2afieldS3798 = _M0L1cS1282->$0;
        int32_t _M0L3valS3608;
        int32_t _M0L6_2atmpS3607;
        int32_t _M0L3valS3610;
        int32_t _M0L6_2atmpS3609;
        moonbit_decref(_M0L1cS1282);
        _M0L3valS3608 = _M0L8_2afieldS3798;
        _M0L6_2atmpS3607 = _M0L3valS3608;
        moonbit_incref(_M0L3resS1279);
        #line 215 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1279, _M0L6_2atmpS3607);
        _M0L3valS3610 = _M0L1iS1281->$0;
        _M0L6_2atmpS3609 = _M0L3valS3610 + 1;
        _M0L1iS1281->$0 = _M0L6_2atmpS3609;
      } else {
        int32_t _M0L3valS3611 = _M0L1cS1282->$0;
        if (_M0L3valS3611 < 224) {
          int32_t _M0L3valS3613 = _M0L1iS1281->$0;
          int32_t _M0L6_2atmpS3612 = _M0L3valS3613 + 1;
          int32_t _M0L3valS3622;
          int32_t _M0L6_2atmpS3621;
          int32_t _M0L6_2atmpS3615;
          int32_t _M0L3valS3620;
          int32_t _M0L6_2atmpS3619;
          int32_t _M0L6_2atmpS3618;
          int32_t _M0L6_2atmpS3617;
          int32_t _M0L6_2atmpS3616;
          int32_t _M0L6_2atmpS3614;
          int32_t _M0L8_2afieldS3799;
          int32_t _M0L3valS3624;
          int32_t _M0L6_2atmpS3623;
          int32_t _M0L3valS3626;
          int32_t _M0L6_2atmpS3625;
          if (_M0L6_2atmpS3612 >= _M0L3lenS1280) {
            moonbit_decref(_M0L1cS1282);
            moonbit_decref(_M0L1iS1281);
            moonbit_decref(_M0L5bytesS1278);
            break;
          }
          _M0L3valS3622 = _M0L1cS1282->$0;
          _M0L6_2atmpS3621 = _M0L3valS3622 & 31;
          _M0L6_2atmpS3615 = _M0L6_2atmpS3621 << 6;
          _M0L3valS3620 = _M0L1iS1281->$0;
          _M0L6_2atmpS3619 = _M0L3valS3620 + 1;
          if (
            _M0L6_2atmpS3619 < 0
            || _M0L6_2atmpS3619 >= Moonbit_array_length(_M0L5bytesS1278)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS3618 = _M0L5bytesS1278[_M0L6_2atmpS3619];
          _M0L6_2atmpS3617 = (int32_t)_M0L6_2atmpS3618;
          _M0L6_2atmpS3616 = _M0L6_2atmpS3617 & 63;
          _M0L6_2atmpS3614 = _M0L6_2atmpS3615 | _M0L6_2atmpS3616;
          _M0L1cS1282->$0 = _M0L6_2atmpS3614;
          _M0L8_2afieldS3799 = _M0L1cS1282->$0;
          moonbit_decref(_M0L1cS1282);
          _M0L3valS3624 = _M0L8_2afieldS3799;
          _M0L6_2atmpS3623 = _M0L3valS3624;
          moonbit_incref(_M0L3resS1279);
          #line 222 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1279, _M0L6_2atmpS3623);
          _M0L3valS3626 = _M0L1iS1281->$0;
          _M0L6_2atmpS3625 = _M0L3valS3626 + 2;
          _M0L1iS1281->$0 = _M0L6_2atmpS3625;
        } else {
          int32_t _M0L3valS3627 = _M0L1cS1282->$0;
          if (_M0L3valS3627 < 240) {
            int32_t _M0L3valS3629 = _M0L1iS1281->$0;
            int32_t _M0L6_2atmpS3628 = _M0L3valS3629 + 2;
            int32_t _M0L3valS3645;
            int32_t _M0L6_2atmpS3644;
            int32_t _M0L6_2atmpS3637;
            int32_t _M0L3valS3643;
            int32_t _M0L6_2atmpS3642;
            int32_t _M0L6_2atmpS3641;
            int32_t _M0L6_2atmpS3640;
            int32_t _M0L6_2atmpS3639;
            int32_t _M0L6_2atmpS3638;
            int32_t _M0L6_2atmpS3631;
            int32_t _M0L3valS3636;
            int32_t _M0L6_2atmpS3635;
            int32_t _M0L6_2atmpS3634;
            int32_t _M0L6_2atmpS3633;
            int32_t _M0L6_2atmpS3632;
            int32_t _M0L6_2atmpS3630;
            int32_t _M0L8_2afieldS3800;
            int32_t _M0L3valS3647;
            int32_t _M0L6_2atmpS3646;
            int32_t _M0L3valS3649;
            int32_t _M0L6_2atmpS3648;
            if (_M0L6_2atmpS3628 >= _M0L3lenS1280) {
              moonbit_decref(_M0L1cS1282);
              moonbit_decref(_M0L1iS1281);
              moonbit_decref(_M0L5bytesS1278);
              break;
            }
            _M0L3valS3645 = _M0L1cS1282->$0;
            _M0L6_2atmpS3644 = _M0L3valS3645 & 15;
            _M0L6_2atmpS3637 = _M0L6_2atmpS3644 << 12;
            _M0L3valS3643 = _M0L1iS1281->$0;
            _M0L6_2atmpS3642 = _M0L3valS3643 + 1;
            if (
              _M0L6_2atmpS3642 < 0
              || _M0L6_2atmpS3642 >= Moonbit_array_length(_M0L5bytesS1278)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3641 = _M0L5bytesS1278[_M0L6_2atmpS3642];
            _M0L6_2atmpS3640 = (int32_t)_M0L6_2atmpS3641;
            _M0L6_2atmpS3639 = _M0L6_2atmpS3640 & 63;
            _M0L6_2atmpS3638 = _M0L6_2atmpS3639 << 6;
            _M0L6_2atmpS3631 = _M0L6_2atmpS3637 | _M0L6_2atmpS3638;
            _M0L3valS3636 = _M0L1iS1281->$0;
            _M0L6_2atmpS3635 = _M0L3valS3636 + 2;
            if (
              _M0L6_2atmpS3635 < 0
              || _M0L6_2atmpS3635 >= Moonbit_array_length(_M0L5bytesS1278)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3634 = _M0L5bytesS1278[_M0L6_2atmpS3635];
            _M0L6_2atmpS3633 = (int32_t)_M0L6_2atmpS3634;
            _M0L6_2atmpS3632 = _M0L6_2atmpS3633 & 63;
            _M0L6_2atmpS3630 = _M0L6_2atmpS3631 | _M0L6_2atmpS3632;
            _M0L1cS1282->$0 = _M0L6_2atmpS3630;
            _M0L8_2afieldS3800 = _M0L1cS1282->$0;
            moonbit_decref(_M0L1cS1282);
            _M0L3valS3647 = _M0L8_2afieldS3800;
            _M0L6_2atmpS3646 = _M0L3valS3647;
            moonbit_incref(_M0L3resS1279);
            #line 231 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1279, _M0L6_2atmpS3646);
            _M0L3valS3649 = _M0L1iS1281->$0;
            _M0L6_2atmpS3648 = _M0L3valS3649 + 3;
            _M0L1iS1281->$0 = _M0L6_2atmpS3648;
          } else {
            int32_t _M0L3valS3651 = _M0L1iS1281->$0;
            int32_t _M0L6_2atmpS3650 = _M0L3valS3651 + 3;
            int32_t _M0L3valS3674;
            int32_t _M0L6_2atmpS3673;
            int32_t _M0L6_2atmpS3666;
            int32_t _M0L3valS3672;
            int32_t _M0L6_2atmpS3671;
            int32_t _M0L6_2atmpS3670;
            int32_t _M0L6_2atmpS3669;
            int32_t _M0L6_2atmpS3668;
            int32_t _M0L6_2atmpS3667;
            int32_t _M0L6_2atmpS3659;
            int32_t _M0L3valS3665;
            int32_t _M0L6_2atmpS3664;
            int32_t _M0L6_2atmpS3663;
            int32_t _M0L6_2atmpS3662;
            int32_t _M0L6_2atmpS3661;
            int32_t _M0L6_2atmpS3660;
            int32_t _M0L6_2atmpS3653;
            int32_t _M0L3valS3658;
            int32_t _M0L6_2atmpS3657;
            int32_t _M0L6_2atmpS3656;
            int32_t _M0L6_2atmpS3655;
            int32_t _M0L6_2atmpS3654;
            int32_t _M0L6_2atmpS3652;
            int32_t _M0L3valS3676;
            int32_t _M0L6_2atmpS3675;
            int32_t _M0L3valS3680;
            int32_t _M0L6_2atmpS3679;
            int32_t _M0L6_2atmpS3678;
            int32_t _M0L6_2atmpS3677;
            int32_t _M0L8_2afieldS3801;
            int32_t _M0L3valS3684;
            int32_t _M0L6_2atmpS3683;
            int32_t _M0L6_2atmpS3682;
            int32_t _M0L6_2atmpS3681;
            int32_t _M0L3valS3686;
            int32_t _M0L6_2atmpS3685;
            if (_M0L6_2atmpS3650 >= _M0L3lenS1280) {
              moonbit_decref(_M0L1cS1282);
              moonbit_decref(_M0L1iS1281);
              moonbit_decref(_M0L5bytesS1278);
              break;
            }
            _M0L3valS3674 = _M0L1cS1282->$0;
            _M0L6_2atmpS3673 = _M0L3valS3674 & 7;
            _M0L6_2atmpS3666 = _M0L6_2atmpS3673 << 18;
            _M0L3valS3672 = _M0L1iS1281->$0;
            _M0L6_2atmpS3671 = _M0L3valS3672 + 1;
            if (
              _M0L6_2atmpS3671 < 0
              || _M0L6_2atmpS3671 >= Moonbit_array_length(_M0L5bytesS1278)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3670 = _M0L5bytesS1278[_M0L6_2atmpS3671];
            _M0L6_2atmpS3669 = (int32_t)_M0L6_2atmpS3670;
            _M0L6_2atmpS3668 = _M0L6_2atmpS3669 & 63;
            _M0L6_2atmpS3667 = _M0L6_2atmpS3668 << 12;
            _M0L6_2atmpS3659 = _M0L6_2atmpS3666 | _M0L6_2atmpS3667;
            _M0L3valS3665 = _M0L1iS1281->$0;
            _M0L6_2atmpS3664 = _M0L3valS3665 + 2;
            if (
              _M0L6_2atmpS3664 < 0
              || _M0L6_2atmpS3664 >= Moonbit_array_length(_M0L5bytesS1278)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3663 = _M0L5bytesS1278[_M0L6_2atmpS3664];
            _M0L6_2atmpS3662 = (int32_t)_M0L6_2atmpS3663;
            _M0L6_2atmpS3661 = _M0L6_2atmpS3662 & 63;
            _M0L6_2atmpS3660 = _M0L6_2atmpS3661 << 6;
            _M0L6_2atmpS3653 = _M0L6_2atmpS3659 | _M0L6_2atmpS3660;
            _M0L3valS3658 = _M0L1iS1281->$0;
            _M0L6_2atmpS3657 = _M0L3valS3658 + 3;
            if (
              _M0L6_2atmpS3657 < 0
              || _M0L6_2atmpS3657 >= Moonbit_array_length(_M0L5bytesS1278)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS3656 = _M0L5bytesS1278[_M0L6_2atmpS3657];
            _M0L6_2atmpS3655 = (int32_t)_M0L6_2atmpS3656;
            _M0L6_2atmpS3654 = _M0L6_2atmpS3655 & 63;
            _M0L6_2atmpS3652 = _M0L6_2atmpS3653 | _M0L6_2atmpS3654;
            _M0L1cS1282->$0 = _M0L6_2atmpS3652;
            _M0L3valS3676 = _M0L1cS1282->$0;
            _M0L6_2atmpS3675 = _M0L3valS3676 - 65536;
            _M0L1cS1282->$0 = _M0L6_2atmpS3675;
            _M0L3valS3680 = _M0L1cS1282->$0;
            _M0L6_2atmpS3679 = _M0L3valS3680 >> 10;
            _M0L6_2atmpS3678 = _M0L6_2atmpS3679 + 55296;
            _M0L6_2atmpS3677 = _M0L6_2atmpS3678;
            moonbit_incref(_M0L3resS1279);
            #line 242 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1279, _M0L6_2atmpS3677);
            _M0L8_2afieldS3801 = _M0L1cS1282->$0;
            moonbit_decref(_M0L1cS1282);
            _M0L3valS3684 = _M0L8_2afieldS3801;
            _M0L6_2atmpS3683 = _M0L3valS3684 & 1023;
            _M0L6_2atmpS3682 = _M0L6_2atmpS3683 + 56320;
            _M0L6_2atmpS3681 = _M0L6_2atmpS3682;
            moonbit_incref(_M0L3resS1279);
            #line 243 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1279, _M0L6_2atmpS3681);
            _M0L3valS3686 = _M0L1iS1281->$0;
            _M0L6_2atmpS3685 = _M0L3valS3686 + 4;
            _M0L1iS1281->$0 = _M0L6_2atmpS3685;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1281);
      moonbit_decref(_M0L5bytesS1278);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1279);
}

int32_t _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1271(
  int32_t _M0L6_2aenvS3597,
  moonbit_string_t _M0L1sS1272
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1273;
  int32_t _M0L3lenS1274;
  int32_t _M0L1iS1275;
  int32_t _M0L8_2afieldS3802;
  #line 197 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L3resS1273
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1273)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1273->$0 = 0;
  _M0L3lenS1274 = Moonbit_array_length(_M0L1sS1272);
  _M0L1iS1275 = 0;
  while (1) {
    if (_M0L1iS1275 < _M0L3lenS1274) {
      int32_t _M0L3valS3602 = _M0L3resS1273->$0;
      int32_t _M0L6_2atmpS3599 = _M0L3valS3602 * 10;
      int32_t _M0L6_2atmpS3601;
      int32_t _M0L6_2atmpS3600;
      int32_t _M0L6_2atmpS3598;
      int32_t _M0L6_2atmpS3603;
      if (
        _M0L1iS1275 < 0 || _M0L1iS1275 >= Moonbit_array_length(_M0L1sS1272)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3601 = _M0L1sS1272[_M0L1iS1275];
      _M0L6_2atmpS3600 = _M0L6_2atmpS3601 - 48;
      _M0L6_2atmpS3598 = _M0L6_2atmpS3599 + _M0L6_2atmpS3600;
      _M0L3resS1273->$0 = _M0L6_2atmpS3598;
      _M0L6_2atmpS3603 = _M0L1iS1275 + 1;
      _M0L1iS1275 = _M0L6_2atmpS3603;
      continue;
    } else {
      moonbit_decref(_M0L1sS1272);
    }
    break;
  }
  _M0L8_2afieldS3802 = _M0L3resS1273->$0;
  moonbit_decref(_M0L3resS1273);
  return _M0L8_2afieldS3802;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1251,
  moonbit_string_t _M0L12_2adiscard__S1252,
  int32_t _M0L12_2adiscard__S1253,
  struct _M0TWssbEu* _M0L12_2adiscard__S1254,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1255
) {
  struct moonbit_result_0 _result_4509;
  #line 34 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1255);
  moonbit_decref(_M0L12_2adiscard__S1254);
  moonbit_decref(_M0L12_2adiscard__S1252);
  moonbit_decref(_M0L12_2adiscard__S1251);
  _result_4509.tag = 1;
  _result_4509.data.ok = 0;
  return _result_4509;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1256,
  moonbit_string_t _M0L12_2adiscard__S1257,
  int32_t _M0L12_2adiscard__S1258,
  struct _M0TWssbEu* _M0L12_2adiscard__S1259,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1260
) {
  struct moonbit_result_0 _result_4510;
  #line 34 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1260);
  moonbit_decref(_M0L12_2adiscard__S1259);
  moonbit_decref(_M0L12_2adiscard__S1257);
  moonbit_decref(_M0L12_2adiscard__S1256);
  _result_4510.tag = 1;
  _result_4510.data.ok = 0;
  return _result_4510;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1261,
  moonbit_string_t _M0L12_2adiscard__S1262,
  int32_t _M0L12_2adiscard__S1263,
  struct _M0TWssbEu* _M0L12_2adiscard__S1264,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1265
) {
  struct moonbit_result_0 _result_4511;
  #line 34 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1265);
  moonbit_decref(_M0L12_2adiscard__S1264);
  moonbit_decref(_M0L12_2adiscard__S1262);
  moonbit_decref(_M0L12_2adiscard__S1261);
  _result_4511.tag = 1;
  _result_4511.data.ok = 0;
  return _result_4511;
}

struct moonbit_result_0 _M0IP016_24default__implP48clawteam8clawteam5oauth5codex21MoonBit__Test__Driver9run__testGRP48clawteam8clawteam5oauth5codex50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1266,
  moonbit_string_t _M0L12_2adiscard__S1267,
  int32_t _M0L12_2adiscard__S1268,
  struct _M0TWssbEu* _M0L12_2adiscard__S1269,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1270
) {
  struct moonbit_result_0 _result_4512;
  #line 34 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1270);
  moonbit_decref(_M0L12_2adiscard__S1269);
  moonbit_decref(_M0L12_2adiscard__S1267);
  moonbit_decref(_M0L12_2adiscard__S1266);
  _result_4512.tag = 1;
  _result_4512.data.ok = 0;
  return _result_4512;
}

int32_t _M0IP016_24default__implP48clawteam8clawteam5oauth5codex28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5oauth5codex34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1250
) {
  #line 12 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1250);
  return 0;
}

struct moonbit_result_0 _M0FP48clawteam8clawteam5oauth5codex29____test__706b63652e6d6274__0(
  
) {
  struct _M0TP48clawteam8clawteam5oauth5codex9PkceCodes* _M0L4pkceS1249;
  moonbit_string_t _M0L8_2afieldS3808;
  moonbit_string_t _M0L14code__verifierS3584;
  int32_t _M0L6_2atmpS3807;
  int32_t _M0L6_2atmpS3583;
  int32_t _M0L6_2atmpS3581;
  moonbit_string_t _M0L6_2atmpS3582;
  struct moonbit_result_0 _tmp_4513;
  moonbit_string_t _M0L8_2afieldS3806;
  moonbit_string_t _M0L15code__challengeS3590;
  int32_t _M0L6_2atmpS3805;
  int32_t _M0L6_2atmpS3589;
  int32_t _M0L6_2atmpS3587;
  moonbit_string_t _M0L6_2atmpS3588;
  struct moonbit_result_0 _tmp_4515;
  moonbit_string_t _M0L8_2afieldS3804;
  moonbit_string_t _M0L14code__verifierS3595;
  moonbit_string_t _M0L8_2afieldS3803;
  int32_t _M0L6_2acntS4408;
  moonbit_string_t _M0L15code__challengeS3596;
  int32_t _M0L6_2atmpS3593;
  moonbit_string_t _M0L6_2atmpS3594;
  #line 58 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  #line 59 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L4pkceS1249 = _M0FP48clawteam8clawteam5oauth5codex14generate__pkce();
  _M0L8_2afieldS3808 = _M0L4pkceS1249->$0;
  _M0L14code__verifierS3584 = _M0L8_2afieldS3808;
  _M0L6_2atmpS3807 = Moonbit_array_length(_M0L14code__verifierS3584);
  _M0L6_2atmpS3583 = _M0L6_2atmpS3807;
  _M0L6_2atmpS3581 = _M0L6_2atmpS3583 > 80;
  _M0L6_2atmpS3582 = 0;
  #line 60 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _tmp_4513
  = _M0FPB12assert__true(_M0L6_2atmpS3581, _M0L6_2atmpS3582, (moonbit_string_t)moonbit_string_literal_9.data);
  if (_tmp_4513.tag) {
    int32_t const _M0L5_2aokS3585 = _tmp_4513.data.ok;
  } else {
    void* const _M0L6_2aerrS3586 = _tmp_4513.data.err;
    struct moonbit_result_0 _result_4514;
    moonbit_decref(_M0L4pkceS1249);
    _result_4514.tag = 0;
    _result_4514.data.err = _M0L6_2aerrS3586;
    return _result_4514;
  }
  _M0L8_2afieldS3806 = _M0L4pkceS1249->$1;
  _M0L15code__challengeS3590 = _M0L8_2afieldS3806;
  _M0L6_2atmpS3805 = Moonbit_array_length(_M0L15code__challengeS3590);
  _M0L6_2atmpS3589 = _M0L6_2atmpS3805;
  _M0L6_2atmpS3587 = _M0L6_2atmpS3589 > 40;
  _M0L6_2atmpS3588 = 0;
  #line 61 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _tmp_4515
  = _M0FPB12assert__true(_M0L6_2atmpS3587, _M0L6_2atmpS3588, (moonbit_string_t)moonbit_string_literal_10.data);
  if (_tmp_4515.tag) {
    int32_t const _M0L5_2aokS3591 = _tmp_4515.data.ok;
  } else {
    void* const _M0L6_2aerrS3592 = _tmp_4515.data.err;
    struct moonbit_result_0 _result_4516;
    moonbit_decref(_M0L4pkceS1249);
    _result_4516.tag = 0;
    _result_4516.data.err = _M0L6_2aerrS3592;
    return _result_4516;
  }
  _M0L8_2afieldS3804 = _M0L4pkceS1249->$0;
  _M0L14code__verifierS3595 = _M0L8_2afieldS3804;
  _M0L8_2afieldS3803 = _M0L4pkceS1249->$1;
  _M0L6_2acntS4408 = Moonbit_object_header(_M0L4pkceS1249)->rc;
  if (_M0L6_2acntS4408 > 1) {
    int32_t _M0L11_2anew__cntS4409 = _M0L6_2acntS4408 - 1;
    Moonbit_object_header(_M0L4pkceS1249)->rc = _M0L11_2anew__cntS4409;
    moonbit_incref(_M0L8_2afieldS3803);
    moonbit_incref(_M0L14code__verifierS3595);
  } else if (_M0L6_2acntS4408 == 1) {
    #line 62 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
    moonbit_free(_M0L4pkceS1249);
  }
  _M0L15code__challengeS3596 = _M0L8_2afieldS3803;
  #line 62 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L6_2atmpS3593
  = _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L14code__verifierS3595, _M0L15code__challengeS3596);
  _M0L6_2atmpS3594 = 0;
  #line 62 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  return _M0FPB12assert__true(_M0L6_2atmpS3593, _M0L6_2atmpS3594, (moonbit_string_t)moonbit_string_literal_11.data);
}

struct _M0TP48clawteam8clawteam5oauth5codex9PkceCodes* _M0FP48clawteam8clawteam5oauth5codex14generate__pkce(
  
) {
  moonbit_bytes_t _M0L5bytesS1244;
  int32_t _M0L6_2atmpS3580;
  int64_t _M0L6_2atmpS3579;
  struct _M0TPC15bytes9BytesView _M0L6_2atmpS3578;
  moonbit_string_t _M0L6_2atmpS3577;
  moonbit_string_t _M0L14code__verifierS1245;
  int32_t _M0L6_2atmpS3576;
  struct _M0TPC16string10StringView _M0L6_2atmpS3575;
  moonbit_bytes_t _M0L6_2atmpS3574;
  moonbit_bytes_t _M0L7_2abindS1248;
  moonbit_bytes_t _M0L6_2atmpS3572;
  int32_t _M0L6_2atmpS3809;
  int32_t _M0L6_2atmpS3573;
  struct _M0TPB9ArrayViewGyE _M0L6_2atmpS3571;
  moonbit_bytes_t _M0L7_2abindS1247;
  int32_t _M0L6_2atmpS3570;
  int64_t _M0L6_2atmpS3569;
  struct _M0TPC15bytes9BytesView _M0L6_2atmpS3568;
  moonbit_string_t _M0L6_2atmpS3567;
  moonbit_string_t _M0L15code__challengeS1246;
  struct _M0TP48clawteam8clawteam5oauth5codex9PkceCodes* _block_4517;
  #line 45 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  #line 46 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L5bytesS1244
  = _M0FP48clawteam8clawteam5oauth5codex23generate__random__bytes(64);
  _M0L6_2atmpS3580 = Moonbit_array_length(_M0L5bytesS1244);
  _M0L6_2atmpS3579 = (int64_t)_M0L6_2atmpS3580;
  #line 47 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L6_2atmpS3578
  = _M0MPC15bytes5Bytes12view_2einner(_M0L5bytesS1244, 0, _M0L6_2atmpS3579);
  #line 47 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L6_2atmpS3577
  = _M0FP411moonbitlang1x5codec6base6414encode_2einner(_M0L6_2atmpS3578, 1);
  #line 47 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L14code__verifierS1245
  = _M0FP48clawteam8clawteam5oauth5codex14strip__padding(_M0L6_2atmpS3577);
  _M0L6_2atmpS3576 = Moonbit_array_length(_M0L14code__verifierS1245);
  moonbit_incref(_M0L14code__verifierS1245);
  _M0L6_2atmpS3575
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS3576, _M0L14code__verifierS1245
  };
  #line 50 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L6_2atmpS3574
  = _M0FPC28encoding4utf814encode_2einner(_M0L6_2atmpS3575, 0);
  #line 50 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L7_2abindS1248
  = _M0FP311moonbitlang1x6crypto6sha256GzE(_M0L6_2atmpS3574);
  moonbit_incref(_M0L7_2abindS1248);
  _M0L6_2atmpS3572 = _M0L7_2abindS1248;
  _M0L6_2atmpS3809 = Moonbit_array_length(_M0L7_2abindS1248);
  moonbit_decref(_M0L7_2abindS1248);
  _M0L6_2atmpS3573 = _M0L6_2atmpS3809;
  _M0L6_2atmpS3571
  = (struct _M0TPB9ArrayViewGyE){
    0, _M0L6_2atmpS3573, _M0L6_2atmpS3572
  };
  #line 50 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L7_2abindS1247 = _M0MPC15bytes5Bytes11from__array(_M0L6_2atmpS3571);
  _M0L6_2atmpS3570 = Moonbit_array_length(_M0L7_2abindS1247);
  _M0L6_2atmpS3569 = (int64_t)_M0L6_2atmpS3570;
  #line 50 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L6_2atmpS3568
  = _M0MPC15bytes5Bytes12view_2einner(_M0L7_2abindS1247, 0, _M0L6_2atmpS3569);
  #line 49 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L6_2atmpS3567
  = _M0FP411moonbitlang1x5codec6base6414encode_2einner(_M0L6_2atmpS3568, 1);
  #line 48 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L15code__challengeS1246
  = _M0FP48clawteam8clawteam5oauth5codex14strip__padding(_M0L6_2atmpS3567);
  _block_4517
  = (struct _M0TP48clawteam8clawteam5oauth5codex9PkceCodes*)moonbit_malloc(sizeof(struct _M0TP48clawteam8clawteam5oauth5codex9PkceCodes));
  Moonbit_object_header(_block_4517)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP48clawteam8clawteam5oauth5codex9PkceCodes, $0) >> 2, 2, 0);
  _block_4517->$0 = _M0L14code__verifierS1245;
  _block_4517->$1 = _M0L15code__challengeS1246;
  return _block_4517;
}

moonbit_string_t _M0FP48clawteam8clawteam5oauth5codex14strip__padding(
  moonbit_string_t _M0L1sS1237
) {
  struct _M0TPB13StringBuilder* _M0L3bufS1235;
  struct _M0TWEOc* _M0L5_2aitS1236;
  #line 34 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  #line 35 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L3bufS1235 = _M0MPB13StringBuilder11new_2einner(0);
  #line 35 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L5_2aitS1236 = _M0MPC16string6String4iter(_M0L1sS1237);
  while (1) {
    int32_t _M0L1cS1239;
    int32_t _M0L7_2abindS1241;
    moonbit_incref(_M0L5_2aitS1236);
    #line 36 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
    _M0L7_2abindS1241 = _M0MPB4Iter4nextGcE(_M0L5_2aitS1236);
    if (_M0L7_2abindS1241 == -1) {
      moonbit_decref(_M0L5_2aitS1236);
    } else {
      int32_t _M0L7_2aSomeS1242 = _M0L7_2abindS1241;
      int32_t _M0L4_2acS1243 = _M0L7_2aSomeS1242;
      _M0L1cS1239 = _M0L4_2acS1243;
      goto join_1238;
    }
    goto joinlet_4519;
    join_1238:;
    if (_M0L1cS1239 != 61) {
      moonbit_incref(_M0L3bufS1235);
      #line 38 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS1235, _M0L1cS1239);
    }
    continue;
    joinlet_4519:;
    break;
  }
  #line 41 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS1235);
}

moonbit_bytes_t _M0FP48clawteam8clawteam5oauth5codex23generate__random__bytes(
  int32_t _M0L6lengthS1231
) {
  void* _M0L11_2atry__errS1229;
  struct _M0TPC16random6Source _M0L8randomerS1227;
  struct moonbit_result_1 _tmp_4521;
  moonbit_bytes_t _M0L5bytesS1230;
  int32_t _M0L7_2abindS1232;
  int32_t _M0L1iS1233;
  moonbit_bytes_t _M0L6_2atmpS3563;
  int32_t _M0L6_2atmpS3810;
  int32_t _M0L6_2atmpS3564;
  struct _M0TPB9ArrayViewGyE _M0L6_2atmpS3562;
  #line 22 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  #line 23 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _tmp_4521 = _M0FP48clawteam8clawteam8internal4rand7chacha8();
  if (_tmp_4521.tag) {
    struct _M0TPC16random6Source const _M0L5_2aokS3565 = _tmp_4521.data.ok;
    _M0L8randomerS1227 = _M0L5_2aokS3565;
  } else {
    void* const _M0L6_2aerrS3566 = _tmp_4521.data.err;
    _M0L11_2atry__errS1229 = _M0L6_2aerrS3566;
    goto join_1228;
  }
  goto joinlet_4520;
  join_1228:;
  moonbit_decref(_M0L11_2atry__errS1229);
  #line 23 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L8randomerS1227
  = _M0FPB5abortGRPC16random4RandE((moonbit_string_t)moonbit_string_literal_12.data, (moonbit_string_t)moonbit_string_literal_13.data);
  joinlet_4520:;
  _M0L5bytesS1230 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6lengthS1231, 0);
  _M0L7_2abindS1232 = 0;
  _M0L1iS1233 = _M0L7_2abindS1232;
  while (1) {
    if (_M0L1iS1233 < _M0L6lengthS1231) {
      int32_t _M0L6_2atmpS3560;
      int32_t _M0L6_2atmpS3561;
      if (_M0L8randomerS1227.$1) {
        moonbit_incref(_M0L8randomerS1227.$1);
      }
      #line 28 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
      _M0L6_2atmpS3560
      = _M0FP48clawteam8clawteam5oauth5codex9gen__byte(_M0L8randomerS1227);
      if (
        _M0L1iS1233 < 0
        || _M0L1iS1233 >= Moonbit_array_length(_M0L5bytesS1230)
      ) {
        #line 28 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
        moonbit_panic();
      }
      _M0L5bytesS1230[_M0L1iS1233] = _M0L6_2atmpS3560;
      _M0L6_2atmpS3561 = _M0L1iS1233 + 1;
      _M0L1iS1233 = _M0L6_2atmpS3561;
      continue;
    } else if (_M0L8randomerS1227.$1) {
      moonbit_decref(_M0L8randomerS1227.$1);
    }
    break;
  }
  moonbit_incref(_M0L5bytesS1230);
  _M0L6_2atmpS3563 = _M0L5bytesS1230;
  _M0L6_2atmpS3810 = Moonbit_array_length(_M0L5bytesS1230);
  moonbit_decref(_M0L5bytesS1230);
  _M0L6_2atmpS3564 = _M0L6_2atmpS3810;
  _M0L6_2atmpS3562
  = (struct _M0TPB9ArrayViewGyE){
    0, _M0L6_2atmpS3564, _M0L6_2atmpS3563
  };
  #line 30 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  return _M0MPC15bytes5Bytes11from__array(_M0L6_2atmpS3562);
}

int32_t _M0FP48clawteam8clawteam5oauth5codex9gen__byte(
  struct _M0TPC16random6Source _M0L8randomerS1226
) {
  int32_t _M0L6_2atmpS3559;
  #line 16 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  #line 17 "E:\\moonbit\\clawteam\\oauth\\codex\\pkce.mbt"
  _M0L6_2atmpS3559 = _M0MPC16random4Rand11int_2einner(_M0L8randomerS1226, 0);
  return _M0L6_2atmpS3559 & 0xff;
}

int32_t _M0IPC15bytes5BytesP311moonbitlang1x6crypto10ByteSource8blit__to(
  moonbit_bytes_t _M0L4selfS1223,
  moonbit_bytes_t _M0L3dstS1221,
  int32_t _M0L3lenS1225,
  int32_t _M0L11src__offsetS1224,
  int32_t _M0L11dst__offsetS1222
) {
  #line 81 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\byte_source.mbt"
  #line 88 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\byte_source.mbt"
  _M0MPC15array10FixedArray17blit__from__bytes(_M0L3dstS1221, _M0L11dst__offsetS1222, _M0L4selfS1223, _M0L11src__offsetS1224, _M0L3lenS1225);
  return 0;
}

int32_t _M0IPC15bytes5BytesP311moonbitlang1x6crypto10ByteSource6length(
  moonbit_bytes_t _M0L4selfS1220
) {
  int32_t _M0L6_2atmpS3811;
  #line 76 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\byte_source.mbt"
  _M0L6_2atmpS3811 = Moonbit_array_length(_M0L4selfS1220);
  moonbit_decref(_M0L4selfS1220);
  return _M0L6_2atmpS3811;
}

moonbit_bytes_t _M0FP311moonbitlang1x6crypto6sha256GzE(
  moonbit_bytes_t _M0L4dataS1219
) {
  uint32_t* _M0L6_2atmpS3558;
  struct _M0TP311moonbitlang1x6crypto6SHA256* _M0L7_2aselfS1218;
  struct _M0TP311moonbitlang1x6crypto6SHA256* _M0L6_2atmpS3557;
  #line 226 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3558 = 0;
  #line 227 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L7_2aselfS1218
  = _M0MP311moonbitlang1x6crypto6SHA2563new(_M0L6_2atmpS3558);
  moonbit_incref(_M0L7_2aselfS1218);
  #line 227 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0MP311moonbitlang1x6crypto6SHA2566updateGzE(_M0L7_2aselfS1218, _M0L4dataS1219);
  _M0L6_2atmpS3557 = _M0L7_2aselfS1218;
  #line 227 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  return _M0MP311moonbitlang1x6crypto6SHA2568finalize(_M0L6_2atmpS3557);
}

moonbit_bytes_t _M0MP311moonbitlang1x6crypto6SHA2568finalize(
  struct _M0TP311moonbitlang1x6crypto6SHA256* _M0L4selfS1217
) {
  int32_t _M0L6_2atmpS3556;
  moonbit_bytes_t _M0L3retS1216;
  #line 147 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  #line 148 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3556 = _M0IPC14byte4BytePB7Default7default();
  _M0L3retS1216 = (moonbit_bytes_t)moonbit_make_bytes(32, _M0L6_2atmpS3556);
  moonbit_incref(_M0L3retS1216);
  #line 149 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0MP311moonbitlang1x6crypto6SHA25624__finalize__into_2einner(_M0L4selfS1217, _M0L3retS1216, 8, 0);
  return _M0L3retS1216;
}

int32_t _M0MP311moonbitlang1x6crypto6SHA25624__finalize__into_2einner(
  struct _M0TP311moonbitlang1x6crypto6SHA256* _M0L4selfS1210,
  moonbit_bytes_t _M0L6bufferS1214,
  int32_t _M0L4sizeS1213,
  int32_t _M0L6offsetS1215
) {
  int32_t _M0L6_2atmpS3555;
  moonbit_bytes_t _M0L4dataS1208;
  int32_t _M0L10buf__indexS3554;
  struct _M0TPC13ref3RefGiE* _M0L3cntS1209;
  uint64_t _M0L3lenS3550;
  int32_t _M0L3valS3553;
  uint64_t _M0L6_2atmpS3552;
  uint64_t _M0L6_2atmpS3551;
  uint64_t _M0L3lenS1211;
  moonbit_bytes_t _M0L8_2afieldS3814;
  moonbit_bytes_t _M0L3bufS3525;
  int32_t _M0L3valS3526;
  uint32_t* _M0L8_2afieldS3813;
  int32_t _M0L6_2acntS4410;
  uint32_t* _M0L3regS3549;
  uint32_t* _M0L3regS1212;
  int32_t _M0L3valS3527;
  int32_t _M0L3valS3529;
  int32_t _M0L6_2atmpS3528;
  int32_t _M0L8_2afieldS3812;
  int32_t _M0L3valS3530;
  uint64_t _M0L6_2atmpS3532;
  int32_t _M0L6_2atmpS3531;
  uint64_t _M0L6_2atmpS3534;
  int32_t _M0L6_2atmpS3533;
  uint64_t _M0L6_2atmpS3536;
  int32_t _M0L6_2atmpS3535;
  uint64_t _M0L6_2atmpS3538;
  int32_t _M0L6_2atmpS3537;
  uint64_t _M0L6_2atmpS3540;
  int32_t _M0L6_2atmpS3539;
  uint64_t _M0L6_2atmpS3542;
  int32_t _M0L6_2atmpS3541;
  uint64_t _M0L6_2atmpS3544;
  int32_t _M0L6_2atmpS3543;
  uint64_t _M0L6_2atmpS3546;
  int32_t _M0L6_2atmpS3545;
  struct _M0TWEOj* _M0L6_2atmpS3548;
  struct _M0TWEOj* _M0L6_2atmpS3547;
  #line 155 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  #line 162 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3555 = _M0IPC14byte4BytePB7Default7default();
  _M0L4dataS1208 = (moonbit_bytes_t)moonbit_make_bytes(64, _M0L6_2atmpS3555);
  _M0L10buf__indexS3554 = _M0L4selfS1210->$3;
  _M0L3cntS1209
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3cntS1209)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3cntS1209->$0 = _M0L10buf__indexS3554;
  _M0L3lenS3550 = _M0L4selfS1210->$1;
  _M0L3valS3553 = _M0L3cntS1209->$0;
  #line 164 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3552 = _M0MPC13int3Int10to__uint64(_M0L3valS3553);
  _M0L6_2atmpS3551 = 8ull * _M0L6_2atmpS3552;
  _M0L3lenS1211 = _M0L3lenS3550 + _M0L6_2atmpS3551;
  _M0L8_2afieldS3814 = _M0L4selfS1210->$2;
  _M0L3bufS3525 = _M0L8_2afieldS3814;
  _M0L3valS3526 = _M0L3cntS1209->$0;
  moonbit_incref(_M0L3bufS3525);
  moonbit_incref(_M0L4dataS1208);
  #line 165 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0MPC15array10FixedArray16blit__to_2einnerGyE(_M0L3bufS3525, _M0L4dataS1208, _M0L3valS3526, 0, 0);
  _M0L8_2afieldS3813 = _M0L4selfS1210->$0;
  _M0L6_2acntS4410 = Moonbit_object_header(_M0L4selfS1210)->rc;
  if (_M0L6_2acntS4410 > 1) {
    int32_t _M0L11_2anew__cntS4412 = _M0L6_2acntS4410 - 1;
    Moonbit_object_header(_M0L4selfS1210)->rc = _M0L11_2anew__cntS4412;
    moonbit_incref(_M0L8_2afieldS3813);
  } else if (_M0L6_2acntS4410 == 1) {
    moonbit_bytes_t _M0L8_2afieldS4411 = _M0L4selfS1210->$2;
    moonbit_decref(_M0L8_2afieldS4411);
    #line 166 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
    moonbit_free(_M0L4selfS1210);
  }
  _M0L3regS3549 = _M0L8_2afieldS3813;
  #line 166 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L3regS1212 = _M0MPC15array10FixedArray4copyGjE(_M0L3regS3549);
  _M0L3valS3527 = _M0L3cntS1209->$0;
  if (
    _M0L3valS3527 < 0
    || _M0L3valS3527 >= Moonbit_array_length(_M0L4dataS1208)
  ) {
    #line 169 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
    moonbit_panic();
  }
  _M0L4dataS1208[_M0L3valS3527] = 128;
  _M0L3valS3529 = _M0L3cntS1209->$0;
  _M0L6_2atmpS3528 = _M0L3valS3529 + 1;
  _M0L3cntS1209->$0 = _M0L6_2atmpS3528;
  _M0L8_2afieldS3812 = _M0L3cntS1209->$0;
  moonbit_decref(_M0L3cntS1209);
  _M0L3valS3530 = _M0L8_2afieldS3812;
  if (_M0L3valS3530 > 56) {
    moonbit_incref(_M0L3regS1212);
    moonbit_incref(_M0L4dataS1208);
    #line 172 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
    _M0MP311moonbitlang1x6crypto6SHA2569transform(_M0L4dataS1208, _M0L3regS1212);
    moonbit_incref(_M0L4dataS1208);
    #line 173 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
    _M0MPC15array10FixedArray12fill_2einnerGyE(_M0L4dataS1208, 0, 0, 4294967296ll);
  }
  _M0L6_2atmpS3532 = _M0L3lenS1211 >> 56;
  #line 175 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3531 = _M0MPC16uint646UInt648to__byte(_M0L6_2atmpS3532);
  _M0L4dataS1208[56] = _M0L6_2atmpS3531;
  _M0L6_2atmpS3534 = _M0L3lenS1211 >> 48;
  #line 176 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3533 = _M0MPC16uint646UInt648to__byte(_M0L6_2atmpS3534);
  _M0L4dataS1208[57] = _M0L6_2atmpS3533;
  _M0L6_2atmpS3536 = _M0L3lenS1211 >> 40;
  #line 177 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3535 = _M0MPC16uint646UInt648to__byte(_M0L6_2atmpS3536);
  _M0L4dataS1208[58] = _M0L6_2atmpS3535;
  _M0L6_2atmpS3538 = _M0L3lenS1211 >> 32;
  #line 178 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3537 = _M0MPC16uint646UInt648to__byte(_M0L6_2atmpS3538);
  _M0L4dataS1208[59] = _M0L6_2atmpS3537;
  _M0L6_2atmpS3540 = _M0L3lenS1211 >> 24;
  #line 179 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3539 = _M0MPC16uint646UInt648to__byte(_M0L6_2atmpS3540);
  _M0L4dataS1208[60] = _M0L6_2atmpS3539;
  _M0L6_2atmpS3542 = _M0L3lenS1211 >> 16;
  #line 180 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3541 = _M0MPC16uint646UInt648to__byte(_M0L6_2atmpS3542);
  _M0L4dataS1208[61] = _M0L6_2atmpS3541;
  _M0L6_2atmpS3544 = _M0L3lenS1211 >> 8;
  #line 181 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3543 = _M0MPC16uint646UInt648to__byte(_M0L6_2atmpS3544);
  _M0L4dataS1208[62] = _M0L6_2atmpS3543;
  _M0L6_2atmpS3546 = _M0L3lenS1211 >> 0;
  #line 182 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3545 = _M0MPC16uint646UInt648to__byte(_M0L6_2atmpS3546);
  _M0L4dataS1208[63] = _M0L6_2atmpS3545;
  moonbit_incref(_M0L3regS1212);
  #line 183 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0MP311moonbitlang1x6crypto6SHA2569transform(_M0L4dataS1208, _M0L3regS1212);
  #line 186 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3548 = _M0MPC15array10FixedArray4iterGjE(_M0L3regS1212);
  #line 186 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3547 = _M0MPB4Iter4takeGjE(_M0L6_2atmpS3548, _M0L4sizeS1213);
  #line 186 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0FP311moonbitlang1x6crypto24arr__u32__to__u8be__into(_M0L6_2atmpS3547, _M0L6bufferS1214, _M0L6offsetS1215);
  return 0;
}

int32_t _M0MP311moonbitlang1x6crypto6SHA2566updateGzE(
  struct _M0TP311moonbitlang1x6crypto6SHA256* _M0L4selfS1206,
  moonbit_bytes_t _M0L4dataS1204
) {
  struct _M0TPC13ref3RefGiE* _M0L6offsetS1203;
  #line 122 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6offsetS1203
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L6offsetS1203)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L6offsetS1203->$0 = 0;
  while (1) {
    int32_t _M0L3valS3503 = _M0L6offsetS1203->$0;
    int32_t _M0L6_2atmpS3504;
    moonbit_incref(_M0L4dataS1204);
    #line 124 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
    _M0L6_2atmpS3504
    = _M0IPC15bytes5BytesP311moonbitlang1x6crypto10ByteSource6length(_M0L4dataS1204);
    if (_M0L3valS3503 < _M0L6_2atmpS3504) {
      int32_t _M0L10buf__indexS3521 = _M0L4selfS1206->$3;
      int32_t _M0L6_2atmpS3517 = 64 - _M0L10buf__indexS3521;
      int32_t _M0L6_2atmpS3519;
      int32_t _M0L3valS3520;
      int32_t _M0L6_2atmpS3518;
      int32_t _M0L8min__lenS1205;
      moonbit_bytes_t _M0L8_2afieldS3817;
      moonbit_bytes_t _M0L3bufS3505;
      int32_t _M0L3valS3506;
      int32_t _M0L10buf__indexS3507;
      int32_t _M0L10buf__indexS3509;
      int32_t _M0L6_2atmpS3508;
      int32_t _M0L10buf__indexS3510;
      int32_t _M0L3valS3516;
      int32_t _M0L6_2atmpS3515;
      moonbit_incref(_M0L4dataS1204);
      #line 125 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
      _M0L6_2atmpS3519
      = _M0IPC15bytes5BytesP311moonbitlang1x6crypto10ByteSource6length(_M0L4dataS1204);
      _M0L3valS3520 = _M0L6offsetS1203->$0;
      _M0L6_2atmpS3518 = _M0L6_2atmpS3519 - _M0L3valS3520;
      if (_M0L6_2atmpS3517 >= _M0L6_2atmpS3518) {
        int32_t _M0L6_2atmpS3522;
        int32_t _M0L3valS3523;
        moonbit_incref(_M0L4dataS1204);
        #line 126 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3522
        = _M0IPC15bytes5BytesP311moonbitlang1x6crypto10ByteSource6length(_M0L4dataS1204);
        _M0L3valS3523 = _M0L6offsetS1203->$0;
        _M0L8min__lenS1205 = _M0L6_2atmpS3522 - _M0L3valS3523;
      } else {
        int32_t _M0L10buf__indexS3524 = _M0L4selfS1206->$3;
        _M0L8min__lenS1205 = 64 - _M0L10buf__indexS3524;
      }
      _M0L8_2afieldS3817 = _M0L4selfS1206->$2;
      _M0L3bufS3505 = _M0L8_2afieldS3817;
      _M0L3valS3506 = _M0L6offsetS1203->$0;
      _M0L10buf__indexS3507 = _M0L4selfS1206->$3;
      moonbit_incref(_M0L3bufS3505);
      moonbit_incref(_M0L4dataS1204);
      #line 130 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
      _M0IPC15bytes5BytesP311moonbitlang1x6crypto10ByteSource8blit__to(_M0L4dataS1204, _M0L3bufS3505, _M0L8min__lenS1205, _M0L3valS3506, _M0L10buf__indexS3507);
      _M0L10buf__indexS3509 = _M0L4selfS1206->$3;
      _M0L6_2atmpS3508 = _M0L10buf__indexS3509 + _M0L8min__lenS1205;
      _M0L4selfS1206->$3 = _M0L6_2atmpS3508;
      _M0L10buf__indexS3510 = _M0L4selfS1206->$3;
      if (_M0L10buf__indexS3510 == 64) {
        uint64_t _M0L3lenS3512 = _M0L4selfS1206->$1;
        uint64_t _M0L6_2atmpS3511 = _M0L3lenS3512 + 512ull;
        moonbit_bytes_t _M0L8_2afieldS3816;
        moonbit_bytes_t _M0L3bufS3513;
        uint32_t* _M0L8_2afieldS3815;
        uint32_t* _M0L3regS3514;
        _M0L4selfS1206->$1 = _M0L6_2atmpS3511;
        _M0L4selfS1206->$3 = 0;
        _M0L8_2afieldS3816 = _M0L4selfS1206->$2;
        _M0L3bufS3513 = _M0L8_2afieldS3816;
        _M0L8_2afieldS3815 = _M0L4selfS1206->$0;
        _M0L3regS3514 = _M0L8_2afieldS3815;
        moonbit_incref(_M0L3regS3514);
        moonbit_incref(_M0L3bufS3513);
        #line 140 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0MP311moonbitlang1x6crypto6SHA2569transform(_M0L3bufS3513, _M0L3regS3514);
      }
      _M0L3valS3516 = _M0L6offsetS1203->$0;
      _M0L6_2atmpS3515 = _M0L3valS3516 + _M0L8min__lenS1205;
      _M0L6offsetS1203->$0 = _M0L6_2atmpS3515;
      continue;
    } else {
      moonbit_decref(_M0L4selfS1206);
      moonbit_decref(_M0L4dataS1204);
      moonbit_decref(_M0L6offsetS1203);
    }
    break;
  }
  return 0;
}

int32_t _M0MP311moonbitlang1x6crypto6SHA2569transform(
  moonbit_bytes_t _M0L4dataS1191,
  uint32_t* _M0L3regS1181
) {
  uint32_t* _M0L1wS1180;
  int32_t _M0L6_2atmpS3401;
  #line 51 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L1wS1180 = (uint32_t*)moonbit_make_int32_array(64, 0u);
  _M0L6_2atmpS3401 = Moonbit_array_length(_M0L3regS1181);
  if (_M0L6_2atmpS3401 == 8) {
    uint32_t _M0L6_2atmpS3502 = (uint32_t)_M0L3regS1181[0];
    struct _M0TPC13ref3RefGjE* _M0L1aS1182 =
      (struct _M0TPC13ref3RefGjE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGjE));
    uint32_t _M0L6_2atmpS3501;
    struct _M0TPC13ref3RefGjE* _M0L1bS1183;
    uint32_t _M0L6_2atmpS3500;
    struct _M0TPC13ref3RefGjE* _M0L1cS1184;
    uint32_t _M0L6_2atmpS3499;
    struct _M0TPC13ref3RefGjE* _M0L1dS1185;
    uint32_t _M0L6_2atmpS3498;
    struct _M0TPC13ref3RefGjE* _M0L1eS1186;
    uint32_t _M0L6_2atmpS3497;
    struct _M0TPC13ref3RefGjE* _M0L1fS1187;
    uint32_t _M0L6_2atmpS3496;
    struct _M0TPC13ref3RefGjE* _M0L1gS1188;
    uint32_t _M0L6_2atmpS3495;
    struct _M0TPC13ref3RefGjE* _M0L1hS1189;
    int32_t _M0L5indexS1190;
    int32_t _M0L5indexS1193;
    int32_t _M0L5indexS1197;
    uint32_t _M0L6_2atmpS3472;
    uint32_t _M0L8_2afieldS3825;
    uint32_t _M0L3valS3473;
    uint32_t _M0L6_2atmpS3471;
    uint32_t _M0L6_2atmpS3475;
    uint32_t _M0L8_2afieldS3824;
    uint32_t _M0L3valS3476;
    uint32_t _M0L6_2atmpS3474;
    uint32_t _M0L6_2atmpS3478;
    uint32_t _M0L8_2afieldS3823;
    uint32_t _M0L3valS3479;
    uint32_t _M0L6_2atmpS3477;
    uint32_t _M0L6_2atmpS3481;
    uint32_t _M0L8_2afieldS3822;
    uint32_t _M0L3valS3482;
    uint32_t _M0L6_2atmpS3480;
    uint32_t _M0L6_2atmpS3484;
    uint32_t _M0L8_2afieldS3821;
    uint32_t _M0L3valS3485;
    uint32_t _M0L6_2atmpS3483;
    uint32_t _M0L6_2atmpS3487;
    uint32_t _M0L8_2afieldS3820;
    uint32_t _M0L3valS3488;
    uint32_t _M0L6_2atmpS3486;
    uint32_t _M0L6_2atmpS3490;
    uint32_t _M0L8_2afieldS3819;
    uint32_t _M0L3valS3491;
    uint32_t _M0L6_2atmpS3489;
    uint32_t _M0L6_2atmpS3493;
    uint32_t _M0L8_2afieldS3818;
    uint32_t _M0L3valS3494;
    uint32_t _M0L6_2atmpS3492;
    Moonbit_object_header(_M0L1aS1182)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGjE) >> 2, 0, 0);
    _M0L1aS1182->$0 = _M0L6_2atmpS3502;
    _M0L6_2atmpS3501 = (uint32_t)_M0L3regS1181[1];
    _M0L1bS1183
    = (struct _M0TPC13ref3RefGjE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGjE));
    Moonbit_object_header(_M0L1bS1183)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGjE) >> 2, 0, 0);
    _M0L1bS1183->$0 = _M0L6_2atmpS3501;
    _M0L6_2atmpS3500 = (uint32_t)_M0L3regS1181[2];
    _M0L1cS1184
    = (struct _M0TPC13ref3RefGjE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGjE));
    Moonbit_object_header(_M0L1cS1184)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGjE) >> 2, 0, 0);
    _M0L1cS1184->$0 = _M0L6_2atmpS3500;
    _M0L6_2atmpS3499 = (uint32_t)_M0L3regS1181[3];
    _M0L1dS1185
    = (struct _M0TPC13ref3RefGjE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGjE));
    Moonbit_object_header(_M0L1dS1185)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGjE) >> 2, 0, 0);
    _M0L1dS1185->$0 = _M0L6_2atmpS3499;
    _M0L6_2atmpS3498 = (uint32_t)_M0L3regS1181[4];
    _M0L1eS1186
    = (struct _M0TPC13ref3RefGjE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGjE));
    Moonbit_object_header(_M0L1eS1186)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGjE) >> 2, 0, 0);
    _M0L1eS1186->$0 = _M0L6_2atmpS3498;
    _M0L6_2atmpS3497 = (uint32_t)_M0L3regS1181[5];
    _M0L1fS1187
    = (struct _M0TPC13ref3RefGjE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGjE));
    Moonbit_object_header(_M0L1fS1187)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGjE) >> 2, 0, 0);
    _M0L1fS1187->$0 = _M0L6_2atmpS3497;
    _M0L6_2atmpS3496 = (uint32_t)_M0L3regS1181[6];
    _M0L1gS1188
    = (struct _M0TPC13ref3RefGjE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGjE));
    Moonbit_object_header(_M0L1gS1188)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGjE) >> 2, 0, 0);
    _M0L1gS1188->$0 = _M0L6_2atmpS3496;
    _M0L6_2atmpS3495 = (uint32_t)_M0L3regS1181[7];
    _M0L1hS1189
    = (struct _M0TPC13ref3RefGjE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGjE));
    Moonbit_object_header(_M0L1hS1189)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGjE) >> 2, 0, 0);
    _M0L1hS1189->$0 = _M0L6_2atmpS3495;
    _M0L5indexS1190 = 0;
    while (1) {
      if (_M0L5indexS1190 < 16) {
        int32_t _M0L6_2atmpS3403 = 4 * _M0L5indexS1190;
        uint32_t _M0L6_2atmpS3402;
        int32_t _M0L6_2atmpS3404;
        moonbit_incref(_M0L4dataS1191);
        #line 63 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3402
        = _M0FP311moonbitlang1x6crypto28bytes__u8__to__u32be_2einner(_M0L4dataS1191, _M0L6_2atmpS3403);
        if (
          _M0L5indexS1190 < 0
          || _M0L5indexS1190 >= Moonbit_array_length(_M0L1wS1180)
        ) {
          #line 63 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L1wS1180[_M0L5indexS1190] = _M0L6_2atmpS3402;
        _M0L6_2atmpS3404 = _M0L5indexS1190 + 1;
        _M0L5indexS1190 = _M0L6_2atmpS3404;
        continue;
      } else {
        moonbit_decref(_M0L4dataS1191);
      }
      break;
    }
    _M0L5indexS1193 = 16;
    while (1) {
      if (_M0L5indexS1193 < 64) {
        int32_t _M0L6_2atmpS3431 = _M0L5indexS1193 - 15;
        uint32_t _M0L6_2atmpS3430;
        uint32_t _M0L6_2atmpS3426;
        int32_t _M0L6_2atmpS3429;
        uint32_t _M0L6_2atmpS3428;
        uint32_t _M0L6_2atmpS3427;
        uint32_t _M0L6_2atmpS3422;
        int32_t _M0L6_2atmpS3425;
        uint32_t _M0L6_2atmpS3424;
        uint32_t _M0L6_2atmpS3423;
        uint32_t _M0L8sigma__0S1194;
        int32_t _M0L6_2atmpS3421;
        uint32_t _M0L6_2atmpS3420;
        uint32_t _M0L6_2atmpS3416;
        int32_t _M0L6_2atmpS3419;
        uint32_t _M0L6_2atmpS3418;
        uint32_t _M0L6_2atmpS3417;
        uint32_t _M0L6_2atmpS3412;
        int32_t _M0L6_2atmpS3415;
        uint32_t _M0L6_2atmpS3414;
        uint32_t _M0L6_2atmpS3413;
        uint32_t _M0L8sigma__1S1195;
        int32_t _M0L6_2atmpS3411;
        uint32_t _M0L6_2atmpS3410;
        uint32_t _M0L6_2atmpS3407;
        int32_t _M0L6_2atmpS3409;
        uint32_t _M0L6_2atmpS3408;
        uint32_t _M0L6_2atmpS3406;
        uint32_t _M0L6_2atmpS3405;
        int32_t _M0L6_2atmpS3432;
        if (
          _M0L6_2atmpS3431 < 0
          || _M0L6_2atmpS3431 >= Moonbit_array_length(_M0L1wS1180)
        ) {
          #line 66 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3430 = (uint32_t)_M0L1wS1180[_M0L6_2atmpS3431];
        #line 66 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3426
        = _M0FP311moonbitlang1x6crypto16rotate__right__u(_M0L6_2atmpS3430, 7);
        _M0L6_2atmpS3429 = _M0L5indexS1193 - 15;
        if (
          _M0L6_2atmpS3429 < 0
          || _M0L6_2atmpS3429 >= Moonbit_array_length(_M0L1wS1180)
        ) {
          #line 67 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3428 = (uint32_t)_M0L1wS1180[_M0L6_2atmpS3429];
        #line 67 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3427
        = _M0FP311moonbitlang1x6crypto16rotate__right__u(_M0L6_2atmpS3428, 18);
        _M0L6_2atmpS3422 = _M0L6_2atmpS3426 ^ _M0L6_2atmpS3427;
        _M0L6_2atmpS3425 = _M0L5indexS1193 - 15;
        if (
          _M0L6_2atmpS3425 < 0
          || _M0L6_2atmpS3425 >= Moonbit_array_length(_M0L1wS1180)
        ) {
          #line 68 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3424 = (uint32_t)_M0L1wS1180[_M0L6_2atmpS3425];
        _M0L6_2atmpS3423 = _M0L6_2atmpS3424 >> 3;
        _M0L8sigma__0S1194 = _M0L6_2atmpS3422 ^ _M0L6_2atmpS3423;
        _M0L6_2atmpS3421 = _M0L5indexS1193 - 2;
        if (
          _M0L6_2atmpS3421 < 0
          || _M0L6_2atmpS3421 >= Moonbit_array_length(_M0L1wS1180)
        ) {
          #line 69 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3420 = (uint32_t)_M0L1wS1180[_M0L6_2atmpS3421];
        #line 69 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3416
        = _M0FP311moonbitlang1x6crypto16rotate__right__u(_M0L6_2atmpS3420, 17);
        _M0L6_2atmpS3419 = _M0L5indexS1193 - 2;
        if (
          _M0L6_2atmpS3419 < 0
          || _M0L6_2atmpS3419 >= Moonbit_array_length(_M0L1wS1180)
        ) {
          #line 70 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3418 = (uint32_t)_M0L1wS1180[_M0L6_2atmpS3419];
        #line 70 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3417
        = _M0FP311moonbitlang1x6crypto16rotate__right__u(_M0L6_2atmpS3418, 19);
        _M0L6_2atmpS3412 = _M0L6_2atmpS3416 ^ _M0L6_2atmpS3417;
        _M0L6_2atmpS3415 = _M0L5indexS1193 - 2;
        if (
          _M0L6_2atmpS3415 < 0
          || _M0L6_2atmpS3415 >= Moonbit_array_length(_M0L1wS1180)
        ) {
          #line 71 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3414 = (uint32_t)_M0L1wS1180[_M0L6_2atmpS3415];
        _M0L6_2atmpS3413 = _M0L6_2atmpS3414 >> 10;
        _M0L8sigma__1S1195 = _M0L6_2atmpS3412 ^ _M0L6_2atmpS3413;
        _M0L6_2atmpS3411 = _M0L5indexS1193 - 16;
        if (
          _M0L6_2atmpS3411 < 0
          || _M0L6_2atmpS3411 >= Moonbit_array_length(_M0L1wS1180)
        ) {
          #line 72 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3410 = (uint32_t)_M0L1wS1180[_M0L6_2atmpS3411];
        _M0L6_2atmpS3407 = _M0L6_2atmpS3410 + _M0L8sigma__0S1194;
        _M0L6_2atmpS3409 = _M0L5indexS1193 - 7;
        if (
          _M0L6_2atmpS3409 < 0
          || _M0L6_2atmpS3409 >= Moonbit_array_length(_M0L1wS1180)
        ) {
          #line 72 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3408 = (uint32_t)_M0L1wS1180[_M0L6_2atmpS3409];
        _M0L6_2atmpS3406 = _M0L6_2atmpS3407 + _M0L6_2atmpS3408;
        _M0L6_2atmpS3405 = _M0L6_2atmpS3406 + _M0L8sigma__1S1195;
        if (
          _M0L5indexS1193 < 0
          || _M0L5indexS1193 >= Moonbit_array_length(_M0L1wS1180)
        ) {
          #line 72 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L1wS1180[_M0L5indexS1193] = _M0L6_2atmpS3405;
        _M0L6_2atmpS3432 = _M0L5indexS1193 + 1;
        _M0L5indexS1193 = _M0L6_2atmpS3432;
        continue;
      }
      break;
    }
    _M0L5indexS1197 = 0;
    while (1) {
      if (_M0L5indexS1197 < 64) {
        uint32_t _M0L3valS3469 = _M0L1eS1186->$0;
        uint32_t _M0L6_2atmpS3466;
        uint32_t _M0L3valS3468;
        uint32_t _M0L6_2atmpS3467;
        uint32_t _M0L6_2atmpS3463;
        uint32_t _M0L3valS3465;
        uint32_t _M0L6_2atmpS3464;
        uint32_t _M0L13big__sigma__1S1198;
        uint32_t _M0L3valS3462;
        uint32_t _M0L6_2atmpS3457;
        uint32_t _M0L3valS3459;
        uint32_t _M0L3valS3460;
        uint32_t _M0L3valS3461;
        uint32_t _M0L6_2atmpS3458;
        uint32_t _M0L6_2atmpS3455;
        uint32_t _M0L6_2atmpS3456;
        uint32_t _M0L6_2atmpS3453;
        uint32_t _M0L6_2atmpS3454;
        uint32_t _M0L4t__1S1199;
        uint32_t _M0L3valS3452;
        uint32_t _M0L6_2atmpS3449;
        uint32_t _M0L3valS3451;
        uint32_t _M0L6_2atmpS3450;
        uint32_t _M0L6_2atmpS3446;
        uint32_t _M0L3valS3448;
        uint32_t _M0L6_2atmpS3447;
        uint32_t _M0L13big__sigma__0S1200;
        uint32_t _M0L3valS3443;
        uint32_t _M0L3valS3444;
        uint32_t _M0L3valS3445;
        uint32_t _M0L6_2atmpS3442;
        uint32_t _M0L4t__2S1201;
        uint32_t _M0L3valS3433;
        uint32_t _M0L3valS3434;
        uint32_t _M0L3valS3435;
        uint32_t _M0L3valS3437;
        uint32_t _M0L6_2atmpS3436;
        uint32_t _M0L3valS3438;
        uint32_t _M0L3valS3439;
        uint32_t _M0L3valS3440;
        uint32_t _M0L6_2atmpS3441;
        int32_t _M0L6_2atmpS3470;
        #line 75 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3466
        = _M0FP311moonbitlang1x6crypto16rotate__right__u(_M0L3valS3469, 6);
        _M0L3valS3468 = _M0L1eS1186->$0;
        #line 76 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3467
        = _M0FP311moonbitlang1x6crypto16rotate__right__u(_M0L3valS3468, 11);
        _M0L6_2atmpS3463 = _M0L6_2atmpS3466 ^ _M0L6_2atmpS3467;
        _M0L3valS3465 = _M0L1eS1186->$0;
        #line 77 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3464
        = _M0FP311moonbitlang1x6crypto16rotate__right__u(_M0L3valS3465, 25);
        _M0L13big__sigma__1S1198 = _M0L6_2atmpS3463 ^ _M0L6_2atmpS3464;
        _M0L3valS3462 = _M0L1hS1189->$0;
        _M0L6_2atmpS3457 = _M0L3valS3462 + _M0L13big__sigma__1S1198;
        _M0L3valS3459 = _M0L1eS1186->$0;
        _M0L3valS3460 = _M0L1fS1187->$0;
        _M0L3valS3461 = _M0L1gS1188->$0;
        #line 78 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3458
        = _M0MP311moonbitlang1x6crypto3SM35gg__1(_M0L3valS3459, _M0L3valS3460, _M0L3valS3461);
        _M0L6_2atmpS3455 = _M0L6_2atmpS3457 + _M0L6_2atmpS3458;
        if (
          _M0L5indexS1197 < 0
          || _M0L5indexS1197
             >= Moonbit_array_length(_M0FP311moonbitlang1x6crypto9sha256__t)
        ) {
          #line 78 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3456
        = (uint32_t)_M0FP311moonbitlang1x6crypto9sha256__t[_M0L5indexS1197];
        _M0L6_2atmpS3453 = _M0L6_2atmpS3455 + _M0L6_2atmpS3456;
        if (
          _M0L5indexS1197 < 0
          || _M0L5indexS1197 >= Moonbit_array_length(_M0L1wS1180)
        ) {
          #line 78 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3454 = (uint32_t)_M0L1wS1180[_M0L5indexS1197];
        _M0L4t__1S1199 = _M0L6_2atmpS3453 + _M0L6_2atmpS3454;
        _M0L3valS3452 = _M0L1aS1182->$0;
        #line 79 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3449
        = _M0FP311moonbitlang1x6crypto16rotate__right__u(_M0L3valS3452, 2);
        _M0L3valS3451 = _M0L1aS1182->$0;
        #line 80 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3450
        = _M0FP311moonbitlang1x6crypto16rotate__right__u(_M0L3valS3451, 13);
        _M0L6_2atmpS3446 = _M0L6_2atmpS3449 ^ _M0L6_2atmpS3450;
        _M0L3valS3448 = _M0L1aS1182->$0;
        #line 81 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3447
        = _M0FP311moonbitlang1x6crypto16rotate__right__u(_M0L3valS3448, 22);
        _M0L13big__sigma__0S1200 = _M0L6_2atmpS3446 ^ _M0L6_2atmpS3447;
        _M0L3valS3443 = _M0L1aS1182->$0;
        _M0L3valS3444 = _M0L1bS1183->$0;
        _M0L3valS3445 = _M0L1cS1184->$0;
        #line 82 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
        _M0L6_2atmpS3442
        = _M0MP311moonbitlang1x6crypto3SM35ff__1(_M0L3valS3443, _M0L3valS3444, _M0L3valS3445);
        _M0L4t__2S1201 = _M0L13big__sigma__0S1200 + _M0L6_2atmpS3442;
        _M0L3valS3433 = _M0L1gS1188->$0;
        _M0L1hS1189->$0 = _M0L3valS3433;
        _M0L3valS3434 = _M0L1fS1187->$0;
        _M0L1gS1188->$0 = _M0L3valS3434;
        _M0L3valS3435 = _M0L1eS1186->$0;
        _M0L1fS1187->$0 = _M0L3valS3435;
        _M0L3valS3437 = _M0L1dS1185->$0;
        _M0L6_2atmpS3436 = _M0L3valS3437 + _M0L4t__1S1199;
        _M0L1eS1186->$0 = _M0L6_2atmpS3436;
        _M0L3valS3438 = _M0L1cS1184->$0;
        _M0L1dS1185->$0 = _M0L3valS3438;
        _M0L3valS3439 = _M0L1bS1183->$0;
        _M0L1cS1184->$0 = _M0L3valS3439;
        _M0L3valS3440 = _M0L1aS1182->$0;
        _M0L1bS1183->$0 = _M0L3valS3440;
        _M0L6_2atmpS3441 = _M0L4t__1S1199 + _M0L4t__2S1201;
        _M0L1aS1182->$0 = _M0L6_2atmpS3441;
        _M0L6_2atmpS3470 = _M0L5indexS1197 + 1;
        _M0L5indexS1197 = _M0L6_2atmpS3470;
        continue;
      } else {
        moonbit_decref(_M0L1wS1180);
      }
      break;
    }
    _M0L6_2atmpS3472 = (uint32_t)_M0L3regS1181[0];
    _M0L8_2afieldS3825 = _M0L1aS1182->$0;
    moonbit_decref(_M0L1aS1182);
    _M0L3valS3473 = _M0L8_2afieldS3825;
    _M0L6_2atmpS3471 = _M0L6_2atmpS3472 + _M0L3valS3473;
    _M0L3regS1181[0] = _M0L6_2atmpS3471;
    _M0L6_2atmpS3475 = (uint32_t)_M0L3regS1181[1];
    _M0L8_2afieldS3824 = _M0L1bS1183->$0;
    moonbit_decref(_M0L1bS1183);
    _M0L3valS3476 = _M0L8_2afieldS3824;
    _M0L6_2atmpS3474 = _M0L6_2atmpS3475 + _M0L3valS3476;
    _M0L3regS1181[1] = _M0L6_2atmpS3474;
    _M0L6_2atmpS3478 = (uint32_t)_M0L3regS1181[2];
    _M0L8_2afieldS3823 = _M0L1cS1184->$0;
    moonbit_decref(_M0L1cS1184);
    _M0L3valS3479 = _M0L8_2afieldS3823;
    _M0L6_2atmpS3477 = _M0L6_2atmpS3478 + _M0L3valS3479;
    _M0L3regS1181[2] = _M0L6_2atmpS3477;
    _M0L6_2atmpS3481 = (uint32_t)_M0L3regS1181[3];
    _M0L8_2afieldS3822 = _M0L1dS1185->$0;
    moonbit_decref(_M0L1dS1185);
    _M0L3valS3482 = _M0L8_2afieldS3822;
    _M0L6_2atmpS3480 = _M0L6_2atmpS3481 + _M0L3valS3482;
    _M0L3regS1181[3] = _M0L6_2atmpS3480;
    _M0L6_2atmpS3484 = (uint32_t)_M0L3regS1181[4];
    _M0L8_2afieldS3821 = _M0L1eS1186->$0;
    moonbit_decref(_M0L1eS1186);
    _M0L3valS3485 = _M0L8_2afieldS3821;
    _M0L6_2atmpS3483 = _M0L6_2atmpS3484 + _M0L3valS3485;
    _M0L3regS1181[4] = _M0L6_2atmpS3483;
    _M0L6_2atmpS3487 = (uint32_t)_M0L3regS1181[5];
    _M0L8_2afieldS3820 = _M0L1fS1187->$0;
    moonbit_decref(_M0L1fS1187);
    _M0L3valS3488 = _M0L8_2afieldS3820;
    _M0L6_2atmpS3486 = _M0L6_2atmpS3487 + _M0L3valS3488;
    _M0L3regS1181[5] = _M0L6_2atmpS3486;
    _M0L6_2atmpS3490 = (uint32_t)_M0L3regS1181[6];
    _M0L8_2afieldS3819 = _M0L1gS1188->$0;
    moonbit_decref(_M0L1gS1188);
    _M0L3valS3491 = _M0L8_2afieldS3819;
    _M0L6_2atmpS3489 = _M0L6_2atmpS3490 + _M0L3valS3491;
    _M0L3regS1181[6] = _M0L6_2atmpS3489;
    _M0L6_2atmpS3493 = (uint32_t)_M0L3regS1181[7];
    _M0L8_2afieldS3818 = _M0L1hS1189->$0;
    moonbit_decref(_M0L1hS1189);
    _M0L3valS3494 = _M0L8_2afieldS3818;
    _M0L6_2atmpS3492 = _M0L6_2atmpS3493 + _M0L3valS3494;
    _M0L3regS1181[7] = _M0L6_2atmpS3492;
    moonbit_decref(_M0L3regS1181);
  } else {
    moonbit_decref(_M0L4dataS1191);
    moonbit_decref(_M0L3regS1181);
    moonbit_decref(_M0L1wS1180);
    #line 53 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
    moonbit_panic();
  }
  return 0;
}

struct _M0TP311moonbitlang1x6crypto6SHA256* _M0MP311moonbitlang1x6crypto6SHA2563new(
  uint32_t* _M0L9reg_2eoptS1178
) {
  uint32_t* _M0L3regS1177;
  if (_M0L9reg_2eoptS1178 == 0) {
    if (_M0L9reg_2eoptS1178) {
      moonbit_decref(_M0L9reg_2eoptS1178);
    }
    _M0L3regS1177 = (uint32_t*)moonbit_make_int32_array_raw(8);
    _M0L3regS1177[0] = 1779033703u;
    _M0L3regS1177[1] = 3144134277u;
    _M0L3regS1177[2] = 1013904242u;
    _M0L3regS1177[3] = 2773480762u;
    _M0L3regS1177[4] = 1359893119u;
    _M0L3regS1177[5] = 2600822924u;
    _M0L3regS1177[6] = 528734635u;
    _M0L3regS1177[7] = 1541459225u;
  } else {
    uint32_t* _M0L7_2aSomeS1179 = _M0L9reg_2eoptS1178;
    _M0L3regS1177 = _M0L7_2aSomeS1179;
  }
  return _M0MP311moonbitlang1x6crypto6SHA25611new_2einner(_M0L3regS1177);
}

struct _M0TP311moonbitlang1x6crypto6SHA256* _M0MP311moonbitlang1x6crypto6SHA25611new_2einner(
  uint32_t* _M0L3regS1176
) {
  int32_t _M0L6_2atmpS3400;
  moonbit_bytes_t _M0L6_2atmpS3399;
  struct _M0TP311moonbitlang1x6crypto6SHA256* _block_4527;
  #line 27 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  #line 33 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sha256.mbt"
  _M0L6_2atmpS3400 = _M0IPC14byte4BytePB7Default7default();
  _M0L6_2atmpS3399
  = (moonbit_bytes_t)moonbit_make_bytes(64, _M0L6_2atmpS3400);
  _block_4527
  = (struct _M0TP311moonbitlang1x6crypto6SHA256*)moonbit_malloc(sizeof(struct _M0TP311moonbitlang1x6crypto6SHA256));
  Moonbit_object_header(_block_4527)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP311moonbitlang1x6crypto6SHA256, $0) >> 2, 2, 0);
  _block_4527->$0 = _M0L3regS1176;
  _block_4527->$1 = 0ull;
  _block_4527->$2 = _M0L6_2atmpS3399;
  _block_4527->$3 = 0;
  return _block_4527;
}

uint32_t _M0MP311moonbitlang1x6crypto3SM35gg__1(
  uint32_t _M0L1xS1175,
  uint32_t _M0L1yS1173,
  uint32_t _M0L1zS1174
) {
  uint32_t _M0L6_2atmpS3398;
  uint32_t _M0L6_2atmpS3397;
  #line 79 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sm3.mbt"
  _M0L6_2atmpS3398 = _M0L1yS1173 ^ _M0L1zS1174;
  _M0L6_2atmpS3397 = _M0L6_2atmpS3398 & _M0L1xS1175;
  return _M0L6_2atmpS3397 ^ _M0L1zS1174;
}

uint32_t _M0MP311moonbitlang1x6crypto3SM35ff__1(
  uint32_t _M0L1xS1170,
  uint32_t _M0L1yS1171,
  uint32_t _M0L1zS1172
) {
  uint32_t _M0L6_2atmpS3395;
  uint32_t _M0L6_2atmpS3396;
  uint32_t _M0L6_2atmpS3393;
  uint32_t _M0L6_2atmpS3394;
  #line 66 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\sm3.mbt"
  _M0L6_2atmpS3395 = _M0L1xS1170 & _M0L1yS1171;
  _M0L6_2atmpS3396 = _M0L1xS1170 & _M0L1zS1172;
  _M0L6_2atmpS3393 = _M0L6_2atmpS3395 | _M0L6_2atmpS3396;
  _M0L6_2atmpS3394 = _M0L1yS1171 & _M0L1zS1172;
  return _M0L6_2atmpS3393 | _M0L6_2atmpS3394;
}

uint32_t _M0FP311moonbitlang1x6crypto16rotate__right__u(
  uint32_t _M0L1xS1168,
  int32_t _M0L1nS1169
) {
  uint32_t _M0L6_2atmpS3390;
  int32_t _M0L6_2atmpS3392;
  uint32_t _M0L6_2atmpS3391;
  #line 139 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L6_2atmpS3390 = _M0L1xS1168 >> (_M0L1nS1169 & 31);
  _M0L6_2atmpS3392 = 32 - _M0L1nS1169;
  _M0L6_2atmpS3391 = _M0L1xS1168 << (_M0L6_2atmpS3392 & 31);
  return _M0L6_2atmpS3390 | _M0L6_2atmpS3391;
}

int32_t _M0FP311moonbitlang1x6crypto24arr__u32__to__u8be__into(
  struct _M0TWEOj* _M0L1xS1165,
  moonbit_bytes_t _M0L6bufferS1167,
  int32_t _M0L6offsetS1164
) {
  struct _M0TPC13ref3RefGiE* _M0L3idxS1163;
  struct _M0R76_24moonbitlang_2fx_2fcrypto_2earr__u32__to__u8be__into_2eanon__u3371__l117__* _closure_4528;
  struct _M0TWjEu* _M0L6_2atmpS3370;
  #line 111 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L3idxS1163
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3idxS1163)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3idxS1163->$0 = _M0L6offsetS1164;
  _closure_4528
  = (struct _M0R76_24moonbitlang_2fx_2fcrypto_2earr__u32__to__u8be__into_2eanon__u3371__l117__*)moonbit_malloc(sizeof(struct _M0R76_24moonbitlang_2fx_2fcrypto_2earr__u32__to__u8be__into_2eanon__u3371__l117__));
  Moonbit_object_header(_closure_4528)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R76_24moonbitlang_2fx_2fcrypto_2earr__u32__to__u8be__into_2eanon__u3371__l117__, $0) >> 2, 2, 0);
  _closure_4528->code
  = &_M0FP311moonbitlang1x6crypto24arr__u32__to__u8be__intoC3371l117;
  _closure_4528->$0 = _M0L6bufferS1167;
  _closure_4528->$1 = _M0L3idxS1163;
  _M0L6_2atmpS3370 = (struct _M0TWjEu*)_closure_4528;
  #line 117 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0MPB4Iter4eachGjE(_M0L1xS1165, _M0L6_2atmpS3370);
  return 0;
}

int32_t _M0FP311moonbitlang1x6crypto24arr__u32__to__u8be__intoC3371l117(
  struct _M0TWjEu* _M0L6_2aenvS3372,
  uint32_t _M0L1dS1166
) {
  struct _M0R76_24moonbitlang_2fx_2fcrypto_2earr__u32__to__u8be__into_2eanon__u3371__l117__* _M0L14_2acasted__envS3373;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3827;
  struct _M0TPC13ref3RefGiE* _M0L3idxS1163;
  moonbit_bytes_t _M0L8_2afieldS3826;
  int32_t _M0L6_2acntS4413;
  moonbit_bytes_t _M0L6bufferS1167;
  int32_t _M0L3valS3374;
  uint32_t _M0L6_2atmpS3376;
  int32_t _M0L6_2atmpS3375;
  int32_t _M0L3valS3380;
  int32_t _M0L6_2atmpS3377;
  uint32_t _M0L6_2atmpS3379;
  int32_t _M0L6_2atmpS3378;
  int32_t _M0L3valS3384;
  int32_t _M0L6_2atmpS3381;
  uint32_t _M0L6_2atmpS3383;
  int32_t _M0L6_2atmpS3382;
  int32_t _M0L3valS3387;
  int32_t _M0L6_2atmpS3385;
  int32_t _M0L6_2atmpS3386;
  int32_t _M0L3valS3389;
  int32_t _M0L6_2atmpS3388;
  #line 117 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L14_2acasted__envS3373
  = (struct _M0R76_24moonbitlang_2fx_2fcrypto_2earr__u32__to__u8be__into_2eanon__u3371__l117__*)_M0L6_2aenvS3372;
  _M0L8_2afieldS3827 = _M0L14_2acasted__envS3373->$1;
  _M0L3idxS1163 = _M0L8_2afieldS3827;
  _M0L8_2afieldS3826 = _M0L14_2acasted__envS3373->$0;
  _M0L6_2acntS4413 = Moonbit_object_header(_M0L14_2acasted__envS3373)->rc;
  if (_M0L6_2acntS4413 > 1) {
    int32_t _M0L11_2anew__cntS4414 = _M0L6_2acntS4413 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3373)->rc
    = _M0L11_2anew__cntS4414;
    moonbit_incref(_M0L3idxS1163);
    moonbit_incref(_M0L8_2afieldS3826);
  } else if (_M0L6_2acntS4413 == 1) {
    #line 117 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
    moonbit_free(_M0L14_2acasted__envS3373);
  }
  _M0L6bufferS1167 = _M0L8_2afieldS3826;
  _M0L3valS3374 = _M0L3idxS1163->$0;
  _M0L6_2atmpS3376 = _M0L1dS1166 >> 24;
  #line 118 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L6_2atmpS3375 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS3376);
  if (
    _M0L3valS3374 < 0
    || _M0L3valS3374 >= Moonbit_array_length(_M0L6bufferS1167)
  ) {
    #line 118 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
    moonbit_panic();
  }
  _M0L6bufferS1167[_M0L3valS3374] = _M0L6_2atmpS3375;
  _M0L3valS3380 = _M0L3idxS1163->$0;
  _M0L6_2atmpS3377 = _M0L3valS3380 + 1;
  _M0L6_2atmpS3379 = _M0L1dS1166 >> 16;
  #line 119 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L6_2atmpS3378 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS3379);
  if (
    _M0L6_2atmpS3377 < 0
    || _M0L6_2atmpS3377 >= Moonbit_array_length(_M0L6bufferS1167)
  ) {
    #line 119 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
    moonbit_panic();
  }
  _M0L6bufferS1167[_M0L6_2atmpS3377] = _M0L6_2atmpS3378;
  _M0L3valS3384 = _M0L3idxS1163->$0;
  _M0L6_2atmpS3381 = _M0L3valS3384 + 2;
  _M0L6_2atmpS3383 = _M0L1dS1166 >> 8;
  #line 120 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L6_2atmpS3382 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS3383);
  if (
    _M0L6_2atmpS3381 < 0
    || _M0L6_2atmpS3381 >= Moonbit_array_length(_M0L6bufferS1167)
  ) {
    #line 120 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
    moonbit_panic();
  }
  _M0L6bufferS1167[_M0L6_2atmpS3381] = _M0L6_2atmpS3382;
  _M0L3valS3387 = _M0L3idxS1163->$0;
  _M0L6_2atmpS3385 = _M0L3valS3387 + 3;
  #line 121 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L6_2atmpS3386 = _M0MPC14uint4UInt8to__byte(_M0L1dS1166);
  if (
    _M0L6_2atmpS3385 < 0
    || _M0L6_2atmpS3385 >= Moonbit_array_length(_M0L6bufferS1167)
  ) {
    #line 121 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
    moonbit_panic();
  }
  _M0L6bufferS1167[_M0L6_2atmpS3385] = _M0L6_2atmpS3386;
  moonbit_decref(_M0L6bufferS1167);
  _M0L3valS3389 = _M0L3idxS1163->$0;
  _M0L6_2atmpS3388 = _M0L3valS3389 + 4;
  _M0L3idxS1163->$0 = _M0L6_2atmpS3388;
  moonbit_decref(_M0L3idxS1163);
  return 0;
}

uint32_t _M0FP311moonbitlang1x6crypto28bytes__u8__to__u32be_2einner(
  moonbit_bytes_t _M0L1xS1161,
  int32_t _M0L1iS1162
) {
  int32_t _M0L6_2atmpS3369;
  uint32_t _M0L6_2atmpS3368;
  uint32_t _M0L6_2atmpS3363;
  int32_t _M0L6_2atmpS3367;
  int32_t _M0L6_2atmpS3366;
  uint32_t _M0L6_2atmpS3365;
  uint32_t _M0L6_2atmpS3364;
  uint32_t _M0L6_2atmpS3358;
  int32_t _M0L6_2atmpS3362;
  int32_t _M0L6_2atmpS3361;
  uint32_t _M0L6_2atmpS3360;
  uint32_t _M0L6_2atmpS3359;
  uint32_t _M0L6_2atmpS3354;
  int32_t _M0L6_2atmpS3357;
  int32_t _M0L6_2atmpS3828;
  int32_t _M0L6_2atmpS3356;
  uint32_t _M0L6_2atmpS3355;
  #line 103 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  if (_M0L1iS1162 < 0 || _M0L1iS1162 >= Moonbit_array_length(_M0L1xS1161)) {
    #line 104 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3369 = (int32_t)_M0L1xS1161[_M0L1iS1162];
  #line 104 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L6_2atmpS3368 = _M0FP311moonbitlang1x6crypto6uint32(_M0L6_2atmpS3369);
  _M0L6_2atmpS3363 = _M0L6_2atmpS3368 << 24;
  _M0L6_2atmpS3367 = _M0L1iS1162 + 1;
  if (
    _M0L6_2atmpS3367 < 0
    || _M0L6_2atmpS3367 >= Moonbit_array_length(_M0L1xS1161)
  ) {
    #line 105 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3366 = (int32_t)_M0L1xS1161[_M0L6_2atmpS3367];
  #line 105 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L6_2atmpS3365 = _M0FP311moonbitlang1x6crypto6uint32(_M0L6_2atmpS3366);
  _M0L6_2atmpS3364 = _M0L6_2atmpS3365 << 16;
  _M0L6_2atmpS3358 = _M0L6_2atmpS3363 | _M0L6_2atmpS3364;
  _M0L6_2atmpS3362 = _M0L1iS1162 + 2;
  if (
    _M0L6_2atmpS3362 < 0
    || _M0L6_2atmpS3362 >= Moonbit_array_length(_M0L1xS1161)
  ) {
    #line 106 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3361 = (int32_t)_M0L1xS1161[_M0L6_2atmpS3362];
  #line 106 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L6_2atmpS3360 = _M0FP311moonbitlang1x6crypto6uint32(_M0L6_2atmpS3361);
  _M0L6_2atmpS3359 = _M0L6_2atmpS3360 << 8;
  _M0L6_2atmpS3354 = _M0L6_2atmpS3358 | _M0L6_2atmpS3359;
  _M0L6_2atmpS3357 = _M0L1iS1162 + 3;
  if (
    _M0L6_2atmpS3357 < 0
    || _M0L6_2atmpS3357 >= Moonbit_array_length(_M0L1xS1161)
  ) {
    #line 107 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3828 = (int32_t)_M0L1xS1161[_M0L6_2atmpS3357];
  moonbit_decref(_M0L1xS1161);
  _M0L6_2atmpS3356 = _M0L6_2atmpS3828;
  #line 107 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L6_2atmpS3355 = _M0FP311moonbitlang1x6crypto6uint32(_M0L6_2atmpS3356);
  return _M0L6_2atmpS3354 | _M0L6_2atmpS3355;
}

uint32_t _M0FP311moonbitlang1x6crypto6uint32(int32_t _M0L1xS1160) {
  int32_t _M0L6_2atmpS3353;
  #line 68 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\crypto\\utils.mbt"
  _M0L6_2atmpS3353 = (int32_t)_M0L1xS1160;
  return *(uint32_t*)&_M0L6_2atmpS3353;
}

moonbit_string_t _M0FP411moonbitlang1x5codec6base6414encode_2einner(
  struct _M0TPC15bytes9BytesView _M0L5bytesS1157,
  int32_t _M0L9url__safeS1159
) {
  struct _M0TPB13StringBuilder* _M0L7builderS1155;
  struct _M0TP411moonbitlang1x5codec6base647Encoder* _M0L7encoderS1156;
  struct _M0R74_24moonbitlang_2fx_2fcodec_2fbase64_2eencode_2einner_2eanon__u3350__l113__* _closure_4529;
  struct _M0TWuEu* _M0L6_2atmpS3349;
  #line 108 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
  #line 109 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
  _M0L7builderS1155 = _M0MPB13StringBuilder11new_2einner(0);
  #line 110 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
  _M0L7encoderS1156 = _M0MP411moonbitlang1x5codec6base647Encoder3new();
  moonbit_incref(_M0L7builderS1155);
  _closure_4529
  = (struct _M0R74_24moonbitlang_2fx_2fcodec_2fbase64_2eencode_2einner_2eanon__u3350__l113__*)moonbit_malloc(sizeof(struct _M0R74_24moonbitlang_2fx_2fcodec_2fbase64_2eencode_2einner_2eanon__u3350__l113__));
  Moonbit_object_header(_closure_4529)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R74_24moonbitlang_2fx_2fcodec_2fbase64_2eencode_2einner_2eanon__u3350__l113__, $0) >> 2, 1, 0);
  _closure_4529->code
  = &_M0FP411moonbitlang1x5codec6base6414encode_2einnerC3350l113;
  _closure_4529->$0 = _M0L7builderS1155;
  _M0L6_2atmpS3349 = (struct _M0TWuEu*)_closure_4529;
  #line 111 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
  _M0MP411moonbitlang1x5codec6base647Encoder18encode__to_2einner(_M0L7encoderS1156, _M0L5bytesS1157, _M0L6_2atmpS3349, _M0L9url__safeS1159, 1);
  #line 117 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L7builderS1155);
}

int32_t _M0FP411moonbitlang1x5codec6base6414encode_2einnerC3350l113(
  struct _M0TWuEu* _M0L6_2aenvS3351,
  int32_t _M0L2chS1158
) {
  struct _M0R74_24moonbitlang_2fx_2fcodec_2fbase64_2eencode_2einner_2eanon__u3350__l113__* _M0L14_2acasted__envS3352;
  struct _M0TPB13StringBuilder* _M0L8_2afieldS3829;
  int32_t _M0L6_2acntS4415;
  struct _M0TPB13StringBuilder* _M0L7builderS1155;
  #line 113 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
  _M0L14_2acasted__envS3352
  = (struct _M0R74_24moonbitlang_2fx_2fcodec_2fbase64_2eencode_2einner_2eanon__u3350__l113__*)_M0L6_2aenvS3351;
  _M0L8_2afieldS3829 = _M0L14_2acasted__envS3352->$0;
  _M0L6_2acntS4415 = Moonbit_object_header(_M0L14_2acasted__envS3352)->rc;
  if (_M0L6_2acntS4415 > 1) {
    int32_t _M0L11_2anew__cntS4416 = _M0L6_2acntS4415 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3352)->rc
    = _M0L11_2anew__cntS4416;
    moonbit_incref(_M0L8_2afieldS3829);
  } else if (_M0L6_2acntS4415 == 1) {
    #line 113 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
    moonbit_free(_M0L14_2acasted__envS3352);
  }
  _M0L7builderS1155 = _M0L8_2afieldS3829;
  #line 113 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7builderS1155, _M0L2chS1158);
  return 0;
}

int32_t _M0MP411moonbitlang1x5codec6base647Encoder18encode__to_2einner(
  struct _M0TP411moonbitlang1x5codec6base647Encoder* _M0L4selfS1146,
  struct _M0TPC15bytes9BytesView _M0L5bytesS1141,
  struct _M0TWuEu* _M0L2cbS1147,
  int32_t _M0L9url__safeS1148,
  int32_t _M0L7paddingS1153
) {
  struct _M0TWEOc* _M0L5_2aitS1140;
  #line 58 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
  #line 58 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
  _M0L5_2aitS1140 = _M0MPC15bytes9BytesView4iter(_M0L5bytesS1141);
  while (1) {
    int32_t _M0L4byteS1143;
    int32_t _M0L7_2abindS1150;
    int32_t _M0L4byteS1144;
    int32_t _M0L7_2abindS1145;
    moonbit_incref(_M0L5_2aitS1140);
    #line 66 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
    _M0L7_2abindS1150 = _M0MPB4Iter4nextGyE(_M0L5_2aitS1140);
    if (_M0L7_2abindS1150 == -1) {
      moonbit_decref(_M0L5_2aitS1140);
    } else {
      int32_t _M0L7_2aSomeS1151 = _M0L7_2abindS1150;
      int32_t _M0L7_2abyteS1152 = _M0L7_2aSomeS1151;
      _M0L4byteS1143 = _M0L7_2abyteS1152;
      goto join_1142;
    }
    goto joinlet_4531;
    join_1142:;
    _M0L4byteS1144 = (int32_t)_M0L4byteS1143;
    _M0L7_2abindS1145 = _M0L4selfS1146->$0;
    switch (_M0L7_2abindS1145) {
      case 0: {
        int32_t _M0L6_2atmpS3330 = _M0L4byteS1144 >> 2;
        int32_t _M0L6_2atmpS3329;
        int32_t _M0L6_2atmpS3332;
        int32_t _M0L6_2atmpS3331;
        #line 70 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L6_2atmpS3329
        = _M0FP411moonbitlang1x5codec6base6415index__to__char(_M0L6_2atmpS3330, _M0L9url__safeS1148);
        moonbit_incref(_M0L2cbS1147);
        #line 70 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L2cbS1147->code(_M0L2cbS1147, _M0L6_2atmpS3329);
        _M0L6_2atmpS3332 = _M0L4byteS1144 & 3;
        _M0L6_2atmpS3331 = _M0L6_2atmpS3332 << 4;
        _M0L4selfS1146->$1 = _M0L6_2atmpS3331;
        _M0L4selfS1146->$0 = 1;
        break;
      }
      
      case 1: {
        int32_t _M0L6bufferS3335 = _M0L4selfS1146->$1;
        int32_t _M0L6_2atmpS3336 = _M0L4byteS1144 >> 4;
        int32_t _M0L6_2atmpS3334 = _M0L6bufferS3335 | _M0L6_2atmpS3336;
        int32_t _M0L6_2atmpS3333;
        int32_t _M0L6_2atmpS3338;
        int32_t _M0L6_2atmpS3337;
        #line 75 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L6_2atmpS3333
        = _M0FP411moonbitlang1x5codec6base6415index__to__char(_M0L6_2atmpS3334, _M0L9url__safeS1148);
        moonbit_incref(_M0L2cbS1147);
        #line 75 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L2cbS1147->code(_M0L2cbS1147, _M0L6_2atmpS3333);
        _M0L6_2atmpS3338 = _M0L4byteS1144 & 15;
        _M0L6_2atmpS3337 = _M0L6_2atmpS3338 << 2;
        _M0L4selfS1146->$1 = _M0L6_2atmpS3337;
        _M0L4selfS1146->$0 = 2;
        break;
      }
      
      case 2: {
        int32_t _M0L6bufferS3341 = _M0L4selfS1146->$1;
        int32_t _M0L6_2atmpS3342 = _M0L4byteS1144 >> 6;
        int32_t _M0L6_2atmpS3340 = _M0L6bufferS3341 | _M0L6_2atmpS3342;
        int32_t _M0L6_2atmpS3339;
        int32_t _M0L6_2atmpS3344;
        int32_t _M0L6_2atmpS3343;
        #line 80 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L6_2atmpS3339
        = _M0FP411moonbitlang1x5codec6base6415index__to__char(_M0L6_2atmpS3340, _M0L9url__safeS1148);
        moonbit_incref(_M0L2cbS1147);
        #line 80 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L2cbS1147->code(_M0L2cbS1147, _M0L6_2atmpS3339);
        _M0L6_2atmpS3344 = _M0L4byteS1144 & 63;
        #line 81 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L6_2atmpS3343
        = _M0FP411moonbitlang1x5codec6base6415index__to__char(_M0L6_2atmpS3344, _M0L9url__safeS1148);
        moonbit_incref(_M0L2cbS1147);
        #line 81 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L2cbS1147->code(_M0L2cbS1147, _M0L6_2atmpS3343);
        _M0L4selfS1146->$1 = 0;
        _M0L4selfS1146->$0 = 0;
        break;
      }
      default: {
        #line 85 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        moonbit_panic();
        break;
      }
    }
    continue;
    joinlet_4531:;
    break;
  }
  if (_M0L7paddingS1153) {
    int32_t _M0L7_2abindS1154 = _M0L4selfS1146->$0;
    switch (_M0L7_2abindS1154) {
      case 0: {
        moonbit_decref(_M0L2cbS1147);
        break;
      }
      
      case 1: {
        int32_t _M0L6bufferS3346 = _M0L4selfS1146->$1;
        int32_t _M0L6_2atmpS3345;
        #line 92 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L6_2atmpS3345
        = _M0FP411moonbitlang1x5codec6base6415index__to__char(_M0L6bufferS3346, _M0L9url__safeS1148);
        moonbit_incref(_M0L2cbS1147);
        #line 92 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L2cbS1147->code(_M0L2cbS1147, _M0L6_2atmpS3345);
        moonbit_incref(_M0L2cbS1147);
        #line 93 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L2cbS1147->code(_M0L2cbS1147, 61);
        #line 94 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L2cbS1147->code(_M0L2cbS1147, 61);
        break;
      }
      
      case 2: {
        int32_t _M0L6bufferS3348 = _M0L4selfS1146->$1;
        int32_t _M0L6_2atmpS3347;
        #line 97 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L6_2atmpS3347
        = _M0FP411moonbitlang1x5codec6base6415index__to__char(_M0L6bufferS3348, _M0L9url__safeS1148);
        moonbit_incref(_M0L2cbS1147);
        #line 97 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L2cbS1147->code(_M0L2cbS1147, _M0L6_2atmpS3347);
        #line 98 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        _M0L2cbS1147->code(_M0L2cbS1147, 61);
        break;
      }
      default: {
        moonbit_decref(_M0L2cbS1147);
        #line 100 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
        moonbit_panic();
        break;
      }
    }
    _M0L4selfS1146->$0 = 0;
    moonbit_decref(_M0L4selfS1146);
  } else {
    moonbit_decref(_M0L2cbS1147);
    moonbit_decref(_M0L4selfS1146);
  }
  return 0;
}

struct _M0TP411moonbitlang1x5codec6base647Encoder* _M0MP411moonbitlang1x5codec6base647Encoder3new(
  
) {
  struct _M0TP411moonbitlang1x5codec6base647Encoder* _block_4532;
  #line 53 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
  _block_4532
  = (struct _M0TP411moonbitlang1x5codec6base647Encoder*)moonbit_malloc(sizeof(struct _M0TP411moonbitlang1x5codec6base647Encoder));
  Moonbit_object_header(_block_4532)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TP411moonbitlang1x5codec6base647Encoder) >> 2, 0, 0);
  _block_4532->$0 = 0;
  _block_4532->$1 = 0;
  return _block_4532;
}

int32_t _M0FP411moonbitlang1x5codec6base6415index__to__char(
  int32_t _M0L5indexS1138,
  int32_t _M0L9url__safeS1139
) {
  #line 20 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
  if (_M0L5indexS1138 >= 0 && _M0L5indexS1138 < 26) {
    int32_t _M0L6_2atmpS3328 = _M0L5indexS1138 + 65;
    return _M0L6_2atmpS3328;
  } else if (_M0L5indexS1138 >= 26 && _M0L5indexS1138 < 52) {
    int32_t _M0L6_2atmpS3327 = _M0L5indexS1138 - 26;
    int32_t _M0L6_2atmpS3326 = _M0L6_2atmpS3327 + 97;
    return _M0L6_2atmpS3326;
  } else if (_M0L5indexS1138 >= 52 && _M0L5indexS1138 < 62) {
    int32_t _M0L6_2atmpS3325 = _M0L5indexS1138 - 52;
    int32_t _M0L6_2atmpS3324 = _M0L6_2atmpS3325 + 48;
    return _M0L6_2atmpS3324;
  } else if (_M0L5indexS1138 == 62) {
    if (_M0L9url__safeS1139) {
      return 45;
    } else {
      return 43;
    }
  } else if (_M0L5indexS1138 == 63) {
    if (_M0L9url__safeS1139) {
      return 95;
    } else {
      return 47;
    }
  } else {
    #line 27 "E:\\moonbit\\clawteam\\.mooncakes\\moonbitlang\\x\\codec\\base64\\base64.mbt"
    moonbit_panic();
  }
}

struct moonbit_result_1 _M0FP48clawteam8clawteam8internal4rand7chacha8() {
  struct moonbit_result_2 _tmp_4533;
  moonbit_bytes_t _M0L6_2atmpS3321;
  struct _M0TPC16random6Source _M0L6_2atmpS3320;
  struct moonbit_result_1 _result_4535;
  #line 16 "E:\\moonbit\\clawteam\\internal\\rand\\rand.mbt"
  #line 17 "E:\\moonbit\\clawteam\\internal\\rand\\rand.mbt"
  _tmp_4533 = _M0FP48clawteam8clawteam8internal4rand5bytes(32);
  if (_tmp_4533.tag) {
    moonbit_bytes_t const _M0L5_2aokS3322 = _tmp_4533.data.ok;
    _M0L6_2atmpS3321 = _M0L5_2aokS3322;
  } else {
    void* const _M0L6_2aerrS3323 = _tmp_4533.data.err;
    struct moonbit_result_1 _result_4534;
    _result_4534.tag = 0;
    _result_4534.data.err = _M0L6_2aerrS3323;
    return _result_4534;
  }
  #line 17 "E:\\moonbit\\clawteam\\internal\\rand\\rand.mbt"
  _M0L6_2atmpS3320 = _M0MPC16random4Rand15chacha8_2einner(_M0L6_2atmpS3321);
  _result_4535.tag = 1;
  _result_4535.data.ok = _M0L6_2atmpS3320;
  return _result_4535;
}

struct moonbit_result_2 _M0FP48clawteam8clawteam8internal4rand5bytes(
  int32_t _M0L1nS1136
) {
  moonbit_bytes_t _M0L3bufS1135;
  int32_t _M0L5errnoS1137;
  struct moonbit_result_2 _result_4537;
  #line 6 "E:\\moonbit\\clawteam\\internal\\rand\\rand.mbt"
  #line 7 "E:\\moonbit\\clawteam\\internal\\rand\\rand.mbt"
  _M0L3bufS1135 = _M0MPC15bytes5Bytes4make(_M0L1nS1136, 0);
  #line 8 "E:\\moonbit\\clawteam\\internal\\rand\\rand.mbt"
  _M0L5errnoS1137
  = _M0FP48clawteam8clawteam8internal4rand11rand__bytes(_M0L3bufS1135);
  if (_M0L5errnoS1137 != 0) {
    void* _M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3319;
    struct moonbit_result_2 _result_4536;
    moonbit_decref(_M0L3bufS1135);
    _M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3319
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno));
    Moonbit_object_header(_M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3319)->meta
    = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno) >> 2, 0, 1);
    ((struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno*)_M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3319)->$0
    = _M0L5errnoS1137;
    _result_4536.tag = 0;
    _result_4536.data.err
    = _M0L54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrnoS3319;
    return _result_4536;
  }
  _result_4537.tag = 1;
  _result_4537.data.ok = _M0L3bufS1135;
  return _result_4537;
}

int32_t _M0IP48clawteam8clawteam8internal5errno5ErrnoPB4Show6output(
  void* _M0L4selfS1132,
  struct _M0TPB6Logger _M0L6loggerS1125
) {
  int32_t _M0L6errnumS1123;
  struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno* _M0L8_2aErrnoS1133;
  int32_t _M0L8_2afieldS3831;
  int32_t _M0L9_2aerrnumS1134;
  void* _M0L6c__strS1124;
  #line 28 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
  _M0L8_2aErrnoS1133
  = (struct _M0DTPC15error5Error54clawteam_2fclawteam_2finternal_2ferrno_2eErrno_2eErrno*)_M0L4selfS1132;
  _M0L8_2afieldS3831 = _M0L8_2aErrnoS1133->$0;
  moonbit_decref(_M0L8_2aErrnoS1133);
  _M0L9_2aerrnumS1134 = _M0L8_2afieldS3831;
  _M0L6errnumS1123 = _M0L9_2aerrnumS1134;
  goto join_1122;
  goto joinlet_4538;
  join_1122:;
  #line 30 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
  _M0L6c__strS1124
  = _M0FP48clawteam8clawteam8internal5errno15errno__strerror(_M0L6errnumS1123);
  #line 31 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
  if (
    _M0MP48clawteam8clawteam8internal1c7Pointer8is__nullGyE(_M0L6c__strS1124)
  ) {
    moonbit_string_t _M0L6_2atmpS3312;
    moonbit_string_t _M0L6_2atmpS3830;
    moonbit_string_t _M0L6_2atmpS3311;
    #line 32 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3312
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6errnumS1123);
    #line 32 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3830
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_14.data, _M0L6_2atmpS3312);
    moonbit_decref(_M0L6_2atmpS3312);
    _M0L6_2atmpS3311 = _M0L6_2atmpS3830;
    #line 32 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6loggerS1125.$0->$method_0(_M0L6loggerS1125.$1, _M0L6_2atmpS3311);
  } else {
    uint64_t _M0L6_2atmpS3318;
    int32_t _M0L6c__lenS1126;
    moonbit_bytes_t _M0L3bufS1127;
    int32_t _M0L1iS1128;
    moonbit_bytes_t _M0L7_2abindS1131;
    int32_t _M0L6_2atmpS3317;
    int64_t _M0L6_2atmpS3316;
    struct _M0TPC15bytes9BytesView _M0L6_2atmpS3315;
    moonbit_string_t _M0L3strS1130;
    #line 34 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3318
    = _M0FP48clawteam8clawteam8internal1c6strlen(_M0L6c__strS1124);
    _M0L6c__lenS1126 = (int32_t)_M0L6_2atmpS3318;
    _M0L3bufS1127 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6c__lenS1126, 0);
    _M0L1iS1128 = 0;
    while (1) {
      if (_M0L1iS1128 < _M0L6c__lenS1126) {
        int32_t _M0L6_2atmpS3313;
        int32_t _M0L6_2atmpS3314;
        #line 37 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
        _M0L6_2atmpS3313
        = _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(_M0L6c__strS1124, _M0L1iS1128);
        if (
          _M0L1iS1128 < 0
          || _M0L1iS1128 >= Moonbit_array_length(_M0L3bufS1127)
        ) {
          #line 37 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
          moonbit_panic();
        }
        _M0L3bufS1127[_M0L1iS1128] = _M0L6_2atmpS3313;
        _M0L6_2atmpS3314 = _M0L1iS1128 + 1;
        _M0L1iS1128 = _M0L6_2atmpS3314;
        continue;
      }
      break;
    }
    _M0L7_2abindS1131 = _M0L3bufS1127;
    _M0L6_2atmpS3317 = Moonbit_array_length(_M0L7_2abindS1131);
    _M0L6_2atmpS3316 = (int64_t)_M0L6_2atmpS3317;
    #line 39 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6_2atmpS3315
    = _M0MPC15bytes5Bytes12view_2einner(_M0L7_2abindS1131, 0, _M0L6_2atmpS3316);
    #line 39 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L3strS1130
    = _M0FPC28encoding4utf821decode__lossy_2einner(_M0L6_2atmpS3315, 0);
    #line 40 "E:\\moonbit\\clawteam\\internal\\errno\\errno.mbt"
    _M0L6loggerS1125.$0->$method_0(_M0L6loggerS1125.$1, _M0L3strS1130);
  }
  joinlet_4538:;
  return 0;
}

int32_t _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(
  void* _M0L7pointerS1120,
  int32_t _M0L6offsetS1121
) {
  #line 145 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 146 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0FP48clawteam8clawteam8internal1c22moonbit__c__load__byte(_M0L7pointerS1120, _M0L6offsetS1121);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer7op__getGyE(
  void* _M0L4selfS1118,
  int32_t _M0L5indexS1119
) {
  #line 53 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  #line 54 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0IPC14byte4ByteP48clawteam8clawteam8internal1c4Load4load(_M0L4selfS1118, _M0L5indexS1119);
}

int32_t _M0MP48clawteam8clawteam8internal1c7Pointer8is__nullGyE(
  void* _M0L4selfS1117
) {
  void* _M0L6_2atmpS3310;
  #line 24 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  _M0L6_2atmpS3310 = _M0L4selfS1117;
  #line 25 "E:\\moonbit\\clawteam\\internal\\c\\pointer.mbt"
  return _M0MP48clawteam8clawteam8internal1c7Pointer10__is__null(_M0L6_2atmpS3310);
}

int32_t _M0MPC16random4Rand11int_2einner(
  struct _M0TPC16random6Source _M0L4selfS1116,
  int32_t _M0L5limitS1115
) {
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  if (_M0L5limitS1115 == 0) {
    uint64_t _M0L6_2atmpS3307;
    uint64_t _M0L6_2atmpS3306;
    #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
    _M0L6_2atmpS3307 = _M0MPC16random4Rand4next(_M0L4selfS1116);
    _M0L6_2atmpS3306 = _M0L6_2atmpS3307 >> 33;
    return (int32_t)_M0L6_2atmpS3306;
  } else {
    uint32_t _M0L6_2atmpS3309 = *(uint32_t*)&_M0L5limitS1115;
    uint32_t _M0L6_2atmpS3308;
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
    _M0L6_2atmpS3308
    = _M0MPC16random4Rand12uint_2einner(_M0L4selfS1116, _M0L6_2atmpS3309);
    return *(int32_t*)&_M0L6_2atmpS3308;
  }
}

uint32_t _M0MPC16random4Rand12uint_2einner(
  struct _M0TPC16random6Source _M0L4selfS1114,
  uint32_t _M0L5limitS1113
) {
  uint64_t _M0L6_2atmpS3305;
  uint64_t _M0L6_2atmpS3304;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  if (_M0L5limitS1113 == 0u) {
    uint64_t _M0L6_2atmpS3303;
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
    _M0L6_2atmpS3303 = _M0MPC16random4Rand4next(_M0L4selfS1114);
    return (uint32_t)_M0L6_2atmpS3303;
  }
  #line 117 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  _M0L6_2atmpS3305 = _M0MPC14uint4UInt10to__uint64(_M0L5limitS1113);
  #line 117 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  _M0L6_2atmpS3304
  = _M0MPC16random4Rand14uint64_2einner(_M0L4selfS1114, _M0L6_2atmpS3305);
  return (uint32_t)_M0L6_2atmpS3304;
}

uint64_t _M0MPC16random4Rand14uint64_2einner(
  struct _M0TPC16random6Source _M0L4selfS1109,
  uint64_t _M0L5limitS1108
) {
  uint64_t _M0L6_2atmpS3302;
  struct _M0TPC16random7UInt128 _M0Lm1rS1110;
  struct _M0TPC16random7UInt128 _M0L6_2atmpS3296;
  uint64_t _M0L2loS3295;
  struct _M0TPC16random7UInt128 _M0L6_2atmpS3301;
  #line 138 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  if (_M0L5limitS1108 == 0ull) {
    #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
    return _M0MPC16random4Rand4next(_M0L4selfS1109);
  } else {
    uint64_t _M0L6_2atmpS3292 = _M0L5limitS1108 - 1ull;
    uint64_t _M0L6_2atmpS3291 = _M0L5limitS1108 & _M0L6_2atmpS3292;
    if (_M0L6_2atmpS3291 == 0ull) {
      uint64_t _M0L6_2atmpS3293;
      uint64_t _M0L6_2atmpS3294;
      #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
      _M0L6_2atmpS3293 = _M0MPC16random4Rand4next(_M0L4selfS1109);
      _M0L6_2atmpS3294 = _M0L5limitS1108 - 1ull;
      return _M0L6_2atmpS3293 & _M0L6_2atmpS3294;
    }
  }
  if (_M0L4selfS1109.$1) {
    moonbit_incref(_M0L4selfS1109.$1);
  }
  #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  _M0L6_2atmpS3302 = _M0MPC16random4Rand4next(_M0L4selfS1109);
  #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  _M0Lm1rS1110 = _M0FPC16random7umul128(_M0L6_2atmpS3302, _M0L5limitS1108);
  _M0L6_2atmpS3296 = _M0Lm1rS1110;
  _M0L2loS3295 = _M0L6_2atmpS3296.$1;
  if (_M0L2loS3295 < _M0L5limitS1108) {
    uint64_t _M0L6_2atmpS3300 = ~_M0L5limitS1108;
    uint64_t _M0L6threshS1111 = _M0L6_2atmpS3300 % _M0L5limitS1108;
    while (1) {
      struct _M0TPC16random7UInt128 _M0L6_2atmpS3298 = _M0Lm1rS1110;
      uint64_t _M0L2loS3297 = _M0L6_2atmpS3298.$1;
      if (_M0L2loS3297 < _M0L6threshS1111) {
        uint64_t _M0L6_2atmpS3299;
        if (_M0L4selfS1109.$1) {
          moonbit_incref(_M0L4selfS1109.$1);
        }
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
        _M0L6_2atmpS3299 = _M0MPC16random4Rand4next(_M0L4selfS1109);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
        _M0Lm1rS1110
        = _M0FPC16random7umul128(_M0L6_2atmpS3299, _M0L5limitS1108);
        continue;
      } else if (_M0L4selfS1109.$1) {
        moonbit_decref(_M0L4selfS1109.$1);
      }
      break;
    }
  } else if (_M0L4selfS1109.$1) {
    moonbit_decref(_M0L4selfS1109.$1);
  }
  _M0L6_2atmpS3301 = _M0Lm1rS1110;
  return _M0L6_2atmpS3301.$0;
}

struct _M0TPC16random7UInt128 _M0FPC16random7umul128(
  uint64_t _M0L1aS1099,
  uint64_t _M0L1bS1102
) {
  uint64_t _M0L3aLoS1098;
  uint64_t _M0L3aHiS1100;
  uint64_t _M0L3bLoS1101;
  uint64_t _M0L3bHiS1103;
  uint64_t _M0L1xS1104;
  uint64_t _M0L6_2atmpS3289;
  uint64_t _M0L6_2atmpS3290;
  uint64_t _M0L1yS1105;
  uint64_t _M0L6_2atmpS3287;
  uint64_t _M0L6_2atmpS3288;
  uint64_t _M0L1zS1106;
  uint64_t _M0L6_2atmpS3285;
  uint64_t _M0L6_2atmpS3286;
  uint64_t _M0L6_2atmpS3283;
  uint64_t _M0L6_2atmpS3284;
  uint64_t _M0L1wS1107;
  uint64_t _M0L6_2atmpS3282;
  #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  _M0L3aLoS1098 = _M0L1aS1099 & 4294967295ull;
  _M0L3aHiS1100 = _M0L1aS1099 >> 32;
  _M0L3bLoS1101 = _M0L1bS1102 & 4294967295ull;
  _M0L3bHiS1103 = _M0L1bS1102 >> 32;
  _M0L1xS1104 = _M0L3aLoS1098 * _M0L3bLoS1101;
  _M0L6_2atmpS3289 = _M0L3aHiS1100 * _M0L3bLoS1101;
  _M0L6_2atmpS3290 = _M0L1xS1104 >> 32;
  _M0L1yS1105 = _M0L6_2atmpS3289 + _M0L6_2atmpS3290;
  _M0L6_2atmpS3287 = _M0L3aLoS1098 * _M0L3bHiS1103;
  _M0L6_2atmpS3288 = _M0L1yS1105 & 4294967295ull;
  _M0L1zS1106 = _M0L6_2atmpS3287 + _M0L6_2atmpS3288;
  _M0L6_2atmpS3285 = _M0L3aHiS1100 * _M0L3bHiS1103;
  _M0L6_2atmpS3286 = _M0L1yS1105 >> 32;
  _M0L6_2atmpS3283 = _M0L6_2atmpS3285 + _M0L6_2atmpS3286;
  _M0L6_2atmpS3284 = _M0L1zS1106 >> 32;
  _M0L1wS1107 = _M0L6_2atmpS3283 + _M0L6_2atmpS3284;
  _M0L6_2atmpS3282 = _M0L1aS1099 * _M0L1bS1102;
  return (struct _M0TPC16random7UInt128){_M0L1wS1107, _M0L6_2atmpS3282};
}

uint64_t _M0MPC16random4Rand4next(
  struct _M0TPC16random6Source _M0L4selfS1097
) {
  struct _M0TPC16random6Source _M0L4_2asS1096;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  _M0L4_2asS1096 = _M0L4selfS1097;
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  return _M0L4_2asS1096.$0->$method_0(_M0L4_2asS1096.$1);
}

struct _M0TPC16random6Source _M0MPC16random4Rand15chacha8_2einner(
  moonbit_bytes_t _M0L4seedS1095
) {
  int32_t _M0L6_2atmpS3280;
  struct _M0TPC36random8internal14random__source7ChaCha8* _M0L6_2atmpS3281;
  #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  _M0L6_2atmpS3280 = Moonbit_array_length(_M0L4seedS1095);
  if (_M0L6_2atmpS3280 != 32) {
    #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_15.data, (moonbit_string_t)moonbit_string_literal_16.data);
  }
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  _M0L6_2atmpS3281
  = _M0MPC36random8internal14random__source7ChaCha83new(_M0L4seedS1095);
  return (struct _M0TPC16random6Source){_M0FP0139moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2fChaCha8_2eas___40moonbitlang_2fcore_2frandom_2eSource_2estatic__method__table__id,
                                          _M0L6_2atmpS3281};
}

uint64_t _M0IPC36random8internal14random__source7ChaCha8PC16random6Source4next(
  struct _M0TPC36random8internal14random__source7ChaCha8* _M0L4selfS1091
) {
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
  while (1) {
    void* _M0L7_2abindS1090;
    moonbit_incref(_M0L4selfS1091);
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
    _M0L7_2abindS1090
    = _M0MPC36random8internal14random__source7ChaCha84next(_M0L4selfS1091);
    switch (Moonbit_object_tag(_M0L7_2abindS1090)) {
      case 1: {
        struct _M0DTPC16option6OptionGmE4Some* _M0L7_2aSomeS1092;
        uint64_t _M0L8_2afieldS3832;
        uint64_t _M0L4_2axS1093;
        moonbit_decref(_M0L4selfS1091);
        _M0L7_2aSomeS1092
        = (struct _M0DTPC16option6OptionGmE4Some*)_M0L7_2abindS1090;
        _M0L8_2afieldS3832 = _M0L7_2aSomeS1092->$0;
        moonbit_decref(_M0L7_2aSomeS1092);
        _M0L4_2axS1093 = _M0L8_2afieldS3832;
        return _M0L4_2axS1093;
        break;
      }
      default: {
        moonbit_decref(_M0L7_2abindS1090);
        break;
      }
    }
    moonbit_incref(_M0L4selfS1091);
    #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\random.mbt"
    _M0MPC36random8internal14random__source7ChaCha86refill(_M0L4selfS1091);
    continue;
    break;
  }
}

int32_t _M0MPC36random8internal14random__source7ChaCha86refill(
  struct _M0TPC36random8internal14random__source7ChaCha8* _M0L4selfS1089
) {
  uint32_t _M0L7counterS3271;
  uint32_t _M0L6_2atmpS3270;
  uint32_t _M0L7counterS3272;
  uint32_t* _M0L8_2afieldS3834;
  uint32_t* _M0L4seedS3275;
  uint32_t* _M0L8_2afieldS3833;
  uint32_t* _M0L6bufferS3276;
  uint32_t _M0L7counterS3277;
  uint32_t _M0L7counterS3279;
  uint32_t _M0L6_2atmpS3278;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _M0L7counterS3271 = _M0L4selfS1089->$4;
  _M0L6_2atmpS3270 = _M0L7counterS3271 + 4u;
  _M0L4selfS1089->$4 = _M0L6_2atmpS3270;
  _M0L7counterS3272 = _M0L4selfS1089->$4;
  if (_M0L7counterS3272 == 16u) {
    uint32_t* _M0L8_2afieldS3836 = _M0L4selfS1089->$0;
    uint32_t* _M0L6bufferS3273 = _M0L8_2afieldS3836;
    uint32_t* _M0L8_2afieldS3835 = _M0L4selfS1089->$1;
    uint32_t* _M0L4seedS3274 = _M0L8_2afieldS3835;
    moonbit_incref(_M0L4seedS3274);
    moonbit_incref(_M0L6bufferS3273);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    _M0MPC15array10FixedArray16blit__to_2einnerGjE(_M0L6bufferS3273, _M0L4seedS3274, 8, 56, 0);
    _M0L4selfS1089->$4 = 0u;
  }
  _M0L8_2afieldS3834 = _M0L4selfS1089->$1;
  _M0L4seedS3275 = _M0L8_2afieldS3834;
  _M0L8_2afieldS3833 = _M0L4selfS1089->$0;
  _M0L6bufferS3276 = _M0L8_2afieldS3833;
  _M0L7counterS3277 = _M0L4selfS1089->$4;
  moonbit_incref(_M0L6bufferS3276);
  moonbit_incref(_M0L4seedS3275);
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _M0FPC36random8internal14random__source13chacha__block(_M0L4seedS3275, _M0L6bufferS3276, _M0L7counterS3277);
  _M0L4selfS1089->$2 = 0u;
  _M0L7counterS3279 = _M0L4selfS1089->$4;
  if (_M0L7counterS3279 == 12u) {
    int32_t _tmp_4542 = 28;
    _M0L6_2atmpS3278 = *(uint32_t*)&_tmp_4542;
  } else {
    int32_t _tmp_4543 = 32;
    _M0L6_2atmpS3278 = *(uint32_t*)&_tmp_4543;
  }
  _M0L4selfS1089->$3 = _M0L6_2atmpS3278;
  moonbit_decref(_M0L4selfS1089);
  return 0;
}

void* _M0MPC36random8internal14random__source7ChaCha84next(
  struct _M0TPC36random8internal14random__source7ChaCha8* _M0L4selfS1085
) {
  uint32_t _M0L1iS1084;
  uint32_t _M0L1nS3258;
  uint32_t _M0L6_2atmpS3259;
  int32_t _M0L6_2atmpS3269;
  int32_t _M0L5indexS1086;
  uint32_t* _M0L8_2afieldS3840;
  uint32_t* _M0L6bufferS3267;
  int32_t _M0L6_2atmpS3268;
  uint32_t _M0L6_2atmpS3839;
  uint32_t _M0L6_2atmpS3266;
  uint64_t _M0L2loS1087;
  uint32_t* _M0L8_2afieldS3838;
  int32_t _M0L6_2acntS4417;
  uint32_t* _M0L6bufferS3263;
  int32_t _M0L6_2atmpS3265;
  int32_t _M0L6_2atmpS3264;
  uint32_t _M0L6_2atmpS3837;
  uint32_t _M0L6_2atmpS3262;
  uint64_t _M0L2hiS1088;
  uint64_t _M0L6_2atmpS3261;
  uint64_t _M0L6_2atmpS3260;
  void* _block_4544;
  #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _M0L1iS1084 = _M0L4selfS1085->$2;
  _M0L1nS3258 = _M0L4selfS1085->$3;
  if (_M0L1iS1084 >= _M0L1nS3258) {
    moonbit_decref(_M0L4selfS1085);
    return (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
  }
  _M0L6_2atmpS3259 = _M0L1iS1084 + 1u;
  _M0L4selfS1085->$2 = _M0L6_2atmpS3259;
  _M0L6_2atmpS3269 = *(int32_t*)&_M0L1iS1084;
  _M0L5indexS1086 = _M0L6_2atmpS3269 & 31;
  _M0L8_2afieldS3840 = _M0L4selfS1085->$0;
  _M0L6bufferS3267 = _M0L8_2afieldS3840;
  _M0L6_2atmpS3268 = _M0L5indexS1086 * 2;
  if (
    _M0L6_2atmpS3268 < 0
    || _M0L6_2atmpS3268 >= Moonbit_array_length(_M0L6bufferS3267)
  ) {
    #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3839 = (uint32_t)_M0L6bufferS3267[_M0L6_2atmpS3268];
  _M0L6_2atmpS3266 = _M0L6_2atmpS3839;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _M0L2loS1087 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3266);
  _M0L8_2afieldS3838 = _M0L4selfS1085->$0;
  _M0L6_2acntS4417 = Moonbit_object_header(_M0L4selfS1085)->rc;
  if (_M0L6_2acntS4417 > 1) {
    int32_t _M0L11_2anew__cntS4419 = _M0L6_2acntS4417 - 1;
    Moonbit_object_header(_M0L4selfS1085)->rc = _M0L11_2anew__cntS4419;
    moonbit_incref(_M0L8_2afieldS3838);
  } else if (_M0L6_2acntS4417 == 1) {
    uint32_t* _M0L8_2afieldS4418 = _M0L4selfS1085->$1;
    moonbit_decref(_M0L8_2afieldS4418);
    #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_free(_M0L4selfS1085);
  }
  _M0L6bufferS3263 = _M0L8_2afieldS3838;
  _M0L6_2atmpS3265 = _M0L5indexS1086 * 2;
  _M0L6_2atmpS3264 = _M0L6_2atmpS3265 + 1;
  if (
    _M0L6_2atmpS3264 < 0
    || _M0L6_2atmpS3264 >= Moonbit_array_length(_M0L6bufferS3263)
  ) {
    #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3837 = (uint32_t)_M0L6bufferS3263[_M0L6_2atmpS3264];
  moonbit_decref(_M0L6bufferS3263);
  _M0L6_2atmpS3262 = _M0L6_2atmpS3837;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _M0L2hiS1088 = _M0MPC14uint4UInt10to__uint64(_M0L6_2atmpS3262);
  _M0L6_2atmpS3261 = _M0L2hiS1088 << 32;
  _M0L6_2atmpS3260 = _M0L6_2atmpS3261 | _M0L2loS1087;
  _block_4544
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGmE4Some));
  Moonbit_object_header(_block_4544)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0DTPC16option6OptionGmE4Some) >> 2, 0, 1);
  ((struct _M0DTPC16option6OptionGmE4Some*)_block_4544)->$0
  = _M0L6_2atmpS3260;
  return _block_4544;
}

struct _M0TPC36random8internal14random__source7ChaCha8* _M0MPC36random8internal14random__source7ChaCha83new(
  moonbit_bytes_t _M0L4seedS1081
) {
  struct _M0R98_40moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2eChaCha8_3a_3anew_2eanon__u3251__l29__* _closure_4545;
  struct _M0TWiEj* _M0L6_2atmpS3250;
  uint32_t* _M0L4seedS1078;
  uint32_t* _M0L6bufferS1082;
  int32_t _tmp_4546;
  uint32_t _M0L7_2abindS1083;
  struct _M0TPC36random8internal14random__source7ChaCha8* _block_4547;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _closure_4545
  = (struct _M0R98_40moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2eChaCha8_3a_3anew_2eanon__u3251__l29__*)moonbit_malloc(sizeof(struct _M0R98_40moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2eChaCha8_3a_3anew_2eanon__u3251__l29__));
  Moonbit_object_header(_closure_4545)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R98_40moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2eChaCha8_3a_3anew_2eanon__u3251__l29__, $0) >> 2, 1, 0);
  _closure_4545->code
  = &_M0MPC36random8internal14random__source7ChaCha83newC3251l29;
  _closure_4545->$0 = _M0L4seedS1081;
  _M0L6_2atmpS3250 = (struct _M0TWiEj*)_closure_4545;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _M0L4seedS1078 = _M0MPC15array10FixedArray5makeiGjE(8, _M0L6_2atmpS3250);
  _M0L6bufferS1082 = (uint32_t*)moonbit_make_int32_array(64, 0u);
  moonbit_incref(_M0L6bufferS1082);
  moonbit_incref(_M0L4seedS1078);
  #line 36 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _M0FPC36random8internal14random__source13chacha__block(_M0L4seedS1078, _M0L6bufferS1082, 0u);
  _tmp_4546 = 32;
  _M0L7_2abindS1083 = *(uint32_t*)&_tmp_4546;
  _block_4547
  = (struct _M0TPC36random8internal14random__source7ChaCha8*)moonbit_malloc(sizeof(struct _M0TPC36random8internal14random__source7ChaCha8));
  Moonbit_object_header(_block_4547)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC36random8internal14random__source7ChaCha8, $0) >> 2, 2, 0);
  _block_4547->$0 = _M0L6bufferS1082;
  _block_4547->$1 = _M0L4seedS1078;
  _block_4547->$2 = 0u;
  _block_4547->$3 = _M0L7_2abindS1083;
  _block_4547->$4 = 0u;
  return _block_4547;
}

uint32_t _M0MPC36random8internal14random__source7ChaCha83newC3251l29(
  struct _M0TWiEj* _M0L6_2aenvS3252,
  int32_t _M0L1iS1079
) {
  struct _M0R98_40moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2eChaCha8_3a_3anew_2eanon__u3251__l29__* _M0L14_2acasted__envS3253;
  moonbit_bytes_t _M0L8_2afieldS3841;
  int32_t _M0L6_2acntS4420;
  moonbit_bytes_t _M0L4seedS1081;
  int32_t _M0L6_2atmpS3257;
  struct _M0TPC15bytes9BytesView _M0L7_2abindS1080;
  int32_t _M0L3endS3255;
  int32_t _M0L5startS3256;
  int32_t _M0L6_2atmpS3254;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _M0L14_2acasted__envS3253
  = (struct _M0R98_40moonbitlang_2fcore_2frandom_2finternal_2frandom__source_2eChaCha8_3a_3anew_2eanon__u3251__l29__*)_M0L6_2aenvS3252;
  _M0L8_2afieldS3841 = _M0L14_2acasted__envS3253->$0;
  _M0L6_2acntS4420 = Moonbit_object_header(_M0L14_2acasted__envS3253)->rc;
  if (_M0L6_2acntS4420 > 1) {
    int32_t _M0L11_2anew__cntS4421 = _M0L6_2acntS4420 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS3253)->rc
    = _M0L11_2anew__cntS4421;
    moonbit_incref(_M0L8_2afieldS3841);
  } else if (_M0L6_2acntS4420 == 1) {
    #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_free(_M0L14_2acasted__envS3253);
  }
  _M0L4seedS1081 = _M0L8_2afieldS3841;
  _M0L6_2atmpS3257 = _M0L1iS1079 * 4;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _M0L7_2abindS1080
  = _M0MPC15bytes5Bytes12view_2einner(_M0L4seedS1081, _M0L6_2atmpS3257, 4294967296ll);
  _M0L3endS3255 = _M0L7_2abindS1080.$2;
  _M0L5startS3256 = _M0L7_2abindS1080.$1;
  _M0L6_2atmpS3254 = _M0L3endS3255 - _M0L5startS3256;
  if (_M0L6_2atmpS3254 >= 4) {
    #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    return _M0MPC15bytes9BytesView25unsafe__extract__uint__le(_M0L7_2abindS1080, 0, 32);
  } else {
    moonbit_decref(_M0L7_2abindS1080.$0);
    #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    return _M0FPB5abortGjE((moonbit_string_t)moonbit_string_literal_15.data, (moonbit_string_t)moonbit_string_literal_17.data);
  }
}

int32_t _M0FPC36random8internal14random__source13chacha__block(
  uint32_t* _M0L4seedS1007,
  uint32_t* _M0L3bufS1008,
  uint32_t _M0L7counterS1009
) {
  int32_t _M0L1iS1010;
  #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  moonbit_incref(_M0L3bufS1008);
  #line 117 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _M0FPC36random8internal14random__source5setup(_M0L4seedS1007, _M0L3bufS1008, _M0L7counterS1009);
  _M0L1iS1010 = 0;
  while (1) {
    if (_M0L1iS1010 < 4) {
      uint32_t _M0Lm2b0S1011;
      int32_t _M0L6_2atmpS3248;
      uint32_t _M0Lm2b1S1012;
      int32_t _M0L6_2atmpS3247;
      uint32_t _M0Lm2b2S1013;
      int32_t _M0L6_2atmpS3246;
      uint32_t _M0Lm2b3S1014;
      int32_t _M0L6_2atmpS3245;
      uint32_t _M0Lm2b4S1015;
      int32_t _M0L6_2atmpS3244;
      uint32_t _M0Lm2b5S1016;
      int32_t _M0L6_2atmpS3243;
      uint32_t _M0Lm2b6S1017;
      int32_t _M0L6_2atmpS3242;
      uint32_t _M0Lm2b7S1018;
      int32_t _M0L6_2atmpS3241;
      uint32_t _M0Lm2b8S1019;
      int32_t _M0L6_2atmpS3240;
      uint32_t _M0Lm2b9S1020;
      int32_t _M0L6_2atmpS3239;
      uint32_t _M0Lm3b10S1021;
      int32_t _M0L6_2atmpS3238;
      uint32_t _M0Lm3b11S1022;
      int32_t _M0L6_2atmpS3237;
      uint32_t _M0Lm3b12S1023;
      int32_t _M0L6_2atmpS3236;
      uint32_t _M0Lm3b13S1024;
      int32_t _M0L6_2atmpS3235;
      uint32_t _M0Lm3b14S1025;
      int32_t _M0L6_2atmpS3234;
      uint32_t _M0Lm3b15S1026;
      int32_t _M0L2__S1027;
      uint32_t _M0L6_2atmpS3195;
      int32_t _M0L6_2atmpS3196;
      uint32_t _M0L6_2atmpS3197;
      int32_t _M0L6_2atmpS3198;
      uint32_t _M0L6_2atmpS3199;
      int32_t _M0L6_2atmpS3200;
      uint32_t _M0L6_2atmpS3201;
      int32_t _M0L11_2aindex__2S1069;
      uint32_t _M0L6_2atmpS3203;
      uint32_t _M0L6_2atmpS3204;
      uint32_t _M0L6_2atmpS3202;
      int32_t _M0L11_2aindex__4S1070;
      uint32_t _M0L6_2atmpS3206;
      uint32_t _M0L6_2atmpS3207;
      uint32_t _M0L6_2atmpS3205;
      int32_t _M0L11_2aindex__6S1071;
      uint32_t _M0L6_2atmpS3209;
      uint32_t _M0L6_2atmpS3210;
      uint32_t _M0L6_2atmpS3208;
      int32_t _M0L11_2aindex__8S1072;
      uint32_t _M0L6_2atmpS3212;
      uint32_t _M0L6_2atmpS3213;
      uint32_t _M0L6_2atmpS3211;
      int32_t _M0L12_2aindex__10S1073;
      uint32_t _M0L6_2atmpS3215;
      uint32_t _M0L6_2atmpS3216;
      uint32_t _M0L6_2atmpS3214;
      int32_t _M0L12_2aindex__12S1074;
      uint32_t _M0L6_2atmpS3218;
      uint32_t _M0L6_2atmpS3219;
      uint32_t _M0L6_2atmpS3217;
      int32_t _M0L12_2aindex__14S1075;
      uint32_t _M0L6_2atmpS3221;
      uint32_t _M0L6_2atmpS3222;
      uint32_t _M0L6_2atmpS3220;
      int32_t _M0L12_2aindex__16S1076;
      uint32_t _M0L6_2atmpS3224;
      uint32_t _M0L6_2atmpS3225;
      uint32_t _M0L6_2atmpS3223;
      int32_t _M0L6_2atmpS3226;
      uint32_t _M0L6_2atmpS3227;
      int32_t _M0L6_2atmpS3228;
      uint32_t _M0L6_2atmpS3229;
      int32_t _M0L6_2atmpS3230;
      uint32_t _M0L6_2atmpS3231;
      int32_t _M0L6_2atmpS3232;
      uint32_t _M0L6_2atmpS3233;
      int32_t _M0L6_2atmpS3249;
      if (
        _M0L1iS1010 < 0 || _M0L1iS1010 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm2b0S1011 = (uint32_t)_M0L3bufS1008[_M0L1iS1010];
      _M0L6_2atmpS3248 = 4 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3248 < 0
        || _M0L6_2atmpS3248 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm2b1S1012 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3248];
      _M0L6_2atmpS3247 = 8 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3247 < 0
        || _M0L6_2atmpS3247 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm2b2S1013 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3247];
      _M0L6_2atmpS3246 = 12 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3246 < 0
        || _M0L6_2atmpS3246 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 122 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm2b3S1014 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3246];
      _M0L6_2atmpS3245 = 16 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3245 < 0
        || _M0L6_2atmpS3245 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm2b4S1015 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3245];
      _M0L6_2atmpS3244 = 20 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3244 < 0
        || _M0L6_2atmpS3244 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm2b5S1016 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3244];
      _M0L6_2atmpS3243 = 24 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3243 < 0
        || _M0L6_2atmpS3243 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm2b6S1017 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3243];
      _M0L6_2atmpS3242 = 28 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3242 < 0
        || _M0L6_2atmpS3242 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 126 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm2b7S1018 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3242];
      _M0L6_2atmpS3241 = 32 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3241 < 0
        || _M0L6_2atmpS3241 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 127 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm2b8S1019 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3241];
      _M0L6_2atmpS3240 = 36 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3240 < 0
        || _M0L6_2atmpS3240 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm2b9S1020 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3240];
      _M0L6_2atmpS3239 = 40 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3239 < 0
        || _M0L6_2atmpS3239 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm3b10S1021 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3239];
      _M0L6_2atmpS3238 = 44 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3238 < 0
        || _M0L6_2atmpS3238 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm3b11S1022 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3238];
      _M0L6_2atmpS3237 = 48 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3237 < 0
        || _M0L6_2atmpS3237 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 131 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm3b12S1023 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3237];
      _M0L6_2atmpS3236 = 52 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3236 < 0
        || _M0L6_2atmpS3236 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 132 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm3b13S1024 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3236];
      _M0L6_2atmpS3235 = 56 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3235 < 0
        || _M0L6_2atmpS3235 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm3b14S1025 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3235];
      _M0L6_2atmpS3234 = 60 + _M0L1iS1010;
      if (
        _M0L6_2atmpS3234 < 0
        || _M0L6_2atmpS3234 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0Lm3b15S1026 = (uint32_t)_M0L3bufS1008[_M0L6_2atmpS3234];
      _M0L2__S1027 = 0;
      while (1) {
        if (_M0L2__S1027 < 4) {
          uint32_t _M0L6_2atmpS3190 = _M0Lm2b0S1011;
          uint32_t _M0L6_2atmpS3191 = _M0Lm2b4S1015;
          uint32_t _M0L6_2atmpS3192 = _M0Lm2b8S1019;
          uint32_t _M0L6_2atmpS3193 = _M0Lm3b12S1023;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L6_2atmpS3189 =
            (struct _M0TPC36random8internal14random__source9Quadruple){_M0L6_2atmpS3190,
                                                                    _M0L6_2atmpS3191,
                                                                    _M0L6_2atmpS3192,
                                                                    _M0L6_2atmpS3193};
          struct _M0TPC36random8internal14random__source9Quadruple _M0L7_2abindS1028;
          uint32_t _M0L9_2atb1__0S1029;
          uint32_t _M0L9_2atb1__1S1030;
          uint32_t _M0L9_2atb1__2S1031;
          uint32_t _M0L9_2atb1__3S1032;
          uint32_t _M0L6_2atmpS3185;
          uint32_t _M0L6_2atmpS3186;
          uint32_t _M0L6_2atmpS3187;
          uint32_t _M0L6_2atmpS3188;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L6_2atmpS3184;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L7_2abindS1033;
          uint32_t _M0L9_2atb2__0S1034;
          uint32_t _M0L9_2atb2__1S1035;
          uint32_t _M0L9_2atb2__2S1036;
          uint32_t _M0L9_2atb2__3S1037;
          uint32_t _M0L6_2atmpS3180;
          uint32_t _M0L6_2atmpS3181;
          uint32_t _M0L6_2atmpS3182;
          uint32_t _M0L6_2atmpS3183;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L6_2atmpS3179;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L7_2abindS1038;
          uint32_t _M0L9_2atb3__0S1039;
          uint32_t _M0L9_2atb3__1S1040;
          uint32_t _M0L9_2atb3__2S1041;
          uint32_t _M0L9_2atb3__3S1042;
          uint32_t _M0L6_2atmpS3175;
          uint32_t _M0L6_2atmpS3176;
          uint32_t _M0L6_2atmpS3177;
          uint32_t _M0L6_2atmpS3178;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L6_2atmpS3174;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L7_2abindS1043;
          uint32_t _M0L9_2atb4__0S1044;
          uint32_t _M0L9_2atb4__1S1045;
          uint32_t _M0L9_2atb4__2S1046;
          uint32_t _M0L9_2atb4__3S1047;
          uint32_t _M0L6_2atmpS3170;
          uint32_t _M0L6_2atmpS3171;
          uint32_t _M0L6_2atmpS3172;
          uint32_t _M0L6_2atmpS3173;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L6_2atmpS3169;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L7_2abindS1048;
          uint32_t _M0L9_2atb5__0S1049;
          uint32_t _M0L9_2atb5__1S1050;
          uint32_t _M0L9_2atb5__2S1051;
          uint32_t _M0L9_2atb5__3S1052;
          uint32_t _M0L6_2atmpS3165;
          uint32_t _M0L6_2atmpS3166;
          uint32_t _M0L6_2atmpS3167;
          uint32_t _M0L6_2atmpS3168;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L6_2atmpS3164;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L7_2abindS1053;
          uint32_t _M0L9_2atb6__0S1054;
          uint32_t _M0L9_2atb6__1S1055;
          uint32_t _M0L9_2atb6__2S1056;
          uint32_t _M0L9_2atb6__3S1057;
          uint32_t _M0L6_2atmpS3160;
          uint32_t _M0L6_2atmpS3161;
          uint32_t _M0L6_2atmpS3162;
          uint32_t _M0L6_2atmpS3163;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L6_2atmpS3159;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L7_2abindS1058;
          uint32_t _M0L9_2atb7__0S1059;
          uint32_t _M0L9_2atb7__1S1060;
          uint32_t _M0L9_2atb7__2S1061;
          uint32_t _M0L9_2atb7__3S1062;
          uint32_t _M0L6_2atmpS3155;
          uint32_t _M0L6_2atmpS3156;
          uint32_t _M0L6_2atmpS3157;
          uint32_t _M0L6_2atmpS3158;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L6_2atmpS3154;
          struct _M0TPC36random8internal14random__source9Quadruple _M0L7_2abindS1063;
          uint32_t _M0L9_2atb8__0S1064;
          uint32_t _M0L9_2atb8__1S1065;
          uint32_t _M0L9_2atb8__2S1066;
          uint32_t _M0L9_2atb8__3S1067;
          int32_t _M0L6_2atmpS3194;
          #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
          _M0L7_2abindS1028
          = _M0FPC36random8internal14random__source13chacha__blockN2qrS30(_M0L6_2atmpS3189);
          _M0L9_2atb1__0S1029 = _M0L7_2abindS1028.$0;
          _M0L9_2atb1__1S1030 = _M0L7_2abindS1028.$1;
          _M0L9_2atb1__2S1031 = _M0L7_2abindS1028.$2;
          _M0L9_2atb1__3S1032 = _M0L7_2abindS1028.$3;
          _M0Lm2b0S1011 = _M0L9_2atb1__0S1029;
          _M0Lm2b4S1015 = _M0L9_2atb1__1S1030;
          _M0Lm2b8S1019 = _M0L9_2atb1__2S1031;
          _M0Lm3b12S1023 = _M0L9_2atb1__3S1032;
          _M0L6_2atmpS3185 = _M0Lm2b1S1012;
          _M0L6_2atmpS3186 = _M0Lm2b5S1016;
          _M0L6_2atmpS3187 = _M0Lm2b9S1020;
          _M0L6_2atmpS3188 = _M0Lm3b13S1024;
          _M0L6_2atmpS3184
          = (struct _M0TPC36random8internal14random__source9Quadruple){
            _M0L6_2atmpS3185,
              _M0L6_2atmpS3186,
              _M0L6_2atmpS3187,
              _M0L6_2atmpS3188
          };
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
          _M0L7_2abindS1033
          = _M0FPC36random8internal14random__source13chacha__blockN2qrS30(_M0L6_2atmpS3184);
          _M0L9_2atb2__0S1034 = _M0L7_2abindS1033.$0;
          _M0L9_2atb2__1S1035 = _M0L7_2abindS1033.$1;
          _M0L9_2atb2__2S1036 = _M0L7_2abindS1033.$2;
          _M0L9_2atb2__3S1037 = _M0L7_2abindS1033.$3;
          _M0Lm2b1S1012 = _M0L9_2atb2__0S1034;
          _M0Lm2b5S1016 = _M0L9_2atb2__1S1035;
          _M0Lm2b9S1020 = _M0L9_2atb2__2S1036;
          _M0Lm3b13S1024 = _M0L9_2atb2__3S1037;
          _M0L6_2atmpS3180 = _M0Lm2b2S1013;
          _M0L6_2atmpS3181 = _M0Lm2b6S1017;
          _M0L6_2atmpS3182 = _M0Lm3b10S1021;
          _M0L6_2atmpS3183 = _M0Lm3b14S1025;
          _M0L6_2atmpS3179
          = (struct _M0TPC36random8internal14random__source9Quadruple){
            _M0L6_2atmpS3180,
              _M0L6_2atmpS3181,
              _M0L6_2atmpS3182,
              _M0L6_2atmpS3183
          };
          #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
          _M0L7_2abindS1038
          = _M0FPC36random8internal14random__source13chacha__blockN2qrS30(_M0L6_2atmpS3179);
          _M0L9_2atb3__0S1039 = _M0L7_2abindS1038.$0;
          _M0L9_2atb3__1S1040 = _M0L7_2abindS1038.$1;
          _M0L9_2atb3__2S1041 = _M0L7_2abindS1038.$2;
          _M0L9_2atb3__3S1042 = _M0L7_2abindS1038.$3;
          _M0Lm2b2S1013 = _M0L9_2atb3__0S1039;
          _M0Lm2b6S1017 = _M0L9_2atb3__1S1040;
          _M0Lm3b10S1021 = _M0L9_2atb3__2S1041;
          _M0Lm3b14S1025 = _M0L9_2atb3__3S1042;
          _M0L6_2atmpS3175 = _M0Lm2b3S1014;
          _M0L6_2atmpS3176 = _M0Lm2b7S1018;
          _M0L6_2atmpS3177 = _M0Lm3b11S1022;
          _M0L6_2atmpS3178 = _M0Lm3b15S1026;
          _M0L6_2atmpS3174
          = (struct _M0TPC36random8internal14random__source9Quadruple){
            _M0L6_2atmpS3175,
              _M0L6_2atmpS3176,
              _M0L6_2atmpS3177,
              _M0L6_2atmpS3178
          };
          #line 153 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
          _M0L7_2abindS1043
          = _M0FPC36random8internal14random__source13chacha__blockN2qrS30(_M0L6_2atmpS3174);
          _M0L9_2atb4__0S1044 = _M0L7_2abindS1043.$0;
          _M0L9_2atb4__1S1045 = _M0L7_2abindS1043.$1;
          _M0L9_2atb4__2S1046 = _M0L7_2abindS1043.$2;
          _M0L9_2atb4__3S1047 = _M0L7_2abindS1043.$3;
          _M0Lm2b3S1014 = _M0L9_2atb4__0S1044;
          _M0Lm2b7S1018 = _M0L9_2atb4__1S1045;
          _M0Lm3b11S1022 = _M0L9_2atb4__2S1046;
          _M0Lm3b15S1026 = _M0L9_2atb4__3S1047;
          _M0L6_2atmpS3170 = _M0Lm2b0S1011;
          _M0L6_2atmpS3171 = _M0Lm2b5S1016;
          _M0L6_2atmpS3172 = _M0Lm3b10S1021;
          _M0L6_2atmpS3173 = _M0Lm3b15S1026;
          _M0L6_2atmpS3169
          = (struct _M0TPC36random8internal14random__source9Quadruple){
            _M0L6_2atmpS3170,
              _M0L6_2atmpS3171,
              _M0L6_2atmpS3172,
              _M0L6_2atmpS3173
          };
          #line 160 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
          _M0L7_2abindS1048
          = _M0FPC36random8internal14random__source13chacha__blockN2qrS30(_M0L6_2atmpS3169);
          _M0L9_2atb5__0S1049 = _M0L7_2abindS1048.$0;
          _M0L9_2atb5__1S1050 = _M0L7_2abindS1048.$1;
          _M0L9_2atb5__2S1051 = _M0L7_2abindS1048.$2;
          _M0L9_2atb5__3S1052 = _M0L7_2abindS1048.$3;
          _M0Lm2b0S1011 = _M0L9_2atb5__0S1049;
          _M0Lm2b5S1016 = _M0L9_2atb5__1S1050;
          _M0Lm3b10S1021 = _M0L9_2atb5__2S1051;
          _M0Lm3b15S1026 = _M0L9_2atb5__3S1052;
          _M0L6_2atmpS3165 = _M0Lm2b1S1012;
          _M0L6_2atmpS3166 = _M0Lm2b6S1017;
          _M0L6_2atmpS3167 = _M0Lm3b11S1022;
          _M0L6_2atmpS3168 = _M0Lm3b12S1023;
          _M0L6_2atmpS3164
          = (struct _M0TPC36random8internal14random__source9Quadruple){
            _M0L6_2atmpS3165,
              _M0L6_2atmpS3166,
              _M0L6_2atmpS3167,
              _M0L6_2atmpS3168
          };
          #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
          _M0L7_2abindS1053
          = _M0FPC36random8internal14random__source13chacha__blockN2qrS30(_M0L6_2atmpS3164);
          _M0L9_2atb6__0S1054 = _M0L7_2abindS1053.$0;
          _M0L9_2atb6__1S1055 = _M0L7_2abindS1053.$1;
          _M0L9_2atb6__2S1056 = _M0L7_2abindS1053.$2;
          _M0L9_2atb6__3S1057 = _M0L7_2abindS1053.$3;
          _M0Lm2b1S1012 = _M0L9_2atb6__0S1054;
          _M0Lm2b6S1017 = _M0L9_2atb6__1S1055;
          _M0Lm3b11S1022 = _M0L9_2atb6__2S1056;
          _M0Lm3b12S1023 = _M0L9_2atb6__3S1057;
          _M0L6_2atmpS3160 = _M0Lm2b2S1013;
          _M0L6_2atmpS3161 = _M0Lm2b7S1018;
          _M0L6_2atmpS3162 = _M0Lm2b8S1019;
          _M0L6_2atmpS3163 = _M0Lm3b13S1024;
          _M0L6_2atmpS3159
          = (struct _M0TPC36random8internal14random__source9Quadruple){
            _M0L6_2atmpS3160,
              _M0L6_2atmpS3161,
              _M0L6_2atmpS3162,
              _M0L6_2atmpS3163
          };
          #line 174 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
          _M0L7_2abindS1058
          = _M0FPC36random8internal14random__source13chacha__blockN2qrS30(_M0L6_2atmpS3159);
          _M0L9_2atb7__0S1059 = _M0L7_2abindS1058.$0;
          _M0L9_2atb7__1S1060 = _M0L7_2abindS1058.$1;
          _M0L9_2atb7__2S1061 = _M0L7_2abindS1058.$2;
          _M0L9_2atb7__3S1062 = _M0L7_2abindS1058.$3;
          _M0Lm2b2S1013 = _M0L9_2atb7__0S1059;
          _M0Lm2b7S1018 = _M0L9_2atb7__1S1060;
          _M0Lm2b8S1019 = _M0L9_2atb7__2S1061;
          _M0Lm3b13S1024 = _M0L9_2atb7__3S1062;
          _M0L6_2atmpS3155 = _M0Lm2b3S1014;
          _M0L6_2atmpS3156 = _M0Lm2b4S1015;
          _M0L6_2atmpS3157 = _M0Lm2b9S1020;
          _M0L6_2atmpS3158 = _M0Lm3b14S1025;
          _M0L6_2atmpS3154
          = (struct _M0TPC36random8internal14random__source9Quadruple){
            _M0L6_2atmpS3155,
              _M0L6_2atmpS3156,
              _M0L6_2atmpS3157,
              _M0L6_2atmpS3158
          };
          #line 179 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
          _M0L7_2abindS1063
          = _M0FPC36random8internal14random__source13chacha__blockN2qrS30(_M0L6_2atmpS3154);
          _M0L9_2atb8__0S1064 = _M0L7_2abindS1063.$0;
          _M0L9_2atb8__1S1065 = _M0L7_2abindS1063.$1;
          _M0L9_2atb8__2S1066 = _M0L7_2abindS1063.$2;
          _M0L9_2atb8__3S1067 = _M0L7_2abindS1063.$3;
          _M0Lm2b3S1014 = _M0L9_2atb8__0S1064;
          _M0Lm2b4S1015 = _M0L9_2atb8__1S1065;
          _M0Lm2b9S1020 = _M0L9_2atb8__2S1066;
          _M0Lm3b14S1025 = _M0L9_2atb8__3S1067;
          _M0L6_2atmpS3194 = _M0L2__S1027 + 1;
          _M0L2__S1027 = _M0L6_2atmpS3194;
          continue;
        }
        break;
      }
      _M0L6_2atmpS3195 = _M0Lm2b0S1011;
      if (
        _M0L1iS1010 < 0 || _M0L1iS1010 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L1iS1010] = _M0L6_2atmpS3195;
      _M0L6_2atmpS3196 = 4 + _M0L1iS1010;
      _M0L6_2atmpS3197 = _M0Lm2b1S1012;
      if (
        _M0L6_2atmpS3196 < 0
        || _M0L6_2atmpS3196 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L6_2atmpS3196] = _M0L6_2atmpS3197;
      _M0L6_2atmpS3198 = 8 + _M0L1iS1010;
      _M0L6_2atmpS3199 = _M0Lm2b2S1013;
      if (
        _M0L6_2atmpS3198 < 0
        || _M0L6_2atmpS3198 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L6_2atmpS3198] = _M0L6_2atmpS3199;
      _M0L6_2atmpS3200 = 12 + _M0L1iS1010;
      _M0L6_2atmpS3201 = _M0Lm2b3S1014;
      if (
        _M0L6_2atmpS3200 < 0
        || _M0L6_2atmpS3200 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L6_2atmpS3200] = _M0L6_2atmpS3201;
      _M0L11_2aindex__2S1069 = 16 + _M0L1iS1010;
      if (
        _M0L11_2aindex__2S1069 < 0
        || _M0L11_2aindex__2S1069 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 189 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3203 = (uint32_t)_M0L3bufS1008[_M0L11_2aindex__2S1069];
      _M0L6_2atmpS3204 = _M0Lm2b4S1015;
      _M0L6_2atmpS3202 = _M0L6_2atmpS3203 + _M0L6_2atmpS3204;
      if (
        _M0L11_2aindex__2S1069 < 0
        || _M0L11_2aindex__2S1069 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 189 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L11_2aindex__2S1069] = _M0L6_2atmpS3202;
      _M0L11_2aindex__4S1070 = 20 + _M0L1iS1010;
      if (
        _M0L11_2aindex__4S1070 < 0
        || _M0L11_2aindex__4S1070 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3206 = (uint32_t)_M0L3bufS1008[_M0L11_2aindex__4S1070];
      _M0L6_2atmpS3207 = _M0Lm2b5S1016;
      _M0L6_2atmpS3205 = _M0L6_2atmpS3206 + _M0L6_2atmpS3207;
      if (
        _M0L11_2aindex__4S1070 < 0
        || _M0L11_2aindex__4S1070 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L11_2aindex__4S1070] = _M0L6_2atmpS3205;
      _M0L11_2aindex__6S1071 = 24 + _M0L1iS1010;
      if (
        _M0L11_2aindex__6S1071 < 0
        || _M0L11_2aindex__6S1071 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3209 = (uint32_t)_M0L3bufS1008[_M0L11_2aindex__6S1071];
      _M0L6_2atmpS3210 = _M0Lm2b6S1017;
      _M0L6_2atmpS3208 = _M0L6_2atmpS3209 + _M0L6_2atmpS3210;
      if (
        _M0L11_2aindex__6S1071 < 0
        || _M0L11_2aindex__6S1071 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L11_2aindex__6S1071] = _M0L6_2atmpS3208;
      _M0L11_2aindex__8S1072 = 28 + _M0L1iS1010;
      if (
        _M0L11_2aindex__8S1072 < 0
        || _M0L11_2aindex__8S1072 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 192 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3212 = (uint32_t)_M0L3bufS1008[_M0L11_2aindex__8S1072];
      _M0L6_2atmpS3213 = _M0Lm2b7S1018;
      _M0L6_2atmpS3211 = _M0L6_2atmpS3212 + _M0L6_2atmpS3213;
      if (
        _M0L11_2aindex__8S1072 < 0
        || _M0L11_2aindex__8S1072 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 192 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L11_2aindex__8S1072] = _M0L6_2atmpS3211;
      _M0L12_2aindex__10S1073 = 32 + _M0L1iS1010;
      if (
        _M0L12_2aindex__10S1073 < 0
        || _M0L12_2aindex__10S1073 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3215 = (uint32_t)_M0L3bufS1008[_M0L12_2aindex__10S1073];
      _M0L6_2atmpS3216 = _M0Lm2b8S1019;
      _M0L6_2atmpS3214 = _M0L6_2atmpS3215 + _M0L6_2atmpS3216;
      if (
        _M0L12_2aindex__10S1073 < 0
        || _M0L12_2aindex__10S1073 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L12_2aindex__10S1073] = _M0L6_2atmpS3214;
      _M0L12_2aindex__12S1074 = 36 + _M0L1iS1010;
      if (
        _M0L12_2aindex__12S1074 < 0
        || _M0L12_2aindex__12S1074 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 194 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3218 = (uint32_t)_M0L3bufS1008[_M0L12_2aindex__12S1074];
      _M0L6_2atmpS3219 = _M0Lm2b9S1020;
      _M0L6_2atmpS3217 = _M0L6_2atmpS3218 + _M0L6_2atmpS3219;
      if (
        _M0L12_2aindex__12S1074 < 0
        || _M0L12_2aindex__12S1074 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 194 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L12_2aindex__12S1074] = _M0L6_2atmpS3217;
      _M0L12_2aindex__14S1075 = 40 + _M0L1iS1010;
      if (
        _M0L12_2aindex__14S1075 < 0
        || _M0L12_2aindex__14S1075 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3221 = (uint32_t)_M0L3bufS1008[_M0L12_2aindex__14S1075];
      _M0L6_2atmpS3222 = _M0Lm3b10S1021;
      _M0L6_2atmpS3220 = _M0L6_2atmpS3221 + _M0L6_2atmpS3222;
      if (
        _M0L12_2aindex__14S1075 < 0
        || _M0L12_2aindex__14S1075 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L12_2aindex__14S1075] = _M0L6_2atmpS3220;
      _M0L12_2aindex__16S1076 = 44 + _M0L1iS1010;
      if (
        _M0L12_2aindex__16S1076 < 0
        || _M0L12_2aindex__16S1076 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3224 = (uint32_t)_M0L3bufS1008[_M0L12_2aindex__16S1076];
      _M0L6_2atmpS3225 = _M0Lm3b11S1022;
      _M0L6_2atmpS3223 = _M0L6_2atmpS3224 + _M0L6_2atmpS3225;
      if (
        _M0L12_2aindex__16S1076 < 0
        || _M0L12_2aindex__16S1076 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L12_2aindex__16S1076] = _M0L6_2atmpS3223;
      _M0L6_2atmpS3226 = 48 + _M0L1iS1010;
      _M0L6_2atmpS3227 = _M0Lm3b12S1023;
      if (
        _M0L6_2atmpS3226 < 0
        || _M0L6_2atmpS3226 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L6_2atmpS3226] = _M0L6_2atmpS3227;
      _M0L6_2atmpS3228 = 52 + _M0L1iS1010;
      _M0L6_2atmpS3229 = _M0Lm3b13S1024;
      if (
        _M0L6_2atmpS3228 < 0
        || _M0L6_2atmpS3228 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L6_2atmpS3228] = _M0L6_2atmpS3229;
      _M0L6_2atmpS3230 = 56 + _M0L1iS1010;
      _M0L6_2atmpS3231 = _M0Lm3b14S1025;
      if (
        _M0L6_2atmpS3230 < 0
        || _M0L6_2atmpS3230 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L6_2atmpS3230] = _M0L6_2atmpS3231;
      _M0L6_2atmpS3232 = 60 + _M0L1iS1010;
      _M0L6_2atmpS3233 = _M0Lm3b15S1026;
      if (
        _M0L6_2atmpS3232 < 0
        || _M0L6_2atmpS3232 >= Moonbit_array_length(_M0L3bufS1008)
      ) {
        #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
        moonbit_panic();
      }
      _M0L3bufS1008[_M0L6_2atmpS3232] = _M0L6_2atmpS3233;
      _M0L6_2atmpS3249 = _M0L1iS1010 + 1;
      _M0L1iS1010 = _M0L6_2atmpS3249;
      continue;
    } else {
      moonbit_decref(_M0L3bufS1008);
    }
    break;
  }
  return 0;
}

struct _M0TPC36random8internal14random__source9Quadruple _M0FPC36random8internal14random__source13chacha__blockN2qrS30(
  struct _M0TPC36random8internal14random__source9Quadruple _M0L1tS991
) {
  uint32_t _M0L4_2aaS990;
  uint32_t _M0L4_2abS992;
  uint32_t _M0L4_2acS993;
  uint32_t _M0L4_2adS994;
  uint32_t _M0L1aS995;
  uint32_t _M0L1dS996;
  uint32_t _M0L6_2atmpS3152;
  uint32_t _M0L6_2atmpS3153;
  uint32_t _M0L1dS997;
  uint32_t _M0L1cS998;
  uint32_t _M0L1bS999;
  uint32_t _M0L6_2atmpS3150;
  uint32_t _M0L6_2atmpS3151;
  uint32_t _M0L1bS1000;
  uint32_t _M0L1aS1001;
  uint32_t _M0L1dS1002;
  uint32_t _M0L6_2atmpS3148;
  uint32_t _M0L6_2atmpS3149;
  uint32_t _M0L1dS1003;
  uint32_t _M0L1cS1004;
  uint32_t _M0L1bS1005;
  uint32_t _M0L6_2atmpS3146;
  uint32_t _M0L6_2atmpS3147;
  uint32_t _M0L1bS1006;
  #line 100 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  _M0L4_2aaS990 = _M0L1tS991.$0;
  _M0L4_2abS992 = _M0L1tS991.$1;
  _M0L4_2acS993 = _M0L1tS991.$2;
  _M0L4_2adS994 = _M0L1tS991.$3;
  _M0L1aS995 = _M0L4_2aaS990 + _M0L4_2abS992;
  _M0L1dS996 = _M0L4_2adS994 ^ _M0L1aS995;
  _M0L6_2atmpS3152 = _M0L1dS996 << 16;
  _M0L6_2atmpS3153 = _M0L1dS996 >> 16;
  _M0L1dS997 = _M0L6_2atmpS3152 | _M0L6_2atmpS3153;
  _M0L1cS998 = _M0L4_2acS993 + _M0L1dS997;
  _M0L1bS999 = _M0L4_2abS992 ^ _M0L1cS998;
  _M0L6_2atmpS3150 = _M0L1bS999 << 12;
  _M0L6_2atmpS3151 = _M0L1bS999 >> 20;
  _M0L1bS1000 = _M0L6_2atmpS3150 | _M0L6_2atmpS3151;
  _M0L1aS1001 = _M0L1aS995 + _M0L1bS1000;
  _M0L1dS1002 = _M0L1dS997 ^ _M0L1aS1001;
  _M0L6_2atmpS3148 = _M0L1dS1002 << 8;
  _M0L6_2atmpS3149 = _M0L1dS1002 >> 24;
  _M0L1dS1003 = _M0L6_2atmpS3148 | _M0L6_2atmpS3149;
  _M0L1cS1004 = _M0L1cS998 + _M0L1dS1003;
  _M0L1bS1005 = _M0L1bS1000 ^ _M0L1cS1004;
  _M0L6_2atmpS3146 = _M0L1bS1005 << 7;
  _M0L6_2atmpS3147 = _M0L1bS1005 >> 25;
  _M0L1bS1006 = _M0L6_2atmpS3146 | _M0L6_2atmpS3147;
  return (struct _M0TPC36random8internal14random__source9Quadruple){_M0L1aS1001,
                                                                    _M0L1bS1006,
                                                                    _M0L1cS1004,
                                                                    _M0L1dS1003};
}

int32_t _M0FPC36random8internal14random__source5setup(
  uint32_t* _M0L4seedS988,
  uint32_t* _M0L3b32S987,
  uint32_t _M0L7counterS989
) {
  uint32_t _M0L6_2atmpS3111;
  uint32_t _M0L6_2atmpS3112;
  uint32_t _M0L6_2atmpS3113;
  uint32_t _M0L6_2atmpS3114;
  uint32_t _M0L6_2atmpS3115;
  uint32_t _M0L6_2atmpS3116;
  uint32_t _M0L6_2atmpS3117;
  uint32_t _M0L6_2atmpS3118;
  uint32_t _M0L6_2atmpS3119;
  uint32_t _M0L6_2atmpS3120;
  uint32_t _M0L6_2atmpS3121;
  uint32_t _M0L6_2atmpS3122;
  uint32_t _M0L6_2atmpS3123;
  uint32_t _M0L6_2atmpS3124;
  uint32_t _M0L6_2atmpS3125;
  uint32_t _M0L6_2atmpS3126;
  uint32_t _M0L6_2atmpS3127;
  uint32_t _M0L6_2atmpS3128;
  uint32_t _M0L6_2atmpS3129;
  uint32_t _M0L6_2atmpS3130;
  uint32_t _M0L6_2atmpS3131;
  uint32_t _M0L6_2atmpS3132;
  uint32_t _M0L6_2atmpS3133;
  uint32_t _M0L6_2atmpS3134;
  uint32_t _M0L6_2atmpS3135;
  uint32_t _M0L6_2atmpS3136;
  uint32_t _M0L6_2atmpS3137;
  uint32_t _M0L6_2atmpS3138;
  uint32_t _M0L6_2atmpS3139;
  uint32_t _M0L6_2atmpS3140;
  uint32_t _M0L6_2atmpS3141;
  uint32_t _M0L6_2atmpS3842;
  uint32_t _M0L6_2atmpS3142;
  uint32_t _M0L6_2atmpS3143;
  uint32_t _M0L6_2atmpS3144;
  uint32_t _M0L6_2atmpS3145;
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
  if (0 < 0 || 0 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 210 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[0] = 1634760805u;
  if (1 < 0 || 1 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 211 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[1] = 1634760805u;
  if (2 < 0 || 2 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 212 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[2] = 1634760805u;
  if (3 < 0 || 3 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 213 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[3] = 1634760805u;
  if (4 < 0 || 4 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 214 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[4] = 857760878u;
  if (5 < 0 || 5 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 215 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[5] = 857760878u;
  if (6 < 0 || 6 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[6] = 857760878u;
  if (7 < 0 || 7 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[7] = 857760878u;
  if (8 < 0 || 8 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 218 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[8] = 2036477234u;
  if (9 < 0 || 9 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[9] = 2036477234u;
  if (10 < 0 || 10 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[10] = 2036477234u;
  if (11 < 0 || 11 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[11] = 2036477234u;
  if (12 < 0 || 12 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 222 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[12] = 1797285236u;
  if (13 < 0 || 13 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 223 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[13] = 1797285236u;
  if (14 < 0 || 14 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[14] = 1797285236u;
  if (15 < 0 || 15 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 225 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[15] = 1797285236u;
  if (0 < 0 || 0 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 226 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3111 = (uint32_t)_M0L4seedS988[0];
  if (16 < 0 || 16 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 226 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[16] = _M0L6_2atmpS3111;
  if (0 < 0 || 0 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 227 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3112 = (uint32_t)_M0L4seedS988[0];
  if (17 < 0 || 17 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 227 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[17] = _M0L6_2atmpS3112;
  if (0 < 0 || 0 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 228 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3113 = (uint32_t)_M0L4seedS988[0];
  if (18 < 0 || 18 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 228 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[18] = _M0L6_2atmpS3113;
  if (0 < 0 || 0 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 229 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3114 = (uint32_t)_M0L4seedS988[0];
  if (19 < 0 || 19 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 229 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[19] = _M0L6_2atmpS3114;
  if (1 < 0 || 1 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 230 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3115 = (uint32_t)_M0L4seedS988[1];
  if (20 < 0 || 20 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 230 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[20] = _M0L6_2atmpS3115;
  if (1 < 0 || 1 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3116 = (uint32_t)_M0L4seedS988[1];
  if (21 < 0 || 21 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[21] = _M0L6_2atmpS3116;
  if (1 < 0 || 1 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3117 = (uint32_t)_M0L4seedS988[1];
  if (22 < 0 || 22 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 232 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[22] = _M0L6_2atmpS3117;
  if (1 < 0 || 1 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3118 = (uint32_t)_M0L4seedS988[1];
  if (23 < 0 || 23 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[23] = _M0L6_2atmpS3118;
  if (2 < 0 || 2 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3119 = (uint32_t)_M0L4seedS988[2];
  if (24 < 0 || 24 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[24] = _M0L6_2atmpS3119;
  if (2 < 0 || 2 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 235 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3120 = (uint32_t)_M0L4seedS988[2];
  if (25 < 0 || 25 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 235 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[25] = _M0L6_2atmpS3120;
  if (2 < 0 || 2 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3121 = (uint32_t)_M0L4seedS988[2];
  if (26 < 0 || 26 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[26] = _M0L6_2atmpS3121;
  if (2 < 0 || 2 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 237 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3122 = (uint32_t)_M0L4seedS988[2];
  if (27 < 0 || 27 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 237 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[27] = _M0L6_2atmpS3122;
  if (3 < 0 || 3 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3123 = (uint32_t)_M0L4seedS988[3];
  if (28 < 0 || 28 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[28] = _M0L6_2atmpS3123;
  if (3 < 0 || 3 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 239 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3124 = (uint32_t)_M0L4seedS988[3];
  if (29 < 0 || 29 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 239 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[29] = _M0L6_2atmpS3124;
  if (3 < 0 || 3 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3125 = (uint32_t)_M0L4seedS988[3];
  if (30 < 0 || 30 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 240 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[30] = _M0L6_2atmpS3125;
  if (3 < 0 || 3 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 241 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3126 = (uint32_t)_M0L4seedS988[3];
  if (31 < 0 || 31 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 241 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[31] = _M0L6_2atmpS3126;
  if (4 < 0 || 4 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3127 = (uint32_t)_M0L4seedS988[4];
  if (32 < 0 || 32 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[32] = _M0L6_2atmpS3127;
  if (4 < 0 || 4 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3128 = (uint32_t)_M0L4seedS988[4];
  if (33 < 0 || 33 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[33] = _M0L6_2atmpS3128;
  if (4 < 0 || 4 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3129 = (uint32_t)_M0L4seedS988[4];
  if (34 < 0 || 34 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[34] = _M0L6_2atmpS3129;
  if (4 < 0 || 4 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3130 = (uint32_t)_M0L4seedS988[4];
  if (35 < 0 || 35 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 245 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[35] = _M0L6_2atmpS3130;
  if (5 < 0 || 5 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3131 = (uint32_t)_M0L4seedS988[5];
  if (36 < 0 || 36 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[36] = _M0L6_2atmpS3131;
  if (5 < 0 || 5 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3132 = (uint32_t)_M0L4seedS988[5];
  if (37 < 0 || 37 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[37] = _M0L6_2atmpS3132;
  if (5 < 0 || 5 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3133 = (uint32_t)_M0L4seedS988[5];
  if (38 < 0 || 38 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 248 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[38] = _M0L6_2atmpS3133;
  if (5 < 0 || 5 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3134 = (uint32_t)_M0L4seedS988[5];
  if (39 < 0 || 39 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 249 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[39] = _M0L6_2atmpS3134;
  if (6 < 0 || 6 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3135 = (uint32_t)_M0L4seedS988[6];
  if (40 < 0 || 40 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[40] = _M0L6_2atmpS3135;
  if (6 < 0 || 6 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3136 = (uint32_t)_M0L4seedS988[6];
  if (41 < 0 || 41 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 251 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[41] = _M0L6_2atmpS3136;
  if (6 < 0 || 6 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3137 = (uint32_t)_M0L4seedS988[6];
  if (42 < 0 || 42 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[42] = _M0L6_2atmpS3137;
  if (6 < 0 || 6 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3138 = (uint32_t)_M0L4seedS988[6];
  if (43 < 0 || 43 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[43] = _M0L6_2atmpS3138;
  if (7 < 0 || 7 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3139 = (uint32_t)_M0L4seedS988[7];
  if (44 < 0 || 44 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[44] = _M0L6_2atmpS3139;
  if (7 < 0 || 7 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 255 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3140 = (uint32_t)_M0L4seedS988[7];
  if (45 < 0 || 45 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 255 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[45] = _M0L6_2atmpS3140;
  if (7 < 0 || 7 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 256 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3141 = (uint32_t)_M0L4seedS988[7];
  if (46 < 0 || 46 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 256 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[46] = _M0L6_2atmpS3141;
  if (7 < 0 || 7 >= Moonbit_array_length(_M0L4seedS988)) {
    #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS3842 = (uint32_t)_M0L4seedS988[7];
  moonbit_decref(_M0L4seedS988);
  _M0L6_2atmpS3142 = _M0L6_2atmpS3842;
  if (47 < 0 || 47 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 257 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[47] = _M0L6_2atmpS3142;
  if (48 < 0 || 48 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 259 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[48] = _M0L7counterS989;
  _M0L6_2atmpS3143 = _M0L7counterS989 + 1u;
  if (49 < 0 || 49 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[49] = _M0L6_2atmpS3143;
  _M0L6_2atmpS3144 = _M0L7counterS989 + 2u;
  if (50 < 0 || 50 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 261 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[50] = _M0L6_2atmpS3144;
  _M0L6_2atmpS3145 = _M0L7counterS989 + 3u;
  if (51 < 0 || 51 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[51] = _M0L6_2atmpS3145;
  if (52 < 0 || 52 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 263 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[52] = 0u;
  if (53 < 0 || 53 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 264 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[53] = 0u;
  if (54 < 0 || 54 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 265 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[54] = 0u;
  if (55 < 0 || 55 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[55] = 0u;
  if (56 < 0 || 56 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 267 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[56] = 0u;
  if (57 < 0 || 57 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 268 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[57] = 0u;
  if (58 < 0 || 58 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 269 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[58] = 0u;
  if (59 < 0 || 59 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 270 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[59] = 0u;
  if (60 < 0 || 60 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 271 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[60] = 0u;
  if (61 < 0 || 61 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 272 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[61] = 0u;
  if (62 < 0 || 62 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 273 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[62] = 0u;
  if (63 < 0 || 63 >= Moonbit_array_length(_M0L3b32S987)) {
    #line 274 "C:\\Users\\Administrator\\.moon\\lib\\core\\random\\internal\\random_source\\random_source_chacha.mbt"
    moonbit_panic();
  }
  _M0L3b32S987[63] = 0u;
  moonbit_decref(_M0L3b32S987);
  return 0;
}

moonbit_string_t _M0FPC28encoding4utf821decode__lossy_2einner(
  struct _M0TPC15bytes9BytesView _M0L5bytesS779,
  int32_t _M0L11ignore__bomS780
) {
  struct _M0TPC15bytes9BytesView _M0L5bytesS777;
  int32_t _M0L6_2atmpS3095;
  int32_t _M0L6_2atmpS3094;
  moonbit_bytes_t _M0L1tS785;
  int32_t _M0L4tlenS786;
  int32_t _M0L11_2aparam__0S787;
  struct _M0TPC15bytes9BytesView _M0L11_2aparam__1S788;
  moonbit_bytes_t _M0L6_2atmpS2413;
  int64_t _M0L6_2atmpS2414;
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  if (_M0L11ignore__bomS780) {
    int32_t _M0L3endS3097 = _M0L5bytesS779.$2;
    int32_t _M0L5startS3098 = _M0L5bytesS779.$1;
    int32_t _M0L6_2atmpS3096 = _M0L3endS3097 - _M0L5startS3098;
    if (_M0L6_2atmpS3096 >= 3) {
      moonbit_bytes_t _M0L8_2afieldS4164 = _M0L5bytesS779.$0;
      moonbit_bytes_t _M0L5bytesS3109 = _M0L8_2afieldS4164;
      int32_t _M0L5startS3110 = _M0L5bytesS779.$1;
      int32_t _M0L6_2atmpS4163 = _M0L5bytesS3109[_M0L5startS3110];
      int32_t _M0L4_2axS782 = _M0L6_2atmpS4163;
      if (_M0L4_2axS782 == 239) {
        moonbit_bytes_t _M0L8_2afieldS4162 = _M0L5bytesS779.$0;
        moonbit_bytes_t _M0L5bytesS3106 = _M0L8_2afieldS4162;
        int32_t _M0L5startS3108 = _M0L5bytesS779.$1;
        int32_t _M0L6_2atmpS3107 = _M0L5startS3108 + 1;
        int32_t _M0L6_2atmpS4161 = _M0L5bytesS3106[_M0L6_2atmpS3107];
        int32_t _M0L4_2axS783 = _M0L6_2atmpS4161;
        if (_M0L4_2axS783 == 187) {
          moonbit_bytes_t _M0L8_2afieldS4160 = _M0L5bytesS779.$0;
          moonbit_bytes_t _M0L5bytesS3103 = _M0L8_2afieldS4160;
          int32_t _M0L5startS3105 = _M0L5bytesS779.$1;
          int32_t _M0L6_2atmpS3104 = _M0L5startS3105 + 2;
          int32_t _M0L6_2atmpS4159 = _M0L5bytesS3103[_M0L6_2atmpS3104];
          int32_t _M0L4_2axS784 = _M0L6_2atmpS4159;
          if (_M0L4_2axS784 == 191) {
            moonbit_bytes_t _M0L8_2afieldS4158 = _M0L5bytesS779.$0;
            moonbit_bytes_t _M0L5bytesS3099 = _M0L8_2afieldS4158;
            int32_t _M0L5startS3102 = _M0L5bytesS779.$1;
            int32_t _M0L6_2atmpS3100 = _M0L5startS3102 + 3;
            int32_t _M0L8_2afieldS4157 = _M0L5bytesS779.$2;
            int32_t _M0L3endS3101 = _M0L8_2afieldS4157;
            _M0L5bytesS777
            = (struct _M0TPC15bytes9BytesView){
              _M0L6_2atmpS3100, _M0L3endS3101, _M0L5bytesS3099
            };
          } else {
            goto join_781;
          }
        } else {
          goto join_781;
        }
      } else {
        goto join_781;
      }
    } else {
      goto join_781;
    }
    goto joinlet_4551;
    join_781:;
    goto join_778;
    joinlet_4551:;
  } else {
    goto join_778;
  }
  goto joinlet_4550;
  join_778:;
  _M0L5bytesS777 = _M0L5bytesS779;
  joinlet_4550:;
  moonbit_incref(_M0L5bytesS777.$0);
  #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  _M0L6_2atmpS3095 = _M0MPC15bytes9BytesView6length(_M0L5bytesS777);
  _M0L6_2atmpS3094 = _M0L6_2atmpS3095 * 2;
  _M0L1tS785 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS3094, 0);
  _M0L11_2aparam__0S787 = 0;
  _M0L11_2aparam__1S788 = _M0L5bytesS777;
  while (1) {
    int32_t _M0L4tlenS790;
    struct _M0TPC15bytes9BytesView _M0L4restS791;
    struct _M0TPC15bytes9BytesView _M0L4restS794;
    int32_t _M0L4tlenS795;
    struct _M0TPC15bytes9BytesView _M0L4restS797;
    int32_t _M0L4tlenS798;
    struct _M0TPC15bytes9BytesView _M0L4restS800;
    int32_t _M0L4tlenS801;
    int32_t _M0L4tlenS803;
    int32_t _M0L2b0S804;
    int32_t _M0L2b1S805;
    int32_t _M0L2b2S806;
    int32_t _M0L2b3S807;
    struct _M0TPC15bytes9BytesView _M0L4restS808;
    int32_t _M0L4tlenS814;
    int32_t _M0L2b0S815;
    int32_t _M0L2b1S816;
    int32_t _M0L2b2S817;
    struct _M0TPC15bytes9BytesView _M0L4restS818;
    int32_t _M0L4tlenS821;
    struct _M0TPC15bytes9BytesView _M0L4restS822;
    int32_t _M0L2b0S823;
    int32_t _M0L2b1S824;
    int32_t _M0L4tlenS827;
    struct _M0TPC15bytes9BytesView _M0L4restS828;
    int32_t _M0L1bS829;
    int32_t _M0L3endS2474 = _M0L11_2aparam__1S788.$2;
    int32_t _M0L5startS2475 = _M0L11_2aparam__1S788.$1;
    int32_t _M0L6_2atmpS2473 = _M0L3endS2474 - _M0L5startS2475;
    int32_t _M0L6_2atmpS2472;
    int32_t _M0L6_2atmpS2471;
    int32_t _M0L6_2atmpS2470;
    int32_t _M0L6_2atmpS2467;
    int32_t _M0L6_2atmpS2469;
    int32_t _M0L6_2atmpS2468;
    int32_t _M0L2chS825;
    int32_t _M0L6_2atmpS2462;
    int32_t _M0L6_2atmpS2463;
    int32_t _M0L6_2atmpS2465;
    int32_t _M0L6_2atmpS2464;
    int32_t _M0L6_2atmpS2466;
    int32_t _M0L6_2atmpS2461;
    int32_t _M0L6_2atmpS2460;
    int32_t _M0L6_2atmpS2456;
    int32_t _M0L6_2atmpS2459;
    int32_t _M0L6_2atmpS2458;
    int32_t _M0L6_2atmpS2457;
    int32_t _M0L6_2atmpS2453;
    int32_t _M0L6_2atmpS2455;
    int32_t _M0L6_2atmpS2454;
    int32_t _M0L2chS819;
    int32_t _M0L6_2atmpS2448;
    int32_t _M0L6_2atmpS2449;
    int32_t _M0L6_2atmpS2451;
    int32_t _M0L6_2atmpS2450;
    int32_t _M0L6_2atmpS2452;
    int32_t _M0L6_2atmpS2447;
    int32_t _M0L6_2atmpS2446;
    int32_t _M0L6_2atmpS2442;
    int32_t _M0L6_2atmpS2445;
    int32_t _M0L6_2atmpS2444;
    int32_t _M0L6_2atmpS2443;
    int32_t _M0L6_2atmpS2438;
    int32_t _M0L6_2atmpS2441;
    int32_t _M0L6_2atmpS2440;
    int32_t _M0L6_2atmpS2439;
    int32_t _M0L6_2atmpS2435;
    int32_t _M0L6_2atmpS2437;
    int32_t _M0L6_2atmpS2436;
    int32_t _M0L2chS809;
    int32_t _M0L3chmS810;
    int32_t _M0L6_2atmpS2434;
    int32_t _M0L3ch1S811;
    int32_t _M0L6_2atmpS2433;
    int32_t _M0L3ch2S812;
    int32_t _M0L6_2atmpS2423;
    int32_t _M0L6_2atmpS2424;
    int32_t _M0L6_2atmpS2426;
    int32_t _M0L6_2atmpS2425;
    int32_t _M0L6_2atmpS2427;
    int32_t _M0L6_2atmpS2428;
    int32_t _M0L6_2atmpS2429;
    int32_t _M0L6_2atmpS2431;
    int32_t _M0L6_2atmpS2430;
    int32_t _M0L6_2atmpS2432;
    int32_t _M0L6_2atmpS2421;
    int32_t _M0L6_2atmpS2422;
    int32_t _M0L6_2atmpS2419;
    int32_t _M0L6_2atmpS2420;
    int32_t _M0L6_2atmpS2417;
    int32_t _M0L6_2atmpS2418;
    int32_t _M0L6_2atmpS2415;
    int32_t _M0L6_2atmpS2416;
    if (_M0L6_2atmpS2473 == 0) {
      moonbit_decref(_M0L11_2aparam__1S788.$0);
      _M0L4tlenS786 = _M0L11_2aparam__0S787;
    } else {
      int32_t _M0L3endS2477 = _M0L11_2aparam__1S788.$2;
      int32_t _M0L5startS2478 = _M0L11_2aparam__1S788.$1;
      int32_t _M0L6_2atmpS2476 = _M0L3endS2477 - _M0L5startS2478;
      if (_M0L6_2atmpS2476 >= 8) {
        moonbit_bytes_t _M0L8_2afieldS3964 = _M0L11_2aparam__1S788.$0;
        moonbit_bytes_t _M0L5bytesS2702 = _M0L8_2afieldS3964;
        int32_t _M0L5startS2703 = _M0L11_2aparam__1S788.$1;
        int32_t _M0L6_2atmpS3963 = _M0L5bytesS2702[_M0L5startS2703];
        int32_t _M0L4_2axS830 = _M0L6_2atmpS3963;
        if (_M0L4_2axS830 <= 127) {
          moonbit_bytes_t _M0L8_2afieldS3872 = _M0L11_2aparam__1S788.$0;
          moonbit_bytes_t _M0L5bytesS2699 = _M0L8_2afieldS3872;
          int32_t _M0L5startS2701 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS2700 = _M0L5startS2701 + 1;
          int32_t _M0L6_2atmpS3871 = _M0L5bytesS2699[_M0L6_2atmpS2700];
          int32_t _M0L4_2axS831 = _M0L6_2atmpS3871;
          if (_M0L4_2axS831 <= 127) {
            moonbit_bytes_t _M0L8_2afieldS3868 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2696 = _M0L8_2afieldS3868;
            int32_t _M0L5startS2698 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2697 = _M0L5startS2698 + 2;
            int32_t _M0L6_2atmpS3867 = _M0L5bytesS2696[_M0L6_2atmpS2697];
            int32_t _M0L4_2axS832 = _M0L6_2atmpS3867;
            if (_M0L4_2axS832 <= 127) {
              moonbit_bytes_t _M0L8_2afieldS3864 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2693 = _M0L8_2afieldS3864;
              int32_t _M0L5startS2695 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2694 = _M0L5startS2695 + 3;
              int32_t _M0L6_2atmpS3863 = _M0L5bytesS2693[_M0L6_2atmpS2694];
              int32_t _M0L4_2axS833 = _M0L6_2atmpS3863;
              if (_M0L4_2axS833 <= 127) {
                moonbit_bytes_t _M0L8_2afieldS3860 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS2690 = _M0L8_2afieldS3860;
                int32_t _M0L5startS2692 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS2691 = _M0L5startS2692 + 4;
                int32_t _M0L6_2atmpS3859 = _M0L5bytesS2690[_M0L6_2atmpS2691];
                int32_t _M0L4_2axS834 = _M0L6_2atmpS3859;
                if (_M0L4_2axS834 <= 127) {
                  moonbit_bytes_t _M0L8_2afieldS3856 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS2687 = _M0L8_2afieldS3856;
                  int32_t _M0L5startS2689 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS2688 = _M0L5startS2689 + 5;
                  int32_t _M0L6_2atmpS3855 =
                    _M0L5bytesS2687[_M0L6_2atmpS2688];
                  int32_t _M0L4_2axS835 = _M0L6_2atmpS3855;
                  if (_M0L4_2axS835 <= 127) {
                    moonbit_bytes_t _M0L8_2afieldS3852 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS2684 = _M0L8_2afieldS3852;
                    int32_t _M0L5startS2686 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2685 = _M0L5startS2686 + 6;
                    int32_t _M0L6_2atmpS3851 =
                      _M0L5bytesS2684[_M0L6_2atmpS2685];
                    int32_t _M0L4_2axS836 = _M0L6_2atmpS3851;
                    if (_M0L4_2axS836 <= 127) {
                      moonbit_bytes_t _M0L8_2afieldS3848 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2681 = _M0L8_2afieldS3848;
                      int32_t _M0L5startS2683 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2682 = _M0L5startS2683 + 7;
                      int32_t _M0L6_2atmpS3847 =
                        _M0L5bytesS2681[_M0L6_2atmpS2682];
                      int32_t _M0L4_2axS837 = _M0L6_2atmpS3847;
                      if (_M0L4_2axS837 <= 127) {
                        moonbit_bytes_t _M0L8_2afieldS3844 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2677 = _M0L8_2afieldS3844;
                        int32_t _M0L5startS2680 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2678 = _M0L5startS2680 + 8;
                        int32_t _M0L8_2afieldS3843 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2679 = _M0L8_2afieldS3843;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS838 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2678,
                                                             _M0L3endS2679,
                                                             _M0L5bytesS2677};
                        int32_t _M0L6_2atmpS2669;
                        int32_t _M0L6_2atmpS2670;
                        int32_t _M0L6_2atmpS2671;
                        int32_t _M0L6_2atmpS2672;
                        int32_t _M0L6_2atmpS2673;
                        int32_t _M0L6_2atmpS2674;
                        int32_t _M0L6_2atmpS2675;
                        int32_t _M0L6_2atmpS2676;
                        _M0L1tS785[_M0L11_2aparam__0S787] = _M0L4_2axS830;
                        _M0L6_2atmpS2669 = _M0L11_2aparam__0S787 + 2;
                        _M0L1tS785[_M0L6_2atmpS2669] = _M0L4_2axS831;
                        _M0L6_2atmpS2670 = _M0L11_2aparam__0S787 + 4;
                        _M0L1tS785[_M0L6_2atmpS2670] = _M0L4_2axS832;
                        _M0L6_2atmpS2671 = _M0L11_2aparam__0S787 + 6;
                        _M0L1tS785[_M0L6_2atmpS2671] = _M0L4_2axS833;
                        _M0L6_2atmpS2672 = _M0L11_2aparam__0S787 + 8;
                        _M0L1tS785[_M0L6_2atmpS2672] = _M0L4_2axS834;
                        _M0L6_2atmpS2673 = _M0L11_2aparam__0S787 + 10;
                        _M0L1tS785[_M0L6_2atmpS2673] = _M0L4_2axS835;
                        _M0L6_2atmpS2674 = _M0L11_2aparam__0S787 + 12;
                        _M0L1tS785[_M0L6_2atmpS2674] = _M0L4_2axS836;
                        _M0L6_2atmpS2675 = _M0L11_2aparam__0S787 + 14;
                        _M0L1tS785[_M0L6_2atmpS2675] = _M0L4_2axS837;
                        _M0L6_2atmpS2676 = _M0L11_2aparam__0S787 + 16;
                        _M0L11_2aparam__0S787 = _M0L6_2atmpS2676;
                        _M0L11_2aparam__1S788 = _M0L4_2axS838;
                        continue;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3846 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2665 = _M0L8_2afieldS3846;
                        int32_t _M0L5startS2668 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2666 = _M0L5startS2668 + 1;
                        int32_t _M0L8_2afieldS3845 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2667 = _M0L8_2afieldS3845;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS839 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2666,
                                                             _M0L3endS2667,
                                                             _M0L5bytesS2665};
                        _M0L4tlenS827 = _M0L11_2aparam__0S787;
                        _M0L4restS828 = _M0L4_2axS839;
                        _M0L1bS829 = _M0L4_2axS830;
                        goto join_826;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3850 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2661 = _M0L8_2afieldS3850;
                      int32_t _M0L5startS2664 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2662 = _M0L5startS2664 + 1;
                      int32_t _M0L8_2afieldS3849 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2663 = _M0L8_2afieldS3849;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS840 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2662,
                                                           _M0L3endS2663,
                                                           _M0L5bytesS2661};
                      _M0L4tlenS827 = _M0L11_2aparam__0S787;
                      _M0L4restS828 = _M0L4_2axS840;
                      _M0L1bS829 = _M0L4_2axS830;
                      goto join_826;
                    }
                  } else {
                    moonbit_bytes_t _M0L8_2afieldS3854 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS2657 = _M0L8_2afieldS3854;
                    int32_t _M0L5startS2660 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2658 = _M0L5startS2660 + 1;
                    int32_t _M0L8_2afieldS3853 = _M0L11_2aparam__1S788.$2;
                    int32_t _M0L3endS2659 = _M0L8_2afieldS3853;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS841 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2658,
                                                         _M0L3endS2659,
                                                         _M0L5bytesS2657};
                    _M0L4tlenS827 = _M0L11_2aparam__0S787;
                    _M0L4restS828 = _M0L4_2axS841;
                    _M0L1bS829 = _M0L4_2axS830;
                    goto join_826;
                  }
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3858 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS2653 = _M0L8_2afieldS3858;
                  int32_t _M0L5startS2656 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS2654 = _M0L5startS2656 + 1;
                  int32_t _M0L8_2afieldS3857 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS2655 = _M0L8_2afieldS3857;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS842 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2654,
                                                       _M0L3endS2655,
                                                       _M0L5bytesS2653};
                  _M0L4tlenS827 = _M0L11_2aparam__0S787;
                  _M0L4restS828 = _M0L4_2axS842;
                  _M0L1bS829 = _M0L4_2axS830;
                  goto join_826;
                }
              } else {
                moonbit_bytes_t _M0L8_2afieldS3862 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS2649 = _M0L8_2afieldS3862;
                int32_t _M0L5startS2652 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS2650 = _M0L5startS2652 + 1;
                int32_t _M0L8_2afieldS3861 = _M0L11_2aparam__1S788.$2;
                int32_t _M0L3endS2651 = _M0L8_2afieldS3861;
                struct _M0TPC15bytes9BytesView _M0L4_2axS843 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2650,
                                                     _M0L3endS2651,
                                                     _M0L5bytesS2649};
                _M0L4tlenS827 = _M0L11_2aparam__0S787;
                _M0L4restS828 = _M0L4_2axS843;
                _M0L1bS829 = _M0L4_2axS830;
                goto join_826;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3866 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2645 = _M0L8_2afieldS3866;
              int32_t _M0L5startS2648 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2646 = _M0L5startS2648 + 1;
              int32_t _M0L8_2afieldS3865 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2647 = _M0L8_2afieldS3865;
              struct _M0TPC15bytes9BytesView _M0L4_2axS844 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2646,
                                                   _M0L3endS2647,
                                                   _M0L5bytesS2645};
              _M0L4tlenS827 = _M0L11_2aparam__0S787;
              _M0L4restS828 = _M0L4_2axS844;
              _M0L1bS829 = _M0L4_2axS830;
              goto join_826;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3870 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2641 = _M0L8_2afieldS3870;
            int32_t _M0L5startS2644 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2642 = _M0L5startS2644 + 1;
            int32_t _M0L8_2afieldS3869 = _M0L11_2aparam__1S788.$2;
            int32_t _M0L3endS2643 = _M0L8_2afieldS3869;
            struct _M0TPC15bytes9BytesView _M0L4_2axS845 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2642,
                                                 _M0L3endS2643,
                                                 _M0L5bytesS2641};
            _M0L4tlenS827 = _M0L11_2aparam__0S787;
            _M0L4restS828 = _M0L4_2axS845;
            _M0L1bS829 = _M0L4_2axS830;
            goto join_826;
          }
        } else if (_M0L4_2axS830 >= 194 && _M0L4_2axS830 <= 223) {
          moonbit_bytes_t _M0L8_2afieldS3878 = _M0L11_2aparam__1S788.$0;
          moonbit_bytes_t _M0L5bytesS2638 = _M0L8_2afieldS3878;
          int32_t _M0L5startS2640 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS2639 = _M0L5startS2640 + 1;
          int32_t _M0L6_2atmpS3877 = _M0L5bytesS2638[_M0L6_2atmpS2639];
          int32_t _M0L4_2axS846 = _M0L6_2atmpS3877;
          if (_M0L4_2axS846 >= 128 && _M0L4_2axS846 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3874 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2634 = _M0L8_2afieldS3874;
            int32_t _M0L5startS2637 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2635 = _M0L5startS2637 + 2;
            int32_t _M0L8_2afieldS3873 = _M0L11_2aparam__1S788.$2;
            int32_t _M0L3endS2636 = _M0L8_2afieldS3873;
            struct _M0TPC15bytes9BytesView _M0L4_2axS847 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2635,
                                                 _M0L3endS2636,
                                                 _M0L5bytesS2634};
            _M0L4tlenS821 = _M0L11_2aparam__0S787;
            _M0L4restS822 = _M0L4_2axS847;
            _M0L2b0S823 = _M0L4_2axS830;
            _M0L2b1S824 = _M0L4_2axS846;
            goto join_820;
          } else {
            moonbit_bytes_t _M0L8_2afieldS3876 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2630 = _M0L8_2afieldS3876;
            int32_t _M0L5startS2633 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2631 = _M0L5startS2633 + 1;
            int32_t _M0L8_2afieldS3875 = _M0L11_2aparam__1S788.$2;
            int32_t _M0L3endS2632 = _M0L8_2afieldS3875;
            struct _M0TPC15bytes9BytesView _M0L4_2axS848 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2631,
                                                 _M0L3endS2632,
                                                 _M0L5bytesS2630};
            _M0L4tlenS790 = _M0L11_2aparam__0S787;
            _M0L4restS791 = _M0L4_2axS848;
            goto join_789;
          }
        } else if (_M0L4_2axS830 == 224) {
          moonbit_bytes_t _M0L8_2afieldS3888 = _M0L11_2aparam__1S788.$0;
          moonbit_bytes_t _M0L5bytesS2627 = _M0L8_2afieldS3888;
          int32_t _M0L5startS2629 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS2628 = _M0L5startS2629 + 1;
          int32_t _M0L6_2atmpS3887 = _M0L5bytesS2627[_M0L6_2atmpS2628];
          int32_t _M0L4_2axS849 = _M0L6_2atmpS3887;
          if (_M0L4_2axS849 >= 160 && _M0L4_2axS849 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3884 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2624 = _M0L8_2afieldS3884;
            int32_t _M0L5startS2626 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2625 = _M0L5startS2626 + 2;
            int32_t _M0L6_2atmpS3883 = _M0L5bytesS2624[_M0L6_2atmpS2625];
            int32_t _M0L4_2axS850 = _M0L6_2atmpS3883;
            if (_M0L4_2axS850 >= 128 && _M0L4_2axS850 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3880 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2620 = _M0L8_2afieldS3880;
              int32_t _M0L5startS2623 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2621 = _M0L5startS2623 + 3;
              int32_t _M0L8_2afieldS3879 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2622 = _M0L8_2afieldS3879;
              struct _M0TPC15bytes9BytesView _M0L4_2axS851 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2621,
                                                   _M0L3endS2622,
                                                   _M0L5bytesS2620};
              _M0L4tlenS814 = _M0L11_2aparam__0S787;
              _M0L2b0S815 = _M0L4_2axS830;
              _M0L2b1S816 = _M0L4_2axS849;
              _M0L2b2S817 = _M0L4_2axS850;
              _M0L4restS818 = _M0L4_2axS851;
              goto join_813;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3882 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2616 = _M0L8_2afieldS3882;
              int32_t _M0L5startS2619 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2617 = _M0L5startS2619 + 2;
              int32_t _M0L8_2afieldS3881 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2618 = _M0L8_2afieldS3881;
              struct _M0TPC15bytes9BytesView _M0L4_2axS852 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2617,
                                                   _M0L3endS2618,
                                                   _M0L5bytesS2616};
              _M0L4restS800 = _M0L4_2axS852;
              _M0L4tlenS801 = _M0L11_2aparam__0S787;
              goto join_799;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3886 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2612 = _M0L8_2afieldS3886;
            int32_t _M0L5startS2615 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2613 = _M0L5startS2615 + 1;
            int32_t _M0L8_2afieldS3885 = _M0L11_2aparam__1S788.$2;
            int32_t _M0L3endS2614 = _M0L8_2afieldS3885;
            struct _M0TPC15bytes9BytesView _M0L4_2axS853 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2613,
                                                 _M0L3endS2614,
                                                 _M0L5bytesS2612};
            _M0L4tlenS790 = _M0L11_2aparam__0S787;
            _M0L4restS791 = _M0L4_2axS853;
            goto join_789;
          }
        } else if (_M0L4_2axS830 >= 225 && _M0L4_2axS830 <= 236) {
          moonbit_bytes_t _M0L8_2afieldS3898 = _M0L11_2aparam__1S788.$0;
          moonbit_bytes_t _M0L5bytesS2609 = _M0L8_2afieldS3898;
          int32_t _M0L5startS2611 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS2610 = _M0L5startS2611 + 1;
          int32_t _M0L6_2atmpS3897 = _M0L5bytesS2609[_M0L6_2atmpS2610];
          int32_t _M0L4_2axS854 = _M0L6_2atmpS3897;
          if (_M0L4_2axS854 >= 128 && _M0L4_2axS854 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3894 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2606 = _M0L8_2afieldS3894;
            int32_t _M0L5startS2608 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2607 = _M0L5startS2608 + 2;
            int32_t _M0L6_2atmpS3893 = _M0L5bytesS2606[_M0L6_2atmpS2607];
            int32_t _M0L4_2axS855 = _M0L6_2atmpS3893;
            if (_M0L4_2axS855 >= 128 && _M0L4_2axS855 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3890 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2602 = _M0L8_2afieldS3890;
              int32_t _M0L5startS2605 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2603 = _M0L5startS2605 + 3;
              int32_t _M0L8_2afieldS3889 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2604 = _M0L8_2afieldS3889;
              struct _M0TPC15bytes9BytesView _M0L4_2axS856 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2603,
                                                   _M0L3endS2604,
                                                   _M0L5bytesS2602};
              _M0L4tlenS814 = _M0L11_2aparam__0S787;
              _M0L2b0S815 = _M0L4_2axS830;
              _M0L2b1S816 = _M0L4_2axS854;
              _M0L2b2S817 = _M0L4_2axS855;
              _M0L4restS818 = _M0L4_2axS856;
              goto join_813;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3892 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2598 = _M0L8_2afieldS3892;
              int32_t _M0L5startS2601 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2599 = _M0L5startS2601 + 2;
              int32_t _M0L8_2afieldS3891 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2600 = _M0L8_2afieldS3891;
              struct _M0TPC15bytes9BytesView _M0L4_2axS857 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2599,
                                                   _M0L3endS2600,
                                                   _M0L5bytesS2598};
              _M0L4restS800 = _M0L4_2axS857;
              _M0L4tlenS801 = _M0L11_2aparam__0S787;
              goto join_799;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3896 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2594 = _M0L8_2afieldS3896;
            int32_t _M0L5startS2597 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2595 = _M0L5startS2597 + 1;
            int32_t _M0L8_2afieldS3895 = _M0L11_2aparam__1S788.$2;
            int32_t _M0L3endS2596 = _M0L8_2afieldS3895;
            struct _M0TPC15bytes9BytesView _M0L4_2axS858 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2595,
                                                 _M0L3endS2596,
                                                 _M0L5bytesS2594};
            _M0L4tlenS790 = _M0L11_2aparam__0S787;
            _M0L4restS791 = _M0L4_2axS858;
            goto join_789;
          }
        } else if (_M0L4_2axS830 == 237) {
          moonbit_bytes_t _M0L8_2afieldS3908 = _M0L11_2aparam__1S788.$0;
          moonbit_bytes_t _M0L5bytesS2591 = _M0L8_2afieldS3908;
          int32_t _M0L5startS2593 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS2592 = _M0L5startS2593 + 1;
          int32_t _M0L6_2atmpS3907 = _M0L5bytesS2591[_M0L6_2atmpS2592];
          int32_t _M0L4_2axS859 = _M0L6_2atmpS3907;
          if (_M0L4_2axS859 >= 128 && _M0L4_2axS859 <= 159) {
            moonbit_bytes_t _M0L8_2afieldS3904 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2588 = _M0L8_2afieldS3904;
            int32_t _M0L5startS2590 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2589 = _M0L5startS2590 + 2;
            int32_t _M0L6_2atmpS3903 = _M0L5bytesS2588[_M0L6_2atmpS2589];
            int32_t _M0L4_2axS860 = _M0L6_2atmpS3903;
            if (_M0L4_2axS860 >= 128 && _M0L4_2axS860 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3900 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2584 = _M0L8_2afieldS3900;
              int32_t _M0L5startS2587 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2585 = _M0L5startS2587 + 3;
              int32_t _M0L8_2afieldS3899 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2586 = _M0L8_2afieldS3899;
              struct _M0TPC15bytes9BytesView _M0L4_2axS861 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2585,
                                                   _M0L3endS2586,
                                                   _M0L5bytesS2584};
              _M0L4tlenS814 = _M0L11_2aparam__0S787;
              _M0L2b0S815 = _M0L4_2axS830;
              _M0L2b1S816 = _M0L4_2axS859;
              _M0L2b2S817 = _M0L4_2axS860;
              _M0L4restS818 = _M0L4_2axS861;
              goto join_813;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3902 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2580 = _M0L8_2afieldS3902;
              int32_t _M0L5startS2583 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2581 = _M0L5startS2583 + 2;
              int32_t _M0L8_2afieldS3901 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2582 = _M0L8_2afieldS3901;
              struct _M0TPC15bytes9BytesView _M0L4_2axS862 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2581,
                                                   _M0L3endS2582,
                                                   _M0L5bytesS2580};
              _M0L4restS800 = _M0L4_2axS862;
              _M0L4tlenS801 = _M0L11_2aparam__0S787;
              goto join_799;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3906 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2576 = _M0L8_2afieldS3906;
            int32_t _M0L5startS2579 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2577 = _M0L5startS2579 + 1;
            int32_t _M0L8_2afieldS3905 = _M0L11_2aparam__1S788.$2;
            int32_t _M0L3endS2578 = _M0L8_2afieldS3905;
            struct _M0TPC15bytes9BytesView _M0L4_2axS863 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2577,
                                                 _M0L3endS2578,
                                                 _M0L5bytesS2576};
            _M0L4tlenS790 = _M0L11_2aparam__0S787;
            _M0L4restS791 = _M0L4_2axS863;
            goto join_789;
          }
        } else if (_M0L4_2axS830 >= 238 && _M0L4_2axS830 <= 239) {
          moonbit_bytes_t _M0L8_2afieldS3918 = _M0L11_2aparam__1S788.$0;
          moonbit_bytes_t _M0L5bytesS2573 = _M0L8_2afieldS3918;
          int32_t _M0L5startS2575 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS2574 = _M0L5startS2575 + 1;
          int32_t _M0L6_2atmpS3917 = _M0L5bytesS2573[_M0L6_2atmpS2574];
          int32_t _M0L4_2axS864 = _M0L6_2atmpS3917;
          if (_M0L4_2axS864 >= 128 && _M0L4_2axS864 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3914 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2570 = _M0L8_2afieldS3914;
            int32_t _M0L5startS2572 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2571 = _M0L5startS2572 + 2;
            int32_t _M0L6_2atmpS3913 = _M0L5bytesS2570[_M0L6_2atmpS2571];
            int32_t _M0L4_2axS865 = _M0L6_2atmpS3913;
            if (_M0L4_2axS865 >= 128 && _M0L4_2axS865 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3910 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2566 = _M0L8_2afieldS3910;
              int32_t _M0L5startS2569 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2567 = _M0L5startS2569 + 3;
              int32_t _M0L8_2afieldS3909 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2568 = _M0L8_2afieldS3909;
              struct _M0TPC15bytes9BytesView _M0L4_2axS866 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2567,
                                                   _M0L3endS2568,
                                                   _M0L5bytesS2566};
              _M0L4tlenS814 = _M0L11_2aparam__0S787;
              _M0L2b0S815 = _M0L4_2axS830;
              _M0L2b1S816 = _M0L4_2axS864;
              _M0L2b2S817 = _M0L4_2axS865;
              _M0L4restS818 = _M0L4_2axS866;
              goto join_813;
            } else {
              moonbit_bytes_t _M0L8_2afieldS3912 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2562 = _M0L8_2afieldS3912;
              int32_t _M0L5startS2565 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2563 = _M0L5startS2565 + 2;
              int32_t _M0L8_2afieldS3911 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2564 = _M0L8_2afieldS3911;
              struct _M0TPC15bytes9BytesView _M0L4_2axS867 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2563,
                                                   _M0L3endS2564,
                                                   _M0L5bytesS2562};
              _M0L4restS800 = _M0L4_2axS867;
              _M0L4tlenS801 = _M0L11_2aparam__0S787;
              goto join_799;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3916 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2558 = _M0L8_2afieldS3916;
            int32_t _M0L5startS2561 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2559 = _M0L5startS2561 + 1;
            int32_t _M0L8_2afieldS3915 = _M0L11_2aparam__1S788.$2;
            int32_t _M0L3endS2560 = _M0L8_2afieldS3915;
            struct _M0TPC15bytes9BytesView _M0L4_2axS868 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2559,
                                                 _M0L3endS2560,
                                                 _M0L5bytesS2558};
            _M0L4tlenS790 = _M0L11_2aparam__0S787;
            _M0L4restS791 = _M0L4_2axS868;
            goto join_789;
          }
        } else if (_M0L4_2axS830 == 240) {
          moonbit_bytes_t _M0L8_2afieldS3932 = _M0L11_2aparam__1S788.$0;
          moonbit_bytes_t _M0L5bytesS2555 = _M0L8_2afieldS3932;
          int32_t _M0L5startS2557 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS2556 = _M0L5startS2557 + 1;
          int32_t _M0L6_2atmpS3931 = _M0L5bytesS2555[_M0L6_2atmpS2556];
          int32_t _M0L4_2axS869 = _M0L6_2atmpS3931;
          if (_M0L4_2axS869 >= 144 && _M0L4_2axS869 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3928 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2552 = _M0L8_2afieldS3928;
            int32_t _M0L5startS2554 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2553 = _M0L5startS2554 + 2;
            int32_t _M0L6_2atmpS3927 = _M0L5bytesS2552[_M0L6_2atmpS2553];
            int32_t _M0L4_2axS870 = _M0L6_2atmpS3927;
            if (_M0L4_2axS870 >= 128 && _M0L4_2axS870 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3924 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2549 = _M0L8_2afieldS3924;
              int32_t _M0L5startS2551 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2550 = _M0L5startS2551 + 3;
              int32_t _M0L6_2atmpS3923 = _M0L5bytesS2549[_M0L6_2atmpS2550];
              int32_t _M0L4_2axS871 = _M0L6_2atmpS3923;
              if (_M0L4_2axS871 >= 128 && _M0L4_2axS871 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3920 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS2545 = _M0L8_2afieldS3920;
                int32_t _M0L5startS2548 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS2546 = _M0L5startS2548 + 4;
                int32_t _M0L8_2afieldS3919 = _M0L11_2aparam__1S788.$2;
                int32_t _M0L3endS2547 = _M0L8_2afieldS3919;
                struct _M0TPC15bytes9BytesView _M0L4_2axS872 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2546,
                                                     _M0L3endS2547,
                                                     _M0L5bytesS2545};
                _M0L4tlenS803 = _M0L11_2aparam__0S787;
                _M0L2b0S804 = _M0L4_2axS830;
                _M0L2b1S805 = _M0L4_2axS869;
                _M0L2b2S806 = _M0L4_2axS870;
                _M0L2b3S807 = _M0L4_2axS871;
                _M0L4restS808 = _M0L4_2axS872;
                goto join_802;
              } else {
                moonbit_bytes_t _M0L8_2afieldS3922 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS2541 = _M0L8_2afieldS3922;
                int32_t _M0L5startS2544 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS2542 = _M0L5startS2544 + 3;
                int32_t _M0L8_2afieldS3921 = _M0L11_2aparam__1S788.$2;
                int32_t _M0L3endS2543 = _M0L8_2afieldS3921;
                struct _M0TPC15bytes9BytesView _M0L4_2axS873 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2542,
                                                     _M0L3endS2543,
                                                     _M0L5bytesS2541};
                _M0L4restS797 = _M0L4_2axS873;
                _M0L4tlenS798 = _M0L11_2aparam__0S787;
                goto join_796;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3926 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2537 = _M0L8_2afieldS3926;
              int32_t _M0L5startS2540 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2538 = _M0L5startS2540 + 2;
              int32_t _M0L8_2afieldS3925 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2539 = _M0L8_2afieldS3925;
              struct _M0TPC15bytes9BytesView _M0L4_2axS874 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2538,
                                                   _M0L3endS2539,
                                                   _M0L5bytesS2537};
              _M0L4restS794 = _M0L4_2axS874;
              _M0L4tlenS795 = _M0L11_2aparam__0S787;
              goto join_793;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3930 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2533 = _M0L8_2afieldS3930;
            int32_t _M0L5startS2536 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2534 = _M0L5startS2536 + 1;
            int32_t _M0L8_2afieldS3929 = _M0L11_2aparam__1S788.$2;
            int32_t _M0L3endS2535 = _M0L8_2afieldS3929;
            struct _M0TPC15bytes9BytesView _M0L4_2axS875 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2534,
                                                 _M0L3endS2535,
                                                 _M0L5bytesS2533};
            _M0L4tlenS790 = _M0L11_2aparam__0S787;
            _M0L4restS791 = _M0L4_2axS875;
            goto join_789;
          }
        } else if (_M0L4_2axS830 >= 241 && _M0L4_2axS830 <= 243) {
          moonbit_bytes_t _M0L8_2afieldS3946 = _M0L11_2aparam__1S788.$0;
          moonbit_bytes_t _M0L5bytesS2530 = _M0L8_2afieldS3946;
          int32_t _M0L5startS2532 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS2531 = _M0L5startS2532 + 1;
          int32_t _M0L6_2atmpS3945 = _M0L5bytesS2530[_M0L6_2atmpS2531];
          int32_t _M0L4_2axS876 = _M0L6_2atmpS3945;
          if (_M0L4_2axS876 >= 128 && _M0L4_2axS876 <= 191) {
            moonbit_bytes_t _M0L8_2afieldS3942 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2527 = _M0L8_2afieldS3942;
            int32_t _M0L5startS2529 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2528 = _M0L5startS2529 + 2;
            int32_t _M0L6_2atmpS3941 = _M0L5bytesS2527[_M0L6_2atmpS2528];
            int32_t _M0L4_2axS877 = _M0L6_2atmpS3941;
            if (_M0L4_2axS877 >= 128 && _M0L4_2axS877 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3938 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2524 = _M0L8_2afieldS3938;
              int32_t _M0L5startS2526 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2525 = _M0L5startS2526 + 3;
              int32_t _M0L6_2atmpS3937 = _M0L5bytesS2524[_M0L6_2atmpS2525];
              int32_t _M0L4_2axS878 = _M0L6_2atmpS3937;
              if (_M0L4_2axS878 >= 128 && _M0L4_2axS878 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3934 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS2520 = _M0L8_2afieldS3934;
                int32_t _M0L5startS2523 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS2521 = _M0L5startS2523 + 4;
                int32_t _M0L8_2afieldS3933 = _M0L11_2aparam__1S788.$2;
                int32_t _M0L3endS2522 = _M0L8_2afieldS3933;
                struct _M0TPC15bytes9BytesView _M0L4_2axS879 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2521,
                                                     _M0L3endS2522,
                                                     _M0L5bytesS2520};
                _M0L4tlenS803 = _M0L11_2aparam__0S787;
                _M0L2b0S804 = _M0L4_2axS830;
                _M0L2b1S805 = _M0L4_2axS876;
                _M0L2b2S806 = _M0L4_2axS877;
                _M0L2b3S807 = _M0L4_2axS878;
                _M0L4restS808 = _M0L4_2axS879;
                goto join_802;
              } else {
                moonbit_bytes_t _M0L8_2afieldS3936 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS2516 = _M0L8_2afieldS3936;
                int32_t _M0L5startS2519 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS2517 = _M0L5startS2519 + 3;
                int32_t _M0L8_2afieldS3935 = _M0L11_2aparam__1S788.$2;
                int32_t _M0L3endS2518 = _M0L8_2afieldS3935;
                struct _M0TPC15bytes9BytesView _M0L4_2axS880 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2517,
                                                     _M0L3endS2518,
                                                     _M0L5bytesS2516};
                _M0L4restS797 = _M0L4_2axS880;
                _M0L4tlenS798 = _M0L11_2aparam__0S787;
                goto join_796;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3940 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2512 = _M0L8_2afieldS3940;
              int32_t _M0L5startS2515 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2513 = _M0L5startS2515 + 2;
              int32_t _M0L8_2afieldS3939 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2514 = _M0L8_2afieldS3939;
              struct _M0TPC15bytes9BytesView _M0L4_2axS881 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2513,
                                                   _M0L3endS2514,
                                                   _M0L5bytesS2512};
              _M0L4restS794 = _M0L4_2axS881;
              _M0L4tlenS795 = _M0L11_2aparam__0S787;
              goto join_793;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3944 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2508 = _M0L8_2afieldS3944;
            int32_t _M0L5startS2511 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2509 = _M0L5startS2511 + 1;
            int32_t _M0L8_2afieldS3943 = _M0L11_2aparam__1S788.$2;
            int32_t _M0L3endS2510 = _M0L8_2afieldS3943;
            struct _M0TPC15bytes9BytesView _M0L4_2axS882 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2509,
                                                 _M0L3endS2510,
                                                 _M0L5bytesS2508};
            _M0L4tlenS790 = _M0L11_2aparam__0S787;
            _M0L4restS791 = _M0L4_2axS882;
            goto join_789;
          }
        } else if (_M0L4_2axS830 == 244) {
          moonbit_bytes_t _M0L8_2afieldS3960 = _M0L11_2aparam__1S788.$0;
          moonbit_bytes_t _M0L5bytesS2505 = _M0L8_2afieldS3960;
          int32_t _M0L5startS2507 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS2506 = _M0L5startS2507 + 1;
          int32_t _M0L6_2atmpS3959 = _M0L5bytesS2505[_M0L6_2atmpS2506];
          int32_t _M0L4_2axS883 = _M0L6_2atmpS3959;
          if (_M0L4_2axS883 >= 128 && _M0L4_2axS883 <= 143) {
            moonbit_bytes_t _M0L8_2afieldS3956 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2502 = _M0L8_2afieldS3956;
            int32_t _M0L5startS2504 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2503 = _M0L5startS2504 + 2;
            int32_t _M0L6_2atmpS3955 = _M0L5bytesS2502[_M0L6_2atmpS2503];
            int32_t _M0L4_2axS884 = _M0L6_2atmpS3955;
            if (_M0L4_2axS884 >= 128 && _M0L4_2axS884 <= 191) {
              moonbit_bytes_t _M0L8_2afieldS3952 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2499 = _M0L8_2afieldS3952;
              int32_t _M0L5startS2501 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2500 = _M0L5startS2501 + 3;
              int32_t _M0L6_2atmpS3951 = _M0L5bytesS2499[_M0L6_2atmpS2500];
              int32_t _M0L4_2axS885 = _M0L6_2atmpS3951;
              if (_M0L4_2axS885 >= 128 && _M0L4_2axS885 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3948 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS2495 = _M0L8_2afieldS3948;
                int32_t _M0L5startS2498 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS2496 = _M0L5startS2498 + 4;
                int32_t _M0L8_2afieldS3947 = _M0L11_2aparam__1S788.$2;
                int32_t _M0L3endS2497 = _M0L8_2afieldS3947;
                struct _M0TPC15bytes9BytesView _M0L4_2axS886 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2496,
                                                     _M0L3endS2497,
                                                     _M0L5bytesS2495};
                _M0L4tlenS803 = _M0L11_2aparam__0S787;
                _M0L2b0S804 = _M0L4_2axS830;
                _M0L2b1S805 = _M0L4_2axS883;
                _M0L2b2S806 = _M0L4_2axS884;
                _M0L2b3S807 = _M0L4_2axS885;
                _M0L4restS808 = _M0L4_2axS886;
                goto join_802;
              } else {
                moonbit_bytes_t _M0L8_2afieldS3950 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS2491 = _M0L8_2afieldS3950;
                int32_t _M0L5startS2494 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS2492 = _M0L5startS2494 + 3;
                int32_t _M0L8_2afieldS3949 = _M0L11_2aparam__1S788.$2;
                int32_t _M0L3endS2493 = _M0L8_2afieldS3949;
                struct _M0TPC15bytes9BytesView _M0L4_2axS887 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2492,
                                                     _M0L3endS2493,
                                                     _M0L5bytesS2491};
                _M0L4restS797 = _M0L4_2axS887;
                _M0L4tlenS798 = _M0L11_2aparam__0S787;
                goto join_796;
              }
            } else {
              moonbit_bytes_t _M0L8_2afieldS3954 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS2487 = _M0L8_2afieldS3954;
              int32_t _M0L5startS2490 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2488 = _M0L5startS2490 + 2;
              int32_t _M0L8_2afieldS3953 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L3endS2489 = _M0L8_2afieldS3953;
              struct _M0TPC15bytes9BytesView _M0L4_2axS888 =
                (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2488,
                                                   _M0L3endS2489,
                                                   _M0L5bytesS2487};
              _M0L4restS794 = _M0L4_2axS888;
              _M0L4tlenS795 = _M0L11_2aparam__0S787;
              goto join_793;
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS3958 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS2483 = _M0L8_2afieldS3958;
            int32_t _M0L5startS2486 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS2484 = _M0L5startS2486 + 1;
            int32_t _M0L8_2afieldS3957 = _M0L11_2aparam__1S788.$2;
            int32_t _M0L3endS2485 = _M0L8_2afieldS3957;
            struct _M0TPC15bytes9BytesView _M0L4_2axS889 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2484,
                                                 _M0L3endS2485,
                                                 _M0L5bytesS2483};
            _M0L4tlenS790 = _M0L11_2aparam__0S787;
            _M0L4restS791 = _M0L4_2axS889;
            goto join_789;
          }
        } else {
          moonbit_bytes_t _M0L8_2afieldS3962 = _M0L11_2aparam__1S788.$0;
          moonbit_bytes_t _M0L5bytesS2479 = _M0L8_2afieldS3962;
          int32_t _M0L5startS2482 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS2480 = _M0L5startS2482 + 1;
          int32_t _M0L8_2afieldS3961 = _M0L11_2aparam__1S788.$2;
          int32_t _M0L3endS2481 = _M0L8_2afieldS3961;
          struct _M0TPC15bytes9BytesView _M0L4_2axS890 =
            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2480,
                                               _M0L3endS2481,
                                               _M0L5bytesS2479};
          _M0L4tlenS790 = _M0L11_2aparam__0S787;
          _M0L4restS791 = _M0L4_2axS890;
          goto join_789;
        }
      } else {
        moonbit_bytes_t _M0L8_2afieldS4156 = _M0L11_2aparam__1S788.$0;
        moonbit_bytes_t _M0L5bytesS3092 = _M0L8_2afieldS4156;
        int32_t _M0L5startS3093 = _M0L11_2aparam__1S788.$1;
        int32_t _M0L6_2atmpS4155 = _M0L5bytesS3092[_M0L5startS3093];
        int32_t _M0L4_2axS891 = _M0L6_2atmpS4155;
        if (_M0L4_2axS891 >= 0 && _M0L4_2axS891 <= 127) {
          moonbit_bytes_t _M0L8_2afieldS3966 = _M0L11_2aparam__1S788.$0;
          moonbit_bytes_t _M0L5bytesS3088 = _M0L8_2afieldS3966;
          int32_t _M0L5startS3091 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS3089 = _M0L5startS3091 + 1;
          int32_t _M0L8_2afieldS3965 = _M0L11_2aparam__1S788.$2;
          int32_t _M0L3endS3090 = _M0L8_2afieldS3965;
          struct _M0TPC15bytes9BytesView _M0L4_2axS892 =
            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3089,
                                               _M0L3endS3090,
                                               _M0L5bytesS3088};
          _M0L4tlenS827 = _M0L11_2aparam__0S787;
          _M0L4restS828 = _M0L4_2axS892;
          _M0L1bS829 = _M0L4_2axS891;
          goto join_826;
        } else {
          int32_t _M0L3endS2705 = _M0L11_2aparam__1S788.$2;
          int32_t _M0L5startS2706 = _M0L11_2aparam__1S788.$1;
          int32_t _M0L6_2atmpS2704 = _M0L3endS2705 - _M0L5startS2706;
          if (_M0L6_2atmpS2704 >= 2) {
            if (_M0L4_2axS891 >= 194 && _M0L4_2axS891 <= 223) {
              moonbit_bytes_t _M0L8_2afieldS3976 = _M0L11_2aparam__1S788.$0;
              moonbit_bytes_t _M0L5bytesS3081 = _M0L8_2afieldS3976;
              int32_t _M0L5startS3083 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS3082 = _M0L5startS3083 + 1;
              int32_t _M0L6_2atmpS3975 = _M0L5bytesS3081[_M0L6_2atmpS3082];
              int32_t _M0L4_2axS893 = _M0L6_2atmpS3975;
              if (_M0L4_2axS893 >= 128 && _M0L4_2axS893 <= 191) {
                moonbit_bytes_t _M0L8_2afieldS3968 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS3077 = _M0L8_2afieldS3968;
                int32_t _M0L5startS3080 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS3078 = _M0L5startS3080 + 2;
                int32_t _M0L8_2afieldS3967 = _M0L11_2aparam__1S788.$2;
                int32_t _M0L3endS3079 = _M0L8_2afieldS3967;
                struct _M0TPC15bytes9BytesView _M0L4_2axS894 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3078,
                                                     _M0L3endS3079,
                                                     _M0L5bytesS3077};
                _M0L4tlenS821 = _M0L11_2aparam__0S787;
                _M0L4restS822 = _M0L4_2axS894;
                _M0L2b0S823 = _M0L4_2axS891;
                _M0L2b1S824 = _M0L4_2axS893;
                goto join_820;
              } else {
                int32_t _M0L3endS3060 = _M0L11_2aparam__1S788.$2;
                int32_t _M0L5startS3061 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS3059 = _M0L3endS3060 - _M0L5startS3061;
                if (_M0L6_2atmpS3059 >= 3) {
                  int32_t _M0L3endS3063 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L5startS3064 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3062 = _M0L3endS3063 - _M0L5startS3064;
                  if (_M0L6_2atmpS3062 >= 4) {
                    moonbit_bytes_t _M0L8_2afieldS3970 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS3065 = _M0L8_2afieldS3970;
                    int32_t _M0L5startS3068 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS3066 = _M0L5startS3068 + 1;
                    int32_t _M0L8_2afieldS3969 = _M0L11_2aparam__1S788.$2;
                    int32_t _M0L3endS3067 = _M0L8_2afieldS3969;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS895 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3066,
                                                         _M0L3endS3067,
                                                         _M0L5bytesS3065};
                    _M0L4tlenS790 = _M0L11_2aparam__0S787;
                    _M0L4restS791 = _M0L4_2axS895;
                    goto join_789;
                  } else {
                    moonbit_bytes_t _M0L8_2afieldS3972 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS3069 = _M0L8_2afieldS3972;
                    int32_t _M0L5startS3072 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS3070 = _M0L5startS3072 + 1;
                    int32_t _M0L8_2afieldS3971 = _M0L11_2aparam__1S788.$2;
                    int32_t _M0L3endS3071 = _M0L8_2afieldS3971;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS896 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3070,
                                                         _M0L3endS3071,
                                                         _M0L5bytesS3069};
                    _M0L4tlenS790 = _M0L11_2aparam__0S787;
                    _M0L4restS791 = _M0L4_2axS896;
                    goto join_789;
                  }
                } else {
                  moonbit_bytes_t _M0L8_2afieldS3974 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS3073 = _M0L8_2afieldS3974;
                  int32_t _M0L5startS3076 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3074 = _M0L5startS3076 + 1;
                  int32_t _M0L8_2afieldS3973 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS3075 = _M0L8_2afieldS3973;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS897 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3074,
                                                       _M0L3endS3075,
                                                       _M0L5bytesS3073};
                  _M0L4tlenS790 = _M0L11_2aparam__0S787;
                  _M0L4restS791 = _M0L4_2axS897;
                  goto join_789;
                }
              }
            } else {
              int32_t _M0L3endS2708 = _M0L11_2aparam__1S788.$2;
              int32_t _M0L5startS2709 = _M0L11_2aparam__1S788.$1;
              int32_t _M0L6_2atmpS2707 = _M0L3endS2708 - _M0L5startS2709;
              if (_M0L6_2atmpS2707 >= 3) {
                if (_M0L4_2axS891 == 224) {
                  moonbit_bytes_t _M0L8_2afieldS3990 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS2975 = _M0L8_2afieldS3990;
                  int32_t _M0L5startS2977 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS2976 = _M0L5startS2977 + 1;
                  int32_t _M0L6_2atmpS3989 =
                    _M0L5bytesS2975[_M0L6_2atmpS2976];
                  int32_t _M0L4_2axS898 = _M0L6_2atmpS3989;
                  if (_M0L4_2axS898 >= 160 && _M0L4_2axS898 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS3984 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS2972 = _M0L8_2afieldS3984;
                    int32_t _M0L5startS2974 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2973 = _M0L5startS2974 + 2;
                    int32_t _M0L6_2atmpS3983 =
                      _M0L5bytesS2972[_M0L6_2atmpS2973];
                    int32_t _M0L4_2axS899 = _M0L6_2atmpS3983;
                    if (_M0L4_2axS899 >= 128 && _M0L4_2axS899 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3978 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2968 = _M0L8_2afieldS3978;
                      int32_t _M0L5startS2971 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2969 = _M0L5startS2971 + 3;
                      int32_t _M0L8_2afieldS3977 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2970 = _M0L8_2afieldS3977;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS900 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2969,
                                                           _M0L3endS2970,
                                                           _M0L5bytesS2968};
                      _M0L4tlenS814 = _M0L11_2aparam__0S787;
                      _M0L2b0S815 = _M0L4_2axS891;
                      _M0L2b1S816 = _M0L4_2axS898;
                      _M0L2b2S817 = _M0L4_2axS899;
                      _M0L4restS818 = _M0L4_2axS900;
                      goto join_813;
                    } else {
                      int32_t _M0L3endS2958 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L5startS2959 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2957 =
                        _M0L3endS2958 - _M0L5startS2959;
                      if (_M0L6_2atmpS2957 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS3980 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2960 = _M0L8_2afieldS3980;
                        int32_t _M0L5startS2963 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2961 = _M0L5startS2963 + 2;
                        int32_t _M0L8_2afieldS3979 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2962 = _M0L8_2afieldS3979;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS901 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2961,
                                                             _M0L3endS2962,
                                                             _M0L5bytesS2960};
                        _M0L4restS800 = _M0L4_2axS901;
                        _M0L4tlenS801 = _M0L11_2aparam__0S787;
                        goto join_799;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3982 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2964 = _M0L8_2afieldS3982;
                        int32_t _M0L5startS2967 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2965 = _M0L5startS2967 + 2;
                        int32_t _M0L8_2afieldS3981 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2966 = _M0L8_2afieldS3981;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS902 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2965,
                                                             _M0L3endS2966,
                                                             _M0L5bytesS2964};
                        _M0L4restS800 = _M0L4_2axS902;
                        _M0L4tlenS801 = _M0L11_2aparam__0S787;
                        goto join_799;
                      }
                    }
                  } else {
                    int32_t _M0L3endS2947 = _M0L11_2aparam__1S788.$2;
                    int32_t _M0L5startS2948 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2946 =
                      _M0L3endS2947 - _M0L5startS2948;
                    if (_M0L6_2atmpS2946 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS3986 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2949 = _M0L8_2afieldS3986;
                      int32_t _M0L5startS2952 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2950 = _M0L5startS2952 + 1;
                      int32_t _M0L8_2afieldS3985 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2951 = _M0L8_2afieldS3985;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS903 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2950,
                                                           _M0L3endS2951,
                                                           _M0L5bytesS2949};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS903;
                      goto join_789;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS3988 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2953 = _M0L8_2afieldS3988;
                      int32_t _M0L5startS2956 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2954 = _M0L5startS2956 + 1;
                      int32_t _M0L8_2afieldS3987 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2955 = _M0L8_2afieldS3987;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS904 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2954,
                                                           _M0L3endS2955,
                                                           _M0L5bytesS2953};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS904;
                      goto join_789;
                    }
                  }
                } else if (_M0L4_2axS891 >= 225 && _M0L4_2axS891 <= 236) {
                  moonbit_bytes_t _M0L8_2afieldS4004 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS2943 = _M0L8_2afieldS4004;
                  int32_t _M0L5startS2945 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS2944 = _M0L5startS2945 + 1;
                  int32_t _M0L6_2atmpS4003 =
                    _M0L5bytesS2943[_M0L6_2atmpS2944];
                  int32_t _M0L4_2axS905 = _M0L6_2atmpS4003;
                  if (_M0L4_2axS905 >= 128 && _M0L4_2axS905 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS3998 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS2940 = _M0L8_2afieldS3998;
                    int32_t _M0L5startS2942 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2941 = _M0L5startS2942 + 2;
                    int32_t _M0L6_2atmpS3997 =
                      _M0L5bytesS2940[_M0L6_2atmpS2941];
                    int32_t _M0L4_2axS906 = _M0L6_2atmpS3997;
                    if (_M0L4_2axS906 >= 128 && _M0L4_2axS906 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS3992 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2936 = _M0L8_2afieldS3992;
                      int32_t _M0L5startS2939 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2937 = _M0L5startS2939 + 3;
                      int32_t _M0L8_2afieldS3991 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2938 = _M0L8_2afieldS3991;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS907 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2937,
                                                           _M0L3endS2938,
                                                           _M0L5bytesS2936};
                      _M0L4tlenS814 = _M0L11_2aparam__0S787;
                      _M0L2b0S815 = _M0L4_2axS891;
                      _M0L2b1S816 = _M0L4_2axS905;
                      _M0L2b2S817 = _M0L4_2axS906;
                      _M0L4restS818 = _M0L4_2axS907;
                      goto join_813;
                    } else {
                      int32_t _M0L3endS2926 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L5startS2927 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2925 =
                        _M0L3endS2926 - _M0L5startS2927;
                      if (_M0L6_2atmpS2925 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS3994 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2928 = _M0L8_2afieldS3994;
                        int32_t _M0L5startS2931 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2929 = _M0L5startS2931 + 2;
                        int32_t _M0L8_2afieldS3993 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2930 = _M0L8_2afieldS3993;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS908 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2929,
                                                             _M0L3endS2930,
                                                             _M0L5bytesS2928};
                        _M0L4restS800 = _M0L4_2axS908;
                        _M0L4tlenS801 = _M0L11_2aparam__0S787;
                        goto join_799;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS3996 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2932 = _M0L8_2afieldS3996;
                        int32_t _M0L5startS2935 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2933 = _M0L5startS2935 + 2;
                        int32_t _M0L8_2afieldS3995 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2934 = _M0L8_2afieldS3995;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS909 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2933,
                                                             _M0L3endS2934,
                                                             _M0L5bytesS2932};
                        _M0L4restS800 = _M0L4_2axS909;
                        _M0L4tlenS801 = _M0L11_2aparam__0S787;
                        goto join_799;
                      }
                    }
                  } else {
                    int32_t _M0L3endS2915 = _M0L11_2aparam__1S788.$2;
                    int32_t _M0L5startS2916 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2914 =
                      _M0L3endS2915 - _M0L5startS2916;
                    if (_M0L6_2atmpS2914 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS4000 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2917 = _M0L8_2afieldS4000;
                      int32_t _M0L5startS2920 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2918 = _M0L5startS2920 + 1;
                      int32_t _M0L8_2afieldS3999 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2919 = _M0L8_2afieldS3999;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS910 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2918,
                                                           _M0L3endS2919,
                                                           _M0L5bytesS2917};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS910;
                      goto join_789;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS4002 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2921 = _M0L8_2afieldS4002;
                      int32_t _M0L5startS2924 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2922 = _M0L5startS2924 + 1;
                      int32_t _M0L8_2afieldS4001 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2923 = _M0L8_2afieldS4001;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS911 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2922,
                                                           _M0L3endS2923,
                                                           _M0L5bytesS2921};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS911;
                      goto join_789;
                    }
                  }
                } else if (_M0L4_2axS891 == 237) {
                  moonbit_bytes_t _M0L8_2afieldS4018 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS2911 = _M0L8_2afieldS4018;
                  int32_t _M0L5startS2913 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS2912 = _M0L5startS2913 + 1;
                  int32_t _M0L6_2atmpS4017 =
                    _M0L5bytesS2911[_M0L6_2atmpS2912];
                  int32_t _M0L4_2axS912 = _M0L6_2atmpS4017;
                  if (_M0L4_2axS912 >= 128 && _M0L4_2axS912 <= 159) {
                    moonbit_bytes_t _M0L8_2afieldS4012 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS2908 = _M0L8_2afieldS4012;
                    int32_t _M0L5startS2910 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2909 = _M0L5startS2910 + 2;
                    int32_t _M0L6_2atmpS4011 =
                      _M0L5bytesS2908[_M0L6_2atmpS2909];
                    int32_t _M0L4_2axS913 = _M0L6_2atmpS4011;
                    if (_M0L4_2axS913 >= 128 && _M0L4_2axS913 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS4006 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2904 = _M0L8_2afieldS4006;
                      int32_t _M0L5startS2907 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2905 = _M0L5startS2907 + 3;
                      int32_t _M0L8_2afieldS4005 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2906 = _M0L8_2afieldS4005;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS914 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2905,
                                                           _M0L3endS2906,
                                                           _M0L5bytesS2904};
                      _M0L4tlenS814 = _M0L11_2aparam__0S787;
                      _M0L2b0S815 = _M0L4_2axS891;
                      _M0L2b1S816 = _M0L4_2axS912;
                      _M0L2b2S817 = _M0L4_2axS913;
                      _M0L4restS818 = _M0L4_2axS914;
                      goto join_813;
                    } else {
                      int32_t _M0L3endS2894 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L5startS2895 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2893 =
                        _M0L3endS2894 - _M0L5startS2895;
                      if (_M0L6_2atmpS2893 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS4008 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2896 = _M0L8_2afieldS4008;
                        int32_t _M0L5startS2899 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2897 = _M0L5startS2899 + 2;
                        int32_t _M0L8_2afieldS4007 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2898 = _M0L8_2afieldS4007;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS915 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2897,
                                                             _M0L3endS2898,
                                                             _M0L5bytesS2896};
                        _M0L4restS800 = _M0L4_2axS915;
                        _M0L4tlenS801 = _M0L11_2aparam__0S787;
                        goto join_799;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS4010 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2900 = _M0L8_2afieldS4010;
                        int32_t _M0L5startS2903 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2901 = _M0L5startS2903 + 2;
                        int32_t _M0L8_2afieldS4009 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2902 = _M0L8_2afieldS4009;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS916 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2901,
                                                             _M0L3endS2902,
                                                             _M0L5bytesS2900};
                        _M0L4restS800 = _M0L4_2axS916;
                        _M0L4tlenS801 = _M0L11_2aparam__0S787;
                        goto join_799;
                      }
                    }
                  } else {
                    int32_t _M0L3endS2883 = _M0L11_2aparam__1S788.$2;
                    int32_t _M0L5startS2884 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2882 =
                      _M0L3endS2883 - _M0L5startS2884;
                    if (_M0L6_2atmpS2882 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS4014 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2885 = _M0L8_2afieldS4014;
                      int32_t _M0L5startS2888 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2886 = _M0L5startS2888 + 1;
                      int32_t _M0L8_2afieldS4013 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2887 = _M0L8_2afieldS4013;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS917 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2886,
                                                           _M0L3endS2887,
                                                           _M0L5bytesS2885};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS917;
                      goto join_789;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS4016 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2889 = _M0L8_2afieldS4016;
                      int32_t _M0L5startS2892 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2890 = _M0L5startS2892 + 1;
                      int32_t _M0L8_2afieldS4015 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2891 = _M0L8_2afieldS4015;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS918 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2890,
                                                           _M0L3endS2891,
                                                           _M0L5bytesS2889};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS918;
                      goto join_789;
                    }
                  }
                } else if (_M0L4_2axS891 >= 238 && _M0L4_2axS891 <= 239) {
                  moonbit_bytes_t _M0L8_2afieldS4032 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS2879 = _M0L8_2afieldS4032;
                  int32_t _M0L5startS2881 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS2880 = _M0L5startS2881 + 1;
                  int32_t _M0L6_2atmpS4031 =
                    _M0L5bytesS2879[_M0L6_2atmpS2880];
                  int32_t _M0L4_2axS919 = _M0L6_2atmpS4031;
                  if (_M0L4_2axS919 >= 128 && _M0L4_2axS919 <= 191) {
                    moonbit_bytes_t _M0L8_2afieldS4026 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS2876 = _M0L8_2afieldS4026;
                    int32_t _M0L5startS2878 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2877 = _M0L5startS2878 + 2;
                    int32_t _M0L6_2atmpS4025 =
                      _M0L5bytesS2876[_M0L6_2atmpS2877];
                    int32_t _M0L4_2axS920 = _M0L6_2atmpS4025;
                    if (_M0L4_2axS920 >= 128 && _M0L4_2axS920 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS4020 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2872 = _M0L8_2afieldS4020;
                      int32_t _M0L5startS2875 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2873 = _M0L5startS2875 + 3;
                      int32_t _M0L8_2afieldS4019 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2874 = _M0L8_2afieldS4019;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS921 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2873,
                                                           _M0L3endS2874,
                                                           _M0L5bytesS2872};
                      _M0L4tlenS814 = _M0L11_2aparam__0S787;
                      _M0L2b0S815 = _M0L4_2axS891;
                      _M0L2b1S816 = _M0L4_2axS919;
                      _M0L2b2S817 = _M0L4_2axS920;
                      _M0L4restS818 = _M0L4_2axS921;
                      goto join_813;
                    } else {
                      int32_t _M0L3endS2862 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L5startS2863 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2861 =
                        _M0L3endS2862 - _M0L5startS2863;
                      if (_M0L6_2atmpS2861 >= 4) {
                        moonbit_bytes_t _M0L8_2afieldS4022 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2864 = _M0L8_2afieldS4022;
                        int32_t _M0L5startS2867 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2865 = _M0L5startS2867 + 2;
                        int32_t _M0L8_2afieldS4021 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2866 = _M0L8_2afieldS4021;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS922 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2865,
                                                             _M0L3endS2866,
                                                             _M0L5bytesS2864};
                        _M0L4restS800 = _M0L4_2axS922;
                        _M0L4tlenS801 = _M0L11_2aparam__0S787;
                        goto join_799;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS4024 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2868 = _M0L8_2afieldS4024;
                        int32_t _M0L5startS2871 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2869 = _M0L5startS2871 + 2;
                        int32_t _M0L8_2afieldS4023 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2870 = _M0L8_2afieldS4023;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS923 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2869,
                                                             _M0L3endS2870,
                                                             _M0L5bytesS2868};
                        _M0L4restS800 = _M0L4_2axS923;
                        _M0L4tlenS801 = _M0L11_2aparam__0S787;
                        goto join_799;
                      }
                    }
                  } else {
                    int32_t _M0L3endS2851 = _M0L11_2aparam__1S788.$2;
                    int32_t _M0L5startS2852 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2850 =
                      _M0L3endS2851 - _M0L5startS2852;
                    if (_M0L6_2atmpS2850 >= 4) {
                      moonbit_bytes_t _M0L8_2afieldS4028 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2853 = _M0L8_2afieldS4028;
                      int32_t _M0L5startS2856 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2854 = _M0L5startS2856 + 1;
                      int32_t _M0L8_2afieldS4027 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2855 = _M0L8_2afieldS4027;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS924 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2854,
                                                           _M0L3endS2855,
                                                           _M0L5bytesS2853};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS924;
                      goto join_789;
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS4030 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2857 = _M0L8_2afieldS4030;
                      int32_t _M0L5startS2860 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2858 = _M0L5startS2860 + 1;
                      int32_t _M0L8_2afieldS4029 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2859 = _M0L8_2afieldS4029;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS925 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2858,
                                                           _M0L3endS2859,
                                                           _M0L5bytesS2857};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS925;
                      goto join_789;
                    }
                  }
                } else {
                  int32_t _M0L3endS2711 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L5startS2712 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS2710 = _M0L3endS2711 - _M0L5startS2712;
                  if (_M0L6_2atmpS2710 >= 4) {
                    if (_M0L4_2axS891 == 240) {
                      moonbit_bytes_t _M0L8_2afieldS4046 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2789 = _M0L8_2afieldS4046;
                      int32_t _M0L5startS2791 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2790 = _M0L5startS2791 + 1;
                      int32_t _M0L6_2atmpS4045 =
                        _M0L5bytesS2789[_M0L6_2atmpS2790];
                      int32_t _M0L4_2axS926 = _M0L6_2atmpS4045;
                      if (_M0L4_2axS926 >= 144 && _M0L4_2axS926 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS4042 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2786 = _M0L8_2afieldS4042;
                        int32_t _M0L5startS2788 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2787 = _M0L5startS2788 + 2;
                        int32_t _M0L6_2atmpS4041 =
                          _M0L5bytesS2786[_M0L6_2atmpS2787];
                        int32_t _M0L4_2axS927 = _M0L6_2atmpS4041;
                        if (_M0L4_2axS927 >= 128 && _M0L4_2axS927 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS4038 =
                            _M0L11_2aparam__1S788.$0;
                          moonbit_bytes_t _M0L5bytesS2783 =
                            _M0L8_2afieldS4038;
                          int32_t _M0L5startS2785 = _M0L11_2aparam__1S788.$1;
                          int32_t _M0L6_2atmpS2784 = _M0L5startS2785 + 3;
                          int32_t _M0L6_2atmpS4037 =
                            _M0L5bytesS2783[_M0L6_2atmpS2784];
                          int32_t _M0L4_2axS928 = _M0L6_2atmpS4037;
                          if (_M0L4_2axS928 >= 128 && _M0L4_2axS928 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS4034 =
                              _M0L11_2aparam__1S788.$0;
                            moonbit_bytes_t _M0L5bytesS2779 =
                              _M0L8_2afieldS4034;
                            int32_t _M0L5startS2782 =
                              _M0L11_2aparam__1S788.$1;
                            int32_t _M0L6_2atmpS2780 = _M0L5startS2782 + 4;
                            int32_t _M0L8_2afieldS4033 =
                              _M0L11_2aparam__1S788.$2;
                            int32_t _M0L3endS2781 = _M0L8_2afieldS4033;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS929 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2780,
                                                                 _M0L3endS2781,
                                                                 _M0L5bytesS2779};
                            _M0L4tlenS803 = _M0L11_2aparam__0S787;
                            _M0L2b0S804 = _M0L4_2axS891;
                            _M0L2b1S805 = _M0L4_2axS926;
                            _M0L2b2S806 = _M0L4_2axS927;
                            _M0L2b3S807 = _M0L4_2axS928;
                            _M0L4restS808 = _M0L4_2axS929;
                            goto join_802;
                          } else {
                            moonbit_bytes_t _M0L8_2afieldS4036 =
                              _M0L11_2aparam__1S788.$0;
                            moonbit_bytes_t _M0L5bytesS2775 =
                              _M0L8_2afieldS4036;
                            int32_t _M0L5startS2778 =
                              _M0L11_2aparam__1S788.$1;
                            int32_t _M0L6_2atmpS2776 = _M0L5startS2778 + 3;
                            int32_t _M0L8_2afieldS4035 =
                              _M0L11_2aparam__1S788.$2;
                            int32_t _M0L3endS2777 = _M0L8_2afieldS4035;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS930 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2776,
                                                                 _M0L3endS2777,
                                                                 _M0L5bytesS2775};
                            _M0L4restS797 = _M0L4_2axS930;
                            _M0L4tlenS798 = _M0L11_2aparam__0S787;
                            goto join_796;
                          }
                        } else {
                          moonbit_bytes_t _M0L8_2afieldS4040 =
                            _M0L11_2aparam__1S788.$0;
                          moonbit_bytes_t _M0L5bytesS2771 =
                            _M0L8_2afieldS4040;
                          int32_t _M0L5startS2774 = _M0L11_2aparam__1S788.$1;
                          int32_t _M0L6_2atmpS2772 = _M0L5startS2774 + 2;
                          int32_t _M0L8_2afieldS4039 =
                            _M0L11_2aparam__1S788.$2;
                          int32_t _M0L3endS2773 = _M0L8_2afieldS4039;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS931 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2772,
                                                               _M0L3endS2773,
                                                               _M0L5bytesS2771};
                          _M0L4restS794 = _M0L4_2axS931;
                          _M0L4tlenS795 = _M0L11_2aparam__0S787;
                          goto join_793;
                        }
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS4044 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2767 = _M0L8_2afieldS4044;
                        int32_t _M0L5startS2770 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2768 = _M0L5startS2770 + 1;
                        int32_t _M0L8_2afieldS4043 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2769 = _M0L8_2afieldS4043;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS932 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2768,
                                                             _M0L3endS2769,
                                                             _M0L5bytesS2767};
                        _M0L4tlenS790 = _M0L11_2aparam__0S787;
                        _M0L4restS791 = _M0L4_2axS932;
                        goto join_789;
                      }
                    } else if (_M0L4_2axS891 >= 241 && _M0L4_2axS891 <= 243) {
                      moonbit_bytes_t _M0L8_2afieldS4060 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2764 = _M0L8_2afieldS4060;
                      int32_t _M0L5startS2766 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2765 = _M0L5startS2766 + 1;
                      int32_t _M0L6_2atmpS4059 =
                        _M0L5bytesS2764[_M0L6_2atmpS2765];
                      int32_t _M0L4_2axS933 = _M0L6_2atmpS4059;
                      if (_M0L4_2axS933 >= 128 && _M0L4_2axS933 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS4056 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2761 = _M0L8_2afieldS4056;
                        int32_t _M0L5startS2763 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2762 = _M0L5startS2763 + 2;
                        int32_t _M0L6_2atmpS4055 =
                          _M0L5bytesS2761[_M0L6_2atmpS2762];
                        int32_t _M0L4_2axS934 = _M0L6_2atmpS4055;
                        if (_M0L4_2axS934 >= 128 && _M0L4_2axS934 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS4052 =
                            _M0L11_2aparam__1S788.$0;
                          moonbit_bytes_t _M0L5bytesS2758 =
                            _M0L8_2afieldS4052;
                          int32_t _M0L5startS2760 = _M0L11_2aparam__1S788.$1;
                          int32_t _M0L6_2atmpS2759 = _M0L5startS2760 + 3;
                          int32_t _M0L6_2atmpS4051 =
                            _M0L5bytesS2758[_M0L6_2atmpS2759];
                          int32_t _M0L4_2axS935 = _M0L6_2atmpS4051;
                          if (_M0L4_2axS935 >= 128 && _M0L4_2axS935 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS4048 =
                              _M0L11_2aparam__1S788.$0;
                            moonbit_bytes_t _M0L5bytesS2754 =
                              _M0L8_2afieldS4048;
                            int32_t _M0L5startS2757 =
                              _M0L11_2aparam__1S788.$1;
                            int32_t _M0L6_2atmpS2755 = _M0L5startS2757 + 4;
                            int32_t _M0L8_2afieldS4047 =
                              _M0L11_2aparam__1S788.$2;
                            int32_t _M0L3endS2756 = _M0L8_2afieldS4047;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS936 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2755,
                                                                 _M0L3endS2756,
                                                                 _M0L5bytesS2754};
                            _M0L4tlenS803 = _M0L11_2aparam__0S787;
                            _M0L2b0S804 = _M0L4_2axS891;
                            _M0L2b1S805 = _M0L4_2axS933;
                            _M0L2b2S806 = _M0L4_2axS934;
                            _M0L2b3S807 = _M0L4_2axS935;
                            _M0L4restS808 = _M0L4_2axS936;
                            goto join_802;
                          } else {
                            moonbit_bytes_t _M0L8_2afieldS4050 =
                              _M0L11_2aparam__1S788.$0;
                            moonbit_bytes_t _M0L5bytesS2750 =
                              _M0L8_2afieldS4050;
                            int32_t _M0L5startS2753 =
                              _M0L11_2aparam__1S788.$1;
                            int32_t _M0L6_2atmpS2751 = _M0L5startS2753 + 3;
                            int32_t _M0L8_2afieldS4049 =
                              _M0L11_2aparam__1S788.$2;
                            int32_t _M0L3endS2752 = _M0L8_2afieldS4049;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS937 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2751,
                                                                 _M0L3endS2752,
                                                                 _M0L5bytesS2750};
                            _M0L4restS797 = _M0L4_2axS937;
                            _M0L4tlenS798 = _M0L11_2aparam__0S787;
                            goto join_796;
                          }
                        } else {
                          moonbit_bytes_t _M0L8_2afieldS4054 =
                            _M0L11_2aparam__1S788.$0;
                          moonbit_bytes_t _M0L5bytesS2746 =
                            _M0L8_2afieldS4054;
                          int32_t _M0L5startS2749 = _M0L11_2aparam__1S788.$1;
                          int32_t _M0L6_2atmpS2747 = _M0L5startS2749 + 2;
                          int32_t _M0L8_2afieldS4053 =
                            _M0L11_2aparam__1S788.$2;
                          int32_t _M0L3endS2748 = _M0L8_2afieldS4053;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS938 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2747,
                                                               _M0L3endS2748,
                                                               _M0L5bytesS2746};
                          _M0L4restS794 = _M0L4_2axS938;
                          _M0L4tlenS795 = _M0L11_2aparam__0S787;
                          goto join_793;
                        }
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS4058 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2742 = _M0L8_2afieldS4058;
                        int32_t _M0L5startS2745 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2743 = _M0L5startS2745 + 1;
                        int32_t _M0L8_2afieldS4057 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2744 = _M0L8_2afieldS4057;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS939 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2743,
                                                             _M0L3endS2744,
                                                             _M0L5bytesS2742};
                        _M0L4tlenS790 = _M0L11_2aparam__0S787;
                        _M0L4restS791 = _M0L4_2axS939;
                        goto join_789;
                      }
                    } else if (_M0L4_2axS891 == 244) {
                      moonbit_bytes_t _M0L8_2afieldS4074 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2739 = _M0L8_2afieldS4074;
                      int32_t _M0L5startS2741 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2740 = _M0L5startS2741 + 1;
                      int32_t _M0L6_2atmpS4073 =
                        _M0L5bytesS2739[_M0L6_2atmpS2740];
                      int32_t _M0L4_2axS940 = _M0L6_2atmpS4073;
                      if (_M0L4_2axS940 >= 128 && _M0L4_2axS940 <= 143) {
                        moonbit_bytes_t _M0L8_2afieldS4070 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2736 = _M0L8_2afieldS4070;
                        int32_t _M0L5startS2738 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2737 = _M0L5startS2738 + 2;
                        int32_t _M0L6_2atmpS4069 =
                          _M0L5bytesS2736[_M0L6_2atmpS2737];
                        int32_t _M0L4_2axS941 = _M0L6_2atmpS4069;
                        if (_M0L4_2axS941 >= 128 && _M0L4_2axS941 <= 191) {
                          moonbit_bytes_t _M0L8_2afieldS4066 =
                            _M0L11_2aparam__1S788.$0;
                          moonbit_bytes_t _M0L5bytesS2733 =
                            _M0L8_2afieldS4066;
                          int32_t _M0L5startS2735 = _M0L11_2aparam__1S788.$1;
                          int32_t _M0L6_2atmpS2734 = _M0L5startS2735 + 3;
                          int32_t _M0L6_2atmpS4065 =
                            _M0L5bytesS2733[_M0L6_2atmpS2734];
                          int32_t _M0L4_2axS942 = _M0L6_2atmpS4065;
                          if (_M0L4_2axS942 >= 128 && _M0L4_2axS942 <= 191) {
                            moonbit_bytes_t _M0L8_2afieldS4062 =
                              _M0L11_2aparam__1S788.$0;
                            moonbit_bytes_t _M0L5bytesS2729 =
                              _M0L8_2afieldS4062;
                            int32_t _M0L5startS2732 =
                              _M0L11_2aparam__1S788.$1;
                            int32_t _M0L6_2atmpS2730 = _M0L5startS2732 + 4;
                            int32_t _M0L8_2afieldS4061 =
                              _M0L11_2aparam__1S788.$2;
                            int32_t _M0L3endS2731 = _M0L8_2afieldS4061;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS943 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2730,
                                                                 _M0L3endS2731,
                                                                 _M0L5bytesS2729};
                            _M0L4tlenS803 = _M0L11_2aparam__0S787;
                            _M0L2b0S804 = _M0L4_2axS891;
                            _M0L2b1S805 = _M0L4_2axS940;
                            _M0L2b2S806 = _M0L4_2axS941;
                            _M0L2b3S807 = _M0L4_2axS942;
                            _M0L4restS808 = _M0L4_2axS943;
                            goto join_802;
                          } else {
                            moonbit_bytes_t _M0L8_2afieldS4064 =
                              _M0L11_2aparam__1S788.$0;
                            moonbit_bytes_t _M0L5bytesS2725 =
                              _M0L8_2afieldS4064;
                            int32_t _M0L5startS2728 =
                              _M0L11_2aparam__1S788.$1;
                            int32_t _M0L6_2atmpS2726 = _M0L5startS2728 + 3;
                            int32_t _M0L8_2afieldS4063 =
                              _M0L11_2aparam__1S788.$2;
                            int32_t _M0L3endS2727 = _M0L8_2afieldS4063;
                            struct _M0TPC15bytes9BytesView _M0L4_2axS944 =
                              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2726,
                                                                 _M0L3endS2727,
                                                                 _M0L5bytesS2725};
                            _M0L4restS797 = _M0L4_2axS944;
                            _M0L4tlenS798 = _M0L11_2aparam__0S787;
                            goto join_796;
                          }
                        } else {
                          moonbit_bytes_t _M0L8_2afieldS4068 =
                            _M0L11_2aparam__1S788.$0;
                          moonbit_bytes_t _M0L5bytesS2721 =
                            _M0L8_2afieldS4068;
                          int32_t _M0L5startS2724 = _M0L11_2aparam__1S788.$1;
                          int32_t _M0L6_2atmpS2722 = _M0L5startS2724 + 2;
                          int32_t _M0L8_2afieldS4067 =
                            _M0L11_2aparam__1S788.$2;
                          int32_t _M0L3endS2723 = _M0L8_2afieldS4067;
                          struct _M0TPC15bytes9BytesView _M0L4_2axS945 =
                            (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2722,
                                                               _M0L3endS2723,
                                                               _M0L5bytesS2721};
                          _M0L4restS794 = _M0L4_2axS945;
                          _M0L4tlenS795 = _M0L11_2aparam__0S787;
                          goto join_793;
                        }
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS4072 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2717 = _M0L8_2afieldS4072;
                        int32_t _M0L5startS2720 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2718 = _M0L5startS2720 + 1;
                        int32_t _M0L8_2afieldS4071 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2719 = _M0L8_2afieldS4071;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS946 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2718,
                                                             _M0L3endS2719,
                                                             _M0L5bytesS2717};
                        _M0L4tlenS790 = _M0L11_2aparam__0S787;
                        _M0L4restS791 = _M0L4_2axS946;
                        goto join_789;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS4076 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2713 = _M0L8_2afieldS4076;
                      int32_t _M0L5startS2716 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2714 = _M0L5startS2716 + 1;
                      int32_t _M0L8_2afieldS4075 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2715 = _M0L8_2afieldS4075;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS947 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2714,
                                                           _M0L3endS2715,
                                                           _M0L5bytesS2713};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS947;
                      goto join_789;
                    }
                  } else if (_M0L4_2axS891 == 240) {
                    moonbit_bytes_t _M0L8_2afieldS4086 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS2847 = _M0L8_2afieldS4086;
                    int32_t _M0L5startS2849 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2848 = _M0L5startS2849 + 1;
                    int32_t _M0L6_2atmpS4085 =
                      _M0L5bytesS2847[_M0L6_2atmpS2848];
                    int32_t _M0L4_2axS948 = _M0L6_2atmpS4085;
                    if (_M0L4_2axS948 >= 144 && _M0L4_2axS948 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS4082 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2844 = _M0L8_2afieldS4082;
                      int32_t _M0L5startS2846 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2845 = _M0L5startS2846 + 2;
                      int32_t _M0L6_2atmpS4081 =
                        _M0L5bytesS2844[_M0L6_2atmpS2845];
                      int32_t _M0L4_2axS949 = _M0L6_2atmpS4081;
                      if (_M0L4_2axS949 >= 128 && _M0L4_2axS949 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS4078 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2840 = _M0L8_2afieldS4078;
                        int32_t _M0L5startS2843 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2841 = _M0L5startS2843 + 3;
                        int32_t _M0L8_2afieldS4077 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2842 = _M0L8_2afieldS4077;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS950 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2841,
                                                             _M0L3endS2842,
                                                             _M0L5bytesS2840};
                        _M0L4restS797 = _M0L4_2axS950;
                        _M0L4tlenS798 = _M0L11_2aparam__0S787;
                        goto join_796;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS4080 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2836 = _M0L8_2afieldS4080;
                        int32_t _M0L5startS2839 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2837 = _M0L5startS2839 + 2;
                        int32_t _M0L8_2afieldS4079 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2838 = _M0L8_2afieldS4079;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS951 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2837,
                                                             _M0L3endS2838,
                                                             _M0L5bytesS2836};
                        _M0L4restS794 = _M0L4_2axS951;
                        _M0L4tlenS795 = _M0L11_2aparam__0S787;
                        goto join_793;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS4084 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2832 = _M0L8_2afieldS4084;
                      int32_t _M0L5startS2835 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2833 = _M0L5startS2835 + 1;
                      int32_t _M0L8_2afieldS4083 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2834 = _M0L8_2afieldS4083;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS952 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2833,
                                                           _M0L3endS2834,
                                                           _M0L5bytesS2832};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS952;
                      goto join_789;
                    }
                  } else if (_M0L4_2axS891 >= 241 && _M0L4_2axS891 <= 243) {
                    moonbit_bytes_t _M0L8_2afieldS4096 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS2829 = _M0L8_2afieldS4096;
                    int32_t _M0L5startS2831 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2830 = _M0L5startS2831 + 1;
                    int32_t _M0L6_2atmpS4095 =
                      _M0L5bytesS2829[_M0L6_2atmpS2830];
                    int32_t _M0L4_2axS953 = _M0L6_2atmpS4095;
                    if (_M0L4_2axS953 >= 128 && _M0L4_2axS953 <= 191) {
                      moonbit_bytes_t _M0L8_2afieldS4092 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2826 = _M0L8_2afieldS4092;
                      int32_t _M0L5startS2828 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2827 = _M0L5startS2828 + 2;
                      int32_t _M0L6_2atmpS4091 =
                        _M0L5bytesS2826[_M0L6_2atmpS2827];
                      int32_t _M0L4_2axS954 = _M0L6_2atmpS4091;
                      if (_M0L4_2axS954 >= 128 && _M0L4_2axS954 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS4088 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2822 = _M0L8_2afieldS4088;
                        int32_t _M0L5startS2825 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2823 = _M0L5startS2825 + 3;
                        int32_t _M0L8_2afieldS4087 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2824 = _M0L8_2afieldS4087;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS955 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2823,
                                                             _M0L3endS2824,
                                                             _M0L5bytesS2822};
                        _M0L4restS797 = _M0L4_2axS955;
                        _M0L4tlenS798 = _M0L11_2aparam__0S787;
                        goto join_796;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS4090 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2818 = _M0L8_2afieldS4090;
                        int32_t _M0L5startS2821 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2819 = _M0L5startS2821 + 2;
                        int32_t _M0L8_2afieldS4089 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2820 = _M0L8_2afieldS4089;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS956 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2819,
                                                             _M0L3endS2820,
                                                             _M0L5bytesS2818};
                        _M0L4restS794 = _M0L4_2axS956;
                        _M0L4tlenS795 = _M0L11_2aparam__0S787;
                        goto join_793;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS4094 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2814 = _M0L8_2afieldS4094;
                      int32_t _M0L5startS2817 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2815 = _M0L5startS2817 + 1;
                      int32_t _M0L8_2afieldS4093 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2816 = _M0L8_2afieldS4093;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS957 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2815,
                                                           _M0L3endS2816,
                                                           _M0L5bytesS2814};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS957;
                      goto join_789;
                    }
                  } else if (_M0L4_2axS891 == 244) {
                    moonbit_bytes_t _M0L8_2afieldS4106 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS2811 = _M0L8_2afieldS4106;
                    int32_t _M0L5startS2813 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2812 = _M0L5startS2813 + 1;
                    int32_t _M0L6_2atmpS4105 =
                      _M0L5bytesS2811[_M0L6_2atmpS2812];
                    int32_t _M0L4_2axS958 = _M0L6_2atmpS4105;
                    if (_M0L4_2axS958 >= 128 && _M0L4_2axS958 <= 143) {
                      moonbit_bytes_t _M0L8_2afieldS4102 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2808 = _M0L8_2afieldS4102;
                      int32_t _M0L5startS2810 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2809 = _M0L5startS2810 + 2;
                      int32_t _M0L6_2atmpS4101 =
                        _M0L5bytesS2808[_M0L6_2atmpS2809];
                      int32_t _M0L4_2axS959 = _M0L6_2atmpS4101;
                      if (_M0L4_2axS959 >= 128 && _M0L4_2axS959 <= 191) {
                        moonbit_bytes_t _M0L8_2afieldS4098 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2804 = _M0L8_2afieldS4098;
                        int32_t _M0L5startS2807 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2805 = _M0L5startS2807 + 3;
                        int32_t _M0L8_2afieldS4097 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2806 = _M0L8_2afieldS4097;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS960 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2805,
                                                             _M0L3endS2806,
                                                             _M0L5bytesS2804};
                        _M0L4restS797 = _M0L4_2axS960;
                        _M0L4tlenS798 = _M0L11_2aparam__0S787;
                        goto join_796;
                      } else {
                        moonbit_bytes_t _M0L8_2afieldS4100 =
                          _M0L11_2aparam__1S788.$0;
                        moonbit_bytes_t _M0L5bytesS2800 = _M0L8_2afieldS4100;
                        int32_t _M0L5startS2803 = _M0L11_2aparam__1S788.$1;
                        int32_t _M0L6_2atmpS2801 = _M0L5startS2803 + 2;
                        int32_t _M0L8_2afieldS4099 = _M0L11_2aparam__1S788.$2;
                        int32_t _M0L3endS2802 = _M0L8_2afieldS4099;
                        struct _M0TPC15bytes9BytesView _M0L4_2axS961 =
                          (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2801,
                                                             _M0L3endS2802,
                                                             _M0L5bytesS2800};
                        _M0L4restS794 = _M0L4_2axS961;
                        _M0L4tlenS795 = _M0L11_2aparam__0S787;
                        goto join_793;
                      }
                    } else {
                      moonbit_bytes_t _M0L8_2afieldS4104 =
                        _M0L11_2aparam__1S788.$0;
                      moonbit_bytes_t _M0L5bytesS2796 = _M0L8_2afieldS4104;
                      int32_t _M0L5startS2799 = _M0L11_2aparam__1S788.$1;
                      int32_t _M0L6_2atmpS2797 = _M0L5startS2799 + 1;
                      int32_t _M0L8_2afieldS4103 = _M0L11_2aparam__1S788.$2;
                      int32_t _M0L3endS2798 = _M0L8_2afieldS4103;
                      struct _M0TPC15bytes9BytesView _M0L4_2axS962 =
                        (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2797,
                                                           _M0L3endS2798,
                                                           _M0L5bytesS2796};
                      _M0L4tlenS790 = _M0L11_2aparam__0S787;
                      _M0L4restS791 = _M0L4_2axS962;
                      goto join_789;
                    }
                  } else {
                    moonbit_bytes_t _M0L8_2afieldS4108 =
                      _M0L11_2aparam__1S788.$0;
                    moonbit_bytes_t _M0L5bytesS2792 = _M0L8_2afieldS4108;
                    int32_t _M0L5startS2795 = _M0L11_2aparam__1S788.$1;
                    int32_t _M0L6_2atmpS2793 = _M0L5startS2795 + 1;
                    int32_t _M0L8_2afieldS4107 = _M0L11_2aparam__1S788.$2;
                    int32_t _M0L3endS2794 = _M0L8_2afieldS4107;
                    struct _M0TPC15bytes9BytesView _M0L4_2axS963 =
                      (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2793,
                                                         _M0L3endS2794,
                                                         _M0L5bytesS2792};
                    _M0L4tlenS790 = _M0L11_2aparam__0S787;
                    _M0L4restS791 = _M0L4_2axS963;
                    goto join_789;
                  }
                }
              } else if (_M0L4_2axS891 == 224) {
                moonbit_bytes_t _M0L8_2afieldS4114 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS3056 = _M0L8_2afieldS4114;
                int32_t _M0L5startS3058 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS3057 = _M0L5startS3058 + 1;
                int32_t _M0L6_2atmpS4113 = _M0L5bytesS3056[_M0L6_2atmpS3057];
                int32_t _M0L4_2axS964 = _M0L6_2atmpS4113;
                if (_M0L4_2axS964 >= 160 && _M0L4_2axS964 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS4110 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS3052 = _M0L8_2afieldS4110;
                  int32_t _M0L5startS3055 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3053 = _M0L5startS3055 + 2;
                  int32_t _M0L8_2afieldS4109 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS3054 = _M0L8_2afieldS4109;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS965 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3053,
                                                       _M0L3endS3054,
                                                       _M0L5bytesS3052};
                  _M0L4restS800 = _M0L4_2axS965;
                  _M0L4tlenS801 = _M0L11_2aparam__0S787;
                  goto join_799;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS4112 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS3048 = _M0L8_2afieldS4112;
                  int32_t _M0L5startS3051 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3049 = _M0L5startS3051 + 1;
                  int32_t _M0L8_2afieldS4111 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS3050 = _M0L8_2afieldS4111;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS966 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3049,
                                                       _M0L3endS3050,
                                                       _M0L5bytesS3048};
                  _M0L4tlenS790 = _M0L11_2aparam__0S787;
                  _M0L4restS791 = _M0L4_2axS966;
                  goto join_789;
                }
              } else if (_M0L4_2axS891 >= 225 && _M0L4_2axS891 <= 236) {
                moonbit_bytes_t _M0L8_2afieldS4120 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS3045 = _M0L8_2afieldS4120;
                int32_t _M0L5startS3047 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS3046 = _M0L5startS3047 + 1;
                int32_t _M0L6_2atmpS4119 = _M0L5bytesS3045[_M0L6_2atmpS3046];
                int32_t _M0L4_2axS967 = _M0L6_2atmpS4119;
                if (_M0L4_2axS967 >= 128 && _M0L4_2axS967 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS4116 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS3041 = _M0L8_2afieldS4116;
                  int32_t _M0L5startS3044 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3042 = _M0L5startS3044 + 2;
                  int32_t _M0L8_2afieldS4115 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS3043 = _M0L8_2afieldS4115;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS968 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3042,
                                                       _M0L3endS3043,
                                                       _M0L5bytesS3041};
                  _M0L4restS800 = _M0L4_2axS968;
                  _M0L4tlenS801 = _M0L11_2aparam__0S787;
                  goto join_799;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS4118 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS3037 = _M0L8_2afieldS4118;
                  int32_t _M0L5startS3040 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3038 = _M0L5startS3040 + 1;
                  int32_t _M0L8_2afieldS4117 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS3039 = _M0L8_2afieldS4117;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS969 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3038,
                                                       _M0L3endS3039,
                                                       _M0L5bytesS3037};
                  _M0L4tlenS790 = _M0L11_2aparam__0S787;
                  _M0L4restS791 = _M0L4_2axS969;
                  goto join_789;
                }
              } else if (_M0L4_2axS891 == 237) {
                moonbit_bytes_t _M0L8_2afieldS4126 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS3034 = _M0L8_2afieldS4126;
                int32_t _M0L5startS3036 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS3035 = _M0L5startS3036 + 1;
                int32_t _M0L6_2atmpS4125 = _M0L5bytesS3034[_M0L6_2atmpS3035];
                int32_t _M0L4_2axS970 = _M0L6_2atmpS4125;
                if (_M0L4_2axS970 >= 128 && _M0L4_2axS970 <= 159) {
                  moonbit_bytes_t _M0L8_2afieldS4122 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS3030 = _M0L8_2afieldS4122;
                  int32_t _M0L5startS3033 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3031 = _M0L5startS3033 + 2;
                  int32_t _M0L8_2afieldS4121 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS3032 = _M0L8_2afieldS4121;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS971 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3031,
                                                       _M0L3endS3032,
                                                       _M0L5bytesS3030};
                  _M0L4restS800 = _M0L4_2axS971;
                  _M0L4tlenS801 = _M0L11_2aparam__0S787;
                  goto join_799;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS4124 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS3026 = _M0L8_2afieldS4124;
                  int32_t _M0L5startS3029 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3027 = _M0L5startS3029 + 1;
                  int32_t _M0L8_2afieldS4123 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS3028 = _M0L8_2afieldS4123;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS972 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3027,
                                                       _M0L3endS3028,
                                                       _M0L5bytesS3026};
                  _M0L4tlenS790 = _M0L11_2aparam__0S787;
                  _M0L4restS791 = _M0L4_2axS972;
                  goto join_789;
                }
              } else if (_M0L4_2axS891 >= 238 && _M0L4_2axS891 <= 239) {
                moonbit_bytes_t _M0L8_2afieldS4132 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS3023 = _M0L8_2afieldS4132;
                int32_t _M0L5startS3025 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS3024 = _M0L5startS3025 + 1;
                int32_t _M0L6_2atmpS4131 = _M0L5bytesS3023[_M0L6_2atmpS3024];
                int32_t _M0L4_2axS973 = _M0L6_2atmpS4131;
                if (_M0L4_2axS973 >= 128 && _M0L4_2axS973 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS4128 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS3019 = _M0L8_2afieldS4128;
                  int32_t _M0L5startS3022 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3020 = _M0L5startS3022 + 2;
                  int32_t _M0L8_2afieldS4127 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS3021 = _M0L8_2afieldS4127;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS974 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3020,
                                                       _M0L3endS3021,
                                                       _M0L5bytesS3019};
                  _M0L4restS800 = _M0L4_2axS974;
                  _M0L4tlenS801 = _M0L11_2aparam__0S787;
                  goto join_799;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS4130 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS3015 = _M0L8_2afieldS4130;
                  int32_t _M0L5startS3018 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3016 = _M0L5startS3018 + 1;
                  int32_t _M0L8_2afieldS4129 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS3017 = _M0L8_2afieldS4129;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS975 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3016,
                                                       _M0L3endS3017,
                                                       _M0L5bytesS3015};
                  _M0L4tlenS790 = _M0L11_2aparam__0S787;
                  _M0L4restS791 = _M0L4_2axS975;
                  goto join_789;
                }
              } else if (_M0L4_2axS891 == 240) {
                moonbit_bytes_t _M0L8_2afieldS4138 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS3012 = _M0L8_2afieldS4138;
                int32_t _M0L5startS3014 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS3013 = _M0L5startS3014 + 1;
                int32_t _M0L6_2atmpS4137 = _M0L5bytesS3012[_M0L6_2atmpS3013];
                int32_t _M0L4_2axS976 = _M0L6_2atmpS4137;
                if (_M0L4_2axS976 >= 144 && _M0L4_2axS976 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS4134 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS3008 = _M0L8_2afieldS4134;
                  int32_t _M0L5startS3011 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3009 = _M0L5startS3011 + 2;
                  int32_t _M0L8_2afieldS4133 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS3010 = _M0L8_2afieldS4133;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS977 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3009,
                                                       _M0L3endS3010,
                                                       _M0L5bytesS3008};
                  _M0L4restS794 = _M0L4_2axS977;
                  _M0L4tlenS795 = _M0L11_2aparam__0S787;
                  goto join_793;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS4136 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS3004 = _M0L8_2afieldS4136;
                  int32_t _M0L5startS3007 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS3005 = _M0L5startS3007 + 1;
                  int32_t _M0L8_2afieldS4135 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS3006 = _M0L8_2afieldS4135;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS978 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3005,
                                                       _M0L3endS3006,
                                                       _M0L5bytesS3004};
                  _M0L4tlenS790 = _M0L11_2aparam__0S787;
                  _M0L4restS791 = _M0L4_2axS978;
                  goto join_789;
                }
              } else if (_M0L4_2axS891 >= 241 && _M0L4_2axS891 <= 243) {
                moonbit_bytes_t _M0L8_2afieldS4144 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS3001 = _M0L8_2afieldS4144;
                int32_t _M0L5startS3003 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS3002 = _M0L5startS3003 + 1;
                int32_t _M0L6_2atmpS4143 = _M0L5bytesS3001[_M0L6_2atmpS3002];
                int32_t _M0L4_2axS979 = _M0L6_2atmpS4143;
                if (_M0L4_2axS979 >= 128 && _M0L4_2axS979 <= 191) {
                  moonbit_bytes_t _M0L8_2afieldS4140 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS2997 = _M0L8_2afieldS4140;
                  int32_t _M0L5startS3000 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS2998 = _M0L5startS3000 + 2;
                  int32_t _M0L8_2afieldS4139 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS2999 = _M0L8_2afieldS4139;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS980 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2998,
                                                       _M0L3endS2999,
                                                       _M0L5bytesS2997};
                  _M0L4restS794 = _M0L4_2axS980;
                  _M0L4tlenS795 = _M0L11_2aparam__0S787;
                  goto join_793;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS4142 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS2993 = _M0L8_2afieldS4142;
                  int32_t _M0L5startS2996 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS2994 = _M0L5startS2996 + 1;
                  int32_t _M0L8_2afieldS4141 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS2995 = _M0L8_2afieldS4141;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS981 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2994,
                                                       _M0L3endS2995,
                                                       _M0L5bytesS2993};
                  _M0L4tlenS790 = _M0L11_2aparam__0S787;
                  _M0L4restS791 = _M0L4_2axS981;
                  goto join_789;
                }
              } else if (_M0L4_2axS891 == 244) {
                moonbit_bytes_t _M0L8_2afieldS4150 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS2990 = _M0L8_2afieldS4150;
                int32_t _M0L5startS2992 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS2991 = _M0L5startS2992 + 1;
                int32_t _M0L6_2atmpS4149 = _M0L5bytesS2990[_M0L6_2atmpS2991];
                int32_t _M0L4_2axS982 = _M0L6_2atmpS4149;
                if (_M0L4_2axS982 >= 128 && _M0L4_2axS982 <= 143) {
                  moonbit_bytes_t _M0L8_2afieldS4146 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS2986 = _M0L8_2afieldS4146;
                  int32_t _M0L5startS2989 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS2987 = _M0L5startS2989 + 2;
                  int32_t _M0L8_2afieldS4145 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS2988 = _M0L8_2afieldS4145;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS983 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2987,
                                                       _M0L3endS2988,
                                                       _M0L5bytesS2986};
                  _M0L4restS794 = _M0L4_2axS983;
                  _M0L4tlenS795 = _M0L11_2aparam__0S787;
                  goto join_793;
                } else {
                  moonbit_bytes_t _M0L8_2afieldS4148 =
                    _M0L11_2aparam__1S788.$0;
                  moonbit_bytes_t _M0L5bytesS2982 = _M0L8_2afieldS4148;
                  int32_t _M0L5startS2985 = _M0L11_2aparam__1S788.$1;
                  int32_t _M0L6_2atmpS2983 = _M0L5startS2985 + 1;
                  int32_t _M0L8_2afieldS4147 = _M0L11_2aparam__1S788.$2;
                  int32_t _M0L3endS2984 = _M0L8_2afieldS4147;
                  struct _M0TPC15bytes9BytesView _M0L4_2axS984 =
                    (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2983,
                                                       _M0L3endS2984,
                                                       _M0L5bytesS2982};
                  _M0L4tlenS790 = _M0L11_2aparam__0S787;
                  _M0L4restS791 = _M0L4_2axS984;
                  goto join_789;
                }
              } else {
                moonbit_bytes_t _M0L8_2afieldS4152 = _M0L11_2aparam__1S788.$0;
                moonbit_bytes_t _M0L5bytesS2978 = _M0L8_2afieldS4152;
                int32_t _M0L5startS2981 = _M0L11_2aparam__1S788.$1;
                int32_t _M0L6_2atmpS2979 = _M0L5startS2981 + 1;
                int32_t _M0L8_2afieldS4151 = _M0L11_2aparam__1S788.$2;
                int32_t _M0L3endS2980 = _M0L8_2afieldS4151;
                struct _M0TPC15bytes9BytesView _M0L4_2axS985 =
                  (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS2979,
                                                     _M0L3endS2980,
                                                     _M0L5bytesS2978};
                _M0L4tlenS790 = _M0L11_2aparam__0S787;
                _M0L4restS791 = _M0L4_2axS985;
                goto join_789;
              }
            }
          } else {
            moonbit_bytes_t _M0L8_2afieldS4154 = _M0L11_2aparam__1S788.$0;
            moonbit_bytes_t _M0L5bytesS3084 = _M0L8_2afieldS4154;
            int32_t _M0L5startS3087 = _M0L11_2aparam__1S788.$1;
            int32_t _M0L6_2atmpS3085 = _M0L5startS3087 + 1;
            int32_t _M0L8_2afieldS4153 = _M0L11_2aparam__1S788.$2;
            int32_t _M0L3endS3086 = _M0L8_2afieldS4153;
            struct _M0TPC15bytes9BytesView _M0L4_2axS986 =
              (struct _M0TPC15bytes9BytesView){_M0L6_2atmpS3085,
                                                 _M0L3endS3086,
                                                 _M0L5bytesS3084};
            _M0L4tlenS790 = _M0L11_2aparam__0S787;
            _M0L4restS791 = _M0L4_2axS986;
            goto join_789;
          }
        }
      }
    }
    goto joinlet_4560;
    join_826:;
    _M0L1tS785[_M0L4tlenS827] = _M0L1bS829;
    _M0L6_2atmpS2472 = _M0L4tlenS827 + 2;
    _M0L11_2aparam__0S787 = _M0L6_2atmpS2472;
    _M0L11_2aparam__1S788 = _M0L4restS828;
    continue;
    joinlet_4560:;
    goto joinlet_4559;
    join_820:;
    _M0L6_2atmpS2471 = (int32_t)_M0L2b0S823;
    _M0L6_2atmpS2470 = _M0L6_2atmpS2471 & 31;
    _M0L6_2atmpS2467 = _M0L6_2atmpS2470 << 6;
    _M0L6_2atmpS2469 = (int32_t)_M0L2b1S824;
    _M0L6_2atmpS2468 = _M0L6_2atmpS2469 & 63;
    _M0L2chS825 = _M0L6_2atmpS2467 | _M0L6_2atmpS2468;
    _M0L6_2atmpS2462 = _M0L2chS825 & 0xff;
    _M0L1tS785[_M0L4tlenS821] = _M0L6_2atmpS2462;
    _M0L6_2atmpS2463 = _M0L4tlenS821 + 1;
    _M0L6_2atmpS2465 = _M0L2chS825 >> 8;
    _M0L6_2atmpS2464 = _M0L6_2atmpS2465 & 0xff;
    _M0L1tS785[_M0L6_2atmpS2463] = _M0L6_2atmpS2464;
    _M0L6_2atmpS2466 = _M0L4tlenS821 + 2;
    _M0L11_2aparam__0S787 = _M0L6_2atmpS2466;
    _M0L11_2aparam__1S788 = _M0L4restS822;
    continue;
    joinlet_4559:;
    goto joinlet_4558;
    join_813:;
    _M0L6_2atmpS2461 = (int32_t)_M0L2b0S815;
    _M0L6_2atmpS2460 = _M0L6_2atmpS2461 & 15;
    _M0L6_2atmpS2456 = _M0L6_2atmpS2460 << 12;
    _M0L6_2atmpS2459 = (int32_t)_M0L2b1S816;
    _M0L6_2atmpS2458 = _M0L6_2atmpS2459 & 63;
    _M0L6_2atmpS2457 = _M0L6_2atmpS2458 << 6;
    _M0L6_2atmpS2453 = _M0L6_2atmpS2456 | _M0L6_2atmpS2457;
    _M0L6_2atmpS2455 = (int32_t)_M0L2b2S817;
    _M0L6_2atmpS2454 = _M0L6_2atmpS2455 & 63;
    _M0L2chS819 = _M0L6_2atmpS2453 | _M0L6_2atmpS2454;
    _M0L6_2atmpS2448 = _M0L2chS819 & 0xff;
    _M0L1tS785[_M0L4tlenS814] = _M0L6_2atmpS2448;
    _M0L6_2atmpS2449 = _M0L4tlenS814 + 1;
    _M0L6_2atmpS2451 = _M0L2chS819 >> 8;
    _M0L6_2atmpS2450 = _M0L6_2atmpS2451 & 0xff;
    _M0L1tS785[_M0L6_2atmpS2449] = _M0L6_2atmpS2450;
    _M0L6_2atmpS2452 = _M0L4tlenS814 + 2;
    _M0L11_2aparam__0S787 = _M0L6_2atmpS2452;
    _M0L11_2aparam__1S788 = _M0L4restS818;
    continue;
    joinlet_4558:;
    goto joinlet_4557;
    join_802:;
    _M0L6_2atmpS2447 = (int32_t)_M0L2b0S804;
    _M0L6_2atmpS2446 = _M0L6_2atmpS2447 & 7;
    _M0L6_2atmpS2442 = _M0L6_2atmpS2446 << 18;
    _M0L6_2atmpS2445 = (int32_t)_M0L2b1S805;
    _M0L6_2atmpS2444 = _M0L6_2atmpS2445 & 63;
    _M0L6_2atmpS2443 = _M0L6_2atmpS2444 << 12;
    _M0L6_2atmpS2438 = _M0L6_2atmpS2442 | _M0L6_2atmpS2443;
    _M0L6_2atmpS2441 = (int32_t)_M0L2b2S806;
    _M0L6_2atmpS2440 = _M0L6_2atmpS2441 & 63;
    _M0L6_2atmpS2439 = _M0L6_2atmpS2440 << 6;
    _M0L6_2atmpS2435 = _M0L6_2atmpS2438 | _M0L6_2atmpS2439;
    _M0L6_2atmpS2437 = (int32_t)_M0L2b3S807;
    _M0L6_2atmpS2436 = _M0L6_2atmpS2437 & 63;
    _M0L2chS809 = _M0L6_2atmpS2435 | _M0L6_2atmpS2436;
    _M0L3chmS810 = _M0L2chS809 - 65536;
    _M0L6_2atmpS2434 = _M0L3chmS810 >> 10;
    _M0L3ch1S811 = _M0L6_2atmpS2434 + 55296;
    _M0L6_2atmpS2433 = _M0L3chmS810 & 1023;
    _M0L3ch2S812 = _M0L6_2atmpS2433 + 56320;
    _M0L6_2atmpS2423 = _M0L3ch1S811 & 0xff;
    _M0L1tS785[_M0L4tlenS803] = _M0L6_2atmpS2423;
    _M0L6_2atmpS2424 = _M0L4tlenS803 + 1;
    _M0L6_2atmpS2426 = _M0L3ch1S811 >> 8;
    _M0L6_2atmpS2425 = _M0L6_2atmpS2426 & 0xff;
    _M0L1tS785[_M0L6_2atmpS2424] = _M0L6_2atmpS2425;
    _M0L6_2atmpS2427 = _M0L4tlenS803 + 2;
    _M0L6_2atmpS2428 = _M0L3ch2S812 & 0xff;
    _M0L1tS785[_M0L6_2atmpS2427] = _M0L6_2atmpS2428;
    _M0L6_2atmpS2429 = _M0L4tlenS803 + 3;
    _M0L6_2atmpS2431 = _M0L3ch2S812 >> 8;
    _M0L6_2atmpS2430 = _M0L6_2atmpS2431 & 0xff;
    _M0L1tS785[_M0L6_2atmpS2429] = _M0L6_2atmpS2430;
    _M0L6_2atmpS2432 = _M0L4tlenS803 + 4;
    _M0L11_2aparam__0S787 = _M0L6_2atmpS2432;
    _M0L11_2aparam__1S788 = _M0L4restS808;
    continue;
    joinlet_4557:;
    goto joinlet_4556;
    join_799:;
    _M0L1tS785[_M0L4tlenS801] = 253;
    _M0L6_2atmpS2421 = _M0L4tlenS801 + 1;
    _M0L1tS785[_M0L6_2atmpS2421] = 255;
    _M0L6_2atmpS2422 = _M0L4tlenS801 + 2;
    _M0L11_2aparam__0S787 = _M0L6_2atmpS2422;
    _M0L11_2aparam__1S788 = _M0L4restS800;
    continue;
    joinlet_4556:;
    goto joinlet_4555;
    join_796:;
    _M0L1tS785[_M0L4tlenS798] = 253;
    _M0L6_2atmpS2419 = _M0L4tlenS798 + 1;
    _M0L1tS785[_M0L6_2atmpS2419] = 255;
    _M0L6_2atmpS2420 = _M0L4tlenS798 + 2;
    _M0L11_2aparam__0S787 = _M0L6_2atmpS2420;
    _M0L11_2aparam__1S788 = _M0L4restS797;
    continue;
    joinlet_4555:;
    goto joinlet_4554;
    join_793:;
    _M0L1tS785[_M0L4tlenS795] = 253;
    _M0L6_2atmpS2417 = _M0L4tlenS795 + 1;
    _M0L1tS785[_M0L6_2atmpS2417] = 255;
    _M0L6_2atmpS2418 = _M0L4tlenS795 + 2;
    _M0L11_2aparam__0S787 = _M0L6_2atmpS2418;
    _M0L11_2aparam__1S788 = _M0L4restS794;
    continue;
    joinlet_4554:;
    goto joinlet_4553;
    join_789:;
    _M0L1tS785[_M0L4tlenS790] = 253;
    _M0L6_2atmpS2415 = _M0L4tlenS790 + 1;
    _M0L1tS785[_M0L6_2atmpS2415] = 255;
    _M0L6_2atmpS2416 = _M0L4tlenS790 + 2;
    _M0L11_2aparam__0S787 = _M0L6_2atmpS2416;
    _M0L11_2aparam__1S788 = _M0L4restS791;
    continue;
    joinlet_4553:;
    break;
  }
  _M0L6_2atmpS2413 = _M0L1tS785;
  _M0L6_2atmpS2414 = (int64_t)_M0L4tlenS786;
  #line 259 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\decode.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS2413, 0, _M0L6_2atmpS2414);
}

moonbit_bytes_t _M0FPC28encoding4utf814encode_2einner(
  struct _M0TPC16string10StringView _M0L3strS775,
  int32_t _M0L3bomS776
) {
  int32_t _M0L6_2atmpS2412;
  int32_t _M0L6_2atmpS2411;
  struct _M0TPC16buffer6Buffer* _M0L6bufferS774;
  #line 22 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  moonbit_incref(_M0L3strS775.$0);
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  _M0L6_2atmpS2412 = _M0MPC16string10StringView6length(_M0L3strS775);
  _M0L6_2atmpS2411 = _M0L6_2atmpS2412 * 4;
  #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  _M0L6bufferS774 = _M0FPC16buffer11new_2einner(_M0L6_2atmpS2411);
  if (_M0L3bomS776 == 1) {
    moonbit_incref(_M0L6bufferS774);
    #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
    _M0MPC16buffer6Buffer17write__char__utf8(_M0L6bufferS774, 65279);
  }
  moonbit_incref(_M0L6bufferS774);
  #line 27 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  _M0MPC16buffer6Buffer19write__string__utf8(_M0L6bufferS774, _M0L3strS775);
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\encoding\\utf8\\encode.mbt"
  return _M0MPC16buffer6Buffer9to__bytes(_M0L6bufferS774);
}

moonbit_bytes_t _M0MPC16buffer6Buffer9to__bytes(
  struct _M0TPC16buffer6Buffer* _M0L4selfS773
) {
  moonbit_bytes_t _M0L8_2afieldS4166;
  moonbit_bytes_t _M0L4dataS2408;
  int32_t _M0L8_2afieldS4165;
  int32_t _M0L6_2acntS4422;
  int32_t _M0L3lenS2410;
  int64_t _M0L6_2atmpS2409;
  struct _M0TPB9ArrayViewGyE _M0L6_2atmpS2407;
  #line 1112 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L8_2afieldS4166 = _M0L4selfS773->$0;
  _M0L4dataS2408 = _M0L8_2afieldS4166;
  _M0L8_2afieldS4165 = _M0L4selfS773->$1;
  _M0L6_2acntS4422 = Moonbit_object_header(_M0L4selfS773)->rc;
  if (_M0L6_2acntS4422 > 1) {
    int32_t _M0L11_2anew__cntS4423 = _M0L6_2acntS4422 - 1;
    Moonbit_object_header(_M0L4selfS773)->rc = _M0L11_2anew__cntS4423;
    moonbit_incref(_M0L4dataS2408);
  } else if (_M0L6_2acntS4422 == 1) {
    #line 1114 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    moonbit_free(_M0L4selfS773);
  }
  _M0L3lenS2410 = _M0L8_2afieldS4165;
  _M0L6_2atmpS2409 = (int64_t)_M0L3lenS2410;
  #line 1114 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L6_2atmpS2407
  = _M0MPC15array10FixedArray12view_2einnerGyE(_M0L4dataS2408, 0, _M0L6_2atmpS2409);
  #line 1114 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  return _M0MPC15bytes5Bytes11from__array(_M0L6_2atmpS2407);
}

int32_t _M0MPC16buffer6Buffer19write__string__utf8(
  struct _M0TPC16buffer6Buffer* _M0L3bufS771,
  struct _M0TPC16string10StringView _M0L6stringS767
) {
  struct _M0TWEOc* _M0L5_2aitS766;
  #line 880 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  #line 880 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L5_2aitS766 = _M0MPC16string10StringView4iter(_M0L6stringS767);
  while (1) {
    int32_t _M0L7_2abindS768;
    moonbit_incref(_M0L5_2aitS766);
    #line 881 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L7_2abindS768 = _M0MPB4Iter4nextGcE(_M0L5_2aitS766);
    if (_M0L7_2abindS768 == -1) {
      moonbit_decref(_M0L3bufS771);
      moonbit_decref(_M0L5_2aitS766);
    } else {
      int32_t _M0L7_2aSomeS769 = _M0L7_2abindS768;
      int32_t _M0L5_2achS770 = _M0L7_2aSomeS769;
      moonbit_incref(_M0L3bufS771);
      #line 882 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      _M0MPC16buffer6Buffer17write__char__utf8(_M0L3bufS771, _M0L5_2achS770);
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16buffer6Buffer17write__char__utf8(
  struct _M0TPC16buffer6Buffer* _M0L3bufS765,
  int32_t _M0L5valueS764
) {
  uint32_t _M0L4codeS763;
  #line 782 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  #line 783 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L4codeS763 = _M0MPC14char4Char8to__uint(_M0L5valueS764);
  if (_M0L4codeS763 < 128u) {
    int32_t _M0L3lenS2399 = _M0L3bufS765->$1;
    int32_t _M0L6_2atmpS2398 = _M0L3lenS2399 + 1;
    moonbit_bytes_t _M0L8_2afieldS4167;
    moonbit_bytes_t _M0L4dataS2400;
    int32_t _M0L3lenS2401;
    uint32_t _M0L6_2atmpS2404;
    uint32_t _M0L6_2atmpS2403;
    int32_t _M0L6_2atmpS2402;
    int32_t _M0L3lenS2406;
    int32_t _M0L6_2atmpS2405;
    moonbit_incref(_M0L3bufS765);
    #line 786 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS765, _M0L6_2atmpS2398);
    _M0L8_2afieldS4167 = _M0L3bufS765->$0;
    _M0L4dataS2400 = _M0L8_2afieldS4167;
    _M0L3lenS2401 = _M0L3bufS765->$1;
    _M0L6_2atmpS2404 = _M0L4codeS763 & 127u;
    _M0L6_2atmpS2403 = _M0L6_2atmpS2404 | 0u;
    moonbit_incref(_M0L4dataS2400);
    #line 787 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS2402 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2403);
    if (
      _M0L3lenS2401 < 0
      || _M0L3lenS2401 >= Moonbit_array_length(_M0L4dataS2400)
    ) {
      #line 787 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2400[_M0L3lenS2401] = _M0L6_2atmpS2402;
    moonbit_decref(_M0L4dataS2400);
    _M0L3lenS2406 = _M0L3bufS765->$1;
    _M0L6_2atmpS2405 = _M0L3lenS2406 + 1;
    _M0L3bufS765->$1 = _M0L6_2atmpS2405;
    moonbit_decref(_M0L3bufS765);
  } else if (_M0L4codeS763 < 2048u) {
    int32_t _M0L3lenS2383 = _M0L3bufS765->$1;
    int32_t _M0L6_2atmpS2382 = _M0L3lenS2383 + 2;
    moonbit_bytes_t _M0L8_2afieldS4169;
    moonbit_bytes_t _M0L4dataS2384;
    int32_t _M0L3lenS2385;
    uint32_t _M0L6_2atmpS2389;
    uint32_t _M0L6_2atmpS2388;
    uint32_t _M0L6_2atmpS2387;
    int32_t _M0L6_2atmpS2386;
    moonbit_bytes_t _M0L8_2afieldS4168;
    moonbit_bytes_t _M0L4dataS2390;
    int32_t _M0L3lenS2395;
    int32_t _M0L6_2atmpS2391;
    uint32_t _M0L6_2atmpS2394;
    uint32_t _M0L6_2atmpS2393;
    int32_t _M0L6_2atmpS2392;
    int32_t _M0L3lenS2397;
    int32_t _M0L6_2atmpS2396;
    moonbit_incref(_M0L3bufS765);
    #line 791 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS765, _M0L6_2atmpS2382);
    _M0L8_2afieldS4169 = _M0L3bufS765->$0;
    _M0L4dataS2384 = _M0L8_2afieldS4169;
    _M0L3lenS2385 = _M0L3bufS765->$1;
    _M0L6_2atmpS2389 = _M0L4codeS763 >> 6;
    _M0L6_2atmpS2388 = _M0L6_2atmpS2389 & 31u;
    _M0L6_2atmpS2387 = _M0L6_2atmpS2388 | 192u;
    moonbit_incref(_M0L4dataS2384);
    #line 792 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS2386 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2387);
    if (
      _M0L3lenS2385 < 0
      || _M0L3lenS2385 >= Moonbit_array_length(_M0L4dataS2384)
    ) {
      #line 792 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2384[_M0L3lenS2385] = _M0L6_2atmpS2386;
    moonbit_decref(_M0L4dataS2384);
    _M0L8_2afieldS4168 = _M0L3bufS765->$0;
    _M0L4dataS2390 = _M0L8_2afieldS4168;
    _M0L3lenS2395 = _M0L3bufS765->$1;
    _M0L6_2atmpS2391 = _M0L3lenS2395 + 1;
    _M0L6_2atmpS2394 = _M0L4codeS763 & 63u;
    _M0L6_2atmpS2393 = _M0L6_2atmpS2394 | 128u;
    moonbit_incref(_M0L4dataS2390);
    #line 793 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS2392 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2393);
    if (
      _M0L6_2atmpS2391 < 0
      || _M0L6_2atmpS2391 >= Moonbit_array_length(_M0L4dataS2390)
    ) {
      #line 793 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2390[_M0L6_2atmpS2391] = _M0L6_2atmpS2392;
    moonbit_decref(_M0L4dataS2390);
    _M0L3lenS2397 = _M0L3bufS765->$1;
    _M0L6_2atmpS2396 = _M0L3lenS2397 + 2;
    _M0L3bufS765->$1 = _M0L6_2atmpS2396;
    moonbit_decref(_M0L3bufS765);
  } else if (_M0L4codeS763 < 65536u) {
    int32_t _M0L3lenS2360 = _M0L3bufS765->$1;
    int32_t _M0L6_2atmpS2359 = _M0L3lenS2360 + 3;
    moonbit_bytes_t _M0L8_2afieldS4172;
    moonbit_bytes_t _M0L4dataS2361;
    int32_t _M0L3lenS2362;
    uint32_t _M0L6_2atmpS2366;
    uint32_t _M0L6_2atmpS2365;
    uint32_t _M0L6_2atmpS2364;
    int32_t _M0L6_2atmpS2363;
    moonbit_bytes_t _M0L8_2afieldS4171;
    moonbit_bytes_t _M0L4dataS2367;
    int32_t _M0L3lenS2373;
    int32_t _M0L6_2atmpS2368;
    uint32_t _M0L6_2atmpS2372;
    uint32_t _M0L6_2atmpS2371;
    uint32_t _M0L6_2atmpS2370;
    int32_t _M0L6_2atmpS2369;
    moonbit_bytes_t _M0L8_2afieldS4170;
    moonbit_bytes_t _M0L4dataS2374;
    int32_t _M0L3lenS2379;
    int32_t _M0L6_2atmpS2375;
    uint32_t _M0L6_2atmpS2378;
    uint32_t _M0L6_2atmpS2377;
    int32_t _M0L6_2atmpS2376;
    int32_t _M0L3lenS2381;
    int32_t _M0L6_2atmpS2380;
    moonbit_incref(_M0L3bufS765);
    #line 797 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS765, _M0L6_2atmpS2359);
    _M0L8_2afieldS4172 = _M0L3bufS765->$0;
    _M0L4dataS2361 = _M0L8_2afieldS4172;
    _M0L3lenS2362 = _M0L3bufS765->$1;
    _M0L6_2atmpS2366 = _M0L4codeS763 >> 12;
    _M0L6_2atmpS2365 = _M0L6_2atmpS2366 & 15u;
    _M0L6_2atmpS2364 = _M0L6_2atmpS2365 | 224u;
    moonbit_incref(_M0L4dataS2361);
    #line 798 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS2363 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2364);
    if (
      _M0L3lenS2362 < 0
      || _M0L3lenS2362 >= Moonbit_array_length(_M0L4dataS2361)
    ) {
      #line 798 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2361[_M0L3lenS2362] = _M0L6_2atmpS2363;
    moonbit_decref(_M0L4dataS2361);
    _M0L8_2afieldS4171 = _M0L3bufS765->$0;
    _M0L4dataS2367 = _M0L8_2afieldS4171;
    _M0L3lenS2373 = _M0L3bufS765->$1;
    _M0L6_2atmpS2368 = _M0L3lenS2373 + 1;
    _M0L6_2atmpS2372 = _M0L4codeS763 >> 6;
    _M0L6_2atmpS2371 = _M0L6_2atmpS2372 & 63u;
    _M0L6_2atmpS2370 = _M0L6_2atmpS2371 | 128u;
    moonbit_incref(_M0L4dataS2367);
    #line 799 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS2369 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2370);
    if (
      _M0L6_2atmpS2368 < 0
      || _M0L6_2atmpS2368 >= Moonbit_array_length(_M0L4dataS2367)
    ) {
      #line 799 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2367[_M0L6_2atmpS2368] = _M0L6_2atmpS2369;
    moonbit_decref(_M0L4dataS2367);
    _M0L8_2afieldS4170 = _M0L3bufS765->$0;
    _M0L4dataS2374 = _M0L8_2afieldS4170;
    _M0L3lenS2379 = _M0L3bufS765->$1;
    _M0L6_2atmpS2375 = _M0L3lenS2379 + 2;
    _M0L6_2atmpS2378 = _M0L4codeS763 & 63u;
    _M0L6_2atmpS2377 = _M0L6_2atmpS2378 | 128u;
    moonbit_incref(_M0L4dataS2374);
    #line 800 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS2376 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2377);
    if (
      _M0L6_2atmpS2375 < 0
      || _M0L6_2atmpS2375 >= Moonbit_array_length(_M0L4dataS2374)
    ) {
      #line 800 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2374[_M0L6_2atmpS2375] = _M0L6_2atmpS2376;
    moonbit_decref(_M0L4dataS2374);
    _M0L3lenS2381 = _M0L3bufS765->$1;
    _M0L6_2atmpS2380 = _M0L3lenS2381 + 3;
    _M0L3bufS765->$1 = _M0L6_2atmpS2380;
    moonbit_decref(_M0L3bufS765);
  } else if (_M0L4codeS763 < 1114112u) {
    int32_t _M0L3lenS2330 = _M0L3bufS765->$1;
    int32_t _M0L6_2atmpS2329 = _M0L3lenS2330 + 4;
    moonbit_bytes_t _M0L8_2afieldS4176;
    moonbit_bytes_t _M0L4dataS2331;
    int32_t _M0L3lenS2332;
    uint32_t _M0L6_2atmpS2336;
    uint32_t _M0L6_2atmpS2335;
    uint32_t _M0L6_2atmpS2334;
    int32_t _M0L6_2atmpS2333;
    moonbit_bytes_t _M0L8_2afieldS4175;
    moonbit_bytes_t _M0L4dataS2337;
    int32_t _M0L3lenS2343;
    int32_t _M0L6_2atmpS2338;
    uint32_t _M0L6_2atmpS2342;
    uint32_t _M0L6_2atmpS2341;
    uint32_t _M0L6_2atmpS2340;
    int32_t _M0L6_2atmpS2339;
    moonbit_bytes_t _M0L8_2afieldS4174;
    moonbit_bytes_t _M0L4dataS2344;
    int32_t _M0L3lenS2350;
    int32_t _M0L6_2atmpS2345;
    uint32_t _M0L6_2atmpS2349;
    uint32_t _M0L6_2atmpS2348;
    uint32_t _M0L6_2atmpS2347;
    int32_t _M0L6_2atmpS2346;
    moonbit_bytes_t _M0L8_2afieldS4173;
    moonbit_bytes_t _M0L4dataS2351;
    int32_t _M0L3lenS2356;
    int32_t _M0L6_2atmpS2352;
    uint32_t _M0L6_2atmpS2355;
    uint32_t _M0L6_2atmpS2354;
    int32_t _M0L6_2atmpS2353;
    int32_t _M0L3lenS2358;
    int32_t _M0L6_2atmpS2357;
    moonbit_incref(_M0L3bufS765);
    #line 804 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC16buffer6Buffer19grow__if__necessary(_M0L3bufS765, _M0L6_2atmpS2329);
    _M0L8_2afieldS4176 = _M0L3bufS765->$0;
    _M0L4dataS2331 = _M0L8_2afieldS4176;
    _M0L3lenS2332 = _M0L3bufS765->$1;
    _M0L6_2atmpS2336 = _M0L4codeS763 >> 18;
    _M0L6_2atmpS2335 = _M0L6_2atmpS2336 & 7u;
    _M0L6_2atmpS2334 = _M0L6_2atmpS2335 | 240u;
    moonbit_incref(_M0L4dataS2331);
    #line 805 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS2333 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2334);
    if (
      _M0L3lenS2332 < 0
      || _M0L3lenS2332 >= Moonbit_array_length(_M0L4dataS2331)
    ) {
      #line 805 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2331[_M0L3lenS2332] = _M0L6_2atmpS2333;
    moonbit_decref(_M0L4dataS2331);
    _M0L8_2afieldS4175 = _M0L3bufS765->$0;
    _M0L4dataS2337 = _M0L8_2afieldS4175;
    _M0L3lenS2343 = _M0L3bufS765->$1;
    _M0L6_2atmpS2338 = _M0L3lenS2343 + 1;
    _M0L6_2atmpS2342 = _M0L4codeS763 >> 12;
    _M0L6_2atmpS2341 = _M0L6_2atmpS2342 & 63u;
    _M0L6_2atmpS2340 = _M0L6_2atmpS2341 | 128u;
    moonbit_incref(_M0L4dataS2337);
    #line 806 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS2339 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2340);
    if (
      _M0L6_2atmpS2338 < 0
      || _M0L6_2atmpS2338 >= Moonbit_array_length(_M0L4dataS2337)
    ) {
      #line 806 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2337[_M0L6_2atmpS2338] = _M0L6_2atmpS2339;
    moonbit_decref(_M0L4dataS2337);
    _M0L8_2afieldS4174 = _M0L3bufS765->$0;
    _M0L4dataS2344 = _M0L8_2afieldS4174;
    _M0L3lenS2350 = _M0L3bufS765->$1;
    _M0L6_2atmpS2345 = _M0L3lenS2350 + 2;
    _M0L6_2atmpS2349 = _M0L4codeS763 >> 6;
    _M0L6_2atmpS2348 = _M0L6_2atmpS2349 & 63u;
    _M0L6_2atmpS2347 = _M0L6_2atmpS2348 | 128u;
    moonbit_incref(_M0L4dataS2344);
    #line 807 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS2346 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2347);
    if (
      _M0L6_2atmpS2345 < 0
      || _M0L6_2atmpS2345 >= Moonbit_array_length(_M0L4dataS2344)
    ) {
      #line 807 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2344[_M0L6_2atmpS2345] = _M0L6_2atmpS2346;
    moonbit_decref(_M0L4dataS2344);
    _M0L8_2afieldS4173 = _M0L3bufS765->$0;
    _M0L4dataS2351 = _M0L8_2afieldS4173;
    _M0L3lenS2356 = _M0L3bufS765->$1;
    _M0L6_2atmpS2352 = _M0L3lenS2356 + 3;
    _M0L6_2atmpS2355 = _M0L4codeS763 & 63u;
    _M0L6_2atmpS2354 = _M0L6_2atmpS2355 | 128u;
    moonbit_incref(_M0L4dataS2351);
    #line 808 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS2353 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS2354);
    if (
      _M0L6_2atmpS2352 < 0
      || _M0L6_2atmpS2352 >= Moonbit_array_length(_M0L4dataS2351)
    ) {
      #line 808 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
      moonbit_panic();
    }
    _M0L4dataS2351[_M0L6_2atmpS2352] = _M0L6_2atmpS2353;
    moonbit_decref(_M0L4dataS2351);
    _M0L3lenS2358 = _M0L3bufS765->$1;
    _M0L6_2atmpS2357 = _M0L3lenS2358 + 4;
    _M0L3bufS765->$1 = _M0L6_2atmpS2357;
    moonbit_decref(_M0L3bufS765);
  } else {
    moonbit_decref(_M0L3bufS765);
    #line 811 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_18.data, (moonbit_string_t)moonbit_string_literal_19.data);
  }
  return 0;
}

struct _M0TPC16buffer6Buffer* _M0FPC16buffer11new_2einner(
  int32_t _M0L10size__hintS761
) {
  int32_t _M0L7initialS760;
  int32_t _M0L6_2atmpS2328;
  moonbit_bytes_t _M0L4dataS762;
  struct _M0TPC16buffer6Buffer* _block_4562;
  #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  if (_M0L10size__hintS761 < 1) {
    _M0L7initialS760 = 1;
  } else {
    _M0L7initialS760 = _M0L10size__hintS761;
  }
  #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L6_2atmpS2328 = _M0IPC14byte4BytePB7Default7default();
  _M0L4dataS762
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS760, _M0L6_2atmpS2328);
  _block_4562
  = (struct _M0TPC16buffer6Buffer*)moonbit_malloc(sizeof(struct _M0TPC16buffer6Buffer));
  Moonbit_object_header(_block_4562)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPC16buffer6Buffer, $0) >> 2, 1, 0);
  _block_4562->$0 = _M0L4dataS762;
  _block_4562->$1 = 0;
  return _block_4562;
}

int32_t _M0MPC16buffer6Buffer19grow__if__necessary(
  struct _M0TPC16buffer6Buffer* _M0L4selfS754,
  int32_t _M0L8requiredS757
) {
  moonbit_bytes_t _M0L8_2afieldS4184;
  moonbit_bytes_t _M0L4dataS2326;
  int32_t _M0L6_2atmpS4183;
  int32_t _M0L6_2atmpS2325;
  int32_t _M0L5startS753;
  int32_t _M0L13enough__spaceS755;
  int32_t _M0L5spaceS756;
  moonbit_bytes_t _M0L8_2afieldS4180;
  moonbit_bytes_t _M0L4dataS2320;
  int32_t _M0L6_2atmpS4179;
  int32_t _M0L6_2atmpS2319;
  #line 46 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
  _M0L8_2afieldS4184 = _M0L4selfS754->$0;
  _M0L4dataS2326 = _M0L8_2afieldS4184;
  _M0L6_2atmpS4183 = Moonbit_array_length(_M0L4dataS2326);
  _M0L6_2atmpS2325 = _M0L6_2atmpS4183;
  if (_M0L6_2atmpS2325 <= 0) {
    _M0L5startS753 = 1;
  } else {
    moonbit_bytes_t _M0L8_2afieldS4182 = _M0L4selfS754->$0;
    moonbit_bytes_t _M0L4dataS2327 = _M0L8_2afieldS4182;
    int32_t _M0L6_2atmpS4181 = Moonbit_array_length(_M0L4dataS2327);
    _M0L5startS753 = _M0L6_2atmpS4181;
  }
  _M0L5spaceS756 = _M0L5startS753;
  while (1) {
    int32_t _M0L6_2atmpS2324;
    if (_M0L5spaceS756 >= _M0L8requiredS757) {
      _M0L13enough__spaceS755 = _M0L5spaceS756;
      break;
    }
    _M0L6_2atmpS2324 = _M0L5spaceS756 * 2;
    _M0L5spaceS756 = _M0L6_2atmpS2324;
    continue;
    break;
  }
  _M0L8_2afieldS4180 = _M0L4selfS754->$0;
  _M0L4dataS2320 = _M0L8_2afieldS4180;
  _M0L6_2atmpS4179 = Moonbit_array_length(_M0L4dataS2320);
  _M0L6_2atmpS2319 = _M0L6_2atmpS4179;
  if (_M0L13enough__spaceS755 != _M0L6_2atmpS2319) {
    int32_t _M0L6_2atmpS2323;
    moonbit_bytes_t _M0L9new__dataS759;
    moonbit_bytes_t _M0L8_2afieldS4178;
    moonbit_bytes_t _M0L4dataS2321;
    int32_t _M0L3lenS2322;
    moonbit_bytes_t _M0L6_2aoldS4177;
    #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0L6_2atmpS2323 = _M0IPC14byte4BytePB7Default7default();
    _M0L9new__dataS759
    = (moonbit_bytes_t)moonbit_make_bytes(_M0L13enough__spaceS755, _M0L6_2atmpS2323);
    _M0L8_2afieldS4178 = _M0L4selfS754->$0;
    _M0L4dataS2321 = _M0L8_2afieldS4178;
    _M0L3lenS2322 = _M0L4selfS754->$1;
    moonbit_incref(_M0L4dataS2321);
    moonbit_incref(_M0L9new__dataS759);
    #line 56 "C:\\Users\\Administrator\\.moon\\lib\\core\\buffer\\buffer.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS759, 0, _M0L4dataS2321, 0, _M0L3lenS2322);
    _M0L6_2aoldS4177 = _M0L4selfS754->$0;
    moonbit_decref(_M0L6_2aoldS4177);
    _M0L4selfS754->$0 = _M0L9new__dataS759;
    moonbit_decref(_M0L4selfS754);
  } else {
    moonbit_decref(_M0L4selfS754);
  }
  return 0;
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS751,
  struct _M0TPB6Logger _M0L6loggerS752
) {
  moonbit_string_t _M0L6_2atmpS2318;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2317;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2318 = _M0L4selfS751;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2317 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2318);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2317, _M0L6loggerS752);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS728,
  struct _M0TPB6Logger _M0L6loggerS750
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS4193;
  struct _M0TPC16string10StringView _M0L3pkgS727;
  moonbit_string_t _M0L7_2adataS729;
  int32_t _M0L8_2astartS730;
  int32_t _M0L6_2atmpS2316;
  int32_t _M0L6_2aendS731;
  int32_t _M0Lm9_2acursorS732;
  int32_t _M0Lm13accept__stateS733;
  int32_t _M0Lm10match__endS734;
  int32_t _M0Lm20match__tag__saver__0S735;
  int32_t _M0Lm6tag__0S736;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS737;
  struct _M0TPC16string10StringView _M0L8_2afieldS4192;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS746;
  void* _M0L8_2afieldS4191;
  int32_t _M0L6_2acntS4424;
  void* _M0L16_2apackage__nameS747;
  struct _M0TPC16string10StringView _M0L8_2afieldS4189;
  struct _M0TPC16string10StringView _M0L8filenameS2293;
  struct _M0TPC16string10StringView _M0L8_2afieldS4188;
  struct _M0TPC16string10StringView _M0L11start__lineS2294;
  struct _M0TPC16string10StringView _M0L8_2afieldS4187;
  struct _M0TPC16string10StringView _M0L13start__columnS2295;
  struct _M0TPC16string10StringView _M0L8_2afieldS4186;
  struct _M0TPC16string10StringView _M0L9end__lineS2296;
  struct _M0TPC16string10StringView _M0L8_2afieldS4185;
  int32_t _M0L6_2acntS4428;
  struct _M0TPC16string10StringView _M0L11end__columnS2297;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS4193
  = (struct _M0TPC16string10StringView){
    _M0L4selfS728->$0_1, _M0L4selfS728->$0_2, _M0L4selfS728->$0_0
  };
  _M0L3pkgS727 = _M0L8_2afieldS4193;
  moonbit_incref(_M0L3pkgS727.$0);
  moonbit_incref(_M0L3pkgS727.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS729 = _M0MPC16string10StringView4data(_M0L3pkgS727);
  moonbit_incref(_M0L3pkgS727.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS730 = _M0MPC16string10StringView13start__offset(_M0L3pkgS727);
  moonbit_incref(_M0L3pkgS727.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2316 = _M0MPC16string10StringView6length(_M0L3pkgS727);
  _M0L6_2aendS731 = _M0L8_2astartS730 + _M0L6_2atmpS2316;
  _M0Lm9_2acursorS732 = _M0L8_2astartS730;
  _M0Lm13accept__stateS733 = -1;
  _M0Lm10match__endS734 = -1;
  _M0Lm20match__tag__saver__0S735 = -1;
  _M0Lm6tag__0S736 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2308 = _M0Lm9_2acursorS732;
    if (_M0L6_2atmpS2308 < _M0L6_2aendS731) {
      int32_t _M0L6_2atmpS2315 = _M0Lm9_2acursorS732;
      int32_t _M0L10next__charS741;
      int32_t _M0L6_2atmpS2309;
      moonbit_incref(_M0L7_2adataS729);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS741
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS729, _M0L6_2atmpS2315);
      _M0L6_2atmpS2309 = _M0Lm9_2acursorS732;
      _M0Lm9_2acursorS732 = _M0L6_2atmpS2309 + 1;
      if (_M0L10next__charS741 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2310;
          _M0Lm6tag__0S736 = _M0Lm9_2acursorS732;
          _M0L6_2atmpS2310 = _M0Lm9_2acursorS732;
          if (_M0L6_2atmpS2310 < _M0L6_2aendS731) {
            int32_t _M0L6_2atmpS2314 = _M0Lm9_2acursorS732;
            int32_t _M0L10next__charS742;
            int32_t _M0L6_2atmpS2311;
            moonbit_incref(_M0L7_2adataS729);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS742
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS729, _M0L6_2atmpS2314);
            _M0L6_2atmpS2311 = _M0Lm9_2acursorS732;
            _M0Lm9_2acursorS732 = _M0L6_2atmpS2311 + 1;
            if (_M0L10next__charS742 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2312 = _M0Lm9_2acursorS732;
                if (_M0L6_2atmpS2312 < _M0L6_2aendS731) {
                  int32_t _M0L6_2atmpS2313 = _M0Lm9_2acursorS732;
                  _M0Lm9_2acursorS732 = _M0L6_2atmpS2313 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S735 = _M0Lm6tag__0S736;
                  _M0Lm13accept__stateS733 = 0;
                  _M0Lm10match__endS734 = _M0Lm9_2acursorS732;
                  goto join_738;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_738;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_738;
    }
    break;
  }
  goto joinlet_4564;
  join_738:;
  switch (_M0Lm13accept__stateS733) {
    case 0: {
      int32_t _M0L6_2atmpS2306;
      int32_t _M0L6_2atmpS2305;
      int64_t _M0L6_2atmpS2302;
      int32_t _M0L6_2atmpS2304;
      int64_t _M0L6_2atmpS2303;
      struct _M0TPC16string10StringView _M0L13package__nameS739;
      int64_t _M0L6_2atmpS2299;
      int32_t _M0L6_2atmpS2301;
      int64_t _M0L6_2atmpS2300;
      struct _M0TPC16string10StringView _M0L12module__nameS740;
      void* _M0L4SomeS2298;
      moonbit_decref(_M0L3pkgS727.$0);
      _M0L6_2atmpS2306 = _M0Lm20match__tag__saver__0S735;
      _M0L6_2atmpS2305 = _M0L6_2atmpS2306 + 1;
      _M0L6_2atmpS2302 = (int64_t)_M0L6_2atmpS2305;
      _M0L6_2atmpS2304 = _M0Lm10match__endS734;
      _M0L6_2atmpS2303 = (int64_t)_M0L6_2atmpS2304;
      moonbit_incref(_M0L7_2adataS729);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS739
      = _M0MPC16string6String4view(_M0L7_2adataS729, _M0L6_2atmpS2302, _M0L6_2atmpS2303);
      _M0L6_2atmpS2299 = (int64_t)_M0L8_2astartS730;
      _M0L6_2atmpS2301 = _M0Lm20match__tag__saver__0S735;
      _M0L6_2atmpS2300 = (int64_t)_M0L6_2atmpS2301;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS740
      = _M0MPC16string6String4view(_M0L7_2adataS729, _M0L6_2atmpS2299, _M0L6_2atmpS2300);
      _M0L4SomeS2298
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2298)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2298)->$0_0
      = _M0L13package__nameS739.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2298)->$0_1
      = _M0L13package__nameS739.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2298)->$0_2
      = _M0L13package__nameS739.$2;
      _M0L7_2abindS737
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS737)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS737->$0_0 = _M0L12module__nameS740.$0;
      _M0L7_2abindS737->$0_1 = _M0L12module__nameS740.$1;
      _M0L7_2abindS737->$0_2 = _M0L12module__nameS740.$2;
      _M0L7_2abindS737->$1 = _M0L4SomeS2298;
      break;
    }
    default: {
      void* _M0L4NoneS2307;
      moonbit_decref(_M0L7_2adataS729);
      _M0L4NoneS2307
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS737
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS737)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS737->$0_0 = _M0L3pkgS727.$0;
      _M0L7_2abindS737->$0_1 = _M0L3pkgS727.$1;
      _M0L7_2abindS737->$0_2 = _M0L3pkgS727.$2;
      _M0L7_2abindS737->$1 = _M0L4NoneS2307;
      break;
    }
  }
  joinlet_4564:;
  _M0L8_2afieldS4192
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS737->$0_1, _M0L7_2abindS737->$0_2, _M0L7_2abindS737->$0_0
  };
  _M0L15_2amodule__nameS746 = _M0L8_2afieldS4192;
  _M0L8_2afieldS4191 = _M0L7_2abindS737->$1;
  _M0L6_2acntS4424 = Moonbit_object_header(_M0L7_2abindS737)->rc;
  if (_M0L6_2acntS4424 > 1) {
    int32_t _M0L11_2anew__cntS4425 = _M0L6_2acntS4424 - 1;
    Moonbit_object_header(_M0L7_2abindS737)->rc = _M0L11_2anew__cntS4425;
    moonbit_incref(_M0L8_2afieldS4191);
    moonbit_incref(_M0L15_2amodule__nameS746.$0);
  } else if (_M0L6_2acntS4424 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS737);
  }
  _M0L16_2apackage__nameS747 = _M0L8_2afieldS4191;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS747)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS748 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS747;
      struct _M0TPC16string10StringView _M0L8_2afieldS4190 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS748->$0_1,
                                              _M0L7_2aSomeS748->$0_2,
                                              _M0L7_2aSomeS748->$0_0};
      int32_t _M0L6_2acntS4426 = Moonbit_object_header(_M0L7_2aSomeS748)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS749;
      if (_M0L6_2acntS4426 > 1) {
        int32_t _M0L11_2anew__cntS4427 = _M0L6_2acntS4426 - 1;
        Moonbit_object_header(_M0L7_2aSomeS748)->rc = _M0L11_2anew__cntS4427;
        moonbit_incref(_M0L8_2afieldS4190.$0);
      } else if (_M0L6_2acntS4426 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS748);
      }
      _M0L12_2apkg__nameS749 = _M0L8_2afieldS4190;
      if (_M0L6loggerS750.$1) {
        moonbit_incref(_M0L6loggerS750.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS750.$0->$method_2(_M0L6loggerS750.$1, _M0L12_2apkg__nameS749);
      if (_M0L6loggerS750.$1) {
        moonbit_incref(_M0L6loggerS750.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS750.$0->$method_3(_M0L6loggerS750.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS747);
      break;
    }
  }
  _M0L8_2afieldS4189
  = (struct _M0TPC16string10StringView){
    _M0L4selfS728->$1_1, _M0L4selfS728->$1_2, _M0L4selfS728->$1_0
  };
  _M0L8filenameS2293 = _M0L8_2afieldS4189;
  moonbit_incref(_M0L8filenameS2293.$0);
  if (_M0L6loggerS750.$1) {
    moonbit_incref(_M0L6loggerS750.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS750.$0->$method_2(_M0L6loggerS750.$1, _M0L8filenameS2293);
  if (_M0L6loggerS750.$1) {
    moonbit_incref(_M0L6loggerS750.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS750.$0->$method_3(_M0L6loggerS750.$1, 58);
  _M0L8_2afieldS4188
  = (struct _M0TPC16string10StringView){
    _M0L4selfS728->$2_1, _M0L4selfS728->$2_2, _M0L4selfS728->$2_0
  };
  _M0L11start__lineS2294 = _M0L8_2afieldS4188;
  moonbit_incref(_M0L11start__lineS2294.$0);
  if (_M0L6loggerS750.$1) {
    moonbit_incref(_M0L6loggerS750.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS750.$0->$method_2(_M0L6loggerS750.$1, _M0L11start__lineS2294);
  if (_M0L6loggerS750.$1) {
    moonbit_incref(_M0L6loggerS750.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS750.$0->$method_3(_M0L6loggerS750.$1, 58);
  _M0L8_2afieldS4187
  = (struct _M0TPC16string10StringView){
    _M0L4selfS728->$3_1, _M0L4selfS728->$3_2, _M0L4selfS728->$3_0
  };
  _M0L13start__columnS2295 = _M0L8_2afieldS4187;
  moonbit_incref(_M0L13start__columnS2295.$0);
  if (_M0L6loggerS750.$1) {
    moonbit_incref(_M0L6loggerS750.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS750.$0->$method_2(_M0L6loggerS750.$1, _M0L13start__columnS2295);
  if (_M0L6loggerS750.$1) {
    moonbit_incref(_M0L6loggerS750.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS750.$0->$method_3(_M0L6loggerS750.$1, 45);
  _M0L8_2afieldS4186
  = (struct _M0TPC16string10StringView){
    _M0L4selfS728->$4_1, _M0L4selfS728->$4_2, _M0L4selfS728->$4_0
  };
  _M0L9end__lineS2296 = _M0L8_2afieldS4186;
  moonbit_incref(_M0L9end__lineS2296.$0);
  if (_M0L6loggerS750.$1) {
    moonbit_incref(_M0L6loggerS750.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS750.$0->$method_2(_M0L6loggerS750.$1, _M0L9end__lineS2296);
  if (_M0L6loggerS750.$1) {
    moonbit_incref(_M0L6loggerS750.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS750.$0->$method_3(_M0L6loggerS750.$1, 58);
  _M0L8_2afieldS4185
  = (struct _M0TPC16string10StringView){
    _M0L4selfS728->$5_1, _M0L4selfS728->$5_2, _M0L4selfS728->$5_0
  };
  _M0L6_2acntS4428 = Moonbit_object_header(_M0L4selfS728)->rc;
  if (_M0L6_2acntS4428 > 1) {
    int32_t _M0L11_2anew__cntS4434 = _M0L6_2acntS4428 - 1;
    Moonbit_object_header(_M0L4selfS728)->rc = _M0L11_2anew__cntS4434;
    moonbit_incref(_M0L8_2afieldS4185.$0);
  } else if (_M0L6_2acntS4428 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS4433 =
      (struct _M0TPC16string10StringView){_M0L4selfS728->$4_1,
                                            _M0L4selfS728->$4_2,
                                            _M0L4selfS728->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS4432;
    struct _M0TPC16string10StringView _M0L8_2afieldS4431;
    struct _M0TPC16string10StringView _M0L8_2afieldS4430;
    struct _M0TPC16string10StringView _M0L8_2afieldS4429;
    moonbit_decref(_M0L8_2afieldS4433.$0);
    _M0L8_2afieldS4432
    = (struct _M0TPC16string10StringView){
      _M0L4selfS728->$3_1, _M0L4selfS728->$3_2, _M0L4selfS728->$3_0
    };
    moonbit_decref(_M0L8_2afieldS4432.$0);
    _M0L8_2afieldS4431
    = (struct _M0TPC16string10StringView){
      _M0L4selfS728->$2_1, _M0L4selfS728->$2_2, _M0L4selfS728->$2_0
    };
    moonbit_decref(_M0L8_2afieldS4431.$0);
    _M0L8_2afieldS4430
    = (struct _M0TPC16string10StringView){
      _M0L4selfS728->$1_1, _M0L4selfS728->$1_2, _M0L4selfS728->$1_0
    };
    moonbit_decref(_M0L8_2afieldS4430.$0);
    _M0L8_2afieldS4429
    = (struct _M0TPC16string10StringView){
      _M0L4selfS728->$0_1, _M0L4selfS728->$0_2, _M0L4selfS728->$0_0
    };
    moonbit_decref(_M0L8_2afieldS4429.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS728);
  }
  _M0L11end__columnS2297 = _M0L8_2afieldS4185;
  if (_M0L6loggerS750.$1) {
    moonbit_incref(_M0L6loggerS750.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS750.$0->$method_2(_M0L6loggerS750.$1, _M0L11end__columnS2297);
  if (_M0L6loggerS750.$1) {
    moonbit_incref(_M0L6loggerS750.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS750.$0->$method_3(_M0L6loggerS750.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS750.$0->$method_2(_M0L6loggerS750.$1, _M0L15_2amodule__nameS746);
  return 0;
}

uint32_t _M0MPC15bytes9BytesView25unsafe__extract__uint__le(
  struct _M0TPC15bytes9BytesView _M0L2bsS719,
  int32_t _M0L6offsetS720,
  int32_t _M0L3lenS717
) {
  int32_t _M0L6_2atmpS2292;
  int32_t _M0L13bytes__neededS716;
  uint32_t _M0L2b0S718;
  #line 664 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
  _M0L6_2atmpS2292 = _M0L3lenS717 + 7;
  _M0L13bytes__neededS716 = _M0L6_2atmpS2292 / 8;
  moonbit_incref(_M0L2bsS719.$0);
  #line 674 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
  _M0L2b0S718
  = _M0MPC15bytes9BytesView21unsafe__extract__byte(_M0L2bsS719, _M0L6offsetS720, 8);
  switch (_M0L13bytes__neededS716) {
    case 2: {
      int32_t _M0L6_2atmpS2275 = _M0L6offsetS720 + 8;
      int32_t _M0L6_2atmpS2276 = _M0L3lenS717 - 8;
      uint32_t _M0L2b1S721;
      uint32_t _M0L6_2atmpS2274;
      #line 677 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L2b1S721
      = _M0MPC15bytes9BytesView21unsafe__extract__byte(_M0L2bsS719, _M0L6_2atmpS2275, _M0L6_2atmpS2276);
      _M0L6_2atmpS2274 = _M0L2b1S721 << 8;
      return _M0L6_2atmpS2274 | _M0L2b0S718;
      break;
    }
    
    case 3: {
      int32_t _M0L6_2atmpS2282 = _M0L6offsetS720 + 8;
      uint32_t _M0L2b1S722;
      int32_t _M0L6_2atmpS2280;
      int32_t _M0L6_2atmpS2281;
      uint32_t _M0L2b2S723;
      uint32_t _M0L6_2atmpS2278;
      uint32_t _M0L6_2atmpS2279;
      uint32_t _M0L6_2atmpS2277;
      moonbit_incref(_M0L2bsS719.$0);
      #line 681 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L2b1S722
      = _M0MPC15bytes9BytesView21unsafe__extract__byte(_M0L2bsS719, _M0L6_2atmpS2282, 8);
      _M0L6_2atmpS2280 = _M0L6offsetS720 + 16;
      _M0L6_2atmpS2281 = _M0L3lenS717 - 16;
      #line 682 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L2b2S723
      = _M0MPC15bytes9BytesView21unsafe__extract__byte(_M0L2bsS719, _M0L6_2atmpS2280, _M0L6_2atmpS2281);
      _M0L6_2atmpS2278 = _M0L2b2S723 << 16;
      _M0L6_2atmpS2279 = _M0L2b1S722 << 8;
      _M0L6_2atmpS2277 = _M0L6_2atmpS2278 | _M0L6_2atmpS2279;
      return _M0L6_2atmpS2277 | _M0L2b0S718;
      break;
    }
    
    case 4: {
      int32_t _M0L6_2atmpS2291 = _M0L6offsetS720 + 8;
      uint32_t _M0L2b1S724;
      int32_t _M0L6_2atmpS2290;
      uint32_t _M0L2b2S725;
      int32_t _M0L6_2atmpS2288;
      int32_t _M0L6_2atmpS2289;
      uint32_t _M0L2b3S726;
      uint32_t _M0L6_2atmpS2286;
      uint32_t _M0L6_2atmpS2287;
      uint32_t _M0L6_2atmpS2284;
      uint32_t _M0L6_2atmpS2285;
      uint32_t _M0L6_2atmpS2283;
      moonbit_incref(_M0L2bsS719.$0);
      #line 686 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L2b1S724
      = _M0MPC15bytes9BytesView21unsafe__extract__byte(_M0L2bsS719, _M0L6_2atmpS2291, 8);
      _M0L6_2atmpS2290 = _M0L6offsetS720 + 16;
      moonbit_incref(_M0L2bsS719.$0);
      #line 687 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L2b2S725
      = _M0MPC15bytes9BytesView21unsafe__extract__byte(_M0L2bsS719, _M0L6_2atmpS2290, 8);
      _M0L6_2atmpS2288 = _M0L6offsetS720 + 24;
      _M0L6_2atmpS2289 = _M0L3lenS717 - 24;
      #line 688 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L2b3S726
      = _M0MPC15bytes9BytesView21unsafe__extract__byte(_M0L2bsS719, _M0L6_2atmpS2288, _M0L6_2atmpS2289);
      _M0L6_2atmpS2286 = _M0L2b3S726 << 24;
      _M0L6_2atmpS2287 = _M0L2b2S725 << 16;
      _M0L6_2atmpS2284 = _M0L6_2atmpS2286 | _M0L6_2atmpS2287;
      _M0L6_2atmpS2285 = _M0L2b1S724 << 8;
      _M0L6_2atmpS2283 = _M0L6_2atmpS2284 | _M0L6_2atmpS2285;
      return _M0L6_2atmpS2283 | _M0L2b0S718;
      break;
    }
    default: {
      moonbit_decref(_M0L2bsS719.$0);
      #line 691 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      return _M0FPB5abortGjE((moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data);
      break;
    }
  }
}

uint32_t _M0MPC15bytes9BytesView21unsafe__extract__byte(
  struct _M0TPC15bytes9BytesView _M0L2bsS705,
  int32_t _M0L6offsetS703,
  int32_t _M0L3lenS706
) {
  int32_t _M0L11byte__indexS702;
  int32_t _M0L6_2atmpS2255;
  #line 626 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
  _M0L11byte__indexS702 = _M0L6offsetS703 >> 3;
  _M0L6_2atmpS2255 = _M0L6offsetS703 & 7;
  if (_M0L6_2atmpS2255 == 0) {
    int32_t _M0L4byteS704;
    int32_t _M0L6_2atmpS2257;
    int32_t _M0L6_2atmpS2256;
    #line 636 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
    _M0L4byteS704
    = _M0MPC15bytes9BytesView11unsafe__get(_M0L2bsS705, _M0L11byte__indexS702);
    _M0L6_2atmpS2257 = 8 - _M0L3lenS706;
    #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
    _M0L6_2atmpS2256
    = _M0IPC14byte4BytePB3Shr3shr(_M0L4byteS704, _M0L6_2atmpS2257);
    #line 637 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
    return _M0MPC14byte4Byte8to__uint(_M0L6_2atmpS2256);
  } else {
    int32_t _M0L6_2atmpS2259 = _M0L6offsetS703 & 7;
    int32_t _M0L6_2atmpS2258 = _M0L6_2atmpS2259 + _M0L3lenS706;
    if (_M0L6_2atmpS2258 <= 8) {
      int32_t _M0L6_2atmpS2264;
      uint32_t _M0L4byteS707;
      int32_t _M0L6_2atmpS2263;
      int32_t _M0L6_2atmpS2262;
      int32_t _M0L5shiftS708;
      uint32_t _M0L6_2atmpS2261;
      uint32_t _M0L4maskS709;
      uint32_t _M0L6_2atmpS2260;
      #line 640 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L6_2atmpS2264
      = _M0MPC15bytes9BytesView11unsafe__get(_M0L2bsS705, _M0L11byte__indexS702);
      #line 640 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L4byteS707 = _M0MPC14byte4Byte8to__uint(_M0L6_2atmpS2264);
      _M0L6_2atmpS2263 = _M0L6offsetS703 & 7;
      _M0L6_2atmpS2262 = _M0L6_2atmpS2263 + _M0L3lenS706;
      _M0L5shiftS708 = 8 - _M0L6_2atmpS2262;
      _M0L6_2atmpS2261 = 1u << (_M0L3lenS706 & 31);
      _M0L4maskS709 = _M0L6_2atmpS2261 - 1u;
      _M0L6_2atmpS2260 = _M0L4byteS707 >> (_M0L5shiftS708 & 31);
      return _M0L6_2atmpS2260 & _M0L4maskS709;
    } else {
      int32_t _M0L6_2atmpS2273;
      uint32_t _M0L2b0S710;
      int32_t _M0L6_2atmpS2272;
      int32_t _M0L6_2atmpS2271;
      uint32_t _M0L2b1S711;
      uint32_t _M0L6_2atmpS2270;
      uint32_t _M0L4dataS712;
      int32_t _M0L6_2atmpS2269;
      int32_t _M0L6_2atmpS2268;
      uint32_t _M0L6_2atmpS2267;
      uint32_t _M0L9bit__maskS713;
      uint32_t _M0L4dataS714;
      int32_t _M0L6_2atmpS2266;
      int32_t _M0L6_2atmpS2265;
      int32_t _M0L5shiftS715;
      moonbit_incref(_M0L2bsS705.$0);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L6_2atmpS2273
      = _M0MPC15bytes9BytesView11unsafe__get(_M0L2bsS705, _M0L11byte__indexS702);
      #line 646 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L2b0S710 = _M0MPC14byte4Byte8to__uint(_M0L6_2atmpS2273);
      _M0L6_2atmpS2272 = _M0L11byte__indexS702 + 1;
      #line 647 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L6_2atmpS2271
      = _M0MPC15bytes9BytesView11unsafe__get(_M0L2bsS705, _M0L6_2atmpS2272);
      #line 647 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bitstring.mbt"
      _M0L2b1S711 = _M0MPC14byte4Byte8to__uint(_M0L6_2atmpS2271);
      _M0L6_2atmpS2270 = _M0L2b0S710 << 8;
      _M0L4dataS712 = _M0L6_2atmpS2270 | _M0L2b1S711;
      _M0L6_2atmpS2269 = _M0L6offsetS703 & 7;
      _M0L6_2atmpS2268 = 16 - _M0L6_2atmpS2269;
      _M0L6_2atmpS2267 = 1u << (_M0L6_2atmpS2268 & 31);
      _M0L9bit__maskS713 = _M0L6_2atmpS2267 - 1u;
      _M0L4dataS714 = _M0L4dataS712 & _M0L9bit__maskS713;
      _M0L6_2atmpS2266 = _M0L6offsetS703 & 7;
      _M0L6_2atmpS2265 = _M0L6_2atmpS2266 + _M0L3lenS706;
      _M0L5shiftS715 = 16 - _M0L6_2atmpS2265;
      return _M0L4dataS714 >> (_M0L5shiftS715 & 31);
    }
  }
}

int32_t _M0IPC14byte4BytePB3Shr3shr(
  int32_t _M0L4selfS700,
  int32_t _M0L5countS701
) {
  uint32_t _M0L6_2atmpS2254;
  uint32_t _M0L6_2atmpS2253;
  int32_t _M0L6_2atmpS2252;
  #line 371 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  #line 372 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2254 = _M0MPC14byte4Byte8to__uint(_M0L4selfS700);
  _M0L6_2atmpS2253 = _M0L6_2atmpS2254 >> (_M0L5countS701 & 31);
  _M0L6_2atmpS2252 = *(int32_t*)&_M0L6_2atmpS2253;
  return _M0L6_2atmpS2252 & 0xff;
}

moonbit_bytes_t _M0MPC15bytes5Bytes11from__array(
  struct _M0TPB9ArrayViewGyE _M0L3arrS698
) {
  int32_t _M0L6_2atmpS2247;
  struct _M0R44Bytes_3a_3afrom__array_2eanon__u2249__l455__* _closure_4568;
  struct _M0TWuEu* _M0L6_2atmpS2248;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  moonbit_incref(_M0L3arrS698.$0);
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS2247 = _M0MPC15array9ArrayView6lengthGyE(_M0L3arrS698);
  _closure_4568
  = (struct _M0R44Bytes_3a_3afrom__array_2eanon__u2249__l455__*)moonbit_malloc(sizeof(struct _M0R44Bytes_3a_3afrom__array_2eanon__u2249__l455__));
  Moonbit_object_header(_closure_4568)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R44Bytes_3a_3afrom__array_2eanon__u2249__l455__, $0_0) >> 2, 1, 0);
  _closure_4568->code = &_M0MPC15bytes5Bytes11from__arrayC2249l455;
  _closure_4568->$0_0 = _M0L3arrS698.$0;
  _closure_4568->$0_1 = _M0L3arrS698.$1;
  _closure_4568->$0_2 = _M0L3arrS698.$2;
  _M0L6_2atmpS2248 = (struct _M0TWuEu*)_closure_4568;
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  return _M0MPC15bytes5Bytes5makei(_M0L6_2atmpS2247, _M0L6_2atmpS2248);
}

int32_t _M0MPC15bytes5Bytes11from__arrayC2249l455(
  struct _M0TWuEu* _M0L6_2aenvS2250,
  int32_t _M0L1iS699
) {
  struct _M0R44Bytes_3a_3afrom__array_2eanon__u2249__l455__* _M0L14_2acasted__envS2251;
  struct _M0TPB9ArrayViewGyE _M0L8_2afieldS4194;
  int32_t _M0L6_2acntS4435;
  struct _M0TPB9ArrayViewGyE _M0L3arrS698;
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L14_2acasted__envS2251
  = (struct _M0R44Bytes_3a_3afrom__array_2eanon__u2249__l455__*)_M0L6_2aenvS2250;
  _M0L8_2afieldS4194
  = (struct _M0TPB9ArrayViewGyE){
    _M0L14_2acasted__envS2251->$0_1,
      _M0L14_2acasted__envS2251->$0_2,
      _M0L14_2acasted__envS2251->$0_0
  };
  _M0L6_2acntS4435 = Moonbit_object_header(_M0L14_2acasted__envS2251)->rc;
  if (_M0L6_2acntS4435 > 1) {
    int32_t _M0L11_2anew__cntS4436 = _M0L6_2acntS4435 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2251)->rc
    = _M0L11_2anew__cntS4436;
    moonbit_incref(_M0L8_2afieldS4194.$0);
  } else if (_M0L6_2acntS4435 == 1) {
    #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_free(_M0L14_2acasted__envS2251);
  }
  _M0L3arrS698 = _M0L8_2afieldS4194;
  #line 455 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  return _M0MPC15array9ArrayView2atGyE(_M0L3arrS698, _M0L1iS699);
}

int32_t _M0MPC15array10FixedArray17blit__from__bytes(
  moonbit_bytes_t _M0L4selfS695,
  int32_t _M0L13bytes__offsetS690,
  moonbit_bytes_t _M0L3srcS697,
  int32_t _M0L11src__offsetS693,
  int32_t _M0L6lengthS691
) {
  int32_t _M0L6_2atmpS2246;
  int32_t _M0L2e1S689;
  int32_t _M0L6_2atmpS2245;
  int32_t _M0L2e2S692;
  int32_t _M0L4len1S694;
  int32_t _M0L4len2S696;
  int32_t _if__result_4569;
  #line 153 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS2246 = _M0L13bytes__offsetS690 + _M0L6lengthS691;
  _M0L2e1S689 = _M0L6_2atmpS2246 - 1;
  _M0L6_2atmpS2245 = _M0L11src__offsetS693 + _M0L6lengthS691;
  _M0L2e2S692 = _M0L6_2atmpS2245 - 1;
  _M0L4len1S694 = Moonbit_array_length(_M0L4selfS695);
  _M0L4len2S696 = Moonbit_array_length(_M0L3srcS697);
  if (_M0L6lengthS691 >= 0) {
    if (_M0L13bytes__offsetS690 >= 0) {
      if (_M0L2e1S689 < _M0L4len1S694) {
        if (_M0L11src__offsetS693 >= 0) {
          _if__result_4569 = _M0L2e2S692 < _M0L4len2S696;
        } else {
          _if__result_4569 = 0;
        }
      } else {
        _if__result_4569 = 0;
      }
    } else {
      _if__result_4569 = 0;
    }
  } else {
    _if__result_4569 = 0;
  }
  if (_if__result_4569) {
    moonbit_bytes_t _M0L6_2atmpS2244 = _M0L3srcS697;
    #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L4selfS695, _M0L13bytes__offsetS690, _M0L6_2atmpS2244, _M0L11src__offsetS693, _M0L6lengthS691);
  } else {
    moonbit_decref(_M0L3srcS697);
    moonbit_decref(_M0L4selfS695);
    #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

struct _M0TWEOc* _M0MPC15bytes9BytesView4iter(
  struct _M0TPC15bytes9BytesView _M0L4selfS687
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS685;
  int32_t _M0L3lenS686;
  struct _M0R41BytesView_3a_3aiter_2eanon__u2237__l234__* _closure_4570;
  struct _M0TWEOc* _M0L6_2atmpS2236;
  #line 230 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L1iS685
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS685)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS685->$0 = 0;
  moonbit_incref(_M0L4selfS687.$0);
  #line 233 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L3lenS686 = _M0MPC15bytes9BytesView6length(_M0L4selfS687);
  _closure_4570
  = (struct _M0R41BytesView_3a_3aiter_2eanon__u2237__l234__*)moonbit_malloc(sizeof(struct _M0R41BytesView_3a_3aiter_2eanon__u2237__l234__));
  Moonbit_object_header(_closure_4570)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R41BytesView_3a_3aiter_2eanon__u2237__l234__, $0_0) >> 2, 2, 0);
  _closure_4570->code = &_M0MPC15bytes9BytesView4iterC2237l234;
  _closure_4570->$0_0 = _M0L4selfS687.$0;
  _closure_4570->$0_1 = _M0L4selfS687.$1;
  _closure_4570->$0_2 = _M0L4selfS687.$2;
  _closure_4570->$1 = _M0L3lenS686;
  _closure_4570->$2 = _M0L1iS685;
  _M0L6_2atmpS2236 = (struct _M0TWEOc*)_closure_4570;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  return _M0MPB4Iter3newGyE(_M0L6_2atmpS2236);
}

int32_t _M0MPC15bytes9BytesView4iterC2237l234(
  struct _M0TWEOc* _M0L6_2aenvS2238
) {
  struct _M0R41BytesView_3a_3aiter_2eanon__u2237__l234__* _M0L14_2acasted__envS2239;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4196;
  struct _M0TPC13ref3RefGiE* _M0L1iS685;
  int32_t _M0L3lenS686;
  struct _M0TPC15bytes9BytesView _M0L8_2afieldS4195;
  int32_t _M0L6_2acntS4437;
  struct _M0TPC15bytes9BytesView _M0L4selfS687;
  int32_t _M0L3valS2240;
  #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L14_2acasted__envS2239
  = (struct _M0R41BytesView_3a_3aiter_2eanon__u2237__l234__*)_M0L6_2aenvS2238;
  _M0L8_2afieldS4196 = _M0L14_2acasted__envS2239->$2;
  _M0L1iS685 = _M0L8_2afieldS4196;
  _M0L3lenS686 = _M0L14_2acasted__envS2239->$1;
  _M0L8_2afieldS4195
  = (struct _M0TPC15bytes9BytesView){
    _M0L14_2acasted__envS2239->$0_1,
      _M0L14_2acasted__envS2239->$0_2,
      _M0L14_2acasted__envS2239->$0_0
  };
  _M0L6_2acntS4437 = Moonbit_object_header(_M0L14_2acasted__envS2239)->rc;
  if (_M0L6_2acntS4437 > 1) {
    int32_t _M0L11_2anew__cntS4438 = _M0L6_2acntS4437 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2239)->rc
    = _M0L11_2anew__cntS4438;
    moonbit_incref(_M0L1iS685);
    moonbit_incref(_M0L8_2afieldS4195.$0);
  } else if (_M0L6_2acntS4437 == 1) {
    #line 234 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
    moonbit_free(_M0L14_2acasted__envS2239);
  }
  _M0L4selfS687 = _M0L8_2afieldS4195;
  _M0L3valS2240 = _M0L1iS685->$0;
  if (_M0L3valS2240 < _M0L3lenS686) {
    int32_t _M0L3valS2243 = _M0L1iS685->$0;
    int32_t _M0L6resultS688;
    int32_t _M0L3valS2242;
    int32_t _M0L6_2atmpS2241;
    #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
    _M0L6resultS688
    = _M0MPC15bytes9BytesView11unsafe__get(_M0L4selfS687, _M0L3valS2243);
    _M0L3valS2242 = _M0L1iS685->$0;
    _M0L6_2atmpS2241 = _M0L3valS2242 + 1;
    _M0L1iS685->$0 = _M0L6_2atmpS2241;
    moonbit_decref(_M0L1iS685);
    return _M0L6resultS688;
  } else {
    moonbit_decref(_M0L4selfS687.$0);
    moonbit_decref(_M0L1iS685);
    return -1;
  }
}

struct _M0TPC15bytes9BytesView _M0MPC15bytes5Bytes12view_2einner(
  moonbit_bytes_t _M0L4selfS677,
  int32_t _M0L5startS683,
  int64_t _M0L3endS679
) {
  int32_t _M0L3lenS676;
  int32_t _M0L3endS678;
  int32_t _M0L5startS682;
  int32_t _if__result_4571;
  #line 170 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L3lenS676 = Moonbit_array_length(_M0L4selfS677);
  if (_M0L3endS679 == 4294967296ll) {
    _M0L3endS678 = _M0L3lenS676;
  } else {
    int64_t _M0L7_2aSomeS680 = _M0L3endS679;
    int32_t _M0L6_2aendS681 = (int32_t)_M0L7_2aSomeS680;
    if (_M0L6_2aendS681 < 0) {
      _M0L3endS678 = _M0L3lenS676 + _M0L6_2aendS681;
    } else {
      _M0L3endS678 = _M0L6_2aendS681;
    }
  }
  if (_M0L5startS683 < 0) {
    _M0L5startS682 = _M0L3lenS676 + _M0L5startS683;
  } else {
    _M0L5startS682 = _M0L5startS683;
  }
  if (_M0L5startS682 >= 0) {
    if (_M0L5startS682 <= _M0L3endS678) {
      _if__result_4571 = _M0L3endS678 <= _M0L3lenS676;
    } else {
      _if__result_4571 = 0;
    }
  } else {
    _if__result_4571 = 0;
  }
  if (_if__result_4571) {
    int32_t _M0L7_2abindS684 = _M0L3endS678 - _M0L5startS682;
    int32_t _M0L6_2atmpS2235 = _M0L5startS682 + _M0L7_2abindS684;
    return (struct _M0TPC15bytes9BytesView){_M0L5startS682,
                                              _M0L6_2atmpS2235,
                                              _M0L4selfS677};
  } else {
    moonbit_decref(_M0L4selfS677);
    #line 180 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
    return _M0FPB5abortGRPC15bytes9BytesViewE((moonbit_string_t)moonbit_string_literal_22.data, (moonbit_string_t)moonbit_string_literal_23.data);
  }
}

int32_t _M0MPC15bytes9BytesView11unsafe__get(
  struct _M0TPC15bytes9BytesView _M0L4selfS674,
  int32_t _M0L5indexS675
) {
  moonbit_bytes_t _M0L8_2afieldS4199;
  moonbit_bytes_t _M0L5bytesS2232;
  int32_t _M0L8_2afieldS4198;
  int32_t _M0L5startS2234;
  int32_t _M0L6_2atmpS2233;
  int32_t _M0L6_2atmpS4197;
  #line 149 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L8_2afieldS4199 = _M0L4selfS674.$0;
  _M0L5bytesS2232 = _M0L8_2afieldS4199;
  _M0L8_2afieldS4198 = _M0L4selfS674.$1;
  _M0L5startS2234 = _M0L8_2afieldS4198;
  _M0L6_2atmpS2233 = _M0L5startS2234 + _M0L5indexS675;
  if (
    _M0L6_2atmpS2233 < 0
    || _M0L6_2atmpS2233 >= Moonbit_array_length(_M0L5bytesS2232)
  ) {
    #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
    moonbit_panic();
  }
  _M0L6_2atmpS4197 = _M0L5bytesS2232[_M0L6_2atmpS2233];
  moonbit_decref(_M0L5bytesS2232);
  return _M0L6_2atmpS4197;
}

int32_t _M0MPC15bytes9BytesView6length(
  struct _M0TPC15bytes9BytesView _M0L4selfS673
) {
  int32_t _M0L3endS2230;
  int32_t _M0L8_2afieldS4200;
  int32_t _M0L5startS2231;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytesview.mbt"
  _M0L3endS2230 = _M0L4selfS673.$2;
  _M0L8_2afieldS4200 = _M0L4selfS673.$1;
  moonbit_decref(_M0L4selfS673.$0);
  _M0L5startS2231 = _M0L8_2afieldS4200;
  return _M0L3endS2230 - _M0L5startS2231;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS672) {
  moonbit_string_t _M0L6_2atmpS2229;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2229 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS672);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2229);
  moonbit_decref(_M0L6_2atmpS2229);
  return 0;
}

moonbit_bytes_t _M0MPC15bytes5Bytes5makei(
  int32_t _M0L6lengthS667,
  struct _M0TWuEu* _M0L5valueS669
) {
  int32_t _M0L6_2atmpS2228;
  moonbit_bytes_t _M0L3arrS668;
  int32_t _M0L1iS670;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  if (_M0L6lengthS667 <= 0) {
    moonbit_decref(_M0L5valueS669);
    return (moonbit_bytes_t)moonbit_bytes_literal_0.data;
  }
  moonbit_incref(_M0L5valueS669);
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS2228 = _M0L5valueS669->code(_M0L5valueS669, 0);
  _M0L3arrS668
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6lengthS667, _M0L6_2atmpS2228);
  _M0L1iS670 = 1;
  while (1) {
    if (_M0L1iS670 < _M0L6lengthS667) {
      int32_t _M0L6_2atmpS2226;
      int32_t _M0L6_2atmpS2227;
      moonbit_incref(_M0L5valueS669);
      #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      _M0L6_2atmpS2226 = _M0L5valueS669->code(_M0L5valueS669, _M0L1iS670);
      if (_M0L1iS670 < 0 || _M0L1iS670 >= Moonbit_array_length(_M0L3arrS668)) {
        #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        moonbit_panic();
      }
      _M0L3arrS668[_M0L1iS670] = _M0L6_2atmpS2226;
      _M0L6_2atmpS2227 = _M0L1iS670 + 1;
      _M0L1iS670 = _M0L6_2atmpS2227;
      continue;
    } else {
      moonbit_decref(_M0L5valueS669);
    }
    break;
  }
  return _M0L3arrS668;
}

uint32_t* _M0MPC15array10FixedArray4copyGjE(uint32_t* _M0L4selfS665) {
  int32_t _M0L3lenS664;
  #line 1551 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  _M0L3lenS664 = Moonbit_array_length(_M0L4selfS665);
  if (_M0L3lenS664 == 0) {
    moonbit_decref(_M0L4selfS665);
    return (uint32_t*)moonbit_empty_int32_array;
  } else {
    uint32_t _M0L6_2atmpS2225;
    uint32_t* _M0L3arrS666;
    if (0 < 0 || 0 >= Moonbit_array_length(_M0L4selfS665)) {
      #line 1557 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2225 = (uint32_t)_M0L4selfS665[0];
    _M0L3arrS666
    = (uint32_t*)moonbit_make_int32_array(_M0L3lenS664, _M0L6_2atmpS2225);
    moonbit_incref(_M0L3arrS666);
    #line 1558 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGjE(_M0L3arrS666, 0, _M0L4selfS665, 0, _M0L3lenS664);
    return _M0L3arrS666;
  }
}

struct moonbit_result_0 _M0FPB12assert__true(
  int32_t _M0L1xS659,
  moonbit_string_t _M0L3msgS661,
  moonbit_string_t _M0L3locS663
) {
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
  if (!_M0L1xS659) {
    moonbit_string_t _M0L9fail__msgS660;
    if (_M0L3msgS661 == 0) {
      moonbit_string_t _M0L6_2atmpS2223;
      moonbit_string_t _M0L6_2atmpS4202;
      moonbit_string_t _M0L6_2atmpS2222;
      moonbit_string_t _M0L6_2atmpS4201;
      if (_M0L3msgS661) {
        moonbit_decref(_M0L3msgS661);
      }
      #line 127 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS2223
      = _M0IP016_24default__implPB4Show10to__stringGbE(_M0L1xS659);
      #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4202
      = moonbit_add_string((moonbit_string_t)moonbit_string_literal_24.data, _M0L6_2atmpS2223);
      moonbit_decref(_M0L6_2atmpS2223);
      _M0L6_2atmpS2222 = _M0L6_2atmpS4202;
      #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
      _M0L6_2atmpS4201
      = moonbit_add_string(_M0L6_2atmpS2222, (moonbit_string_t)moonbit_string_literal_25.data);
      moonbit_decref(_M0L6_2atmpS2222);
      _M0L9fail__msgS660 = _M0L6_2atmpS4201;
    } else {
      moonbit_string_t _M0L7_2aSomeS662 = _M0L3msgS661;
      _M0L9fail__msgS660 = _M0L7_2aSomeS662;
    }
    #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\assert.mbt"
    return _M0FPB4failGuE(_M0L9fail__msgS660, _M0L3locS663);
  } else {
    int32_t _M0L6_2atmpS2224;
    struct moonbit_result_0 _result_4573;
    moonbit_decref(_M0L3locS663);
    if (_M0L3msgS661) {
      moonbit_decref(_M0L3msgS661);
    }
    _M0L6_2atmpS2224 = 0;
    _result_4573.tag = 1;
    _result_4573.data.ok = _M0L6_2atmpS2224;
    return _result_4573;
  }
}

int32_t _M0MPC15array10FixedArray16blit__to_2einnerGyE(
  moonbit_bytes_t _M0L4selfS653,
  moonbit_bytes_t _M0L3dstS652,
  int32_t _M0L3lenS651,
  int32_t _M0L11src__offsetS650,
  int32_t _M0L11dst__offsetS649
) {
  int32_t _if__result_4574;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L11dst__offsetS649 >= 0) {
    if (_M0L11src__offsetS650 >= 0) {
      int32_t _M0L6_2atmpS2184 = _M0L11dst__offsetS649 + _M0L3lenS651;
      int32_t _M0L6_2atmpS2185 = Moonbit_array_length(_M0L3dstS652);
      if (_M0L6_2atmpS2184 <= _M0L6_2atmpS2185) {
        int32_t _M0L6_2atmpS2182 = _M0L11src__offsetS650 + _M0L3lenS651;
        int32_t _M0L6_2atmpS2183 = Moonbit_array_length(_M0L4selfS653);
        _if__result_4574 = _M0L6_2atmpS2182 <= _M0L6_2atmpS2183;
      } else {
        _if__result_4574 = 0;
      }
    } else {
      _if__result_4574 = 0;
    }
  } else {
    _if__result_4574 = 0;
  }
  if (_if__result_4574) {
    #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L3dstS652, _M0L11dst__offsetS649, _M0L4selfS653, _M0L11src__offsetS650, _M0L3lenS651);
  } else {
    moonbit_string_t _M0L6_2atmpS2201;
    moonbit_string_t _M0L6_2atmpS4213;
    moonbit_string_t _M0L6_2atmpS2200;
    moonbit_string_t _M0L6_2atmpS4212;
    moonbit_string_t _M0L6_2atmpS2198;
    moonbit_string_t _M0L6_2atmpS2199;
    moonbit_string_t _M0L6_2atmpS4211;
    moonbit_string_t _M0L6_2atmpS2197;
    moonbit_string_t _M0L6_2atmpS4210;
    moonbit_string_t _M0L6_2atmpS2195;
    moonbit_string_t _M0L6_2atmpS2196;
    moonbit_string_t _M0L6_2atmpS4209;
    moonbit_string_t _M0L6_2atmpS2194;
    moonbit_string_t _M0L6_2atmpS4208;
    moonbit_string_t _M0L6_2atmpS2191;
    int32_t _M0L6_2atmpS4207;
    int32_t _M0L6_2atmpS2193;
    moonbit_string_t _M0L6_2atmpS2192;
    moonbit_string_t _M0L6_2atmpS4206;
    moonbit_string_t _M0L6_2atmpS2190;
    moonbit_string_t _M0L6_2atmpS4205;
    moonbit_string_t _M0L6_2atmpS2187;
    int32_t _M0L6_2atmpS4204;
    int32_t _M0L6_2atmpS2189;
    moonbit_string_t _M0L6_2atmpS2188;
    moonbit_string_t _M0L6_2atmpS4203;
    moonbit_string_t _M0L6_2atmpS2186;
    #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS2201
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L11dst__offsetS649);
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4213
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_26.data, _M0L6_2atmpS2201);
    moonbit_decref(_M0L6_2atmpS2201);
    _M0L6_2atmpS2200 = _M0L6_2atmpS4213;
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4212
    = moonbit_add_string(_M0L6_2atmpS2200, (moonbit_string_t)moonbit_string_literal_27.data);
    moonbit_decref(_M0L6_2atmpS2200);
    _M0L6_2atmpS2198 = _M0L6_2atmpS4212;
    #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS2199
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L11src__offsetS650);
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4211 = moonbit_add_string(_M0L6_2atmpS2198, _M0L6_2atmpS2199);
    moonbit_decref(_M0L6_2atmpS2198);
    moonbit_decref(_M0L6_2atmpS2199);
    _M0L6_2atmpS2197 = _M0L6_2atmpS4211;
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4210
    = moonbit_add_string(_M0L6_2atmpS2197, (moonbit_string_t)moonbit_string_literal_28.data);
    moonbit_decref(_M0L6_2atmpS2197);
    _M0L6_2atmpS2195 = _M0L6_2atmpS4210;
    #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS2196
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L3lenS651);
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4209 = moonbit_add_string(_M0L6_2atmpS2195, _M0L6_2atmpS2196);
    moonbit_decref(_M0L6_2atmpS2195);
    moonbit_decref(_M0L6_2atmpS2196);
    _M0L6_2atmpS2194 = _M0L6_2atmpS4209;
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4208
    = moonbit_add_string(_M0L6_2atmpS2194, (moonbit_string_t)moonbit_string_literal_29.data);
    moonbit_decref(_M0L6_2atmpS2194);
    _M0L6_2atmpS2191 = _M0L6_2atmpS4208;
    _M0L6_2atmpS4207 = Moonbit_array_length(_M0L3dstS652);
    moonbit_decref(_M0L3dstS652);
    _M0L6_2atmpS2193 = _M0L6_2atmpS4207;
    #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS2192
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6_2atmpS2193);
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4206 = moonbit_add_string(_M0L6_2atmpS2191, _M0L6_2atmpS2192);
    moonbit_decref(_M0L6_2atmpS2191);
    moonbit_decref(_M0L6_2atmpS2192);
    _M0L6_2atmpS2190 = _M0L6_2atmpS4206;
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4205
    = moonbit_add_string(_M0L6_2atmpS2190, (moonbit_string_t)moonbit_string_literal_30.data);
    moonbit_decref(_M0L6_2atmpS2190);
    _M0L6_2atmpS2187 = _M0L6_2atmpS4205;
    _M0L6_2atmpS4204 = Moonbit_array_length(_M0L4selfS653);
    moonbit_decref(_M0L4selfS653);
    _M0L6_2atmpS2189 = _M0L6_2atmpS4204;
    #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS2188
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6_2atmpS2189);
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4203 = moonbit_add_string(_M0L6_2atmpS2187, _M0L6_2atmpS2188);
    moonbit_decref(_M0L6_2atmpS2187);
    moonbit_decref(_M0L6_2atmpS2188);
    _M0L6_2atmpS2186 = _M0L6_2atmpS4203;
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0FPB5abortGuE(_M0L6_2atmpS2186, (moonbit_string_t)moonbit_string_literal_31.data);
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray16blit__to_2einnerGjE(
  uint32_t* _M0L4selfS658,
  uint32_t* _M0L3dstS657,
  int32_t _M0L3lenS656,
  int32_t _M0L11src__offsetS655,
  int32_t _M0L11dst__offsetS654
) {
  int32_t _if__result_4575;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L11dst__offsetS654 >= 0) {
    if (_M0L11src__offsetS655 >= 0) {
      int32_t _M0L6_2atmpS2204 = _M0L11dst__offsetS654 + _M0L3lenS656;
      int32_t _M0L6_2atmpS2205 = Moonbit_array_length(_M0L3dstS657);
      if (_M0L6_2atmpS2204 <= _M0L6_2atmpS2205) {
        int32_t _M0L6_2atmpS2202 = _M0L11src__offsetS655 + _M0L3lenS656;
        int32_t _M0L6_2atmpS2203 = Moonbit_array_length(_M0L4selfS658);
        _if__result_4575 = _M0L6_2atmpS2202 <= _M0L6_2atmpS2203;
      } else {
        _if__result_4575 = 0;
      }
    } else {
      _if__result_4575 = 0;
    }
  } else {
    _if__result_4575 = 0;
  }
  if (_if__result_4575) {
    #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0MPC15array10FixedArray12unsafe__blitGjE(_M0L3dstS657, _M0L11dst__offsetS654, _M0L4selfS658, _M0L11src__offsetS655, _M0L3lenS656);
  } else {
    moonbit_string_t _M0L6_2atmpS2221;
    moonbit_string_t _M0L6_2atmpS4224;
    moonbit_string_t _M0L6_2atmpS2220;
    moonbit_string_t _M0L6_2atmpS4223;
    moonbit_string_t _M0L6_2atmpS2218;
    moonbit_string_t _M0L6_2atmpS2219;
    moonbit_string_t _M0L6_2atmpS4222;
    moonbit_string_t _M0L6_2atmpS2217;
    moonbit_string_t _M0L6_2atmpS4221;
    moonbit_string_t _M0L6_2atmpS2215;
    moonbit_string_t _M0L6_2atmpS2216;
    moonbit_string_t _M0L6_2atmpS4220;
    moonbit_string_t _M0L6_2atmpS2214;
    moonbit_string_t _M0L6_2atmpS4219;
    moonbit_string_t _M0L6_2atmpS2211;
    int32_t _M0L6_2atmpS4218;
    int32_t _M0L6_2atmpS2213;
    moonbit_string_t _M0L6_2atmpS2212;
    moonbit_string_t _M0L6_2atmpS4217;
    moonbit_string_t _M0L6_2atmpS2210;
    moonbit_string_t _M0L6_2atmpS4216;
    moonbit_string_t _M0L6_2atmpS2207;
    int32_t _M0L6_2atmpS4215;
    int32_t _M0L6_2atmpS2209;
    moonbit_string_t _M0L6_2atmpS2208;
    moonbit_string_t _M0L6_2atmpS4214;
    moonbit_string_t _M0L6_2atmpS2206;
    #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS2221
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L11dst__offsetS654);
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4224
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_26.data, _M0L6_2atmpS2221);
    moonbit_decref(_M0L6_2atmpS2221);
    _M0L6_2atmpS2220 = _M0L6_2atmpS4224;
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4223
    = moonbit_add_string(_M0L6_2atmpS2220, (moonbit_string_t)moonbit_string_literal_27.data);
    moonbit_decref(_M0L6_2atmpS2220);
    _M0L6_2atmpS2218 = _M0L6_2atmpS4223;
    #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS2219
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L11src__offsetS655);
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4222 = moonbit_add_string(_M0L6_2atmpS2218, _M0L6_2atmpS2219);
    moonbit_decref(_M0L6_2atmpS2218);
    moonbit_decref(_M0L6_2atmpS2219);
    _M0L6_2atmpS2217 = _M0L6_2atmpS4222;
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4221
    = moonbit_add_string(_M0L6_2atmpS2217, (moonbit_string_t)moonbit_string_literal_28.data);
    moonbit_decref(_M0L6_2atmpS2217);
    _M0L6_2atmpS2215 = _M0L6_2atmpS4221;
    #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS2216
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L3lenS656);
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4220 = moonbit_add_string(_M0L6_2atmpS2215, _M0L6_2atmpS2216);
    moonbit_decref(_M0L6_2atmpS2215);
    moonbit_decref(_M0L6_2atmpS2216);
    _M0L6_2atmpS2214 = _M0L6_2atmpS4220;
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4219
    = moonbit_add_string(_M0L6_2atmpS2214, (moonbit_string_t)moonbit_string_literal_29.data);
    moonbit_decref(_M0L6_2atmpS2214);
    _M0L6_2atmpS2211 = _M0L6_2atmpS4219;
    _M0L6_2atmpS4218 = Moonbit_array_length(_M0L3dstS657);
    moonbit_decref(_M0L3dstS657);
    _M0L6_2atmpS2213 = _M0L6_2atmpS4218;
    #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS2212
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6_2atmpS2213);
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4217 = moonbit_add_string(_M0L6_2atmpS2211, _M0L6_2atmpS2212);
    moonbit_decref(_M0L6_2atmpS2211);
    moonbit_decref(_M0L6_2atmpS2212);
    _M0L6_2atmpS2210 = _M0L6_2atmpS4217;
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4216
    = moonbit_add_string(_M0L6_2atmpS2210, (moonbit_string_t)moonbit_string_literal_30.data);
    moonbit_decref(_M0L6_2atmpS2210);
    _M0L6_2atmpS2207 = _M0L6_2atmpS4216;
    _M0L6_2atmpS4215 = Moonbit_array_length(_M0L4selfS658);
    moonbit_decref(_M0L4selfS658);
    _M0L6_2atmpS2209 = _M0L6_2atmpS4215;
    #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS2208
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6_2atmpS2209);
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0L6_2atmpS4214 = moonbit_add_string(_M0L6_2atmpS2207, _M0L6_2atmpS2208);
    moonbit_decref(_M0L6_2atmpS2207);
    moonbit_decref(_M0L6_2atmpS2208);
    _M0L6_2atmpS2206 = _M0L6_2atmpS4214;
    #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
    _M0FPB5abortGuE(_M0L6_2atmpS2206, (moonbit_string_t)moonbit_string_literal_31.data);
  }
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS648,
  struct _M0TPB6Hasher* _M0L6hasherS647
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS647, _M0L4selfS648);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS646,
  struct _M0TPB6Hasher* _M0L6hasherS645
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS645, _M0L4selfS646);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS643,
  moonbit_string_t _M0L5valueS641
) {
  int32_t _M0L7_2abindS640;
  int32_t _M0L1iS642;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS640 = Moonbit_array_length(_M0L5valueS641);
  _M0L1iS642 = 0;
  while (1) {
    if (_M0L1iS642 < _M0L7_2abindS640) {
      int32_t _M0L6_2atmpS2180 = _M0L5valueS641[_M0L1iS642];
      int32_t _M0L6_2atmpS2179 = (int32_t)_M0L6_2atmpS2180;
      uint32_t _M0L6_2atmpS2178 = *(uint32_t*)&_M0L6_2atmpS2179;
      int32_t _M0L6_2atmpS2181;
      moonbit_incref(_M0L4selfS643);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS643, _M0L6_2atmpS2178);
      _M0L6_2atmpS2181 = _M0L1iS642 + 1;
      _M0L1iS642 = _M0L6_2atmpS2181;
      continue;
    } else {
      moonbit_decref(_M0L4selfS643);
      moonbit_decref(_M0L5valueS641);
    }
    break;
  }
  return 0;
}

uint32_t _M0MPC14byte4Byte8to__uint(int32_t _M0L4selfS639) {
  int32_t _M0L6_2atmpS2177;
  #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS2177 = (int32_t)_M0L4selfS639;
  return *(uint32_t*)&_M0L6_2atmpS2177;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS637,
  int32_t _M0L3idxS638
) {
  int32_t _M0L6_2atmpS4225;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4225 = _M0L4selfS637[_M0L3idxS638];
  moonbit_decref(_M0L4selfS637);
  return _M0L6_2atmpS4225;
}

moonbit_bytes_t _M0MPC15bytes5Bytes4make(
  int32_t _M0L3lenS635,
  int32_t _M0L4initS636
) {
  #line 1486 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  if (_M0L3lenS635 < 0) {
    return (moonbit_bytes_t)moonbit_bytes_literal_0.data;
  }
  #line 1490 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return moonbit_make_bytes(_M0L3lenS635, _M0L4initS636);
}

struct _M0TWEOj* _M0MPB4Iter4takeGjE(
  struct _M0TWEOj* _M0L4selfS634,
  int32_t _M0L1nS632
) {
  struct _M0TPC13ref3RefGiE* _M0L9remainingS631;
  struct _M0R52Iter_3a_3atake_7c_5bUInt_5d_7c_2eanon__u2171__l481__* _closure_4577;
  #line 479 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L9remainingS631
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L9remainingS631)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L9remainingS631->$0 = _M0L1nS632;
  _closure_4577
  = (struct _M0R52Iter_3a_3atake_7c_5bUInt_5d_7c_2eanon__u2171__l481__*)moonbit_malloc(sizeof(struct _M0R52Iter_3a_3atake_7c_5bUInt_5d_7c_2eanon__u2171__l481__));
  Moonbit_object_header(_closure_4577)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R52Iter_3a_3atake_7c_5bUInt_5d_7c_2eanon__u2171__l481__, $0) >> 2, 2, 0);
  _closure_4577->code = &_M0MPB4Iter4takeGjEC2171l481;
  _closure_4577->$0 = _M0L4selfS634;
  _closure_4577->$1 = _M0L9remainingS631;
  return (struct _M0TWEOj*)_closure_4577;
}

int64_t _M0MPB4Iter4takeGjEC2171l481(struct _M0TWEOj* _M0L6_2aenvS2172) {
  struct _M0R52Iter_3a_3atake_7c_5bUInt_5d_7c_2eanon__u2171__l481__* _M0L14_2acasted__envS2173;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4227;
  struct _M0TPC13ref3RefGiE* _M0L9remainingS631;
  struct _M0TWEOj* _M0L8_2afieldS4226;
  int32_t _M0L6_2acntS4439;
  struct _M0TWEOj* _M0L4selfS634;
  int32_t _M0L3valS2174;
  #line 481 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L14_2acasted__envS2173
  = (struct _M0R52Iter_3a_3atake_7c_5bUInt_5d_7c_2eanon__u2171__l481__*)_M0L6_2aenvS2172;
  _M0L8_2afieldS4227 = _M0L14_2acasted__envS2173->$1;
  _M0L9remainingS631 = _M0L8_2afieldS4227;
  _M0L8_2afieldS4226 = _M0L14_2acasted__envS2173->$0;
  _M0L6_2acntS4439 = Moonbit_object_header(_M0L14_2acasted__envS2173)->rc;
  if (_M0L6_2acntS4439 > 1) {
    int32_t _M0L11_2anew__cntS4440 = _M0L6_2acntS4439 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2173)->rc
    = _M0L11_2anew__cntS4440;
    moonbit_incref(_M0L9remainingS631);
    moonbit_incref(_M0L8_2afieldS4226);
  } else if (_M0L6_2acntS4439 == 1) {
    #line 481 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    moonbit_free(_M0L14_2acasted__envS2173);
  }
  _M0L4selfS634 = _M0L8_2afieldS4226;
  _M0L3valS2174 = _M0L9remainingS631->$0;
  if (_M0L3valS2174 > 0) {
    int64_t _M0L6resultS633;
    #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    _M0L6resultS633 = _M0MPB4Iter4nextGjE(_M0L4selfS634);
    if (_M0L6resultS633 == 4294967296ll) {
      moonbit_decref(_M0L9remainingS631);
    } else {
      int32_t _M0L3valS2176 = _M0L9remainingS631->$0;
      int32_t _M0L6_2atmpS2175 = _M0L3valS2176 - 1;
      _M0L9remainingS631->$0 = _M0L6_2atmpS2175;
      moonbit_decref(_M0L9remainingS631);
    }
    return _M0L6resultS633;
  } else {
    moonbit_decref(_M0L4selfS634);
    moonbit_decref(_M0L9remainingS631);
    return 4294967296ll;
  }
}

int32_t _M0MPB4Iter4eachGjE(
  struct _M0TWEOj* _M0L4selfS626,
  struct _M0TWjEu* _M0L1fS629
) {
  #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  while (1) {
    int64_t _M0L7_2abindS625;
    moonbit_incref(_M0L4selfS626);
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
    _M0L7_2abindS625 = _M0MPB4Iter4nextGjE(_M0L4selfS626);
    if (_M0L7_2abindS625 == 4294967296ll) {
      moonbit_decref(_M0L1fS629);
      moonbit_decref(_M0L4selfS626);
    } else {
      int64_t _M0L7_2aSomeS627 = _M0L7_2abindS625;
      int32_t _M0L4_2axS628 = (int32_t)_M0L7_2aSomeS627;
      moonbit_incref(_M0L1fS629);
      #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
      _M0L1fS629->code(_M0L1fS629, _M0L4_2axS628);
      continue;
    }
    break;
  }
  return 0;
}

uint64_t _M0MPC14uint4UInt10to__uint64(uint32_t _M0L4selfS624) {
  #line 2529 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return (uint64_t)_M0L4selfS624;
}

int32_t _M0MPC15array10FixedArray12fill_2einnerGyE(
  moonbit_bytes_t _M0L4selfS617,
  int32_t _M0L5valueS623,
  int32_t _M0L5startS618,
  int64_t _M0L3endS620
) {
  int32_t _M0L13array__lengthS616;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  _M0L13array__lengthS616 = Moonbit_array_length(_M0L4selfS617);
  if (_M0L13array__lengthS616 > 0) {
    int32_t _if__result_4579;
    if (_M0L5startS618 >= 0) {
      _if__result_4579 = _M0L5startS618 < _M0L13array__lengthS616;
    } else {
      _if__result_4579 = 0;
    }
    if (_if__result_4579) {
      int32_t _M0L6lengthS619;
      if (_M0L3endS620 == 4294967296ll) {
        _M0L6lengthS619 = _M0L13array__lengthS616 - _M0L5startS618;
      } else {
        int64_t _M0L7_2aSomeS621 = _M0L3endS620;
        int32_t _M0L4_2aeS622 = (int32_t)_M0L7_2aSomeS621;
        int32_t _if__result_4580;
        if (_M0L4_2aeS622 >= _M0L5startS618) {
          _if__result_4580 = _M0L4_2aeS622 <= _M0L13array__lengthS616;
        } else {
          _if__result_4580 = 0;
        }
        if (_if__result_4580) {
          _M0L6lengthS619 = _M0L4_2aeS622 - _M0L5startS618;
        } else {
          #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
          moonbit_panic();
        }
      }
      #line 122 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
      _M0MPC15array10FixedArray15unchecked__fillGyE(_M0L4selfS617, _M0L5startS618, _M0L5valueS623, _M0L6lengthS619);
    } else {
      moonbit_decref(_M0L4selfS617);
      #line 114 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
      moonbit_panic();
    }
  } else {
    moonbit_decref(_M0L4selfS617);
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray15unchecked__fillGyE(
  moonbit_bytes_t _M0L4selfS613,
  int32_t _M0L5startS610,
  int32_t _M0L5valueS614,
  int32_t _M0L3lenS611
) {
  int32_t _M0L7_2abindS609;
  int32_t _M0L1iS612;
  #line 126 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  _M0L7_2abindS609 = _M0L5startS610 + _M0L3lenS611;
  _M0L1iS612 = _M0L5startS610;
  while (1) {
    if (_M0L1iS612 < _M0L7_2abindS609) {
      int32_t _M0L6_2atmpS2170;
      if (
        _M0L1iS612 < 0 || _M0L1iS612 >= Moonbit_array_length(_M0L4selfS613)
      ) {
        #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
        moonbit_panic();
      }
      _M0L4selfS613[_M0L1iS612] = _M0L5valueS614;
      _M0L6_2atmpS2170 = _M0L1iS612 + 1;
      _M0L1iS612 = _M0L6_2atmpS2170;
      continue;
    } else {
      moonbit_decref(_M0L4selfS613);
    }
    break;
  }
  return 0;
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS596,
  int32_t _M0L3keyS592
) {
  int32_t _M0L4hashS591;
  int32_t _M0L14capacity__maskS2155;
  int32_t _M0L6_2atmpS2154;
  int32_t _M0L1iS593;
  int32_t _M0L3idxS594;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS591 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS592);
  _M0L14capacity__maskS2155 = _M0L4selfS596->$3;
  _M0L6_2atmpS2154 = _M0L4hashS591 & _M0L14capacity__maskS2155;
  _M0L1iS593 = 0;
  _M0L3idxS594 = _M0L6_2atmpS2154;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4231 =
      _M0L4selfS596->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2153 =
      _M0L8_2afieldS4231;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4230;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS595;
    if (
      _M0L3idxS594 < 0
      || _M0L3idxS594 >= Moonbit_array_length(_M0L7entriesS2153)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4230
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2153[
        _M0L3idxS594
      ];
    _M0L7_2abindS595 = _M0L6_2atmpS4230;
    if (_M0L7_2abindS595 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2142;
      if (_M0L7_2abindS595) {
        moonbit_incref(_M0L7_2abindS595);
      }
      moonbit_decref(_M0L4selfS596);
      if (_M0L7_2abindS595) {
        moonbit_decref(_M0L7_2abindS595);
      }
      _M0L6_2atmpS2142 = 0;
      return _M0L6_2atmpS2142;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS597 =
        _M0L7_2abindS595;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS598 =
        _M0L7_2aSomeS597;
      int32_t _M0L4hashS2144 = _M0L8_2aentryS598->$3;
      int32_t _if__result_4583;
      int32_t _M0L8_2afieldS4228;
      int32_t _M0L3pslS2147;
      int32_t _M0L6_2atmpS2149;
      int32_t _M0L6_2atmpS2151;
      int32_t _M0L14capacity__maskS2152;
      int32_t _M0L6_2atmpS2150;
      if (_M0L4hashS2144 == _M0L4hashS591) {
        int32_t _M0L3keyS2143 = _M0L8_2aentryS598->$4;
        _if__result_4583 = _M0L3keyS2143 == _M0L3keyS592;
      } else {
        _if__result_4583 = 0;
      }
      if (_if__result_4583) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4229;
        int32_t _M0L6_2acntS4441;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2146;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2145;
        moonbit_incref(_M0L8_2aentryS598);
        moonbit_decref(_M0L4selfS596);
        _M0L8_2afieldS4229 = _M0L8_2aentryS598->$5;
        _M0L6_2acntS4441 = Moonbit_object_header(_M0L8_2aentryS598)->rc;
        if (_M0L6_2acntS4441 > 1) {
          int32_t _M0L11_2anew__cntS4443 = _M0L6_2acntS4441 - 1;
          Moonbit_object_header(_M0L8_2aentryS598)->rc
          = _M0L11_2anew__cntS4443;
          moonbit_incref(_M0L8_2afieldS4229);
        } else if (_M0L6_2acntS4441 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4442 =
            _M0L8_2aentryS598->$1;
          if (_M0L8_2afieldS4442) {
            moonbit_decref(_M0L8_2afieldS4442);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS598);
        }
        _M0L5valueS2146 = _M0L8_2afieldS4229;
        _M0L6_2atmpS2145 = _M0L5valueS2146;
        return _M0L6_2atmpS2145;
      } else {
        moonbit_incref(_M0L8_2aentryS598);
      }
      _M0L8_2afieldS4228 = _M0L8_2aentryS598->$2;
      moonbit_decref(_M0L8_2aentryS598);
      _M0L3pslS2147 = _M0L8_2afieldS4228;
      if (_M0L1iS593 > _M0L3pslS2147) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2148;
        moonbit_decref(_M0L4selfS596);
        _M0L6_2atmpS2148 = 0;
        return _M0L6_2atmpS2148;
      }
      _M0L6_2atmpS2149 = _M0L1iS593 + 1;
      _M0L6_2atmpS2151 = _M0L3idxS594 + 1;
      _M0L14capacity__maskS2152 = _M0L4selfS596->$3;
      _M0L6_2atmpS2150 = _M0L6_2atmpS2151 & _M0L14capacity__maskS2152;
      _M0L1iS593 = _M0L6_2atmpS2149;
      _M0L3idxS594 = _M0L6_2atmpS2150;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS605,
  moonbit_string_t _M0L3keyS601
) {
  int32_t _M0L4hashS600;
  int32_t _M0L14capacity__maskS2169;
  int32_t _M0L6_2atmpS2168;
  int32_t _M0L1iS602;
  int32_t _M0L3idxS603;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS601);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS600 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS601);
  _M0L14capacity__maskS2169 = _M0L4selfS605->$3;
  _M0L6_2atmpS2168 = _M0L4hashS600 & _M0L14capacity__maskS2169;
  _M0L1iS602 = 0;
  _M0L3idxS603 = _M0L6_2atmpS2168;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4237 =
      _M0L4selfS605->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2167 =
      _M0L8_2afieldS4237;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4236;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS604;
    if (
      _M0L3idxS603 < 0
      || _M0L3idxS603 >= Moonbit_array_length(_M0L7entriesS2167)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4236
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2167[
        _M0L3idxS603
      ];
    _M0L7_2abindS604 = _M0L6_2atmpS4236;
    if (_M0L7_2abindS604 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2156;
      if (_M0L7_2abindS604) {
        moonbit_incref(_M0L7_2abindS604);
      }
      moonbit_decref(_M0L4selfS605);
      if (_M0L7_2abindS604) {
        moonbit_decref(_M0L7_2abindS604);
      }
      moonbit_decref(_M0L3keyS601);
      _M0L6_2atmpS2156 = 0;
      return _M0L6_2atmpS2156;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS606 =
        _M0L7_2abindS604;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS607 =
        _M0L7_2aSomeS606;
      int32_t _M0L4hashS2158 = _M0L8_2aentryS607->$3;
      int32_t _if__result_4585;
      int32_t _M0L8_2afieldS4232;
      int32_t _M0L3pslS2161;
      int32_t _M0L6_2atmpS2163;
      int32_t _M0L6_2atmpS2165;
      int32_t _M0L14capacity__maskS2166;
      int32_t _M0L6_2atmpS2164;
      if (_M0L4hashS2158 == _M0L4hashS600) {
        moonbit_string_t _M0L8_2afieldS4235 = _M0L8_2aentryS607->$4;
        moonbit_string_t _M0L3keyS2157 = _M0L8_2afieldS4235;
        int32_t _M0L6_2atmpS4234;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4234
        = moonbit_val_array_equal(_M0L3keyS2157, _M0L3keyS601);
        _if__result_4585 = _M0L6_2atmpS4234;
      } else {
        _if__result_4585 = 0;
      }
      if (_if__result_4585) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4233;
        int32_t _M0L6_2acntS4444;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2160;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2159;
        moonbit_incref(_M0L8_2aentryS607);
        moonbit_decref(_M0L4selfS605);
        moonbit_decref(_M0L3keyS601);
        _M0L8_2afieldS4233 = _M0L8_2aentryS607->$5;
        _M0L6_2acntS4444 = Moonbit_object_header(_M0L8_2aentryS607)->rc;
        if (_M0L6_2acntS4444 > 1) {
          int32_t _M0L11_2anew__cntS4447 = _M0L6_2acntS4444 - 1;
          Moonbit_object_header(_M0L8_2aentryS607)->rc
          = _M0L11_2anew__cntS4447;
          moonbit_incref(_M0L8_2afieldS4233);
        } else if (_M0L6_2acntS4444 == 1) {
          moonbit_string_t _M0L8_2afieldS4446 = _M0L8_2aentryS607->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4445;
          moonbit_decref(_M0L8_2afieldS4446);
          _M0L8_2afieldS4445 = _M0L8_2aentryS607->$1;
          if (_M0L8_2afieldS4445) {
            moonbit_decref(_M0L8_2afieldS4445);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS607);
        }
        _M0L5valueS2160 = _M0L8_2afieldS4233;
        _M0L6_2atmpS2159 = _M0L5valueS2160;
        return _M0L6_2atmpS2159;
      } else {
        moonbit_incref(_M0L8_2aentryS607);
      }
      _M0L8_2afieldS4232 = _M0L8_2aentryS607->$2;
      moonbit_decref(_M0L8_2aentryS607);
      _M0L3pslS2161 = _M0L8_2afieldS4232;
      if (_M0L1iS602 > _M0L3pslS2161) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2162;
        moonbit_decref(_M0L4selfS605);
        moonbit_decref(_M0L3keyS601);
        _M0L6_2atmpS2162 = 0;
        return _M0L6_2atmpS2162;
      }
      _M0L6_2atmpS2163 = _M0L1iS602 + 1;
      _M0L6_2atmpS2165 = _M0L3idxS603 + 1;
      _M0L14capacity__maskS2166 = _M0L4selfS605->$3;
      _M0L6_2atmpS2164 = _M0L6_2atmpS2165 & _M0L14capacity__maskS2166;
      _M0L1iS602 = _M0L6_2atmpS2163;
      _M0L3idxS603 = _M0L6_2atmpS2164;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS576
) {
  int32_t _M0L6lengthS575;
  int32_t _M0Lm8capacityS577;
  int32_t _M0L6_2atmpS2119;
  int32_t _M0L6_2atmpS2118;
  int32_t _M0L6_2atmpS2129;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS578;
  int32_t _M0L3endS2127;
  int32_t _M0L5startS2128;
  int32_t _M0L7_2abindS579;
  int32_t _M0L2__S580;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS576.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS575
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS576);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS577 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS575);
  _M0L6_2atmpS2119 = _M0Lm8capacityS577;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2118 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2119);
  if (_M0L6lengthS575 > _M0L6_2atmpS2118) {
    int32_t _M0L6_2atmpS2120 = _M0Lm8capacityS577;
    _M0Lm8capacityS577 = _M0L6_2atmpS2120 * 2;
  }
  _M0L6_2atmpS2129 = _M0Lm8capacityS577;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS578
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2129);
  _M0L3endS2127 = _M0L3arrS576.$2;
  _M0L5startS2128 = _M0L3arrS576.$1;
  _M0L7_2abindS579 = _M0L3endS2127 - _M0L5startS2128;
  _M0L2__S580 = 0;
  while (1) {
    if (_M0L2__S580 < _M0L7_2abindS579) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4241 =
        _M0L3arrS576.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2124 =
        _M0L8_2afieldS4241;
      int32_t _M0L5startS2126 = _M0L3arrS576.$1;
      int32_t _M0L6_2atmpS2125 = _M0L5startS2126 + _M0L2__S580;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4240 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2124[
          _M0L6_2atmpS2125
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS581 =
        _M0L6_2atmpS4240;
      moonbit_string_t _M0L8_2afieldS4239 = _M0L1eS581->$0;
      moonbit_string_t _M0L6_2atmpS2121 = _M0L8_2afieldS4239;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4238 =
        _M0L1eS581->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2122 =
        _M0L8_2afieldS4238;
      int32_t _M0L6_2atmpS2123;
      moonbit_incref(_M0L6_2atmpS2122);
      moonbit_incref(_M0L6_2atmpS2121);
      moonbit_incref(_M0L1mS578);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS578, _M0L6_2atmpS2121, _M0L6_2atmpS2122);
      _M0L6_2atmpS2123 = _M0L2__S580 + 1;
      _M0L2__S580 = _M0L6_2atmpS2123;
      continue;
    } else {
      moonbit_decref(_M0L3arrS576.$0);
    }
    break;
  }
  return _M0L1mS578;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS584
) {
  int32_t _M0L6lengthS583;
  int32_t _M0Lm8capacityS585;
  int32_t _M0L6_2atmpS2131;
  int32_t _M0L6_2atmpS2130;
  int32_t _M0L6_2atmpS2141;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS586;
  int32_t _M0L3endS2139;
  int32_t _M0L5startS2140;
  int32_t _M0L7_2abindS587;
  int32_t _M0L2__S588;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS584.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS583
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS584);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS585 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS583);
  _M0L6_2atmpS2131 = _M0Lm8capacityS585;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2130 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2131);
  if (_M0L6lengthS583 > _M0L6_2atmpS2130) {
    int32_t _M0L6_2atmpS2132 = _M0Lm8capacityS585;
    _M0Lm8capacityS585 = _M0L6_2atmpS2132 * 2;
  }
  _M0L6_2atmpS2141 = _M0Lm8capacityS585;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS586
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2141);
  _M0L3endS2139 = _M0L3arrS584.$2;
  _M0L5startS2140 = _M0L3arrS584.$1;
  _M0L7_2abindS587 = _M0L3endS2139 - _M0L5startS2140;
  _M0L2__S588 = 0;
  while (1) {
    if (_M0L2__S588 < _M0L7_2abindS587) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4244 =
        _M0L3arrS584.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2136 =
        _M0L8_2afieldS4244;
      int32_t _M0L5startS2138 = _M0L3arrS584.$1;
      int32_t _M0L6_2atmpS2137 = _M0L5startS2138 + _M0L2__S588;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4243 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2136[
          _M0L6_2atmpS2137
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS589 = _M0L6_2atmpS4243;
      int32_t _M0L6_2atmpS2133 = _M0L1eS589->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4242 =
        _M0L1eS589->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2134 =
        _M0L8_2afieldS4242;
      int32_t _M0L6_2atmpS2135;
      moonbit_incref(_M0L6_2atmpS2134);
      moonbit_incref(_M0L1mS586);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS586, _M0L6_2atmpS2133, _M0L6_2atmpS2134);
      _M0L6_2atmpS2135 = _M0L2__S588 + 1;
      _M0L2__S588 = _M0L6_2atmpS2135;
      continue;
    } else {
      moonbit_decref(_M0L3arrS584.$0);
    }
    break;
  }
  return _M0L1mS586;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS569,
  moonbit_string_t _M0L3keyS570,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS571
) {
  int32_t _M0L6_2atmpS2116;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS570);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2116 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS570);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS569, _M0L3keyS570, _M0L5valueS571, _M0L6_2atmpS2116);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS572,
  int32_t _M0L3keyS573,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS574
) {
  int32_t _M0L6_2atmpS2117;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2117 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS573);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS572, _M0L3keyS573, _M0L5valueS574, _M0L6_2atmpS2117);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS548
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4251;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS547;
  int32_t _M0L8capacityS2108;
  int32_t _M0L13new__capacityS549;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2103;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2102;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS4250;
  int32_t _M0L6_2atmpS2104;
  int32_t _M0L8capacityS2106;
  int32_t _M0L6_2atmpS2105;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2107;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4249;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS550;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4251 = _M0L4selfS548->$5;
  _M0L9old__headS547 = _M0L8_2afieldS4251;
  _M0L8capacityS2108 = _M0L4selfS548->$2;
  _M0L13new__capacityS549 = _M0L8capacityS2108 << 1;
  _M0L6_2atmpS2103 = 0;
  _M0L6_2atmpS2102
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS549, _M0L6_2atmpS2103);
  _M0L6_2aoldS4250 = _M0L4selfS548->$0;
  if (_M0L9old__headS547) {
    moonbit_incref(_M0L9old__headS547);
  }
  moonbit_decref(_M0L6_2aoldS4250);
  _M0L4selfS548->$0 = _M0L6_2atmpS2102;
  _M0L4selfS548->$2 = _M0L13new__capacityS549;
  _M0L6_2atmpS2104 = _M0L13new__capacityS549 - 1;
  _M0L4selfS548->$3 = _M0L6_2atmpS2104;
  _M0L8capacityS2106 = _M0L4selfS548->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2105 = _M0FPB21calc__grow__threshold(_M0L8capacityS2106);
  _M0L4selfS548->$4 = _M0L6_2atmpS2105;
  _M0L4selfS548->$1 = 0;
  _M0L6_2atmpS2107 = 0;
  _M0L6_2aoldS4249 = _M0L4selfS548->$5;
  if (_M0L6_2aoldS4249) {
    moonbit_decref(_M0L6_2aoldS4249);
  }
  _M0L4selfS548->$5 = _M0L6_2atmpS2107;
  _M0L4selfS548->$6 = -1;
  _M0L8_2aparamS550 = _M0L9old__headS547;
  while (1) {
    if (_M0L8_2aparamS550 == 0) {
      if (_M0L8_2aparamS550) {
        moonbit_decref(_M0L8_2aparamS550);
      }
      moonbit_decref(_M0L4selfS548);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS551 =
        _M0L8_2aparamS550;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS552 =
        _M0L7_2aSomeS551;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4248 =
        _M0L4_2axS552->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS553 =
        _M0L8_2afieldS4248;
      moonbit_string_t _M0L8_2afieldS4247 = _M0L4_2axS552->$4;
      moonbit_string_t _M0L6_2akeyS554 = _M0L8_2afieldS4247;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4246 =
        _M0L4_2axS552->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS555 =
        _M0L8_2afieldS4246;
      int32_t _M0L8_2afieldS4245 = _M0L4_2axS552->$3;
      int32_t _M0L6_2acntS4448 = Moonbit_object_header(_M0L4_2axS552)->rc;
      int32_t _M0L7_2ahashS556;
      if (_M0L6_2acntS4448 > 1) {
        int32_t _M0L11_2anew__cntS4449 = _M0L6_2acntS4448 - 1;
        Moonbit_object_header(_M0L4_2axS552)->rc = _M0L11_2anew__cntS4449;
        moonbit_incref(_M0L8_2avalueS555);
        moonbit_incref(_M0L6_2akeyS554);
        if (_M0L7_2anextS553) {
          moonbit_incref(_M0L7_2anextS553);
        }
      } else if (_M0L6_2acntS4448 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS552);
      }
      _M0L7_2ahashS556 = _M0L8_2afieldS4245;
      moonbit_incref(_M0L4selfS548);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS548, _M0L6_2akeyS554, _M0L8_2avalueS555, _M0L7_2ahashS556);
      _M0L8_2aparamS550 = _M0L7_2anextS553;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS559
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4257;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS558;
  int32_t _M0L8capacityS2115;
  int32_t _M0L13new__capacityS560;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2110;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2109;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS4256;
  int32_t _M0L6_2atmpS2111;
  int32_t _M0L8capacityS2113;
  int32_t _M0L6_2atmpS2112;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2114;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4255;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS561;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4257 = _M0L4selfS559->$5;
  _M0L9old__headS558 = _M0L8_2afieldS4257;
  _M0L8capacityS2115 = _M0L4selfS559->$2;
  _M0L13new__capacityS560 = _M0L8capacityS2115 << 1;
  _M0L6_2atmpS2110 = 0;
  _M0L6_2atmpS2109
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS560, _M0L6_2atmpS2110);
  _M0L6_2aoldS4256 = _M0L4selfS559->$0;
  if (_M0L9old__headS558) {
    moonbit_incref(_M0L9old__headS558);
  }
  moonbit_decref(_M0L6_2aoldS4256);
  _M0L4selfS559->$0 = _M0L6_2atmpS2109;
  _M0L4selfS559->$2 = _M0L13new__capacityS560;
  _M0L6_2atmpS2111 = _M0L13new__capacityS560 - 1;
  _M0L4selfS559->$3 = _M0L6_2atmpS2111;
  _M0L8capacityS2113 = _M0L4selfS559->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2112 = _M0FPB21calc__grow__threshold(_M0L8capacityS2113);
  _M0L4selfS559->$4 = _M0L6_2atmpS2112;
  _M0L4selfS559->$1 = 0;
  _M0L6_2atmpS2114 = 0;
  _M0L6_2aoldS4255 = _M0L4selfS559->$5;
  if (_M0L6_2aoldS4255) {
    moonbit_decref(_M0L6_2aoldS4255);
  }
  _M0L4selfS559->$5 = _M0L6_2atmpS2114;
  _M0L4selfS559->$6 = -1;
  _M0L8_2aparamS561 = _M0L9old__headS558;
  while (1) {
    if (_M0L8_2aparamS561 == 0) {
      if (_M0L8_2aparamS561) {
        moonbit_decref(_M0L8_2aparamS561);
      }
      moonbit_decref(_M0L4selfS559);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS562 =
        _M0L8_2aparamS561;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS563 =
        _M0L7_2aSomeS562;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4254 =
        _M0L4_2axS563->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS564 =
        _M0L8_2afieldS4254;
      int32_t _M0L6_2akeyS565 = _M0L4_2axS563->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4253 =
        _M0L4_2axS563->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS566 =
        _M0L8_2afieldS4253;
      int32_t _M0L8_2afieldS4252 = _M0L4_2axS563->$3;
      int32_t _M0L6_2acntS4450 = Moonbit_object_header(_M0L4_2axS563)->rc;
      int32_t _M0L7_2ahashS567;
      if (_M0L6_2acntS4450 > 1) {
        int32_t _M0L11_2anew__cntS4451 = _M0L6_2acntS4450 - 1;
        Moonbit_object_header(_M0L4_2axS563)->rc = _M0L11_2anew__cntS4451;
        moonbit_incref(_M0L8_2avalueS566);
        if (_M0L7_2anextS564) {
          moonbit_incref(_M0L7_2anextS564);
        }
      } else if (_M0L6_2acntS4450 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS563);
      }
      _M0L7_2ahashS567 = _M0L8_2afieldS4252;
      moonbit_incref(_M0L4selfS559);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS559, _M0L6_2akeyS565, _M0L8_2avalueS566, _M0L7_2ahashS567);
      _M0L8_2aparamS561 = _M0L7_2anextS564;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS518,
  moonbit_string_t _M0L3keyS524,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS525,
  int32_t _M0L4hashS520
) {
  int32_t _M0L14capacity__maskS2083;
  int32_t _M0L6_2atmpS2082;
  int32_t _M0L3pslS515;
  int32_t _M0L3idxS516;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2083 = _M0L4selfS518->$3;
  _M0L6_2atmpS2082 = _M0L4hashS520 & _M0L14capacity__maskS2083;
  _M0L3pslS515 = 0;
  _M0L3idxS516 = _M0L6_2atmpS2082;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4262 =
      _M0L4selfS518->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2081 =
      _M0L8_2afieldS4262;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4261;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS517;
    if (
      _M0L3idxS516 < 0
      || _M0L3idxS516 >= Moonbit_array_length(_M0L7entriesS2081)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4261
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2081[
        _M0L3idxS516
      ];
    _M0L7_2abindS517 = _M0L6_2atmpS4261;
    if (_M0L7_2abindS517 == 0) {
      int32_t _M0L4sizeS2066 = _M0L4selfS518->$1;
      int32_t _M0L8grow__atS2067 = _M0L4selfS518->$4;
      int32_t _M0L7_2abindS521;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS522;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS523;
      if (_M0L4sizeS2066 >= _M0L8grow__atS2067) {
        int32_t _M0L14capacity__maskS2069;
        int32_t _M0L6_2atmpS2068;
        moonbit_incref(_M0L4selfS518);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS518);
        _M0L14capacity__maskS2069 = _M0L4selfS518->$3;
        _M0L6_2atmpS2068 = _M0L4hashS520 & _M0L14capacity__maskS2069;
        _M0L3pslS515 = 0;
        _M0L3idxS516 = _M0L6_2atmpS2068;
        continue;
      }
      _M0L7_2abindS521 = _M0L4selfS518->$6;
      _M0L7_2abindS522 = 0;
      _M0L5entryS523
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS523)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS523->$0 = _M0L7_2abindS521;
      _M0L5entryS523->$1 = _M0L7_2abindS522;
      _M0L5entryS523->$2 = _M0L3pslS515;
      _M0L5entryS523->$3 = _M0L4hashS520;
      _M0L5entryS523->$4 = _M0L3keyS524;
      _M0L5entryS523->$5 = _M0L5valueS525;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS518, _M0L3idxS516, _M0L5entryS523);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS526 =
        _M0L7_2abindS517;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS527 =
        _M0L7_2aSomeS526;
      int32_t _M0L4hashS2071 = _M0L14_2acurr__entryS527->$3;
      int32_t _if__result_4591;
      int32_t _M0L3pslS2072;
      int32_t _M0L6_2atmpS2077;
      int32_t _M0L6_2atmpS2079;
      int32_t _M0L14capacity__maskS2080;
      int32_t _M0L6_2atmpS2078;
      if (_M0L4hashS2071 == _M0L4hashS520) {
        moonbit_string_t _M0L8_2afieldS4260 = _M0L14_2acurr__entryS527->$4;
        moonbit_string_t _M0L3keyS2070 = _M0L8_2afieldS4260;
        int32_t _M0L6_2atmpS4259;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS4259
        = moonbit_val_array_equal(_M0L3keyS2070, _M0L3keyS524);
        _if__result_4591 = _M0L6_2atmpS4259;
      } else {
        _if__result_4591 = 0;
      }
      if (_if__result_4591) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4258;
        moonbit_incref(_M0L14_2acurr__entryS527);
        moonbit_decref(_M0L3keyS524);
        moonbit_decref(_M0L4selfS518);
        _M0L6_2aoldS4258 = _M0L14_2acurr__entryS527->$5;
        moonbit_decref(_M0L6_2aoldS4258);
        _M0L14_2acurr__entryS527->$5 = _M0L5valueS525;
        moonbit_decref(_M0L14_2acurr__entryS527);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS527);
      }
      _M0L3pslS2072 = _M0L14_2acurr__entryS527->$2;
      if (_M0L3pslS515 > _M0L3pslS2072) {
        int32_t _M0L4sizeS2073 = _M0L4selfS518->$1;
        int32_t _M0L8grow__atS2074 = _M0L4selfS518->$4;
        int32_t _M0L7_2abindS528;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS529;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS530;
        if (_M0L4sizeS2073 >= _M0L8grow__atS2074) {
          int32_t _M0L14capacity__maskS2076;
          int32_t _M0L6_2atmpS2075;
          moonbit_decref(_M0L14_2acurr__entryS527);
          moonbit_incref(_M0L4selfS518);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS518);
          _M0L14capacity__maskS2076 = _M0L4selfS518->$3;
          _M0L6_2atmpS2075 = _M0L4hashS520 & _M0L14capacity__maskS2076;
          _M0L3pslS515 = 0;
          _M0L3idxS516 = _M0L6_2atmpS2075;
          continue;
        }
        moonbit_incref(_M0L4selfS518);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS518, _M0L3idxS516, _M0L14_2acurr__entryS527);
        _M0L7_2abindS528 = _M0L4selfS518->$6;
        _M0L7_2abindS529 = 0;
        _M0L5entryS530
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS530)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS530->$0 = _M0L7_2abindS528;
        _M0L5entryS530->$1 = _M0L7_2abindS529;
        _M0L5entryS530->$2 = _M0L3pslS515;
        _M0L5entryS530->$3 = _M0L4hashS520;
        _M0L5entryS530->$4 = _M0L3keyS524;
        _M0L5entryS530->$5 = _M0L5valueS525;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS518, _M0L3idxS516, _M0L5entryS530);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS527);
      }
      _M0L6_2atmpS2077 = _M0L3pslS515 + 1;
      _M0L6_2atmpS2079 = _M0L3idxS516 + 1;
      _M0L14capacity__maskS2080 = _M0L4selfS518->$3;
      _M0L6_2atmpS2078 = _M0L6_2atmpS2079 & _M0L14capacity__maskS2080;
      _M0L3pslS515 = _M0L6_2atmpS2077;
      _M0L3idxS516 = _M0L6_2atmpS2078;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS534,
  int32_t _M0L3keyS540,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS541,
  int32_t _M0L4hashS536
) {
  int32_t _M0L14capacity__maskS2101;
  int32_t _M0L6_2atmpS2100;
  int32_t _M0L3pslS531;
  int32_t _M0L3idxS532;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2101 = _M0L4selfS534->$3;
  _M0L6_2atmpS2100 = _M0L4hashS536 & _M0L14capacity__maskS2101;
  _M0L3pslS531 = 0;
  _M0L3idxS532 = _M0L6_2atmpS2100;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4265 =
      _M0L4selfS534->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2099 =
      _M0L8_2afieldS4265;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4264;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS533;
    if (
      _M0L3idxS532 < 0
      || _M0L3idxS532 >= Moonbit_array_length(_M0L7entriesS2099)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4264
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2099[
        _M0L3idxS532
      ];
    _M0L7_2abindS533 = _M0L6_2atmpS4264;
    if (_M0L7_2abindS533 == 0) {
      int32_t _M0L4sizeS2084 = _M0L4selfS534->$1;
      int32_t _M0L8grow__atS2085 = _M0L4selfS534->$4;
      int32_t _M0L7_2abindS537;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS538;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS539;
      if (_M0L4sizeS2084 >= _M0L8grow__atS2085) {
        int32_t _M0L14capacity__maskS2087;
        int32_t _M0L6_2atmpS2086;
        moonbit_incref(_M0L4selfS534);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS534);
        _M0L14capacity__maskS2087 = _M0L4selfS534->$3;
        _M0L6_2atmpS2086 = _M0L4hashS536 & _M0L14capacity__maskS2087;
        _M0L3pslS531 = 0;
        _M0L3idxS532 = _M0L6_2atmpS2086;
        continue;
      }
      _M0L7_2abindS537 = _M0L4selfS534->$6;
      _M0L7_2abindS538 = 0;
      _M0L5entryS539
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS539)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS539->$0 = _M0L7_2abindS537;
      _M0L5entryS539->$1 = _M0L7_2abindS538;
      _M0L5entryS539->$2 = _M0L3pslS531;
      _M0L5entryS539->$3 = _M0L4hashS536;
      _M0L5entryS539->$4 = _M0L3keyS540;
      _M0L5entryS539->$5 = _M0L5valueS541;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS534, _M0L3idxS532, _M0L5entryS539);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS542 =
        _M0L7_2abindS533;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS543 =
        _M0L7_2aSomeS542;
      int32_t _M0L4hashS2089 = _M0L14_2acurr__entryS543->$3;
      int32_t _if__result_4593;
      int32_t _M0L3pslS2090;
      int32_t _M0L6_2atmpS2095;
      int32_t _M0L6_2atmpS2097;
      int32_t _M0L14capacity__maskS2098;
      int32_t _M0L6_2atmpS2096;
      if (_M0L4hashS2089 == _M0L4hashS536) {
        int32_t _M0L3keyS2088 = _M0L14_2acurr__entryS543->$4;
        _if__result_4593 = _M0L3keyS2088 == _M0L3keyS540;
      } else {
        _if__result_4593 = 0;
      }
      if (_if__result_4593) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS4263;
        moonbit_incref(_M0L14_2acurr__entryS543);
        moonbit_decref(_M0L4selfS534);
        _M0L6_2aoldS4263 = _M0L14_2acurr__entryS543->$5;
        moonbit_decref(_M0L6_2aoldS4263);
        _M0L14_2acurr__entryS543->$5 = _M0L5valueS541;
        moonbit_decref(_M0L14_2acurr__entryS543);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS543);
      }
      _M0L3pslS2090 = _M0L14_2acurr__entryS543->$2;
      if (_M0L3pslS531 > _M0L3pslS2090) {
        int32_t _M0L4sizeS2091 = _M0L4selfS534->$1;
        int32_t _M0L8grow__atS2092 = _M0L4selfS534->$4;
        int32_t _M0L7_2abindS544;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS545;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS546;
        if (_M0L4sizeS2091 >= _M0L8grow__atS2092) {
          int32_t _M0L14capacity__maskS2094;
          int32_t _M0L6_2atmpS2093;
          moonbit_decref(_M0L14_2acurr__entryS543);
          moonbit_incref(_M0L4selfS534);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS534);
          _M0L14capacity__maskS2094 = _M0L4selfS534->$3;
          _M0L6_2atmpS2093 = _M0L4hashS536 & _M0L14capacity__maskS2094;
          _M0L3pslS531 = 0;
          _M0L3idxS532 = _M0L6_2atmpS2093;
          continue;
        }
        moonbit_incref(_M0L4selfS534);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS534, _M0L3idxS532, _M0L14_2acurr__entryS543);
        _M0L7_2abindS544 = _M0L4selfS534->$6;
        _M0L7_2abindS545 = 0;
        _M0L5entryS546
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS546)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS546->$0 = _M0L7_2abindS544;
        _M0L5entryS546->$1 = _M0L7_2abindS545;
        _M0L5entryS546->$2 = _M0L3pslS531;
        _M0L5entryS546->$3 = _M0L4hashS536;
        _M0L5entryS546->$4 = _M0L3keyS540;
        _M0L5entryS546->$5 = _M0L5valueS541;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS534, _M0L3idxS532, _M0L5entryS546);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS543);
      }
      _M0L6_2atmpS2095 = _M0L3pslS531 + 1;
      _M0L6_2atmpS2097 = _M0L3idxS532 + 1;
      _M0L14capacity__maskS2098 = _M0L4selfS534->$3;
      _M0L6_2atmpS2096 = _M0L6_2atmpS2097 & _M0L14capacity__maskS2098;
      _M0L3pslS531 = _M0L6_2atmpS2095;
      _M0L3idxS532 = _M0L6_2atmpS2096;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS499,
  int32_t _M0L3idxS504,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS503
) {
  int32_t _M0L3pslS2049;
  int32_t _M0L6_2atmpS2045;
  int32_t _M0L6_2atmpS2047;
  int32_t _M0L14capacity__maskS2048;
  int32_t _M0L6_2atmpS2046;
  int32_t _M0L3pslS495;
  int32_t _M0L3idxS496;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS497;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2049 = _M0L5entryS503->$2;
  _M0L6_2atmpS2045 = _M0L3pslS2049 + 1;
  _M0L6_2atmpS2047 = _M0L3idxS504 + 1;
  _M0L14capacity__maskS2048 = _M0L4selfS499->$3;
  _M0L6_2atmpS2046 = _M0L6_2atmpS2047 & _M0L14capacity__maskS2048;
  _M0L3pslS495 = _M0L6_2atmpS2045;
  _M0L3idxS496 = _M0L6_2atmpS2046;
  _M0L5entryS497 = _M0L5entryS503;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4267 =
      _M0L4selfS499->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2044 =
      _M0L8_2afieldS4267;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4266;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS498;
    if (
      _M0L3idxS496 < 0
      || _M0L3idxS496 >= Moonbit_array_length(_M0L7entriesS2044)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4266
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2044[
        _M0L3idxS496
      ];
    _M0L7_2abindS498 = _M0L6_2atmpS4266;
    if (_M0L7_2abindS498 == 0) {
      _M0L5entryS497->$2 = _M0L3pslS495;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS499, _M0L5entryS497, _M0L3idxS496);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS501 =
        _M0L7_2abindS498;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS502 =
        _M0L7_2aSomeS501;
      int32_t _M0L3pslS2034 = _M0L14_2acurr__entryS502->$2;
      if (_M0L3pslS495 > _M0L3pslS2034) {
        int32_t _M0L3pslS2039;
        int32_t _M0L6_2atmpS2035;
        int32_t _M0L6_2atmpS2037;
        int32_t _M0L14capacity__maskS2038;
        int32_t _M0L6_2atmpS2036;
        _M0L5entryS497->$2 = _M0L3pslS495;
        moonbit_incref(_M0L14_2acurr__entryS502);
        moonbit_incref(_M0L4selfS499);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS499, _M0L5entryS497, _M0L3idxS496);
        _M0L3pslS2039 = _M0L14_2acurr__entryS502->$2;
        _M0L6_2atmpS2035 = _M0L3pslS2039 + 1;
        _M0L6_2atmpS2037 = _M0L3idxS496 + 1;
        _M0L14capacity__maskS2038 = _M0L4selfS499->$3;
        _M0L6_2atmpS2036 = _M0L6_2atmpS2037 & _M0L14capacity__maskS2038;
        _M0L3pslS495 = _M0L6_2atmpS2035;
        _M0L3idxS496 = _M0L6_2atmpS2036;
        _M0L5entryS497 = _M0L14_2acurr__entryS502;
        continue;
      } else {
        int32_t _M0L6_2atmpS2040 = _M0L3pslS495 + 1;
        int32_t _M0L6_2atmpS2042 = _M0L3idxS496 + 1;
        int32_t _M0L14capacity__maskS2043 = _M0L4selfS499->$3;
        int32_t _M0L6_2atmpS2041 =
          _M0L6_2atmpS2042 & _M0L14capacity__maskS2043;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_4595 =
          _M0L5entryS497;
        _M0L3pslS495 = _M0L6_2atmpS2040;
        _M0L3idxS496 = _M0L6_2atmpS2041;
        _M0L5entryS497 = _tmp_4595;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS509,
  int32_t _M0L3idxS514,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS513
) {
  int32_t _M0L3pslS2065;
  int32_t _M0L6_2atmpS2061;
  int32_t _M0L6_2atmpS2063;
  int32_t _M0L14capacity__maskS2064;
  int32_t _M0L6_2atmpS2062;
  int32_t _M0L3pslS505;
  int32_t _M0L3idxS506;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS507;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2065 = _M0L5entryS513->$2;
  _M0L6_2atmpS2061 = _M0L3pslS2065 + 1;
  _M0L6_2atmpS2063 = _M0L3idxS514 + 1;
  _M0L14capacity__maskS2064 = _M0L4selfS509->$3;
  _M0L6_2atmpS2062 = _M0L6_2atmpS2063 & _M0L14capacity__maskS2064;
  _M0L3pslS505 = _M0L6_2atmpS2061;
  _M0L3idxS506 = _M0L6_2atmpS2062;
  _M0L5entryS507 = _M0L5entryS513;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4269 =
      _M0L4selfS509->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2060 =
      _M0L8_2afieldS4269;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4268;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS508;
    if (
      _M0L3idxS506 < 0
      || _M0L3idxS506 >= Moonbit_array_length(_M0L7entriesS2060)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4268
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2060[
        _M0L3idxS506
      ];
    _M0L7_2abindS508 = _M0L6_2atmpS4268;
    if (_M0L7_2abindS508 == 0) {
      _M0L5entryS507->$2 = _M0L3pslS505;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS509, _M0L5entryS507, _M0L3idxS506);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS511 =
        _M0L7_2abindS508;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS512 =
        _M0L7_2aSomeS511;
      int32_t _M0L3pslS2050 = _M0L14_2acurr__entryS512->$2;
      if (_M0L3pslS505 > _M0L3pslS2050) {
        int32_t _M0L3pslS2055;
        int32_t _M0L6_2atmpS2051;
        int32_t _M0L6_2atmpS2053;
        int32_t _M0L14capacity__maskS2054;
        int32_t _M0L6_2atmpS2052;
        _M0L5entryS507->$2 = _M0L3pslS505;
        moonbit_incref(_M0L14_2acurr__entryS512);
        moonbit_incref(_M0L4selfS509);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS509, _M0L5entryS507, _M0L3idxS506);
        _M0L3pslS2055 = _M0L14_2acurr__entryS512->$2;
        _M0L6_2atmpS2051 = _M0L3pslS2055 + 1;
        _M0L6_2atmpS2053 = _M0L3idxS506 + 1;
        _M0L14capacity__maskS2054 = _M0L4selfS509->$3;
        _M0L6_2atmpS2052 = _M0L6_2atmpS2053 & _M0L14capacity__maskS2054;
        _M0L3pslS505 = _M0L6_2atmpS2051;
        _M0L3idxS506 = _M0L6_2atmpS2052;
        _M0L5entryS507 = _M0L14_2acurr__entryS512;
        continue;
      } else {
        int32_t _M0L6_2atmpS2056 = _M0L3pslS505 + 1;
        int32_t _M0L6_2atmpS2058 = _M0L3idxS506 + 1;
        int32_t _M0L14capacity__maskS2059 = _M0L4selfS509->$3;
        int32_t _M0L6_2atmpS2057 =
          _M0L6_2atmpS2058 & _M0L14capacity__maskS2059;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_4597 =
          _M0L5entryS507;
        _M0L3pslS505 = _M0L6_2atmpS2056;
        _M0L3idxS506 = _M0L6_2atmpS2057;
        _M0L5entryS507 = _tmp_4597;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS483,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS485,
  int32_t _M0L8new__idxS484
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4272;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2030;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2031;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4271;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS4270;
  int32_t _M0L6_2acntS4452;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS486;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4272 = _M0L4selfS483->$0;
  _M0L7entriesS2030 = _M0L8_2afieldS4272;
  moonbit_incref(_M0L5entryS485);
  _M0L6_2atmpS2031 = _M0L5entryS485;
  if (
    _M0L8new__idxS484 < 0
    || _M0L8new__idxS484 >= Moonbit_array_length(_M0L7entriesS2030)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4271
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2030[
      _M0L8new__idxS484
    ];
  if (_M0L6_2aoldS4271) {
    moonbit_decref(_M0L6_2aoldS4271);
  }
  _M0L7entriesS2030[_M0L8new__idxS484] = _M0L6_2atmpS2031;
  _M0L8_2afieldS4270 = _M0L5entryS485->$1;
  _M0L6_2acntS4452 = Moonbit_object_header(_M0L5entryS485)->rc;
  if (_M0L6_2acntS4452 > 1) {
    int32_t _M0L11_2anew__cntS4455 = _M0L6_2acntS4452 - 1;
    Moonbit_object_header(_M0L5entryS485)->rc = _M0L11_2anew__cntS4455;
    if (_M0L8_2afieldS4270) {
      moonbit_incref(_M0L8_2afieldS4270);
    }
  } else if (_M0L6_2acntS4452 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4454 =
      _M0L5entryS485->$5;
    moonbit_string_t _M0L8_2afieldS4453;
    moonbit_decref(_M0L8_2afieldS4454);
    _M0L8_2afieldS4453 = _M0L5entryS485->$4;
    moonbit_decref(_M0L8_2afieldS4453);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS485);
  }
  _M0L7_2abindS486 = _M0L8_2afieldS4270;
  if (_M0L7_2abindS486 == 0) {
    if (_M0L7_2abindS486) {
      moonbit_decref(_M0L7_2abindS486);
    }
    _M0L4selfS483->$6 = _M0L8new__idxS484;
    moonbit_decref(_M0L4selfS483);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS487;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS488;
    moonbit_decref(_M0L4selfS483);
    _M0L7_2aSomeS487 = _M0L7_2abindS486;
    _M0L7_2anextS488 = _M0L7_2aSomeS487;
    _M0L7_2anextS488->$0 = _M0L8new__idxS484;
    moonbit_decref(_M0L7_2anextS488);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS489,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS491,
  int32_t _M0L8new__idxS490
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4275;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2032;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2033;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4274;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS4273;
  int32_t _M0L6_2acntS4456;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS492;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS4275 = _M0L4selfS489->$0;
  _M0L7entriesS2032 = _M0L8_2afieldS4275;
  moonbit_incref(_M0L5entryS491);
  _M0L6_2atmpS2033 = _M0L5entryS491;
  if (
    _M0L8new__idxS490 < 0
    || _M0L8new__idxS490 >= Moonbit_array_length(_M0L7entriesS2032)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4274
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2032[
      _M0L8new__idxS490
    ];
  if (_M0L6_2aoldS4274) {
    moonbit_decref(_M0L6_2aoldS4274);
  }
  _M0L7entriesS2032[_M0L8new__idxS490] = _M0L6_2atmpS2033;
  _M0L8_2afieldS4273 = _M0L5entryS491->$1;
  _M0L6_2acntS4456 = Moonbit_object_header(_M0L5entryS491)->rc;
  if (_M0L6_2acntS4456 > 1) {
    int32_t _M0L11_2anew__cntS4458 = _M0L6_2acntS4456 - 1;
    Moonbit_object_header(_M0L5entryS491)->rc = _M0L11_2anew__cntS4458;
    if (_M0L8_2afieldS4273) {
      moonbit_incref(_M0L8_2afieldS4273);
    }
  } else if (_M0L6_2acntS4456 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS4457 =
      _M0L5entryS491->$5;
    moonbit_decref(_M0L8_2afieldS4457);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS491);
  }
  _M0L7_2abindS492 = _M0L8_2afieldS4273;
  if (_M0L7_2abindS492 == 0) {
    if (_M0L7_2abindS492) {
      moonbit_decref(_M0L7_2abindS492);
    }
    _M0L4selfS489->$6 = _M0L8new__idxS490;
    moonbit_decref(_M0L4selfS489);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS493;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS494;
    moonbit_decref(_M0L4selfS489);
    _M0L7_2aSomeS493 = _M0L7_2abindS492;
    _M0L7_2anextS494 = _M0L7_2aSomeS493;
    _M0L7_2anextS494->$0 = _M0L8new__idxS490;
    moonbit_decref(_M0L7_2anextS494);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS476,
  int32_t _M0L3idxS478,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS477
) {
  int32_t _M0L7_2abindS475;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4277;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2017;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2018;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4276;
  int32_t _M0L4sizeS2020;
  int32_t _M0L6_2atmpS2019;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS475 = _M0L4selfS476->$6;
  switch (_M0L7_2abindS475) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2012;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4278;
      moonbit_incref(_M0L5entryS477);
      _M0L6_2atmpS2012 = _M0L5entryS477;
      _M0L6_2aoldS4278 = _M0L4selfS476->$5;
      if (_M0L6_2aoldS4278) {
        moonbit_decref(_M0L6_2aoldS4278);
      }
      _M0L4selfS476->$5 = _M0L6_2atmpS2012;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS4281 =
        _M0L4selfS476->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2016 =
        _M0L8_2afieldS4281;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS4280;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2015;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2013;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2014;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS4279;
      if (
        _M0L7_2abindS475 < 0
        || _M0L7_2abindS475 >= Moonbit_array_length(_M0L7entriesS2016)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4280
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2016[
          _M0L7_2abindS475
        ];
      _M0L6_2atmpS2015 = _M0L6_2atmpS4280;
      if (_M0L6_2atmpS2015) {
        moonbit_incref(_M0L6_2atmpS2015);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2013
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS2015);
      moonbit_incref(_M0L5entryS477);
      _M0L6_2atmpS2014 = _M0L5entryS477;
      _M0L6_2aoldS4279 = _M0L6_2atmpS2013->$1;
      if (_M0L6_2aoldS4279) {
        moonbit_decref(_M0L6_2aoldS4279);
      }
      _M0L6_2atmpS2013->$1 = _M0L6_2atmpS2014;
      moonbit_decref(_M0L6_2atmpS2013);
      break;
    }
  }
  _M0L4selfS476->$6 = _M0L3idxS478;
  _M0L8_2afieldS4277 = _M0L4selfS476->$0;
  _M0L7entriesS2017 = _M0L8_2afieldS4277;
  _M0L6_2atmpS2018 = _M0L5entryS477;
  if (
    _M0L3idxS478 < 0
    || _M0L3idxS478 >= Moonbit_array_length(_M0L7entriesS2017)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4276
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2017[
      _M0L3idxS478
    ];
  if (_M0L6_2aoldS4276) {
    moonbit_decref(_M0L6_2aoldS4276);
  }
  _M0L7entriesS2017[_M0L3idxS478] = _M0L6_2atmpS2018;
  _M0L4sizeS2020 = _M0L4selfS476->$1;
  _M0L6_2atmpS2019 = _M0L4sizeS2020 + 1;
  _M0L4selfS476->$1 = _M0L6_2atmpS2019;
  moonbit_decref(_M0L4selfS476);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS480,
  int32_t _M0L3idxS482,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS481
) {
  int32_t _M0L7_2abindS479;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4283;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2026;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2027;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4282;
  int32_t _M0L4sizeS2029;
  int32_t _M0L6_2atmpS2028;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS479 = _M0L4selfS480->$6;
  switch (_M0L7_2abindS479) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2021;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4284;
      moonbit_incref(_M0L5entryS481);
      _M0L6_2atmpS2021 = _M0L5entryS481;
      _M0L6_2aoldS4284 = _M0L4selfS480->$5;
      if (_M0L6_2aoldS4284) {
        moonbit_decref(_M0L6_2aoldS4284);
      }
      _M0L4selfS480->$5 = _M0L6_2atmpS2021;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS4287 =
        _M0L4selfS480->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2025 =
        _M0L8_2afieldS4287;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS4286;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2024;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2022;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2023;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS4285;
      if (
        _M0L7_2abindS479 < 0
        || _M0L7_2abindS479 >= Moonbit_array_length(_M0L7entriesS2025)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS4286
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2025[
          _M0L7_2abindS479
        ];
      _M0L6_2atmpS2024 = _M0L6_2atmpS4286;
      if (_M0L6_2atmpS2024) {
        moonbit_incref(_M0L6_2atmpS2024);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS2022
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2024);
      moonbit_incref(_M0L5entryS481);
      _M0L6_2atmpS2023 = _M0L5entryS481;
      _M0L6_2aoldS4285 = _M0L6_2atmpS2022->$1;
      if (_M0L6_2aoldS4285) {
        moonbit_decref(_M0L6_2aoldS4285);
      }
      _M0L6_2atmpS2022->$1 = _M0L6_2atmpS2023;
      moonbit_decref(_M0L6_2atmpS2022);
      break;
    }
  }
  _M0L4selfS480->$6 = _M0L3idxS482;
  _M0L8_2afieldS4283 = _M0L4selfS480->$0;
  _M0L7entriesS2026 = _M0L8_2afieldS4283;
  _M0L6_2atmpS2027 = _M0L5entryS481;
  if (
    _M0L3idxS482 < 0
    || _M0L3idxS482 >= Moonbit_array_length(_M0L7entriesS2026)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS4282
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2026[
      _M0L3idxS482
    ];
  if (_M0L6_2aoldS4282) {
    moonbit_decref(_M0L6_2aoldS4282);
  }
  _M0L7entriesS2026[_M0L3idxS482] = _M0L6_2atmpS2027;
  _M0L4sizeS2029 = _M0L4selfS480->$1;
  _M0L6_2atmpS2028 = _M0L4sizeS2029 + 1;
  _M0L4selfS480->$1 = _M0L6_2atmpS2028;
  moonbit_decref(_M0L4selfS480);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS464
) {
  int32_t _M0L8capacityS463;
  int32_t _M0L7_2abindS465;
  int32_t _M0L7_2abindS466;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2010;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS467;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS468;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_4598;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS463
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS464);
  _M0L7_2abindS465 = _M0L8capacityS463 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS466 = _M0FPB21calc__grow__threshold(_M0L8capacityS463);
  _M0L6_2atmpS2010 = 0;
  _M0L7_2abindS467
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS463, _M0L6_2atmpS2010);
  _M0L7_2abindS468 = 0;
  _block_4598
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_4598)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_4598->$0 = _M0L7_2abindS467;
  _block_4598->$1 = 0;
  _block_4598->$2 = _M0L8capacityS463;
  _block_4598->$3 = _M0L7_2abindS465;
  _block_4598->$4 = _M0L7_2abindS466;
  _block_4598->$5 = _M0L7_2abindS468;
  _block_4598->$6 = -1;
  return _block_4598;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS470
) {
  int32_t _M0L8capacityS469;
  int32_t _M0L7_2abindS471;
  int32_t _M0L7_2abindS472;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2011;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS473;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS474;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_4599;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS469
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS470);
  _M0L7_2abindS471 = _M0L8capacityS469 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS472 = _M0FPB21calc__grow__threshold(_M0L8capacityS469);
  _M0L6_2atmpS2011 = 0;
  _M0L7_2abindS473
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS469, _M0L6_2atmpS2011);
  _M0L7_2abindS474 = 0;
  _block_4599
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_4599)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_4599->$0 = _M0L7_2abindS473;
  _block_4599->$1 = 0;
  _block_4599->$2 = _M0L8capacityS469;
  _block_4599->$3 = _M0L7_2abindS471;
  _block_4599->$4 = _M0L7_2abindS472;
  _block_4599->$5 = _M0L7_2abindS474;
  _block_4599->$6 = -1;
  return _block_4599;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS462) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS462 >= 0) {
    int32_t _M0L6_2atmpS2009;
    int32_t _M0L6_2atmpS2008;
    int32_t _M0L6_2atmpS2007;
    int32_t _M0L6_2atmpS2006;
    if (_M0L4selfS462 <= 1) {
      return 1;
    }
    if (_M0L4selfS462 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS2009 = _M0L4selfS462 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS2008 = moonbit_clz32(_M0L6_2atmpS2009);
    _M0L6_2atmpS2007 = _M0L6_2atmpS2008 - 1;
    _M0L6_2atmpS2006 = 2147483647 >> (_M0L6_2atmpS2007 & 31);
    return _M0L6_2atmpS2006 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS461) {
  int32_t _M0L6_2atmpS2005;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2005 = _M0L8capacityS461 * 13;
  return _M0L6_2atmpS2005 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS457
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS457 == 0) {
    if (_M0L4selfS457) {
      moonbit_decref(_M0L4selfS457);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS458 =
      _M0L4selfS457;
    return _M0L7_2aSomeS458;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS459
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS459 == 0) {
    if (_M0L4selfS459) {
      moonbit_decref(_M0L4selfS459);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS460 =
      _M0L4selfS459;
    return _M0L7_2aSomeS460;
  }
}

struct _M0TPB9ArrayViewGyE _M0MPC15array10FixedArray12view_2einnerGyE(
  moonbit_bytes_t _M0L4selfS448,
  int32_t _M0L5startS454,
  int64_t _M0L3endS450
) {
  int32_t _M0L3lenS447;
  int32_t _M0L3endS449;
  int32_t _M0L5startS453;
  int32_t _if__result_4600;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3lenS447 = Moonbit_array_length(_M0L4selfS448);
  if (_M0L3endS450 == 4294967296ll) {
    _M0L3endS449 = _M0L3lenS447;
  } else {
    int64_t _M0L7_2aSomeS451 = _M0L3endS450;
    int32_t _M0L6_2aendS452 = (int32_t)_M0L7_2aSomeS451;
    if (_M0L6_2aendS452 < 0) {
      _M0L3endS449 = _M0L3lenS447 + _M0L6_2aendS452;
    } else {
      _M0L3endS449 = _M0L6_2aendS452;
    }
  }
  if (_M0L5startS454 < 0) {
    _M0L5startS453 = _M0L3lenS447 + _M0L5startS454;
  } else {
    _M0L5startS453 = _M0L5startS454;
  }
  if (_M0L5startS453 >= 0) {
    if (_M0L5startS453 <= _M0L3endS449) {
      _if__result_4600 = _M0L3endS449 <= _M0L3lenS447;
    } else {
      _if__result_4600 = 0;
    }
  } else {
    _if__result_4600 = 0;
  }
  if (_if__result_4600) {
    moonbit_bytes_t _M0L7_2abindS455 = _M0L4selfS448;
    int32_t _M0L7_2abindS456 = _M0L3endS449 - _M0L5startS453;
    int32_t _M0L6_2atmpS2004 = _M0L5startS453 + _M0L7_2abindS456;
    return (struct _M0TPB9ArrayViewGyE){_M0L5startS453,
                                          _M0L6_2atmpS2004,
                                          _M0L7_2abindS455};
  } else {
    moonbit_decref(_M0L4selfS448);
    #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    return _M0FPB5abortGRPB9ArrayViewGyEE((moonbit_string_t)moonbit_string_literal_32.data, (moonbit_string_t)moonbit_string_literal_33.data);
  }
}

int32_t _M0MPC15array9ArrayView2atGyE(
  struct _M0TPB9ArrayViewGyE _M0L4selfS446,
  int32_t _M0L5indexS445
) {
  int32_t _if__result_4601;
  #line 132 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  if (_M0L5indexS445 >= 0) {
    int32_t _M0L3endS1991 = _M0L4selfS446.$2;
    int32_t _M0L5startS1992 = _M0L4selfS446.$1;
    int32_t _M0L6_2atmpS1990 = _M0L3endS1991 - _M0L5startS1992;
    _if__result_4601 = _M0L5indexS445 < _M0L6_2atmpS1990;
  } else {
    _if__result_4601 = 0;
  }
  if (_if__result_4601) {
    moonbit_bytes_t _M0L8_2afieldS4290 = _M0L4selfS446.$0;
    moonbit_bytes_t _M0L3bufS1993 = _M0L8_2afieldS4290;
    int32_t _M0L8_2afieldS4289 = _M0L4selfS446.$1;
    int32_t _M0L5startS1995 = _M0L8_2afieldS4289;
    int32_t _M0L6_2atmpS1994 = _M0L5startS1995 + _M0L5indexS445;
    int32_t _M0L6_2atmpS4288;
    if (
      _M0L6_2atmpS1994 < 0
      || _M0L6_2atmpS1994 >= Moonbit_array_length(_M0L3bufS1993)
    ) {
      #line 139 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4288 = (int32_t)_M0L3bufS1993[_M0L6_2atmpS1994];
    moonbit_decref(_M0L3bufS1993);
    return _M0L6_2atmpS4288;
  } else {
    int32_t _M0L3endS2002 = _M0L4selfS446.$2;
    int32_t _M0L8_2afieldS4294 = _M0L4selfS446.$1;
    int32_t _M0L5startS2003;
    int32_t _M0L6_2atmpS2001;
    moonbit_string_t _M0L6_2atmpS2000;
    moonbit_string_t _M0L6_2atmpS4293;
    moonbit_string_t _M0L6_2atmpS1999;
    moonbit_string_t _M0L6_2atmpS4292;
    moonbit_string_t _M0L6_2atmpS1997;
    moonbit_string_t _M0L6_2atmpS1998;
    moonbit_string_t _M0L6_2atmpS4291;
    moonbit_string_t _M0L6_2atmpS1996;
    moonbit_decref(_M0L4selfS446.$0);
    _M0L5startS2003 = _M0L8_2afieldS4294;
    _M0L6_2atmpS2001 = _M0L3endS2002 - _M0L5startS2003;
    #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS2000
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L6_2atmpS2001);
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS4293
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_34.data, _M0L6_2atmpS2000);
    moonbit_decref(_M0L6_2atmpS2000);
    _M0L6_2atmpS1999 = _M0L6_2atmpS4293;
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS4292
    = moonbit_add_string(_M0L6_2atmpS1999, (moonbit_string_t)moonbit_string_literal_35.data);
    moonbit_decref(_M0L6_2atmpS1999);
    _M0L6_2atmpS1997 = _M0L6_2atmpS4292;
    #line 136 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS1998
    = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS445);
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    _M0L6_2atmpS4291 = moonbit_add_string(_M0L6_2atmpS1997, _M0L6_2atmpS1998);
    moonbit_decref(_M0L6_2atmpS1997);
    moonbit_decref(_M0L6_2atmpS1998);
    _M0L6_2atmpS1996 = _M0L6_2atmpS4291;
    #line 135 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    return _M0FPB5abortGyE(_M0L6_2atmpS1996, (moonbit_string_t)moonbit_string_literal_36.data);
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS444
) {
  moonbit_string_t* _M0L6_2atmpS1989;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1989 = _M0L4selfS444;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1989);
}

uint32_t* _M0MPC15array10FixedArray5makeiGjE(
  int32_t _M0L6lengthS439,
  struct _M0TWiEj* _M0L5valueS441
) {
  #line 564 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  if (_M0L6lengthS439 <= 0) {
    moonbit_decref(_M0L5valueS441);
    return (uint32_t*)moonbit_empty_int32_array;
  } else {
    uint32_t _M0L6_2atmpS1988;
    uint32_t* _M0L5arrayS440;
    int32_t _M0L1iS442;
    moonbit_incref(_M0L5valueS441);
    #line 572 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
    _M0L6_2atmpS1988 = _M0L5valueS441->code(_M0L5valueS441, 0);
    _M0L5arrayS440
    = (uint32_t*)moonbit_make_int32_array(_M0L6lengthS439, _M0L6_2atmpS1988);
    _M0L1iS442 = 1;
    while (1) {
      if (_M0L1iS442 < _M0L6lengthS439) {
        uint32_t _M0L6_2atmpS1986;
        int32_t _M0L6_2atmpS1987;
        moonbit_incref(_M0L5valueS441);
        #line 574 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
        _M0L6_2atmpS1986 = _M0L5valueS441->code(_M0L5valueS441, _M0L1iS442);
        if (
          _M0L1iS442 < 0
          || _M0L1iS442 >= Moonbit_array_length(_M0L5arrayS440)
        ) {
          #line 574 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
          moonbit_panic();
        }
        _M0L5arrayS440[_M0L1iS442] = _M0L6_2atmpS1986;
        _M0L6_2atmpS1987 = _M0L1iS442 + 1;
        _M0L1iS442 = _M0L6_2atmpS1987;
        continue;
      } else {
        moonbit_decref(_M0L5valueS441);
      }
      break;
    }
    return _M0L5arrayS440;
  }
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS437
) {
  moonbit_string_t* _M0L6_2atmpS1981;
  int32_t _M0L6_2atmpS4295;
  int32_t _M0L6_2atmpS1982;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1980;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS437);
  _M0L6_2atmpS1981 = _M0L4selfS437;
  _M0L6_2atmpS4295 = Moonbit_array_length(_M0L4selfS437);
  moonbit_decref(_M0L4selfS437);
  _M0L6_2atmpS1982 = _M0L6_2atmpS4295;
  _M0L6_2atmpS1980
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1982, _M0L6_2atmpS1981
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1980);
}

struct _M0TWEOj* _M0MPC15array10FixedArray4iterGjE(uint32_t* _M0L4selfS438) {
  uint32_t* _M0L6_2atmpS1984;
  int32_t _M0L6_2atmpS4296;
  int32_t _M0L6_2atmpS1985;
  struct _M0TPB9ArrayViewGjE _M0L6_2atmpS1983;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS438);
  _M0L6_2atmpS1984 = _M0L4selfS438;
  _M0L6_2atmpS4296 = Moonbit_array_length(_M0L4selfS438);
  moonbit_decref(_M0L4selfS438);
  _M0L6_2atmpS1985 = _M0L6_2atmpS4296;
  _M0L6_2atmpS1983
  = (struct _M0TPB9ArrayViewGjE){
    0, _M0L6_2atmpS1985, _M0L6_2atmpS1984
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGjE(_M0L6_2atmpS1983);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS432
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS431;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1957__l570__* _closure_4603;
  struct _M0TWEOs* _M0L6_2atmpS1956;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS431
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS431)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS431->$0 = 0;
  _closure_4603
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1957__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1957__l570__));
  Moonbit_object_header(_closure_4603)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1957__l570__, $0_0) >> 2, 2, 0);
  _closure_4603->code = &_M0MPC15array9ArrayView4iterGsEC1957l570;
  _closure_4603->$0_0 = _M0L4selfS432.$0;
  _closure_4603->$0_1 = _M0L4selfS432.$1;
  _closure_4603->$0_2 = _M0L4selfS432.$2;
  _closure_4603->$1 = _M0L1iS431;
  _M0L6_2atmpS1956 = (struct _M0TWEOs*)_closure_4603;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1956);
}

struct _M0TWEOj* _M0MPC15array9ArrayView4iterGjE(
  struct _M0TPB9ArrayViewGjE _M0L4selfS435
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS434;
  struct _M0R57ArrayView_3a_3aiter_7c_5bUInt_5d_7c_2eanon__u1969__l570__* _closure_4604;
  struct _M0TWEOj* _M0L6_2atmpS1968;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS434
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS434)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS434->$0 = 0;
  _closure_4604
  = (struct _M0R57ArrayView_3a_3aiter_7c_5bUInt_5d_7c_2eanon__u1969__l570__*)moonbit_malloc(sizeof(struct _M0R57ArrayView_3a_3aiter_7c_5bUInt_5d_7c_2eanon__u1969__l570__));
  Moonbit_object_header(_closure_4604)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R57ArrayView_3a_3aiter_7c_5bUInt_5d_7c_2eanon__u1969__l570__, $0_0) >> 2, 2, 0);
  _closure_4604->code = &_M0MPC15array9ArrayView4iterGjEC1969l570;
  _closure_4604->$0_0 = _M0L4selfS435.$0;
  _closure_4604->$0_1 = _M0L4selfS435.$1;
  _closure_4604->$0_2 = _M0L4selfS435.$2;
  _closure_4604->$1 = _M0L1iS434;
  _M0L6_2atmpS1968 = (struct _M0TWEOj*)_closure_4604;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGjE(_M0L6_2atmpS1968);
}

int64_t _M0MPC15array9ArrayView4iterGjEC1969l570(
  struct _M0TWEOj* _M0L6_2aenvS1970
) {
  struct _M0R57ArrayView_3a_3aiter_7c_5bUInt_5d_7c_2eanon__u1969__l570__* _M0L14_2acasted__envS1971;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4301;
  struct _M0TPC13ref3RefGiE* _M0L1iS434;
  struct _M0TPB9ArrayViewGjE _M0L8_2afieldS4300;
  int32_t _M0L6_2acntS4459;
  struct _M0TPB9ArrayViewGjE _M0L4selfS435;
  int32_t _M0L3valS1972;
  int32_t _M0L6_2atmpS1973;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1971
  = (struct _M0R57ArrayView_3a_3aiter_7c_5bUInt_5d_7c_2eanon__u1969__l570__*)_M0L6_2aenvS1970;
  _M0L8_2afieldS4301 = _M0L14_2acasted__envS1971->$1;
  _M0L1iS434 = _M0L8_2afieldS4301;
  _M0L8_2afieldS4300
  = (struct _M0TPB9ArrayViewGjE){
    _M0L14_2acasted__envS1971->$0_1,
      _M0L14_2acasted__envS1971->$0_2,
      _M0L14_2acasted__envS1971->$0_0
  };
  _M0L6_2acntS4459 = Moonbit_object_header(_M0L14_2acasted__envS1971)->rc;
  if (_M0L6_2acntS4459 > 1) {
    int32_t _M0L11_2anew__cntS4460 = _M0L6_2acntS4459 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1971)->rc
    = _M0L11_2anew__cntS4460;
    moonbit_incref(_M0L1iS434);
    moonbit_incref(_M0L8_2afieldS4300.$0);
  } else if (_M0L6_2acntS4459 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1971);
  }
  _M0L4selfS435 = _M0L8_2afieldS4300;
  _M0L3valS1972 = _M0L1iS434->$0;
  moonbit_incref(_M0L4selfS435.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1973 = _M0MPC15array9ArrayView6lengthGjE(_M0L4selfS435);
  if (_M0L3valS1972 < _M0L6_2atmpS1973) {
    uint32_t* _M0L8_2afieldS4299 = _M0L4selfS435.$0;
    uint32_t* _M0L3bufS1976 = _M0L8_2afieldS4299;
    int32_t _M0L8_2afieldS4298 = _M0L4selfS435.$1;
    int32_t _M0L5startS1978 = _M0L8_2afieldS4298;
    int32_t _M0L3valS1979 = _M0L1iS434->$0;
    int32_t _M0L6_2atmpS1977 = _M0L5startS1978 + _M0L3valS1979;
    uint32_t _M0L6_2atmpS4297 = (uint32_t)_M0L3bufS1976[_M0L6_2atmpS1977];
    uint32_t _M0L4elemS436;
    int32_t _M0L3valS1975;
    int32_t _M0L6_2atmpS1974;
    moonbit_decref(_M0L3bufS1976);
    _M0L4elemS436 = _M0L6_2atmpS4297;
    _M0L3valS1975 = _M0L1iS434->$0;
    _M0L6_2atmpS1974 = _M0L3valS1975 + 1;
    _M0L1iS434->$0 = _M0L6_2atmpS1974;
    moonbit_decref(_M0L1iS434);
    return (int64_t)_M0L4elemS436;
  } else {
    moonbit_decref(_M0L4selfS435.$0);
    moonbit_decref(_M0L1iS434);
    return 4294967296ll;
  }
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1957l570(
  struct _M0TWEOs* _M0L6_2aenvS1958
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1957__l570__* _M0L14_2acasted__envS1959;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4306;
  struct _M0TPC13ref3RefGiE* _M0L1iS431;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS4305;
  int32_t _M0L6_2acntS4461;
  struct _M0TPB9ArrayViewGsE _M0L4selfS432;
  int32_t _M0L3valS1960;
  int32_t _M0L6_2atmpS1961;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1959
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1957__l570__*)_M0L6_2aenvS1958;
  _M0L8_2afieldS4306 = _M0L14_2acasted__envS1959->$1;
  _M0L1iS431 = _M0L8_2afieldS4306;
  _M0L8_2afieldS4305
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1959->$0_1,
      _M0L14_2acasted__envS1959->$0_2,
      _M0L14_2acasted__envS1959->$0_0
  };
  _M0L6_2acntS4461 = Moonbit_object_header(_M0L14_2acasted__envS1959)->rc;
  if (_M0L6_2acntS4461 > 1) {
    int32_t _M0L11_2anew__cntS4462 = _M0L6_2acntS4461 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1959)->rc
    = _M0L11_2anew__cntS4462;
    moonbit_incref(_M0L1iS431);
    moonbit_incref(_M0L8_2afieldS4305.$0);
  } else if (_M0L6_2acntS4461 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1959);
  }
  _M0L4selfS432 = _M0L8_2afieldS4305;
  _M0L3valS1960 = _M0L1iS431->$0;
  moonbit_incref(_M0L4selfS432.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1961 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS432);
  if (_M0L3valS1960 < _M0L6_2atmpS1961) {
    moonbit_string_t* _M0L8_2afieldS4304 = _M0L4selfS432.$0;
    moonbit_string_t* _M0L3bufS1964 = _M0L8_2afieldS4304;
    int32_t _M0L8_2afieldS4303 = _M0L4selfS432.$1;
    int32_t _M0L5startS1966 = _M0L8_2afieldS4303;
    int32_t _M0L3valS1967 = _M0L1iS431->$0;
    int32_t _M0L6_2atmpS1965 = _M0L5startS1966 + _M0L3valS1967;
    moonbit_string_t _M0L6_2atmpS4302 =
      (moonbit_string_t)_M0L3bufS1964[_M0L6_2atmpS1965];
    moonbit_string_t _M0L4elemS433;
    int32_t _M0L3valS1963;
    int32_t _M0L6_2atmpS1962;
    moonbit_incref(_M0L6_2atmpS4302);
    moonbit_decref(_M0L3bufS1964);
    _M0L4elemS433 = _M0L6_2atmpS4302;
    _M0L3valS1963 = _M0L1iS431->$0;
    _M0L6_2atmpS1962 = _M0L3valS1963 + 1;
    _M0L1iS431->$0 = _M0L6_2atmpS1962;
    moonbit_decref(_M0L1iS431);
    return _M0L4elemS433;
  } else {
    moonbit_decref(_M0L4selfS432.$0);
    moonbit_decref(_M0L1iS431);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS430
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS430;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS429,
  struct _M0TPB6Logger _M0L6loggerS428
) {
  moonbit_string_t _M0L6_2atmpS1955;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1955 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS429, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS428.$0->$method_0(_M0L6loggerS428.$1, _M0L6_2atmpS1955);
  return 0;
}

int32_t _M0IPC14bool4BoolPB4Show6output(
  int32_t _M0L4selfS426,
  struct _M0TPB6Logger _M0L6loggerS427
) {
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L4selfS426) {
    #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS427.$0->$method_0(_M0L6loggerS427.$1, (moonbit_string_t)moonbit_string_literal_37.data);
  } else {
    #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS427.$0->$method_0(_M0L6loggerS427.$1, (moonbit_string_t)moonbit_string_literal_38.data);
  }
  return 0;
}

struct _M0TWEOc* _M0MPC16string6String4iter(moonbit_string_t _M0L4selfS421) {
  int32_t _M0L3lenS420;
  struct _M0TPC13ref3RefGiE* _M0L5indexS422;
  struct _M0R38String_3a_3aiter_2eanon__u1939__l247__* _closure_4605;
  struct _M0TWEOc* _M0L6_2atmpS1938;
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L3lenS420 = Moonbit_array_length(_M0L4selfS421);
  _M0L5indexS422
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS422)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS422->$0 = 0;
  _closure_4605
  = (struct _M0R38String_3a_3aiter_2eanon__u1939__l247__*)moonbit_malloc(sizeof(struct _M0R38String_3a_3aiter_2eanon__u1939__l247__));
  Moonbit_object_header(_closure_4605)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R38String_3a_3aiter_2eanon__u1939__l247__, $0) >> 2, 2, 0);
  _closure_4605->code = &_M0MPC16string6String4iterC1939l247;
  _closure_4605->$0 = _M0L5indexS422;
  _closure_4605->$1 = _M0L4selfS421;
  _closure_4605->$2 = _M0L3lenS420;
  _M0L6_2atmpS1938 = (struct _M0TWEOc*)_closure_4605;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1938);
}

int32_t _M0MPC16string6String4iterC1939l247(
  struct _M0TWEOc* _M0L6_2aenvS1940
) {
  struct _M0R38String_3a_3aiter_2eanon__u1939__l247__* _M0L14_2acasted__envS1941;
  int32_t _M0L3lenS420;
  moonbit_string_t _M0L8_2afieldS4309;
  moonbit_string_t _M0L4selfS421;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4308;
  int32_t _M0L6_2acntS4463;
  struct _M0TPC13ref3RefGiE* _M0L5indexS422;
  int32_t _M0L3valS1942;
  #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L14_2acasted__envS1941
  = (struct _M0R38String_3a_3aiter_2eanon__u1939__l247__*)_M0L6_2aenvS1940;
  _M0L3lenS420 = _M0L14_2acasted__envS1941->$2;
  _M0L8_2afieldS4309 = _M0L14_2acasted__envS1941->$1;
  _M0L4selfS421 = _M0L8_2afieldS4309;
  _M0L8_2afieldS4308 = _M0L14_2acasted__envS1941->$0;
  _M0L6_2acntS4463 = Moonbit_object_header(_M0L14_2acasted__envS1941)->rc;
  if (_M0L6_2acntS4463 > 1) {
    int32_t _M0L11_2anew__cntS4464 = _M0L6_2acntS4463 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1941)->rc
    = _M0L11_2anew__cntS4464;
    moonbit_incref(_M0L4selfS421);
    moonbit_incref(_M0L8_2afieldS4308);
  } else if (_M0L6_2acntS4463 == 1) {
    #line 247 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    moonbit_free(_M0L14_2acasted__envS1941);
  }
  _M0L5indexS422 = _M0L8_2afieldS4308;
  _M0L3valS1942 = _M0L5indexS422->$0;
  if (_M0L3valS1942 < _M0L3lenS420) {
    int32_t _M0L3valS1954 = _M0L5indexS422->$0;
    int32_t _M0L2c1S423 = _M0L4selfS421[_M0L3valS1954];
    int32_t _if__result_4606;
    int32_t _M0L3valS1952;
    int32_t _M0L6_2atmpS1951;
    int32_t _M0L6_2atmpS1953;
    #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S423)) {
      int32_t _M0L3valS1944 = _M0L5indexS422->$0;
      int32_t _M0L6_2atmpS1943 = _M0L3valS1944 + 1;
      _if__result_4606 = _M0L6_2atmpS1943 < _M0L3lenS420;
    } else {
      _if__result_4606 = 0;
    }
    if (_if__result_4606) {
      int32_t _M0L3valS1950 = _M0L5indexS422->$0;
      int32_t _M0L6_2atmpS1949 = _M0L3valS1950 + 1;
      int32_t _M0L6_2atmpS4307 = _M0L4selfS421[_M0L6_2atmpS1949];
      int32_t _M0L2c2S424;
      moonbit_decref(_M0L4selfS421);
      _M0L2c2S424 = _M0L6_2atmpS4307;
      #line 252 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S424)) {
        int32_t _M0L6_2atmpS1947 = (int32_t)_M0L2c1S423;
        int32_t _M0L6_2atmpS1948 = (int32_t)_M0L2c2S424;
        int32_t _M0L1cS425;
        int32_t _M0L3valS1946;
        int32_t _M0L6_2atmpS1945;
        #line 253 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        _M0L1cS425
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1947, _M0L6_2atmpS1948);
        _M0L3valS1946 = _M0L5indexS422->$0;
        _M0L6_2atmpS1945 = _M0L3valS1946 + 2;
        _M0L5indexS422->$0 = _M0L6_2atmpS1945;
        moonbit_decref(_M0L5indexS422);
        return _M0L1cS425;
      }
    } else {
      moonbit_decref(_M0L4selfS421);
    }
    _M0L3valS1952 = _M0L5indexS422->$0;
    _M0L6_2atmpS1951 = _M0L3valS1952 + 1;
    _M0L5indexS422->$0 = _M0L6_2atmpS1951;
    moonbit_decref(_M0L5indexS422);
    #line 260 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
    _M0L6_2atmpS1953 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S423);
    return _M0L6_2atmpS1953;
  } else {
    moonbit_decref(_M0L5indexS422);
    moonbit_decref(_M0L4selfS421);
    return -1;
  }
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS414,
  moonbit_string_t _M0L5valueS416
) {
  int32_t _M0L3lenS1928;
  moonbit_string_t* _M0L6_2atmpS1930;
  int32_t _M0L6_2atmpS4312;
  int32_t _M0L6_2atmpS1929;
  int32_t _M0L6lengthS415;
  moonbit_string_t* _M0L8_2afieldS4311;
  moonbit_string_t* _M0L3bufS1931;
  moonbit_string_t _M0L6_2aoldS4310;
  int32_t _M0L6_2atmpS1932;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1928 = _M0L4selfS414->$1;
  moonbit_incref(_M0L4selfS414);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1930 = _M0MPC15array5Array6bufferGsE(_M0L4selfS414);
  _M0L6_2atmpS4312 = Moonbit_array_length(_M0L6_2atmpS1930);
  moonbit_decref(_M0L6_2atmpS1930);
  _M0L6_2atmpS1929 = _M0L6_2atmpS4312;
  if (_M0L3lenS1928 == _M0L6_2atmpS1929) {
    moonbit_incref(_M0L4selfS414);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS414);
  }
  _M0L6lengthS415 = _M0L4selfS414->$1;
  _M0L8_2afieldS4311 = _M0L4selfS414->$0;
  _M0L3bufS1931 = _M0L8_2afieldS4311;
  _M0L6_2aoldS4310 = (moonbit_string_t)_M0L3bufS1931[_M0L6lengthS415];
  moonbit_decref(_M0L6_2aoldS4310);
  _M0L3bufS1931[_M0L6lengthS415] = _M0L5valueS416;
  _M0L6_2atmpS1932 = _M0L6lengthS415 + 1;
  _M0L4selfS414->$1 = _M0L6_2atmpS1932;
  moonbit_decref(_M0L4selfS414);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS417,
  struct _M0TUsiE* _M0L5valueS419
) {
  int32_t _M0L3lenS1933;
  struct _M0TUsiE** _M0L6_2atmpS1935;
  int32_t _M0L6_2atmpS4315;
  int32_t _M0L6_2atmpS1934;
  int32_t _M0L6lengthS418;
  struct _M0TUsiE** _M0L8_2afieldS4314;
  struct _M0TUsiE** _M0L3bufS1936;
  struct _M0TUsiE* _M0L6_2aoldS4313;
  int32_t _M0L6_2atmpS1937;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1933 = _M0L4selfS417->$1;
  moonbit_incref(_M0L4selfS417);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1935 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS417);
  _M0L6_2atmpS4315 = Moonbit_array_length(_M0L6_2atmpS1935);
  moonbit_decref(_M0L6_2atmpS1935);
  _M0L6_2atmpS1934 = _M0L6_2atmpS4315;
  if (_M0L3lenS1933 == _M0L6_2atmpS1934) {
    moonbit_incref(_M0L4selfS417);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS417);
  }
  _M0L6lengthS418 = _M0L4selfS417->$1;
  _M0L8_2afieldS4314 = _M0L4selfS417->$0;
  _M0L3bufS1936 = _M0L8_2afieldS4314;
  _M0L6_2aoldS4313 = (struct _M0TUsiE*)_M0L3bufS1936[_M0L6lengthS418];
  if (_M0L6_2aoldS4313) {
    moonbit_decref(_M0L6_2aoldS4313);
  }
  _M0L3bufS1936[_M0L6lengthS418] = _M0L5valueS419;
  _M0L6_2atmpS1937 = _M0L6lengthS418 + 1;
  _M0L4selfS417->$1 = _M0L6_2atmpS1937;
  moonbit_decref(_M0L4selfS417);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS409) {
  int32_t _M0L8old__capS408;
  int32_t _M0L8new__capS410;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS408 = _M0L4selfS409->$1;
  if (_M0L8old__capS408 == 0) {
    _M0L8new__capS410 = 8;
  } else {
    _M0L8new__capS410 = _M0L8old__capS408 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS409, _M0L8new__capS410);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS412
) {
  int32_t _M0L8old__capS411;
  int32_t _M0L8new__capS413;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS411 = _M0L4selfS412->$1;
  if (_M0L8old__capS411 == 0) {
    _M0L8new__capS413 = 8;
  } else {
    _M0L8new__capS413 = _M0L8old__capS411 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS412, _M0L8new__capS413);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS399,
  int32_t _M0L13new__capacityS397
) {
  moonbit_string_t* _M0L8new__bufS396;
  moonbit_string_t* _M0L8_2afieldS4317;
  moonbit_string_t* _M0L8old__bufS398;
  int32_t _M0L8old__capS400;
  int32_t _M0L9copy__lenS401;
  moonbit_string_t* _M0L6_2aoldS4316;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS396
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS397, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS4317 = _M0L4selfS399->$0;
  _M0L8old__bufS398 = _M0L8_2afieldS4317;
  _M0L8old__capS400 = Moonbit_array_length(_M0L8old__bufS398);
  if (_M0L8old__capS400 < _M0L13new__capacityS397) {
    _M0L9copy__lenS401 = _M0L8old__capS400;
  } else {
    _M0L9copy__lenS401 = _M0L13new__capacityS397;
  }
  moonbit_incref(_M0L8old__bufS398);
  moonbit_incref(_M0L8new__bufS396);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS396, 0, _M0L8old__bufS398, 0, _M0L9copy__lenS401);
  _M0L6_2aoldS4316 = _M0L4selfS399->$0;
  moonbit_decref(_M0L6_2aoldS4316);
  _M0L4selfS399->$0 = _M0L8new__bufS396;
  moonbit_decref(_M0L4selfS399);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS405,
  int32_t _M0L13new__capacityS403
) {
  struct _M0TUsiE** _M0L8new__bufS402;
  struct _M0TUsiE** _M0L8_2afieldS4319;
  struct _M0TUsiE** _M0L8old__bufS404;
  int32_t _M0L8old__capS406;
  int32_t _M0L9copy__lenS407;
  struct _M0TUsiE** _M0L6_2aoldS4318;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS402
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS403, 0);
  _M0L8_2afieldS4319 = _M0L4selfS405->$0;
  _M0L8old__bufS404 = _M0L8_2afieldS4319;
  _M0L8old__capS406 = Moonbit_array_length(_M0L8old__bufS404);
  if (_M0L8old__capS406 < _M0L13new__capacityS403) {
    _M0L9copy__lenS407 = _M0L8old__capS406;
  } else {
    _M0L9copy__lenS407 = _M0L13new__capacityS403;
  }
  moonbit_incref(_M0L8old__bufS404);
  moonbit_incref(_M0L8new__bufS402);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS402, 0, _M0L8old__bufS404, 0, _M0L9copy__lenS407);
  _M0L6_2aoldS4318 = _M0L4selfS405->$0;
  moonbit_decref(_M0L6_2aoldS4318);
  _M0L4selfS405->$0 = _M0L8new__bufS402;
  moonbit_decref(_M0L4selfS405);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS395
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS395 == 0) {
    moonbit_string_t* _M0L6_2atmpS1926 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_4607 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4607)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4607->$0 = _M0L6_2atmpS1926;
    _block_4607->$1 = 0;
    return _block_4607;
  } else {
    moonbit_string_t* _M0L6_2atmpS1927 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS395, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_4608 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_4608)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_4608->$0 = _M0L6_2atmpS1927;
    _block_4608->$1 = 0;
    return _block_4608;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS393,
  struct _M0TPC16string10StringView _M0L3strS394
) {
  int32_t _M0L3lenS1914;
  int32_t _M0L6_2atmpS1916;
  int32_t _M0L6_2atmpS1915;
  int32_t _M0L6_2atmpS1913;
  moonbit_bytes_t _M0L8_2afieldS4320;
  moonbit_bytes_t _M0L4dataS1917;
  int32_t _M0L3lenS1918;
  moonbit_string_t _M0L6_2atmpS1919;
  int32_t _M0L6_2atmpS1920;
  int32_t _M0L6_2atmpS1921;
  int32_t _M0L3lenS1923;
  int32_t _M0L6_2atmpS1925;
  int32_t _M0L6_2atmpS1924;
  int32_t _M0L6_2atmpS1922;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1914 = _M0L4selfS393->$1;
  moonbit_incref(_M0L3strS394.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1916 = _M0MPC16string10StringView6length(_M0L3strS394);
  _M0L6_2atmpS1915 = _M0L6_2atmpS1916 * 2;
  _M0L6_2atmpS1913 = _M0L3lenS1914 + _M0L6_2atmpS1915;
  moonbit_incref(_M0L4selfS393);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS393, _M0L6_2atmpS1913);
  _M0L8_2afieldS4320 = _M0L4selfS393->$0;
  _M0L4dataS1917 = _M0L8_2afieldS4320;
  _M0L3lenS1918 = _M0L4selfS393->$1;
  moonbit_incref(_M0L4dataS1917);
  moonbit_incref(_M0L3strS394.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1919 = _M0MPC16string10StringView4data(_M0L3strS394);
  moonbit_incref(_M0L3strS394.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1920 = _M0MPC16string10StringView13start__offset(_M0L3strS394);
  moonbit_incref(_M0L3strS394.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1921 = _M0MPC16string10StringView6length(_M0L3strS394);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1917, _M0L3lenS1918, _M0L6_2atmpS1919, _M0L6_2atmpS1920, _M0L6_2atmpS1921);
  _M0L3lenS1923 = _M0L4selfS393->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1925 = _M0MPC16string10StringView6length(_M0L3strS394);
  _M0L6_2atmpS1924 = _M0L6_2atmpS1925 * 2;
  _M0L6_2atmpS1922 = _M0L3lenS1923 + _M0L6_2atmpS1924;
  _M0L4selfS393->$1 = _M0L6_2atmpS1922;
  moonbit_decref(_M0L4selfS393);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS385,
  int32_t _M0L3lenS388,
  int32_t _M0L13start__offsetS392,
  int64_t _M0L11end__offsetS383
) {
  int32_t _M0L11end__offsetS382;
  int32_t _M0L5indexS386;
  int32_t _M0L5countS387;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS383 == 4294967296ll) {
    _M0L11end__offsetS382 = Moonbit_array_length(_M0L4selfS385);
  } else {
    int64_t _M0L7_2aSomeS384 = _M0L11end__offsetS383;
    _M0L11end__offsetS382 = (int32_t)_M0L7_2aSomeS384;
  }
  _M0L5indexS386 = _M0L13start__offsetS392;
  _M0L5countS387 = 0;
  while (1) {
    int32_t _if__result_4610;
    if (_M0L5indexS386 < _M0L11end__offsetS382) {
      _if__result_4610 = _M0L5countS387 < _M0L3lenS388;
    } else {
      _if__result_4610 = 0;
    }
    if (_if__result_4610) {
      int32_t _M0L2c1S389 = _M0L4selfS385[_M0L5indexS386];
      int32_t _if__result_4611;
      int32_t _M0L6_2atmpS1911;
      int32_t _M0L6_2atmpS1912;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S389)) {
        int32_t _M0L6_2atmpS1907 = _M0L5indexS386 + 1;
        _if__result_4611 = _M0L6_2atmpS1907 < _M0L11end__offsetS382;
      } else {
        _if__result_4611 = 0;
      }
      if (_if__result_4611) {
        int32_t _M0L6_2atmpS1910 = _M0L5indexS386 + 1;
        int32_t _M0L2c2S390 = _M0L4selfS385[_M0L6_2atmpS1910];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S390)) {
          int32_t _M0L6_2atmpS1908 = _M0L5indexS386 + 2;
          int32_t _M0L6_2atmpS1909 = _M0L5countS387 + 1;
          _M0L5indexS386 = _M0L6_2atmpS1908;
          _M0L5countS387 = _M0L6_2atmpS1909;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_39.data, (moonbit_string_t)moonbit_string_literal_40.data);
        }
      }
      _M0L6_2atmpS1911 = _M0L5indexS386 + 1;
      _M0L6_2atmpS1912 = _M0L5countS387 + 1;
      _M0L5indexS386 = _M0L6_2atmpS1911;
      _M0L5countS387 = _M0L6_2atmpS1912;
      continue;
    } else {
      moonbit_decref(_M0L4selfS385);
      return _M0L5countS387 >= _M0L3lenS388;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS377
) {
  int32_t _M0L3endS1897;
  int32_t _M0L8_2afieldS4321;
  int32_t _M0L5startS1898;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1897 = _M0L4selfS377.$2;
  _M0L8_2afieldS4321 = _M0L4selfS377.$1;
  moonbit_decref(_M0L4selfS377.$0);
  _M0L5startS1898 = _M0L8_2afieldS4321;
  return _M0L3endS1897 - _M0L5startS1898;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS378
) {
  int32_t _M0L3endS1899;
  int32_t _M0L8_2afieldS4322;
  int32_t _M0L5startS1900;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1899 = _M0L4selfS378.$2;
  _M0L8_2afieldS4322 = _M0L4selfS378.$1;
  moonbit_decref(_M0L4selfS378.$0);
  _M0L5startS1900 = _M0L8_2afieldS4322;
  return _M0L3endS1899 - _M0L5startS1900;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS379
) {
  int32_t _M0L3endS1901;
  int32_t _M0L8_2afieldS4323;
  int32_t _M0L5startS1902;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1901 = _M0L4selfS379.$2;
  _M0L8_2afieldS4323 = _M0L4selfS379.$1;
  moonbit_decref(_M0L4selfS379.$0);
  _M0L5startS1902 = _M0L8_2afieldS4323;
  return _M0L3endS1901 - _M0L5startS1902;
}

int32_t _M0MPC15array9ArrayView6lengthGyE(
  struct _M0TPB9ArrayViewGyE _M0L4selfS380
) {
  int32_t _M0L3endS1903;
  int32_t _M0L8_2afieldS4324;
  int32_t _M0L5startS1904;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1903 = _M0L4selfS380.$2;
  _M0L8_2afieldS4324 = _M0L4selfS380.$1;
  moonbit_decref(_M0L4selfS380.$0);
  _M0L5startS1904 = _M0L8_2afieldS4324;
  return _M0L3endS1903 - _M0L5startS1904;
}

int32_t _M0MPC15array9ArrayView6lengthGjE(
  struct _M0TPB9ArrayViewGjE _M0L4selfS381
) {
  int32_t _M0L3endS1905;
  int32_t _M0L8_2afieldS4325;
  int32_t _M0L5startS1906;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1905 = _M0L4selfS381.$2;
  _M0L8_2afieldS4325 = _M0L4selfS381.$1;
  moonbit_decref(_M0L4selfS381.$0);
  _M0L5startS1906 = _M0L8_2afieldS4325;
  return _M0L3endS1905 - _M0L5startS1906;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS375,
  int64_t _M0L19start__offset_2eoptS373,
  int64_t _M0L11end__offsetS376
) {
  int32_t _M0L13start__offsetS372;
  if (_M0L19start__offset_2eoptS373 == 4294967296ll) {
    _M0L13start__offsetS372 = 0;
  } else {
    int64_t _M0L7_2aSomeS374 = _M0L19start__offset_2eoptS373;
    _M0L13start__offsetS372 = (int32_t)_M0L7_2aSomeS374;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS375, _M0L13start__offsetS372, _M0L11end__offsetS376);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS370,
  int32_t _M0L13start__offsetS371,
  int64_t _M0L11end__offsetS368
) {
  int32_t _M0L11end__offsetS367;
  int32_t _if__result_4612;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS368 == 4294967296ll) {
    _M0L11end__offsetS367 = Moonbit_array_length(_M0L4selfS370);
  } else {
    int64_t _M0L7_2aSomeS369 = _M0L11end__offsetS368;
    _M0L11end__offsetS367 = (int32_t)_M0L7_2aSomeS369;
  }
  if (_M0L13start__offsetS371 >= 0) {
    if (_M0L13start__offsetS371 <= _M0L11end__offsetS367) {
      int32_t _M0L6_2atmpS1896 = Moonbit_array_length(_M0L4selfS370);
      _if__result_4612 = _M0L11end__offsetS367 <= _M0L6_2atmpS1896;
    } else {
      _if__result_4612 = 0;
    }
  } else {
    _if__result_4612 = 0;
  }
  if (_if__result_4612) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS371,
                                                 _M0L11end__offsetS367,
                                                 _M0L4selfS370};
  } else {
    moonbit_decref(_M0L4selfS370);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_22.data, (moonbit_string_t)moonbit_string_literal_41.data);
  }
}

struct _M0TWEOc* _M0MPC16string10StringView4iter(
  struct _M0TPC16string10StringView _M0L4selfS362
) {
  int32_t _M0L5startS361;
  int32_t _M0L3endS363;
  struct _M0TPC13ref3RefGiE* _M0L5indexS364;
  struct _M0R42StringView_3a_3aiter_2eanon__u1876__l198__* _closure_4613;
  struct _M0TWEOc* _M0L6_2atmpS1875;
  #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L5startS361 = _M0L4selfS362.$1;
  _M0L3endS363 = _M0L4selfS362.$2;
  _M0L5indexS364
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5indexS364)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5indexS364->$0 = _M0L5startS361;
  _closure_4613
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1876__l198__*)moonbit_malloc(sizeof(struct _M0R42StringView_3a_3aiter_2eanon__u1876__l198__));
  Moonbit_object_header(_closure_4613)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R42StringView_3a_3aiter_2eanon__u1876__l198__, $0) >> 2, 2, 0);
  _closure_4613->code = &_M0MPC16string10StringView4iterC1876l198;
  _closure_4613->$0 = _M0L5indexS364;
  _closure_4613->$1 = _M0L3endS363;
  _closure_4613->$2_0 = _M0L4selfS362.$0;
  _closure_4613->$2_1 = _M0L4selfS362.$1;
  _closure_4613->$2_2 = _M0L4selfS362.$2;
  _M0L6_2atmpS1875 = (struct _M0TWEOc*)_closure_4613;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPB4Iter3newGcE(_M0L6_2atmpS1875);
}

int32_t _M0MPC16string10StringView4iterC1876l198(
  struct _M0TWEOc* _M0L6_2aenvS1877
) {
  struct _M0R42StringView_3a_3aiter_2eanon__u1876__l198__* _M0L14_2acasted__envS1878;
  struct _M0TPC16string10StringView _M0L8_2afieldS4331;
  struct _M0TPC16string10StringView _M0L4selfS362;
  int32_t _M0L3endS363;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS4330;
  int32_t _M0L6_2acntS4465;
  struct _M0TPC13ref3RefGiE* _M0L5indexS364;
  int32_t _M0L3valS1879;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L14_2acasted__envS1878
  = (struct _M0R42StringView_3a_3aiter_2eanon__u1876__l198__*)_M0L6_2aenvS1877;
  _M0L8_2afieldS4331
  = (struct _M0TPC16string10StringView){
    _M0L14_2acasted__envS1878->$2_1,
      _M0L14_2acasted__envS1878->$2_2,
      _M0L14_2acasted__envS1878->$2_0
  };
  _M0L4selfS362 = _M0L8_2afieldS4331;
  _M0L3endS363 = _M0L14_2acasted__envS1878->$1;
  _M0L8_2afieldS4330 = _M0L14_2acasted__envS1878->$0;
  _M0L6_2acntS4465 = Moonbit_object_header(_M0L14_2acasted__envS1878)->rc;
  if (_M0L6_2acntS4465 > 1) {
    int32_t _M0L11_2anew__cntS4466 = _M0L6_2acntS4465 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1878)->rc
    = _M0L11_2anew__cntS4466;
    moonbit_incref(_M0L4selfS362.$0);
    moonbit_incref(_M0L8_2afieldS4330);
  } else if (_M0L6_2acntS4465 == 1) {
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_free(_M0L14_2acasted__envS1878);
  }
  _M0L5indexS364 = _M0L8_2afieldS4330;
  _M0L3valS1879 = _M0L5indexS364->$0;
  if (_M0L3valS1879 < _M0L3endS363) {
    moonbit_string_t _M0L8_2afieldS4329 = _M0L4selfS362.$0;
    moonbit_string_t _M0L3strS1894 = _M0L8_2afieldS4329;
    int32_t _M0L3valS1895 = _M0L5indexS364->$0;
    int32_t _M0L6_2atmpS4328 = _M0L3strS1894[_M0L3valS1895];
    int32_t _M0L2c1S365 = _M0L6_2atmpS4328;
    int32_t _if__result_4614;
    int32_t _M0L3valS1892;
    int32_t _M0L6_2atmpS1891;
    int32_t _M0L6_2atmpS1893;
    #line 201 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S365)) {
      int32_t _M0L3valS1882 = _M0L5indexS364->$0;
      int32_t _M0L6_2atmpS1880 = _M0L3valS1882 + 1;
      int32_t _M0L3endS1881 = _M0L4selfS362.$2;
      _if__result_4614 = _M0L6_2atmpS1880 < _M0L3endS1881;
    } else {
      _if__result_4614 = 0;
    }
    if (_if__result_4614) {
      moonbit_string_t _M0L8_2afieldS4327 = _M0L4selfS362.$0;
      moonbit_string_t _M0L3strS1888 = _M0L8_2afieldS4327;
      int32_t _M0L3valS1890 = _M0L5indexS364->$0;
      int32_t _M0L6_2atmpS1889 = _M0L3valS1890 + 1;
      int32_t _M0L6_2atmpS4326 = _M0L3strS1888[_M0L6_2atmpS1889];
      int32_t _M0L2c2S366;
      moonbit_decref(_M0L3strS1888);
      _M0L2c2S366 = _M0L6_2atmpS4326;
      #line 203 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S366)) {
        int32_t _M0L3valS1884 = _M0L5indexS364->$0;
        int32_t _M0L6_2atmpS1883 = _M0L3valS1884 + 2;
        int32_t _M0L6_2atmpS1886;
        int32_t _M0L6_2atmpS1887;
        int32_t _M0L6_2atmpS1885;
        _M0L5indexS364->$0 = _M0L6_2atmpS1883;
        moonbit_decref(_M0L5indexS364);
        _M0L6_2atmpS1886 = (int32_t)_M0L2c1S365;
        _M0L6_2atmpS1887 = (int32_t)_M0L2c2S366;
        #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        _M0L6_2atmpS1885
        = _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1886, _M0L6_2atmpS1887);
        return _M0L6_2atmpS1885;
      }
    } else {
      moonbit_decref(_M0L4selfS362.$0);
    }
    _M0L3valS1892 = _M0L5indexS364->$0;
    _M0L6_2atmpS1891 = _M0L3valS1892 + 1;
    _M0L5indexS364->$0 = _M0L6_2atmpS1891;
    moonbit_decref(_M0L5indexS364);
    #line 209 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    _M0L6_2atmpS1893 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S365);
    return _M0L6_2atmpS1893;
  } else {
    moonbit_decref(_M0L5indexS364);
    moonbit_decref(_M0L4selfS362.$0);
    return -1;
  }
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS353,
  struct _M0TPB6Logger _M0L6loggerS351
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS352;
  int32_t _M0L3lenS354;
  int32_t _M0L1iS355;
  int32_t _M0L3segS356;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS351.$1) {
    moonbit_incref(_M0L6loggerS351.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS351.$0->$method_3(_M0L6loggerS351.$1, 34);
  moonbit_incref(_M0L4selfS353);
  if (_M0L6loggerS351.$1) {
    moonbit_incref(_M0L6loggerS351.$1);
  }
  _M0L6_2aenvS352
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS352)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS352->$0 = _M0L4selfS353;
  _M0L6_2aenvS352->$1_0 = _M0L6loggerS351.$0;
  _M0L6_2aenvS352->$1_1 = _M0L6loggerS351.$1;
  _M0L3lenS354 = Moonbit_array_length(_M0L4selfS353);
  _M0L1iS355 = 0;
  _M0L3segS356 = 0;
  _2afor_357:;
  while (1) {
    int32_t _M0L4codeS358;
    int32_t _M0L1cS360;
    int32_t _M0L6_2atmpS1859;
    int32_t _M0L6_2atmpS1860;
    int32_t _M0L6_2atmpS1861;
    int32_t _tmp_4618;
    int32_t _tmp_4619;
    if (_M0L1iS355 >= _M0L3lenS354) {
      moonbit_decref(_M0L4selfS353);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS352, _M0L3segS356, _M0L1iS355);
      break;
    }
    _M0L4codeS358 = _M0L4selfS353[_M0L1iS355];
    switch (_M0L4codeS358) {
      case 34: {
        _M0L1cS360 = _M0L4codeS358;
        goto join_359;
        break;
      }
      
      case 92: {
        _M0L1cS360 = _M0L4codeS358;
        goto join_359;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1862;
        int32_t _M0L6_2atmpS1863;
        moonbit_incref(_M0L6_2aenvS352);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS352, _M0L3segS356, _M0L1iS355);
        if (_M0L6loggerS351.$1) {
          moonbit_incref(_M0L6loggerS351.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS351.$0->$method_0(_M0L6loggerS351.$1, (moonbit_string_t)moonbit_string_literal_42.data);
        _M0L6_2atmpS1862 = _M0L1iS355 + 1;
        _M0L6_2atmpS1863 = _M0L1iS355 + 1;
        _M0L1iS355 = _M0L6_2atmpS1862;
        _M0L3segS356 = _M0L6_2atmpS1863;
        goto _2afor_357;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1864;
        int32_t _M0L6_2atmpS1865;
        moonbit_incref(_M0L6_2aenvS352);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS352, _M0L3segS356, _M0L1iS355);
        if (_M0L6loggerS351.$1) {
          moonbit_incref(_M0L6loggerS351.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS351.$0->$method_0(_M0L6loggerS351.$1, (moonbit_string_t)moonbit_string_literal_43.data);
        _M0L6_2atmpS1864 = _M0L1iS355 + 1;
        _M0L6_2atmpS1865 = _M0L1iS355 + 1;
        _M0L1iS355 = _M0L6_2atmpS1864;
        _M0L3segS356 = _M0L6_2atmpS1865;
        goto _2afor_357;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1866;
        int32_t _M0L6_2atmpS1867;
        moonbit_incref(_M0L6_2aenvS352);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS352, _M0L3segS356, _M0L1iS355);
        if (_M0L6loggerS351.$1) {
          moonbit_incref(_M0L6loggerS351.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS351.$0->$method_0(_M0L6loggerS351.$1, (moonbit_string_t)moonbit_string_literal_44.data);
        _M0L6_2atmpS1866 = _M0L1iS355 + 1;
        _M0L6_2atmpS1867 = _M0L1iS355 + 1;
        _M0L1iS355 = _M0L6_2atmpS1866;
        _M0L3segS356 = _M0L6_2atmpS1867;
        goto _2afor_357;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1868;
        int32_t _M0L6_2atmpS1869;
        moonbit_incref(_M0L6_2aenvS352);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS352, _M0L3segS356, _M0L1iS355);
        if (_M0L6loggerS351.$1) {
          moonbit_incref(_M0L6loggerS351.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS351.$0->$method_0(_M0L6loggerS351.$1, (moonbit_string_t)moonbit_string_literal_45.data);
        _M0L6_2atmpS1868 = _M0L1iS355 + 1;
        _M0L6_2atmpS1869 = _M0L1iS355 + 1;
        _M0L1iS355 = _M0L6_2atmpS1868;
        _M0L3segS356 = _M0L6_2atmpS1869;
        goto _2afor_357;
        break;
      }
      default: {
        if (_M0L4codeS358 < 32) {
          int32_t _M0L6_2atmpS1871;
          moonbit_string_t _M0L6_2atmpS1870;
          int32_t _M0L6_2atmpS1872;
          int32_t _M0L6_2atmpS1873;
          moonbit_incref(_M0L6_2aenvS352);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS352, _M0L3segS356, _M0L1iS355);
          if (_M0L6loggerS351.$1) {
            moonbit_incref(_M0L6loggerS351.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS351.$0->$method_0(_M0L6loggerS351.$1, (moonbit_string_t)moonbit_string_literal_46.data);
          _M0L6_2atmpS1871 = _M0L4codeS358 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1870 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1871);
          if (_M0L6loggerS351.$1) {
            moonbit_incref(_M0L6loggerS351.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS351.$0->$method_0(_M0L6loggerS351.$1, _M0L6_2atmpS1870);
          if (_M0L6loggerS351.$1) {
            moonbit_incref(_M0L6loggerS351.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS351.$0->$method_3(_M0L6loggerS351.$1, 125);
          _M0L6_2atmpS1872 = _M0L1iS355 + 1;
          _M0L6_2atmpS1873 = _M0L1iS355 + 1;
          _M0L1iS355 = _M0L6_2atmpS1872;
          _M0L3segS356 = _M0L6_2atmpS1873;
          goto _2afor_357;
        } else {
          int32_t _M0L6_2atmpS1874 = _M0L1iS355 + 1;
          int32_t _tmp_4617 = _M0L3segS356;
          _M0L1iS355 = _M0L6_2atmpS1874;
          _M0L3segS356 = _tmp_4617;
          goto _2afor_357;
        }
        break;
      }
    }
    goto joinlet_4616;
    join_359:;
    moonbit_incref(_M0L6_2aenvS352);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS352, _M0L3segS356, _M0L1iS355);
    if (_M0L6loggerS351.$1) {
      moonbit_incref(_M0L6loggerS351.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS351.$0->$method_3(_M0L6loggerS351.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1859 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS360);
    if (_M0L6loggerS351.$1) {
      moonbit_incref(_M0L6loggerS351.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS351.$0->$method_3(_M0L6loggerS351.$1, _M0L6_2atmpS1859);
    _M0L6_2atmpS1860 = _M0L1iS355 + 1;
    _M0L6_2atmpS1861 = _M0L1iS355 + 1;
    _M0L1iS355 = _M0L6_2atmpS1860;
    _M0L3segS356 = _M0L6_2atmpS1861;
    continue;
    joinlet_4616:;
    _tmp_4618 = _M0L1iS355;
    _tmp_4619 = _M0L3segS356;
    _M0L1iS355 = _tmp_4618;
    _M0L3segS356 = _tmp_4619;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS351.$0->$method_3(_M0L6loggerS351.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS347,
  int32_t _M0L3segS350,
  int32_t _M0L1iS349
) {
  struct _M0TPB6Logger _M0L8_2afieldS4333;
  struct _M0TPB6Logger _M0L6loggerS346;
  moonbit_string_t _M0L8_2afieldS4332;
  int32_t _M0L6_2acntS4467;
  moonbit_string_t _M0L4selfS348;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS4333
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS347->$1_0, _M0L6_2aenvS347->$1_1
  };
  _M0L6loggerS346 = _M0L8_2afieldS4333;
  _M0L8_2afieldS4332 = _M0L6_2aenvS347->$0;
  _M0L6_2acntS4467 = Moonbit_object_header(_M0L6_2aenvS347)->rc;
  if (_M0L6_2acntS4467 > 1) {
    int32_t _M0L11_2anew__cntS4468 = _M0L6_2acntS4467 - 1;
    Moonbit_object_header(_M0L6_2aenvS347)->rc = _M0L11_2anew__cntS4468;
    if (_M0L6loggerS346.$1) {
      moonbit_incref(_M0L6loggerS346.$1);
    }
    moonbit_incref(_M0L8_2afieldS4332);
  } else if (_M0L6_2acntS4467 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS347);
  }
  _M0L4selfS348 = _M0L8_2afieldS4332;
  if (_M0L1iS349 > _M0L3segS350) {
    int32_t _M0L6_2atmpS1858 = _M0L1iS349 - _M0L3segS350;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS346.$0->$method_1(_M0L6loggerS346.$1, _M0L4selfS348, _M0L3segS350, _M0L6_2atmpS1858);
  } else {
    moonbit_decref(_M0L4selfS348);
    if (_M0L6loggerS346.$1) {
      moonbit_decref(_M0L6loggerS346.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS345) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS344;
  int32_t _M0L6_2atmpS1855;
  int32_t _M0L6_2atmpS1854;
  int32_t _M0L6_2atmpS1857;
  int32_t _M0L6_2atmpS1856;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1853;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS344 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1855 = _M0IPC14byte4BytePB3Div3div(_M0L1bS345, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1854
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1855);
  moonbit_incref(_M0L7_2aselfS344);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS344, _M0L6_2atmpS1854);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1857 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS345, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1856
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1857);
  moonbit_incref(_M0L7_2aselfS344);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS344, _M0L6_2atmpS1856);
  _M0L6_2atmpS1853 = _M0L7_2aselfS344;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1853);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS343) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS343 < 10) {
    int32_t _M0L6_2atmpS1850;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1850 = _M0IPC14byte4BytePB3Add3add(_M0L1iS343, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1850);
  } else {
    int32_t _M0L6_2atmpS1852;
    int32_t _M0L6_2atmpS1851;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1852 = _M0IPC14byte4BytePB3Add3add(_M0L1iS343, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1851 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1852, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1851);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS341,
  int32_t _M0L4thatS342
) {
  int32_t _M0L6_2atmpS1848;
  int32_t _M0L6_2atmpS1849;
  int32_t _M0L6_2atmpS1847;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1848 = (int32_t)_M0L4selfS341;
  _M0L6_2atmpS1849 = (int32_t)_M0L4thatS342;
  _M0L6_2atmpS1847 = _M0L6_2atmpS1848 - _M0L6_2atmpS1849;
  return _M0L6_2atmpS1847 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS339,
  int32_t _M0L4thatS340
) {
  int32_t _M0L6_2atmpS1845;
  int32_t _M0L6_2atmpS1846;
  int32_t _M0L6_2atmpS1844;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1845 = (int32_t)_M0L4selfS339;
  _M0L6_2atmpS1846 = (int32_t)_M0L4thatS340;
  _M0L6_2atmpS1844 = _M0L6_2atmpS1845 % _M0L6_2atmpS1846;
  return _M0L6_2atmpS1844 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS337,
  int32_t _M0L4thatS338
) {
  int32_t _M0L6_2atmpS1842;
  int32_t _M0L6_2atmpS1843;
  int32_t _M0L6_2atmpS1841;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1842 = (int32_t)_M0L4selfS337;
  _M0L6_2atmpS1843 = (int32_t)_M0L4thatS338;
  _M0L6_2atmpS1841 = _M0L6_2atmpS1842 / _M0L6_2atmpS1843;
  return _M0L6_2atmpS1841 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS335,
  int32_t _M0L4thatS336
) {
  int32_t _M0L6_2atmpS1839;
  int32_t _M0L6_2atmpS1840;
  int32_t _M0L6_2atmpS1838;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1839 = (int32_t)_M0L4selfS335;
  _M0L6_2atmpS1840 = (int32_t)_M0L4thatS336;
  _M0L6_2atmpS1838 = _M0L6_2atmpS1839 + _M0L6_2atmpS1840;
  return _M0L6_2atmpS1838 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS332,
  int32_t _M0L5startS330,
  int32_t _M0L3endS331
) {
  int32_t _if__result_4620;
  int32_t _M0L3lenS333;
  int32_t _M0L6_2atmpS1836;
  int32_t _M0L6_2atmpS1837;
  moonbit_bytes_t _M0L5bytesS334;
  moonbit_bytes_t _M0L6_2atmpS1835;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS330 == 0) {
    int32_t _M0L6_2atmpS1834 = Moonbit_array_length(_M0L3strS332);
    _if__result_4620 = _M0L3endS331 == _M0L6_2atmpS1834;
  } else {
    _if__result_4620 = 0;
  }
  if (_if__result_4620) {
    return _M0L3strS332;
  }
  _M0L3lenS333 = _M0L3endS331 - _M0L5startS330;
  _M0L6_2atmpS1836 = _M0L3lenS333 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1837 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS334
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1836, _M0L6_2atmpS1837);
  moonbit_incref(_M0L5bytesS334);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS334, 0, _M0L3strS332, _M0L5startS330, _M0L3lenS333);
  _M0L6_2atmpS1835 = _M0L5bytesS334;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1835, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS326) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS326;
}

struct _M0TWEOc* _M0MPB4Iter3newGcE(struct _M0TWEOc* _M0L1fS327) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS327;
}

struct _M0TWEOc* _M0MPB4Iter3newGyE(struct _M0TWEOc* _M0L1fS328) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS328;
}

struct _M0TWEOj* _M0MPB4Iter3newGjE(struct _M0TWEOj* _M0L1fS329) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS329;
}

struct moonbit_result_0 _M0FPB4failGuE(
  moonbit_string_t _M0L3msgS325,
  moonbit_string_t _M0L3locS324
) {
  moonbit_string_t _M0L6_2atmpS1833;
  moonbit_string_t _M0L6_2atmpS4335;
  moonbit_string_t _M0L6_2atmpS1831;
  moonbit_string_t _M0L6_2atmpS1832;
  moonbit_string_t _M0L6_2atmpS4334;
  moonbit_string_t _M0L6_2atmpS1830;
  void* _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1829;
  struct moonbit_result_0 _result_4621;
  #line 55 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS1833
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS324);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS4335
  = moonbit_add_string(_M0L6_2atmpS1833, (moonbit_string_t)moonbit_string_literal_47.data);
  moonbit_decref(_M0L6_2atmpS1833);
  _M0L6_2atmpS1831 = _M0L6_2atmpS4335;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS1832 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS325);
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L6_2atmpS4334 = moonbit_add_string(_M0L6_2atmpS1831, _M0L6_2atmpS1832);
  moonbit_decref(_M0L6_2atmpS1831);
  moonbit_decref(_M0L6_2atmpS1832);
  _M0L6_2atmpS1830 = _M0L6_2atmpS4334;
  _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1829
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure));
  Moonbit_object_header(_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1829)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure, $0) >> 2, 1, 0);
  ((struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1829)->$0
  = _M0L6_2atmpS1830;
  _result_4621.tag = 0;
  _result_4621.data.err
  = _M0L48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailureS1829;
  return _result_4621;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS308,
  int32_t _M0L5radixS307
) {
  int32_t _if__result_4622;
  int32_t _M0L12is__negativeS309;
  uint32_t _M0L3numS310;
  uint16_t* _M0L6bufferS311;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS307 < 2) {
    _if__result_4622 = 1;
  } else {
    _if__result_4622 = _M0L5radixS307 > 36;
  }
  if (_if__result_4622) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_48.data, (moonbit_string_t)moonbit_string_literal_49.data);
  }
  if (_M0L4selfS308 == 0) {
    return (moonbit_string_t)moonbit_string_literal_50.data;
  }
  _M0L12is__negativeS309 = _M0L4selfS308 < 0;
  if (_M0L12is__negativeS309) {
    int32_t _M0L6_2atmpS1828 = -_M0L4selfS308;
    _M0L3numS310 = *(uint32_t*)&_M0L6_2atmpS1828;
  } else {
    _M0L3numS310 = *(uint32_t*)&_M0L4selfS308;
  }
  switch (_M0L5radixS307) {
    case 10: {
      int32_t _M0L10digit__lenS312;
      int32_t _M0L6_2atmpS1825;
      int32_t _M0L10total__lenS313;
      uint16_t* _M0L6bufferS314;
      int32_t _M0L12digit__startS315;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS312 = _M0FPB12dec__count32(_M0L3numS310);
      if (_M0L12is__negativeS309) {
        _M0L6_2atmpS1825 = 1;
      } else {
        _M0L6_2atmpS1825 = 0;
      }
      _M0L10total__lenS313 = _M0L10digit__lenS312 + _M0L6_2atmpS1825;
      _M0L6bufferS314
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS313, 0);
      if (_M0L12is__negativeS309) {
        _M0L12digit__startS315 = 1;
      } else {
        _M0L12digit__startS315 = 0;
      }
      moonbit_incref(_M0L6bufferS314);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS314, _M0L3numS310, _M0L12digit__startS315, _M0L10total__lenS313);
      _M0L6bufferS311 = _M0L6bufferS314;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS316;
      int32_t _M0L6_2atmpS1826;
      int32_t _M0L10total__lenS317;
      uint16_t* _M0L6bufferS318;
      int32_t _M0L12digit__startS319;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS316 = _M0FPB12hex__count32(_M0L3numS310);
      if (_M0L12is__negativeS309) {
        _M0L6_2atmpS1826 = 1;
      } else {
        _M0L6_2atmpS1826 = 0;
      }
      _M0L10total__lenS317 = _M0L10digit__lenS316 + _M0L6_2atmpS1826;
      _M0L6bufferS318
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS317, 0);
      if (_M0L12is__negativeS309) {
        _M0L12digit__startS319 = 1;
      } else {
        _M0L12digit__startS319 = 0;
      }
      moonbit_incref(_M0L6bufferS318);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS318, _M0L3numS310, _M0L12digit__startS319, _M0L10total__lenS317);
      _M0L6bufferS311 = _M0L6bufferS318;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS320;
      int32_t _M0L6_2atmpS1827;
      int32_t _M0L10total__lenS321;
      uint16_t* _M0L6bufferS322;
      int32_t _M0L12digit__startS323;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS320
      = _M0FPB14radix__count32(_M0L3numS310, _M0L5radixS307);
      if (_M0L12is__negativeS309) {
        _M0L6_2atmpS1827 = 1;
      } else {
        _M0L6_2atmpS1827 = 0;
      }
      _M0L10total__lenS321 = _M0L10digit__lenS320 + _M0L6_2atmpS1827;
      _M0L6bufferS322
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS321, 0);
      if (_M0L12is__negativeS309) {
        _M0L12digit__startS323 = 1;
      } else {
        _M0L12digit__startS323 = 0;
      }
      moonbit_incref(_M0L6bufferS322);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS322, _M0L3numS310, _M0L12digit__startS323, _M0L10total__lenS321, _M0L5radixS307);
      _M0L6bufferS311 = _M0L6bufferS322;
      break;
    }
  }
  if (_M0L12is__negativeS309) {
    _M0L6bufferS311[0] = 45;
  }
  return _M0L6bufferS311;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS301,
  int32_t _M0L5radixS304
) {
  uint32_t _M0Lm3numS302;
  uint32_t _M0L4baseS303;
  int32_t _M0Lm5countS305;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS301 == 0u) {
    return 1;
  }
  _M0Lm3numS302 = _M0L5valueS301;
  _M0L4baseS303 = *(uint32_t*)&_M0L5radixS304;
  _M0Lm5countS305 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1822 = _M0Lm3numS302;
    if (_M0L6_2atmpS1822 > 0u) {
      int32_t _M0L6_2atmpS1823 = _M0Lm5countS305;
      uint32_t _M0L6_2atmpS1824;
      _M0Lm5countS305 = _M0L6_2atmpS1823 + 1;
      _M0L6_2atmpS1824 = _M0Lm3numS302;
      _M0Lm3numS302 = _M0L6_2atmpS1824 / _M0L4baseS303;
      continue;
    }
    break;
  }
  return _M0Lm5countS305;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS299) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS299 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS300;
    int32_t _M0L6_2atmpS1821;
    int32_t _M0L6_2atmpS1820;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS300 = moonbit_clz32(_M0L5valueS299);
    _M0L6_2atmpS1821 = 31 - _M0L14leading__zerosS300;
    _M0L6_2atmpS1820 = _M0L6_2atmpS1821 / 4;
    return _M0L6_2atmpS1820 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS298) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS298 >= 100000u) {
    if (_M0L5valueS298 >= 10000000u) {
      if (_M0L5valueS298 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS298 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS298 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS298 >= 1000u) {
    if (_M0L5valueS298 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS298 >= 100u) {
    return 3;
  } else if (_M0L5valueS298 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS288,
  uint32_t _M0L3numS276,
  int32_t _M0L12digit__startS279,
  int32_t _M0L10total__lenS278
) {
  uint32_t _M0Lm3numS275;
  int32_t _M0Lm6offsetS277;
  uint32_t _M0L6_2atmpS1819;
  int32_t _M0Lm9remainingS290;
  int32_t _M0L6_2atmpS1800;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS275 = _M0L3numS276;
  _M0Lm6offsetS277 = _M0L10total__lenS278 - _M0L12digit__startS279;
  while (1) {
    uint32_t _M0L6_2atmpS1763 = _M0Lm3numS275;
    if (_M0L6_2atmpS1763 >= 10000u) {
      uint32_t _M0L6_2atmpS1786 = _M0Lm3numS275;
      uint32_t _M0L1tS280 = _M0L6_2atmpS1786 / 10000u;
      uint32_t _M0L6_2atmpS1785 = _M0Lm3numS275;
      uint32_t _M0L6_2atmpS1784 = _M0L6_2atmpS1785 % 10000u;
      int32_t _M0L1rS281 = *(int32_t*)&_M0L6_2atmpS1784;
      int32_t _M0L2d1S282;
      int32_t _M0L2d2S283;
      int32_t _M0L6_2atmpS1764;
      int32_t _M0L6_2atmpS1783;
      int32_t _M0L6_2atmpS1782;
      int32_t _M0L6d1__hiS284;
      int32_t _M0L6_2atmpS1781;
      int32_t _M0L6_2atmpS1780;
      int32_t _M0L6d1__loS285;
      int32_t _M0L6_2atmpS1779;
      int32_t _M0L6_2atmpS1778;
      int32_t _M0L6d2__hiS286;
      int32_t _M0L6_2atmpS1777;
      int32_t _M0L6_2atmpS1776;
      int32_t _M0L6d2__loS287;
      int32_t _M0L6_2atmpS1766;
      int32_t _M0L6_2atmpS1765;
      int32_t _M0L6_2atmpS1769;
      int32_t _M0L6_2atmpS1768;
      int32_t _M0L6_2atmpS1767;
      int32_t _M0L6_2atmpS1772;
      int32_t _M0L6_2atmpS1771;
      int32_t _M0L6_2atmpS1770;
      int32_t _M0L6_2atmpS1775;
      int32_t _M0L6_2atmpS1774;
      int32_t _M0L6_2atmpS1773;
      _M0Lm3numS275 = _M0L1tS280;
      _M0L2d1S282 = _M0L1rS281 / 100;
      _M0L2d2S283 = _M0L1rS281 % 100;
      _M0L6_2atmpS1764 = _M0Lm6offsetS277;
      _M0Lm6offsetS277 = _M0L6_2atmpS1764 - 4;
      _M0L6_2atmpS1783 = _M0L2d1S282 / 10;
      _M0L6_2atmpS1782 = 48 + _M0L6_2atmpS1783;
      _M0L6d1__hiS284 = (uint16_t)_M0L6_2atmpS1782;
      _M0L6_2atmpS1781 = _M0L2d1S282 % 10;
      _M0L6_2atmpS1780 = 48 + _M0L6_2atmpS1781;
      _M0L6d1__loS285 = (uint16_t)_M0L6_2atmpS1780;
      _M0L6_2atmpS1779 = _M0L2d2S283 / 10;
      _M0L6_2atmpS1778 = 48 + _M0L6_2atmpS1779;
      _M0L6d2__hiS286 = (uint16_t)_M0L6_2atmpS1778;
      _M0L6_2atmpS1777 = _M0L2d2S283 % 10;
      _M0L6_2atmpS1776 = 48 + _M0L6_2atmpS1777;
      _M0L6d2__loS287 = (uint16_t)_M0L6_2atmpS1776;
      _M0L6_2atmpS1766 = _M0Lm6offsetS277;
      _M0L6_2atmpS1765 = _M0L12digit__startS279 + _M0L6_2atmpS1766;
      _M0L6bufferS288[_M0L6_2atmpS1765] = _M0L6d1__hiS284;
      _M0L6_2atmpS1769 = _M0Lm6offsetS277;
      _M0L6_2atmpS1768 = _M0L12digit__startS279 + _M0L6_2atmpS1769;
      _M0L6_2atmpS1767 = _M0L6_2atmpS1768 + 1;
      _M0L6bufferS288[_M0L6_2atmpS1767] = _M0L6d1__loS285;
      _M0L6_2atmpS1772 = _M0Lm6offsetS277;
      _M0L6_2atmpS1771 = _M0L12digit__startS279 + _M0L6_2atmpS1772;
      _M0L6_2atmpS1770 = _M0L6_2atmpS1771 + 2;
      _M0L6bufferS288[_M0L6_2atmpS1770] = _M0L6d2__hiS286;
      _M0L6_2atmpS1775 = _M0Lm6offsetS277;
      _M0L6_2atmpS1774 = _M0L12digit__startS279 + _M0L6_2atmpS1775;
      _M0L6_2atmpS1773 = _M0L6_2atmpS1774 + 3;
      _M0L6bufferS288[_M0L6_2atmpS1773] = _M0L6d2__loS287;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1819 = _M0Lm3numS275;
  _M0Lm9remainingS290 = *(int32_t*)&_M0L6_2atmpS1819;
  while (1) {
    int32_t _M0L6_2atmpS1787 = _M0Lm9remainingS290;
    if (_M0L6_2atmpS1787 >= 100) {
      int32_t _M0L6_2atmpS1799 = _M0Lm9remainingS290;
      int32_t _M0L1tS291 = _M0L6_2atmpS1799 / 100;
      int32_t _M0L6_2atmpS1798 = _M0Lm9remainingS290;
      int32_t _M0L1dS292 = _M0L6_2atmpS1798 % 100;
      int32_t _M0L6_2atmpS1788;
      int32_t _M0L6_2atmpS1797;
      int32_t _M0L6_2atmpS1796;
      int32_t _M0L5d__hiS293;
      int32_t _M0L6_2atmpS1795;
      int32_t _M0L6_2atmpS1794;
      int32_t _M0L5d__loS294;
      int32_t _M0L6_2atmpS1790;
      int32_t _M0L6_2atmpS1789;
      int32_t _M0L6_2atmpS1793;
      int32_t _M0L6_2atmpS1792;
      int32_t _M0L6_2atmpS1791;
      _M0Lm9remainingS290 = _M0L1tS291;
      _M0L6_2atmpS1788 = _M0Lm6offsetS277;
      _M0Lm6offsetS277 = _M0L6_2atmpS1788 - 2;
      _M0L6_2atmpS1797 = _M0L1dS292 / 10;
      _M0L6_2atmpS1796 = 48 + _M0L6_2atmpS1797;
      _M0L5d__hiS293 = (uint16_t)_M0L6_2atmpS1796;
      _M0L6_2atmpS1795 = _M0L1dS292 % 10;
      _M0L6_2atmpS1794 = 48 + _M0L6_2atmpS1795;
      _M0L5d__loS294 = (uint16_t)_M0L6_2atmpS1794;
      _M0L6_2atmpS1790 = _M0Lm6offsetS277;
      _M0L6_2atmpS1789 = _M0L12digit__startS279 + _M0L6_2atmpS1790;
      _M0L6bufferS288[_M0L6_2atmpS1789] = _M0L5d__hiS293;
      _M0L6_2atmpS1793 = _M0Lm6offsetS277;
      _M0L6_2atmpS1792 = _M0L12digit__startS279 + _M0L6_2atmpS1793;
      _M0L6_2atmpS1791 = _M0L6_2atmpS1792 + 1;
      _M0L6bufferS288[_M0L6_2atmpS1791] = _M0L5d__loS294;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1800 = _M0Lm9remainingS290;
  if (_M0L6_2atmpS1800 >= 10) {
    int32_t _M0L6_2atmpS1801 = _M0Lm6offsetS277;
    int32_t _M0L6_2atmpS1812;
    int32_t _M0L6_2atmpS1811;
    int32_t _M0L6_2atmpS1810;
    int32_t _M0L5d__hiS296;
    int32_t _M0L6_2atmpS1809;
    int32_t _M0L6_2atmpS1808;
    int32_t _M0L6_2atmpS1807;
    int32_t _M0L5d__loS297;
    int32_t _M0L6_2atmpS1803;
    int32_t _M0L6_2atmpS1802;
    int32_t _M0L6_2atmpS1806;
    int32_t _M0L6_2atmpS1805;
    int32_t _M0L6_2atmpS1804;
    _M0Lm6offsetS277 = _M0L6_2atmpS1801 - 2;
    _M0L6_2atmpS1812 = _M0Lm9remainingS290;
    _M0L6_2atmpS1811 = _M0L6_2atmpS1812 / 10;
    _M0L6_2atmpS1810 = 48 + _M0L6_2atmpS1811;
    _M0L5d__hiS296 = (uint16_t)_M0L6_2atmpS1810;
    _M0L6_2atmpS1809 = _M0Lm9remainingS290;
    _M0L6_2atmpS1808 = _M0L6_2atmpS1809 % 10;
    _M0L6_2atmpS1807 = 48 + _M0L6_2atmpS1808;
    _M0L5d__loS297 = (uint16_t)_M0L6_2atmpS1807;
    _M0L6_2atmpS1803 = _M0Lm6offsetS277;
    _M0L6_2atmpS1802 = _M0L12digit__startS279 + _M0L6_2atmpS1803;
    _M0L6bufferS288[_M0L6_2atmpS1802] = _M0L5d__hiS296;
    _M0L6_2atmpS1806 = _M0Lm6offsetS277;
    _M0L6_2atmpS1805 = _M0L12digit__startS279 + _M0L6_2atmpS1806;
    _M0L6_2atmpS1804 = _M0L6_2atmpS1805 + 1;
    _M0L6bufferS288[_M0L6_2atmpS1804] = _M0L5d__loS297;
    moonbit_decref(_M0L6bufferS288);
  } else {
    int32_t _M0L6_2atmpS1813 = _M0Lm6offsetS277;
    int32_t _M0L6_2atmpS1818;
    int32_t _M0L6_2atmpS1814;
    int32_t _M0L6_2atmpS1817;
    int32_t _M0L6_2atmpS1816;
    int32_t _M0L6_2atmpS1815;
    _M0Lm6offsetS277 = _M0L6_2atmpS1813 - 1;
    _M0L6_2atmpS1818 = _M0Lm6offsetS277;
    _M0L6_2atmpS1814 = _M0L12digit__startS279 + _M0L6_2atmpS1818;
    _M0L6_2atmpS1817 = _M0Lm9remainingS290;
    _M0L6_2atmpS1816 = 48 + _M0L6_2atmpS1817;
    _M0L6_2atmpS1815 = (uint16_t)_M0L6_2atmpS1816;
    _M0L6bufferS288[_M0L6_2atmpS1814] = _M0L6_2atmpS1815;
    moonbit_decref(_M0L6bufferS288);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS270,
  uint32_t _M0L3numS264,
  int32_t _M0L12digit__startS262,
  int32_t _M0L10total__lenS261,
  int32_t _M0L5radixS266
) {
  int32_t _M0Lm6offsetS260;
  uint32_t _M0Lm1nS263;
  uint32_t _M0L4baseS265;
  int32_t _M0L6_2atmpS1745;
  int32_t _M0L6_2atmpS1744;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS260 = _M0L10total__lenS261 - _M0L12digit__startS262;
  _M0Lm1nS263 = _M0L3numS264;
  _M0L4baseS265 = *(uint32_t*)&_M0L5radixS266;
  _M0L6_2atmpS1745 = _M0L5radixS266 - 1;
  _M0L6_2atmpS1744 = _M0L5radixS266 & _M0L6_2atmpS1745;
  if (_M0L6_2atmpS1744 == 0) {
    int32_t _M0L5shiftS267;
    uint32_t _M0L4maskS268;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS267 = moonbit_ctz32(_M0L5radixS266);
    _M0L4maskS268 = _M0L4baseS265 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1746 = _M0Lm1nS263;
      if (_M0L6_2atmpS1746 > 0u) {
        int32_t _M0L6_2atmpS1747 = _M0Lm6offsetS260;
        uint32_t _M0L6_2atmpS1753;
        uint32_t _M0L6_2atmpS1752;
        int32_t _M0L5digitS269;
        int32_t _M0L6_2atmpS1750;
        int32_t _M0L6_2atmpS1748;
        int32_t _M0L6_2atmpS1749;
        uint32_t _M0L6_2atmpS1751;
        _M0Lm6offsetS260 = _M0L6_2atmpS1747 - 1;
        _M0L6_2atmpS1753 = _M0Lm1nS263;
        _M0L6_2atmpS1752 = _M0L6_2atmpS1753 & _M0L4maskS268;
        _M0L5digitS269 = *(int32_t*)&_M0L6_2atmpS1752;
        _M0L6_2atmpS1750 = _M0Lm6offsetS260;
        _M0L6_2atmpS1748 = _M0L12digit__startS262 + _M0L6_2atmpS1750;
        _M0L6_2atmpS1749
        = ((moonbit_string_t)moonbit_string_literal_51.data)[
          _M0L5digitS269
        ];
        _M0L6bufferS270[_M0L6_2atmpS1748] = _M0L6_2atmpS1749;
        _M0L6_2atmpS1751 = _M0Lm1nS263;
        _M0Lm1nS263 = _M0L6_2atmpS1751 >> (_M0L5shiftS267 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS270);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1754 = _M0Lm1nS263;
      if (_M0L6_2atmpS1754 > 0u) {
        int32_t _M0L6_2atmpS1755 = _M0Lm6offsetS260;
        uint32_t _M0L6_2atmpS1762;
        uint32_t _M0L1qS272;
        uint32_t _M0L6_2atmpS1760;
        uint32_t _M0L6_2atmpS1761;
        uint32_t _M0L6_2atmpS1759;
        int32_t _M0L5digitS273;
        int32_t _M0L6_2atmpS1758;
        int32_t _M0L6_2atmpS1756;
        int32_t _M0L6_2atmpS1757;
        _M0Lm6offsetS260 = _M0L6_2atmpS1755 - 1;
        _M0L6_2atmpS1762 = _M0Lm1nS263;
        _M0L1qS272 = _M0L6_2atmpS1762 / _M0L4baseS265;
        _M0L6_2atmpS1760 = _M0Lm1nS263;
        _M0L6_2atmpS1761 = _M0L1qS272 * _M0L4baseS265;
        _M0L6_2atmpS1759 = _M0L6_2atmpS1760 - _M0L6_2atmpS1761;
        _M0L5digitS273 = *(int32_t*)&_M0L6_2atmpS1759;
        _M0L6_2atmpS1758 = _M0Lm6offsetS260;
        _M0L6_2atmpS1756 = _M0L12digit__startS262 + _M0L6_2atmpS1758;
        _M0L6_2atmpS1757
        = ((moonbit_string_t)moonbit_string_literal_51.data)[
          _M0L5digitS273
        ];
        _M0L6bufferS270[_M0L6_2atmpS1756] = _M0L6_2atmpS1757;
        _M0Lm1nS263 = _M0L1qS272;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS270);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS257,
  uint32_t _M0L3numS253,
  int32_t _M0L12digit__startS251,
  int32_t _M0L10total__lenS250
) {
  int32_t _M0Lm6offsetS249;
  uint32_t _M0Lm1nS252;
  int32_t _M0L6_2atmpS1740;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS249 = _M0L10total__lenS250 - _M0L12digit__startS251;
  _M0Lm1nS252 = _M0L3numS253;
  while (1) {
    int32_t _M0L6_2atmpS1728 = _M0Lm6offsetS249;
    if (_M0L6_2atmpS1728 >= 2) {
      int32_t _M0L6_2atmpS1729 = _M0Lm6offsetS249;
      uint32_t _M0L6_2atmpS1739;
      uint32_t _M0L6_2atmpS1738;
      int32_t _M0L9byte__valS254;
      int32_t _M0L2hiS255;
      int32_t _M0L2loS256;
      int32_t _M0L6_2atmpS1732;
      int32_t _M0L6_2atmpS1730;
      int32_t _M0L6_2atmpS1731;
      int32_t _M0L6_2atmpS1736;
      int32_t _M0L6_2atmpS1735;
      int32_t _M0L6_2atmpS1733;
      int32_t _M0L6_2atmpS1734;
      uint32_t _M0L6_2atmpS1737;
      _M0Lm6offsetS249 = _M0L6_2atmpS1729 - 2;
      _M0L6_2atmpS1739 = _M0Lm1nS252;
      _M0L6_2atmpS1738 = _M0L6_2atmpS1739 & 255u;
      _M0L9byte__valS254 = *(int32_t*)&_M0L6_2atmpS1738;
      _M0L2hiS255 = _M0L9byte__valS254 / 16;
      _M0L2loS256 = _M0L9byte__valS254 % 16;
      _M0L6_2atmpS1732 = _M0Lm6offsetS249;
      _M0L6_2atmpS1730 = _M0L12digit__startS251 + _M0L6_2atmpS1732;
      _M0L6_2atmpS1731
      = ((moonbit_string_t)moonbit_string_literal_51.data)[
        _M0L2hiS255
      ];
      _M0L6bufferS257[_M0L6_2atmpS1730] = _M0L6_2atmpS1731;
      _M0L6_2atmpS1736 = _M0Lm6offsetS249;
      _M0L6_2atmpS1735 = _M0L12digit__startS251 + _M0L6_2atmpS1736;
      _M0L6_2atmpS1733 = _M0L6_2atmpS1735 + 1;
      _M0L6_2atmpS1734
      = ((moonbit_string_t)moonbit_string_literal_51.data)[
        _M0L2loS256
      ];
      _M0L6bufferS257[_M0L6_2atmpS1733] = _M0L6_2atmpS1734;
      _M0L6_2atmpS1737 = _M0Lm1nS252;
      _M0Lm1nS252 = _M0L6_2atmpS1737 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1740 = _M0Lm6offsetS249;
  if (_M0L6_2atmpS1740 == 1) {
    uint32_t _M0L6_2atmpS1743 = _M0Lm1nS252;
    uint32_t _M0L6_2atmpS1742 = _M0L6_2atmpS1743 & 15u;
    int32_t _M0L6nibbleS259 = *(int32_t*)&_M0L6_2atmpS1742;
    int32_t _M0L6_2atmpS1741 =
      ((moonbit_string_t)moonbit_string_literal_51.data)[_M0L6nibbleS259];
    _M0L6bufferS257[_M0L12digit__startS251] = _M0L6_2atmpS1741;
    moonbit_decref(_M0L6bufferS257);
  } else {
    moonbit_decref(_M0L6bufferS257);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS242) {
  struct _M0TWEOs* _M0L7_2afuncS241;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS241 = _M0L4selfS242;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS241->code(_M0L7_2afuncS241);
}

int32_t _M0MPB4Iter4nextGcE(struct _M0TWEOc* _M0L4selfS244) {
  struct _M0TWEOc* _M0L7_2afuncS243;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS243 = _M0L4selfS244;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS243->code(_M0L7_2afuncS243);
}

int32_t _M0MPB4Iter4nextGyE(struct _M0TWEOc* _M0L4selfS246) {
  struct _M0TWEOc* _M0L7_2afuncS245;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS245 = _M0L4selfS246;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS245->code(_M0L7_2afuncS245);
}

int64_t _M0MPB4Iter4nextGjE(struct _M0TWEOj* _M0L4selfS248) {
  struct _M0TWEOj* _M0L7_2afuncS247;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS247 = _M0L4selfS248;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS247->code(_M0L7_2afuncS247);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS232
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS231;
  struct _M0TPB6Logger _M0L6_2atmpS1723;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS231 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS231);
  _M0L6_2atmpS1723
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS231
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS232, _M0L6_2atmpS1723);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS231);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS234
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS233;
  struct _M0TPB6Logger _M0L6_2atmpS1724;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS233 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS233);
  _M0L6_2atmpS1724
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS233
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS234, _M0L6_2atmpS1724);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS233);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGbE(
  int32_t _M0L4selfS236
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS235;
  struct _M0TPB6Logger _M0L6_2atmpS1725;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS235 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS235);
  _M0L6_2atmpS1725
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS235
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC14bool4BoolPB4Show6output(_M0L4selfS236, _M0L6_2atmpS1725);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS235);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRP48clawteam8clawteam8internal5errno5ErrnoE(
  void* _M0L4selfS238
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS237;
  struct _M0TPB6Logger _M0L6_2atmpS1726;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS237 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS237);
  _M0L6_2atmpS1726
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS237
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IP48clawteam8clawteam8internal5errno5ErrnoPB4Show6output(_M0L4selfS238, _M0L6_2atmpS1726);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS237);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS240
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS239;
  struct _M0TPB6Logger _M0L6_2atmpS1727;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS239 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS239);
  _M0L6_2atmpS1727
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS239
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS240, _M0L6_2atmpS1727);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS239);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS230
) {
  int32_t _M0L8_2afieldS4336;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4336 = _M0L4selfS230.$1;
  moonbit_decref(_M0L4selfS230.$0);
  return _M0L8_2afieldS4336;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS229
) {
  int32_t _M0L3endS1721;
  int32_t _M0L8_2afieldS4337;
  int32_t _M0L5startS1722;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1721 = _M0L4selfS229.$2;
  _M0L8_2afieldS4337 = _M0L4selfS229.$1;
  moonbit_decref(_M0L4selfS229.$0);
  _M0L5startS1722 = _M0L8_2afieldS4337;
  return _M0L3endS1721 - _M0L5startS1722;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS228
) {
  moonbit_string_t _M0L8_2afieldS4338;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS4338 = _M0L4selfS228.$0;
  return _M0L8_2afieldS4338;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS224,
  moonbit_string_t _M0L5valueS225,
  int32_t _M0L5startS226,
  int32_t _M0L3lenS227
) {
  int32_t _M0L6_2atmpS1720;
  int64_t _M0L6_2atmpS1719;
  struct _M0TPC16string10StringView _M0L6_2atmpS1718;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1720 = _M0L5startS226 + _M0L3lenS227;
  _M0L6_2atmpS1719 = (int64_t)_M0L6_2atmpS1720;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1718
  = _M0MPC16string6String11sub_2einner(_M0L5valueS225, _M0L5startS226, _M0L6_2atmpS1719);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS224, _M0L6_2atmpS1718);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS217,
  int32_t _M0L5startS223,
  int64_t _M0L3endS219
) {
  int32_t _M0L3lenS216;
  int32_t _M0L3endS218;
  int32_t _M0L5startS222;
  int32_t _if__result_4629;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS216 = Moonbit_array_length(_M0L4selfS217);
  if (_M0L3endS219 == 4294967296ll) {
    _M0L3endS218 = _M0L3lenS216;
  } else {
    int64_t _M0L7_2aSomeS220 = _M0L3endS219;
    int32_t _M0L6_2aendS221 = (int32_t)_M0L7_2aSomeS220;
    if (_M0L6_2aendS221 < 0) {
      _M0L3endS218 = _M0L3lenS216 + _M0L6_2aendS221;
    } else {
      _M0L3endS218 = _M0L6_2aendS221;
    }
  }
  if (_M0L5startS223 < 0) {
    _M0L5startS222 = _M0L3lenS216 + _M0L5startS223;
  } else {
    _M0L5startS222 = _M0L5startS223;
  }
  if (_M0L5startS222 >= 0) {
    if (_M0L5startS222 <= _M0L3endS218) {
      _if__result_4629 = _M0L3endS218 <= _M0L3lenS216;
    } else {
      _if__result_4629 = 0;
    }
  } else {
    _if__result_4629 = 0;
  }
  if (_if__result_4629) {
    if (_M0L5startS222 < _M0L3lenS216) {
      int32_t _M0L6_2atmpS1715 = _M0L4selfS217[_M0L5startS222];
      int32_t _M0L6_2atmpS1714;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1714
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1715);
      if (!_M0L6_2atmpS1714) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS218 < _M0L3lenS216) {
      int32_t _M0L6_2atmpS1717 = _M0L4selfS217[_M0L3endS218];
      int32_t _M0L6_2atmpS1716;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1716
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1717);
      if (!_M0L6_2atmpS1716) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS222,
                                                 _M0L3endS218,
                                                 _M0L4selfS217};
  } else {
    moonbit_decref(_M0L4selfS217);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS213) {
  struct _M0TPB6Hasher* _M0L1hS212;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS212 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS212);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS212, _M0L4selfS213);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS212);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS215
) {
  struct _M0TPB6Hasher* _M0L1hS214;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS214 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS214);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS214, _M0L4selfS215);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS214);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS210) {
  int32_t _M0L4seedS209;
  if (_M0L10seed_2eoptS210 == 4294967296ll) {
    _M0L4seedS209 = 0;
  } else {
    int64_t _M0L7_2aSomeS211 = _M0L10seed_2eoptS210;
    _M0L4seedS209 = (int32_t)_M0L7_2aSomeS211;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS209);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS208) {
  uint32_t _M0L6_2atmpS1713;
  uint32_t _M0L6_2atmpS1712;
  struct _M0TPB6Hasher* _block_4630;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1713 = *(uint32_t*)&_M0L4seedS208;
  _M0L6_2atmpS1712 = _M0L6_2atmpS1713 + 374761393u;
  _block_4630
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_4630)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_4630->$0 = _M0L6_2atmpS1712;
  return _block_4630;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS207) {
  uint32_t _M0L6_2atmpS1711;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1711 = _M0MPB6Hasher9avalanche(_M0L4selfS207);
  return *(int32_t*)&_M0L6_2atmpS1711;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS206) {
  uint32_t _M0L8_2afieldS4339;
  uint32_t _M0Lm3accS205;
  uint32_t _M0L6_2atmpS1700;
  uint32_t _M0L6_2atmpS1702;
  uint32_t _M0L6_2atmpS1701;
  uint32_t _M0L6_2atmpS1703;
  uint32_t _M0L6_2atmpS1704;
  uint32_t _M0L6_2atmpS1706;
  uint32_t _M0L6_2atmpS1705;
  uint32_t _M0L6_2atmpS1707;
  uint32_t _M0L6_2atmpS1708;
  uint32_t _M0L6_2atmpS1710;
  uint32_t _M0L6_2atmpS1709;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS4339 = _M0L4selfS206->$0;
  moonbit_decref(_M0L4selfS206);
  _M0Lm3accS205 = _M0L8_2afieldS4339;
  _M0L6_2atmpS1700 = _M0Lm3accS205;
  _M0L6_2atmpS1702 = _M0Lm3accS205;
  _M0L6_2atmpS1701 = _M0L6_2atmpS1702 >> 15;
  _M0Lm3accS205 = _M0L6_2atmpS1700 ^ _M0L6_2atmpS1701;
  _M0L6_2atmpS1703 = _M0Lm3accS205;
  _M0Lm3accS205 = _M0L6_2atmpS1703 * 2246822519u;
  _M0L6_2atmpS1704 = _M0Lm3accS205;
  _M0L6_2atmpS1706 = _M0Lm3accS205;
  _M0L6_2atmpS1705 = _M0L6_2atmpS1706 >> 13;
  _M0Lm3accS205 = _M0L6_2atmpS1704 ^ _M0L6_2atmpS1705;
  _M0L6_2atmpS1707 = _M0Lm3accS205;
  _M0Lm3accS205 = _M0L6_2atmpS1707 * 3266489917u;
  _M0L6_2atmpS1708 = _M0Lm3accS205;
  _M0L6_2atmpS1710 = _M0Lm3accS205;
  _M0L6_2atmpS1709 = _M0L6_2atmpS1710 >> 16;
  _M0Lm3accS205 = _M0L6_2atmpS1708 ^ _M0L6_2atmpS1709;
  return _M0Lm3accS205;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS203,
  moonbit_string_t _M0L1yS204
) {
  int32_t _M0L6_2atmpS4340;
  int32_t _M0L6_2atmpS1699;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS4340 = moonbit_val_array_equal(_M0L1xS203, _M0L1yS204);
  moonbit_decref(_M0L1xS203);
  moonbit_decref(_M0L1yS204);
  _M0L6_2atmpS1699 = _M0L6_2atmpS4340;
  return !_M0L6_2atmpS1699;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS200,
  int32_t _M0L5valueS199
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS199, _M0L4selfS200);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS202,
  moonbit_string_t _M0L5valueS201
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS201, _M0L4selfS202);
  return 0;
}

uint64_t _M0MPC13int3Int10to__uint64(int32_t _M0L4selfS198) {
  int64_t _M0L6_2atmpS1698;
  #line 923 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1698 = (int64_t)_M0L4selfS198;
  return *(uint64_t*)&_M0L6_2atmpS1698;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS196,
  int32_t _M0L5valueS197
) {
  uint32_t _M0L6_2atmpS1697;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1697 = *(uint32_t*)&_M0L5valueS197;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS196, _M0L6_2atmpS1697);
  return 0;
}

int32_t _M0MPC16uint646UInt648to__byte(uint64_t _M0L4selfS195) {
  int32_t _M0L6_2atmpS1696;
  #line 1578 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1696 = (int32_t)_M0L4selfS195;
  return _M0L6_2atmpS1696 & 0xff;
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS193,
  moonbit_string_t _M0L3strS194
) {
  int32_t _M0L3lenS1686;
  int32_t _M0L6_2atmpS1688;
  int32_t _M0L6_2atmpS1687;
  int32_t _M0L6_2atmpS1685;
  moonbit_bytes_t _M0L8_2afieldS4342;
  moonbit_bytes_t _M0L4dataS1689;
  int32_t _M0L3lenS1690;
  int32_t _M0L6_2atmpS1691;
  int32_t _M0L3lenS1693;
  int32_t _M0L6_2atmpS4341;
  int32_t _M0L6_2atmpS1695;
  int32_t _M0L6_2atmpS1694;
  int32_t _M0L6_2atmpS1692;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1686 = _M0L4selfS193->$1;
  _M0L6_2atmpS1688 = Moonbit_array_length(_M0L3strS194);
  _M0L6_2atmpS1687 = _M0L6_2atmpS1688 * 2;
  _M0L6_2atmpS1685 = _M0L3lenS1686 + _M0L6_2atmpS1687;
  moonbit_incref(_M0L4selfS193);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS193, _M0L6_2atmpS1685);
  _M0L8_2afieldS4342 = _M0L4selfS193->$0;
  _M0L4dataS1689 = _M0L8_2afieldS4342;
  _M0L3lenS1690 = _M0L4selfS193->$1;
  _M0L6_2atmpS1691 = Moonbit_array_length(_M0L3strS194);
  moonbit_incref(_M0L4dataS1689);
  moonbit_incref(_M0L3strS194);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1689, _M0L3lenS1690, _M0L3strS194, 0, _M0L6_2atmpS1691);
  _M0L3lenS1693 = _M0L4selfS193->$1;
  _M0L6_2atmpS4341 = Moonbit_array_length(_M0L3strS194);
  moonbit_decref(_M0L3strS194);
  _M0L6_2atmpS1695 = _M0L6_2atmpS4341;
  _M0L6_2atmpS1694 = _M0L6_2atmpS1695 * 2;
  _M0L6_2atmpS1692 = _M0L3lenS1693 + _M0L6_2atmpS1694;
  _M0L4selfS193->$1 = _M0L6_2atmpS1692;
  moonbit_decref(_M0L4selfS193);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS185,
  int32_t _M0L13bytes__offsetS180,
  moonbit_string_t _M0L3strS187,
  int32_t _M0L11str__offsetS183,
  int32_t _M0L6lengthS181
) {
  int32_t _M0L6_2atmpS1684;
  int32_t _M0L6_2atmpS1683;
  int32_t _M0L2e1S179;
  int32_t _M0L6_2atmpS1682;
  int32_t _M0L2e2S182;
  int32_t _M0L4len1S184;
  int32_t _M0L4len2S186;
  int32_t _if__result_4631;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1684 = _M0L6lengthS181 * 2;
  _M0L6_2atmpS1683 = _M0L13bytes__offsetS180 + _M0L6_2atmpS1684;
  _M0L2e1S179 = _M0L6_2atmpS1683 - 1;
  _M0L6_2atmpS1682 = _M0L11str__offsetS183 + _M0L6lengthS181;
  _M0L2e2S182 = _M0L6_2atmpS1682 - 1;
  _M0L4len1S184 = Moonbit_array_length(_M0L4selfS185);
  _M0L4len2S186 = Moonbit_array_length(_M0L3strS187);
  if (_M0L6lengthS181 >= 0) {
    if (_M0L13bytes__offsetS180 >= 0) {
      if (_M0L2e1S179 < _M0L4len1S184) {
        if (_M0L11str__offsetS183 >= 0) {
          _if__result_4631 = _M0L2e2S182 < _M0L4len2S186;
        } else {
          _if__result_4631 = 0;
        }
      } else {
        _if__result_4631 = 0;
      }
    } else {
      _if__result_4631 = 0;
    }
  } else {
    _if__result_4631 = 0;
  }
  if (_if__result_4631) {
    int32_t _M0L16end__str__offsetS188 =
      _M0L11str__offsetS183 + _M0L6lengthS181;
    int32_t _M0L1iS189 = _M0L11str__offsetS183;
    int32_t _M0L1jS190 = _M0L13bytes__offsetS180;
    while (1) {
      if (_M0L1iS189 < _M0L16end__str__offsetS188) {
        int32_t _M0L6_2atmpS1679 = _M0L3strS187[_M0L1iS189];
        int32_t _M0L6_2atmpS1678 = (int32_t)_M0L6_2atmpS1679;
        uint32_t _M0L1cS191 = *(uint32_t*)&_M0L6_2atmpS1678;
        uint32_t _M0L6_2atmpS1674 = _M0L1cS191 & 255u;
        int32_t _M0L6_2atmpS1673;
        int32_t _M0L6_2atmpS1675;
        uint32_t _M0L6_2atmpS1677;
        int32_t _M0L6_2atmpS1676;
        int32_t _M0L6_2atmpS1680;
        int32_t _M0L6_2atmpS1681;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1673 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1674);
        if (
          _M0L1jS190 < 0 || _M0L1jS190 >= Moonbit_array_length(_M0L4selfS185)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS185[_M0L1jS190] = _M0L6_2atmpS1673;
        _M0L6_2atmpS1675 = _M0L1jS190 + 1;
        _M0L6_2atmpS1677 = _M0L1cS191 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1676 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1677);
        if (
          _M0L6_2atmpS1675 < 0
          || _M0L6_2atmpS1675 >= Moonbit_array_length(_M0L4selfS185)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS185[_M0L6_2atmpS1675] = _M0L6_2atmpS1676;
        _M0L6_2atmpS1680 = _M0L1iS189 + 1;
        _M0L6_2atmpS1681 = _M0L1jS190 + 2;
        _M0L1iS189 = _M0L6_2atmpS1680;
        _M0L1jS190 = _M0L6_2atmpS1681;
        continue;
      } else {
        moonbit_decref(_M0L3strS187);
        moonbit_decref(_M0L4selfS185);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS187);
    moonbit_decref(_M0L4selfS185);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS125
) {
  int32_t _M0L6_2atmpS1672;
  struct _M0TPC16string10StringView _M0L7_2abindS124;
  moonbit_string_t _M0L7_2adataS126;
  int32_t _M0L8_2astartS127;
  int32_t _M0L6_2atmpS1671;
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
  int32_t _M0L6_2atmpS1629;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1672 = Moonbit_array_length(_M0L4reprS125);
  _M0L7_2abindS124
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1672, _M0L4reprS125
  };
  moonbit_incref(_M0L7_2abindS124.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS126 = _M0MPC16string10StringView4data(_M0L7_2abindS124);
  moonbit_incref(_M0L7_2abindS124.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS127
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS124);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1671 = _M0MPC16string10StringView6length(_M0L7_2abindS124);
  _M0L6_2aendS128 = _M0L8_2astartS127 + _M0L6_2atmpS1671;
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
  _M0L6_2atmpS1629 = _M0Lm9_2acursorS129;
  if (_M0L6_2atmpS1629 < _M0L6_2aendS128) {
    int32_t _M0L6_2atmpS1631 = _M0Lm9_2acursorS129;
    int32_t _M0L6_2atmpS1630;
    moonbit_incref(_M0L7_2adataS126);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1630
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1631);
    if (_M0L6_2atmpS1630 == 64) {
      int32_t _M0L6_2atmpS1632 = _M0Lm9_2acursorS129;
      _M0Lm9_2acursorS129 = _M0L6_2atmpS1632 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1633;
        _M0Lm6tag__0S137 = _M0Lm9_2acursorS129;
        _M0L6_2atmpS1633 = _M0Lm9_2acursorS129;
        if (_M0L6_2atmpS1633 < _M0L6_2aendS128) {
          int32_t _M0L6_2atmpS1670 = _M0Lm9_2acursorS129;
          int32_t _M0L10next__charS152;
          int32_t _M0L6_2atmpS1634;
          moonbit_incref(_M0L7_2adataS126);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS152
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1670);
          _M0L6_2atmpS1634 = _M0Lm9_2acursorS129;
          _M0Lm9_2acursorS129 = _M0L6_2atmpS1634 + 1;
          if (_M0L10next__charS152 == 58) {
            int32_t _M0L6_2atmpS1635 = _M0Lm9_2acursorS129;
            if (_M0L6_2atmpS1635 < _M0L6_2aendS128) {
              int32_t _M0L6_2atmpS1636 = _M0Lm9_2acursorS129;
              int32_t _M0L12dispatch__15S153;
              _M0Lm9_2acursorS129 = _M0L6_2atmpS1636 + 1;
              _M0L12dispatch__15S153 = 0;
              loop__label__15_156:;
              while (1) {
                int32_t _M0L6_2atmpS1637;
                switch (_M0L12dispatch__15S153) {
                  case 3: {
                    int32_t _M0L6_2atmpS1640;
                    _M0Lm9tag__1__2S140 = _M0Lm9tag__1__1S139;
                    _M0Lm9tag__1__1S139 = _M0Lm6tag__1S138;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1640 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1640 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1645 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS160;
                      int32_t _M0L6_2atmpS1641;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS160
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1645);
                      _M0L6_2atmpS1641 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1641 + 1;
                      if (_M0L10next__charS160 < 58) {
                        if (_M0L10next__charS160 < 48) {
                          goto join_159;
                        } else {
                          int32_t _M0L6_2atmpS1642;
                          _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                          _M0Lm9tag__2__1S143 = _M0Lm6tag__2S142;
                          _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                          _M0Lm6tag__3S141 = _M0Lm9_2acursorS129;
                          _M0L6_2atmpS1642 = _M0Lm9_2acursorS129;
                          if (_M0L6_2atmpS1642 < _M0L6_2aendS128) {
                            int32_t _M0L6_2atmpS1644 = _M0Lm9_2acursorS129;
                            int32_t _M0L10next__charS162;
                            int32_t _M0L6_2atmpS1643;
                            moonbit_incref(_M0L7_2adataS126);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS162
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1644);
                            _M0L6_2atmpS1643 = _M0Lm9_2acursorS129;
                            _M0Lm9_2acursorS129 = _M0L6_2atmpS1643 + 1;
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
                    int32_t _M0L6_2atmpS1646;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1646 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1646 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1648 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS164;
                      int32_t _M0L6_2atmpS1647;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS164
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1648);
                      _M0L6_2atmpS1647 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1647 + 1;
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
                    int32_t _M0L6_2atmpS1649;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1649 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1649 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1651 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS165;
                      int32_t _M0L6_2atmpS1650;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS165
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1651);
                      _M0L6_2atmpS1650 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1650 + 1;
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
                    int32_t _M0L6_2atmpS1652;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__4S144 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1652 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1652 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1660 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS167;
                      int32_t _M0L6_2atmpS1653;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS167
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1660);
                      _M0L6_2atmpS1653 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1653 + 1;
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
                        int32_t _M0L6_2atmpS1654;
                        _M0Lm9tag__1__2S140 = _M0Lm9tag__1__1S139;
                        _M0Lm9tag__1__1S139 = _M0Lm6tag__1S138;
                        _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                        _M0L6_2atmpS1654 = _M0Lm9_2acursorS129;
                        if (_M0L6_2atmpS1654 < _M0L6_2aendS128) {
                          int32_t _M0L6_2atmpS1659 = _M0Lm9_2acursorS129;
                          int32_t _M0L10next__charS169;
                          int32_t _M0L6_2atmpS1655;
                          moonbit_incref(_M0L7_2adataS126);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS169
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1659);
                          _M0L6_2atmpS1655 = _M0Lm9_2acursorS129;
                          _M0Lm9_2acursorS129 = _M0L6_2atmpS1655 + 1;
                          if (_M0L10next__charS169 < 58) {
                            if (_M0L10next__charS169 < 48) {
                              goto join_168;
                            } else {
                              int32_t _M0L6_2atmpS1656;
                              _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                              _M0Lm9tag__2__1S143 = _M0Lm6tag__2S142;
                              _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                              _M0L6_2atmpS1656 = _M0Lm9_2acursorS129;
                              if (_M0L6_2atmpS1656 < _M0L6_2aendS128) {
                                int32_t _M0L6_2atmpS1658 =
                                  _M0Lm9_2acursorS129;
                                int32_t _M0L10next__charS171;
                                int32_t _M0L6_2atmpS1657;
                                moonbit_incref(_M0L7_2adataS126);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS171
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1658);
                                _M0L6_2atmpS1657 = _M0Lm9_2acursorS129;
                                _M0Lm9_2acursorS129 = _M0L6_2atmpS1657 + 1;
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
                    int32_t _M0L6_2atmpS1661;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1661 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1661 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1663 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS173;
                      int32_t _M0L6_2atmpS1662;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS173
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1663);
                      _M0L6_2atmpS1662 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1662 + 1;
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
                    int32_t _M0L6_2atmpS1664;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__2S142 = _M0Lm9_2acursorS129;
                    _M0Lm6tag__3S141 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1664 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1664 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1666 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS175;
                      int32_t _M0L6_2atmpS1665;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS175
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1666);
                      _M0L6_2atmpS1665 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1665 + 1;
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
                    int32_t _M0L6_2atmpS1667;
                    _M0Lm9tag__1__1S139 = _M0Lm6tag__1S138;
                    _M0Lm6tag__1S138 = _M0Lm9_2acursorS129;
                    _M0L6_2atmpS1667 = _M0Lm9_2acursorS129;
                    if (_M0L6_2atmpS1667 < _M0L6_2aendS128) {
                      int32_t _M0L6_2atmpS1669 = _M0Lm9_2acursorS129;
                      int32_t _M0L10next__charS177;
                      int32_t _M0L6_2atmpS1668;
                      moonbit_incref(_M0L7_2adataS126);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS177
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1669);
                      _M0L6_2atmpS1668 = _M0Lm9_2acursorS129;
                      _M0Lm9_2acursorS129 = _M0L6_2atmpS1668 + 1;
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
                _M0L6_2atmpS1637 = _M0Lm9_2acursorS129;
                if (_M0L6_2atmpS1637 < _M0L6_2aendS128) {
                  int32_t _M0L6_2atmpS1639 = _M0Lm9_2acursorS129;
                  int32_t _M0L10next__charS157;
                  int32_t _M0L6_2atmpS1638;
                  moonbit_incref(_M0L7_2adataS126);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS157
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS126, _M0L6_2atmpS1639);
                  _M0L6_2atmpS1638 = _M0Lm9_2acursorS129;
                  _M0Lm9_2acursorS129 = _M0L6_2atmpS1638 + 1;
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
      int32_t _M0L6_2atmpS1628 = _M0Lm20match__tag__saver__1S133;
      int32_t _M0L6_2atmpS1627 = _M0L6_2atmpS1628 + 1;
      int64_t _M0L6_2atmpS1624 = (int64_t)_M0L6_2atmpS1627;
      int32_t _M0L6_2atmpS1626 = _M0Lm20match__tag__saver__2S134;
      int64_t _M0L6_2atmpS1625 = (int64_t)_M0L6_2atmpS1626;
      struct _M0TPC16string10StringView _M0L11start__lineS146;
      int32_t _M0L6_2atmpS1623;
      int32_t _M0L6_2atmpS1622;
      int64_t _M0L6_2atmpS1619;
      int32_t _M0L6_2atmpS1621;
      int64_t _M0L6_2atmpS1620;
      struct _M0TPC16string10StringView _M0L13start__columnS147;
      int32_t _M0L6_2atmpS1618;
      int64_t _M0L6_2atmpS1615;
      int32_t _M0L6_2atmpS1617;
      int64_t _M0L6_2atmpS1616;
      struct _M0TPC16string10StringView _M0L3pkgS148;
      int32_t _M0L6_2atmpS1614;
      int32_t _M0L6_2atmpS1613;
      int64_t _M0L6_2atmpS1610;
      int32_t _M0L6_2atmpS1612;
      int64_t _M0L6_2atmpS1611;
      struct _M0TPC16string10StringView _M0L8filenameS149;
      int32_t _M0L6_2atmpS1609;
      int32_t _M0L6_2atmpS1608;
      int64_t _M0L6_2atmpS1605;
      int32_t _M0L6_2atmpS1607;
      int64_t _M0L6_2atmpS1606;
      struct _M0TPC16string10StringView _M0L9end__lineS150;
      int32_t _M0L6_2atmpS1604;
      int32_t _M0L6_2atmpS1603;
      int64_t _M0L6_2atmpS1600;
      int32_t _M0L6_2atmpS1602;
      int64_t _M0L6_2atmpS1601;
      struct _M0TPC16string10StringView _M0L11end__columnS151;
      struct _M0TPB13SourceLocRepr* _block_4648;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS146
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1624, _M0L6_2atmpS1625);
      _M0L6_2atmpS1623 = _M0Lm20match__tag__saver__2S134;
      _M0L6_2atmpS1622 = _M0L6_2atmpS1623 + 1;
      _M0L6_2atmpS1619 = (int64_t)_M0L6_2atmpS1622;
      _M0L6_2atmpS1621 = _M0Lm20match__tag__saver__3S135;
      _M0L6_2atmpS1620 = (int64_t)_M0L6_2atmpS1621;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS147
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1619, _M0L6_2atmpS1620);
      _M0L6_2atmpS1618 = _M0L8_2astartS127 + 1;
      _M0L6_2atmpS1615 = (int64_t)_M0L6_2atmpS1618;
      _M0L6_2atmpS1617 = _M0Lm20match__tag__saver__0S132;
      _M0L6_2atmpS1616 = (int64_t)_M0L6_2atmpS1617;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS148
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1615, _M0L6_2atmpS1616);
      _M0L6_2atmpS1614 = _M0Lm20match__tag__saver__0S132;
      _M0L6_2atmpS1613 = _M0L6_2atmpS1614 + 1;
      _M0L6_2atmpS1610 = (int64_t)_M0L6_2atmpS1613;
      _M0L6_2atmpS1612 = _M0Lm20match__tag__saver__1S133;
      _M0L6_2atmpS1611 = (int64_t)_M0L6_2atmpS1612;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS149
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1610, _M0L6_2atmpS1611);
      _M0L6_2atmpS1609 = _M0Lm20match__tag__saver__3S135;
      _M0L6_2atmpS1608 = _M0L6_2atmpS1609 + 1;
      _M0L6_2atmpS1605 = (int64_t)_M0L6_2atmpS1608;
      _M0L6_2atmpS1607 = _M0Lm20match__tag__saver__4S136;
      _M0L6_2atmpS1606 = (int64_t)_M0L6_2atmpS1607;
      moonbit_incref(_M0L7_2adataS126);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS150
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1605, _M0L6_2atmpS1606);
      _M0L6_2atmpS1604 = _M0Lm20match__tag__saver__4S136;
      _M0L6_2atmpS1603 = _M0L6_2atmpS1604 + 1;
      _M0L6_2atmpS1600 = (int64_t)_M0L6_2atmpS1603;
      _M0L6_2atmpS1602 = _M0Lm10match__endS131;
      _M0L6_2atmpS1601 = (int64_t)_M0L6_2atmpS1602;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS151
      = _M0MPC16string6String4view(_M0L7_2adataS126, _M0L6_2atmpS1600, _M0L6_2atmpS1601);
      _block_4648
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_4648)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_4648->$0_0 = _M0L3pkgS148.$0;
      _block_4648->$0_1 = _M0L3pkgS148.$1;
      _block_4648->$0_2 = _M0L3pkgS148.$2;
      _block_4648->$1_0 = _M0L8filenameS149.$0;
      _block_4648->$1_1 = _M0L8filenameS149.$1;
      _block_4648->$1_2 = _M0L8filenameS149.$2;
      _block_4648->$2_0 = _M0L11start__lineS146.$0;
      _block_4648->$2_1 = _M0L11start__lineS146.$1;
      _block_4648->$2_2 = _M0L11start__lineS146.$2;
      _block_4648->$3_0 = _M0L13start__columnS147.$0;
      _block_4648->$3_1 = _M0L13start__columnS147.$1;
      _block_4648->$3_2 = _M0L13start__columnS147.$2;
      _block_4648->$4_0 = _M0L9end__lineS150.$0;
      _block_4648->$4_1 = _M0L9end__lineS150.$1;
      _block_4648->$4_2 = _M0L9end__lineS150.$2;
      _block_4648->$5_0 = _M0L11end__columnS151.$0;
      _block_4648->$5_1 = _M0L11end__columnS151.$1;
      _block_4648->$5_2 = _M0L11end__columnS151.$2;
      return _block_4648;
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
  int32_t _if__result_4649;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS121 = _M0L4selfS122->$1;
  if (_M0L5indexS123 >= 0) {
    _if__result_4649 = _M0L5indexS123 < _M0L3lenS121;
  } else {
    _if__result_4649 = 0;
  }
  if (_if__result_4649) {
    moonbit_string_t* _M0L6_2atmpS1599;
    moonbit_string_t _M0L6_2atmpS4343;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1599 = _M0MPC15array5Array6bufferGsE(_M0L4selfS122);
    if (
      _M0L5indexS123 < 0
      || _M0L5indexS123 >= Moonbit_array_length(_M0L6_2atmpS1599)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS4343 = (moonbit_string_t)_M0L6_2atmpS1599[_M0L5indexS123];
    moonbit_incref(_M0L6_2atmpS4343);
    moonbit_decref(_M0L6_2atmpS1599);
    return _M0L6_2atmpS4343;
  } else {
    moonbit_decref(_M0L4selfS122);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS119
) {
  moonbit_string_t* _M0L8_2afieldS4344;
  int32_t _M0L6_2acntS4469;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4344 = _M0L4selfS119->$0;
  _M0L6_2acntS4469 = Moonbit_object_header(_M0L4selfS119)->rc;
  if (_M0L6_2acntS4469 > 1) {
    int32_t _M0L11_2anew__cntS4470 = _M0L6_2acntS4469 - 1;
    Moonbit_object_header(_M0L4selfS119)->rc = _M0L11_2anew__cntS4470;
    moonbit_incref(_M0L8_2afieldS4344);
  } else if (_M0L6_2acntS4469 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS119);
  }
  return _M0L8_2afieldS4344;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS120
) {
  struct _M0TUsiE** _M0L8_2afieldS4345;
  int32_t _M0L6_2acntS4471;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS4345 = _M0L4selfS120->$0;
  _M0L6_2acntS4471 = Moonbit_object_header(_M0L4selfS120)->rc;
  if (_M0L6_2acntS4471 > 1) {
    int32_t _M0L11_2anew__cntS4472 = _M0L6_2acntS4471 - 1;
    Moonbit_object_header(_M0L4selfS120)->rc = _M0L11_2anew__cntS4472;
    moonbit_incref(_M0L8_2afieldS4345);
  } else if (_M0L6_2acntS4471 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS120);
  }
  return _M0L8_2afieldS4345;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS118) {
  struct _M0TPB13StringBuilder* _M0L3bufS117;
  struct _M0TPB6Logger _M0L6_2atmpS1598;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS117 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS117);
  _M0L6_2atmpS1598
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS117
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS118, _M0L6_2atmpS1598);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS117);
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS116) {
  int32_t _M0L6_2atmpS1597;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1597 = (int32_t)_M0L4selfS116;
  return _M0L6_2atmpS1597;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS114,
  int32_t _M0L8trailingS115
) {
  int32_t _M0L6_2atmpS1596;
  int32_t _M0L6_2atmpS1595;
  int32_t _M0L6_2atmpS1594;
  int32_t _M0L6_2atmpS1593;
  int32_t _M0L6_2atmpS1592;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1596 = _M0L7leadingS114 - 55296;
  _M0L6_2atmpS1595 = _M0L6_2atmpS1596 * 1024;
  _M0L6_2atmpS1594 = _M0L6_2atmpS1595 + _M0L8trailingS115;
  _M0L6_2atmpS1593 = _M0L6_2atmpS1594 - 56320;
  _M0L6_2atmpS1592 = _M0L6_2atmpS1593 + 65536;
  return _M0L6_2atmpS1592;
}

int32_t _M0MPC16uint166UInt1623is__trailing__surrogate(int32_t _M0L4selfS113) {
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS113 >= 56320) {
    return _M0L4selfS113 <= 57343;
  } else {
    return 0;
  }
}

int32_t _M0MPC16uint166UInt1622is__leading__surrogate(int32_t _M0L4selfS112) {
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  if (_M0L4selfS112 >= 55296) {
    return _M0L4selfS112 <= 56319;
  } else {
    return 0;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__char(
  struct _M0TPB13StringBuilder* _M0L4selfS109,
  int32_t _M0L2chS111
) {
  int32_t _M0L3lenS1587;
  int32_t _M0L6_2atmpS1586;
  moonbit_bytes_t _M0L8_2afieldS4346;
  moonbit_bytes_t _M0L4dataS1590;
  int32_t _M0L3lenS1591;
  int32_t _M0L3incS110;
  int32_t _M0L3lenS1589;
  int32_t _M0L6_2atmpS1588;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1587 = _M0L4selfS109->$1;
  _M0L6_2atmpS1586 = _M0L3lenS1587 + 4;
  moonbit_incref(_M0L4selfS109);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS109, _M0L6_2atmpS1586);
  _M0L8_2afieldS4346 = _M0L4selfS109->$0;
  _M0L4dataS1590 = _M0L8_2afieldS4346;
  _M0L3lenS1591 = _M0L4selfS109->$1;
  moonbit_incref(_M0L4dataS1590);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS110
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1590, _M0L3lenS1591, _M0L2chS111);
  _M0L3lenS1589 = _M0L4selfS109->$1;
  _M0L6_2atmpS1588 = _M0L3lenS1589 + _M0L3incS110;
  _M0L4selfS109->$1 = _M0L6_2atmpS1588;
  moonbit_decref(_M0L4selfS109);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS104,
  int32_t _M0L8requiredS105
) {
  moonbit_bytes_t _M0L8_2afieldS4350;
  moonbit_bytes_t _M0L4dataS1585;
  int32_t _M0L6_2atmpS4349;
  int32_t _M0L12current__lenS103;
  int32_t _M0Lm13enough__spaceS106;
  int32_t _M0L6_2atmpS1583;
  int32_t _M0L6_2atmpS1584;
  moonbit_bytes_t _M0L9new__dataS108;
  moonbit_bytes_t _M0L8_2afieldS4348;
  moonbit_bytes_t _M0L4dataS1581;
  int32_t _M0L3lenS1582;
  moonbit_bytes_t _M0L6_2aoldS4347;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4350 = _M0L4selfS104->$0;
  _M0L4dataS1585 = _M0L8_2afieldS4350;
  _M0L6_2atmpS4349 = Moonbit_array_length(_M0L4dataS1585);
  _M0L12current__lenS103 = _M0L6_2atmpS4349;
  if (_M0L8requiredS105 <= _M0L12current__lenS103) {
    moonbit_decref(_M0L4selfS104);
    return 0;
  }
  _M0Lm13enough__spaceS106 = _M0L12current__lenS103;
  while (1) {
    int32_t _M0L6_2atmpS1579 = _M0Lm13enough__spaceS106;
    if (_M0L6_2atmpS1579 < _M0L8requiredS105) {
      int32_t _M0L6_2atmpS1580 = _M0Lm13enough__spaceS106;
      _M0Lm13enough__spaceS106 = _M0L6_2atmpS1580 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1583 = _M0Lm13enough__spaceS106;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1584 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS108
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1583, _M0L6_2atmpS1584);
  _M0L8_2afieldS4348 = _M0L4selfS104->$0;
  _M0L4dataS1581 = _M0L8_2afieldS4348;
  _M0L3lenS1582 = _M0L4selfS104->$1;
  moonbit_incref(_M0L4dataS1581);
  moonbit_incref(_M0L9new__dataS108);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS108, 0, _M0L4dataS1581, 0, _M0L3lenS1582);
  _M0L6_2aoldS4347 = _M0L4selfS104->$0;
  moonbit_decref(_M0L6_2aoldS4347);
  _M0L4selfS104->$0 = _M0L9new__dataS108;
  moonbit_decref(_M0L4selfS104);
  return 0;
}

int32_t _M0IPC14byte4BytePB7Default7default() {
  #line 231 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  return 0;
}

int32_t _M0MPC15array10FixedArray18set__utf16le__char(
  moonbit_bytes_t _M0L4selfS98,
  int32_t _M0L6offsetS99,
  int32_t _M0L5valueS97
) {
  uint32_t _M0L4codeS96;
  #line 278 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  #line 283 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L4codeS96 = _M0MPC14char4Char8to__uint(_M0L5valueS97);
  if (_M0L4codeS96 < 65536u) {
    uint32_t _M0L6_2atmpS1562 = _M0L4codeS96 & 255u;
    int32_t _M0L6_2atmpS1561;
    int32_t _M0L6_2atmpS1563;
    uint32_t _M0L6_2atmpS1565;
    int32_t _M0L6_2atmpS1564;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1561 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1562);
    if (
      _M0L6offsetS99 < 0
      || _M0L6offsetS99 >= Moonbit_array_length(_M0L4selfS98)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS98[_M0L6offsetS99] = _M0L6_2atmpS1561;
    _M0L6_2atmpS1563 = _M0L6offsetS99 + 1;
    _M0L6_2atmpS1565 = _M0L4codeS96 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1564 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1565);
    if (
      _M0L6_2atmpS1563 < 0
      || _M0L6_2atmpS1563 >= Moonbit_array_length(_M0L4selfS98)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS98[_M0L6_2atmpS1563] = _M0L6_2atmpS1564;
    moonbit_decref(_M0L4selfS98);
    return 2;
  } else if (_M0L4codeS96 < 1114112u) {
    uint32_t _M0L2hiS100 = _M0L4codeS96 - 65536u;
    uint32_t _M0L6_2atmpS1578 = _M0L2hiS100 >> 10;
    uint32_t _M0L2loS101 = _M0L6_2atmpS1578 | 55296u;
    uint32_t _M0L6_2atmpS1577 = _M0L2hiS100 & 1023u;
    uint32_t _M0L2hiS102 = _M0L6_2atmpS1577 | 56320u;
    uint32_t _M0L6_2atmpS1567 = _M0L2loS101 & 255u;
    int32_t _M0L6_2atmpS1566;
    int32_t _M0L6_2atmpS1568;
    uint32_t _M0L6_2atmpS1570;
    int32_t _M0L6_2atmpS1569;
    int32_t _M0L6_2atmpS1571;
    uint32_t _M0L6_2atmpS1573;
    int32_t _M0L6_2atmpS1572;
    int32_t _M0L6_2atmpS1574;
    uint32_t _M0L6_2atmpS1576;
    int32_t _M0L6_2atmpS1575;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1566 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1567);
    if (
      _M0L6offsetS99 < 0
      || _M0L6offsetS99 >= Moonbit_array_length(_M0L4selfS98)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS98[_M0L6offsetS99] = _M0L6_2atmpS1566;
    _M0L6_2atmpS1568 = _M0L6offsetS99 + 1;
    _M0L6_2atmpS1570 = _M0L2loS101 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1569 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1570);
    if (
      _M0L6_2atmpS1568 < 0
      || _M0L6_2atmpS1568 >= Moonbit_array_length(_M0L4selfS98)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS98[_M0L6_2atmpS1568] = _M0L6_2atmpS1569;
    _M0L6_2atmpS1571 = _M0L6offsetS99 + 2;
    _M0L6_2atmpS1573 = _M0L2hiS102 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1572 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1573);
    if (
      _M0L6_2atmpS1571 < 0
      || _M0L6_2atmpS1571 >= Moonbit_array_length(_M0L4selfS98)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS98[_M0L6_2atmpS1571] = _M0L6_2atmpS1572;
    _M0L6_2atmpS1574 = _M0L6offsetS99 + 3;
    _M0L6_2atmpS1576 = _M0L2hiS102 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1575 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1576);
    if (
      _M0L6_2atmpS1574 < 0
      || _M0L6_2atmpS1574 >= Moonbit_array_length(_M0L4selfS98)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS98[_M0L6_2atmpS1574] = _M0L6_2atmpS1575;
    moonbit_decref(_M0L4selfS98);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS98);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_18.data, (moonbit_string_t)moonbit_string_literal_52.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS95) {
  int32_t _M0L6_2atmpS1560;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1560 = *(int32_t*)&_M0L4selfS95;
  return _M0L6_2atmpS1560 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS94) {
  int32_t _M0L6_2atmpS1559;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1559 = _M0L4selfS94;
  return *(uint32_t*)&_M0L6_2atmpS1559;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS93
) {
  moonbit_bytes_t _M0L8_2afieldS4352;
  moonbit_bytes_t _M0L4dataS1558;
  moonbit_bytes_t _M0L6_2atmpS1555;
  int32_t _M0L8_2afieldS4351;
  int32_t _M0L3lenS1557;
  int64_t _M0L6_2atmpS1556;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS4352 = _M0L4selfS93->$0;
  _M0L4dataS1558 = _M0L8_2afieldS4352;
  moonbit_incref(_M0L4dataS1558);
  _M0L6_2atmpS1555 = _M0L4dataS1558;
  _M0L8_2afieldS4351 = _M0L4selfS93->$1;
  moonbit_decref(_M0L4selfS93);
  _M0L3lenS1557 = _M0L8_2afieldS4351;
  _M0L6_2atmpS1556 = (int64_t)_M0L3lenS1557;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1555, 0, _M0L6_2atmpS1556);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS88,
  int32_t _M0L6offsetS92,
  int64_t _M0L6lengthS90
) {
  int32_t _M0L3lenS87;
  int32_t _M0L6lengthS89;
  int32_t _if__result_4651;
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L3lenS87 = Moonbit_array_length(_M0L4selfS88);
  if (_M0L6lengthS90 == 4294967296ll) {
    _M0L6lengthS89 = _M0L3lenS87 - _M0L6offsetS92;
  } else {
    int64_t _M0L7_2aSomeS91 = _M0L6lengthS90;
    _M0L6lengthS89 = (int32_t)_M0L7_2aSomeS91;
  }
  if (_M0L6offsetS92 >= 0) {
    if (_M0L6lengthS89 >= 0) {
      int32_t _M0L6_2atmpS1554 = _M0L6offsetS92 + _M0L6lengthS89;
      _if__result_4651 = _M0L6_2atmpS1554 <= _M0L3lenS87;
    } else {
      _if__result_4651 = 0;
    }
  } else {
    _if__result_4651 = 0;
  }
  if (_if__result_4651) {
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB19unsafe__sub__string(_M0L4selfS88, _M0L6offsetS92, _M0L6lengthS89);
  } else {
    moonbit_decref(_M0L4selfS88);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
}

struct _M0TPB13StringBuilder* _M0MPB13StringBuilder11new_2einner(
  int32_t _M0L10size__hintS85
) {
  int32_t _M0L7initialS84;
  moonbit_bytes_t _M0L4dataS86;
  struct _M0TPB13StringBuilder* _block_4652;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS85 < 1) {
    _M0L7initialS84 = 1;
  } else {
    _M0L7initialS84 = _M0L10size__hintS85;
  }
  _M0L4dataS86 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS84, 0);
  _block_4652
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_4652)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_4652->$0 = _M0L4dataS86;
  _block_4652->$1 = 0;
  return _block_4652;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS83) {
  int32_t _M0L6_2atmpS1553;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1553 = (int32_t)_M0L4selfS83;
  return _M0L6_2atmpS1553;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGsE(
  moonbit_string_t* _M0L3dstS73,
  int32_t _M0L11dst__offsetS74,
  moonbit_string_t* _M0L3srcS75,
  int32_t _M0L11src__offsetS76,
  int32_t _M0L3lenS77
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(_M0L3dstS73, _M0L11dst__offsetS74, _M0L3srcS75, _M0L11src__offsetS76, _M0L3lenS77);
  return 0;
}

int32_t _M0MPB18UninitializedArray12unsafe__blitGUsiEE(
  struct _M0TUsiE** _M0L3dstS78,
  int32_t _M0L11dst__offsetS79,
  struct _M0TUsiE** _M0L3srcS80,
  int32_t _M0L11src__offsetS81,
  int32_t _M0L3lenS82
) {
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uninitialized_array.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(_M0L3dstS78, _M0L11dst__offsetS79, _M0L3srcS80, _M0L11src__offsetS81, _M0L3lenS82);
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGyE(
  moonbit_bytes_t _M0L3dstS37,
  int32_t _M0L11dst__offsetS39,
  moonbit_bytes_t _M0L3srcS38,
  int32_t _M0L11src__offsetS40,
  int32_t _M0L3lenS42
) {
  int32_t _if__result_4653;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS37 == _M0L3srcS38) {
    _if__result_4653 = _M0L11dst__offsetS39 < _M0L11src__offsetS40;
  } else {
    _if__result_4653 = 0;
  }
  if (_if__result_4653) {
    int32_t _M0L1iS41 = 0;
    while (1) {
      if (_M0L1iS41 < _M0L3lenS42) {
        int32_t _M0L6_2atmpS1517 = _M0L11dst__offsetS39 + _M0L1iS41;
        int32_t _M0L6_2atmpS1519 = _M0L11src__offsetS40 + _M0L1iS41;
        int32_t _M0L6_2atmpS1518;
        int32_t _M0L6_2atmpS1520;
        if (
          _M0L6_2atmpS1519 < 0
          || _M0L6_2atmpS1519 >= Moonbit_array_length(_M0L3srcS38)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1518 = (int32_t)_M0L3srcS38[_M0L6_2atmpS1519];
        if (
          _M0L6_2atmpS1517 < 0
          || _M0L6_2atmpS1517 >= Moonbit_array_length(_M0L3dstS37)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS37[_M0L6_2atmpS1517] = _M0L6_2atmpS1518;
        _M0L6_2atmpS1520 = _M0L1iS41 + 1;
        _M0L1iS41 = _M0L6_2atmpS1520;
        continue;
      } else {
        moonbit_decref(_M0L3srcS38);
        moonbit_decref(_M0L3dstS37);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1525 = _M0L3lenS42 - 1;
    int32_t _M0L1iS44 = _M0L6_2atmpS1525;
    while (1) {
      if (_M0L1iS44 >= 0) {
        int32_t _M0L6_2atmpS1521 = _M0L11dst__offsetS39 + _M0L1iS44;
        int32_t _M0L6_2atmpS1523 = _M0L11src__offsetS40 + _M0L1iS44;
        int32_t _M0L6_2atmpS1522;
        int32_t _M0L6_2atmpS1524;
        if (
          _M0L6_2atmpS1523 < 0
          || _M0L6_2atmpS1523 >= Moonbit_array_length(_M0L3srcS38)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1522 = (int32_t)_M0L3srcS38[_M0L6_2atmpS1523];
        if (
          _M0L6_2atmpS1521 < 0
          || _M0L6_2atmpS1521 >= Moonbit_array_length(_M0L3dstS37)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS37[_M0L6_2atmpS1521] = _M0L6_2atmpS1522;
        _M0L6_2atmpS1524 = _M0L1iS44 - 1;
        _M0L1iS44 = _M0L6_2atmpS1524;
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

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGsEE(
  moonbit_string_t* _M0L3dstS46,
  int32_t _M0L11dst__offsetS48,
  moonbit_string_t* _M0L3srcS47,
  int32_t _M0L11src__offsetS49,
  int32_t _M0L3lenS51
) {
  int32_t _if__result_4656;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS46 == _M0L3srcS47) {
    _if__result_4656 = _M0L11dst__offsetS48 < _M0L11src__offsetS49;
  } else {
    _if__result_4656 = 0;
  }
  if (_if__result_4656) {
    int32_t _M0L1iS50 = 0;
    while (1) {
      if (_M0L1iS50 < _M0L3lenS51) {
        int32_t _M0L6_2atmpS1526 = _M0L11dst__offsetS48 + _M0L1iS50;
        int32_t _M0L6_2atmpS1528 = _M0L11src__offsetS49 + _M0L1iS50;
        moonbit_string_t _M0L6_2atmpS4354;
        moonbit_string_t _M0L6_2atmpS1527;
        moonbit_string_t _M0L6_2aoldS4353;
        int32_t _M0L6_2atmpS1529;
        if (
          _M0L6_2atmpS1528 < 0
          || _M0L6_2atmpS1528 >= Moonbit_array_length(_M0L3srcS47)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4354 = (moonbit_string_t)_M0L3srcS47[_M0L6_2atmpS1528];
        _M0L6_2atmpS1527 = _M0L6_2atmpS4354;
        if (
          _M0L6_2atmpS1526 < 0
          || _M0L6_2atmpS1526 >= Moonbit_array_length(_M0L3dstS46)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4353 = (moonbit_string_t)_M0L3dstS46[_M0L6_2atmpS1526];
        moonbit_incref(_M0L6_2atmpS1527);
        moonbit_decref(_M0L6_2aoldS4353);
        _M0L3dstS46[_M0L6_2atmpS1526] = _M0L6_2atmpS1527;
        _M0L6_2atmpS1529 = _M0L1iS50 + 1;
        _M0L1iS50 = _M0L6_2atmpS1529;
        continue;
      } else {
        moonbit_decref(_M0L3srcS47);
        moonbit_decref(_M0L3dstS46);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1534 = _M0L3lenS51 - 1;
    int32_t _M0L1iS53 = _M0L6_2atmpS1534;
    while (1) {
      if (_M0L1iS53 >= 0) {
        int32_t _M0L6_2atmpS1530 = _M0L11dst__offsetS48 + _M0L1iS53;
        int32_t _M0L6_2atmpS1532 = _M0L11src__offsetS49 + _M0L1iS53;
        moonbit_string_t _M0L6_2atmpS4356;
        moonbit_string_t _M0L6_2atmpS1531;
        moonbit_string_t _M0L6_2aoldS4355;
        int32_t _M0L6_2atmpS1533;
        if (
          _M0L6_2atmpS1532 < 0
          || _M0L6_2atmpS1532 >= Moonbit_array_length(_M0L3srcS47)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4356 = (moonbit_string_t)_M0L3srcS47[_M0L6_2atmpS1532];
        _M0L6_2atmpS1531 = _M0L6_2atmpS4356;
        if (
          _M0L6_2atmpS1530 < 0
          || _M0L6_2atmpS1530 >= Moonbit_array_length(_M0L3dstS46)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4355 = (moonbit_string_t)_M0L3dstS46[_M0L6_2atmpS1530];
        moonbit_incref(_M0L6_2atmpS1531);
        moonbit_decref(_M0L6_2aoldS4355);
        _M0L3dstS46[_M0L6_2atmpS1530] = _M0L6_2atmpS1531;
        _M0L6_2atmpS1533 = _M0L1iS53 - 1;
        _M0L1iS53 = _M0L6_2atmpS1533;
        continue;
      } else {
        moonbit_decref(_M0L3srcS47);
        moonbit_decref(_M0L3dstS46);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGRPB17UnsafeMaybeUninitGUsiEEE(
  struct _M0TUsiE** _M0L3dstS55,
  int32_t _M0L11dst__offsetS57,
  struct _M0TUsiE** _M0L3srcS56,
  int32_t _M0L11src__offsetS58,
  int32_t _M0L3lenS60
) {
  int32_t _if__result_4659;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS55 == _M0L3srcS56) {
    _if__result_4659 = _M0L11dst__offsetS57 < _M0L11src__offsetS58;
  } else {
    _if__result_4659 = 0;
  }
  if (_if__result_4659) {
    int32_t _M0L1iS59 = 0;
    while (1) {
      if (_M0L1iS59 < _M0L3lenS60) {
        int32_t _M0L6_2atmpS1535 = _M0L11dst__offsetS57 + _M0L1iS59;
        int32_t _M0L6_2atmpS1537 = _M0L11src__offsetS58 + _M0L1iS59;
        struct _M0TUsiE* _M0L6_2atmpS4358;
        struct _M0TUsiE* _M0L6_2atmpS1536;
        struct _M0TUsiE* _M0L6_2aoldS4357;
        int32_t _M0L6_2atmpS1538;
        if (
          _M0L6_2atmpS1537 < 0
          || _M0L6_2atmpS1537 >= Moonbit_array_length(_M0L3srcS56)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4358 = (struct _M0TUsiE*)_M0L3srcS56[_M0L6_2atmpS1537];
        _M0L6_2atmpS1536 = _M0L6_2atmpS4358;
        if (
          _M0L6_2atmpS1535 < 0
          || _M0L6_2atmpS1535 >= Moonbit_array_length(_M0L3dstS55)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4357 = (struct _M0TUsiE*)_M0L3dstS55[_M0L6_2atmpS1535];
        if (_M0L6_2atmpS1536) {
          moonbit_incref(_M0L6_2atmpS1536);
        }
        if (_M0L6_2aoldS4357) {
          moonbit_decref(_M0L6_2aoldS4357);
        }
        _M0L3dstS55[_M0L6_2atmpS1535] = _M0L6_2atmpS1536;
        _M0L6_2atmpS1538 = _M0L1iS59 + 1;
        _M0L1iS59 = _M0L6_2atmpS1538;
        continue;
      } else {
        moonbit_decref(_M0L3srcS56);
        moonbit_decref(_M0L3dstS55);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1543 = _M0L3lenS60 - 1;
    int32_t _M0L1iS62 = _M0L6_2atmpS1543;
    while (1) {
      if (_M0L1iS62 >= 0) {
        int32_t _M0L6_2atmpS1539 = _M0L11dst__offsetS57 + _M0L1iS62;
        int32_t _M0L6_2atmpS1541 = _M0L11src__offsetS58 + _M0L1iS62;
        struct _M0TUsiE* _M0L6_2atmpS4360;
        struct _M0TUsiE* _M0L6_2atmpS1540;
        struct _M0TUsiE* _M0L6_2aoldS4359;
        int32_t _M0L6_2atmpS1542;
        if (
          _M0L6_2atmpS1541 < 0
          || _M0L6_2atmpS1541 >= Moonbit_array_length(_M0L3srcS56)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS4360 = (struct _M0TUsiE*)_M0L3srcS56[_M0L6_2atmpS1541];
        _M0L6_2atmpS1540 = _M0L6_2atmpS4360;
        if (
          _M0L6_2atmpS1539 < 0
          || _M0L6_2atmpS1539 >= Moonbit_array_length(_M0L3dstS55)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS4359 = (struct _M0TUsiE*)_M0L3dstS55[_M0L6_2atmpS1539];
        if (_M0L6_2atmpS1540) {
          moonbit_incref(_M0L6_2atmpS1540);
        }
        if (_M0L6_2aoldS4359) {
          moonbit_decref(_M0L6_2aoldS4359);
        }
        _M0L3dstS55[_M0L6_2atmpS1539] = _M0L6_2atmpS1540;
        _M0L6_2atmpS1542 = _M0L1iS62 - 1;
        _M0L1iS62 = _M0L6_2atmpS1542;
        continue;
      } else {
        moonbit_decref(_M0L3srcS56);
        moonbit_decref(_M0L3dstS55);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0MPC15array10FixedArray12unsafe__blitGjE(
  uint32_t* _M0L3dstS64,
  int32_t _M0L11dst__offsetS66,
  uint32_t* _M0L3srcS65,
  int32_t _M0L11src__offsetS67,
  int32_t _M0L3lenS69
) {
  int32_t _if__result_4662;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS64 == _M0L3srcS65) {
    _if__result_4662 = _M0L11dst__offsetS66 < _M0L11src__offsetS67;
  } else {
    _if__result_4662 = 0;
  }
  if (_if__result_4662) {
    int32_t _M0L1iS68 = 0;
    while (1) {
      if (_M0L1iS68 < _M0L3lenS69) {
        int32_t _M0L6_2atmpS1544 = _M0L11dst__offsetS66 + _M0L1iS68;
        int32_t _M0L6_2atmpS1546 = _M0L11src__offsetS67 + _M0L1iS68;
        uint32_t _M0L6_2atmpS1545;
        int32_t _M0L6_2atmpS1547;
        if (
          _M0L6_2atmpS1546 < 0
          || _M0L6_2atmpS1546 >= Moonbit_array_length(_M0L3srcS65)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1545 = (uint32_t)_M0L3srcS65[_M0L6_2atmpS1546];
        if (
          _M0L6_2atmpS1544 < 0
          || _M0L6_2atmpS1544 >= Moonbit_array_length(_M0L3dstS64)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS64[_M0L6_2atmpS1544] = _M0L6_2atmpS1545;
        _M0L6_2atmpS1547 = _M0L1iS68 + 1;
        _M0L1iS68 = _M0L6_2atmpS1547;
        continue;
      } else {
        moonbit_decref(_M0L3srcS65);
        moonbit_decref(_M0L3dstS64);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1552 = _M0L3lenS69 - 1;
    int32_t _M0L1iS71 = _M0L6_2atmpS1552;
    while (1) {
      if (_M0L1iS71 >= 0) {
        int32_t _M0L6_2atmpS1548 = _M0L11dst__offsetS66 + _M0L1iS71;
        int32_t _M0L6_2atmpS1550 = _M0L11src__offsetS67 + _M0L1iS71;
        uint32_t _M0L6_2atmpS1549;
        int32_t _M0L6_2atmpS1551;
        if (
          _M0L6_2atmpS1550 < 0
          || _M0L6_2atmpS1550 >= Moonbit_array_length(_M0L3srcS65)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1549 = (uint32_t)_M0L3srcS65[_M0L6_2atmpS1550];
        if (
          _M0L6_2atmpS1548 < 0
          || _M0L6_2atmpS1548 >= Moonbit_array_length(_M0L3dstS64)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS64[_M0L6_2atmpS1548] = _M0L6_2atmpS1549;
        _M0L6_2atmpS1551 = _M0L1iS71 - 1;
        _M0L1iS71 = _M0L6_2atmpS1551;
        continue;
      } else {
        moonbit_decref(_M0L3srcS65);
        moonbit_decref(_M0L3dstS64);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB5abortGiE(
  moonbit_string_t _M0L6stringS21,
  moonbit_string_t _M0L3locS22
) {
  moonbit_string_t _M0L6_2atmpS1481;
  moonbit_string_t _M0L6_2atmpS4363;
  moonbit_string_t _M0L6_2atmpS1479;
  moonbit_string_t _M0L6_2atmpS1480;
  moonbit_string_t _M0L6_2atmpS4362;
  moonbit_string_t _M0L6_2atmpS1478;
  moonbit_string_t _M0L6_2atmpS4361;
  moonbit_string_t _M0L6_2atmpS1477;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1481 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4363
  = moonbit_add_string(_M0L6_2atmpS1481, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1481);
  _M0L6_2atmpS1479 = _M0L6_2atmpS4363;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1480
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS22);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4362 = moonbit_add_string(_M0L6_2atmpS1479, _M0L6_2atmpS1480);
  moonbit_decref(_M0L6_2atmpS1479);
  moonbit_decref(_M0L6_2atmpS1480);
  _M0L6_2atmpS1478 = _M0L6_2atmpS4362;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4361
  = moonbit_add_string(_M0L6_2atmpS1478, (moonbit_string_t)moonbit_string_literal_54.data);
  moonbit_decref(_M0L6_2atmpS1478);
  _M0L6_2atmpS1477 = _M0L6_2atmpS4361;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1477);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS23,
  moonbit_string_t _M0L3locS24
) {
  moonbit_string_t _M0L6_2atmpS1486;
  moonbit_string_t _M0L6_2atmpS4366;
  moonbit_string_t _M0L6_2atmpS1484;
  moonbit_string_t _M0L6_2atmpS1485;
  moonbit_string_t _M0L6_2atmpS4365;
  moonbit_string_t _M0L6_2atmpS1483;
  moonbit_string_t _M0L6_2atmpS4364;
  moonbit_string_t _M0L6_2atmpS1482;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1486 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS23);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4366
  = moonbit_add_string(_M0L6_2atmpS1486, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1486);
  _M0L6_2atmpS1484 = _M0L6_2atmpS4366;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1485
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS24);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4365 = moonbit_add_string(_M0L6_2atmpS1484, _M0L6_2atmpS1485);
  moonbit_decref(_M0L6_2atmpS1484);
  moonbit_decref(_M0L6_2atmpS1485);
  _M0L6_2atmpS1483 = _M0L6_2atmpS4365;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4364
  = moonbit_add_string(_M0L6_2atmpS1483, (moonbit_string_t)moonbit_string_literal_54.data);
  moonbit_decref(_M0L6_2atmpS1483);
  _M0L6_2atmpS1482 = _M0L6_2atmpS4364;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1482);
  return 0;
}

struct _M0TPC16random6Source _M0FPB5abortGRPC16random4RandE(
  moonbit_string_t _M0L6stringS25,
  moonbit_string_t _M0L3locS26
) {
  moonbit_string_t _M0L6_2atmpS1491;
  moonbit_string_t _M0L6_2atmpS4369;
  moonbit_string_t _M0L6_2atmpS1489;
  moonbit_string_t _M0L6_2atmpS1490;
  moonbit_string_t _M0L6_2atmpS4368;
  moonbit_string_t _M0L6_2atmpS1488;
  moonbit_string_t _M0L6_2atmpS4367;
  moonbit_string_t _M0L6_2atmpS1487;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1491 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS25);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4369
  = moonbit_add_string(_M0L6_2atmpS1491, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1491);
  _M0L6_2atmpS1489 = _M0L6_2atmpS4369;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1490
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS26);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4368 = moonbit_add_string(_M0L6_2atmpS1489, _M0L6_2atmpS1490);
  moonbit_decref(_M0L6_2atmpS1489);
  moonbit_decref(_M0L6_2atmpS1490);
  _M0L6_2atmpS1488 = _M0L6_2atmpS4368;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4367
  = moonbit_add_string(_M0L6_2atmpS1488, (moonbit_string_t)moonbit_string_literal_54.data);
  moonbit_decref(_M0L6_2atmpS1488);
  _M0L6_2atmpS1487 = _M0L6_2atmpS4367;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16random4RandE(_M0L6_2atmpS1487);
}

struct _M0TPC15bytes9BytesView _M0FPB5abortGRPC15bytes9BytesViewE(
  moonbit_string_t _M0L6stringS27,
  moonbit_string_t _M0L3locS28
) {
  moonbit_string_t _M0L6_2atmpS1496;
  moonbit_string_t _M0L6_2atmpS4372;
  moonbit_string_t _M0L6_2atmpS1494;
  moonbit_string_t _M0L6_2atmpS1495;
  moonbit_string_t _M0L6_2atmpS4371;
  moonbit_string_t _M0L6_2atmpS1493;
  moonbit_string_t _M0L6_2atmpS4370;
  moonbit_string_t _M0L6_2atmpS1492;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1496 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS27);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4372
  = moonbit_add_string(_M0L6_2atmpS1496, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1496);
  _M0L6_2atmpS1494 = _M0L6_2atmpS4372;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1495
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS28);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4371 = moonbit_add_string(_M0L6_2atmpS1494, _M0L6_2atmpS1495);
  moonbit_decref(_M0L6_2atmpS1494);
  moonbit_decref(_M0L6_2atmpS1495);
  _M0L6_2atmpS1493 = _M0L6_2atmpS4371;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4370
  = moonbit_add_string(_M0L6_2atmpS1493, (moonbit_string_t)moonbit_string_literal_54.data);
  moonbit_decref(_M0L6_2atmpS1493);
  _M0L6_2atmpS1492 = _M0L6_2atmpS4370;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC15bytes9BytesViewE(_M0L6_2atmpS1492);
}

int32_t _M0FPB5abortGyE(
  moonbit_string_t _M0L6stringS29,
  moonbit_string_t _M0L3locS30
) {
  moonbit_string_t _M0L6_2atmpS1501;
  moonbit_string_t _M0L6_2atmpS4375;
  moonbit_string_t _M0L6_2atmpS1499;
  moonbit_string_t _M0L6_2atmpS1500;
  moonbit_string_t _M0L6_2atmpS4374;
  moonbit_string_t _M0L6_2atmpS1498;
  moonbit_string_t _M0L6_2atmpS4373;
  moonbit_string_t _M0L6_2atmpS1497;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1501 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS29);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4375
  = moonbit_add_string(_M0L6_2atmpS1501, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1501);
  _M0L6_2atmpS1499 = _M0L6_2atmpS4375;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1500
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS30);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4374 = moonbit_add_string(_M0L6_2atmpS1499, _M0L6_2atmpS1500);
  moonbit_decref(_M0L6_2atmpS1499);
  moonbit_decref(_M0L6_2atmpS1500);
  _M0L6_2atmpS1498 = _M0L6_2atmpS4374;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4373
  = moonbit_add_string(_M0L6_2atmpS1498, (moonbit_string_t)moonbit_string_literal_54.data);
  moonbit_decref(_M0L6_2atmpS1498);
  _M0L6_2atmpS1497 = _M0L6_2atmpS4373;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGyE(_M0L6_2atmpS1497);
}

struct _M0TPB9ArrayViewGyE _M0FPB5abortGRPB9ArrayViewGyEE(
  moonbit_string_t _M0L6stringS31,
  moonbit_string_t _M0L3locS32
) {
  moonbit_string_t _M0L6_2atmpS1506;
  moonbit_string_t _M0L6_2atmpS4378;
  moonbit_string_t _M0L6_2atmpS1504;
  moonbit_string_t _M0L6_2atmpS1505;
  moonbit_string_t _M0L6_2atmpS4377;
  moonbit_string_t _M0L6_2atmpS1503;
  moonbit_string_t _M0L6_2atmpS4376;
  moonbit_string_t _M0L6_2atmpS1502;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1506 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS31);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4378
  = moonbit_add_string(_M0L6_2atmpS1506, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1506);
  _M0L6_2atmpS1504 = _M0L6_2atmpS4378;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1505
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS32);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4377 = moonbit_add_string(_M0L6_2atmpS1504, _M0L6_2atmpS1505);
  moonbit_decref(_M0L6_2atmpS1504);
  moonbit_decref(_M0L6_2atmpS1505);
  _M0L6_2atmpS1503 = _M0L6_2atmpS4377;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4376
  = moonbit_add_string(_M0L6_2atmpS1503, (moonbit_string_t)moonbit_string_literal_54.data);
  moonbit_decref(_M0L6_2atmpS1503);
  _M0L6_2atmpS1502 = _M0L6_2atmpS4376;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPB9ArrayViewGyEE(_M0L6_2atmpS1502);
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS33,
  moonbit_string_t _M0L3locS34
) {
  moonbit_string_t _M0L6_2atmpS1511;
  moonbit_string_t _M0L6_2atmpS4381;
  moonbit_string_t _M0L6_2atmpS1509;
  moonbit_string_t _M0L6_2atmpS1510;
  moonbit_string_t _M0L6_2atmpS4380;
  moonbit_string_t _M0L6_2atmpS1508;
  moonbit_string_t _M0L6_2atmpS4379;
  moonbit_string_t _M0L6_2atmpS1507;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1511 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS33);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4381
  = moonbit_add_string(_M0L6_2atmpS1511, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1511);
  _M0L6_2atmpS1509 = _M0L6_2atmpS4381;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1510
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS34);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4380 = moonbit_add_string(_M0L6_2atmpS1509, _M0L6_2atmpS1510);
  moonbit_decref(_M0L6_2atmpS1509);
  moonbit_decref(_M0L6_2atmpS1510);
  _M0L6_2atmpS1508 = _M0L6_2atmpS4380;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4379
  = moonbit_add_string(_M0L6_2atmpS1508, (moonbit_string_t)moonbit_string_literal_54.data);
  moonbit_decref(_M0L6_2atmpS1508);
  _M0L6_2atmpS1507 = _M0L6_2atmpS4379;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1507);
}

uint32_t _M0FPB5abortGjE(
  moonbit_string_t _M0L6stringS35,
  moonbit_string_t _M0L3locS36
) {
  moonbit_string_t _M0L6_2atmpS1516;
  moonbit_string_t _M0L6_2atmpS4384;
  moonbit_string_t _M0L6_2atmpS1514;
  moonbit_string_t _M0L6_2atmpS1515;
  moonbit_string_t _M0L6_2atmpS4383;
  moonbit_string_t _M0L6_2atmpS1513;
  moonbit_string_t _M0L6_2atmpS4382;
  moonbit_string_t _M0L6_2atmpS1512;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1516 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS35);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4384
  = moonbit_add_string(_M0L6_2atmpS1516, (moonbit_string_t)moonbit_string_literal_53.data);
  moonbit_decref(_M0L6_2atmpS1516);
  _M0L6_2atmpS1514 = _M0L6_2atmpS4384;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1515
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS36);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4383 = moonbit_add_string(_M0L6_2atmpS1514, _M0L6_2atmpS1515);
  moonbit_decref(_M0L6_2atmpS1514);
  moonbit_decref(_M0L6_2atmpS1515);
  _M0L6_2atmpS1513 = _M0L6_2atmpS4383;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS4382
  = moonbit_add_string(_M0L6_2atmpS1513, (moonbit_string_t)moonbit_string_literal_54.data);
  moonbit_decref(_M0L6_2atmpS1513);
  _M0L6_2atmpS1512 = _M0L6_2atmpS4382;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGjE(_M0L6_2atmpS1512);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS19,
  uint32_t _M0L5valueS20
) {
  uint32_t _M0L3accS1476;
  uint32_t _M0L6_2atmpS1475;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1476 = _M0L4selfS19->$0;
  _M0L6_2atmpS1475 = _M0L3accS1476 + 4u;
  _M0L4selfS19->$0 = _M0L6_2atmpS1475;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS19, _M0L5valueS20);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS17,
  uint32_t _M0L5inputS18
) {
  uint32_t _M0L3accS1473;
  uint32_t _M0L6_2atmpS1474;
  uint32_t _M0L6_2atmpS1472;
  uint32_t _M0L6_2atmpS1471;
  uint32_t _M0L6_2atmpS1470;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1473 = _M0L4selfS17->$0;
  _M0L6_2atmpS1474 = _M0L5inputS18 * 3266489917u;
  _M0L6_2atmpS1472 = _M0L3accS1473 + _M0L6_2atmpS1474;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1471 = _M0FPB4rotl(_M0L6_2atmpS1472, 17);
  _M0L6_2atmpS1470 = _M0L6_2atmpS1471 * 668265263u;
  _M0L4selfS17->$0 = _M0L6_2atmpS1470;
  moonbit_decref(_M0L4selfS17);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS15, int32_t _M0L1rS16) {
  uint32_t _M0L6_2atmpS1467;
  int32_t _M0L6_2atmpS1469;
  uint32_t _M0L6_2atmpS1468;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1467 = _M0L1xS15 << (_M0L1rS16 & 31);
  _M0L6_2atmpS1469 = 32 - _M0L1rS16;
  _M0L6_2atmpS1468 = _M0L1xS15 >> (_M0L6_2atmpS1469 & 31);
  return _M0L6_2atmpS1467 | _M0L6_2atmpS1468;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S11,
  struct _M0TPB6Logger _M0L10_2ax__4934S14
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS12;
  moonbit_string_t _M0L8_2afieldS4385;
  int32_t _M0L6_2acntS4473;
  moonbit_string_t _M0L15_2a_2aarg__4935S13;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS12
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S11;
  _M0L8_2afieldS4385 = _M0L10_2aFailureS12->$0;
  _M0L6_2acntS4473 = Moonbit_object_header(_M0L10_2aFailureS12)->rc;
  if (_M0L6_2acntS4473 > 1) {
    int32_t _M0L11_2anew__cntS4474 = _M0L6_2acntS4473 - 1;
    Moonbit_object_header(_M0L10_2aFailureS12)->rc = _M0L11_2anew__cntS4474;
    moonbit_incref(_M0L8_2afieldS4385);
  } else if (_M0L6_2acntS4473 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS12);
  }
  _M0L15_2a_2aarg__4935S13 = _M0L8_2afieldS4385;
  if (_M0L10_2ax__4934S14.$1) {
    moonbit_incref(_M0L10_2ax__4934S14.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S14.$0->$method_0(_M0L10_2ax__4934S14.$1, (moonbit_string_t)moonbit_string_literal_55.data);
  if (_M0L10_2ax__4934S14.$1) {
    moonbit_incref(_M0L10_2ax__4934S14.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S14, _M0L15_2a_2aarg__4935S13);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S14.$0->$method_0(_M0L10_2ax__4934S14.$1, (moonbit_string_t)moonbit_string_literal_56.data);
  return 0;
}

int32_t _M0MPB6Logger13write__objectGsE(
  struct _M0TPB6Logger _M0L4selfS10,
  moonbit_string_t _M0L3objS9
) {
  #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L3objS9, _M0L4selfS10);
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

struct _M0TPC16random6Source _M0FPC15abort5abortGRPC16random4RandE(
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

int32_t _M0FPC15abort5abortGyE(moonbit_string_t _M0L3msgS5) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS5);
  moonbit_decref(_M0L3msgS5);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

struct _M0TPB9ArrayViewGyE _M0FPC15abort5abortGRPB9ArrayViewGyEE(
  moonbit_string_t _M0L3msgS6
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS6);
  moonbit_decref(_M0L3msgS6);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

struct _M0TPC16string10StringView _M0FPC15abort5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L3msgS7
) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS7);
  moonbit_decref(_M0L3msgS7);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

uint32_t _M0FPC15abort5abortGjE(moonbit_string_t _M0L3msgS8) {
  #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_println(_M0L3msgS8);
  moonbit_decref(_M0L3msgS8);
  #line 50 "C:\\Users\\Administrator\\.moon\\lib\\core\\abort\\abort.mbt"
  moonbit_panic();
}

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1399) {
  switch (Moonbit_object_tag(_M0L4_2aeS1399)) {
    case 5: {
      moonbit_decref(_M0L4_2aeS1399);
      return (moonbit_string_t)moonbit_string_literal_57.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1399);
      return (moonbit_string_t)moonbit_string_literal_58.data;
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
    
    case 3: {
      moonbit_decref(_M0L4_2aeS1399);
      return (moonbit_string_t)moonbit_string_literal_59.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1399);
      return (moonbit_string_t)moonbit_string_literal_60.data;
      break;
    }
  }
}

uint64_t _M0IPC36random8internal14random__source7ChaCha8PC16random6Source59next_2edyncall__as___40moonbitlang_2fcore_2frandom_2eSource(
  void* _M0L11_2aobj__ptrS1424
) {
  struct _M0TPC36random8internal14random__source7ChaCha8* _M0L7_2aselfS1423 =
    (struct _M0TPC36random8internal14random__source7ChaCha8*)_M0L11_2aobj__ptrS1424;
  return _M0IPC36random8internal14random__source7ChaCha8PC16random6Source4next(_M0L7_2aselfS1423);
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1422,
  int32_t _M0L8_2aparamS1421
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1420 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1422;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1420, _M0L8_2aparamS1421);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1419,
  struct _M0TPC16string10StringView _M0L8_2aparamS1418
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1417 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1419;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1417, _M0L8_2aparamS1418);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1416,
  moonbit_string_t _M0L8_2aparamS1413,
  int32_t _M0L8_2aparamS1414,
  int32_t _M0L8_2aparamS1415
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1412 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1416;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1412, _M0L8_2aparamS1413, _M0L8_2aparamS1414, _M0L8_2aparamS1415);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1411,
  moonbit_string_t _M0L8_2aparamS1410
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1409 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1411;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1409, _M0L8_2aparamS1410);
  return 0;
}

void moonbit_init() {
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1319;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1466;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1465;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1464;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1432;
  moonbit_string_t* _M0L6_2atmpS1463;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1462;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1461;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1320;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1460;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1459;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1458;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1433;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1321;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1457;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1456;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1455;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1434;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1322;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1454;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1453;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1452;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1435;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1323;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1451;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1450;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1449;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1436;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1324;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1448;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1447;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1446;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1437;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1325;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1445;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1444;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1443;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1438;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1326;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1442;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1441;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1440;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1439;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1318;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1431;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1430;
  _M0FP311moonbitlang1x6crypto9sha256__t
  = (uint32_t*)moonbit_make_int32_array_raw(64);
  _M0FP311moonbitlang1x6crypto9sha256__t[0] = 1116352408u;
  _M0FP311moonbitlang1x6crypto9sha256__t[1] = 1899447441u;
  _M0FP311moonbitlang1x6crypto9sha256__t[2] = 3049323471u;
  _M0FP311moonbitlang1x6crypto9sha256__t[3] = 3921009573u;
  _M0FP311moonbitlang1x6crypto9sha256__t[4] = 961987163u;
  _M0FP311moonbitlang1x6crypto9sha256__t[5] = 1508970993u;
  _M0FP311moonbitlang1x6crypto9sha256__t[6] = 2453635748u;
  _M0FP311moonbitlang1x6crypto9sha256__t[7] = 2870763221u;
  _M0FP311moonbitlang1x6crypto9sha256__t[8] = 3624381080u;
  _M0FP311moonbitlang1x6crypto9sha256__t[9] = 310598401u;
  _M0FP311moonbitlang1x6crypto9sha256__t[10] = 607225278u;
  _M0FP311moonbitlang1x6crypto9sha256__t[11] = 1426881987u;
  _M0FP311moonbitlang1x6crypto9sha256__t[12] = 1925078388u;
  _M0FP311moonbitlang1x6crypto9sha256__t[13] = 2162078206u;
  _M0FP311moonbitlang1x6crypto9sha256__t[14] = 2614888103u;
  _M0FP311moonbitlang1x6crypto9sha256__t[15] = 3248222580u;
  _M0FP311moonbitlang1x6crypto9sha256__t[16] = 3835390401u;
  _M0FP311moonbitlang1x6crypto9sha256__t[17] = 4022224774u;
  _M0FP311moonbitlang1x6crypto9sha256__t[18] = 264347078u;
  _M0FP311moonbitlang1x6crypto9sha256__t[19] = 604807628u;
  _M0FP311moonbitlang1x6crypto9sha256__t[20] = 770255983u;
  _M0FP311moonbitlang1x6crypto9sha256__t[21] = 1249150122u;
  _M0FP311moonbitlang1x6crypto9sha256__t[22] = 1555081692u;
  _M0FP311moonbitlang1x6crypto9sha256__t[23] = 1996064986u;
  _M0FP311moonbitlang1x6crypto9sha256__t[24] = 2554220882u;
  _M0FP311moonbitlang1x6crypto9sha256__t[25] = 2821834349u;
  _M0FP311moonbitlang1x6crypto9sha256__t[26] = 2952996808u;
  _M0FP311moonbitlang1x6crypto9sha256__t[27] = 3210313671u;
  _M0FP311moonbitlang1x6crypto9sha256__t[28] = 3336571891u;
  _M0FP311moonbitlang1x6crypto9sha256__t[29] = 3584528711u;
  _M0FP311moonbitlang1x6crypto9sha256__t[30] = 113926993u;
  _M0FP311moonbitlang1x6crypto9sha256__t[31] = 338241895u;
  _M0FP311moonbitlang1x6crypto9sha256__t[32] = 666307205u;
  _M0FP311moonbitlang1x6crypto9sha256__t[33] = 773529912u;
  _M0FP311moonbitlang1x6crypto9sha256__t[34] = 1294757372u;
  _M0FP311moonbitlang1x6crypto9sha256__t[35] = 1396182291u;
  _M0FP311moonbitlang1x6crypto9sha256__t[36] = 1695183700u;
  _M0FP311moonbitlang1x6crypto9sha256__t[37] = 1986661051u;
  _M0FP311moonbitlang1x6crypto9sha256__t[38] = 2177026350u;
  _M0FP311moonbitlang1x6crypto9sha256__t[39] = 2456956037u;
  _M0FP311moonbitlang1x6crypto9sha256__t[40] = 2730485921u;
  _M0FP311moonbitlang1x6crypto9sha256__t[41] = 2820302411u;
  _M0FP311moonbitlang1x6crypto9sha256__t[42] = 3259730800u;
  _M0FP311moonbitlang1x6crypto9sha256__t[43] = 3345764771u;
  _M0FP311moonbitlang1x6crypto9sha256__t[44] = 3516065817u;
  _M0FP311moonbitlang1x6crypto9sha256__t[45] = 3600352804u;
  _M0FP311moonbitlang1x6crypto9sha256__t[46] = 4094571909u;
  _M0FP311moonbitlang1x6crypto9sha256__t[47] = 275423344u;
  _M0FP311moonbitlang1x6crypto9sha256__t[48] = 430227734u;
  _M0FP311moonbitlang1x6crypto9sha256__t[49] = 506948616u;
  _M0FP311moonbitlang1x6crypto9sha256__t[50] = 659060556u;
  _M0FP311moonbitlang1x6crypto9sha256__t[51] = 883997877u;
  _M0FP311moonbitlang1x6crypto9sha256__t[52] = 958139571u;
  _M0FP311moonbitlang1x6crypto9sha256__t[53] = 1322822218u;
  _M0FP311moonbitlang1x6crypto9sha256__t[54] = 1537002063u;
  _M0FP311moonbitlang1x6crypto9sha256__t[55] = 1747873779u;
  _M0FP311moonbitlang1x6crypto9sha256__t[56] = 1955562222u;
  _M0FP311moonbitlang1x6crypto9sha256__t[57] = 2024104815u;
  _M0FP311moonbitlang1x6crypto9sha256__t[58] = 2227730452u;
  _M0FP311moonbitlang1x6crypto9sha256__t[59] = 2361852424u;
  _M0FP311moonbitlang1x6crypto9sha256__t[60] = 2428436474u;
  _M0FP311moonbitlang1x6crypto9sha256__t[61] = 2756734187u;
  _M0FP311moonbitlang1x6crypto9sha256__t[62] = 3204031479u;
  _M0FP311moonbitlang1x6crypto9sha256__t[63] = 3329325298u;
  _M0L7_2abindS1319
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1466 = _M0L7_2abindS1319;
  _M0L6_2atmpS1465
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1466
  };
  #line 398 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1464
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1465);
  _M0L8_2atupleS1432
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1432)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1432->$0 = (moonbit_string_t)moonbit_string_literal_61.data;
  _M0L8_2atupleS1432->$1 = _M0L6_2atmpS1464;
  _M0L6_2atmpS1463 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1463[0] = (moonbit_string_t)moonbit_string_literal_62.data;
  moonbit_incref(_M0FP48clawteam8clawteam5oauth5codex35____test__706b63652e6d6274__0_2eclo);
  _M0L8_2atupleS1462
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1462)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1462->$0
  = _M0FP48clawteam8clawteam5oauth5codex35____test__706b63652e6d6274__0_2eclo;
  _M0L8_2atupleS1462->$1 = _M0L6_2atmpS1463;
  _M0L8_2atupleS1461
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1461)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1461->$0 = 0;
  _M0L8_2atupleS1461->$1 = _M0L8_2atupleS1462;
  _M0L7_2abindS1320
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(1);
  _M0L7_2abindS1320[0] = _M0L8_2atupleS1461;
  _M0L6_2atmpS1460 = _M0L7_2abindS1320;
  _M0L6_2atmpS1459
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 1, _M0L6_2atmpS1460
  };
  #line 400 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1458
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1459);
  _M0L8_2atupleS1433
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1433)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1433->$0 = (moonbit_string_t)moonbit_string_literal_63.data;
  _M0L8_2atupleS1433->$1 = _M0L6_2atmpS1458;
  _M0L7_2abindS1321
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1457 = _M0L7_2abindS1321;
  _M0L6_2atmpS1456
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1457
  };
  #line 403 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1455
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1456);
  _M0L8_2atupleS1434
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1434)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1434->$0 = (moonbit_string_t)moonbit_string_literal_64.data;
  _M0L8_2atupleS1434->$1 = _M0L6_2atmpS1455;
  _M0L7_2abindS1322
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1454 = _M0L7_2abindS1322;
  _M0L6_2atmpS1453
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1454
  };
  #line 405 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1452
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1453);
  _M0L8_2atupleS1435
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1435)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1435->$0 = (moonbit_string_t)moonbit_string_literal_65.data;
  _M0L8_2atupleS1435->$1 = _M0L6_2atmpS1452;
  _M0L7_2abindS1323
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1451 = _M0L7_2abindS1323;
  _M0L6_2atmpS1450
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1451
  };
  #line 407 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1449
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1450);
  _M0L8_2atupleS1436
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1436)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1436->$0 = (moonbit_string_t)moonbit_string_literal_66.data;
  _M0L8_2atupleS1436->$1 = _M0L6_2atmpS1449;
  _M0L7_2abindS1324
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1448 = _M0L7_2abindS1324;
  _M0L6_2atmpS1447
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1448
  };
  #line 409 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1446
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1447);
  _M0L8_2atupleS1437
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1437)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1437->$0 = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L8_2atupleS1437->$1 = _M0L6_2atmpS1446;
  _M0L7_2abindS1325
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1445 = _M0L7_2abindS1325;
  _M0L6_2atmpS1444
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1445
  };
  #line 411 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1443
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1444);
  _M0L8_2atupleS1438
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1438)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1438->$0 = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L8_2atupleS1438->$1 = _M0L6_2atmpS1443;
  _M0L7_2abindS1326
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1442 = _M0L7_2abindS1326;
  _M0L6_2atmpS1441
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1442
  };
  #line 413 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L6_2atmpS1440
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1441);
  _M0L8_2atupleS1439
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1439)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1439->$0 = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L8_2atupleS1439->$1 = _M0L6_2atmpS1440;
  _M0L7_2abindS1318
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(8);
  _M0L7_2abindS1318[0] = _M0L8_2atupleS1432;
  _M0L7_2abindS1318[1] = _M0L8_2atupleS1433;
  _M0L7_2abindS1318[2] = _M0L8_2atupleS1434;
  _M0L7_2abindS1318[3] = _M0L8_2atupleS1435;
  _M0L7_2abindS1318[4] = _M0L8_2atupleS1436;
  _M0L7_2abindS1318[5] = _M0L8_2atupleS1437;
  _M0L7_2abindS1318[6] = _M0L8_2atupleS1438;
  _M0L7_2abindS1318[7] = _M0L8_2atupleS1439;
  _M0L6_2atmpS1431 = _M0L7_2abindS1318;
  _M0L6_2atmpS1430
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 8, _M0L6_2atmpS1431
  };
  #line 397 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0FP48clawteam8clawteam5oauth5codex48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1430);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1429;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1393;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1394;
  int32_t _M0L7_2abindS1395;
  int32_t _M0L2__S1396;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1429
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1393
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1393)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1393->$0 = _M0L6_2atmpS1429;
  _M0L12async__testsS1393->$1 = 0;
  #line 452 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0L7_2abindS1394
  = _M0FP48clawteam8clawteam5oauth5codex52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1395 = _M0L7_2abindS1394->$1;
  _M0L2__S1396 = 0;
  while (1) {
    if (_M0L2__S1396 < _M0L7_2abindS1395) {
      struct _M0TUsiE** _M0L8_2afieldS4389 = _M0L7_2abindS1394->$0;
      struct _M0TUsiE** _M0L3bufS1428 = _M0L8_2afieldS4389;
      struct _M0TUsiE* _M0L6_2atmpS4388 =
        (struct _M0TUsiE*)_M0L3bufS1428[_M0L2__S1396];
      struct _M0TUsiE* _M0L3argS1397 = _M0L6_2atmpS4388;
      moonbit_string_t _M0L8_2afieldS4387 = _M0L3argS1397->$0;
      moonbit_string_t _M0L6_2atmpS1425 = _M0L8_2afieldS4387;
      int32_t _M0L8_2afieldS4386 = _M0L3argS1397->$1;
      int32_t _M0L6_2atmpS1426 = _M0L8_2afieldS4386;
      int32_t _M0L6_2atmpS1427;
      moonbit_incref(_M0L6_2atmpS1425);
      moonbit_incref(_M0L12async__testsS1393);
      #line 453 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
      _M0FP48clawteam8clawteam5oauth5codex44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1393, _M0L6_2atmpS1425, _M0L6_2atmpS1426);
      _M0L6_2atmpS1427 = _M0L2__S1396 + 1;
      _M0L2__S1396 = _M0L6_2atmpS1427;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1394);
    }
    break;
  }
  #line 455 "E:\\moonbit\\clawteam\\oauth\\codex\\__generated_driver_for_internal_test.mbt"
  _M0IP016_24default__implP48clawteam8clawteam5oauth5codex28MoonBit__Async__Test__Driver17run__async__testsGRP48clawteam8clawteam5oauth5codex34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1393);
  return 0;
}