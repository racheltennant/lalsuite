/* <lalLaTeX>
\subsection{Module \texttt{ComputeStrain.c}}

\subsubsection*{Description}


\subsubsection*{Algorithm}


\subsubsection*{Uses}
\begin{verbatim}
None
\end{verbatim}

\subsubsection*{Notes}

</lalLaTeX> */

#include <math.h>
#include <lal/LALStdlib.h>
#include <lal/LALConstants.h>
#include <lal/Calibration.h>
#include <lal/LALDatatypes.h>
#include <lal/LALStdlib.h>
#include <lal/LALStdio.h>
#include <lal/FileIO.h>
#include <lal/AVFactories.h>
#include <lal/Window.h>
#include <lal/BandPassTimeSeries.h>

#include <gsl/gsl_interp.h>
#include <gsl/gsl_spline.h>

#include <lal/filters-H1-S3.h>

#define MAXALPHAS 10000

NRCSID( COMPUTESTRAINC, "$Id$" );

void LALComputeStrain(
    LALStatus              *status,
    StrainOut              *output,    
    StrainIn               *input
    )
{
/* Inverse sensing, servo, analog actuation, digital x 
actuation  digital y actuation */
static REAL8TimeSeries hR,uphR,hC,hCX,hCY;
int p;

 INITSTATUS( status, "LALComputeStrain", COMPUTESTRAINC );
 ATTATCHSTATUSPTR( status );

 XLALGetFactors(status, output, input);
 CHECKSTATUSPTR( status );
  
 /* Create vectors that will hold the residual, control and net strain signals */
 LALDCreateVector(status->statusPtr,&hR.data,input->AS_Q.data->length);
 CHECKSTATUSPTR( status );
 LALDCreateVector(status->statusPtr,&hC.data,input->AS_Q.data->length);
 CHECKSTATUSPTR( status );
 LALDCreateVector(status->statusPtr,&hCX.data,input->AS_Q.data->length);
 CHECKSTATUSPTR( status );
 LALDCreateVector(status->statusPtr,&hCY.data,input->AS_Q.data->length);
 CHECKSTATUSPTR( status );
 LALDCreateVector(status->statusPtr,&uphR.data,input->CinvUSF*input->AS_Q.data->length);
 CHECKSTATUSPTR( status );

 hR.deltaT=input->AS_Q.deltaT;
 hC.deltaT=input->AS_Q.deltaT;
 uphR.deltaT=input->AS_Q.deltaT/input->CinvUSF;

 /* copy AS_Q input into residual strain as double */  
 for (p=0; p<(int)hR.data->length; p++) 
   {
     hR.data->data[p]=input->AS_Q.data->data[p];
   }
 /* copy AS_Q input into control strain as double */  
 for (p=0; p<(int)hC.data->length; p++) 
   {
     hC.data->data[p]=input->AS_Q.data->data[p];
   }

 /* ---------- Compute Residual Strain -------------*/
 /* to get the residual strain we must first divide AS_Q by alpha */
 if(XLALhROverAlpha(&hR, output)) 
   {
     ABORT(status,116,"Broke at hR/alpha");
   }
 
 /* then we upsample (and smooth it with a low pass filter) */
 if (input->CinvUSF > 1){
   if(XLALUpsamplehR(&uphR, &hR, input->CinvUSF)) 
     { 
       ABORT(status,117,"Broke upsampling hR");
     }
   /* FIXME: Low pass filter to smooth time series */
 }

 /* then we filter through the inverse of the sensing function */
 for(p = 0; p < input->NCinv; p++){
   LALIIRFilterREAL8Vector(status->statusPtr,uphR.data,&(input->Cinv[p]));
   CHECKSTATUSPTR( status );
 }
  
 /* If there's a delay (or advance), apply it */ 
 if (input->CinvDelay != 0){
   /* Now implement the time advance filter */
   for (p=0; p<(int)uphR.data->length+input->CinvDelay; p++){
     uphR.data->data[p]=uphR.data->data[p-input->CinvDelay];
   }
 }
 
 /* FIXME: Low pass again before downsampling */
 
 /* then we downsample and voila' */
 for (p=0; p<(int)hR.data->length; p++) {
   output->h.data->data[p]=uphR.data->data[p*input->CinvUSF];
 }

 /* Clean up stuff associated with residual strain */
 LALDDestroyVector(status->statusPtr,&uphR.data);
 CHECKSTATUSPTR( status );
 LALDDestroyVector(status->statusPtr,&hR.data);
 CHECKSTATUSPTR( status );

 /* ---------- Compute Control Strain -------------*/
 /* to get the control strain we first multiply AS_Q by beta */
 if( XLALhCTimesBeta(&hC, output))
   { 
     ABORT(status,120,"Broke in hC x beta");
   }
 
 /* Now we filter through the servo, */
 for(p = 0; p < input->NG; p++){
   LALIIRFilterREAL8Vector(status->statusPtr,hC.data,&(input->G[p]));
   CHECKSTATUSPTR( status );
 }

 /* Now we filter through the analog part of actuation */
 for(p = 0; p < input->NAA; p++){
   LALIIRFilterREAL8Vector(status->statusPtr,hC.data,&(input->AA[p]));
   CHECKSTATUSPTR( status );
 }

 /* and apply the delay  if any */
 if (input->AADelay != 0){
   /* Now implement the time advance filter */
   for (p=0; p<(int)uphR.data->length+input->AADelay; p++){
     hC.data->data[p]=hC.data->data[p-input->AADelay];
   }
 }

 /* Copy data into x and y time series for parallel filtering */
 for (p=0; p < (int)hC.data->length; p++){
   hCX.data->data[p]= hCY.data->data[p]=hC.data->data[p];
 }

 /* Destroy vector holding control signal */
 LALDDestroyVector(status->statusPtr,&hC.data);
 CHECKSTATUSPTR( status );

 /* Filter x-arm */
 for(p = 0; p < input->NAX; p++){
   LALIIRFilterREAL8Vector(status->statusPtr,hCX.data,&(input->AX[p]));
   CHECKSTATUSPTR( status );
 }

 /* Filter y-arm */
 for(p = 0; p < input->NAY; p++){
   LALIIRFilterREAL8Vector(status->statusPtr,hCY.data,&(input->AY[p]));
   CHECKSTATUSPTR( status );
 }

 /* ---------- Compute Net Strain -------------*/

 /* add x-arm and y-arm (adjusting gains) and voila' */
 for (p=0; p< (int)output->h.data->length; p++){
   output->h.data->data[p] += (hCX.data->data[p]+hCY.data->data[p])/2;
 }
 
 /* destroy vectors that hold the data */
 LALDDestroyVector(status->statusPtr,&hCX.data);
 CHECKSTATUSPTR( status );
 LALDDestroyVector(status->statusPtr,&hCY.data);
 CHECKSTATUSPTR( status );

 DETATCHSTATUSPTR( status );
 RETURN( status );

}

