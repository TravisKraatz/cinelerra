/*
 * Copyright (c) 2003 Matteo Frigo
 * Copyright (c) 2003 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* This file was automatically generated --- DO NOT EDIT */
/* Generated on Sat Jul  5 21:29:59 EDT 2003 */

#include "codelet-dft.h"

/* Generated by: /homee/stevenj/cvs/fftw3.0.1/genfft/gen_twiddle -compact -variables 4 -n 8 -name t1_8 -include t.h */

/*
 * This function contains 66 FP additions, 32 FP multiplications,
 * (or, 52 additions, 18 multiplications, 14 fused multiply/add),
 * 28 stack variables, and 32 memory accesses
 */
/*
 * Generator Id's : 
 * $Id: algsimp.ml,v 1.7 2003/03/15 20:29:42 stevenj Exp $
 * $Id: fft.ml,v 1.2 2003/03/15 20:29:42 stevenj Exp $
 * $Id: gen_twiddle.ml,v 1.16 2003/04/16 19:51:27 athena Exp $
 */

#include "t.h"

static const R *t1_8(R *ri, R *ii, const R *W, stride ios, int m, int dist)
{
     DK(KP707106781, +0.707106781186547524400844362104849039284835938);
     int i;
     for (i = m; i > 0; i = i - 1, ri = ri + dist, ii = ii + dist, W = W + 14) {
	  E T7, T1e, TH, T19, TF, T13, TR, TU, Ti, T1f, TK, T16, Tu, T12, TM;
	  E TP;
	  {
	       E T1, T18, T6, T17;
	       T1 = ri[0];
	       T18 = ii[0];
	       {
		    E T3, T5, T2, T4;
		    T3 = ri[WS(ios, 4)];
		    T5 = ii[WS(ios, 4)];
		    T2 = W[6];
		    T4 = W[7];
		    T6 = FMA(T2, T3, T4 * T5);
		    T17 = FNMS(T4, T3, T2 * T5);
	       }
	       T7 = T1 + T6;
	       T1e = T18 - T17;
	       TH = T1 - T6;
	       T19 = T17 + T18;
	  }
	  {
	       E Tz, TS, TE, TT;
	       {
		    E Tw, Ty, Tv, Tx;
		    Tw = ri[WS(ios, 7)];
		    Ty = ii[WS(ios, 7)];
		    Tv = W[12];
		    Tx = W[13];
		    Tz = FMA(Tv, Tw, Tx * Ty);
		    TS = FNMS(Tx, Tw, Tv * Ty);
	       }
	       {
		    E TB, TD, TA, TC;
		    TB = ri[WS(ios, 3)];
		    TD = ii[WS(ios, 3)];
		    TA = W[4];
		    TC = W[5];
		    TE = FMA(TA, TB, TC * TD);
		    TT = FNMS(TC, TB, TA * TD);
	       }
	       TF = Tz + TE;
	       T13 = TS + TT;
	       TR = Tz - TE;
	       TU = TS - TT;
	  }
	  {
	       E Tc, TI, Th, TJ;
	       {
		    E T9, Tb, T8, Ta;
		    T9 = ri[WS(ios, 2)];
		    Tb = ii[WS(ios, 2)];
		    T8 = W[2];
		    Ta = W[3];
		    Tc = FMA(T8, T9, Ta * Tb);
		    TI = FNMS(Ta, T9, T8 * Tb);
	       }
	       {
		    E Te, Tg, Td, Tf;
		    Te = ri[WS(ios, 6)];
		    Tg = ii[WS(ios, 6)];
		    Td = W[10];
		    Tf = W[11];
		    Th = FMA(Td, Te, Tf * Tg);
		    TJ = FNMS(Tf, Te, Td * Tg);
	       }
	       Ti = Tc + Th;
	       T1f = Tc - Th;
	       TK = TI - TJ;
	       T16 = TI + TJ;
	  }
	  {
	       E To, TN, Tt, TO;
	       {
		    E Tl, Tn, Tk, Tm;
		    Tl = ri[WS(ios, 1)];
		    Tn = ii[WS(ios, 1)];
		    Tk = W[0];
		    Tm = W[1];
		    To = FMA(Tk, Tl, Tm * Tn);
		    TN = FNMS(Tm, Tl, Tk * Tn);
	       }
	       {
		    E Tq, Ts, Tp, Tr;
		    Tq = ri[WS(ios, 5)];
		    Ts = ii[WS(ios, 5)];
		    Tp = W[8];
		    Tr = W[9];
		    Tt = FMA(Tp, Tq, Tr * Ts);
		    TO = FNMS(Tr, Tq, Tp * Ts);
	       }
	       Tu = To + Tt;
	       T12 = TN + TO;
	       TM = To - Tt;
	       TP = TN - TO;
	  }
	  {
	       E Tj, TG, T1b, T1c;
	       Tj = T7 + Ti;
	       TG = Tu + TF;
	       ri[WS(ios, 4)] = Tj - TG;
	       ri[0] = Tj + TG;
	       {
		    E T15, T1a, T11, T14;
		    T15 = T12 + T13;
		    T1a = T16 + T19;
		    ii[0] = T15 + T1a;
		    ii[WS(ios, 4)] = T1a - T15;
		    T11 = T7 - Ti;
		    T14 = T12 - T13;
		    ri[WS(ios, 6)] = T11 - T14;
		    ri[WS(ios, 2)] = T11 + T14;
	       }
	       T1b = TF - Tu;
	       T1c = T19 - T16;
	       ii[WS(ios, 2)] = T1b + T1c;
	       ii[WS(ios, 6)] = T1c - T1b;
	       {
		    E TX, T1g, T10, T1d, TY, TZ;
		    TX = TH - TK;
		    T1g = T1e - T1f;
		    TY = TP - TM;
		    TZ = TR + TU;
		    T10 = KP707106781 * (TY - TZ);
		    T1d = KP707106781 * (TY + TZ);
		    ri[WS(ios, 7)] = TX - T10;
		    ii[WS(ios, 5)] = T1g - T1d;
		    ri[WS(ios, 3)] = TX + T10;
		    ii[WS(ios, 1)] = T1d + T1g;
	       }
	       {
		    E TL, T1i, TW, T1h, TQ, TV;
		    TL = TH + TK;
		    T1i = T1f + T1e;
		    TQ = TM + TP;
		    TV = TR - TU;
		    TW = KP707106781 * (TQ + TV);
		    T1h = KP707106781 * (TV - TQ);
		    ri[WS(ios, 5)] = TL - TW;
		    ii[WS(ios, 7)] = T1i - T1h;
		    ri[WS(ios, 1)] = TL + TW;
		    ii[WS(ios, 3)] = T1h + T1i;
	       }
	  }
     }
     return W;
}

static const tw_instr twinstr[] = {
     {TW_FULL, 0, 8},
     {TW_NEXT, 1, 0}
};

static const ct_desc desc = { 8, "t1_8", twinstr, {52, 18, 14, 0}, &GENUS, 0, 0, 0 };

void X(codelet_t1_8) (planner *p) {
     X(kdft_dit_register) (p, t1_8, &desc);
}
