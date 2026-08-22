/* t046_simd.c - SIMD B1 打包 SSE 指令测试 (点积 / 基本运算 / 字段访问) */
#include "simd.h"
#include <stdio.h>
#include <math.h>

static int fails = 0;
static void check(int c, const char *m)
{
    if (getenv("SIMD_TRACE"))
        printf("TRACE %s\n", m), fflush(stdout);
    if (!c) {
        printf("FAIL %s\n", m);
        fflush(stdout);
        fails++;
    }
}

int main(void)
{
    float a[4] = {1.f, 2.f, 3.f, 4.f};
    float b[4] = {5.f, 6.f, 7.f, 8.f};
    float o[4];

    /* load + mul (点积核心) */
    v4f av = _mm_load_ps(a);
    v4f bv = _mm_load_ps(b);
    v4f prod = _mm_mul_ps(av, bv);
    _mm_store_ps(o, prod);
    check(o[0]==5.f && o[1]==12.f && o[2]==21.f && o[3]==32.f, "mul");

    /* setzero */
    v4f z = _mm_setzero_ps();
    _mm_store_ps(o, z);
    check(o[0]==0.f && o[1]==0.f && o[2]==0.f && o[3]==0.f, "zero");

    /* add */
    v4f sum = _mm_add_ps(av, bv);
    _mm_store_ps(o, sum);
    check(o[0]==6.f && o[1]==8.f && o[2]==10.f && o[3]==12.f, "add");

    /* sub */
    v4f sub = _mm_sub_ps(bv, av);
    _mm_store_ps(o, sub);
    check(o[0]==4.f && o[1]==4.f && o[2]==4.f && o[3]==4.f, "sub");

    /* div */
    v4f div = _mm_div_ps(bv, av);
    _mm_store_ps(o, div);
    if (!(fabsf(o[0]-5.f)<1e-5f && fabsf(o[1]-3.f)<1e-5f &&
          fabsf(o[2]-7.0f/3.f)<1e-5f && fabsf(o[3]-2.f)<1e-5f))
        fails++;
    check(fabsf(o[0]-5.f)<1e-5f, "div");

    /* 点积累加 a·b = 1*5+2*6+3*7+4*8 = 70 */
    v4f dacc = _mm_add_ps(_mm_mul_ps(av, bv), _mm_setzero_ps());
    check(dacc.x==5.f, "field access on expr");

    /* ---- double (v2d): addpd/subpd/mulpd/divpd ---- */
    double da[2] __attribute__((aligned(16))) = {1.5, 3.5};
    double db[2] __attribute__((aligned(16))) = {2.0, 4.0};
    double do2[2] __attribute__((aligned(16)));
    v2d dav = _mm_load_pd(da);
    v2d dbv = _mm_load_pd(db);
    _mm_store_pd(do2, _mm_add_pd(dav, dbv));
    check(do2[0]==3.5 && do2[1]==7.5, "dpd add");
    _mm_store_pd(do2, _mm_sub_pd(dbv, dav));
    check(do2[0]==0.5 && do2[1]==0.5, "dpd sub");
    _mm_store_pd(do2, _mm_mul_pd(dav, dbv));
    check(do2[0]==3.0 && do2[1]==14.0, "dpd mul");
    _mm_store_pd(do2, _mm_div_pd(dbv, dav));
    check(do2[0]==4.0/3.0 && do2[1]==8.0/7.0, "dpd div");
    double dz[2] __attribute__((aligned(16)));
    _mm_store_pd(dz, _mm_setzero_pd());
    check(dz[0]==0.0 && dz[1]==0.0, "dpd zero");

    /* ---- int32 (v4i): paddd/psubd, pmulld, 标量 idiv ---- */
    int ia[4] __attribute__((aligned(16))) = {10, -5, 30, 40};
    int ib[4] __attribute__((aligned(16))) = {2, 5, 7, 4};
    int io2[4] __attribute__((aligned(16)));
    v4i iav = _mm_load_epi32(ia);
    v4i ibv = _mm_load_epi32(ib);
    _mm_store_epi32(io2, _mm_add_epi32(iav, ibv));
    check(io2[0]==12 && io2[1]==0 && io2[2]==37 && io2[3]==44, "epi add");
    _mm_store_epi32(io2, _mm_sub_epi32(iav, ibv));
    check(io2[0]==8 && io2[1]==-10 && io2[2]==23 && io2[3]==36, "epi sub");
    _mm_store_epi32(io2, _mm_mul_epi32(iav, ibv));
    check(io2[0]==20 && io2[1]==-25 && io2[2]==210 && io2[3]==160, "epi mul");
    _mm_store_epi32(io2, _mm_div_epi32(iav, ibv));
    check(io2[0]==5 && io2[1]==-1 && io2[2]==4 && io2[3]==10, "epi div");
    int iz[4] __attribute__((aligned(16)));
    _mm_store_epi32(iz, _mm_setzero_epi32());
    check(iz[0]==0 && iz[1]==0 && iz[2]==0 && iz[3]==0, "epi zero");

    /* ---- int16 (v8h): paddw/psubw + pmullw + 标量除法 ---- */
    short sa[8] __attribute__((aligned(16))) = {10,-8,20,33,-50,60,7,9};
    short sb[8] __attribute__((aligned(16))) = {2,4,5,11,25,6,7,3};
    short so[8] __attribute__((aligned(16)));
    v8h sav = _mm_load_epi16(sa), sbv = _mm_load_epi16(sb);
    _mm_store_epi16(so, _mm_add_epi16(sav, sbv));
    check(so[0]==12 && so[1]==-4 && so[2]==25 && so[3]==44 &&
          so[4]==-25 && so[5]==66 && so[6]==14 && so[7]==12, "epi16 add");
    _mm_store_epi16(so, _mm_sub_epi16(sav, sbv));
    check(so[0]==8 && so[1]==-12 && so[2]==15 && so[3]==22 && so[4]==-75, "epi16 sub");
    _mm_store_epi16(so, _mm_mul_epi16(sav, sbv));
    check(so[0]==20 && so[1]==-32 && so[2]==100 && so[3]==363 && so[4]==-1250
          && so[5]==360 && so[6]==49 && so[7]==27, "epi16 mul");
    _mm_store_epi16(so, _mm_div_epi16(sav, sbv));
    check(so[0]==5 && so[1]==-2 && so[2]==4 && so[3]==3 && so[4]==-2
          && so[5]==10 && so[6]==1 && so[7]==3, "epi16 div");
    /* 无符号除法: -8(signed) 视为 0xFFF8=65528, 65528/4=16382 */
    _mm_store_epi16(so, _mm_div_epu16(sav, sbv));
    check(so[0]==5 && so[1]==16382 && so[6]==1 && so[7]==3, "epi16 udiv");

    /* ---- int8 (v16b): paddb/psubb + 标量乘除 ---- */
    signed char ba[16] __attribute__((aligned(16))) =
        {10,20,-30,-40,50,60,-70,80,-90,100,110,-120,1,2,3,4};
    signed char bb[16] __attribute__((aligned(16))) =
        {2,5,6,4,5,6,7,8,9,10,11,12,2,2,2,2};
    signed char bo_[16] __attribute__((aligned(16)));
    v16b bav = _mm_load_epi8(ba), bbv = _mm_load_epi8(bb);
    _mm_store_epi8(bo_, _mm_add_epi8(bav, bbv));
    check(bo_[0]==12 && bo_[1]==25 && bo_[2]==-24 && bo_[3]==-36, "epi8 add");
    _mm_store_epi8(bo_, _mm_sub_epi8(bav, bbv));
    check(bo_[0]==8 && bo_[1]==15 && bo_[2]==-36 && bo_[3]==-44, "epi8 sub");
    _mm_store_epi8(bo_, _mm_mul_epi8(bav, bbv));
    /* 8 位乘积取低 8 位: -30*6=0x54C->0x4C=76, -40*4=0x360->0x60=96,
       -70(0xBA)*7=186*7=1302=0x516->低字节0x16=22, 80*8=0x280->0x80=-128 */
    check(bo_[0]==20 && bo_[1]==100 && bo_[2]==76 && bo_[3]==96
          && bo_[4]==-6 && bo_[5]==104 && bo_[6]==22 && bo_[7]==-128
          && bo_[12]==2 && bo_[13]==4 && bo_[14]==6 && bo_[15]==8, "epi8 mul");
    _mm_store_epi8(bo_, _mm_div_epi8(bav, bbv));
    check(bo_[0]==5 && bo_[1]==4 && bo_[2]==-5 && bo_[3]==-10
          && bo_[12]==0 && bo_[13]==1 && bo_[14]==1 && bo_[15]==2, "epi8 div");

    /* =================== 常用扩展 =================== */

    /* ---- 整型位运算 (pand/por/pxor/pandn) via _mm_*_si128 ---- */
    {
        int wa[4] __attribute__((aligned(16))) = {12, 10, 3, 0};
        int wb[4] __attribute__((aligned(16))) = {10, 6, 5, -1};
        int wo[4] __attribute__((aligned(16)));
        v4i wav = _mm_load_epi32(wa), wbv = _mm_load_epi32(wb);
        _mm_store_epi32(wo, _mm_and_si128(wav, wbv));
        check(wo[0]==8 && wo[1]==2 && wo[2]==1 && wo[3]==0, "si128 and");
        _mm_store_epi32(wo, _mm_or_si128(wav, wbv));
        check(wo[0]==14 && wo[1]==14 && wo[2]==7 && wo[3]==-1, "si128 or");
        _mm_store_epi32(wo, _mm_xor_si128(wav, wbv));
        check(wo[0]==6 && wo[1]==12 && wo[2]==6 && wo[3]==-1, "si128 xor");
        _mm_store_epi32(wo, _mm_andnot_si128(wav, wbv));  /* ~a & b */
        check(wo[0]==2 && wo[1]==4 && wo[2]==4 && wo[3]==-1, "si128 andnot");
    }

    /* ---- int32 min/max 有符号/无符号 (SSE4.1) ---- */
    {
        int ma[4] __attribute__((aligned(16))) = {10, -5, 30, 40};
        int mb[4] __attribute__((aligned(16))) = {2, 5, 7, 4};
        int mo[4] __attribute__((aligned(16)));
        unsigned int ua[4] __attribute__((aligned(16))) = {0xFFFFFFF0u, 1u, 100u, 0x80000000u};
        unsigned int ub[4] __attribute__((aligned(16))) = {5u, 0xFFFFFFFFu, 200u, 3u};
        unsigned int uo[4] __attribute__((aligned(16)));
        v4i mav = _mm_load_epi32(ma), mbv = _mm_load_epi32(mb);
        _mm_store_epi32(mo, _mm_min_epi32(mav, mbv));
        check(mo[0]==2 && mo[1]==-5 && mo[2]==7 && mo[3]==4, "epi32 min");
        _mm_store_epi32(mo, _mm_max_epi32(mav, mbv));
        check(mo[0]==10 && mo[1]==5 && mo[2]==30 && mo[3]==40, "epi32 max");
        _mm_store_epi32(uo, _mm_min_epu32(_mm_load_epi32((int*)ua), _mm_load_epi32((int*)ub)));
        check(uo[0]==5 && uo[1]==1 && uo[2]==100 && uo[3]==3, "epu32 min");
        _mm_store_epi32(uo, _mm_max_epu32(_mm_load_epi32((int*)ua), _mm_load_epi32((int*)ub)));
        check(uo[0]==0xFFFFFFF0u && uo[1]==0xFFFFFFFFu && uo[2]==200 && uo[3]==0x80000000u,
              "epu32 max");
    }

    /* ---- int32 比较 -> v4i 掩码 (pcmpeqd/pcmpgtd) ---- */
    {
        int ca[4] __attribute__((aligned(16))) = {3, 7, 7, 10};
        int cb[4] __attribute__((aligned(16))) = {7, 7, 5, 10};
        int cm[4] __attribute__((aligned(16)));
        v4i cav = _mm_load_epi32(ca), cbv = _mm_load_epi32(cb);
        _mm_store_epi32(cm, _mm_cmpeq_epi32(cav, cbv));
        check(cm[0]==0 && cm[1]==-1 && cm[2]==0 && cm[3]==-1, "epi32 cmpeq");
        _mm_store_epi32(cm, _mm_cmpgt_epi32(cav, cbv));
        check(cm[0]==0 && cm[1]==0 && cm[2]==-1 && cm[3]==0, "epi32 cmpgt");
        _mm_store_epi32(cm, _mm_cmplt_epi32(cav, cbv));
        check(cm[0]==-1 && cm[1]==0 && cm[2]==0 && cm[3]==0, "epi32 cmplt");
    }

    /* ---- int32 移位 imm (pslld/psrld/psrad) ---- */
    {
        int sa[4] __attribute__((aligned(16))) = {1, 8, -16, 0x1000};
        int so2[4] __attribute__((aligned(16)));
        v4i sav = _mm_load_epi32(sa);
        _mm_store_epi32(so2, _mm_srli_epi32(sav, 2));
        check(so2[0]==0 && so2[1]==2 && so2[2]==0x3FFFFFFC && so2[3]==0x400, "epi32 srl");
        _mm_store_epi32(so2, _mm_srai_epi32(sav, 2));
        check(so2[0]==0 && so2[1]==2 && so2[2]==-4 && so2[3]==0x400, "epi32 sra");
        _mm_store_epi32(so2, _mm_slli_epi32(sav, 2));
        check(so2[0]==4 && so2[1]==32 && so2[2]==-64 && so2[3]==0x4000, "epi32 sll");
    }

    /* ---- float min/max/sqrt (minps/maxps/sqrtps) ---- */
    {
        static const float fa[4] __attribute__((aligned(16))) = {1.f, 9.f, -4.f, 16.f};
        static const float fb[4] __attribute__((aligned(16))) = {2.f, 3.f, 5.f, 0.5f};
        static const float fs[4] __attribute__((aligned(16))) = {1.f, 4.f, 9.f, 16.f};
        float fo[4];
        _mm_store_ps(fo, _mm_min_ps(_mm_load_ps(fa), _mm_load_ps(fb)));
        check(fo[0]==1.f && fo[1]==3.f && fo[2]==-4.f && fo[3]==0.5f, "ps min");
        _mm_store_ps(fo, _mm_max_ps(_mm_load_ps(fa), _mm_load_ps(fb)));
        check(fo[0]==2.f && fo[1]==9.f && fo[2]==5.f && fo[3]==16.f, "ps max");
        _mm_store_ps(fo, _mm_sqrt_ps(_mm_load_ps(fs)));
        check(fabsf(fo[0]-1.f)<1e-6f && fabsf(fo[1]-2.f)<1e-6f
              && fabsf(fo[2]-3.f)<1e-6f && fabsf(fo[3]-4.f)<1e-6f, "ps sqrt");
    }

    /* ---- float 比较 -> 掩码槽 (cmpps; 检查 32 位掩码位) ---- */
    {
        static const float ca[4] __attribute__((aligned(16))) = {1.f, 2.f, 3.f, 4.f};
        static const float cb[4] __attribute__((aligned(16))) = {1.f, 5.f, 3.f, 2.f};
        float cm[4];
#define FBIT(v) (*(unsigned*)&(v))   /* 读掩码: 全 1 = true, 0 = false */
        _mm_store_ps(cm, _mm_cmpeq_ps(_mm_load_ps(ca), _mm_load_ps(cb)));
        check(FBIT(cm[0])==~0u && FBIT(cm[1])==0 && FBIT(cm[2])==~0u && FBIT(cm[3])==0, "ps cmpeq");
        _mm_store_ps(cm, _mm_cmplt_ps(_mm_load_ps(ca), _mm_load_ps(cb)));
        check(FBIT(cm[0])==0 && FBIT(cm[1])==~0u && FBIT(cm[2])==0 && FBIT(cm[3])==0, "ps cmplt");
        _mm_store_ps(cm, _mm_cmpgt_ps(_mm_load_ps(ca), _mm_load_ps(cb)));
        check(FBIT(cm[0])==0 && FBIT(cm[1])==0 && FBIT(cm[2])==0 && FBIT(cm[3])==~0u, "ps cmpgt");
        _mm_store_ps(cm, _mm_cmpge_ps(_mm_load_ps(ca), _mm_load_ps(cb)));
        check(FBIT(cm[0])==~0u && FBIT(cm[1])==0 && FBIT(cm[2])==~0u && FBIT(cm[3])==~0u, "ps cmpge");
        _mm_store_ps(cm, _mm_cmpneq_ps(_mm_load_ps(ca), _mm_load_ps(cb)));
        check(FBIT(cm[0])==0 && FBIT(cm[1])==~0u && FBIT(cm[2])==0 && FBIT(cm[3])==~0u, "ps cmpneq");
#undef FBIT
    }

    /* ---- double min/max/sqrt + 比较 (minpd/maxpd/sqrtpd/cmppd) ---- */
    {
        static const double da[2] __attribute__((aligned(16))) = {1., 9.};
        static const double db[2] __attribute__((aligned(16))) = {2., 3.};
        static const double ds[2] __attribute__((aligned(16))) = {1., 4.};
        double d[2] __attribute__((aligned(16)));
        _mm_store_pd(d, _mm_min_pd(_mm_load_pd(da), _mm_load_pd(db)));
        check(d[0]==1. && d[1]==3., "pd min");
        _mm_store_pd(d, _mm_max_pd(_mm_load_pd(da), _mm_load_pd(db)));
        check(d[0]==2. && d[1]==9., "pd max");
        _mm_store_pd(d, _mm_sqrt_pd(_mm_load_pd(ds)));
        check(d[0]==1. && d[1]==2., "pd sqrt");
    }

    /* ---- 类型转换 int32<->float32 ---- */
    {
        static const int ia[4] __attribute__((aligned(16))) = {1, -2, 3, -4};
        static const float fa[4] __attribute__((aligned(16))) = {1.9f, -2.9f, 3.0f, 4.2f};
        float fo[4];
        int io2[4] __attribute__((aligned(16)));
        _mm_store_ps(fo, _mm_cvtepi32_ps(_mm_load_epi32((int*)ia)));
        check(fo[0]==1.f && fo[1]==-2.f && fo[2]==3.f && fo[3]==-4.f, "cvtdq2ps");
        _mm_store_epi32(io2, _mm_cvttps_epi32(_mm_load_ps(fa)));   /* 截断 */
        check(io2[0]==1 && io2[1]==-2 && io2[2]==3 && io2[3]==4, "cvttps2dq");
        _mm_store_epi32(io2, _mm_cvtps_epi32(_mm_load_ps(fa)));    /* 舍入 */
        check(io2[0]==2 && io2[1]==-3 && io2[2]==3 && io2[3]==4, "cvtps2dq");
    }

    if (fails) {
        printf("t046 FAILED (%d)\n", fails);
        return 1;
    }
    printf("t046 PASS\n");
    return 0;
}