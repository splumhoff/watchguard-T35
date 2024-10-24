/*
 * Portions Copyrighted by Freescale Semiconductor, Inc., 2010-2011
 */

#include "fsl_secboot_types.h"
#include <linux/string.h>

static u32 STDMIN(u32 a, u32 b)
{
       return b < a ? b : a;
}

static size_t BitsToWords(size_t bitCount)
{
       return (bitCount + WORD_BITS - 1) / (WORD_BITS);
}

static size_t CountWords(const u32 *X, size_t N)
{
       while (N && X[N - 1] == 0)
               N--;

       return N;
}

static void SetWords(u32 *r, size_t n)
{
       int i;

       for (i = n - 1; i >= 0; i--)
               r[i] = 0;
}

static void CopyWords(u32 *r, const u32 *a, size_t n)
{
       if (r != a)
               memcpy(r, a, n * WORD_SIZE);
}

static u32 ShiftWordsLeftByBits(u32 *r, size_t n, unsigned int shiftBits)
{
       int i;
       u32 u, carry = 0;
       if (shiftBits) {
               for (i = 0; i < n; i++) {
                       u = r[i];
                       r[i] = (u << shiftBits) | carry;
                       carry = u >> (WORD_BITS - shiftBits);
               }
       }
       return carry;
}

static u32 ShiftWordsRightByBits(u32 *r, size_t n, unsigned int shiftBits)
{
       int i;
       u32 u, carry = 0;
       if (shiftBits) {
               for (i = n; i > 0; i--) {
                       u = r[i - 1];
                       r[i - 1] = (u >> shiftBits) | carry;
                       carry = u << (WORD_BITS - shiftBits);
               }
       }
       return carry;
}

static void ShiftWordsLeftByWords(u32 *r, size_t n, size_t shiftWords)
{
       int i;
       shiftWords = STDMIN(shiftWords, n);
       if (shiftWords) {
               for (i = n - 1; i >= shiftWords; i--)
                       r[i] = r[i - shiftWords];

               SetWords(r, shiftWords);
       }
}

static void ShiftWordsRightByWords(u32 *r, size_t n, size_t shiftWords)
{
       int i;
       shiftWords = STDMIN(shiftWords, n);
       if (shiftWords) {
               for (i = 0; i + shiftWords < n; i++)
                       r[i] = r[i + shiftWords];

               SetWords(r + n - shiftWords, shiftWords);
       }
}

u32 divide64(u64 *number, u32 divisor)
{
       u64 rem = *number;
       u64 base = divisor;
       u64 res, tmp = 1;
       u32 high = rem >> 32;

       res = 0;
       if (high >= divisor) {
               high /= divisor;
               res = (u64) high << 32;
               rem -= (u64) (high * divisor) << 32;
       }

       while ((int64_t) base > 0 && base < rem) {
               base = base + base;
               tmp = tmp + tmp;
       }

       do {
               if (rem >= base) {
                       rem -= base;
                       res += tmp;
               }
               base >>= 1;
               tmp >>= 1;
       } while (tmp);

       *number = res;
       return rem;
}

static unsigned int BitPrecision(u32 value)
{
       unsigned int l = 0, h = 8 * sizeof(value);

       unsigned int t;

       if (!value)
               return 0;

       while (h - l > 1) {
               t = (l + h) / 2;

               if (value >> t)
                       l = t;
               else
                       h = t;
       }

       return h;
}

static void swap(u32 *A, u32 *B)
{
       u32 tmp = *A;
       *A = *B;
       *B = tmp;
}
static int Compare(const u32 *A, const u32 *B, size_t N)
{
       while (N--)
               if (A[N] > B[N])
                       return 1;
               else if (A[N] < B[N])
                       return -1;

       return 0;
}
static int Increment_3(u32 *A, size_t N, u32 B)
{
       int i;
       u32 t = A[0];
       A[0] = t + B;
       if (A[0] >= t)
               return 0;
       for (i = 1; i < N; i++)
               if (++A[i])
                       return 0;
       return 1;
}

static int Decrement_3(u32 *A, size_t N, u32 B)
{
       int i;
       u32 t = A[0];
       A[0] = t - B;
       if (A[0] <= t)
               return 0;
       for (i = 1; i < N; i++)
               if (A[i]--)
                       return 0;
       return 1;
}
static void TwosComplement(u32 *A, size_t N)
{
       int i;
       Decrement_3(A, N, 1);
       for (i = 0; i < N; i++)
               A[i] = ~A[i];
}

static u32 AtomicInverseModPower2(u32 A)
{
       int i;

       u32 R = A % 8;

       for (i = 3; i < WORD_BITS; i *= 2)
               R = R * (2 - R * A);

       return R;
}

#define Declare2Words(x)               u64 x;
#define MultiplyWords(p, a, b)         p = (u64)a*b;
#define AssignWord(a, b)               a = b;
#define Add2WordsBy1(a, b, c)          a = b + c;
#define Acc2WordsBy2(a, b)             a += b;
#define LowWord(a)                     (u32)(a)
#define HighWord(a)                    (u32)(a>>WORD_BITS)
#define Double3Words(c, d)             d = 2*d + (c>>(WORD_BITS-1)); c *= 2;
#define AddWithCarry(u, a, b)          u = (u64)(a) + b + GetCarry(u);
#define SubtractWithBorrow(u, a, b)    u = (u64)(a) - b - GetBorrow(u);
#define GetCarry(u)                    HighWord(u)
#define GetBorrow(u)                   ((u32)(u>>(WORD_BITS*2-1)))
#define MulAcc(c, d, a, b)             MultiplyWords(p, a, b); Acc2WordsBy1(p, c); c = LowWord(p); Acc2WordsBy1(d, HighWord(p));
#define Acc2WordsBy1(a, b)             Add2WordsBy1(a, a, b)
#define Acc3WordsBy2(c, d, e)          Acc2WordsBy1(e, c); c = LowWord(e); Add2WordsBy1(e, d, HighWord(e));

static void pMulAcc(u32 *c, u64 *d, u32 a, u32 b, u64 *p)
{
       MultiplyWords(*p, a, b);
       Acc2WordsBy1(*p, *c);
       *c = LowWord(*p);
       Acc2WordsBy1(*d, HighWord(*p));
}

static void DWord_2(DWord *dw, u32 low)
{
       dw->m_whole = low;
}

static void DWord_3(DWord *dw, u32 low, u32 high)
{
       dw->m_halfs.low = low;
       dw->m_halfs.high = high;
}

static void DWord__Multiply(DWord *dw, u32 a, u32 b)
{
       dw->m_whole = (u64) a * b;
}

static void DWord__opPlusEQ(DWord *dw, u32 a)
{
       dw->m_whole = dw->m_whole + a;
}

static void DWord__opMinus_DWord(DWord *dw, DWord *a)
{
       dw->m_whole = dw->m_whole - a->m_whole;
}

static void DWord__opMinus_word(DWord *ret, DWord *dw, u32 a)
{
       ret->m_whole = dw->m_whole - a;
}

static u32 DWord__opDivide(DWord *dw, u32 divisor)
{
       u64 tmp = dw->m_whole;
       div(tmp, divisor);
       return (u32) tmp;
}

static bool DWord__operatorNOT(DWord *dw)
{
       return !dw->m_whole;
}

static u32 DWord__operatorMod(DWord *dw, u32 a)
{
       u32 mod;
       u64 tmp = dw->m_whole;
       mod = div(tmp, a);
       return mod;
}

