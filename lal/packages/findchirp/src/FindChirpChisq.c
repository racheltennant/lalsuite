/*----------------------------------------------------------------------- 
 * 
 * File Name: FindChirpChisq.c
 *
 * Author: Anderson, W. G., and Brown, D. A.
 * 
 * Revision: $Id$
 * 
 *-----------------------------------------------------------------------
 */

#if 0
<lalVerbatim file="FindChirpChisqCV">
Author: Anderson, W. G., and Brown D. A., Modified by Messaritaki E.
$Id$
</lalVerbatim>

<lalLaTeX>
\subsection{Module \texttt{FindChirpChisq.c}}
\label{ss:FindChirpChisq.c}

Module to implement the $\chi^2$ veto.

\subsubsection*{Prototypes}
\vspace{0.1in}
\input{FindChirpChisqCP}
\idx{LALFindChirpChisqVetoInit()}
\idx{LALFindChirpChisqVetoFinalize()}
\idx{LALFindChirpChisqVeto()}

\subsubsection*{Description}

The function \texttt{LALFindChirpChisqVetoInit()} takes as input the number of
bins required to contruct the $\chi^2$ veto and the number of points a data
segment as a parameter. The pointer \texttt{*params} must contain the
address of a structure of type \texttt{FindChirpChisqParams} for which storage
has already been allocated.  On exit this structure will be populated with the
correct values for execution of the function \texttt{LALFindChirpChisqVeto()}.
The workspace arrays and the inverse FFTW plan used by the veto will be
created.

The function \texttt{LALFindChirpChisqVetoFinalize()} takes the address of a
structure of type \texttt{FindChirpChisqParams} which has been populated by
\texttt{LALFindChirpChisqVetoInit()} as input. It takes the number of bins
required to contruct the $\chi^2$ veto and as a parameter. On exit all memory
allocated by the \texttt{LALFindChirpChisqVetoInit()} will be freed.

The function \texttt{LALFindChirpChisqVeto()} perfoms a $\chi^2$ veto on 
an entire data segment using the algorithm described below. On exit the
vector \texttt{chisqVec} contains the value $\chi^2(t_j)$ for the data
segment.

The function \texttt{LALFindChirpBCVChisqVeto()} perfoms a $\chi^2$ veto on 
an entire data segment using the corresponding algorithm for the BCV templates,
described below. On exit the
vector \texttt{chisqVec} contains the value $\chi^2(t_j)$ for the data
segment.


\subsubsection*{Algorithm}

chisq algorithm here

\subsubsection*{Uses}
\begin{verbatim}
LALCreateReverseComplexFFTPlan()
LALDestroyComplexFFTPlan()
LALCCreateVector()
LALCDestroyVector()
LALCOMPLEX8VectorFFT()
\end{verbatim}

\subsubsection*{Notes}

\vfill{\footnotesize\input{FindChirpChisqCV}}
</lalLaTeX>
#endif

#include <stdio.h>
#include <lal/LALStdlib.h>
#include <lal/LALConstants.h>
#include <lal/AVFactories.h>
#include <lal/ComplexFFT.h>
#include <lal/FindChirp.h>
#include <lal/FindChirpChisq.h>

NRCSID (FINDCHIRPCHISQC, "$Id$");

/* <lalVerbatim file="FindChirpChisqCP"> */
void
LALFindChirpChisqVetoInit (
    LALStatus                  *status,
    FindChirpChisqParams       *params,
    UINT4                       numChisqBins,
    UINT4                       numPoints
    )
