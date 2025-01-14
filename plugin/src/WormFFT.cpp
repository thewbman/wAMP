/*      fix_fft.c - Fixed-point Fast Fourier Transform  */
/*
        fix_fft()       perform FFT or inverse FFT
        window()        applies a Hanning window to the (time) input
        fix_loud()      calculates the loudness of the signal, for
                        each freq point. Result is an integer array,
                        units are dB (values will be negative).
        iscale()        scale an integer value by (numer/denom).
        fix_mpy()       perform fixed-point multiplication.
        Sinewave[1024]  sinewave normalized to 32767 (= 1.0).
        Loudampl[100]   Amplitudes for lopudnesses from 0 to -99 dB.
        Low_pass        Low-pass filter, cutoff at sample_freq / 4.


        All data are fixed-point short integers, in which
        -32768 to +32768 represent -1.0 to +1.0. Integer arithmetic
        is used for speed, instead of the more natural floating-point.

        For the forward FFT (time -> freq), fixed scaling is
        performed to prevent arithmetic overflow, and to map a 0dB
        sine/cosine wave (i.e. amplitude = 32767) to two -6dB freq
        coefficients; the one in the lower half is reported as 0dB
        by fix_loud(). The return value is always 0.

        For the inverse FFT (freq -> time), fixed scaling cannot be
        done, as two 0dB coefficients would sum to a peak amplitude of
        64K, overflowing the 32k range of the fixed-point integers.
        Thus, the fix_fft() routine performs variable scaling, and
        returns a value which is the number of bits LEFT by which
        the output must be shifted to get the actual amplitude
        (i.e. if fix_fft() returns 3, each value of fr[] and fi[]
        must be multiplied by 8 (2**3) for proper scaling.
        Clearly, this cannot be done within the fixed-point short
        integers. In practice, if the result is to be used as a
        filter, the scale_shift can usually be ignored, as the
        result will be approximately correctly normalized as is.


        TURBO C, any memory model; uses inline assembly for speed
        and for carefully-scaled arithmetic.

        Written by:  Tom Roberts  11/8/89
        Made portable:  Malcolm Slaney 12/15/94 malcolm@interval.com

                Timing on a Macintosh PowerBook 180.... (using Symantec C6.0)
                        fix_fft (1024 points)             8 ticks
                        fft (1024 points - Using SANE)  112 Ticks
                        fft (1024 points - Using FPU)    11

*/

#include "WormFFT.h"
#include "WormMacro.h"
#include <math.h>

/* FIX_MPY() - fixed-point multiplication macro.
   This macro is a statement, not an expression (uses asm).
   BEWARE: make sure _DX is not clobbered by evaluating (A) or DEST.
   args are all of type fixed.
   Scaling ensures that 32767*32767 = 32767. */
#define dosFIX_MPY(DEST,A,B)       {       \
        _DX = (B);                      \
        _AX = (A);                      \
        asm imul dx;                    \
        asm add ax,ax;                  \
        asm adc dx,dx;                  \
        DEST = _DX;             }

#define FIX_MPY(DEST,A,B)       DEST = ((long)(A) * (long)(B))>>15

#define N_WAVE          1024    /* dimension of Sinewave[] */
#define LOG2_N_WAVE     10      /* log2(N_WAVE) */
#define N_LOUD          100     /* dimension of Loudampl[] */


extern fixed Sinewave[N_WAVE]; /* placed at end of this file for clarity */
extern fixed Loudampl[N_LOUD];
int db_from_ampl(fixed re, fixed im);
fixed fix_mpy(fixed a, fixed b);