static u32 DWord__GetLowHalf(DWord *dw)
{
       return dw->m_halfs.low;
}

static u32 DWord__GetHighHalf(DWord *dw)
{
       return dw->m_halfs.high;
}

static u32 DWord__GetHighHalfAsBorrow(DWord *dw)
{
       return 0 - dw->m_halfs.high;
}

static u32 DivideThreeWordsByTwo(u32 *A, u32 B0, u32 B1)
{

       /* estimate the quotient: do a 2 S by 1 S divide */
       u32 Q;
       u32 barrow;
       DWord p;
       DWord tmp, u;
       DWord tmp2;

       if ((u32) (B1 + 1) == 0) {
               Q = A[2];
       } else {
               DWord_3(&tmp, A[1], A[2]);
               Q = DWord__opDivide(&tmp, B1 + 1);
       }

       /* now subtract Q*B from A */
       DWord__Multiply(&p, B0, Q);

       DWord_2(&tmp, A[0]);
       DWord__opMinus_word(&u, &tmp, DWord__GetLowHalf(&p));
       A[0] = DWord__GetLowHalf(&u);

       barrow = DWord__GetHighHalfAsBorrow(&u);
       DWord_2(&tmp, A[1]);
       DWord__opMinus_word(&u, &tmp, DWord__GetHighHalf(&p));
       DWord__opMinus_word(&u, &u, barrow);
       DWord__Multiply(&tmp, B1, Q);
       DWord__opMinus_DWord(&u, &tmp);

       A[1] = DWord__GetLowHalf(&u);
       A[2] += DWord__GetHighHalf(&u);

       /* Q <= actual quotient, so fix it */
       while (A[2] || A[1] > B1 || (A[1] == B1 && A[0] >= B0)) {
               DWord_2(&tmp, A[0]);
               DWord__opMinus_word(&u, &tmp, B0);
               A[0] = DWord__GetLowHalf(&u);

               DWord_2(&tmp, A[1]);
               DWord__opMinus_word(&tmp2, &tmp, B1);
               DWord__opMinus_word(&u, &tmp2, DWord__GetHighHalfAsBorrow(&u));
               A[1] = DWord__GetLowHalf(&u);
               A[2] += DWord__GetHighHalf(&u);
               Q++;
       }

       return Q;
}

static void DivideFourWordsByTwo(u32 *T, DWord *Al, DWord *Ah, DWord *B,
                                DWord *ret)
{
       u32 Q[2];

       if (DWord__operatorNOT(B)) {    /* if divisor is 0, we assume divisor==2**(2*WORD_BITS) */
               DWord_3(ret, DWord__GetLowHalf(Ah), DWord__GetHighHalf(Ah));
       } else {
               T[0] = DWord__GetLowHalf(Al);
               T[1] = DWord__GetHighHalf(Al);
               T[2] = DWord__GetLowHalf(Ah);
               T[3] = DWord__GetHighHalf(Ah);
               Q[1] =
                   DivideThreeWordsByTwo(T + 1, DWord__GetLowHalf(B),
                                         DWord__GetHighHalf(B));
               Q[0] =
                   DivideThreeWordsByTwo(T, DWord__GetLowHalf(B),
                                         DWord__GetHighHalf(B));
               DWord_3(ret, Q[0], Q[1]);
       }
}

static int Baseline_Add(size_t N, u32 *C, const u32 *A, const u32 *B)
{

       int i;
       Declare2Words(u);
       AssignWord(u, 0);
       for (i = 0; i < N; i += 2) {
               AddWithCarry(u, A[i], B[i]);
               C[i] = LowWord(u);
               AddWithCarry(u, A[i + 1], B[i + 1]);
               C[i + 1] = LowWord(u);
       }
       return (int)(GetCarry(u));
}

static int Baseline_Sub(size_t N, u32 *C, const u32 *A, const u32 *B)
{

       int i;
       Declare2Words(u);
       AssignWord(u, 0);
       for (i = 0; i < N; i += 2) {
               SubtractWithBorrow(u, A[i], B[i]);
               C[i] = LowWord(u);
               SubtractWithBorrow(u, A[i + 1], B[i + 1]);
               C[i + 1] = LowWord(u);
       }
       return (int)(GetBorrow(u));
}

static u32 LinearMultiply(u32 *C, const u32 *A, u32 B, size_t N)
{
       int i;
       u32 carry = 0;
       for (i = 0; i < N; i++) {
               Declare2Words(p);
               MultiplyWords(p, A[i], B);
               Acc2WordsBy1(p, carry);
               C[i] = LowWord(p);
               carry = HighWord(p);
       }
       return carry;
}

#define Mul_Begin                              \
       Declare2Words(p)                                \
       u32 c;  \
       Declare2Words(d)                                \
       MultiplyWords(p, A[0], B[0])    \
       c = LowWord(p);         \
       AssignWord(d, HighWord(p))

void pMul_Acc(u32 *c, u64 *d, const u32 *A, const u32 *B, u32 i,
               u32 j, u64 *p)
{
       pMulAcc(c, d, A[i], B[j], p);
}
void pMul_SaveAcc(u32 k, u32 i, u32 j, u32 *c, u64 *d, u32 *R,
                 const u32 *A, const u32 *B, u64 *p)
{
       R[k] = *c;
       *c = LowWord(*d);
       *d = HighWord(*d);
       pMulAcc(c, d, A[i], B[j], p);
}

#define Mul_End(k, i)                                  \
       R[k] = c;                       \
       MultiplyWords(p, A[i], B[i])    \
       Acc2WordsBy2(p, d)                              \
       R[k+1] = LowWord(p);                    \
       R[k+2] = HighWord(p);

#define Bot_SaveAcc(k, i, j)           \
       R[k] = c;                               \
       c = LowWord(d); \
       c += A[i] * B[j];

#define Bot_Acc(i, j)  \
       c += A[i] * B[j];

#define Bot_End(n)             \
       R[n-1] = c;

#define Squ_Begin(n)                           \
       Declare2Words(p)                                \
       u32 c;                          \
       Declare2Words(d)                                \
       Declare2Words(e)                                \
       MultiplyWords(p, A[0], A[0])    \
       R[0] = LowWord(p);                              \
       AssignWord(e, HighWord(p))              \
       MultiplyWords(p, A[0], A[1])    \
       c = LowWord(p);         \
       AssignWord(d, HighWord(p))              \
       Squ_NonDiag                                             \

#define Squ_NonDiag                            \
       Double3Words(c, d)

void pSqu_NonDiag(u32 *c, u64 *d)
{
       Double3Words(*c, *d)
}

#define Squ_SaveAcc(k, i, j)           \
       Acc3WordsBy2(c, d, e)                   \
       R[k] = c;                               \
       MultiplyWords(p, A[i], A[j])    \
       c = LowWord(p);         \
       AssignWord(d, HighWord(p))              \

static void pSqu_SaveAcc(u32 k, u32 i, u32 j, u32 *c, u64 *d, u64 *e,
                        u32 *R, const u32 *A, u64 *p)
{
       Acc3WordsBy2(*c, *d, *e);

       R[k] = *c;
       *p = (u64) A[i] * A[j];
       *c = LowWord(*p);
       *d = HighWord(*p);
}
static void pSqu_Acc(u32 i, u32 j, u32 *c, u64 *d, const u32 *A,
                    u64 *p)
{
       pMulAcc(c, d, A[i], A[j], p);
}
void pSqu_Diag(u32 i, u32 *c, u64 *d, const u32 *A, u64 *p)
{
       pSqu_NonDiag(c, d);
       pMulAcc(c, d, A[i], A[i], p);
}