/* </lalVerbatim> */
{
  UINT4                         l, m;

  INITSTATUS( status, "FindChirpChisqVetoInit", FINDCHIRPCHISQC );
  ATTATCHSTATUSPTR( status );

  ASSERT( params, status, 
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );

  ASSERT( numPoints > 0, status,
      FINDCHIRPCHISQH_ENUMZ, FINDCHIRPCHISQH_MSGENUMZ );

  ASSERT( ! params->plan, status, 
      FINDCHIRPCHISQH_ENNUL, FINDCHIRPCHISQH_MSGENNUL );
  ASSERT( ! params->qtildeBinVec, status, 
      FINDCHIRPCHISQH_ENNUL, FINDCHIRPCHISQH_MSGENNUL );
  ASSERT( ! params->qtildeBinVec, status, 
      FINDCHIRPCHISQH_ENNUL, FINDCHIRPCHISQH_MSGENNUL );


  /*
   *
   * if numChisqBins is zero, don't initialize anything
   *
   */


  if ( numChisqBins == 0 )
  {
    DETATCHSTATUSPTR( status );
    RETURN( status );
  }



  /*
   *
   * create storage
   *
   */


  /* create plan for chisq filter */
  LALCreateReverseComplexFFTPlan( status->statusPtr, 
      &(params->plan), numPoints, 0 );
  CHECKSTATUSPTR( status );

  /* create one vector for the fourier domain data */
  LALCCreateVector( status->statusPtr, 
      &(params->qtildeBinVec), numPoints );
  BEGINFAIL( status )
  {
    TRY( LALDestroyComplexFFTPlan( status->statusPtr, 
          &(params->plan) ), status );
  }
  ENDFAIL( status );

  /* create numBins vectors for the time domain data */
  params->qBinVecPtr = (COMPLEX8Vector **) 
    LALCalloc( 1, numChisqBins * sizeof(COMPLEX8Vector*) );
  if ( ! params->qBinVecPtr )
  {
    TRY( LALCDestroyVector( status->statusPtr, 
           &(params->qtildeBinVec) ), status );
    TRY( LALDestroyComplexFFTPlan( status->statusPtr, 
          &(params->plan) ), status );
    ABORT( status, FINDCHIRPCHISQH_EALOC, FINDCHIRPCHISQH_MSGEALOC );
  }

  for ( l = 0; l < numChisqBins; ++l )
  {
    LALCCreateVector( status->statusPtr, params->qBinVecPtr + l, numPoints );
    BEGINFAIL( status )
    {
      for ( m = 0; m < l ; ++m )
      {
        TRY( LALCDestroyVector( status->statusPtr, 
              params->qBinVecPtr + m ), status );
      }
      LALFree( params->qBinVecPtr );
      TRY( LALCDestroyVector( status->statusPtr, 
            &(params->qtildeBinVec) ), status );
      TRY( LALDestroyComplexFFTPlan( status->statusPtr, 
            &(params->plan) ), status );
    }
    ENDFAIL( status );
  }


  /* normal exit */
  DETATCHSTATUSPTR( status );
  RETURN( status );
}


/* <lalVerbatim file="FindChirpChisqCP"> */
void
LALFindChirpChisqVetoFinalize (
    LALStatus                  *status,
    FindChirpChisqParams       *params,
    UINT4                       numChisqBins
    )
