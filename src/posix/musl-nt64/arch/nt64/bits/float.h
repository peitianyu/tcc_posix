/* tcc_posix: TCC 的 long double == double (53 位 mantissa),
   非 x86_64 的 80 位 x87。按 double 语义声明, 使 musl 的
   LDBL_MANT_DIG==53 适配分支生效 (vfprintf/frexpl/floatscan/libm)。 */
#define LDBL_TRUE_MIN 4.94065645841246544177e-324L
#define LDBL_MIN     2.22507385850720138309e-308L
#define LDBL_MAX     1.79769313486231570815e+308L
#define LDBL_EPSILON 2.22044604925031308085e-16L

#define LDBL_MANT_DIG 53
#define LDBL_MIN_EXP (-1021)
#define LDBL_MAX_EXP 1024

#define LDBL_DIG 15
#define LDBL_MIN_10_EXP (-307)
#define LDBL_MAX_10_EXP 308

#define DECIMAL_DIG 17