#define Squ_End(n)                                     \
       Acc3WordsBy2(c, d, e)                   \
       R[2*n-3] = c;                   \
       MultiplyWords(p, A[n-1], A[n-1])\
       Acc2WordsBy2(p, e)                              \
       R[2*n-2] = LowWord(p);                  \
       R[2*n-1] = HighWord(p);

void Baseline_Multiply2(u32 *R, const u32 *A, const u32 *B)
{
       Mul_Begin pMul_SaveAcc(0, 0, 1, &c, &d, R, A, B, &p);

       pMul_Acc(&c, &d, A, B, 1, 0, &p);

       Mul_End(1, 1)
}

void Baseline_Multiply4(u32 *R, const u32 *A, const u32 *B)
{
       u32 i, j, k;

       Mul_Begin for (i = 0; i < 3; i++) {
               pMul_SaveAcc(i, 0, i + 1, &c, &d, R, A, B, &p);

               for (j = 0; j <= i; j++)
                       pMul_Acc(&c, &d, A, B, 1 + j, i - j, &p);
       }

       for (k = 1; k < 3; k++, i++) {
               pMul_SaveAcc(i, k, 3, &c, &d, R, A, B, &p);

               for (j = 0; (j + k) < 3; j++)
                       pMul_Acc(&c, &d, A, B, k + 1 + j, 6 - j, &p);
       }

       Mul_End(5, 3)
}

void Baseline_Multiply8(u32 *R, const u32 *A, const u32 *B)
{
       u32 i, j, k;

       Mul_Begin for (i = 0; i < 7; i++) {
               pMul_SaveAcc(i, 0, i + 1, &c, &d, R, A, B, &p);

               for (j = 0; j <= i; j++)
                       pMul_Acc(&c, &d, A, B, 1 + j, i - j, &p);
       }

       for (k = 1; k < 7; k++, i++) {
               pMul_SaveAcc(i, k, 7, &c, &d, R, A, B, &p);

               for (j = 0; (j + k) < 7; j++)
                       pMul_Acc(&c, &d, A, B, k + 1 + j, 6 - j, &p);
       }

       Mul_End(13, 7)
}

void Baseline_Square2(u32 *R, const u32 *A)
{
       Squ_Begin(2)
           Squ_End(2)
}

void Baseline_Square4(u32 *R, const u32 *A)
{
       u32 i, j, k;

       Squ_Begin(4)

           for (i = 1; i < 3; i++) {
               pSqu_SaveAcc(i, 0, i + 1, &c, &d, &e, R, A, &p);

               for (j = 0; j < i / 2; j++)
                       pSqu_Acc(1 + j, i - j, &c, &d, A, &p);
               if (i % 2 != 0)
                       pSqu_Diag(i / 2 + 1, &c, &d, A, &p);
               else
                       pSqu_NonDiag(&c, &d);
       }
       for (k = 1; k < 3; k++, i++) {
               pSqu_SaveAcc(i, k, 3, &c, &d, &e, R, A, &p);

               if (i % 2 != 0)
                       pSqu_Diag(i / 2 + 1, &c, &d, A, &p);
               else
                       pSqu_NonDiag(&c, &d);
       }

       Squ_End(4)
}

void Baseline_Square8(u32 *R, const u32 *A)
{
       u32 i, j, k;

       Squ_Begin(8)

           for (i = 1; i < 7; i++) {
               pSqu_SaveAcc(i, 0, i + 1, &c, &d, &e, R, A, &p);

               for (j = 0; j < i / 2; j++)
                       pSqu_Acc(1 + j, i - j, &c, &d, A, &p);
               if (i % 2 != 0)
                       pSqu_Diag(i / 2 + 1, &c, &d, A, &p);
               else
                       pSqu_NonDiag(&c, &d);
       }

       for (k = 1; k < 7; k++, i++) {
               pSqu_SaveAcc(i, k, 7, &c, &d, &e, R, A, &p);

               for (j = 0; (j + k) < i / 2; j++)
                       pSqu_Acc(k + 1 + j, 6 - j, &c, &d, A, &p);
               if (i % 2 != 0)
                       pSqu_Diag(i / 2 + 1, &c, &d, A, &p);
               else
                       pSqu_NonDiag(&c, &d);
       }

       Squ_End(8)

}

void Baseline_MultiplyBottom2(u32 *R, const u32 *A, const u32 *B)
{
       Mul_Begin Bot_SaveAcc(0, 0, 1) Bot_Acc(1, 0)
           Bot_End(2)
}

void Baseline_MultiplyBottom4(u32 *R, const u32 *A, const u32 *B)
{
       Mul_Begin pMul_SaveAcc(0, 0, 1, &c, &d, R, A, B, &p);
       pMul_Acc(&c, &d, A, B, 1, 0, &p);

       pMul_SaveAcc(1, 2, 0, &c, &d, R, A, B, &p);
       pMul_Acc(&c, &d, A, B, 1, 1, &p);
       pMul_Acc(&c, &d, A, B, 0, 2, &p);

       Bot_SaveAcc(2, 0, 3)
           Bot_Acc(1, 2) Bot_Acc(2, 1) Bot_Acc(3, 0)
           Bot_End(4)
}

void Baseline_MultiplyBottom8(u32 *R, const u32 *A, const u32 *B)
{
       u32 i, j;
       Mul_Begin for (i = 0; i < 7; i++) {
               pMul_SaveAcc(i, 0, i + 1, &c, &d, R, A, B, &p);

               for (j = 0; j <= i; j++)
                       pMul_Acc(&c, &d, A, B, 1 + j, i - j, &p);
       }

       Bot_End(8)
}

#define Top_Begin(n)                           \
       Declare2Words(p)                                \
       u32 c;  \
       Declare2Words(d)                                \
       MultiplyWords(p, A[0], B[n-2]);\
       AssignWord(d, HighWord(p));

void pTop_Acc(u32 i, u32 j, u64 *d, const u32 *A, const u32 *B,
             u64 *p)
{
       *p = (u64) A[i] * B[j];
       *d = *d + HighWord(*p);
}

#define Top_SaveAcc0(i, j)             \
       c = LowWord(d); \
       AssignWord(d, HighWord(d))      \
       MulAcc(c, d, A[i], B[j])

#define Top_SaveAcc1(i, j)             \
       c = L < c; \
       Acc2WordsBy1(d, c);     \
       c = LowWord(d); \
       AssignWord(d, HighWord(d))      \
       MulAcc(c, d, A[i], B[j])

void Baseline_MultiplyTop2(u32 *R, const u32 *A, const u32 *B, u32 L)
{
       u32 T[4];
       Baseline_Multiply2(T, A, B);
       R[0] = T[2];
       R[1] = T[3];
}