/* </lalVerbatim> */
{
  UINT4                         l;

  INITSTATUS( status, "FindChirpChisqVetoInit", FINDCHIRPCHISQC );
  ATTATCHSTATUSPTR( status );


  /*
   *
   * if numChisqBins is zero, don't finalize anything
   *
   */


  if ( numChisqBins == 0 )
  {
    DETATCHSTATUSPTR( status );
    RETURN( status );
  }


  /*
   *
   * check arguments
   *
   */


  ASSERT( params, status, 
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( params->plan, status, 
      FINDCHIRPCHISQH_ENNUL, FINDCHIRPCHISQH_MSGENNUL );
  ASSERT( params->qtildeBinVec, status, 
      FINDCHIRPCHISQH_ENNUL, FINDCHIRPCHISQH_MSGENNUL );
  ASSERT( params->qtildeBinVec, status, 
      FINDCHIRPCHISQH_ENNUL, FINDCHIRPCHISQH_MSGENNUL );

  /*
   *
   * destroy storage
   *
   */


  for ( l = 0; l < numChisqBins; ++l )
  {
    LALCDestroyVector( status->statusPtr, (params->qBinVecPtr + l) );
    CHECKSTATUSPTR( status );
  }

  LALFree( params->qBinVecPtr );

  LALCDestroyVector( status->statusPtr, &(params->qtildeBinVec) );
  CHECKSTATUSPTR( status );

  /* destroy plan for chisq filter */
  LALDestroyComplexFFTPlan( status->statusPtr, &(params->plan) );
  CHECKSTATUSPTR( status );


  /* normal exit */
  DETATCHSTATUSPTR( status );
  RETURN( status );
}


/* <lalVerbatim file="FindChirpChisqCP"> */
void
LALFindChirpChisqVeto (
    LALStatus                  *status,
    REAL4Vector                *chisqVec,
    FindChirpChisqInput        *input,
    FindChirpChisqParams       *params
    )
/* </lalVerbatim> */
{
  UINT4                 j, l;
  UINT4                 numPoints;

  REAL4                *chisq;

  COMPLEX8             *q;
  COMPLEX8             *qtilde;

  UINT4                 numChisqPts;
  UINT4                 numChisqBins;
  UINT4                *chisqBin;
  REAL4                 chisqNorm;
  REAL4                 rhosq;
#if 0
  REAL4                 mismatch;
#endif

  COMPLEX8             *qtildeBin;

  INITSTATUS( status, "LALFindChirpChisqVeto", FINDCHIRPCHISQC );
  ATTATCHSTATUSPTR( status );


  /*
   *
   * check that the arguments are reasonable
   *
   */


  /* check that the output pointer is non-null and has room to store data */ 
  ASSERT( chisqVec, status, FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( chisqVec->data, status, FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );

  /* check that the parameter structure exists */
  ASSERT( params, status, FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );

  /* check that the chisq bin vector is reasonable */
  ASSERT( params->chisqBinVec, status, 
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( params->chisqBinVec->data, status, 
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( params->chisqBinVec->length > 0, status, 
      FINDCHIRPCHISQH_ECHIZ, FINDCHIRPCHISQH_MSGECHIZ );

  /* check that the fft plan exists */
  ASSERT( params->plan, status, FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );

  /* check that the input exists */
  ASSERT( input, status, FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );

  /* check that the input contains some data */
  ASSERT( input->qVec, status, 
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( input->qVec->data, status, 
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( input->qtildeVec, status, 
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( input->qtildeVec->data, status, 
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );

  /* check that the workspace vectors exist */
  ASSERT( params->qtildeBinVec, status, 
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( params->qtildeBinVec->data, status, 
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( params->qtildeBinVec->length > 0, status, 
      FINDCHIRPCHISQH_ECHIZ, FINDCHIRPCHISQH_MSGECHIZ );

#if 0
  /* check that the bank match has been set */
  if ( params->bankMatch < 0 || params->bankMatch >= 1 )
  {
    ABORT( status, FINDCHIRPCHISQH_EMTCH, FINDCHIRPCHISQH_MSGEMTCH );
  }
#endif


  /*
   *
   * point local pointers to structure pointers
   *
   */


  chisq     = chisqVec->data;

  numPoints = input->qVec->length;
  q         = input->qVec->data;
  qtilde    = input->qtildeVec->data;

  numChisqPts  = params->chisqBinVec->length;
  numChisqBins = numChisqPts - 1;
  chisqBin     = params->chisqBinVec->data;
  chisqNorm    = sqrt( params->norm );
#if 0
  mismatch     = 1.0 - params->bankMatch;
#endif

  qtildeBin = params->qtildeBinVec->data;


  /* 
   *
   * fill the numBins time series vectors for the chi-squared statistic
   *
   */


  for ( l = 0; l < numChisqBins; ++l ) 
  {
    memset( qtildeBin, 0, numPoints * sizeof(COMPLEX8) );

    memcpy( qtildeBin + chisqBin[l], qtilde + chisqBin[l], 
        (chisqBin[l+1] - chisqBin[l]) * sizeof(COMPLEX8) );

    LALCOMPLEX8VectorFFT( status->statusPtr, params->qBinVecPtr[l], 
        params->qtildeBinVec, params->plan );
    CHECKSTATUSPTR( status );
  }


  /* 
   *
   * calculate the chi-squared value at each time
   *
   */


  memset( chisq, 0, numPoints * sizeof(REAL4) );

  for ( j = 0; j < numPoints; ++j ) 
  {
    for ( l = 0; l < numChisqBins; ++l ) 
    {
      REAL4 Xl = params->qBinVecPtr[l]->data[j].re;
      REAL4 Yl = params->qBinVecPtr[l]->data[j].im;
      REAL4 deltaXl = chisqNorm * Xl -
        (chisqNorm * q[j].re / (REAL4) (numChisqBins));
      REAL4 deltaYl = chisqNorm * Yl -
        (chisqNorm * q[j].im / (REAL4) (numChisqBins));

      chisq[j] += deltaXl * deltaXl + deltaYl * deltaYl;
    }
  }

#if 0
  /* now modify the value to compute the new veto */
  for ( j = 0; j < numPoints; ++j ) 
  {
    rhosq = params->norm * (q[j].re * q[j].re + q[j].im * q[j].im);
    chisq[j] /= 1.0 + rhosq * mismatch * mismatch / (REAL4) numChisqBins;
  }
#endif

  /* normal exit */
  DETATCHSTATUSPTR( status );
  RETURN( status );
}



/* <lalVerbatim file="FindChirpChisqCP"> */
void
LALFindChirpBCVChisqVeto (
    LALStatus                  *status,
    REAL4Vector                *chisqVec,
    FindChirpChisqInput        *input1,
    FindChirpChisqInput        *input2,
    FindChirpChisqParams       *params
    )
/* </lalVerbatim> */
{
  UINT4                 j, l;
  UINT4                 numPoints;

  REAL4                *chisq;

  COMPLEX8             *q;
  COMPLEX8             *qtilde;

  COMPLEX8             *q1;
  COMPLEX8             *q2;
  COMPLEX8             *qtilde1;
  COMPLEX8             *qtilde2;

  UINT4                 numChisqPts;
  UINT4                 numChisqBins;
  UINT4                *chisqBin;
  REAL4                 chisqNorm;
  REAL4                 rhosq;
#if 0
  REAL4                 mismatch;
#endif

  COMPLEX8             *qtildeBin1;
  COMPLEX8             *qtildeBin2;
  COMPLEX8Vector       *qtildeBinVec1;
  COMPLEX8Vector       *qtildeBinVec2;
  COMPLEX8Vector      **qBinVecPtr1;
  COMPLEX8Vector      **qBinVecPtr2;

  REAL4                 b1;
  REAL4                 a2;

  INITSTATUS( status, "LALFindChirpChisqVeto", FINDCHIRPCHISQC );
  ATTATCHSTATUSPTR( status );


  /*
   *
   * check that the arguments are reasonable
   *
   */


  /* check that the output pointer is non-null and has room to store data */
  ASSERT( chisqVec, status, FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( chisqVec->data, status, FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );

  /* check that the parameter structure exists */
  ASSERT( params, status, FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );

  /* check that the chisq bin vector is reasonable */
  ASSERT( params->chisqBinVec, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( params->chisqBinVec->data, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( params->chisqBinVec->length > 0, status,
      FINDCHIRPCHISQH_ECHIZ, FINDCHIRPCHISQH_MSGECHIZ );

  /* check that the fft plan exists */
  ASSERT( params->plan, status, FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );

  /* check that the input exists */
  ASSERT( input1, status, FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( input2, status, FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );

  /* check that the input contains some data */
  ASSERT( input1->qVec, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( input1->qVec->data, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( input1->qtildeVec, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( input1->qtildeVec->data, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( input2->qVec, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( input2->qVec->data, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( input2->qtildeVec, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( input2->qtildeVec->data, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );


  /* check that the workspace vectors exist */
  ASSERT( qtildeBinVec1, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( qtildeBinVec1->data, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( qtildeBinVec1->length > 0, status,
      FINDCHIRPCHISQH_ECHIZ, FINDCHIRPCHISQH_MSGECHIZ );
  ASSERT( qtildeBinVec2, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( qtildeBinVec2->data, status,
      FINDCHIRPCHISQH_ENULL, FINDCHIRPCHISQH_MSGENULL );
  ASSERT( qtildeBinVec2->length > 0, status,
      FINDCHIRPCHISQH_ECHIZ, FINDCHIRPCHISQH_MSGECHIZ );

#if 0
  /* check that the bank match has been set */
  if ( params->bankMatch < 0 || params->bankMatch >= 1 )
  {
    ABORT( status, FINDCHIRPCHISQH_EMTCH, FINDCHIRPCHISQH_MSGEMTCH );
  }
#endif


  /* temporarily */
  b1 = 7.0 / 3.0 ;
  a2 = 1.0;


  /*
   *
   * point local pointers to structure pointers
   *
   */


  chisq     = chisqVec->data;

  numPoints = input1->qVec->length;
  q1         = input1->qVec->data;
  q2         = input2->qVec->data;
  qtilde1    = input1->qtildeVec->data;
  qtilde2    = input2->qtildeVec->data;

  numChisqPts  = params->chisqBinVec->length;
  numChisqBins = numChisqPts - 1;
  chisqBin     = params->chisqBinVec->data;
  chisqNorm    = sqrt( params->norm );
#if 0
  mismatch     = 1.0 - params->bankMatch;
#endif

/*  qtildeBin = params->qtildeBinVec->data; */

    qtildeBin1 = qtildeBinVec1->data;
    qtildeBin2 = qtildeBinVec2->data;


  /* 
   *
   * fill the numBins time series vectors for the chi-squared statistic
   *
   */


  for ( l = 0; l < numChisqBins; ++l )
  {
    memset( qtildeBin1, 0, numPoints * sizeof(COMPLEX8) );
    memset( qtildeBin2, 0, numPoints * sizeof(COMPLEX8) );

    memcpy( qtildeBin1 + chisqBin[l], qtilde1 + chisqBin[l],
        (chisqBin[l+1] - chisqBin[l]) * sizeof(COMPLEX8) );
    memcpy( qtildeBin2 + chisqBin[l], qtilde2 + chisqBin[l],
        (chisqBin[l+1] - chisqBin[l]) * sizeof(COMPLEX8) );

    LALCOMPLEX8VectorFFT( status->statusPtr, qBinVecPtr1[l],
        qtildeBinVec1, params->plan );
    LALCOMPLEX8VectorFFT( status->statusPtr, qBinVecPtr2[l],
	qtildeBinVec2, params->plan );
    CHECKSTATUSPTR( status );
  }


  /* 
   *
   * calculate the chi-squared value at each time
   *
   */


  memset( chisq, 0, numPoints * sizeof(REAL4) );

  for ( j = 0; j < numPoints; ++j )
  {
    for ( l = 0; l < numChisqBins; ++l )
    {
      REAL4 X1 = qBinVecPtr1[l]->data[j].re;
      REAL4 Y1 = qBinVecPtr1[l]->data[j].im;
      REAL4 X2 = qBinVecPtr2[l]->data[j].re;
      REAL4 Y2 = qBinVecPtr2[l]->data[j].im;

#if 0
      REAL4 deltaXl = chisqNorm * Xl -
        (chisqNorm * q[j].re / (REAL4) (numChisqBins));
      REAL4 deltaYl = chisqNorm * Yl -
        (chisqNorm * q[j].im / (REAL4) (numChisqBins));

      chisq[j] += deltaXl * deltaXl + deltaYl * deltaYl;
#endif

      REAL4 mod1 = sqrt( 
		      ( 2.0 * b1 * X1 - 2.0 * a2 * X2 -  
      ( 2.0 * b1 * q1[j].re - 2.0 * a2 * q2[j].re ) / (REAL4) (numChisqBins) ) *
                      ( 2.0 * b1 * X1 - 2.0 * a2 * X2 -  
      ( 2.0 * b1 * q1[j].re - 2.0 * a2 * q2[j].re ) / (REAL4) (numChisqBins) ) +
                      ( 2.0 * b1 * Y1 - 2.0 * a2 * Y2 -  
      ( 2.0 * b1 * q1[j].im - 2.0 * a2 * q2[j].im ) / (REAL4) (numChisqBins) ) *
                      ( 2.0 * b1 * Y1 - 2.0 * a2 * Y2 - 
      ( 2.0 * b1 * q1[j].im - 2.0 * a2 * q2[j].im ) / (REAL4) (numChisqBins) )
		        );			    

      REAL4 mod2 = sqrt( 
                      ( 2.0 * b1 * X1 + 2.0 * a2 * X2 -
      ( 2.0 * b1 * q1[j].re + 2.0 * a2 * q2[j].re ) / (REAL4) (numChisqBins) ) *
		      ( 2.0 * b1 * X1 + 2.0 * a2 * X2 -     
      ( 2.0 * b1 * q1[j].re + 2.0 * a2 * q2[j].re ) / (REAL4) (numChisqBins) ) +
           	      ( 2.0 * b1 * Y1 + 2.0 * a2 * Y2 -    
      ( 2.0 * b1 * q1[j].im + 2.0 * a2 * q2[j].im ) / (REAL4) (numChisqBins) ) *
                      ( 2.0 * b1 * Y1 + 2.0 * a2 * Y2 -    
      ( 2.0 * b1 * q1[j].im + 2.0 * a2 * q2[j].im ) / (REAL4) (numChisqBins) ) 
                        );

      REAL4 deltachisq = mod1 + mod2 ; 
            deltachisq *= deltachisq ;

      chisq[j] += deltachisq ;	    

    }
  }

#if 0
  /* now modify the value to compute the new veto */
  for ( j = 0; j < numPoints; ++j ) 
  {
    rhosq = params->norm * (q[j].re * q[j].re + q[j].im * q[j].im);
    chisq[j] /= 1.0 + rhosq * mismatch * mismatch / (REAL4) numChisqBins;
  }
#endif

  /* normal exit */
  DETATCHSTATUSPTR( status );
  RETURN( status );
}

