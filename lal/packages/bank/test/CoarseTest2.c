/* <lalVerbatim file="LALCoarseTest2CV">
Author: Churches, D. K. and Sathyaprakash, B. S.
$Id$
</lalVerbatim> */

/* <lalLaTeX>
\subsection{Program \texttt{LALCoarseTest2.c}}
\label{ss:LALCoarseTest2.c}

Test code for the inspiral modules.

\subsubsection*{Usage}
\begin{verbatim}
LALCoarseTest2
\end{verbatim}

\subsubsection*{Description}

This test code gives an example of how to generate a template bank and
generates vertices of the ambiguity 'rectangle' around each lattice point
suitable for plotting with xmgr or xgrace.

\subsubsection*{Exit codes}
\input{LALCoarseTest2CE}

\subsubsection*{Uses}
\begin{verbatim}
lalDebugLevel
LALRectangleVertices
LALInspiralCreateCoarseBank
\end{verbatim}

\subsubsection*{Notes}

\vfill{\footnotesize\input{LALCoarseTest2CV}}
</lalLaTeX> */

/* <lalErrTable file="LALCoarseTest2CE"> */
/* </lalErrTable> */

#include <stdio.h>
#include <lal/LALInspiralBank.h>
#include <lal/LALNoiseModels.h>

INT4 lalDebugLevel=0;

int 
main ( void )
{
   InspiralTemplateList *coarseList=NULL;
   static LALStatus status;
   static InspiralCoarseBankIn coarseIn;
   static InspiralFineBankIn   fineIn;
   static INT4 i, j, nlist;
   static InspiralBankParams bankPars;
   static RectangleIn RectIn;
   static RectangleOut RectOut;

   coarseIn.mMin = 1.0;
   coarseIn.MMax = 40.0;
   coarseIn.mmCoarse = 0.80;
   coarseIn.mmFine = 0.95;
   coarseIn.fLower = 40.;
   coarseIn.fUpper = 2000;
   coarseIn.iflso = 0;
   coarseIn.tSampling = 4096.;
   coarseIn.NoisePsd = LALLIGOIPsd;
   coarseIn.order = twoPN;
   coarseIn.space = Tau0Tau3;
   coarseIn.approximant = TaylorT1;
/* minimum value of eta */
   coarseIn.etamin = coarseIn.mMin * ( coarseIn.MMax - coarseIn.mMin) /
      pow(coarseIn.MMax,2.);

   LALInspiralCreateCoarseBank(&status, &coarseList, &nlist, coarseIn);

   fprintf(stderr, "nlist=%d\n",nlist);
   for (i=0; i<nlist; i++) {
      printf("%e %e %e %e %e %e %e\n", 
         coarseList[i].params.t0, 
         coarseList[i].params.t3, 
         coarseList[i].params.t2, 
         coarseList[i].params.totalMass,
         coarseList[i].params.eta, 
         coarseList[i].params.mass1, 
         coarseList[i].params.mass2);
   }

  printf("&\n");

  fineIn.coarseIn = coarseIn;
  for (j=0; j<nlist; j++) {
     
     RectIn.dx = sqrt(2. * (1. - coarseIn.mmCoarse)/coarseList[j].metric.g00 );
     RectIn.dy = sqrt(2. * (1. - coarseIn.mmCoarse)/coarseList[j].metric.g11 );
     RectIn.theta = coarseList[j].metric.theta;
     RectIn.x0 = coarseList[j].params.t0;

     switch (coarseIn.space) 
     {
        case Tau0Tau2: 
           RectIn.y0 = coarseList[j].params.t2;
        break;
        case Tau0Tau3: 
           RectIn.y0 = coarseList[j].params.t3;
        break;
        default: 
           exit(0);
     }

     LALRectangleVertices(&status, &RectOut, &RectIn);
     printf("%e %e\n%e %e\n%e %e\n%e %e\n%e %e\n", 
        RectOut.x1, RectOut.y1, 
        RectOut.x2, RectOut.y2, 
        RectOut.x3, RectOut.y3, 
        RectOut.x4, RectOut.y4, 
        RectOut.x5, RectOut.y5);
  }
  if (coarseList!=NULL) LALFree(coarseList);
  LALCheckMemoryLeaks();
  return(0);
}