void Baseline_MultiplyTop4(u32 *R, const u32 *A, const u32 *B, u32 L)
{
       u32 i;

       Top_Begin(4)

           for (i = 1; i < 3; i++) {
               pTop_Acc(i, 2 - i, &d, A, B, &p);

       }

       Top_SaveAcc0(0, 3)

           for (i = 1; i < 4; i++)
               pMul_Acc(&c, &d, A, B, i, 3 - i, &p);

       Top_SaveAcc1(1, 3)

           for (i = 2; i < 4; i++)
               pMul_Acc(&c, &d, A, B, i, 4 - i, &p);
       pMul_SaveAcc(0, 2, 3, &c, &d, R, A, B, &p);

       pMul_Acc(&c, &d, A, B, 3, 2, &p);

       Mul_End(1, 3)

}
void Baseline_MultiplyTop8(u32 *R, const u32 *A, const u32 *B, u32 L)
{
       u32 i, j, k;

       Top_Begin(8)

           for (i = 1; i < 7; i++)
               pTop_Acc(i, 6 - i, &d, A, B, &p);

       Top_SaveAcc0(0, 7)

           for (i = 1; i < 8; i++)
               pMul_Acc(&c, &d, A, B, i, 7 - i, &p);

       Top_SaveAcc1(1, 7)

           for (i = 2; i < 8; i++)
               pMul_Acc(&c, &d, A, B, i, 8 - i, &p);

       for (k = 0; k < 5; k++) {
               pMul_SaveAcc(k, k + 2, 7, &c, &d, R, A, B, &p);

               for (j = 0; j + k < 5; j++)
                       pMul_Acc(&c, &d, A, B, j + k + 3, 6 - j, &p);
       }

       Mul_End(5, 7)

}
void Baseline_Multiply16(u32 *R, const u32 *A, const u32 *B)
{
       u32 i, j, k;

       Mul_Begin for (i = 0; i < 15; i++) {
               pMul_SaveAcc(i, 0, i + 1, &c, &d, R, A, B, &p);

               for (j = 0; j <= i; j++)
                       pMul_Acc(&c, &d, A, B, 1 + j, i - j, &p);
       }

       for (k = 1; k < 15; k++, i++) {
               pMul_SaveAcc(i, k, 15, &c, &d, R, A, B, &p);

               for (j = 0; (j + k) < 15; j++)
                       pMul_Acc(&c, &d, A, B, k + 1 + j, 14 - j, &p);
       }
       Mul_End(29, 15)

}

void Baseline_Square16(u32 *R, const u32 *A)
{
       u32 i, j, k;

       Squ_Begin(16)

           for (i = 1; i < 15; i++) {
               pSqu_SaveAcc(i, 0, i + 1, &c, &d, &e, R, A, &p);

               for (j = 0; j < i / 2; j++)
                       pSqu_Acc(1 + j, i - j, &c, &d, A, &p);
               if (i % 2 != 0)
                       pSqu_Diag(i / 2 + 1, &c, &d, A, &p);
               else
                       pSqu_NonDiag(&c, &d);
       }

       for (k = 1; k < 15; k++, i++) {
               pSqu_SaveAcc(i, k, 15, &c, &d, &e, R, A, &p);

               for (j = 0; (j + k) < i / 2; j++)
                       pSqu_Acc(k + 1 + j, 14 - j, &c, &d, A, &p);
               if (i % 2 != 0)
                       pSqu_Diag(i / 2 + 1, &c, &d, A, &p);
               else
                       pSqu_NonDiag(&c, &d);
       }

       Squ_End(16)
}

void Baseline_MultiplyBottom16(u32 *R, const u32 *A, const u32 *B)
{
       u32 i, j;

       Mul_Begin for (i = 0; i < 15; i++) {
               pMul_SaveAcc(i, 0, i + 1, &c, &d, R, A, B, &p);

               for (j = 0; j <= i; j++)
                       pMul_Acc(&c, &d, A, B, 1 + j, i - j, &p);
       }

       Bot_End(16)
}

void Baseline_MultiplyTop16(u32 *R, const u32 *A, const u32 *B, u32 L)
{
       u32 i, j, k;

       Top_Begin(16)

           for (i = 1; i < 15; i++)
               pTop_Acc(i, 14 - i, &d, A, B, &p);

       Top_SaveAcc0(0, 15)

           for (i = 1; i < 16; i++)
               pMul_Acc(&c, &d, A, B, i, 15 - i, &p);

       Top_SaveAcc1(1, 15)

           for (i = 2; i < 16; i++)
               pMul_Acc(&c, &d, A, B, i, 16 - i, &p);

       for (k = 0; k < 13; k++) {
               pMul_SaveAcc(k, k + 2, 15, &c, &d, R, A, B, &p);

               for (j = 0; j + k < 13; j++)
                       pMul_Acc(&c, &d, A, B, j + k + 3, 14 - j, &p);
       }

       Mul_End(13, 15)
}

typedef void (*PMul) (u32 *C, const u32 *A, const u32 *B);
typedef void (*PSqu) (u32 *C, const u32 *A);
typedef void (*PMulTop) (u32 *C, const u32 *A, const u32 *B, u32 L);

static const size_t s_recursionLimit = 16;

PMul s_pMul[5] = { Baseline_Multiply2, Baseline_Multiply4,
       Baseline_Multiply8, 0,
Baseline_Multiply16 };
PMul s_pBot[5] =
    { Baseline_MultiplyBottom2, Baseline_MultiplyBottom4,
Baseline_MultiplyBottom8, 0, Baseline_MultiplyBottom16 };
PSqu s_pSqu[5] =
    { Baseline_Square2, Baseline_Square4, Baseline_Square8, 0,
Baseline_Square16 };
PMulTop s_pTop[5] =
    { Baseline_MultiplyTop2, Baseline_MultiplyTop4, Baseline_MultiplyTop8, 0,
Baseline_MultiplyTop16 };

static int Add(u32 *C, const u32 *A, const u32 *B, size_t N)
{
       return Baseline_Add(N, C, A, B);
}

static int Subtract(u32 *C, const u32 *A, const u32 *B, size_t N)
{
       return Baseline_Sub(N, C, A, B);
}

#define A0             A
#define A1             (A+N2)
#define B0             B
#define B1             (B+N2)

#define T0             T
#define T1             (T+N2)
#define T2             (T+N)
#define T3             (T+N+N2)

#define R0             R
#define R1             (R+N2)
#define R2             (R+N)
#define R3             (R+N+N2)

/* R[2*N] - result = A*B */
/* T[2*N] - temporary work space */
/* A[N] --- multiplier */
/* B[N] --- multiplicant */
static void RecursiveMultiply(u32 *R, u32 *T, const u32 *A,
                             const u32 *B, size_t N)
{
       const size_t N2 = N / 2;
       size_t AN2, BN2;
       int c2, c3;

       if (N <= s_recursionLimit)
               s_pMul[N / 4] (R, A, B);
       else {

               AN2 = Compare(A0, A1, N2) > 0 ? 0 : N2;
               Subtract(R0, A + AN2, A + (N2 ^ AN2), N2);

               BN2 = Compare(B0, B1, N2) > 0 ? 0 : N2;
               Subtract(R1, B + BN2, B + (N2 ^ BN2), N2);

               RecursiveMultiply(R2, T2, A1, B1, N2);
               RecursiveMultiply(T0, T2, R0, R1, N2);
               RecursiveMultiply(R0, T2, A0, B0, N2);

               /*
                * now T[01] holds (A1-A0)*(B0-B1), R[01] holds A0*B0 and
                * R[23] holds A1*B1
                */
               c2 = Add(R2, R2, R1, N2);
               c3 = c2;
               c2 += Add(R1, R2, R0, N2);
               c3 += Add(R2, R2, R3, N2);

               if (AN2 == BN2)
                       c3 -= Subtract(R1, R1, T0, N);
               else
                       c3 += Add(R1, R1, T0, N);

               c3 += Increment_3(R2, N2, c2);
               Increment_3(R3, N2, c3);
       }
}