/*******************************************************************************/

int XLALhCTimesBeta(REAL8TimeSeries *hC, StrainOut *output)
{
  int n,m;
  REAL8 time,InterpolatedBeta;
  static REAL8 beta[MAXALPHAS],tainterp[MAXALPHAS];

  /* copy ouput betas into local array */
  for(m=0; m < (int)output->beta.data->length; m++)
    {
      beta[m]=output->beta.data->data[m].re;
      tainterp[m]= m*output->beta.deltaT;
    }

  time=0.0;    /* time variable */

  {
    gsl_interp_accel *acc_alpha = gsl_interp_accel_alloc();      /* GSL spline interpolation stuff */
    gsl_spline *spline_alpha = gsl_spline_alloc(gsl_interp_cspline,output->beta.data->length);
    gsl_spline_init(spline_alpha,tainterp,beta,output->beta.data->length);

    for (n = 0; n < (int)hC->data->length; n++) 
      {
	InterpolatedBeta=gsl_spline_eval(spline_alpha,time,acc_alpha); 
	hC->data->data[n] *= InterpolatedBeta;
	time=time+hC->deltaT;
      }

    /* clean up GSL spline interpolation stuff */
    gsl_spline_free(spline_alpha);
    gsl_interp_accel_free(acc_alpha);
  }

  return 0;
}

/*******************************************************************************/

int XLALUpsamplehR(REAL8TimeSeries *uphR, REAL8TimeSeries *hR, int up_factor)
{
  int n;

  /* Set all values to 0 */
  for (n=0; n < (int)uphR->data->length; n++) {
    uphR->data->data[n] = 0.0;
  }

  /* Set one in every up_factor to the value of hR x USR */
  for (n=0; n < (int)hR->data->length; n++) {
    uphR->data->data[n * up_factor] = up_factor * hR->data->data[n];
  }

  return 0;
}

/*******************************************************************************/

