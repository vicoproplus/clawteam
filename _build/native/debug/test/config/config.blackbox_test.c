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
struct _M0Y3Int;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14DispatchFailed;

struct _M0TP38clawteam8clawteam6config14OutputPatterns;

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure;

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1849__l570__;

struct _M0TP38clawteam8clawteam6config13SkillTemplate;

struct _M0DTP38clawteam8clawteam5types12SkillVarType6Select;

struct _M0TWssbEu;

struct _M0TUsiE;

struct _M0TPB9ArrayViewGUssEE;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE;

struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE;

struct _M0TPB5ArrayGORPB9SourceLocE;

struct _M0TPB5EntryGssE;

struct _M0DTP38clawteam8clawteam6config10RoleConfig9Assistant;

struct _M0TP38clawteam8clawteam6config5Skill;

struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error;

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE;

struct _M0TP38clawteam8clawteam6config13SkillVariable;

struct _M0TP38clawteam8clawteam6config11AuditConfig;

struct _M0R118_24clawteam_2fclawteam_2fconfig__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1119;

struct _M0TPB6Logger;

struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE;

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError;

struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE;

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ProcessCrashed;

struct _M0TWEOs;

struct _M0TP38clawteam8clawteam6config14ClawTeamConfig;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError18ProcessStartFailed;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid;

struct _M0TPB4Show;

struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2830__l440__;

struct _M0KTPB4ShowS3Int;

struct _M0TP38clawteam8clawteam6config13ChannelConfig;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14WebhookInvalid;

struct _M0TWRPC15error5ErrorEu;

struct _M0TPB6Hasher;

struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err;

struct _M0TUiUWEuQRPC15error5ErrorNsEE;

struct _M0TP38clawteam8clawteam6config15AssistantConfig;

struct _M0TPB5ArrayGRP38clawteam8clawteam6config13SkillVariableE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err;

struct _M0TP38clawteam8clawteam6config13CliToolConfig;

struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2826__l441__;

struct _M0KTPB6LoggerTPB13StringBuilder;

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam22config__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err;

struct _M0DTP38clawteam8clawteam6config10RoleConfig10Supervisor;

struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE2Ok;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13SkillNotFound;

struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError12ChannelError;

struct _M0TWEu;

struct _M0TPB9ArrayViewGsE;

struct _M0TP38clawteam8clawteam6config11AgentConfig;

struct _M0DTPC15error5Error114clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError;

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE;

struct _M0TP38clawteam8clawteam6config16SupervisorConfig;

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError;

struct _M0KTPB4ShowS4Bool;

struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE;

struct _M0Y4Bool;

struct _M0TUsRPB6LoggerE;

struct _M0DTPC15error5Error116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest;

struct _M0TPB13StringBuilder;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ConfigNotFound;

struct _M0TUssE;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError20TemplateRenderFailed;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError9AgentBusy;

struct _M0TPB5ArrayGUsiEE;

struct _M0TWRPC15error5ErrorEs;

struct _M0BTPB6Logger;

struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE;

struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13ChannelConfigEE;

struct _M0TWEuQRPC15error5Error;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError15CliToolNotFound;

struct _M0TP38clawteam8clawteam6config13GatewayConfig;

struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13CliToolConfigEE;

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok;

struct _M0DTPC16result6ResultGuRPC15error5ErrorE2Ok;

struct _M0TPB3MapGssE;

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE3Err;

struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE;

struct _M0TPB13SourceLocRepr;

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE;

struct _M0BTPB4Show;

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam22config__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok;

struct _M0TWuEu;

struct _M0TP38clawteam8clawteam6config12WorkerConfig;

struct _M0TPC16string10StringView;

struct _M0KTPB4ShowS6String;

struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE;

struct _M0TP38clawteam8clawteam6config15WorkspaceConfig;

struct _M0DTP38clawteam8clawteam6config10RoleConfig6Worker;

struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE;

struct _M0TPB5ArrayGsE;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13AgentNotFound;

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError19AuditLogWriteFailed;

struct _M0TPC13ref3RefGiE;

struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE;

struct _M0TUWEuQRPC15error5ErrorNsE;

struct _M0TPB5ArrayGRP38clawteam8clawteam5types9AgentRoleE;

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ProcessTimeout;

struct _M0Y3Int {
  int32_t $0;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14DispatchFailed {
  moonbit_string_t $0;
  
};

struct _M0TP38clawteam8clawteam6config14OutputPatterns {
  moonbit_string_t $0;
  moonbit_string_t $1;
  moonbit_string_t $2;
  
};

struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure {
  moonbit_string_t $0;
  
};

struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1849__l570__ {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t* $0_0;
  struct _M0TPC13ref3RefGiE* $1;
  
};

struct _M0TP38clawteam8clawteam6config13SkillTemplate {
  int64_t $2;
  moonbit_string_t $0;
  struct _M0TPB5ArrayGRP38clawteam8clawteam6config13SkillVariableE* $1;
  
};

struct _M0DTP38clawteam8clawteam5types12SkillVarType6Select {
  struct _M0TPB5ArrayGsE* $0;
  
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

struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE {
  int32_t $1;
  struct _M0TP38clawteam8clawteam6config5Skill** $0;
  
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

struct _M0DTP38clawteam8clawteam6config10RoleConfig9Assistant {
  struct _M0TP38clawteam8clawteam6config15AssistantConfig* $0;
  
};

struct _M0TP38clawteam8clawteam6config5Skill {
  moonbit_string_t $0;
  moonbit_string_t $1;
  moonbit_string_t $2;
  moonbit_string_t $3;
  moonbit_string_t $4;
  moonbit_string_t $5;
  moonbit_string_t $6;
  moonbit_string_t $7;
  moonbit_string_t $8;
  struct _M0TPB5ArrayGsE* $9;
  struct _M0TPB5ArrayGRP38clawteam8clawteam5types9AgentRoleE* $10;
  struct _M0TP38clawteam8clawteam6config13SkillTemplate* $11;
  
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

struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE {
  moonbit_string_t $0;
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* $1;
  
};

struct _M0TP38clawteam8clawteam6config13SkillVariable {
  int32_t $3;
  moonbit_string_t $0;
  moonbit_string_t $1;
  void* $2;
  moonbit_string_t $4;
  
};

struct _M0TP38clawteam8clawteam6config11AuditConfig {
  int32_t $0;
  int32_t $1;
  moonbit_string_t $2;
  
};

struct _M0R118_24clawteam_2fclawteam_2fconfig__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1119 {
  int32_t(* code)(
    struct _M0TWssbEu*,
    moonbit_string_t,
    moonbit_string_t,
    int32_t
  );
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB6Logger {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* $1;
  moonbit_string_t $4;
  struct _M0TP38clawteam8clawteam6config13ChannelConfig* $5;
  
};

struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError {
  moonbit_string_t $0;
  
};

struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE {
  int32_t $1;
  struct _M0TP38clawteam8clawteam6config11AgentConfig** $0;
  
};

struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE {
  moonbit_string_t $0;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* $1;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ProcessCrashed {
  int32_t $0;
  
};

struct _M0TWEOs {
  moonbit_string_t(* code)(struct _M0TWEOs*);
  
};

struct _M0TP38clawteam8clawteam6config14ClawTeamConfig {
  moonbit_string_t $0;
  struct _M0TP38clawteam8clawteam6config13GatewayConfig* $1;
  struct _M0TP38clawteam8clawteam6config15WorkspaceConfig* $2;
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* $3;
  struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE* $4;
  struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE* $5;
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* $6;
  struct _M0TP38clawteam8clawteam6config11AuditConfig* $7;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError18ProcessStartFailed {
  moonbit_string_t $0;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid {
  moonbit_string_t $0;
  
};

struct _M0TPB4Show {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2830__l440__ {
  int32_t(* code)(struct _M0TWEu*);
  struct _M0TWssbEu* $0;
  moonbit_string_t $1;
  
};

struct _M0KTPB4ShowS3Int {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0TP38clawteam8clawteam6config13ChannelConfig {
  int32_t $0;
  moonbit_string_t $1;
  moonbit_string_t $2;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14WebhookInvalid {
  moonbit_string_t $0;
  
};

struct _M0TWRPC15error5ErrorEu {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  
};

struct _M0TPB6Hasher {
  uint32_t $0;
  
};

struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** $0;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* $5;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE3Err {
  void* $0;
  
};

struct _M0TUiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  struct _M0TUWEuQRPC15error5ErrorNsE* $1;
  
};

struct _M0TP38clawteam8clawteam6config15AssistantConfig {
  int32_t $2;
  struct _M0TPB5ArrayGsE* $0;
  struct _M0TPB5ArrayGsE* $1;
  
};

struct _M0TPB5ArrayGRP38clawteam8clawteam6config13SkillVariableE {
  int32_t $1;
  struct _M0TP38clawteam8clawteam6config13SkillVariable** $0;
  
};

struct _M0DTPC16result6ResultGOuRPC15error5ErrorE2Ok {
  int32_t $0;
  
};

struct _M0DTPC16result6ResultGuRPC15error5ErrorE3Err {
  void* $0;
  
};

struct _M0TP38clawteam8clawteam6config13CliToolConfig {
  int32_t $0;
  int64_t $8;
  moonbit_string_t $1;
  struct _M0TPB5ArrayGsE* $2;
  struct _M0TPB3MapGssE* $3;
  moonbit_string_t $4;
  struct _M0TPB5ArrayGsE* $5;
  struct _M0TPB5ArrayGsE* $6;
  struct _M0TP38clawteam8clawteam6config14OutputPatterns* $7;
  
};

struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2826__l441__ {
  int32_t(* code)(struct _M0TWRPC15error5ErrorEu*, void*);
  struct _M0TWRPC15error5ErrorEs* $0;
  struct _M0TWssbEu* $1;
  moonbit_string_t $2;
  
};

struct _M0KTPB6LoggerTPB13StringBuilder {
  struct _M0BTPB6Logger* $0;
  void* $1;
  
};

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam22config__blackbox__test33MoonBitTestDriverInternalSkipTestE3Err {
  void* $0;
  
};

struct _M0DTP38clawteam8clawteam6config10RoleConfig10Supervisor {
  struct _M0TP38clawteam8clawteam6config16SupervisorConfig* $0;
  
};

struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE2Ok {
  struct _M0TP38clawteam8clawteam6config14ClawTeamConfig* $0;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13SkillNotFound {
  moonbit_string_t $0;
  
};

struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err {
  void* $0;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError12ChannelError {
  moonbit_string_t $0;
  
};

struct _M0TWEu {
  int32_t(* code)(struct _M0TWEu*);
  
};

struct _M0TPB9ArrayViewGsE {
  int32_t $1;
  int32_t $2;
  moonbit_string_t* $0;
  
};

struct _M0TP38clawteam8clawteam6config11AgentConfig {
  int32_t $2;
  int32_t $7;
  int32_t $10;
  moonbit_string_t $0;
  moonbit_string_t $1;
  moonbit_string_t $3;
  struct _M0TPB5ArrayGsE* $4;
  struct _M0TPB3MapGssE* $5;
  moonbit_string_t $6;
  void* $8;
  struct _M0TPB5ArrayGsE* $9;
  
};

struct _M0DTPC15error5Error114clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError {
  moonbit_string_t $0;
  
};

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* $1;
  struct _M0TUWEuQRPC15error5ErrorNsE* $5;
  
};

struct _M0TP38clawteam8clawteam6config16SupervisorConfig {
  moonbit_string_t $0;
  struct _M0TPB5ArrayGsE* $1;
  
};

struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError {
  moonbit_string_t $0;
  
};

struct _M0KTPB4ShowS4Bool {
  struct _M0BTPB4Show* $0;
  void* $1;
  
};

struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE {
  int32_t $0;
  int32_t $2;
  int32_t $3;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* $1;
  moonbit_string_t $4;
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* $5;
  
};

struct _M0Y4Bool {
  int32_t $0;
  
};

struct _M0TUsRPB6LoggerE {
  moonbit_string_t $0;
  struct _M0BTPB6Logger* $1_0;
  void* $1_1;
  
};

struct _M0DTPC15error5Error116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest {
  moonbit_string_t $0;
  
};

struct _M0TPB13StringBuilder {
  int32_t $1;
  moonbit_bytes_t $0;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ConfigNotFound {
  moonbit_string_t $0;
  
};

struct _M0TUssE {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError20TemplateRenderFailed {
  moonbit_string_t $0;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError9AgentBusy {
  moonbit_string_t $0;
  
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

struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE {
  moonbit_string_t $0;
  struct _M0TP38clawteam8clawteam6config13ChannelConfig* $1;
  
};

struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13ChannelConfigEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE** $0;
  
};

struct _M0TWEuQRPC15error5Error {
  struct moonbit_result_0(* code)(struct _M0TWEuQRPC15error5Error*);
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError15CliToolNotFound {
  moonbit_string_t $0;
  
};

struct _M0TP38clawteam8clawteam6config13GatewayConfig {
  int32_t $0;
  moonbit_string_t $1;
  
};

struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13CliToolConfigEE {
  int32_t $1;
  int32_t $2;
  struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE** $0;
  
};

struct _M0DTPC16result6ResultGuRPB12InspectErrorE2Ok {
  int32_t $0;
  
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

struct _M0BTPB4Show {
  int32_t(* $method_0)(void*, struct _M0TPB6Logger);
  moonbit_string_t(* $method_1)(void*);
  
};

struct _M0DTPC16result6ResultGbRP38clawteam8clawteam22config__blackbox__test33MoonBitTestDriverInternalSkipTestE2Ok {
  int32_t $0;
  
};

struct _M0TWuEu {
  int32_t(* code)(struct _M0TWuEu*, int32_t);
  
};

struct _M0TP38clawteam8clawteam6config12WorkerConfig {
  moonbit_string_t $0;
  struct _M0TPB5ArrayGsE* $1;
  
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

struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE {
  int32_t $1;
  int32_t $2;
  int32_t $3;
  int32_t $4;
  int32_t $6;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** $0;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* $5;
  
};

struct _M0TP38clawteam8clawteam6config15WorkspaceConfig {
  moonbit_string_t $0;
  moonbit_string_t $1;
  
};

struct _M0DTP38clawteam8clawteam6config10RoleConfig6Worker {
  struct _M0TP38clawteam8clawteam6config12WorkerConfig* $0;
  
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

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13AgentNotFound {
  moonbit_string_t $0;
  
};

struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some {
  int32_t $0_1;
  int32_t $0_2;
  moonbit_string_t $0_0;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError19AuditLogWriteFailed {
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

struct _M0TPB5ArrayGRP38clawteam8clawteam5types9AgentRoleE {
  int32_t $1;
  int32_t* $0;
  
};

struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ProcessTimeout {
  moonbit_string_t $0;
  
};

struct moonbit_result_0 {
  int tag;
  union { int32_t ok; void* err;  } data;
  
};

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__8_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__5_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test52____test__74797065735f746573742e6d6274__11_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__9_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test52____test__74797065735f746573742e6d6274__10_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__7_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__6_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error*
);

int32_t _M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t
);

moonbit_string_t _M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1128(
  struct _M0TWRPC15error5ErrorEs*,
  void*
);

int32_t _M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1119(
  struct _M0TWssbEu*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

struct moonbit_result_0 _M0IP38clawteam8clawteam22config__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP38clawteam8clawteam22config__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testC2830l440(
  struct _M0TWEu*
);

int32_t _M0IP38clawteam8clawteam22config__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testC2826l441(
  struct _M0TWRPC15error5ErrorEu*,
  void*
);

int32_t _M0FP38clawteam8clawteam22config__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error*,
  struct _M0TWEu*,
  struct _M0TWRPC15error5ErrorEu*
);

struct _M0TPB5ArrayGUsiEE* _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
);

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1051(
  int32_t,
  moonbit_string_t,
  int32_t
);

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1046(
  int32_t
);

moonbit_string_t _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1039(
  int32_t,
  moonbit_bytes_t
);

int32_t _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1033(
  int32_t,
  moonbit_string_t
);

#define _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi moonbit_get_cli_args

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*,
  moonbit_string_t,
  int32_t,
  struct _M0TWssbEu*,
  struct _M0TWRPC15error5ErrorEs*
);

int32_t _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam22config__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test42____test__74797065735f746573742e6d6274__11(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test42____test__74797065735f746573742e6d6274__10(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__9(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__8(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__7(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__6(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__5(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__4(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__3(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__2(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__1(
  
);

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__0(
  
);

void* _M0FP38clawteam8clawteam6config16validate__config(
  struct _M0TP38clawteam8clawteam6config14ClawTeamConfig*
);

struct _M0TP38clawteam8clawteam6config14ClawTeamConfig* _M0MP38clawteam8clawteam6config12ConfigLoader15default__config(
  moonbit_string_t
);

struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0FP38clawteam8clawteam6config19default__cli__tools(
  
);

struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0FP38clawteam8clawteam6config22default__shell__config(
  
);

struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0FP38clawteam8clawteam6config22default__codex__config(
  
);

struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0FP38clawteam8clawteam6config23default__gemini__config(
  
);

struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0FP38clawteam8clawteam6config23default__claude__config(
  
);

struct _M0TP38clawteam8clawteam6config11AuditConfig* _M0FP38clawteam8clawteam6config22default__audit__config(
  moonbit_string_t
);

struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0FP38clawteam8clawteam6config24default__gateway__config(
  
);

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam6config13config__paths(
  moonbit_string_t,
  moonbit_string_t
);

int32_t _M0IP38clawteam8clawteam5types9AgentRolePB2Eq5equal(int32_t, int32_t);

int32_t _M0IP38clawteam8clawteam5types11CliToolTypePB2Eq5equal(
  int32_t,
  int32_t
);

int32_t _M0IP38clawteam8clawteam5types16DispatchStrategyPB2Eq5equal(
  int32_t,
  int32_t
);

moonbit_string_t _M0MP38clawteam8clawteam6errors13ClawTeamError11to__message(
  void*
);

int32_t _M0MPC15array5Array8containsGsE(
  struct _M0TPB5ArrayGsE*,
  moonbit_string_t
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

int32_t _M0MPB3Map8containsGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE*,
  moonbit_string_t
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

struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0MPB3Map11from__arrayGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13CliToolConfigEE
);

struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0MPB3Map11from__arrayGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13ChannelConfigEE
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

int32_t _M0MPB3Map3setGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE*,
  moonbit_string_t,
  struct _M0TP38clawteam8clawteam6config13CliToolConfig*
);

int32_t _M0MPB3Map3setGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE*,
  moonbit_string_t,
  struct _M0TP38clawteam8clawteam6config13ChannelConfig*
);

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*
);

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*
);

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE*);

int32_t _M0MPB3Map4growGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE*
);

int32_t _M0MPB3Map4growGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE*
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

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE*,
  moonbit_string_t,
  moonbit_string_t,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE*,
  moonbit_string_t,
  struct _M0TP38clawteam8clawteam6config13CliToolConfig*,
  int32_t
);

int32_t _M0MPB3Map15set__with__hashGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE*,
  moonbit_string_t,
  struct _M0TP38clawteam8clawteam6config13ChannelConfig*,
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

int32_t _M0MPB3Map10push__awayGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE*,
  int32_t,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*
);

int32_t _M0MPB3Map10push__awayGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE*,
  int32_t,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE*
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

int32_t _M0MPB3Map10set__entryGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE*,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*,
  int32_t
);

int32_t _M0MPB3Map10set__entryGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE*,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE*,
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

int32_t _M0MPB3Map20add__entry__to__tailGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE*,
  int32_t,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*
);

int32_t _M0MPB3Map20add__entry__to__tailGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE*,
  int32_t,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE*
);

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t
);

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t
);

struct _M0TPB3MapGssE* _M0MPB3Map11new_2einnerGssE(int32_t);

struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0MPB3Map11new_2einnerGsRP38clawteam8clawteam6config13CliToolConfigE(
  int32_t
);

struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0MPB3Map11new_2einnerGsRP38clawteam8clawteam6config13ChannelConfigE(
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

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE*
);

struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigEE(
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*
);

struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigEE(
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE*
);

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(moonbit_string_t*);

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(struct _M0TPB9ArrayViewGsE);

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1849l570(struct _M0TWEOs*);

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(moonbit_string_t);

int32_t _M0IPC13int3IntPB4Show6output(int32_t, struct _M0TPB6Logger);

int32_t _M0IPC14bool4BoolPB4Show6output(int32_t, struct _M0TPB6Logger);

int32_t _M0MPC16string6String8contains(
  moonbit_string_t,
  struct _M0TPC16string10StringView
);

int32_t _M0MPC16string10StringView8contains(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

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

int64_t _M0MPC16string10StringView4find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int64_t _M0FPB18brute__force__find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

int64_t _M0FPB28boyer__moore__horspool__find(
  struct _M0TPC16string10StringView,
  struct _M0TPC16string10StringView
);

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

int32_t _M0MPC15array9ArrayView6lengthGUsRP38clawteam8clawteam6config13CliToolConfigEE(
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13CliToolConfigEE
);

int32_t _M0MPC15array9ArrayView6lengthGUsRP38clawteam8clawteam6config13ChannelConfigEE(
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13ChannelConfigEE
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

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView,
  int32_t
);

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

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE*);

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

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGbE(
  void*
);

int32_t _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
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

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGiE(
  void*
);

int32_t _M0IPC13int3IntPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void*,
  struct _M0TPB6Logger
);

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_38 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 56, 58, 49, 
    50, 45, 57, 56, 58, 51, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[1]; 
} const moonbit_string_literal_0 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 0), 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_21 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 50, 55, 58, 
    49, 53, 45, 49, 50, 55, 58, 55, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_130 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    47, 99, 108, 97, 119, 116, 101, 97, 109, 46, 106, 115, 111, 110, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_194 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    71, 97, 116, 101, 119, 97, 121, 67, 111, 110, 102, 105, 103, 32, 
    99, 114, 101, 97, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_185 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 46, 83, 110, 97, 
    112, 115, 104, 111, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_139 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    36827, 31243, 36229, 26102, 32, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_152 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_174 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 101, 110, 100, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_80 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 53, 58, 51, 
    45, 54, 53, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_58 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 49, 58, 51, 
    45, 57, 49, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_141 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10),
    36827, 31243, 23849, 28291, 65292, 36864, 20986, 30721, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_159 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 48, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_91 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 54, 58, 51, 
    45, 53, 54, 58, 55, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_131 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    26080, 21487, 29992, 32, 65, 103, 101, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_40 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 56, 58, 51, 
    45, 57, 56, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_62 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 56, 50, 58, 49, 
    52, 45, 56, 50, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_78 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 53, 58, 49, 
    50, 45, 54, 53, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_77 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 52, 58, 51, 
    45, 54, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_12 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    115, 104, 111, 117, 108, 100, 32, 102, 97, 105, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_169 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    110, 117, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_115 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 55, 58, 49, 50, 
    45, 55, 58, 50, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_4 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 105, 110, 100, 101, 120, 34, 58, 32, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_184 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 41, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_49 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 52, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_75 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 52, 58, 49, 
    50, 45, 54, 52, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_24 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    51, 48, 48, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_198 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    100, 101, 102, 97, 117, 108, 116, 32, 99, 108, 105, 32, 116, 111, 
    111, 108, 115, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_96 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 55, 58, 49, 
    50, 45, 51, 55, 58, 50, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_134 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    37197, 32622, 25991, 20214, 26080, 25928, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_172 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 115, 116, 97, 114, 116, 95, 108, 105, 110, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_79 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 53, 58, 51, 
    51, 45, 54, 53, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[30]; 
} const moonbit_string_literal_67 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 29), 
    47, 118, 97, 114, 47, 108, 111, 103, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 97, 117, 100, 105, 116, 46, 106, 115, 111, 110, 108, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_61 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 56, 50, 58, 52, 
    48, 45, 56, 50, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_146 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10),
    23457, 35745, 26085, 24535, 20889, 20837, 22833, 36133, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_89 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 54, 58, 49, 
    50, 45, 53, 54, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_173 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    44, 34, 115, 116, 97, 114, 116, 95, 99, 111, 108, 117, 109, 110, 
    34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_44 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 57, 58, 51, 
    45, 57, 57, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_23 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 48, 52, 58, 
    51, 51, 45, 49, 48, 52, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_178 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 55, 48, 58, 53, 45, 55, 
    48, 58, 54, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_48 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 56, 57, 58, 51, 
    54, 45, 56, 57, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_163 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 32, 34, 97, 114, 103, 115, 95, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_140 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    109, 115, 41, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_51 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 48, 58, 49, 
    50, 45, 57, 48, 58, 50, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_155 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_153 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_195 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    65, 117, 100, 105, 116, 67, 111, 110, 102, 105, 103, 32, 99, 114, 
    101, 97, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_164 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_117 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 55, 58, 51, 45, 
    55, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_13 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 50, 54, 58, 
    50, 51, 45, 49, 50, 54, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_106 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 51, 58, 49, 
    50, 45, 49, 51, 58, 51, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_63 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 56, 51, 58, 50, 
    52, 45, 56, 51, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_145 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    87, 101, 98, 104, 111, 111, 107, 32, 26080, 25928, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_57 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 47, 99, 108, 97, 119, 
    116, 101, 97, 109, 46, 106, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_105 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 49, 58, 51, 
    45, 50, 49, 58, 53, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[16]; 
} const moonbit_string_literal_65 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 15), 
    115, 104, 111, 117, 108, 100, 32, 110, 111, 116, 32, 102, 97, 105, 
    108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_42 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 57, 58, 49, 
    50, 45, 57, 57, 58, 51, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[17]; 
} const moonbit_string_literal_10 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 16), 
    47, 116, 109, 112, 47, 97, 117, 100, 105, 116, 46, 106, 115, 111, 
    110, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_3 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    123, 34, 112, 97, 99, 107, 97, 103, 101, 34, 58, 32, 34, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    47, 99, 111, 110, 102, 105, 103, 34, 44, 32, 34, 102, 105, 108, 101, 
    110, 97, 109, 101, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_143 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    27966, 21457, 22833, 36133, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_113 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 58, 51, 56, 
    45, 54, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_112 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 58, 49, 50, 
    45, 54, 58, 50, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_39 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 56, 58, 52, 
    53, 45, 57, 56, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_32 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 54, 58, 51, 
    45, 57, 54, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_124 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    98, 97, 115, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[65]; 
} const moonbit_string_literal_116 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 64), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 55, 58, 51, 55, 
    45, 55, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_52 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 48, 58, 51, 
    48, 45, 57, 48, 58, 54, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_37 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    99, 111, 100, 101, 120, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_33 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    103, 101, 109, 105, 110, 105, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_201 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    116, 121, 112, 101, 115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 
    0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_18 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 50, 55, 58, 
    50, 52, 45, 49, 50, 55, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_69 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 55, 52, 58, 51, 
    54, 45, 55, 52, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_45 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    47, 104, 111, 109, 101, 47, 117, 115, 101, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_16 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 50, 54, 58, 
    49, 52, 45, 49, 50, 54, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[50]; 
} const moonbit_string_literal_149 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 49), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 52, 53, 52, 58, 57, 45, 
    52, 53, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[35]; 
} const moonbit_string_literal_53 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 34), 
    47, 104, 111, 109, 101, 47, 117, 115, 101, 114, 47, 46, 99, 108, 
    97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 97, 109, 
    46, 106, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_167 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_54 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 48, 58, 51, 
    45, 57, 48, 58, 54, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[34]; 
} const moonbit_string_literal_200 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 33), 
    118, 97, 108, 105, 100, 97, 116, 101, 32, 99, 111, 110, 102, 105, 
    103, 32, 119, 105, 116, 104, 32, 105, 110, 118, 97, 108, 105, 100, 
    32, 112, 111, 114, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[26]; 
} const moonbit_string_literal_191 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 25), 
    68, 105, 115, 112, 97, 116, 99, 104, 83, 116, 114, 97, 116, 101, 
    103, 121, 32, 101, 113, 117, 97, 108, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_142 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    65, 103, 101, 110, 116, 32, 24537, 30860, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_109 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 52, 58, 49, 
    50, 45, 49, 52, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[28]; 
} const moonbit_string_literal_128 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 27), 
    47, 46, 99, 108, 97, 119, 116, 101, 97, 109, 47, 108, 111, 103, 115, 
    47, 97, 117, 100, 105, 116, 46, 106, 115, 111, 110, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_92 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    9581, 9472, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_82 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    80, 114, 105, 109, 97, 114, 121, 32, 65, 115, 115, 105, 115, 116, 
    97, 110, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_35 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 55, 58, 52, 
    54, 45, 57, 55, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_41 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    115, 104, 101, 108, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_137 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    67, 76, 73, 32, 24037, 20855, 26410, 25214, 21040, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_19 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 50, 55, 58, 
    54, 53, 45, 49, 50, 55, 58, 55, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_171 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 34, 102, 105, 108, 101, 110, 97, 109, 101, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_100 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 48, 58, 52, 
    56, 45, 50, 48, 58, 53, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_81 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    97, 103, 101, 110, 116, 45, 48, 48, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[22]; 
} const moonbit_string_literal_197 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 21), 
    99, 111, 110, 102, 105, 103, 32, 112, 97, 116, 104, 115, 32, 112, 
    114, 105, 111, 114, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_29 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    99, 108, 97, 117, 100, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_22 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 48, 52, 58, 
    49, 50, 45, 49, 48, 52, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_1 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    115, 107, 105, 112, 112, 101, 100, 32, 116, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_192 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    67, 108, 105, 84, 111, 111, 108, 67, 111, 110, 102, 105, 103, 32, 
    99, 114, 101, 97, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_168 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    44, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_26 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 48, 53, 58, 
    49, 50, 45, 49, 48, 53, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[25]; 
} const moonbit_string_literal_129 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 24), 
    47, 46, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 
    116, 101, 97, 109, 46, 106, 115, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_90 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 54, 58, 54, 
    52, 45, 53, 54, 58, 55, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_43 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 57, 58, 52, 
    53, 45, 57, 57, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_11 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    49, 46, 48, 46, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_85 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 52, 58, 51, 
    45, 53, 52, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_148 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 117, 114, 114, 111, 103, 
    97, 116, 101, 32, 112, 97, 105, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_97 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 55, 58, 51, 
    54, 45, 51, 55, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_203 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    108, 111, 97, 100, 101, 114, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_181 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    10, 32, 32, 97, 116, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_60 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 56, 50, 58, 50, 
    51, 45, 56, 50, 58, 51, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[68]; 
} const moonbit_string_literal_28 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 67), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 48, 53, 58, 
    51, 45, 49, 48, 53, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[105]; 
} const moonbit_string_literal_186 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 104), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 111, 110, 66, 
    105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 
    116, 101, 114, 110, 97, 108, 74, 115, 69, 114, 114, 111, 114, 46, 
    77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 105, 
    118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 74, 115, 69, 
    114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[31]; 
} const moonbit_string_literal_157 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 30), 
    114, 97, 100, 105, 120, 32, 109, 117, 115, 116, 32, 98, 101, 32, 
    98, 101, 116, 119, 101, 101, 110, 32, 50, 32, 97, 110, 100, 32, 51, 
    54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_47 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 56, 57, 58, 49, 
    50, 45, 56, 57, 58, 50, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_193 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    65, 103, 101, 110, 116, 67, 111, 110, 102, 105, 103, 32, 99, 114, 
    101, 97, 116, 105, 111, 110, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[20]; 
} const moonbit_string_literal_166 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 19), 
    44, 32, 34, 101, 120, 112, 101, 99, 116, 95, 98, 97, 115, 101, 54, 
    52, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_59 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    118, 97, 108, 105, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_56 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 49, 58, 51, 
    48, 45, 57, 49, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_74 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 55, 53, 58, 51, 
    45, 55, 53, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_70 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 55, 52, 58, 51, 
    45, 55, 52, 58, 52, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_31 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 54, 58, 52, 
    54, 45, 57, 54, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[6]; 
} const moonbit_string_literal_104 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 5), 
    102, 97, 108, 115, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_162 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    64, 69, 88, 80, 69, 67, 84, 95, 70, 65, 73, 76, 69, 68, 32, 123, 
    34, 108, 111, 99, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[64]; 
} const moonbit_string_literal_114 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 63), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 58, 51, 45, 
    54, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_110 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 52, 58, 52, 
    49, 45, 49, 52, 58, 52, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[56]; 
} const moonbit_string_literal_177 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 55), 
    105, 110, 118, 97, 108, 105, 100, 32, 115, 116, 97, 114, 116, 32, 
    111, 114, 32, 101, 110, 100, 32, 105, 110, 100, 101, 120, 32, 102, 
    111, 114, 32, 83, 116, 114, 105, 110, 103, 58, 58, 99, 111, 100, 
    101, 112, 111, 105, 110, 116, 95, 108, 101, 110, 103, 116, 104, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_123 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 36, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_189 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    67, 108, 105, 84, 111, 111, 108, 84, 121, 112, 101, 32, 101, 113, 
    117, 97, 108, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[13]; 
} const moonbit_string_literal_165 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 12), 
    44, 32, 34, 97, 99, 116, 117, 97, 108, 34, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_15 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    102, 97, 105, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[49]; 
} const moonbit_string_literal_180 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 48), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 98, 121, 
    116, 101, 115, 46, 109, 98, 116, 58, 50, 57, 56, 58, 53, 45, 50, 
    57, 56, 58, 51, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_121 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    68, 117, 112, 108, 105, 99, 97, 116, 101, 32, 115, 107, 105, 108, 
    108, 32, 73, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[107]; 
} const moonbit_string_literal_187 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 106), 
    99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 101, 
    97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 107, 
    98, 111, 120, 95, 116, 101, 115, 116, 46, 77, 111, 111, 110, 66, 
    105, 116, 84, 101, 115, 116, 68, 114, 105, 118, 101, 114, 73, 110, 
    116, 101, 114, 110, 97, 108, 83, 107, 105, 112, 84, 101, 115, 116, 
    46, 77, 111, 111, 110, 66, 105, 116, 84, 101, 115, 116, 68, 114, 
    105, 118, 101, 114, 73, 110, 116, 101, 114, 110, 97, 108, 83, 107, 
    105, 112, 84, 101, 115, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[37]; 
} const moonbit_string_literal_160 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 36), 
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 
    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 
    116, 117, 118, 119, 120, 121, 122, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[54]; 
} const moonbit_string_literal_151 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 53), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 118, 105, 101, 119, 46, 109, 98, 116, 58, 51, 
    57, 57, 58, 53, 45, 51, 57, 57, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_36 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 55, 58, 51, 
    45, 57, 55, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_107 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 51, 58, 52, 
    52, 45, 49, 51, 58, 53, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_83 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 52, 58, 49, 
    50, 45, 53, 52, 58, 50, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_14 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 50, 54, 58, 
    52, 54, 45, 49, 50, 54, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[51]; 
} const moonbit_string_literal_188 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 50), 
    109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 111, 
    114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 46, 73, 110, 115, 
    112, 101, 99, 116, 69, 114, 114, 111, 114, 46, 73, 110, 115, 112, 
    101, 99, 116, 69, 114, 114, 111, 114, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_182 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 10, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[33]; 
} const moonbit_string_literal_8 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 32), 
    45, 45, 45, 45, 45, 32, 69, 78, 68, 32, 77, 79, 79, 78, 32, 84, 69, 
    83, 84, 32, 82, 69, 83, 85, 76, 84, 32, 45, 45, 45, 45, 45, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[68]; 
} const moonbit_string_literal_25 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 67), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 48, 52, 58, 
    51, 45, 49, 48, 52, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_50 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 56, 57, 58, 51, 
    45, 56, 57, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_122 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    45, 108, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_86 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 53, 58, 49, 
    50, 45, 53, 53, 58, 50, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_76 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 54, 52, 58, 51, 
    51, 45, 54, 52, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[29]; 
} const moonbit_string_literal_119 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 28), 
    71, 97, 116, 101, 119, 97, 121, 32, 104, 111, 115, 116, 32, 99, 97, 
    110, 110, 111, 116, 32, 98, 101, 32, 101, 109, 112, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_88 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 53, 58, 51, 
    45, 53, 53, 58, 53, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[19]; 
} const moonbit_string_literal_190 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 18), 
    65, 103, 101, 110, 116, 82, 111, 108, 101, 32, 101, 113, 117, 97, 
    108, 105, 116, 121, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_183 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    70, 97, 105, 108, 117, 114, 101, 40, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_125 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 62, 0};

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

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_93 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 54, 58, 49, 
    50, 45, 51, 54, 58, 53, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_71 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 55, 53, 58, 49, 
    50, 45, 55, 53, 58, 51, 51, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_95 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 54, 58, 51, 
    45, 51, 54, 58, 55, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_138 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    36827, 31243, 21551, 21160, 22833, 36133, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_84 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 52, 58, 51, 
    49, 45, 53, 52, 58, 52, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[24]; 
} const moonbit_string_literal_196 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 23), 
    100, 101, 102, 97, 117, 108, 116, 32, 99, 111, 110, 102, 105, 103, 
    32, 105, 115, 32, 118, 97, 108, 105, 100, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[21]; 
} const moonbit_string_literal_120 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 20), 
    68, 117, 112, 108, 105, 99, 97, 116, 101, 32, 97, 103, 101, 110, 
    116, 32, 73, 68, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_66 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 56, 51, 58, 49, 
    53, 45, 56, 51, 58, 54, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_150 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    73, 110, 118, 97, 108, 105, 100, 32, 105, 110, 100, 101, 120, 32, 
    102, 111, 114, 32, 86, 105, 101, 119, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_170 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    123, 34, 112, 107, 103, 34, 58, 34, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_87 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 53, 53, 58, 51, 
    51, 45, 53, 53, 58, 53, 50, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_17 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    112, 111, 114, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_68 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 55, 52, 58, 49, 
    50, 45, 55, 52, 58, 50, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[18]; 
} const moonbit_string_literal_179 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 17), 
    67, 104, 97, 114, 32, 111, 117, 116, 32, 111, 102, 32, 114, 97, 110, 
    103, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[23]; 
} const moonbit_string_literal_199 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 22), 
    100, 101, 102, 97, 117, 108, 116, 32, 103, 97, 116, 101, 119, 97, 
    121, 32, 99, 111, 110, 102, 105, 103, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_64 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 56, 51, 58, 52, 
    56, 45, 56, 51, 58, 54, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_30 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 54, 58, 49, 
    50, 45, 57, 54, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[4]; 
} const moonbit_string_literal_156 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 3), 
    92, 117, 123, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_73 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    51, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[9]; 
} const moonbit_string_literal_147 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 8), 
    27169, 26495, 28210, 26579, 22833, 36133, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_99 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 48, 58, 49, 
    50, 45, 50, 48, 58, 51, 56, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_46 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    47, 119, 111, 114, 107, 115, 112, 97, 99, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[53]; 
} const moonbit_string_literal_158 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 52), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 116, 111, 
    95, 115, 116, 114, 105, 110, 103, 46, 109, 98, 116, 58, 50, 50, 52, 
    58, 53, 45, 50, 50, 52, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_135 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    65, 103, 101, 110, 116, 32, 26410, 25214, 21040, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[41]; 
} const moonbit_string_literal_118 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 40), 
    71, 97, 116, 101, 119, 97, 121, 32, 112, 111, 114, 116, 32, 109, 
    117, 115, 116, 32, 98, 101, 32, 98, 101, 116, 119, 101, 101, 110, 
    32, 49, 32, 97, 110, 100, 32, 54, 53, 53, 51, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_133 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    37197, 32622, 25991, 20214, 26410, 25214, 21040, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_55 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 49, 58, 49, 
    50, 45, 57, 49, 58, 50, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[5]; 
} const moonbit_string_literal_20 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 4), 
    116, 114, 117, 101, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_136 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    83, 107, 105, 108, 108, 32, 26410, 25214, 21040, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[2]; 
} const moonbit_string_literal_161 =
  { -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 1), 34, 0};

struct { int32_t rc; uint32_t meta; uint16_t const data[3]; 
} const moonbit_string_literal_154 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 2), 
    92, 98, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[69]; 
} const moonbit_string_literal_27 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 68), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 48, 53, 58, 
    51, 51, 45, 49, 48, 53, 58, 52, 52, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[48]; 
} const moonbit_string_literal_176 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 47), 
    64, 109, 111, 111, 110, 98, 105, 116, 108, 97, 110, 103, 47, 99, 
    111, 114, 101, 47, 98, 117, 105, 108, 116, 105, 110, 58, 115, 116, 
    114, 105, 110, 103, 46, 109, 98, 116, 58, 56, 49, 58, 57, 45, 56, 
    49, 58, 52, 48, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[15]; 
} const moonbit_string_literal_175 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 14), 
    44, 34, 101, 110, 100, 95, 99, 111, 108, 117, 109, 110, 34, 58, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_102 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 49, 58, 49, 
    50, 45, 50, 49, 58, 51, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_72 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 55, 53, 58, 52, 
    51, 45, 55, 53, 58, 52, 55, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[8]; 
} const moonbit_string_literal_132 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 7), 
    26080, 25928, 30340, 35282, 33394, 31867, 22411, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[11]; 
} const moonbit_string_literal_127 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 10), 
    87, 111, 114, 107, 105, 110, 103, 46, 46, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_108 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 51, 58, 51, 
    45, 49, 51, 58, 53, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_103 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 49, 58, 52, 
    57, 45, 50, 49, 58, 53, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_98 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 55, 58, 51, 
    45, 51, 55, 58, 52, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_34 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 57, 55, 58, 49, 
    50, 45, 57, 55, 58, 51, 54, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_9 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    49, 50, 55, 46, 48, 46, 48, 46, 49, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[7]; 
} const moonbit_string_literal_144 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 6), 
    28192, 36947, 38169, 35823, 58, 32, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[12]; 
} const moonbit_string_literal_126 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 11), 
    84, 104, 105, 110, 107, 105, 110, 103, 46, 46, 46, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_101 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 50, 48, 58, 51, 
    45, 50, 48, 58, 53, 53, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[10]; 
} const moonbit_string_literal_202 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 9), 
    116, 121, 112, 101, 115, 46, 109, 98, 116, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[66]; 
} const moonbit_string_literal_111 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 65), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 49, 52, 58, 51, 
    45, 49, 52, 58, 52, 57, 0
  };

struct { int32_t rc; uint32_t meta; uint16_t const data[67]; 
} const moonbit_string_literal_94 =
  {
    -1, Moonbit_make_array_header(moonbit_BLOCK_KIND_VAL_ARRAY, 1, 66), 
    64, 99, 108, 97, 119, 116, 101, 97, 109, 47, 99, 108, 97, 119, 116, 
    101, 97, 109, 47, 99, 111, 110, 102, 105, 103, 95, 98, 108, 97, 99, 
    107, 98, 111, 120, 95, 116, 101, 115, 116, 58, 116, 121, 112, 101, 
    115, 95, 116, 101, 115, 116, 46, 109, 98, 116, 58, 51, 54, 58, 54, 
    56, 45, 51, 54, 58, 55, 52, 0
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
} const _M0FP38clawteam8clawteam22config__blackbox__test52____test__74797065735f746573742e6d6274__11_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test52____test__74797065735f746573742e6d6274__11_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__1_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__1_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__7_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__7_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__6_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__6_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__9_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__9_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__4_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__4_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__3_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__3_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test52____test__74797065735f746573742e6d6274__10_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test52____test__74797065735f746573742e6d6274__10_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__2_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__2_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__0_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__0_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__8_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__8_2edyncall
  };

struct { int32_t rc; uint32_t meta; struct _M0TWRPC15error5ErrorEs data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1128$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1128
  };

struct { int32_t rc; uint32_t meta; struct _M0TWEuQRPC15error5Error data; 
} const _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__5_2edyncall$closure =
  {
    -1, Moonbit_make_regular_object_header(2, 0, 0),
    _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__5_2edyncall
  };

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__4_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__4_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__6_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__6_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__7_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__7_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test48____test__74797065735f746573742e6d6274__10_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test52____test__74797065735f746573742e6d6274__10_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__1_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__1_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__0_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__0_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__2_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__2_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__3_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__3_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__9_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__9_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test48____test__74797065735f746573742e6d6274__11_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test52____test__74797065735f746573742e6d6274__11_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__5_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__5_2edyncall$closure.data;

struct _M0TWEuQRPC15error5Error* _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__8_2eclo =
  (struct _M0TWEuQRPC15error5Error*)&_M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__8_2edyncall$closure.data;

struct { int32_t rc; uint32_t meta; struct _M0BTPB4Show data; 
} _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC13int3IntPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow,
       .$method_1 = _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGiE}
  };

struct _M0BTPB4Show* _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

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
} _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object =
  {
    -1,
    Moonbit_make_regular_object_header(sizeof(struct _M0BTPB4Show) >> 2, 0, 0),
    {.$method_0 = _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow,
       .$method_1 = _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGbE}
  };

struct _M0BTPB4Show* _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id =
  &_M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id$object.data;

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

int64_t _M0FPB33brute__force__find_2econstr_2f439 = 0ll;

int64_t _M0FPB43boyer__moore__horspool__find_2econstr_2f425 = 0ll;

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0FP38clawteam8clawteam22config__blackbox__test48moonbit__test__driver__internal__no__args__tests;

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__8_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2872
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__8();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__5_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2871
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__5();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test52____test__74797065735f746573742e6d6274__11_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2870
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test42____test__74797065735f746573742e6d6274__11();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__9_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2869
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__9();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__3_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2868
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__3();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__2_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2867
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__2();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__0_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2866
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__0();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__1_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2865
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__1();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test52____test__74797065735f746573742e6d6274__10_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2864
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test42____test__74797065735f746573742e6d6274__10();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__7_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2863
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__7();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__6_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2862
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__6();
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test51____test__74797065735f746573742e6d6274__4_2edyncall(
  struct _M0TWEuQRPC15error5Error* _M0L6_2aenvS2861
) {
  return _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__4();
}

int32_t _M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__execute(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1149,
  moonbit_string_t _M0L8filenameS1124,
  int32_t _M0L5indexS1127
) {
  struct _M0R118_24clawteam_2fclawteam_2fconfig__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1119* _closure_3383;
  struct _M0TWssbEu* _M0L14handle__resultS1119;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1128;
  void* _M0L11_2atry__errS1143;
  struct moonbit_result_0 _tmp_3385;
  int32_t _handle__error__result_3386;
  int32_t _M0L6_2atmpS2849;
  void* _M0L3errS1144;
  moonbit_string_t _M0L4nameS1146;
  struct _M0DTPC15error5Error116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest* _M0L36_2aMoonBitTestDriverInternalSkipTestS1147;
  moonbit_string_t _M0L8_2afieldS2873;
  int32_t _M0L6_2acntS3237;
  moonbit_string_t _M0L7_2anameS1148;
  #line 539 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  moonbit_incref(_M0L8filenameS1124);
  _closure_3383
  = (struct _M0R118_24clawteam_2fclawteam_2fconfig__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1119*)moonbit_malloc(sizeof(struct _M0R118_24clawteam_2fclawteam_2fconfig__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1119));
  Moonbit_object_header(_closure_3383)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R118_24clawteam_2fclawteam_2fconfig__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1119, $1) >> 2, 1, 0);
  _closure_3383->code
  = &_M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1119;
  _closure_3383->$0 = _M0L5indexS1127;
  _closure_3383->$1 = _M0L8filenameS1124;
  _M0L14handle__resultS1119 = (struct _M0TWssbEu*)_closure_3383;
  _M0L17error__to__stringS1128
  = (struct _M0TWRPC15error5ErrorEs*)&_M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1128$closure.data;
  moonbit_incref(_M0L12async__testsS1149);
  moonbit_incref(_M0L17error__to__stringS1128);
  moonbit_incref(_M0L8filenameS1124);
  moonbit_incref(_M0L14handle__resultS1119);
  #line 573 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3385
  = _M0IP38clawteam8clawteam22config__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__test(_M0L12async__testsS1149, _M0L8filenameS1124, _M0L5indexS1127, _M0L14handle__resultS1119, _M0L17error__to__stringS1128);
  if (_tmp_3385.tag) {
    int32_t const _M0L5_2aokS2858 = _tmp_3385.data.ok;
    _handle__error__result_3386 = _M0L5_2aokS2858;
  } else {
    void* const _M0L6_2aerrS2859 = _tmp_3385.data.err;
    moonbit_decref(_M0L12async__testsS1149);
    moonbit_decref(_M0L17error__to__stringS1128);
    moonbit_decref(_M0L8filenameS1124);
    _M0L11_2atry__errS1143 = _M0L6_2aerrS2859;
    goto join_1142;
  }
  if (_handle__error__result_3386) {
    moonbit_decref(_M0L12async__testsS1149);
    moonbit_decref(_M0L17error__to__stringS1128);
    moonbit_decref(_M0L8filenameS1124);
    _M0L6_2atmpS2849 = 1;
  } else {
    struct moonbit_result_0 _tmp_3387;
    int32_t _handle__error__result_3388;
    moonbit_incref(_M0L12async__testsS1149);
    moonbit_incref(_M0L17error__to__stringS1128);
    moonbit_incref(_M0L8filenameS1124);
    moonbit_incref(_M0L14handle__resultS1119);
    #line 576 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
    _tmp_3387
    = _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(_M0L12async__testsS1149, _M0L8filenameS1124, _M0L5indexS1127, _M0L14handle__resultS1119, _M0L17error__to__stringS1128);
    if (_tmp_3387.tag) {
      int32_t const _M0L5_2aokS2856 = _tmp_3387.data.ok;
      _handle__error__result_3388 = _M0L5_2aokS2856;
    } else {
      void* const _M0L6_2aerrS2857 = _tmp_3387.data.err;
      moonbit_decref(_M0L12async__testsS1149);
      moonbit_decref(_M0L17error__to__stringS1128);
      moonbit_decref(_M0L8filenameS1124);
      _M0L11_2atry__errS1143 = _M0L6_2aerrS2857;
      goto join_1142;
    }
    if (_handle__error__result_3388) {
      moonbit_decref(_M0L12async__testsS1149);
      moonbit_decref(_M0L17error__to__stringS1128);
      moonbit_decref(_M0L8filenameS1124);
      _M0L6_2atmpS2849 = 1;
    } else {
      struct moonbit_result_0 _tmp_3389;
      int32_t _handle__error__result_3390;
      moonbit_incref(_M0L12async__testsS1149);
      moonbit_incref(_M0L17error__to__stringS1128);
      moonbit_incref(_M0L8filenameS1124);
      moonbit_incref(_M0L14handle__resultS1119);
      #line 579 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _tmp_3389
      = _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(_M0L12async__testsS1149, _M0L8filenameS1124, _M0L5indexS1127, _M0L14handle__resultS1119, _M0L17error__to__stringS1128);
      if (_tmp_3389.tag) {
        int32_t const _M0L5_2aokS2854 = _tmp_3389.data.ok;
        _handle__error__result_3390 = _M0L5_2aokS2854;
      } else {
        void* const _M0L6_2aerrS2855 = _tmp_3389.data.err;
        moonbit_decref(_M0L12async__testsS1149);
        moonbit_decref(_M0L17error__to__stringS1128);
        moonbit_decref(_M0L8filenameS1124);
        _M0L11_2atry__errS1143 = _M0L6_2aerrS2855;
        goto join_1142;
      }
      if (_handle__error__result_3390) {
        moonbit_decref(_M0L12async__testsS1149);
        moonbit_decref(_M0L17error__to__stringS1128);
        moonbit_decref(_M0L8filenameS1124);
        _M0L6_2atmpS2849 = 1;
      } else {
        struct moonbit_result_0 _tmp_3391;
        int32_t _handle__error__result_3392;
        moonbit_incref(_M0L12async__testsS1149);
        moonbit_incref(_M0L17error__to__stringS1128);
        moonbit_incref(_M0L8filenameS1124);
        moonbit_incref(_M0L14handle__resultS1119);
        #line 582 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        _tmp_3391
        = _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(_M0L12async__testsS1149, _M0L8filenameS1124, _M0L5indexS1127, _M0L14handle__resultS1119, _M0L17error__to__stringS1128);
        if (_tmp_3391.tag) {
          int32_t const _M0L5_2aokS2852 = _tmp_3391.data.ok;
          _handle__error__result_3392 = _M0L5_2aokS2852;
        } else {
          void* const _M0L6_2aerrS2853 = _tmp_3391.data.err;
          moonbit_decref(_M0L12async__testsS1149);
          moonbit_decref(_M0L17error__to__stringS1128);
          moonbit_decref(_M0L8filenameS1124);
          _M0L11_2atry__errS1143 = _M0L6_2aerrS2853;
          goto join_1142;
        }
        if (_handle__error__result_3392) {
          moonbit_decref(_M0L12async__testsS1149);
          moonbit_decref(_M0L17error__to__stringS1128);
          moonbit_decref(_M0L8filenameS1124);
          _M0L6_2atmpS2849 = 1;
        } else {
          struct moonbit_result_0 _tmp_3393;
          moonbit_incref(_M0L14handle__resultS1119);
          #line 585 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
          _tmp_3393
          = _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(_M0L12async__testsS1149, _M0L8filenameS1124, _M0L5indexS1127, _M0L14handle__resultS1119, _M0L17error__to__stringS1128);
          if (_tmp_3393.tag) {
            int32_t const _M0L5_2aokS2850 = _tmp_3393.data.ok;
            _M0L6_2atmpS2849 = _M0L5_2aokS2850;
          } else {
            void* const _M0L6_2aerrS2851 = _tmp_3393.data.err;
            _M0L11_2atry__errS1143 = _M0L6_2aerrS2851;
            goto join_1142;
          }
        }
      }
    }
  }
  if (!_M0L6_2atmpS2849) {
    void* _M0L116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2860 =
      (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
    Moonbit_object_header(_M0L116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2860)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
    ((struct _M0DTPC15error5Error116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2860)->$0
    = (moonbit_string_t)moonbit_string_literal_0.data;
    _M0L11_2atry__errS1143
    = _M0L116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2860;
    goto join_1142;
  } else {
    moonbit_decref(_M0L14handle__resultS1119);
  }
  goto joinlet_3384;
  join_1142:;
  _M0L3errS1144 = _M0L11_2atry__errS1143;
  _M0L36_2aMoonBitTestDriverInternalSkipTestS1147
  = (struct _M0DTPC15error5Error116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L3errS1144;
  _M0L8_2afieldS2873 = _M0L36_2aMoonBitTestDriverInternalSkipTestS1147->$0;
  _M0L6_2acntS3237
  = Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1147)->rc;
  if (_M0L6_2acntS3237 > 1) {
    int32_t _M0L11_2anew__cntS3238 = _M0L6_2acntS3237 - 1;
    Moonbit_object_header(_M0L36_2aMoonBitTestDriverInternalSkipTestS1147)->rc
    = _M0L11_2anew__cntS3238;
    moonbit_incref(_M0L8_2afieldS2873);
  } else if (_M0L6_2acntS3237 == 1) {
    #line 592 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L36_2aMoonBitTestDriverInternalSkipTestS1147);
  }
  _M0L7_2anameS1148 = _M0L8_2afieldS2873;
  _M0L4nameS1146 = _M0L7_2anameS1148;
  goto join_1145;
  goto joinlet_3394;
  join_1145:;
  #line 593 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1119(_M0L14handle__resultS1119, _M0L4nameS1146, (moonbit_string_t)moonbit_string_literal_1.data, 1);
  joinlet_3394:;
  joinlet_3384:;
  return 0;
}

moonbit_string_t _M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__executeN17error__to__stringS1128(
  struct _M0TWRPC15error5ErrorEs* _M0L6_2aenvS2848,
  void* _M0L3errS1129
) {
  void* _M0L1eS1131;
  moonbit_string_t _M0L1eS1133;
  #line 562 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L6_2aenvS2848);
  switch (Moonbit_object_tag(_M0L3errS1129)) {
    case 0: {
      struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS1134 =
        (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L3errS1129;
      moonbit_string_t _M0L8_2afieldS2874 = _M0L10_2aFailureS1134->$0;
      int32_t _M0L6_2acntS3239 =
        Moonbit_object_header(_M0L10_2aFailureS1134)->rc;
      moonbit_string_t _M0L4_2aeS1135;
      if (_M0L6_2acntS3239 > 1) {
        int32_t _M0L11_2anew__cntS3240 = _M0L6_2acntS3239 - 1;
        Moonbit_object_header(_M0L10_2aFailureS1134)->rc
        = _M0L11_2anew__cntS3240;
        moonbit_incref(_M0L8_2afieldS2874);
      } else if (_M0L6_2acntS3239 == 1) {
        #line 563 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L10_2aFailureS1134);
      }
      _M0L4_2aeS1135 = _M0L8_2afieldS2874;
      _M0L1eS1133 = _M0L4_2aeS1135;
      goto join_1132;
      break;
    }
    
    case 1: {
      struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError* _M0L15_2aInspectErrorS1136 =
        (struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L3errS1129;
      moonbit_string_t _M0L8_2afieldS2875 = _M0L15_2aInspectErrorS1136->$0;
      int32_t _M0L6_2acntS3241 =
        Moonbit_object_header(_M0L15_2aInspectErrorS1136)->rc;
      moonbit_string_t _M0L4_2aeS1137;
      if (_M0L6_2acntS3241 > 1) {
        int32_t _M0L11_2anew__cntS3242 = _M0L6_2acntS3241 - 1;
        Moonbit_object_header(_M0L15_2aInspectErrorS1136)->rc
        = _M0L11_2anew__cntS3242;
        moonbit_incref(_M0L8_2afieldS2875);
      } else if (_M0L6_2acntS3241 == 1) {
        #line 563 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L15_2aInspectErrorS1136);
      }
      _M0L4_2aeS1137 = _M0L8_2afieldS2875;
      _M0L1eS1133 = _M0L4_2aeS1137;
      goto join_1132;
      break;
    }
    
    case 3: {
      struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError* _M0L16_2aSnapshotErrorS1138 =
        (struct _M0DTPC15error5Error60moonbitlang_2fcore_2fbuiltin_2eSnapshotError_2eSnapshotError*)_M0L3errS1129;
      moonbit_string_t _M0L8_2afieldS2876 = _M0L16_2aSnapshotErrorS1138->$0;
      int32_t _M0L6_2acntS3243 =
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1138)->rc;
      moonbit_string_t _M0L4_2aeS1139;
      if (_M0L6_2acntS3243 > 1) {
        int32_t _M0L11_2anew__cntS3244 = _M0L6_2acntS3243 - 1;
        Moonbit_object_header(_M0L16_2aSnapshotErrorS1138)->rc
        = _M0L11_2anew__cntS3244;
        moonbit_incref(_M0L8_2afieldS2876);
      } else if (_M0L6_2acntS3243 == 1) {
        #line 563 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L16_2aSnapshotErrorS1138);
      }
      _M0L4_2aeS1139 = _M0L8_2afieldS2876;
      _M0L1eS1133 = _M0L4_2aeS1139;
      goto join_1132;
      break;
    }
    
    case 4: {
      struct _M0DTPC15error5Error114clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError* _M0L35_2aMoonBitTestDriverInternalJsErrorS1140 =
        (struct _M0DTPC15error5Error114clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalJsError_2eMoonBitTestDriverInternalJsError*)_M0L3errS1129;
      moonbit_string_t _M0L8_2afieldS2877 =
        _M0L35_2aMoonBitTestDriverInternalJsErrorS1140->$0;
      int32_t _M0L6_2acntS3245 =
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1140)->rc;
      moonbit_string_t _M0L4_2aeS1141;
      if (_M0L6_2acntS3245 > 1) {
        int32_t _M0L11_2anew__cntS3246 = _M0L6_2acntS3245 - 1;
        Moonbit_object_header(_M0L35_2aMoonBitTestDriverInternalJsErrorS1140)->rc
        = _M0L11_2anew__cntS3246;
        moonbit_incref(_M0L8_2afieldS2877);
      } else if (_M0L6_2acntS3245 == 1) {
        #line 563 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        moonbit_free(_M0L35_2aMoonBitTestDriverInternalJsErrorS1140);
      }
      _M0L4_2aeS1141 = _M0L8_2afieldS2877;
      _M0L1eS1133 = _M0L4_2aeS1141;
      goto join_1132;
      break;
    }
    default: {
      _M0L1eS1131 = _M0L3errS1129;
      goto join_1130;
      break;
    }
  }
  join_1132:;
  return _M0L1eS1133;
  join_1130:;
  #line 568 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  return _M0FP15Error10to__string(_M0L1eS1131);
}

int32_t _M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__executeN14handle__resultS1119(
  struct _M0TWssbEu* _M0L6_2aenvS2834,
  moonbit_string_t _M0L8testnameS1120,
  moonbit_string_t _M0L7messageS1121,
  int32_t _M0L7skippedS1122
) {
  struct _M0R118_24clawteam_2fclawteam_2fconfig__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1119* _M0L14_2acasted__envS2835;
  moonbit_string_t _M0L8_2afieldS2887;
  moonbit_string_t _M0L8filenameS1124;
  int32_t _M0L8_2afieldS2886;
  int32_t _M0L6_2acntS3247;
  int32_t _M0L5indexS1127;
  int32_t _if__result_3397;
  moonbit_string_t _M0L10file__nameS1123;
  moonbit_string_t _M0L10test__nameS1125;
  moonbit_string_t _M0L7messageS1126;
  moonbit_string_t _M0L6_2atmpS2847;
  moonbit_string_t _M0L6_2atmpS2885;
  moonbit_string_t _M0L6_2atmpS2846;
  moonbit_string_t _M0L6_2atmpS2884;
  moonbit_string_t _M0L6_2atmpS2844;
  moonbit_string_t _M0L6_2atmpS2845;
  moonbit_string_t _M0L6_2atmpS2883;
  moonbit_string_t _M0L6_2atmpS2843;
  moonbit_string_t _M0L6_2atmpS2882;
  moonbit_string_t _M0L6_2atmpS2841;
  moonbit_string_t _M0L6_2atmpS2842;
  moonbit_string_t _M0L6_2atmpS2881;
  moonbit_string_t _M0L6_2atmpS2840;
  moonbit_string_t _M0L6_2atmpS2880;
  moonbit_string_t _M0L6_2atmpS2838;
  moonbit_string_t _M0L6_2atmpS2839;
  moonbit_string_t _M0L6_2atmpS2879;
  moonbit_string_t _M0L6_2atmpS2837;
  moonbit_string_t _M0L6_2atmpS2878;
  moonbit_string_t _M0L6_2atmpS2836;
  #line 546 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2835
  = (struct _M0R118_24clawteam_2fclawteam_2fconfig__blackbox__test_2emoonbit__test__driver__internal__do__execute_2ehandle__result_7c1119*)_M0L6_2aenvS2834;
  _M0L8_2afieldS2887 = _M0L14_2acasted__envS2835->$1;
  _M0L8filenameS1124 = _M0L8_2afieldS2887;
  _M0L8_2afieldS2886 = _M0L14_2acasted__envS2835->$0;
  _M0L6_2acntS3247 = Moonbit_object_header(_M0L14_2acasted__envS2835)->rc;
  if (_M0L6_2acntS3247 > 1) {
    int32_t _M0L11_2anew__cntS3248 = _M0L6_2acntS3247 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2835)->rc
    = _M0L11_2anew__cntS3248;
    moonbit_incref(_M0L8filenameS1124);
  } else if (_M0L6_2acntS3247 == 1) {
    #line 546 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2835);
  }
  _M0L5indexS1127 = _M0L8_2afieldS2886;
  if (!_M0L7skippedS1122) {
    _if__result_3397 = 1;
  } else {
    _if__result_3397 = 0;
  }
  if (_if__result_3397) {
    
  }
  #line 552 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L10file__nameS1123 = _M0MPC16string6String6escape(_M0L8filenameS1124);
  #line 553 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__nameS1125 = _M0MPC16string6String6escape(_M0L8testnameS1120);
  #line 554 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L7messageS1126 = _M0MPC16string6String6escape(_M0L7messageS1121);
  #line 555 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_2.data);
  #line 557 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2847
  = _M0IPC16string6StringPB4Show10to__string(_M0L10file__nameS1123);
  #line 556 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2885
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_3.data, _M0L6_2atmpS2847);
  moonbit_decref(_M0L6_2atmpS2847);
  _M0L6_2atmpS2846 = _M0L6_2atmpS2885;
  #line 556 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2884
  = moonbit_add_string(_M0L6_2atmpS2846, (moonbit_string_t)moonbit_string_literal_4.data);
  moonbit_decref(_M0L6_2atmpS2846);
  _M0L6_2atmpS2844 = _M0L6_2atmpS2884;
  #line 557 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2845
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L5indexS1127);
  #line 556 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2883 = moonbit_add_string(_M0L6_2atmpS2844, _M0L6_2atmpS2845);
  moonbit_decref(_M0L6_2atmpS2844);
  moonbit_decref(_M0L6_2atmpS2845);
  _M0L6_2atmpS2843 = _M0L6_2atmpS2883;
  #line 556 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2882
  = moonbit_add_string(_M0L6_2atmpS2843, (moonbit_string_t)moonbit_string_literal_5.data);
  moonbit_decref(_M0L6_2atmpS2843);
  _M0L6_2atmpS2841 = _M0L6_2atmpS2882;
  #line 557 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2842
  = _M0IPC16string6StringPB4Show10to__string(_M0L10test__nameS1125);
  #line 556 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2881 = moonbit_add_string(_M0L6_2atmpS2841, _M0L6_2atmpS2842);
  moonbit_decref(_M0L6_2atmpS2841);
  moonbit_decref(_M0L6_2atmpS2842);
  _M0L6_2atmpS2840 = _M0L6_2atmpS2881;
  #line 556 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2880
  = moonbit_add_string(_M0L6_2atmpS2840, (moonbit_string_t)moonbit_string_literal_6.data);
  moonbit_decref(_M0L6_2atmpS2840);
  _M0L6_2atmpS2838 = _M0L6_2atmpS2880;
  #line 557 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2839
  = _M0IPC16string6StringPB4Show10to__string(_M0L7messageS1126);
  #line 556 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2879 = moonbit_add_string(_M0L6_2atmpS2838, _M0L6_2atmpS2839);
  moonbit_decref(_M0L6_2atmpS2838);
  moonbit_decref(_M0L6_2atmpS2839);
  _M0L6_2atmpS2837 = _M0L6_2atmpS2879;
  #line 556 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2878
  = moonbit_add_string(_M0L6_2atmpS2837, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS2837);
  _M0L6_2atmpS2836 = _M0L6_2atmpS2878;
  #line 556 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE(_M0L6_2atmpS2836);
  #line 559 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0FPB7printlnGsE((moonbit_string_t)moonbit_string_literal_8.data);
  return 0;
}

struct moonbit_result_0 _M0IP38clawteam8clawteam22config__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__test(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1118,
  moonbit_string_t _M0L8filenameS1115,
  int32_t _M0L5indexS1109,
  struct _M0TWssbEu* _M0L14handle__resultS1105,
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1107
) {
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L10index__mapS1085;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS1114;
  struct _M0TWEuQRPC15error5Error* _M0L1fS1087;
  moonbit_string_t* _M0L5attrsS1088;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2abindS1108;
  moonbit_string_t _M0L4nameS1091;
  moonbit_string_t _M0L4nameS1089;
  int32_t _M0L6_2atmpS2833;
  struct _M0TWEOs* _M0L5_2aitS1093;
  struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2830__l440__* _closure_3406;
  struct _M0TWEu* _M0L6_2atmpS2824;
  struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2826__l441__* _closure_3407;
  struct _M0TWRPC15error5ErrorEu* _M0L6_2atmpS2825;
  struct moonbit_result_0 _result_3408;
  #line 420 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1118);
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test48moonbit__test__driver__internal__no__args__tests);
  #line 427 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1114
  = _M0MPB3Map3getGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0FP38clawteam8clawteam22config__blackbox__test48moonbit__test__driver__internal__no__args__tests, _M0L8filenameS1115);
  if (_M0L7_2abindS1114 == 0) {
    struct moonbit_result_0 _result_3399;
    if (_M0L7_2abindS1114) {
      moonbit_decref(_M0L7_2abindS1114);
    }
    moonbit_decref(_M0L17error__to__stringS1107);
    moonbit_decref(_M0L14handle__resultS1105);
    _result_3399.tag = 1;
    _result_3399.data.ok = 0;
    return _result_3399;
  } else {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS1116 =
      _M0L7_2abindS1114;
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L13_2aindex__mapS1117 =
      _M0L7_2aSomeS1116;
    _M0L10index__mapS1085 = _M0L13_2aindex__mapS1117;
    goto join_1084;
  }
  join_1084:;
  #line 429 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1108
  = _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(_M0L10index__mapS1085, _M0L5indexS1109);
  if (_M0L7_2abindS1108 == 0) {
    struct moonbit_result_0 _result_3401;
    if (_M0L7_2abindS1108) {
      moonbit_decref(_M0L7_2abindS1108);
    }
    moonbit_decref(_M0L17error__to__stringS1107);
    moonbit_decref(_M0L14handle__resultS1105);
    _result_3401.tag = 1;
    _result_3401.data.ok = 0;
    return _result_3401;
  } else {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L7_2aSomeS1110 =
      _M0L7_2abindS1108;
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L4_2axS1111 = _M0L7_2aSomeS1110;
    struct _M0TWEuQRPC15error5Error* _M0L8_2afieldS2891 = _M0L4_2axS1111->$0;
    struct _M0TWEuQRPC15error5Error* _M0L4_2afS1112 = _M0L8_2afieldS2891;
    moonbit_string_t* _M0L8_2afieldS2890 = _M0L4_2axS1111->$1;
    int32_t _M0L6_2acntS3249 = Moonbit_object_header(_M0L4_2axS1111)->rc;
    moonbit_string_t* _M0L8_2aattrsS1113;
    if (_M0L6_2acntS3249 > 1) {
      int32_t _M0L11_2anew__cntS3250 = _M0L6_2acntS3249 - 1;
      Moonbit_object_header(_M0L4_2axS1111)->rc = _M0L11_2anew__cntS3250;
      moonbit_incref(_M0L8_2afieldS2890);
      moonbit_incref(_M0L4_2afS1112);
    } else if (_M0L6_2acntS3249 == 1) {
      #line 427 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      moonbit_free(_M0L4_2axS1111);
    }
    _M0L8_2aattrsS1113 = _M0L8_2afieldS2890;
    _M0L1fS1087 = _M0L4_2afS1112;
    _M0L5attrsS1088 = _M0L8_2aattrsS1113;
    goto join_1086;
  }
  join_1086:;
  _M0L6_2atmpS2833 = Moonbit_array_length(_M0L5attrsS1088);
  if (_M0L6_2atmpS2833 >= 1) {
    moonbit_string_t _M0L6_2atmpS2889 = (moonbit_string_t)_M0L5attrsS1088[0];
    moonbit_string_t _M0L7_2anameS1092 = _M0L6_2atmpS2889;
    moonbit_incref(_M0L7_2anameS1092);
    _M0L4nameS1091 = _M0L7_2anameS1092;
    goto join_1090;
  } else {
    _M0L4nameS1089 = (moonbit_string_t)moonbit_string_literal_0.data;
  }
  goto joinlet_3402;
  join_1090:;
  _M0L4nameS1089 = _M0L4nameS1091;
  joinlet_3402:;
  #line 430 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L5_2aitS1093 = _M0MPC15array13ReadOnlyArray4iterGsE(_M0L5attrsS1088);
  while (1) {
    moonbit_string_t _M0L4attrS1095;
    moonbit_string_t _M0L7_2abindS1102;
    int32_t _M0L6_2atmpS2817;
    int64_t _M0L6_2atmpS2816;
    moonbit_incref(_M0L5_2aitS1093);
    #line 432 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
    _M0L7_2abindS1102 = _M0MPB4Iter4nextGsE(_M0L5_2aitS1093);
    if (_M0L7_2abindS1102 == 0) {
      if (_M0L7_2abindS1102) {
        moonbit_decref(_M0L7_2abindS1102);
      }
      moonbit_decref(_M0L5_2aitS1093);
    } else {
      moonbit_string_t _M0L7_2aSomeS1103 = _M0L7_2abindS1102;
      moonbit_string_t _M0L7_2aattrS1104 = _M0L7_2aSomeS1103;
      _M0L4attrS1095 = _M0L7_2aattrS1104;
      goto join_1094;
    }
    goto joinlet_3404;
    join_1094:;
    _M0L6_2atmpS2817 = Moonbit_array_length(_M0L4attrS1095);
    _M0L6_2atmpS2816 = (int64_t)_M0L6_2atmpS2817;
    moonbit_incref(_M0L4attrS1095);
    #line 433 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
    if (
      _M0MPC16string6String24char__length__ge_2einner(_M0L4attrS1095, 5, 0, _M0L6_2atmpS2816)
    ) {
      int32_t _M0L6_2atmpS2823 = _M0L4attrS1095[0];
      int32_t _M0L4_2axS1096 = _M0L6_2atmpS2823;
      if (_M0L4_2axS1096 == 112) {
        int32_t _M0L6_2atmpS2822 = _M0L4attrS1095[1];
        int32_t _M0L4_2axS1097 = _M0L6_2atmpS2822;
        if (_M0L4_2axS1097 == 97) {
          int32_t _M0L6_2atmpS2821 = _M0L4attrS1095[2];
          int32_t _M0L4_2axS1098 = _M0L6_2atmpS2821;
          if (_M0L4_2axS1098 == 110) {
            int32_t _M0L6_2atmpS2820 = _M0L4attrS1095[3];
            int32_t _M0L4_2axS1099 = _M0L6_2atmpS2820;
            if (_M0L4_2axS1099 == 105) {
              int32_t _M0L6_2atmpS2888 = _M0L4attrS1095[4];
              int32_t _M0L6_2atmpS2819;
              int32_t _M0L4_2axS1100;
              moonbit_decref(_M0L4attrS1095);
              _M0L6_2atmpS2819 = _M0L6_2atmpS2888;
              _M0L4_2axS1100 = _M0L6_2atmpS2819;
              if (_M0L4_2axS1100 == 99) {
                void* _M0L116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2818;
                struct moonbit_result_0 _result_3405;
                moonbit_decref(_M0L17error__to__stringS1107);
                moonbit_decref(_M0L14handle__resultS1105);
                moonbit_decref(_M0L5_2aitS1093);
                moonbit_decref(_M0L1fS1087);
                _M0L116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2818
                = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest));
                Moonbit_object_header(_M0L116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2818)->meta
                = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest, $0) >> 2, 1, 2);
                ((struct _M0DTPC15error5Error116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTest*)_M0L116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2818)->$0
                = _M0L4nameS1089;
                _result_3405.tag = 0;
                _result_3405.data.err
                = _M0L116clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBitTestDriverInternalSkipTest_2eMoonBitTestDriverInternalSkipTestS2818;
                return _result_3405;
              }
            } else {
              moonbit_decref(_M0L4attrS1095);
            }
          } else {
            moonbit_decref(_M0L4attrS1095);
          }
        } else {
          moonbit_decref(_M0L4attrS1095);
        }
      } else {
        moonbit_decref(_M0L4attrS1095);
      }
    } else {
      moonbit_decref(_M0L4attrS1095);
    }
    continue;
    joinlet_3404:;
    break;
  }
  moonbit_incref(_M0L14handle__resultS1105);
  moonbit_incref(_M0L4nameS1089);
  _closure_3406
  = (struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2830__l440__*)moonbit_malloc(sizeof(struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2830__l440__));
  Moonbit_object_header(_closure_3406)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2830__l440__, $0) >> 2, 2, 0);
  _closure_3406->code
  = &_M0IP38clawteam8clawteam22config__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testC2830l440;
  _closure_3406->$0 = _M0L14handle__resultS1105;
  _closure_3406->$1 = _M0L4nameS1089;
  _M0L6_2atmpS2824 = (struct _M0TWEu*)_closure_3406;
  _closure_3407
  = (struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2826__l441__*)moonbit_malloc(sizeof(struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2826__l441__));
  Moonbit_object_header(_closure_3407)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2826__l441__, $0) >> 2, 3, 0);
  _closure_3407->code
  = &_M0IP38clawteam8clawteam22config__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testC2826l441;
  _closure_3407->$0 = _M0L17error__to__stringS1107;
  _closure_3407->$1 = _M0L14handle__resultS1105;
  _closure_3407->$2 = _M0L4nameS1089;
  _M0L6_2atmpS2825 = (struct _M0TWRPC15error5ErrorEu*)_closure_3407;
  #line 438 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0FP38clawteam8clawteam22config__blackbox__test45moonbit__test__driver__internal__catch__error(_M0L1fS1087, _M0L6_2atmpS2824, _M0L6_2atmpS2825);
  _result_3408.tag = 1;
  _result_3408.data.ok = 1;
  return _result_3408;
}

int32_t _M0IP38clawteam8clawteam22config__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testC2830l440(
  struct _M0TWEu* _M0L6_2aenvS2831
) {
  struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2830__l440__* _M0L14_2acasted__envS2832;
  moonbit_string_t _M0L8_2afieldS2893;
  moonbit_string_t _M0L4nameS1089;
  struct _M0TWssbEu* _M0L8_2afieldS2892;
  int32_t _M0L6_2acntS3251;
  struct _M0TWssbEu* _M0L14handle__resultS1105;
  #line 440 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2832
  = (struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2830__l440__*)_M0L6_2aenvS2831;
  _M0L8_2afieldS2893 = _M0L14_2acasted__envS2832->$1;
  _M0L4nameS1089 = _M0L8_2afieldS2893;
  _M0L8_2afieldS2892 = _M0L14_2acasted__envS2832->$0;
  _M0L6_2acntS3251 = Moonbit_object_header(_M0L14_2acasted__envS2832)->rc;
  if (_M0L6_2acntS3251 > 1) {
    int32_t _M0L11_2anew__cntS3252 = _M0L6_2acntS3251 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2832)->rc
    = _M0L11_2anew__cntS3252;
    moonbit_incref(_M0L4nameS1089);
    moonbit_incref(_M0L8_2afieldS2892);
  } else if (_M0L6_2acntS3251 == 1) {
    #line 440 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2832);
  }
  _M0L14handle__resultS1105 = _M0L8_2afieldS2892;
  #line 440 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1105->code(_M0L14handle__resultS1105, _M0L4nameS1089, (moonbit_string_t)moonbit_string_literal_0.data, 0);
  return 0;
}

int32_t _M0IP38clawteam8clawteam22config__blackbox__test41MoonBit__Test__Driver__Internal__No__ArgsP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testC2826l441(
  struct _M0TWRPC15error5ErrorEu* _M0L6_2aenvS2827,
  void* _M0L3errS1106
) {
  struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2826__l441__* _M0L14_2acasted__envS2828;
  moonbit_string_t _M0L8_2afieldS2896;
  moonbit_string_t _M0L4nameS1089;
  struct _M0TWssbEu* _M0L8_2afieldS2895;
  struct _M0TWssbEu* _M0L14handle__resultS1105;
  struct _M0TWRPC15error5ErrorEs* _M0L8_2afieldS2894;
  int32_t _M0L6_2acntS3253;
  struct _M0TWRPC15error5ErrorEs* _M0L17error__to__stringS1107;
  moonbit_string_t _M0L6_2atmpS2829;
  #line 441 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L14_2acasted__envS2828
  = (struct _M0R205_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver_3a_3arun__test_7c_40clawteam_2fclawteam_2fconfig__blackbox__test_2eMoonBit__Test__Driver__Internal__No__Args_7c_2eanon__u2826__l441__*)_M0L6_2aenvS2827;
  _M0L8_2afieldS2896 = _M0L14_2acasted__envS2828->$2;
  _M0L4nameS1089 = _M0L8_2afieldS2896;
  _M0L8_2afieldS2895 = _M0L14_2acasted__envS2828->$1;
  _M0L14handle__resultS1105 = _M0L8_2afieldS2895;
  _M0L8_2afieldS2894 = _M0L14_2acasted__envS2828->$0;
  _M0L6_2acntS3253 = Moonbit_object_header(_M0L14_2acasted__envS2828)->rc;
  if (_M0L6_2acntS3253 > 1) {
    int32_t _M0L11_2anew__cntS3254 = _M0L6_2acntS3253 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS2828)->rc
    = _M0L11_2anew__cntS3254;
    moonbit_incref(_M0L4nameS1089);
    moonbit_incref(_M0L14handle__resultS1105);
    moonbit_incref(_M0L8_2afieldS2894);
  } else if (_M0L6_2acntS3253 == 1) {
    #line 441 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
    moonbit_free(_M0L14_2acasted__envS2828);
  }
  _M0L17error__to__stringS1107 = _M0L8_2afieldS2894;
  #line 441 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2829
  = _M0L17error__to__stringS1107->code(_M0L17error__to__stringS1107, _M0L3errS1106);
  #line 441 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L14handle__resultS1105->code(_M0L14handle__resultS1105, _M0L4nameS1089, _M0L6_2atmpS2829, 0);
  return 0;
}

int32_t _M0FP38clawteam8clawteam22config__blackbox__test45moonbit__test__driver__internal__catch__error(
  struct _M0TWEuQRPC15error5Error* _M0L1fS1078,
  struct _M0TWEu* _M0L6on__okS1079,
  struct _M0TWRPC15error5ErrorEu* _M0L7on__errS1076
) {
  void* _M0L11_2atry__errS1074;
  struct moonbit_result_0 _tmp_3410;
  void* _M0L3errS1075;
  #line 375 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  #line 382 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _tmp_3410 = _M0L1fS1078->code(_M0L1fS1078);
  if (_tmp_3410.tag) {
    int32_t const _M0L5_2aokS2814 = _tmp_3410.data.ok;
    moonbit_decref(_M0L7on__errS1076);
  } else {
    void* const _M0L6_2aerrS2815 = _tmp_3410.data.err;
    moonbit_decref(_M0L6on__okS1079);
    _M0L11_2atry__errS1074 = _M0L6_2aerrS2815;
    goto join_1073;
  }
  #line 382 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6on__okS1079->code(_M0L6on__okS1079);
  goto joinlet_3409;
  join_1073:;
  _M0L3errS1075 = _M0L11_2atry__errS1074;
  #line 383 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L7on__errS1076->code(_M0L7on__errS1076, _M0L3errS1075);
  joinlet_3409:;
  return 0;
}

struct _M0TPB5ArrayGUsiEE* _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__args(
  
) {
  int32_t _M0L45moonbit__test__driver__internal__parse__int__S1033;
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1039;
  int32_t _M0L57moonbit__test__driver__internal__get__cli__args__internalS1046;
  int32_t _M0L51moonbit__test__driver__internal__split__mbt__stringS1051;
  struct _M0TUsiE** _M0L6_2atmpS2813;
  struct _M0TPB5ArrayGUsiEE* _M0L16file__and__indexS1058;
  struct _M0TPB5ArrayGsE* _M0L9cli__argsS1059;
  moonbit_string_t _M0L6_2atmpS2812;
  struct _M0TPB5ArrayGsE* _M0L10test__argsS1060;
  int32_t _M0L7_2abindS1061;
  int32_t _M0L2__S1062;
  #line 193 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L45moonbit__test__driver__internal__parse__int__S1033 = 0;
  _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1039
  = 0;
  _M0L57moonbit__test__driver__internal__get__cli__args__internalS1046
  = _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1039;
  _M0L51moonbit__test__driver__internal__split__mbt__stringS1051 = 0;
  _M0L6_2atmpS2813 = (struct _M0TUsiE**)moonbit_empty_ref_array;
  _M0L16file__and__indexS1058
  = (struct _M0TPB5ArrayGUsiEE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGUsiEE));
  Moonbit_object_header(_M0L16file__and__indexS1058)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGUsiEE, $0) >> 2, 1, 0);
  _M0L16file__and__indexS1058->$0 = _M0L6_2atmpS2813;
  _M0L16file__and__indexS1058->$1 = 0;
  #line 282 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L9cli__argsS1059
  = _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1046(_M0L57moonbit__test__driver__internal__get__cli__args__internalS1046);
  #line 284 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2812 = _M0MPC15array5Array2atGsE(_M0L9cli__argsS1059, 1);
  #line 283 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L10test__argsS1060
  = _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1051(_M0L51moonbit__test__driver__internal__split__mbt__stringS1051, _M0L6_2atmpS2812, 47);
  _M0L7_2abindS1061 = _M0L10test__argsS1060->$1;
  _M0L2__S1062 = 0;
  while (1) {
    if (_M0L2__S1062 < _M0L7_2abindS1061) {
      moonbit_string_t* _M0L8_2afieldS2898 = _M0L10test__argsS1060->$0;
      moonbit_string_t* _M0L3bufS2811 = _M0L8_2afieldS2898;
      moonbit_string_t _M0L6_2atmpS2897 =
        (moonbit_string_t)_M0L3bufS2811[_M0L2__S1062];
      moonbit_string_t _M0L3argS1063 = _M0L6_2atmpS2897;
      struct _M0TPB5ArrayGsE* _M0L16file__and__rangeS1064;
      moonbit_string_t _M0L4fileS1065;
      moonbit_string_t _M0L5rangeS1066;
      struct _M0TPB5ArrayGsE* _M0L15start__and__endS1067;
      moonbit_string_t _M0L6_2atmpS2809;
      int32_t _M0L5startS1068;
      moonbit_string_t _M0L6_2atmpS2808;
      int32_t _M0L3endS1069;
      int32_t _M0L1iS1070;
      int32_t _M0L6_2atmpS2810;
      moonbit_incref(_M0L3argS1063);
      #line 288 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _M0L16file__and__rangeS1064
      = _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1051(_M0L51moonbit__test__driver__internal__split__mbt__stringS1051, _M0L3argS1063, 58);
      moonbit_incref(_M0L16file__and__rangeS1064);
      #line 289 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _M0L4fileS1065
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1064, 0);
      #line 290 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _M0L5rangeS1066
      = _M0MPC15array5Array2atGsE(_M0L16file__and__rangeS1064, 1);
      #line 291 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _M0L15start__and__endS1067
      = _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1051(_M0L51moonbit__test__driver__internal__split__mbt__stringS1051, _M0L5rangeS1066, 45);
      moonbit_incref(_M0L15start__and__endS1067);
      #line 294 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2809
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1067, 0);
      #line 294 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _M0L5startS1068
      = _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1033(_M0L45moonbit__test__driver__internal__parse__int__S1033, _M0L6_2atmpS2809);
      #line 295 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2808
      = _M0MPC15array5Array2atGsE(_M0L15start__and__endS1067, 1);
      #line 295 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _M0L3endS1069
      = _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1033(_M0L45moonbit__test__driver__internal__parse__int__S1033, _M0L6_2atmpS2808);
      _M0L1iS1070 = _M0L5startS1068;
      while (1) {
        if (_M0L1iS1070 < _M0L3endS1069) {
          struct _M0TUsiE* _M0L8_2atupleS2806;
          int32_t _M0L6_2atmpS2807;
          moonbit_incref(_M0L4fileS1065);
          _M0L8_2atupleS2806
          = (struct _M0TUsiE*)moonbit_malloc(sizeof(struct _M0TUsiE));
          Moonbit_object_header(_M0L8_2atupleS2806)->meta
          = Moonbit_make_regular_object_header(offsetof(struct _M0TUsiE, $0) >> 2, 1, 0);
          _M0L8_2atupleS2806->$0 = _M0L4fileS1065;
          _M0L8_2atupleS2806->$1 = _M0L1iS1070;
          moonbit_incref(_M0L16file__and__indexS1058);
          #line 297 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
          _M0MPC15array5Array4pushGUsiEE(_M0L16file__and__indexS1058, _M0L8_2atupleS2806);
          _M0L6_2atmpS2807 = _M0L1iS1070 + 1;
          _M0L1iS1070 = _M0L6_2atmpS2807;
          continue;
        } else {
          moonbit_decref(_M0L4fileS1065);
        }
        break;
      }
      _M0L6_2atmpS2810 = _M0L2__S1062 + 1;
      _M0L2__S1062 = _M0L6_2atmpS2810;
      continue;
    } else {
      moonbit_decref(_M0L10test__argsS1060);
    }
    break;
  }
  return _M0L16file__and__indexS1058;
}

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN51moonbit__test__driver__internal__split__mbt__stringS1051(
  int32_t _M0L6_2aenvS2787,
  moonbit_string_t _M0L1sS1052,
  int32_t _M0L3sepS1053
) {
  moonbit_string_t* _M0L6_2atmpS2805;
  struct _M0TPB5ArrayGsE* _M0L3resS1054;
  struct _M0TPC13ref3RefGiE* _M0L1iS1055;
  struct _M0TPC13ref3RefGiE* _M0L5startS1056;
  int32_t _M0L3valS2800;
  int32_t _M0L6_2atmpS2801;
  #line 261 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS2805 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L3resS1054
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L3resS1054)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L3resS1054->$0 = _M0L6_2atmpS2805;
  _M0L3resS1054->$1 = 0;
  _M0L1iS1055
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1055)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1055->$0 = 0;
  _M0L5startS1056
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L5startS1056)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L5startS1056->$0 = 0;
  while (1) {
    int32_t _M0L3valS2788 = _M0L1iS1055->$0;
    int32_t _M0L6_2atmpS2789 = Moonbit_array_length(_M0L1sS1052);
    if (_M0L3valS2788 < _M0L6_2atmpS2789) {
      int32_t _M0L3valS2792 = _M0L1iS1055->$0;
      int32_t _M0L6_2atmpS2791;
      int32_t _M0L6_2atmpS2790;
      int32_t _M0L3valS2799;
      int32_t _M0L6_2atmpS2798;
      if (
        _M0L3valS2792 < 0
        || _M0L3valS2792 >= Moonbit_array_length(_M0L1sS1052)
      ) {
        #line 269 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2791 = _M0L1sS1052[_M0L3valS2792];
      _M0L6_2atmpS2790 = _M0L6_2atmpS2791;
      if (_M0L6_2atmpS2790 == _M0L3sepS1053) {
        int32_t _M0L3valS2794 = _M0L5startS1056->$0;
        int32_t _M0L3valS2795 = _M0L1iS1055->$0;
        moonbit_string_t _M0L6_2atmpS2793;
        int32_t _M0L3valS2797;
        int32_t _M0L6_2atmpS2796;
        moonbit_incref(_M0L1sS1052);
        #line 270 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        _M0L6_2atmpS2793
        = _M0MPC16string6String17unsafe__substring(_M0L1sS1052, _M0L3valS2794, _M0L3valS2795);
        moonbit_incref(_M0L3resS1054);
        #line 270 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        _M0MPC15array5Array4pushGsE(_M0L3resS1054, _M0L6_2atmpS2793);
        _M0L3valS2797 = _M0L1iS1055->$0;
        _M0L6_2atmpS2796 = _M0L3valS2797 + 1;
        _M0L5startS1056->$0 = _M0L6_2atmpS2796;
      }
      _M0L3valS2799 = _M0L1iS1055->$0;
      _M0L6_2atmpS2798 = _M0L3valS2799 + 1;
      _M0L1iS1055->$0 = _M0L6_2atmpS2798;
      continue;
    } else {
      moonbit_decref(_M0L1iS1055);
    }
    break;
  }
  _M0L3valS2800 = _M0L5startS1056->$0;
  _M0L6_2atmpS2801 = Moonbit_array_length(_M0L1sS1052);
  if (_M0L3valS2800 < _M0L6_2atmpS2801) {
    int32_t _M0L8_2afieldS2899 = _M0L5startS1056->$0;
    int32_t _M0L3valS2803;
    int32_t _M0L6_2atmpS2804;
    moonbit_string_t _M0L6_2atmpS2802;
    moonbit_decref(_M0L5startS1056);
    _M0L3valS2803 = _M0L8_2afieldS2899;
    _M0L6_2atmpS2804 = Moonbit_array_length(_M0L1sS1052);
    #line 276 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
    _M0L6_2atmpS2802
    = _M0MPC16string6String17unsafe__substring(_M0L1sS1052, _M0L3valS2803, _M0L6_2atmpS2804);
    moonbit_incref(_M0L3resS1054);
    #line 276 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
    _M0MPC15array5Array4pushGsE(_M0L3resS1054, _M0L6_2atmpS2802);
  } else {
    moonbit_decref(_M0L5startS1056);
    moonbit_decref(_M0L1sS1052);
  }
  return _M0L3resS1054;
}

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN57moonbit__test__driver__internal__get__cli__args__internalS1046(
  int32_t _M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1039
) {
  moonbit_bytes_t* _M0L3tmpS1047;
  int32_t _M0L6_2atmpS2786;
  struct _M0TPB5ArrayGsE* _M0L3resS1048;
  int32_t _M0L1iS1049;
  #line 250 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  #line 253 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L3tmpS1047
  = _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__get__cli__args__ffi();
  _M0L6_2atmpS2786 = Moonbit_array_length(_M0L3tmpS1047);
  #line 254 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1048 = _M0MPC15array5Array11new_2einnerGsE(_M0L6_2atmpS2786);
  _M0L1iS1049 = 0;
  while (1) {
    int32_t _M0L6_2atmpS2782 = Moonbit_array_length(_M0L3tmpS1047);
    if (_M0L1iS1049 < _M0L6_2atmpS2782) {
      moonbit_bytes_t _M0L6_2atmpS2900;
      moonbit_bytes_t _M0L6_2atmpS2784;
      moonbit_string_t _M0L6_2atmpS2783;
      int32_t _M0L6_2atmpS2785;
      if (
        _M0L1iS1049 < 0 || _M0L1iS1049 >= Moonbit_array_length(_M0L3tmpS1047)
      ) {
        #line 256 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2900 = (moonbit_bytes_t)_M0L3tmpS1047[_M0L1iS1049];
      _M0L6_2atmpS2784 = _M0L6_2atmpS2900;
      moonbit_incref(_M0L6_2atmpS2784);
      #line 256 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _M0L6_2atmpS2783
      = _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1039(_M0L61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1039, _M0L6_2atmpS2784);
      moonbit_incref(_M0L3resS1048);
      #line 256 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _M0MPC15array5Array4pushGsE(_M0L3resS1048, _M0L6_2atmpS2783);
      _M0L6_2atmpS2785 = _M0L1iS1049 + 1;
      _M0L1iS1049 = _M0L6_2atmpS2785;
      continue;
    } else {
      moonbit_decref(_M0L3tmpS1047);
    }
    break;
  }
  return _M0L3resS1048;
}

moonbit_string_t _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN61moonbit__test__driver__internal__utf8__bytes__to__mbt__stringS1039(
  int32_t _M0L6_2aenvS2696,
  moonbit_bytes_t _M0L5bytesS1040
) {
  struct _M0TPB13StringBuilder* _M0L3resS1041;
  int32_t _M0L3lenS1042;
  struct _M0TPC13ref3RefGiE* _M0L1iS1043;
  #line 206 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  #line 209 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1041 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L3lenS1042 = Moonbit_array_length(_M0L5bytesS1040);
  _M0L1iS1043
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS1043)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS1043->$0 = 0;
  while (1) {
    int32_t _M0L3valS2697 = _M0L1iS1043->$0;
    if (_M0L3valS2697 < _M0L3lenS1042) {
      int32_t _M0L3valS2781 = _M0L1iS1043->$0;
      int32_t _M0L6_2atmpS2780;
      int32_t _M0L6_2atmpS2779;
      struct _M0TPC13ref3RefGiE* _M0L1cS1044;
      int32_t _M0L3valS2698;
      if (
        _M0L3valS2781 < 0
        || _M0L3valS2781 >= Moonbit_array_length(_M0L5bytesS1040)
      ) {
        #line 213 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2780 = _M0L5bytesS1040[_M0L3valS2781];
      _M0L6_2atmpS2779 = (int32_t)_M0L6_2atmpS2780;
      _M0L1cS1044
      = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
      Moonbit_object_header(_M0L1cS1044)->meta
      = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
      _M0L1cS1044->$0 = _M0L6_2atmpS2779;
      _M0L3valS2698 = _M0L1cS1044->$0;
      if (_M0L3valS2698 < 128) {
        int32_t _M0L8_2afieldS2901 = _M0L1cS1044->$0;
        int32_t _M0L3valS2700;
        int32_t _M0L6_2atmpS2699;
        int32_t _M0L3valS2702;
        int32_t _M0L6_2atmpS2701;
        moonbit_decref(_M0L1cS1044);
        _M0L3valS2700 = _M0L8_2afieldS2901;
        _M0L6_2atmpS2699 = _M0L3valS2700;
        moonbit_incref(_M0L3resS1041);
        #line 215 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1041, _M0L6_2atmpS2699);
        _M0L3valS2702 = _M0L1iS1043->$0;
        _M0L6_2atmpS2701 = _M0L3valS2702 + 1;
        _M0L1iS1043->$0 = _M0L6_2atmpS2701;
      } else {
        int32_t _M0L3valS2703 = _M0L1cS1044->$0;
        if (_M0L3valS2703 < 224) {
          int32_t _M0L3valS2705 = _M0L1iS1043->$0;
          int32_t _M0L6_2atmpS2704 = _M0L3valS2705 + 1;
          int32_t _M0L3valS2714;
          int32_t _M0L6_2atmpS2713;
          int32_t _M0L6_2atmpS2707;
          int32_t _M0L3valS2712;
          int32_t _M0L6_2atmpS2711;
          int32_t _M0L6_2atmpS2710;
          int32_t _M0L6_2atmpS2709;
          int32_t _M0L6_2atmpS2708;
          int32_t _M0L6_2atmpS2706;
          int32_t _M0L8_2afieldS2902;
          int32_t _M0L3valS2716;
          int32_t _M0L6_2atmpS2715;
          int32_t _M0L3valS2718;
          int32_t _M0L6_2atmpS2717;
          if (_M0L6_2atmpS2704 >= _M0L3lenS1042) {
            moonbit_decref(_M0L1cS1044);
            moonbit_decref(_M0L1iS1043);
            moonbit_decref(_M0L5bytesS1040);
            break;
          }
          _M0L3valS2714 = _M0L1cS1044->$0;
          _M0L6_2atmpS2713 = _M0L3valS2714 & 31;
          _M0L6_2atmpS2707 = _M0L6_2atmpS2713 << 6;
          _M0L3valS2712 = _M0L1iS1043->$0;
          _M0L6_2atmpS2711 = _M0L3valS2712 + 1;
          if (
            _M0L6_2atmpS2711 < 0
            || _M0L6_2atmpS2711 >= Moonbit_array_length(_M0L5bytesS1040)
          ) {
            #line 221 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS2710 = _M0L5bytesS1040[_M0L6_2atmpS2711];
          _M0L6_2atmpS2709 = (int32_t)_M0L6_2atmpS2710;
          _M0L6_2atmpS2708 = _M0L6_2atmpS2709 & 63;
          _M0L6_2atmpS2706 = _M0L6_2atmpS2707 | _M0L6_2atmpS2708;
          _M0L1cS1044->$0 = _M0L6_2atmpS2706;
          _M0L8_2afieldS2902 = _M0L1cS1044->$0;
          moonbit_decref(_M0L1cS1044);
          _M0L3valS2716 = _M0L8_2afieldS2902;
          _M0L6_2atmpS2715 = _M0L3valS2716;
          moonbit_incref(_M0L3resS1041);
          #line 222 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
          _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1041, _M0L6_2atmpS2715);
          _M0L3valS2718 = _M0L1iS1043->$0;
          _M0L6_2atmpS2717 = _M0L3valS2718 + 2;
          _M0L1iS1043->$0 = _M0L6_2atmpS2717;
        } else {
          int32_t _M0L3valS2719 = _M0L1cS1044->$0;
          if (_M0L3valS2719 < 240) {
            int32_t _M0L3valS2721 = _M0L1iS1043->$0;
            int32_t _M0L6_2atmpS2720 = _M0L3valS2721 + 2;
            int32_t _M0L3valS2737;
            int32_t _M0L6_2atmpS2736;
            int32_t _M0L6_2atmpS2729;
            int32_t _M0L3valS2735;
            int32_t _M0L6_2atmpS2734;
            int32_t _M0L6_2atmpS2733;
            int32_t _M0L6_2atmpS2732;
            int32_t _M0L6_2atmpS2731;
            int32_t _M0L6_2atmpS2730;
            int32_t _M0L6_2atmpS2723;
            int32_t _M0L3valS2728;
            int32_t _M0L6_2atmpS2727;
            int32_t _M0L6_2atmpS2726;
            int32_t _M0L6_2atmpS2725;
            int32_t _M0L6_2atmpS2724;
            int32_t _M0L6_2atmpS2722;
            int32_t _M0L8_2afieldS2903;
            int32_t _M0L3valS2739;
            int32_t _M0L6_2atmpS2738;
            int32_t _M0L3valS2741;
            int32_t _M0L6_2atmpS2740;
            if (_M0L6_2atmpS2720 >= _M0L3lenS1042) {
              moonbit_decref(_M0L1cS1044);
              moonbit_decref(_M0L1iS1043);
              moonbit_decref(_M0L5bytesS1040);
              break;
            }
            _M0L3valS2737 = _M0L1cS1044->$0;
            _M0L6_2atmpS2736 = _M0L3valS2737 & 15;
            _M0L6_2atmpS2729 = _M0L6_2atmpS2736 << 12;
            _M0L3valS2735 = _M0L1iS1043->$0;
            _M0L6_2atmpS2734 = _M0L3valS2735 + 1;
            if (
              _M0L6_2atmpS2734 < 0
              || _M0L6_2atmpS2734 >= Moonbit_array_length(_M0L5bytesS1040)
            ) {
              #line 229 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2733 = _M0L5bytesS1040[_M0L6_2atmpS2734];
            _M0L6_2atmpS2732 = (int32_t)_M0L6_2atmpS2733;
            _M0L6_2atmpS2731 = _M0L6_2atmpS2732 & 63;
            _M0L6_2atmpS2730 = _M0L6_2atmpS2731 << 6;
            _M0L6_2atmpS2723 = _M0L6_2atmpS2729 | _M0L6_2atmpS2730;
            _M0L3valS2728 = _M0L1iS1043->$0;
            _M0L6_2atmpS2727 = _M0L3valS2728 + 2;
            if (
              _M0L6_2atmpS2727 < 0
              || _M0L6_2atmpS2727 >= Moonbit_array_length(_M0L5bytesS1040)
            ) {
              #line 230 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2726 = _M0L5bytesS1040[_M0L6_2atmpS2727];
            _M0L6_2atmpS2725 = (int32_t)_M0L6_2atmpS2726;
            _M0L6_2atmpS2724 = _M0L6_2atmpS2725 & 63;
            _M0L6_2atmpS2722 = _M0L6_2atmpS2723 | _M0L6_2atmpS2724;
            _M0L1cS1044->$0 = _M0L6_2atmpS2722;
            _M0L8_2afieldS2903 = _M0L1cS1044->$0;
            moonbit_decref(_M0L1cS1044);
            _M0L3valS2739 = _M0L8_2afieldS2903;
            _M0L6_2atmpS2738 = _M0L3valS2739;
            moonbit_incref(_M0L3resS1041);
            #line 231 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1041, _M0L6_2atmpS2738);
            _M0L3valS2741 = _M0L1iS1043->$0;
            _M0L6_2atmpS2740 = _M0L3valS2741 + 3;
            _M0L1iS1043->$0 = _M0L6_2atmpS2740;
          } else {
            int32_t _M0L3valS2743 = _M0L1iS1043->$0;
            int32_t _M0L6_2atmpS2742 = _M0L3valS2743 + 3;
            int32_t _M0L3valS2766;
            int32_t _M0L6_2atmpS2765;
            int32_t _M0L6_2atmpS2758;
            int32_t _M0L3valS2764;
            int32_t _M0L6_2atmpS2763;
            int32_t _M0L6_2atmpS2762;
            int32_t _M0L6_2atmpS2761;
            int32_t _M0L6_2atmpS2760;
            int32_t _M0L6_2atmpS2759;
            int32_t _M0L6_2atmpS2751;
            int32_t _M0L3valS2757;
            int32_t _M0L6_2atmpS2756;
            int32_t _M0L6_2atmpS2755;
            int32_t _M0L6_2atmpS2754;
            int32_t _M0L6_2atmpS2753;
            int32_t _M0L6_2atmpS2752;
            int32_t _M0L6_2atmpS2745;
            int32_t _M0L3valS2750;
            int32_t _M0L6_2atmpS2749;
            int32_t _M0L6_2atmpS2748;
            int32_t _M0L6_2atmpS2747;
            int32_t _M0L6_2atmpS2746;
            int32_t _M0L6_2atmpS2744;
            int32_t _M0L3valS2768;
            int32_t _M0L6_2atmpS2767;
            int32_t _M0L3valS2772;
            int32_t _M0L6_2atmpS2771;
            int32_t _M0L6_2atmpS2770;
            int32_t _M0L6_2atmpS2769;
            int32_t _M0L8_2afieldS2904;
            int32_t _M0L3valS2776;
            int32_t _M0L6_2atmpS2775;
            int32_t _M0L6_2atmpS2774;
            int32_t _M0L6_2atmpS2773;
            int32_t _M0L3valS2778;
            int32_t _M0L6_2atmpS2777;
            if (_M0L6_2atmpS2742 >= _M0L3lenS1042) {
              moonbit_decref(_M0L1cS1044);
              moonbit_decref(_M0L1iS1043);
              moonbit_decref(_M0L5bytesS1040);
              break;
            }
            _M0L3valS2766 = _M0L1cS1044->$0;
            _M0L6_2atmpS2765 = _M0L3valS2766 & 7;
            _M0L6_2atmpS2758 = _M0L6_2atmpS2765 << 18;
            _M0L3valS2764 = _M0L1iS1043->$0;
            _M0L6_2atmpS2763 = _M0L3valS2764 + 1;
            if (
              _M0L6_2atmpS2763 < 0
              || _M0L6_2atmpS2763 >= Moonbit_array_length(_M0L5bytesS1040)
            ) {
              #line 238 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2762 = _M0L5bytesS1040[_M0L6_2atmpS2763];
            _M0L6_2atmpS2761 = (int32_t)_M0L6_2atmpS2762;
            _M0L6_2atmpS2760 = _M0L6_2atmpS2761 & 63;
            _M0L6_2atmpS2759 = _M0L6_2atmpS2760 << 12;
            _M0L6_2atmpS2751 = _M0L6_2atmpS2758 | _M0L6_2atmpS2759;
            _M0L3valS2757 = _M0L1iS1043->$0;
            _M0L6_2atmpS2756 = _M0L3valS2757 + 2;
            if (
              _M0L6_2atmpS2756 < 0
              || _M0L6_2atmpS2756 >= Moonbit_array_length(_M0L5bytesS1040)
            ) {
              #line 239 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2755 = _M0L5bytesS1040[_M0L6_2atmpS2756];
            _M0L6_2atmpS2754 = (int32_t)_M0L6_2atmpS2755;
            _M0L6_2atmpS2753 = _M0L6_2atmpS2754 & 63;
            _M0L6_2atmpS2752 = _M0L6_2atmpS2753 << 6;
            _M0L6_2atmpS2745 = _M0L6_2atmpS2751 | _M0L6_2atmpS2752;
            _M0L3valS2750 = _M0L1iS1043->$0;
            _M0L6_2atmpS2749 = _M0L3valS2750 + 3;
            if (
              _M0L6_2atmpS2749 < 0
              || _M0L6_2atmpS2749 >= Moonbit_array_length(_M0L5bytesS1040)
            ) {
              #line 240 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
              moonbit_panic();
            }
            _M0L6_2atmpS2748 = _M0L5bytesS1040[_M0L6_2atmpS2749];
            _M0L6_2atmpS2747 = (int32_t)_M0L6_2atmpS2748;
            _M0L6_2atmpS2746 = _M0L6_2atmpS2747 & 63;
            _M0L6_2atmpS2744 = _M0L6_2atmpS2745 | _M0L6_2atmpS2746;
            _M0L1cS1044->$0 = _M0L6_2atmpS2744;
            _M0L3valS2768 = _M0L1cS1044->$0;
            _M0L6_2atmpS2767 = _M0L3valS2768 - 65536;
            _M0L1cS1044->$0 = _M0L6_2atmpS2767;
            _M0L3valS2772 = _M0L1cS1044->$0;
            _M0L6_2atmpS2771 = _M0L3valS2772 >> 10;
            _M0L6_2atmpS2770 = _M0L6_2atmpS2771 + 55296;
            _M0L6_2atmpS2769 = _M0L6_2atmpS2770;
            moonbit_incref(_M0L3resS1041);
            #line 242 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1041, _M0L6_2atmpS2769);
            _M0L8_2afieldS2904 = _M0L1cS1044->$0;
            moonbit_decref(_M0L1cS1044);
            _M0L3valS2776 = _M0L8_2afieldS2904;
            _M0L6_2atmpS2775 = _M0L3valS2776 & 1023;
            _M0L6_2atmpS2774 = _M0L6_2atmpS2775 + 56320;
            _M0L6_2atmpS2773 = _M0L6_2atmpS2774;
            moonbit_incref(_M0L3resS1041);
            #line 243 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
            _M0IPB13StringBuilderPB6Logger11write__char(_M0L3resS1041, _M0L6_2atmpS2773);
            _M0L3valS2778 = _M0L1iS1043->$0;
            _M0L6_2atmpS2777 = _M0L3valS2778 + 4;
            _M0L1iS1043->$0 = _M0L6_2atmpS2777;
          }
        }
      }
      continue;
    } else {
      moonbit_decref(_M0L1iS1043);
      moonbit_decref(_M0L5bytesS1040);
    }
    break;
  }
  #line 247 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3resS1041);
}

int32_t _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__argsN45moonbit__test__driver__internal__parse__int__S1033(
  int32_t _M0L6_2aenvS2689,
  moonbit_string_t _M0L1sS1034
) {
  struct _M0TPC13ref3RefGiE* _M0L3resS1035;
  int32_t _M0L3lenS1036;
  int32_t _M0L1iS1037;
  int32_t _M0L8_2afieldS2905;
  #line 197 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L3resS1035
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L3resS1035)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L3resS1035->$0 = 0;
  _M0L3lenS1036 = Moonbit_array_length(_M0L1sS1034);
  _M0L1iS1037 = 0;
  while (1) {
    if (_M0L1iS1037 < _M0L3lenS1036) {
      int32_t _M0L3valS2694 = _M0L3resS1035->$0;
      int32_t _M0L6_2atmpS2691 = _M0L3valS2694 * 10;
      int32_t _M0L6_2atmpS2693;
      int32_t _M0L6_2atmpS2692;
      int32_t _M0L6_2atmpS2690;
      int32_t _M0L6_2atmpS2695;
      if (
        _M0L1iS1037 < 0 || _M0L1iS1037 >= Moonbit_array_length(_M0L1sS1034)
      ) {
        #line 201 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS2693 = _M0L1sS1034[_M0L1iS1037];
      _M0L6_2atmpS2692 = _M0L6_2atmpS2693 - 48;
      _M0L6_2atmpS2690 = _M0L6_2atmpS2691 + _M0L6_2atmpS2692;
      _M0L3resS1035->$0 = _M0L6_2atmpS2690;
      _M0L6_2atmpS2695 = _M0L1iS1037 + 1;
      _M0L1iS1037 = _M0L6_2atmpS2695;
      continue;
    } else {
      moonbit_decref(_M0L1sS1034);
    }
    break;
  }
  _M0L8_2afieldS2905 = _M0L3resS1035->$0;
  moonbit_decref(_M0L3resS1035);
  return _M0L8_2afieldS2905;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test43MoonBit__Test__Driver__Internal__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1013,
  moonbit_string_t _M0L12_2adiscard__S1014,
  int32_t _M0L12_2adiscard__S1015,
  struct _M0TWssbEu* _M0L12_2adiscard__S1016,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1017
) {
  struct moonbit_result_0 _result_3417;
  #line 34 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1017);
  moonbit_decref(_M0L12_2adiscard__S1016);
  moonbit_decref(_M0L12_2adiscard__S1014);
  moonbit_decref(_M0L12_2adiscard__S1013);
  _result_3417.tag = 1;
  _result_3417.data.ok = 0;
  return _result_3417;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test48MoonBit__Test__Driver__Internal__Async__No__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1018,
  moonbit_string_t _M0L12_2adiscard__S1019,
  int32_t _M0L12_2adiscard__S1020,
  struct _M0TWssbEu* _M0L12_2adiscard__S1021,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1022
) {
  struct moonbit_result_0 _result_3418;
  #line 34 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1022);
  moonbit_decref(_M0L12_2adiscard__S1021);
  moonbit_decref(_M0L12_2adiscard__S1019);
  moonbit_decref(_M0L12_2adiscard__S1018);
  _result_3418.tag = 1;
  _result_3418.data.ok = 0;
  return _result_3418;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test50MoonBit__Test__Driver__Internal__Async__With__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1023,
  moonbit_string_t _M0L12_2adiscard__S1024,
  int32_t _M0L12_2adiscard__S1025,
  struct _M0TWssbEu* _M0L12_2adiscard__S1026,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1027
) {
  struct moonbit_result_0 _result_3419;
  #line 34 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1027);
  moonbit_decref(_M0L12_2adiscard__S1026);
  moonbit_decref(_M0L12_2adiscard__S1024);
  moonbit_decref(_M0L12_2adiscard__S1023);
  _result_3419.tag = 1;
  _result_3419.data.ok = 0;
  return _result_3419;
}

struct moonbit_result_0 _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test21MoonBit__Test__Driver9run__testGRP38clawteam8clawteam22config__blackbox__test50MoonBit__Test__Driver__Internal__With__Bench__ArgsE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1028,
  moonbit_string_t _M0L12_2adiscard__S1029,
  int32_t _M0L12_2adiscard__S1030,
  struct _M0TWssbEu* _M0L12_2adiscard__S1031,
  struct _M0TWRPC15error5ErrorEs* _M0L12_2adiscard__S1032
) {
  struct moonbit_result_0 _result_3420;
  #line 34 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1032);
  moonbit_decref(_M0L12_2adiscard__S1031);
  moonbit_decref(_M0L12_2adiscard__S1029);
  moonbit_decref(_M0L12_2adiscard__S1028);
  _result_3420.tag = 1;
  _result_3420.data.ok = 0;
  return _result_3420;
}

int32_t _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam22config__blackbox__test34MoonBit__Async__Test__Driver__ImplE(
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12_2adiscard__S1012
) {
  #line 12 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  moonbit_decref(_M0L12_2adiscard__S1012);
  return 0;
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test42____test__74797065735f746573742e6d6274__11(
  
) {
  struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0L6_2atmpS2676;
  struct _M0TP38clawteam8clawteam6config15WorkspaceConfig* _M0L6_2atmpS2677;
  struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE** _M0L7_2abindS1004;
  struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE** _M0L6_2atmpS2688;
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13CliToolConfigEE _M0L6_2atmpS2687;
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS2678;
  struct _M0TP38clawteam8clawteam6config11AgentConfig** _M0L6_2atmpS2686;
  struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE* _M0L6_2atmpS2679;
  struct _M0TP38clawteam8clawteam6config5Skill** _M0L6_2atmpS2685;
  struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE* _M0L6_2atmpS2680;
  struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE** _M0L7_2abindS1005;
  struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE** _M0L6_2atmpS2684;
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13ChannelConfigEE _M0L6_2atmpS2683;
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS2681;
  struct _M0TP38clawteam8clawteam6config11AuditConfig* _M0L6_2atmpS2682;
  struct _M0TP38clawteam8clawteam6config14ClawTeamConfig* _M0L6configS1003;
  void* _M0L6resultS1006;
  void* _M0L1eS1008;
  moonbit_string_t _M0L6_2atmpS2666;
  moonbit_string_t _M0L7_2abindS1009;
  int32_t _M0L6_2atmpS2668;
  struct _M0TPC16string10StringView _M0L6_2atmpS2667;
  int32_t _M0L6_2atmpS2664;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2665;
  struct _M0TPB4Show _M0L6_2atmpS2657;
  moonbit_string_t _M0L6_2atmpS2660;
  moonbit_string_t _M0L6_2atmpS2661;
  moonbit_string_t _M0L6_2atmpS2662;
  moonbit_string_t _M0L6_2atmpS2663;
  moonbit_string_t* _M0L6_2atmpS2659;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2658;
  #line 108 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2676
  = (struct _M0TP38clawteam8clawteam6config13GatewayConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config13GatewayConfig));
  Moonbit_object_header(_M0L6_2atmpS2676)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config13GatewayConfig, $1) >> 2, 1, 0);
  _M0L6_2atmpS2676->$0 = 0;
  _M0L6_2atmpS2676->$1 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L6_2atmpS2677 = 0;
  _M0L7_2abindS1004
  = (struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2688 = _M0L7_2abindS1004;
  _M0L6_2atmpS2687
  = (struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13CliToolConfigEE){
    0, 0, _M0L6_2atmpS2688
  };
  #line 114 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2678
  = _M0MPB3Map11from__arrayGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L6_2atmpS2687);
  _M0L6_2atmpS2686
  = (struct _M0TP38clawteam8clawteam6config11AgentConfig**)moonbit_empty_ref_array;
  _M0L6_2atmpS2679
  = (struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE));
  Moonbit_object_header(_M0L6_2atmpS2679)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2679->$0 = _M0L6_2atmpS2686;
  _M0L6_2atmpS2679->$1 = 0;
  _M0L6_2atmpS2685
  = (struct _M0TP38clawteam8clawteam6config5Skill**)moonbit_empty_ref_array;
  _M0L6_2atmpS2680
  = (struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE));
  Moonbit_object_header(_M0L6_2atmpS2680)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2680->$0 = _M0L6_2atmpS2685;
  _M0L6_2atmpS2680->$1 = 0;
  _M0L7_2abindS1005
  = (struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2684 = _M0L7_2abindS1005;
  _M0L6_2atmpS2683
  = (struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13ChannelConfigEE){
    0, 0, _M0L6_2atmpS2684
  };
  #line 117 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2681
  = _M0MPB3Map11from__arrayGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L6_2atmpS2683);
  _M0L6_2atmpS2682
  = (struct _M0TP38clawteam8clawteam6config11AuditConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config11AuditConfig));
  Moonbit_object_header(_M0L6_2atmpS2682)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config11AuditConfig, $2) >> 2, 1, 0);
  _M0L6_2atmpS2682->$0 = 1;
  _M0L6_2atmpS2682->$1 = 30;
  _M0L6_2atmpS2682->$2 = (moonbit_string_t)moonbit_string_literal_10.data;
  _M0L6configS1003
  = (struct _M0TP38clawteam8clawteam6config14ClawTeamConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config14ClawTeamConfig));
  Moonbit_object_header(_M0L6configS1003)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config14ClawTeamConfig, $0) >> 2, 8, 0);
  _M0L6configS1003->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _M0L6configS1003->$1 = _M0L6_2atmpS2676;
  _M0L6configS1003->$2 = _M0L6_2atmpS2677;
  _M0L6configS1003->$3 = _M0L6_2atmpS2678;
  _M0L6configS1003->$4 = _M0L6_2atmpS2679;
  _M0L6configS1003->$5 = _M0L6_2atmpS2680;
  _M0L6configS1003->$6 = _M0L6_2atmpS2681;
  _M0L6configS1003->$7 = _M0L6_2atmpS2682;
  #line 124 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6resultS1006
  = _M0FP38clawteam8clawteam6config16validate__config(_M0L6configS1003);
  switch (Moonbit_object_tag(_M0L6resultS1006)) {
    case 1: {
      struct _M0TPB4Show _M0L6_2atmpS2669;
      moonbit_string_t _M0L6_2atmpS2672;
      moonbit_string_t _M0L6_2atmpS2673;
      moonbit_string_t _M0L6_2atmpS2674;
      moonbit_string_t _M0L6_2atmpS2675;
      moonbit_string_t* _M0L6_2atmpS2671;
      struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2670;
      moonbit_decref(_M0L6resultS1006);
      _M0L6_2atmpS2669
      = (struct _M0TPB4Show){
        _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
          (moonbit_string_t)moonbit_string_literal_12.data
      };
      _M0L6_2atmpS2672 = (moonbit_string_t)moonbit_string_literal_13.data;
      _M0L6_2atmpS2673 = (moonbit_string_t)moonbit_string_literal_14.data;
      _M0L6_2atmpS2674 = 0;
      _M0L6_2atmpS2675 = 0;
      _M0L6_2atmpS2671 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
      _M0L6_2atmpS2671[0] = _M0L6_2atmpS2672;
      _M0L6_2atmpS2671[1] = _M0L6_2atmpS2673;
      _M0L6_2atmpS2671[2] = _M0L6_2atmpS2674;
      _M0L6_2atmpS2671[3] = _M0L6_2atmpS2675;
      _M0L6_2atmpS2670
      = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
      Moonbit_object_header(_M0L6_2atmpS2670)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
      _M0L6_2atmpS2670->$0 = _M0L6_2atmpS2671;
      _M0L6_2atmpS2670->$1 = 4;
      #line 126 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
      return _M0FPB15inspect_2einner(_M0L6_2atmpS2669, (moonbit_string_t)moonbit_string_literal_15.data, (moonbit_string_t)moonbit_string_literal_16.data, _M0L6_2atmpS2670);
      break;
    }
    default: {
      struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err* _M0L6_2aErrS1010 =
        (struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err*)_M0L6resultS1006;
      void* _M0L8_2afieldS2906 = _M0L6_2aErrS1010->$0;
      int32_t _M0L6_2acntS3255 = Moonbit_object_header(_M0L6_2aErrS1010)->rc;
      void* _M0L4_2aeS1011;
      if (_M0L6_2acntS3255 > 1) {
        int32_t _M0L11_2anew__cntS3256 = _M0L6_2acntS3255 - 1;
        Moonbit_object_header(_M0L6_2aErrS1010)->rc = _M0L11_2anew__cntS3256;
        moonbit_incref(_M0L8_2afieldS2906);
      } else if (_M0L6_2acntS3255 == 1) {
        #line 125 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
        moonbit_free(_M0L6_2aErrS1010);
      }
      _M0L4_2aeS1011 = _M0L8_2afieldS2906;
      _M0L1eS1008 = _M0L4_2aeS1011;
      goto join_1007;
      break;
    }
  }
  join_1007:;
  #line 127 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2666
  = _M0MP38clawteam8clawteam6errors13ClawTeamError11to__message(_M0L1eS1008);
  _M0L7_2abindS1009 = (moonbit_string_t)moonbit_string_literal_17.data;
  _M0L6_2atmpS2668 = Moonbit_array_length(_M0L7_2abindS1009);
  _M0L6_2atmpS2667
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS2668, _M0L7_2abindS1009
  };
  #line 127 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2664
  = _M0MPC16string6String8contains(_M0L6_2atmpS2666, _M0L6_2atmpS2667);
  _M0L14_2aboxed__selfS2665
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2665)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2665->$0 = _M0L6_2atmpS2664;
  _M0L6_2atmpS2657
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2665
  };
  _M0L6_2atmpS2660 = (moonbit_string_t)moonbit_string_literal_18.data;
  _M0L6_2atmpS2661 = (moonbit_string_t)moonbit_string_literal_19.data;
  _M0L6_2atmpS2662 = 0;
  _M0L6_2atmpS2663 = 0;
  _M0L6_2atmpS2659 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2659[0] = _M0L6_2atmpS2660;
  _M0L6_2atmpS2659[1] = _M0L6_2atmpS2661;
  _M0L6_2atmpS2659[2] = _M0L6_2atmpS2662;
  _M0L6_2atmpS2659[3] = _M0L6_2atmpS2663;
  _M0L6_2atmpS2658
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2658)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2658->$0 = _M0L6_2atmpS2659;
  _M0L6_2atmpS2658->$1 = 4;
  #line 127 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2657, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_21.data, _M0L6_2atmpS2658);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test42____test__74797065735f746573742e6d6274__10(
  
) {
  struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0L6configS1002;
  int32_t _M0L4portS2645;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2646;
  struct _M0TPB4Show _M0L6_2atmpS2638;
  moonbit_string_t _M0L6_2atmpS2641;
  moonbit_string_t _M0L6_2atmpS2642;
  moonbit_string_t _M0L6_2atmpS2643;
  moonbit_string_t _M0L6_2atmpS2644;
  moonbit_string_t* _M0L6_2atmpS2640;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2639;
  struct moonbit_result_0 _tmp_3422;
  moonbit_string_t _M0L8_2afieldS2907;
  int32_t _M0L6_2acntS3257;
  moonbit_string_t _M0L4hostS2656;
  struct _M0TPB4Show _M0L6_2atmpS2649;
  moonbit_string_t _M0L6_2atmpS2652;
  moonbit_string_t _M0L6_2atmpS2653;
  moonbit_string_t _M0L6_2atmpS2654;
  moonbit_string_t _M0L6_2atmpS2655;
  moonbit_string_t* _M0L6_2atmpS2651;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2650;
  #line 102 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  #line 103 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6configS1002
  = _M0FP38clawteam8clawteam6config24default__gateway__config();
  _M0L4portS2645 = _M0L6configS1002->$0;
  _M0L14_2aboxed__selfS2646
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2646)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2646->$0 = _M0L4portS2645;
  _M0L6_2atmpS2638
  = (struct _M0TPB4Show){
    _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2646
  };
  _M0L6_2atmpS2641 = (moonbit_string_t)moonbit_string_literal_22.data;
  _M0L6_2atmpS2642 = (moonbit_string_t)moonbit_string_literal_23.data;
  _M0L6_2atmpS2643 = 0;
  _M0L6_2atmpS2644 = 0;
  _M0L6_2atmpS2640 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2640[0] = _M0L6_2atmpS2641;
  _M0L6_2atmpS2640[1] = _M0L6_2atmpS2642;
  _M0L6_2atmpS2640[2] = _M0L6_2atmpS2643;
  _M0L6_2atmpS2640[3] = _M0L6_2atmpS2644;
  _M0L6_2atmpS2639
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2639)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2639->$0 = _M0L6_2atmpS2640;
  _M0L6_2atmpS2639->$1 = 4;
  #line 104 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3422
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2638, (moonbit_string_t)moonbit_string_literal_24.data, (moonbit_string_t)moonbit_string_literal_25.data, _M0L6_2atmpS2639);
  if (_tmp_3422.tag) {
    int32_t const _M0L5_2aokS2647 = _tmp_3422.data.ok;
  } else {
    void* const _M0L6_2aerrS2648 = _tmp_3422.data.err;
    struct moonbit_result_0 _result_3423;
    moonbit_decref(_M0L6configS1002);
    _result_3423.tag = 0;
    _result_3423.data.err = _M0L6_2aerrS2648;
    return _result_3423;
  }
  _M0L8_2afieldS2907 = _M0L6configS1002->$1;
  _M0L6_2acntS3257 = Moonbit_object_header(_M0L6configS1002)->rc;
  if (_M0L6_2acntS3257 > 1) {
    int32_t _M0L11_2anew__cntS3258 = _M0L6_2acntS3257 - 1;
    Moonbit_object_header(_M0L6configS1002)->rc = _M0L11_2anew__cntS3258;
    moonbit_incref(_M0L8_2afieldS2907);
  } else if (_M0L6_2acntS3257 == 1) {
    #line 105 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
    moonbit_free(_M0L6configS1002);
  }
  _M0L4hostS2656 = _M0L8_2afieldS2907;
  _M0L6_2atmpS2649
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L4hostS2656
  };
  _M0L6_2atmpS2652 = (moonbit_string_t)moonbit_string_literal_26.data;
  _M0L6_2atmpS2653 = (moonbit_string_t)moonbit_string_literal_27.data;
  _M0L6_2atmpS2654 = 0;
  _M0L6_2atmpS2655 = 0;
  _M0L6_2atmpS2651 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2651[0] = _M0L6_2atmpS2652;
  _M0L6_2atmpS2651[1] = _M0L6_2atmpS2653;
  _M0L6_2atmpS2651[2] = _M0L6_2atmpS2654;
  _M0L6_2atmpS2651[3] = _M0L6_2atmpS2655;
  _M0L6_2atmpS2650
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2650)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2650->$0 = _M0L6_2atmpS2651;
  _M0L6_2atmpS2650->$1 = 4;
  #line 105 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2649, (moonbit_string_t)moonbit_string_literal_9.data, (moonbit_string_t)moonbit_string_literal_28.data, _M0L6_2atmpS2650);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__9(
  
) {
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L5toolsS1001;
  int32_t _M0L6_2atmpS2603;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2604;
  struct _M0TPB4Show _M0L6_2atmpS2596;
  moonbit_string_t _M0L6_2atmpS2599;
  moonbit_string_t _M0L6_2atmpS2600;
  moonbit_string_t _M0L6_2atmpS2601;
  moonbit_string_t _M0L6_2atmpS2602;
  moonbit_string_t* _M0L6_2atmpS2598;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2597;
  struct moonbit_result_0 _tmp_3424;
  int32_t _M0L6_2atmpS2614;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2615;
  struct _M0TPB4Show _M0L6_2atmpS2607;
  moonbit_string_t _M0L6_2atmpS2610;
  moonbit_string_t _M0L6_2atmpS2611;
  moonbit_string_t _M0L6_2atmpS2612;
  moonbit_string_t _M0L6_2atmpS2613;
  moonbit_string_t* _M0L6_2atmpS2609;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2608;
  struct moonbit_result_0 _tmp_3426;
  int32_t _M0L6_2atmpS2625;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2626;
  struct _M0TPB4Show _M0L6_2atmpS2618;
  moonbit_string_t _M0L6_2atmpS2621;
  moonbit_string_t _M0L6_2atmpS2622;
  moonbit_string_t _M0L6_2atmpS2623;
  moonbit_string_t _M0L6_2atmpS2624;
  moonbit_string_t* _M0L6_2atmpS2620;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2619;
  struct moonbit_result_0 _tmp_3428;
  int32_t _M0L6_2atmpS2636;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2637;
  struct _M0TPB4Show _M0L6_2atmpS2629;
  moonbit_string_t _M0L6_2atmpS2632;
  moonbit_string_t _M0L6_2atmpS2633;
  moonbit_string_t _M0L6_2atmpS2634;
  moonbit_string_t _M0L6_2atmpS2635;
  moonbit_string_t* _M0L6_2atmpS2631;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2630;
  #line 94 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  #line 95 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L5toolsS1001 = _M0FP38clawteam8clawteam6config19default__cli__tools();
  moonbit_incref(_M0L5toolsS1001);
  #line 96 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2603
  = _M0MPB3Map8containsGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L5toolsS1001, (moonbit_string_t)moonbit_string_literal_29.data);
  _M0L14_2aboxed__selfS2604
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2604)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2604->$0 = _M0L6_2atmpS2603;
  _M0L6_2atmpS2596
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2604
  };
  _M0L6_2atmpS2599 = (moonbit_string_t)moonbit_string_literal_30.data;
  _M0L6_2atmpS2600 = (moonbit_string_t)moonbit_string_literal_31.data;
  _M0L6_2atmpS2601 = 0;
  _M0L6_2atmpS2602 = 0;
  _M0L6_2atmpS2598 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2598[0] = _M0L6_2atmpS2599;
  _M0L6_2atmpS2598[1] = _M0L6_2atmpS2600;
  _M0L6_2atmpS2598[2] = _M0L6_2atmpS2601;
  _M0L6_2atmpS2598[3] = _M0L6_2atmpS2602;
  _M0L6_2atmpS2597
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2597)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2597->$0 = _M0L6_2atmpS2598;
  _M0L6_2atmpS2597->$1 = 4;
  #line 96 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3424
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2596, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_32.data, _M0L6_2atmpS2597);
  if (_tmp_3424.tag) {
    int32_t const _M0L5_2aokS2605 = _tmp_3424.data.ok;
  } else {
    void* const _M0L6_2aerrS2606 = _tmp_3424.data.err;
    struct moonbit_result_0 _result_3425;
    moonbit_decref(_M0L5toolsS1001);
    _result_3425.tag = 0;
    _result_3425.data.err = _M0L6_2aerrS2606;
    return _result_3425;
  }
  moonbit_incref(_M0L5toolsS1001);
  #line 97 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2614
  = _M0MPB3Map8containsGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L5toolsS1001, (moonbit_string_t)moonbit_string_literal_33.data);
  _M0L14_2aboxed__selfS2615
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2615)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2615->$0 = _M0L6_2atmpS2614;
  _M0L6_2atmpS2607
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2615
  };
  _M0L6_2atmpS2610 = (moonbit_string_t)moonbit_string_literal_34.data;
  _M0L6_2atmpS2611 = (moonbit_string_t)moonbit_string_literal_35.data;
  _M0L6_2atmpS2612 = 0;
  _M0L6_2atmpS2613 = 0;
  _M0L6_2atmpS2609 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2609[0] = _M0L6_2atmpS2610;
  _M0L6_2atmpS2609[1] = _M0L6_2atmpS2611;
  _M0L6_2atmpS2609[2] = _M0L6_2atmpS2612;
  _M0L6_2atmpS2609[3] = _M0L6_2atmpS2613;
  _M0L6_2atmpS2608
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2608)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2608->$0 = _M0L6_2atmpS2609;
  _M0L6_2atmpS2608->$1 = 4;
  #line 97 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3426
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2607, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_36.data, _M0L6_2atmpS2608);
  if (_tmp_3426.tag) {
    int32_t const _M0L5_2aokS2616 = _tmp_3426.data.ok;
  } else {
    void* const _M0L6_2aerrS2617 = _tmp_3426.data.err;
    struct moonbit_result_0 _result_3427;
    moonbit_decref(_M0L5toolsS1001);
    _result_3427.tag = 0;
    _result_3427.data.err = _M0L6_2aerrS2617;
    return _result_3427;
  }
  moonbit_incref(_M0L5toolsS1001);
  #line 98 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2625
  = _M0MPB3Map8containsGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L5toolsS1001, (moonbit_string_t)moonbit_string_literal_37.data);
  _M0L14_2aboxed__selfS2626
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2626)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2626->$0 = _M0L6_2atmpS2625;
  _M0L6_2atmpS2618
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2626
  };
  _M0L6_2atmpS2621 = (moonbit_string_t)moonbit_string_literal_38.data;
  _M0L6_2atmpS2622 = (moonbit_string_t)moonbit_string_literal_39.data;
  _M0L6_2atmpS2623 = 0;
  _M0L6_2atmpS2624 = 0;
  _M0L6_2atmpS2620 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2620[0] = _M0L6_2atmpS2621;
  _M0L6_2atmpS2620[1] = _M0L6_2atmpS2622;
  _M0L6_2atmpS2620[2] = _M0L6_2atmpS2623;
  _M0L6_2atmpS2620[3] = _M0L6_2atmpS2624;
  _M0L6_2atmpS2619
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2619)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2619->$0 = _M0L6_2atmpS2620;
  _M0L6_2atmpS2619->$1 = 4;
  #line 98 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3428
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2618, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_40.data, _M0L6_2atmpS2619);
  if (_tmp_3428.tag) {
    int32_t const _M0L5_2aokS2627 = _tmp_3428.data.ok;
  } else {
    void* const _M0L6_2aerrS2628 = _tmp_3428.data.err;
    struct moonbit_result_0 _result_3429;
    moonbit_decref(_M0L5toolsS1001);
    _result_3429.tag = 0;
    _result_3429.data.err = _M0L6_2aerrS2628;
    return _result_3429;
  }
  #line 99 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2636
  = _M0MPB3Map8containsGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L5toolsS1001, (moonbit_string_t)moonbit_string_literal_41.data);
  _M0L14_2aboxed__selfS2637
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2637)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2637->$0 = _M0L6_2atmpS2636;
  _M0L6_2atmpS2629
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2637
  };
  _M0L6_2atmpS2632 = (moonbit_string_t)moonbit_string_literal_42.data;
  _M0L6_2atmpS2633 = (moonbit_string_t)moonbit_string_literal_43.data;
  _M0L6_2atmpS2634 = 0;
  _M0L6_2atmpS2635 = 0;
  _M0L6_2atmpS2631 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2631[0] = _M0L6_2atmpS2632;
  _M0L6_2atmpS2631[1] = _M0L6_2atmpS2633;
  _M0L6_2atmpS2631[2] = _M0L6_2atmpS2634;
  _M0L6_2atmpS2631[3] = _M0L6_2atmpS2635;
  _M0L6_2atmpS2630
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2630)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2630->$0 = _M0L6_2atmpS2631;
  _M0L6_2atmpS2630->$1 = 4;
  #line 99 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2629, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_44.data, _M0L6_2atmpS2630);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__8(
  
) {
  struct _M0TPB5ArrayGsE* _M0L5pathsS1000;
  int32_t _M0L6_2atmpS2574;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2575;
  struct _M0TPB4Show _M0L6_2atmpS2567;
  moonbit_string_t _M0L6_2atmpS2570;
  moonbit_string_t _M0L6_2atmpS2571;
  moonbit_string_t _M0L6_2atmpS2572;
  moonbit_string_t _M0L6_2atmpS2573;
  moonbit_string_t* _M0L6_2atmpS2569;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2568;
  struct moonbit_result_0 _tmp_3430;
  moonbit_string_t _M0L6_2atmpS2585;
  struct _M0TPB4Show _M0L6_2atmpS2578;
  moonbit_string_t _M0L6_2atmpS2581;
  moonbit_string_t _M0L6_2atmpS2582;
  moonbit_string_t _M0L6_2atmpS2583;
  moonbit_string_t _M0L6_2atmpS2584;
  moonbit_string_t* _M0L6_2atmpS2580;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2579;
  struct moonbit_result_0 _tmp_3432;
  moonbit_string_t _M0L6_2atmpS2595;
  struct _M0TPB4Show _M0L6_2atmpS2588;
  moonbit_string_t _M0L6_2atmpS2591;
  moonbit_string_t _M0L6_2atmpS2592;
  moonbit_string_t _M0L6_2atmpS2593;
  moonbit_string_t _M0L6_2atmpS2594;
  moonbit_string_t* _M0L6_2atmpS2590;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2589;
  #line 87 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  #line 88 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L5pathsS1000
  = _M0FP38clawteam8clawteam6config13config__paths((moonbit_string_t)moonbit_string_literal_45.data, (moonbit_string_t)moonbit_string_literal_46.data);
  moonbit_incref(_M0L5pathsS1000);
  #line 89 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2574 = _M0MPC15array5Array6lengthGsE(_M0L5pathsS1000);
  _M0L14_2aboxed__selfS2575
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2575)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2575->$0 = _M0L6_2atmpS2574;
  _M0L6_2atmpS2567
  = (struct _M0TPB4Show){
    _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2575
  };
  _M0L6_2atmpS2570 = (moonbit_string_t)moonbit_string_literal_47.data;
  _M0L6_2atmpS2571 = (moonbit_string_t)moonbit_string_literal_48.data;
  _M0L6_2atmpS2572 = 0;
  _M0L6_2atmpS2573 = 0;
  _M0L6_2atmpS2569 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2569[0] = _M0L6_2atmpS2570;
  _M0L6_2atmpS2569[1] = _M0L6_2atmpS2571;
  _M0L6_2atmpS2569[2] = _M0L6_2atmpS2572;
  _M0L6_2atmpS2569[3] = _M0L6_2atmpS2573;
  _M0L6_2atmpS2568
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2568)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2568->$0 = _M0L6_2atmpS2569;
  _M0L6_2atmpS2568->$1 = 4;
  #line 89 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3430
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2567, (moonbit_string_t)moonbit_string_literal_49.data, (moonbit_string_t)moonbit_string_literal_50.data, _M0L6_2atmpS2568);
  if (_tmp_3430.tag) {
    int32_t const _M0L5_2aokS2576 = _tmp_3430.data.ok;
  } else {
    void* const _M0L6_2aerrS2577 = _tmp_3430.data.err;
    struct moonbit_result_0 _result_3431;
    moonbit_decref(_M0L5pathsS1000);
    _result_3431.tag = 0;
    _result_3431.data.err = _M0L6_2aerrS2577;
    return _result_3431;
  }
  moonbit_incref(_M0L5pathsS1000);
  #line 90 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2585 = _M0MPC15array5Array2atGsE(_M0L5pathsS1000, 0);
  _M0L6_2atmpS2578
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2585
  };
  _M0L6_2atmpS2581 = (moonbit_string_t)moonbit_string_literal_51.data;
  _M0L6_2atmpS2582 = (moonbit_string_t)moonbit_string_literal_52.data;
  _M0L6_2atmpS2583 = 0;
  _M0L6_2atmpS2584 = 0;
  _M0L6_2atmpS2580 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2580[0] = _M0L6_2atmpS2581;
  _M0L6_2atmpS2580[1] = _M0L6_2atmpS2582;
  _M0L6_2atmpS2580[2] = _M0L6_2atmpS2583;
  _M0L6_2atmpS2580[3] = _M0L6_2atmpS2584;
  _M0L6_2atmpS2579
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2579)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2579->$0 = _M0L6_2atmpS2580;
  _M0L6_2atmpS2579->$1 = 4;
  #line 90 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3432
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2578, (moonbit_string_t)moonbit_string_literal_53.data, (moonbit_string_t)moonbit_string_literal_54.data, _M0L6_2atmpS2579);
  if (_tmp_3432.tag) {
    int32_t const _M0L5_2aokS2586 = _tmp_3432.data.ok;
  } else {
    void* const _M0L6_2aerrS2587 = _tmp_3432.data.err;
    struct moonbit_result_0 _result_3433;
    moonbit_decref(_M0L5pathsS1000);
    _result_3433.tag = 0;
    _result_3433.data.err = _M0L6_2aerrS2587;
    return _result_3433;
  }
  #line 91 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2595 = _M0MPC15array5Array2atGsE(_M0L5pathsS1000, 3);
  _M0L6_2atmpS2588
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2595
  };
  _M0L6_2atmpS2591 = (moonbit_string_t)moonbit_string_literal_55.data;
  _M0L6_2atmpS2592 = (moonbit_string_t)moonbit_string_literal_56.data;
  _M0L6_2atmpS2593 = 0;
  _M0L6_2atmpS2594 = 0;
  _M0L6_2atmpS2590 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2590[0] = _M0L6_2atmpS2591;
  _M0L6_2atmpS2590[1] = _M0L6_2atmpS2592;
  _M0L6_2atmpS2590[2] = _M0L6_2atmpS2593;
  _M0L6_2atmpS2590[3] = _M0L6_2atmpS2594;
  _M0L6_2atmpS2589
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2589)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2589->$0 = _M0L6_2atmpS2590;
  _M0L6_2atmpS2589->$1 = 4;
  #line 91 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2588, (moonbit_string_t)moonbit_string_literal_57.data, (moonbit_string_t)moonbit_string_literal_58.data, _M0L6_2atmpS2589);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__7(
  
) {
  struct _M0TP38clawteam8clawteam6config14ClawTeamConfig* _M0L6configS994;
  void* _M0L6resultS995;
  void* _M0L1eS997;
  moonbit_string_t _M0L6_2atmpS2559;
  struct _M0TPB4Show _M0L6_2atmpS2552;
  moonbit_string_t _M0L6_2atmpS2555;
  moonbit_string_t _M0L6_2atmpS2556;
  moonbit_string_t _M0L6_2atmpS2557;
  moonbit_string_t _M0L6_2atmpS2558;
  moonbit_string_t* _M0L6_2atmpS2554;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2553;
  #line 78 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  #line 79 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6configS994
  = _M0MP38clawteam8clawteam6config12ConfigLoader15default__config((moonbit_string_t)moonbit_string_literal_45.data);
  #line 80 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6resultS995
  = _M0FP38clawteam8clawteam6config16validate__config(_M0L6configS994);
  switch (Moonbit_object_tag(_M0L6resultS995)) {
    case 1: {
      struct _M0TPB4Show _M0L6_2atmpS2560;
      moonbit_string_t _M0L6_2atmpS2563;
      moonbit_string_t _M0L6_2atmpS2564;
      moonbit_string_t _M0L6_2atmpS2565;
      moonbit_string_t _M0L6_2atmpS2566;
      moonbit_string_t* _M0L6_2atmpS2562;
      struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2561;
      moonbit_decref(_M0L6resultS995);
      _M0L6_2atmpS2560
      = (struct _M0TPB4Show){
        _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
          (moonbit_string_t)moonbit_string_literal_59.data
      };
      _M0L6_2atmpS2563 = (moonbit_string_t)moonbit_string_literal_60.data;
      _M0L6_2atmpS2564 = (moonbit_string_t)moonbit_string_literal_61.data;
      _M0L6_2atmpS2565 = 0;
      _M0L6_2atmpS2566 = 0;
      _M0L6_2atmpS2562 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
      _M0L6_2atmpS2562[0] = _M0L6_2atmpS2563;
      _M0L6_2atmpS2562[1] = _M0L6_2atmpS2564;
      _M0L6_2atmpS2562[2] = _M0L6_2atmpS2565;
      _M0L6_2atmpS2562[3] = _M0L6_2atmpS2566;
      _M0L6_2atmpS2561
      = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
      Moonbit_object_header(_M0L6_2atmpS2561)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
      _M0L6_2atmpS2561->$0 = _M0L6_2atmpS2562;
      _M0L6_2atmpS2561->$1 = 4;
      #line 82 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
      return _M0FPB15inspect_2einner(_M0L6_2atmpS2560, (moonbit_string_t)moonbit_string_literal_59.data, (moonbit_string_t)moonbit_string_literal_62.data, _M0L6_2atmpS2561);
      break;
    }
    default: {
      struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err* _M0L6_2aErrS998 =
        (struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err*)_M0L6resultS995;
      void* _M0L8_2afieldS2908 = _M0L6_2aErrS998->$0;
      int32_t _M0L6_2acntS3259 = Moonbit_object_header(_M0L6_2aErrS998)->rc;
      void* _M0L4_2aeS999;
      if (_M0L6_2acntS3259 > 1) {
        int32_t _M0L11_2anew__cntS3260 = _M0L6_2acntS3259 - 1;
        Moonbit_object_header(_M0L6_2aErrS998)->rc = _M0L11_2anew__cntS3260;
        moonbit_incref(_M0L8_2afieldS2908);
      } else if (_M0L6_2acntS3259 == 1) {
        #line 81 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
        moonbit_free(_M0L6_2aErrS998);
      }
      _M0L4_2aeS999 = _M0L8_2afieldS2908;
      _M0L1eS997 = _M0L4_2aeS999;
      goto join_996;
      break;
    }
  }
  join_996:;
  #line 83 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2559
  = _M0MP38clawteam8clawteam6errors13ClawTeamError11to__message(_M0L1eS997);
  _M0L6_2atmpS2552
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L6_2atmpS2559
  };
  _M0L6_2atmpS2555 = (moonbit_string_t)moonbit_string_literal_63.data;
  _M0L6_2atmpS2556 = (moonbit_string_t)moonbit_string_literal_64.data;
  _M0L6_2atmpS2557 = 0;
  _M0L6_2atmpS2558 = 0;
  _M0L6_2atmpS2554 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2554[0] = _M0L6_2atmpS2555;
  _M0L6_2atmpS2554[1] = _M0L6_2atmpS2556;
  _M0L6_2atmpS2554[2] = _M0L6_2atmpS2557;
  _M0L6_2atmpS2554[3] = _M0L6_2atmpS2558;
  _M0L6_2atmpS2553
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2553)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2553->$0 = _M0L6_2atmpS2554;
  _M0L6_2atmpS2553->$1 = 4;
  #line 83 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2552, (moonbit_string_t)moonbit_string_literal_65.data, (moonbit_string_t)moonbit_string_literal_66.data, _M0L6_2atmpS2553);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__6(
  
) {
  struct _M0TP38clawteam8clawteam6config11AuditConfig* _M0L6configS993;
  int32_t _M0L7enabledS2539;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2540;
  struct _M0TPB4Show _M0L6_2atmpS2532;
  moonbit_string_t _M0L6_2atmpS2535;
  moonbit_string_t _M0L6_2atmpS2536;
  moonbit_string_t _M0L6_2atmpS2537;
  moonbit_string_t _M0L6_2atmpS2538;
  moonbit_string_t* _M0L6_2atmpS2534;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2533;
  struct moonbit_result_0 _tmp_3435;
  int32_t _M0L8_2afieldS2909;
  int32_t _M0L15retention__daysS2550;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2551;
  struct _M0TPB4Show _M0L6_2atmpS2543;
  moonbit_string_t _M0L6_2atmpS2546;
  moonbit_string_t _M0L6_2atmpS2547;
  moonbit_string_t _M0L6_2atmpS2548;
  moonbit_string_t _M0L6_2atmpS2549;
  moonbit_string_t* _M0L6_2atmpS2545;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2544;
  #line 68 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6configS993
  = (struct _M0TP38clawteam8clawteam6config11AuditConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config11AuditConfig));
  Moonbit_object_header(_M0L6configS993)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config11AuditConfig, $2) >> 2, 1, 0);
  _M0L6configS993->$0 = 1;
  _M0L6configS993->$1 = 30;
  _M0L6configS993->$2 = (moonbit_string_t)moonbit_string_literal_67.data;
  _M0L7enabledS2539 = _M0L6configS993->$0;
  _M0L14_2aboxed__selfS2540
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2540)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2540->$0 = _M0L7enabledS2539;
  _M0L6_2atmpS2532
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2540
  };
  _M0L6_2atmpS2535 = (moonbit_string_t)moonbit_string_literal_68.data;
  _M0L6_2atmpS2536 = (moonbit_string_t)moonbit_string_literal_69.data;
  _M0L6_2atmpS2537 = 0;
  _M0L6_2atmpS2538 = 0;
  _M0L6_2atmpS2534 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2534[0] = _M0L6_2atmpS2535;
  _M0L6_2atmpS2534[1] = _M0L6_2atmpS2536;
  _M0L6_2atmpS2534[2] = _M0L6_2atmpS2537;
  _M0L6_2atmpS2534[3] = _M0L6_2atmpS2538;
  _M0L6_2atmpS2533
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2533)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2533->$0 = _M0L6_2atmpS2534;
  _M0L6_2atmpS2533->$1 = 4;
  #line 74 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3435
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2532, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_70.data, _M0L6_2atmpS2533);
  if (_tmp_3435.tag) {
    int32_t const _M0L5_2aokS2541 = _tmp_3435.data.ok;
  } else {
    void* const _M0L6_2aerrS2542 = _tmp_3435.data.err;
    struct moonbit_result_0 _result_3436;
    moonbit_decref(_M0L6configS993);
    _result_3436.tag = 0;
    _result_3436.data.err = _M0L6_2aerrS2542;
    return _result_3436;
  }
  _M0L8_2afieldS2909 = _M0L6configS993->$1;
  moonbit_decref(_M0L6configS993);
  _M0L15retention__daysS2550 = _M0L8_2afieldS2909;
  _M0L14_2aboxed__selfS2551
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2551)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2551->$0 = _M0L15retention__daysS2550;
  _M0L6_2atmpS2543
  = (struct _M0TPB4Show){
    _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2551
  };
  _M0L6_2atmpS2546 = (moonbit_string_t)moonbit_string_literal_71.data;
  _M0L6_2atmpS2547 = (moonbit_string_t)moonbit_string_literal_72.data;
  _M0L6_2atmpS2548 = 0;
  _M0L6_2atmpS2549 = 0;
  _M0L6_2atmpS2545 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2545[0] = _M0L6_2atmpS2546;
  _M0L6_2atmpS2545[1] = _M0L6_2atmpS2547;
  _M0L6_2atmpS2545[2] = _M0L6_2atmpS2548;
  _M0L6_2atmpS2545[3] = _M0L6_2atmpS2549;
  _M0L6_2atmpS2544
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2544)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2544->$0 = _M0L6_2atmpS2545;
  _M0L6_2atmpS2544->$1 = 4;
  #line 75 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2543, (moonbit_string_t)moonbit_string_literal_73.data, (moonbit_string_t)moonbit_string_literal_74.data, _M0L6_2atmpS2544);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__5(
  
) {
  struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0L6configS992;
  int32_t _M0L4portS2520;
  struct _M0Y3Int* _M0L14_2aboxed__selfS2521;
  struct _M0TPB4Show _M0L6_2atmpS2513;
  moonbit_string_t _M0L6_2atmpS2516;
  moonbit_string_t _M0L6_2atmpS2517;
  moonbit_string_t _M0L6_2atmpS2518;
  moonbit_string_t _M0L6_2atmpS2519;
  moonbit_string_t* _M0L6_2atmpS2515;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2514;
  struct moonbit_result_0 _tmp_3437;
  moonbit_string_t _M0L8_2afieldS2910;
  int32_t _M0L6_2acntS3261;
  moonbit_string_t _M0L4hostS2531;
  struct _M0TPB4Show _M0L6_2atmpS2524;
  moonbit_string_t _M0L6_2atmpS2527;
  moonbit_string_t _M0L6_2atmpS2528;
  moonbit_string_t _M0L6_2atmpS2529;
  moonbit_string_t _M0L6_2atmpS2530;
  moonbit_string_t* _M0L6_2atmpS2526;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2525;
  #line 59 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6configS992
  = (struct _M0TP38clawteam8clawteam6config13GatewayConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config13GatewayConfig));
  Moonbit_object_header(_M0L6configS992)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config13GatewayConfig, $1) >> 2, 1, 0);
  _M0L6configS992->$0 = 3000;
  _M0L6configS992->$1 = (moonbit_string_t)moonbit_string_literal_9.data;
  _M0L4portS2520 = _M0L6configS992->$0;
  _M0L14_2aboxed__selfS2521
  = (struct _M0Y3Int*)moonbit_malloc(sizeof(struct _M0Y3Int));
  Moonbit_object_header(_M0L14_2aboxed__selfS2521)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y3Int) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2521->$0 = _M0L4portS2520;
  _M0L6_2atmpS2513
  = (struct _M0TPB4Show){
    _M0FP076Int_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2521
  };
  _M0L6_2atmpS2516 = (moonbit_string_t)moonbit_string_literal_75.data;
  _M0L6_2atmpS2517 = (moonbit_string_t)moonbit_string_literal_76.data;
  _M0L6_2atmpS2518 = 0;
  _M0L6_2atmpS2519 = 0;
  _M0L6_2atmpS2515 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2515[0] = _M0L6_2atmpS2516;
  _M0L6_2atmpS2515[1] = _M0L6_2atmpS2517;
  _M0L6_2atmpS2515[2] = _M0L6_2atmpS2518;
  _M0L6_2atmpS2515[3] = _M0L6_2atmpS2519;
  _M0L6_2atmpS2514
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2514)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2514->$0 = _M0L6_2atmpS2515;
  _M0L6_2atmpS2514->$1 = 4;
  #line 64 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3437
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2513, (moonbit_string_t)moonbit_string_literal_24.data, (moonbit_string_t)moonbit_string_literal_77.data, _M0L6_2atmpS2514);
  if (_tmp_3437.tag) {
    int32_t const _M0L5_2aokS2522 = _tmp_3437.data.ok;
  } else {
    void* const _M0L6_2aerrS2523 = _tmp_3437.data.err;
    struct moonbit_result_0 _result_3438;
    moonbit_decref(_M0L6configS992);
    _result_3438.tag = 0;
    _result_3438.data.err = _M0L6_2aerrS2523;
    return _result_3438;
  }
  _M0L8_2afieldS2910 = _M0L6configS992->$1;
  _M0L6_2acntS3261 = Moonbit_object_header(_M0L6configS992)->rc;
  if (_M0L6_2acntS3261 > 1) {
    int32_t _M0L11_2anew__cntS3262 = _M0L6_2acntS3261 - 1;
    Moonbit_object_header(_M0L6configS992)->rc = _M0L11_2anew__cntS3262;
    moonbit_incref(_M0L8_2afieldS2910);
  } else if (_M0L6_2acntS3261 == 1) {
    #line 65 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
    moonbit_free(_M0L6configS992);
  }
  _M0L4hostS2531 = _M0L8_2afieldS2910;
  _M0L6_2atmpS2524
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L4hostS2531
  };
  _M0L6_2atmpS2527 = (moonbit_string_t)moonbit_string_literal_78.data;
  _M0L6_2atmpS2528 = (moonbit_string_t)moonbit_string_literal_79.data;
  _M0L6_2atmpS2529 = 0;
  _M0L6_2atmpS2530 = 0;
  _M0L6_2atmpS2526 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2526[0] = _M0L6_2atmpS2527;
  _M0L6_2atmpS2526[1] = _M0L6_2atmpS2528;
  _M0L6_2atmpS2526[2] = _M0L6_2atmpS2529;
  _M0L6_2atmpS2526[3] = _M0L6_2atmpS2530;
  _M0L6_2atmpS2525
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2525)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2525->$0 = _M0L6_2atmpS2526;
  _M0L6_2atmpS2525->$1 = 4;
  #line 65 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2524, (moonbit_string_t)moonbit_string_literal_9.data, (moonbit_string_t)moonbit_string_literal_80.data, _M0L6_2atmpS2525);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__4(
  
) {
  moonbit_string_t* _M0L6_2atmpS2512;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2504;
  struct _M0TUssE** _M0L7_2abindS991;
  struct _M0TUssE** _M0L6_2atmpS2511;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS2510;
  struct _M0TPB3MapGssE* _M0L6_2atmpS2505;
  moonbit_string_t _M0L6_2atmpS2506;
  void* _M0L6_2atmpS2507;
  moonbit_string_t* _M0L6_2atmpS2509;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2508;
  struct _M0TP38clawteam8clawteam6config11AgentConfig* _M0L6configS990;
  moonbit_string_t _M0L8_2afieldS2913;
  moonbit_string_t _M0L2idS2481;
  struct _M0TPB4Show _M0L6_2atmpS2474;
  moonbit_string_t _M0L6_2atmpS2477;
  moonbit_string_t _M0L6_2atmpS2478;
  moonbit_string_t _M0L6_2atmpS2479;
  moonbit_string_t _M0L6_2atmpS2480;
  moonbit_string_t* _M0L6_2atmpS2476;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2475;
  struct moonbit_result_0 _tmp_3439;
  moonbit_string_t _M0L8_2afieldS2912;
  moonbit_string_t _M0L4nameS2491;
  struct _M0TPB4Show _M0L6_2atmpS2484;
  moonbit_string_t _M0L6_2atmpS2487;
  moonbit_string_t _M0L6_2atmpS2488;
  moonbit_string_t _M0L6_2atmpS2489;
  moonbit_string_t _M0L6_2atmpS2490;
  moonbit_string_t* _M0L6_2atmpS2486;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2485;
  struct moonbit_result_0 _tmp_3441;
  int32_t _M0L8_2afieldS2911;
  int32_t _M0L4roleS2503;
  int32_t _M0L6_2atmpS2501;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2502;
  struct _M0TPB4Show _M0L6_2atmpS2494;
  moonbit_string_t _M0L6_2atmpS2497;
  moonbit_string_t _M0L6_2atmpS2498;
  moonbit_string_t _M0L6_2atmpS2499;
  moonbit_string_t _M0L6_2atmpS2500;
  moonbit_string_t* _M0L6_2atmpS2496;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2495;
  #line 40 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2512 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS2504
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2504)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2504->$0 = _M0L6_2atmpS2512;
  _M0L6_2atmpS2504->$1 = 0;
  _M0L7_2abindS991 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2511 = _M0L7_2abindS991;
  _M0L6_2atmpS2510 = (struct _M0TPB9ArrayViewGUssEE){0, 0, _M0L6_2atmpS2511};
  #line 47 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2505 = _M0MPB3Map11from__arrayGssE(_M0L6_2atmpS2510);
  _M0L6_2atmpS2506 = 0;
  _M0L6_2atmpS2507 = 0;
  _M0L6_2atmpS2509 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS2508
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2508)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2508->$0 = _M0L6_2atmpS2509;
  _M0L6_2atmpS2508->$1 = 0;
  _M0L6configS990
  = (struct _M0TP38clawteam8clawteam6config11AgentConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config11AgentConfig));
  Moonbit_object_header(_M0L6configS990)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config11AgentConfig, $0) >> 2, 8, 0);
  _M0L6configS990->$0 = (moonbit_string_t)moonbit_string_literal_81.data;
  _M0L6configS990->$1 = (moonbit_string_t)moonbit_string_literal_82.data;
  _M0L6configS990->$2 = 0;
  _M0L6configS990->$3 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6configS990->$4 = _M0L6_2atmpS2504;
  _M0L6configS990->$5 = _M0L6_2atmpS2505;
  _M0L6configS990->$6 = _M0L6_2atmpS2506;
  _M0L6configS990->$7 = 1;
  _M0L6configS990->$8 = _M0L6_2atmpS2507;
  _M0L6configS990->$9 = _M0L6_2atmpS2508;
  _M0L6configS990->$10 = 1;
  _M0L8_2afieldS2913 = _M0L6configS990->$0;
  _M0L2idS2481 = _M0L8_2afieldS2913;
  moonbit_incref(_M0L2idS2481);
  _M0L6_2atmpS2474
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L2idS2481
  };
  _M0L6_2atmpS2477 = (moonbit_string_t)moonbit_string_literal_83.data;
  _M0L6_2atmpS2478 = (moonbit_string_t)moonbit_string_literal_84.data;
  _M0L6_2atmpS2479 = 0;
  _M0L6_2atmpS2480 = 0;
  _M0L6_2atmpS2476 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2476[0] = _M0L6_2atmpS2477;
  _M0L6_2atmpS2476[1] = _M0L6_2atmpS2478;
  _M0L6_2atmpS2476[2] = _M0L6_2atmpS2479;
  _M0L6_2atmpS2476[3] = _M0L6_2atmpS2480;
  _M0L6_2atmpS2475
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2475)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2475->$0 = _M0L6_2atmpS2476;
  _M0L6_2atmpS2475->$1 = 4;
  #line 54 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3439
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2474, (moonbit_string_t)moonbit_string_literal_81.data, (moonbit_string_t)moonbit_string_literal_85.data, _M0L6_2atmpS2475);
  if (_tmp_3439.tag) {
    int32_t const _M0L5_2aokS2482 = _tmp_3439.data.ok;
  } else {
    void* const _M0L6_2aerrS2483 = _tmp_3439.data.err;
    struct moonbit_result_0 _result_3440;
    moonbit_decref(_M0L6configS990);
    _result_3440.tag = 0;
    _result_3440.data.err = _M0L6_2aerrS2483;
    return _result_3440;
  }
  _M0L8_2afieldS2912 = _M0L6configS990->$1;
  _M0L4nameS2491 = _M0L8_2afieldS2912;
  moonbit_incref(_M0L4nameS2491);
  _M0L6_2atmpS2484
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L4nameS2491
  };
  _M0L6_2atmpS2487 = (moonbit_string_t)moonbit_string_literal_86.data;
  _M0L6_2atmpS2488 = (moonbit_string_t)moonbit_string_literal_87.data;
  _M0L6_2atmpS2489 = 0;
  _M0L6_2atmpS2490 = 0;
  _M0L6_2atmpS2486 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2486[0] = _M0L6_2atmpS2487;
  _M0L6_2atmpS2486[1] = _M0L6_2atmpS2488;
  _M0L6_2atmpS2486[2] = _M0L6_2atmpS2489;
  _M0L6_2atmpS2486[3] = _M0L6_2atmpS2490;
  _M0L6_2atmpS2485
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2485)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2485->$0 = _M0L6_2atmpS2486;
  _M0L6_2atmpS2485->$1 = 4;
  #line 55 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3441
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2484, (moonbit_string_t)moonbit_string_literal_82.data, (moonbit_string_t)moonbit_string_literal_88.data, _M0L6_2atmpS2485);
  if (_tmp_3441.tag) {
    int32_t const _M0L5_2aokS2492 = _tmp_3441.data.ok;
  } else {
    void* const _M0L6_2aerrS2493 = _tmp_3441.data.err;
    struct moonbit_result_0 _result_3442;
    moonbit_decref(_M0L6configS990);
    _result_3442.tag = 0;
    _result_3442.data.err = _M0L6_2aerrS2493;
    return _result_3442;
  }
  _M0L8_2afieldS2911 = _M0L6configS990->$2;
  moonbit_decref(_M0L6configS990);
  _M0L4roleS2503 = _M0L8_2afieldS2911;
  #line 56 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2501
  = _M0IP38clawteam8clawteam5types9AgentRolePB2Eq5equal(_M0L4roleS2503, 0);
  _M0L14_2aboxed__selfS2502
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2502)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2502->$0 = _M0L6_2atmpS2501;
  _M0L6_2atmpS2494
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2502
  };
  _M0L6_2atmpS2497 = (moonbit_string_t)moonbit_string_literal_89.data;
  _M0L6_2atmpS2498 = (moonbit_string_t)moonbit_string_literal_90.data;
  _M0L6_2atmpS2499 = 0;
  _M0L6_2atmpS2500 = 0;
  _M0L6_2atmpS2496 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2496[0] = _M0L6_2atmpS2497;
  _M0L6_2atmpS2496[1] = _M0L6_2atmpS2498;
  _M0L6_2atmpS2496[2] = _M0L6_2atmpS2499;
  _M0L6_2atmpS2496[3] = _M0L6_2atmpS2500;
  _M0L6_2atmpS2495
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2495)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2495->$0 = _M0L6_2atmpS2496;
  _M0L6_2atmpS2495->$1 = 4;
  #line 56 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2494, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_91.data, _M0L6_2atmpS2495);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__3(
  
) {
  moonbit_string_t* _M0L6_2atmpS2473;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2463;
  struct _M0TUssE** _M0L7_2abindS989;
  struct _M0TUssE** _M0L6_2atmpS2472;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS2471;
  struct _M0TPB3MapGssE* _M0L6_2atmpS2464;
  moonbit_string_t _M0L6_2atmpS2465;
  moonbit_string_t* _M0L6_2atmpS2470;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2466;
  moonbit_string_t* _M0L6_2atmpS2469;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2467;
  struct _M0TP38clawteam8clawteam6config14OutputPatterns* _M0L6_2atmpS2468;
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L6configS988;
  int32_t _M0L10tool__typeS2452;
  int32_t _M0L6_2atmpS2450;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2451;
  struct _M0TPB4Show _M0L6_2atmpS2443;
  moonbit_string_t _M0L6_2atmpS2446;
  moonbit_string_t _M0L6_2atmpS2447;
  moonbit_string_t _M0L6_2atmpS2448;
  moonbit_string_t _M0L6_2atmpS2449;
  moonbit_string_t* _M0L6_2atmpS2445;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2444;
  struct moonbit_result_0 _tmp_3443;
  moonbit_string_t _M0L8_2afieldS2914;
  int32_t _M0L6_2acntS3263;
  moonbit_string_t _M0L7commandS2462;
  struct _M0TPB4Show _M0L6_2atmpS2455;
  moonbit_string_t _M0L6_2atmpS2458;
  moonbit_string_t _M0L6_2atmpS2459;
  moonbit_string_t _M0L6_2atmpS2460;
  moonbit_string_t _M0L6_2atmpS2461;
  moonbit_string_t* _M0L6_2atmpS2457;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2456;
  #line 24 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2473 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS2463
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2463)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2463->$0 = _M0L6_2atmpS2473;
  _M0L6_2atmpS2463->$1 = 0;
  _M0L7_2abindS989 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2472 = _M0L7_2abindS989;
  _M0L6_2atmpS2471 = (struct _M0TPB9ArrayViewGUssEE){0, 0, _M0L6_2atmpS2472};
  #line 29 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2464 = _M0MPB3Map11from__arrayGssE(_M0L6_2atmpS2471);
  _M0L6_2atmpS2465 = 0;
  _M0L6_2atmpS2470 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2470[0] = (moonbit_string_t)moonbit_string_literal_92.data;
  _M0L6_2atmpS2466
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2466)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2466->$0 = _M0L6_2atmpS2470;
  _M0L6_2atmpS2466->$1 = 1;
  _M0L6_2atmpS2469 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS2467
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2467)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2467->$0 = _M0L6_2atmpS2469;
  _M0L6_2atmpS2467->$1 = 0;
  _M0L6_2atmpS2468 = 0;
  _M0L6configS988
  = (struct _M0TP38clawteam8clawteam6config13CliToolConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config13CliToolConfig));
  Moonbit_object_header(_M0L6configS988)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config13CliToolConfig, $1) >> 2, 7, 0);
  _M0L6configS988->$0 = 0;
  _M0L6configS988->$1 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L6configS988->$2 = _M0L6_2atmpS2463;
  _M0L6configS988->$3 = _M0L6_2atmpS2464;
  _M0L6configS988->$4 = _M0L6_2atmpS2465;
  _M0L6configS988->$5 = _M0L6_2atmpS2466;
  _M0L6configS988->$6 = _M0L6_2atmpS2467;
  _M0L6configS988->$7 = _M0L6_2atmpS2468;
  _M0L6configS988->$8 = 4294967296ll;
  _M0L10tool__typeS2452 = _M0L6configS988->$0;
  #line 36 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2450
  = _M0IP38clawteam8clawteam5types11CliToolTypePB2Eq5equal(_M0L10tool__typeS2452, 0);
  _M0L14_2aboxed__selfS2451
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2451)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2451->$0 = _M0L6_2atmpS2450;
  _M0L6_2atmpS2443
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2451
  };
  _M0L6_2atmpS2446 = (moonbit_string_t)moonbit_string_literal_93.data;
  _M0L6_2atmpS2447 = (moonbit_string_t)moonbit_string_literal_94.data;
  _M0L6_2atmpS2448 = 0;
  _M0L6_2atmpS2449 = 0;
  _M0L6_2atmpS2445 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2445[0] = _M0L6_2atmpS2446;
  _M0L6_2atmpS2445[1] = _M0L6_2atmpS2447;
  _M0L6_2atmpS2445[2] = _M0L6_2atmpS2448;
  _M0L6_2atmpS2445[3] = _M0L6_2atmpS2449;
  _M0L6_2atmpS2444
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2444)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2444->$0 = _M0L6_2atmpS2445;
  _M0L6_2atmpS2444->$1 = 4;
  #line 36 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3443
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2443, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_95.data, _M0L6_2atmpS2444);
  if (_tmp_3443.tag) {
    int32_t const _M0L5_2aokS2453 = _tmp_3443.data.ok;
  } else {
    void* const _M0L6_2aerrS2454 = _tmp_3443.data.err;
    struct moonbit_result_0 _result_3444;
    moonbit_decref(_M0L6configS988);
    _result_3444.tag = 0;
    _result_3444.data.err = _M0L6_2aerrS2454;
    return _result_3444;
  }
  _M0L8_2afieldS2914 = _M0L6configS988->$1;
  _M0L6_2acntS3263 = Moonbit_object_header(_M0L6configS988)->rc;
  if (_M0L6_2acntS3263 > 1) {
    int32_t _M0L11_2anew__cntS3270 = _M0L6_2acntS3263 - 1;
    Moonbit_object_header(_M0L6configS988)->rc = _M0L11_2anew__cntS3270;
    moonbit_incref(_M0L8_2afieldS2914);
  } else if (_M0L6_2acntS3263 == 1) {
    struct _M0TP38clawteam8clawteam6config14OutputPatterns* _M0L8_2afieldS3269 =
      _M0L6configS988->$7;
    struct _M0TPB5ArrayGsE* _M0L8_2afieldS3268;
    struct _M0TPB5ArrayGsE* _M0L8_2afieldS3267;
    moonbit_string_t _M0L8_2afieldS3266;
    struct _M0TPB3MapGssE* _M0L8_2afieldS3265;
    struct _M0TPB5ArrayGsE* _M0L8_2afieldS3264;
    if (_M0L8_2afieldS3269) {
      moonbit_decref(_M0L8_2afieldS3269);
    }
    _M0L8_2afieldS3268 = _M0L6configS988->$6;
    moonbit_decref(_M0L8_2afieldS3268);
    _M0L8_2afieldS3267 = _M0L6configS988->$5;
    moonbit_decref(_M0L8_2afieldS3267);
    _M0L8_2afieldS3266 = _M0L6configS988->$4;
    if (_M0L8_2afieldS3266) {
      moonbit_decref(_M0L8_2afieldS3266);
    }
    _M0L8_2afieldS3265 = _M0L6configS988->$3;
    moonbit_decref(_M0L8_2afieldS3265);
    _M0L8_2afieldS3264 = _M0L6configS988->$2;
    moonbit_decref(_M0L8_2afieldS3264);
    #line 37 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
    moonbit_free(_M0L6configS988);
  }
  _M0L7commandS2462 = _M0L8_2afieldS2914;
  _M0L6_2atmpS2455
  = (struct _M0TPB4Show){
    _M0FP079String_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L7commandS2462
  };
  _M0L6_2atmpS2458 = (moonbit_string_t)moonbit_string_literal_96.data;
  _M0L6_2atmpS2459 = (moonbit_string_t)moonbit_string_literal_97.data;
  _M0L6_2atmpS2460 = 0;
  _M0L6_2atmpS2461 = 0;
  _M0L6_2atmpS2457 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2457[0] = _M0L6_2atmpS2458;
  _M0L6_2atmpS2457[1] = _M0L6_2atmpS2459;
  _M0L6_2atmpS2457[2] = _M0L6_2atmpS2460;
  _M0L6_2atmpS2457[3] = _M0L6_2atmpS2461;
  _M0L6_2atmpS2456
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2456)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2456->$0 = _M0L6_2atmpS2457;
  _M0L6_2atmpS2456->$1 = 4;
  #line 37 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2455, (moonbit_string_t)moonbit_string_literal_29.data, (moonbit_string_t)moonbit_string_literal_98.data, _M0L6_2atmpS2456);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__2(
  
) {
  int32_t _M0L12round__robinS986;
  int32_t _M0L13least__loadedS987;
  int32_t _M0L6_2atmpS2430;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2431;
  struct _M0TPB4Show _M0L6_2atmpS2423;
  moonbit_string_t _M0L6_2atmpS2426;
  moonbit_string_t _M0L6_2atmpS2427;
  moonbit_string_t _M0L6_2atmpS2428;
  moonbit_string_t _M0L6_2atmpS2429;
  moonbit_string_t* _M0L6_2atmpS2425;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2424;
  struct moonbit_result_0 _tmp_3445;
  int32_t _M0L6_2atmpS2441;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2442;
  struct _M0TPB4Show _M0L6_2atmpS2434;
  moonbit_string_t _M0L6_2atmpS2437;
  moonbit_string_t _M0L6_2atmpS2438;
  moonbit_string_t _M0L6_2atmpS2439;
  moonbit_string_t _M0L6_2atmpS2440;
  moonbit_string_t* _M0L6_2atmpS2436;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2435;
  #line 17 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L12round__robinS986 = 0;
  _M0L13least__loadedS987 = 1;
  #line 20 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2430
  = _M0IP38clawteam8clawteam5types16DispatchStrategyPB2Eq5equal(_M0L12round__robinS986, _M0L12round__robinS986);
  _M0L14_2aboxed__selfS2431
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2431)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2431->$0 = _M0L6_2atmpS2430;
  _M0L6_2atmpS2423
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2431
  };
  _M0L6_2atmpS2426 = (moonbit_string_t)moonbit_string_literal_99.data;
  _M0L6_2atmpS2427 = (moonbit_string_t)moonbit_string_literal_100.data;
  _M0L6_2atmpS2428 = 0;
  _M0L6_2atmpS2429 = 0;
  _M0L6_2atmpS2425 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2425[0] = _M0L6_2atmpS2426;
  _M0L6_2atmpS2425[1] = _M0L6_2atmpS2427;
  _M0L6_2atmpS2425[2] = _M0L6_2atmpS2428;
  _M0L6_2atmpS2425[3] = _M0L6_2atmpS2429;
  _M0L6_2atmpS2424
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2424)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2424->$0 = _M0L6_2atmpS2425;
  _M0L6_2atmpS2424->$1 = 4;
  #line 20 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3445
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2423, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_101.data, _M0L6_2atmpS2424);
  if (_tmp_3445.tag) {
    int32_t const _M0L5_2aokS2432 = _tmp_3445.data.ok;
  } else {
    void* const _M0L6_2aerrS2433 = _tmp_3445.data.err;
    struct moonbit_result_0 _result_3446;
    _result_3446.tag = 0;
    _result_3446.data.err = _M0L6_2aerrS2433;
    return _result_3446;
  }
  #line 21 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2441
  = _M0IP38clawteam8clawteam5types16DispatchStrategyPB2Eq5equal(_M0L12round__robinS986, _M0L13least__loadedS987);
  _M0L14_2aboxed__selfS2442
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2442)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2442->$0 = _M0L6_2atmpS2441;
  _M0L6_2atmpS2434
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2442
  };
  _M0L6_2atmpS2437 = (moonbit_string_t)moonbit_string_literal_102.data;
  _M0L6_2atmpS2438 = (moonbit_string_t)moonbit_string_literal_103.data;
  _M0L6_2atmpS2439 = 0;
  _M0L6_2atmpS2440 = 0;
  _M0L6_2atmpS2436 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2436[0] = _M0L6_2atmpS2437;
  _M0L6_2atmpS2436[1] = _M0L6_2atmpS2438;
  _M0L6_2atmpS2436[2] = _M0L6_2atmpS2439;
  _M0L6_2atmpS2436[3] = _M0L6_2atmpS2440;
  _M0L6_2atmpS2435
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2435)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2435->$0 = _M0L6_2atmpS2436;
  _M0L6_2atmpS2435->$1 = 4;
  #line 21 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2434, (moonbit_string_t)moonbit_string_literal_104.data, (moonbit_string_t)moonbit_string_literal_105.data, _M0L6_2atmpS2435);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__1(
  
) {
  int32_t _M0L9assistantS984;
  int32_t _M0L6workerS985;
  int32_t _M0L6_2atmpS2410;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2411;
  struct _M0TPB4Show _M0L6_2atmpS2403;
  moonbit_string_t _M0L6_2atmpS2406;
  moonbit_string_t _M0L6_2atmpS2407;
  moonbit_string_t _M0L6_2atmpS2408;
  moonbit_string_t _M0L6_2atmpS2409;
  moonbit_string_t* _M0L6_2atmpS2405;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2404;
  struct moonbit_result_0 _tmp_3447;
  int32_t _M0L6_2atmpS2421;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2422;
  struct _M0TPB4Show _M0L6_2atmpS2414;
  moonbit_string_t _M0L6_2atmpS2417;
  moonbit_string_t _M0L6_2atmpS2418;
  moonbit_string_t _M0L6_2atmpS2419;
  moonbit_string_t _M0L6_2atmpS2420;
  moonbit_string_t* _M0L6_2atmpS2416;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2415;
  #line 10 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L9assistantS984 = 0;
  _M0L6workerS985 = 1;
  #line 13 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2410
  = _M0IP38clawteam8clawteam5types9AgentRolePB2Eq5equal(_M0L9assistantS984, _M0L9assistantS984);
  _M0L14_2aboxed__selfS2411
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2411)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2411->$0 = _M0L6_2atmpS2410;
  _M0L6_2atmpS2403
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2411
  };
  _M0L6_2atmpS2406 = (moonbit_string_t)moonbit_string_literal_106.data;
  _M0L6_2atmpS2407 = (moonbit_string_t)moonbit_string_literal_107.data;
  _M0L6_2atmpS2408 = 0;
  _M0L6_2atmpS2409 = 0;
  _M0L6_2atmpS2405 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2405[0] = _M0L6_2atmpS2406;
  _M0L6_2atmpS2405[1] = _M0L6_2atmpS2407;
  _M0L6_2atmpS2405[2] = _M0L6_2atmpS2408;
  _M0L6_2atmpS2405[3] = _M0L6_2atmpS2409;
  _M0L6_2atmpS2404
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2404)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2404->$0 = _M0L6_2atmpS2405;
  _M0L6_2atmpS2404->$1 = 4;
  #line 13 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3447
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2403, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_108.data, _M0L6_2atmpS2404);
  if (_tmp_3447.tag) {
    int32_t const _M0L5_2aokS2412 = _tmp_3447.data.ok;
  } else {
    void* const _M0L6_2aerrS2413 = _tmp_3447.data.err;
    struct moonbit_result_0 _result_3448;
    _result_3448.tag = 0;
    _result_3448.data.err = _M0L6_2aerrS2413;
    return _result_3448;
  }
  #line 14 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2421
  = _M0IP38clawteam8clawteam5types9AgentRolePB2Eq5equal(_M0L9assistantS984, _M0L6workerS985);
  _M0L14_2aboxed__selfS2422
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2422)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2422->$0 = _M0L6_2atmpS2421;
  _M0L6_2atmpS2414
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2422
  };
  _M0L6_2atmpS2417 = (moonbit_string_t)moonbit_string_literal_109.data;
  _M0L6_2atmpS2418 = (moonbit_string_t)moonbit_string_literal_110.data;
  _M0L6_2atmpS2419 = 0;
  _M0L6_2atmpS2420 = 0;
  _M0L6_2atmpS2416 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2416[0] = _M0L6_2atmpS2417;
  _M0L6_2atmpS2416[1] = _M0L6_2atmpS2418;
  _M0L6_2atmpS2416[2] = _M0L6_2atmpS2419;
  _M0L6_2atmpS2416[3] = _M0L6_2atmpS2420;
  _M0L6_2atmpS2415
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2415)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2415->$0 = _M0L6_2atmpS2416;
  _M0L6_2atmpS2415->$1 = 4;
  #line 14 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2414, (moonbit_string_t)moonbit_string_literal_104.data, (moonbit_string_t)moonbit_string_literal_111.data, _M0L6_2atmpS2415);
}

struct moonbit_result_0 _M0FP38clawteam8clawteam22config__blackbox__test41____test__74797065735f746573742e6d6274__0(
  
) {
  int32_t _M0L6claudeS982;
  int32_t _M0L5codexS983;
  int32_t _M0L6_2atmpS2390;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2391;
  struct _M0TPB4Show _M0L6_2atmpS2383;
  moonbit_string_t _M0L6_2atmpS2386;
  moonbit_string_t _M0L6_2atmpS2387;
  moonbit_string_t _M0L6_2atmpS2388;
  moonbit_string_t _M0L6_2atmpS2389;
  moonbit_string_t* _M0L6_2atmpS2385;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2384;
  struct moonbit_result_0 _tmp_3449;
  int32_t _M0L6_2atmpS2401;
  struct _M0Y4Bool* _M0L14_2aboxed__selfS2402;
  struct _M0TPB4Show _M0L6_2atmpS2394;
  moonbit_string_t _M0L6_2atmpS2397;
  moonbit_string_t _M0L6_2atmpS2398;
  moonbit_string_t _M0L6_2atmpS2399;
  moonbit_string_t _M0L6_2atmpS2400;
  moonbit_string_t* _M0L6_2atmpS2396;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L6_2atmpS2395;
  #line 3 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6claudeS982 = 0;
  _M0L5codexS983 = 1;
  #line 6 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2390
  = _M0IP38clawteam8clawteam5types11CliToolTypePB2Eq5equal(_M0L6claudeS982, _M0L6claudeS982);
  _M0L14_2aboxed__selfS2391
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2391)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2391->$0 = _M0L6_2atmpS2390;
  _M0L6_2atmpS2383
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2391
  };
  _M0L6_2atmpS2386 = (moonbit_string_t)moonbit_string_literal_112.data;
  _M0L6_2atmpS2387 = (moonbit_string_t)moonbit_string_literal_113.data;
  _M0L6_2atmpS2388 = 0;
  _M0L6_2atmpS2389 = 0;
  _M0L6_2atmpS2385 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2385[0] = _M0L6_2atmpS2386;
  _M0L6_2atmpS2385[1] = _M0L6_2atmpS2387;
  _M0L6_2atmpS2385[2] = _M0L6_2atmpS2388;
  _M0L6_2atmpS2385[3] = _M0L6_2atmpS2389;
  _M0L6_2atmpS2384
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2384)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2384->$0 = _M0L6_2atmpS2385;
  _M0L6_2atmpS2384->$1 = 4;
  #line 6 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _tmp_3449
  = _M0FPB15inspect_2einner(_M0L6_2atmpS2383, (moonbit_string_t)moonbit_string_literal_20.data, (moonbit_string_t)moonbit_string_literal_114.data, _M0L6_2atmpS2384);
  if (_tmp_3449.tag) {
    int32_t const _M0L5_2aokS2392 = _tmp_3449.data.ok;
  } else {
    void* const _M0L6_2aerrS2393 = _tmp_3449.data.err;
    struct moonbit_result_0 _result_3450;
    _result_3450.tag = 0;
    _result_3450.data.err = _M0L6_2aerrS2393;
    return _result_3450;
  }
  #line 7 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  _M0L6_2atmpS2401
  = _M0IP38clawteam8clawteam5types11CliToolTypePB2Eq5equal(_M0L6claudeS982, _M0L5codexS983);
  _M0L14_2aboxed__selfS2402
  = (struct _M0Y4Bool*)moonbit_malloc(sizeof(struct _M0Y4Bool));
  Moonbit_object_header(_M0L14_2aboxed__selfS2402)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0Y4Bool) >> 2, 0, 0);
  _M0L14_2aboxed__selfS2402->$0 = _M0L6_2atmpS2401;
  _M0L6_2atmpS2394
  = (struct _M0TPB4Show){
    _M0FP077Bool_2eas___40moonbitlang_2fcore_2fbuiltin_2eShow_2estatic__method__table__id,
      _M0L14_2aboxed__selfS2402
  };
  _M0L6_2atmpS2397 = (moonbit_string_t)moonbit_string_literal_115.data;
  _M0L6_2atmpS2398 = (moonbit_string_t)moonbit_string_literal_116.data;
  _M0L6_2atmpS2399 = 0;
  _M0L6_2atmpS2400 = 0;
  _M0L6_2atmpS2396 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2396[0] = _M0L6_2atmpS2397;
  _M0L6_2atmpS2396[1] = _M0L6_2atmpS2398;
  _M0L6_2atmpS2396[2] = _M0L6_2atmpS2399;
  _M0L6_2atmpS2396[3] = _M0L6_2atmpS2400;
  _M0L6_2atmpS2395
  = (struct _M0TPB5ArrayGORPB9SourceLocE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGORPB9SourceLocE));
  Moonbit_object_header(_M0L6_2atmpS2395)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGORPB9SourceLocE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2395->$0 = _M0L6_2atmpS2396;
  _M0L6_2atmpS2395->$1 = 4;
  #line 7 "E:\\moonbit\\clawteam\\config\\types_test.mbt"
  return _M0FPB15inspect_2einner(_M0L6_2atmpS2394, (moonbit_string_t)moonbit_string_literal_104.data, (moonbit_string_t)moonbit_string_literal_117.data, _M0L6_2atmpS2395);
}

void* _M0FP38clawteam8clawteam6config16validate__config(
  struct _M0TP38clawteam8clawteam6config14ClawTeamConfig* _M0L6configS969
) {
  struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0L8_2afieldS2935;
  struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0L7gatewayS2359;
  int32_t _M0L8_2afieldS2934;
  int32_t _M0L4portS2358;
  int32_t _if__result_3451;
  struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0L8_2afieldS2931;
  struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0L7gatewayS2363;
  moonbit_string_t _M0L8_2afieldS2930;
  moonbit_string_t _M0L4hostS2362;
  int32_t _M0L6_2atmpS2929;
  int32_t _M0L6_2atmpS2361;
  moonbit_string_t* _M0L6_2atmpS2382;
  struct _M0TPB5ArrayGsE* _M0L10agent__idsS970;
  struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE* _M0L8_2afieldS2928;
  struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE* _M0L7_2abindS971;
  int32_t _M0L7_2abindS972;
  int32_t _M0L2__S973;
  moonbit_string_t* _M0L6_2atmpS2381;
  struct _M0TPB5ArrayGsE* _M0L10skill__idsS976;
  struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE* _M0L8_2afieldS2921;
  struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE* _M0L7_2abindS977;
  int32_t _M0L7_2abindS978;
  int32_t _M0L2__S979;
  void* _block_3458;
  #line 148 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L8_2afieldS2935 = _M0L6configS969->$1;
  _M0L7gatewayS2359 = _M0L8_2afieldS2935;
  _M0L8_2afieldS2934 = _M0L7gatewayS2359->$0;
  _M0L4portS2358 = _M0L8_2afieldS2934;
  if (_M0L4portS2358 < 1) {
    _if__result_3451 = 1;
  } else {
    struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0L8_2afieldS2933 =
      _M0L6configS969->$1;
    struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0L7gatewayS2357 =
      _M0L8_2afieldS2933;
    int32_t _M0L8_2afieldS2932 = _M0L7gatewayS2357->$0;
    int32_t _M0L4portS2356 = _M0L8_2afieldS2932;
    _if__result_3451 = _M0L4portS2356 > 65535;
  }
  if (_if__result_3451) {
    void* _M0L13ConfigInvalidS2360;
    void* _block_3452;
    moonbit_decref(_M0L6configS969);
    _M0L13ConfigInvalidS2360
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid));
    Moonbit_object_header(_M0L13ConfigInvalidS2360)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid, $0) >> 2, 1, 1);
    ((struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid*)_M0L13ConfigInvalidS2360)->$0
    = (moonbit_string_t)moonbit_string_literal_118.data;
    _block_3452
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err));
    Moonbit_object_header(_block_3452)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err*)_block_3452)->$0
    = _M0L13ConfigInvalidS2360;
    return _block_3452;
  }
  _M0L8_2afieldS2931 = _M0L6configS969->$1;
  _M0L7gatewayS2363 = _M0L8_2afieldS2931;
  _M0L8_2afieldS2930 = _M0L7gatewayS2363->$1;
  _M0L4hostS2362 = _M0L8_2afieldS2930;
  _M0L6_2atmpS2929 = Moonbit_array_length(_M0L4hostS2362);
  _M0L6_2atmpS2361 = _M0L6_2atmpS2929;
  if (_M0L6_2atmpS2361 == 0) {
    void* _M0L13ConfigInvalidS2364;
    void* _block_3453;
    moonbit_decref(_M0L6configS969);
    _M0L13ConfigInvalidS2364
    = (void*)moonbit_malloc(sizeof(struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid));
    Moonbit_object_header(_M0L13ConfigInvalidS2364)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid, $0) >> 2, 1, 1);
    ((struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid*)_M0L13ConfigInvalidS2364)->$0
    = (moonbit_string_t)moonbit_string_literal_119.data;
    _block_3453
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err));
    Moonbit_object_header(_block_3453)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err, $0) >> 2, 1, 0);
    ((struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err*)_block_3453)->$0
    = _M0L13ConfigInvalidS2364;
    return _block_3453;
  }
  _M0L6_2atmpS2382 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L10agent__idsS970
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L10agent__idsS970)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L10agent__idsS970->$0 = _M0L6_2atmpS2382;
  _M0L10agent__idsS970->$1 = 0;
  _M0L8_2afieldS2928 = _M0L6configS969->$4;
  _M0L7_2abindS971 = _M0L8_2afieldS2928;
  _M0L7_2abindS972 = _M0L7_2abindS971->$1;
  moonbit_incref(_M0L7_2abindS971);
  _M0L2__S973 = 0;
  while (1) {
    if (_M0L2__S973 < _M0L7_2abindS972) {
      struct _M0TP38clawteam8clawteam6config11AgentConfig** _M0L8_2afieldS2927 =
        _M0L7_2abindS971->$0;
      struct _M0TP38clawteam8clawteam6config11AgentConfig** _M0L3bufS2372 =
        _M0L8_2afieldS2927;
      struct _M0TP38clawteam8clawteam6config11AgentConfig* _M0L6_2atmpS2926 =
        (struct _M0TP38clawteam8clawteam6config11AgentConfig*)_M0L3bufS2372[
          _M0L2__S973
        ];
      struct _M0TP38clawteam8clawteam6config11AgentConfig* _M0L5agentS974 =
        _M0L6_2atmpS2926;
      moonbit_string_t _M0L8_2afieldS2925 = _M0L5agentS974->$0;
      moonbit_string_t _M0L2idS2365 = _M0L8_2afieldS2925;
      moonbit_string_t _M0L8_2afieldS2922;
      int32_t _M0L6_2acntS3271;
      moonbit_string_t _M0L2idS2370;
      int32_t _M0L6_2atmpS2371;
      moonbit_incref(_M0L2idS2365);
      moonbit_incref(_M0L5agentS974);
      moonbit_incref(_M0L10agent__idsS970);
      #line 162 "E:\\moonbit\\clawteam\\config\\loader.mbt"
      if (
        _M0MPC15array5Array8containsGsE(_M0L10agent__idsS970, _M0L2idS2365)
      ) {
        moonbit_string_t _M0L8_2afieldS2924;
        moonbit_string_t _M0L2idS2369;
        moonbit_string_t _M0L6_2atmpS2368;
        moonbit_string_t _M0L6_2atmpS2923;
        moonbit_string_t _M0L6_2atmpS2367;
        void* _M0L13ConfigInvalidS2366;
        void* _block_3455;
        moonbit_decref(_M0L7_2abindS971);
        moonbit_decref(_M0L10agent__idsS970);
        moonbit_decref(_M0L6configS969);
        _M0L8_2afieldS2924 = _M0L5agentS974->$0;
        moonbit_incref(_M0L8_2afieldS2924);
        moonbit_decref(_M0L5agentS974);
        _M0L2idS2369 = _M0L8_2afieldS2924;
        #line 163 "E:\\moonbit\\clawteam\\config\\loader.mbt"
        _M0L6_2atmpS2368
        = _M0IPC16string6StringPB4Show10to__string(_M0L2idS2369);
        #line 163 "E:\\moonbit\\clawteam\\config\\loader.mbt"
        _M0L6_2atmpS2923
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_120.data, _M0L6_2atmpS2368);
        moonbit_decref(_M0L6_2atmpS2368);
        _M0L6_2atmpS2367 = _M0L6_2atmpS2923;
        _M0L13ConfigInvalidS2366
        = (void*)moonbit_malloc(sizeof(struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid));
        Moonbit_object_header(_M0L13ConfigInvalidS2366)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid, $0) >> 2, 1, 1);
        ((struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid*)_M0L13ConfigInvalidS2366)->$0
        = _M0L6_2atmpS2367;
        _block_3455
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err));
        Moonbit_object_header(_block_3455)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err, $0) >> 2, 1, 0);
        ((struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err*)_block_3455)->$0
        = _M0L13ConfigInvalidS2366;
        return _block_3455;
      }
      _M0L8_2afieldS2922 = _M0L5agentS974->$0;
      _M0L6_2acntS3271 = Moonbit_object_header(_M0L5agentS974)->rc;
      if (_M0L6_2acntS3271 > 1) {
        int32_t _M0L11_2anew__cntS3279 = _M0L6_2acntS3271 - 1;
        Moonbit_object_header(_M0L5agentS974)->rc = _M0L11_2anew__cntS3279;
        moonbit_incref(_M0L8_2afieldS2922);
      } else if (_M0L6_2acntS3271 == 1) {
        struct _M0TPB5ArrayGsE* _M0L8_2afieldS3278 = _M0L5agentS974->$9;
        void* _M0L8_2afieldS3277;
        moonbit_string_t _M0L8_2afieldS3276;
        struct _M0TPB3MapGssE* _M0L8_2afieldS3275;
        struct _M0TPB5ArrayGsE* _M0L8_2afieldS3274;
        moonbit_string_t _M0L8_2afieldS3273;
        moonbit_string_t _M0L8_2afieldS3272;
        moonbit_decref(_M0L8_2afieldS3278);
        _M0L8_2afieldS3277 = _M0L5agentS974->$8;
        if (_M0L8_2afieldS3277) {
          moonbit_decref(_M0L8_2afieldS3277);
        }
        _M0L8_2afieldS3276 = _M0L5agentS974->$6;
        if (_M0L8_2afieldS3276) {
          moonbit_decref(_M0L8_2afieldS3276);
        }
        _M0L8_2afieldS3275 = _M0L5agentS974->$5;
        moonbit_decref(_M0L8_2afieldS3275);
        _M0L8_2afieldS3274 = _M0L5agentS974->$4;
        moonbit_decref(_M0L8_2afieldS3274);
        _M0L8_2afieldS3273 = _M0L5agentS974->$3;
        moonbit_decref(_M0L8_2afieldS3273);
        _M0L8_2afieldS3272 = _M0L5agentS974->$1;
        moonbit_decref(_M0L8_2afieldS3272);
        #line 165 "E:\\moonbit\\clawteam\\config\\loader.mbt"
        moonbit_free(_M0L5agentS974);
      }
      _M0L2idS2370 = _M0L8_2afieldS2922;
      moonbit_incref(_M0L10agent__idsS970);
      #line 165 "E:\\moonbit\\clawteam\\config\\loader.mbt"
      _M0MPC15array5Array4pushGsE(_M0L10agent__idsS970, _M0L2idS2370);
      _M0L6_2atmpS2371 = _M0L2__S973 + 1;
      _M0L2__S973 = _M0L6_2atmpS2371;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS971);
      moonbit_decref(_M0L10agent__idsS970);
    }
    break;
  }
  _M0L6_2atmpS2381 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L10skill__idsS976
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L10skill__idsS976)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L10skill__idsS976->$0 = _M0L6_2atmpS2381;
  _M0L10skill__idsS976->$1 = 0;
  _M0L8_2afieldS2921 = _M0L6configS969->$5;
  _M0L7_2abindS977 = _M0L8_2afieldS2921;
  _M0L7_2abindS978 = _M0L7_2abindS977->$1;
  moonbit_incref(_M0L7_2abindS977);
  _M0L2__S979 = 0;
  while (1) {
    if (_M0L2__S979 < _M0L7_2abindS978) {
      struct _M0TP38clawteam8clawteam6config5Skill** _M0L8_2afieldS2920 =
        _M0L7_2abindS977->$0;
      struct _M0TP38clawteam8clawteam6config5Skill** _M0L3bufS2380 =
        _M0L8_2afieldS2920;
      struct _M0TP38clawteam8clawteam6config5Skill* _M0L6_2atmpS2919 =
        (struct _M0TP38clawteam8clawteam6config5Skill*)_M0L3bufS2380[
          _M0L2__S979
        ];
      struct _M0TP38clawteam8clawteam6config5Skill* _M0L5skillS980 =
        _M0L6_2atmpS2919;
      moonbit_string_t _M0L8_2afieldS2918 = _M0L5skillS980->$0;
      moonbit_string_t _M0L2idS2373 = _M0L8_2afieldS2918;
      moonbit_string_t _M0L8_2afieldS2915;
      int32_t _M0L6_2acntS3280;
      moonbit_string_t _M0L2idS2378;
      int32_t _M0L6_2atmpS2379;
      moonbit_incref(_M0L2idS2373);
      moonbit_incref(_M0L5skillS980);
      moonbit_incref(_M0L10skill__idsS976);
      #line 171 "E:\\moonbit\\clawteam\\config\\loader.mbt"
      if (
        _M0MPC15array5Array8containsGsE(_M0L10skill__idsS976, _M0L2idS2373)
      ) {
        moonbit_string_t _M0L8_2afieldS2917;
        moonbit_string_t _M0L2idS2377;
        moonbit_string_t _M0L6_2atmpS2376;
        moonbit_string_t _M0L6_2atmpS2916;
        moonbit_string_t _M0L6_2atmpS2375;
        void* _M0L13ConfigInvalidS2374;
        void* _block_3457;
        moonbit_decref(_M0L7_2abindS977);
        moonbit_decref(_M0L10skill__idsS976);
        moonbit_decref(_M0L6configS969);
        _M0L8_2afieldS2917 = _M0L5skillS980->$0;
        moonbit_incref(_M0L8_2afieldS2917);
        moonbit_decref(_M0L5skillS980);
        _M0L2idS2377 = _M0L8_2afieldS2917;
        #line 172 "E:\\moonbit\\clawteam\\config\\loader.mbt"
        _M0L6_2atmpS2376
        = _M0IPC16string6StringPB4Show10to__string(_M0L2idS2377);
        #line 172 "E:\\moonbit\\clawteam\\config\\loader.mbt"
        _M0L6_2atmpS2916
        = moonbit_add_string((moonbit_string_t)moonbit_string_literal_121.data, _M0L6_2atmpS2376);
        moonbit_decref(_M0L6_2atmpS2376);
        _M0L6_2atmpS2375 = _M0L6_2atmpS2916;
        _M0L13ConfigInvalidS2374
        = (void*)moonbit_malloc(sizeof(struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid));
        Moonbit_object_header(_M0L13ConfigInvalidS2374)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid, $0) >> 2, 1, 1);
        ((struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid*)_M0L13ConfigInvalidS2374)->$0
        = _M0L6_2atmpS2375;
        _block_3457
        = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err));
        Moonbit_object_header(_block_3457)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err, $0) >> 2, 1, 0);
        ((struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE3Err*)_block_3457)->$0
        = _M0L13ConfigInvalidS2374;
        return _block_3457;
      }
      _M0L8_2afieldS2915 = _M0L5skillS980->$0;
      _M0L6_2acntS3280 = Moonbit_object_header(_M0L5skillS980)->rc;
      if (_M0L6_2acntS3280 > 1) {
        int32_t _M0L11_2anew__cntS3292 = _M0L6_2acntS3280 - 1;
        Moonbit_object_header(_M0L5skillS980)->rc = _M0L11_2anew__cntS3292;
        moonbit_incref(_M0L8_2afieldS2915);
      } else if (_M0L6_2acntS3280 == 1) {
        struct _M0TP38clawteam8clawteam6config13SkillTemplate* _M0L8_2afieldS3291 =
          _M0L5skillS980->$11;
        struct _M0TPB5ArrayGRP38clawteam8clawteam5types9AgentRoleE* _M0L8_2afieldS3290;
        struct _M0TPB5ArrayGsE* _M0L8_2afieldS3289;
        moonbit_string_t _M0L8_2afieldS3288;
        moonbit_string_t _M0L8_2afieldS3287;
        moonbit_string_t _M0L8_2afieldS3286;
        moonbit_string_t _M0L8_2afieldS3285;
        moonbit_string_t _M0L8_2afieldS3284;
        moonbit_string_t _M0L8_2afieldS3283;
        moonbit_string_t _M0L8_2afieldS3282;
        moonbit_string_t _M0L8_2afieldS3281;
        moonbit_decref(_M0L8_2afieldS3291);
        _M0L8_2afieldS3290 = _M0L5skillS980->$10;
        moonbit_decref(_M0L8_2afieldS3290);
        _M0L8_2afieldS3289 = _M0L5skillS980->$9;
        moonbit_decref(_M0L8_2afieldS3289);
        _M0L8_2afieldS3288 = _M0L5skillS980->$8;
        moonbit_decref(_M0L8_2afieldS3288);
        _M0L8_2afieldS3287 = _M0L5skillS980->$7;
        moonbit_decref(_M0L8_2afieldS3287);
        _M0L8_2afieldS3286 = _M0L5skillS980->$6;
        moonbit_decref(_M0L8_2afieldS3286);
        _M0L8_2afieldS3285 = _M0L5skillS980->$5;
        moonbit_decref(_M0L8_2afieldS3285);
        _M0L8_2afieldS3284 = _M0L5skillS980->$4;
        moonbit_decref(_M0L8_2afieldS3284);
        _M0L8_2afieldS3283 = _M0L5skillS980->$3;
        moonbit_decref(_M0L8_2afieldS3283);
        _M0L8_2afieldS3282 = _M0L5skillS980->$2;
        moonbit_decref(_M0L8_2afieldS3282);
        _M0L8_2afieldS3281 = _M0L5skillS980->$1;
        moonbit_decref(_M0L8_2afieldS3281);
        #line 174 "E:\\moonbit\\clawteam\\config\\loader.mbt"
        moonbit_free(_M0L5skillS980);
      }
      _M0L2idS2378 = _M0L8_2afieldS2915;
      moonbit_incref(_M0L10skill__idsS976);
      #line 174 "E:\\moonbit\\clawteam\\config\\loader.mbt"
      _M0MPC15array5Array4pushGsE(_M0L10skill__idsS976, _M0L2idS2378);
      _M0L6_2atmpS2379 = _M0L2__S979 + 1;
      _M0L2__S979 = _M0L6_2atmpS2379;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS977);
      moonbit_decref(_M0L10skill__idsS976);
    }
    break;
  }
  _block_3458
  = (void*)moonbit_malloc(sizeof(struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE2Ok));
  Moonbit_object_header(_block_3458)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE2Ok, $0) >> 2, 1, 1);
  ((struct _M0DTPC16result6ResultGRP38clawteam8clawteam6config14ClawTeamConfigRP38clawteam8clawteam6errors13ClawTeamErrorE2Ok*)_block_3458)->$0
  = _M0L6configS969;
  return _block_3458;
}

struct _M0TP38clawteam8clawteam6config14ClawTeamConfig* _M0MP38clawteam8clawteam6config12ConfigLoader15default__config(
  moonbit_string_t _M0L4homeS968
) {
  struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0L6_2atmpS2345;
  struct _M0TP38clawteam8clawteam6config15WorkspaceConfig* _M0L6_2atmpS2346;
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS2347;
  struct _M0TP38clawteam8clawteam6config11AgentConfig** _M0L6_2atmpS2355;
  struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE* _M0L6_2atmpS2348;
  struct _M0TP38clawteam8clawteam6config5Skill** _M0L6_2atmpS2354;
  struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE* _M0L6_2atmpS2349;
  struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE** _M0L7_2abindS967;
  struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE** _M0L6_2atmpS2353;
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13ChannelConfigEE _M0L6_2atmpS2352;
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS2350;
  struct _M0TP38clawteam8clawteam6config11AuditConfig* _M0L6_2atmpS2351;
  struct _M0TP38clawteam8clawteam6config14ClawTeamConfig* _block_3459;
  #line 133 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  #line 136 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2345
  = _M0FP38clawteam8clawteam6config24default__gateway__config();
  _M0L6_2atmpS2346 = 0;
  #line 138 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2347 = _M0FP38clawteam8clawteam6config19default__cli__tools();
  _M0L6_2atmpS2355
  = (struct _M0TP38clawteam8clawteam6config11AgentConfig**)moonbit_empty_ref_array;
  _M0L6_2atmpS2348
  = (struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE));
  Moonbit_object_header(_M0L6_2atmpS2348)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP38clawteam8clawteam6config11AgentConfigE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2348->$0 = _M0L6_2atmpS2355;
  _M0L6_2atmpS2348->$1 = 0;
  _M0L6_2atmpS2354
  = (struct _M0TP38clawteam8clawteam6config5Skill**)moonbit_empty_ref_array;
  _M0L6_2atmpS2349
  = (struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE));
  Moonbit_object_header(_M0L6_2atmpS2349)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGRP38clawteam8clawteam6config5SkillE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2349->$0 = _M0L6_2atmpS2354;
  _M0L6_2atmpS2349->$1 = 0;
  _M0L7_2abindS967
  = (struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2353 = _M0L7_2abindS967;
  _M0L6_2atmpS2352
  = (struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13ChannelConfigEE){
    0, 0, _M0L6_2atmpS2353
  };
  #line 141 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2350
  = _M0MPB3Map11from__arrayGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L6_2atmpS2352);
  #line 142 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2351
  = _M0FP38clawteam8clawteam6config22default__audit__config(_M0L4homeS968);
  _block_3459
  = (struct _M0TP38clawteam8clawteam6config14ClawTeamConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config14ClawTeamConfig));
  Moonbit_object_header(_block_3459)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config14ClawTeamConfig, $0) >> 2, 8, 0);
  _block_3459->$0 = (moonbit_string_t)moonbit_string_literal_11.data;
  _block_3459->$1 = _M0L6_2atmpS2345;
  _block_3459->$2 = _M0L6_2atmpS2346;
  _block_3459->$3 = _M0L6_2atmpS2347;
  _block_3459->$4 = _M0L6_2atmpS2348;
  _block_3459->$5 = _M0L6_2atmpS2349;
  _block_3459->$6 = _M0L6_2atmpS2350;
  _block_3459->$7 = _M0L6_2atmpS2351;
  return _block_3459;
}

struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0FP38clawteam8clawteam6config19default__cli__tools(
  
) {
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L6_2atmpS2344;
  struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE* _M0L8_2atupleS2337;
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L6_2atmpS2343;
  struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE* _M0L8_2atupleS2338;
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L6_2atmpS2342;
  struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE* _M0L8_2atupleS2339;
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L6_2atmpS2341;
  struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE* _M0L8_2atupleS2340;
  struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE** _M0L7_2abindS966;
  struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE** _M0L6_2atmpS2336;
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13CliToolConfigEE _M0L6_2atmpS2335;
  #line 120 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  #line 122 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2344
  = _M0FP38clawteam8clawteam6config23default__claude__config();
  _M0L8_2atupleS2337
  = (struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE*)moonbit_malloc(sizeof(struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE));
  Moonbit_object_header(_M0L8_2atupleS2337)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2337->$0 = (moonbit_string_t)moonbit_string_literal_29.data;
  _M0L8_2atupleS2337->$1 = _M0L6_2atmpS2344;
  #line 123 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2343
  = _M0FP38clawteam8clawteam6config23default__gemini__config();
  _M0L8_2atupleS2338
  = (struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE*)moonbit_malloc(sizeof(struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE));
  Moonbit_object_header(_M0L8_2atupleS2338)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2338->$0 = (moonbit_string_t)moonbit_string_literal_33.data;
  _M0L8_2atupleS2338->$1 = _M0L6_2atmpS2343;
  #line 124 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2342
  = _M0FP38clawteam8clawteam6config22default__codex__config();
  _M0L8_2atupleS2339
  = (struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE*)moonbit_malloc(sizeof(struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE));
  Moonbit_object_header(_M0L8_2atupleS2339)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2339->$0 = (moonbit_string_t)moonbit_string_literal_37.data;
  _M0L8_2atupleS2339->$1 = _M0L6_2atmpS2342;
  #line 125 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2341
  = _M0FP38clawteam8clawteam6config22default__shell__config();
  _M0L8_2atupleS2340
  = (struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE*)moonbit_malloc(sizeof(struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE));
  Moonbit_object_header(_M0L8_2atupleS2340)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE, $0) >> 2, 2, 0);
  _M0L8_2atupleS2340->$0 = (moonbit_string_t)moonbit_string_literal_41.data;
  _M0L8_2atupleS2340->$1 = _M0L6_2atmpS2341;
  _M0L7_2abindS966
  = (struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE**)moonbit_make_ref_array_raw(4);
  _M0L7_2abindS966[0] = _M0L8_2atupleS2337;
  _M0L7_2abindS966[1] = _M0L8_2atupleS2338;
  _M0L7_2abindS966[2] = _M0L8_2atupleS2339;
  _M0L7_2abindS966[3] = _M0L8_2atupleS2340;
  _M0L6_2atmpS2336 = _M0L7_2abindS966;
  _M0L6_2atmpS2335
  = (struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13CliToolConfigEE){
    0, 4, _M0L6_2atmpS2336
  };
  #line 121 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  return _M0MPB3Map11from__arrayGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L6_2atmpS2335);
}

struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0FP38clawteam8clawteam6config22default__shell__config(
  
) {
  moonbit_string_t* _M0L6_2atmpS2334;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2324;
  struct _M0TUssE** _M0L7_2abindS965;
  struct _M0TUssE** _M0L6_2atmpS2333;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS2332;
  struct _M0TPB3MapGssE* _M0L6_2atmpS2325;
  moonbit_string_t _M0L6_2atmpS2326;
  moonbit_string_t* _M0L6_2atmpS2331;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2327;
  moonbit_string_t* _M0L6_2atmpS2330;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2328;
  struct _M0TP38clawteam8clawteam6config14OutputPatterns* _M0L6_2atmpS2329;
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* _block_3460;
  #line 104 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2334 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2334[0] = (moonbit_string_t)moonbit_string_literal_122.data;
  _M0L6_2atmpS2324
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2324)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2324->$0 = _M0L6_2atmpS2334;
  _M0L6_2atmpS2324->$1 = 1;
  _M0L7_2abindS965 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2333 = _M0L7_2abindS965;
  _M0L6_2atmpS2332 = (struct _M0TPB9ArrayViewGUssEE){0, 0, _M0L6_2atmpS2333};
  #line 109 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2325 = _M0MPB3Map11from__arrayGssE(_M0L6_2atmpS2332);
  _M0L6_2atmpS2326 = 0;
  _M0L6_2atmpS2331 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2331[0] = (moonbit_string_t)moonbit_string_literal_123.data;
  _M0L6_2atmpS2327
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2327)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2327->$0 = _M0L6_2atmpS2331;
  _M0L6_2atmpS2327->$1 = 1;
  _M0L6_2atmpS2330 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS2328
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2328)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2328->$0 = _M0L6_2atmpS2330;
  _M0L6_2atmpS2328->$1 = 0;
  _M0L6_2atmpS2329 = 0;
  _block_3460
  = (struct _M0TP38clawteam8clawteam6config13CliToolConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config13CliToolConfig));
  Moonbit_object_header(_block_3460)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config13CliToolConfig, $1) >> 2, 7, 0);
  _block_3460->$0 = 6;
  _block_3460->$1 = (moonbit_string_t)moonbit_string_literal_124.data;
  _block_3460->$2 = _M0L6_2atmpS2324;
  _block_3460->$3 = _M0L6_2atmpS2325;
  _block_3460->$4 = _M0L6_2atmpS2326;
  _block_3460->$5 = _M0L6_2atmpS2327;
  _block_3460->$6 = _M0L6_2atmpS2328;
  _block_3460->$7 = _M0L6_2atmpS2329;
  _block_3460->$8 = 4294967296ll;
  return _block_3460;
}

struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0FP38clawteam8clawteam6config22default__codex__config(
  
) {
  moonbit_string_t* _M0L6_2atmpS2323;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2313;
  struct _M0TUssE** _M0L7_2abindS964;
  struct _M0TUssE** _M0L6_2atmpS2322;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS2321;
  struct _M0TPB3MapGssE* _M0L6_2atmpS2314;
  moonbit_string_t _M0L6_2atmpS2315;
  moonbit_string_t* _M0L6_2atmpS2320;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2316;
  moonbit_string_t* _M0L6_2atmpS2319;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2317;
  struct _M0TP38clawteam8clawteam6config14OutputPatterns* _M0L6_2atmpS2318;
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* _block_3461;
  #line 88 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2323 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS2313
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2313)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2313->$0 = _M0L6_2atmpS2323;
  _M0L6_2atmpS2313->$1 = 0;
  _M0L7_2abindS964 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2322 = _M0L7_2abindS964;
  _M0L6_2atmpS2321 = (struct _M0TPB9ArrayViewGUssEE){0, 0, _M0L6_2atmpS2322};
  #line 93 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2314 = _M0MPB3Map11from__arrayGssE(_M0L6_2atmpS2321);
  _M0L6_2atmpS2315 = 0;
  _M0L6_2atmpS2320 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2320[0] = (moonbit_string_t)moonbit_string_literal_125.data;
  _M0L6_2atmpS2316
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2316)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2316->$0 = _M0L6_2atmpS2320;
  _M0L6_2atmpS2316->$1 = 1;
  _M0L6_2atmpS2319 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS2317
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2317)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2317->$0 = _M0L6_2atmpS2319;
  _M0L6_2atmpS2317->$1 = 0;
  _M0L6_2atmpS2318 = 0;
  _block_3461
  = (struct _M0TP38clawteam8clawteam6config13CliToolConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config13CliToolConfig));
  Moonbit_object_header(_block_3461)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config13CliToolConfig, $1) >> 2, 7, 0);
  _block_3461->$0 = 1;
  _block_3461->$1 = (moonbit_string_t)moonbit_string_literal_37.data;
  _block_3461->$2 = _M0L6_2atmpS2313;
  _block_3461->$3 = _M0L6_2atmpS2314;
  _block_3461->$4 = _M0L6_2atmpS2315;
  _block_3461->$5 = _M0L6_2atmpS2316;
  _block_3461->$6 = _M0L6_2atmpS2317;
  _block_3461->$7 = _M0L6_2atmpS2318;
  _block_3461->$8 = 4294967296ll;
  return _block_3461;
}

struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0FP38clawteam8clawteam6config23default__gemini__config(
  
) {
  moonbit_string_t* _M0L6_2atmpS2312;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2302;
  struct _M0TUssE** _M0L7_2abindS963;
  struct _M0TUssE** _M0L6_2atmpS2311;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS2310;
  struct _M0TPB3MapGssE* _M0L6_2atmpS2303;
  moonbit_string_t _M0L6_2atmpS2304;
  moonbit_string_t* _M0L6_2atmpS2309;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2305;
  moonbit_string_t* _M0L6_2atmpS2308;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2306;
  struct _M0TP38clawteam8clawteam6config14OutputPatterns* _M0L6_2atmpS2307;
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* _block_3462;
  #line 72 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2312 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS2302
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2302)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2302->$0 = _M0L6_2atmpS2312;
  _M0L6_2atmpS2302->$1 = 0;
  _M0L7_2abindS963 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2311 = _M0L7_2abindS963;
  _M0L6_2atmpS2310 = (struct _M0TPB9ArrayViewGUssEE){0, 0, _M0L6_2atmpS2311};
  #line 77 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2303 = _M0MPB3Map11from__arrayGssE(_M0L6_2atmpS2310);
  _M0L6_2atmpS2304 = 0;
  _M0L6_2atmpS2309 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2309[0] = (moonbit_string_t)moonbit_string_literal_125.data;
  _M0L6_2atmpS2305
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2305)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2305->$0 = _M0L6_2atmpS2309;
  _M0L6_2atmpS2305->$1 = 1;
  _M0L6_2atmpS2308 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2308[0] = (moonbit_string_t)moonbit_string_literal_126.data;
  _M0L6_2atmpS2306
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2306)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2306->$0 = _M0L6_2atmpS2308;
  _M0L6_2atmpS2306->$1 = 1;
  _M0L6_2atmpS2307 = 0;
  _block_3462
  = (struct _M0TP38clawteam8clawteam6config13CliToolConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config13CliToolConfig));
  Moonbit_object_header(_block_3462)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config13CliToolConfig, $1) >> 2, 7, 0);
  _block_3462->$0 = 2;
  _block_3462->$1 = (moonbit_string_t)moonbit_string_literal_33.data;
  _block_3462->$2 = _M0L6_2atmpS2302;
  _block_3462->$3 = _M0L6_2atmpS2303;
  _block_3462->$4 = _M0L6_2atmpS2304;
  _block_3462->$5 = _M0L6_2atmpS2305;
  _block_3462->$6 = _M0L6_2atmpS2306;
  _block_3462->$7 = _M0L6_2atmpS2307;
  _block_3462->$8 = 4294967296ll;
  return _block_3462;
}

struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0FP38clawteam8clawteam6config23default__claude__config(
  
) {
  moonbit_string_t* _M0L6_2atmpS2301;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2291;
  struct _M0TUssE** _M0L7_2abindS962;
  struct _M0TUssE** _M0L6_2atmpS2300;
  struct _M0TPB9ArrayViewGUssEE _M0L6_2atmpS2299;
  struct _M0TPB3MapGssE* _M0L6_2atmpS2292;
  moonbit_string_t _M0L6_2atmpS2293;
  moonbit_string_t* _M0L6_2atmpS2298;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2294;
  moonbit_string_t* _M0L6_2atmpS2297;
  struct _M0TPB5ArrayGsE* _M0L6_2atmpS2295;
  struct _M0TP38clawteam8clawteam6config14OutputPatterns* _M0L6_2atmpS2296;
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* _block_3463;
  #line 56 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2301 = (moonbit_string_t*)moonbit_empty_ref_array;
  _M0L6_2atmpS2291
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2291)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2291->$0 = _M0L6_2atmpS2301;
  _M0L6_2atmpS2291->$1 = 0;
  _M0L7_2abindS962 = (struct _M0TUssE**)moonbit_empty_ref_array;
  _M0L6_2atmpS2300 = _M0L7_2abindS962;
  _M0L6_2atmpS2299 = (struct _M0TPB9ArrayViewGUssEE){0, 0, _M0L6_2atmpS2300};
  #line 61 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2292 = _M0MPB3Map11from__arrayGssE(_M0L6_2atmpS2299);
  _M0L6_2atmpS2293 = 0;
  _M0L6_2atmpS2298 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS2298[0] = (moonbit_string_t)moonbit_string_literal_92.data;
  _M0L6_2atmpS2294
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2294)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2294->$0 = _M0L6_2atmpS2298;
  _M0L6_2atmpS2294->$1 = 1;
  _M0L6_2atmpS2297 = (moonbit_string_t*)moonbit_make_ref_array_raw(2);
  _M0L6_2atmpS2297[0] = (moonbit_string_t)moonbit_string_literal_126.data;
  _M0L6_2atmpS2297[1] = (moonbit_string_t)moonbit_string_literal_127.data;
  _M0L6_2atmpS2295
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_M0L6_2atmpS2295)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _M0L6_2atmpS2295->$0 = _M0L6_2atmpS2297;
  _M0L6_2atmpS2295->$1 = 2;
  _M0L6_2atmpS2296 = 0;
  _block_3463
  = (struct _M0TP38clawteam8clawteam6config13CliToolConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config13CliToolConfig));
  Moonbit_object_header(_block_3463)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config13CliToolConfig, $1) >> 2, 7, 0);
  _block_3463->$0 = 0;
  _block_3463->$1 = (moonbit_string_t)moonbit_string_literal_29.data;
  _block_3463->$2 = _M0L6_2atmpS2291;
  _block_3463->$3 = _M0L6_2atmpS2292;
  _block_3463->$4 = _M0L6_2atmpS2293;
  _block_3463->$5 = _M0L6_2atmpS2294;
  _block_3463->$6 = _M0L6_2atmpS2295;
  _block_3463->$7 = _M0L6_2atmpS2296;
  _block_3463->$8 = 4294967296ll;
  return _block_3463;
}

struct _M0TP38clawteam8clawteam6config11AuditConfig* _M0FP38clawteam8clawteam6config22default__audit__config(
  moonbit_string_t _M0L4homeS961
) {
  moonbit_string_t _M0L6_2atmpS2936;
  moonbit_string_t _M0L6_2atmpS2290;
  struct _M0TP38clawteam8clawteam6config11AuditConfig* _block_3464;
  #line 46 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  #line 50 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2936
  = moonbit_add_string(_M0L4homeS961, (moonbit_string_t)moonbit_string_literal_128.data);
  moonbit_decref(_M0L4homeS961);
  _M0L6_2atmpS2290 = _M0L6_2atmpS2936;
  _block_3464
  = (struct _M0TP38clawteam8clawteam6config11AuditConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config11AuditConfig));
  Moonbit_object_header(_block_3464)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config11AuditConfig, $2) >> 2, 1, 0);
  _block_3464->$0 = 1;
  _block_3464->$1 = 30;
  _block_3464->$2 = _M0L6_2atmpS2290;
  return _block_3464;
}

struct _M0TP38clawteam8clawteam6config13GatewayConfig* _M0FP38clawteam8clawteam6config24default__gateway__config(
  
) {
  struct _M0TP38clawteam8clawteam6config13GatewayConfig* _block_3465;
  #line 37 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _block_3465
  = (struct _M0TP38clawteam8clawteam6config13GatewayConfig*)moonbit_malloc(sizeof(struct _M0TP38clawteam8clawteam6config13GatewayConfig));
  Moonbit_object_header(_block_3465)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TP38clawteam8clawteam6config13GatewayConfig, $1) >> 2, 1, 0);
  _block_3465->$0 = 3000;
  _block_3465->$1 = (moonbit_string_t)moonbit_string_literal_9.data;
  return _block_3465;
}

struct _M0TPB5ArrayGsE* _M0FP38clawteam8clawteam6config13config__paths(
  moonbit_string_t _M0L4homeS959,
  moonbit_string_t _M0L3cwdS960
) {
  moonbit_string_t _M0L6_2atmpS2286;
  moonbit_string_t _M0L6_2atmpS2938;
  moonbit_string_t _M0L6_2atmpS2287;
  moonbit_string_t _M0L6_2atmpS2288;
  moonbit_string_t _M0L6_2atmpS2937;
  moonbit_string_t _M0L6_2atmpS2289;
  moonbit_string_t* _M0L6_2atmpS2285;
  struct _M0TPB5ArrayGsE* _block_3466;
  #line 26 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  #line 28 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2286
  = moonbit_add_string(_M0L4homeS959, (moonbit_string_t)moonbit_string_literal_129.data);
  #line 29 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2938
  = moonbit_add_string(_M0L4homeS959, (moonbit_string_t)moonbit_string_literal_130.data);
  moonbit_decref(_M0L4homeS959);
  _M0L6_2atmpS2287 = _M0L6_2atmpS2938;
  #line 30 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2288
  = moonbit_add_string(_M0L3cwdS960, (moonbit_string_t)moonbit_string_literal_129.data);
  #line 31 "E:\\moonbit\\clawteam\\config\\loader.mbt"
  _M0L6_2atmpS2937
  = moonbit_add_string(_M0L3cwdS960, (moonbit_string_t)moonbit_string_literal_130.data);
  moonbit_decref(_M0L3cwdS960);
  _M0L6_2atmpS2289 = _M0L6_2atmpS2937;
  _M0L6_2atmpS2285 = (moonbit_string_t*)moonbit_make_ref_array_raw(4);
  _M0L6_2atmpS2285[0] = _M0L6_2atmpS2286;
  _M0L6_2atmpS2285[1] = _M0L6_2atmpS2287;
  _M0L6_2atmpS2285[2] = _M0L6_2atmpS2288;
  _M0L6_2atmpS2285[3] = _M0L6_2atmpS2289;
  _block_3466
  = (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
  Moonbit_object_header(_block_3466)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
  _block_3466->$0 = _M0L6_2atmpS2285;
  _block_3466->$1 = 4;
  return _block_3466;
}

int32_t _M0IP38clawteam8clawteam5types9AgentRolePB2Eq5equal(
  int32_t _M0L8_2ax__41S957,
  int32_t _M0L8_2ax__42S958
) {
  #line 26 "E:\\moonbit\\clawteam\\types\\types.mbt"
  switch (_M0L8_2ax__41S957) {
    case 0: {
      switch (_M0L8_2ax__42S958) {
        case 0: {
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
    
    case 1: {
      switch (_M0L8_2ax__42S958) {
        case 1: {
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
    default: {
      switch (_M0L8_2ax__42S958) {
        case 2: {
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

int32_t _M0IP38clawteam8clawteam5types11CliToolTypePB2Eq5equal(
  int32_t _M0L8_2ax__59S955,
  int32_t _M0L8_2ax__60S956
) {
  #line 14 "E:\\moonbit\\clawteam\\types\\types.mbt"
  switch (_M0L8_2ax__59S955) {
    case 0: {
      switch (_M0L8_2ax__60S956) {
        case 0: {
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
    
    case 1: {
      switch (_M0L8_2ax__60S956) {
        case 1: {
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
    
    case 2: {
      switch (_M0L8_2ax__60S956) {
        case 2: {
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
    
    case 3: {
      switch (_M0L8_2ax__60S956) {
        case 3: {
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
    
    case 4: {
      switch (_M0L8_2ax__60S956) {
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
    
    case 5: {
      switch (_M0L8_2ax__60S956) {
        case 5: {
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
    default: {
      switch (_M0L8_2ax__60S956) {
        case 6: {
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

int32_t _M0IP38clawteam8clawteam5types16DispatchStrategyPB2Eq5equal(
  int32_t _M0L9_2ax__113S953,
  int32_t _M0L9_2ax__114S954
) {
  #line 44 "E:\\moonbit\\clawteam\\types\\types.mbt"
  switch (_M0L9_2ax__113S953) {
    case 0: {
      switch (_M0L9_2ax__114S954) {
        case 0: {
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
    
    case 1: {
      switch (_M0L9_2ax__114S954) {
        case 1: {
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
    
    case 2: {
      switch (_M0L9_2ax__114S954) {
        case 2: {
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
    default: {
      switch (_M0L9_2ax__114S954) {
        case 3: {
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

moonbit_string_t _M0MP38clawteam8clawteam6errors13ClawTeamError11to__message(
  void* _M0L4selfS924
) {
  moonbit_string_t _M0L3msgS897;
  moonbit_string_t _M0L3msgS899;
  moonbit_string_t _M0L3msgS901;
  moonbit_string_t _M0L3msgS903;
  moonbit_string_t _M0L3msgS905;
  moonbit_string_t _M0L2idS907;
  int32_t _M0L4codeS909;
  moonbit_string_t _M0L7timeoutS911;
  moonbit_string_t _M0L3cmdS913;
  moonbit_string_t _M0L4nameS915;
  moonbit_string_t _M0L2idS917;
  moonbit_string_t _M0L2idS919;
  moonbit_string_t _M0L3msgS921;
  moonbit_string_t _M0L4pathS923;
  moonbit_string_t _M0L6_2atmpS2284;
  moonbit_string_t _M0L6_2atmpS2953;
  moonbit_string_t _M0L6_2atmpS2283;
  moonbit_string_t _M0L6_2atmpS2952;
  moonbit_string_t _M0L6_2atmpS2282;
  moonbit_string_t _M0L6_2atmpS2951;
  moonbit_string_t _M0L6_2atmpS2281;
  moonbit_string_t _M0L6_2atmpS2950;
  moonbit_string_t _M0L6_2atmpS2280;
  moonbit_string_t _M0L6_2atmpS2949;
  moonbit_string_t _M0L6_2atmpS2279;
  moonbit_string_t _M0L6_2atmpS2948;
  moonbit_string_t _M0L6_2atmpS2278;
  moonbit_string_t _M0L6_2atmpS2947;
  moonbit_string_t _M0L6_2atmpS2277;
  moonbit_string_t _M0L6_2atmpS2946;
  moonbit_string_t _M0L6_2atmpS2276;
  moonbit_string_t _M0L6_2atmpS2945;
  moonbit_string_t _M0L6_2atmpS2275;
  moonbit_string_t _M0L6_2atmpS2944;
  moonbit_string_t _M0L6_2atmpS2274;
  moonbit_string_t _M0L6_2atmpS2943;
  moonbit_string_t _M0L6_2atmpS2273;
  moonbit_string_t _M0L6_2atmpS2942;
  moonbit_string_t _M0L6_2atmpS2272;
  moonbit_string_t _M0L6_2atmpS2941;
  moonbit_string_t _M0L6_2atmpS2271;
  moonbit_string_t _M0L6_2atmpS2940;
  moonbit_string_t _M0L6_2atmpS2270;
  moonbit_string_t _M0L6_2atmpS2939;
  #line 39 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  switch (Moonbit_object_tag(_M0L4selfS924)) {
    case 0: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ConfigNotFound* _M0L17_2aConfigNotFoundS925 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ConfigNotFound*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2954 = _M0L17_2aConfigNotFoundS925->$0;
      int32_t _M0L6_2acntS3293 =
        Moonbit_object_header(_M0L17_2aConfigNotFoundS925)->rc;
      moonbit_string_t _M0L7_2apathS926;
      if (_M0L6_2acntS3293 > 1) {
        int32_t _M0L11_2anew__cntS3294 = _M0L6_2acntS3293 - 1;
        Moonbit_object_header(_M0L17_2aConfigNotFoundS925)->rc
        = _M0L11_2anew__cntS3294;
        moonbit_incref(_M0L8_2afieldS2954);
      } else if (_M0L6_2acntS3293 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L17_2aConfigNotFoundS925);
      }
      _M0L7_2apathS926 = _M0L8_2afieldS2954;
      _M0L4pathS923 = _M0L7_2apathS926;
      goto join_922;
      break;
    }
    
    case 1: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid* _M0L16_2aConfigInvalidS927 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13ConfigInvalid*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2955 = _M0L16_2aConfigInvalidS927->$0;
      int32_t _M0L6_2acntS3295 =
        Moonbit_object_header(_M0L16_2aConfigInvalidS927)->rc;
      moonbit_string_t _M0L6_2amsgS928;
      if (_M0L6_2acntS3295 > 1) {
        int32_t _M0L11_2anew__cntS3296 = _M0L6_2acntS3295 - 1;
        Moonbit_object_header(_M0L16_2aConfigInvalidS927)->rc
        = _M0L11_2anew__cntS3296;
        moonbit_incref(_M0L8_2afieldS2955);
      } else if (_M0L6_2acntS3295 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L16_2aConfigInvalidS927);
      }
      _M0L6_2amsgS928 = _M0L8_2afieldS2955;
      _M0L3msgS921 = _M0L6_2amsgS928;
      goto join_920;
      break;
    }
    
    case 2: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13AgentNotFound* _M0L16_2aAgentNotFoundS929 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13AgentNotFound*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2956 = _M0L16_2aAgentNotFoundS929->$0;
      int32_t _M0L6_2acntS3297 =
        Moonbit_object_header(_M0L16_2aAgentNotFoundS929)->rc;
      moonbit_string_t _M0L5_2aidS930;
      if (_M0L6_2acntS3297 > 1) {
        int32_t _M0L11_2anew__cntS3298 = _M0L6_2acntS3297 - 1;
        Moonbit_object_header(_M0L16_2aAgentNotFoundS929)->rc
        = _M0L11_2anew__cntS3298;
        moonbit_incref(_M0L8_2afieldS2956);
      } else if (_M0L6_2acntS3297 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L16_2aAgentNotFoundS929);
      }
      _M0L5_2aidS930 = _M0L8_2afieldS2956;
      _M0L2idS919 = _M0L5_2aidS930;
      goto join_918;
      break;
    }
    
    case 3: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13SkillNotFound* _M0L16_2aSkillNotFoundS931 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError13SkillNotFound*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2957 = _M0L16_2aSkillNotFoundS931->$0;
      int32_t _M0L6_2acntS3299 =
        Moonbit_object_header(_M0L16_2aSkillNotFoundS931)->rc;
      moonbit_string_t _M0L5_2aidS932;
      if (_M0L6_2acntS3299 > 1) {
        int32_t _M0L11_2anew__cntS3300 = _M0L6_2acntS3299 - 1;
        Moonbit_object_header(_M0L16_2aSkillNotFoundS931)->rc
        = _M0L11_2anew__cntS3300;
        moonbit_incref(_M0L8_2afieldS2957);
      } else if (_M0L6_2acntS3299 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L16_2aSkillNotFoundS931);
      }
      _M0L5_2aidS932 = _M0L8_2afieldS2957;
      _M0L2idS917 = _M0L5_2aidS932;
      goto join_916;
      break;
    }
    
    case 4: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError15CliToolNotFound* _M0L18_2aCliToolNotFoundS933 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError15CliToolNotFound*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2958 = _M0L18_2aCliToolNotFoundS933->$0;
      int32_t _M0L6_2acntS3301 =
        Moonbit_object_header(_M0L18_2aCliToolNotFoundS933)->rc;
      moonbit_string_t _M0L7_2anameS934;
      if (_M0L6_2acntS3301 > 1) {
        int32_t _M0L11_2anew__cntS3302 = _M0L6_2acntS3301 - 1;
        Moonbit_object_header(_M0L18_2aCliToolNotFoundS933)->rc
        = _M0L11_2anew__cntS3302;
        moonbit_incref(_M0L8_2afieldS2958);
      } else if (_M0L6_2acntS3301 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L18_2aCliToolNotFoundS933);
      }
      _M0L7_2anameS934 = _M0L8_2afieldS2958;
      _M0L4nameS915 = _M0L7_2anameS934;
      goto join_914;
      break;
    }
    
    case 5: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError18ProcessStartFailed* _M0L21_2aProcessStartFailedS935 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError18ProcessStartFailed*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2959 =
        _M0L21_2aProcessStartFailedS935->$0;
      int32_t _M0L6_2acntS3303 =
        Moonbit_object_header(_M0L21_2aProcessStartFailedS935)->rc;
      moonbit_string_t _M0L6_2acmdS936;
      if (_M0L6_2acntS3303 > 1) {
        int32_t _M0L11_2anew__cntS3304 = _M0L6_2acntS3303 - 1;
        Moonbit_object_header(_M0L21_2aProcessStartFailedS935)->rc
        = _M0L11_2anew__cntS3304;
        moonbit_incref(_M0L8_2afieldS2959);
      } else if (_M0L6_2acntS3303 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L21_2aProcessStartFailedS935);
      }
      _M0L6_2acmdS936 = _M0L8_2afieldS2959;
      _M0L3cmdS913 = _M0L6_2acmdS936;
      goto join_912;
      break;
    }
    
    case 6: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ProcessTimeout* _M0L17_2aProcessTimeoutS937 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ProcessTimeout*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2960 = _M0L17_2aProcessTimeoutS937->$0;
      int32_t _M0L6_2acntS3305 =
        Moonbit_object_header(_M0L17_2aProcessTimeoutS937)->rc;
      moonbit_string_t _M0L10_2atimeoutS938;
      if (_M0L6_2acntS3305 > 1) {
        int32_t _M0L11_2anew__cntS3306 = _M0L6_2acntS3305 - 1;
        Moonbit_object_header(_M0L17_2aProcessTimeoutS937)->rc
        = _M0L11_2anew__cntS3306;
        moonbit_incref(_M0L8_2afieldS2960);
      } else if (_M0L6_2acntS3305 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L17_2aProcessTimeoutS937);
      }
      _M0L10_2atimeoutS938 = _M0L8_2afieldS2960;
      _M0L7timeoutS911 = _M0L10_2atimeoutS938;
      goto join_910;
      break;
    }
    
    case 7: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ProcessCrashed* _M0L17_2aProcessCrashedS939 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14ProcessCrashed*)_M0L4selfS924;
      int32_t _M0L8_2afieldS2961 = _M0L17_2aProcessCrashedS939->$0;
      int32_t _M0L7_2acodeS940;
      moonbit_decref(_M0L17_2aProcessCrashedS939);
      _M0L7_2acodeS940 = _M0L8_2afieldS2961;
      _M0L4codeS909 = _M0L7_2acodeS940;
      goto join_908;
      break;
    }
    
    case 8: {
      return (moonbit_string_t)moonbit_string_literal_131.data;
      break;
    }
    
    case 9: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError9AgentBusy* _M0L12_2aAgentBusyS941 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError9AgentBusy*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2962 = _M0L12_2aAgentBusyS941->$0;
      int32_t _M0L6_2acntS3307 =
        Moonbit_object_header(_M0L12_2aAgentBusyS941)->rc;
      moonbit_string_t _M0L5_2aidS942;
      if (_M0L6_2acntS3307 > 1) {
        int32_t _M0L11_2anew__cntS3308 = _M0L6_2acntS3307 - 1;
        Moonbit_object_header(_M0L12_2aAgentBusyS941)->rc
        = _M0L11_2anew__cntS3308;
        moonbit_incref(_M0L8_2afieldS2962);
      } else if (_M0L6_2acntS3307 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L12_2aAgentBusyS941);
      }
      _M0L5_2aidS942 = _M0L8_2afieldS2962;
      _M0L2idS907 = _M0L5_2aidS942;
      goto join_906;
      break;
    }
    
    case 10: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14DispatchFailed* _M0L17_2aDispatchFailedS943 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14DispatchFailed*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2963 = _M0L17_2aDispatchFailedS943->$0;
      int32_t _M0L6_2acntS3309 =
        Moonbit_object_header(_M0L17_2aDispatchFailedS943)->rc;
      moonbit_string_t _M0L6_2amsgS944;
      if (_M0L6_2acntS3309 > 1) {
        int32_t _M0L11_2anew__cntS3310 = _M0L6_2acntS3309 - 1;
        Moonbit_object_header(_M0L17_2aDispatchFailedS943)->rc
        = _M0L11_2anew__cntS3310;
        moonbit_incref(_M0L8_2afieldS2963);
      } else if (_M0L6_2acntS3309 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L17_2aDispatchFailedS943);
      }
      _M0L6_2amsgS944 = _M0L8_2afieldS2963;
      _M0L3msgS905 = _M0L6_2amsgS944;
      goto join_904;
      break;
    }
    
    case 11: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError12ChannelError* _M0L15_2aChannelErrorS945 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError12ChannelError*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2964 = _M0L15_2aChannelErrorS945->$0;
      int32_t _M0L6_2acntS3311 =
        Moonbit_object_header(_M0L15_2aChannelErrorS945)->rc;
      moonbit_string_t _M0L6_2amsgS946;
      if (_M0L6_2acntS3311 > 1) {
        int32_t _M0L11_2anew__cntS3312 = _M0L6_2acntS3311 - 1;
        Moonbit_object_header(_M0L15_2aChannelErrorS945)->rc
        = _M0L11_2anew__cntS3312;
        moonbit_incref(_M0L8_2afieldS2964);
      } else if (_M0L6_2acntS3311 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L15_2aChannelErrorS945);
      }
      _M0L6_2amsgS946 = _M0L8_2afieldS2964;
      _M0L3msgS903 = _M0L6_2amsgS946;
      goto join_902;
      break;
    }
    
    case 12: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14WebhookInvalid* _M0L17_2aWebhookInvalidS947 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError14WebhookInvalid*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2965 = _M0L17_2aWebhookInvalidS947->$0;
      int32_t _M0L6_2acntS3313 =
        Moonbit_object_header(_M0L17_2aWebhookInvalidS947)->rc;
      moonbit_string_t _M0L6_2amsgS948;
      if (_M0L6_2acntS3313 > 1) {
        int32_t _M0L11_2anew__cntS3314 = _M0L6_2acntS3313 - 1;
        Moonbit_object_header(_M0L17_2aWebhookInvalidS947)->rc
        = _M0L11_2anew__cntS3314;
        moonbit_incref(_M0L8_2afieldS2965);
      } else if (_M0L6_2acntS3313 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L17_2aWebhookInvalidS947);
      }
      _M0L6_2amsgS948 = _M0L8_2afieldS2965;
      _M0L3msgS901 = _M0L6_2amsgS948;
      goto join_900;
      break;
    }
    
    case 13: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError19AuditLogWriteFailed* _M0L22_2aAuditLogWriteFailedS949 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError19AuditLogWriteFailed*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2966 =
        _M0L22_2aAuditLogWriteFailedS949->$0;
      int32_t _M0L6_2acntS3315 =
        Moonbit_object_header(_M0L22_2aAuditLogWriteFailedS949)->rc;
      moonbit_string_t _M0L6_2amsgS950;
      if (_M0L6_2acntS3315 > 1) {
        int32_t _M0L11_2anew__cntS3316 = _M0L6_2acntS3315 - 1;
        Moonbit_object_header(_M0L22_2aAuditLogWriteFailedS949)->rc
        = _M0L11_2anew__cntS3316;
        moonbit_incref(_M0L8_2afieldS2966);
      } else if (_M0L6_2acntS3315 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L22_2aAuditLogWriteFailedS949);
      }
      _M0L6_2amsgS950 = _M0L8_2afieldS2966;
      _M0L3msgS899 = _M0L6_2amsgS950;
      goto join_898;
      break;
    }
    
    case 14: {
      return (moonbit_string_t)moonbit_string_literal_132.data;
      break;
    }
    default: {
      struct _M0DTP38clawteam8clawteam6errors13ClawTeamError20TemplateRenderFailed* _M0L23_2aTemplateRenderFailedS951 =
        (struct _M0DTP38clawteam8clawteam6errors13ClawTeamError20TemplateRenderFailed*)_M0L4selfS924;
      moonbit_string_t _M0L8_2afieldS2967 =
        _M0L23_2aTemplateRenderFailedS951->$0;
      int32_t _M0L6_2acntS3317 =
        Moonbit_object_header(_M0L23_2aTemplateRenderFailedS951)->rc;
      moonbit_string_t _M0L6_2amsgS952;
      if (_M0L6_2acntS3317 > 1) {
        int32_t _M0L11_2anew__cntS3318 = _M0L6_2acntS3317 - 1;
        Moonbit_object_header(_M0L23_2aTemplateRenderFailedS951)->rc
        = _M0L11_2anew__cntS3318;
        moonbit_incref(_M0L8_2afieldS2967);
      } else if (_M0L6_2acntS3317 == 1) {
        #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
        moonbit_free(_M0L23_2aTemplateRenderFailedS951);
      }
      _M0L6_2amsgS952 = _M0L8_2afieldS2967;
      _M0L3msgS897 = _M0L6_2amsgS952;
      goto join_896;
      break;
    }
  }
  join_922:;
  #line 41 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2284 = _M0IPC16string6StringPB4Show10to__string(_M0L4pathS923);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2953
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_133.data, _M0L6_2atmpS2284);
  moonbit_decref(_M0L6_2atmpS2284);
  return _M0L6_2atmpS2953;
  join_920:;
  #line 42 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2283 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS921);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2952
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_134.data, _M0L6_2atmpS2283);
  moonbit_decref(_M0L6_2atmpS2283);
  return _M0L6_2atmpS2952;
  join_918:;
  #line 43 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2282 = _M0IPC16string6StringPB4Show10to__string(_M0L2idS919);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2951
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_135.data, _M0L6_2atmpS2282);
  moonbit_decref(_M0L6_2atmpS2282);
  return _M0L6_2atmpS2951;
  join_916:;
  #line 44 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2281 = _M0IPC16string6StringPB4Show10to__string(_M0L2idS917);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2950
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_136.data, _M0L6_2atmpS2281);
  moonbit_decref(_M0L6_2atmpS2281);
  return _M0L6_2atmpS2950;
  join_914:;
  #line 45 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2280 = _M0IPC16string6StringPB4Show10to__string(_M0L4nameS915);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2949
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_137.data, _M0L6_2atmpS2280);
  moonbit_decref(_M0L6_2atmpS2280);
  return _M0L6_2atmpS2949;
  join_912:;
  #line 46 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2279 = _M0IPC16string6StringPB4Show10to__string(_M0L3cmdS913);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2948
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_138.data, _M0L6_2atmpS2279);
  moonbit_decref(_M0L6_2atmpS2279);
  return _M0L6_2atmpS2948;
  join_910:;
  #line 47 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2278
  = _M0IPC16string6StringPB4Show10to__string(_M0L7timeoutS911);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2947
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_139.data, _M0L6_2atmpS2278);
  moonbit_decref(_M0L6_2atmpS2278);
  _M0L6_2atmpS2277 = _M0L6_2atmpS2947;
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2946
  = moonbit_add_string(_M0L6_2atmpS2277, (moonbit_string_t)moonbit_string_literal_140.data);
  moonbit_decref(_M0L6_2atmpS2277);
  return _M0L6_2atmpS2946;
  join_908:;
  #line 48 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2276
  = _M0IP016_24default__implPB4Show10to__stringGiE(_M0L4codeS909);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2945
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_141.data, _M0L6_2atmpS2276);
  moonbit_decref(_M0L6_2atmpS2276);
  return _M0L6_2atmpS2945;
  join_906:;
  #line 50 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2275 = _M0IPC16string6StringPB4Show10to__string(_M0L2idS907);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2944
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_142.data, _M0L6_2atmpS2275);
  moonbit_decref(_M0L6_2atmpS2275);
  return _M0L6_2atmpS2944;
  join_904:;
  #line 51 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2274 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS905);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2943
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_143.data, _M0L6_2atmpS2274);
  moonbit_decref(_M0L6_2atmpS2274);
  return _M0L6_2atmpS2943;
  join_902:;
  #line 52 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2273 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS903);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2942
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_144.data, _M0L6_2atmpS2273);
  moonbit_decref(_M0L6_2atmpS2273);
  return _M0L6_2atmpS2942;
  join_900:;
  #line 53 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2272 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS901);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2941
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_145.data, _M0L6_2atmpS2272);
  moonbit_decref(_M0L6_2atmpS2272);
  return _M0L6_2atmpS2941;
  join_898:;
  #line 54 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2271 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS899);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2940
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_146.data, _M0L6_2atmpS2271);
  moonbit_decref(_M0L6_2atmpS2271);
  return _M0L6_2atmpS2940;
  join_896:;
  #line 56 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2270 = _M0IPC16string6StringPB4Show10to__string(_M0L3msgS897);
  #line 40 "E:\\moonbit\\clawteam\\errors\\errors.mbt"
  _M0L6_2atmpS2939
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_147.data, _M0L6_2atmpS2270);
  moonbit_decref(_M0L6_2atmpS2270);
  return _M0L6_2atmpS2939;
}

int32_t _M0MPC15array5Array8containsGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS891,
  moonbit_string_t _M0L5valueS894
) {
  int32_t _M0L7_2abindS890;
  int32_t _M0L2__S892;
  #line 838 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L7_2abindS890 = _M0L4selfS891->$1;
  _M0L2__S892 = 0;
  while (1) {
    if (_M0L2__S892 < _M0L7_2abindS890) {
      moonbit_string_t* _M0L8_2afieldS2970 = _M0L4selfS891->$0;
      moonbit_string_t* _M0L3bufS2269 = _M0L8_2afieldS2970;
      moonbit_string_t _M0L6_2atmpS2969 =
        (moonbit_string_t)_M0L3bufS2269[_M0L2__S892];
      moonbit_string_t _M0L1vS893 = _M0L6_2atmpS2969;
      int32_t _M0L6_2atmpS2968;
      int32_t _M0L6_2atmpS2268;
      #line 840 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      _M0L6_2atmpS2968 = moonbit_val_array_equal(_M0L1vS893, _M0L5valueS894);
      if (_M0L6_2atmpS2968) {
        moonbit_decref(_M0L5valueS894);
        moonbit_decref(_M0L4selfS891);
        return 1;
      }
      _M0L6_2atmpS2268 = _M0L2__S892 + 1;
      _M0L2__S892 = _M0L6_2atmpS2268;
      continue;
    } else {
      moonbit_decref(_M0L5valueS894);
      moonbit_decref(_M0L4selfS891);
      return 0;
    }
    break;
  }
}

int32_t _M0IPB9SourceLocPB4Show6output(
  moonbit_string_t _M0L4selfS888,
  struct _M0TPB6Logger _M0L6loggerS889
) {
  moonbit_string_t _M0L6_2atmpS2267;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS2266;
  #line 43 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2267 = _M0L4selfS888;
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2266 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS2267);
  #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13SourceLocReprPB4Show6output(_M0L6_2atmpS2266, _M0L6loggerS889);
  return 0;
}

int32_t _M0IPB13SourceLocReprPB4Show6output(
  struct _M0TPB13SourceLocRepr* _M0L4selfS865,
  struct _M0TPB6Logger _M0L6loggerS887
) {
  struct _M0TPC16string10StringView _M0L8_2afieldS2979;
  struct _M0TPC16string10StringView _M0L3pkgS864;
  moonbit_string_t _M0L7_2adataS866;
  int32_t _M0L8_2astartS867;
  int32_t _M0L6_2atmpS2265;
  int32_t _M0L6_2aendS868;
  int32_t _M0Lm9_2acursorS869;
  int32_t _M0Lm13accept__stateS870;
  int32_t _M0Lm10match__endS871;
  int32_t _M0Lm20match__tag__saver__0S872;
  int32_t _M0Lm6tag__0S873;
  struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE* _M0L7_2abindS874;
  struct _M0TPC16string10StringView _M0L8_2afieldS2978;
  struct _M0TPC16string10StringView _M0L15_2amodule__nameS883;
  void* _M0L8_2afieldS2977;
  int32_t _M0L6_2acntS3319;
  void* _M0L16_2apackage__nameS884;
  struct _M0TPC16string10StringView _M0L8_2afieldS2975;
  struct _M0TPC16string10StringView _M0L8filenameS2242;
  struct _M0TPC16string10StringView _M0L8_2afieldS2974;
  struct _M0TPC16string10StringView _M0L11start__lineS2243;
  struct _M0TPC16string10StringView _M0L8_2afieldS2973;
  struct _M0TPC16string10StringView _M0L13start__columnS2244;
  struct _M0TPC16string10StringView _M0L8_2afieldS2972;
  struct _M0TPC16string10StringView _M0L9end__lineS2245;
  struct _M0TPC16string10StringView _M0L8_2afieldS2971;
  int32_t _M0L6_2acntS3323;
  struct _M0TPC16string10StringView _M0L11end__columnS2246;
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2afieldS2979
  = (struct _M0TPC16string10StringView){
    _M0L4selfS865->$0_1, _M0L4selfS865->$0_2, _M0L4selfS865->$0_0
  };
  _M0L3pkgS864 = _M0L8_2afieldS2979;
  moonbit_incref(_M0L3pkgS864.$0);
  moonbit_incref(_M0L3pkgS864.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS866 = _M0MPC16string10StringView4data(_M0L3pkgS864);
  moonbit_incref(_M0L3pkgS864.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS867 = _M0MPC16string10StringView13start__offset(_M0L3pkgS864);
  moonbit_incref(_M0L3pkgS864.$0);
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS2265 = _M0MPC16string10StringView6length(_M0L3pkgS864);
  _M0L6_2aendS868 = _M0L8_2astartS867 + _M0L6_2atmpS2265;
  _M0Lm9_2acursorS869 = _M0L8_2astartS867;
  _M0Lm13accept__stateS870 = -1;
  _M0Lm10match__endS871 = -1;
  _M0Lm20match__tag__saver__0S872 = -1;
  _M0Lm6tag__0S873 = -1;
  while (1) {
    int32_t _M0L6_2atmpS2257 = _M0Lm9_2acursorS869;
    if (_M0L6_2atmpS2257 < _M0L6_2aendS868) {
      int32_t _M0L6_2atmpS2264 = _M0Lm9_2acursorS869;
      int32_t _M0L10next__charS878;
      int32_t _M0L6_2atmpS2258;
      moonbit_incref(_M0L7_2adataS866);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L10next__charS878
      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS866, _M0L6_2atmpS2264);
      _M0L6_2atmpS2258 = _M0Lm9_2acursorS869;
      _M0Lm9_2acursorS869 = _M0L6_2atmpS2258 + 1;
      if (_M0L10next__charS878 == 47) {
        while (1) {
          int32_t _M0L6_2atmpS2259;
          _M0Lm6tag__0S873 = _M0Lm9_2acursorS869;
          _M0L6_2atmpS2259 = _M0Lm9_2acursorS869;
          if (_M0L6_2atmpS2259 < _M0L6_2aendS868) {
            int32_t _M0L6_2atmpS2263 = _M0Lm9_2acursorS869;
            int32_t _M0L10next__charS879;
            int32_t _M0L6_2atmpS2260;
            moonbit_incref(_M0L7_2adataS866);
            #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
            _M0L10next__charS879
            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS866, _M0L6_2atmpS2263);
            _M0L6_2atmpS2260 = _M0Lm9_2acursorS869;
            _M0Lm9_2acursorS869 = _M0L6_2atmpS2260 + 1;
            if (_M0L10next__charS879 == 47) {
              while (1) {
                int32_t _M0L6_2atmpS2261 = _M0Lm9_2acursorS869;
                if (_M0L6_2atmpS2261 < _M0L6_2aendS868) {
                  int32_t _M0L6_2atmpS2262 = _M0Lm9_2acursorS869;
                  _M0Lm9_2acursorS869 = _M0L6_2atmpS2262 + 1;
                  continue;
                } else {
                  _M0Lm20match__tag__saver__0S872 = _M0Lm6tag__0S873;
                  _M0Lm13accept__stateS870 = 0;
                  _M0Lm10match__endS871 = _M0Lm9_2acursorS869;
                  goto join_875;
                }
                break;
              }
            } else {
              continue;
            }
          } else {
            goto join_875;
          }
          break;
        }
      } else {
        continue;
      }
    } else {
      goto join_875;
    }
    break;
  }
  goto joinlet_3482;
  join_875:;
  switch (_M0Lm13accept__stateS870) {
    case 0: {
      int32_t _M0L6_2atmpS2255;
      int32_t _M0L6_2atmpS2254;
      int64_t _M0L6_2atmpS2251;
      int32_t _M0L6_2atmpS2253;
      int64_t _M0L6_2atmpS2252;
      struct _M0TPC16string10StringView _M0L13package__nameS876;
      int64_t _M0L6_2atmpS2248;
      int32_t _M0L6_2atmpS2250;
      int64_t _M0L6_2atmpS2249;
      struct _M0TPC16string10StringView _M0L12module__nameS877;
      void* _M0L4SomeS2247;
      moonbit_decref(_M0L3pkgS864.$0);
      _M0L6_2atmpS2255 = _M0Lm20match__tag__saver__0S872;
      _M0L6_2atmpS2254 = _M0L6_2atmpS2255 + 1;
      _M0L6_2atmpS2251 = (int64_t)_M0L6_2atmpS2254;
      _M0L6_2atmpS2253 = _M0Lm10match__endS871;
      _M0L6_2atmpS2252 = (int64_t)_M0L6_2atmpS2253;
      moonbit_incref(_M0L7_2adataS866);
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13package__nameS876
      = _M0MPC16string6String4view(_M0L7_2adataS866, _M0L6_2atmpS2251, _M0L6_2atmpS2252);
      _M0L6_2atmpS2248 = (int64_t)_M0L8_2astartS867;
      _M0L6_2atmpS2250 = _M0Lm20match__tag__saver__0S872;
      _M0L6_2atmpS2249 = (int64_t)_M0L6_2atmpS2250;
      #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L12module__nameS877
      = _M0MPC16string6String4view(_M0L7_2adataS866, _M0L6_2atmpS2248, _M0L6_2atmpS2249);
      _M0L4SomeS2247
      = (void*)moonbit_malloc(sizeof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some));
      Moonbit_object_header(_M0L4SomeS2247)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some, $0_0) >> 2, 1, 1);
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2247)->$0_0
      = _M0L13package__nameS876.$0;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2247)->$0_1
      = _M0L13package__nameS876.$1;
      ((struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L4SomeS2247)->$0_2
      = _M0L13package__nameS876.$2;
      _M0L7_2abindS874
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS874)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS874->$0_0 = _M0L12module__nameS877.$0;
      _M0L7_2abindS874->$0_1 = _M0L12module__nameS877.$1;
      _M0L7_2abindS874->$0_2 = _M0L12module__nameS877.$2;
      _M0L7_2abindS874->$1 = _M0L4SomeS2247;
      break;
    }
    default: {
      void* _M0L4NoneS2256;
      moonbit_decref(_M0L7_2adataS866);
      _M0L4NoneS2256
      = (struct moonbit_object*)&moonbit_constant_constructor_0 + 1;
      _M0L7_2abindS874
      = (struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE*)moonbit_malloc(sizeof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE));
      Moonbit_object_header(_M0L7_2abindS874)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TURPC16string10StringViewRPC16option6OptionGRPC16string10StringViewEE, $0_0) >> 2, 2, 0);
      _M0L7_2abindS874->$0_0 = _M0L3pkgS864.$0;
      _M0L7_2abindS874->$0_1 = _M0L3pkgS864.$1;
      _M0L7_2abindS874->$0_2 = _M0L3pkgS864.$2;
      _M0L7_2abindS874->$1 = _M0L4NoneS2256;
      break;
    }
  }
  joinlet_3482:;
  _M0L8_2afieldS2978
  = (struct _M0TPC16string10StringView){
    _M0L7_2abindS874->$0_1, _M0L7_2abindS874->$0_2, _M0L7_2abindS874->$0_0
  };
  _M0L15_2amodule__nameS883 = _M0L8_2afieldS2978;
  _M0L8_2afieldS2977 = _M0L7_2abindS874->$1;
  _M0L6_2acntS3319 = Moonbit_object_header(_M0L7_2abindS874)->rc;
  if (_M0L6_2acntS3319 > 1) {
    int32_t _M0L11_2anew__cntS3320 = _M0L6_2acntS3319 - 1;
    Moonbit_object_header(_M0L7_2abindS874)->rc = _M0L11_2anew__cntS3320;
    moonbit_incref(_M0L8_2afieldS2977);
    moonbit_incref(_M0L15_2amodule__nameS883.$0);
  } else if (_M0L6_2acntS3319 == 1) {
    #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L7_2abindS874);
  }
  _M0L16_2apackage__nameS884 = _M0L8_2afieldS2977;
  switch (Moonbit_object_tag(_M0L16_2apackage__nameS884)) {
    case 1: {
      struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some* _M0L7_2aSomeS885 =
        (struct _M0DTPC16option6OptionGRPC16string10StringViewE4Some*)_M0L16_2apackage__nameS884;
      struct _M0TPC16string10StringView _M0L8_2afieldS2976 =
        (struct _M0TPC16string10StringView){_M0L7_2aSomeS885->$0_1,
                                              _M0L7_2aSomeS885->$0_2,
                                              _M0L7_2aSomeS885->$0_0};
      int32_t _M0L6_2acntS3321 = Moonbit_object_header(_M0L7_2aSomeS885)->rc;
      struct _M0TPC16string10StringView _M0L12_2apkg__nameS886;
      if (_M0L6_2acntS3321 > 1) {
        int32_t _M0L11_2anew__cntS3322 = _M0L6_2acntS3321 - 1;
        Moonbit_object_header(_M0L7_2aSomeS885)->rc = _M0L11_2anew__cntS3322;
        moonbit_incref(_M0L8_2afieldS2976.$0);
      } else if (_M0L6_2acntS3321 == 1) {
        #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_free(_M0L7_2aSomeS885);
      }
      _M0L12_2apkg__nameS886 = _M0L8_2afieldS2976;
      if (_M0L6loggerS887.$1) {
        moonbit_incref(_M0L6loggerS887.$1);
      }
      #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS887.$0->$method_2(_M0L6loggerS887.$1, _M0L12_2apkg__nameS886);
      if (_M0L6loggerS887.$1) {
        moonbit_incref(_M0L6loggerS887.$1);
      }
      #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L6loggerS887.$0->$method_3(_M0L6loggerS887.$1, 47);
      break;
    }
    default: {
      moonbit_decref(_M0L16_2apackage__nameS884);
      break;
    }
  }
  _M0L8_2afieldS2975
  = (struct _M0TPC16string10StringView){
    _M0L4selfS865->$1_1, _M0L4selfS865->$1_2, _M0L4selfS865->$1_0
  };
  _M0L8filenameS2242 = _M0L8_2afieldS2975;
  moonbit_incref(_M0L8filenameS2242.$0);
  if (_M0L6loggerS887.$1) {
    moonbit_incref(_M0L6loggerS887.$1);
  }
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS887.$0->$method_2(_M0L6loggerS887.$1, _M0L8filenameS2242);
  if (_M0L6loggerS887.$1) {
    moonbit_incref(_M0L6loggerS887.$1);
  }
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS887.$0->$method_3(_M0L6loggerS887.$1, 58);
  _M0L8_2afieldS2974
  = (struct _M0TPC16string10StringView){
    _M0L4selfS865->$2_1, _M0L4selfS865->$2_2, _M0L4selfS865->$2_0
  };
  _M0L11start__lineS2243 = _M0L8_2afieldS2974;
  moonbit_incref(_M0L11start__lineS2243.$0);
  if (_M0L6loggerS887.$1) {
    moonbit_incref(_M0L6loggerS887.$1);
  }
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS887.$0->$method_2(_M0L6loggerS887.$1, _M0L11start__lineS2243);
  if (_M0L6loggerS887.$1) {
    moonbit_incref(_M0L6loggerS887.$1);
  }
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS887.$0->$method_3(_M0L6loggerS887.$1, 58);
  _M0L8_2afieldS2973
  = (struct _M0TPC16string10StringView){
    _M0L4selfS865->$3_1, _M0L4selfS865->$3_2, _M0L4selfS865->$3_0
  };
  _M0L13start__columnS2244 = _M0L8_2afieldS2973;
  moonbit_incref(_M0L13start__columnS2244.$0);
  if (_M0L6loggerS887.$1) {
    moonbit_incref(_M0L6loggerS887.$1);
  }
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS887.$0->$method_2(_M0L6loggerS887.$1, _M0L13start__columnS2244);
  if (_M0L6loggerS887.$1) {
    moonbit_incref(_M0L6loggerS887.$1);
  }
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS887.$0->$method_3(_M0L6loggerS887.$1, 45);
  _M0L8_2afieldS2972
  = (struct _M0TPC16string10StringView){
    _M0L4selfS865->$4_1, _M0L4selfS865->$4_2, _M0L4selfS865->$4_0
  };
  _M0L9end__lineS2245 = _M0L8_2afieldS2972;
  moonbit_incref(_M0L9end__lineS2245.$0);
  if (_M0L6loggerS887.$1) {
    moonbit_incref(_M0L6loggerS887.$1);
  }
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS887.$0->$method_2(_M0L6loggerS887.$1, _M0L9end__lineS2245);
  if (_M0L6loggerS887.$1) {
    moonbit_incref(_M0L6loggerS887.$1);
  }
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS887.$0->$method_3(_M0L6loggerS887.$1, 58);
  _M0L8_2afieldS2971
  = (struct _M0TPC16string10StringView){
    _M0L4selfS865->$5_1, _M0L4selfS865->$5_2, _M0L4selfS865->$5_0
  };
  _M0L6_2acntS3323 = Moonbit_object_header(_M0L4selfS865)->rc;
  if (_M0L6_2acntS3323 > 1) {
    int32_t _M0L11_2anew__cntS3329 = _M0L6_2acntS3323 - 1;
    Moonbit_object_header(_M0L4selfS865)->rc = _M0L11_2anew__cntS3329;
    moonbit_incref(_M0L8_2afieldS2971.$0);
  } else if (_M0L6_2acntS3323 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3328 =
      (struct _M0TPC16string10StringView){_M0L4selfS865->$4_1,
                                            _M0L4selfS865->$4_2,
                                            _M0L4selfS865->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3327;
    struct _M0TPC16string10StringView _M0L8_2afieldS3326;
    struct _M0TPC16string10StringView _M0L8_2afieldS3325;
    struct _M0TPC16string10StringView _M0L8_2afieldS3324;
    moonbit_decref(_M0L8_2afieldS3328.$0);
    _M0L8_2afieldS3327
    = (struct _M0TPC16string10StringView){
      _M0L4selfS865->$3_1, _M0L4selfS865->$3_2, _M0L4selfS865->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3327.$0);
    _M0L8_2afieldS3326
    = (struct _M0TPC16string10StringView){
      _M0L4selfS865->$2_1, _M0L4selfS865->$2_2, _M0L4selfS865->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3326.$0);
    _M0L8_2afieldS3325
    = (struct _M0TPC16string10StringView){
      _M0L4selfS865->$1_1, _M0L4selfS865->$1_2, _M0L4selfS865->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3325.$0);
    _M0L8_2afieldS3324
    = (struct _M0TPC16string10StringView){
      _M0L4selfS865->$0_1, _M0L4selfS865->$0_2, _M0L4selfS865->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3324.$0);
    #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS865);
  }
  _M0L11end__columnS2246 = _M0L8_2afieldS2971;
  if (_M0L6loggerS887.$1) {
    moonbit_incref(_M0L6loggerS887.$1);
  }
  #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS887.$0->$method_2(_M0L6loggerS887.$1, _M0L11end__columnS2246);
  if (_M0L6loggerS887.$1) {
    moonbit_incref(_M0L6loggerS887.$1);
  }
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS887.$0->$method_3(_M0L6loggerS887.$1, 64);
  #line 79 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6loggerS887.$0->$method_2(_M0L6loggerS887.$1, _M0L15_2amodule__nameS883);
  return 0;
}

int32_t _M0FPB7printlnGsE(moonbit_string_t _M0L5inputS863) {
  moonbit_string_t _M0L6_2atmpS2241;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6_2atmpS2241 = _M0IPC16string6StringPB4Show10to__string(_M0L5inputS863);
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_println(_M0L6_2atmpS2241);
  moonbit_decref(_M0L6_2atmpS2241);
  return 0;
}

int32_t _M0IPC13int3IntPB4Hash13hash__combine(
  int32_t _M0L4selfS862,
  struct _M0TPB6Hasher* _M0L6hasherS861
) {
  #line 532 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 533 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher12combine__int(_M0L6hasherS861, _M0L4selfS862);
  return 0;
}

int32_t _M0IPC16string6StringPB4Hash13hash__combine(
  moonbit_string_t _M0L4selfS860,
  struct _M0TPB6Hasher* _M0L6hasherS859
) {
  #line 498 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 499 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher15combine__string(_M0L6hasherS859, _M0L4selfS860);
  return 0;
}

int32_t _M0MPB6Hasher15combine__string(
  struct _M0TPB6Hasher* _M0L4selfS857,
  moonbit_string_t _M0L5valueS855
) {
  int32_t _M0L7_2abindS854;
  int32_t _M0L1iS856;
  #line 389 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L7_2abindS854 = Moonbit_array_length(_M0L5valueS855);
  _M0L1iS856 = 0;
  while (1) {
    if (_M0L1iS856 < _M0L7_2abindS854) {
      int32_t _M0L6_2atmpS2239 = _M0L5valueS855[_M0L1iS856];
      int32_t _M0L6_2atmpS2238 = (int32_t)_M0L6_2atmpS2239;
      uint32_t _M0L6_2atmpS2237 = *(uint32_t*)&_M0L6_2atmpS2238;
      int32_t _M0L6_2atmpS2240;
      moonbit_incref(_M0L4selfS857);
      #line 391 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
      _M0MPB6Hasher13combine__uint(_M0L4selfS857, _M0L6_2atmpS2237);
      _M0L6_2atmpS2240 = _M0L1iS856 + 1;
      _M0L1iS856 = _M0L6_2atmpS2240;
      continue;
    } else {
      moonbit_decref(_M0L4selfS857);
      moonbit_decref(_M0L5valueS855);
    }
    break;
  }
  return 0;
}

int32_t _M0MPC16string6String20unsafe__charcode__at(
  moonbit_string_t _M0L4selfS852,
  int32_t _M0L3idxS853
) {
  int32_t _M0L6_2atmpS2980;
  #line 1794 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS2980 = _M0L4selfS852[_M0L3idxS853];
  moonbit_decref(_M0L4selfS852);
  return _M0L6_2atmpS2980;
}

int32_t _M0MPB3Map8containsGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L4selfS848,
  moonbit_string_t _M0L3keyS844
) {
  int32_t _M0L4hashS843;
  int32_t _M0L14capacity__maskS2236;
  int32_t _M0L6_2atmpS2235;
  int32_t _M0L1iS845;
  int32_t _M0L3idxS846;
  #line 340 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS844);
  #line 342 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS843 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS844);
  _M0L14capacity__maskS2236 = _M0L4selfS848->$3;
  _M0L6_2atmpS2235 = _M0L4hashS843 & _M0L14capacity__maskS2236;
  _M0L1iS845 = 0;
  _M0L3idxS846 = _M0L6_2atmpS2235;
  while (1) {
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L8_2afieldS2985 =
      _M0L4selfS848->$0;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L7entriesS2234 =
      _M0L8_2afieldS2985;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS2984;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2abindS847;
    if (
      _M0L3idxS846 < 0
      || _M0L3idxS846 >= Moonbit_array_length(_M0L7entriesS2234)
    ) {
      #line 344 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2984
    = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*)_M0L7entriesS2234[
        _M0L3idxS846
      ];
    _M0L7_2abindS847 = _M0L6_2atmpS2984;
    if (_M0L7_2abindS847 == 0) {
      if (_M0L7_2abindS847) {
        moonbit_incref(_M0L7_2abindS847);
      }
      moonbit_decref(_M0L4selfS848);
      if (_M0L7_2abindS847) {
        moonbit_decref(_M0L7_2abindS847);
      }
      moonbit_decref(_M0L3keyS844);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2aSomeS849 =
        _M0L7_2abindS847;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L8_2aentryS850 =
        _M0L7_2aSomeS849;
      int32_t _M0L4hashS2228 = _M0L8_2aentryS850->$3;
      int32_t _if__result_3488;
      int32_t _M0L8_2afieldS2981;
      int32_t _M0L3pslS2229;
      int32_t _M0L6_2atmpS2230;
      int32_t _M0L6_2atmpS2232;
      int32_t _M0L14capacity__maskS2233;
      int32_t _M0L6_2atmpS2231;
      if (_M0L4hashS2228 == _M0L4hashS843) {
        moonbit_string_t _M0L8_2afieldS2983 = _M0L8_2aentryS850->$4;
        moonbit_string_t _M0L3keyS2227 = _M0L8_2afieldS2983;
        int32_t _M0L6_2atmpS2982;
        #line 345 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2982
        = moonbit_val_array_equal(_M0L3keyS2227, _M0L3keyS844);
        _if__result_3488 = _M0L6_2atmpS2982;
      } else {
        _if__result_3488 = 0;
      }
      if (_if__result_3488) {
        moonbit_decref(_M0L4selfS848);
        moonbit_decref(_M0L3keyS844);
        return 1;
      } else {
        moonbit_incref(_M0L8_2aentryS850);
      }
      _M0L8_2afieldS2981 = _M0L8_2aentryS850->$2;
      moonbit_decref(_M0L8_2aentryS850);
      _M0L3pslS2229 = _M0L8_2afieldS2981;
      if (_M0L1iS845 > _M0L3pslS2229) {
        moonbit_decref(_M0L4selfS848);
        moonbit_decref(_M0L3keyS844);
        return 0;
      }
      _M0L6_2atmpS2230 = _M0L1iS845 + 1;
      _M0L6_2atmpS2232 = _M0L3idxS846 + 1;
      _M0L14capacity__maskS2233 = _M0L4selfS848->$3;
      _M0L6_2atmpS2231 = _M0L6_2atmpS2232 & _M0L14capacity__maskS2233;
      _M0L1iS845 = _M0L6_2atmpS2230;
      _M0L3idxS846 = _M0L6_2atmpS2231;
      continue;
    }
    break;
  }
}

struct _M0TUWEuQRPC15error5ErrorNsE* _M0MPB3Map3getGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS830,
  int32_t _M0L3keyS826
) {
  int32_t _M0L4hashS825;
  int32_t _M0L14capacity__maskS2212;
  int32_t _M0L6_2atmpS2211;
  int32_t _M0L1iS827;
  int32_t _M0L3idxS828;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS825 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS826);
  _M0L14capacity__maskS2212 = _M0L4selfS830->$3;
  _M0L6_2atmpS2211 = _M0L4hashS825 & _M0L14capacity__maskS2212;
  _M0L1iS827 = 0;
  _M0L3idxS828 = _M0L6_2atmpS2211;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS2989 =
      _M0L4selfS830->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2210 =
      _M0L8_2afieldS2989;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2988;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS829;
    if (
      _M0L3idxS828 < 0
      || _M0L3idxS828 >= Moonbit_array_length(_M0L7entriesS2210)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2988
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2210[
        _M0L3idxS828
      ];
    _M0L7_2abindS829 = _M0L6_2atmpS2988;
    if (_M0L7_2abindS829 == 0) {
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2199;
      if (_M0L7_2abindS829) {
        moonbit_incref(_M0L7_2abindS829);
      }
      moonbit_decref(_M0L4selfS830);
      if (_M0L7_2abindS829) {
        moonbit_decref(_M0L7_2abindS829);
      }
      _M0L6_2atmpS2199 = 0;
      return _M0L6_2atmpS2199;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS831 =
        _M0L7_2abindS829;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aentryS832 =
        _M0L7_2aSomeS831;
      int32_t _M0L4hashS2201 = _M0L8_2aentryS832->$3;
      int32_t _if__result_3490;
      int32_t _M0L8_2afieldS2986;
      int32_t _M0L3pslS2204;
      int32_t _M0L6_2atmpS2206;
      int32_t _M0L6_2atmpS2208;
      int32_t _M0L14capacity__maskS2209;
      int32_t _M0L6_2atmpS2207;
      if (_M0L4hashS2201 == _M0L4hashS825) {
        int32_t _M0L3keyS2200 = _M0L8_2aentryS832->$4;
        _if__result_3490 = _M0L3keyS2200 == _M0L3keyS826;
      } else {
        _if__result_3490 = 0;
      }
      if (_if__result_3490) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS2987;
        int32_t _M0L6_2acntS3330;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS2203;
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2202;
        moonbit_incref(_M0L8_2aentryS832);
        moonbit_decref(_M0L4selfS830);
        _M0L8_2afieldS2987 = _M0L8_2aentryS832->$5;
        _M0L6_2acntS3330 = Moonbit_object_header(_M0L8_2aentryS832)->rc;
        if (_M0L6_2acntS3330 > 1) {
          int32_t _M0L11_2anew__cntS3332 = _M0L6_2acntS3330 - 1;
          Moonbit_object_header(_M0L8_2aentryS832)->rc
          = _M0L11_2anew__cntS3332;
          moonbit_incref(_M0L8_2afieldS2987);
        } else if (_M0L6_2acntS3330 == 1) {
          struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3331 =
            _M0L8_2aentryS832->$1;
          if (_M0L8_2afieldS3331) {
            moonbit_decref(_M0L8_2afieldS3331);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS832);
        }
        _M0L5valueS2203 = _M0L8_2afieldS2987;
        _M0L6_2atmpS2202 = _M0L5valueS2203;
        return _M0L6_2atmpS2202;
      } else {
        moonbit_incref(_M0L8_2aentryS832);
      }
      _M0L8_2afieldS2986 = _M0L8_2aentryS832->$2;
      moonbit_decref(_M0L8_2aentryS832);
      _M0L3pslS2204 = _M0L8_2afieldS2986;
      if (_M0L1iS827 > _M0L3pslS2204) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2205;
        moonbit_decref(_M0L4selfS830);
        _M0L6_2atmpS2205 = 0;
        return _M0L6_2atmpS2205;
      }
      _M0L6_2atmpS2206 = _M0L1iS827 + 1;
      _M0L6_2atmpS2208 = _M0L3idxS828 + 1;
      _M0L14capacity__maskS2209 = _M0L4selfS830->$3;
      _M0L6_2atmpS2207 = _M0L6_2atmpS2208 & _M0L14capacity__maskS2209;
      _M0L1iS827 = _M0L6_2atmpS2206;
      _M0L3idxS828 = _M0L6_2atmpS2207;
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
  int32_t _M0L14capacity__maskS2226;
  int32_t _M0L6_2atmpS2225;
  int32_t _M0L1iS836;
  int32_t _M0L3idxS837;
  #line 216 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS835);
  #line 217 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L4hashS834 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS835);
  _M0L14capacity__maskS2226 = _M0L4selfS839->$3;
  _M0L6_2atmpS2225 = _M0L4hashS834 & _M0L14capacity__maskS2226;
  _M0L1iS836 = 0;
  _M0L3idxS837 = _M0L6_2atmpS2225;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2995 =
      _M0L4selfS839->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2224 =
      _M0L8_2afieldS2995;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2994;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS838;
    if (
      _M0L3idxS837 < 0
      || _M0L3idxS837 >= Moonbit_array_length(_M0L7entriesS2224)
    ) {
      #line 219 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS2994
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2224[
        _M0L3idxS837
      ];
    _M0L7_2abindS838 = _M0L6_2atmpS2994;
    if (_M0L7_2abindS838 == 0) {
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2213;
      if (_M0L7_2abindS838) {
        moonbit_incref(_M0L7_2abindS838);
      }
      moonbit_decref(_M0L4selfS839);
      if (_M0L7_2abindS838) {
        moonbit_decref(_M0L7_2abindS838);
      }
      moonbit_decref(_M0L3keyS835);
      _M0L6_2atmpS2213 = 0;
      return _M0L6_2atmpS2213;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS840 =
        _M0L7_2abindS838;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aentryS841 =
        _M0L7_2aSomeS840;
      int32_t _M0L4hashS2215 = _M0L8_2aentryS841->$3;
      int32_t _if__result_3492;
      int32_t _M0L8_2afieldS2990;
      int32_t _M0L3pslS2218;
      int32_t _M0L6_2atmpS2220;
      int32_t _M0L6_2atmpS2222;
      int32_t _M0L14capacity__maskS2223;
      int32_t _M0L6_2atmpS2221;
      if (_M0L4hashS2215 == _M0L4hashS834) {
        moonbit_string_t _M0L8_2afieldS2993 = _M0L8_2aentryS841->$4;
        moonbit_string_t _M0L3keyS2214 = _M0L8_2afieldS2993;
        int32_t _M0L6_2atmpS2992;
        #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS2992
        = moonbit_val_array_equal(_M0L3keyS2214, _M0L3keyS835);
        _if__result_3492 = _M0L6_2atmpS2992;
      } else {
        _if__result_3492 = 0;
      }
      if (_if__result_3492) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2991;
        int32_t _M0L6_2acntS3333;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS2217;
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2216;
        moonbit_incref(_M0L8_2aentryS841);
        moonbit_decref(_M0L4selfS839);
        moonbit_decref(_M0L3keyS835);
        _M0L8_2afieldS2991 = _M0L8_2aentryS841->$5;
        _M0L6_2acntS3333 = Moonbit_object_header(_M0L8_2aentryS841)->rc;
        if (_M0L6_2acntS3333 > 1) {
          int32_t _M0L11_2anew__cntS3336 = _M0L6_2acntS3333 - 1;
          Moonbit_object_header(_M0L8_2aentryS841)->rc
          = _M0L11_2anew__cntS3336;
          moonbit_incref(_M0L8_2afieldS2991);
        } else if (_M0L6_2acntS3333 == 1) {
          moonbit_string_t _M0L8_2afieldS3335 = _M0L8_2aentryS841->$4;
          struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3334;
          moonbit_decref(_M0L8_2afieldS3335);
          _M0L8_2afieldS3334 = _M0L8_2aentryS841->$1;
          if (_M0L8_2afieldS3334) {
            moonbit_decref(_M0L8_2afieldS3334);
          }
          #line 221 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          moonbit_free(_M0L8_2aentryS841);
        }
        _M0L5valueS2217 = _M0L8_2afieldS2991;
        _M0L6_2atmpS2216 = _M0L5valueS2217;
        return _M0L6_2atmpS2216;
      } else {
        moonbit_incref(_M0L8_2aentryS841);
      }
      _M0L8_2afieldS2990 = _M0L8_2aentryS841->$2;
      moonbit_decref(_M0L8_2aentryS841);
      _M0L3pslS2218 = _M0L8_2afieldS2990;
      if (_M0L1iS836 > _M0L3pslS2218) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2219;
        moonbit_decref(_M0L4selfS839);
        moonbit_decref(_M0L3keyS835);
        _M0L6_2atmpS2219 = 0;
        return _M0L6_2atmpS2219;
      }
      _M0L6_2atmpS2220 = _M0L1iS836 + 1;
      _M0L6_2atmpS2222 = _M0L3idxS837 + 1;
      _M0L14capacity__maskS2223 = _M0L4selfS839->$3;
      _M0L6_2atmpS2221 = _M0L6_2atmpS2222 & _M0L14capacity__maskS2223;
      _M0L1iS836 = _M0L6_2atmpS2220;
      _M0L3idxS837 = _M0L6_2atmpS2221;
      continue;
    }
    break;
  }
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L3arrS786
) {
  int32_t _M0L6lengthS785;
  int32_t _M0Lm8capacityS787;
  int32_t _M0L6_2atmpS2140;
  int32_t _M0L6_2atmpS2139;
  int32_t _M0L6_2atmpS2150;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1mS788;
  int32_t _M0L3endS2148;
  int32_t _M0L5startS2149;
  int32_t _M0L7_2abindS789;
  int32_t _M0L2__S790;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS786.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS785
  = _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L3arrS786);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS787 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS785);
  _M0L6_2atmpS2140 = _M0Lm8capacityS787;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2139 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2140);
  if (_M0L6lengthS785 > _M0L6_2atmpS2139) {
    int32_t _M0L6_2atmpS2141 = _M0Lm8capacityS787;
    _M0Lm8capacityS787 = _M0L6_2atmpS2141 * 2;
  }
  _M0L6_2atmpS2150 = _M0Lm8capacityS787;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS788
  = _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS2150);
  _M0L3endS2148 = _M0L3arrS786.$2;
  _M0L5startS2149 = _M0L3arrS786.$1;
  _M0L7_2abindS789 = _M0L3endS2148 - _M0L5startS2149;
  _M0L2__S790 = 0;
  while (1) {
    if (_M0L2__S790 < _M0L7_2abindS789) {
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS2999 =
        _M0L3arrS786.$0;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L3bufS2145 =
        _M0L8_2afieldS2999;
      int32_t _M0L5startS2147 = _M0L3arrS786.$1;
      int32_t _M0L6_2atmpS2146 = _M0L5startS2147 + _M0L2__S790;
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2998 =
        (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L3bufS2145[
          _M0L6_2atmpS2146
        ];
      struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L1eS791 =
        _M0L6_2atmpS2998;
      moonbit_string_t _M0L8_2afieldS2997 = _M0L1eS791->$0;
      moonbit_string_t _M0L6_2atmpS2142 = _M0L8_2afieldS2997;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS2996 =
        _M0L1eS791->$1;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2143 =
        _M0L8_2afieldS2996;
      int32_t _M0L6_2atmpS2144;
      moonbit_incref(_M0L6_2atmpS2143);
      moonbit_incref(_M0L6_2atmpS2142);
      moonbit_incref(_M0L1mS788);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L1mS788, _M0L6_2atmpS2142, _M0L6_2atmpS2143);
      _M0L6_2atmpS2144 = _M0L2__S790 + 1;
      _M0L2__S790 = _M0L6_2atmpS2144;
      continue;
    } else {
      moonbit_decref(_M0L3arrS786.$0);
    }
    break;
  }
  return _M0L1mS788;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L3arrS794
) {
  int32_t _M0L6lengthS793;
  int32_t _M0Lm8capacityS795;
  int32_t _M0L6_2atmpS2152;
  int32_t _M0L6_2atmpS2151;
  int32_t _M0L6_2atmpS2162;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L1mS796;
  int32_t _M0L3endS2160;
  int32_t _M0L5startS2161;
  int32_t _M0L7_2abindS797;
  int32_t _M0L2__S798;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS794.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS793
  = _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(_M0L3arrS794);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS795 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS793);
  _M0L6_2atmpS2152 = _M0Lm8capacityS795;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2151 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2152);
  if (_M0L6lengthS793 > _M0L6_2atmpS2151) {
    int32_t _M0L6_2atmpS2153 = _M0Lm8capacityS795;
    _M0Lm8capacityS795 = _M0L6_2atmpS2153 * 2;
  }
  _M0L6_2atmpS2162 = _M0Lm8capacityS795;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS796
  = _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS2162);
  _M0L3endS2160 = _M0L3arrS794.$2;
  _M0L5startS2161 = _M0L3arrS794.$1;
  _M0L7_2abindS797 = _M0L3endS2160 - _M0L5startS2161;
  _M0L2__S798 = 0;
  while (1) {
    if (_M0L2__S798 < _M0L7_2abindS797) {
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3002 =
        _M0L3arrS794.$0;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L3bufS2157 =
        _M0L8_2afieldS3002;
      int32_t _M0L5startS2159 = _M0L3arrS794.$1;
      int32_t _M0L6_2atmpS2158 = _M0L5startS2159 + _M0L2__S798;
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3001 =
        (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)_M0L3bufS2157[
          _M0L6_2atmpS2158
        ];
      struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L1eS799 = _M0L6_2atmpS3001;
      int32_t _M0L6_2atmpS2154 = _M0L1eS799->$0;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3000 =
        _M0L1eS799->$1;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2atmpS2155 =
        _M0L8_2afieldS3000;
      int32_t _M0L6_2atmpS2156;
      moonbit_incref(_M0L6_2atmpS2155);
      moonbit_incref(_M0L1mS796);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(_M0L1mS796, _M0L6_2atmpS2154, _M0L6_2atmpS2155);
      _M0L6_2atmpS2156 = _M0L2__S798 + 1;
      _M0L2__S798 = _M0L6_2atmpS2156;
      continue;
    } else {
      moonbit_decref(_M0L3arrS794.$0);
    }
    break;
  }
  return _M0L1mS796;
}

struct _M0TPB3MapGssE* _M0MPB3Map11from__arrayGssE(
  struct _M0TPB9ArrayViewGUssEE _M0L3arrS802
) {
  int32_t _M0L6lengthS801;
  int32_t _M0Lm8capacityS803;
  int32_t _M0L6_2atmpS2164;
  int32_t _M0L6_2atmpS2163;
  int32_t _M0L6_2atmpS2174;
  struct _M0TPB3MapGssE* _M0L1mS804;
  int32_t _M0L3endS2172;
  int32_t _M0L5startS2173;
  int32_t _M0L7_2abindS805;
  int32_t _M0L2__S806;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS802.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS801 = _M0MPC15array9ArrayView6lengthGUssEE(_M0L3arrS802);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS803 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS801);
  _M0L6_2atmpS2164 = _M0Lm8capacityS803;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2163 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2164);
  if (_M0L6lengthS801 > _M0L6_2atmpS2163) {
    int32_t _M0L6_2atmpS2165 = _M0Lm8capacityS803;
    _M0Lm8capacityS803 = _M0L6_2atmpS2165 * 2;
  }
  _M0L6_2atmpS2174 = _M0Lm8capacityS803;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS804 = _M0MPB3Map11new_2einnerGssE(_M0L6_2atmpS2174);
  _M0L3endS2172 = _M0L3arrS802.$2;
  _M0L5startS2173 = _M0L3arrS802.$1;
  _M0L7_2abindS805 = _M0L3endS2172 - _M0L5startS2173;
  _M0L2__S806 = 0;
  while (1) {
    if (_M0L2__S806 < _M0L7_2abindS805) {
      struct _M0TUssE** _M0L8_2afieldS3006 = _M0L3arrS802.$0;
      struct _M0TUssE** _M0L3bufS2169 = _M0L8_2afieldS3006;
      int32_t _M0L5startS2171 = _M0L3arrS802.$1;
      int32_t _M0L6_2atmpS2170 = _M0L5startS2171 + _M0L2__S806;
      struct _M0TUssE* _M0L6_2atmpS3005 =
        (struct _M0TUssE*)_M0L3bufS2169[_M0L6_2atmpS2170];
      struct _M0TUssE* _M0L1eS807 = _M0L6_2atmpS3005;
      moonbit_string_t _M0L8_2afieldS3004 = _M0L1eS807->$0;
      moonbit_string_t _M0L6_2atmpS2166 = _M0L8_2afieldS3004;
      moonbit_string_t _M0L8_2afieldS3003 = _M0L1eS807->$1;
      moonbit_string_t _M0L6_2atmpS2167 = _M0L8_2afieldS3003;
      int32_t _M0L6_2atmpS2168;
      moonbit_incref(_M0L6_2atmpS2167);
      moonbit_incref(_M0L6_2atmpS2166);
      moonbit_incref(_M0L1mS804);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGssE(_M0L1mS804, _M0L6_2atmpS2166, _M0L6_2atmpS2167);
      _M0L6_2atmpS2168 = _M0L2__S806 + 1;
      _M0L2__S806 = _M0L6_2atmpS2168;
      continue;
    } else {
      moonbit_decref(_M0L3arrS802.$0);
    }
    break;
  }
  return _M0L1mS804;
}

struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0MPB3Map11from__arrayGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13CliToolConfigEE _M0L3arrS810
) {
  int32_t _M0L6lengthS809;
  int32_t _M0Lm8capacityS811;
  int32_t _M0L6_2atmpS2176;
  int32_t _M0L6_2atmpS2175;
  int32_t _M0L6_2atmpS2186;
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L1mS812;
  int32_t _M0L3endS2184;
  int32_t _M0L5startS2185;
  int32_t _M0L7_2abindS813;
  int32_t _M0L2__S814;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS810.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS809
  = _M0MPC15array9ArrayView6lengthGUsRP38clawteam8clawteam6config13CliToolConfigEE(_M0L3arrS810);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS811 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS809);
  _M0L6_2atmpS2176 = _M0Lm8capacityS811;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2175 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2176);
  if (_M0L6lengthS809 > _M0L6_2atmpS2175) {
    int32_t _M0L6_2atmpS2177 = _M0Lm8capacityS811;
    _M0Lm8capacityS811 = _M0L6_2atmpS2177 * 2;
  }
  _M0L6_2atmpS2186 = _M0Lm8capacityS811;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS812
  = _M0MPB3Map11new_2einnerGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L6_2atmpS2186);
  _M0L3endS2184 = _M0L3arrS810.$2;
  _M0L5startS2185 = _M0L3arrS810.$1;
  _M0L7_2abindS813 = _M0L3endS2184 - _M0L5startS2185;
  _M0L2__S814 = 0;
  while (1) {
    if (_M0L2__S814 < _M0L7_2abindS813) {
      struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE** _M0L8_2afieldS3010 =
        _M0L3arrS810.$0;
      struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE** _M0L3bufS2181 =
        _M0L8_2afieldS3010;
      int32_t _M0L5startS2183 = _M0L3arrS810.$1;
      int32_t _M0L6_2atmpS2182 = _M0L5startS2183 + _M0L2__S814;
      struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS3009 =
        (struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE*)_M0L3bufS2181[
          _M0L6_2atmpS2182
        ];
      struct _M0TUsRP38clawteam8clawteam6config13CliToolConfigE* _M0L1eS815 =
        _M0L6_2atmpS3009;
      moonbit_string_t _M0L8_2afieldS3008 = _M0L1eS815->$0;
      moonbit_string_t _M0L6_2atmpS2178 = _M0L8_2afieldS3008;
      struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L8_2afieldS3007 =
        _M0L1eS815->$1;
      struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L6_2atmpS2179 =
        _M0L8_2afieldS3007;
      int32_t _M0L6_2atmpS2180;
      moonbit_incref(_M0L6_2atmpS2179);
      moonbit_incref(_M0L6_2atmpS2178);
      moonbit_incref(_M0L1mS812);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L1mS812, _M0L6_2atmpS2178, _M0L6_2atmpS2179);
      _M0L6_2atmpS2180 = _M0L2__S814 + 1;
      _M0L2__S814 = _M0L6_2atmpS2180;
      continue;
    } else {
      moonbit_decref(_M0L3arrS810.$0);
    }
    break;
  }
  return _M0L1mS812;
}

struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0MPB3Map11from__arrayGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13ChannelConfigEE _M0L3arrS818
) {
  int32_t _M0L6lengthS817;
  int32_t _M0Lm8capacityS819;
  int32_t _M0L6_2atmpS2188;
  int32_t _M0L6_2atmpS2187;
  int32_t _M0L6_2atmpS2198;
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L1mS820;
  int32_t _M0L3endS2196;
  int32_t _M0L5startS2197;
  int32_t _M0L7_2abindS821;
  int32_t _M0L2__S822;
  #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3arrS818.$0);
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6lengthS817
  = _M0MPC15array9ArrayView6lengthGUsRP38clawteam8clawteam6config13ChannelConfigEE(_M0L3arrS818);
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0Lm8capacityS819 = _M0MPC13int3Int20next__power__of__two(_M0L6lengthS817);
  _M0L6_2atmpS2188 = _M0Lm8capacityS819;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2187 = _M0FPB21calc__grow__threshold(_M0L6_2atmpS2188);
  if (_M0L6lengthS817 > _M0L6_2atmpS2187) {
    int32_t _M0L6_2atmpS2189 = _M0Lm8capacityS819;
    _M0Lm8capacityS819 = _M0L6_2atmpS2189 * 2;
  }
  _M0L6_2atmpS2198 = _M0Lm8capacityS819;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L1mS820
  = _M0MPB3Map11new_2einnerGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L6_2atmpS2198);
  _M0L3endS2196 = _M0L3arrS818.$2;
  _M0L5startS2197 = _M0L3arrS818.$1;
  _M0L7_2abindS821 = _M0L3endS2196 - _M0L5startS2197;
  _M0L2__S822 = 0;
  while (1) {
    if (_M0L2__S822 < _M0L7_2abindS821) {
      struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE** _M0L8_2afieldS3014 =
        _M0L3arrS818.$0;
      struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE** _M0L3bufS2193 =
        _M0L8_2afieldS3014;
      int32_t _M0L5startS2195 = _M0L3arrS818.$1;
      int32_t _M0L6_2atmpS2194 = _M0L5startS2195 + _M0L2__S822;
      struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS3013 =
        (struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE*)_M0L3bufS2193[
          _M0L6_2atmpS2194
        ];
      struct _M0TUsRP38clawteam8clawteam6config13ChannelConfigE* _M0L1eS823 =
        _M0L6_2atmpS3013;
      moonbit_string_t _M0L8_2afieldS3012 = _M0L1eS823->$0;
      moonbit_string_t _M0L6_2atmpS2190 = _M0L8_2afieldS3012;
      struct _M0TP38clawteam8clawteam6config13ChannelConfig* _M0L8_2afieldS3011 =
        _M0L1eS823->$1;
      struct _M0TP38clawteam8clawteam6config13ChannelConfig* _M0L6_2atmpS2191 =
        _M0L8_2afieldS3011;
      int32_t _M0L6_2atmpS2192;
      moonbit_incref(_M0L6_2atmpS2191);
      moonbit_incref(_M0L6_2atmpS2190);
      moonbit_incref(_M0L1mS820);
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map3setGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L1mS820, _M0L6_2atmpS2190, _M0L6_2atmpS2191);
      _M0L6_2atmpS2192 = _M0L2__S822 + 1;
      _M0L2__S822 = _M0L6_2atmpS2192;
      continue;
    } else {
      moonbit_decref(_M0L3arrS818.$0);
    }
    break;
  }
  return _M0L1mS820;
}

int32_t _M0MPB3Map3setGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS770,
  moonbit_string_t _M0L3keyS771,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS772
) {
  int32_t _M0L6_2atmpS2134;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS771);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2134 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS771);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS770, _M0L3keyS771, _M0L5valueS772, _M0L6_2atmpS2134);
  return 0;
}

int32_t _M0MPB3Map3setGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS773,
  int32_t _M0L3keyS774,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS775
) {
  int32_t _M0L6_2atmpS2135;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2135 = _M0IP016_24default__implPB4Hash4hashGiE(_M0L3keyS774);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS773, _M0L3keyS774, _M0L5valueS775, _M0L6_2atmpS2135);
  return 0;
}

int32_t _M0MPB3Map3setGssE(
  struct _M0TPB3MapGssE* _M0L4selfS776,
  moonbit_string_t _M0L3keyS777,
  moonbit_string_t _M0L5valueS778
) {
  int32_t _M0L6_2atmpS2136;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS777);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2136 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS777);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGssE(_M0L4selfS776, _M0L3keyS777, _M0L5valueS778, _M0L6_2atmpS2136);
  return 0;
}

int32_t _M0MPB3Map3setGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L4selfS779,
  moonbit_string_t _M0L3keyS780,
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L5valueS781
) {
  int32_t _M0L6_2atmpS2137;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS780);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2137 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS780);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L4selfS779, _M0L3keyS780, _M0L5valueS781, _M0L6_2atmpS2137);
  return 0;
}

int32_t _M0MPB3Map3setGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L4selfS782,
  moonbit_string_t _M0L3keyS783,
  struct _M0TP38clawteam8clawteam6config13ChannelConfig* _M0L5valueS784
) {
  int32_t _M0L6_2atmpS2138;
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  moonbit_incref(_M0L3keyS783);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2138 = _M0IP016_24default__implPB4Hash4hashGsE(_M0L3keyS783);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0MPB3Map15set__with__hashGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L4selfS782, _M0L3keyS783, _M0L5valueS784, _M0L6_2atmpS2138);
  return 0;
}

int32_t _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS716
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3021;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L9old__headS715;
  int32_t _M0L8capacityS2105;
  int32_t _M0L13new__capacityS717;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2100;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS2099;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2aoldS3020;
  int32_t _M0L6_2atmpS2101;
  int32_t _M0L8capacityS2103;
  int32_t _M0L6_2atmpS2102;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS2104;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3019;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2aparamS718;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3021 = _M0L4selfS716->$5;
  _M0L9old__headS715 = _M0L8_2afieldS3021;
  _M0L8capacityS2105 = _M0L4selfS716->$2;
  _M0L13new__capacityS717 = _M0L8capacityS2105 << 1;
  _M0L6_2atmpS2100 = 0;
  _M0L6_2atmpS2099
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L13new__capacityS717, _M0L6_2atmpS2100);
  _M0L6_2aoldS3020 = _M0L4selfS716->$0;
  if (_M0L9old__headS715) {
    moonbit_incref(_M0L9old__headS715);
  }
  moonbit_decref(_M0L6_2aoldS3020);
  _M0L4selfS716->$0 = _M0L6_2atmpS2099;
  _M0L4selfS716->$2 = _M0L13new__capacityS717;
  _M0L6_2atmpS2101 = _M0L13new__capacityS717 - 1;
  _M0L4selfS716->$3 = _M0L6_2atmpS2101;
  _M0L8capacityS2103 = _M0L4selfS716->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2102 = _M0FPB21calc__grow__threshold(_M0L8capacityS2103);
  _M0L4selfS716->$4 = _M0L6_2atmpS2102;
  _M0L4selfS716->$1 = 0;
  _M0L6_2atmpS2104 = 0;
  _M0L6_2aoldS3019 = _M0L4selfS716->$5;
  if (_M0L6_2aoldS3019) {
    moonbit_decref(_M0L6_2aoldS3019);
  }
  _M0L4selfS716->$5 = _M0L6_2atmpS2104;
  _M0L4selfS716->$6 = -1;
  _M0L8_2aparamS718 = _M0L9old__headS715;
  while (1) {
    if (_M0L8_2aparamS718 == 0) {
      if (_M0L8_2aparamS718) {
        moonbit_decref(_M0L8_2aparamS718);
      }
      moonbit_decref(_M0L4selfS716);
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS719 =
        _M0L8_2aparamS718;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4_2axS720 =
        _M0L7_2aSomeS719;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3018 =
        _M0L4_2axS720->$1;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS721 =
        _M0L8_2afieldS3018;
      moonbit_string_t _M0L8_2afieldS3017 = _M0L4_2axS720->$4;
      moonbit_string_t _M0L6_2akeyS722 = _M0L8_2afieldS3017;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3016 =
        _M0L4_2axS720->$5;
      struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2avalueS723 =
        _M0L8_2afieldS3016;
      int32_t _M0L8_2afieldS3015 = _M0L4_2axS720->$3;
      int32_t _M0L6_2acntS3337 = Moonbit_object_header(_M0L4_2axS720)->rc;
      int32_t _M0L7_2ahashS724;
      if (_M0L6_2acntS3337 > 1) {
        int32_t _M0L11_2anew__cntS3338 = _M0L6_2acntS3337 - 1;
        Moonbit_object_header(_M0L4_2axS720)->rc = _M0L11_2anew__cntS3338;
        moonbit_incref(_M0L8_2avalueS723);
        moonbit_incref(_M0L6_2akeyS722);
        if (_M0L7_2anextS721) {
          moonbit_incref(_M0L7_2anextS721);
        }
      } else if (_M0L6_2acntS3337 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS720);
      }
      _M0L7_2ahashS724 = _M0L8_2afieldS3015;
      moonbit_incref(_M0L4selfS716);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS716, _M0L6_2akeyS722, _M0L8_2avalueS723, _M0L7_2ahashS724);
      _M0L8_2aparamS718 = _M0L7_2anextS721;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS727
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3027;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L9old__headS726;
  int32_t _M0L8capacityS2112;
  int32_t _M0L13new__capacityS728;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2107;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS2106;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L6_2aoldS3026;
  int32_t _M0L6_2atmpS2108;
  int32_t _M0L8capacityS2110;
  int32_t _M0L6_2atmpS2109;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS2111;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3025;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2aparamS729;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3027 = _M0L4selfS727->$5;
  _M0L9old__headS726 = _M0L8_2afieldS3027;
  _M0L8capacityS2112 = _M0L4selfS727->$2;
  _M0L13new__capacityS728 = _M0L8capacityS2112 << 1;
  _M0L6_2atmpS2107 = 0;
  _M0L6_2atmpS2106
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L13new__capacityS728, _M0L6_2atmpS2107);
  _M0L6_2aoldS3026 = _M0L4selfS727->$0;
  if (_M0L9old__headS726) {
    moonbit_incref(_M0L9old__headS726);
  }
  moonbit_decref(_M0L6_2aoldS3026);
  _M0L4selfS727->$0 = _M0L6_2atmpS2106;
  _M0L4selfS727->$2 = _M0L13new__capacityS728;
  _M0L6_2atmpS2108 = _M0L13new__capacityS728 - 1;
  _M0L4selfS727->$3 = _M0L6_2atmpS2108;
  _M0L8capacityS2110 = _M0L4selfS727->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2109 = _M0FPB21calc__grow__threshold(_M0L8capacityS2110);
  _M0L4selfS727->$4 = _M0L6_2atmpS2109;
  _M0L4selfS727->$1 = 0;
  _M0L6_2atmpS2111 = 0;
  _M0L6_2aoldS3025 = _M0L4selfS727->$5;
  if (_M0L6_2aoldS3025) {
    moonbit_decref(_M0L6_2aoldS3025);
  }
  _M0L4selfS727->$5 = _M0L6_2atmpS2111;
  _M0L4selfS727->$6 = -1;
  _M0L8_2aparamS729 = _M0L9old__headS726;
  while (1) {
    if (_M0L8_2aparamS729 == 0) {
      if (_M0L8_2aparamS729) {
        moonbit_decref(_M0L8_2aparamS729);
      }
      moonbit_decref(_M0L4selfS727);
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS730 =
        _M0L8_2aparamS729;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4_2axS731 =
        _M0L7_2aSomeS730;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3024 =
        _M0L4_2axS731->$1;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS732 =
        _M0L8_2afieldS3024;
      int32_t _M0L6_2akeyS733 = _M0L4_2axS731->$4;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3023 =
        _M0L4_2axS731->$5;
      struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2avalueS734 =
        _M0L8_2afieldS3023;
      int32_t _M0L8_2afieldS3022 = _M0L4_2axS731->$3;
      int32_t _M0L6_2acntS3339 = Moonbit_object_header(_M0L4_2axS731)->rc;
      int32_t _M0L7_2ahashS735;
      if (_M0L6_2acntS3339 > 1) {
        int32_t _M0L11_2anew__cntS3340 = _M0L6_2acntS3339 - 1;
        Moonbit_object_header(_M0L4_2axS731)->rc = _M0L11_2anew__cntS3340;
        moonbit_incref(_M0L8_2avalueS734);
        if (_M0L7_2anextS732) {
          moonbit_incref(_M0L7_2anextS732);
        }
      } else if (_M0L6_2acntS3339 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS731);
      }
      _M0L7_2ahashS735 = _M0L8_2afieldS3022;
      moonbit_incref(_M0L4selfS727);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS727, _M0L6_2akeyS733, _M0L8_2avalueS734, _M0L7_2ahashS735);
      _M0L8_2aparamS729 = _M0L7_2anextS732;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGssE(struct _M0TPB3MapGssE* _M0L4selfS738) {
  struct _M0TPB5EntryGssE* _M0L8_2afieldS3034;
  struct _M0TPB5EntryGssE* _M0L9old__headS737;
  int32_t _M0L8capacityS2119;
  int32_t _M0L13new__capacityS739;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2114;
  struct _M0TPB5EntryGssE** _M0L6_2atmpS2113;
  struct _M0TPB5EntryGssE** _M0L6_2aoldS3033;
  int32_t _M0L6_2atmpS2115;
  int32_t _M0L8capacityS2117;
  int32_t _M0L6_2atmpS2116;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS2118;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3032;
  struct _M0TPB5EntryGssE* _M0L8_2aparamS740;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3034 = _M0L4selfS738->$5;
  _M0L9old__headS737 = _M0L8_2afieldS3034;
  _M0L8capacityS2119 = _M0L4selfS738->$2;
  _M0L13new__capacityS739 = _M0L8capacityS2119 << 1;
  _M0L6_2atmpS2114 = 0;
  _M0L6_2atmpS2113
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L13new__capacityS739, _M0L6_2atmpS2114);
  _M0L6_2aoldS3033 = _M0L4selfS738->$0;
  if (_M0L9old__headS737) {
    moonbit_incref(_M0L9old__headS737);
  }
  moonbit_decref(_M0L6_2aoldS3033);
  _M0L4selfS738->$0 = _M0L6_2atmpS2113;
  _M0L4selfS738->$2 = _M0L13new__capacityS739;
  _M0L6_2atmpS2115 = _M0L13new__capacityS739 - 1;
  _M0L4selfS738->$3 = _M0L6_2atmpS2115;
  _M0L8capacityS2117 = _M0L4selfS738->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2116 = _M0FPB21calc__grow__threshold(_M0L8capacityS2117);
  _M0L4selfS738->$4 = _M0L6_2atmpS2116;
  _M0L4selfS738->$1 = 0;
  _M0L6_2atmpS2118 = 0;
  _M0L6_2aoldS3032 = _M0L4selfS738->$5;
  if (_M0L6_2aoldS3032) {
    moonbit_decref(_M0L6_2aoldS3032);
  }
  _M0L4selfS738->$5 = _M0L6_2atmpS2118;
  _M0L4selfS738->$6 = -1;
  _M0L8_2aparamS740 = _M0L9old__headS737;
  while (1) {
    if (_M0L8_2aparamS740 == 0) {
      if (_M0L8_2aparamS740) {
        moonbit_decref(_M0L8_2aparamS740);
      }
      moonbit_decref(_M0L4selfS738);
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS741 = _M0L8_2aparamS740;
      struct _M0TPB5EntryGssE* _M0L4_2axS742 = _M0L7_2aSomeS741;
      struct _M0TPB5EntryGssE* _M0L8_2afieldS3031 = _M0L4_2axS742->$1;
      struct _M0TPB5EntryGssE* _M0L7_2anextS743 = _M0L8_2afieldS3031;
      moonbit_string_t _M0L8_2afieldS3030 = _M0L4_2axS742->$4;
      moonbit_string_t _M0L6_2akeyS744 = _M0L8_2afieldS3030;
      moonbit_string_t _M0L8_2afieldS3029 = _M0L4_2axS742->$5;
      moonbit_string_t _M0L8_2avalueS745 = _M0L8_2afieldS3029;
      int32_t _M0L8_2afieldS3028 = _M0L4_2axS742->$3;
      int32_t _M0L6_2acntS3341 = Moonbit_object_header(_M0L4_2axS742)->rc;
      int32_t _M0L7_2ahashS746;
      if (_M0L6_2acntS3341 > 1) {
        int32_t _M0L11_2anew__cntS3342 = _M0L6_2acntS3341 - 1;
        Moonbit_object_header(_M0L4_2axS742)->rc = _M0L11_2anew__cntS3342;
        moonbit_incref(_M0L8_2avalueS745);
        moonbit_incref(_M0L6_2akeyS744);
        if (_M0L7_2anextS743) {
          moonbit_incref(_M0L7_2anextS743);
        }
      } else if (_M0L6_2acntS3341 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS742);
      }
      _M0L7_2ahashS746 = _M0L8_2afieldS3028;
      moonbit_incref(_M0L4selfS738);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGssE(_M0L4selfS738, _M0L6_2akeyS744, _M0L8_2avalueS745, _M0L7_2ahashS746);
      _M0L8_2aparamS740 = _M0L7_2anextS743;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L4selfS749
) {
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L8_2afieldS3041;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L9old__headS748;
  int32_t _M0L8capacityS2126;
  int32_t _M0L13new__capacityS750;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS2121;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L6_2atmpS2120;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L6_2aoldS3040;
  int32_t _M0L6_2atmpS2122;
  int32_t _M0L8capacityS2124;
  int32_t _M0L6_2atmpS2123;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS2125;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2aoldS3039;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L8_2aparamS751;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3041 = _M0L4selfS749->$5;
  _M0L9old__headS748 = _M0L8_2afieldS3041;
  _M0L8capacityS2126 = _M0L4selfS749->$2;
  _M0L13new__capacityS750 = _M0L8capacityS2126 << 1;
  _M0L6_2atmpS2121 = 0;
  _M0L6_2atmpS2120
  = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE**)moonbit_make_ref_array(_M0L13new__capacityS750, _M0L6_2atmpS2121);
  _M0L6_2aoldS3040 = _M0L4selfS749->$0;
  if (_M0L9old__headS748) {
    moonbit_incref(_M0L9old__headS748);
  }
  moonbit_decref(_M0L6_2aoldS3040);
  _M0L4selfS749->$0 = _M0L6_2atmpS2120;
  _M0L4selfS749->$2 = _M0L13new__capacityS750;
  _M0L6_2atmpS2122 = _M0L13new__capacityS750 - 1;
  _M0L4selfS749->$3 = _M0L6_2atmpS2122;
  _M0L8capacityS2124 = _M0L4selfS749->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2123 = _M0FPB21calc__grow__threshold(_M0L8capacityS2124);
  _M0L4selfS749->$4 = _M0L6_2atmpS2123;
  _M0L4selfS749->$1 = 0;
  _M0L6_2atmpS2125 = 0;
  _M0L6_2aoldS3039 = _M0L4selfS749->$5;
  if (_M0L6_2aoldS3039) {
    moonbit_decref(_M0L6_2aoldS3039);
  }
  _M0L4selfS749->$5 = _M0L6_2atmpS2125;
  _M0L4selfS749->$6 = -1;
  _M0L8_2aparamS751 = _M0L9old__headS748;
  while (1) {
    if (_M0L8_2aparamS751 == 0) {
      if (_M0L8_2aparamS751) {
        moonbit_decref(_M0L8_2aparamS751);
      }
      moonbit_decref(_M0L4selfS749);
    } else {
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2aSomeS752 =
        _M0L8_2aparamS751;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L4_2axS753 =
        _M0L7_2aSomeS752;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L8_2afieldS3038 =
        _M0L4_2axS753->$1;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2anextS754 =
        _M0L8_2afieldS3038;
      moonbit_string_t _M0L8_2afieldS3037 = _M0L4_2axS753->$4;
      moonbit_string_t _M0L6_2akeyS755 = _M0L8_2afieldS3037;
      struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L8_2afieldS3036 =
        _M0L4_2axS753->$5;
      struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L8_2avalueS756 =
        _M0L8_2afieldS3036;
      int32_t _M0L8_2afieldS3035 = _M0L4_2axS753->$3;
      int32_t _M0L6_2acntS3343 = Moonbit_object_header(_M0L4_2axS753)->rc;
      int32_t _M0L7_2ahashS757;
      if (_M0L6_2acntS3343 > 1) {
        int32_t _M0L11_2anew__cntS3344 = _M0L6_2acntS3343 - 1;
        Moonbit_object_header(_M0L4_2axS753)->rc = _M0L11_2anew__cntS3344;
        moonbit_incref(_M0L8_2avalueS756);
        moonbit_incref(_M0L6_2akeyS755);
        if (_M0L7_2anextS754) {
          moonbit_incref(_M0L7_2anextS754);
        }
      } else if (_M0L6_2acntS3343 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS753);
      }
      _M0L7_2ahashS757 = _M0L8_2afieldS3035;
      moonbit_incref(_M0L4selfS749);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L4selfS749, _M0L6_2akeyS755, _M0L8_2avalueS756, _M0L7_2ahashS757);
      _M0L8_2aparamS751 = _M0L7_2anextS754;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map4growGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L4selfS760
) {
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L8_2afieldS3048;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L9old__headS759;
  int32_t _M0L8capacityS2133;
  int32_t _M0L13new__capacityS761;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS2128;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L6_2atmpS2127;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L6_2aoldS3047;
  int32_t _M0L6_2atmpS2129;
  int32_t _M0L8capacityS2131;
  int32_t _M0L6_2atmpS2130;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS2132;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2aoldS3046;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L8_2aparamS762;
  #line 483 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3048 = _M0L4selfS760->$5;
  _M0L9old__headS759 = _M0L8_2afieldS3048;
  _M0L8capacityS2133 = _M0L4selfS760->$2;
  _M0L13new__capacityS761 = _M0L8capacityS2133 << 1;
  _M0L6_2atmpS2128 = 0;
  _M0L6_2atmpS2127
  = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE**)moonbit_make_ref_array(_M0L13new__capacityS761, _M0L6_2atmpS2128);
  _M0L6_2aoldS3047 = _M0L4selfS760->$0;
  if (_M0L9old__headS759) {
    moonbit_incref(_M0L9old__headS759);
  }
  moonbit_decref(_M0L6_2aoldS3047);
  _M0L4selfS760->$0 = _M0L6_2atmpS2127;
  _M0L4selfS760->$2 = _M0L13new__capacityS761;
  _M0L6_2atmpS2129 = _M0L13new__capacityS761 - 1;
  _M0L4selfS760->$3 = _M0L6_2atmpS2129;
  _M0L8capacityS2131 = _M0L4selfS760->$2;
  #line 489 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS2130 = _M0FPB21calc__grow__threshold(_M0L8capacityS2131);
  _M0L4selfS760->$4 = _M0L6_2atmpS2130;
  _M0L4selfS760->$1 = 0;
  _M0L6_2atmpS2132 = 0;
  _M0L6_2aoldS3046 = _M0L4selfS760->$5;
  if (_M0L6_2aoldS3046) {
    moonbit_decref(_M0L6_2aoldS3046);
  }
  _M0L4selfS760->$5 = _M0L6_2atmpS2132;
  _M0L4selfS760->$6 = -1;
  _M0L8_2aparamS762 = _M0L9old__headS759;
  while (1) {
    if (_M0L8_2aparamS762 == 0) {
      if (_M0L8_2aparamS762) {
        moonbit_decref(_M0L8_2aparamS762);
      }
      moonbit_decref(_M0L4selfS760);
    } else {
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2aSomeS763 =
        _M0L8_2aparamS762;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L4_2axS764 =
        _M0L7_2aSomeS763;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L8_2afieldS3045 =
        _M0L4_2axS764->$1;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2anextS765 =
        _M0L8_2afieldS3045;
      moonbit_string_t _M0L8_2afieldS3044 = _M0L4_2axS764->$4;
      moonbit_string_t _M0L6_2akeyS766 = _M0L8_2afieldS3044;
      struct _M0TP38clawteam8clawteam6config13ChannelConfig* _M0L8_2afieldS3043 =
        _M0L4_2axS764->$5;
      struct _M0TP38clawteam8clawteam6config13ChannelConfig* _M0L8_2avalueS767 =
        _M0L8_2afieldS3043;
      int32_t _M0L8_2afieldS3042 = _M0L4_2axS764->$3;
      int32_t _M0L6_2acntS3345 = Moonbit_object_header(_M0L4_2axS764)->rc;
      int32_t _M0L7_2ahashS768;
      if (_M0L6_2acntS3345 > 1) {
        int32_t _M0L11_2anew__cntS3346 = _M0L6_2acntS3345 - 1;
        Moonbit_object_header(_M0L4_2axS764)->rc = _M0L11_2anew__cntS3346;
        moonbit_incref(_M0L8_2avalueS767);
        moonbit_incref(_M0L6_2akeyS766);
        if (_M0L7_2anextS765) {
          moonbit_incref(_M0L7_2anextS765);
        }
      } else if (_M0L6_2acntS3345 == 1) {
        #line 493 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_free(_M0L4_2axS764);
      }
      _M0L7_2ahashS768 = _M0L8_2afieldS3042;
      moonbit_incref(_M0L4selfS760);
      #line 495 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map15set__with__hashGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L4selfS760, _M0L6_2akeyS766, _M0L8_2avalueS767, _M0L7_2ahashS768);
      _M0L8_2aparamS762 = _M0L7_2anextS765;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS638,
  moonbit_string_t _M0L3keyS644,
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L5valueS645,
  int32_t _M0L4hashS640
) {
  int32_t _M0L14capacity__maskS2026;
  int32_t _M0L6_2atmpS2025;
  int32_t _M0L3pslS635;
  int32_t _M0L3idxS636;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2026 = _M0L4selfS638->$3;
  _M0L6_2atmpS2025 = _M0L4hashS640 & _M0L14capacity__maskS2026;
  _M0L3pslS635 = 0;
  _M0L3idxS636 = _M0L6_2atmpS2025;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3053 =
      _M0L4selfS638->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS2024 =
      _M0L8_2afieldS3053;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3052;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS637;
    if (
      _M0L3idxS636 < 0
      || _M0L3idxS636 >= Moonbit_array_length(_M0L7entriesS2024)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3052
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS2024[
        _M0L3idxS636
      ];
    _M0L7_2abindS637 = _M0L6_2atmpS3052;
    if (_M0L7_2abindS637 == 0) {
      int32_t _M0L4sizeS2009 = _M0L4selfS638->$1;
      int32_t _M0L8grow__atS2010 = _M0L4selfS638->$4;
      int32_t _M0L7_2abindS641;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS642;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS643;
      if (_M0L4sizeS2009 >= _M0L8grow__atS2010) {
        int32_t _M0L14capacity__maskS2012;
        int32_t _M0L6_2atmpS2011;
        moonbit_incref(_M0L4selfS638);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS638);
        _M0L14capacity__maskS2012 = _M0L4selfS638->$3;
        _M0L6_2atmpS2011 = _M0L4hashS640 & _M0L14capacity__maskS2012;
        _M0L3pslS635 = 0;
        _M0L3idxS636 = _M0L6_2atmpS2011;
        continue;
      }
      _M0L7_2abindS641 = _M0L4selfS638->$6;
      _M0L7_2abindS642 = 0;
      _M0L5entryS643
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
      Moonbit_object_header(_M0L5entryS643)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
      _M0L5entryS643->$0 = _M0L7_2abindS641;
      _M0L5entryS643->$1 = _M0L7_2abindS642;
      _M0L5entryS643->$2 = _M0L3pslS635;
      _M0L5entryS643->$3 = _M0L4hashS640;
      _M0L5entryS643->$4 = _M0L3keyS644;
      _M0L5entryS643->$5 = _M0L5valueS645;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS638, _M0L3idxS636, _M0L5entryS643);
      return 0;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS646 =
        _M0L7_2abindS637;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS647 =
        _M0L7_2aSomeS646;
      int32_t _M0L4hashS2014 = _M0L14_2acurr__entryS647->$3;
      int32_t _if__result_3504;
      int32_t _M0L3pslS2015;
      int32_t _M0L6_2atmpS2020;
      int32_t _M0L6_2atmpS2022;
      int32_t _M0L14capacity__maskS2023;
      int32_t _M0L6_2atmpS2021;
      if (_M0L4hashS2014 == _M0L4hashS640) {
        moonbit_string_t _M0L8_2afieldS3051 = _M0L14_2acurr__entryS647->$4;
        moonbit_string_t _M0L3keyS2013 = _M0L8_2afieldS3051;
        int32_t _M0L6_2atmpS3050;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3050
        = moonbit_val_array_equal(_M0L3keyS2013, _M0L3keyS644);
        _if__result_3504 = _M0L6_2atmpS3050;
      } else {
        _if__result_3504 = 0;
      }
      if (_if__result_3504) {
        struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3049;
        moonbit_incref(_M0L14_2acurr__entryS647);
        moonbit_decref(_M0L3keyS644);
        moonbit_decref(_M0L4selfS638);
        _M0L6_2aoldS3049 = _M0L14_2acurr__entryS647->$5;
        moonbit_decref(_M0L6_2aoldS3049);
        _M0L14_2acurr__entryS647->$5 = _M0L5valueS645;
        moonbit_decref(_M0L14_2acurr__entryS647);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS647);
      }
      _M0L3pslS2015 = _M0L14_2acurr__entryS647->$2;
      if (_M0L3pslS635 > _M0L3pslS2015) {
        int32_t _M0L4sizeS2016 = _M0L4selfS638->$1;
        int32_t _M0L8grow__atS2017 = _M0L4selfS638->$4;
        int32_t _M0L7_2abindS648;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS649;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS650;
        if (_M0L4sizeS2016 >= _M0L8grow__atS2017) {
          int32_t _M0L14capacity__maskS2019;
          int32_t _M0L6_2atmpS2018;
          moonbit_decref(_M0L14_2acurr__entryS647);
          moonbit_incref(_M0L4selfS638);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS638);
          _M0L14capacity__maskS2019 = _M0L4selfS638->$3;
          _M0L6_2atmpS2018 = _M0L4hashS640 & _M0L14capacity__maskS2019;
          _M0L3pslS635 = 0;
          _M0L3idxS636 = _M0L6_2atmpS2018;
          continue;
        }
        moonbit_incref(_M0L4selfS638);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS638, _M0L3idxS636, _M0L14_2acurr__entryS647);
        _M0L7_2abindS648 = _M0L4selfS638->$6;
        _M0L7_2abindS649 = 0;
        _M0L5entryS650
        = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
        Moonbit_object_header(_M0L5entryS650)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $1) >> 2, 3, 0);
        _M0L5entryS650->$0 = _M0L7_2abindS648;
        _M0L5entryS650->$1 = _M0L7_2abindS649;
        _M0L5entryS650->$2 = _M0L3pslS635;
        _M0L5entryS650->$3 = _M0L4hashS640;
        _M0L5entryS650->$4 = _M0L3keyS644;
        _M0L5entryS650->$5 = _M0L5valueS645;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS638, _M0L3idxS636, _M0L5entryS650);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS647);
      }
      _M0L6_2atmpS2020 = _M0L3pslS635 + 1;
      _M0L6_2atmpS2022 = _M0L3idxS636 + 1;
      _M0L14capacity__maskS2023 = _M0L4selfS638->$3;
      _M0L6_2atmpS2021 = _M0L6_2atmpS2022 & _M0L14capacity__maskS2023;
      _M0L3pslS635 = _M0L6_2atmpS2020;
      _M0L3idxS636 = _M0L6_2atmpS2021;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS654,
  int32_t _M0L3keyS660,
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L5valueS661,
  int32_t _M0L4hashS656
) {
  int32_t _M0L14capacity__maskS2044;
  int32_t _M0L6_2atmpS2043;
  int32_t _M0L3pslS651;
  int32_t _M0L3idxS652;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2044 = _M0L4selfS654->$3;
  _M0L6_2atmpS2043 = _M0L4hashS656 & _M0L14capacity__maskS2044;
  _M0L3pslS651 = 0;
  _M0L3idxS652 = _M0L6_2atmpS2043;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3056 =
      _M0L4selfS654->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS2042 =
      _M0L8_2afieldS3056;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3055;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS653;
    if (
      _M0L3idxS652 < 0
      || _M0L3idxS652 >= Moonbit_array_length(_M0L7entriesS2042)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3055
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS2042[
        _M0L3idxS652
      ];
    _M0L7_2abindS653 = _M0L6_2atmpS3055;
    if (_M0L7_2abindS653 == 0) {
      int32_t _M0L4sizeS2027 = _M0L4selfS654->$1;
      int32_t _M0L8grow__atS2028 = _M0L4selfS654->$4;
      int32_t _M0L7_2abindS657;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS658;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS659;
      if (_M0L4sizeS2027 >= _M0L8grow__atS2028) {
        int32_t _M0L14capacity__maskS2030;
        int32_t _M0L6_2atmpS2029;
        moonbit_incref(_M0L4selfS654);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS654);
        _M0L14capacity__maskS2030 = _M0L4selfS654->$3;
        _M0L6_2atmpS2029 = _M0L4hashS656 & _M0L14capacity__maskS2030;
        _M0L3pslS651 = 0;
        _M0L3idxS652 = _M0L6_2atmpS2029;
        continue;
      }
      _M0L7_2abindS657 = _M0L4selfS654->$6;
      _M0L7_2abindS658 = 0;
      _M0L5entryS659
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
      Moonbit_object_header(_M0L5entryS659)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
      _M0L5entryS659->$0 = _M0L7_2abindS657;
      _M0L5entryS659->$1 = _M0L7_2abindS658;
      _M0L5entryS659->$2 = _M0L3pslS651;
      _M0L5entryS659->$3 = _M0L4hashS656;
      _M0L5entryS659->$4 = _M0L3keyS660;
      _M0L5entryS659->$5 = _M0L5valueS661;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS654, _M0L3idxS652, _M0L5entryS659);
      return 0;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS662 =
        _M0L7_2abindS653;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS663 =
        _M0L7_2aSomeS662;
      int32_t _M0L4hashS2032 = _M0L14_2acurr__entryS663->$3;
      int32_t _if__result_3506;
      int32_t _M0L3pslS2033;
      int32_t _M0L6_2atmpS2038;
      int32_t _M0L6_2atmpS2040;
      int32_t _M0L14capacity__maskS2041;
      int32_t _M0L6_2atmpS2039;
      if (_M0L4hashS2032 == _M0L4hashS656) {
        int32_t _M0L3keyS2031 = _M0L14_2acurr__entryS663->$4;
        _if__result_3506 = _M0L3keyS2031 == _M0L3keyS660;
      } else {
        _if__result_3506 = 0;
      }
      if (_if__result_3506) {
        struct _M0TUWEuQRPC15error5ErrorNsE* _M0L6_2aoldS3054;
        moonbit_incref(_M0L14_2acurr__entryS663);
        moonbit_decref(_M0L4selfS654);
        _M0L6_2aoldS3054 = _M0L14_2acurr__entryS663->$5;
        moonbit_decref(_M0L6_2aoldS3054);
        _M0L14_2acurr__entryS663->$5 = _M0L5valueS661;
        moonbit_decref(_M0L14_2acurr__entryS663);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS663);
      }
      _M0L3pslS2033 = _M0L14_2acurr__entryS663->$2;
      if (_M0L3pslS651 > _M0L3pslS2033) {
        int32_t _M0L4sizeS2034 = _M0L4selfS654->$1;
        int32_t _M0L8grow__atS2035 = _M0L4selfS654->$4;
        int32_t _M0L7_2abindS664;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS665;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS666;
        if (_M0L4sizeS2034 >= _M0L8grow__atS2035) {
          int32_t _M0L14capacity__maskS2037;
          int32_t _M0L6_2atmpS2036;
          moonbit_decref(_M0L14_2acurr__entryS663);
          moonbit_incref(_M0L4selfS654);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS654);
          _M0L14capacity__maskS2037 = _M0L4selfS654->$3;
          _M0L6_2atmpS2036 = _M0L4hashS656 & _M0L14capacity__maskS2037;
          _M0L3pslS651 = 0;
          _M0L3idxS652 = _M0L6_2atmpS2036;
          continue;
        }
        moonbit_incref(_M0L4selfS654);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS654, _M0L3idxS652, _M0L14_2acurr__entryS663);
        _M0L7_2abindS664 = _M0L4selfS654->$6;
        _M0L7_2abindS665 = 0;
        _M0L5entryS666
        = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE));
        Moonbit_object_header(_M0L5entryS666)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 2, 0);
        _M0L5entryS666->$0 = _M0L7_2abindS664;
        _M0L5entryS666->$1 = _M0L7_2abindS665;
        _M0L5entryS666->$2 = _M0L3pslS651;
        _M0L5entryS666->$3 = _M0L4hashS656;
        _M0L5entryS666->$4 = _M0L3keyS660;
        _M0L5entryS666->$5 = _M0L5valueS661;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS654, _M0L3idxS652, _M0L5entryS666);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS663);
      }
      _M0L6_2atmpS2038 = _M0L3pslS651 + 1;
      _M0L6_2atmpS2040 = _M0L3idxS652 + 1;
      _M0L14capacity__maskS2041 = _M0L4selfS654->$3;
      _M0L6_2atmpS2039 = _M0L6_2atmpS2040 & _M0L14capacity__maskS2041;
      _M0L3pslS651 = _M0L6_2atmpS2038;
      _M0L3idxS652 = _M0L6_2atmpS2039;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGssE(
  struct _M0TPB3MapGssE* _M0L4selfS670,
  moonbit_string_t _M0L3keyS676,
  moonbit_string_t _M0L5valueS677,
  int32_t _M0L4hashS672
) {
  int32_t _M0L14capacity__maskS2062;
  int32_t _M0L6_2atmpS2061;
  int32_t _M0L3pslS667;
  int32_t _M0L3idxS668;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2062 = _M0L4selfS670->$3;
  _M0L6_2atmpS2061 = _M0L4hashS672 & _M0L14capacity__maskS2062;
  _M0L3pslS667 = 0;
  _M0L3idxS668 = _M0L6_2atmpS2061;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L8_2afieldS3061 = _M0L4selfS670->$0;
    struct _M0TPB5EntryGssE** _M0L7entriesS2060 = _M0L8_2afieldS3061;
    struct _M0TPB5EntryGssE* _M0L6_2atmpS3060;
    struct _M0TPB5EntryGssE* _M0L7_2abindS669;
    if (
      _M0L3idxS668 < 0
      || _M0L3idxS668 >= Moonbit_array_length(_M0L7entriesS2060)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3060
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS2060[_M0L3idxS668];
    _M0L7_2abindS669 = _M0L6_2atmpS3060;
    if (_M0L7_2abindS669 == 0) {
      int32_t _M0L4sizeS2045 = _M0L4selfS670->$1;
      int32_t _M0L8grow__atS2046 = _M0L4selfS670->$4;
      int32_t _M0L7_2abindS673;
      struct _M0TPB5EntryGssE* _M0L7_2abindS674;
      struct _M0TPB5EntryGssE* _M0L5entryS675;
      if (_M0L4sizeS2045 >= _M0L8grow__atS2046) {
        int32_t _M0L14capacity__maskS2048;
        int32_t _M0L6_2atmpS2047;
        moonbit_incref(_M0L4selfS670);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGssE(_M0L4selfS670);
        _M0L14capacity__maskS2048 = _M0L4selfS670->$3;
        _M0L6_2atmpS2047 = _M0L4hashS672 & _M0L14capacity__maskS2048;
        _M0L3pslS667 = 0;
        _M0L3idxS668 = _M0L6_2atmpS2047;
        continue;
      }
      _M0L7_2abindS673 = _M0L4selfS670->$6;
      _M0L7_2abindS674 = 0;
      _M0L5entryS675
      = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
      Moonbit_object_header(_M0L5entryS675)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGssE, $1) >> 2, 3, 0);
      _M0L5entryS675->$0 = _M0L7_2abindS673;
      _M0L5entryS675->$1 = _M0L7_2abindS674;
      _M0L5entryS675->$2 = _M0L3pslS667;
      _M0L5entryS675->$3 = _M0L4hashS672;
      _M0L5entryS675->$4 = _M0L3keyS676;
      _M0L5entryS675->$5 = _M0L5valueS677;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS670, _M0L3idxS668, _M0L5entryS675);
      return 0;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS678 = _M0L7_2abindS669;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS679 = _M0L7_2aSomeS678;
      int32_t _M0L4hashS2050 = _M0L14_2acurr__entryS679->$3;
      int32_t _if__result_3508;
      int32_t _M0L3pslS2051;
      int32_t _M0L6_2atmpS2056;
      int32_t _M0L6_2atmpS2058;
      int32_t _M0L14capacity__maskS2059;
      int32_t _M0L6_2atmpS2057;
      if (_M0L4hashS2050 == _M0L4hashS672) {
        moonbit_string_t _M0L8_2afieldS3059 = _M0L14_2acurr__entryS679->$4;
        moonbit_string_t _M0L3keyS2049 = _M0L8_2afieldS3059;
        int32_t _M0L6_2atmpS3058;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3058
        = moonbit_val_array_equal(_M0L3keyS2049, _M0L3keyS676);
        _if__result_3508 = _M0L6_2atmpS3058;
      } else {
        _if__result_3508 = 0;
      }
      if (_if__result_3508) {
        moonbit_string_t _M0L6_2aoldS3057;
        moonbit_incref(_M0L14_2acurr__entryS679);
        moonbit_decref(_M0L3keyS676);
        moonbit_decref(_M0L4selfS670);
        _M0L6_2aoldS3057 = _M0L14_2acurr__entryS679->$5;
        moonbit_decref(_M0L6_2aoldS3057);
        _M0L14_2acurr__entryS679->$5 = _M0L5valueS677;
        moonbit_decref(_M0L14_2acurr__entryS679);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS679);
      }
      _M0L3pslS2051 = _M0L14_2acurr__entryS679->$2;
      if (_M0L3pslS667 > _M0L3pslS2051) {
        int32_t _M0L4sizeS2052 = _M0L4selfS670->$1;
        int32_t _M0L8grow__atS2053 = _M0L4selfS670->$4;
        int32_t _M0L7_2abindS680;
        struct _M0TPB5EntryGssE* _M0L7_2abindS681;
        struct _M0TPB5EntryGssE* _M0L5entryS682;
        if (_M0L4sizeS2052 >= _M0L8grow__atS2053) {
          int32_t _M0L14capacity__maskS2055;
          int32_t _M0L6_2atmpS2054;
          moonbit_decref(_M0L14_2acurr__entryS679);
          moonbit_incref(_M0L4selfS670);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGssE(_M0L4selfS670);
          _M0L14capacity__maskS2055 = _M0L4selfS670->$3;
          _M0L6_2atmpS2054 = _M0L4hashS672 & _M0L14capacity__maskS2055;
          _M0L3pslS667 = 0;
          _M0L3idxS668 = _M0L6_2atmpS2054;
          continue;
        }
        moonbit_incref(_M0L4selfS670);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGssE(_M0L4selfS670, _M0L3idxS668, _M0L14_2acurr__entryS679);
        _M0L7_2abindS680 = _M0L4selfS670->$6;
        _M0L7_2abindS681 = 0;
        _M0L5entryS682
        = (struct _M0TPB5EntryGssE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGssE));
        Moonbit_object_header(_M0L5entryS682)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGssE, $1) >> 2, 3, 0);
        _M0L5entryS682->$0 = _M0L7_2abindS680;
        _M0L5entryS682->$1 = _M0L7_2abindS681;
        _M0L5entryS682->$2 = _M0L3pslS667;
        _M0L5entryS682->$3 = _M0L4hashS672;
        _M0L5entryS682->$4 = _M0L3keyS676;
        _M0L5entryS682->$5 = _M0L5valueS677;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGssE(_M0L4selfS670, _M0L3idxS668, _M0L5entryS682);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS679);
      }
      _M0L6_2atmpS2056 = _M0L3pslS667 + 1;
      _M0L6_2atmpS2058 = _M0L3idxS668 + 1;
      _M0L14capacity__maskS2059 = _M0L4selfS670->$3;
      _M0L6_2atmpS2057 = _M0L6_2atmpS2058 & _M0L14capacity__maskS2059;
      _M0L3pslS667 = _M0L6_2atmpS2056;
      _M0L3idxS668 = _M0L6_2atmpS2057;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L4selfS686,
  moonbit_string_t _M0L3keyS692,
  struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L5valueS693,
  int32_t _M0L4hashS688
) {
  int32_t _M0L14capacity__maskS2080;
  int32_t _M0L6_2atmpS2079;
  int32_t _M0L3pslS683;
  int32_t _M0L3idxS684;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2080 = _M0L4selfS686->$3;
  _M0L6_2atmpS2079 = _M0L4hashS688 & _M0L14capacity__maskS2080;
  _M0L3pslS683 = 0;
  _M0L3idxS684 = _M0L6_2atmpS2079;
  while (1) {
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L8_2afieldS3066 =
      _M0L4selfS686->$0;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L7entriesS2078 =
      _M0L8_2afieldS3066;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS3065;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2abindS685;
    if (
      _M0L3idxS684 < 0
      || _M0L3idxS684 >= Moonbit_array_length(_M0L7entriesS2078)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3065
    = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*)_M0L7entriesS2078[
        _M0L3idxS684
      ];
    _M0L7_2abindS685 = _M0L6_2atmpS3065;
    if (_M0L7_2abindS685 == 0) {
      int32_t _M0L4sizeS2063 = _M0L4selfS686->$1;
      int32_t _M0L8grow__atS2064 = _M0L4selfS686->$4;
      int32_t _M0L7_2abindS689;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2abindS690;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L5entryS691;
      if (_M0L4sizeS2063 >= _M0L8grow__atS2064) {
        int32_t _M0L14capacity__maskS2066;
        int32_t _M0L6_2atmpS2065;
        moonbit_incref(_M0L4selfS686);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L4selfS686);
        _M0L14capacity__maskS2066 = _M0L4selfS686->$3;
        _M0L6_2atmpS2065 = _M0L4hashS688 & _M0L14capacity__maskS2066;
        _M0L3pslS683 = 0;
        _M0L3idxS684 = _M0L6_2atmpS2065;
        continue;
      }
      _M0L7_2abindS689 = _M0L4selfS686->$6;
      _M0L7_2abindS690 = 0;
      _M0L5entryS691
      = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE));
      Moonbit_object_header(_M0L5entryS691)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE, $1) >> 2, 3, 0);
      _M0L5entryS691->$0 = _M0L7_2abindS689;
      _M0L5entryS691->$1 = _M0L7_2abindS690;
      _M0L5entryS691->$2 = _M0L3pslS683;
      _M0L5entryS691->$3 = _M0L4hashS688;
      _M0L5entryS691->$4 = _M0L3keyS692;
      _M0L5entryS691->$5 = _M0L5valueS693;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L4selfS686, _M0L3idxS684, _M0L5entryS691);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2aSomeS694 =
        _M0L7_2abindS685;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L14_2acurr__entryS695 =
        _M0L7_2aSomeS694;
      int32_t _M0L4hashS2068 = _M0L14_2acurr__entryS695->$3;
      int32_t _if__result_3510;
      int32_t _M0L3pslS2069;
      int32_t _M0L6_2atmpS2074;
      int32_t _M0L6_2atmpS2076;
      int32_t _M0L14capacity__maskS2077;
      int32_t _M0L6_2atmpS2075;
      if (_M0L4hashS2068 == _M0L4hashS688) {
        moonbit_string_t _M0L8_2afieldS3064 = _M0L14_2acurr__entryS695->$4;
        moonbit_string_t _M0L3keyS2067 = _M0L8_2afieldS3064;
        int32_t _M0L6_2atmpS3063;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3063
        = moonbit_val_array_equal(_M0L3keyS2067, _M0L3keyS692);
        _if__result_3510 = _M0L6_2atmpS3063;
      } else {
        _if__result_3510 = 0;
      }
      if (_if__result_3510) {
        struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L6_2aoldS3062;
        moonbit_incref(_M0L14_2acurr__entryS695);
        moonbit_decref(_M0L3keyS692);
        moonbit_decref(_M0L4selfS686);
        _M0L6_2aoldS3062 = _M0L14_2acurr__entryS695->$5;
        moonbit_decref(_M0L6_2aoldS3062);
        _M0L14_2acurr__entryS695->$5 = _M0L5valueS693;
        moonbit_decref(_M0L14_2acurr__entryS695);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS695);
      }
      _M0L3pslS2069 = _M0L14_2acurr__entryS695->$2;
      if (_M0L3pslS683 > _M0L3pslS2069) {
        int32_t _M0L4sizeS2070 = _M0L4selfS686->$1;
        int32_t _M0L8grow__atS2071 = _M0L4selfS686->$4;
        int32_t _M0L7_2abindS696;
        struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2abindS697;
        struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L5entryS698;
        if (_M0L4sizeS2070 >= _M0L8grow__atS2071) {
          int32_t _M0L14capacity__maskS2073;
          int32_t _M0L6_2atmpS2072;
          moonbit_decref(_M0L14_2acurr__entryS695);
          moonbit_incref(_M0L4selfS686);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L4selfS686);
          _M0L14capacity__maskS2073 = _M0L4selfS686->$3;
          _M0L6_2atmpS2072 = _M0L4hashS688 & _M0L14capacity__maskS2073;
          _M0L3pslS683 = 0;
          _M0L3idxS684 = _M0L6_2atmpS2072;
          continue;
        }
        moonbit_incref(_M0L4selfS686);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L4selfS686, _M0L3idxS684, _M0L14_2acurr__entryS695);
        _M0L7_2abindS696 = _M0L4selfS686->$6;
        _M0L7_2abindS697 = 0;
        _M0L5entryS698
        = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE));
        Moonbit_object_header(_M0L5entryS698)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE, $1) >> 2, 3, 0);
        _M0L5entryS698->$0 = _M0L7_2abindS696;
        _M0L5entryS698->$1 = _M0L7_2abindS697;
        _M0L5entryS698->$2 = _M0L3pslS683;
        _M0L5entryS698->$3 = _M0L4hashS688;
        _M0L5entryS698->$4 = _M0L3keyS692;
        _M0L5entryS698->$5 = _M0L5valueS693;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L4selfS686, _M0L3idxS684, _M0L5entryS698);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS695);
      }
      _M0L6_2atmpS2074 = _M0L3pslS683 + 1;
      _M0L6_2atmpS2076 = _M0L3idxS684 + 1;
      _M0L14capacity__maskS2077 = _M0L4selfS686->$3;
      _M0L6_2atmpS2075 = _M0L6_2atmpS2076 & _M0L14capacity__maskS2077;
      _M0L3pslS683 = _M0L6_2atmpS2074;
      _M0L3idxS684 = _M0L6_2atmpS2075;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map15set__with__hashGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L4selfS702,
  moonbit_string_t _M0L3keyS708,
  struct _M0TP38clawteam8clawteam6config13ChannelConfig* _M0L5valueS709,
  int32_t _M0L4hashS704
) {
  int32_t _M0L14capacity__maskS2098;
  int32_t _M0L6_2atmpS2097;
  int32_t _M0L3pslS699;
  int32_t _M0L3idxS700;
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L14capacity__maskS2098 = _M0L4selfS702->$3;
  _M0L6_2atmpS2097 = _M0L4hashS704 & _M0L14capacity__maskS2098;
  _M0L3pslS699 = 0;
  _M0L3idxS700 = _M0L6_2atmpS2097;
  while (1) {
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L8_2afieldS3071 =
      _M0L4selfS702->$0;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L7entriesS2096 =
      _M0L8_2afieldS3071;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS3070;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2abindS701;
    if (
      _M0L3idxS700 < 0
      || _M0L3idxS700 >= Moonbit_array_length(_M0L7entriesS2096)
    ) {
      #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3070
    = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE*)_M0L7entriesS2096[
        _M0L3idxS700
      ];
    _M0L7_2abindS701 = _M0L6_2atmpS3070;
    if (_M0L7_2abindS701 == 0) {
      int32_t _M0L4sizeS2081 = _M0L4selfS702->$1;
      int32_t _M0L8grow__atS2082 = _M0L4selfS702->$4;
      int32_t _M0L7_2abindS705;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2abindS706;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L5entryS707;
      if (_M0L4sizeS2081 >= _M0L8grow__atS2082) {
        int32_t _M0L14capacity__maskS2084;
        int32_t _M0L6_2atmpS2083;
        moonbit_incref(_M0L4selfS702);
        #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map4growGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L4selfS702);
        _M0L14capacity__maskS2084 = _M0L4selfS702->$3;
        _M0L6_2atmpS2083 = _M0L4hashS704 & _M0L14capacity__maskS2084;
        _M0L3pslS699 = 0;
        _M0L3idxS700 = _M0L6_2atmpS2083;
        continue;
      }
      _M0L7_2abindS705 = _M0L4selfS702->$6;
      _M0L7_2abindS706 = 0;
      _M0L5entryS707
      = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE));
      Moonbit_object_header(_M0L5entryS707)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE, $1) >> 2, 3, 0);
      _M0L5entryS707->$0 = _M0L7_2abindS705;
      _M0L5entryS707->$1 = _M0L7_2abindS706;
      _M0L5entryS707->$2 = _M0L3pslS699;
      _M0L5entryS707->$3 = _M0L4hashS704;
      _M0L5entryS707->$4 = _M0L3keyS708;
      _M0L5entryS707->$5 = _M0L5valueS709;
      #line 130 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map20add__entry__to__tailGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L4selfS702, _M0L3idxS700, _M0L5entryS707);
      return 0;
    } else {
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2aSomeS710 =
        _M0L7_2abindS701;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L14_2acurr__entryS711 =
        _M0L7_2aSomeS710;
      int32_t _M0L4hashS2086 = _M0L14_2acurr__entryS711->$3;
      int32_t _if__result_3512;
      int32_t _M0L3pslS2087;
      int32_t _M0L6_2atmpS2092;
      int32_t _M0L6_2atmpS2094;
      int32_t _M0L14capacity__maskS2095;
      int32_t _M0L6_2atmpS2093;
      if (_M0L4hashS2086 == _M0L4hashS704) {
        moonbit_string_t _M0L8_2afieldS3069 = _M0L14_2acurr__entryS711->$4;
        moonbit_string_t _M0L3keyS2085 = _M0L8_2afieldS3069;
        int32_t _M0L6_2atmpS3068;
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0L6_2atmpS3068
        = moonbit_val_array_equal(_M0L3keyS2085, _M0L3keyS708);
        _if__result_3512 = _M0L6_2atmpS3068;
      } else {
        _if__result_3512 = 0;
      }
      if (_if__result_3512) {
        struct _M0TP38clawteam8clawteam6config13ChannelConfig* _M0L6_2aoldS3067;
        moonbit_incref(_M0L14_2acurr__entryS711);
        moonbit_decref(_M0L3keyS708);
        moonbit_decref(_M0L4selfS702);
        _M0L6_2aoldS3067 = _M0L14_2acurr__entryS711->$5;
        moonbit_decref(_M0L6_2aoldS3067);
        _M0L14_2acurr__entryS711->$5 = _M0L5valueS709;
        moonbit_decref(_M0L14_2acurr__entryS711);
        return 0;
      } else {
        moonbit_incref(_M0L14_2acurr__entryS711);
      }
      _M0L3pslS2087 = _M0L14_2acurr__entryS711->$2;
      if (_M0L3pslS699 > _M0L3pslS2087) {
        int32_t _M0L4sizeS2088 = _M0L4selfS702->$1;
        int32_t _M0L8grow__atS2089 = _M0L4selfS702->$4;
        int32_t _M0L7_2abindS712;
        struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2abindS713;
        struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L5entryS714;
        if (_M0L4sizeS2088 >= _M0L8grow__atS2089) {
          int32_t _M0L14capacity__maskS2091;
          int32_t _M0L6_2atmpS2090;
          moonbit_decref(_M0L14_2acurr__entryS711);
          moonbit_incref(_M0L4selfS702);
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
          _M0MPB3Map4growGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L4selfS702);
          _M0L14capacity__maskS2091 = _M0L4selfS702->$3;
          _M0L6_2atmpS2090 = _M0L4hashS704 & _M0L14capacity__maskS2091;
          _M0L3pslS699 = 0;
          _M0L3idxS700 = _M0L6_2atmpS2090;
          continue;
        }
        moonbit_incref(_M0L4selfS702);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10push__awayGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L4selfS702, _M0L3idxS700, _M0L14_2acurr__entryS711);
        _M0L7_2abindS712 = _M0L4selfS702->$6;
        _M0L7_2abindS713 = 0;
        _M0L5entryS714
        = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE*)moonbit_malloc(sizeof(struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE));
        Moonbit_object_header(_M0L5entryS714)->meta
        = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE, $1) >> 2, 3, 0);
        _M0L5entryS714->$0 = _M0L7_2abindS712;
        _M0L5entryS714->$1 = _M0L7_2abindS713;
        _M0L5entryS714->$2 = _M0L3pslS699;
        _M0L5entryS714->$3 = _M0L4hashS704;
        _M0L5entryS714->$4 = _M0L3keyS708;
        _M0L5entryS714->$5 = _M0L5valueS709;
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map20add__entry__to__tailGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L4selfS702, _M0L3idxS700, _M0L5entryS714);
        return 0;
      } else {
        moonbit_decref(_M0L14_2acurr__entryS711);
      }
      _M0L6_2atmpS2092 = _M0L3pslS699 + 1;
      _M0L6_2atmpS2094 = _M0L3idxS700 + 1;
      _M0L14capacity__maskS2095 = _M0L4selfS702->$3;
      _M0L6_2atmpS2093 = _M0L6_2atmpS2094 & _M0L14capacity__maskS2095;
      _M0L3pslS699 = _M0L6_2atmpS2092;
      _M0L3idxS700 = _M0L6_2atmpS2093;
      continue;
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS589,
  int32_t _M0L3idxS594,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS593
) {
  int32_t _M0L3pslS1944;
  int32_t _M0L6_2atmpS1940;
  int32_t _M0L6_2atmpS1942;
  int32_t _M0L14capacity__maskS1943;
  int32_t _M0L6_2atmpS1941;
  int32_t _M0L3pslS585;
  int32_t _M0L3idxS586;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS587;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1944 = _M0L5entryS593->$2;
  _M0L6_2atmpS1940 = _M0L3pslS1944 + 1;
  _M0L6_2atmpS1942 = _M0L3idxS594 + 1;
  _M0L14capacity__maskS1943 = _M0L4selfS589->$3;
  _M0L6_2atmpS1941 = _M0L6_2atmpS1942 & _M0L14capacity__maskS1943;
  _M0L3pslS585 = _M0L6_2atmpS1940;
  _M0L3idxS586 = _M0L6_2atmpS1941;
  _M0L5entryS587 = _M0L5entryS593;
  while (1) {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3073 =
      _M0L4selfS589->$0;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1939 =
      _M0L8_2afieldS3073;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3072;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS588;
    if (
      _M0L3idxS586 < 0
      || _M0L3idxS586 >= Moonbit_array_length(_M0L7entriesS1939)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3072
    = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1939[
        _M0L3idxS586
      ];
    _M0L7_2abindS588 = _M0L6_2atmpS3072;
    if (_M0L7_2abindS588 == 0) {
      _M0L5entryS587->$2 = _M0L3pslS585;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS589, _M0L5entryS587, _M0L3idxS586);
      break;
    } else {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS591 =
        _M0L7_2abindS588;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L14_2acurr__entryS592 =
        _M0L7_2aSomeS591;
      int32_t _M0L3pslS1929 = _M0L14_2acurr__entryS592->$2;
      if (_M0L3pslS585 > _M0L3pslS1929) {
        int32_t _M0L3pslS1934;
        int32_t _M0L6_2atmpS1930;
        int32_t _M0L6_2atmpS1932;
        int32_t _M0L14capacity__maskS1933;
        int32_t _M0L6_2atmpS1931;
        _M0L5entryS587->$2 = _M0L3pslS585;
        moonbit_incref(_M0L14_2acurr__entryS592);
        moonbit_incref(_M0L4selfS589);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L4selfS589, _M0L5entryS587, _M0L3idxS586);
        _M0L3pslS1934 = _M0L14_2acurr__entryS592->$2;
        _M0L6_2atmpS1930 = _M0L3pslS1934 + 1;
        _M0L6_2atmpS1932 = _M0L3idxS586 + 1;
        _M0L14capacity__maskS1933 = _M0L4selfS589->$3;
        _M0L6_2atmpS1931 = _M0L6_2atmpS1932 & _M0L14capacity__maskS1933;
        _M0L3pslS585 = _M0L6_2atmpS1930;
        _M0L3idxS586 = _M0L6_2atmpS1931;
        _M0L5entryS587 = _M0L14_2acurr__entryS592;
        continue;
      } else {
        int32_t _M0L6_2atmpS1935 = _M0L3pslS585 + 1;
        int32_t _M0L6_2atmpS1937 = _M0L3idxS586 + 1;
        int32_t _M0L14capacity__maskS1938 = _M0L4selfS589->$3;
        int32_t _M0L6_2atmpS1936 =
          _M0L6_2atmpS1937 & _M0L14capacity__maskS1938;
        struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _tmp_3514 =
          _M0L5entryS587;
        _M0L3pslS585 = _M0L6_2atmpS1935;
        _M0L3idxS586 = _M0L6_2atmpS1936;
        _M0L5entryS587 = _tmp_3514;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS599,
  int32_t _M0L3idxS604,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS603
) {
  int32_t _M0L3pslS1960;
  int32_t _M0L6_2atmpS1956;
  int32_t _M0L6_2atmpS1958;
  int32_t _M0L14capacity__maskS1959;
  int32_t _M0L6_2atmpS1957;
  int32_t _M0L3pslS595;
  int32_t _M0L3idxS596;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS597;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1960 = _M0L5entryS603->$2;
  _M0L6_2atmpS1956 = _M0L3pslS1960 + 1;
  _M0L6_2atmpS1958 = _M0L3idxS604 + 1;
  _M0L14capacity__maskS1959 = _M0L4selfS599->$3;
  _M0L6_2atmpS1957 = _M0L6_2atmpS1958 & _M0L14capacity__maskS1959;
  _M0L3pslS595 = _M0L6_2atmpS1956;
  _M0L3idxS596 = _M0L6_2atmpS1957;
  _M0L5entryS597 = _M0L5entryS603;
  while (1) {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3075 =
      _M0L4selfS599->$0;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1955 =
      _M0L8_2afieldS3075;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3074;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS598;
    if (
      _M0L3idxS596 < 0
      || _M0L3idxS596 >= Moonbit_array_length(_M0L7entriesS1955)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3074
    = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1955[
        _M0L3idxS596
      ];
    _M0L7_2abindS598 = _M0L6_2atmpS3074;
    if (_M0L7_2abindS598 == 0) {
      _M0L5entryS597->$2 = _M0L3pslS595;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS599, _M0L5entryS597, _M0L3idxS596);
      break;
    } else {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS601 =
        _M0L7_2abindS598;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L14_2acurr__entryS602 =
        _M0L7_2aSomeS601;
      int32_t _M0L3pslS1945 = _M0L14_2acurr__entryS602->$2;
      if (_M0L3pslS595 > _M0L3pslS1945) {
        int32_t _M0L3pslS1950;
        int32_t _M0L6_2atmpS1946;
        int32_t _M0L6_2atmpS1948;
        int32_t _M0L14capacity__maskS1949;
        int32_t _M0L6_2atmpS1947;
        _M0L5entryS597->$2 = _M0L3pslS595;
        moonbit_incref(_M0L14_2acurr__entryS602);
        moonbit_incref(_M0L4selfS599);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(_M0L4selfS599, _M0L5entryS597, _M0L3idxS596);
        _M0L3pslS1950 = _M0L14_2acurr__entryS602->$2;
        _M0L6_2atmpS1946 = _M0L3pslS1950 + 1;
        _M0L6_2atmpS1948 = _M0L3idxS596 + 1;
        _M0L14capacity__maskS1949 = _M0L4selfS599->$3;
        _M0L6_2atmpS1947 = _M0L6_2atmpS1948 & _M0L14capacity__maskS1949;
        _M0L3pslS595 = _M0L6_2atmpS1946;
        _M0L3idxS596 = _M0L6_2atmpS1947;
        _M0L5entryS597 = _M0L14_2acurr__entryS602;
        continue;
      } else {
        int32_t _M0L6_2atmpS1951 = _M0L3pslS595 + 1;
        int32_t _M0L6_2atmpS1953 = _M0L3idxS596 + 1;
        int32_t _M0L14capacity__maskS1954 = _M0L4selfS599->$3;
        int32_t _M0L6_2atmpS1952 =
          _M0L6_2atmpS1953 & _M0L14capacity__maskS1954;
        struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _tmp_3516 =
          _M0L5entryS597;
        _M0L3pslS595 = _M0L6_2atmpS1951;
        _M0L3idxS596 = _M0L6_2atmpS1952;
        _M0L5entryS597 = _tmp_3516;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGssE(
  struct _M0TPB3MapGssE* _M0L4selfS609,
  int32_t _M0L3idxS614,
  struct _M0TPB5EntryGssE* _M0L5entryS613
) {
  int32_t _M0L3pslS1976;
  int32_t _M0L6_2atmpS1972;
  int32_t _M0L6_2atmpS1974;
  int32_t _M0L14capacity__maskS1975;
  int32_t _M0L6_2atmpS1973;
  int32_t _M0L3pslS605;
  int32_t _M0L3idxS606;
  struct _M0TPB5EntryGssE* _M0L5entryS607;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1976 = _M0L5entryS613->$2;
  _M0L6_2atmpS1972 = _M0L3pslS1976 + 1;
  _M0L6_2atmpS1974 = _M0L3idxS614 + 1;
  _M0L14capacity__maskS1975 = _M0L4selfS609->$3;
  _M0L6_2atmpS1973 = _M0L6_2atmpS1974 & _M0L14capacity__maskS1975;
  _M0L3pslS605 = _M0L6_2atmpS1972;
  _M0L3idxS606 = _M0L6_2atmpS1973;
  _M0L5entryS607 = _M0L5entryS613;
  while (1) {
    struct _M0TPB5EntryGssE** _M0L8_2afieldS3077 = _M0L4selfS609->$0;
    struct _M0TPB5EntryGssE** _M0L7entriesS1971 = _M0L8_2afieldS3077;
    struct _M0TPB5EntryGssE* _M0L6_2atmpS3076;
    struct _M0TPB5EntryGssE* _M0L7_2abindS608;
    if (
      _M0L3idxS606 < 0
      || _M0L3idxS606 >= Moonbit_array_length(_M0L7entriesS1971)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3076
    = (struct _M0TPB5EntryGssE*)_M0L7entriesS1971[_M0L3idxS606];
    _M0L7_2abindS608 = _M0L6_2atmpS3076;
    if (_M0L7_2abindS608 == 0) {
      _M0L5entryS607->$2 = _M0L3pslS605;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGssE(_M0L4selfS609, _M0L5entryS607, _M0L3idxS606);
      break;
    } else {
      struct _M0TPB5EntryGssE* _M0L7_2aSomeS611 = _M0L7_2abindS608;
      struct _M0TPB5EntryGssE* _M0L14_2acurr__entryS612 = _M0L7_2aSomeS611;
      int32_t _M0L3pslS1961 = _M0L14_2acurr__entryS612->$2;
      if (_M0L3pslS605 > _M0L3pslS1961) {
        int32_t _M0L3pslS1966;
        int32_t _M0L6_2atmpS1962;
        int32_t _M0L6_2atmpS1964;
        int32_t _M0L14capacity__maskS1965;
        int32_t _M0L6_2atmpS1963;
        _M0L5entryS607->$2 = _M0L3pslS605;
        moonbit_incref(_M0L14_2acurr__entryS612);
        moonbit_incref(_M0L4selfS609);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGssE(_M0L4selfS609, _M0L5entryS607, _M0L3idxS606);
        _M0L3pslS1966 = _M0L14_2acurr__entryS612->$2;
        _M0L6_2atmpS1962 = _M0L3pslS1966 + 1;
        _M0L6_2atmpS1964 = _M0L3idxS606 + 1;
        _M0L14capacity__maskS1965 = _M0L4selfS609->$3;
        _M0L6_2atmpS1963 = _M0L6_2atmpS1964 & _M0L14capacity__maskS1965;
        _M0L3pslS605 = _M0L6_2atmpS1962;
        _M0L3idxS606 = _M0L6_2atmpS1963;
        _M0L5entryS607 = _M0L14_2acurr__entryS612;
        continue;
      } else {
        int32_t _M0L6_2atmpS1967 = _M0L3pslS605 + 1;
        int32_t _M0L6_2atmpS1969 = _M0L3idxS606 + 1;
        int32_t _M0L14capacity__maskS1970 = _M0L4selfS609->$3;
        int32_t _M0L6_2atmpS1968 =
          _M0L6_2atmpS1969 & _M0L14capacity__maskS1970;
        struct _M0TPB5EntryGssE* _tmp_3518 = _M0L5entryS607;
        _M0L3pslS605 = _M0L6_2atmpS1967;
        _M0L3idxS606 = _M0L6_2atmpS1968;
        _M0L5entryS607 = _tmp_3518;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L4selfS619,
  int32_t _M0L3idxS624,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L5entryS623
) {
  int32_t _M0L3pslS1992;
  int32_t _M0L6_2atmpS1988;
  int32_t _M0L6_2atmpS1990;
  int32_t _M0L14capacity__maskS1991;
  int32_t _M0L6_2atmpS1989;
  int32_t _M0L3pslS615;
  int32_t _M0L3idxS616;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L5entryS617;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS1992 = _M0L5entryS623->$2;
  _M0L6_2atmpS1988 = _M0L3pslS1992 + 1;
  _M0L6_2atmpS1990 = _M0L3idxS624 + 1;
  _M0L14capacity__maskS1991 = _M0L4selfS619->$3;
  _M0L6_2atmpS1989 = _M0L6_2atmpS1990 & _M0L14capacity__maskS1991;
  _M0L3pslS615 = _M0L6_2atmpS1988;
  _M0L3idxS616 = _M0L6_2atmpS1989;
  _M0L5entryS617 = _M0L5entryS623;
  while (1) {
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L8_2afieldS3079 =
      _M0L4selfS619->$0;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L7entriesS1987 =
      _M0L8_2afieldS3079;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS3078;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2abindS618;
    if (
      _M0L3idxS616 < 0
      || _M0L3idxS616 >= Moonbit_array_length(_M0L7entriesS1987)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3078
    = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*)_M0L7entriesS1987[
        _M0L3idxS616
      ];
    _M0L7_2abindS618 = _M0L6_2atmpS3078;
    if (_M0L7_2abindS618 == 0) {
      _M0L5entryS617->$2 = _M0L3pslS615;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L4selfS619, _M0L5entryS617, _M0L3idxS616);
      break;
    } else {
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2aSomeS621 =
        _M0L7_2abindS618;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L14_2acurr__entryS622 =
        _M0L7_2aSomeS621;
      int32_t _M0L3pslS1977 = _M0L14_2acurr__entryS622->$2;
      if (_M0L3pslS615 > _M0L3pslS1977) {
        int32_t _M0L3pslS1982;
        int32_t _M0L6_2atmpS1978;
        int32_t _M0L6_2atmpS1980;
        int32_t _M0L14capacity__maskS1981;
        int32_t _M0L6_2atmpS1979;
        _M0L5entryS617->$2 = _M0L3pslS615;
        moonbit_incref(_M0L14_2acurr__entryS622);
        moonbit_incref(_M0L4selfS619);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP38clawteam8clawteam6config13CliToolConfigE(_M0L4selfS619, _M0L5entryS617, _M0L3idxS616);
        _M0L3pslS1982 = _M0L14_2acurr__entryS622->$2;
        _M0L6_2atmpS1978 = _M0L3pslS1982 + 1;
        _M0L6_2atmpS1980 = _M0L3idxS616 + 1;
        _M0L14capacity__maskS1981 = _M0L4selfS619->$3;
        _M0L6_2atmpS1979 = _M0L6_2atmpS1980 & _M0L14capacity__maskS1981;
        _M0L3pslS615 = _M0L6_2atmpS1978;
        _M0L3idxS616 = _M0L6_2atmpS1979;
        _M0L5entryS617 = _M0L14_2acurr__entryS622;
        continue;
      } else {
        int32_t _M0L6_2atmpS1983 = _M0L3pslS615 + 1;
        int32_t _M0L6_2atmpS1985 = _M0L3idxS616 + 1;
        int32_t _M0L14capacity__maskS1986 = _M0L4selfS619->$3;
        int32_t _M0L6_2atmpS1984 =
          _M0L6_2atmpS1985 & _M0L14capacity__maskS1986;
        struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _tmp_3520 =
          _M0L5entryS617;
        _M0L3pslS615 = _M0L6_2atmpS1983;
        _M0L3idxS616 = _M0L6_2atmpS1984;
        _M0L5entryS617 = _tmp_3520;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10push__awayGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L4selfS629,
  int32_t _M0L3idxS634,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L5entryS633
) {
  int32_t _M0L3pslS2008;
  int32_t _M0L6_2atmpS2004;
  int32_t _M0L6_2atmpS2006;
  int32_t _M0L14capacity__maskS2007;
  int32_t _M0L6_2atmpS2005;
  int32_t _M0L3pslS625;
  int32_t _M0L3idxS626;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L5entryS627;
  #line 158 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L3pslS2008 = _M0L5entryS633->$2;
  _M0L6_2atmpS2004 = _M0L3pslS2008 + 1;
  _M0L6_2atmpS2006 = _M0L3idxS634 + 1;
  _M0L14capacity__maskS2007 = _M0L4selfS629->$3;
  _M0L6_2atmpS2005 = _M0L6_2atmpS2006 & _M0L14capacity__maskS2007;
  _M0L3pslS625 = _M0L6_2atmpS2004;
  _M0L3idxS626 = _M0L6_2atmpS2005;
  _M0L5entryS627 = _M0L5entryS633;
  while (1) {
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L8_2afieldS3081 =
      _M0L4selfS629->$0;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L7entriesS2003 =
      _M0L8_2afieldS3081;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS3080;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2abindS628;
    if (
      _M0L3idxS626 < 0
      || _M0L3idxS626 >= Moonbit_array_length(_M0L7entriesS2003)
    ) {
      #line 164 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3080
    = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE*)_M0L7entriesS2003[
        _M0L3idxS626
      ];
    _M0L7_2abindS628 = _M0L6_2atmpS3080;
    if (_M0L7_2abindS628 == 0) {
      _M0L5entryS627->$2 = _M0L3pslS625;
      #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0MPB3Map10set__entryGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L4selfS629, _M0L5entryS627, _M0L3idxS626);
      break;
    } else {
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2aSomeS631 =
        _M0L7_2abindS628;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L14_2acurr__entryS632 =
        _M0L7_2aSomeS631;
      int32_t _M0L3pslS1993 = _M0L14_2acurr__entryS632->$2;
      if (_M0L3pslS625 > _M0L3pslS1993) {
        int32_t _M0L3pslS1998;
        int32_t _M0L6_2atmpS1994;
        int32_t _M0L6_2atmpS1996;
        int32_t _M0L14capacity__maskS1997;
        int32_t _M0L6_2atmpS1995;
        _M0L5entryS627->$2 = _M0L3pslS625;
        moonbit_incref(_M0L14_2acurr__entryS632);
        moonbit_incref(_M0L4selfS629);
        #line 173 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        _M0MPB3Map10set__entryGsRP38clawteam8clawteam6config13ChannelConfigE(_M0L4selfS629, _M0L5entryS627, _M0L3idxS626);
        _M0L3pslS1998 = _M0L14_2acurr__entryS632->$2;
        _M0L6_2atmpS1994 = _M0L3pslS1998 + 1;
        _M0L6_2atmpS1996 = _M0L3idxS626 + 1;
        _M0L14capacity__maskS1997 = _M0L4selfS629->$3;
        _M0L6_2atmpS1995 = _M0L6_2atmpS1996 & _M0L14capacity__maskS1997;
        _M0L3pslS625 = _M0L6_2atmpS1994;
        _M0L3idxS626 = _M0L6_2atmpS1995;
        _M0L5entryS627 = _M0L14_2acurr__entryS632;
        continue;
      } else {
        int32_t _M0L6_2atmpS1999 = _M0L3pslS625 + 1;
        int32_t _M0L6_2atmpS2001 = _M0L3idxS626 + 1;
        int32_t _M0L14capacity__maskS2002 = _M0L4selfS629->$3;
        int32_t _M0L6_2atmpS2000 =
          _M0L6_2atmpS2001 & _M0L14capacity__maskS2002;
        struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _tmp_3522 =
          _M0L5entryS627;
        _M0L3pslS625 = _M0L6_2atmpS1999;
        _M0L3idxS626 = _M0L6_2atmpS2000;
        _M0L5entryS627 = _tmp_3522;
        continue;
      }
    }
    break;
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS555,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS557,
  int32_t _M0L8new__idxS556
) {
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3084;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1919;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1920;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3083;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2afieldS3082;
  int32_t _M0L6_2acntS3347;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS558;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3084 = _M0L4selfS555->$0;
  _M0L7entriesS1919 = _M0L8_2afieldS3084;
  moonbit_incref(_M0L5entryS557);
  _M0L6_2atmpS1920 = _M0L5entryS557;
  if (
    _M0L8new__idxS556 < 0
    || _M0L8new__idxS556 >= Moonbit_array_length(_M0L7entriesS1919)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3083
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1919[
      _M0L8new__idxS556
    ];
  if (_M0L6_2aoldS3083) {
    moonbit_decref(_M0L6_2aoldS3083);
  }
  _M0L7entriesS1919[_M0L8new__idxS556] = _M0L6_2atmpS1920;
  _M0L8_2afieldS3082 = _M0L5entryS557->$1;
  _M0L6_2acntS3347 = Moonbit_object_header(_M0L5entryS557)->rc;
  if (_M0L6_2acntS3347 > 1) {
    int32_t _M0L11_2anew__cntS3350 = _M0L6_2acntS3347 - 1;
    Moonbit_object_header(_M0L5entryS557)->rc = _M0L11_2anew__cntS3350;
    if (_M0L8_2afieldS3082) {
      moonbit_incref(_M0L8_2afieldS3082);
    }
  } else if (_M0L6_2acntS3347 == 1) {
    struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3349 =
      _M0L5entryS557->$5;
    moonbit_string_t _M0L8_2afieldS3348;
    moonbit_decref(_M0L8_2afieldS3349);
    _M0L8_2afieldS3348 = _M0L5entryS557->$4;
    moonbit_decref(_M0L8_2afieldS3348);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS557);
  }
  _M0L7_2abindS558 = _M0L8_2afieldS3082;
  if (_M0L7_2abindS558 == 0) {
    if (_M0L7_2abindS558) {
      moonbit_decref(_M0L7_2abindS558);
    }
    _M0L4selfS555->$6 = _M0L8new__idxS556;
    moonbit_decref(_M0L4selfS555);
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS559;
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2anextS560;
    moonbit_decref(_M0L4selfS555);
    _M0L7_2aSomeS559 = _M0L7_2abindS558;
    _M0L7_2anextS560 = _M0L7_2aSomeS559;
    _M0L7_2anextS560->$0 = _M0L8new__idxS556;
    moonbit_decref(_M0L7_2anextS560);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS561,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS563,
  int32_t _M0L8new__idxS562
) {
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3087;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1921;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1922;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3086;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L8_2afieldS3085;
  int32_t _M0L6_2acntS3351;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS564;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3087 = _M0L4selfS561->$0;
  _M0L7entriesS1921 = _M0L8_2afieldS3087;
  moonbit_incref(_M0L5entryS563);
  _M0L6_2atmpS1922 = _M0L5entryS563;
  if (
    _M0L8new__idxS562 < 0
    || _M0L8new__idxS562 >= Moonbit_array_length(_M0L7entriesS1921)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3086
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1921[
      _M0L8new__idxS562
    ];
  if (_M0L6_2aoldS3086) {
    moonbit_decref(_M0L6_2aoldS3086);
  }
  _M0L7entriesS1921[_M0L8new__idxS562] = _M0L6_2atmpS1922;
  _M0L8_2afieldS3085 = _M0L5entryS563->$1;
  _M0L6_2acntS3351 = Moonbit_object_header(_M0L5entryS563)->rc;
  if (_M0L6_2acntS3351 > 1) {
    int32_t _M0L11_2anew__cntS3353 = _M0L6_2acntS3351 - 1;
    Moonbit_object_header(_M0L5entryS563)->rc = _M0L11_2anew__cntS3353;
    if (_M0L8_2afieldS3085) {
      moonbit_incref(_M0L8_2afieldS3085);
    }
  } else if (_M0L6_2acntS3351 == 1) {
    struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2afieldS3352 =
      _M0L5entryS563->$5;
    moonbit_decref(_M0L8_2afieldS3352);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS563);
  }
  _M0L7_2abindS564 = _M0L8_2afieldS3085;
  if (_M0L7_2abindS564 == 0) {
    if (_M0L7_2abindS564) {
      moonbit_decref(_M0L7_2abindS564);
    }
    _M0L4selfS561->$6 = _M0L8new__idxS562;
    moonbit_decref(_M0L4selfS561);
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS565;
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2anextS566;
    moonbit_decref(_M0L4selfS561);
    _M0L7_2aSomeS565 = _M0L7_2abindS564;
    _M0L7_2anextS566 = _M0L7_2aSomeS565;
    _M0L7_2anextS566->$0 = _M0L8new__idxS562;
    moonbit_decref(_M0L7_2anextS566);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGssE(
  struct _M0TPB3MapGssE* _M0L4selfS567,
  struct _M0TPB5EntryGssE* _M0L5entryS569,
  int32_t _M0L8new__idxS568
) {
  struct _M0TPB5EntryGssE** _M0L8_2afieldS3090;
  struct _M0TPB5EntryGssE** _M0L7entriesS1923;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS1924;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3089;
  struct _M0TPB5EntryGssE* _M0L8_2afieldS3088;
  int32_t _M0L6_2acntS3354;
  struct _M0TPB5EntryGssE* _M0L7_2abindS570;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3090 = _M0L4selfS567->$0;
  _M0L7entriesS1923 = _M0L8_2afieldS3090;
  moonbit_incref(_M0L5entryS569);
  _M0L6_2atmpS1924 = _M0L5entryS569;
  if (
    _M0L8new__idxS568 < 0
    || _M0L8new__idxS568 >= Moonbit_array_length(_M0L7entriesS1923)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3089
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS1923[_M0L8new__idxS568];
  if (_M0L6_2aoldS3089) {
    moonbit_decref(_M0L6_2aoldS3089);
  }
  _M0L7entriesS1923[_M0L8new__idxS568] = _M0L6_2atmpS1924;
  _M0L8_2afieldS3088 = _M0L5entryS569->$1;
  _M0L6_2acntS3354 = Moonbit_object_header(_M0L5entryS569)->rc;
  if (_M0L6_2acntS3354 > 1) {
    int32_t _M0L11_2anew__cntS3357 = _M0L6_2acntS3354 - 1;
    Moonbit_object_header(_M0L5entryS569)->rc = _M0L11_2anew__cntS3357;
    if (_M0L8_2afieldS3088) {
      moonbit_incref(_M0L8_2afieldS3088);
    }
  } else if (_M0L6_2acntS3354 == 1) {
    moonbit_string_t _M0L8_2afieldS3356 = _M0L5entryS569->$5;
    moonbit_string_t _M0L8_2afieldS3355;
    moonbit_decref(_M0L8_2afieldS3356);
    _M0L8_2afieldS3355 = _M0L5entryS569->$4;
    moonbit_decref(_M0L8_2afieldS3355);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS569);
  }
  _M0L7_2abindS570 = _M0L8_2afieldS3088;
  if (_M0L7_2abindS570 == 0) {
    if (_M0L7_2abindS570) {
      moonbit_decref(_M0L7_2abindS570);
    }
    _M0L4selfS567->$6 = _M0L8new__idxS568;
    moonbit_decref(_M0L4selfS567);
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS571;
    struct _M0TPB5EntryGssE* _M0L7_2anextS572;
    moonbit_decref(_M0L4selfS567);
    _M0L7_2aSomeS571 = _M0L7_2abindS570;
    _M0L7_2anextS572 = _M0L7_2aSomeS571;
    _M0L7_2anextS572->$0 = _M0L8new__idxS568;
    moonbit_decref(_M0L7_2anextS572);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L4selfS573,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L5entryS575,
  int32_t _M0L8new__idxS574
) {
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L8_2afieldS3093;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L7entriesS1925;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS1926;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2aoldS3092;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L8_2afieldS3091;
  int32_t _M0L6_2acntS3358;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2abindS576;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3093 = _M0L4selfS573->$0;
  _M0L7entriesS1925 = _M0L8_2afieldS3093;
  moonbit_incref(_M0L5entryS575);
  _M0L6_2atmpS1926 = _M0L5entryS575;
  if (
    _M0L8new__idxS574 < 0
    || _M0L8new__idxS574 >= Moonbit_array_length(_M0L7entriesS1925)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3092
  = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*)_M0L7entriesS1925[
      _M0L8new__idxS574
    ];
  if (_M0L6_2aoldS3092) {
    moonbit_decref(_M0L6_2aoldS3092);
  }
  _M0L7entriesS1925[_M0L8new__idxS574] = _M0L6_2atmpS1926;
  _M0L8_2afieldS3091 = _M0L5entryS575->$1;
  _M0L6_2acntS3358 = Moonbit_object_header(_M0L5entryS575)->rc;
  if (_M0L6_2acntS3358 > 1) {
    int32_t _M0L11_2anew__cntS3361 = _M0L6_2acntS3358 - 1;
    Moonbit_object_header(_M0L5entryS575)->rc = _M0L11_2anew__cntS3361;
    if (_M0L8_2afieldS3091) {
      moonbit_incref(_M0L8_2afieldS3091);
    }
  } else if (_M0L6_2acntS3358 == 1) {
    struct _M0TP38clawteam8clawteam6config13CliToolConfig* _M0L8_2afieldS3360 =
      _M0L5entryS575->$5;
    moonbit_string_t _M0L8_2afieldS3359;
    moonbit_decref(_M0L8_2afieldS3360);
    _M0L8_2afieldS3359 = _M0L5entryS575->$4;
    moonbit_decref(_M0L8_2afieldS3359);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS575);
  }
  _M0L7_2abindS576 = _M0L8_2afieldS3091;
  if (_M0L7_2abindS576 == 0) {
    if (_M0L7_2abindS576) {
      moonbit_decref(_M0L7_2abindS576);
    }
    _M0L4selfS573->$6 = _M0L8new__idxS574;
    moonbit_decref(_M0L4selfS573);
  } else {
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2aSomeS577;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2anextS578;
    moonbit_decref(_M0L4selfS573);
    _M0L7_2aSomeS577 = _M0L7_2abindS576;
    _M0L7_2anextS578 = _M0L7_2aSomeS577;
    _M0L7_2anextS578->$0 = _M0L8new__idxS574;
    moonbit_decref(_M0L7_2anextS578);
  }
  return 0;
}

int32_t _M0MPB3Map10set__entryGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L4selfS579,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L5entryS581,
  int32_t _M0L8new__idxS580
) {
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L8_2afieldS3096;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L7entriesS1927;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS1928;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2aoldS3095;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L8_2afieldS3094;
  int32_t _M0L6_2acntS3362;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2abindS582;
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8_2afieldS3096 = _M0L4selfS579->$0;
  _M0L7entriesS1927 = _M0L8_2afieldS3096;
  moonbit_incref(_M0L5entryS581);
  _M0L6_2atmpS1928 = _M0L5entryS581;
  if (
    _M0L8new__idxS580 < 0
    || _M0L8new__idxS580 >= Moonbit_array_length(_M0L7entriesS1927)
  ) {
    #line 190 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3095
  = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE*)_M0L7entriesS1927[
      _M0L8new__idxS580
    ];
  if (_M0L6_2aoldS3095) {
    moonbit_decref(_M0L6_2aoldS3095);
  }
  _M0L7entriesS1927[_M0L8new__idxS580] = _M0L6_2atmpS1928;
  _M0L8_2afieldS3094 = _M0L5entryS581->$1;
  _M0L6_2acntS3362 = Moonbit_object_header(_M0L5entryS581)->rc;
  if (_M0L6_2acntS3362 > 1) {
    int32_t _M0L11_2anew__cntS3365 = _M0L6_2acntS3362 - 1;
    Moonbit_object_header(_M0L5entryS581)->rc = _M0L11_2anew__cntS3365;
    if (_M0L8_2afieldS3094) {
      moonbit_incref(_M0L8_2afieldS3094);
    }
  } else if (_M0L6_2acntS3362 == 1) {
    struct _M0TP38clawteam8clawteam6config13ChannelConfig* _M0L8_2afieldS3364 =
      _M0L5entryS581->$5;
    moonbit_string_t _M0L8_2afieldS3363;
    moonbit_decref(_M0L8_2afieldS3364);
    _M0L8_2afieldS3363 = _M0L5entryS581->$4;
    moonbit_decref(_M0L8_2afieldS3363);
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_free(_M0L5entryS581);
  }
  _M0L7_2abindS582 = _M0L8_2afieldS3094;
  if (_M0L7_2abindS582 == 0) {
    if (_M0L7_2abindS582) {
      moonbit_decref(_M0L7_2abindS582);
    }
    _M0L4selfS579->$6 = _M0L8new__idxS580;
    moonbit_decref(_M0L4selfS579);
  } else {
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2aSomeS583;
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2anextS584;
    moonbit_decref(_M0L4selfS579);
    _M0L7_2aSomeS583 = _M0L7_2abindS582;
    _M0L7_2anextS584 = _M0L7_2aSomeS583;
    _M0L7_2anextS584->$0 = _M0L8new__idxS580;
    moonbit_decref(_M0L7_2anextS584);
  }
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS536,
  int32_t _M0L3idxS538,
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L5entryS537
) {
  int32_t _M0L7_2abindS535;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3098;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1879;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1880;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3097;
  int32_t _M0L4sizeS1882;
  int32_t _M0L6_2atmpS1881;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS535 = _M0L4selfS536->$6;
  switch (_M0L7_2abindS535) {
    case -1: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1874;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3099;
      moonbit_incref(_M0L5entryS537);
      _M0L6_2atmpS1874 = _M0L5entryS537;
      _M0L6_2aoldS3099 = _M0L4selfS536->$5;
      if (_M0L6_2aoldS3099) {
        moonbit_decref(_M0L6_2aoldS3099);
      }
      _M0L4selfS536->$5 = _M0L6_2atmpS1874;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L8_2afieldS3102 =
        _M0L4selfS536->$0;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7entriesS1878 =
        _M0L8_2afieldS3102;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS3101;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1877;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1875;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1876;
      struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2aoldS3100;
      if (
        _M0L7_2abindS535 < 0
        || _M0L7_2abindS535 >= Moonbit_array_length(_M0L7entriesS1878)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3101
      = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1878[
          _M0L7_2abindS535
        ];
      _M0L6_2atmpS1877 = _M0L6_2atmpS3101;
      if (_M0L6_2atmpS1877) {
        moonbit_incref(_M0L6_2atmpS1877);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1875
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(_M0L6_2atmpS1877);
      moonbit_incref(_M0L5entryS537);
      _M0L6_2atmpS1876 = _M0L5entryS537;
      _M0L6_2aoldS3100 = _M0L6_2atmpS1875->$1;
      if (_M0L6_2aoldS3100) {
        moonbit_decref(_M0L6_2aoldS3100);
      }
      _M0L6_2atmpS1875->$1 = _M0L6_2atmpS1876;
      moonbit_decref(_M0L6_2atmpS1875);
      break;
    }
  }
  _M0L4selfS536->$6 = _M0L3idxS538;
  _M0L8_2afieldS3098 = _M0L4selfS536->$0;
  _M0L7entriesS1879 = _M0L8_2afieldS3098;
  _M0L6_2atmpS1880 = _M0L5entryS537;
  if (
    _M0L3idxS538 < 0
    || _M0L3idxS538 >= Moonbit_array_length(_M0L7entriesS1879)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3097
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)_M0L7entriesS1879[
      _M0L3idxS538
    ];
  if (_M0L6_2aoldS3097) {
    moonbit_decref(_M0L6_2aoldS3097);
  }
  _M0L7entriesS1879[_M0L3idxS538] = _M0L6_2atmpS1880;
  _M0L4sizeS1882 = _M0L4selfS536->$1;
  _M0L6_2atmpS1881 = _M0L4sizeS1882 + 1;
  _M0L4selfS536->$1 = _M0L6_2atmpS1881;
  moonbit_decref(_M0L4selfS536);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGiUWEuQRPC15error5ErrorNsEE(
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS540,
  int32_t _M0L3idxS542,
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L5entryS541
) {
  int32_t _M0L7_2abindS539;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3104;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1888;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1889;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3103;
  int32_t _M0L4sizeS1891;
  int32_t _M0L6_2atmpS1890;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS539 = _M0L4selfS540->$6;
  switch (_M0L7_2abindS539) {
    case -1: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1883;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3105;
      moonbit_incref(_M0L5entryS541);
      _M0L6_2atmpS1883 = _M0L5entryS541;
      _M0L6_2aoldS3105 = _M0L4selfS540->$5;
      if (_M0L6_2aoldS3105) {
        moonbit_decref(_M0L6_2aoldS3105);
      }
      _M0L4selfS540->$5 = _M0L6_2atmpS1883;
      break;
    }
    default: {
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L8_2afieldS3108 =
        _M0L4selfS540->$0;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7entriesS1887 =
        _M0L8_2afieldS3108;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS3107;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1886;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1884;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1885;
      struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2aoldS3106;
      if (
        _M0L7_2abindS539 < 0
        || _M0L7_2abindS539 >= Moonbit_array_length(_M0L7entriesS1887)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3107
      = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1887[
          _M0L7_2abindS539
        ];
      _M0L6_2atmpS1886 = _M0L6_2atmpS3107;
      if (_M0L6_2atmpS1886) {
        moonbit_incref(_M0L6_2atmpS1886);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1884
      = _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1886);
      moonbit_incref(_M0L5entryS541);
      _M0L6_2atmpS1885 = _M0L5entryS541;
      _M0L6_2aoldS3106 = _M0L6_2atmpS1884->$1;
      if (_M0L6_2aoldS3106) {
        moonbit_decref(_M0L6_2aoldS3106);
      }
      _M0L6_2atmpS1884->$1 = _M0L6_2atmpS1885;
      moonbit_decref(_M0L6_2atmpS1884);
      break;
    }
  }
  _M0L4selfS540->$6 = _M0L3idxS542;
  _M0L8_2afieldS3104 = _M0L4selfS540->$0;
  _M0L7entriesS1888 = _M0L8_2afieldS3104;
  _M0L6_2atmpS1889 = _M0L5entryS541;
  if (
    _M0L3idxS542 < 0
    || _M0L3idxS542 >= Moonbit_array_length(_M0L7entriesS1888)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3103
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE*)_M0L7entriesS1888[
      _M0L3idxS542
    ];
  if (_M0L6_2aoldS3103) {
    moonbit_decref(_M0L6_2aoldS3103);
  }
  _M0L7entriesS1888[_M0L3idxS542] = _M0L6_2atmpS1889;
  _M0L4sizeS1891 = _M0L4selfS540->$1;
  _M0L6_2atmpS1890 = _M0L4sizeS1891 + 1;
  _M0L4selfS540->$1 = _M0L6_2atmpS1890;
  moonbit_decref(_M0L4selfS540);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGssE(
  struct _M0TPB3MapGssE* _M0L4selfS544,
  int32_t _M0L3idxS546,
  struct _M0TPB5EntryGssE* _M0L5entryS545
) {
  int32_t _M0L7_2abindS543;
  struct _M0TPB5EntryGssE** _M0L8_2afieldS3110;
  struct _M0TPB5EntryGssE** _M0L7entriesS1897;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS1898;
  struct _M0TPB5EntryGssE* _M0L6_2aoldS3109;
  int32_t _M0L4sizeS1900;
  int32_t _M0L6_2atmpS1899;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS543 = _M0L4selfS544->$6;
  switch (_M0L7_2abindS543) {
    case -1: {
      struct _M0TPB5EntryGssE* _M0L6_2atmpS1892;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3111;
      moonbit_incref(_M0L5entryS545);
      _M0L6_2atmpS1892 = _M0L5entryS545;
      _M0L6_2aoldS3111 = _M0L4selfS544->$5;
      if (_M0L6_2aoldS3111) {
        moonbit_decref(_M0L6_2aoldS3111);
      }
      _M0L4selfS544->$5 = _M0L6_2atmpS1892;
      break;
    }
    default: {
      struct _M0TPB5EntryGssE** _M0L8_2afieldS3114 = _M0L4selfS544->$0;
      struct _M0TPB5EntryGssE** _M0L7entriesS1896 = _M0L8_2afieldS3114;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS3113;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS1895;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS1893;
      struct _M0TPB5EntryGssE* _M0L6_2atmpS1894;
      struct _M0TPB5EntryGssE* _M0L6_2aoldS3112;
      if (
        _M0L7_2abindS543 < 0
        || _M0L7_2abindS543 >= Moonbit_array_length(_M0L7entriesS1896)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3113
      = (struct _M0TPB5EntryGssE*)_M0L7entriesS1896[_M0L7_2abindS543];
      _M0L6_2atmpS1895 = _M0L6_2atmpS3113;
      if (_M0L6_2atmpS1895) {
        moonbit_incref(_M0L6_2atmpS1895);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1893
      = _M0MPC16option6Option6unwrapGRPB5EntryGssEE(_M0L6_2atmpS1895);
      moonbit_incref(_M0L5entryS545);
      _M0L6_2atmpS1894 = _M0L5entryS545;
      _M0L6_2aoldS3112 = _M0L6_2atmpS1893->$1;
      if (_M0L6_2aoldS3112) {
        moonbit_decref(_M0L6_2aoldS3112);
      }
      _M0L6_2atmpS1893->$1 = _M0L6_2atmpS1894;
      moonbit_decref(_M0L6_2atmpS1893);
      break;
    }
  }
  _M0L4selfS544->$6 = _M0L3idxS546;
  _M0L8_2afieldS3110 = _M0L4selfS544->$0;
  _M0L7entriesS1897 = _M0L8_2afieldS3110;
  _M0L6_2atmpS1898 = _M0L5entryS545;
  if (
    _M0L3idxS546 < 0
    || _M0L3idxS546 >= Moonbit_array_length(_M0L7entriesS1897)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3109
  = (struct _M0TPB5EntryGssE*)_M0L7entriesS1897[_M0L3idxS546];
  if (_M0L6_2aoldS3109) {
    moonbit_decref(_M0L6_2aoldS3109);
  }
  _M0L7entriesS1897[_M0L3idxS546] = _M0L6_2atmpS1898;
  _M0L4sizeS1900 = _M0L4selfS544->$1;
  _M0L6_2atmpS1899 = _M0L4sizeS1900 + 1;
  _M0L4selfS544->$1 = _M0L6_2atmpS1899;
  moonbit_decref(_M0L4selfS544);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRP38clawteam8clawteam6config13CliToolConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L4selfS548,
  int32_t _M0L3idxS550,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L5entryS549
) {
  int32_t _M0L7_2abindS547;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L8_2afieldS3116;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L7entriesS1906;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS1907;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2aoldS3115;
  int32_t _M0L4sizeS1909;
  int32_t _M0L6_2atmpS1908;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS547 = _M0L4selfS548->$6;
  switch (_M0L7_2abindS547) {
    case -1: {
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS1901;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2aoldS3117;
      moonbit_incref(_M0L5entryS549);
      _M0L6_2atmpS1901 = _M0L5entryS549;
      _M0L6_2aoldS3117 = _M0L4selfS548->$5;
      if (_M0L6_2aoldS3117) {
        moonbit_decref(_M0L6_2aoldS3117);
      }
      _M0L4selfS548->$5 = _M0L6_2atmpS1901;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L8_2afieldS3120 =
        _M0L4selfS548->$0;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L7entriesS1905 =
        _M0L8_2afieldS3120;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS3119;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS1904;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS1902;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS1903;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2aoldS3118;
      if (
        _M0L7_2abindS547 < 0
        || _M0L7_2abindS547 >= Moonbit_array_length(_M0L7entriesS1905)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3119
      = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*)_M0L7entriesS1905[
          _M0L7_2abindS547
        ];
      _M0L6_2atmpS1904 = _M0L6_2atmpS3119;
      if (_M0L6_2atmpS1904) {
        moonbit_incref(_M0L6_2atmpS1904);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1902
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigEE(_M0L6_2atmpS1904);
      moonbit_incref(_M0L5entryS549);
      _M0L6_2atmpS1903 = _M0L5entryS549;
      _M0L6_2aoldS3118 = _M0L6_2atmpS1902->$1;
      if (_M0L6_2aoldS3118) {
        moonbit_decref(_M0L6_2aoldS3118);
      }
      _M0L6_2atmpS1902->$1 = _M0L6_2atmpS1903;
      moonbit_decref(_M0L6_2atmpS1902);
      break;
    }
  }
  _M0L4selfS548->$6 = _M0L3idxS550;
  _M0L8_2afieldS3116 = _M0L4selfS548->$0;
  _M0L7entriesS1906 = _M0L8_2afieldS3116;
  _M0L6_2atmpS1907 = _M0L5entryS549;
  if (
    _M0L3idxS550 < 0
    || _M0L3idxS550 >= Moonbit_array_length(_M0L7entriesS1906)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3115
  = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE*)_M0L7entriesS1906[
      _M0L3idxS550
    ];
  if (_M0L6_2aoldS3115) {
    moonbit_decref(_M0L6_2aoldS3115);
  }
  _M0L7entriesS1906[_M0L3idxS550] = _M0L6_2atmpS1907;
  _M0L4sizeS1909 = _M0L4selfS548->$1;
  _M0L6_2atmpS1908 = _M0L4sizeS1909 + 1;
  _M0L4selfS548->$1 = _M0L6_2atmpS1908;
  moonbit_decref(_M0L4selfS548);
  return 0;
}

int32_t _M0MPB3Map20add__entry__to__tailGsRP38clawteam8clawteam6config13ChannelConfigE(
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L4selfS552,
  int32_t _M0L3idxS554,
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L5entryS553
) {
  int32_t _M0L7_2abindS551;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L8_2afieldS3122;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L7entriesS1915;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS1916;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2aoldS3121;
  int32_t _M0L4sizeS1918;
  int32_t _M0L6_2atmpS1917;
  #line 443 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS551 = _M0L4selfS552->$6;
  switch (_M0L7_2abindS551) {
    case -1: {
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS1910;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2aoldS3123;
      moonbit_incref(_M0L5entryS553);
      _M0L6_2atmpS1910 = _M0L5entryS553;
      _M0L6_2aoldS3123 = _M0L4selfS552->$5;
      if (_M0L6_2aoldS3123) {
        moonbit_decref(_M0L6_2aoldS3123);
      }
      _M0L4selfS552->$5 = _M0L6_2atmpS1910;
      break;
    }
    default: {
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L8_2afieldS3126 =
        _M0L4selfS552->$0;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L7entriesS1914 =
        _M0L8_2afieldS3126;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS3125;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS1913;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS1911;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS1912;
      struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2aoldS3124;
      if (
        _M0L7_2abindS551 < 0
        || _M0L7_2abindS551 >= Moonbit_array_length(_M0L7entriesS1914)
      ) {
        #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS3125
      = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE*)_M0L7entriesS1914[
          _M0L7_2abindS551
        ];
      _M0L6_2atmpS1913 = _M0L6_2atmpS3125;
      if (_M0L6_2atmpS1913) {
        moonbit_incref(_M0L6_2atmpS1913);
      }
      #line 450 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
      _M0L6_2atmpS1911
      = _M0MPC16option6Option6unwrapGRPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigEE(_M0L6_2atmpS1913);
      moonbit_incref(_M0L5entryS553);
      _M0L6_2atmpS1912 = _M0L5entryS553;
      _M0L6_2aoldS3124 = _M0L6_2atmpS1911->$1;
      if (_M0L6_2aoldS3124) {
        moonbit_decref(_M0L6_2aoldS3124);
      }
      _M0L6_2atmpS1911->$1 = _M0L6_2atmpS1912;
      moonbit_decref(_M0L6_2atmpS1911);
      break;
    }
  }
  _M0L4selfS552->$6 = _M0L3idxS554;
  _M0L8_2afieldS3122 = _M0L4selfS552->$0;
  _M0L7entriesS1915 = _M0L8_2afieldS3122;
  _M0L6_2atmpS1916 = _M0L5entryS553;
  if (
    _M0L3idxS554 < 0
    || _M0L3idxS554 >= Moonbit_array_length(_M0L7entriesS1915)
  ) {
    #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
    moonbit_panic();
  }
  _M0L6_2aoldS3121
  = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE*)_M0L7entriesS1915[
      _M0L3idxS554
    ];
  if (_M0L6_2aoldS3121) {
    moonbit_decref(_M0L6_2aoldS3121);
  }
  _M0L7entriesS1915[_M0L3idxS554] = _M0L6_2atmpS1916;
  _M0L4sizeS1918 = _M0L4selfS552->$1;
  _M0L6_2atmpS1917 = _M0L4sizeS1918 + 1;
  _M0L4selfS552->$1 = _M0L6_2atmpS1917;
  moonbit_decref(_M0L4selfS552);
  return 0;
}

struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPB3Map11new_2einnerGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(
  int32_t _M0L8capacityS506
) {
  int32_t _M0L8capacityS505;
  int32_t _M0L7_2abindS507;
  int32_t _M0L7_2abindS508;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L6_2atmpS1869;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS509;
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2abindS510;
  struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _block_3523;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS505
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS506);
  _M0L7_2abindS507 = _M0L8capacityS505 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS508 = _M0FPB21calc__grow__threshold(_M0L8capacityS505);
  _M0L6_2atmpS1869 = 0;
  _M0L7_2abindS509
  = (struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array(_M0L8capacityS505, _M0L6_2atmpS1869);
  _M0L7_2abindS510 = 0;
  _block_3523
  = (struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_block_3523)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _block_3523->$0 = _M0L7_2abindS509;
  _block_3523->$1 = 0;
  _block_3523->$2 = _M0L8capacityS505;
  _block_3523->$3 = _M0L7_2abindS507;
  _block_3523->$4 = _M0L7_2abindS508;
  _block_3523->$5 = _M0L7_2abindS510;
  _block_3523->$6 = -1;
  return _block_3523;
}

struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0MPB3Map11new_2einnerGiUWEuQRPC15error5ErrorNsEE(
  int32_t _M0L8capacityS512
) {
  int32_t _M0L8capacityS511;
  int32_t _M0L7_2abindS513;
  int32_t _M0L7_2abindS514;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1870;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS515;
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2abindS516;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _block_3524;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS511
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS512);
  _M0L7_2abindS513 = _M0L8capacityS511 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS514 = _M0FPB21calc__grow__threshold(_M0L8capacityS511);
  _M0L6_2atmpS1870 = 0;
  _M0L7_2abindS515
  = (struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array(_M0L8capacityS511, _M0L6_2atmpS1870);
  _M0L7_2abindS516 = 0;
  _block_3524
  = (struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_block_3524)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE, $0) >> 2, 2, 0);
  _block_3524->$0 = _M0L7_2abindS515;
  _block_3524->$1 = 0;
  _block_3524->$2 = _M0L8capacityS511;
  _block_3524->$3 = _M0L7_2abindS513;
  _block_3524->$4 = _M0L7_2abindS514;
  _block_3524->$5 = _M0L7_2abindS516;
  _block_3524->$6 = -1;
  return _block_3524;
}

struct _M0TPB3MapGssE* _M0MPB3Map11new_2einnerGssE(int32_t _M0L8capacityS518) {
  int32_t _M0L8capacityS517;
  int32_t _M0L7_2abindS519;
  int32_t _M0L7_2abindS520;
  struct _M0TPB5EntryGssE* _M0L6_2atmpS1871;
  struct _M0TPB5EntryGssE** _M0L7_2abindS521;
  struct _M0TPB5EntryGssE* _M0L7_2abindS522;
  struct _M0TPB3MapGssE* _block_3525;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS517
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS518);
  _M0L7_2abindS519 = _M0L8capacityS517 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS520 = _M0FPB21calc__grow__threshold(_M0L8capacityS517);
  _M0L6_2atmpS1871 = 0;
  _M0L7_2abindS521
  = (struct _M0TPB5EntryGssE**)moonbit_make_ref_array(_M0L8capacityS517, _M0L6_2atmpS1871);
  _M0L7_2abindS522 = 0;
  _block_3525
  = (struct _M0TPB3MapGssE*)moonbit_malloc(sizeof(struct _M0TPB3MapGssE));
  Moonbit_object_header(_block_3525)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGssE, $0) >> 2, 2, 0);
  _block_3525->$0 = _M0L7_2abindS521;
  _block_3525->$1 = 0;
  _block_3525->$2 = _M0L8capacityS517;
  _block_3525->$3 = _M0L7_2abindS519;
  _block_3525->$4 = _M0L7_2abindS520;
  _block_3525->$5 = _M0L7_2abindS522;
  _block_3525->$6 = -1;
  return _block_3525;
}

struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _M0MPB3Map11new_2einnerGsRP38clawteam8clawteam6config13CliToolConfigE(
  int32_t _M0L8capacityS524
) {
  int32_t _M0L8capacityS523;
  int32_t _M0L7_2abindS525;
  int32_t _M0L7_2abindS526;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L6_2atmpS1872;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE** _M0L7_2abindS527;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2abindS528;
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE* _block_3526;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS523
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS524);
  _M0L7_2abindS525 = _M0L8capacityS523 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS526 = _M0FPB21calc__grow__threshold(_M0L8capacityS523);
  _M0L6_2atmpS1872 = 0;
  _M0L7_2abindS527
  = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE**)moonbit_make_ref_array(_M0L8capacityS523, _M0L6_2atmpS1872);
  _M0L7_2abindS528 = 0;
  _block_3526
  = (struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE));
  Moonbit_object_header(_block_3526)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRP38clawteam8clawteam6config13CliToolConfigE, $0) >> 2, 2, 0);
  _block_3526->$0 = _M0L7_2abindS527;
  _block_3526->$1 = 0;
  _block_3526->$2 = _M0L8capacityS523;
  _block_3526->$3 = _M0L7_2abindS525;
  _block_3526->$4 = _M0L7_2abindS526;
  _block_3526->$5 = _M0L7_2abindS528;
  _block_3526->$6 = -1;
  return _block_3526;
}

struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _M0MPB3Map11new_2einnerGsRP38clawteam8clawteam6config13ChannelConfigE(
  int32_t _M0L8capacityS530
) {
  int32_t _M0L8capacityS529;
  int32_t _M0L7_2abindS531;
  int32_t _M0L7_2abindS532;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L6_2atmpS1873;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE** _M0L7_2abindS533;
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2abindS534;
  struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE* _block_3527;
  #line 57 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  #line 58 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L8capacityS529
  = _M0MPC13int3Int20next__power__of__two(_M0L8capacityS530);
  _M0L7_2abindS531 = _M0L8capacityS529 - 1;
  #line 63 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L7_2abindS532 = _M0FPB21calc__grow__threshold(_M0L8capacityS529);
  _M0L6_2atmpS1873 = 0;
  _M0L7_2abindS533
  = (struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE**)moonbit_make_ref_array(_M0L8capacityS529, _M0L6_2atmpS1873);
  _M0L7_2abindS534 = 0;
  _block_3527
  = (struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE*)moonbit_malloc(sizeof(struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE));
  Moonbit_object_header(_block_3527)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB3MapGsRP38clawteam8clawteam6config13ChannelConfigE, $0) >> 2, 2, 0);
  _block_3527->$0 = _M0L7_2abindS533;
  _block_3527->$1 = 0;
  _block_3527->$2 = _M0L8capacityS529;
  _block_3527->$3 = _M0L7_2abindS531;
  _block_3527->$4 = _M0L7_2abindS532;
  _block_3527->$5 = _M0L7_2abindS534;
  _block_3527->$6 = -1;
  return _block_3527;
}

int32_t _M0MPC13int3Int20next__power__of__two(int32_t _M0L4selfS504) {
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
  if (_M0L4selfS504 >= 0) {
    int32_t _M0L6_2atmpS1868;
    int32_t _M0L6_2atmpS1867;
    int32_t _M0L6_2atmpS1866;
    int32_t _M0L6_2atmpS1865;
    if (_M0L4selfS504 <= 1) {
      return 1;
    }
    if (_M0L4selfS504 > 1073741824) {
      return 1073741824;
    }
    _M0L6_2atmpS1868 = _M0L4selfS504 - 1;
    #line 44 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    _M0L6_2atmpS1867 = moonbit_clz32(_M0L6_2atmpS1868);
    _M0L6_2atmpS1866 = _M0L6_2atmpS1867 - 1;
    _M0L6_2atmpS1865 = 2147483647 >> (_M0L6_2atmpS1866 & 31);
    return _M0L6_2atmpS1865 + 1;
  } else {
    #line 34 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\int.mbt"
    moonbit_panic();
  }
}

int32_t _M0FPB21calc__grow__threshold(int32_t _M0L8capacityS503) {
  int32_t _M0L6_2atmpS1864;
  #line 503 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\linked_hash_map.mbt"
  _M0L6_2atmpS1864 = _M0L8capacityS503 * 13;
  return _M0L6_2atmpS1864 / 16;
}

struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0MPC16option6Option6unwrapGRPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L4selfS493
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS493 == 0) {
    if (_M0L4selfS493) {
      moonbit_decref(_M0L4selfS493);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L7_2aSomeS494 =
      _M0L4selfS493;
    return _M0L7_2aSomeS494;
  }
}

struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0MPC16option6Option6unwrapGRPB5EntryGiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L4selfS495
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS495 == 0) {
    if (_M0L4selfS495) {
      moonbit_decref(_M0L4selfS495);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGiUWEuQRPC15error5ErrorNsEE* _M0L7_2aSomeS496 =
      _M0L4selfS495;
    return _M0L7_2aSomeS496;
  }
}

struct _M0TPB5EntryGssE* _M0MPC16option6Option6unwrapGRPB5EntryGssEE(
  struct _M0TPB5EntryGssE* _M0L4selfS497
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS497 == 0) {
    if (_M0L4selfS497) {
      moonbit_decref(_M0L4selfS497);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGssE* _M0L7_2aSomeS498 = _M0L4selfS497;
    return _M0L7_2aSomeS498;
  }
}

struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigEE(
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L4selfS499
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS499 == 0) {
    if (_M0L4selfS499) {
      moonbit_decref(_M0L4selfS499);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13CliToolConfigE* _M0L7_2aSomeS500 =
      _M0L4selfS499;
    return _M0L7_2aSomeS500;
  }
}

struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0MPC16option6Option6unwrapGRPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigEE(
  struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L4selfS501
) {
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
  if (_M0L4selfS501 == 0) {
    if (_M0L4selfS501) {
      moonbit_decref(_M0L4selfS501);
    }
    #line 39 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\option.mbt"
    moonbit_panic();
  } else {
    struct _M0TPB5EntryGsRP38clawteam8clawteam6config13ChannelConfigE* _M0L7_2aSomeS502 =
      _M0L4selfS501;
    return _M0L7_2aSomeS502;
  }
}

struct _M0TWEOs* _M0MPC15array13ReadOnlyArray4iterGsE(
  moonbit_string_t* _M0L4selfS492
) {
  moonbit_string_t* _M0L6_2atmpS1863;
  #line 165 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  _M0L6_2atmpS1863 = _M0L4selfS492;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\readonlyarray.mbt"
  return _M0MPC15array10FixedArray4iterGsE(_M0L6_2atmpS1863);
}

struct _M0TWEOs* _M0MPC15array10FixedArray4iterGsE(
  moonbit_string_t* _M0L4selfS491
) {
  moonbit_string_t* _M0L6_2atmpS1861;
  int32_t _M0L6_2atmpS3127;
  int32_t _M0L6_2atmpS1862;
  struct _M0TPB9ArrayViewGsE _M0L6_2atmpS1860;
  #line 1508 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  moonbit_incref(_M0L4selfS491);
  _M0L6_2atmpS1861 = _M0L4selfS491;
  _M0L6_2atmpS3127 = Moonbit_array_length(_M0L4selfS491);
  moonbit_decref(_M0L4selfS491);
  _M0L6_2atmpS1862 = _M0L6_2atmpS3127;
  _M0L6_2atmpS1860
  = (struct _M0TPB9ArrayViewGsE){
    0, _M0L6_2atmpS1862, _M0L6_2atmpS1861
  };
  #line 1510 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray.mbt"
  return _M0MPC15array9ArrayView4iterGsE(_M0L6_2atmpS1860);
}

struct _M0TWEOs* _M0MPC15array9ArrayView4iterGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS489
) {
  struct _M0TPC13ref3RefGiE* _M0L1iS488;
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1849__l570__* _closure_3528;
  struct _M0TWEOs* _M0L6_2atmpS1848;
  #line 567 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L1iS488
  = (struct _M0TPC13ref3RefGiE*)moonbit_malloc(sizeof(struct _M0TPC13ref3RefGiE));
  Moonbit_object_header(_M0L1iS488)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPC13ref3RefGiE) >> 2, 0, 0);
  _M0L1iS488->$0 = 0;
  _closure_3528
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1849__l570__*)moonbit_malloc(sizeof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1849__l570__));
  Moonbit_object_header(_closure_3528)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1849__l570__, $0_0) >> 2, 2, 0);
  _closure_3528->code = &_M0MPC15array9ArrayView4iterGsEC1849l570;
  _closure_3528->$0_0 = _M0L4selfS489.$0;
  _closure_3528->$0_1 = _M0L4selfS489.$1;
  _closure_3528->$0_2 = _M0L4selfS489.$2;
  _closure_3528->$1 = _M0L1iS488;
  _M0L6_2atmpS1848 = (struct _M0TWEOs*)_closure_3528;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  return _M0MPB4Iter3newGsE(_M0L6_2atmpS1848);
}

moonbit_string_t _M0MPC15array9ArrayView4iterGsEC1849l570(
  struct _M0TWEOs* _M0L6_2aenvS1850
) {
  struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1849__l570__* _M0L14_2acasted__envS1851;
  struct _M0TPC13ref3RefGiE* _M0L8_2afieldS3132;
  struct _M0TPC13ref3RefGiE* _M0L1iS488;
  struct _M0TPB9ArrayViewGsE _M0L8_2afieldS3131;
  int32_t _M0L6_2acntS3366;
  struct _M0TPB9ArrayViewGsE _M0L4selfS489;
  int32_t _M0L3valS1852;
  int32_t _M0L6_2atmpS1853;
  #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L14_2acasted__envS1851
  = (struct _M0R59ArrayView_3a_3aiter_7c_5bString_5d_7c_2eanon__u1849__l570__*)_M0L6_2aenvS1850;
  _M0L8_2afieldS3132 = _M0L14_2acasted__envS1851->$1;
  _M0L1iS488 = _M0L8_2afieldS3132;
  _M0L8_2afieldS3131
  = (struct _M0TPB9ArrayViewGsE){
    _M0L14_2acasted__envS1851->$0_1,
      _M0L14_2acasted__envS1851->$0_2,
      _M0L14_2acasted__envS1851->$0_0
  };
  _M0L6_2acntS3366 = Moonbit_object_header(_M0L14_2acasted__envS1851)->rc;
  if (_M0L6_2acntS3366 > 1) {
    int32_t _M0L11_2anew__cntS3367 = _M0L6_2acntS3366 - 1;
    Moonbit_object_header(_M0L14_2acasted__envS1851)->rc
    = _M0L11_2anew__cntS3367;
    moonbit_incref(_M0L1iS488);
    moonbit_incref(_M0L8_2afieldS3131.$0);
  } else if (_M0L6_2acntS3366 == 1) {
    #line 570 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
    moonbit_free(_M0L14_2acasted__envS1851);
  }
  _M0L4selfS489 = _M0L8_2afieldS3131;
  _M0L3valS1852 = _M0L1iS488->$0;
  moonbit_incref(_M0L4selfS489.$0);
  #line 571 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L6_2atmpS1853 = _M0MPC15array9ArrayView6lengthGsE(_M0L4selfS489);
  if (_M0L3valS1852 < _M0L6_2atmpS1853) {
    moonbit_string_t* _M0L8_2afieldS3130 = _M0L4selfS489.$0;
    moonbit_string_t* _M0L3bufS1856 = _M0L8_2afieldS3130;
    int32_t _M0L8_2afieldS3129 = _M0L4selfS489.$1;
    int32_t _M0L5startS1858 = _M0L8_2afieldS3129;
    int32_t _M0L3valS1859 = _M0L1iS488->$0;
    int32_t _M0L6_2atmpS1857 = _M0L5startS1858 + _M0L3valS1859;
    moonbit_string_t _M0L6_2atmpS3128 =
      (moonbit_string_t)_M0L3bufS1856[_M0L6_2atmpS1857];
    moonbit_string_t _M0L4elemS490;
    int32_t _M0L3valS1855;
    int32_t _M0L6_2atmpS1854;
    moonbit_incref(_M0L6_2atmpS3128);
    moonbit_decref(_M0L3bufS1856);
    _M0L4elemS490 = _M0L6_2atmpS3128;
    _M0L3valS1855 = _M0L1iS488->$0;
    _M0L6_2atmpS1854 = _M0L3valS1855 + 1;
    _M0L1iS488->$0 = _M0L6_2atmpS1854;
    moonbit_decref(_M0L1iS488);
    return _M0L4elemS490;
  } else {
    moonbit_decref(_M0L4selfS489.$0);
    moonbit_decref(_M0L1iS488);
    return 0;
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show10to__string(
  moonbit_string_t _M0L4selfS487
) {
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0L4selfS487;
}

int32_t _M0IPC13int3IntPB4Show6output(
  int32_t _M0L4selfS486,
  struct _M0TPB6Logger _M0L6loggerS485
) {
  moonbit_string_t _M0L6_2atmpS1847;
  #line 30 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1847 = _M0MPC13int3Int18to__string_2einner(_M0L4selfS486, 10);
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS485.$0->$method_0(_M0L6loggerS485.$1, _M0L6_2atmpS1847);
  return 0;
}

int32_t _M0IPC14bool4BoolPB4Show6output(
  int32_t _M0L4selfS483,
  struct _M0TPB6Logger _M0L6loggerS484
) {
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L4selfS483) {
    #line 23 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS484.$0->$method_0(_M0L6loggerS484.$1, (moonbit_string_t)moonbit_string_literal_20.data);
  } else {
    #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS484.$0->$method_0(_M0L6loggerS484.$1, (moonbit_string_t)moonbit_string_literal_104.data);
  }
  return 0;
}

int32_t _M0MPC16string6String8contains(
  moonbit_string_t _M0L4selfS481,
  struct _M0TPC16string10StringView _M0L3strS482
) {
  int32_t _M0L6_2atmpS1846;
  struct _M0TPC16string10StringView _M0L6_2atmpS1845;
  #line 536 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS1846 = Moonbit_array_length(_M0L4selfS481);
  _M0L6_2atmpS1845
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1846, _M0L4selfS481
  };
  #line 537 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  return _M0MPC16string10StringView8contains(_M0L6_2atmpS1845, _M0L3strS482);
}

int32_t _M0MPC16string10StringView8contains(
  struct _M0TPC16string10StringView _M0L4selfS479,
  struct _M0TPC16string10StringView _M0L3strS480
) {
  int64_t _M0L7_2abindS478;
  int32_t _M0L6_2atmpS1844;
  #line 530 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  #line 531 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L7_2abindS478
  = _M0MPC16string10StringView4find(_M0L4selfS479, _M0L3strS480);
  _M0L6_2atmpS1844 = _M0L7_2abindS478 == 4294967296ll;
  return !_M0L6_2atmpS1844;
}

int32_t _M0MPC15array5Array4pushGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS472,
  moonbit_string_t _M0L5valueS474
) {
  int32_t _M0L3lenS1834;
  moonbit_string_t* _M0L6_2atmpS1836;
  int32_t _M0L6_2atmpS3135;
  int32_t _M0L6_2atmpS1835;
  int32_t _M0L6lengthS473;
  moonbit_string_t* _M0L8_2afieldS3134;
  moonbit_string_t* _M0L3bufS1837;
  moonbit_string_t _M0L6_2aoldS3133;
  int32_t _M0L6_2atmpS1838;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1834 = _M0L4selfS472->$1;
  moonbit_incref(_M0L4selfS472);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1836 = _M0MPC15array5Array6bufferGsE(_M0L4selfS472);
  _M0L6_2atmpS3135 = Moonbit_array_length(_M0L6_2atmpS1836);
  moonbit_decref(_M0L6_2atmpS1836);
  _M0L6_2atmpS1835 = _M0L6_2atmpS3135;
  if (_M0L3lenS1834 == _M0L6_2atmpS1835) {
    moonbit_incref(_M0L4selfS472);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGsE(_M0L4selfS472);
  }
  _M0L6lengthS473 = _M0L4selfS472->$1;
  _M0L8_2afieldS3134 = _M0L4selfS472->$0;
  _M0L3bufS1837 = _M0L8_2afieldS3134;
  _M0L6_2aoldS3133 = (moonbit_string_t)_M0L3bufS1837[_M0L6lengthS473];
  moonbit_decref(_M0L6_2aoldS3133);
  _M0L3bufS1837[_M0L6lengthS473] = _M0L5valueS474;
  _M0L6_2atmpS1838 = _M0L6lengthS473 + 1;
  _M0L4selfS472->$1 = _M0L6_2atmpS1838;
  moonbit_decref(_M0L4selfS472);
  return 0;
}

int32_t _M0MPC15array5Array4pushGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS475,
  struct _M0TUsiE* _M0L5valueS477
) {
  int32_t _M0L3lenS1839;
  struct _M0TUsiE** _M0L6_2atmpS1841;
  int32_t _M0L6_2atmpS3138;
  int32_t _M0L6_2atmpS1840;
  int32_t _M0L6lengthS476;
  struct _M0TUsiE** _M0L8_2afieldS3137;
  struct _M0TUsiE** _M0L3bufS1842;
  struct _M0TUsiE* _M0L6_2aoldS3136;
  int32_t _M0L6_2atmpS1843;
  #line 242 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L3lenS1839 = _M0L4selfS475->$1;
  moonbit_incref(_M0L4selfS475);
  #line 243 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L6_2atmpS1841 = _M0MPC15array5Array6bufferGUsiEE(_M0L4selfS475);
  _M0L6_2atmpS3138 = Moonbit_array_length(_M0L6_2atmpS1841);
  moonbit_decref(_M0L6_2atmpS1841);
  _M0L6_2atmpS1840 = _M0L6_2atmpS3138;
  if (_M0L3lenS1839 == _M0L6_2atmpS1840) {
    moonbit_incref(_M0L4selfS475);
    #line 244 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    _M0MPC15array5Array7reallocGUsiEE(_M0L4selfS475);
  }
  _M0L6lengthS476 = _M0L4selfS475->$1;
  _M0L8_2afieldS3137 = _M0L4selfS475->$0;
  _M0L3bufS1842 = _M0L8_2afieldS3137;
  _M0L6_2aoldS3136 = (struct _M0TUsiE*)_M0L3bufS1842[_M0L6lengthS476];
  if (_M0L6_2aoldS3136) {
    moonbit_decref(_M0L6_2aoldS3136);
  }
  _M0L3bufS1842[_M0L6lengthS476] = _M0L5valueS477;
  _M0L6_2atmpS1843 = _M0L6lengthS476 + 1;
  _M0L4selfS475->$1 = _M0L6_2atmpS1843;
  moonbit_decref(_M0L4selfS475);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGsE(struct _M0TPB5ArrayGsE* _M0L4selfS467) {
  int32_t _M0L8old__capS466;
  int32_t _M0L8new__capS468;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS466 = _M0L4selfS467->$1;
  if (_M0L8old__capS466 == 0) {
    _M0L8new__capS468 = 8;
  } else {
    _M0L8new__capS468 = _M0L8old__capS466 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGsE(_M0L4selfS467, _M0L8new__capS468);
  return 0;
}

int32_t _M0MPC15array5Array7reallocGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS470
) {
  int32_t _M0L8old__capS469;
  int32_t _M0L8new__capS471;
  #line 182 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8old__capS469 = _M0L4selfS470->$1;
  if (_M0L8old__capS469 == 0) {
    _M0L8new__capS471 = 8;
  } else {
    _M0L8new__capS471 = _M0L8old__capS469 * 2;
  }
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPC15array5Array14resize__bufferGUsiEE(_M0L4selfS470, _M0L8new__capS471);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS457,
  int32_t _M0L13new__capacityS455
) {
  moonbit_string_t* _M0L8new__bufS454;
  moonbit_string_t* _M0L8_2afieldS3140;
  moonbit_string_t* _M0L8old__bufS456;
  int32_t _M0L8old__capS458;
  int32_t _M0L9copy__lenS459;
  moonbit_string_t* _M0L6_2aoldS3139;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS454
  = (moonbit_string_t*)moonbit_make_ref_array(_M0L13new__capacityS455, (moonbit_string_t)moonbit_string_literal_0.data);
  _M0L8_2afieldS3140 = _M0L4selfS457->$0;
  _M0L8old__bufS456 = _M0L8_2afieldS3140;
  _M0L8old__capS458 = Moonbit_array_length(_M0L8old__bufS456);
  if (_M0L8old__capS458 < _M0L13new__capacityS455) {
    _M0L9copy__lenS459 = _M0L8old__capS458;
  } else {
    _M0L9copy__lenS459 = _M0L13new__capacityS455;
  }
  moonbit_incref(_M0L8old__bufS456);
  moonbit_incref(_M0L8new__bufS454);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGsE(_M0L8new__bufS454, 0, _M0L8old__bufS456, 0, _M0L9copy__lenS459);
  _M0L6_2aoldS3139 = _M0L4selfS457->$0;
  moonbit_decref(_M0L6_2aoldS3139);
  _M0L4selfS457->$0 = _M0L8new__bufS454;
  moonbit_decref(_M0L4selfS457);
  return 0;
}

int32_t _M0MPC15array5Array14resize__bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS463,
  int32_t _M0L13new__capacityS461
) {
  struct _M0TUsiE** _M0L8new__bufS460;
  struct _M0TUsiE** _M0L8_2afieldS3142;
  struct _M0TUsiE** _M0L8old__bufS462;
  int32_t _M0L8old__capS464;
  int32_t _M0L9copy__lenS465;
  struct _M0TUsiE** _M0L6_2aoldS3141;
  #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8new__bufS460
  = (struct _M0TUsiE**)moonbit_make_ref_array(_M0L13new__capacityS461, 0);
  _M0L8_2afieldS3142 = _M0L4selfS463->$0;
  _M0L8old__bufS462 = _M0L8_2afieldS3142;
  _M0L8old__capS464 = Moonbit_array_length(_M0L8old__bufS462);
  if (_M0L8old__capS464 < _M0L13new__capacityS461) {
    _M0L9copy__lenS465 = _M0L8old__capS464;
  } else {
    _M0L9copy__lenS465 = _M0L13new__capacityS461;
  }
  moonbit_incref(_M0L8old__bufS462);
  moonbit_incref(_M0L8new__bufS460);
  #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0MPB18UninitializedArray12unsafe__blitGUsiEE(_M0L8new__bufS460, 0, _M0L8old__bufS462, 0, _M0L9copy__lenS465);
  _M0L6_2aoldS3141 = _M0L4selfS463->$0;
  moonbit_decref(_M0L6_2aoldS3141);
  _M0L4selfS463->$0 = _M0L8new__bufS460;
  moonbit_decref(_M0L4selfS463);
  return 0;
}

struct _M0TPB5ArrayGsE* _M0MPC15array5Array11new_2einnerGsE(
  int32_t _M0L8capacityS453
) {
  #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  if (_M0L8capacityS453 == 0) {
    moonbit_string_t* _M0L6_2atmpS1832 =
      (moonbit_string_t*)moonbit_empty_ref_array;
    struct _M0TPB5ArrayGsE* _block_3529 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3529)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3529->$0 = _M0L6_2atmpS1832;
    _block_3529->$1 = 0;
    return _block_3529;
  } else {
    moonbit_string_t* _M0L6_2atmpS1833 =
      (moonbit_string_t*)moonbit_make_ref_array(_M0L8capacityS453, (moonbit_string_t)moonbit_string_literal_0.data);
    struct _M0TPB5ArrayGsE* _block_3530 =
      (struct _M0TPB5ArrayGsE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGsE));
    Moonbit_object_header(_block_3530)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGsE, $0) >> 2, 1, 0);
    _block_3530->$0 = _M0L6_2atmpS1833;
    _block_3530->$1 = 0;
    return _block_3530;
  }
}

int64_t _M0MPC16string10StringView4find(
  struct _M0TPC16string10StringView _M0L4selfS452,
  struct _M0TPC16string10StringView _M0L3strS451
) {
  int32_t _M0L6_2atmpS1831;
  #line 18 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L3strS451.$0);
  #line 19 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L6_2atmpS1831 = _M0MPC16string10StringView6length(_M0L3strS451);
  if (_M0L6_2atmpS1831 <= 4) {
    #line 20 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0FPB18brute__force__find(_M0L4selfS452, _M0L3strS451);
  } else {
    #line 22 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
    return _M0FPB28boyer__moore__horspool__find(_M0L4selfS452, _M0L3strS451);
  }
}

int64_t _M0FPB18brute__force__find(
  struct _M0TPC16string10StringView _M0L8haystackS441,
  struct _M0TPC16string10StringView _M0L6needleS443
) {
  int32_t _M0L13haystack__lenS440;
  int32_t _M0L11needle__lenS442;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L8haystackS441.$0);
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L13haystack__lenS440
  = _M0MPC16string10StringView6length(_M0L8haystackS441);
  moonbit_incref(_M0L6needleS443.$0);
  #line 33 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L11needle__lenS442 = _M0MPC16string10StringView6length(_M0L6needleS443);
  if (_M0L11needle__lenS442 > 0) {
    if (_M0L13haystack__lenS440 >= _M0L11needle__lenS442) {
      int32_t _M0L13needle__firstS444;
      int32_t _M0L12forward__lenS445;
      int32_t _M0Lm1iS446;
      moonbit_incref(_M0L6needleS443.$0);
      #line 36 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
      _M0L13needle__firstS444
      = _M0MPC16string10StringView11unsafe__get(_M0L6needleS443, 0);
      _M0L12forward__lenS445
      = _M0L13haystack__lenS440 - _M0L11needle__lenS442;
      _M0Lm1iS446 = 0;
      while (1) {
        int32_t _M0L6_2atmpS1818 = _M0Lm1iS446;
        if (_M0L6_2atmpS1818 <= _M0L12forward__lenS445) {
          int32_t _M0L6_2atmpS1823;
          while (1) {
            int32_t _M0L6_2atmpS1821 = _M0Lm1iS446;
            int32_t _if__result_3533;
            if (_M0L6_2atmpS1821 <= _M0L12forward__lenS445) {
              int32_t _M0L6_2atmpS1820 = _M0Lm1iS446;
              int32_t _M0L6_2atmpS1819;
              moonbit_incref(_M0L8haystackS441.$0);
              #line 41 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
              _M0L6_2atmpS1819
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS441, _M0L6_2atmpS1820);
              _if__result_3533 = _M0L6_2atmpS1819 != _M0L13needle__firstS444;
            } else {
              _if__result_3533 = 0;
            }
            if (_if__result_3533) {
              int32_t _M0L6_2atmpS1822 = _M0Lm1iS446;
              _M0Lm1iS446 = _M0L6_2atmpS1822 + 1;
              continue;
            }
            break;
          }
          _M0L6_2atmpS1823 = _M0Lm1iS446;
          if (_M0L6_2atmpS1823 <= _M0L12forward__lenS445) {
            int32_t _M0L1jS448 = 1;
            int32_t _M0L6_2atmpS1830;
            while (1) {
              if (_M0L1jS448 < _M0L11needle__lenS442) {
                int32_t _M0L6_2atmpS1827 = _M0Lm1iS446;
                int32_t _M0L6_2atmpS1826 = _M0L6_2atmpS1827 + _M0L1jS448;
                int32_t _M0L6_2atmpS1824;
                int32_t _M0L6_2atmpS1825;
                int32_t _M0L6_2atmpS1828;
                moonbit_incref(_M0L8haystackS441.$0);
                #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
                _M0L6_2atmpS1824
                = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS441, _M0L6_2atmpS1826);
                moonbit_incref(_M0L6needleS443.$0);
                #line 47 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
                _M0L6_2atmpS1825
                = _M0MPC16string10StringView11unsafe__get(_M0L6needleS443, _M0L1jS448);
                if (_M0L6_2atmpS1824 != _M0L6_2atmpS1825) {
                  break;
                }
                _M0L6_2atmpS1828 = _M0L1jS448 + 1;
                _M0L1jS448 = _M0L6_2atmpS1828;
                continue;
              } else {
                int32_t _M0L6_2atmpS1829;
                moonbit_decref(_M0L6needleS443.$0);
                moonbit_decref(_M0L8haystackS441.$0);
                _M0L6_2atmpS1829 = _M0Lm1iS446;
                return (int64_t)_M0L6_2atmpS1829;
              }
              break;
            }
            _M0L6_2atmpS1830 = _M0Lm1iS446;
            _M0Lm1iS446 = _M0L6_2atmpS1830 + 1;
          }
          continue;
        } else {
          moonbit_decref(_M0L6needleS443.$0);
          moonbit_decref(_M0L8haystackS441.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS443.$0);
      moonbit_decref(_M0L8haystackS441.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS443.$0);
    moonbit_decref(_M0L8haystackS441.$0);
    return _M0FPB33brute__force__find_2econstr_2f439;
  }
}

int64_t _M0FPB28boyer__moore__horspool__find(
  struct _M0TPC16string10StringView _M0L8haystackS427,
  struct _M0TPC16string10StringView _M0L6needleS429
) {
  int32_t _M0L13haystack__lenS426;
  int32_t _M0L11needle__lenS428;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  moonbit_incref(_M0L8haystackS427.$0);
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L13haystack__lenS426
  = _M0MPC16string10StringView6length(_M0L8haystackS427);
  moonbit_incref(_M0L6needleS429.$0);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
  _M0L11needle__lenS428 = _M0MPC16string10StringView6length(_M0L6needleS429);
  if (_M0L11needle__lenS428 > 0) {
    if (_M0L13haystack__lenS426 >= _M0L11needle__lenS428) {
      int32_t* _M0L11skip__tableS430 =
        (int32_t*)moonbit_make_int32_array(256, _M0L11needle__lenS428);
      int32_t _M0L7_2abindS431 = _M0L11needle__lenS428 - 1;
      int32_t _M0L1iS432 = 0;
      int32_t _M0L1iS434;
      while (1) {
        if (_M0L1iS432 < _M0L7_2abindS431) {
          int32_t _M0L6_2atmpS1804;
          int32_t _M0L6_2atmpS1803;
          int32_t _M0L6_2atmpS1800;
          int32_t _M0L6_2atmpS1802;
          int32_t _M0L6_2atmpS1801;
          int32_t _M0L6_2atmpS1805;
          moonbit_incref(_M0L6needleS429.$0);
          #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS1804
          = _M0MPC16string10StringView11unsafe__get(_M0L6needleS429, _M0L1iS432);
          _M0L6_2atmpS1803 = (int32_t)_M0L6_2atmpS1804;
          _M0L6_2atmpS1800 = _M0L6_2atmpS1803 & 255;
          _M0L6_2atmpS1802 = _M0L11needle__lenS428 - 1;
          _M0L6_2atmpS1801 = _M0L6_2atmpS1802 - _M0L1iS432;
          if (
            _M0L6_2atmpS1800 < 0
            || _M0L6_2atmpS1800
               >= Moonbit_array_length(_M0L11skip__tableS430)
          ) {
            #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
            moonbit_panic();
          }
          _M0L11skip__tableS430[_M0L6_2atmpS1800] = _M0L6_2atmpS1801;
          _M0L6_2atmpS1805 = _M0L1iS432 + 1;
          _M0L1iS432 = _M0L6_2atmpS1805;
          continue;
        }
        break;
      }
      _M0L1iS434 = 0;
      while (1) {
        int32_t _M0L6_2atmpS1806 =
          _M0L13haystack__lenS426 - _M0L11needle__lenS428;
        if (_M0L1iS434 <= _M0L6_2atmpS1806) {
          int32_t _M0L7_2abindS435 = _M0L11needle__lenS428 - 1;
          int32_t _M0L1jS436 = 0;
          int32_t _M0L6_2atmpS1817;
          int32_t _M0L6_2atmpS1816;
          int32_t _M0L6_2atmpS1815;
          int32_t _M0L6_2atmpS1814;
          int32_t _M0L6_2atmpS1813;
          int32_t _M0L6_2atmpS1812;
          int32_t _M0L6_2atmpS1811;
          while (1) {
            if (_M0L1jS436 <= _M0L7_2abindS435) {
              int32_t _M0L6_2atmpS1809 = _M0L1iS434 + _M0L1jS436;
              int32_t _M0L6_2atmpS1807;
              int32_t _M0L6_2atmpS1808;
              int32_t _M0L6_2atmpS1810;
              moonbit_incref(_M0L8haystackS427.$0);
              #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
              _M0L6_2atmpS1807
              = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS427, _M0L6_2atmpS1809);
              moonbit_incref(_M0L6needleS429.$0);
              #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
              _M0L6_2atmpS1808
              = _M0MPC16string10StringView11unsafe__get(_M0L6needleS429, _M0L1jS436);
              if (_M0L6_2atmpS1807 != _M0L6_2atmpS1808) {
                break;
              }
              _M0L6_2atmpS1810 = _M0L1jS436 + 1;
              _M0L1jS436 = _M0L6_2atmpS1810;
              continue;
            } else {
              moonbit_decref(_M0L11skip__tableS430);
              moonbit_decref(_M0L6needleS429.$0);
              moonbit_decref(_M0L8haystackS427.$0);
              return (int64_t)_M0L1iS434;
            }
            break;
          }
          _M0L6_2atmpS1817 = _M0L1iS434 + _M0L11needle__lenS428;
          _M0L6_2atmpS1816 = _M0L6_2atmpS1817 - 1;
          moonbit_incref(_M0L8haystackS427.$0);
          #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
          _M0L6_2atmpS1815
          = _M0MPC16string10StringView11unsafe__get(_M0L8haystackS427, _M0L6_2atmpS1816);
          _M0L6_2atmpS1814 = (int32_t)_M0L6_2atmpS1815;
          _M0L6_2atmpS1813 = _M0L6_2atmpS1814 & 255;
          if (
            _M0L6_2atmpS1813 < 0
            || _M0L6_2atmpS1813
               >= Moonbit_array_length(_M0L11skip__tableS430)
          ) {
            #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string_methods.mbt"
            moonbit_panic();
          }
          _M0L6_2atmpS1812 = (int32_t)_M0L11skip__tableS430[_M0L6_2atmpS1813];
          _M0L6_2atmpS1811 = _M0L1iS434 + _M0L6_2atmpS1812;
          _M0L1iS434 = _M0L6_2atmpS1811;
          continue;
        } else {
          moonbit_decref(_M0L11skip__tableS430);
          moonbit_decref(_M0L6needleS429.$0);
          moonbit_decref(_M0L8haystackS427.$0);
        }
        break;
      }
      return 4294967296ll;
    } else {
      moonbit_decref(_M0L6needleS429.$0);
      moonbit_decref(_M0L8haystackS427.$0);
      return 4294967296ll;
    }
  } else {
    moonbit_decref(_M0L6needleS429.$0);
    moonbit_decref(_M0L8haystackS427.$0);
    return _M0FPB43boyer__moore__horspool__find_2econstr_2f425;
  }
}

int32_t _M0IPB13StringBuilderPB6Logger11write__view(
  struct _M0TPB13StringBuilder* _M0L4selfS423,
  struct _M0TPC16string10StringView _M0L3strS424
) {
  int32_t _M0L3lenS1788;
  int32_t _M0L6_2atmpS1790;
  int32_t _M0L6_2atmpS1789;
  int32_t _M0L6_2atmpS1787;
  moonbit_bytes_t _M0L8_2afieldS3143;
  moonbit_bytes_t _M0L4dataS1791;
  int32_t _M0L3lenS1792;
  moonbit_string_t _M0L6_2atmpS1793;
  int32_t _M0L6_2atmpS1794;
  int32_t _M0L6_2atmpS1795;
  int32_t _M0L3lenS1797;
  int32_t _M0L6_2atmpS1799;
  int32_t _M0L6_2atmpS1798;
  int32_t _M0L6_2atmpS1796;
  #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1788 = _M0L4selfS423->$1;
  moonbit_incref(_M0L3strS424.$0);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1790 = _M0MPC16string10StringView6length(_M0L3strS424);
  _M0L6_2atmpS1789 = _M0L6_2atmpS1790 * 2;
  _M0L6_2atmpS1787 = _M0L3lenS1788 + _M0L6_2atmpS1789;
  moonbit_incref(_M0L4selfS423);
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS423, _M0L6_2atmpS1787);
  _M0L8_2afieldS3143 = _M0L4selfS423->$0;
  _M0L4dataS1791 = _M0L8_2afieldS3143;
  _M0L3lenS1792 = _M0L4selfS423->$1;
  moonbit_incref(_M0L4dataS1791);
  moonbit_incref(_M0L3strS424.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1793 = _M0MPC16string10StringView4data(_M0L3strS424);
  moonbit_incref(_M0L3strS424.$0);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1794 = _M0MPC16string10StringView13start__offset(_M0L3strS424);
  moonbit_incref(_M0L3strS424.$0);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1795 = _M0MPC16string10StringView6length(_M0L3strS424);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1791, _M0L3lenS1792, _M0L6_2atmpS1793, _M0L6_2atmpS1794, _M0L6_2atmpS1795);
  _M0L3lenS1797 = _M0L4selfS423->$1;
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1799 = _M0MPC16string10StringView6length(_M0L3strS424);
  _M0L6_2atmpS1798 = _M0L6_2atmpS1799 * 2;
  _M0L6_2atmpS1796 = _M0L3lenS1797 + _M0L6_2atmpS1798;
  _M0L4selfS423->$1 = _M0L6_2atmpS1796;
  moonbit_decref(_M0L4selfS423);
  return 0;
}

int32_t _M0MPC16string6String24char__length__ge_2einner(
  moonbit_string_t _M0L4selfS415,
  int32_t _M0L3lenS418,
  int32_t _M0L13start__offsetS422,
  int64_t _M0L11end__offsetS413
) {
  int32_t _M0L11end__offsetS412;
  int32_t _M0L5indexS416;
  int32_t _M0L5countS417;
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS413 == 4294967296ll) {
    _M0L11end__offsetS412 = Moonbit_array_length(_M0L4selfS415);
  } else {
    int64_t _M0L7_2aSomeS414 = _M0L11end__offsetS413;
    _M0L11end__offsetS412 = (int32_t)_M0L7_2aSomeS414;
  }
  _M0L5indexS416 = _M0L13start__offsetS422;
  _M0L5countS417 = 0;
  while (1) {
    int32_t _if__result_3539;
    if (_M0L5indexS416 < _M0L11end__offsetS412) {
      _if__result_3539 = _M0L5countS417 < _M0L3lenS418;
    } else {
      _if__result_3539 = 0;
    }
    if (_if__result_3539) {
      int32_t _M0L2c1S419 = _M0L4selfS415[_M0L5indexS416];
      int32_t _if__result_3540;
      int32_t _M0L6_2atmpS1785;
      int32_t _M0L6_2atmpS1786;
      #line 449 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
      if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S419)) {
        int32_t _M0L6_2atmpS1781 = _M0L5indexS416 + 1;
        _if__result_3540 = _M0L6_2atmpS1781 < _M0L11end__offsetS412;
      } else {
        _if__result_3540 = 0;
      }
      if (_if__result_3540) {
        int32_t _M0L6_2atmpS1784 = _M0L5indexS416 + 1;
        int32_t _M0L2c2S420 = _M0L4selfS415[_M0L6_2atmpS1784];
        #line 451 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S420)) {
          int32_t _M0L6_2atmpS1782 = _M0L5indexS416 + 2;
          int32_t _M0L6_2atmpS1783 = _M0L5countS417 + 1;
          _M0L5indexS416 = _M0L6_2atmpS1782;
          _M0L5countS417 = _M0L6_2atmpS1783;
          continue;
        } else {
          #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_148.data, (moonbit_string_t)moonbit_string_literal_149.data);
        }
      }
      _M0L6_2atmpS1785 = _M0L5indexS416 + 1;
      _M0L6_2atmpS1786 = _M0L5countS417 + 1;
      _M0L5indexS416 = _M0L6_2atmpS1785;
      _M0L5countS417 = _M0L6_2atmpS1786;
      continue;
    } else {
      moonbit_decref(_M0L4selfS415);
      return _M0L5countS417 >= _M0L3lenS418;
    }
    break;
  }
}

int32_t _M0MPC15array9ArrayView6lengthGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE(
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L4selfS406
) {
  int32_t _M0L3endS1769;
  int32_t _M0L8_2afieldS3144;
  int32_t _M0L5startS1770;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1769 = _M0L4selfS406.$2;
  _M0L8_2afieldS3144 = _M0L4selfS406.$1;
  moonbit_decref(_M0L4selfS406.$0);
  _M0L5startS1770 = _M0L8_2afieldS3144;
  return _M0L3endS1769 - _M0L5startS1770;
}

int32_t _M0MPC15array9ArrayView6lengthGUiUWEuQRPC15error5ErrorNsEEE(
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L4selfS407
) {
  int32_t _M0L3endS1771;
  int32_t _M0L8_2afieldS3145;
  int32_t _M0L5startS1772;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1771 = _M0L4selfS407.$2;
  _M0L8_2afieldS3145 = _M0L4selfS407.$1;
  moonbit_decref(_M0L4selfS407.$0);
  _M0L5startS1772 = _M0L8_2afieldS3145;
  return _M0L3endS1771 - _M0L5startS1772;
}

int32_t _M0MPC15array9ArrayView6lengthGsE(
  struct _M0TPB9ArrayViewGsE _M0L4selfS408
) {
  int32_t _M0L3endS1773;
  int32_t _M0L8_2afieldS3146;
  int32_t _M0L5startS1774;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1773 = _M0L4selfS408.$2;
  _M0L8_2afieldS3146 = _M0L4selfS408.$1;
  moonbit_decref(_M0L4selfS408.$0);
  _M0L5startS1774 = _M0L8_2afieldS3146;
  return _M0L3endS1773 - _M0L5startS1774;
}

int32_t _M0MPC15array9ArrayView6lengthGUssEE(
  struct _M0TPB9ArrayViewGUssEE _M0L4selfS409
) {
  int32_t _M0L3endS1775;
  int32_t _M0L8_2afieldS3147;
  int32_t _M0L5startS1776;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1775 = _M0L4selfS409.$2;
  _M0L8_2afieldS3147 = _M0L4selfS409.$1;
  moonbit_decref(_M0L4selfS409.$0);
  _M0L5startS1776 = _M0L8_2afieldS3147;
  return _M0L3endS1775 - _M0L5startS1776;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRP38clawteam8clawteam6config13CliToolConfigEE(
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13CliToolConfigEE _M0L4selfS410
) {
  int32_t _M0L3endS1777;
  int32_t _M0L8_2afieldS3148;
  int32_t _M0L5startS1778;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1777 = _M0L4selfS410.$2;
  _M0L8_2afieldS3148 = _M0L4selfS410.$1;
  moonbit_decref(_M0L4selfS410.$0);
  _M0L5startS1778 = _M0L8_2afieldS3148;
  return _M0L3endS1777 - _M0L5startS1778;
}

int32_t _M0MPC15array9ArrayView6lengthGUsRP38clawteam8clawteam6config13ChannelConfigEE(
  struct _M0TPB9ArrayViewGUsRP38clawteam8clawteam6config13ChannelConfigEE _M0L4selfS411
) {
  int32_t _M0L3endS1779;
  int32_t _M0L8_2afieldS3149;
  int32_t _M0L5startS1780;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arrayview.mbt"
  _M0L3endS1779 = _M0L4selfS411.$2;
  _M0L8_2afieldS3149 = _M0L4selfS411.$1;
  moonbit_decref(_M0L4selfS411.$0);
  _M0L5startS1780 = _M0L8_2afieldS3149;
  return _M0L3endS1779 - _M0L5startS1780;
}

struct _M0TPC16string10StringView _M0MPC16string6String4view(
  moonbit_string_t _M0L4selfS404,
  int64_t _M0L19start__offset_2eoptS402,
  int64_t _M0L11end__offsetS405
) {
  int32_t _M0L13start__offsetS401;
  if (_M0L19start__offset_2eoptS402 == 4294967296ll) {
    _M0L13start__offsetS401 = 0;
  } else {
    int64_t _M0L7_2aSomeS403 = _M0L19start__offset_2eoptS402;
    _M0L13start__offsetS401 = (int32_t)_M0L7_2aSomeS403;
  }
  return _M0MPC16string6String12view_2einner(_M0L4selfS404, _M0L13start__offsetS401, _M0L11end__offsetS405);
}

struct _M0TPC16string10StringView _M0MPC16string6String12view_2einner(
  moonbit_string_t _M0L4selfS399,
  int32_t _M0L13start__offsetS400,
  int64_t _M0L11end__offsetS397
) {
  int32_t _M0L11end__offsetS396;
  int32_t _if__result_3541;
  #line 390 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  if (_M0L11end__offsetS397 == 4294967296ll) {
    _M0L11end__offsetS396 = Moonbit_array_length(_M0L4selfS399);
  } else {
    int64_t _M0L7_2aSomeS398 = _M0L11end__offsetS397;
    _M0L11end__offsetS396 = (int32_t)_M0L7_2aSomeS398;
  }
  if (_M0L13start__offsetS400 >= 0) {
    if (_M0L13start__offsetS400 <= _M0L11end__offsetS396) {
      int32_t _M0L6_2atmpS1768 = Moonbit_array_length(_M0L4selfS399);
      _if__result_3541 = _M0L11end__offsetS396 <= _M0L6_2atmpS1768;
    } else {
      _if__result_3541 = 0;
    }
  } else {
    _if__result_3541 = 0;
  }
  if (_if__result_3541) {
    return (struct _M0TPC16string10StringView){_M0L13start__offsetS400,
                                                 _M0L11end__offsetS396,
                                                 _M0L4selfS399};
  } else {
    moonbit_decref(_M0L4selfS399);
    #line 399 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    return _M0FPB5abortGRPC16string10StringViewE((moonbit_string_t)moonbit_string_literal_150.data, (moonbit_string_t)moonbit_string_literal_151.data);
  }
}

moonbit_string_t _M0IPC16string10StringViewPB4Show10to__string(
  struct _M0TPC16string10StringView _M0L4selfS395
) {
  moonbit_string_t _M0L8_2afieldS3151;
  moonbit_string_t _M0L3strS1765;
  int32_t _M0L5startS1766;
  int32_t _M0L8_2afieldS3150;
  int32_t _M0L3endS1767;
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3151 = _M0L4selfS395.$0;
  _M0L3strS1765 = _M0L8_2afieldS3151;
  _M0L5startS1766 = _M0L4selfS395.$1;
  _M0L8_2afieldS3150 = _M0L4selfS395.$2;
  _M0L3endS1767 = _M0L8_2afieldS3150;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  return _M0MPC16string6String17unsafe__substring(_M0L3strS1765, _M0L5startS1766, _M0L3endS1767);
}

int32_t _M0IPC16string10StringViewPB4Show6output(
  struct _M0TPC16string10StringView _M0L4selfS393,
  struct _M0TPB6Logger _M0L6loggerS394
) {
  moonbit_string_t _M0L8_2afieldS3153;
  moonbit_string_t _M0L3strS1762;
  int32_t _M0L5startS1763;
  int32_t _M0L8_2afieldS3152;
  int32_t _M0L3endS1764;
  moonbit_string_t _M0L6substrS392;
  #line 166 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3153 = _M0L4selfS393.$0;
  _M0L3strS1762 = _M0L8_2afieldS3153;
  _M0L5startS1763 = _M0L4selfS393.$1;
  _M0L8_2afieldS3152 = _M0L4selfS393.$2;
  _M0L3endS1764 = _M0L8_2afieldS3152;
  #line 167 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L6substrS392
  = _M0MPC16string6String17unsafe__substring(_M0L3strS1762, _M0L5startS1763, _M0L3endS1764);
  #line 168 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L6substrS392, _M0L6loggerS394);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6output(
  moonbit_string_t _M0L4selfS384,
  struct _M0TPB6Logger _M0L6loggerS382
) {
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS383;
  int32_t _M0L3lenS385;
  int32_t _M0L1iS386;
  int32_t _M0L3segS387;
  #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L6loggerS382.$1) {
    moonbit_incref(_M0L6loggerS382.$1);
  }
  #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS382.$0->$method_3(_M0L6loggerS382.$1, 34);
  moonbit_incref(_M0L4selfS384);
  if (_M0L6loggerS382.$1) {
    moonbit_incref(_M0L6loggerS382.$1);
  }
  _M0L6_2aenvS383
  = (struct _M0TUsRPB6LoggerE*)moonbit_malloc(sizeof(struct _M0TUsRPB6LoggerE));
  Moonbit_object_header(_M0L6_2aenvS383)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB6LoggerE, $0) >> 2, 3, 0);
  _M0L6_2aenvS383->$0 = _M0L4selfS384;
  _M0L6_2aenvS383->$1_0 = _M0L6loggerS382.$0;
  _M0L6_2aenvS383->$1_1 = _M0L6loggerS382.$1;
  _M0L3lenS385 = Moonbit_array_length(_M0L4selfS384);
  _M0L1iS386 = 0;
  _M0L3segS387 = 0;
  _2afor_388:;
  while (1) {
    int32_t _M0L4codeS389;
    int32_t _M0L1cS391;
    int32_t _M0L6_2atmpS1746;
    int32_t _M0L6_2atmpS1747;
    int32_t _M0L6_2atmpS1748;
    int32_t _tmp_3545;
    int32_t _tmp_3546;
    if (_M0L1iS386 >= _M0L3lenS385) {
      moonbit_decref(_M0L4selfS384);
      #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
      _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS383, _M0L3segS387, _M0L1iS386);
      break;
    }
    _M0L4codeS389 = _M0L4selfS384[_M0L1iS386];
    switch (_M0L4codeS389) {
      case 34: {
        _M0L1cS391 = _M0L4codeS389;
        goto join_390;
        break;
      }
      
      case 92: {
        _M0L1cS391 = _M0L4codeS389;
        goto join_390;
        break;
      }
      
      case 10: {
        int32_t _M0L6_2atmpS1749;
        int32_t _M0L6_2atmpS1750;
        moonbit_incref(_M0L6_2aenvS383);
        #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS383, _M0L3segS387, _M0L1iS386);
        if (_M0L6loggerS382.$1) {
          moonbit_incref(_M0L6loggerS382.$1);
        }
        #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS382.$0->$method_0(_M0L6loggerS382.$1, (moonbit_string_t)moonbit_string_literal_152.data);
        _M0L6_2atmpS1749 = _M0L1iS386 + 1;
        _M0L6_2atmpS1750 = _M0L1iS386 + 1;
        _M0L1iS386 = _M0L6_2atmpS1749;
        _M0L3segS387 = _M0L6_2atmpS1750;
        goto _2afor_388;
        break;
      }
      
      case 13: {
        int32_t _M0L6_2atmpS1751;
        int32_t _M0L6_2atmpS1752;
        moonbit_incref(_M0L6_2aenvS383);
        #line 123 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS383, _M0L3segS387, _M0L1iS386);
        if (_M0L6loggerS382.$1) {
          moonbit_incref(_M0L6loggerS382.$1);
        }
        #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS382.$0->$method_0(_M0L6loggerS382.$1, (moonbit_string_t)moonbit_string_literal_153.data);
        _M0L6_2atmpS1751 = _M0L1iS386 + 1;
        _M0L6_2atmpS1752 = _M0L1iS386 + 1;
        _M0L1iS386 = _M0L6_2atmpS1751;
        _M0L3segS387 = _M0L6_2atmpS1752;
        goto _2afor_388;
        break;
      }
      
      case 8: {
        int32_t _M0L6_2atmpS1753;
        int32_t _M0L6_2atmpS1754;
        moonbit_incref(_M0L6_2aenvS383);
        #line 128 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS383, _M0L3segS387, _M0L1iS386);
        if (_M0L6loggerS382.$1) {
          moonbit_incref(_M0L6loggerS382.$1);
        }
        #line 129 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS382.$0->$method_0(_M0L6loggerS382.$1, (moonbit_string_t)moonbit_string_literal_154.data);
        _M0L6_2atmpS1753 = _M0L1iS386 + 1;
        _M0L6_2atmpS1754 = _M0L1iS386 + 1;
        _M0L1iS386 = _M0L6_2atmpS1753;
        _M0L3segS387 = _M0L6_2atmpS1754;
        goto _2afor_388;
        break;
      }
      
      case 9: {
        int32_t _M0L6_2atmpS1755;
        int32_t _M0L6_2atmpS1756;
        moonbit_incref(_M0L6_2aenvS383);
        #line 133 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS383, _M0L3segS387, _M0L1iS386);
        if (_M0L6loggerS382.$1) {
          moonbit_incref(_M0L6loggerS382.$1);
        }
        #line 134 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
        _M0L6loggerS382.$0->$method_0(_M0L6loggerS382.$1, (moonbit_string_t)moonbit_string_literal_155.data);
        _M0L6_2atmpS1755 = _M0L1iS386 + 1;
        _M0L6_2atmpS1756 = _M0L1iS386 + 1;
        _M0L1iS386 = _M0L6_2atmpS1755;
        _M0L3segS387 = _M0L6_2atmpS1756;
        goto _2afor_388;
        break;
      }
      default: {
        if (_M0L4codeS389 < 32) {
          int32_t _M0L6_2atmpS1758;
          moonbit_string_t _M0L6_2atmpS1757;
          int32_t _M0L6_2atmpS1759;
          int32_t _M0L6_2atmpS1760;
          moonbit_incref(_M0L6_2aenvS383);
          #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS383, _M0L3segS387, _M0L1iS386);
          if (_M0L6loggerS382.$1) {
            moonbit_incref(_M0L6loggerS382.$1);
          }
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS382.$0->$method_0(_M0L6loggerS382.$1, (moonbit_string_t)moonbit_string_literal_156.data);
          _M0L6_2atmpS1758 = _M0L4codeS389 & 0xff;
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6_2atmpS1757 = _M0MPC14byte4Byte7to__hex(_M0L6_2atmpS1758);
          if (_M0L6loggerS382.$1) {
            moonbit_incref(_M0L6loggerS382.$1);
          }
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS382.$0->$method_0(_M0L6loggerS382.$1, _M0L6_2atmpS1757);
          if (_M0L6loggerS382.$1) {
            moonbit_incref(_M0L6loggerS382.$1);
          }
          #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
          _M0L6loggerS382.$0->$method_3(_M0L6loggerS382.$1, 125);
          _M0L6_2atmpS1759 = _M0L1iS386 + 1;
          _M0L6_2atmpS1760 = _M0L1iS386 + 1;
          _M0L1iS386 = _M0L6_2atmpS1759;
          _M0L3segS387 = _M0L6_2atmpS1760;
          goto _2afor_388;
        } else {
          int32_t _M0L6_2atmpS1761 = _M0L1iS386 + 1;
          int32_t _tmp_3544 = _M0L3segS387;
          _M0L1iS386 = _M0L6_2atmpS1761;
          _M0L3segS387 = _tmp_3544;
          goto _2afor_388;
        }
        break;
      }
    }
    goto joinlet_3543;
    join_390:;
    moonbit_incref(_M0L6_2aenvS383);
    #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(_M0L6_2aenvS383, _M0L3segS387, _M0L1iS386);
    if (_M0L6loggerS382.$1) {
      moonbit_incref(_M0L6loggerS382.$1);
    }
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS382.$0->$method_3(_M0L6loggerS382.$1, 92);
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1746 = _M0MPC16uint166UInt1616unsafe__to__char(_M0L1cS391);
    if (_M0L6loggerS382.$1) {
      moonbit_incref(_M0L6loggerS382.$1);
    }
    #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS382.$0->$method_3(_M0L6loggerS382.$1, _M0L6_2atmpS1746);
    _M0L6_2atmpS1747 = _M0L1iS386 + 1;
    _M0L6_2atmpS1748 = _M0L1iS386 + 1;
    _M0L1iS386 = _M0L6_2atmpS1747;
    _M0L3segS387 = _M0L6_2atmpS1748;
    continue;
    joinlet_3543:;
    _tmp_3545 = _M0L1iS386;
    _tmp_3546 = _M0L3segS387;
    _M0L1iS386 = _tmp_3545;
    _M0L3segS387 = _tmp_3546;
    continue;
    break;
  }
  #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6loggerS382.$0->$method_3(_M0L6loggerS382.$1, 34);
  return 0;
}

int32_t _M0IPC16string6StringPB4Show6outputN14flush__segmentS3534(
  struct _M0TUsRPB6LoggerE* _M0L6_2aenvS378,
  int32_t _M0L3segS381,
  int32_t _M0L1iS380
) {
  struct _M0TPB6Logger _M0L8_2afieldS3155;
  struct _M0TPB6Logger _M0L6loggerS377;
  moonbit_string_t _M0L8_2afieldS3154;
  int32_t _M0L6_2acntS3368;
  moonbit_string_t _M0L4selfS379;
  #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L8_2afieldS3155
  = (struct _M0TPB6Logger){
    _M0L6_2aenvS378->$1_0, _M0L6_2aenvS378->$1_1
  };
  _M0L6loggerS377 = _M0L8_2afieldS3155;
  _M0L8_2afieldS3154 = _M0L6_2aenvS378->$0;
  _M0L6_2acntS3368 = Moonbit_object_header(_M0L6_2aenvS378)->rc;
  if (_M0L6_2acntS3368 > 1) {
    int32_t _M0L11_2anew__cntS3369 = _M0L6_2acntS3368 - 1;
    Moonbit_object_header(_M0L6_2aenvS378)->rc = _M0L11_2anew__cntS3369;
    if (_M0L6loggerS377.$1) {
      moonbit_incref(_M0L6loggerS377.$1);
    }
    moonbit_incref(_M0L8_2afieldS3154);
  } else if (_M0L6_2acntS3368 == 1) {
    #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    moonbit_free(_M0L6_2aenvS378);
  }
  _M0L4selfS379 = _M0L8_2afieldS3154;
  if (_M0L1iS380 > _M0L3segS381) {
    int32_t _M0L6_2atmpS1745 = _M0L1iS380 - _M0L3segS381;
    #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6loggerS377.$0->$method_1(_M0L6loggerS377.$1, _M0L4selfS379, _M0L3segS381, _M0L6_2atmpS1745);
  } else {
    moonbit_decref(_M0L4selfS379);
    if (_M0L6loggerS377.$1) {
      moonbit_decref(_M0L6loggerS377.$1);
    }
  }
  return 0;
}

moonbit_string_t _M0MPC14byte4Byte7to__hex(int32_t _M0L1bS376) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS375;
  int32_t _M0L6_2atmpS1742;
  int32_t _M0L6_2atmpS1741;
  int32_t _M0L6_2atmpS1744;
  int32_t _M0L6_2atmpS1743;
  struct _M0TPB13StringBuilder* _M0L6_2atmpS1740;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L7_2aselfS375 = _M0MPB13StringBuilder11new_2einner(0);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1742 = _M0IPC14byte4BytePB3Div3div(_M0L1bS376, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1741
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1742);
  moonbit_incref(_M0L7_2aselfS375);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS375, _M0L6_2atmpS1741);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1744 = _M0IPC14byte4BytePB3Mod3mod(_M0L1bS376, 16);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L6_2atmpS1743
  = _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(_M0L6_2atmpS1744);
  moonbit_incref(_M0L7_2aselfS375);
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS375, _M0L6_2atmpS1743);
  _M0L6_2atmpS1740 = _M0L7_2aselfS375;
  #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6_2atmpS1740);
}

int32_t _M0MPC14byte4Byte7to__hexN14to__hex__digitS3544(int32_t _M0L1iS374) {
  #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  if (_M0L1iS374 < 10) {
    int32_t _M0L6_2atmpS1737;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1737 = _M0IPC14byte4BytePB3Add3add(_M0L1iS374, 48);
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1737);
  } else {
    int32_t _M0L6_2atmpS1739;
    int32_t _M0L6_2atmpS1738;
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1739 = _M0IPC14byte4BytePB3Add3add(_M0L1iS374, 97);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    _M0L6_2atmpS1738 = _M0IPC14byte4BytePB3Sub3sub(_M0L6_2atmpS1739, 10);
    #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
    return _M0MPC14byte4Byte8to__char(_M0L6_2atmpS1738);
  }
}

int32_t _M0IPC14byte4BytePB3Sub3sub(
  int32_t _M0L4selfS372,
  int32_t _M0L4thatS373
) {
  int32_t _M0L6_2atmpS1735;
  int32_t _M0L6_2atmpS1736;
  int32_t _M0L6_2atmpS1734;
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1735 = (int32_t)_M0L4selfS372;
  _M0L6_2atmpS1736 = (int32_t)_M0L4thatS373;
  _M0L6_2atmpS1734 = _M0L6_2atmpS1735 - _M0L6_2atmpS1736;
  return _M0L6_2atmpS1734 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Mod3mod(
  int32_t _M0L4selfS370,
  int32_t _M0L4thatS371
) {
  int32_t _M0L6_2atmpS1732;
  int32_t _M0L6_2atmpS1733;
  int32_t _M0L6_2atmpS1731;
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1732 = (int32_t)_M0L4selfS370;
  _M0L6_2atmpS1733 = (int32_t)_M0L4thatS371;
  _M0L6_2atmpS1731 = _M0L6_2atmpS1732 % _M0L6_2atmpS1733;
  return _M0L6_2atmpS1731 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Div3div(
  int32_t _M0L4selfS368,
  int32_t _M0L4thatS369
) {
  int32_t _M0L6_2atmpS1729;
  int32_t _M0L6_2atmpS1730;
  int32_t _M0L6_2atmpS1728;
  #line 62 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1729 = (int32_t)_M0L4selfS368;
  _M0L6_2atmpS1730 = (int32_t)_M0L4thatS369;
  _M0L6_2atmpS1728 = _M0L6_2atmpS1729 / _M0L6_2atmpS1730;
  return _M0L6_2atmpS1728 & 0xff;
}

int32_t _M0IPC14byte4BytePB3Add3add(
  int32_t _M0L4selfS366,
  int32_t _M0L4thatS367
) {
  int32_t _M0L6_2atmpS1726;
  int32_t _M0L6_2atmpS1727;
  int32_t _M0L6_2atmpS1725;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\byte.mbt"
  _M0L6_2atmpS1726 = (int32_t)_M0L4selfS366;
  _M0L6_2atmpS1727 = (int32_t)_M0L4thatS367;
  _M0L6_2atmpS1725 = _M0L6_2atmpS1726 + _M0L6_2atmpS1727;
  return _M0L6_2atmpS1725 & 0xff;
}

moonbit_string_t _M0MPC16string6String17unsafe__substring(
  moonbit_string_t _M0L3strS363,
  int32_t _M0L5startS361,
  int32_t _M0L3endS362
) {
  int32_t _if__result_3547;
  int32_t _M0L3lenS364;
  int32_t _M0L6_2atmpS1723;
  int32_t _M0L6_2atmpS1724;
  moonbit_bytes_t _M0L5bytesS365;
  moonbit_bytes_t _M0L6_2atmpS1722;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L5startS361 == 0) {
    int32_t _M0L6_2atmpS1721 = Moonbit_array_length(_M0L3strS363);
    _if__result_3547 = _M0L3endS362 == _M0L6_2atmpS1721;
  } else {
    _if__result_3547 = 0;
  }
  if (_if__result_3547) {
    return _M0L3strS363;
  }
  _M0L3lenS364 = _M0L3endS362 - _M0L5startS361;
  _M0L6_2atmpS1723 = _M0L3lenS364 * 2;
  #line 101 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1724 = _M0IPC14byte4BytePB7Default7default();
  _M0L5bytesS365
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1723, _M0L6_2atmpS1724);
  moonbit_incref(_M0L5bytesS365);
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L5bytesS365, 0, _M0L3strS363, _M0L5startS361, _M0L3lenS364);
  _M0L6_2atmpS1722 = _M0L5bytesS365;
  #line 103 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1722, 0, 4294967296ll);
}

struct _M0TWEOs* _M0MPB4Iter3newGsE(struct _M0TWEOs* _M0L1fS360) {
  #line 205 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L1fS360;
}

int32_t _M0MPC16string10StringView11unsafe__get(
  struct _M0TPC16string10StringView _M0L4selfS358,
  int32_t _M0L5indexS359
) {
  moonbit_string_t _M0L8_2afieldS3158;
  moonbit_string_t _M0L3strS1718;
  int32_t _M0L8_2afieldS3157;
  int32_t _M0L5startS1720;
  int32_t _M0L6_2atmpS1719;
  int32_t _M0L6_2atmpS3156;
  #line 127 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3158 = _M0L4selfS358.$0;
  _M0L3strS1718 = _M0L8_2afieldS3158;
  _M0L8_2afieldS3157 = _M0L4selfS358.$1;
  _M0L5startS1720 = _M0L8_2afieldS3157;
  _M0L6_2atmpS1719 = _M0L5startS1720 + _M0L5indexS359;
  _M0L6_2atmpS3156 = _M0L3strS1718[_M0L6_2atmpS1719];
  moonbit_decref(_M0L3strS1718);
  return _M0L6_2atmpS3156;
}

moonbit_string_t _M0MPC13int3Int18to__string_2einner(
  int32_t _M0L4selfS342,
  int32_t _M0L5radixS341
) {
  int32_t _if__result_3548;
  int32_t _M0L12is__negativeS343;
  uint32_t _M0L3numS344;
  uint16_t* _M0L6bufferS345;
  #line 220 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5radixS341 < 2) {
    _if__result_3548 = 1;
  } else {
    _if__result_3548 = _M0L5radixS341 > 36;
  }
  if (_if__result_3548) {
    #line 224 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_157.data, (moonbit_string_t)moonbit_string_literal_158.data);
  }
  if (_M0L4selfS342 == 0) {
    return (moonbit_string_t)moonbit_string_literal_159.data;
  }
  _M0L12is__negativeS343 = _M0L4selfS342 < 0;
  if (_M0L12is__negativeS343) {
    int32_t _M0L6_2atmpS1717 = -_M0L4selfS342;
    _M0L3numS344 = *(uint32_t*)&_M0L6_2atmpS1717;
  } else {
    _M0L3numS344 = *(uint32_t*)&_M0L4selfS342;
  }
  switch (_M0L5radixS341) {
    case 10: {
      int32_t _M0L10digit__lenS346;
      int32_t _M0L6_2atmpS1714;
      int32_t _M0L10total__lenS347;
      uint16_t* _M0L6bufferS348;
      int32_t _M0L12digit__startS349;
      #line 246 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS346 = _M0FPB12dec__count32(_M0L3numS344);
      if (_M0L12is__negativeS343) {
        _M0L6_2atmpS1714 = 1;
      } else {
        _M0L6_2atmpS1714 = 0;
      }
      _M0L10total__lenS347 = _M0L10digit__lenS346 + _M0L6_2atmpS1714;
      _M0L6bufferS348
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS347, 0);
      if (_M0L12is__negativeS343) {
        _M0L12digit__startS349 = 1;
      } else {
        _M0L12digit__startS349 = 0;
      }
      moonbit_incref(_M0L6bufferS348);
      #line 250 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__dec(_M0L6bufferS348, _M0L3numS344, _M0L12digit__startS349, _M0L10total__lenS347);
      _M0L6bufferS345 = _M0L6bufferS348;
      break;
    }
    
    case 16: {
      int32_t _M0L10digit__lenS350;
      int32_t _M0L6_2atmpS1715;
      int32_t _M0L10total__lenS351;
      uint16_t* _M0L6bufferS352;
      int32_t _M0L12digit__startS353;
      #line 254 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS350 = _M0FPB12hex__count32(_M0L3numS344);
      if (_M0L12is__negativeS343) {
        _M0L6_2atmpS1715 = 1;
      } else {
        _M0L6_2atmpS1715 = 0;
      }
      _M0L10total__lenS351 = _M0L10digit__lenS350 + _M0L6_2atmpS1715;
      _M0L6bufferS352
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS351, 0);
      if (_M0L12is__negativeS343) {
        _M0L12digit__startS353 = 1;
      } else {
        _M0L12digit__startS353 = 0;
      }
      moonbit_incref(_M0L6bufferS352);
      #line 258 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB20int__to__string__hex(_M0L6bufferS352, _M0L3numS344, _M0L12digit__startS353, _M0L10total__lenS351);
      _M0L6bufferS345 = _M0L6bufferS352;
      break;
    }
    default: {
      int32_t _M0L10digit__lenS354;
      int32_t _M0L6_2atmpS1716;
      int32_t _M0L10total__lenS355;
      uint16_t* _M0L6bufferS356;
      int32_t _M0L12digit__startS357;
      #line 262 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0L10digit__lenS354
      = _M0FPB14radix__count32(_M0L3numS344, _M0L5radixS341);
      if (_M0L12is__negativeS343) {
        _M0L6_2atmpS1716 = 1;
      } else {
        _M0L6_2atmpS1716 = 0;
      }
      _M0L10total__lenS355 = _M0L10digit__lenS354 + _M0L6_2atmpS1716;
      _M0L6bufferS356
      = (uint16_t*)moonbit_make_string(_M0L10total__lenS355, 0);
      if (_M0L12is__negativeS343) {
        _M0L12digit__startS357 = 1;
      } else {
        _M0L12digit__startS357 = 0;
      }
      moonbit_incref(_M0L6bufferS356);
      #line 266 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
      _M0FPB24int__to__string__generic(_M0L6bufferS356, _M0L3numS344, _M0L12digit__startS357, _M0L10total__lenS355, _M0L5radixS341);
      _M0L6bufferS345 = _M0L6bufferS356;
      break;
    }
  }
  if (_M0L12is__negativeS343) {
    _M0L6bufferS345[0] = 45;
  }
  return _M0L6bufferS345;
}

int32_t _M0FPB14radix__count32(
  uint32_t _M0L5valueS335,
  int32_t _M0L5radixS338
) {
  uint32_t _M0Lm3numS336;
  uint32_t _M0L4baseS337;
  int32_t _M0Lm5countS339;
  #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS335 == 0u) {
    return 1;
  }
  _M0Lm3numS336 = _M0L5valueS335;
  _M0L4baseS337 = *(uint32_t*)&_M0L5radixS338;
  _M0Lm5countS339 = 0;
  while (1) {
    uint32_t _M0L6_2atmpS1711 = _M0Lm3numS336;
    if (_M0L6_2atmpS1711 > 0u) {
      int32_t _M0L6_2atmpS1712 = _M0Lm5countS339;
      uint32_t _M0L6_2atmpS1713;
      _M0Lm5countS339 = _M0L6_2atmpS1712 + 1;
      _M0L6_2atmpS1713 = _M0Lm3numS336;
      _M0Lm3numS336 = _M0L6_2atmpS1713 / _M0L4baseS337;
      continue;
    }
    break;
  }
  return _M0Lm5countS339;
}

int32_t _M0FPB12hex__count32(uint32_t _M0L5valueS333) {
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS333 == 0u) {
    return 1;
  } else {
    int32_t _M0L14leading__zerosS334;
    int32_t _M0L6_2atmpS1710;
    int32_t _M0L6_2atmpS1709;
    #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L14leading__zerosS334 = moonbit_clz32(_M0L5valueS333);
    _M0L6_2atmpS1710 = 31 - _M0L14leading__zerosS334;
    _M0L6_2atmpS1709 = _M0L6_2atmpS1710 / 4;
    return _M0L6_2atmpS1709 + 1;
  }
}

int32_t _M0FPB12dec__count32(uint32_t _M0L5valueS332) {
  #line 152 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  if (_M0L5valueS332 >= 100000u) {
    if (_M0L5valueS332 >= 10000000u) {
      if (_M0L5valueS332 >= 1000000000u) {
        return 10;
      } else if (_M0L5valueS332 >= 100000000u) {
        return 9;
      } else {
        return 8;
      }
    } else if (_M0L5valueS332 >= 1000000u) {
      return 7;
    } else {
      return 6;
    }
  } else if (_M0L5valueS332 >= 1000u) {
    if (_M0L5valueS332 >= 10000u) {
      return 5;
    } else {
      return 4;
    }
  } else if (_M0L5valueS332 >= 100u) {
    return 3;
  } else if (_M0L5valueS332 >= 10u) {
    return 2;
  } else {
    return 1;
  }
}

int32_t _M0FPB20int__to__string__dec(
  uint16_t* _M0L6bufferS322,
  uint32_t _M0L3numS310,
  int32_t _M0L12digit__startS313,
  int32_t _M0L10total__lenS312
) {
  uint32_t _M0Lm3numS309;
  int32_t _M0Lm6offsetS311;
  uint32_t _M0L6_2atmpS1708;
  int32_t _M0Lm9remainingS324;
  int32_t _M0L6_2atmpS1689;
  #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm3numS309 = _M0L3numS310;
  _M0Lm6offsetS311 = _M0L10total__lenS312 - _M0L12digit__startS313;
  while (1) {
    uint32_t _M0L6_2atmpS1652 = _M0Lm3numS309;
    if (_M0L6_2atmpS1652 >= 10000u) {
      uint32_t _M0L6_2atmpS1675 = _M0Lm3numS309;
      uint32_t _M0L1tS314 = _M0L6_2atmpS1675 / 10000u;
      uint32_t _M0L6_2atmpS1674 = _M0Lm3numS309;
      uint32_t _M0L6_2atmpS1673 = _M0L6_2atmpS1674 % 10000u;
      int32_t _M0L1rS315 = *(int32_t*)&_M0L6_2atmpS1673;
      int32_t _M0L2d1S316;
      int32_t _M0L2d2S317;
      int32_t _M0L6_2atmpS1653;
      int32_t _M0L6_2atmpS1672;
      int32_t _M0L6_2atmpS1671;
      int32_t _M0L6d1__hiS318;
      int32_t _M0L6_2atmpS1670;
      int32_t _M0L6_2atmpS1669;
      int32_t _M0L6d1__loS319;
      int32_t _M0L6_2atmpS1668;
      int32_t _M0L6_2atmpS1667;
      int32_t _M0L6d2__hiS320;
      int32_t _M0L6_2atmpS1666;
      int32_t _M0L6_2atmpS1665;
      int32_t _M0L6d2__loS321;
      int32_t _M0L6_2atmpS1655;
      int32_t _M0L6_2atmpS1654;
      int32_t _M0L6_2atmpS1658;
      int32_t _M0L6_2atmpS1657;
      int32_t _M0L6_2atmpS1656;
      int32_t _M0L6_2atmpS1661;
      int32_t _M0L6_2atmpS1660;
      int32_t _M0L6_2atmpS1659;
      int32_t _M0L6_2atmpS1664;
      int32_t _M0L6_2atmpS1663;
      int32_t _M0L6_2atmpS1662;
      _M0Lm3numS309 = _M0L1tS314;
      _M0L2d1S316 = _M0L1rS315 / 100;
      _M0L2d2S317 = _M0L1rS315 % 100;
      _M0L6_2atmpS1653 = _M0Lm6offsetS311;
      _M0Lm6offsetS311 = _M0L6_2atmpS1653 - 4;
      _M0L6_2atmpS1672 = _M0L2d1S316 / 10;
      _M0L6_2atmpS1671 = 48 + _M0L6_2atmpS1672;
      _M0L6d1__hiS318 = (uint16_t)_M0L6_2atmpS1671;
      _M0L6_2atmpS1670 = _M0L2d1S316 % 10;
      _M0L6_2atmpS1669 = 48 + _M0L6_2atmpS1670;
      _M0L6d1__loS319 = (uint16_t)_M0L6_2atmpS1669;
      _M0L6_2atmpS1668 = _M0L2d2S317 / 10;
      _M0L6_2atmpS1667 = 48 + _M0L6_2atmpS1668;
      _M0L6d2__hiS320 = (uint16_t)_M0L6_2atmpS1667;
      _M0L6_2atmpS1666 = _M0L2d2S317 % 10;
      _M0L6_2atmpS1665 = 48 + _M0L6_2atmpS1666;
      _M0L6d2__loS321 = (uint16_t)_M0L6_2atmpS1665;
      _M0L6_2atmpS1655 = _M0Lm6offsetS311;
      _M0L6_2atmpS1654 = _M0L12digit__startS313 + _M0L6_2atmpS1655;
      _M0L6bufferS322[_M0L6_2atmpS1654] = _M0L6d1__hiS318;
      _M0L6_2atmpS1658 = _M0Lm6offsetS311;
      _M0L6_2atmpS1657 = _M0L12digit__startS313 + _M0L6_2atmpS1658;
      _M0L6_2atmpS1656 = _M0L6_2atmpS1657 + 1;
      _M0L6bufferS322[_M0L6_2atmpS1656] = _M0L6d1__loS319;
      _M0L6_2atmpS1661 = _M0Lm6offsetS311;
      _M0L6_2atmpS1660 = _M0L12digit__startS313 + _M0L6_2atmpS1661;
      _M0L6_2atmpS1659 = _M0L6_2atmpS1660 + 2;
      _M0L6bufferS322[_M0L6_2atmpS1659] = _M0L6d2__hiS320;
      _M0L6_2atmpS1664 = _M0Lm6offsetS311;
      _M0L6_2atmpS1663 = _M0L12digit__startS313 + _M0L6_2atmpS1664;
      _M0L6_2atmpS1662 = _M0L6_2atmpS1663 + 3;
      _M0L6bufferS322[_M0L6_2atmpS1662] = _M0L6d2__loS321;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1708 = _M0Lm3numS309;
  _M0Lm9remainingS324 = *(int32_t*)&_M0L6_2atmpS1708;
  while (1) {
    int32_t _M0L6_2atmpS1676 = _M0Lm9remainingS324;
    if (_M0L6_2atmpS1676 >= 100) {
      int32_t _M0L6_2atmpS1688 = _M0Lm9remainingS324;
      int32_t _M0L1tS325 = _M0L6_2atmpS1688 / 100;
      int32_t _M0L6_2atmpS1687 = _M0Lm9remainingS324;
      int32_t _M0L1dS326 = _M0L6_2atmpS1687 % 100;
      int32_t _M0L6_2atmpS1677;
      int32_t _M0L6_2atmpS1686;
      int32_t _M0L6_2atmpS1685;
      int32_t _M0L5d__hiS327;
      int32_t _M0L6_2atmpS1684;
      int32_t _M0L6_2atmpS1683;
      int32_t _M0L5d__loS328;
      int32_t _M0L6_2atmpS1679;
      int32_t _M0L6_2atmpS1678;
      int32_t _M0L6_2atmpS1682;
      int32_t _M0L6_2atmpS1681;
      int32_t _M0L6_2atmpS1680;
      _M0Lm9remainingS324 = _M0L1tS325;
      _M0L6_2atmpS1677 = _M0Lm6offsetS311;
      _M0Lm6offsetS311 = _M0L6_2atmpS1677 - 2;
      _M0L6_2atmpS1686 = _M0L1dS326 / 10;
      _M0L6_2atmpS1685 = 48 + _M0L6_2atmpS1686;
      _M0L5d__hiS327 = (uint16_t)_M0L6_2atmpS1685;
      _M0L6_2atmpS1684 = _M0L1dS326 % 10;
      _M0L6_2atmpS1683 = 48 + _M0L6_2atmpS1684;
      _M0L5d__loS328 = (uint16_t)_M0L6_2atmpS1683;
      _M0L6_2atmpS1679 = _M0Lm6offsetS311;
      _M0L6_2atmpS1678 = _M0L12digit__startS313 + _M0L6_2atmpS1679;
      _M0L6bufferS322[_M0L6_2atmpS1678] = _M0L5d__hiS327;
      _M0L6_2atmpS1682 = _M0Lm6offsetS311;
      _M0L6_2atmpS1681 = _M0L12digit__startS313 + _M0L6_2atmpS1682;
      _M0L6_2atmpS1680 = _M0L6_2atmpS1681 + 1;
      _M0L6bufferS322[_M0L6_2atmpS1680] = _M0L5d__loS328;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1689 = _M0Lm9remainingS324;
  if (_M0L6_2atmpS1689 >= 10) {
    int32_t _M0L6_2atmpS1690 = _M0Lm6offsetS311;
    int32_t _M0L6_2atmpS1701;
    int32_t _M0L6_2atmpS1700;
    int32_t _M0L6_2atmpS1699;
    int32_t _M0L5d__hiS330;
    int32_t _M0L6_2atmpS1698;
    int32_t _M0L6_2atmpS1697;
    int32_t _M0L6_2atmpS1696;
    int32_t _M0L5d__loS331;
    int32_t _M0L6_2atmpS1692;
    int32_t _M0L6_2atmpS1691;
    int32_t _M0L6_2atmpS1695;
    int32_t _M0L6_2atmpS1694;
    int32_t _M0L6_2atmpS1693;
    _M0Lm6offsetS311 = _M0L6_2atmpS1690 - 2;
    _M0L6_2atmpS1701 = _M0Lm9remainingS324;
    _M0L6_2atmpS1700 = _M0L6_2atmpS1701 / 10;
    _M0L6_2atmpS1699 = 48 + _M0L6_2atmpS1700;
    _M0L5d__hiS330 = (uint16_t)_M0L6_2atmpS1699;
    _M0L6_2atmpS1698 = _M0Lm9remainingS324;
    _M0L6_2atmpS1697 = _M0L6_2atmpS1698 % 10;
    _M0L6_2atmpS1696 = 48 + _M0L6_2atmpS1697;
    _M0L5d__loS331 = (uint16_t)_M0L6_2atmpS1696;
    _M0L6_2atmpS1692 = _M0Lm6offsetS311;
    _M0L6_2atmpS1691 = _M0L12digit__startS313 + _M0L6_2atmpS1692;
    _M0L6bufferS322[_M0L6_2atmpS1691] = _M0L5d__hiS330;
    _M0L6_2atmpS1695 = _M0Lm6offsetS311;
    _M0L6_2atmpS1694 = _M0L12digit__startS313 + _M0L6_2atmpS1695;
    _M0L6_2atmpS1693 = _M0L6_2atmpS1694 + 1;
    _M0L6bufferS322[_M0L6_2atmpS1693] = _M0L5d__loS331;
    moonbit_decref(_M0L6bufferS322);
  } else {
    int32_t _M0L6_2atmpS1702 = _M0Lm6offsetS311;
    int32_t _M0L6_2atmpS1707;
    int32_t _M0L6_2atmpS1703;
    int32_t _M0L6_2atmpS1706;
    int32_t _M0L6_2atmpS1705;
    int32_t _M0L6_2atmpS1704;
    _M0Lm6offsetS311 = _M0L6_2atmpS1702 - 1;
    _M0L6_2atmpS1707 = _M0Lm6offsetS311;
    _M0L6_2atmpS1703 = _M0L12digit__startS313 + _M0L6_2atmpS1707;
    _M0L6_2atmpS1706 = _M0Lm9remainingS324;
    _M0L6_2atmpS1705 = 48 + _M0L6_2atmpS1706;
    _M0L6_2atmpS1704 = (uint16_t)_M0L6_2atmpS1705;
    _M0L6bufferS322[_M0L6_2atmpS1703] = _M0L6_2atmpS1704;
    moonbit_decref(_M0L6bufferS322);
  }
  return 0;
}

int32_t _M0FPB24int__to__string__generic(
  uint16_t* _M0L6bufferS304,
  uint32_t _M0L3numS298,
  int32_t _M0L12digit__startS296,
  int32_t _M0L10total__lenS295,
  int32_t _M0L5radixS300
) {
  int32_t _M0Lm6offsetS294;
  uint32_t _M0Lm1nS297;
  uint32_t _M0L4baseS299;
  int32_t _M0L6_2atmpS1634;
  int32_t _M0L6_2atmpS1633;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS294 = _M0L10total__lenS295 - _M0L12digit__startS296;
  _M0Lm1nS297 = _M0L3numS298;
  _M0L4baseS299 = *(uint32_t*)&_M0L5radixS300;
  _M0L6_2atmpS1634 = _M0L5radixS300 - 1;
  _M0L6_2atmpS1633 = _M0L5radixS300 & _M0L6_2atmpS1634;
  if (_M0L6_2atmpS1633 == 0) {
    int32_t _M0L5shiftS301;
    uint32_t _M0L4maskS302;
    #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
    _M0L5shiftS301 = moonbit_ctz32(_M0L5radixS300);
    _M0L4maskS302 = _M0L4baseS299 - 1u;
    while (1) {
      uint32_t _M0L6_2atmpS1635 = _M0Lm1nS297;
      if (_M0L6_2atmpS1635 > 0u) {
        int32_t _M0L6_2atmpS1636 = _M0Lm6offsetS294;
        uint32_t _M0L6_2atmpS1642;
        uint32_t _M0L6_2atmpS1641;
        int32_t _M0L5digitS303;
        int32_t _M0L6_2atmpS1639;
        int32_t _M0L6_2atmpS1637;
        int32_t _M0L6_2atmpS1638;
        uint32_t _M0L6_2atmpS1640;
        _M0Lm6offsetS294 = _M0L6_2atmpS1636 - 1;
        _M0L6_2atmpS1642 = _M0Lm1nS297;
        _M0L6_2atmpS1641 = _M0L6_2atmpS1642 & _M0L4maskS302;
        _M0L5digitS303 = *(int32_t*)&_M0L6_2atmpS1641;
        _M0L6_2atmpS1639 = _M0Lm6offsetS294;
        _M0L6_2atmpS1637 = _M0L12digit__startS296 + _M0L6_2atmpS1639;
        _M0L6_2atmpS1638
        = ((moonbit_string_t)moonbit_string_literal_160.data)[
          _M0L5digitS303
        ];
        _M0L6bufferS304[_M0L6_2atmpS1637] = _M0L6_2atmpS1638;
        _M0L6_2atmpS1640 = _M0Lm1nS297;
        _M0Lm1nS297 = _M0L6_2atmpS1640 >> (_M0L5shiftS301 & 31);
        continue;
      } else {
        moonbit_decref(_M0L6bufferS304);
      }
      break;
    }
  } else {
    while (1) {
      uint32_t _M0L6_2atmpS1643 = _M0Lm1nS297;
      if (_M0L6_2atmpS1643 > 0u) {
        int32_t _M0L6_2atmpS1644 = _M0Lm6offsetS294;
        uint32_t _M0L6_2atmpS1651;
        uint32_t _M0L1qS306;
        uint32_t _M0L6_2atmpS1649;
        uint32_t _M0L6_2atmpS1650;
        uint32_t _M0L6_2atmpS1648;
        int32_t _M0L5digitS307;
        int32_t _M0L6_2atmpS1647;
        int32_t _M0L6_2atmpS1645;
        int32_t _M0L6_2atmpS1646;
        _M0Lm6offsetS294 = _M0L6_2atmpS1644 - 1;
        _M0L6_2atmpS1651 = _M0Lm1nS297;
        _M0L1qS306 = _M0L6_2atmpS1651 / _M0L4baseS299;
        _M0L6_2atmpS1649 = _M0Lm1nS297;
        _M0L6_2atmpS1650 = _M0L1qS306 * _M0L4baseS299;
        _M0L6_2atmpS1648 = _M0L6_2atmpS1649 - _M0L6_2atmpS1650;
        _M0L5digitS307 = *(int32_t*)&_M0L6_2atmpS1648;
        _M0L6_2atmpS1647 = _M0Lm6offsetS294;
        _M0L6_2atmpS1645 = _M0L12digit__startS296 + _M0L6_2atmpS1647;
        _M0L6_2atmpS1646
        = ((moonbit_string_t)moonbit_string_literal_160.data)[
          _M0L5digitS307
        ];
        _M0L6bufferS304[_M0L6_2atmpS1645] = _M0L6_2atmpS1646;
        _M0Lm1nS297 = _M0L1qS306;
        continue;
      } else {
        moonbit_decref(_M0L6bufferS304);
      }
      break;
    }
  }
  return 0;
}

int32_t _M0FPB20int__to__string__hex(
  uint16_t* _M0L6bufferS291,
  uint32_t _M0L3numS287,
  int32_t _M0L12digit__startS285,
  int32_t _M0L10total__lenS284
) {
  int32_t _M0Lm6offsetS283;
  uint32_t _M0Lm1nS286;
  int32_t _M0L6_2atmpS1629;
  #line 29 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\to_string.mbt"
  _M0Lm6offsetS283 = _M0L10total__lenS284 - _M0L12digit__startS285;
  _M0Lm1nS286 = _M0L3numS287;
  while (1) {
    int32_t _M0L6_2atmpS1617 = _M0Lm6offsetS283;
    if (_M0L6_2atmpS1617 >= 2) {
      int32_t _M0L6_2atmpS1618 = _M0Lm6offsetS283;
      uint32_t _M0L6_2atmpS1628;
      uint32_t _M0L6_2atmpS1627;
      int32_t _M0L9byte__valS288;
      int32_t _M0L2hiS289;
      int32_t _M0L2loS290;
      int32_t _M0L6_2atmpS1621;
      int32_t _M0L6_2atmpS1619;
      int32_t _M0L6_2atmpS1620;
      int32_t _M0L6_2atmpS1625;
      int32_t _M0L6_2atmpS1624;
      int32_t _M0L6_2atmpS1622;
      int32_t _M0L6_2atmpS1623;
      uint32_t _M0L6_2atmpS1626;
      _M0Lm6offsetS283 = _M0L6_2atmpS1618 - 2;
      _M0L6_2atmpS1628 = _M0Lm1nS286;
      _M0L6_2atmpS1627 = _M0L6_2atmpS1628 & 255u;
      _M0L9byte__valS288 = *(int32_t*)&_M0L6_2atmpS1627;
      _M0L2hiS289 = _M0L9byte__valS288 / 16;
      _M0L2loS290 = _M0L9byte__valS288 % 16;
      _M0L6_2atmpS1621 = _M0Lm6offsetS283;
      _M0L6_2atmpS1619 = _M0L12digit__startS285 + _M0L6_2atmpS1621;
      _M0L6_2atmpS1620
      = ((moonbit_string_t)moonbit_string_literal_160.data)[
        _M0L2hiS289
      ];
      _M0L6bufferS291[_M0L6_2atmpS1619] = _M0L6_2atmpS1620;
      _M0L6_2atmpS1625 = _M0Lm6offsetS283;
      _M0L6_2atmpS1624 = _M0L12digit__startS285 + _M0L6_2atmpS1625;
      _M0L6_2atmpS1622 = _M0L6_2atmpS1624 + 1;
      _M0L6_2atmpS1623
      = ((moonbit_string_t)moonbit_string_literal_160.data)[
        _M0L2loS290
      ];
      _M0L6bufferS291[_M0L6_2atmpS1622] = _M0L6_2atmpS1623;
      _M0L6_2atmpS1626 = _M0Lm1nS286;
      _M0Lm1nS286 = _M0L6_2atmpS1626 >> 8;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1629 = _M0Lm6offsetS283;
  if (_M0L6_2atmpS1629 == 1) {
    uint32_t _M0L6_2atmpS1632 = _M0Lm1nS286;
    uint32_t _M0L6_2atmpS1631 = _M0L6_2atmpS1632 & 15u;
    int32_t _M0L6nibbleS293 = *(int32_t*)&_M0L6_2atmpS1631;
    int32_t _M0L6_2atmpS1630 =
      ((moonbit_string_t)moonbit_string_literal_160.data)[_M0L6nibbleS293];
    _M0L6bufferS291[_M0L12digit__startS285] = _M0L6_2atmpS1630;
    moonbit_decref(_M0L6bufferS291);
  } else {
    moonbit_decref(_M0L6bufferS291);
  }
  return 0;
}

moonbit_string_t _M0MPB4Iter4nextGsE(struct _M0TWEOs* _M0L4selfS282) {
  struct _M0TWEOs* _M0L7_2afuncS281;
  #line 28 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  _M0L7_2afuncS281 = _M0L4selfS282;
  #line 31 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\iterator.mbt"
  return _M0L7_2afuncS281->code(_M0L7_2afuncS281);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGiE(
  int32_t _M0L4selfS274
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS273;
  struct _M0TPB6Logger _M0L6_2atmpS1613;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS273 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS273);
  _M0L6_2atmpS1613
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS273
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC13int3IntPB4Show6output(_M0L4selfS274, _M0L6_2atmpS1613);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS273);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(
  moonbit_string_t _M0L4selfS276
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS275;
  struct _M0TPB6Logger _M0L6_2atmpS1614;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS275 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS275);
  _M0L6_2atmpS1614
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS275
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB9SourceLocPB4Show6output(_M0L4selfS276, _M0L6_2atmpS1614);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS275);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGbE(
  int32_t _M0L4selfS278
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS277;
  struct _M0TPB6Logger _M0L6_2atmpS1615;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS277 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS277);
  _M0L6_2atmpS1615
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS277
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPC14bool4BoolPB4Show6output(_M0L4selfS278, _M0L6_2atmpS1615);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS277);
}

moonbit_string_t _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(
  void* _M0L4selfS280
) {
  struct _M0TPB13StringBuilder* _M0L6loggerS279;
  struct _M0TPB6Logger _M0L6_2atmpS1616;
  #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6loggerS279 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L6loggerS279);
  _M0L6_2atmpS1616
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L6loggerS279
  };
  #line 144 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB7FailurePB4Show6output(_M0L4selfS280, _M0L6_2atmpS1616);
  #line 145 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L6loggerS279);
}

int32_t _M0MPC16string10StringView13start__offset(
  struct _M0TPC16string10StringView _M0L4selfS272
) {
  int32_t _M0L8_2afieldS3159;
  #line 98 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3159 = _M0L4selfS272.$1;
  moonbit_decref(_M0L4selfS272.$0);
  return _M0L8_2afieldS3159;
}

int32_t _M0MPC16string10StringView6length(
  struct _M0TPC16string10StringView _M0L4selfS271
) {
  int32_t _M0L3endS1611;
  int32_t _M0L8_2afieldS3160;
  int32_t _M0L5startS1612;
  #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3endS1611 = _M0L4selfS271.$2;
  _M0L8_2afieldS3160 = _M0L4selfS271.$1;
  moonbit_decref(_M0L4selfS271.$0);
  _M0L5startS1612 = _M0L8_2afieldS3160;
  return _M0L3endS1611 - _M0L5startS1612;
}

moonbit_string_t _M0MPC16string10StringView4data(
  struct _M0TPC16string10StringView _M0L4selfS270
) {
  moonbit_string_t _M0L8_2afieldS3161;
  #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L8_2afieldS3161 = _M0L4selfS270.$0;
  return _M0L8_2afieldS3161;
}

int32_t _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(
  struct _M0TPB13StringBuilder* _M0L4selfS266,
  moonbit_string_t _M0L5valueS267,
  int32_t _M0L5startS268,
  int32_t _M0L3lenS269
) {
  int32_t _M0L6_2atmpS1610;
  int64_t _M0L6_2atmpS1609;
  struct _M0TPC16string10StringView _M0L6_2atmpS1608;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1610 = _M0L5startS268 + _M0L3lenS269;
  _M0L6_2atmpS1609 = (int64_t)_M0L6_2atmpS1610;
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS1608
  = _M0MPC16string6String11sub_2einner(_M0L5valueS267, _M0L5startS268, _M0L6_2atmpS1609);
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L4selfS266, _M0L6_2atmpS1608);
  return 0;
}

struct _M0TPC16string10StringView _M0MPC16string6String11sub_2einner(
  moonbit_string_t _M0L4selfS259,
  int32_t _M0L5startS265,
  int64_t _M0L3endS261
) {
  int32_t _M0L3lenS258;
  int32_t _M0L3endS260;
  int32_t _M0L5startS264;
  int32_t _if__result_3555;
  #line 458 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
  _M0L3lenS258 = Moonbit_array_length(_M0L4selfS259);
  if (_M0L3endS261 == 4294967296ll) {
    _M0L3endS260 = _M0L3lenS258;
  } else {
    int64_t _M0L7_2aSomeS262 = _M0L3endS261;
    int32_t _M0L6_2aendS263 = (int32_t)_M0L7_2aSomeS262;
    if (_M0L6_2aendS263 < 0) {
      _M0L3endS260 = _M0L3lenS258 + _M0L6_2aendS263;
    } else {
      _M0L3endS260 = _M0L6_2aendS263;
    }
  }
  if (_M0L5startS265 < 0) {
    _M0L5startS264 = _M0L3lenS258 + _M0L5startS265;
  } else {
    _M0L5startS264 = _M0L5startS265;
  }
  if (_M0L5startS264 >= 0) {
    if (_M0L5startS264 <= _M0L3endS260) {
      _if__result_3555 = _M0L3endS260 <= _M0L3lenS258;
    } else {
      _if__result_3555 = 0;
    }
  } else {
    _if__result_3555 = 0;
  }
  if (_if__result_3555) {
    if (_M0L5startS264 < _M0L3lenS258) {
      int32_t _M0L6_2atmpS1605 = _M0L4selfS259[_M0L5startS264];
      int32_t _M0L6_2atmpS1604;
      #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1604
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1605);
      if (!_M0L6_2atmpS1604) {
        
      } else {
        #line 468 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    if (_M0L3endS260 < _M0L3lenS258) {
      int32_t _M0L6_2atmpS1607 = _M0L4selfS259[_M0L3endS260];
      int32_t _M0L6_2atmpS1606;
      #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
      _M0L6_2atmpS1606
      = _M0MPC16uint166UInt1623is__trailing__surrogate(_M0L6_2atmpS1607);
      if (!_M0L6_2atmpS1606) {
        
      } else {
        #line 471 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
        moonbit_panic();
      }
    }
    return (struct _M0TPC16string10StringView){_M0L5startS264,
                                                 _M0L3endS260,
                                                 _M0L4selfS259};
  } else {
    moonbit_decref(_M0L4selfS259);
    #line 466 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringview.mbt"
    moonbit_panic();
  }
}

int32_t _M0IP016_24default__implPB4Hash4hashGiE(int32_t _M0L4selfS255) {
  struct _M0TPB6Hasher* _M0L1hS254;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS254 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS254);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGiE(_M0L1hS254, _M0L4selfS255);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS254);
}

int32_t _M0IP016_24default__implPB4Hash4hashGsE(
  moonbit_string_t _M0L4selfS257
) {
  struct _M0TPB6Hasher* _M0L1hS256;
  #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L1hS256 = _M0MPB6Hasher3new(4294967296ll);
  moonbit_incref(_M0L1hS256);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0MPB6Hasher7combineGsE(_M0L1hS256, _M0L4selfS257);
  #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  return _M0MPB6Hasher8finalize(_M0L1hS256);
}

struct _M0TPB6Hasher* _M0MPB6Hasher3new(int64_t _M0L10seed_2eoptS252) {
  int32_t _M0L4seedS251;
  if (_M0L10seed_2eoptS252 == 4294967296ll) {
    _M0L4seedS251 = 0;
  } else {
    int64_t _M0L7_2aSomeS253 = _M0L10seed_2eoptS252;
    _M0L4seedS251 = (int32_t)_M0L7_2aSomeS253;
  }
  return _M0MPB6Hasher11new_2einner(_M0L4seedS251);
}

struct _M0TPB6Hasher* _M0MPB6Hasher11new_2einner(int32_t _M0L4seedS250) {
  uint32_t _M0L6_2atmpS1603;
  uint32_t _M0L6_2atmpS1602;
  struct _M0TPB6Hasher* _block_3556;
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1603 = *(uint32_t*)&_M0L4seedS250;
  _M0L6_2atmpS1602 = _M0L6_2atmpS1603 + 374761393u;
  _block_3556
  = (struct _M0TPB6Hasher*)moonbit_malloc(sizeof(struct _M0TPB6Hasher));
  Moonbit_object_header(_block_3556)->meta
  = Moonbit_make_regular_object_header(sizeof(struct _M0TPB6Hasher) >> 2, 0, 0);
  _block_3556->$0 = _M0L6_2atmpS1602;
  return _block_3556;
}

int32_t _M0MPB6Hasher8finalize(struct _M0TPB6Hasher* _M0L4selfS249) {
  uint32_t _M0L6_2atmpS1601;
  #line 437 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 438 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1601 = _M0MPB6Hasher9avalanche(_M0L4selfS249);
  return *(int32_t*)&_M0L6_2atmpS1601;
}

uint32_t _M0MPB6Hasher9avalanche(struct _M0TPB6Hasher* _M0L4selfS248) {
  uint32_t _M0L8_2afieldS3162;
  uint32_t _M0Lm3accS247;
  uint32_t _M0L6_2atmpS1590;
  uint32_t _M0L6_2atmpS1592;
  uint32_t _M0L6_2atmpS1591;
  uint32_t _M0L6_2atmpS1593;
  uint32_t _M0L6_2atmpS1594;
  uint32_t _M0L6_2atmpS1596;
  uint32_t _M0L6_2atmpS1595;
  uint32_t _M0L6_2atmpS1597;
  uint32_t _M0L6_2atmpS1598;
  uint32_t _M0L6_2atmpS1600;
  uint32_t _M0L6_2atmpS1599;
  #line 442 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L8_2afieldS3162 = _M0L4selfS248->$0;
  moonbit_decref(_M0L4selfS248);
  _M0Lm3accS247 = _M0L8_2afieldS3162;
  _M0L6_2atmpS1590 = _M0Lm3accS247;
  _M0L6_2atmpS1592 = _M0Lm3accS247;
  _M0L6_2atmpS1591 = _M0L6_2atmpS1592 >> 15;
  _M0Lm3accS247 = _M0L6_2atmpS1590 ^ _M0L6_2atmpS1591;
  _M0L6_2atmpS1593 = _M0Lm3accS247;
  _M0Lm3accS247 = _M0L6_2atmpS1593 * 2246822519u;
  _M0L6_2atmpS1594 = _M0Lm3accS247;
  _M0L6_2atmpS1596 = _M0Lm3accS247;
  _M0L6_2atmpS1595 = _M0L6_2atmpS1596 >> 13;
  _M0Lm3accS247 = _M0L6_2atmpS1594 ^ _M0L6_2atmpS1595;
  _M0L6_2atmpS1597 = _M0Lm3accS247;
  _M0Lm3accS247 = _M0L6_2atmpS1597 * 3266489917u;
  _M0L6_2atmpS1598 = _M0Lm3accS247;
  _M0L6_2atmpS1600 = _M0Lm3accS247;
  _M0L6_2atmpS1599 = _M0L6_2atmpS1600 >> 16;
  _M0Lm3accS247 = _M0L6_2atmpS1598 ^ _M0L6_2atmpS1599;
  return _M0Lm3accS247;
}

int32_t _M0IP016_24default__implPB2Eq10not__equalGsE(
  moonbit_string_t _M0L1xS245,
  moonbit_string_t _M0L1yS246
) {
  int32_t _M0L6_2atmpS3163;
  int32_t _M0L6_2atmpS1589;
  #line 25 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  #line 26 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\traits.mbt"
  _M0L6_2atmpS3163 = moonbit_val_array_equal(_M0L1xS245, _M0L1yS246);
  moonbit_decref(_M0L1xS245);
  moonbit_decref(_M0L1yS246);
  _M0L6_2atmpS1589 = _M0L6_2atmpS3163;
  return !_M0L6_2atmpS1589;
}

int32_t _M0MPB6Hasher7combineGiE(
  struct _M0TPB6Hasher* _M0L4selfS242,
  int32_t _M0L5valueS241
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC13int3IntPB4Hash13hash__combine(_M0L5valueS241, _M0L4selfS242);
  return 0;
}

int32_t _M0MPB6Hasher7combineGsE(
  struct _M0TPB6Hasher* _M0L4selfS244,
  moonbit_string_t _M0L5valueS243
) {
  #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  #line 121 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0IPC16string6StringPB4Hash13hash__combine(_M0L5valueS243, _M0L4selfS244);
  return 0;
}

int32_t _M0MPB6Hasher12combine__int(
  struct _M0TPB6Hasher* _M0L4selfS239,
  int32_t _M0L5valueS240
) {
  uint32_t _M0L6_2atmpS1588;
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1588 = *(uint32_t*)&_M0L5valueS240;
  #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher13combine__uint(_M0L4selfS239, _M0L6_2atmpS1588);
  return 0;
}

struct moonbit_result_0 _M0FPB15inspect_2einner(
  struct _M0TPB4Show _M0L3objS229,
  moonbit_string_t _M0L7contentS230,
  moonbit_string_t _M0L3locS232,
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L9args__locS234
) {
  moonbit_string_t _M0L6actualS228;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  #line 191 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L6actualS228 = _M0L3objS229.$0->$method_1(_M0L3objS229.$1);
  moonbit_incref(_M0L7contentS230);
  moonbit_incref(_M0L6actualS228);
  #line 192 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  if (
    _M0IP016_24default__implPB2Eq10not__equalGsE(_M0L6actualS228, _M0L7contentS230)
  ) {
    moonbit_string_t _M0L3locS231;
    moonbit_string_t _M0L9args__locS233;
    moonbit_string_t _M0L15expect__escapedS235;
    moonbit_string_t _M0L15actual__escapedS236;
    moonbit_string_t _M0L6_2atmpS1586;
    moonbit_string_t _M0L6_2atmpS1585;
    moonbit_string_t _M0L6_2atmpS3179;
    moonbit_string_t _M0L6_2atmpS1584;
    moonbit_string_t _M0L6_2atmpS3178;
    moonbit_string_t _M0L14expect__base64S237;
    moonbit_string_t _M0L6_2atmpS1583;
    moonbit_string_t _M0L6_2atmpS1582;
    moonbit_string_t _M0L6_2atmpS3177;
    moonbit_string_t _M0L6_2atmpS1581;
    moonbit_string_t _M0L6_2atmpS3176;
    moonbit_string_t _M0L14actual__base64S238;
    moonbit_string_t _M0L6_2atmpS1580;
    moonbit_string_t _M0L6_2atmpS3175;
    moonbit_string_t _M0L6_2atmpS1579;
    moonbit_string_t _M0L6_2atmpS3174;
    moonbit_string_t _M0L6_2atmpS1577;
    moonbit_string_t _M0L6_2atmpS1578;
    moonbit_string_t _M0L6_2atmpS3173;
    moonbit_string_t _M0L6_2atmpS1576;
    moonbit_string_t _M0L6_2atmpS3172;
    moonbit_string_t _M0L6_2atmpS1574;
    moonbit_string_t _M0L6_2atmpS1575;
    moonbit_string_t _M0L6_2atmpS3171;
    moonbit_string_t _M0L6_2atmpS1573;
    moonbit_string_t _M0L6_2atmpS3170;
    moonbit_string_t _M0L6_2atmpS1571;
    moonbit_string_t _M0L6_2atmpS1572;
    moonbit_string_t _M0L6_2atmpS3169;
    moonbit_string_t _M0L6_2atmpS1570;
    moonbit_string_t _M0L6_2atmpS3168;
    moonbit_string_t _M0L6_2atmpS1568;
    moonbit_string_t _M0L6_2atmpS1569;
    moonbit_string_t _M0L6_2atmpS3167;
    moonbit_string_t _M0L6_2atmpS1567;
    moonbit_string_t _M0L6_2atmpS3166;
    moonbit_string_t _M0L6_2atmpS1565;
    moonbit_string_t _M0L6_2atmpS1566;
    moonbit_string_t _M0L6_2atmpS3165;
    moonbit_string_t _M0L6_2atmpS1564;
    moonbit_string_t _M0L6_2atmpS3164;
    moonbit_string_t _M0L6_2atmpS1563;
    void* _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1562;
    struct moonbit_result_0 _result_3557;
    #line 193 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L3locS231 = _M0MPB9SourceLoc16to__json__string(_M0L3locS232);
    #line 194 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L9args__locS233 = _M0MPB7ArgsLoc8to__json(_M0L9args__locS234);
    moonbit_incref(_M0L7contentS230);
    #line 195 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L15expect__escapedS235
    = _M0MPC16string6String6escape(_M0L7contentS230);
    moonbit_incref(_M0L6actualS228);
    #line 196 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L15actual__escapedS236 = _M0MPC16string6String6escape(_M0L6actualS228);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1586
    = _M0FPB33base64__encode__string__codepoint(_M0L7contentS230);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1585
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1586);
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3179
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_161.data, _M0L6_2atmpS1585);
    moonbit_decref(_M0L6_2atmpS1585);
    _M0L6_2atmpS1584 = _M0L6_2atmpS3179;
    #line 197 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3178
    = moonbit_add_string(_M0L6_2atmpS1584, (moonbit_string_t)moonbit_string_literal_161.data);
    moonbit_decref(_M0L6_2atmpS1584);
    _M0L14expect__base64S237 = _M0L6_2atmpS3178;
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1583
    = _M0FPB33base64__encode__string__codepoint(_M0L6actualS228);
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1582
    = _M0IPC16string6StringPB4Show10to__string(_M0L6_2atmpS1583);
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3177
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_161.data, _M0L6_2atmpS1582);
    moonbit_decref(_M0L6_2atmpS1582);
    _M0L6_2atmpS1581 = _M0L6_2atmpS3177;
    #line 198 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3176
    = moonbit_add_string(_M0L6_2atmpS1581, (moonbit_string_t)moonbit_string_literal_161.data);
    moonbit_decref(_M0L6_2atmpS1581);
    _M0L14actual__base64S238 = _M0L6_2atmpS3176;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1580 = _M0IPC16string6StringPB4Show10to__string(_M0L3locS231);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3175
    = moonbit_add_string((moonbit_string_t)moonbit_string_literal_162.data, _M0L6_2atmpS1580);
    moonbit_decref(_M0L6_2atmpS1580);
    _M0L6_2atmpS1579 = _M0L6_2atmpS3175;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3174
    = moonbit_add_string(_M0L6_2atmpS1579, (moonbit_string_t)moonbit_string_literal_163.data);
    moonbit_decref(_M0L6_2atmpS1579);
    _M0L6_2atmpS1577 = _M0L6_2atmpS3174;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1578
    = _M0IPC16string6StringPB4Show10to__string(_M0L9args__locS233);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3173 = moonbit_add_string(_M0L6_2atmpS1577, _M0L6_2atmpS1578);
    moonbit_decref(_M0L6_2atmpS1577);
    moonbit_decref(_M0L6_2atmpS1578);
    _M0L6_2atmpS1576 = _M0L6_2atmpS3173;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3172
    = moonbit_add_string(_M0L6_2atmpS1576, (moonbit_string_t)moonbit_string_literal_164.data);
    moonbit_decref(_M0L6_2atmpS1576);
    _M0L6_2atmpS1574 = _M0L6_2atmpS3172;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1575
    = _M0IPC16string6StringPB4Show10to__string(_M0L15expect__escapedS235);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3171 = moonbit_add_string(_M0L6_2atmpS1574, _M0L6_2atmpS1575);
    moonbit_decref(_M0L6_2atmpS1574);
    moonbit_decref(_M0L6_2atmpS1575);
    _M0L6_2atmpS1573 = _M0L6_2atmpS3171;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3170
    = moonbit_add_string(_M0L6_2atmpS1573, (moonbit_string_t)moonbit_string_literal_165.data);
    moonbit_decref(_M0L6_2atmpS1573);
    _M0L6_2atmpS1571 = _M0L6_2atmpS3170;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1572
    = _M0IPC16string6StringPB4Show10to__string(_M0L15actual__escapedS236);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3169 = moonbit_add_string(_M0L6_2atmpS1571, _M0L6_2atmpS1572);
    moonbit_decref(_M0L6_2atmpS1571);
    moonbit_decref(_M0L6_2atmpS1572);
    _M0L6_2atmpS1570 = _M0L6_2atmpS3169;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3168
    = moonbit_add_string(_M0L6_2atmpS1570, (moonbit_string_t)moonbit_string_literal_166.data);
    moonbit_decref(_M0L6_2atmpS1570);
    _M0L6_2atmpS1568 = _M0L6_2atmpS3168;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1569
    = _M0IPC16string6StringPB4Show10to__string(_M0L14expect__base64S237);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3167 = moonbit_add_string(_M0L6_2atmpS1568, _M0L6_2atmpS1569);
    moonbit_decref(_M0L6_2atmpS1568);
    moonbit_decref(_M0L6_2atmpS1569);
    _M0L6_2atmpS1567 = _M0L6_2atmpS3167;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3166
    = moonbit_add_string(_M0L6_2atmpS1567, (moonbit_string_t)moonbit_string_literal_167.data);
    moonbit_decref(_M0L6_2atmpS1567);
    _M0L6_2atmpS1565 = _M0L6_2atmpS3166;
    #line 200 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1566
    = _M0IPC16string6StringPB4Show10to__string(_M0L14actual__base64S238);
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3165 = moonbit_add_string(_M0L6_2atmpS1565, _M0L6_2atmpS1566);
    moonbit_decref(_M0L6_2atmpS1565);
    moonbit_decref(_M0L6_2atmpS1566);
    _M0L6_2atmpS1564 = _M0L6_2atmpS3165;
    #line 199 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS3164
    = moonbit_add_string(_M0L6_2atmpS1564, (moonbit_string_t)moonbit_string_literal_7.data);
    moonbit_decref(_M0L6_2atmpS1564);
    _M0L6_2atmpS1563 = _M0L6_2atmpS3164;
    _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1562
    = (void*)moonbit_malloc(sizeof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError));
    Moonbit_object_header(_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1562)->meta
    = Moonbit_make_regular_object_header(offsetof(struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError, $0) >> 2, 1, 1);
    ((struct _M0DTPC15error5Error58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectError*)_M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1562)->$0
    = _M0L6_2atmpS1563;
    _result_3557.tag = 0;
    _result_3557.data.err
    = _M0L58moonbitlang_2fcore_2fbuiltin_2eInspectError_2eInspectErrorS1562;
    return _result_3557;
  } else {
    int32_t _M0L6_2atmpS1587;
    struct moonbit_result_0 _result_3558;
    moonbit_decref(_M0L9args__locS234);
    moonbit_decref(_M0L3locS232);
    moonbit_decref(_M0L7contentS230);
    moonbit_decref(_M0L6actualS228);
    _M0L6_2atmpS1587 = 0;
    _result_3558.tag = 1;
    _result_3558.data.ok = _M0L6_2atmpS1587;
    return _result_3558;
  }
}

moonbit_string_t _M0MPB7ArgsLoc8to__json(
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L4selfS221
) {
  struct _M0TPB13StringBuilder* _M0L3bufS219;
  struct _M0TPB5ArrayGORPB9SourceLocE* _M0L7_2aselfS220;
  int32_t _M0L7_2abindS222;
  int32_t _M0L1iS223;
  #line 140 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L3bufS219 = _M0MPB13StringBuilder11new_2einner(10);
  _M0L7_2aselfS220 = _M0L4selfS221;
  moonbit_incref(_M0L3bufS219);
  #line 143 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS219, 91);
  _M0L7_2abindS222 = _M0L7_2aselfS220->$1;
  _M0L1iS223 = 0;
  while (1) {
    if (_M0L1iS223 < _M0L7_2abindS222) {
      int32_t _if__result_3560;
      moonbit_string_t* _M0L8_2afieldS3181;
      moonbit_string_t* _M0L3bufS1560;
      moonbit_string_t _M0L6_2atmpS3180;
      moonbit_string_t _M0L4itemS224;
      int32_t _M0L6_2atmpS1561;
      if (_M0L1iS223 != 0) {
        moonbit_incref(_M0L3bufS219);
        #line 146 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS219, (moonbit_string_t)moonbit_string_literal_168.data);
      }
      if (_M0L1iS223 < 0) {
        _if__result_3560 = 1;
      } else {
        int32_t _M0L3lenS1559 = _M0L7_2aselfS220->$1;
        _if__result_3560 = _M0L1iS223 >= _M0L3lenS1559;
      }
      if (_if__result_3560) {
        #line 148 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        moonbit_panic();
      }
      _M0L8_2afieldS3181 = _M0L7_2aselfS220->$0;
      _M0L3bufS1560 = _M0L8_2afieldS3181;
      _M0L6_2atmpS3180 = (moonbit_string_t)_M0L3bufS1560[_M0L1iS223];
      _M0L4itemS224 = _M0L6_2atmpS3180;
      if (_M0L4itemS224 == 0) {
        moonbit_incref(_M0L3bufS219);
        #line 150 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS219, (moonbit_string_t)moonbit_string_literal_169.data);
      } else {
        moonbit_string_t _M0L7_2aSomeS225 = _M0L4itemS224;
        moonbit_string_t _M0L6_2alocS226 = _M0L7_2aSomeS225;
        moonbit_string_t _M0L6_2atmpS1558;
        moonbit_incref(_M0L6_2alocS226);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0L6_2atmpS1558
        = _M0MPB9SourceLoc16to__json__string(_M0L6_2alocS226);
        moonbit_incref(_M0L3bufS219);
        #line 151 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
        _M0IPB13StringBuilderPB6Logger13write__string(_M0L3bufS219, _M0L6_2atmpS1558);
      }
      _M0L6_2atmpS1561 = _M0L1iS223 + 1;
      _M0L1iS223 = _M0L6_2atmpS1561;
      continue;
    } else {
      moonbit_decref(_M0L7_2aselfS220);
    }
    break;
  }
  moonbit_incref(_M0L3bufS219);
  #line 154 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS219, 93);
  #line 155 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS219);
}

moonbit_string_t _M0MPB9SourceLoc16to__json__string(
  moonbit_string_t _M0L4selfS218
) {
  moonbit_string_t _M0L6_2atmpS1557;
  struct _M0TPB13SourceLocRepr* _M0L6_2atmpS1556;
  #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1557 = _M0L4selfS218;
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1556 = _M0MPB13SourceLocRepr5parse(_M0L6_2atmpS1557);
  #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13SourceLocRepr16to__json__string(_M0L6_2atmpS1556);
}

moonbit_string_t _M0MPB13SourceLocRepr16to__json__string(
  struct _M0TPB13SourceLocRepr* _M0L4selfS217
) {
  struct _M0TPB13StringBuilder* _M0L2sbS216;
  struct _M0TPC16string10StringView _M0L8_2afieldS3194;
  struct _M0TPC16string10StringView _M0L3pkgS1541;
  moonbit_string_t _M0L6_2atmpS1540;
  moonbit_string_t _M0L6_2atmpS3193;
  moonbit_string_t _M0L6_2atmpS1539;
  moonbit_string_t _M0L6_2atmpS3192;
  moonbit_string_t _M0L6_2atmpS1538;
  struct _M0TPC16string10StringView _M0L8_2afieldS3191;
  struct _M0TPC16string10StringView _M0L8filenameS1542;
  struct _M0TPC16string10StringView _M0L8_2afieldS3190;
  struct _M0TPC16string10StringView _M0L11start__lineS1545;
  moonbit_string_t _M0L6_2atmpS1544;
  moonbit_string_t _M0L6_2atmpS3189;
  moonbit_string_t _M0L6_2atmpS1543;
  struct _M0TPC16string10StringView _M0L8_2afieldS3188;
  struct _M0TPC16string10StringView _M0L13start__columnS1548;
  moonbit_string_t _M0L6_2atmpS1547;
  moonbit_string_t _M0L6_2atmpS3187;
  moonbit_string_t _M0L6_2atmpS1546;
  struct _M0TPC16string10StringView _M0L8_2afieldS3186;
  struct _M0TPC16string10StringView _M0L9end__lineS1551;
  moonbit_string_t _M0L6_2atmpS1550;
  moonbit_string_t _M0L6_2atmpS3185;
  moonbit_string_t _M0L6_2atmpS1549;
  struct _M0TPC16string10StringView _M0L8_2afieldS3184;
  int32_t _M0L6_2acntS3370;
  struct _M0TPC16string10StringView _M0L11end__columnS1555;
  moonbit_string_t _M0L6_2atmpS1554;
  moonbit_string_t _M0L6_2atmpS3183;
  moonbit_string_t _M0L6_2atmpS1553;
  moonbit_string_t _M0L6_2atmpS3182;
  moonbit_string_t _M0L6_2atmpS1552;
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  #line 105 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L2sbS216 = _M0MPB13StringBuilder11new_2einner(0);
  _M0L8_2afieldS3194
  = (struct _M0TPC16string10StringView){
    _M0L4selfS217->$0_1, _M0L4selfS217->$0_2, _M0L4selfS217->$0_0
  };
  _M0L3pkgS1541 = _M0L8_2afieldS3194;
  moonbit_incref(_M0L3pkgS1541.$0);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1540
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L3pkgS1541);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3193
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_170.data, _M0L6_2atmpS1540);
  moonbit_decref(_M0L6_2atmpS1540);
  _M0L6_2atmpS1539 = _M0L6_2atmpS3193;
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3192
  = moonbit_add_string(_M0L6_2atmpS1539, (moonbit_string_t)moonbit_string_literal_161.data);
  moonbit_decref(_M0L6_2atmpS1539);
  _M0L6_2atmpS1538 = _M0L6_2atmpS3192;
  moonbit_incref(_M0L2sbS216);
  #line 106 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS216, _M0L6_2atmpS1538);
  moonbit_incref(_M0L2sbS216);
  #line 107 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS216, (moonbit_string_t)moonbit_string_literal_171.data);
  _M0L8_2afieldS3191
  = (struct _M0TPC16string10StringView){
    _M0L4selfS217->$1_1, _M0L4selfS217->$1_2, _M0L4selfS217->$1_0
  };
  _M0L8filenameS1542 = _M0L8_2afieldS3191;
  moonbit_incref(_M0L8filenameS1542.$0);
  moonbit_incref(_M0L2sbS216);
  #line 108 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(_M0L2sbS216, _M0L8filenameS1542);
  _M0L8_2afieldS3190
  = (struct _M0TPC16string10StringView){
    _M0L4selfS217->$2_1, _M0L4selfS217->$2_2, _M0L4selfS217->$2_0
  };
  _M0L11start__lineS1545 = _M0L8_2afieldS3190;
  moonbit_incref(_M0L11start__lineS1545.$0);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1544
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11start__lineS1545);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3189
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_172.data, _M0L6_2atmpS1544);
  moonbit_decref(_M0L6_2atmpS1544);
  _M0L6_2atmpS1543 = _M0L6_2atmpS3189;
  moonbit_incref(_M0L2sbS216);
  #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS216, _M0L6_2atmpS1543);
  _M0L8_2afieldS3188
  = (struct _M0TPC16string10StringView){
    _M0L4selfS217->$3_1, _M0L4selfS217->$3_2, _M0L4selfS217->$3_0
  };
  _M0L13start__columnS1548 = _M0L8_2afieldS3188;
  moonbit_incref(_M0L13start__columnS1548.$0);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1547
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L13start__columnS1548);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3187
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_173.data, _M0L6_2atmpS1547);
  moonbit_decref(_M0L6_2atmpS1547);
  _M0L6_2atmpS1546 = _M0L6_2atmpS3187;
  moonbit_incref(_M0L2sbS216);
  #line 110 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS216, _M0L6_2atmpS1546);
  _M0L8_2afieldS3186
  = (struct _M0TPC16string10StringView){
    _M0L4selfS217->$4_1, _M0L4selfS217->$4_2, _M0L4selfS217->$4_0
  };
  _M0L9end__lineS1551 = _M0L8_2afieldS3186;
  moonbit_incref(_M0L9end__lineS1551.$0);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1550
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L9end__lineS1551);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3185
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_174.data, _M0L6_2atmpS1550);
  moonbit_decref(_M0L6_2atmpS1550);
  _M0L6_2atmpS1549 = _M0L6_2atmpS3185;
  moonbit_incref(_M0L2sbS216);
  #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS216, _M0L6_2atmpS1549);
  _M0L8_2afieldS3184
  = (struct _M0TPC16string10StringView){
    _M0L4selfS217->$5_1, _M0L4selfS217->$5_2, _M0L4selfS217->$5_0
  };
  _M0L6_2acntS3370 = Moonbit_object_header(_M0L4selfS217)->rc;
  if (_M0L6_2acntS3370 > 1) {
    int32_t _M0L11_2anew__cntS3376 = _M0L6_2acntS3370 - 1;
    Moonbit_object_header(_M0L4selfS217)->rc = _M0L11_2anew__cntS3376;
    moonbit_incref(_M0L8_2afieldS3184.$0);
  } else if (_M0L6_2acntS3370 == 1) {
    struct _M0TPC16string10StringView _M0L8_2afieldS3375 =
      (struct _M0TPC16string10StringView){_M0L4selfS217->$4_1,
                                            _M0L4selfS217->$4_2,
                                            _M0L4selfS217->$4_0};
    struct _M0TPC16string10StringView _M0L8_2afieldS3374;
    struct _M0TPC16string10StringView _M0L8_2afieldS3373;
    struct _M0TPC16string10StringView _M0L8_2afieldS3372;
    struct _M0TPC16string10StringView _M0L8_2afieldS3371;
    moonbit_decref(_M0L8_2afieldS3375.$0);
    _M0L8_2afieldS3374
    = (struct _M0TPC16string10StringView){
      _M0L4selfS217->$3_1, _M0L4selfS217->$3_2, _M0L4selfS217->$3_0
    };
    moonbit_decref(_M0L8_2afieldS3374.$0);
    _M0L8_2afieldS3373
    = (struct _M0TPC16string10StringView){
      _M0L4selfS217->$2_1, _M0L4selfS217->$2_2, _M0L4selfS217->$2_0
    };
    moonbit_decref(_M0L8_2afieldS3373.$0);
    _M0L8_2afieldS3372
    = (struct _M0TPC16string10StringView){
      _M0L4selfS217->$1_1, _M0L4selfS217->$1_2, _M0L4selfS217->$1_0
    };
    moonbit_decref(_M0L8_2afieldS3372.$0);
    _M0L8_2afieldS3371
    = (struct _M0TPC16string10StringView){
      _M0L4selfS217->$0_1, _M0L4selfS217->$0_2, _M0L4selfS217->$0_0
    };
    moonbit_decref(_M0L8_2afieldS3371.$0);
    #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    moonbit_free(_M0L4selfS217);
  }
  _M0L11end__columnS1555 = _M0L8_2afieldS3184;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1554
  = _M0IPC16string10StringViewPB4Show10to__string(_M0L11end__columnS1555);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3183
  = moonbit_add_string((moonbit_string_t)moonbit_string_literal_175.data, _M0L6_2atmpS1554);
  moonbit_decref(_M0L6_2atmpS1554);
  _M0L6_2atmpS1553 = _M0L6_2atmpS3183;
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS3182
  = moonbit_add_string(_M0L6_2atmpS1553, (moonbit_string_t)moonbit_string_literal_7.data);
  moonbit_decref(_M0L6_2atmpS1553);
  _M0L6_2atmpS1552 = _M0L6_2atmpS3182;
  moonbit_incref(_M0L2sbS216);
  #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L2sbS216, _M0L6_2atmpS1552);
  #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L2sbS216);
}

int32_t _M0IPB13StringBuilderPB6Logger13write__string(
  struct _M0TPB13StringBuilder* _M0L4selfS214,
  moonbit_string_t _M0L3strS215
) {
  int32_t _M0L3lenS1528;
  int32_t _M0L6_2atmpS1530;
  int32_t _M0L6_2atmpS1529;
  int32_t _M0L6_2atmpS1527;
  moonbit_bytes_t _M0L8_2afieldS3196;
  moonbit_bytes_t _M0L4dataS1531;
  int32_t _M0L3lenS1532;
  int32_t _M0L6_2atmpS1533;
  int32_t _M0L3lenS1535;
  int32_t _M0L6_2atmpS3195;
  int32_t _M0L6_2atmpS1537;
  int32_t _M0L6_2atmpS1536;
  int32_t _M0L6_2atmpS1534;
  #line 66 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1528 = _M0L4selfS214->$1;
  _M0L6_2atmpS1530 = Moonbit_array_length(_M0L3strS215);
  _M0L6_2atmpS1529 = _M0L6_2atmpS1530 * 2;
  _M0L6_2atmpS1527 = _M0L3lenS1528 + _M0L6_2atmpS1529;
  moonbit_incref(_M0L4selfS214);
  #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS214, _M0L6_2atmpS1527);
  _M0L8_2afieldS3196 = _M0L4selfS214->$0;
  _M0L4dataS1531 = _M0L8_2afieldS3196;
  _M0L3lenS1532 = _M0L4selfS214->$1;
  _M0L6_2atmpS1533 = Moonbit_array_length(_M0L3strS215);
  moonbit_incref(_M0L4dataS1531);
  moonbit_incref(_M0L3strS215);
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray18blit__from__string(_M0L4dataS1531, _M0L3lenS1532, _M0L3strS215, 0, _M0L6_2atmpS1533);
  _M0L3lenS1535 = _M0L4selfS214->$1;
  _M0L6_2atmpS3195 = Moonbit_array_length(_M0L3strS215);
  moonbit_decref(_M0L3strS215);
  _M0L6_2atmpS1537 = _M0L6_2atmpS3195;
  _M0L6_2atmpS1536 = _M0L6_2atmpS1537 * 2;
  _M0L6_2atmpS1534 = _M0L3lenS1535 + _M0L6_2atmpS1536;
  _M0L4selfS214->$1 = _M0L6_2atmpS1534;
  moonbit_decref(_M0L4selfS214);
  return 0;
}

int32_t _M0MPC15array10FixedArray18blit__from__string(
  moonbit_bytes_t _M0L4selfS206,
  int32_t _M0L13bytes__offsetS201,
  moonbit_string_t _M0L3strS208,
  int32_t _M0L11str__offsetS204,
  int32_t _M0L6lengthS202
) {
  int32_t _M0L6_2atmpS1526;
  int32_t _M0L6_2atmpS1525;
  int32_t _M0L2e1S200;
  int32_t _M0L6_2atmpS1524;
  int32_t _M0L2e2S203;
  int32_t _M0L4len1S205;
  int32_t _M0L4len2S207;
  int32_t _if__result_3561;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
  _M0L6_2atmpS1526 = _M0L6lengthS202 * 2;
  _M0L6_2atmpS1525 = _M0L13bytes__offsetS201 + _M0L6_2atmpS1526;
  _M0L2e1S200 = _M0L6_2atmpS1525 - 1;
  _M0L6_2atmpS1524 = _M0L11str__offsetS204 + _M0L6lengthS202;
  _M0L2e2S203 = _M0L6_2atmpS1524 - 1;
  _M0L4len1S205 = Moonbit_array_length(_M0L4selfS206);
  _M0L4len2S207 = Moonbit_array_length(_M0L3strS208);
  if (_M0L6lengthS202 >= 0) {
    if (_M0L13bytes__offsetS201 >= 0) {
      if (_M0L2e1S200 < _M0L4len1S205) {
        if (_M0L11str__offsetS204 >= 0) {
          _if__result_3561 = _M0L2e2S203 < _M0L4len2S207;
        } else {
          _if__result_3561 = 0;
        }
      } else {
        _if__result_3561 = 0;
      }
    } else {
      _if__result_3561 = 0;
    }
  } else {
    _if__result_3561 = 0;
  }
  if (_if__result_3561) {
    int32_t _M0L16end__str__offsetS209 =
      _M0L11str__offsetS204 + _M0L6lengthS202;
    int32_t _M0L1iS210 = _M0L11str__offsetS204;
    int32_t _M0L1jS211 = _M0L13bytes__offsetS201;
    while (1) {
      if (_M0L1iS210 < _M0L16end__str__offsetS209) {
        int32_t _M0L6_2atmpS1521 = _M0L3strS208[_M0L1iS210];
        int32_t _M0L6_2atmpS1520 = (int32_t)_M0L6_2atmpS1521;
        uint32_t _M0L1cS212 = *(uint32_t*)&_M0L6_2atmpS1520;
        uint32_t _M0L6_2atmpS1516 = _M0L1cS212 & 255u;
        int32_t _M0L6_2atmpS1515;
        int32_t _M0L6_2atmpS1517;
        uint32_t _M0L6_2atmpS1519;
        int32_t _M0L6_2atmpS1518;
        int32_t _M0L6_2atmpS1522;
        int32_t _M0L6_2atmpS1523;
        #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1515 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1516);
        if (
          _M0L1jS211 < 0 || _M0L1jS211 >= Moonbit_array_length(_M0L4selfS206)
        ) {
          #line 141 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS206[_M0L1jS211] = _M0L6_2atmpS1515;
        _M0L6_2atmpS1517 = _M0L1jS211 + 1;
        _M0L6_2atmpS1519 = _M0L1cS212 >> 8;
        #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
        _M0L6_2atmpS1518 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1519);
        if (
          _M0L6_2atmpS1517 < 0
          || _M0L6_2atmpS1517 >= Moonbit_array_length(_M0L4selfS206)
        ) {
          #line 142 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
          moonbit_panic();
        }
        _M0L4selfS206[_M0L6_2atmpS1517] = _M0L6_2atmpS1518;
        _M0L6_2atmpS1522 = _M0L1iS210 + 1;
        _M0L6_2atmpS1523 = _M0L1jS211 + 2;
        _M0L1iS210 = _M0L6_2atmpS1522;
        _M0L1jS211 = _M0L6_2atmpS1523;
        continue;
      } else {
        moonbit_decref(_M0L3strS208);
        moonbit_decref(_M0L4selfS206);
      }
      break;
    }
  } else {
    moonbit_decref(_M0L3strS208);
    moonbit_decref(_M0L4selfS206);
    #line 137 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    moonbit_panic();
  }
  return 0;
}

int32_t _M0MPB13StringBuilder13write__objectGRPC16string10StringViewE(
  struct _M0TPB13StringBuilder* _M0L4selfS199,
  struct _M0TPC16string10StringView _M0L3objS198
) {
  struct _M0TPB6Logger _M0L6_2atmpS1514;
  #line 17 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0L6_2atmpS1514
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L4selfS199
  };
  #line 21 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder.mbt"
  _M0IPC16string10StringViewPB4Show6output(_M0L3objS198, _M0L6_2atmpS1514);
  return 0;
}

struct _M0TPB13SourceLocRepr* _M0MPB13SourceLocRepr5parse(
  moonbit_string_t _M0L4reprS144
) {
  int32_t _M0L6_2atmpS1513;
  struct _M0TPC16string10StringView _M0L7_2abindS143;
  moonbit_string_t _M0L7_2adataS145;
  int32_t _M0L8_2astartS146;
  int32_t _M0L6_2atmpS1512;
  int32_t _M0L6_2aendS147;
  int32_t _M0Lm9_2acursorS148;
  int32_t _M0Lm13accept__stateS149;
  int32_t _M0Lm10match__endS150;
  int32_t _M0Lm20match__tag__saver__0S151;
  int32_t _M0Lm20match__tag__saver__1S152;
  int32_t _M0Lm20match__tag__saver__2S153;
  int32_t _M0Lm20match__tag__saver__3S154;
  int32_t _M0Lm20match__tag__saver__4S155;
  int32_t _M0Lm6tag__0S156;
  int32_t _M0Lm6tag__1S157;
  int32_t _M0Lm9tag__1__1S158;
  int32_t _M0Lm9tag__1__2S159;
  int32_t _M0Lm6tag__3S160;
  int32_t _M0Lm6tag__2S161;
  int32_t _M0Lm9tag__2__1S162;
  int32_t _M0Lm6tag__4S163;
  int32_t _M0L6_2atmpS1470;
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1513 = Moonbit_array_length(_M0L4reprS144);
  _M0L7_2abindS143
  = (struct _M0TPC16string10StringView){
    0, _M0L6_2atmpS1513, _M0L4reprS144
  };
  moonbit_incref(_M0L7_2abindS143.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L7_2adataS145 = _M0MPC16string10StringView4data(_M0L7_2abindS143);
  moonbit_incref(_M0L7_2abindS143.$0);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L8_2astartS146
  = _M0MPC16string10StringView13start__offset(_M0L7_2abindS143);
  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
  _M0L6_2atmpS1512 = _M0MPC16string10StringView6length(_M0L7_2abindS143);
  _M0L6_2aendS147 = _M0L8_2astartS146 + _M0L6_2atmpS1512;
  _M0Lm9_2acursorS148 = _M0L8_2astartS146;
  _M0Lm13accept__stateS149 = -1;
  _M0Lm10match__endS150 = -1;
  _M0Lm20match__tag__saver__0S151 = -1;
  _M0Lm20match__tag__saver__1S152 = -1;
  _M0Lm20match__tag__saver__2S153 = -1;
  _M0Lm20match__tag__saver__3S154 = -1;
  _M0Lm20match__tag__saver__4S155 = -1;
  _M0Lm6tag__0S156 = -1;
  _M0Lm6tag__1S157 = -1;
  _M0Lm9tag__1__1S158 = -1;
  _M0Lm9tag__1__2S159 = -1;
  _M0Lm6tag__3S160 = -1;
  _M0Lm6tag__2S161 = -1;
  _M0Lm9tag__2__1S162 = -1;
  _M0Lm6tag__4S163 = -1;
  _M0L6_2atmpS1470 = _M0Lm9_2acursorS148;
  if (_M0L6_2atmpS1470 < _M0L6_2aendS147) {
    int32_t _M0L6_2atmpS1472 = _M0Lm9_2acursorS148;
    int32_t _M0L6_2atmpS1471;
    moonbit_incref(_M0L7_2adataS145);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
    _M0L6_2atmpS1471
    = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1472);
    if (_M0L6_2atmpS1471 == 64) {
      int32_t _M0L6_2atmpS1473 = _M0Lm9_2acursorS148;
      _M0Lm9_2acursorS148 = _M0L6_2atmpS1473 + 1;
      while (1) {
        int32_t _M0L6_2atmpS1474;
        _M0Lm6tag__0S156 = _M0Lm9_2acursorS148;
        _M0L6_2atmpS1474 = _M0Lm9_2acursorS148;
        if (_M0L6_2atmpS1474 < _M0L6_2aendS147) {
          int32_t _M0L6_2atmpS1511 = _M0Lm9_2acursorS148;
          int32_t _M0L10next__charS171;
          int32_t _M0L6_2atmpS1475;
          moonbit_incref(_M0L7_2adataS145);
          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
          _M0L10next__charS171
          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1511);
          _M0L6_2atmpS1475 = _M0Lm9_2acursorS148;
          _M0Lm9_2acursorS148 = _M0L6_2atmpS1475 + 1;
          if (_M0L10next__charS171 == 58) {
            int32_t _M0L6_2atmpS1476 = _M0Lm9_2acursorS148;
            if (_M0L6_2atmpS1476 < _M0L6_2aendS147) {
              int32_t _M0L6_2atmpS1477 = _M0Lm9_2acursorS148;
              int32_t _M0L12dispatch__15S172;
              _M0Lm9_2acursorS148 = _M0L6_2atmpS1477 + 1;
              _M0L12dispatch__15S172 = 0;
              loop__label__15_175:;
              while (1) {
                int32_t _M0L6_2atmpS1478;
                switch (_M0L12dispatch__15S172) {
                  case 3: {
                    int32_t _M0L6_2atmpS1481;
                    _M0Lm9tag__1__2S159 = _M0Lm9tag__1__1S158;
                    _M0Lm9tag__1__1S158 = _M0Lm6tag__1S157;
                    _M0Lm6tag__1S157 = _M0Lm9_2acursorS148;
                    _M0L6_2atmpS1481 = _M0Lm9_2acursorS148;
                    if (_M0L6_2atmpS1481 < _M0L6_2aendS147) {
                      int32_t _M0L6_2atmpS1486 = _M0Lm9_2acursorS148;
                      int32_t _M0L10next__charS179;
                      int32_t _M0L6_2atmpS1482;
                      moonbit_incref(_M0L7_2adataS145);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS179
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1486);
                      _M0L6_2atmpS1482 = _M0Lm9_2acursorS148;
                      _M0Lm9_2acursorS148 = _M0L6_2atmpS1482 + 1;
                      if (_M0L10next__charS179 < 58) {
                        if (_M0L10next__charS179 < 48) {
                          goto join_178;
                        } else {
                          int32_t _M0L6_2atmpS1483;
                          _M0Lm6tag__1S157 = _M0Lm9_2acursorS148;
                          _M0Lm9tag__2__1S162 = _M0Lm6tag__2S161;
                          _M0Lm6tag__2S161 = _M0Lm9_2acursorS148;
                          _M0Lm6tag__3S160 = _M0Lm9_2acursorS148;
                          _M0L6_2atmpS1483 = _M0Lm9_2acursorS148;
                          if (_M0L6_2atmpS1483 < _M0L6_2aendS147) {
                            int32_t _M0L6_2atmpS1485 = _M0Lm9_2acursorS148;
                            int32_t _M0L10next__charS181;
                            int32_t _M0L6_2atmpS1484;
                            moonbit_incref(_M0L7_2adataS145);
                            #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                            _M0L10next__charS181
                            = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1485);
                            _M0L6_2atmpS1484 = _M0Lm9_2acursorS148;
                            _M0Lm9_2acursorS148 = _M0L6_2atmpS1484 + 1;
                            if (_M0L10next__charS181 < 48) {
                              if (_M0L10next__charS181 == 45) {
                                goto join_173;
                              } else {
                                goto join_180;
                              }
                            } else if (_M0L10next__charS181 > 57) {
                              if (_M0L10next__charS181 < 59) {
                                _M0L12dispatch__15S172 = 3;
                                goto loop__label__15_175;
                              } else {
                                goto join_180;
                              }
                            } else {
                              _M0L12dispatch__15S172 = 6;
                              goto loop__label__15_175;
                            }
                            join_180:;
                            _M0L12dispatch__15S172 = 0;
                            goto loop__label__15_175;
                          } else {
                            goto join_164;
                          }
                        }
                      } else if (_M0L10next__charS179 > 58) {
                        goto join_178;
                      } else {
                        _M0L12dispatch__15S172 = 1;
                        goto loop__label__15_175;
                      }
                      join_178:;
                      _M0L12dispatch__15S172 = 0;
                      goto loop__label__15_175;
                    } else {
                      goto join_164;
                    }
                    break;
                  }
                  
                  case 2: {
                    int32_t _M0L6_2atmpS1487;
                    _M0Lm6tag__1S157 = _M0Lm9_2acursorS148;
                    _M0Lm6tag__2S161 = _M0Lm9_2acursorS148;
                    _M0L6_2atmpS1487 = _M0Lm9_2acursorS148;
                    if (_M0L6_2atmpS1487 < _M0L6_2aendS147) {
                      int32_t _M0L6_2atmpS1489 = _M0Lm9_2acursorS148;
                      int32_t _M0L10next__charS183;
                      int32_t _M0L6_2atmpS1488;
                      moonbit_incref(_M0L7_2adataS145);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS183
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1489);
                      _M0L6_2atmpS1488 = _M0Lm9_2acursorS148;
                      _M0Lm9_2acursorS148 = _M0L6_2atmpS1488 + 1;
                      if (_M0L10next__charS183 < 58) {
                        if (_M0L10next__charS183 < 48) {
                          goto join_182;
                        } else {
                          _M0L12dispatch__15S172 = 2;
                          goto loop__label__15_175;
                        }
                      } else if (_M0L10next__charS183 > 58) {
                        goto join_182;
                      } else {
                        _M0L12dispatch__15S172 = 3;
                        goto loop__label__15_175;
                      }
                      join_182:;
                      _M0L12dispatch__15S172 = 0;
                      goto loop__label__15_175;
                    } else {
                      goto join_164;
                    }
                    break;
                  }
                  
                  case 0: {
                    int32_t _M0L6_2atmpS1490;
                    _M0Lm6tag__1S157 = _M0Lm9_2acursorS148;
                    _M0L6_2atmpS1490 = _M0Lm9_2acursorS148;
                    if (_M0L6_2atmpS1490 < _M0L6_2aendS147) {
                      int32_t _M0L6_2atmpS1492 = _M0Lm9_2acursorS148;
                      int32_t _M0L10next__charS184;
                      int32_t _M0L6_2atmpS1491;
                      moonbit_incref(_M0L7_2adataS145);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS184
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1492);
                      _M0L6_2atmpS1491 = _M0Lm9_2acursorS148;
                      _M0Lm9_2acursorS148 = _M0L6_2atmpS1491 + 1;
                      if (_M0L10next__charS184 == 58) {
                        _M0L12dispatch__15S172 = 1;
                        goto loop__label__15_175;
                      } else {
                        _M0L12dispatch__15S172 = 0;
                        goto loop__label__15_175;
                      }
                    } else {
                      goto join_164;
                    }
                    break;
                  }
                  
                  case 4: {
                    int32_t _M0L6_2atmpS1493;
                    _M0Lm6tag__1S157 = _M0Lm9_2acursorS148;
                    _M0Lm6tag__4S163 = _M0Lm9_2acursorS148;
                    _M0L6_2atmpS1493 = _M0Lm9_2acursorS148;
                    if (_M0L6_2atmpS1493 < _M0L6_2aendS147) {
                      int32_t _M0L6_2atmpS1501 = _M0Lm9_2acursorS148;
                      int32_t _M0L10next__charS186;
                      int32_t _M0L6_2atmpS1494;
                      moonbit_incref(_M0L7_2adataS145);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS186
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1501);
                      _M0L6_2atmpS1494 = _M0Lm9_2acursorS148;
                      _M0Lm9_2acursorS148 = _M0L6_2atmpS1494 + 1;
                      if (_M0L10next__charS186 < 58) {
                        if (_M0L10next__charS186 < 48) {
                          goto join_185;
                        } else {
                          _M0L12dispatch__15S172 = 4;
                          goto loop__label__15_175;
                        }
                      } else if (_M0L10next__charS186 > 58) {
                        goto join_185;
                      } else {
                        int32_t _M0L6_2atmpS1495;
                        _M0Lm9tag__1__2S159 = _M0Lm9tag__1__1S158;
                        _M0Lm9tag__1__1S158 = _M0Lm6tag__1S157;
                        _M0Lm6tag__1S157 = _M0Lm9_2acursorS148;
                        _M0L6_2atmpS1495 = _M0Lm9_2acursorS148;
                        if (_M0L6_2atmpS1495 < _M0L6_2aendS147) {
                          int32_t _M0L6_2atmpS1500 = _M0Lm9_2acursorS148;
                          int32_t _M0L10next__charS188;
                          int32_t _M0L6_2atmpS1496;
                          moonbit_incref(_M0L7_2adataS145);
                          #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                          _M0L10next__charS188
                          = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1500);
                          _M0L6_2atmpS1496 = _M0Lm9_2acursorS148;
                          _M0Lm9_2acursorS148 = _M0L6_2atmpS1496 + 1;
                          if (_M0L10next__charS188 < 58) {
                            if (_M0L10next__charS188 < 48) {
                              goto join_187;
                            } else {
                              int32_t _M0L6_2atmpS1497;
                              _M0Lm6tag__1S157 = _M0Lm9_2acursorS148;
                              _M0Lm9tag__2__1S162 = _M0Lm6tag__2S161;
                              _M0Lm6tag__2S161 = _M0Lm9_2acursorS148;
                              _M0L6_2atmpS1497 = _M0Lm9_2acursorS148;
                              if (_M0L6_2atmpS1497 < _M0L6_2aendS147) {
                                int32_t _M0L6_2atmpS1499 =
                                  _M0Lm9_2acursorS148;
                                int32_t _M0L10next__charS190;
                                int32_t _M0L6_2atmpS1498;
                                moonbit_incref(_M0L7_2adataS145);
                                #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                                _M0L10next__charS190
                                = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1499);
                                _M0L6_2atmpS1498 = _M0Lm9_2acursorS148;
                                _M0Lm9_2acursorS148 = _M0L6_2atmpS1498 + 1;
                                if (_M0L10next__charS190 < 58) {
                                  if (_M0L10next__charS190 < 48) {
                                    goto join_189;
                                  } else {
                                    _M0L12dispatch__15S172 = 5;
                                    goto loop__label__15_175;
                                  }
                                } else if (_M0L10next__charS190 > 58) {
                                  goto join_189;
                                } else {
                                  _M0L12dispatch__15S172 = 3;
                                  goto loop__label__15_175;
                                }
                                join_189:;
                                _M0L12dispatch__15S172 = 0;
                                goto loop__label__15_175;
                              } else {
                                goto join_177;
                              }
                            }
                          } else if (_M0L10next__charS188 > 58) {
                            goto join_187;
                          } else {
                            _M0L12dispatch__15S172 = 1;
                            goto loop__label__15_175;
                          }
                          join_187:;
                          _M0L12dispatch__15S172 = 0;
                          goto loop__label__15_175;
                        } else {
                          goto join_164;
                        }
                      }
                      join_185:;
                      _M0L12dispatch__15S172 = 0;
                      goto loop__label__15_175;
                    } else {
                      goto join_164;
                    }
                    break;
                  }
                  
                  case 5: {
                    int32_t _M0L6_2atmpS1502;
                    _M0Lm6tag__1S157 = _M0Lm9_2acursorS148;
                    _M0Lm6tag__2S161 = _M0Lm9_2acursorS148;
                    _M0L6_2atmpS1502 = _M0Lm9_2acursorS148;
                    if (_M0L6_2atmpS1502 < _M0L6_2aendS147) {
                      int32_t _M0L6_2atmpS1504 = _M0Lm9_2acursorS148;
                      int32_t _M0L10next__charS192;
                      int32_t _M0L6_2atmpS1503;
                      moonbit_incref(_M0L7_2adataS145);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS192
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1504);
                      _M0L6_2atmpS1503 = _M0Lm9_2acursorS148;
                      _M0Lm9_2acursorS148 = _M0L6_2atmpS1503 + 1;
                      if (_M0L10next__charS192 < 58) {
                        if (_M0L10next__charS192 < 48) {
                          goto join_191;
                        } else {
                          _M0L12dispatch__15S172 = 5;
                          goto loop__label__15_175;
                        }
                      } else if (_M0L10next__charS192 > 58) {
                        goto join_191;
                      } else {
                        _M0L12dispatch__15S172 = 3;
                        goto loop__label__15_175;
                      }
                      join_191:;
                      _M0L12dispatch__15S172 = 0;
                      goto loop__label__15_175;
                    } else {
                      goto join_177;
                    }
                    break;
                  }
                  
                  case 6: {
                    int32_t _M0L6_2atmpS1505;
                    _M0Lm6tag__1S157 = _M0Lm9_2acursorS148;
                    _M0Lm6tag__2S161 = _M0Lm9_2acursorS148;
                    _M0Lm6tag__3S160 = _M0Lm9_2acursorS148;
                    _M0L6_2atmpS1505 = _M0Lm9_2acursorS148;
                    if (_M0L6_2atmpS1505 < _M0L6_2aendS147) {
                      int32_t _M0L6_2atmpS1507 = _M0Lm9_2acursorS148;
                      int32_t _M0L10next__charS194;
                      int32_t _M0L6_2atmpS1506;
                      moonbit_incref(_M0L7_2adataS145);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS194
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1507);
                      _M0L6_2atmpS1506 = _M0Lm9_2acursorS148;
                      _M0Lm9_2acursorS148 = _M0L6_2atmpS1506 + 1;
                      if (_M0L10next__charS194 < 48) {
                        if (_M0L10next__charS194 == 45) {
                          goto join_173;
                        } else {
                          goto join_193;
                        }
                      } else if (_M0L10next__charS194 > 57) {
                        if (_M0L10next__charS194 < 59) {
                          _M0L12dispatch__15S172 = 3;
                          goto loop__label__15_175;
                        } else {
                          goto join_193;
                        }
                      } else {
                        _M0L12dispatch__15S172 = 6;
                        goto loop__label__15_175;
                      }
                      join_193:;
                      _M0L12dispatch__15S172 = 0;
                      goto loop__label__15_175;
                    } else {
                      goto join_164;
                    }
                    break;
                  }
                  
                  case 1: {
                    int32_t _M0L6_2atmpS1508;
                    _M0Lm9tag__1__1S158 = _M0Lm6tag__1S157;
                    _M0Lm6tag__1S157 = _M0Lm9_2acursorS148;
                    _M0L6_2atmpS1508 = _M0Lm9_2acursorS148;
                    if (_M0L6_2atmpS1508 < _M0L6_2aendS147) {
                      int32_t _M0L6_2atmpS1510 = _M0Lm9_2acursorS148;
                      int32_t _M0L10next__charS196;
                      int32_t _M0L6_2atmpS1509;
                      moonbit_incref(_M0L7_2adataS145);
                      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                      _M0L10next__charS196
                      = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1510);
                      _M0L6_2atmpS1509 = _M0Lm9_2acursorS148;
                      _M0Lm9_2acursorS148 = _M0L6_2atmpS1509 + 1;
                      if (_M0L10next__charS196 < 58) {
                        if (_M0L10next__charS196 < 48) {
                          goto join_195;
                        } else {
                          _M0L12dispatch__15S172 = 2;
                          goto loop__label__15_175;
                        }
                      } else if (_M0L10next__charS196 > 58) {
                        goto join_195;
                      } else {
                        _M0L12dispatch__15S172 = 1;
                        goto loop__label__15_175;
                      }
                      join_195:;
                      _M0L12dispatch__15S172 = 0;
                      goto loop__label__15_175;
                    } else {
                      goto join_164;
                    }
                    break;
                  }
                  default: {
                    goto join_164;
                    break;
                  }
                }
                join_177:;
                _M0Lm6tag__1S157 = _M0Lm9tag__1__2S159;
                _M0Lm6tag__2S161 = _M0Lm9tag__2__1S162;
                _M0Lm20match__tag__saver__0S151 = _M0Lm6tag__0S156;
                _M0Lm20match__tag__saver__1S152 = _M0Lm6tag__1S157;
                _M0Lm20match__tag__saver__2S153 = _M0Lm6tag__2S161;
                _M0Lm20match__tag__saver__3S154 = _M0Lm6tag__3S160;
                _M0Lm20match__tag__saver__4S155 = _M0Lm6tag__4S163;
                _M0Lm13accept__stateS149 = 0;
                _M0Lm10match__endS150 = _M0Lm9_2acursorS148;
                goto join_164;
                join_173:;
                _M0Lm9tag__1__1S158 = _M0Lm9tag__1__2S159;
                _M0Lm6tag__1S157 = _M0Lm9_2acursorS148;
                _M0Lm6tag__2S161 = _M0Lm9tag__2__1S162;
                _M0L6_2atmpS1478 = _M0Lm9_2acursorS148;
                if (_M0L6_2atmpS1478 < _M0L6_2aendS147) {
                  int32_t _M0L6_2atmpS1480 = _M0Lm9_2acursorS148;
                  int32_t _M0L10next__charS176;
                  int32_t _M0L6_2atmpS1479;
                  moonbit_incref(_M0L7_2adataS145);
                  #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
                  _M0L10next__charS176
                  = _M0MPC16string6String20unsafe__charcode__at(_M0L7_2adataS145, _M0L6_2atmpS1480);
                  _M0L6_2atmpS1479 = _M0Lm9_2acursorS148;
                  _M0Lm9_2acursorS148 = _M0L6_2atmpS1479 + 1;
                  if (_M0L10next__charS176 < 58) {
                    if (_M0L10next__charS176 < 48) {
                      goto join_174;
                    } else {
                      _M0L12dispatch__15S172 = 4;
                      continue;
                    }
                  } else if (_M0L10next__charS176 > 58) {
                    goto join_174;
                  } else {
                    _M0L12dispatch__15S172 = 1;
                    continue;
                  }
                  join_174:;
                  _M0L12dispatch__15S172 = 0;
                  continue;
                } else {
                  goto join_164;
                }
                break;
              }
            } else {
              goto join_164;
            }
          } else {
            continue;
          }
        } else {
          goto join_164;
        }
        break;
      }
    } else {
      goto join_164;
    }
  } else {
    goto join_164;
  }
  join_164:;
  switch (_M0Lm13accept__stateS149) {
    case 0: {
      int32_t _M0L6_2atmpS1469 = _M0Lm20match__tag__saver__1S152;
      int32_t _M0L6_2atmpS1468 = _M0L6_2atmpS1469 + 1;
      int64_t _M0L6_2atmpS1465 = (int64_t)_M0L6_2atmpS1468;
      int32_t _M0L6_2atmpS1467 = _M0Lm20match__tag__saver__2S153;
      int64_t _M0L6_2atmpS1466 = (int64_t)_M0L6_2atmpS1467;
      struct _M0TPC16string10StringView _M0L11start__lineS165;
      int32_t _M0L6_2atmpS1464;
      int32_t _M0L6_2atmpS1463;
      int64_t _M0L6_2atmpS1460;
      int32_t _M0L6_2atmpS1462;
      int64_t _M0L6_2atmpS1461;
      struct _M0TPC16string10StringView _M0L13start__columnS166;
      int32_t _M0L6_2atmpS1459;
      int64_t _M0L6_2atmpS1456;
      int32_t _M0L6_2atmpS1458;
      int64_t _M0L6_2atmpS1457;
      struct _M0TPC16string10StringView _M0L3pkgS167;
      int32_t _M0L6_2atmpS1455;
      int32_t _M0L6_2atmpS1454;
      int64_t _M0L6_2atmpS1451;
      int32_t _M0L6_2atmpS1453;
      int64_t _M0L6_2atmpS1452;
      struct _M0TPC16string10StringView _M0L8filenameS168;
      int32_t _M0L6_2atmpS1450;
      int32_t _M0L6_2atmpS1449;
      int64_t _M0L6_2atmpS1446;
      int32_t _M0L6_2atmpS1448;
      int64_t _M0L6_2atmpS1447;
      struct _M0TPC16string10StringView _M0L9end__lineS169;
      int32_t _M0L6_2atmpS1445;
      int32_t _M0L6_2atmpS1444;
      int64_t _M0L6_2atmpS1441;
      int32_t _M0L6_2atmpS1443;
      int64_t _M0L6_2atmpS1442;
      struct _M0TPC16string10StringView _M0L11end__columnS170;
      struct _M0TPB13SourceLocRepr* _block_3578;
      moonbit_incref(_M0L7_2adataS145);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11start__lineS165
      = _M0MPC16string6String4view(_M0L7_2adataS145, _M0L6_2atmpS1465, _M0L6_2atmpS1466);
      _M0L6_2atmpS1464 = _M0Lm20match__tag__saver__2S153;
      _M0L6_2atmpS1463 = _M0L6_2atmpS1464 + 1;
      _M0L6_2atmpS1460 = (int64_t)_M0L6_2atmpS1463;
      _M0L6_2atmpS1462 = _M0Lm20match__tag__saver__3S154;
      _M0L6_2atmpS1461 = (int64_t)_M0L6_2atmpS1462;
      moonbit_incref(_M0L7_2adataS145);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L13start__columnS166
      = _M0MPC16string6String4view(_M0L7_2adataS145, _M0L6_2atmpS1460, _M0L6_2atmpS1461);
      _M0L6_2atmpS1459 = _M0L8_2astartS146 + 1;
      _M0L6_2atmpS1456 = (int64_t)_M0L6_2atmpS1459;
      _M0L6_2atmpS1458 = _M0Lm20match__tag__saver__0S151;
      _M0L6_2atmpS1457 = (int64_t)_M0L6_2atmpS1458;
      moonbit_incref(_M0L7_2adataS145);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L3pkgS167
      = _M0MPC16string6String4view(_M0L7_2adataS145, _M0L6_2atmpS1456, _M0L6_2atmpS1457);
      _M0L6_2atmpS1455 = _M0Lm20match__tag__saver__0S151;
      _M0L6_2atmpS1454 = _M0L6_2atmpS1455 + 1;
      _M0L6_2atmpS1451 = (int64_t)_M0L6_2atmpS1454;
      _M0L6_2atmpS1453 = _M0Lm20match__tag__saver__1S152;
      _M0L6_2atmpS1452 = (int64_t)_M0L6_2atmpS1453;
      moonbit_incref(_M0L7_2adataS145);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L8filenameS168
      = _M0MPC16string6String4view(_M0L7_2adataS145, _M0L6_2atmpS1451, _M0L6_2atmpS1452);
      _M0L6_2atmpS1450 = _M0Lm20match__tag__saver__3S154;
      _M0L6_2atmpS1449 = _M0L6_2atmpS1450 + 1;
      _M0L6_2atmpS1446 = (int64_t)_M0L6_2atmpS1449;
      _M0L6_2atmpS1448 = _M0Lm20match__tag__saver__4S155;
      _M0L6_2atmpS1447 = (int64_t)_M0L6_2atmpS1448;
      moonbit_incref(_M0L7_2adataS145);
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L9end__lineS169
      = _M0MPC16string6String4view(_M0L7_2adataS145, _M0L6_2atmpS1446, _M0L6_2atmpS1447);
      _M0L6_2atmpS1445 = _M0Lm20match__tag__saver__4S155;
      _M0L6_2atmpS1444 = _M0L6_2atmpS1445 + 1;
      _M0L6_2atmpS1441 = (int64_t)_M0L6_2atmpS1444;
      _M0L6_2atmpS1443 = _M0Lm10match__endS150;
      _M0L6_2atmpS1442 = (int64_t)_M0L6_2atmpS1443;
      #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      _M0L11end__columnS170
      = _M0MPC16string6String4view(_M0L7_2adataS145, _M0L6_2atmpS1441, _M0L6_2atmpS1442);
      _block_3578
      = (struct _M0TPB13SourceLocRepr*)moonbit_malloc(sizeof(struct _M0TPB13SourceLocRepr));
      Moonbit_object_header(_block_3578)->meta
      = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13SourceLocRepr, $0_0) >> 2, 6, 0);
      _block_3578->$0_0 = _M0L3pkgS167.$0;
      _block_3578->$0_1 = _M0L3pkgS167.$1;
      _block_3578->$0_2 = _M0L3pkgS167.$2;
      _block_3578->$1_0 = _M0L8filenameS168.$0;
      _block_3578->$1_1 = _M0L8filenameS168.$1;
      _block_3578->$1_2 = _M0L8filenameS168.$2;
      _block_3578->$2_0 = _M0L11start__lineS165.$0;
      _block_3578->$2_1 = _M0L11start__lineS165.$1;
      _block_3578->$2_2 = _M0L11start__lineS165.$2;
      _block_3578->$3_0 = _M0L13start__columnS166.$0;
      _block_3578->$3_1 = _M0L13start__columnS166.$1;
      _block_3578->$3_2 = _M0L13start__columnS166.$2;
      _block_3578->$4_0 = _M0L9end__lineS169.$0;
      _block_3578->$4_1 = _M0L9end__lineS169.$1;
      _block_3578->$4_2 = _M0L9end__lineS169.$2;
      _block_3578->$5_0 = _M0L11end__columnS170.$0;
      _block_3578->$5_1 = _M0L11end__columnS170.$1;
      _block_3578->$5_2 = _M0L11end__columnS170.$2;
      return _block_3578;
      break;
    }
    default: {
      moonbit_decref(_M0L7_2adataS145);
      #line 99 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\autoloc.mbt"
      moonbit_panic();
      break;
    }
  }
}

moonbit_string_t _M0MPC15array5Array2atGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS141,
  int32_t _M0L5indexS142
) {
  int32_t _M0L3lenS140;
  int32_t _if__result_3579;
  #line 183 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
  _M0L3lenS140 = _M0L4selfS141->$1;
  if (_M0L5indexS142 >= 0) {
    _if__result_3579 = _M0L5indexS142 < _M0L3lenS140;
  } else {
    _if__result_3579 = 0;
  }
  if (_if__result_3579) {
    moonbit_string_t* _M0L6_2atmpS1440;
    moonbit_string_t _M0L6_2atmpS3197;
    #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    _M0L6_2atmpS1440 = _M0MPC15array5Array6bufferGsE(_M0L4selfS141);
    if (
      _M0L5indexS142 < 0
      || _M0L5indexS142 >= Moonbit_array_length(_M0L6_2atmpS1440)
    ) {
      #line 188 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3197 = (moonbit_string_t)_M0L6_2atmpS1440[_M0L5indexS142];
    moonbit_incref(_M0L6_2atmpS3197);
    moonbit_decref(_M0L6_2atmpS1440);
    return _M0L6_2atmpS3197;
  } else {
    moonbit_decref(_M0L4selfS141);
    #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\array.mbt"
    moonbit_panic();
  }
}

int32_t _M0MPC15array5Array6lengthGsE(struct _M0TPB5ArrayGsE* _M0L4selfS139) {
  int32_t _M0L8_2afieldS3198;
  #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3198 = _M0L4selfS139->$1;
  moonbit_decref(_M0L4selfS139);
  return _M0L8_2afieldS3198;
}

moonbit_string_t* _M0MPC15array5Array6bufferGsE(
  struct _M0TPB5ArrayGsE* _M0L4selfS137
) {
  moonbit_string_t* _M0L8_2afieldS3199;
  int32_t _M0L6_2acntS3377;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3199 = _M0L4selfS137->$0;
  _M0L6_2acntS3377 = Moonbit_object_header(_M0L4selfS137)->rc;
  if (_M0L6_2acntS3377 > 1) {
    int32_t _M0L11_2anew__cntS3378 = _M0L6_2acntS3377 - 1;
    Moonbit_object_header(_M0L4selfS137)->rc = _M0L11_2anew__cntS3378;
    moonbit_incref(_M0L8_2afieldS3199);
  } else if (_M0L6_2acntS3377 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS137);
  }
  return _M0L8_2afieldS3199;
}

struct _M0TUsiE** _M0MPC15array5Array6bufferGUsiEE(
  struct _M0TPB5ArrayGUsiEE* _M0L4selfS138
) {
  struct _M0TUsiE** _M0L8_2afieldS3200;
  int32_t _M0L6_2acntS3379;
  #line 124 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
  _M0L8_2afieldS3200 = _M0L4selfS138->$0;
  _M0L6_2acntS3379 = Moonbit_object_header(_M0L4selfS138)->rc;
  if (_M0L6_2acntS3379 > 1) {
    int32_t _M0L11_2anew__cntS3380 = _M0L6_2acntS3379 - 1;
    Moonbit_object_header(_M0L4selfS138)->rc = _M0L11_2anew__cntS3380;
    moonbit_incref(_M0L8_2afieldS3200);
  } else if (_M0L6_2acntS3379 == 1) {
    #line 125 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\arraycore_nonjs.mbt"
    moonbit_free(_M0L4selfS138);
  }
  return _M0L8_2afieldS3200;
}

moonbit_string_t _M0MPC16string6String6escape(moonbit_string_t _M0L4selfS136) {
  struct _M0TPB13StringBuilder* _M0L3bufS135;
  struct _M0TPB6Logger _M0L6_2atmpS1439;
  #line 184 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  #line 185 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0L3bufS135 = _M0MPB13StringBuilder11new_2einner(0);
  moonbit_incref(_M0L3bufS135);
  _M0L6_2atmpS1439
  = (struct _M0TPB6Logger){
    _M0FP0119moonbitlang_2fcore_2fbuiltin_2fStringBuilder_2eas___40moonbitlang_2fcore_2fbuiltin_2eLogger_2estatic__method__table__id,
      _M0L3bufS135
  };
  #line 186 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  _M0IPC16string6StringPB4Show6output(_M0L4selfS136, _M0L6_2atmpS1439);
  #line 187 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\show.mbt"
  return _M0MPB13StringBuilder10to__string(_M0L3bufS135);
}

moonbit_string_t _M0FPB33base64__encode__string__codepoint(
  moonbit_string_t _M0L1sS129
) {
  int32_t _M0L17codepoint__lengthS128;
  int32_t _M0L6_2atmpS1438;
  moonbit_bytes_t _M0L4dataS130;
  int32_t _M0L1iS131;
  int32_t _M0L12utf16__indexS132;
  #line 102 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  moonbit_incref(_M0L1sS129);
  #line 104 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
  _M0L17codepoint__lengthS128
  = _M0MPC16string6String20char__length_2einner(_M0L1sS129, 0, 4294967296ll);
  _M0L6_2atmpS1438 = _M0L17codepoint__lengthS128 * 4;
  _M0L4dataS130 = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1438, 0);
  _M0L1iS131 = 0;
  _M0L12utf16__indexS132 = 0;
  while (1) {
    if (_M0L1iS131 < _M0L17codepoint__lengthS128) {
      int32_t _M0L6_2atmpS1435;
      int32_t _M0L1cS133;
      int32_t _M0L6_2atmpS1436;
      int32_t _M0L6_2atmpS1437;
      moonbit_incref(_M0L1sS129);
      #line 109 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1435
      = _M0MPC16string6String16unsafe__char__at(_M0L1sS129, _M0L12utf16__indexS132);
      _M0L1cS133 = _M0L6_2atmpS1435;
      if (_M0L1cS133 > 65535) {
        int32_t _M0L6_2atmpS1403 = _M0L1iS131 * 4;
        int32_t _M0L6_2atmpS1405 = _M0L1cS133 & 255;
        int32_t _M0L6_2atmpS1404 = _M0L6_2atmpS1405 & 0xff;
        int32_t _M0L6_2atmpS1410;
        int32_t _M0L6_2atmpS1406;
        int32_t _M0L6_2atmpS1409;
        int32_t _M0L6_2atmpS1408;
        int32_t _M0L6_2atmpS1407;
        int32_t _M0L6_2atmpS1415;
        int32_t _M0L6_2atmpS1411;
        int32_t _M0L6_2atmpS1414;
        int32_t _M0L6_2atmpS1413;
        int32_t _M0L6_2atmpS1412;
        int32_t _M0L6_2atmpS1420;
        int32_t _M0L6_2atmpS1416;
        int32_t _M0L6_2atmpS1419;
        int32_t _M0L6_2atmpS1418;
        int32_t _M0L6_2atmpS1417;
        int32_t _M0L6_2atmpS1421;
        int32_t _M0L6_2atmpS1422;
        if (
          _M0L6_2atmpS1403 < 0
          || _M0L6_2atmpS1403 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 111 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1403] = _M0L6_2atmpS1404;
        _M0L6_2atmpS1410 = _M0L1iS131 * 4;
        _M0L6_2atmpS1406 = _M0L6_2atmpS1410 + 1;
        _M0L6_2atmpS1409 = _M0L1cS133 >> 8;
        _M0L6_2atmpS1408 = _M0L6_2atmpS1409 & 255;
        _M0L6_2atmpS1407 = _M0L6_2atmpS1408 & 0xff;
        if (
          _M0L6_2atmpS1406 < 0
          || _M0L6_2atmpS1406 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 112 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1406] = _M0L6_2atmpS1407;
        _M0L6_2atmpS1415 = _M0L1iS131 * 4;
        _M0L6_2atmpS1411 = _M0L6_2atmpS1415 + 2;
        _M0L6_2atmpS1414 = _M0L1cS133 >> 16;
        _M0L6_2atmpS1413 = _M0L6_2atmpS1414 & 255;
        _M0L6_2atmpS1412 = _M0L6_2atmpS1413 & 0xff;
        if (
          _M0L6_2atmpS1411 < 0
          || _M0L6_2atmpS1411 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 113 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1411] = _M0L6_2atmpS1412;
        _M0L6_2atmpS1420 = _M0L1iS131 * 4;
        _M0L6_2atmpS1416 = _M0L6_2atmpS1420 + 3;
        _M0L6_2atmpS1419 = _M0L1cS133 >> 24;
        _M0L6_2atmpS1418 = _M0L6_2atmpS1419 & 255;
        _M0L6_2atmpS1417 = _M0L6_2atmpS1418 & 0xff;
        if (
          _M0L6_2atmpS1416 < 0
          || _M0L6_2atmpS1416 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 114 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1416] = _M0L6_2atmpS1417;
        _M0L6_2atmpS1421 = _M0L1iS131 + 1;
        _M0L6_2atmpS1422 = _M0L12utf16__indexS132 + 2;
        _M0L1iS131 = _M0L6_2atmpS1421;
        _M0L12utf16__indexS132 = _M0L6_2atmpS1422;
        continue;
      } else {
        int32_t _M0L6_2atmpS1423 = _M0L1iS131 * 4;
        int32_t _M0L6_2atmpS1425 = _M0L1cS133 & 255;
        int32_t _M0L6_2atmpS1424 = _M0L6_2atmpS1425 & 0xff;
        int32_t _M0L6_2atmpS1430;
        int32_t _M0L6_2atmpS1426;
        int32_t _M0L6_2atmpS1429;
        int32_t _M0L6_2atmpS1428;
        int32_t _M0L6_2atmpS1427;
        int32_t _M0L6_2atmpS1432;
        int32_t _M0L6_2atmpS1431;
        int32_t _M0L6_2atmpS1434;
        int32_t _M0L6_2atmpS1433;
        if (
          _M0L6_2atmpS1423 < 0
          || _M0L6_2atmpS1423 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 117 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1423] = _M0L6_2atmpS1424;
        _M0L6_2atmpS1430 = _M0L1iS131 * 4;
        _M0L6_2atmpS1426 = _M0L6_2atmpS1430 + 1;
        _M0L6_2atmpS1429 = _M0L1cS133 >> 8;
        _M0L6_2atmpS1428 = _M0L6_2atmpS1429 & 255;
        _M0L6_2atmpS1427 = _M0L6_2atmpS1428 & 0xff;
        if (
          _M0L6_2atmpS1426 < 0
          || _M0L6_2atmpS1426 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 118 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1426] = _M0L6_2atmpS1427;
        _M0L6_2atmpS1432 = _M0L1iS131 * 4;
        _M0L6_2atmpS1431 = _M0L6_2atmpS1432 + 2;
        if (
          _M0L6_2atmpS1431 < 0
          || _M0L6_2atmpS1431 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 119 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1431] = 0;
        _M0L6_2atmpS1434 = _M0L1iS131 * 4;
        _M0L6_2atmpS1433 = _M0L6_2atmpS1434 + 3;
        if (
          _M0L6_2atmpS1433 < 0
          || _M0L6_2atmpS1433 >= Moonbit_array_length(_M0L4dataS130)
        ) {
          #line 120 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
          moonbit_panic();
        }
        _M0L4dataS130[_M0L6_2atmpS1433] = 0;
      }
      _M0L6_2atmpS1436 = _M0L1iS131 + 1;
      _M0L6_2atmpS1437 = _M0L12utf16__indexS132 + 1;
      _M0L1iS131 = _M0L6_2atmpS1436;
      _M0L12utf16__indexS132 = _M0L6_2atmpS1437;
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
    int32_t _M0L6_2atmpS1402 = _M0L5indexS126 + 1;
    int32_t _M0L6_2atmpS3201 = _M0L4selfS125[_M0L6_2atmpS1402];
    int32_t _M0L2c2S127;
    int32_t _M0L6_2atmpS1400;
    int32_t _M0L6_2atmpS1401;
    moonbit_decref(_M0L4selfS125);
    _M0L2c2S127 = _M0L6_2atmpS3201;
    _M0L6_2atmpS1400 = (int32_t)_M0L2c1S124;
    _M0L6_2atmpS1401 = (int32_t)_M0L2c2S127;
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0FPB32code__point__of__surrogate__pair(_M0L6_2atmpS1400, _M0L6_2atmpS1401);
  } else {
    moonbit_decref(_M0L4selfS125);
    #line 97 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\deprecated.mbt"
    return _M0MPC16uint166UInt1616unsafe__to__char(_M0L2c1S124);
  }
}

int32_t _M0MPC16uint166UInt1616unsafe__to__char(int32_t _M0L4selfS123) {
  int32_t _M0L6_2atmpS1399;
  #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\uint16_char.mbt"
  _M0L6_2atmpS1399 = (int32_t)_M0L4selfS123;
  return _M0L6_2atmpS1399;
}

int32_t _M0FPB32code__point__of__surrogate__pair(
  int32_t _M0L7leadingS121,
  int32_t _M0L8trailingS122
) {
  int32_t _M0L6_2atmpS1398;
  int32_t _M0L6_2atmpS1397;
  int32_t _M0L6_2atmpS1396;
  int32_t _M0L6_2atmpS1395;
  int32_t _M0L6_2atmpS1394;
  #line 40 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  _M0L6_2atmpS1398 = _M0L7leadingS121 - 55296;
  _M0L6_2atmpS1397 = _M0L6_2atmpS1398 * 1024;
  _M0L6_2atmpS1396 = _M0L6_2atmpS1397 + _M0L8trailingS122;
  _M0L6_2atmpS1395 = _M0L6_2atmpS1396 - 56320;
  _M0L6_2atmpS1394 = _M0L6_2atmpS1395 + 65536;
  return _M0L6_2atmpS1394;
}

int32_t _M0MPC16string6String20char__length_2einner(
  moonbit_string_t _M0L4selfS114,
  int32_t _M0L13start__offsetS115,
  int64_t _M0L11end__offsetS112
) {
  int32_t _M0L11end__offsetS111;
  int32_t _if__result_3581;
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
  if (_M0L11end__offsetS112 == 4294967296ll) {
    _M0L11end__offsetS111 = Moonbit_array_length(_M0L4selfS114);
  } else {
    int64_t _M0L7_2aSomeS113 = _M0L11end__offsetS112;
    _M0L11end__offsetS111 = (int32_t)_M0L7_2aSomeS113;
  }
  if (_M0L13start__offsetS115 >= 0) {
    if (_M0L13start__offsetS115 <= _M0L11end__offsetS111) {
      int32_t _M0L6_2atmpS1387 = Moonbit_array_length(_M0L4selfS114);
      _if__result_3581 = _M0L11end__offsetS111 <= _M0L6_2atmpS1387;
    } else {
      _if__result_3581 = 0;
    }
  } else {
    _if__result_3581 = 0;
  }
  if (_if__result_3581) {
    int32_t _M0L12utf16__indexS116 = _M0L13start__offsetS115;
    int32_t _M0L11char__countS117 = 0;
    while (1) {
      if (_M0L12utf16__indexS116 < _M0L11end__offsetS111) {
        int32_t _M0L2c1S118 = _M0L4selfS114[_M0L12utf16__indexS116];
        int32_t _if__result_3583;
        int32_t _M0L6_2atmpS1392;
        int32_t _M0L6_2atmpS1393;
        #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
        if (_M0MPC16uint166UInt1622is__leading__surrogate(_M0L2c1S118)) {
          int32_t _M0L6_2atmpS1388 = _M0L12utf16__indexS116 + 1;
          _if__result_3583 = _M0L6_2atmpS1388 < _M0L11end__offsetS111;
        } else {
          _if__result_3583 = 0;
        }
        if (_if__result_3583) {
          int32_t _M0L6_2atmpS1391 = _M0L12utf16__indexS116 + 1;
          int32_t _M0L2c2S119 = _M0L4selfS114[_M0L6_2atmpS1391];
          #line 78 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
          if (_M0MPC16uint166UInt1623is__trailing__surrogate(_M0L2c2S119)) {
            int32_t _M0L6_2atmpS1389 = _M0L12utf16__indexS116 + 2;
            int32_t _M0L6_2atmpS1390 = _M0L11char__countS117 + 1;
            _M0L12utf16__indexS116 = _M0L6_2atmpS1389;
            _M0L11char__countS117 = _M0L6_2atmpS1390;
            continue;
          } else {
            #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\string.mbt"
            _M0FPB5abortGuE((moonbit_string_t)moonbit_string_literal_148.data, (moonbit_string_t)moonbit_string_literal_176.data);
          }
        }
        _M0L6_2atmpS1392 = _M0L12utf16__indexS116 + 1;
        _M0L6_2atmpS1393 = _M0L11char__countS117 + 1;
        _M0L12utf16__indexS116 = _M0L6_2atmpS1392;
        _M0L11char__countS117 = _M0L6_2atmpS1393;
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
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_177.data, (moonbit_string_t)moonbit_string_literal_178.data);
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
    int32_t _M0L6_2atmpS1339 = _M0L3lenS89 - _M0L3remS91;
    if (_M0L1iS92 < _M0L6_2atmpS1339) {
      int32_t _M0L6_2atmpS1361;
      int32_t _M0L2b0S93;
      int32_t _M0L6_2atmpS1360;
      int32_t _M0L6_2atmpS1359;
      int32_t _M0L2b1S94;
      int32_t _M0L6_2atmpS1358;
      int32_t _M0L6_2atmpS1357;
      int32_t _M0L2b2S95;
      int32_t _M0L6_2atmpS1356;
      int32_t _M0L6_2atmpS1355;
      int32_t _M0L2x0S96;
      int32_t _M0L6_2atmpS1354;
      int32_t _M0L6_2atmpS1351;
      int32_t _M0L6_2atmpS1353;
      int32_t _M0L6_2atmpS1352;
      int32_t _M0L6_2atmpS1350;
      int32_t _M0L2x1S97;
      int32_t _M0L6_2atmpS1349;
      int32_t _M0L6_2atmpS1346;
      int32_t _M0L6_2atmpS1348;
      int32_t _M0L6_2atmpS1347;
      int32_t _M0L6_2atmpS1345;
      int32_t _M0L2x2S98;
      int32_t _M0L6_2atmpS1344;
      int32_t _M0L2x3S99;
      int32_t _M0L6_2atmpS1340;
      int32_t _M0L6_2atmpS1341;
      int32_t _M0L6_2atmpS1342;
      int32_t _M0L6_2atmpS1343;
      int32_t _M0L6_2atmpS1362;
      if (_M0L1iS92 < 0 || _M0L1iS92 >= Moonbit_array_length(_M0L4dataS90)) {
        #line 67 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1361 = (int32_t)_M0L4dataS90[_M0L1iS92];
      _M0L2b0S93 = (int32_t)_M0L6_2atmpS1361;
      _M0L6_2atmpS1360 = _M0L1iS92 + 1;
      if (
        _M0L6_2atmpS1360 < 0
        || _M0L6_2atmpS1360 >= Moonbit_array_length(_M0L4dataS90)
      ) {
        #line 68 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1359 = (int32_t)_M0L4dataS90[_M0L6_2atmpS1360];
      _M0L2b1S94 = (int32_t)_M0L6_2atmpS1359;
      _M0L6_2atmpS1358 = _M0L1iS92 + 2;
      if (
        _M0L6_2atmpS1358 < 0
        || _M0L6_2atmpS1358 >= Moonbit_array_length(_M0L4dataS90)
      ) {
        #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L6_2atmpS1357 = (int32_t)_M0L4dataS90[_M0L6_2atmpS1358];
      _M0L2b2S95 = (int32_t)_M0L6_2atmpS1357;
      _M0L6_2atmpS1356 = _M0L2b0S93 & 252;
      _M0L6_2atmpS1355 = _M0L6_2atmpS1356 >> 2;
      if (
        _M0L6_2atmpS1355 < 0
        || _M0L6_2atmpS1355
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 70 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x0S96 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1355];
      _M0L6_2atmpS1354 = _M0L2b0S93 & 3;
      _M0L6_2atmpS1351 = _M0L6_2atmpS1354 << 4;
      _M0L6_2atmpS1353 = _M0L2b1S94 & 240;
      _M0L6_2atmpS1352 = _M0L6_2atmpS1353 >> 4;
      _M0L6_2atmpS1350 = _M0L6_2atmpS1351 | _M0L6_2atmpS1352;
      if (
        _M0L6_2atmpS1350 < 0
        || _M0L6_2atmpS1350
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x1S97 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1350];
      _M0L6_2atmpS1349 = _M0L2b1S94 & 15;
      _M0L6_2atmpS1346 = _M0L6_2atmpS1349 << 2;
      _M0L6_2atmpS1348 = _M0L2b2S95 & 192;
      _M0L6_2atmpS1347 = _M0L6_2atmpS1348 >> 6;
      _M0L6_2atmpS1345 = _M0L6_2atmpS1346 | _M0L6_2atmpS1347;
      if (
        _M0L6_2atmpS1345 < 0
        || _M0L6_2atmpS1345
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 72 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x2S98 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1345];
      _M0L6_2atmpS1344 = _M0L2b2S95 & 63;
      if (
        _M0L6_2atmpS1344 < 0
        || _M0L6_2atmpS1344
           >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
      ) {
        #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
        moonbit_panic();
      }
      _M0L2x3S99 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1344];
      #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1340 = _M0MPC14byte4Byte8to__char(_M0L2x0S96);
      moonbit_incref(_M0L3bufS88);
      #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1340);
      #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1341 = _M0MPC14byte4Byte8to__char(_M0L2x1S97);
      moonbit_incref(_M0L3bufS88);
      #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1341);
      #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1342 = _M0MPC14byte4Byte8to__char(_M0L2x2S98);
      moonbit_incref(_M0L3bufS88);
      #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1342);
      #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0L6_2atmpS1343 = _M0MPC14byte4Byte8to__char(_M0L2x3S99);
      moonbit_incref(_M0L3bufS88);
      #line 77 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1343);
      _M0L6_2atmpS1362 = _M0L1iS92 + 3;
      _M0L1iS92 = _M0L6_2atmpS1362;
      continue;
    }
    break;
  }
  if (_M0L3remS91 == 1) {
    int32_t _M0L6_2atmpS1370 = _M0L3lenS89 - 1;
    int32_t _M0L6_2atmpS3202;
    int32_t _M0L6_2atmpS1369;
    int32_t _M0L2b0S101;
    int32_t _M0L6_2atmpS1368;
    int32_t _M0L6_2atmpS1367;
    int32_t _M0L2x0S102;
    int32_t _M0L6_2atmpS1366;
    int32_t _M0L6_2atmpS1365;
    int32_t _M0L2x1S103;
    int32_t _M0L6_2atmpS1363;
    int32_t _M0L6_2atmpS1364;
    if (
      _M0L6_2atmpS1370 < 0
      || _M0L6_2atmpS1370 >= Moonbit_array_length(_M0L4dataS90)
    ) {
      #line 80 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3202 = (int32_t)_M0L4dataS90[_M0L6_2atmpS1370];
    moonbit_decref(_M0L4dataS90);
    _M0L6_2atmpS1369 = _M0L6_2atmpS3202;
    _M0L2b0S101 = (int32_t)_M0L6_2atmpS1369;
    _M0L6_2atmpS1368 = _M0L2b0S101 & 252;
    _M0L6_2atmpS1367 = _M0L6_2atmpS1368 >> 2;
    if (
      _M0L6_2atmpS1367 < 0
      || _M0L6_2atmpS1367
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 81 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x0S102 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1367];
    _M0L6_2atmpS1366 = _M0L2b0S101 & 3;
    _M0L6_2atmpS1365 = _M0L6_2atmpS1366 << 4;
    if (
      _M0L6_2atmpS1365 < 0
      || _M0L6_2atmpS1365
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 82 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x1S103 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1365];
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1363 = _M0MPC14byte4Byte8to__char(_M0L2x0S102);
    moonbit_incref(_M0L3bufS88);
    #line 83 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1363);
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1364 = _M0MPC14byte4Byte8to__char(_M0L2x1S103);
    moonbit_incref(_M0L3bufS88);
    #line 84 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1364);
    moonbit_incref(_M0L3bufS88);
    #line 85 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, 61);
    moonbit_incref(_M0L3bufS88);
    #line 86 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, 61);
  } else if (_M0L3remS91 == 2) {
    int32_t _M0L6_2atmpS1386 = _M0L3lenS89 - 2;
    int32_t _M0L6_2atmpS1385;
    int32_t _M0L2b0S104;
    int32_t _M0L6_2atmpS1384;
    int32_t _M0L6_2atmpS3203;
    int32_t _M0L6_2atmpS1383;
    int32_t _M0L2b1S105;
    int32_t _M0L6_2atmpS1382;
    int32_t _M0L6_2atmpS1381;
    int32_t _M0L2x0S106;
    int32_t _M0L6_2atmpS1380;
    int32_t _M0L6_2atmpS1377;
    int32_t _M0L6_2atmpS1379;
    int32_t _M0L6_2atmpS1378;
    int32_t _M0L6_2atmpS1376;
    int32_t _M0L2x1S107;
    int32_t _M0L6_2atmpS1375;
    int32_t _M0L6_2atmpS1374;
    int32_t _M0L2x2S108;
    int32_t _M0L6_2atmpS1371;
    int32_t _M0L6_2atmpS1372;
    int32_t _M0L6_2atmpS1373;
    if (
      _M0L6_2atmpS1386 < 0
      || _M0L6_2atmpS1386 >= Moonbit_array_length(_M0L4dataS90)
    ) {
      #line 88 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS1385 = (int32_t)_M0L4dataS90[_M0L6_2atmpS1386];
    _M0L2b0S104 = (int32_t)_M0L6_2atmpS1385;
    _M0L6_2atmpS1384 = _M0L3lenS89 - 1;
    if (
      _M0L6_2atmpS1384 < 0
      || _M0L6_2atmpS1384 >= Moonbit_array_length(_M0L4dataS90)
    ) {
      #line 89 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L6_2atmpS3203 = (int32_t)_M0L4dataS90[_M0L6_2atmpS1384];
    moonbit_decref(_M0L4dataS90);
    _M0L6_2atmpS1383 = _M0L6_2atmpS3203;
    _M0L2b1S105 = (int32_t)_M0L6_2atmpS1383;
    _M0L6_2atmpS1382 = _M0L2b0S104 & 252;
    _M0L6_2atmpS1381 = _M0L6_2atmpS1382 >> 2;
    if (
      _M0L6_2atmpS1381 < 0
      || _M0L6_2atmpS1381
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 90 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x0S106 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1381];
    _M0L6_2atmpS1380 = _M0L2b0S104 & 3;
    _M0L6_2atmpS1377 = _M0L6_2atmpS1380 << 4;
    _M0L6_2atmpS1379 = _M0L2b1S105 & 240;
    _M0L6_2atmpS1378 = _M0L6_2atmpS1379 >> 4;
    _M0L6_2atmpS1376 = _M0L6_2atmpS1377 | _M0L6_2atmpS1378;
    if (
      _M0L6_2atmpS1376 < 0
      || _M0L6_2atmpS1376
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 91 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x1S107 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1376];
    _M0L6_2atmpS1375 = _M0L2b1S105 & 15;
    _M0L6_2atmpS1374 = _M0L6_2atmpS1375 << 2;
    if (
      _M0L6_2atmpS1374 < 0
      || _M0L6_2atmpS1374
         >= Moonbit_array_length(_M0FPB14base64__encodeN6base64S1657)
    ) {
      #line 92 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
      moonbit_panic();
    }
    _M0L2x2S108 = _M0FPB14base64__encodeN6base64S1657[_M0L6_2atmpS1374];
    #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1371 = _M0MPC14byte4Byte8to__char(_M0L2x0S106);
    moonbit_incref(_M0L3bufS88);
    #line 93 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1371);
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1372 = _M0MPC14byte4Byte8to__char(_M0L2x1S107);
    moonbit_incref(_M0L3bufS88);
    #line 94 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1372);
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0L6_2atmpS1373 = _M0MPC14byte4Byte8to__char(_M0L2x2S108);
    moonbit_incref(_M0L3bufS88);
    #line 95 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\console.mbt"
    _M0IPB13StringBuilderPB6Logger11write__char(_M0L3bufS88, _M0L6_2atmpS1373);
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
  int32_t _M0L3lenS1334;
  int32_t _M0L6_2atmpS1333;
  moonbit_bytes_t _M0L8_2afieldS3204;
  moonbit_bytes_t _M0L4dataS1337;
  int32_t _M0L3lenS1338;
  int32_t _M0L3incS86;
  int32_t _M0L3lenS1336;
  int32_t _M0L6_2atmpS1335;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3lenS1334 = _M0L4selfS85->$1;
  _M0L6_2atmpS1333 = _M0L3lenS1334 + 4;
  moonbit_incref(_M0L4selfS85);
  #line 75 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPB13StringBuilder19grow__if__necessary(_M0L4selfS85, _M0L6_2atmpS1333);
  _M0L8_2afieldS3204 = _M0L4selfS85->$0;
  _M0L4dataS1337 = _M0L8_2afieldS3204;
  _M0L3lenS1338 = _M0L4selfS85->$1;
  moonbit_incref(_M0L4dataS1337);
  #line 76 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L3incS86
  = _M0MPC15array10FixedArray18set__utf16le__char(_M0L4dataS1337, _M0L3lenS1338, _M0L2chS87);
  _M0L3lenS1336 = _M0L4selfS85->$1;
  _M0L6_2atmpS1335 = _M0L3lenS1336 + _M0L3incS86;
  _M0L4selfS85->$1 = _M0L6_2atmpS1335;
  moonbit_decref(_M0L4selfS85);
  return 0;
}

int32_t _M0MPB13StringBuilder19grow__if__necessary(
  struct _M0TPB13StringBuilder* _M0L4selfS80,
  int32_t _M0L8requiredS81
) {
  moonbit_bytes_t _M0L8_2afieldS3208;
  moonbit_bytes_t _M0L4dataS1332;
  int32_t _M0L6_2atmpS3207;
  int32_t _M0L12current__lenS79;
  int32_t _M0Lm13enough__spaceS82;
  int32_t _M0L6_2atmpS1330;
  int32_t _M0L6_2atmpS1331;
  moonbit_bytes_t _M0L9new__dataS84;
  moonbit_bytes_t _M0L8_2afieldS3206;
  moonbit_bytes_t _M0L4dataS1328;
  int32_t _M0L3lenS1329;
  moonbit_bytes_t _M0L6_2aoldS3205;
  #line 45 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3208 = _M0L4selfS80->$0;
  _M0L4dataS1332 = _M0L8_2afieldS3208;
  _M0L6_2atmpS3207 = Moonbit_array_length(_M0L4dataS1332);
  _M0L12current__lenS79 = _M0L6_2atmpS3207;
  if (_M0L8requiredS81 <= _M0L12current__lenS79) {
    moonbit_decref(_M0L4selfS80);
    return 0;
  }
  _M0Lm13enough__spaceS82 = _M0L12current__lenS79;
  while (1) {
    int32_t _M0L6_2atmpS1326 = _M0Lm13enough__spaceS82;
    if (_M0L6_2atmpS1326 < _M0L8requiredS81) {
      int32_t _M0L6_2atmpS1327 = _M0Lm13enough__spaceS82;
      _M0Lm13enough__spaceS82 = _M0L6_2atmpS1327 * 2;
      continue;
    }
    break;
  }
  _M0L6_2atmpS1330 = _M0Lm13enough__spaceS82;
  #line 59 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L6_2atmpS1331 = _M0IPC14byte4BytePB7Default7default();
  _M0L9new__dataS84
  = (moonbit_bytes_t)moonbit_make_bytes(_M0L6_2atmpS1330, _M0L6_2atmpS1331);
  _M0L8_2afieldS3206 = _M0L4selfS80->$0;
  _M0L4dataS1328 = _M0L8_2afieldS3206;
  _M0L3lenS1329 = _M0L4selfS80->$1;
  moonbit_incref(_M0L4dataS1328);
  moonbit_incref(_M0L9new__dataS84);
  #line 60 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0MPC15array10FixedArray12unsafe__blitGyE(_M0L9new__dataS84, 0, _M0L4dataS1328, 0, _M0L3lenS1329);
  _M0L6_2aoldS3205 = _M0L4selfS80->$0;
  moonbit_decref(_M0L6_2aoldS3205);
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
    uint32_t _M0L6_2atmpS1309 = _M0L4codeS72 & 255u;
    int32_t _M0L6_2atmpS1308;
    int32_t _M0L6_2atmpS1310;
    uint32_t _M0L6_2atmpS1312;
    int32_t _M0L6_2atmpS1311;
    #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1308 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1309);
    if (
      _M0L6offsetS75 < 0
      || _M0L6offsetS75 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 285 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6offsetS75] = _M0L6_2atmpS1308;
    _M0L6_2atmpS1310 = _M0L6offsetS75 + 1;
    _M0L6_2atmpS1312 = _M0L4codeS72 >> 8;
    #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1311 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1312);
    if (
      _M0L6_2atmpS1310 < 0
      || _M0L6_2atmpS1310 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 286 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS1310] = _M0L6_2atmpS1311;
    moonbit_decref(_M0L4selfS74);
    return 2;
  } else if (_M0L4codeS72 < 1114112u) {
    uint32_t _M0L2hiS76 = _M0L4codeS72 - 65536u;
    uint32_t _M0L6_2atmpS1325 = _M0L2hiS76 >> 10;
    uint32_t _M0L2loS77 = _M0L6_2atmpS1325 | 55296u;
    uint32_t _M0L6_2atmpS1324 = _M0L2hiS76 & 1023u;
    uint32_t _M0L2hiS78 = _M0L6_2atmpS1324 | 56320u;
    uint32_t _M0L6_2atmpS1314 = _M0L2loS77 & 255u;
    int32_t _M0L6_2atmpS1313;
    int32_t _M0L6_2atmpS1315;
    uint32_t _M0L6_2atmpS1317;
    int32_t _M0L6_2atmpS1316;
    int32_t _M0L6_2atmpS1318;
    uint32_t _M0L6_2atmpS1320;
    int32_t _M0L6_2atmpS1319;
    int32_t _M0L6_2atmpS1321;
    uint32_t _M0L6_2atmpS1323;
    int32_t _M0L6_2atmpS1322;
    #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1313 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1314);
    if (
      _M0L6offsetS75 < 0
      || _M0L6offsetS75 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 292 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6offsetS75] = _M0L6_2atmpS1313;
    _M0L6_2atmpS1315 = _M0L6offsetS75 + 1;
    _M0L6_2atmpS1317 = _M0L2loS77 >> 8;
    #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1316 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1317);
    if (
      _M0L6_2atmpS1315 < 0
      || _M0L6_2atmpS1315 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 293 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS1315] = _M0L6_2atmpS1316;
    _M0L6_2atmpS1318 = _M0L6offsetS75 + 2;
    _M0L6_2atmpS1320 = _M0L2hiS78 & 255u;
    #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1319 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1320);
    if (
      _M0L6_2atmpS1318 < 0
      || _M0L6_2atmpS1318 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 294 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS1318] = _M0L6_2atmpS1319;
    _M0L6_2atmpS1321 = _M0L6offsetS75 + 3;
    _M0L6_2atmpS1323 = _M0L2hiS78 >> 8;
    #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    _M0L6_2atmpS1322 = _M0MPC14uint4UInt8to__byte(_M0L6_2atmpS1323);
    if (
      _M0L6_2atmpS1321 < 0
      || _M0L6_2atmpS1321 >= Moonbit_array_length(_M0L4selfS74)
    ) {
      #line 295 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
      moonbit_panic();
    }
    _M0L4selfS74[_M0L6_2atmpS1321] = _M0L6_2atmpS1322;
    moonbit_decref(_M0L4selfS74);
    return 4;
  } else {
    moonbit_decref(_M0L4selfS74);
    #line 298 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\bytes.mbt"
    return _M0FPB5abortGiE((moonbit_string_t)moonbit_string_literal_179.data, (moonbit_string_t)moonbit_string_literal_180.data);
  }
}

int32_t _M0MPC14uint4UInt8to__byte(uint32_t _M0L4selfS71) {
  int32_t _M0L6_2atmpS1307;
  #line 2554 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1307 = *(int32_t*)&_M0L4selfS71;
  return _M0L6_2atmpS1307 & 0xff;
}

uint32_t _M0MPC14char4Char8to__uint(int32_t _M0L4selfS70) {
  int32_t _M0L6_2atmpS1306;
  #line 1270 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1306 = _M0L4selfS70;
  return *(uint32_t*)&_M0L6_2atmpS1306;
}

moonbit_string_t _M0MPB13StringBuilder10to__string(
  struct _M0TPB13StringBuilder* _M0L4selfS69
) {
  moonbit_bytes_t _M0L8_2afieldS3210;
  moonbit_bytes_t _M0L4dataS1305;
  moonbit_bytes_t _M0L6_2atmpS1302;
  int32_t _M0L8_2afieldS3209;
  int32_t _M0L3lenS1304;
  int64_t _M0L6_2atmpS1303;
  #line 115 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  _M0L8_2afieldS3210 = _M0L4selfS69->$0;
  _M0L4dataS1305 = _M0L8_2afieldS3210;
  moonbit_incref(_M0L4dataS1305);
  _M0L6_2atmpS1302 = _M0L4dataS1305;
  _M0L8_2afieldS3209 = _M0L4selfS69->$1;
  moonbit_decref(_M0L4selfS69);
  _M0L3lenS1304 = _M0L8_2afieldS3209;
  _M0L6_2atmpS1303 = (int64_t)_M0L3lenS1304;
  #line 116 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  return _M0MPC15bytes5Bytes29to__unchecked__string_2einner(_M0L6_2atmpS1302, 0, _M0L6_2atmpS1303);
}

moonbit_string_t _M0MPC15bytes5Bytes29to__unchecked__string_2einner(
  moonbit_bytes_t _M0L4selfS64,
  int32_t _M0L6offsetS68,
  int64_t _M0L6lengthS66
) {
  int32_t _M0L3lenS63;
  int32_t _M0L6lengthS65;
  int32_t _if__result_3586;
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
      int32_t _M0L6_2atmpS1301 = _M0L6offsetS68 + _M0L6lengthS65;
      _if__result_3586 = _M0L6_2atmpS1301 <= _M0L3lenS63;
    } else {
      _if__result_3586 = 0;
    }
  } else {
    _if__result_3586 = 0;
  }
  if (_if__result_3586) {
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
  struct _M0TPB13StringBuilder* _block_3587;
  #line 32 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\stringbuilder_buffer.mbt"
  if (_M0L10size__hintS61 < 1) {
    _M0L7initialS60 = 1;
  } else {
    _M0L7initialS60 = _M0L10size__hintS61;
  }
  _M0L4dataS62 = (moonbit_bytes_t)moonbit_make_bytes(_M0L7initialS60, 0);
  _block_3587
  = (struct _M0TPB13StringBuilder*)moonbit_malloc(sizeof(struct _M0TPB13StringBuilder));
  Moonbit_object_header(_block_3587)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB13StringBuilder, $0) >> 2, 1, 0);
  _block_3587->$0 = _M0L4dataS62;
  _block_3587->$1 = 0;
  return _block_3587;
}

int32_t _M0MPC14byte4Byte8to__char(int32_t _M0L4selfS59) {
  int32_t _M0L6_2atmpS1300;
  #line 1903 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1300 = (int32_t)_M0L4selfS59;
  return _M0L6_2atmpS1300;
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
  int32_t _if__result_3588;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS22 == _M0L3srcS23) {
    _if__result_3588 = _M0L11dst__offsetS24 < _M0L11src__offsetS25;
  } else {
    _if__result_3588 = 0;
  }
  if (_if__result_3588) {
    int32_t _M0L1iS26 = 0;
    while (1) {
      if (_M0L1iS26 < _M0L3lenS27) {
        int32_t _M0L6_2atmpS1273 = _M0L11dst__offsetS24 + _M0L1iS26;
        int32_t _M0L6_2atmpS1275 = _M0L11src__offsetS25 + _M0L1iS26;
        int32_t _M0L6_2atmpS1274;
        int32_t _M0L6_2atmpS1276;
        if (
          _M0L6_2atmpS1275 < 0
          || _M0L6_2atmpS1275 >= Moonbit_array_length(_M0L3srcS23)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1274 = (int32_t)_M0L3srcS23[_M0L6_2atmpS1275];
        if (
          _M0L6_2atmpS1273 < 0
          || _M0L6_2atmpS1273 >= Moonbit_array_length(_M0L3dstS22)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS22[_M0L6_2atmpS1273] = _M0L6_2atmpS1274;
        _M0L6_2atmpS1276 = _M0L1iS26 + 1;
        _M0L1iS26 = _M0L6_2atmpS1276;
        continue;
      } else {
        moonbit_decref(_M0L3srcS23);
        moonbit_decref(_M0L3dstS22);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1281 = _M0L3lenS27 - 1;
    int32_t _M0L1iS29 = _M0L6_2atmpS1281;
    while (1) {
      if (_M0L1iS29 >= 0) {
        int32_t _M0L6_2atmpS1277 = _M0L11dst__offsetS24 + _M0L1iS29;
        int32_t _M0L6_2atmpS1279 = _M0L11src__offsetS25 + _M0L1iS29;
        int32_t _M0L6_2atmpS1278;
        int32_t _M0L6_2atmpS1280;
        if (
          _M0L6_2atmpS1279 < 0
          || _M0L6_2atmpS1279 >= Moonbit_array_length(_M0L3srcS23)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS1278 = (int32_t)_M0L3srcS23[_M0L6_2atmpS1279];
        if (
          _M0L6_2atmpS1277 < 0
          || _M0L6_2atmpS1277 >= Moonbit_array_length(_M0L3dstS22)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L3dstS22[_M0L6_2atmpS1277] = _M0L6_2atmpS1278;
        _M0L6_2atmpS1280 = _M0L1iS29 - 1;
        _M0L1iS29 = _M0L6_2atmpS1280;
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
  int32_t _if__result_3591;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS31 == _M0L3srcS32) {
    _if__result_3591 = _M0L11dst__offsetS33 < _M0L11src__offsetS34;
  } else {
    _if__result_3591 = 0;
  }
  if (_if__result_3591) {
    int32_t _M0L1iS35 = 0;
    while (1) {
      if (_M0L1iS35 < _M0L3lenS36) {
        int32_t _M0L6_2atmpS1282 = _M0L11dst__offsetS33 + _M0L1iS35;
        int32_t _M0L6_2atmpS1284 = _M0L11src__offsetS34 + _M0L1iS35;
        moonbit_string_t _M0L6_2atmpS3212;
        moonbit_string_t _M0L6_2atmpS1283;
        moonbit_string_t _M0L6_2aoldS3211;
        int32_t _M0L6_2atmpS1285;
        if (
          _M0L6_2atmpS1284 < 0
          || _M0L6_2atmpS1284 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3212 = (moonbit_string_t)_M0L3srcS32[_M0L6_2atmpS1284];
        _M0L6_2atmpS1283 = _M0L6_2atmpS3212;
        if (
          _M0L6_2atmpS1282 < 0
          || _M0L6_2atmpS1282 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3211 = (moonbit_string_t)_M0L3dstS31[_M0L6_2atmpS1282];
        moonbit_incref(_M0L6_2atmpS1283);
        moonbit_decref(_M0L6_2aoldS3211);
        _M0L3dstS31[_M0L6_2atmpS1282] = _M0L6_2atmpS1283;
        _M0L6_2atmpS1285 = _M0L1iS35 + 1;
        _M0L1iS35 = _M0L6_2atmpS1285;
        continue;
      } else {
        moonbit_decref(_M0L3srcS32);
        moonbit_decref(_M0L3dstS31);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1290 = _M0L3lenS36 - 1;
    int32_t _M0L1iS38 = _M0L6_2atmpS1290;
    while (1) {
      if (_M0L1iS38 >= 0) {
        int32_t _M0L6_2atmpS1286 = _M0L11dst__offsetS33 + _M0L1iS38;
        int32_t _M0L6_2atmpS1288 = _M0L11src__offsetS34 + _M0L1iS38;
        moonbit_string_t _M0L6_2atmpS3214;
        moonbit_string_t _M0L6_2atmpS1287;
        moonbit_string_t _M0L6_2aoldS3213;
        int32_t _M0L6_2atmpS1289;
        if (
          _M0L6_2atmpS1288 < 0
          || _M0L6_2atmpS1288 >= Moonbit_array_length(_M0L3srcS32)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3214 = (moonbit_string_t)_M0L3srcS32[_M0L6_2atmpS1288];
        _M0L6_2atmpS1287 = _M0L6_2atmpS3214;
        if (
          _M0L6_2atmpS1286 < 0
          || _M0L6_2atmpS1286 >= Moonbit_array_length(_M0L3dstS31)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3213 = (moonbit_string_t)_M0L3dstS31[_M0L6_2atmpS1286];
        moonbit_incref(_M0L6_2atmpS1287);
        moonbit_decref(_M0L6_2aoldS3213);
        _M0L3dstS31[_M0L6_2atmpS1286] = _M0L6_2atmpS1287;
        _M0L6_2atmpS1289 = _M0L1iS38 - 1;
        _M0L1iS38 = _M0L6_2atmpS1289;
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
  int32_t _if__result_3594;
  #line 38 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
  if (_M0L3dstS40 == _M0L3srcS41) {
    _if__result_3594 = _M0L11dst__offsetS42 < _M0L11src__offsetS43;
  } else {
    _if__result_3594 = 0;
  }
  if (_if__result_3594) {
    int32_t _M0L1iS44 = 0;
    while (1) {
      if (_M0L1iS44 < _M0L3lenS45) {
        int32_t _M0L6_2atmpS1291 = _M0L11dst__offsetS42 + _M0L1iS44;
        int32_t _M0L6_2atmpS1293 = _M0L11src__offsetS43 + _M0L1iS44;
        struct _M0TUsiE* _M0L6_2atmpS3216;
        struct _M0TUsiE* _M0L6_2atmpS1292;
        struct _M0TUsiE* _M0L6_2aoldS3215;
        int32_t _M0L6_2atmpS1294;
        if (
          _M0L6_2atmpS1293 < 0
          || _M0L6_2atmpS1293 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3216 = (struct _M0TUsiE*)_M0L3srcS41[_M0L6_2atmpS1293];
        _M0L6_2atmpS1292 = _M0L6_2atmpS3216;
        if (
          _M0L6_2atmpS1291 < 0
          || _M0L6_2atmpS1291 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 49 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3215 = (struct _M0TUsiE*)_M0L3dstS40[_M0L6_2atmpS1291];
        if (_M0L6_2atmpS1292) {
          moonbit_incref(_M0L6_2atmpS1292);
        }
        if (_M0L6_2aoldS3215) {
          moonbit_decref(_M0L6_2aoldS3215);
        }
        _M0L3dstS40[_M0L6_2atmpS1291] = _M0L6_2atmpS1292;
        _M0L6_2atmpS1294 = _M0L1iS44 + 1;
        _M0L1iS44 = _M0L6_2atmpS1294;
        continue;
      } else {
        moonbit_decref(_M0L3srcS41);
        moonbit_decref(_M0L3dstS40);
      }
      break;
    }
  } else {
    int32_t _M0L6_2atmpS1299 = _M0L3lenS45 - 1;
    int32_t _M0L1iS47 = _M0L6_2atmpS1299;
    while (1) {
      if (_M0L1iS47 >= 0) {
        int32_t _M0L6_2atmpS1295 = _M0L11dst__offsetS42 + _M0L1iS47;
        int32_t _M0L6_2atmpS1297 = _M0L11src__offsetS43 + _M0L1iS47;
        struct _M0TUsiE* _M0L6_2atmpS3218;
        struct _M0TUsiE* _M0L6_2atmpS1296;
        struct _M0TUsiE* _M0L6_2aoldS3217;
        int32_t _M0L6_2atmpS1298;
        if (
          _M0L6_2atmpS1297 < 0
          || _M0L6_2atmpS1297 >= Moonbit_array_length(_M0L3srcS41)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2atmpS3218 = (struct _M0TUsiE*)_M0L3srcS41[_M0L6_2atmpS1297];
        _M0L6_2atmpS1296 = _M0L6_2atmpS3218;
        if (
          _M0L6_2atmpS1295 < 0
          || _M0L6_2atmpS1295 >= Moonbit_array_length(_M0L3dstS40)
        ) {
          #line 53 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\fixedarray_block.mbt"
          moonbit_panic();
        }
        _M0L6_2aoldS3217 = (struct _M0TUsiE*)_M0L3dstS40[_M0L6_2atmpS1295];
        if (_M0L6_2atmpS1296) {
          moonbit_incref(_M0L6_2atmpS1296);
        }
        if (_M0L6_2aoldS3217) {
          moonbit_decref(_M0L6_2aoldS3217);
        }
        _M0L3dstS40[_M0L6_2atmpS1295] = _M0L6_2atmpS1296;
        _M0L6_2atmpS1298 = _M0L1iS47 - 1;
        _M0L1iS47 = _M0L6_2atmpS1298;
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
  moonbit_string_t _M0L6_2atmpS1262;
  moonbit_string_t _M0L6_2atmpS3221;
  moonbit_string_t _M0L6_2atmpS1260;
  moonbit_string_t _M0L6_2atmpS1261;
  moonbit_string_t _M0L6_2atmpS3220;
  moonbit_string_t _M0L6_2atmpS1259;
  moonbit_string_t _M0L6_2atmpS3219;
  moonbit_string_t _M0L6_2atmpS1258;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1262 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS16);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3221
  = moonbit_add_string(_M0L6_2atmpS1262, (moonbit_string_t)moonbit_string_literal_181.data);
  moonbit_decref(_M0L6_2atmpS1262);
  _M0L6_2atmpS1260 = _M0L6_2atmpS3221;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1261
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS17);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3220 = moonbit_add_string(_M0L6_2atmpS1260, _M0L6_2atmpS1261);
  moonbit_decref(_M0L6_2atmpS1260);
  moonbit_decref(_M0L6_2atmpS1261);
  _M0L6_2atmpS1259 = _M0L6_2atmpS3220;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3219
  = moonbit_add_string(_M0L6_2atmpS1259, (moonbit_string_t)moonbit_string_literal_182.data);
  moonbit_decref(_M0L6_2atmpS1259);
  _M0L6_2atmpS1258 = _M0L6_2atmpS3219;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGiE(_M0L6_2atmpS1258);
}

int32_t _M0FPB5abortGuE(
  moonbit_string_t _M0L6stringS18,
  moonbit_string_t _M0L3locS19
) {
  moonbit_string_t _M0L6_2atmpS1267;
  moonbit_string_t _M0L6_2atmpS3224;
  moonbit_string_t _M0L6_2atmpS1265;
  moonbit_string_t _M0L6_2atmpS1266;
  moonbit_string_t _M0L6_2atmpS3223;
  moonbit_string_t _M0L6_2atmpS1264;
  moonbit_string_t _M0L6_2atmpS3222;
  moonbit_string_t _M0L6_2atmpS1263;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1267 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS18);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3224
  = moonbit_add_string(_M0L6_2atmpS1267, (moonbit_string_t)moonbit_string_literal_181.data);
  moonbit_decref(_M0L6_2atmpS1267);
  _M0L6_2atmpS1265 = _M0L6_2atmpS3224;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1266
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS19);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3223 = moonbit_add_string(_M0L6_2atmpS1265, _M0L6_2atmpS1266);
  moonbit_decref(_M0L6_2atmpS1265);
  moonbit_decref(_M0L6_2atmpS1266);
  _M0L6_2atmpS1264 = _M0L6_2atmpS3223;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3222
  = moonbit_add_string(_M0L6_2atmpS1264, (moonbit_string_t)moonbit_string_literal_182.data);
  moonbit_decref(_M0L6_2atmpS1264);
  _M0L6_2atmpS1263 = _M0L6_2atmpS3222;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0FPC15abort5abortGuE(_M0L6_2atmpS1263);
  return 0;
}

struct _M0TPC16string10StringView _M0FPB5abortGRPC16string10StringViewE(
  moonbit_string_t _M0L6stringS20,
  moonbit_string_t _M0L3locS21
) {
  moonbit_string_t _M0L6_2atmpS1272;
  moonbit_string_t _M0L6_2atmpS3227;
  moonbit_string_t _M0L6_2atmpS1270;
  moonbit_string_t _M0L6_2atmpS1271;
  moonbit_string_t _M0L6_2atmpS3226;
  moonbit_string_t _M0L6_2atmpS1269;
  moonbit_string_t _M0L6_2atmpS3225;
  moonbit_string_t _M0L6_2atmpS1268;
  #line 69 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  #line 73 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1272 = _M0IPC16string6StringPB4Show10to__string(_M0L6stringS20);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3227
  = moonbit_add_string(_M0L6_2atmpS1272, (moonbit_string_t)moonbit_string_literal_181.data);
  moonbit_decref(_M0L6_2atmpS1272);
  _M0L6_2atmpS1270 = _M0L6_2atmpS3227;
  #line 74 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS1271
  = _M0IP016_24default__implPB4Show10to__stringGRPB9SourceLocE(_M0L3locS21);
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3226 = moonbit_add_string(_M0L6_2atmpS1270, _M0L6_2atmpS1271);
  moonbit_decref(_M0L6_2atmpS1270);
  moonbit_decref(_M0L6_2atmpS1271);
  _M0L6_2atmpS1269 = _M0L6_2atmpS3226;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  _M0L6_2atmpS3225
  = moonbit_add_string(_M0L6_2atmpS1269, (moonbit_string_t)moonbit_string_literal_182.data);
  moonbit_decref(_M0L6_2atmpS1269);
  _M0L6_2atmpS1268 = _M0L6_2atmpS3225;
  #line 71 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\intrinsics.mbt"
  return _M0FPC15abort5abortGRPC16string10StringViewE(_M0L6_2atmpS1268);
}

int32_t _M0MPB6Hasher13combine__uint(
  struct _M0TPB6Hasher* _M0L4selfS14,
  uint32_t _M0L5valueS15
) {
  uint32_t _M0L3accS1257;
  uint32_t _M0L6_2atmpS1256;
  #line 236 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1257 = _M0L4selfS14->$0;
  _M0L6_2atmpS1256 = _M0L3accS1257 + 4u;
  _M0L4selfS14->$0 = _M0L6_2atmpS1256;
  #line 238 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0MPB6Hasher8consume4(_M0L4selfS14, _M0L5valueS15);
  return 0;
}

int32_t _M0MPB6Hasher8consume4(
  struct _M0TPB6Hasher* _M0L4selfS12,
  uint32_t _M0L5inputS13
) {
  uint32_t _M0L3accS1254;
  uint32_t _M0L6_2atmpS1255;
  uint32_t _M0L6_2atmpS1253;
  uint32_t _M0L6_2atmpS1252;
  uint32_t _M0L6_2atmpS1251;
  #line 453 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L3accS1254 = _M0L4selfS12->$0;
  _M0L6_2atmpS1255 = _M0L5inputS13 * 3266489917u;
  _M0L6_2atmpS1253 = _M0L3accS1254 + _M0L6_2atmpS1255;
  #line 454 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1252 = _M0FPB4rotl(_M0L6_2atmpS1253, 17);
  _M0L6_2atmpS1251 = _M0L6_2atmpS1252 * 668265263u;
  _M0L4selfS12->$0 = _M0L6_2atmpS1251;
  moonbit_decref(_M0L4selfS12);
  return 0;
}

uint32_t _M0FPB4rotl(uint32_t _M0L1xS10, int32_t _M0L1rS11) {
  uint32_t _M0L6_2atmpS1248;
  int32_t _M0L6_2atmpS1250;
  uint32_t _M0L6_2atmpS1249;
  #line 463 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\hasher.mbt"
  _M0L6_2atmpS1248 = _M0L1xS10 << (_M0L1rS11 & 31);
  _M0L6_2atmpS1250 = 32 - _M0L1rS11;
  _M0L6_2atmpS1249 = _M0L1xS10 >> (_M0L6_2atmpS1250 & 31);
  return _M0L6_2atmpS1248 | _M0L6_2atmpS1249;
}

int32_t _M0IPB7FailurePB4Show6output(
  void* _M0L10_2ax__4933S6,
  struct _M0TPB6Logger _M0L10_2ax__4934S9
) {
  struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure* _M0L10_2aFailureS7;
  moonbit_string_t _M0L8_2afieldS3228;
  int32_t _M0L6_2acntS3381;
  moonbit_string_t _M0L15_2a_2aarg__4935S8;
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2aFailureS7
  = (struct _M0DTPC15error5Error48moonbitlang_2fcore_2fbuiltin_2eFailure_2eFailure*)_M0L10_2ax__4933S6;
  _M0L8_2afieldS3228 = _M0L10_2aFailureS7->$0;
  _M0L6_2acntS3381 = Moonbit_object_header(_M0L10_2aFailureS7)->rc;
  if (_M0L6_2acntS3381 > 1) {
    int32_t _M0L11_2anew__cntS3382 = _M0L6_2acntS3381 - 1;
    Moonbit_object_header(_M0L10_2aFailureS7)->rc = _M0L11_2anew__cntS3382;
    moonbit_incref(_M0L8_2afieldS3228);
  } else if (_M0L6_2acntS3381 == 1) {
    #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
    moonbit_free(_M0L10_2aFailureS7);
  }
  _M0L15_2a_2aarg__4935S8 = _M0L8_2afieldS3228;
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_183.data);
  if (_M0L10_2ax__4934S9.$1) {
    moonbit_incref(_M0L10_2ax__4934S9.$1);
  }
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0MPB6Logger13write__objectGsE(_M0L10_2ax__4934S9, _M0L15_2a_2aarg__4935S8);
  #line 37 "C:\\Users\\Administrator\\.moon\\lib\\core\\builtin\\failure.mbt"
  _M0L10_2ax__4934S9.$0->$method_0(_M0L10_2ax__4934S9.$1, (moonbit_string_t)moonbit_string_literal_184.data);
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

moonbit_string_t _M0FP15Error10to__string(void* _M0L4_2aeS1156) {
  switch (Moonbit_object_tag(_M0L4_2aeS1156)) {
    case 3: {
      moonbit_decref(_M0L4_2aeS1156);
      return (moonbit_string_t)moonbit_string_literal_185.data;
      break;
    }
    
    case 4: {
      moonbit_decref(_M0L4_2aeS1156);
      return (moonbit_string_t)moonbit_string_literal_186.data;
      break;
    }
    
    case 0: {
      return _M0IP016_24default__implPB4Show10to__stringGRPB7FailureE(_M0L4_2aeS1156);
      break;
    }
    
    case 2: {
      moonbit_decref(_M0L4_2aeS1156);
      return (moonbit_string_t)moonbit_string_literal_187.data;
      break;
    }
    default: {
      moonbit_decref(_M0L4_2aeS1156);
      return (moonbit_string_t)moonbit_string_literal_188.data;
      break;
    }
  }
}

moonbit_string_t _M0IPC16string6StringPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1192
) {
  moonbit_string_t _M0L7_2aselfS1191 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1192;
  return _M0IPC16string6StringPB4Show10to__string(_M0L7_2aselfS1191);
}

int32_t _M0IPC16string6StringPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1190,
  struct _M0TPB6Logger _M0L8_2aparamS1189
) {
  moonbit_string_t _M0L7_2aselfS1188 =
    (moonbit_string_t)_M0L11_2aobj__ptrS1190;
  _M0IPC16string6StringPB4Show6output(_M0L7_2aselfS1188, _M0L8_2aparamS1189);
  return 0;
}

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGbE(
  void* _M0L11_2aobj__ptrS1186
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1187 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS1186;
  int32_t _M0L8_2afieldS3229 = _M0L14_2aboxed__selfS1187->$0;
  int32_t _M0L7_2aselfS1185;
  moonbit_decref(_M0L14_2aboxed__selfS1187);
  _M0L7_2aselfS1185 = _M0L8_2afieldS3229;
  return _M0IP016_24default__implPB4Show10to__stringGbE(_M0L7_2aselfS1185);
}

int32_t _M0IPC14bool4BoolPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1183,
  struct _M0TPB6Logger _M0L8_2aparamS1182
) {
  struct _M0Y4Bool* _M0L14_2aboxed__selfS1184 =
    (struct _M0Y4Bool*)_M0L11_2aobj__ptrS1183;
  int32_t _M0L8_2afieldS3230 = _M0L14_2aboxed__selfS1184->$0;
  int32_t _M0L7_2aselfS1181;
  moonbit_decref(_M0L14_2aboxed__selfS1184);
  _M0L7_2aselfS1181 = _M0L8_2afieldS3230;
  _M0IPC14bool4BoolPB4Show6output(_M0L7_2aselfS1181, _M0L8_2aparamS1182);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__char_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1180,
  int32_t _M0L8_2aparamS1179
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1178 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1180;
  _M0IPB13StringBuilderPB6Logger11write__char(_M0L7_2aselfS1178, _M0L8_2aparamS1179);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger67write__view_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1177,
  struct _M0TPC16string10StringView _M0L8_2aparamS1176
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1175 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1177;
  _M0IPB13StringBuilderPB6Logger11write__view(_M0L7_2aselfS1175, _M0L8_2aparamS1176);
  return 0;
}

int32_t _M0IP016_24default__implPB6Logger72write__substring_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLoggerGRPB13StringBuilderE(
  void* _M0L11_2aobj__ptrS1174,
  moonbit_string_t _M0L8_2aparamS1171,
  int32_t _M0L8_2aparamS1172,
  int32_t _M0L8_2aparamS1173
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1170 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1174;
  _M0IP016_24default__implPB6Logger16write__substringGRPB13StringBuilderE(_M0L7_2aselfS1170, _M0L8_2aparamS1171, _M0L8_2aparamS1172, _M0L8_2aparamS1173);
  return 0;
}

int32_t _M0IPB13StringBuilderPB6Logger69write__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eLogger(
  void* _M0L11_2aobj__ptrS1169,
  moonbit_string_t _M0L8_2aparamS1168
) {
  struct _M0TPB13StringBuilder* _M0L7_2aselfS1167 =
    (struct _M0TPB13StringBuilder*)_M0L11_2aobj__ptrS1169;
  _M0IPB13StringBuilderPB6Logger13write__string(_M0L7_2aselfS1167, _M0L8_2aparamS1168);
  return 0;
}

moonbit_string_t _M0IP016_24default__implPB4Show64to__string_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShowGiE(
  void* _M0L11_2aobj__ptrS1165
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS1166 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS1165;
  int32_t _M0L8_2afieldS3231 = _M0L14_2aboxed__selfS1166->$0;
  int32_t _M0L7_2aselfS1164;
  moonbit_decref(_M0L14_2aboxed__selfS1166);
  _M0L7_2aselfS1164 = _M0L8_2afieldS3231;
  return _M0IP016_24default__implPB4Show10to__stringGiE(_M0L7_2aselfS1164);
}

int32_t _M0IPC13int3IntPB4Show60output_2edyncall__as___40moonbitlang_2fcore_2fbuiltin_2eShow(
  void* _M0L11_2aobj__ptrS1162,
  struct _M0TPB6Logger _M0L8_2aparamS1161
) {
  struct _M0Y3Int* _M0L14_2aboxed__selfS1163 =
    (struct _M0Y3Int*)_M0L11_2aobj__ptrS1162;
  int32_t _M0L8_2afieldS3232 = _M0L14_2aboxed__selfS1163->$0;
  int32_t _M0L7_2aselfS1160;
  moonbit_decref(_M0L14_2aboxed__selfS1163);
  _M0L7_2aselfS1160 = _M0L8_2afieldS3232;
  _M0IPC13int3IntPB4Show6output(_M0L7_2aselfS1160, _M0L8_2aparamS1161);
  return 0;
}

void moonbit_init() {
  moonbit_string_t* _M0L6_2atmpS1247 =
    (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1246;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1212;
  moonbit_string_t* _M0L6_2atmpS1245;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1244;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1213;
  moonbit_string_t* _M0L6_2atmpS1243;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1242;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1214;
  moonbit_string_t* _M0L6_2atmpS1241;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1240;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1215;
  moonbit_string_t* _M0L6_2atmpS1239;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1238;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1216;
  moonbit_string_t* _M0L6_2atmpS1237;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1236;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1217;
  moonbit_string_t* _M0L6_2atmpS1235;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1234;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1218;
  moonbit_string_t* _M0L6_2atmpS1233;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1232;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1219;
  moonbit_string_t* _M0L6_2atmpS1231;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1230;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1220;
  moonbit_string_t* _M0L6_2atmpS1229;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1228;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1221;
  moonbit_string_t* _M0L6_2atmpS1227;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1226;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1222;
  moonbit_string_t* _M0L6_2atmpS1225;
  struct _M0TUWEuQRPC15error5ErrorNsE* _M0L8_2atupleS1224;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE* _M0L8_2atupleS1223;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1081;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1211;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1210;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1209;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1200;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1082;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1208;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1207;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1206;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1201;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L7_2abindS1083;
  struct _M0TUiUWEuQRPC15error5ErrorNsEE** _M0L6_2atmpS1205;
  struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE _M0L6_2atmpS1204;
  struct _M0TPB3MapGiUWEuQRPC15error5ErrorNsEE* _M0L6_2atmpS1203;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE* _M0L8_2atupleS1202;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L7_2abindS1080;
  struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE** _M0L6_2atmpS1199;
  struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE _M0L6_2atmpS1198;
  _M0L6_2atmpS1247[0] = (moonbit_string_t)moonbit_string_literal_189.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__0_2eclo);
  _M0L8_2atupleS1246
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1246)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1246->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__0_2eclo;
  _M0L8_2atupleS1246->$1 = _M0L6_2atmpS1247;
  _M0L8_2atupleS1212
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1212)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1212->$0 = 0;
  _M0L8_2atupleS1212->$1 = _M0L8_2atupleS1246;
  _M0L6_2atmpS1245 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1245[0] = (moonbit_string_t)moonbit_string_literal_190.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__1_2eclo);
  _M0L8_2atupleS1244
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1244)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1244->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__1_2eclo;
  _M0L8_2atupleS1244->$1 = _M0L6_2atmpS1245;
  _M0L8_2atupleS1213
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1213)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1213->$0 = 1;
  _M0L8_2atupleS1213->$1 = _M0L8_2atupleS1244;
  _M0L6_2atmpS1243 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1243[0] = (moonbit_string_t)moonbit_string_literal_191.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__2_2eclo);
  _M0L8_2atupleS1242
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1242)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1242->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__2_2eclo;
  _M0L8_2atupleS1242->$1 = _M0L6_2atmpS1243;
  _M0L8_2atupleS1214
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1214)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1214->$0 = 2;
  _M0L8_2atupleS1214->$1 = _M0L8_2atupleS1242;
  _M0L6_2atmpS1241 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1241[0] = (moonbit_string_t)moonbit_string_literal_192.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__3_2eclo);
  _M0L8_2atupleS1240
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1240)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1240->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__3_2eclo;
  _M0L8_2atupleS1240->$1 = _M0L6_2atmpS1241;
  _M0L8_2atupleS1215
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1215)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1215->$0 = 3;
  _M0L8_2atupleS1215->$1 = _M0L8_2atupleS1240;
  _M0L6_2atmpS1239 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1239[0] = (moonbit_string_t)moonbit_string_literal_193.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__4_2eclo);
  _M0L8_2atupleS1238
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1238)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1238->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__4_2eclo;
  _M0L8_2atupleS1238->$1 = _M0L6_2atmpS1239;
  _M0L8_2atupleS1216
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1216)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1216->$0 = 4;
  _M0L8_2atupleS1216->$1 = _M0L8_2atupleS1238;
  _M0L6_2atmpS1237 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1237[0] = (moonbit_string_t)moonbit_string_literal_194.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__5_2eclo);
  _M0L8_2atupleS1236
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1236)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1236->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__5_2eclo;
  _M0L8_2atupleS1236->$1 = _M0L6_2atmpS1237;
  _M0L8_2atupleS1217
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1217)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1217->$0 = 5;
  _M0L8_2atupleS1217->$1 = _M0L8_2atupleS1236;
  _M0L6_2atmpS1235 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1235[0] = (moonbit_string_t)moonbit_string_literal_195.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__6_2eclo);
  _M0L8_2atupleS1234
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1234)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1234->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__6_2eclo;
  _M0L8_2atupleS1234->$1 = _M0L6_2atmpS1235;
  _M0L8_2atupleS1218
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1218)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1218->$0 = 6;
  _M0L8_2atupleS1218->$1 = _M0L8_2atupleS1234;
  _M0L6_2atmpS1233 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1233[0] = (moonbit_string_t)moonbit_string_literal_196.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__7_2eclo);
  _M0L8_2atupleS1232
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1232)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1232->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__7_2eclo;
  _M0L8_2atupleS1232->$1 = _M0L6_2atmpS1233;
  _M0L8_2atupleS1219
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1219)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1219->$0 = 7;
  _M0L8_2atupleS1219->$1 = _M0L8_2atupleS1232;
  _M0L6_2atmpS1231 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1231[0] = (moonbit_string_t)moonbit_string_literal_197.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__8_2eclo);
  _M0L8_2atupleS1230
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1230)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1230->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__8_2eclo;
  _M0L8_2atupleS1230->$1 = _M0L6_2atmpS1231;
  _M0L8_2atupleS1220
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1220)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1220->$0 = 8;
  _M0L8_2atupleS1220->$1 = _M0L8_2atupleS1230;
  _M0L6_2atmpS1229 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1229[0] = (moonbit_string_t)moonbit_string_literal_198.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__9_2eclo);
  _M0L8_2atupleS1228
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1228)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1228->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test47____test__74797065735f746573742e6d6274__9_2eclo;
  _M0L8_2atupleS1228->$1 = _M0L6_2atmpS1229;
  _M0L8_2atupleS1221
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1221)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1221->$0 = 9;
  _M0L8_2atupleS1221->$1 = _M0L8_2atupleS1228;
  _M0L6_2atmpS1227 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1227[0] = (moonbit_string_t)moonbit_string_literal_199.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test48____test__74797065735f746573742e6d6274__10_2eclo);
  _M0L8_2atupleS1226
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1226)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1226->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test48____test__74797065735f746573742e6d6274__10_2eclo;
  _M0L8_2atupleS1226->$1 = _M0L6_2atmpS1227;
  _M0L8_2atupleS1222
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1222)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1222->$0 = 10;
  _M0L8_2atupleS1222->$1 = _M0L8_2atupleS1226;
  _M0L6_2atmpS1225 = (moonbit_string_t*)moonbit_make_ref_array_raw(1);
  _M0L6_2atmpS1225[0] = (moonbit_string_t)moonbit_string_literal_200.data;
  moonbit_incref(_M0FP38clawteam8clawteam22config__blackbox__test48____test__74797065735f746573742e6d6274__11_2eclo);
  _M0L8_2atupleS1224
  = (struct _M0TUWEuQRPC15error5ErrorNsE*)moonbit_malloc(sizeof(struct _M0TUWEuQRPC15error5ErrorNsE));
  Moonbit_object_header(_M0L8_2atupleS1224)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUWEuQRPC15error5ErrorNsE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1224->$0
  = _M0FP38clawteam8clawteam22config__blackbox__test48____test__74797065735f746573742e6d6274__11_2eclo;
  _M0L8_2atupleS1224->$1 = _M0L6_2atmpS1225;
  _M0L8_2atupleS1223
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE*)moonbit_malloc(sizeof(struct _M0TUiUWEuQRPC15error5ErrorNsEE));
  Moonbit_object_header(_M0L8_2atupleS1223)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUiUWEuQRPC15error5ErrorNsEE, $1) >> 2, 1, 0);
  _M0L8_2atupleS1223->$0 = 11;
  _M0L8_2atupleS1223->$1 = _M0L8_2atupleS1224;
  _M0L7_2abindS1081
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_make_ref_array_raw(12);
  _M0L7_2abindS1081[0] = _M0L8_2atupleS1212;
  _M0L7_2abindS1081[1] = _M0L8_2atupleS1213;
  _M0L7_2abindS1081[2] = _M0L8_2atupleS1214;
  _M0L7_2abindS1081[3] = _M0L8_2atupleS1215;
  _M0L7_2abindS1081[4] = _M0L8_2atupleS1216;
  _M0L7_2abindS1081[5] = _M0L8_2atupleS1217;
  _M0L7_2abindS1081[6] = _M0L8_2atupleS1218;
  _M0L7_2abindS1081[7] = _M0L8_2atupleS1219;
  _M0L7_2abindS1081[8] = _M0L8_2atupleS1220;
  _M0L7_2abindS1081[9] = _M0L8_2atupleS1221;
  _M0L7_2abindS1081[10] = _M0L8_2atupleS1222;
  _M0L7_2abindS1081[11] = _M0L8_2atupleS1223;
  _M0L6_2atmpS1211 = _M0L7_2abindS1081;
  _M0L6_2atmpS1210
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 12, _M0L6_2atmpS1211
  };
  #line 398 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1209
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1210);
  _M0L8_2atupleS1200
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1200)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1200->$0 = (moonbit_string_t)moonbit_string_literal_201.data;
  _M0L8_2atupleS1200->$1 = _M0L6_2atmpS1209;
  _M0L7_2abindS1082
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1208 = _M0L7_2abindS1082;
  _M0L6_2atmpS1207
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1208
  };
  #line 412 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1206
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1207);
  _M0L8_2atupleS1201
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1201)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1201->$0 = (moonbit_string_t)moonbit_string_literal_202.data;
  _M0L8_2atupleS1201->$1 = _M0L6_2atmpS1206;
  _M0L7_2abindS1083
  = (struct _M0TUiUWEuQRPC15error5ErrorNsEE**)moonbit_empty_ref_array;
  _M0L6_2atmpS1205 = _M0L7_2abindS1083;
  _M0L6_2atmpS1204
  = (struct _M0TPB9ArrayViewGUiUWEuQRPC15error5ErrorNsEEE){
    0, 0, _M0L6_2atmpS1205
  };
  #line 414 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L6_2atmpS1203
  = _M0MPB3Map11from__arrayGiUWEuQRPC15error5ErrorNsEE(_M0L6_2atmpS1204);
  _M0L8_2atupleS1202
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE*)moonbit_malloc(sizeof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE));
  Moonbit_object_header(_M0L8_2atupleS1202)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE, $0) >> 2, 2, 0);
  _M0L8_2atupleS1202->$0 = (moonbit_string_t)moonbit_string_literal_203.data;
  _M0L8_2atupleS1202->$1 = _M0L6_2atmpS1203;
  _M0L7_2abindS1080
  = (struct _M0TUsRPB3MapGiUWEuQRPC15error5ErrorNsEEE**)moonbit_make_ref_array_raw(3);
  _M0L7_2abindS1080[0] = _M0L8_2atupleS1200;
  _M0L7_2abindS1080[1] = _M0L8_2atupleS1201;
  _M0L7_2abindS1080[2] = _M0L8_2atupleS1202;
  _M0L6_2atmpS1199 = _M0L7_2abindS1080;
  _M0L6_2atmpS1198
  = (struct _M0TPB9ArrayViewGUsRPB3MapGiUWEuQRPC15error5ErrorNsEEEE){
    0, 3, _M0L6_2atmpS1199
  };
  #line 397 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0FP38clawteam8clawteam22config__blackbox__test48moonbit__test__driver__internal__no__args__tests
  = _M0MPB3Map11from__arrayGsRPB3MapGiUWEuQRPC15error5ErrorNsEEE(_M0L6_2atmpS1198);
}

int main(int argc, char** argv) {
  struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error** _M0L6_2atmpS1197;
  struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE* _M0L12async__testsS1150;
  struct _M0TPB5ArrayGUsiEE* _M0L7_2abindS1151;
  int32_t _M0L7_2abindS1152;
  int32_t _M0L2__S1153;
  moonbit_runtime_init(argc, argv);
  moonbit_init();
  _M0L6_2atmpS1197
  = (struct _M0TWWuEuWRPC15error5ErrorEuEOuQRPC15error5Error**)moonbit_empty_ref_array;
  _M0L12async__testsS1150
  = (struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE*)moonbit_malloc(sizeof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE));
  Moonbit_object_header(_M0L12async__testsS1150)->meta
  = Moonbit_make_regular_object_header(offsetof(struct _M0TPB5ArrayGVWEuQRPC15error5ErrorE, $0) >> 2, 1, 0);
  _M0L12async__testsS1150->$0 = _M0L6_2atmpS1197;
  _M0L12async__testsS1150->$1 = 0;
  #line 453 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0L7_2abindS1151
  = _M0FP38clawteam8clawteam22config__blackbox__test52moonbit__test__driver__internal__native__parse__args();
  _M0L7_2abindS1152 = _M0L7_2abindS1151->$1;
  _M0L2__S1153 = 0;
  while (1) {
    if (_M0L2__S1153 < _M0L7_2abindS1152) {
      struct _M0TUsiE** _M0L8_2afieldS3236 = _M0L7_2abindS1151->$0;
      struct _M0TUsiE** _M0L3bufS1196 = _M0L8_2afieldS3236;
      struct _M0TUsiE* _M0L6_2atmpS3235 =
        (struct _M0TUsiE*)_M0L3bufS1196[_M0L2__S1153];
      struct _M0TUsiE* _M0L3argS1154 = _M0L6_2atmpS3235;
      moonbit_string_t _M0L8_2afieldS3234 = _M0L3argS1154->$0;
      moonbit_string_t _M0L6_2atmpS1193 = _M0L8_2afieldS3234;
      int32_t _M0L8_2afieldS3233 = _M0L3argS1154->$1;
      int32_t _M0L6_2atmpS1194 = _M0L8_2afieldS3233;
      int32_t _M0L6_2atmpS1195;
      moonbit_incref(_M0L6_2atmpS1193);
      moonbit_incref(_M0L12async__testsS1150);
      #line 454 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
      _M0FP38clawteam8clawteam22config__blackbox__test44moonbit__test__driver__internal__do__execute(_M0L12async__testsS1150, _M0L6_2atmpS1193, _M0L6_2atmpS1194);
      _M0L6_2atmpS1195 = _M0L2__S1153 + 1;
      _M0L2__S1153 = _M0L6_2atmpS1195;
      continue;
    } else {
      moonbit_decref(_M0L7_2abindS1151);
    }
    break;
  }
  #line 456 "E:\\moonbit\\clawteam\\config\\__generated_driver_for_blackbox_test.mbt"
  _M0IP016_24default__implP38clawteam8clawteam22config__blackbox__test28MoonBit__Async__Test__Driver17run__async__testsGRP38clawteam8clawteam22config__blackbox__test34MoonBit__Async__Test__Driver__ImplE(_M0L12async__testsS1150);
  return 0;
}