/* R[2*N] - result = A*A */
/* T[2*N] - temporary work space */
/* A[N] --- number to be squared */
static void RecursiveSquare(u32 *R, u32 *T, const u32 *A, size_t N)
{
       int carry;

       if (N <= s_recursionLimit)
               s_pSqu[N / 4] (R, A);
       else {
               const size_t N2 = N / 2;

               RecursiveSquare(R0, T2, A0, N2);
               RecursiveSquare(R2, T2, A1, N2);
               RecursiveMultiply(T0, T2, A0, A1, N2);

               carry = Add(R1, R1, T0, N);
               carry += Add(R1, R1, T0, N);
               Increment_3(R3, N2, carry);
       }
}

/* R[N] - bottom half of A*B */
/* T[3*N/2] - temporary work space */
/* A[N] - multiplier */
/* B[N] - multiplicant */
static void RecursiveMultiplyBottom(u32 *R, u32 *T, const u32 *A,
                                   const u32 *B, size_t N)
{
       const size_t N2 = N / 2;

       if (N <= s_recursionLimit)
               s_pBot[N / 4] (R, A, B);
       else {
               RecursiveMultiply(R, T, A0, B0, N2);
               RecursiveMultiplyBottom(T0, T1, A1, B0, N2);
               Add(R1, R1, T0, N2);
               RecursiveMultiplyBottom(T0, T1, A0, B1, N2);
               Add(R1, R1, T0, N2);
       }
}

/* R[N] --- upper half of A*B */
/* T[2*N] - temporary work space */
/* L[N] --- lower half of A*B */
/* A[N] --- multiplier */
/* B[N] --- multiplicant */
static void MultiplyTop(u32 *R, u32 *T, const u32 *L, const u32 *A,
                       const u32 *B, size_t N)
{
       const size_t N2 = N / 2;

       size_t AN2, BN2;

       int t, c2, c3;

       if (N <= s_recursionLimit)
               s_pTop[N / 4] (R, A, B, L[N - 1]);
       else {
               AN2 = Compare(A0, A1, N2) > 0 ? 0 : N2;
               Subtract(R0, A + AN2, A + (N2 ^ AN2), N2);

               BN2 = Compare(B0, B1, N2) > 0 ? 0 : N2;
               Subtract(R1, B + BN2, B + (N2 ^ BN2), N2);

               RecursiveMultiply(T0, T2, R0, R1, N2);
               RecursiveMultiply(R0, T2, A1, B1, N2);

               /* now T[01] holds (A1-A0)*(B0-B1) = A1*B0+A0*B1-A1*B1-A0*B0, R[01] holds A1*B1 */
               c2 = Subtract(T2, L + N2, L, N2);

               if (AN2 == BN2) {
                       c2 -= Add(T2, T2, T0, N2);
                       t = (Compare(T2, R0, N2) == -1);
                       c3 = t - Subtract(T2, T2, T1, N2);
               } else {
                       c2 += Subtract(T2, T2, T0, N2);
                       t = (Compare(T2, R0, N2) == -1);
                       c3 = t + Add(T2, T2, T1, N2);
               }

               c2 += t;
               if (c2 >= 0)
                       c3 += Increment_3(T2, N2, c2);
               else
                       c3 -= Decrement_3(T2, N2, -c2);
               c3 += Add(R0, T2, R1, N2);

               Increment_3(R1, N2, c3);
       }
}

static void Multiply(u32 *R, u32 *T, const u32 *A, const u32 *B,
                    size_t N)
{
       RecursiveMultiply(R, T, A, B, N);
}

static void Square(u32 *R, u32 *T, const u32 *A, size_t N)
{
       RecursiveSquare(R, T, A, N);
}

static void MultiplyBottom(u32 *R, u32 *T, const u32 *A, const u32 *B,
                          size_t N)
{
       RecursiveMultiplyBottom(R, T, A, B, N);
}

/* R[NA+NB] - result = A*B */
/* T[NA+NB] - temporary work space */
/* A[NA] ---- multiplier */
/* B[NB] ---- multiplicant */
static void AsymmetricMultiply(u32 *R, u32 *T, u32 *A, size_t NA,
                              u32 *B, size_t NB)
{
       size_t i;

       if (NA == NB) {
               if (A == B)
                       Square(R, T, A, NA);
               else
                       Multiply(R, T, A, B, NA);

               return;
       }

       if (NA > NB)
               swap(A, B);

       if (NA == 2 && !A[1]) {
               switch (A[0]) {
               case 0:
                       SetWords(R, NB + 2);
                       return;
               case 1:
                       CopyWords(R, B, NB);
                       R[NB] = R[NB + 1] = 0;
                       return;
               default:
                       R[NB] = LinearMultiply(R, B, A[0], NB);
                       R[NB + 1] = 0;
                       return;
               }
       }

       if ((NB / NA) % 2 == 0) {
               Multiply(R, T, A, B, NA);
               CopyWords(T + 2 * NA, R + NA, NA);
               for (i = 2 * NA; i < NB; i += 2 * NA)
                       Multiply(T + NA + i, T, A, B + i, NA);
               for (i = NA; i < NB; i += 2 * NA)
                       Multiply(R + i, T, A, B + i, NA);
       } else {
               for (i = 0; i < NB; i += 2 * NA)
                       Multiply(R + i, T, A, B + i, NA);
               for (i = NA; i < NB; i += 2 * NA)
                       Multiply(T + NA + i, T, A, B + i, NA);
       }

       if (Add(R + NA, R + NA, T + 2 * NA, NB - NA))
               Increment_3(R + NB, NA, 1);
}

/* R[N] ----- result = A inverse mod 2**(WORD_BITS*N) */
/* T[3*N/2] - temporary work space */
/* A[N] ----- an odd number as input */
static void RecursiveInverseModPower2(u32 *R, u32 *T, const u32 *A,
                                     size_t N)
{
       const size_t N2 = N / 2;

       if (N == 2) {
               T[0] = AtomicInverseModPower2(A[0]);
               T[1] = 0;
               s_pBot[0] (T + 2, T, A);
               TwosComplement(T + 2, 2);
               Increment_3(T + 2, 2, 2);
               s_pBot[0] (R, T, T + 2);
       } else {
               RecursiveInverseModPower2(R0, T0, A0, N2);
               T0[0] = 1;
               SetWords(T0 + 1, N2 - 1);
               MultiplyTop(R1, T1, T0, R0, A0, N2);
               MultiplyBottom(T0, T1, R0, A1, N2);
               Add(T0, R1, T0, N2);
               TwosComplement(T0, N2);
               MultiplyBottom(R1, T1, R0, T0, N2);
       }
}

/* R[N] --- result = X/(2**(WORD_BITS*N)) mod M */
/* T[3*N] - temporary work space */
/* X[2*N] - number to be reduced */
/* M[N] --- modulus */
/* U[N] --- multiplicative inverse of M mod 2**(WORD_BITS*N) */
static void MontgomeryReduce(u32 *R, u32 *T, u32 *X, const u32 *M,
                            const u32 *U, size_t N)
{
       MultiplyBottom(R, T, X, U, N);
       MultiplyTop(T, T + N, X, R, M, N);
       u32 borrow = Subtract(T, X + N, T, N);
       /* defend against timing attack by doing this */
       /* Add even when not needed */
       Add(T + N, T, M, N);
       CopyWords(R, T + ((0 - borrow) & N), N);
}