/*
        fix_fft() - perform fast Fourier transform.

        if n>0 FFT is done, if n<0 inverse FFT is done
        fr[n],fi[n] are real,imaginary arrays, INPUT AND RESULT.
        size of data = 2**m
        set inverse to 0=dft, 1=idft
*/
int fix_fft(fixed fr[], fixed fi[], int m, int inverse)
{
        int mr,nn,i,j,l,k,istep, n, scale, shift;
        fixed qr,qi,tr,ti,wr,wi,t;

                n = 1<<m;

        if(n > N_WAVE)
                return -1;

        mr = 0;
        nn = n - 1;
        scale = 0;

        /* decimation in time - re-order data */
        for(m=1; m<=nn; ++m) {
                l = n;
                do {
                        l >>= 1;
                } while(mr+l > nn);
                mr = (mr & (l-1)) + l;

                if(mr <= m) continue;
                tr = fr[m];
                fr[m] = fr[mr];
                fr[mr] = tr;
                ti = fi[m];
                fi[m] = fi[mr];
                fi[mr] = ti;
        }

        l = 1;
        k = LOG2_N_WAVE-1;
        while(l < n) {
                if(inverse) {
                        /* variable scaling, depending upon data */
                        shift = 0;
                        for(i=0; i<n; ++i) {
                                j = fr[i];
                                if(j < 0)
                                        j = -j;
                                m = fi[i];
                                if(m < 0)
                                        m = -m;
                                if(j > 16383 || m > 16383) {
                                        shift = 1;
                                        break;
                                }
                        }
                        if(shift)
                                ++scale;
                } else {
                        /* fixed scaling, for proper normalization -
                           there will be log2(n) passes, so this
                           results in an overall factor of 1/n,
                           distributed to maximize arithmetic accuracy. */
                        shift = 1;
                }
                /* it may not be obvious, but the shift will be performed
                   on each data point exactly once, during this pass. */
                istep = l << 1;
                for(m=0; m<l; ++m) {
                        j = m << k;
                        /* 0 <= j < N_WAVE/2 */
                        wr =  Sinewave[j+N_WAVE/4];
                        wi = -Sinewave[j];
                        if(inverse)
                                wi = -wi;
                        if(shift) {
                                wr >>= 1;
                                wi >>= 1;
                        }
                        for(i=m; i<n; i+=istep) {
                                j = i + l;
                                        tr = fix_mpy(wr,fr[j]) -
                                        		fix_mpy(wi,fi[j]);
                                        ti = fix_mpy(wr,fi[j]) +
                                        		fix_mpy(wi,fr[j]);
                                qr = fr[i];
                                qi = fi[i];
                                if(shift) {
                                        qr >>= 1;
                                        qi >>= 1;
                                }
                                fr[j] = qr - tr;
                                fi[j] = qi - ti;
                                fr[i] = qr + tr;
                                fi[i] = qi + ti;
                        }
                }
                --k;
                l = istep;
        }

        return scale;
}


/*      window() - apply a Hanning window       */
void window(fixed fr[], int n)
{
        int i,j,k;

        j = N_WAVE/n;
        n >>= 1;
        for(i=0,k=N_WAVE/4; i<n; ++i,k+=j)
                FIX_MPY(fr[i],fr[i],16384-(Sinewave[k]>>1));
        n <<= 1;
        for(k-=j; i<n; ++i,k-=j)
                FIX_MPY(fr[i],fr[i],16384-(Sinewave[k]>>1));
}

/*      fix_loud() - compute loudness of freq-spectrum components.
        n should be ntot/2, where ntot was passed to fix_fft();
        6 dB is added to account for the omitted alias components.
        scale_shift should be the result of fix_fft(), if the time-series
        was obtained from an inverse FFT, 0 otherwise.
        loud[] is the loudness, in dB wrt 32767; will be +10 to -N_LOUD.
*/
void fix_loud(fixed loud[], fixed fr[], fixed fi[], int n, int scale_shift)
{
        int i, max;

        max = 0;
        if(scale_shift > 0)
                max = 10;
        scale_shift = (scale_shift+1) * 6;

        for(i=0; i<n; ++i) {
                loud[i] = db_from_ampl(fr[i],fi[i]) + scale_shift;
                if(loud[i] > max)
                        loud[i] = max;
        }
}

/*      db_from_ampl() - find loudness (in dB) from
        the complex amplitude.
*/
int db_from_ampl(fixed re, fixed im)
{
        static long loud2[N_LOUD] = {0};
        long v;
        int i;

        if(loud2[0] == 0) {
                loud2[0] = (long)Loudampl[0] * (long)Loudampl[0];
                for(i=1; i<N_LOUD; ++i) {
                        v = (long)Loudampl[i] * (long)Loudampl[i];
                        loud2[i] = v;
                        loud2[i-1] = (loud2[i-1]+v) / 2;
                }
        }

        v = (long)re * (long)re + (long)im * (long)im;

        for(i=0; i<N_LOUD; ++i)
                if(loud2[i] <= v)
                        break;

        return (-i);
}