int XLALhROverAlpha(REAL8TimeSeries *hR, StrainOut *output)
{
  int n,m;
  double time,InterpolatedAlpha;
  double alpha[MAXALPHAS],tainterp[MAXALPHAS];

  /* copy ouput alphas into local array */
  for(m=0; m < (int)output->alpha.data->length; m++)
    {
      alpha[m]=output->alpha.data->data[m].re;
      tainterp[m]= m*output->alpha.deltaT;
    }

  time=0.0;    /* time variable */
  
  {
    gsl_interp_accel *acc_alpha = gsl_interp_accel_alloc();      /* GSL spline interpolation stuff */
    gsl_spline *spline_alpha = gsl_spline_alloc(gsl_interp_cspline,output->alpha.data->length);
    gsl_spline_init(spline_alpha,tainterp,alpha,output->alpha.data->length);

    for (n = 0; n < (int)hR->data->length; n++) 
      {
	InterpolatedAlpha=gsl_spline_eval(spline_alpha,time,acc_alpha);
	
	hR->data->data[n] /= InterpolatedAlpha;
	time=time+hR->deltaT;
      }
  /* clean up GSL spline interpolation stuff */
  gsl_spline_free(spline_alpha);
  gsl_interp_accel_free(acc_alpha);
  }

  return 0;
}

/*******************************************************************************/

void XLALGetFactors(LALStatus *status, StrainOut *output, StrainIn *input)
{

static REAL4TimeSeries darm;
static REAL4TimeSeries asq;
static REAL4TimeSeries exc;

CalFactors factors;
UpdateFactorsParams params;

REAL4Vector *asqwin=NULL,*excwin=NULL,*darmwin=NULL;  /* windows */
LALWindowParams winparams;
INT4 k,m;
 
REAL4 deltaT=input->AS_Q.deltaT, To=input->To;
INT4 length = input->AS_Q.data->length;

  /* Create local data vectors */
  LALCreateVector(status->statusPtr,&asq.data,(UINT4)(To/input->AS_Q.deltaT +0.5));
  LALCreateVector(status->statusPtr,&darm.data,(UINT4)(To/input->DARM.deltaT +0.5));
  LALCreateVector(status->statusPtr,&exc.data,(UINT4)(To/input->EXC.deltaT +0.5));

  /* Create Window vectors */
  LALCreateVector(status->statusPtr,&asqwin,(UINT4)(To/input->AS_Q.deltaT +0.5));
  LALCreateVector(status->statusPtr,&darmwin,(UINT4)(To/input->DARM.deltaT +0.5));
  LALCreateVector(status->statusPtr,&excwin,(UINT4)(To/input->EXC.deltaT +0.5));

  /* assign time spacing for local time series */
  asq.deltaT=input->AS_Q.deltaT;
  darm.deltaT=input->DARM.deltaT;
  exc.deltaT=input->EXC.deltaT;

  winparams.type=Hann;
   
  /* windows for time domain channels */
  /* asq */
  winparams.length=(INT4)(To/asq.deltaT +0.5);
  LALWindow(status->statusPtr,asqwin,&winparams);
  
  /* darm */
  winparams.length=(INT4)(To/darm.deltaT +0.5);
  LALWindow(status->statusPtr,darmwin,&winparams);

  /* exc */
  winparams.length=(INT4)(To/exc.deltaT +0.5);
  LALWindow(status->statusPtr,excwin,&winparams);

  for(m=0; m < (UINT4)(deltaT*length) / To; m++)
    {

      /* assign and window the data */
      for(k=0;k<(INT4)(To/asq.deltaT +0.5);k++)
	{
	  asq.data->data[k] = input->AS_Q.data->data[m * (UINT4)(To/asq.deltaT) + k] * 2.0 * asqwin->data[k];
	}
      for(k=0;k<(INT4)(input->To/darm.deltaT +0.5);k++)
	{
	  darm.data->data[k] = input->DARM.data->data[m *(UINT4)(To/darm.deltaT) + k] * 2.0 * darmwin->data[k];
	}
      for(k=0;k<(INT4)(input->To/exc.deltaT +0.5);k++)
	{
	  exc.data->data[k] = input->EXC.data->data[m * (UINT4)(To/exc.deltaT) + k] * 2.0 * excwin->data[k];
	}

      /* set params to call LALComputeCalibrationFactors */
      params.darmCtrl = &darm;
      params.asQ = &asq;
      params.exc = &exc;

      params.lineFrequency = input->f;
      params.openloop =  input->Go;
      params.digital = input->Do;

      LALComputeCalibrationFactors(status->statusPtr,&factors,&params);

      output->alpha.data->data[m]= factors.alpha;
      output->beta.data->data[m]= factors.beta;

      if(m == MAXALPHAS)
	{
	  fprintf(stderr,"Too many values of the factors, maximum allowed is %d\n",MAXALPHAS);
	  RETURN(status);
	}
    }

  /* Clean up */
  LALDestroyVector(status->statusPtr,&darm.data);
  LALDestroyVector(status->statusPtr,&exc.data);
  LALDestroyVector(status->statusPtr,&asq.data);

  LALDestroyVector(status->statusPtr,&asqwin);
  LALDestroyVector(status->statusPtr,&darmwin);
  LALDestroyVector(status->statusPtr,&excwin);


  RETURN (status);
}