static void AtomicDivide(u32 *Q, u32 *A, u32 *B)
{
       u32 T[4];
       DWord q, AL, AH, BH;
       u32 P[4];

       Q[0] = Q[1] = 0;
       DWord_3(&AL, A[0], A[1]);
       DWord_3(&AH, A[2], A[3]);
       DWord_3(&BH, B[0], B[1]);

       DivideFourWordsByTwo(T, &AL, &AH, &BH, &q);
       Q[0] = DWord__GetLowHalf(&q);
       Q[1] = DWord__GetHighHalf(&q);

       if (B[0] || B[1]) {
               /* multiply quotient and divisor and add remainder and */
               /* make sure it equals dividend */
               s_pMul[0] (P, Q, B);
               Add(P, P, T, 4);
       }
}

/* For use by Divide(), corrects the underestimated quotient {Q1,Q0} */
static void CorrectQuotientEstimate(u32 *R, u32 *T, u32 *Q, u32 *B,
                                   size_t N)
{
       AsymmetricMultiply(T, T + N + 2, Q, 2, B, N);

       Subtract(R, R, T, N + 2);

       while (R[N] || Compare(R, B, N) >= 0) {
               R[N] -= Subtract(R, R, B, N);
               Q[1] += (++Q[0] == 0);
       }
}

static void Divide(u32 *R, u32 *Q, u32 *T, const u32 *A, size_t NA,
                  const u32 *B, size_t NB)
{
       /* set up temporary work space */
       int i;
       u32 *const TA = T;
       u32 *const TB = T + NA + 2;
       u32 *const TP = T + NA + 2 + NB;
       unsigned shiftBits;

       /*
        * copy B into TB and normalize it so that
        * TB has highest bit set to 1
        */
       unsigned shiftWords = (B[NB - 1] == 0);

       u32 BT[2];

       TB[0] = TB[NB - 1] = 0;
       CopyWords(TB + shiftWords, B, NB - shiftWords);
       shiftBits = WORD_BITS - BitPrecision(TB[NB - 1]);
       ShiftWordsLeftByBits(TB, NB, shiftBits);

       /* copy A into TA and normalize it */
       TA[0] = TA[NA] = TA[NA + 1] = 0;
       CopyWords(TA + shiftWords, A, NA);
       ShiftWordsLeftByBits(TA, NA + 2, shiftBits);

       if (TA[NA + 1] == 0 && TA[NA] <= 1) {
               Q[NA - NB + 1] = Q[NA - NB] = 0;
               while (TA[NA] || Compare(TA + NA - NB, TB, NB) >= 0) {
                       TA[NA] -= Subtract(TA + NA - NB, TA + NA - NB, TB, NB);
                       ++Q[NA - NB];
               }
       } else {
               NA += 2;
       }

       BT[0] = TB[NB - 2] + 1;
       BT[1] = TB[NB - 1] + (BT[0] == 0);

       /* start reducing TA mod TB, 2 words at a time */
       for (i = NA - 2; i >= NB; i -= 2) {
               AtomicDivide(Q + i - NB, TA + i - 2, BT);
               CorrectQuotientEstimate(TA + i - NB, TP, Q + i - NB, TB, NB);
       }

       /* copy TA into R, and denormalize it */
       CopyWords(R, TA + shiftWords, NB);
       ShiftWordsRightByBits(R, NB, shiftBits);
}

/* return k */
/* R[N] --- result = A^(-1) * 2^k mod M */
/* T[4*N] - temporary work space */
/* A[NA] -- number to take inverse of */
/* M[N] --- modulus */
static size_t RoundupSize(size_t n)
{
       if (n <= 2)
               return 2;
       else if (n <= 4)
               return 4;
       else if (n <= 8)
               return 8;
       else if (n <= 16)
               return 16;
       else if (n <= 32)
               return 32;
       else if (n <= 64)
               return 64;
       else
               return (size_t) (1) << BitPrecision(n - 1);
}

static void Integer__Integer_IN(Integer *d, Integer *s)
{
       CopyWords(d->reg, s->reg, s->reg_size);
       d->reg_size = s->reg_size;
}

static void Integer__Zero(Integer *in)
{
       int i;
       for (i = 0; i < 4 * KEY_SIZE_WORDS; i++)
               in->reg[i] = 0;
}

static void Integer__One(Integer *one)
{
       Integer__Zero(one);
       one->reg[0] = 1;
}

static bool Integer__GetBit(Integer *in, size_t n)
{
       if (n / WORD_BITS >= in->reg_size)
               return 0;
       else
               return (bool) ((in->reg[n / WORD_BITS] >> (n % WORD_BITS)) & 1);
}

static void Integer__SetBit(Integer *in, size_t n)
{
       in->reg_size = RoundupSize(BitsToWords(n + 1));
       in->reg[n / WORD_BITS] |= (1 << (n % WORD_BITS));
}

static void Integer__Integer_V_L(Integer *in, u32 value, size_t length)
{
       int i;
       in->reg_size = length;
       in->sign = POSITIVE;
       in->reg[0] = value;
       for (i = 0; i < length; i++)
               in->reg[i] = 0;
}

static void Integer__Power2(Integer *in, size_t e)
{
       Integer__Integer_V_L(in, 0, RoundupSize(BitsToWords(e + 1)));
       Integer__SetBit(in, e);
}

static unsigned int Integer__WordCount(Integer *in)
{

       return (unsigned int)CountWords(in->reg, in->reg_size);
}

static unsigned int Integer__BitCount(Integer *in)
{
       unsigned wordCount = Integer__WordCount(&(*in));
       if (wordCount) {
               return (wordCount - 1) * WORD_BITS +
                   BitPrecision(in->reg[wordCount - 1]);
       } else
               return 0;
}

void Integer__operatorLLE(Integer *in, size_t n, Integer *out)
{
       size_t wordCount = Integer__WordCount(in);
       size_t shiftWords = n / WORD_BITS;
       unsigned int shiftBits = (unsigned int)(n % WORD_BITS);
       Integer tmp;
       Integer__Integer_IN(&tmp, in);

       tmp.reg_size = RoundupSize(wordCount + BitsToWords(n));
       ShiftWordsLeftByWords(tmp.reg, wordCount + shiftWords, shiftWords);
       ShiftWordsLeftByBits(tmp.reg + shiftWords,
                            wordCount + BitsToWords(shiftBits), shiftBits);
       tmp.reg_size = RoundupSize(wordCount + BitsToWords(n));
       Integer__Integer_IN(out, &tmp);
}

static bool Integer__IsNegative(Integer *in)
{
       return in->sign == NEGATIVE;
}

static bool Integer__NotNegative(Integer *in)
{
       return !Integer__IsNegative(in);
}

static int Integer__PositiveCompare(Integer *in, Integer *t)
{
       unsigned size = Integer__WordCount(in);
       unsigned tSize = Integer__WordCount(t);

       if (size == tSize)
               return Compare(in->reg, t->reg, size);
       else
               return size > tSize ? 1 : -1;
}

static int PositiveDivide(Integer *remainder, Integer *quotient, Integer *a,
                         Integer *b)
{
       int i;

       u32 T[8 * KEY_SIZE_WORDS];

       unsigned aSize = Integer__WordCount(a);
       unsigned bSize = Integer__WordCount(b);

       a->reg_size = 2 * remainder->reg_size;
       b->reg_size = remainder->reg_size;

       if (!bSize)
               return -1;
       if (Integer__PositiveCompare(a, b) == -1) {
               remainder = a;
               remainder->sign = POSITIVE;
               Integer__Zero(quotient);
               return 0;
       }

       aSize += aSize % 2;     /* round up to next even number */
       bSize += bSize % 2;

       remainder->sign = POSITIVE;
       remainder->reg_size = RoundupSize(bSize);
       quotient->sign = POSITIVE;
       quotient->reg_size = RoundupSize(aSize - bSize + 2);
       for (i = quotient->reg_size - 1; i != 0; i--)
               quotient->reg[i] = 0;

       for (i = 8 * KEY_SIZE_WORDS - 1; i != 0; i--)
               T[i] = 0;

       Divide(remainder->reg, quotient->reg, T, a->reg, aSize, b->reg, bSize);
       return 0;
}