#define NUM_HIS_BIN 43


/*
        fix_mpy() - fixed-point multiplication
*/
fixed fix_mpy(fixed a, fixed b)
{
        FIX_MPY(a,a,b);
        return a;
}

/*
        iscale() - scale an integer value by (numer/denom)
*/
int iscale(int value, int numer, int denom)
{
	return (long) value * (long)numer/(long)denom;
}

/*
        fix_dot() - dot product of two fixed arrays
*/
fixed fix_dot(fixed *hpa, fixed *pb, int n)
{
        fixed *pa;
        long sum;
        register fixed a,b;
        unsigned int seg,off;

        sum = 0L;
        while(n--) {
                a = *pa++;
                b = *pb++;
                FIX_MPY(a,a,b);
                sum += a;
        }

        if(sum > 0x7FFF)
                sum = 0x7FFF;
        else if(sum < -0x7FFF)
                sum = -0x7FFF;

        return (fixed)sum;

}


#if N_WAVE != 1024
        ERROR: N_WAVE != 1024
#endif
fixed Sinewave[1024] = {
      0,    201,    402,    603,    804,   1005,   1206,   1406,
   1607,   1808,   2009,   2209,   2410,   2610,   2811,   3011,
   3211,   3411,   3611,   3811,   4011,   4210,   4409,   4608,
   4807,   5006,   5205,   5403,   5601,   5799,   5997,   6195,
   6392,   6589,   6786,   6982,   7179,   7375,   7571,   7766,
   7961,   8156,   8351,   8545,   8739,   8932,   9126,   9319,
   9511,   9703,   9895,  10087,  10278,  10469,  10659,  10849,
  11038,  11227,  11416,  11604,  11792,  11980,  12166,  12353,
  12539,  12724,  12909,  13094,  13278,  13462,  13645,  13827,
  14009,  14191,  14372,  14552,  14732,  14911,  15090,  15268,
  15446,  15623,  15799,  15975,  16150,  16325,  16499,  16672,
  16845,  17017,  17189,  17360,  17530,  17699,  17868,  18036,
  18204,  18371,  18537,  18702,  18867,  19031,  19194,  19357,
  19519,  19680,  19840,  20000,  20159,  20317,  20474,  20631,
  20787,  20942,  21096,  21249,  21402,  21554,  21705,  21855,
  22004,  22153,  22301,  22448,  22594,  22739,  22883,  23027,
  23169,  23311,  23452,  23592,  23731,  23869,  24006,  24143,
  24278,  24413,  24546,  24679,  24811,  24942,  25072,  25201,
  25329,  25456,  25582,  25707,  25831,  25954,  26077,  26198,
  26318,  26437,  26556,  26673,  26789,  26905,  27019,  27132,
  27244,  27355,  27466,  27575,  27683,  27790,  27896,  28001,
  28105,  28208,  28309,  28410,  28510,  28608,  28706,  28802,
  28897,  28992,  29085,  29177,  29268,  29358,  29446,  29534,
  29621,  29706,  29790,  29873,  29955,  30036,  30116,  30195,
  30272,  30349,  30424,  30498,  30571,  30643,  30713,  30783,
  30851,  30918,  30984,  31049,
  31113,  31175,  31236,  31297,
  31356,  31413,  31470,  31525,  31580,  31633,  31684,  31735,
  31785,  31833,  31880,  31926,  31970,  32014,  32056,  32097,
  32137,  32176,  32213,  32249,  32284,  32318,  32350,  32382,
  32412,  32441,  32468,  32495,  32520,  32544,  32567,  32588,
  32609,  32628,  32646,  32662,  32678,  32692,  32705,  32717,
  32727,  32736,  32744,  32751,  32757,  32761,  32764,  32766,
  32767,  32766,  32764,  32761,  32757,  32751,  32744,  32736,
  32727,  32717,  32705,  32692,  32678,  32662,  32646,  32628,
  32609,  32588,  32567,  32544,  32520,  32495,  32468,  32441,
  32412,  32382,  32350,  32318,  32284,  32249,  32213,  32176,
  32137,  32097,  32056,  32014,  31970,  31926,  31880,  31833,
  31785,  31735,  31684,  31633,  31580,  31525,  31470,  31413,
  31356,  31297,  31236,  31175,  31113,  31049,  30984,  30918,
  30851,  30783,  30713,  30643,  30571,  30498,  30424,  30349,
  30272,  30195,  30116,  30036,  29955,  29873,  29790,  29706,
  29621,  29534,  29446,  29358,  29268,  29177,  29085,  28992,
  28897,  28802,  28706,  28608,  28510,  28410,  28309,  28208,
  28105,  28001,  27896,  27790,  27683,  27575,  27466,  27355,
  27244,  27132,  27019,  26905,  26789,  26673,  26556,  26437,
  26318,  26198,  26077,  25954,  25831,  25707,  25582,  25456,
  25329,  25201,  25072,  24942,  24811,  24679,  24546,  24413,
  24278,  24143,  24006,  23869,  23731,  23592,  23452,  23311,
  23169,  23027,  22883,  22739,  22594,  22448,  22301,  22153,
  22004,  21855,  21705,  21554,  21402,  21249,  21096,  20942,
  20787,  20631,  20474,  20317,  20159,  20000,  19840,  19680,
  19519,  19357,  19194,  19031,  18867,  18702,  18537,  18371,
  18204,  18036,  17868,  17699,  17530,  17360,  17189,  17017,
  16845,  16672,  16499,  16325,  16150,  15975,  15799,  15623,
  15446,  15268,  15090,  14911,  14732,  14552,  14372,  14191,
  14009,  13827,  13645,  13462,  13278,  13094,  12909,  12724,
  12539,  12353,  12166,  11980,  11792,  11604,  11416,  11227,
  11038,  10849,  10659,  10469,  10278,  10087,   9895,   9703,
   9511,   9319,   9126,   8932,   8739,   8545,   8351,   8156,
   7961,   7766,   7571,   7375,   7179,   6982,   6786,   6589,
   6392,   6195,   5997,   5799,   5601,   5403,   5205,   5006,
   4807,   4608,   4409,   4210,   4011,   3811,   3611,   3411,
   3211,   3011,   2811,   2610,   2410,   2209,   2009,   1808,
   1607,   1406,   1206,   1005,    804,    603,    402,    201,
      0,   -201,   -402,   -603,   -804,  -1005,  -1206,  -1406,
  -1607,  -1808,  -2009,  -2209,  -2410,  -2610,  -2811,  -3011,
  -3211,  -3411,  -3611,  -3811,  -4011,  -4210,  -4409,  -4608,
  -4807,  -5006,  -5205,  -5403,  -5601,  -5799,  -5997,  -6195,
  -6392,  -6589,  -6786,  -6982,  -7179,  -7375,  -7571,  -7766,
  -7961,  -8156,  -8351,  -8545,  -8739,  -8932,  -9126,  -9319,
  -9511,  -9703,  -9895, -10087, -10278, -10469, -10659, -10849,
 -11038, -11227, -11416, -11604, -11792, -11980, -12166, -12353,
 -12539, -12724, -12909, -13094, -13278, -13462, -13645, -13827,
 -14009, -14191, -14372, -14552, -14732, -14911, -15090, -15268,
 -15446, -15623, -15799, -15975, -16150, -16325, -16499, -16672,
 -16845, -17017, -17189, -17360, -17530, -17699, -17868, -18036,
 -18204, -18371, -18537, -18702, -18867, -19031, -19194, -19357,
 -19519, -19680, -19840, -20000, -20159, -20317, -20474, -20631,
 -20787, -20942, -21096, -21249, -21402, -21554, -21705, -21855,
 -22004, -22153, -22301, -22448, -22594, -22739, -22883, -23027,
 -23169, -23311, -23452, -23592, -23731, -23869, -24006, -24143,
 -24278, -24413, -24546, -24679, -24811, -24942, -25072, -25201,
 -25329, -25456, -25582, -25707, -25831, -25954, -26077, -26198,
 -26318, -26437, -26556, -26673, -26789, -26905, -27019, -27132,
 -27244, -27355, -27466, -27575, -27683, -27790, -27896, -28001,
 -28105, -28208, -28309, -28410, -28510, -28608, -28706, -28802,
 -28897, -28992, -29085, -29177, -29268, -29358, -29446, -29534,
 -29621, -29706, -29790, -29873, -29955, -30036, -30116, -30195,
 -30272, -30349, -30424, -30498, -30571, -30643, -30713, -30783,
 -30851, -30918, -30984, -31049, -31113, -31175, -31236, -31297,
 -31356, -31413, -31470, -31525, -31580, -31633, -31684, -31735,
 -31785, -31833, -31880, -31926, -31970, -32014, -32056, -32097,
 -32137, -32176, -32213, -32249, -32284, -32318, -32350, -32382,
 -32412, -32441, -32468, -32495, -32520, -32544, -32567, -32588,
 -32609, -32628, -32646, -32662, -32678, -32692, -32705, -32717,
 -32727, -32736, -32744, -32751, -32757, -32761, -32764, -32766,
 -32767, -32766, -32764, -32761, -32757, -32751, -32744, -32736,
 -32727, -32717, -32705, -32692, -32678, -32662, -32646, -32628,
 -32609, -32588, -32567, -32544, -32520, -32495, -32468, -32441,
 -32412, -32382, -32350, -32318, -32284, -32249, -32213, -32176,
 -32137, -32097, -32056, -32014, -31970, -31926, -31880, -31833,
 -31785, -31735, -31684, -31633, -31580, -31525, -31470, -31413,
 -31356, -31297, -31236, -31175, -31113, -31049, -30984, -30918,
 -30851, -30783, -30713, -30643, -30571, -30498, -30424, -30349,
 -30272, -30195, -30116, -30036, -29955, -29873, -29790, -29706,
 -29621, -29534, -29446, -29358, -29268, -29177, -29085, -28992,
 -28897, -28802, -28706, -28608, -28510, -28410, -28309, -28208,
 -28105, -28001, -27896, -27790, -27683, -27575, -27466, -27355,
 -27244, -27132, -27019, -26905, -26789, -26673, -26556, -26437,
 -26318, -26198, -26077, -25954, -25831, -25707, -25582, -25456,
 -25329, -25201, -25072, -24942, -24811, -24679, -24546, -24413,
 -24278, -24143, -24006, -23869, -23731, -23592, -23452, -23311,
 -23169, -23027, -22883, -22739, -22594, -22448, -22301, -22153,
 -22004, -21855, -21705, -21554, -21402, -21249, -21096, -20942,
 -20787, -20631, -20474, -20317, -20159, -20000, -19840, -19680,
 -19519, -19357, -19194, -19031, -18867, -18702, -18537, -18371,
 -18204, -18036, -17868, -17699, -17530, -17360, -17189, -17017,
 -16845, -16672, -16499, -16325, -16150, -15975, -15799, -15623,
 -15446, -15268, -15090, -14911, -14732, -14552, -14372, -14191,
 -14009, -13827, -13645, -13462, -13278, -13094, -12909, -12724,
 -12539, -12353, -12166, -11980, -11792, -11604, -11416, -11227,
 -11038, -10849, -10659, -10469, -10278, -10087,  -9895,  -9703,
  -9511,  -9319,  -9126,  -8932,  -8739,  -8545,  -8351,  -8156,
  -7961,  -7766,  -7571,  -7375,  -7179,  -6982,  -6786,  -6589,
  -6392,  -6195,  -5997,  -5799,  -5601,  -5403,  -5205,  -5006,
  -4807,  -4608,  -4409,  -4210,  -4011,  -3811,  -3611,  -3411,
  -3211,  -3011,  -2811,  -2610,  -2410,  -2209,  -2009,  -1808,
  -1607,  -1406,  -1206,  -1005,   -804,   -603,   -402,   -201,
};

