/*----------------------------------------------------------------------- 
 * 
 * File Name: RealFFTTest.c
 * 
 * Author: Creighton, J. D. E.
 * 
 * Revision: $Id$
 * 
 *----------------------------------------------------------------------- 
 * 
 * NAME 
 *   main()
 *
 * SYNOPSIS 
 * 
 * DESCRIPTION 
 *   Tests real FFT functions.
 * 
 * DIAGNOSTICS
 * 
 * CALLS
 * 
 * NOTES
 * 
 *-----------------------------------------------------------------------
 */

#ifndef _STDIO_H
#include <stdio.h>
#ifndef _STDIO_H
#define _STDIO_H
#endif
#endif

#ifndef _MATH_H
#include <math.h>
#ifndef _MATH_H
#define _MATH_H
#endif
#endif

#ifndef _LALSTDLIB_H
#include "LALStdlib.h"
#ifndef _LALSTDLIB_H
#define _LALSTDLIB_H
#endif
#endif

#ifndef _SEQFACTORIES_H
#include "SeqFactories.h"
#ifndef _SEQFACTORIES_H
#define _SEQFACTORIES_H
#endif
#endif

#ifndef _REALFFT_H
#include "RealFFT.h"
#ifndef _REALFFT_H
#define _REALFFT_H
#endif
#endif

#ifndef _VECTOROPS_H
#include "VectorOps.h"
#ifndef _VECTOROPS_H
#define _VECTOROPS_H
#endif
#endif

NRCSID (MAIN, "$Id$");

int debuglevel = 1;

int main()
{
  const INT4 m = 4;   /* example length of sequence of vectors */
  const INT4 n = 32;  /* example vector length */

  static Status status; 

  RealFFTPlan            *pfwd = NULL;
  RealFFTPlan            *pinv = NULL;
  REAL4Vector            *hvec = NULL;
  COMPLEX8Vector         *Hvec = NULL;
  REAL4Vector            *Pvec = NULL;
  REAL4VectorSequence    *hseq = NULL;
  COMPLEX8VectorSequence *Hseq = NULL;
  REAL4VectorSequence    *Pseq = NULL;
  CreateVectorSequenceIn  seqinp;
  CreateVectorSequenceIn  seqout;

  INT4 i;
  INT4 j;

  /* create vectors and sequences */

  seqinp.length       = m;
  seqinp.vectorLength = n;
  seqout.length       = m;
  seqout.vectorLength = n/2 + 1;

  SCreateVector         (&status, &hvec, n);
  REPORTSTATUS (&status);

  CCreateVector         (&status, &Hvec, n/2 + 1);
  REPORTSTATUS (&status);

  SCreateVector         (&status, &Pvec, n/2 + 1);
  REPORTSTATUS (&status);

  SCreateVectorSequence (&status, &hseq, &seqinp);
  REPORTSTATUS (&status);

  CCreateVectorSequence (&status, &Hseq, &seqout);
  REPORTSTATUS (&status);

  SCreateVectorSequence (&status, &Pseq, &seqout);
  REPORTSTATUS (&status);

  /* create plans */

  EstimateFwdRealFFTPlan (&status, &pfwd, n);
  REPORTSTATUS (&status);

  EstimateInvRealFFTPlan (&status, &pinv, n);
  REPORTSTATUS (&status);

  DestroyRealFFTPlan     (&status, &pfwd);
  REPORTSTATUS (&status);

  DestroyRealFFTPlan     (&status, &pinv);
  REPORTSTATUS (&status);

  MeasureFwdRealFFTPlan  (&status, &pfwd, n);
  REPORTSTATUS (&status);

  MeasureInvRealFFTPlan  (&status, &pinv, n);
  REPORTSTATUS (&status);

  /* assign data ... */
  printf ("\nInitial data:\n");
  for (i = 0; i < n; ++i)
  {
    REAL4 data = cos(7*i) - 1;
    hvec->data[i] = data;
    for (j = 0; j < m; ++j)
    {
      hseq->data[i + j*n] = data;
      printf ("% 9.3f\t", data);
      data += 1;
    }
    printf ("\n");
  }

  /* perform FFTs */

  printf ("\nSingle Forward FFT:\n");
  FwdRealFFT (&status, Hvec, hvec, pfwd);
  REPORTSTATUS (&status);
  for (i = 0; i < Hvec->length; ++i)
    printf ("(% 9.3f, % 9.3f)\n", Hvec->data[i].re, Hvec->data[i].im);

  printf ("\nSingle Forward FFT Power:\n");
  RealPowerSpectrum (&status, Pvec, hvec, pfwd);
  REPORTSTATUS (&status);
  for (i = 0; i < Pvec->length; ++i)
    printf ("%12.3f\n", Pvec->data[i]);

  printf ("\nSingle Inverse FFT:\n");
  InvRealFFT (&status, hvec, Hvec, pinv);
  REPORTSTATUS (&status);
  for (i = 0; i < hvec->length; ++i)
    printf ("% 9.3f\n", hvec->data[i]/n);

  printf ("\nMultiple Forward FFT:\n");
  FwdRealSequenceFFT (&status, Hseq, hseq, pfwd);
  REPORTSTATUS (&status);
  for (i = 0; i < Hseq->vectorLength; ++i)
  {
    for (j = 0; j < Hseq->length; ++j)
      printf ("(% 9.3f, % 9.3f)\t",
                    Hseq->data[i + j*Hseq->vectorLength].re,
                    Hseq->data[i + j*Hseq->vectorLength].im);
    printf ("\n");
  }

  printf ("\nMultiple Forward FFT Power:\n");
  RealSequencePowerSpectrum (&status, Pseq, hseq, pfwd);
  REPORTSTATUS (&status);
  for (i = 0; i < Pseq->vectorLength; ++i)
  {
    for (j = 0; j < Pseq->length; ++j)
      printf ("%12.3f\t", Pseq->data[i + j*Pseq->vectorLength]);
    printf ("\n");
  }

  printf ("\nMultiple Inverse FFT:\n");
  InvRealSequenceFFT (&status, hseq, Hseq, pinv);
  REPORTSTATUS (&status);
  for (i = 0; i < hseq->vectorLength; ++i)
  {
    for (j = 0; j < hseq->length; ++j)
      printf ("% 9.3f\t", hseq->data[i + j*hseq->vectorLength]/n);
    printf ("\n");
  }

  /* destroy plans, vectors, and sequences */
  DestroyRealFFTPlan     (&status, &pfwd);
  REPORTSTATUS (&status);

  DestroyRealFFTPlan     (&status, &pinv);
  REPORTSTATUS (&status);

  SDestroyVector         (&status, &hvec);
  REPORTSTATUS (&status);

  CDestroyVector         (&status, &Hvec);
  REPORTSTATUS (&status);

  SDestroyVector         (&status, &Pvec);
  REPORTSTATUS (&status);

  SDestroyVectorSequence (&status, &hseq);
  REPORTSTATUS (&status);

  CDestroyVectorSequence (&status, &Hseq);
  REPORTSTATUS (&status);

  SDestroyVectorSequence (&status, &Pseq);
  REPORTSTATUS (&status);

  LALCheckMemoryLeaks ();
  return 0;
}