static void Integer__Modulo(Integer *a, Integer *b, Integer *remainder)
{
       Integer quotient;
       quotient.reg_size = remainder->reg_size;
       PositiveDivide(remainder, &quotient, a, b);
}

static u32 Integer__Modulo2(Integer *a, u32 divisor)
{
       u32 remainder;
       unsigned int i;
       DWord sum;
       DWord tmp;

       if (!divisor)
               return -1;

       if ((divisor & (divisor - 1)) == 0) {   /* divisor is a power of 2 */
               remainder = a->reg[0] & (divisor - 1);
       } else {
               i = Integer__WordCount(a);

               if (divisor <= 5) {
                       DWord_3(&sum, 0, 0);
                       while (i--)
                               DWord__opPlusEQ(&sum, a->reg[i]);

                       remainder = DWord__operatorMod(&sum, divisor);
               } else {
                       remainder = 0;
                       while (i--) {
                               DWord_3(&tmp, a->reg[i], remainder);
                               remainder = DWord__operatorMod(&tmp, divisor);
                       }
               }
       }

       if (Integer__IsNegative(a) && remainder)
               remainder = divisor - remainder;

       return remainder;
}

static void
MontgomeryRepresentation__MultiplicativeIdentity(MontgomeryRepresentation *mr,
                                                Integer *a)
{
       Integer tmp;
       mr->m_ma.m_result1.reg_size = a->reg_size;

       Integer__Power2(&tmp, WORD_BITS * mr->m_ma.m_modulus.reg_size);

       Integer__Modulo(&tmp, &mr->m_ma.m_modulus, &mr->m_ma.m_result1);

       Integer__Integer_IN(a, &mr->m_ma.m_result1);
}

static void MontgomeryRepresentation__Multiply(MontgomeryRepresentation *mr,
                                       Integer *a, Integer *b)
{
       u32 *T = mr->m_workspace;
       u32 *R = mr->m_ma.m_result.reg;
       size_t N = mr->m_ma.m_modulus.reg_size;

       AsymmetricMultiply(T, T + 2 * N, a->reg, a->reg_size, b->reg,
                          b->reg_size);
       SetWords(T + a->reg_size + b->reg_size,
                2 * N - a->reg_size - b->reg_size);
       MontgomeryReduce(R, T + 2 * N, T, mr->m_ma.m_modulus.reg, mr->m_u.reg,
                        N);
       Integer__Integer_IN(a, &mr->m_ma.m_result);
}

static void MontgomeryRepresentation__Square(MontgomeryRepresentation *mr,
                                       Integer *a)
{
       u32 *T = mr->m_workspace;
       u32 *R = mr->m_ma.m_result.reg;
       size_t N = mr->m_ma.m_modulus.reg_size;

       Square(T, T + 2 * N, a->reg, a->reg_size);
       SetWords(T + 2 * a->reg_size, 2 * N - 2 * a->reg_size);
       MontgomeryReduce(R, T + 2 * N, T, mr->m_ma.m_modulus.reg, mr->m_u.reg,
                        N);
       Integer__Integer_IN(a, &mr->m_ma.m_result);
}

static void MontgomeryRepresentation__ConvertOut(MontgomeryRepresentation *mr,
                                                Integer *in, Integer *out)
{
       u32 *T = mr->m_workspace;
       u32 *R = mr->m_ma.m_result.reg;
       size_t N = mr->m_ma.m_modulus.reg_size;

       CopyWords(T, in->reg, in->reg_size);
       SetWords(T + in->reg_size, 2 * N - in->reg_size);
       MontgomeryReduce(R, T + 2 * N, T, mr->m_ma.m_modulus.reg, mr->m_u.reg,
                        N);
       Integer__Integer_IN(out, &mr->m_ma.m_result);
}

static bool Integer__IsOdd(Integer *in)
{
       return Integer__GetBit(in, 0) == 1;
}

static void ModularArithmetic__ModularArithmetic(ModularArithmetic *mr,
                                                Integer *n)
{
       int i;
       for (i = 0; i < mr->m_modulus.reg_size; i++) {
               mr->m_modulus.reg[i] = n->reg[i];
               mr->m_result.reg[i] = 0;
               mr->m_result1.reg[i] = 0;
       }
}

static int
MontgomeryRepresentation__MontgomeryRepresentation(MontgomeryRepresentation *
                                               mr, Integer *m)
{                              /* modulus must be odd */
       ModularArithmetic__ModularArithmetic(&mr->m_ma, m);
       Integer__Integer_V_L(&mr->m_u, 0, m->reg_size);

       if (!Integer__IsOdd(&mr->m_ma.m_modulus))
               return -1;

       RecursiveInverseModPower2(mr->m_u.reg, mr->m_workspace,
                                 mr->m_ma.m_modulus.reg,
                                 mr->m_ma.m_modulus.reg_size);
       return 0;
}

static void MontgomeryRepresentation__ConvertIn(MontgomeryRepresentation *mr,
                                               Integer *a, Integer *result)
{
       Integer tmp;

       size_t r_len = mr->m_ma.m_modulus.reg_size;
       result->reg_size = mr->m_ma.m_modulus.reg_size;
       Integer__operatorLLE(a, WORD_BITS * r_len, &tmp);
       Integer__Modulo(&tmp, &mr->m_ma.m_modulus, result);
}

void Integer__Encode(Integer *in, char *bt, size_t outputLen, int signedness)
{
       int i;
       u8 b;

       if (signedness == UNSIGNED || Integer__NotNegative(in)) {
               for (i = outputLen; i > 0; i--) {
                       b = (in->reg[(i - 1) / (WORD_SIZE)]) >> ((i - 1) %
                               (WORD_SIZE)) * 8;

                       bt[outputLen - i] = b;
               }
       }
}

static void PositiveAdd(Integer *sum, Integer *a, Integer *b)
{
       int carry;

       if (a->reg_size == b->reg_size)
               carry = Add(sum->reg, a->reg, b->reg, a->reg_size);
       else if (a->reg_size > b->reg_size) {
               carry = Add(sum->reg, a->reg, b->reg, b->reg_size);

               CopyWords(sum->reg + b->reg_size, a->reg + b->reg_size,
                         a->reg_size - b->reg_size);

               carry = Increment_3(sum->reg + b->reg_size,
                                   a->reg_size - b->reg_size, carry);
       } else {
               carry = Add(sum->reg, a->reg, b->reg, a->reg_size);

               CopyWords(sum->reg + a->reg_size, b->reg + a->reg_size,
                         b->reg_size - a->reg_size);

               carry = Increment_3(sum->reg + a->reg_size,
                                   b->reg_size - a->reg_size, carry);
       }

       if (carry)
               sum->reg[sum->reg_size / 2] = 1;
       sum->sign = POSITIVE;
}