#if N_LOUD != 100
        ERROR: N_LOUD != 100
#endif
fixed Loudampl[100] = {
  32767,  29203,  26027,  23197,  20674,  18426,  16422,  14636,
  13044,  11626,  10361,   9234,   8230,   7335,   6537,   5826,
   5193,   4628,   4125,   3676,   3276,   2920,   2602,   2319,
   2067,   1842,   1642,   1463,   1304,   1162,   1036,    923,
    823,    733,    653,    582,    519,    462,    412,    367,
    327,    292,    260,    231,    206,    184,    164,    146,
    130,    116,    103,     92,     82,     73,     65,     58,
     51,     46,     41,     36,     32,     29,     26,     23,
     20,     18,     16,     14,     13,     11,     10,      9,
      8,      7,      6,      5,      5,      4,      4,      3,
      3,      2,      2,      2,      2,      1,      1,      1,
      1,      1,      1,      0,      0,      0,      0,      0,
      0,      0,      0,      0,
};

int16_t BPMDetect::m_iSquareTable[100] = {0, 0};

int32_t BPMDetect::DetectBeat(fixed *real)
{
	int32_t iAvgLoud = 0;

	for (int i=0;i<FFT_DECOMP_SIZE;i++)
	{
		iAvgLoud += db_from_ampl(real[i], 0) + 100;
	}

	// create a static table for fast square root
	// since we only need an integer approximation
	if (m_iSquareTable[1] == 0)
	{
		for (int i=0; i<100; i++)
			m_iSquareTable[i] = i * i;
	}
	
	int iRet = 0;

	fixed imag[FFT_DECOMP_SIZE];

	memset(imag, 0, sizeof(fixed) * FFT_DECOMP_SIZE);

	fixed loud[FFT_DECOMP_SIZE];

	window(real, FFT_DECOMP_SIZE);

	fix_fft(real, imag, FFT_DECOMP_N, 0);

	// loud is filled with the loudness values
	//	note that values are in db so are negative
	fix_loud(loud, real, imag, FFT_DECOMP_SIZE/2, 0);
	
	for (int i=0; i<BIN_CHECK_NUM; i++)
	{
		int32_t iAvg = 0;

		// first get the average
		for (int j=0; j<BEAT_HIST_LEN; j++)
		{
			iAvg += m_psHistory[i][j];
		}

		// divide by the BEAST_HIST_LEN to get the average
		iAvg >>= BEAT_HIST_N;

		// now calculate the variance for each bin
		int32_t iVar = 0;
		
		// note that loudness will be less than 100, so should be
		//	no need to worry about overflow
		for (int j=0; j<BEAT_HIST_LEN; j++)
		{
			int32_t tmp = m_psHistory[i][j] - iAvg;
			tmp *= tmp;
			iVar += tmp;
		}
		
		// Now find the standard deviation
		int j = 0;
		while (j < 99)
		{
			if (iVar < m_iSquareTable[++j])
				break;
		}
		
		// dial back twice
		//ReportError4("[%i] iAvg=%i, j=%i, loud:%i", i, iAvg, j, loud[i]);
		
		if (iAvg - j > loud[i])
			iRet = 1;
	}

	for (int i=0;i<BIN_CHECK_NUM;i++)
	{
		m_psHistory[i][m_iCurHisBin] = loud[i];
	}

	for (int i=0;i<FFT_DECOMP_SIZE/2;i++)
	{
		m_pcFreqMag[i] = (int8_t) loud[i] + 101;
	}

	m_pcAvgMag[m_pcCurMagWPos] = iAvgLoud >> FFT_DECOMP_N;
	ReportError2("[%i] = %i", m_pcCurMagWPos, m_pcAvgMag[m_pcCurMagWPos]);


	m_pcCurMagWPos++;

	if (m_pcCurMagWPos >= MAG_HIS_LEN)
	{
		m_pcCurMagWPos = 0;
	}

	m_iCurHisBin++;

	if (m_iCurHisBin>=BEAT_HIST_LEN)
	{
		m_iCurHisBin = 0;
		m_iValid = 1;
	}

	//ReportError2("e=%i, cmp=%i", e, (int32_t)(iVar + iAvg));

	return iRet && m_iValid;
}




#ifdef  MAIN

#include        <stdio.h>
#include        <math.h>

#define M       4
#define N       (1<<M)
/*
main(){
        fixed real[N], imag[N];
        int     i;

        for (i=0; i<N; i++){
                real[i] = 1000*cos(i*2*3.1415926535/N);
                imag[i] = 0;
        }

        fix_fft(real, imag, M, 0;

        for (i=0; i<N; i++){
                printf("%d: %d, %d\n", i, real[i], imag[i]);
        }

        fix_fft(real, imag, M, 1);

        for (i=0; i<N; i++){
                printf("%d: %d, %d\n", i, real[i], imag[i]);
        }
}*/
#endif  /* MAIN */