static void PositiveSubtract(Integer *diff, Integer *a, Integer *b)
{
       unsigned aSize = Integer__WordCount(a);

       unsigned bSize = Integer__WordCount(b);

       u32 borrow;

       aSize += aSize % 2;
       bSize += bSize % 2;

       if (aSize == bSize) {
               if (Compare(a->reg, b->reg, aSize) >= 0) {
                       Subtract(diff->reg, a->reg, b->reg, aSize);
                       diff->sign = POSITIVE;
               } else {
                       Subtract(diff->reg, b->reg, a->reg, aSize);
                       diff->sign = NEGATIVE;
               }
       } else if (aSize > bSize) {
               borrow = Subtract(diff->reg, a->reg, b->reg, bSize);
               CopyWords(diff->reg + bSize, a->reg + bSize, aSize - bSize);
               borrow = Decrement_3(diff->reg + bSize, aSize - bSize, borrow);
               diff->sign = POSITIVE;
       } else {
               borrow = Subtract(diff->reg, b->reg, a->reg, aSize);
               CopyWords(diff->reg + aSize, b->reg + aSize, bSize - aSize);
               borrow = Decrement_3(diff->reg + aSize, bSize - aSize, borrow);
               diff->sign = NEGATIVE;
       }
}

static void Integer__operatorGGE(Integer *in, size_t n, Integer *out)
{
       size_t wordCount = Integer__WordCount(in);
       size_t shiftWords = n / WORD_BITS;
       unsigned int shiftBits = (unsigned int)(n % WORD_BITS);
       Integer tmp;

       Integer__Integer_IN(&tmp, in);

       ShiftWordsRightByWords(tmp.reg, wordCount, shiftWords);

       if (wordCount > shiftWords)
               ShiftWordsRightByBits(tmp.reg, wordCount - shiftWords,
                                     shiftBits);
       if (Integer__IsNegative(in) &&
               Integer__WordCount(in) == 0)    /* avoid -0 */
               Integer__Zero(in);

       Integer__Integer_IN(out, &tmp);
}

static void Integer__operatorPE(Integer *op1, Integer *op2, Integer *out)
{
       if (Integer__NotNegative(op1)) {
               if (Integer__NotNegative(op2))
                       PositiveAdd(op1, op1, op2);
               else
                       PositiveSubtract(op1, op1, op2);
       } else {
               if (Integer__NotNegative(op2))
                       PositiveSubtract(op1, op2, op1);
               else {
                       PositiveAdd(op1, op1, op2);
                       op1->sign = NEGATIVE;
               }
       }
       out = op1;
}

static void WindowSlider__WindowSlider(WindowSlider *ws, Integer *expIn,
                                      bool fastNegate,
                                      unsigned int windowSizeIn /*=0*/,
                                      Integer *tmp)
{
       Integer one;

       one.reg_size = tmp->reg_size;
       ws->exp.reg_size = tmp->reg_size;
       ws->windowModulus.reg_size = tmp->reg_size;

       Integer__One(&one);
       Integer__Integer_IN(&ws->exp, expIn);
       Integer__Integer_IN(&ws->windowModulus, &one);

       ws->windowSize = windowSizeIn;
       ws->windowBegin = 0;
       ws->fastNegate = fastNegate;
       ws->firstTime = true;
       ws->finished = false;

       if (ws->windowSize == 0) {
               unsigned int expLen = Integer__BitCount(&ws->exp);
               ws->windowSize =
                   expLen <= 17 ? 1 : (expLen <=
                                       24 ? 2 : (expLen <=
                                                 70 ? 3 : (expLen <=
                                                           197 ? 4 : (expLen <=
                                                                      539 ? 5
                                                                      : (expLen
                                                                         <=
                                                                         1434 ?
                                                                         6 :
                                                                         7)))));
       }

       Integer__operatorLLE(&ws->windowModulus, ws->windowSize, tmp);
       Integer__Integer_IN(&ws->windowModulus, tmp);

       ws->pos = 0;
       ws->expWindow = 0;
}

static void FindNextWindow(WindowSlider *ws)
{
       unsigned int expLen = Integer__WordCount(&ws->exp) * WORD_BITS;
       unsigned int skipCount = ws->firstTime ? 0 : ws->windowSize;
       Integer tmp;
       Integer tmp3;

       ws->firstTime = false;

       while (!Integer__GetBit(&ws->exp, skipCount)) {
               if (skipCount >= expLen) {
                       ws->finished = true;
                       return;
               }
               skipCount++;
       }

       Integer__operatorGGE(&ws->exp, skipCount, &tmp);
       Integer__Integer_IN(&ws->exp, &tmp);

       ws->windowBegin += skipCount;

       ws->expWindow = Integer__Modulo2(&ws->exp, (1 << ws->windowSize));

       if (ws->fastNegate && Integer__GetBit(&ws->exp, ws->windowSize)) {
               ws->negateNext = true;
               ws->expWindow =
                   ((u32) (1) << ws->windowSize) - ws->expWindow;

               Integer__operatorPE(&ws->exp, &ws->windowModulus, &tmp3);
       } else {
               ws->negateNext = false;
       }
}

static void Integer__SimultaneousMultiply(MontgomeryRepresentation *mr,
                                         Integer *results, Integer *base,
                                         Integer *expBegin)
{
       Integer buckets;
       WindowSlider exponents;

       Integer tmp;
       Integer iden;

       unsigned int expBitPosition = 0;
       Integer *g = base;
       bool notDone = true;

       tmp.reg_size = mr->m_ma.m_modulus.reg_size;
       WindowSlider__WindowSlider(&exponents, expBegin, false, 0, &tmp);
       FindNextWindow(&exponents);

       iden.reg_size = mr->m_ma.m_modulus.reg_size;
       MontgomeryRepresentation__MultiplicativeIdentity(mr, &iden);
       Integer__Integer_IN(&buckets, &iden);

       while (notDone) {
               notDone = false;
               if (!exponents.finished
                   && expBitPosition == exponents.windowBegin) {
                       MontgomeryRepresentation__Multiply(mr, &buckets, g);
                       FindNextWindow(&exponents);
               }
               notDone = notDone || !exponents.finished;

               if (notDone) {
                       MontgomeryRepresentation__Square(mr, g);
                       expBitPosition++;
               }
       }

       Integer__Integer_IN(results, &buckets);
}

static void ModularArithmetic__SimultaneousExponentiate(ModularArithmetic *ma,
                                                       Integer *results,
                                                       Integer *base,
                                                       Integer *exponents,
                                                       unsigned int
                                                       exponentsCount)
{
       Integer *result = results;
       MontgomeryRepresentation dr;

       Integer convertIn;
       Integer convertOut;

       dr.m_ma.m_result.reg_size = result->reg_size;
       dr.m_ma.m_modulus.reg_size = result->reg_size;

       if (Integer__IsOdd(&ma->m_modulus)) {
               MontgomeryRepresentation__MontgomeryRepresentation(&dr,
                                                                  &ma->
                                                                  m_modulus);

               MontgomeryRepresentation__ConvertIn(&dr, base, &convertIn);

               Integer__SimultaneousMultiply(&dr, results, &convertIn,
                                             exponents);

               MontgomeryRepresentation__ConvertOut(&dr, results, &convertOut);

               Integer__Integer_IN(results, &convertOut);
       }
}

static void ModularArithmetic__Exponentiate(ModularArithmetic *ma,
                                           Integer *base, Integer *exponent,
                                           Integer *result)
{
       ModularArithmetic__SimultaneousExponentiate(ma, result, base, exponent,
                                                   1);
}

void a_exp_b_mod_c(Integer *x, Integer *e, Integer *n, Integer *r)
{
       ModularArithmetic mr;

       mr.m_modulus.reg_size = n->reg_size;

       ModularArithmetic__ModularArithmetic(&mr, n);

       ModularArithmetic__Exponentiate(&mr, x, e, r);
}
