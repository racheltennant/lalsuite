/* *****************************************************************************
 * Author: Cokelaer, T. and Sathyaprakash, B. S.
 * ************************************************************************** */
#include "BankEfficiency.h"

/* --- version information --- */
NRCSID( BANKEFFICIENCYC, "$Id$");
RCSID(  "$Id$");

#define CVS_ID_STRING_C      "$Id$"
#define CVS_REVISION_C      "$Revision$"


/* --- the main program --- */
size_t nHisto = 200;
double minHisto = 0., maxHisto = 20.;

static COMPLEX8Vector *strainSegment = NULL;
static COMPLEX8FrequencySeries       resp;

void  BEPrintErrorFloat(char *, REAL4);

void
PrintParameters(InspiralCoarseBankIn 	coarse,
		RandomInspiralSignalIn 	rand,
		OtherParamIn    	other);

static int vrbflg = 0;

int
main (INT4 argc, CHAR **argv ) 
{
  /* --- Variables ---*/
  INT4         ntrials = 0;
  INT4 	i;

  /* input */
  OtherParamIn 			otherIn;

  /* signal related */
  REAL4Vector   signal;
  RandomInspiralSignalIn 	randIn;		

  /* template bank related */  
  InspiralTemplateList      	*list = NULL;
  InspiralCoarseBankIn      	coarseBankIn;
  INT4  	sizeBank = 0;

  /* filtering related */
  REAL4Vector    correlation;
  REAL4Vector    FilterBCV1;
  REAL4Vector    FilterBCV2;
  BEPowerVector  powerVector;
  BEMoments      moments;
  InspiralWaveOverlapIn 	overlapin;		
  InspiralWaveOverlapOut     	overlapout; 

  /* results and datamining */
  float 			*bankEfficiencyOverlapIn;			
  float 			*bankEfficiencyOverlapInTime;			
  float 			*bankEfficiencyOverlapCIn;			
  float 			*bankEfficiencyOverlapCInTime;			
  ResultIn                      result;
  OverlapOutputIn OverlapOutputThisTemplate;
  OverlapOutputIn OverlapOutputBestTemplate;

  /* others */
  RealFFTPlan 			*fwdp = NULL;
  RealFFTPlan 			*revp = NULL;


  LALStatus 		        status = blank_status;

  FILE                          *Foutput;

  /* START main code */
  
  /* init some histogram structure */
  gsl_histogram * histogramNoise = gsl_histogram_alloc (200);  
  gsl_histogram_set_ranges_uniform (histogramNoise, 0.,20.);

  /* --- Some initialization --- */ 

  lalDebugLevel = 0;

  /* Initialization of structure */
  ParametersInitialization(&coarseBankIn, 
		  	   &randIn,
			   &otherIn);

  /* Read user parameters */
  ParseParameters(&argc,
		  argv,
		  &coarseBankIn,
		  &randIn,
		  &otherIn);

  /* PrintParameters(coarseBankIn, 
		  randIn, 
		  otherIn);*/

  /* Check input parameters*/
  UpdateParams(&coarseBankIn,
	       &randIn,
	       &otherIn);
  

  /* 
     This is used if one want to use condor script to keep track of the
     input parameters. It creates a prototype to be read by Ascii2xml. 
     Useful with condor. See BankEfficiency documentation.
  */
  if (otherIn.PrintPrototype){
    BEPrintProtoXml(coarseBankIn, randIn, otherIn);    
    exit(0);
  }
  

  /* --- Estimate the size of the signal --- */
  LAL_CALL(BEGetMaximumSize(&status, randIn, &(signal.length)), 
	   &status);

  if (otherIn.numSeconds*randIn.param.tSampling > signal.length) {
    signal.length = otherIn.numSeconds * randIn.param.tSampling;
  }
  else if (otherIn.numSeconds != -1) {
    fprintf(stderr, "you asked a length of %d seconds but a (%f %f) system  might be longer than that length...quitting \n", 
	    otherIn.numSeconds, randIn.mMin, randIn.mMin);
    exit(0);
  }
  else {
    otherIn.numSeconds = (INT4)(signal.length/randIn.param.tSampling);
  }


  /* --- Allocate memory for some vectors --- */
  FilterBCV1.length     = signal.length;
  FilterBCV2.length     = signal.length;
  randIn.psd.length 	= signal.length/2 + 1; 
  correlation.length 	= signal.length;

  signal.data 		= (REAL4*) LALCalloc(1, sizeof(REAL4) * signal.length);
  correlation.data 	= (REAL4*) LALCalloc(1, sizeof(REAL4) * correlation.length);
  randIn.psd.data 	= (REAL8*) LALCalloc(1, sizeof(REAL8) * randIn.psd.length); 
  FilterBCV1.data       = (REAL4*) LALCalloc(1, sizeof(REAL4) * FilterBCV1.length);
  FilterBCV2.data       = (REAL4*) LALCalloc(1, sizeof(REAL4) * FilterBCV2.length);
   

  /* create the PSd noise */
  LAL_CALL(BECreatePsd(&status, &coarseBankIn, &randIn, otherIn), 
	   &status);

  LAL_CALL(BECreateBank(&status, &coarseBankIn, &list, &sizeBank), 
	   &status);
  
  /* --- do we want to print the bank in asccii ? in xml format ?? --- */
  if (otherIn.PrintBank || otherIn.PrintPrototype){
    BEPrintBank(coarseBankIn, &list, sizeBank);		      
    BEPrintBankXml(list,  sizeBank, coarseBankIn, randIn, otherIn);	
  }

  /* Finally let's  create extra storage */
  bankEfficiencyOverlapIn      = (float*) malloc(sizeof(float*)* sizeBank);              	/* allocate memory to store the bank results	*/
  bankEfficiencyOverlapInTime  = (float*) malloc(sizeof(float*)* sizeBank);              	/* allocate memory to store the bank results 	*/
  bankEfficiencyOverlapCIn     = (float*) malloc(sizeof(float*)* sizeBank);              	/* allocate memory to store the bank results	*/
  bankEfficiencyOverlapCInTime = (float*) malloc(sizeof(float*)* sizeBank);              	/* allocate memory to store the bank results 	*/
  
  
  /* --- Estimate the fft's plans --- */
  LAL_CALL(LALCreateForwardRealFFTPlan(&status, &fwdp, signal.length, 0), 
	   &status);
  LAL_CALL(LALCreateReverseRealFFTPlan(&status, &revp, signal.length, 0), 
	   &status);
  
  
  /* --- The overlap structure --- */
  overlapin.nBegin 	= 0;
  overlapin.nEnd 	= 0;
  overlapin.psd 	= randIn.psd;
  overlapin.fwdp 	= randIn.fwdp = fwdp;
  overlapin.revp 	= revp;
  overlapin.ifExtOutput = 0;

  list[1].params.startPhase = 0; /* why ? do not remember but keep it for the time being*/
  
  /* --- Create Matrix for BCV Maximization once for all --- */
  LAL_CALL( BECreatePowerVector(&status, &powerVector, randIn, signal.length), 
	    &status);
  LALCreateMomentVector(&moments, &coarseBankIn.shf,  &(list[0].params), signal.length);
  
  
  /*srand(randIn.useed);*/

  /*
   * Main loop is here. We creae random signal and filter 
   * it with the template bank.
   */
  /* while we want to perform one simulation */
  while (++ntrials <= otherIn.ntrials) 
    {
      UINT4 currentTemplateNumber = 0;
      /* do some initialisation for the output and create data to be analysed*/
      BEInitOverlapOutputIn(&OverlapOutputBestTemplate);
     
      randIn.param.fCutoff 		= coarseBankIn.fUpper; 			      
	
      /* inject signal into some noise if gaussian noise */
      LAL_CALL(BEGenerateInputData(&status,  &signal, &randIn, otherIn), 
	       &status);
      /* or use the real data */
      if (otherIn.NoiseModel == REALPSD && (randIn.type = 3))
	{
	  for (i=0; i<(INT4)signal.length/2; i++){
	    INT4 k = signal.length-i;
	    signal.data[i] = strainSegment->data[i].re; 
	    signal.data[k] = strainSegment->data[i].im; 
	  }	   	      
	}
      
      overlapin.signal 	= signal;
      
      /* we might force to use only one template giving the input psi0 and psi3. 
	 Then, we don't need to use the bank*/
      /*if (otherIn.psi0 != -1 && otherIn.psi3 != -1)
	{
	  list[0].params.psi0 = otherIn.psi0;
	  list[0].params.psi3 = otherIn.psi3;
	  sizeBank  = 1;
	}
      */  
      
      /* --- we can process the data through the bank now ---        */
      for (currentTemplateNumber = 0; currentTemplateNumber < (UINT4)sizeBank; currentTemplateNumber++) 
	{	
	  /* trick to check the code; template == signal and we use only one template 
	     suppose to be optimal if signal and template are equivalent. Obvisously not 
	     if signal family <> template family -- */
	  overlapin.param 	= list[currentTemplateNumber].params;	   
	  overlapout.max        = -1.;
	  overlapin.param.fCutoff = randIn.param.tSampling/2. - 1;;
	  overlapin.param.fFinal  = randIn.param.tSampling/2. - 1;
	  

	  if (otherIn.faithfulness){
	    randIn.param.fCutoff = coarseBankIn.fUpper;
	    list[currentTemplateNumber].params = randIn.param;
	    overlapin.param                    = randIn.param;
	    overlapin.param.approximant        = otherIn.template;
	    sizeBank                           = 1;  /* we dont need the whole bank for checking. only one template needed */ 
	  }
	 
	  

	  /* which template family to be used ? */
	  switch(otherIn.template)
	    {
	    case BCV:	   
	   	  
	      LAL_CALL(LALInspiralOverlapBCV(&status, 
					     &list,
					     &powerVector,
					     &otherIn,
					     &randIn, 
					     currentTemplateNumber,
					     &FilterBCV1,
					     &FilterBCV2, 
					     &overlapin,
					     &OverlapOutputThisTemplate,
					     &correlation, &moments), 
		       &status);
	      
	      break;
	    case TaylorT1:
	    case TaylorT2:
	    case TaylorT3:
	    case TaylorF1:
	    case TaylorF2:
	    case EOB:
	    case PadeT1:
	    case PadeF1:
	    case SpinTaylor:

	       if (otherIn.faithfulness){ /* same parameters but different templates */
		 overlapin.param.fCutoff = randIn.param.tSampling/2. - 1;;
		 overlapin.param.fFinal = randIn.param.tSampling/2. - 1;
	      }
	      


	      if (
		  (otherIn.FastSimulation == 1 ) &&
		  ((fabs(randIn.param.t0 - list[currentTemplateNumber].params.t0)> .1) ||
		   (fabs(randIn.param.t3 - list[currentTemplateNumber].params.t3)> .1) )
		  )
		{
		  /*nothing dto do except plotting template paremter maybe */
		}
	      else
		{
		  LAL_CALL(LALInspiralWaveOverlap(&status,
						  &correlation,
						  &overlapout,
						  &overlapin), &status);
		}

		  BEFillOverlapOutput(overlapout, 
				      &OverlapOutputThisTemplate);
	      
	      break;	     
	    }     /* switch end */
	  
	  
	  
	      
	  /* fill historgranm of the correlation output */
	  if (otherIn.PrintSNRHisto){
	    for (i=0; i<(INT4)correlation.length; i++){
	      gsl_histogram_increment(histogramNoise, correlation.data[i]);
	    }		    
	  }	
	  
	  
	  
	  /* --- if overlap is the largest one, then we keep some 
	   * other information . Later we might just use the previous 
	   * vector. Keep everything and use it outside the bank 
	   * process --- we should keep time as well*/	     
	  if (otherIn.alphaFConstraint==ALPHAFConstraint){
	    bankEfficiencyOverlapCIn[currentTemplateNumber]     = OverlapOutputThisTemplate.rhoMaxConstraint; 
	    bankEfficiencyOverlapCInTime[currentTemplateNumber] = OverlapOutputThisTemplate.rhoBinConstraint; 
	    bankEfficiencyOverlapIn[currentTemplateNumber]     = OverlapOutputThisTemplate.rhoMaxUnconstraint; 
	    bankEfficiencyOverlapInTime[currentTemplateNumber] = OverlapOutputThisTemplate.rhoBinUnconstraint; 
	    
	  }
	  else {
	    bankEfficiencyOverlapIn[currentTemplateNumber]     = OverlapOutputThisTemplate.rhoMaxUnconstraint; 
	    bankEfficiencyOverlapInTime[currentTemplateNumber] = OverlapOutputThisTemplate.rhoBinUnconstraint; 
	  }
	  
	  
	  
	  /* --- if overlap is the largest one, then we keep some 
	   * other information . Later we might just use the previous 
	   * vector. Keep everything and use it outside the bank 
	   * process --- */	     
	  OverlapOutputThisTemplate.freqConstraint                =  list[currentTemplateNumber].params.fFinal;
	  OverlapOutputThisTemplate.freqUnconstraint              =  list[currentTemplateNumber].params.fFinal;
	  OverlapOutputThisTemplate.templateNumberConstraint      = currentTemplateNumber;
	  OverlapOutputThisTemplate.templateNumberUnconstraint    = currentTemplateNumber;
	  OverlapOutputThisTemplate.layerConstraint               = list[currentTemplateNumber].nLayer;
	  OverlapOutputThisTemplate.layerUnconstraint             = list[currentTemplateNumber].nLayer;
	  
	  
	  KeepHighestValues(OverlapOutputThisTemplate, 
			    &OverlapOutputBestTemplate);
	  
	}/*end of  bank process*/
      
      
      
      /* Then print the maximum overlap over the whole bank and the corresponding 
       * parameter of the templates, injected signal and so on. This is the main results
       * We'll print one line for each simulation (ntrials parameter)*/
      
      GetResult(	   &status, 
			   &list, 
			   randIn.param, 
			   OverlapOutputBestTemplate, 
			   &result, 
			   otherIn);
      result.ntrial  = ntrials;
      result.coaTime = randIn.coalescenceTime;
      
      
      PrintResults(result);  
      
      
      if (otherIn.PrintResultXml)
	{
	  BEPrintResultsXml(coarseBankIn,randIn,otherIn,result);	 
	}
      
      
      
      
      /*Now we might want given the best template, to reproduce results and keep overlp, wavefom of the signal , template ...*/
      /* save signal */
      /* overlap     */
      /* save template and correlation */
      
      if (otherIn.template == BCV){
	if (otherIn.PrintBestOverlap || otherIn.PrintBestTemplate){
	  otherIn.extraFinalPrinting = 1;
	  LAL_CALL(LALInspiralOverlapBCV(&status, 
					 &list,
					 &powerVector,
					 &otherIn,
					 &randIn, 
					 OverlapOutputBestTemplate.templateNumberConstraint,
					 &FilterBCV1,
					 &FilterBCV2, 
					 &overlapin,
					 &OverlapOutputThisTemplate,
					 &correlation, &moments), 
		   &status);	
	}
      }
      
      
      
    }  /*end while(trial)*/
  


  if (otherIn.PrintSNRHisto){
    Foutput=  fopen("BE_histo.dat","w");
    gsl_histogram_fprintf(Foutput, histogramNoise, "%f", "%g");
    fclose(Foutput);
  }


  
  /* --- destroy the plans, correlation and signal --- */
  
  LALFree(powerVector.fm5_3.data);
  LALFree(powerVector.fm2_3.data);
  LALFree(powerVector.fm1_2.data);
  LALFree(powerVector.fm7_6.data);

  /*  LALFree(VectorPhase.data);*/

  LALFree(moments.a11.data);
  LALFree(moments.a21.data);
  LALFree(moments.a22.data);
   
  LALFree(FilterBCV2.data);
  LALFree(FilterBCV1.data);
  
  LALFree(randIn.psd.data);
  LALDDestroyVector( &status, &(coarseBankIn.shf.data) );
  
  LALFree(signal.data);
  LALFree(correlation.data);
  LALFree(list);
  
  LALDestroyRealFFTPlan(&status,&fwdp);
  LALDestroyRealFFTPlan(&status,&revp);
  
  gsl_histogram_free(histogramNoise);
   
  LALCheckMemoryLeaks();    
   
   
  return 0;
}



/* ****************************************************************************
 * Initialization of the parameters fromn random signal, bank and others 
 * structures
 * ***************************************************************************/
void ParametersInitialization(	InspiralCoarseBankIn 	*coarseBankIn, 
				RandomInspiralSignalIn	*randIn, 
				OtherParamIn		*otherIn)
{
  
  InitInspiralCoarseBankIn(coarseBankIn);
  InitRandomInspiralSignalIn(randIn);
  InitOtherParamIn(otherIn);
  
}


/* ****************************************************************************
 * CoarseIn initialization
 ***************************************************************************/
void InitInspiralCoarseBankIn(InspiralCoarseBankIn *coarseBankIn)
{

  coarseBankIn->fLower    	= BANKEFFICIENCY_FLOWER;
  coarseBankIn->fUpper    	= BANKEFFICIENCY_TSAMPLING/2. - 1.;
  coarseBankIn->tSampling 	= BANKEFFICIENCY_TSAMPLING;
  coarseBankIn->space     	= BANKEFFICIENCY_SPACE;
  coarseBankIn->mmCoarse  	= BANKEFFICIENCY_MMCOARSE;
  coarseBankIn->mmFine    	= BANKEFFICIENCY_MMFINE;
  coarseBankIn->iflso           = BANKEFFICIENCY_IFLSO ;
  coarseBankIn->mMin      	= BANKEFFICIENCY_MMIN;
  coarseBankIn->mMax      	= BANKEFFICIENCY_MMAX;
  coarseBankIn->MMax      	= BANKEFFICIENCY_MMAX * 2;
  coarseBankIn->massRange       = MinMaxComponentMass; 
  coarseBankIn->etamin 	        = coarseBankIn->mMin * coarseBankIn->mMax  
			        / pow(coarseBankIn->MMax, 2.);

  coarseBankIn->psi0Min   	= BANKEFFICIENCY_PSI0MIN;
  coarseBankIn->psi0Max   	= BANKEFFICIENCY_PSI0MAX;
  coarseBankIn->psi3Min   	= BANKEFFICIENCY_PSI3MIN;
  coarseBankIn->psi3Max   	= BANKEFFICIENCY_PSI3MAX;
  coarseBankIn->alpha     	= BANKEFFICIENCY_ALPHABANK;
  /*unsigned variable ... */
  coarseBankIn->numFcutTemplates = BANKEFFICIENCY_NFCUT;
  coarseBankIn->approximant     = BANKEFFICIENCY_TEMPLATE;  
  coarseBankIn->order           = BANKEFFICIENCY_ORDER_TEMPLATE;  
  coarseBankIn->LowGM     	= BANKEFFICIENCY_LOWGM;
  coarseBankIn->HighGM    	= BANKEFFICIENCY_HIGHGM;
  coarseBankIn->gridType        = Square;
}


/* ****************************************************************************
 * Random initialization 
 * ****************************************************************************/
void InitRandomInspiralSignalIn(RandomInspiralSignalIn *randIn)
{
  randIn->useed         	= BANKEFFICIENCY_USEED;				/* seed for random MC simulation 	*/ 
  randIn->type          	= BANKEFFICIENCY_TYPE;				/* type of simulation 			*/
  randIn->SignalAmp     	= BANKEFFICIENCY_SIGNALAMPLITUDE;		/* if x = n +h 				*/
  randIn->param.order   	= BANKEFFICIENCY_ORDER_SIGNAL;   		/* and its order			*/
  randIn->param.alpha		= BANKEFFICIENCY_ALPHASIGNAL; 			/* alpha value of BCV 			*/
  randIn->param.ieta    	= BANKEFFICIENCY_IETA; 				/*					*/
  randIn->param.mass1 		= BANKEFFICIENCY_MMIN;				/* To aalocate memory			*/ 
  randIn->param.mass2  		= BANKEFFICIENCY_MMIN; 				/* idem 				*/
  randIn->param.fLower  	= BANKEFFICIENCY_FLOWER;			/* Lower cutoff freq. of the template	*/
  randIn->param.OmegaS		= 0.;						/* EOB parameter 			*/
  randIn->param.Theta		= 0.;						/* EOB parameter 			*/
  randIn->mMin          	= BANKEFFICIENCY_MMIN;				/* min mass to inject			*/
  randIn->mMax          	= BANKEFFICIENCY_MMAX;				/* max mass to inject 			*/
  randIn->MMax          	= BANKEFFICIENCY_MMAX * 2;			/* total mass max 			*/
  randIn->etaMin                = BANKEFFICIENCY_MMIN 
    * (BANKEFFICIENCY_MMAX - BANKEFFICIENCY_MMIN) 
    / (BANKEFFICIENCY_MMAX * 2) / (BANKEFFICIENCY_MMAX * 2);
  randIn->psi0Min  		= BANKEFFICIENCY_PSI0MIN;			/* psi0 range 				*/
  randIn->psi0Max  		= BANKEFFICIENCY_PSI0MAX;	
  randIn->psi3Min  		= BANKEFFICIENCY_PSI3MIN;	
  randIn->psi3Max  		= BANKEFFICIENCY_PSI3MAX;	
  randIn->param.approximant 	= BANKEFFICIENCY_SIGNAL;			/* approximant of h, the signal		*/
  randIn->param.tSampling 	= BANKEFFICIENCY_TSAMPLING;			/* sampling 				*/
  randIn->param.fCutoff		= BANKEFFICIENCY_TSAMPLING/2.-1;		/* sampling 				*/
  randIn->param.startTime       = BANKEFFICIENCY_STARTTIME; 
  randIn->param.startPhase      = BANKEFFICIENCY_STARTPHASE; 
  randIn->param.nStartPad       = BANKEFFICIENCY_NSTARTPHASE;
  randIn->param.signalAmplitude = BANKEFFICIENCY_SIGNALAMPLITUDE;
  randIn->param.nEndPad         = BANKEFFICIENCY_NENDPAD;
  randIn->NoiseAmp              = BANKEFFICIENCY_NOISEAMPLITUDE;
}


/* ****************************************************************************
 * Other Param initialization							
 * ***************************************************************************/
void InitOtherParamIn(OtherParamIn *otherIn)
{
  otherIn->alphaFConstraint     = ALPHAFConstraint;
  otherIn->extraFinalPrinting   = 0; 
  otherIn->template		= BANKEFFICIENCY_TEMPLATE;
  otherIn->signalfFinal        =  BANKEFFICIENCY_TSAMPLING/2.-1;
  otherIn->signal       	= BANKEFFICIENCY_SIGNAL;
  otherIn->m1           	= -1;
  otherIn->m2           	= -1;
  otherIn->numSeconds            = -1;
  otherIn->psi0         	= -1;
  otherIn->psi3         	= -1;
  otherIn->tau0         	= -1;
  otherIn->tau3         	= -1;
  otherIn->PrintBestOverlap 	= BANKEFFICIENCY_PRINTBESTOVERLAP;
  otherIn->PrintBestTemplate 	= BANKEFFICIENCY_PRINTBESTTEMPLATE;
  otherIn->PrintSNRHisto        = BANKEFFICIENCY_PRINTSNRHISTO;
  otherIn->PrintPsd             = BANKEFFICIENCY_PRINTPSD;
  otherIn->PrintBank    	= BANKEFFICIENCY_PRINTBANK;
  otherIn->PrintResultXml    	= BANKEFFICIENCY_PRINTRESULTXML;
  otherIn->PrintPrototype    	= BANKEFFICIENCY_PRINTPROTOTYPE;
  otherIn->PrintBankOverlap	= BANKEFFICIENCY_PRINTBANKOVERLAP;
  otherIn->faithfulness        	= BANKEFFICIENCY_FAITHFULNESS;
  otherIn->ntrials 	     	= BANKEFFICIENCY_NTRIALS;
  otherIn->FastSimulation       = BANKEFFICIENCY_FASTSIMULATION;
  otherIn->NoiseModel           = LIGOI;
  otherIn->binaryInjection      = NoUserChoice; 
  otherIn->maxTotalMass         = -1;

  otherIn->detector = L1;
  otherIn->run      = S3;
  otherIn->chanName = NULL;
  otherIn->calCacheName = NULL;
  otherIn->calCacheName = NULL;
  otherIn->startTime  =751956568;
  otherIn->numSeconds = -1;


  otherIn->L1.chanName             = "L1:LSC-AS_Q";
  otherIn->H1.chanName             = "H1:LSC-AS_Q";
  otherIn->H2.chanName             = "H2:LSC-AS_Q";
  
  otherIn->L1.dataFile.S3.calCacheName         =  "/netw/critical/ligoCalibration/cache_files/L1-CAL-V03-751719553-757699245.cache";
  otherIn->L1.dataFile.S3.frInCacheName        =  "/home/cokelaer/Work/inspiralRuns/cacheFiles/CacheFile_L_S3_RDS_R_L3.txt";     
  otherIn->H1.dataFile.S3.calCacheName         =  "/netw/critical/ligoCalibration/cache_files/H1-CAL-V03-751651153-757699245.cache";
  otherIn->H1.dataFile.S3.frInCacheName        =  "/home/cokelaer/Work/inspiralRuns/cacheFiles/CacheFile_H_S3_RDS_R_L3.txt";     
  otherIn->H2.dataFile.S3.calCacheName         =  "/netw/critical/ligoCalibration/cache_files/H2-CAL-V03-751654453-757699245.cache";
  otherIn->H2.dataFile.S3.frInCacheName        =  "/home/cokelaer/Work/inspiralRuns/cacheFiles/CacheFile_H_S3_RDS_R_L3.txt";
  otherIn->L1.dataFile.S2.calCacheName         =  "/netw/critical/ligoCalibration/cache_files/L1-CAL-V03-729273600-734367600.cache";
  otherIn->L1.dataFile.S2.frInCacheName        =  "/home/cokelaer/Work/inspiralRuns/cacheFiles/CacheFile_L_S2_RDS_R_L3.txt";     
  otherIn->H1.dataFile.S2.calCacheName         =  "/netw/critical/ligoCalibration/cache_files/H1-CAL-V03-729273600-734367600.cache";
  otherIn->H1.dataFile.S2.frInCacheName        =  "/home/cokelaer/Work/inspiralRuns/cacheFiles/CacheFile_H_S2_RDS_R_L3.txt";     
  otherIn->H2.dataFile.S2.calCacheName         =  "/netw/critical/ligoCalibration/cache_files/H2-CAL-V03-731849076-734367576.cache";
  otherIn->H2.dataFile.S2.frInCacheName        =  "/home/cokelaer/Work/inspiralRuns/cacheFiles/CacheFile_H_Si2_RDS_R_L3.txt";      
}



/* ***************************************************************************
 *  Function to parse user parameters 
 *  ************************************************************************ */
void 
ParseParameters(	INT4 			*argc, 
			CHAR 			**argv,
			InspiralCoarseBankIn 	*coarseBankIn,
			RandomInspiralSignalIn 	*randIn,
			OtherParamIn    	*otherIn)
{
  INT4  i = 1;
  REAL8 tmp1, tmp2;
  
  while(i < *argc)
    {
      if (!strcmp(argv[i],	"--bank-alpha"))	     
        BEParseGetDouble(argv,  &i, &(coarseBankIn->alpha)); 
      else if (!strcmp(argv[i],"--bank-fcut-range")) 
	{
	  BEParseGetDouble2(argv,  &i, &tmp1, &tmp2);
	  coarseBankIn->LowGM  = (REAL4)tmp1; 
	  coarseBankIn->HighGM = (REAL4)tmp2;
	}
      else if (!strcmp(argv[i],	"--bank-ffinal"))	     
        BEParseGetDouble(argv,  &i, &(coarseBankIn->fUpper)); 
      else if (!strcmp(argv[i], "--bank-grid-type")){
	BEParseGetString(argv, &i);
	if (!strcmp(argv[i],"square"))
	  coarseBankIn->gridType = Square;
	else if (!strcmp(argv[i], "hexagonal"))
	  coarseBankIn->gridType = Hexagonal;
	else if (!strcmp(argv[i], "hexagonalOriented"))
	  coarseBankIn->gridType = OrientedHexagonal;
	else if (!strcmp(argv[i], "squareOriented"))
	  coarseBankIn->gridType = OrientedSquare;
	else 
	  coarseBankIn->gridType = None;        	  	
      }    
      else if (!strcmp(argv[i],	"--bank-number-fcut"))  
        BEParseGetInt(argv,  &i, (INT4*)&(coarseBankIn->numFcutTemplates)); 
      else if (!strcmp(argv[i],"--bank-mass-range"))
	BEParseGetDouble2(argv,  &i, &(coarseBankIn->mMin), &(coarseBankIn->mMax));
      else if (!strcmp(argv[i],	"--bank-psi0-range"))
        BEParseGetDouble2(argv,  &i, &(coarseBankIn->psi0Min), &(coarseBankIn->psi0Max));
      else if (!strcmp(argv[i],	"--bank-psi3-range"))
        BEParseGetDouble2(argv,  &i, &(coarseBankIn->psi3Min), &(coarseBankIn->psi3Max));
      else if (!strcmp(argv[i], "--channel"))
	{
	  BEParseGetString(argv, &i);
	  otherIn->chanName = argv[i];
	}
      else if (!strcmp(argv[i],"--debug")) 
        BEParseGetInt(argv,  &i, &(lalDebugLevel)); 
      else if (!strcmp(argv[i], "--detector"))
	{	
	  BEParseGetString(argv, &i);
	  if (!strcmp(argv[i],  "L1")) 
	    otherIn->detector = L1;
	  else if (!strcmp(argv[i],"H1"))	
	    otherIn->detector = H1;
	  else if (!strcmp(argv[i],"H2"))	
	    otherIn->detector = H2;
	  else 
	    otherIn->detector  = None;
	}
      else if (!strcmp(argv[i],"--fl-signal"))
        BEParseGetDouble(argv,  &i, &(randIn->param.fLower));
      else if  (!strcmp(argv[i],"--fl-template"))
        BEParseGetDouble(argv,  &i, &(coarseBankIn->fLower));
      else if  (!strcmp(argv[i],"--fl")) {
        BEParseGetDouble(argv,  &i, &(coarseBankIn->fLower));
	randIn->param.fLower = coarseBankIn->fLower;
      }  
      else if (!strcmp(argv[i], "--gps-start-time"))
	BEParseGetInt(argv, &i , &(otherIn->startTime));
      else if (!strcmp(argv[i], "--help") || !strcmp(argv[i],"--h"))
	Help(*coarseBankIn, *randIn, *otherIn);      
      else if (!strcmp(argv[i],"--max-total-mass")) 
	BEParseGetDouble(argv,  &i, (REAL8*)(&(otherIn->maxTotalMass)));
      else if (!strcmp(argv[i], "--m1"))
        BEParseGetDouble(argv,  &i, &(otherIn->m1));
      else if (!strcmp(argv[i], "--m2")) 	
        BEParseGetDouble(argv,  &i, &(otherIn->m2));
      else if (!strcmp(argv[i],	"--mm"))
        BEParseGetDouble(argv,  &i, &(coarseBankIn->mmCoarse)); 
      else if (!strcmp(argv[i],	"--n") || !strcmp(argv[i], "--ntrial"))
        BEParseGetInt(argv,  &i, &(otherIn->ntrials));
      else if (!strcmp(argv[i],	"--noise-amplitude"))	     
        BEParseGetDouble(argv,  &i, &(randIn->NoiseAmp)); 
      else if (!strcmp(argv[i],	"--noise-model")){	
	BEParseGetString(argv, &i);
	if (!strcmp(argv[i], "LIGOI"))	
	  otherIn->NoiseModel = LIGOI;
	else if (!strcmp(argv[i], "LIGOA"))  
            otherIn->NoiseModel = LIGOA;
	else if (!strcmp(argv[i], "VIRGO"))  
	  otherIn->NoiseModel = VIRGO;
	else if (!strcmp(argv[i], "TAMA"))  
	  otherIn->NoiseModel = TAMA;
	else if (!strcmp(argv[i], "GEO")) 
            otherIn->NoiseModel = GEO;
	else if (!strcmp(argv[i], "UNITY"))  
	  otherIn->NoiseModel = UNITY;
	else if (!strcmp(argv[i], "REALPSD"))  
	  otherIn->NoiseModel = REALPSD;
	else 
	  otherIn->NoiseModel = None;
      }
      else if (!strcmp(argv[i], "--numSeconds")) 
        BEParseGetInt(argv,  &i, &(otherIn->numSeconds));
      else if (!strcmp(argv[i], "--psi0")) 
        BEParseGetDouble(argv,  &i, &(otherIn->psi0));
      else if (!strcmp(argv[i], "--psi3")) 
        BEParseGetDouble(argv,  &i, &(otherIn->psi3));
      else if (!strcmp(argv[i], "--run"))
	{
	  BEParseGetString(argv, &i);	 
	  if (!strcmp(argv[i],  "S2")) 	
	    otherIn->run = S2;
	  else if (!strcmp(argv[i],"S3"))
	    otherIn->run = S3;	  
	  else 
	    otherIn->run = None;
	} 
      else if (!strcmp(argv[i],"--sampling")) {
        BEParseGetDouble(argv,  &i, &(coarseBankIn->tSampling));
        randIn->param.tSampling = coarseBankIn->tSampling;
	randIn->param.fCutoff = coarseBankIn->tSampling/2. - 1.;
	coarseBankIn->fUpper = coarseBankIn->tSampling/2. - 1.;
      }   
      else if (!strcmp(argv[i],"--seed"))
        BEParseGetInt(argv,  &i, &(randIn->useed));
      else if (!strcmp(argv[i], "--signal"))
	{
	BEParseGetString(argv, &i);

	  if (strcmp(argv[i],	  "TaylorT1")	==0) 		otherIn->signal = TaylorT1;
	  else if (strcmp(argv[i],"TaylorT2")	==0)		otherIn->signal = TaylorT2;
	  else if (strcmp(argv[i],"TaylorT3")	==0)	    	otherIn->signal = TaylorT3;
	  else if (strcmp(argv[i],"TaylorF1")	==0)	    	otherIn->signal = TaylorF1;
	  else if (strcmp(argv[i],"TaylorF2")	==0)	    	otherIn->signal = TaylorF2;
	  else if (strcmp(argv[i],"PadeT1")	==0)	    	otherIn->signal = PadeT1;
	  else if (strcmp(argv[i],"PadeF1")	==0)	    	otherIn->signal = PadeF1;
	  else if (strcmp(argv[i],"EOB")	==0)    	otherIn->signal = EOB;
	  else if (strcmp(argv[i],"BCV")	==0)            otherIn->signal = BCV;
	  else if (strcmp(argv[i],"SpinTaylorT3")==0)	    	otherIn->signal = SpinTaylorT3;
	  else  BEPrintError("wrong approximant\n");
	  randIn->param.approximant = otherIn->signal;		  
	}
      else if (!strcmp(argv[i],	"--signal-alpha"))	     
	BEParseGetDouble(argv,  &i, &(randIn->param.alpha));
      else if (!strcmp(argv[i],	"--signal-amplitude"))	     
	BEParseGetDouble(argv,  &i, &(randIn->SignalAmp));
      else if (!strcmp(argv[i],	"--signal-ffinal")) {
	BEParseGetDouble(argv,  &i, &(randIn->param.fCutoff));
	otherIn->signalfFinal = randIn->param.fCutoff ;
      }
      else if (!strcmp(argv[i],	"--signal-mass-range")){	
        BEParseGetDouble2(argv,  &i, &(randIn->mMin), &(randIn->mMax));
	randIn->param.mass1 = randIn->param.mass2 = randIn->mMin; /*default values for injection*/
	randIn->param.massChoice = m1Andm2;
      }   
      else if (!strcmp(argv[i],	"--signal-order")) 
	BEParseGetInt(argv,  &i, (INT4*)&(randIn->param.order));
      else if (!strcmp(argv[i],	"--signal-psi0-range")) 
        BEParseGetDouble2(argv,  &i, &(coarseBankIn->psi0Min), &(coarseBankIn->psi0Max));
      else if (!strcmp(argv[i],	"--signal-psi3-range"))
        BEParseGetDouble2(argv,  &i, &(coarseBankIn->psi3Min), &(coarseBankIn->psi3Max));
      else if (!strcmp(argv[i],	"--simulation-type"))	     	
	BEParseGetInt(argv,  &i, &(randIn->type));
      else if (!strcmp(argv[i], "--tau0")) 
        BEParseGetDouble(argv,  &i, &(otherIn->tau0));
      else if (!strcmp(argv[i], "--tau3")) 
        BEParseGetDouble(argv,  &i, &(otherIn->tau3));
      else if (!strcmp(argv[i],"--template"))
	{
	  BEParseGetString(argv, &i);

	  if (!strcmp(argv[i],	"TaylorT1")	)	otherIn->template = TaylorT1;
	  else if (!strcmp(argv[i],	"TaylorT2")	)	otherIn->template = TaylorT2;
	  else if (!strcmp(argv[i],	"TaylorT3")	)	otherIn->template = TaylorT3;
	  else if (!strcmp(argv[i],	"TaylorF1")	)	otherIn->template = TaylorF1;
	  else if (!strcmp(argv[i],	"TaylorF2")	)	otherIn->template = TaylorF2;
	  else if (!strcmp(argv[i],	"PadeT1")	)	otherIn->template = PadeT1;
	  else if (!strcmp(argv[i],	"PadeF1")	)	otherIn->template = PadeF1;
	  else if (!strcmp(argv[i],	"EOB")		)	otherIn->template = EOB;
	  else if (!strcmp(argv[i],	"BCV")		)       otherIn->template = BCV;
	  else if (!strcmp(argv[i],	"SpinTaylorT3")	)	otherIn->template = SpinTaylorT3;
	  else BEPrintError("wrong approximant\n");

	  coarseBankIn->approximant = otherIn->template;
	  if ( coarseBankIn->approximant == BCV ) 	
	    coarseBankIn->space = Psi0Psi3; 
	  else 
	    coarseBankIn->space = Tau0Tau3;
	}
      else if (!strcmp(argv[i],	"--template-order"))     
        BEParseGetInt(argv,  &i, (INT4*)(&(coarseBankIn->order)));
      

      /* no user parameters requeste, only flags below*/               
      else if (!strcmp(argv[i],"--alpha-constraint"))     
        otherIn->alphaFConstraint	= ALPHAFConstraint;
      else if (!strcmp(argv[i],"--bhns-injection"))
	otherIn->binaryInjection = BHNS;
      else if (!strcmp(argv[i],"--no-alpha-constraint"))   
        otherIn->alphaFConstraint	= ALPHAFUnconstraint;
      else if (!strcmp(argv[i],"--print-best-overlap"))  
        otherIn->PrintBestOverlap 	= 1;
      else if (!strcmp(argv[i],"--faithfulness"))  
        otherIn->faithfulness    	= 1;
      else if (!strcmp(argv[i],"--real-noise"))  
        randIn->type    	= 3;
      else if (!strcmp(argv[i],"--print-psd"))  
        otherIn->PrintPsd 		= 1;
      else if (!strcmp(argv[i],"--print-bank-overlap"))  
        otherIn->PrintBankOverlap 	= 1;
      else if (!strcmp(argv[i],"--print-snr-histo")) 
        otherIn->PrintSNRHisto 	= 1;
      else if (!strcmp(argv[i],"--verbose")) 
        vrbflg 	= 1;
      else if (!strcmp(argv[i],"--version")) 
        {
	  fprintf(stderr, "BankEfficiency code"
		  "Thomas Cokelaer, Thomas.Cokelaer@astro.cf.ac.uk\n"
		  "CVS Version :" CVS_ID_STRING_C "\n"
		  "CVS Version :" CVS_ID_STRING "\n"
		  "CVS Tag: " CVS_NAME_STRING "\n");
	  exit(0);
	}
      else if (!strcmp(argv[i],"--print-bank")) 
        otherIn->PrintBank		= 1;
      else if (!strcmp(argv[i],"--print-result-xml")) 
        otherIn->PrintResultXml        = 1;
      else if (!strcmp(argv[i],"--print-prototype")) 
        otherIn->PrintPrototype        = 1;
      else if (!strcmp(argv[i],"--fast-simulation")) 
        otherIn->FastSimulation        = 1;           	   	    
      else 
	{
        fprintf(stderr, "%s option does not exist. Type --help for options\n", argv[i]);
	exit(0);
        }      
      i++;       
    } 
  
}


void
BEParseGetInt(  CHAR    **argv, 
                INT4    *index,
                INT4    *data)
{  
  CHAR *tmp;
  CHAR msg[2048];


  if (argv[*index+1]!=NULL)
    {
      *data = strtol(argv[*index+1], &tmp  , 10);
      
      if (*data==0 && strcmp(argv[*index+1],"0"))
	{
	  sprintf(msg, "Expect a float after option %s (got %s)\n ",
		  argv[*index],
		  argv[*index+1]);
	  
	  BEPrintError(msg);
	}
    }
  else  
    {
      sprintf(msg, "Expect a float after option %s (got %s)\n ",
	      argv[*index],
	      argv[*index+1]);
      
      BEPrintError(msg);
    }
  *index =*index + 1;

}

void
BEParseGetString(  CHAR    **argv, 
		   INT4    *index)		  
{  
  CHAR msg[2048];

  
  if (argv[*index+1]==NULL)
    {
      sprintf(msg, "Expect a string after %s\n ",
	      argv[*index] );
      
      BEPrintError(msg);
    }
  *index =*index + 1;

}


void
BEParseGetDouble(CHAR    **argv, 
                INT4    *index,
                REAL8   *data)
{  
  CHAR *tmp;
  CHAR msg[2048];

  if (argv[*index+1]!=NULL)
    {
      *data = strtod(argv[*index+1], &tmp  );
      
      if (*data==0 && strcmp(argv[*index+1],"0"))
	{
	 sprintf(msg, "Expect a float after option %s (got %s)\n ",
	      argv[*index],
	      argv[*index+1]);

	 BEPrintError(msg);
	}
    }
  else  
    {
      sprintf(msg, "Expect a float after option %s (got %s)\n ",
	      argv[*index],
	      argv[*index+1]);
      BEPrintError(msg);

    }
  *index =*index + 1;
}


void
BEParseGetDouble2(CHAR    **argv, 
		  INT4    *index,
		  REAL8   *data1,
		  REAL8   *data2)
{  
  CHAR *tmp; 
 CHAR msg[2048];

  *data1 = 0 ;
  *data2 = 0 ;

  
  if (argv[*index+1]!=NULL && argv[*index+2]!=NULL)
    {
      *data1 = strtod(argv[*index+1], &tmp  );
      *data2 = strtod(argv[*index+2], &tmp  );
      
      if ((!(*data1) && strcmp(argv[*index+1],"0"))
	||  (!(*data2) && strcmp(argv[*index+2],"0")))
	{   
	  sprintf(msg, "Expect 2 floats after option %s (got %s and %s)\n ",
		  argv[*index],
		  argv[*index+1],argv[*index+2]);
	  BEPrintError(msg);
	}
    }
  else  
    {
      sprintf(msg, "Expect 2 floats after option %s (got %s and %s)\n ",
	      argv[*index],
	      argv[*index+1],argv[*index+2]);
      BEPrintError(msg);
    }
  *index = *index +2 ;  
}  
  
    
    /* --- Function to check validity of some parsed parameters (TODO)--- */
void UpdateParams(InspiralCoarseBankIn *coarseBankIn,
		  RandomInspiralSignalIn *randIn,
		  OtherParamIn *otherIn)
{
  REAL4  temp;
  CHAR   msg[2048];

  temp =randIn->param.mass1; /*just to avoid boring warning*/


  if (coarseBankIn->alpha < 0 ){
    sprintf(msg, "--bank-alpha (%f) parameter must be positive in the range [0,1] \n",
	    coarseBankIn->alpha);
    BEPrintError(msg);
    }
 if (randIn->param.alpha < 0 ){
    sprintf(msg, "--signal-alpha (%f) parameter must be positive in the range [0,1] \n",
	    randIn->param.alpha);
    BEPrintError(msg);
    }

  if (coarseBankIn->fUpper <= coarseBankIn->fLower 
      || coarseBankIn->fUpper >= coarseBankIn->tSampling/2){
    sprintf(msg, "--bank-ffinal (%f) paramter must be greater than  bank-fl (%f) and less than sampling/2 %f\n",
	    coarseBankIn->fUpper,
	    coarseBankIn->fLower ,
	    coarseBankIn->tSampling/2); 
    BEPrintError(msg);
  }
  

  if (coarseBankIn->gridType == None  ){
    sprintf(msg, "--bank-grid-type (%d) parameter must be < square, hexagonal, hexagonalOriented, squareOriented> \n",
	    coarseBankIn->gridType);
    BEPrintError(msg);
  }

  
  if  ((coarseBankIn->mMin >= coarseBankIn->mMax ) || (coarseBankIn->mMin <=0)){
    sprintf(msg, "--bank-mass-range (%f %f) paramter must be sorted and > 0 \n",
	    coarseBankIn->mMin, coarseBankIn->mMax);
    BEPrintError(msg);
  }
  else /* if valid parameters */
    {
      if (otherIn->maxTotalMass != -1)
	{
	  if (otherIn->maxTotalMass >= (2 * coarseBankIn->mMax))
	    {
	      coarseBankIn->MMax =  2 * otherIn->maxTotalMass;
	      coarseBankIn->etamin = coarseBankIn->mMin * coarseBankIn->mMax 
		/ (coarseBankIn->mMin + coarseBankIn->mMax) 
		/ (coarseBankIn->mMin + coarseBankIn->mMax);
	    }
	  else
	    {
	      coarseBankIn->MMax =  otherIn->maxTotalMass;
	      coarseBankIn->etamin = coarseBankIn->mMin * (coarseBankIn->MMax  - coarseBankIn->mMin)
		/ (coarseBankIn->MMax )
		/ (coarseBankIn->MMax );
	    }
	}
      else
	{
	  coarseBankIn->MMax = 2 * coarseBankIn->mMax;
	  coarseBankIn->etamin = coarseBankIn->mMin * (coarseBankIn->MMax  - coarseBankIn->mMin)
	    / (coarseBankIn->MMax )
	    / (coarseBankIn->MMax );
	}
    }
  
  if (coarseBankIn->psi0Min <=0 || coarseBankIn->psi0Min > coarseBankIn->psi0Max) {
    sprintf(msg, "--bank-psi0-range (%f %f) paramter must be sorted and > 0 \n",
	    coarseBankIn->psi0Min, coarseBankIn->psi0Max);
    BEPrintError(msg);
  } 

  if (coarseBankIn->psi3Min >=0 || coarseBankIn->psi3Min > coarseBankIn->psi3Max) {
    sprintf(msg, "--bank-psi0-range (%f %f) paramter must be sorted and >= 0 \n",
	    coarseBankIn->psi3Min, coarseBankIn->psi3Max);
    BEPrintError(msg);
  }


  if (otherIn->detector == None)
    {
      sprintf(msg, "--detector (%d) expect option <H1, H2, L1> only",
	      otherIn->detector);
      BEPrintError(msg);
    }

  if (coarseBankIn->LowGM > coarseBankIn->HighGM)
    {
      sprintf(msg, "--bank-fcut-range (%f %f) expect sorted , typically 3 and 6",
	      coarseBankIn->LowGM , coarseBankIn->HighGM);
      BEPrintError(msg);
    }
 
  if (coarseBankIn->fLower <10 || randIn->param.fLower < 10)
    {
      sprintf(msg, "--fl or --fl-signal or --fl-template must be >=10 Hz (%f %f)",
	      randIn->param.fLower , coarseBankIn->fLower);
      BEPrintError(msg);
    }

  if (otherIn->startTime <= 0 )
    {
      sprintf(msg, "--gps-start-time (%d) invalid. must be > 0 ",
	      otherIn->startTime );
      BEPrintError(msg);
    }

  if (otherIn->maxTotalMass != -1)
    {
      if (otherIn->maxTotalMass < 2*randIn->mMin) 
	{	
	  sprintf(msg, "--max-total-mass (%f) must be > twice the minimla mass (%f) ",
	      otherIn->maxTotalMass , randIn->mMin );
	  BEPrintError(msg);
	}
    }

  if ((otherIn->m1 != -1) && (otherIn->m2 != -1)){
    randIn->param.massChoice = fixedMasses; 
    if (otherIn->psi0 != -1 ||otherIn->psi3 != -1 || otherIn->tau0 != -1 || otherIn->tau3 != -1){
      sprintf(msg, "--m1 --m2 --psi0 --psi3 --tau0 --tau3 error. If particular injection is requested,  you must choose either (--m1,--m2) options or (--psi0,--psi3) or (--tau0,--tau3)\n");
      BEPrintError(msg);
    } 
  }
     
 if (otherIn->psi0 != -1 && otherIn->psi3 != -1){
   randIn->param.massChoice = fixedPsi; 
   if (otherIn->m1 != -1 ||otherIn->m2 != -1 || otherIn->tau0 != -1 || otherIn->tau3 != -1){
     sprintf(msg, "--m1 --m2 --psi0 --psi3 --tau0 --tau3 error. If particular injection is requested,  you must choose either (--m1,--m2) options or (--psi0,--psi3) or (--tau0,--tau3)\n");
     BEPrintError(msg);
   } 
 } 

 if (otherIn->tau0 != -1 && otherIn->tau3 != -1){
   randIn->param.massChoice = fixedTau; 
   if (otherIn->psi0 != -1 ||otherIn->psi3 != -1 || otherIn->m1 != -1 || otherIn->m2 != -1){
     sprintf(msg, "--m1 --m2 --psi0 --psi3 --tau0 --tau3 error. If particular injection is requested,  you must choose either (--m1,--m2) options or (--psi0,--psi3) or (--tau0,--tau3)\n");
     BEPrintError(msg);  
   } 
  } 

 if (coarseBankIn->mmCoarse <=0 || coarseBankIn->mmCoarse>=1){
   sprintf(msg, "--mm (%f) must be in the range ]0 1[\n", 
	   coarseBankIn->mmCoarse);
     BEPrintError(msg);  
 } 
  

 if (otherIn->NoiseModel == None){
   sprintf(msg, "--noise-model must be <LIGOI, LIGOA, VIRGO, GEO, TAMA, REALPSD>\n");
   BEPrintError(msg);  
 } 
  
 if (coarseBankIn->numFcutTemplates <= 0){
   sprintf(msg, "--bank-number-fcut (%d) must be > 0>\n", coarseBankIn->numFcutTemplates);
   BEPrintError(msg);  
 } 
  

 if (otherIn->run == None){
   sprintf(msg, "--run must be <S2 or S3>\n");
   BEPrintError(msg);  
 } 
  


 if  ((randIn->mMin >= randIn->mMax ) || (randIn->mMin <=0)){
    sprintf(msg, "--signal-mass-range (%f %f) paramter must be sorted and > 0 \n",
	    randIn->mMin, randIn->mMax);
    BEPrintError(msg);
  }
 else /* if valid parameters */
   {
     if (otherIn->maxTotalMass != -1)
       {  
	 if (otherIn->maxTotalMass >= (2 * randIn->mMax))
	   {
	  randIn->MMax = 2* randIn->mMax;
	  randIn->etaMin = randIn->mMin * randIn->mMax 
	    / (randIn->mMin + randIn->mMax)
	    / (randIn->mMin + randIn->mMax);
	   }
	 else
	   {
	     randIn->MMax = otherIn->maxTotalMass;
	     randIn->etaMin = randIn->mMin * (randIn->MMax  - randIn->mMin)
	       / (randIn->MMax )
	       / (randIn->MMax );
	   }
       }          
     else 
       {
	 randIn->MMax = 2 * randIn->mMax;
	 randIn->etaMin = randIn->mMin * (randIn->MMax  - randIn->mMin)
	   / (randIn->MMax )
	   / (randIn->MMax );
       }
   }



  switch ( otherIn->detector)
    {
    case L1:
      switch (otherIn->run)
	{
	case S2:
	  otherIn->calCacheName   = otherIn->L1.dataFile.S2.calCacheName  ;
	  otherIn->frInCacheName  = otherIn->L1.dataFile.S2.frInCacheName ;       
	  if (!otherIn->chanName)
	    otherIn->chanName  =  otherIn->L1.chanName;

	  break;
	case S3:
	  otherIn->calCacheName   = otherIn->L1.dataFile.S3.calCacheName  ;
	  otherIn->frInCacheName  = otherIn->L1.dataFile.S3.frInCacheName ;
	  if (!otherIn->chanName) otherIn->chanName  =  otherIn->L1.chanName;

	  break;
	case S1:
	case S4:
	case S5:
	case S6:
	  break;
	}
      break;
    case H1:
      switch (otherIn->run)
	{
	case S1:
	case S4:
	case S5:
	case S6:
	  break;
	case S2:
	  otherIn->calCacheName   = otherIn->H1.dataFile.S2.calCacheName  ;
	  otherIn->frInCacheName  = otherIn->H1.dataFile.S2.frInCacheName ;
	 if (!otherIn->chanName)  otherIn->chanName  =  otherIn->H1.chanName;
		  
	  break;
	case S3:
	  otherIn->calCacheName   = otherIn->H1.dataFile.S3.calCacheName  ;
	  otherIn->frInCacheName  = otherIn->H1.dataFile.S3.frInCacheName        ;
	 if (!otherIn->chanName)  otherIn->chanName       = otherIn->H1.chanName;
	  break;
	}
      break;
     
    case H2:
      switch (otherIn->run)
	{
	case S1:
	case S4:
	case S5:
	case S6:
	  break;
	case S2:
	  otherIn->calCacheName   = otherIn->H2.dataFile.S2.calCacheName;
	  otherIn->frInCacheName  = otherIn->H2.dataFile.S2.frInCacheName;
	  if (!otherIn->chanName) otherIn->chanName  =  otherIn->H2.chanName;
	  break;
	case S3:
	  otherIn->calCacheName   = otherIn->H2.dataFile.S3.calCacheName;
	  otherIn->frInCacheName  = otherIn->H2.dataFile.S3.frInCacheName;
	  if (!otherIn->chanName) otherIn->chanName       =  otherIn->H2.chanName;
	  break;
	}
      break;
      
    case V1:break;
      
    case G1:break;
    }






  if (otherIn->faithfulness ==1 && randIn->type == 1)
      BEPrintError("can not check code if no injection performed. use simulation-type = 0 or 2\n");

  if (coarseBankIn->approximant == (Approximant)(-1))
      BEPrintError("--template,  template approximant must be provided\n");

  if (randIn->param.approximant == (Approximant)(-1))
      BEPrintError("--signal, signal approximant must be provided\n");


  if (otherIn->binaryInjection==BHNS){
    if (randIn->mMin >3 || randIn->mMax < 3 ){
      BEPrintError("if you want to inject BHNS systems then adjust the mass-range so that the minimum is less than 3 solar mass and the maximum  is greater than 3solar mass !! \n");
    }
  }
  
    
}



void
BEPrintError(char *chaine)
{
  fprintf(stderr,"//--->  BankEfficiency code Error <---//\n");
  fprintf(stderr,"----->  %s",chaine);
  fprintf(stderr,"//---> please use \"--help\" option to get help documentation <---//\n");
  exit(0);
}

/* ****************************************************************************
 *  Documenation  on line
 *  **************************************************************************/
void Help(	InspiralCoarseBankIn   coarseBankIn,
       		RandomInspiralSignalIn randIn,
		OtherParamIn           otherIn)
{
  INT4 		temp;
	
  fprintf(stderr,"-------------------\n");
  fprintf(stderr,"BankEfficiency Code\n");
  fprintf(stderr,"-------------------\n");
  fprintf(stderr,"\nPURPOSE: Test efficiency of bank of templates using Monte-Carlo simulations\n");

  fprintf(stderr,"\nUSAGE:  [options]\n");
  fprintf(stderr,"\n:  [bank options]\n");
  
  fprintf(stderr,"--bank-alpha		: BCV amplitude correction parameter (%7.2f)\n", 
      BANKEFFICIENCY_ALPHABANK);
  fprintf(stderr,"--bank-mass-range	: SPA mass range [(%7.2f, %7.2f)] \n",  
      BANKEFFICIENCY_MMIN, 
      BANKEFFICIENCY_MMAX);
  fprintf(stderr,"--bank-psi0-range	: BCV psi0 range [(%7.2f, %7.2f)] \n",  
      BANKEFFICIENCY_PSI0MIN, 
      BANKEFFICIENCY_PSI0MAX);
  fprintf(stderr,"--bank-psi3-range	: BCV psi3 range [(%7.2f, %7.2f)] \n",  
      BANKEFFICIENCY_PSI3MIN, 
      BANKEFFICIENCY_PSI3MAX);
  fprintf(stderr,"--bank-ffinal		: BCV final frequency to estimate moments (%7.2f)\n", 
      BANKEFFICIENCY_FUPPER);
/* 

  ADD_PROCESS_PARAM("float",	"%12.5f",	"--fl",		coarseBankIn.fLower);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--fl-signal",	randIn.param.fLower);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--fl-template",      coarseBankIn.fLower);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--freq-moment-bank",	coarseBankIn.fUpper);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--mass-range-bank",	coarseBankIn.mMin);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--mass-range-bank",	coarseBankIn.mMax);
  ADD_PROCESS_PARAM("float",	"%12.5f",       "--max-totalmass",    otherIn.maxTotalMass);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--m1",		otherIn.m1);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--m2",		otherIn.m2);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--mm",		coarseBankIn.mmCoarse);
  ADD_PROCESS_PARAM("int",	"%6d",		"--ntrial",		otherIn.ntrials);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--noise-amplitude",	randIn.NoiseAmp);
  ADD_PROCESS_PARAM("int",	"%6d",		"--number-fcut",	coarseBankIn.numFcutTemplates);
  ADD_PROCESS_PARAM("int",	"%6d",		"--numSeconds",	otherIn.numSeconds);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--psi0",		otherIn.psi0);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--psi3",		otherIn.psi3);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--sampling",		coarseBankIn.tSampling);
  ADD_PROCESS_PARAM("int",	"%6d",		"--simulation-type",	randIn.type);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-amplitude",	randIn.SignalAmp);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-alpha",	randIn.param.alpha);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-ffinal",	otherIn.signalfFinal);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-mass-range",randIn.mMin);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-mass-range",randIn.mMax);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-psi0-range",randIn.psi0Min);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-psi0-range",randIn.psi0Max);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-psi3-range",randIn.psi3Min);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-psi3-range",randIn.psi3Max);
  ADD_PROCESS_PARAM("int",	"%6d",		"--seed",		randIn.useed);
  switch (otherIn.NoiseModel){
  case LIGOI:
    ADD_PROCESS_PARAM("string",	"%6s",	"--noisemodel","LIGOI");
    break;
  case LIGOA:
    ADD_PROCESS_PARAM("string",	"%6s",	"--noisemodel", "LIGOA");
    break;
  case VIRGO:
    ADD_PROCESS_PARAM("string",	"%6s",	"--noisemodel", "VIRGO");
    break;
    */
  fprintf(stderr,"--alpha-signal	: BCV amplitude correction parameter 			(%7.2f)\n",BANKEFFICIENCY_ALPHASIGNAL);
  fprintf(stderr,"--binaryInjection[BHNS]: if argument=BHNS then system are Blackhole+neutron star\n");

  fprintf(stderr,"--fast-simulation	:fast simulation in the SPA case in a box of 0.1seconds square\n");
  fprintf(stderr,"--fl-signal		: lower frequency cutoff             		(%7.2f) Hz\n", BANKEFFICIENCY_FLOWER);
  fprintf(stderr,"--fl-template		: lower frequency cutoff             			(%7.2f) Hz\n", BANKEFFICIENCY_FLOWER);

  fprintf(stderr,"--mass-range		: minimal mass of component stars    			(%7.2f, %7.2f) Mo\n", BANKEFFICIENCY_MMIN, BANKEFFICIENCY_MMAX);
  fprintf(stderr,"--mm			: minimal match for template bank    			(%7.3f)\n", BANKEFFICIENCY_MMCOARSE);
  fprintf(stderr,"--m1 <> --m2 <>	: Set parameter values of the injected signal \n");
  fprintf(stderr,"--tau0 <> --tau3 <>	: which should be one othe three parameters (maases, psi \n");
  fprintf(stderr,"--psi0 <> --psi3 <>	: tau components). That choice is exclusive. You cant set m1 and then tau0\n");
  fprintf(stderr,"--noise-amplitude	: use it with simulation-type <> 0 			(%7.2f)\n", BANKEFFICIENCY_NOISEAMPLITUDE); 
  fprintf(stderr,"--noise-model		: design sensitivity to use(LIGOI ,LIGOA VIRGO, GEO, TAMA	(%s)\n", BANKEFFICIENCY_NOISEMODEL);
  fprintf(stderr,"--number-fcut		: number of layers in Fcut dimension 			(%7.2d)\n", BANKEFFICIENCY_NFCUT);
  fprintf(stderr,"--ntrial		: number of trials                   			(%7.2d)\n", BANKEFFICIENCY_NTRIALS);
  fprintf(stderr,"--psi0-range		: psi0 range in the bank				(%7.2f, %7.2f) \n", BANKEFFICIENCY_PSI0MIN, BANKEFFICIENCY_PSI0MAX);
  fprintf(stderr,"--psi3-range		: psi3 range in the bank 				(%7.2f, %7.2f) \n", BANKEFFICIENCY_PSI3MAX, BANKEFFICIENCY_PSI3MIN);
  fprintf(stderr,"--template		: PN model for template bank, e.g. BCV 			(%7.2d)\n", BANKEFFICIENCY_TEMPLATE);
  fprintf(stderr,"--sampling		: sampling frequency             			(%7.2f)\n", BANKEFFICIENCY_TSAMPLING);
  fprintf(stderr,"--seed			: seed for random generation         		(%7.2d)\n", BANKEFFICIENCY_USEED);
  fprintf(stderr,"--signal		: model to inject in Monte-Carlo, e.g. EOB            	(%7.2d)\n", BANKEFFICIENCY_SIGNAL);
  fprintf(stderr,"--signal-order		: order of PN model                             (%7.2d)\n", BANKEFFICIENCY_ORDER_SIGNAL);
  fprintf(stderr,"--simulation-type	: type of simulation, 0, 1 or 2      			(%7.2d)\n", BANKEFFICIENCY_TYPE);
  fprintf(stderr,"--template-order	: order of signal to injec                            	(%7.2d)\n", BANKEFFICIENCY_ORDER_TEMPLATE);
  fprintf(stderr,"--signal-amplitude	: amplitude of the signal                             	(%7.2f)\n", BANKEFFICIENCY_SIGNALAMPLITUDE);

  fprintf(stderr,"\n --\nThe second category doesn't take any arguments.\n Those options are mainly used to print results in files\n");
  fprintf(stderr,"Verbose Options \n\n");

  /*fprintf(stderr,"     --FMaximization	: Maximize over Ending frequency 			(%d)\n", BANKEFFICIENCY_FMAXIMIZATION);*/
  fprintf(stderr,"     --FastSimulation		: filters only template around the injection (dt0=.5 and dt3=.25)\n");

  fprintf(stderr,"     --print-bank-overlap    : Print the overlap given by each template (default=no printing)		\n");

  fprintf(stderr,"     --print-bank	: Print the bank in ascii format (%s)		\n",  BANKEFFICIENCY_PRINTBANK_FILEASCII);
  fprintf(stderr,"     --print-snr-histo  : Print histogram of all the snr output (%d)	\n",  BANKEFFICIENCY_PRINTSNRHISTO);

  fprintf(stderr,"    --print-result-xml  : Print result in xml format (%s)		\n",  BANKEFFICIENCY_PRINTRESULT_FILEXML);


  fprintf(stderr,"     --print-overlap	: Print the overlap given by the best template	(%s)	(%d)\n", 
		  BANKEFFICIENCY_PRINTOVERLAP_FILE, 
		  BANKEFFICIENCY_PRINTOVERLAP);
  fprintf(stderr,"     --print-psd	: Print the psd in                              	(%s)	(%d)\n", 
		  BANKEFFICIENCY_PRINTPSD_FILE, 
		  BANKEFFICIENCY_PRINTPSD);

  fprintf(stderr,"     --InQuadrature	: Overlap is compute without alpha maximization\n");


  temp = coarseBankIn.approximant;
  temp =  randIn.useed;
  temp =  otherIn.signal;
}


		      
/* ****************************************************************************
 * Some output Results
 * ************************************************************************** */
void
KeepHighestValues(OverlapOutputIn resultThisTemplate,   
		  OverlapOutputIn *resultBestTemplate
		  )
{
  if (resultThisTemplate.rhoMaxConstraint > resultBestTemplate->rhoMaxConstraint){    
      resultBestTemplate->rhoMaxConstraint   = resultThisTemplate.rhoMaxConstraint;
      resultBestTemplate->phaseConstraint    = resultThisTemplate.phaseConstraint;
      resultBestTemplate->alphaConstraint    = resultThisTemplate.alphaConstraint;
      resultBestTemplate->rhoBinConstraint   = resultThisTemplate.rhoBinConstraint;
      resultBestTemplate->freqConstraint     = resultThisTemplate.freqConstraint;
      resultBestTemplate->layerConstraint    = resultThisTemplate.layerConstraint;
      resultBestTemplate->templateNumberConstraint   = resultThisTemplate.templateNumberConstraint;
      /*we do not need all template params for the time being*/
  }
  
  
  if (resultThisTemplate.rhoMaxUnconstraint > resultBestTemplate->rhoMaxUnconstraint){ 
    resultBestTemplate->rhoMaxUnconstraint   = resultThisTemplate.rhoMaxUnconstraint;
    resultBestTemplate->phaseUnconstraint    = resultThisTemplate.phaseUnconstraint;
    resultBestTemplate->alphaUnconstraint   = resultThisTemplate.alphaUnconstraint;
    resultBestTemplate->rhoBinUnconstraint   = resultThisTemplate.rhoBinUnconstraint;
    resultBestTemplate->freqUnconstraint     = resultThisTemplate.freqUnconstraint;
    resultBestTemplate->layerUnconstraint    = resultThisTemplate.layerUnconstraint;
    resultBestTemplate->templateNumberUnconstraint = resultThisTemplate.templateNumberUnconstraint;
  }
  
}


/* ****************************************************************************
 * Some output Results
 * ************************************************************************** */

void 
GetResult(
	  LALStatus                  *status, 
	  InspiralTemplateList        **list,
	  InspiralTemplate       	injected,
	  OverlapOutputIn 	        bestOverlap, 
	  ResultIn                      *result,
	  OtherParamIn                  otherIn )
{
  INT4 templateNumber, templateNumberC;
  InspiralTemplate trigger, triggerC;

  
  INITSTATUS (status, "GetResult", BANKEFFICIENCYC);
  ATTATCHSTATUSPTR(status);
     
  templateNumberC = bestOverlap.templateNumberConstraint;
  templateNumber = bestOverlap.templateNumberUnconstraint;
  trigger = (*list)[templateNumber].params;
  triggerC = (*list)[templateNumberC].params;




  
  
  if (otherIn.template == BCV){

    LALInspiralParameterCalc( status->statusPtr,  &triggerC );
    CHECKSTATUSPTR(status);		

    LALInspiralParameterCalc( status->statusPtr,  &trigger );
    CHECKSTATUSPTR(status);							
    
    result->psi0_trigger = trigger.psi0;
    result->psi3_trigger = trigger.psi3;
    result->psi0_inject  = injected.psi0;
    result->psi3_inject  = injected.psi3;     
    result->psi0_triggerC  = triggerC.psi0;
    result->psi3_triggerC  = triggerC.psi3;     
  }
  else{
    trigger.massChoice = m1Andm2;
    triggerC.massChoice = m1Andm2;

    LALInspiralParameterCalc( status->statusPtr,  &triggerC );
    CHECKSTATUSPTR(status);							 
    LALInspiralParameterCalc( status->statusPtr,  &trigger );
    CHECKSTATUSPTR(status);							
    

    result->psi0_trigger = trigger.t0;
    result->psi3_trigger = trigger.t3;
    result->psi0_inject  = injected.t0;
    result->psi3_inject  = injected.t3; 
    result->psi0_triggerC  = -1;
    result->psi3_triggerC  = -1;     
  }

  result->mass1_inject      = injected.mass1;
  result->mass2_inject      = injected.mass2;
  result->fend_inject       = injected.fFinal;
  result->fend_trigger      = trigger.fFinal;
  result->fend_triggerC      = triggerC.fFinal;
  result->totalMass_trigger = trigger.totalMass;
  result->eta_trigger       = trigger.eta;
  result->totalMass_triggerC = triggerC.totalMass;
  result->eta_triggerC       = triggerC.eta;

  result->rho_finalC   = bestOverlap.rhoMaxConstraint;
  result->alphaC       = bestOverlap.alphaConstraint;
  result->alpha_fC     = bestOverlap.alphaConstraint*pow(triggerC.fFinal,2./3.);
  result->binC         = bestOverlap.rhoBinConstraint;
  result->phaseC       = bestOverlap.phaseConstraint;
  result->layerC       = bestOverlap.layerConstraint;
  
  result->rho_final   = bestOverlap.rhoMaxUnconstraint;
  result->alpha       = bestOverlap.alphaUnconstraint;
  result->alpha_f     = bestOverlap.alphaUnconstraint*pow(trigger.fFinal,2./3.);
  result->bin         = bestOverlap.rhoBinUnconstraint;
  result->phase       = bestOverlap.phaseUnconstraint;
  result->layer       = bestOverlap.layerUnconstraint;
  
    
  DETATCHSTATUSPTR(status);
  RETURN (status);

}


void
PrintResults( ResultIn result)
{

  fprintf(stdout, "%e %e %e %e %e %e  %7.2f %7.2f %7.2f  %e %e  %e %e %e %e    %7.5f %e %e %e  %d %d    %7.5f %e %e %e  %d %d %d\n",
	  result.psi0_trigger, 
	  result.psi3_trigger,
	  result.psi0_triggerC, 
	  result.psi3_triggerC,
	  result.psi0_inject,
	  result.psi3_inject,
	  result.fend_trigger, 
	  result.fend_triggerC, 
	  result.fend_inject,
	  result.totalMass_trigger,
	  result.eta_trigger,
	  result.totalMass_triggerC,
	  result.eta_triggerC,
	  result.mass1_inject,
	  result.mass2_inject,
	  result.rho_final, 
	  result.phase, 
	  result.alpha,
	  result.alpha_f,
	  result.layer,
	  result.bin,
	  result.rho_finalC, 
	  result.phaseC, 
	  result.alphaC,
	  result.alpha_fC,
	  result.layerC,
	  result.binC, 
	  result.coaTime);
  fflush(stdout);
}





/* ****************************************************************************
 * Functio to generate and stored the moments in  REAL4vectors. 
 * ************************************************************************* */
void LALCreateMomentVector(BEMoments             *moments,
			   REAL8FrequencySeries  *psd,
			   InspiralTemplate      *params, 
			   INT4 length)
{
  REAL8 		m7 = 0.;							/* the moments */
  REAL8 		m5 = 0.;	
  REAL8 		m3 = 0.;
  REAL8 		f;
	  
  UINT4			kMin;
  UINT4		  	kMax;
  UINT4			k;

  InspiralMomentsIn 	in;


  moments->a11.length =  length / 2 ;
  moments->a22.length =  length / 2 ;
  moments->a21.length =  length / 2 ;
  moments->a11.data   = (REAL4*) LALCalloc(1, sizeof(REAL4) * moments->a11.length);
  moments->a22.data   = (REAL4*) LALCalloc(1, sizeof(REAL4) * moments->a22.length);
  moments->a21.data   = (REAL4*) LALCalloc(1, sizeof(REAL4) * moments->a21.length);

    
  /* inspiral structure */
  in.shf 	= psd;  							/* The spectrum 			*/
  in.xmin 	= params->fLower;						/* Lower frequency of integral?	is it correct or should we put zero? 	*/
  in.xmax 	= params->tSampling/2.;						/* We dont' need to cut here		*/
  in.norm 	= 1./4.;							/* Some normalization			*/


  in.norm *= params->tSampling * params->tSampling;

  /* The minimum and maximum frequency */
  kMin = (UINT8) floor( (in.xmin - psd->f0) / psd->deltaF );
  kMax = (UINT8) floor( (in.xmax - psd->f0) / psd->deltaF );
  
  /* Skip frequency less than fLower.*/
  for (k = 0; k < kMin ; k++)
    {
      moments->a11.data[k] = 0.;
      moments->a21.data[k] = 0.;
      moments->a22.data[k] = 0.;
    }
  for ( k = kMin; k < kMax; ++k )
    {
      f = psd->f0 + (REAL8) k * psd->deltaF;

      if (psd->data->data[k])
	{
	  m7 +=  pow( f, -(7./3.) ) / psd->data->data[k] * psd->deltaF / in.norm;
	  m5 +=  pow( f, -(5./3) )  / psd->data->data[k] * psd->deltaF / in.norm;
	  m3 +=  pow( f, -(1.) )    / psd->data->data[k] * psd->deltaF / in.norm;

	  
	  moments->a11.data[k] = 1./sqrt(m7);  
	  moments->a22.data[k] = 1. / sqrt(m3 - m5*m5/m7);
	  moments->a21.data[k] = -m5 / m7 * moments->a22.data[k];

	}
    }
}

  

  

/* ****************************************************************************
 * Create an orthogonal filter. by taking its complex conjuguate
 * ************************************************************************** */
void LALGetOrthogonalFilterBCV2(REAL4Vector *filter)
{

  UINT4   i;
  UINT4	  n 	 = filter->length;
  UINT4   nby2   = filter->length / 2;
  
  REAL4	  temp;
  
  for (i = 1; i < nby2; i++)
    {
      temp 		=  filter->data[i];
      filter->data[i] 	= -filter->data[n-i];
      filter->data[n-i] = temp;
    }
}

  

/* *****************************************************************************
 * Create a vector f^{a/b} 
 * ************************************************************************** */
void 
LALCreateVectorFreqPower(REAL4Vector 		*vector,
			 InspiralTemplate 	params,
			 INT4 			a,
			 INT4 			b)
{

  INT4 	        i;
  INT4		n = vector->length;			/* Length of the vector to create 	*/
	  
  REAL8		power = (REAL8)a / (REAL8)b;		/* the frequency power			*/
  REAL8 	f;					/* La frequence				*/
  REAL8		df = params.tSampling/(REAL8)n/2.;	/* sampling frequency			*/

  /* First value equal to zero */
  vector->data[0] = 0.;
  for( i = 1; i < n; i++)
    {
      f = i * df; 
      /* Probably not efficient but this function is 
       * just called once at the beginning */
      vector->data[i] = pow(f, power);
    }
}


/* *****************************************************************************
 * Create the filters for BCV correlation 					
 * ************************************************************************** */
void 
LALCreateFilters(	REAL4Vector 		*FilterBCV1,
		 	REAL4Vector 		*FilterBCV2,
			BEPowerVector           *powerVector,
		 	BEMoments               *moments,				/* moments 				*/
		 	UINT4 			kMin,				/* position of the integration		*/
			UINT4                   kMax,
		 	double 			psi0,				/* change to real4 or real 8		*/
		 	double 			psi3)				/* idem 				*/
{
  UINT4			i;
  UINT4  		n = FilterBCV1->length;
  UINT4	  		nby2 = FilterBCV1->length / 2;
	  
  REAL8 		amplitude;
  REAL8	  		cos_phase;
  REAL8	  		sin_phase;

  REAL4 a11,a21,a22;


  a11 = moments->a11.data[kMax];
  a22 = moments->a22.data[kMax];
  a21 = moments->a21.data[kMax];



  /* Create the templates */	       
  for (i = 0; i< kMin-1; i++)
    {
      FilterBCV1->data[i] = 0;
      FilterBCV2->data[i] = 0;
    }
  /* Should add the upper fendBCV here as well */

  for (i = kMin; i< nby2; i++)
    {
      amplitude  =   psi0 * powerVector->fm5_3.data[i]  				/* The same for the two filters 	*/
	+ psi3 * powerVector->fm2_3.data[i];
      
      cos_phase  = cos(amplitude);						/* The same for the two filters		*/
      sin_phase  = sin(amplitude);
      

      /* Fill the first filter here */
      amplitude  = a11 * powerVector->fm7_6.data[i];
      
      FilterBCV1->data[i]   =  amplitude * cos_phase;      
      FilterBCV1->data[n-i] = -amplitude * sin_phase;

      /* Fill the second one here */
      amplitude =  a21 * powerVector->fm7_6.data[i] + 
	a22 * powerVector->fm1_2.data[i];
      
      FilterBCV2->data[i]   =  amplitude * cos_phase;      
      FilterBCV2->data[n-i] = -amplitude * sin_phase;
    }  
}





/*  ****************************************************************************
 *  The Overlap function for BCV templates 
 *  ***************************************************************************/
void
LALWaveOverlapBCV(	     LALStatus               *status,
			     REAL4Vector             *correlation,
			     InspiralWaveOverlapIn   *overlapin,
			     REAL4Vector             *FilterBCV1,
			     REAL4Vector             *FilterBCV2,
			     OtherParamIn             otherIn ,
			     OverlapOutputIn          *OverlapOutput, 
			     BEMoments *moments
		  	     )
     /*  </lalVerbatim>  */
{
 
  REAL4   rhoMaxConstraint=0   ,   alphaConstraint=0,   phaseConstraint=0;
  REAL4   rhoMaxUnconstraint=0;
  INT4    rhoBinUnconstraint=0, rhoBinConstraint=0;
  REAL4   alphaUnconstraint=0,   phaseUnconstraint=0;
  REAL4   rhoConstraint=0, rhoUnconstraint=0;

  REAL4 df;

  
  REAL4   BestPhase=0,thetab,thetav=0, alphaMax, a11,a22,a21, 
	  x1_2, x2_2, x3_2, x4_2, 						/* some temporary data 	      		*/
	  V0, V1, V2, 								/* idem 				*/
	  rho		= 0,							/* the SNR 				*/
           alpha;
  
  REAL4Vector 
    template, 
    x1, x2, x3, x4,
    phaseV,
    rho1, rho2, rho3, 
    v0, v1, v2;					/* output of the correlations 		*/

  InspiralWaveCorrelateIn 
	  corrin;								/* the correlation input structure 	*/
  UINT4 k,
	  i, 									
	  nBegin, 								/* beginning of valid correlation 	*/
	  nEnd, 								/* end of valid correlation 		*/
	  n = correlation->length; 						/* length of the vectors		*/
  REAL4 phi, phase, cphase, sphase, cphi, sphi;
  FILE *Foutput;

	  
  INITSTATUS (status, "LALWaveOverlapBCV", BANKEFFICIENCYC);
  ATTATCHSTATUSPTR(status);
  
  /* size affectation*/
  x1.length = x2.length = x3.length = x4.length = 
  v1.length = v2.length = v0.length =
    phaseV.length =  rho1.length= rho2.length = rho3.length  =template.length= n;
  
  /* memory allocation*/
  x1.data = (REAL4*) LALMalloc(sizeof(REAL4) * x1.length);
  x2.data = (REAL4*) LALMalloc(sizeof(REAL4) * x2.length);
  x3.data = (REAL4*) LALMalloc(sizeof(REAL4) * x3.length);
  x4.data = (REAL4*) LALMalloc(sizeof(REAL4) * x4.length);

  if (otherIn.PrintBestOverlap && otherIn.extraFinalPrinting){
    phaseV.data = (REAL4*) LALMalloc(sizeof(REAL4) * phaseV.length);
    template.data = (REAL4*) LALMalloc(sizeof(REAL4) * template.length);
    rho1.data = (REAL4*) LALMalloc(sizeof(REAL4) * rho1.length);
    rho2.data = (REAL4*) LALMalloc(sizeof(REAL4) * rho2.length);
    rho3.data = (REAL4*) LALMalloc(sizeof(REAL4) * rho3.length);
    v1.data = (REAL4*) LALMalloc(sizeof(REAL4) * v1.length);
    v2.data = (REAL4*) LALMalloc(sizeof(REAL4) * v2.length);
    v0.data = (REAL4*) LALMalloc(sizeof(REAL4) * v0.length);
  }
  /**/
  overlapin->param.nStartPad 	= 0;
  overlapin->param.startPhase 	= 0;
  
  /*  We want to compute <x|h1> and <x|h2>; let's prepare the correlation  
   *  process; let's get the input data namely x and the spectrum namely psd 
   *  */
  corrin.fCutoff        = overlapin->param.fFinal;				/* the integral's cutoff frequency 	*/
  corrin.samplingRate   = overlapin->param.tSampling;				/* the sampling 			*/
  corrin.df             = overlapin->param.tSampling / 
	  		(REAL8) overlapin->signal.length;			/* the resolution in frequency 		*/
  corrin.psd            = overlapin->psd;					/* the spectrum 			*/
  corrin.signal1        = overlapin->signal;					/* the signal x				*/
  corrin.revp           = overlapin->revp;					/* fourier transform flag		*/
  
  /* Filter h1 and its orthonormal h1* */  
  corrin.signal2        = *FilterBCV1;						/* The first template 			*/
  LALInspiralWaveCorrelate(status->statusPtr, &x1, corrin);			/* the correlation			*/
  CHECKSTATUSPTR(status);							
  LALGetOrthogonalFilterBCV2(FilterBCV1);						/* get the orthonormalized template     */
  corrin.signal2        = *FilterBCV1;						/* the second template 		*/
  LALInspiralWaveCorrelate(status->statusPtr, &x3, corrin);			/* the correlation 			*/
  CHECKSTATUSPTR(status);
  
  /* Filter h2 and its orthornormal filter */
  corrin.signal2        = *FilterBCV2;						/* The first template			*/
  LALInspiralWaveCorrelate(status->statusPtr, &x2, corrin);			/* the correlation 			*/
  CHECKSTATUSPTR(status);		
  LALGetOrthogonalFilterBCV2( FilterBCV2);						/* get the orthonormailzed templates 	*/
  corrin.signal2        = *FilterBCV2;						/* the second template			*/
  LALInspiralWaveCorrelate(status->statusPtr, &x4, corrin);			/* the correlation 			*/
  
  /* nbegin and end for the correlation processus 
   * as well as overlapout affectation */
  nBegin                = overlapin->nBegin;					/* starting bin 			*/
  nEnd              	= FilterBCV1->length - overlapin->nEnd;			/* ending valid bin 			*/


  /* some output for debugging, checking ... */
  if (otherIn.PrintBestOverlap && otherIn.extraFinalPrinting)
    {
      Foutput = fopen( "BE_Filter.dat", "w");
      for (i=0; i< x1.length; i++)
	fprintf(Foutput, "%e %e\n", i/corrin.samplingRate,fabs(x1.data[i]));
      fprintf(Foutput,"&\n");	       
      
      for (i=1; i< FilterBCV1->length; i++)
	fprintf(Foutput, "%e %e\n", i/corrin.samplingRate,fabs(x2.data[i]));
      fprintf(Foutput,"&\n");	       
      
      for (i=0; i< FilterBCV1->length; i++)
	fprintf(Foutput, "%e %e\n", i/corrin.samplingRate,fabs(x3.data[i]));
      fprintf(Foutput,"&\n");	       
      
      for (i=0; i< FilterBCV1->length-1; i++)
	fprintf(Foutput, "%e %e\n", i/corrin.samplingRate ,fabs(x4.data[i]));
      fprintf(Foutput,"&\n");	 
      fclose(Foutput);      
    }

  df 	= overlapin->param.tSampling / (REAL8)(n/2.) / 2.;      
  k = floor(overlapin->param.fFinal / df);

  a11 = moments->a11.data[k];
  a22 = moments->a22.data[k];
  a21 = moments->a21.data[k];


  alphaMax = pow(corrin.fCutoff, -2./3.);



  thetab = (-(a11 * alphaMax)/(a22+a21*alphaMax));
  thetab = atan(thetab);
  rho    = 0.;
  
  if(otherIn.PrintBestOverlap && otherIn.extraFinalPrinting){
  fprintf(stderr,"theta_b = %e a11=%e a21=%e a22=%e alphaMax = %e fCutoff=%e\n",
	  thetab,  a11, a21, a22, alphaMax, corrin.fCutoff);

  }





  /* Once we have the 4 correlations, we can search for the
   * maximum of correlation and compute the final SNR */
  for (i = nBegin; i < nEnd; i++) 
    {
      x1_2 = x1.data[i] * x1.data[i];
      x2_2 = x2.data[i] * x2.data[i];
      x3_2 = x3.data[i] * x3.data[i];
      x4_2 = x4.data[i] * x4.data[i];      
      
      V0 = x1_2 + x2_2 + x3_2 + x4_2;
      V1 = x1_2 + x3_2 - x2_2 - x4_2;
      V2 = 2*(x1.data[i]*x2.data[i] + x3.data[i]*x4.data[i]);
      
      rhoUnconstraint = sqrt((V0 + sqrt(V1*V1+V2*V2))/2.);
      

      /* get thetav first in order to constraint alpha_F*/

      thetav =   atan2(V2,V1);
	
      if(otherIn.PrintBestOverlap && otherIn.extraFinalPrinting){
	rho1.data[i] = rhoUnconstraint;
	rho2.data[i] = sqrt((V0 + V1)/2.);
	rho3.data[i] = sqrt((V0+V1*cos(2*thetab)+V2*sin(2*thetab))/2.);
	v0.data[i]   = V0;
	v1.data[i]   = V1;
	v2.data[i]   = V2;
        phaseV.data[i] = .5*thetav; /*used to compute alpha*/
      }
             

      if (thetab >= 0){
	if ( 0  <= thetav && thetav <= 2 * thetab){	  
	  rhoConstraint = rhoUnconstraint;
	}
	else if (thetab-LAL_PI <= thetav && thetav < 0) {
	  rhoConstraint = sqrt((V0 + V1)/2.);	  
	}
	else if( (2*thetab  < thetav && thetav<=LAL_PI+1e-4 )
		 || (-LAL_PI-1e-4<=thetav && thetav < -LAL_PI+thetab)){
	  rhoConstraint =sqrt((V0+V1*cos(2*thetab)+V2*sin(2*thetab))/2.);;
	}
	else  if ( 0  <= thetav && thetav <= 2 * thetab)
	  {	
	    rhoConstraint = rhoUnconstraint;
	  }
	else
	  {
	    fprintf(stderr,"must not enter here  thetav = %e thetab=%e\n ", thetav , thetab);
	    exit(0);
	  }
      }
      else{
	if ( 2*thetab  <= thetav && thetav <= 0){	  
	  rhoConstraint = rhoUnconstraint;
	}
	else if (0 < thetav &&  thetav  <= LAL_PI + thetab ) {
	  rhoConstraint = sqrt((V0 + V1)/2.);	  
	}
	else if( (-LAL_PI-1e-4  <= thetav && thetav < 2*thetab ) || 
		 (LAL_PI +thetab <= thetav && thetav <= LAL_PI+1e-4))
{
	  rhoConstraint =sqrt((V0+V1*cos(2*thetab)+V2*sin(2*thetab))/2.);;
	}
	else 
	  {
	    fprintf(stderr,"must not enter herethetav = %e thetab=%e %e %e %d\n ",thetav , thetab, V1, V2, i);
	    exit(0);
	  }
      }
    
  
      if (otherIn.alphaFConstraint == ALPHAFConstraint){
  
	if(otherIn.PrintBestOverlap || otherIn.PrintSNRHisto) {
	  correlation->data[i] = rhoConstraint;
	}
      }
      else 
	{
	  if(otherIn.PrintBestOverlap || otherIn.PrintSNRHisto) {
	    correlation->data[i] = rhoUnconstraint;
	  }
	}
      

      if ( rhoConstraint > rhoMaxConstraint)			
	/* Keep the position of the max only	*/
	{
	  rhoMaxConstraint 	  = rhoConstraint;
	  rhoBinConstraint 	  = i;
	  phaseConstraint   =    .5*thetav;
	  alphaConstraint = -(a22 * tan(phaseConstraint)) 				
	    / (a11 + a21* tan(phaseConstraint));
	}

      if ( rhoUnconstraint > rhoMaxUnconstraint)			
	/* Keep the position of the max only	*/
	{
	  rhoMaxUnconstraint 	= rhoUnconstraint  ;
	  rhoBinUnconstraint 	= i;
	  phaseUnconstraint   = .5*thetav ; 
	  alphaUnconstraint = -(a22 * tan(phaseUnconstraint)) 				
	    / (a11 + a21* tan(phaseUnconstraint));
	}


	
    }

  
  /* Finally get the alpha value corresponding to the best rho */ 


 
  OverlapOutput->rhoMaxConstraint     = rhoMaxConstraint;
  OverlapOutput->rhoBinConstraint     = rhoBinConstraint;
  OverlapOutput->alphaConstraint      = alphaConstraint;
  OverlapOutput->phaseConstraint      = phaseConstraint;
  OverlapOutput->rhoMaxUnconstraint   = rhoMaxUnconstraint;
  OverlapOutput->rhoBinUnconstraint   = rhoBinUnconstraint;
  OverlapOutput->alphaUnconstraint    = alphaUnconstraint;
  OverlapOutput->phaseUnconstraint    = phaseUnconstraint;



  /* The final template to be print ? */
  if (otherIn.PrintBestOverlap  && otherIn.extraFinalPrinting){

    /*print overlap, combinaison of the x_i filters*/
    Foutput=fopen("BE_Phase.dat","w");
    for (i=0; i< phaseV.length; i++){
      fprintf(Foutput, "%e\n",  atan2(v2.data[i],v1.data[i]));
    }
    fclose(Foutput);
    

  Foutput=fopen("BE_rho1.dat","w");
    for (i=0; i< phaseV.length; i++){
      fprintf(Foutput, "%e\n", rho1.data[i]);
    }
    fclose(Foutput);
    

  Foutput=fopen("BE_rho2.dat","w");
    for (i=0; i< phaseV.length; i++){
      fprintf(Foutput, "%e\n", rho2.data[i]);
    }
    fclose(Foutput);
    

    Foutput=fopen("BE_rho3.dat","w");
    for (i=0; i< phaseV.length; i++){
      fprintf(Foutput, "%e\n",rho3.data[i]);
    }
    fclose(Foutput);
     
    Foutput=fopen("BE_v0.dat","w");
    for (i=0; i< phaseV.length; i++){
      fprintf(Foutput, "%e\n",v0.data[i]);
    }
    fclose(Foutput);

    Foutput=fopen("BE_v1.dat","w");
    for (i=0; i< phaseV.length; i++){
      fprintf(Foutput, "%e\n", v1.data[i]);
    }
    fclose(Foutput); 

    Foutput=fopen("BE_v2.dat","w");
    for (i=0; i< phaseV.length; i++){
      fprintf(Foutput, "%e\n", v2.data[i]);
    }
    fclose(Foutput);
    
    Foutput=fopen("BE_alpha.dat","w");
    for (i=0; i< correlation->length; i++){
      alpha = -(a22 * tan(phaseV.data[i])) 				/* Compute the final alpha parameter 	*/
	/ (a11 + a21* tan(phaseV.data[i]));
      fprintf(Foutput, "%e \n",alpha*pow(corrin.fCutoff,2./3.));
    }
    fclose(Foutput);

    /*print overlap, combinaison of the x_i filters*/
    Foutput=fopen("BE_Overlap.dat","w");
    for (i=0; i< correlation->length; i++){
      fprintf(Foutput, "%e\n", fabs(correlation->data[i]));
    }
    fclose(Foutput);
  }


    if (otherIn.extraFinalPrinting && otherIn.PrintBestOverlap){
    /* print the final template given phase, alpha ans so on*/
    
    phase 		= BestPhase;
    phi   		= overlapin->param.startPhase; /*how to get it ohterwise ? */
   
    for (i=0; i<(UINT4)correlation->length; i++) 
    {
      cphase = cos(phase);
      cphi   = cos(phi);
      sphase = sqrt(1-cphase*cphase);
      sphi   = sqrt(1-cphi*cphi);
      
      correlation->data[i] = x1.data[i] * cphase * cphi
	+ x2.data[i] * sphase * cphi
	+ x3.data[i] * cphase * sphi
	+ x4.data[i] * sphase * sphi;	
    }

    Foutput=fopen("BE_Correlation.dat", "w");
    for (i=0; i< correlation->length; i++){
      fprintf(Foutput, "%e %e\n", i/corrin.samplingRate,fabs(correlation->data[i]));
    }
    fclose(Foutput);


    LALGetOrthogonalFilterBCV2(FilterBCV1);						/* get the orthonormalized template     */
    corrin.signal2        = *FilterBCV1;

    for (i=0; i<(UINT4)correlation->length; i++) 
    { 
      cphase = cos(phaseV.data[i]);
      cphi   = cos(phi);
      sphase = sqrt(1-cphase*cphase);
      sphi   = sqrt(1-cphi*cphi);
      
      template.data[i]=FilterBCV1->data[i]*cphase*cphi; 
      template.data[i]+=corrin.signal2.data[i] * sphase*cphi; 

	
    }

    LALGetOrthogonalFilterBCV2(FilterBCV2);						/* get the orthonormalized template     */
    corrin.signal2        = *FilterBCV2;

    for (i=0; i<(UINT4)correlation->length; i++) 
    { 
      cphase = cos(phaseV.data[i]);
      cphi   = cos(phi);
      sphase = sqrt(1-cphase*cphase);
      sphi   = sqrt(1-cphi*cphi);
      
      template.data[i]=FilterBCV2->data[i]*sphase*cphi; 
      template.data[i]+=corrin.signal2.data[i]*sphase*sphi; 

	
    }

    Foutput=fopen("BE_BestTemplate.dat", "w");
    for (i=0; i< correlation->length; i++){
      fprintf(Foutput, "%e %e\n", i/corrin.samplingRate,template.data[i]);
    }
    fclose(Foutput);



  }/*end if printTemplate*/

  /* Free memory */
  LALFree(x1.data);
  LALFree(x2.data);
  LALFree(x3.data);
  LALFree(x4.data);

  if (otherIn.extraFinalPrinting && otherIn.PrintBestOverlap){
    LALFree(phaseV.data);
    LALFree(template.data);
    LALFree(rho1.data);
    LALFree(rho2.data);
    LALFree(rho3.data);
    LALFree(v0.data);
    LALFree(v1.data);
    LALFree(v2.data);
  }

  DETATCHSTATUSPTR(status);
  RETURN(status);
}


/* *****************************************************************************
 *  A draft function to finalized later ; purpose: print the bank 
 *  ****************************************************************************/
void
BEPrintBank(	InspiralCoarseBankIn coarseBankIn,
		InspiralTemplateList **list,
      		UINT4 sizeBank
		) 
{
  UINT4 	i;

  FILE 		*output;


  output = fopen(BANKEFFICIENCY_PRINTBANK_FILEASCII,"w");

  fprintf(output,"#Number of Coarse Bank Templates=%d\n",sizeBank);
  if (coarseBankIn.approximant == BCV)
    {
      fprintf(output, "#psi0Min=%e, psi0Max=%e, psi3Min=%e, psi3Max=%e\n", 
	      coarseBankIn.psi0Min, coarseBankIn.psi0Max, coarseBankIn.psi3Min, coarseBankIn.psi3Max);
      fprintf(output, "#psi0 psi3 nLayer totalMass fFinal\n");
      
    }
  else
    {
      fprintf(output, "#mMin=%e, mMax=%e\n", coarseBankIn.mMin,coarseBankIn.mMax);
      fprintf(output, "#tau0, tau3, mass1, mass2\n");
    }

  for ( i = 0; i < sizeBank; i++)
    {
      if (coarseBankIn.approximant == BCV)
	{      		   
      		fprintf(output, "%e %e %d %e %e\n", 	(*list)[i].params.psi0, 
				  			(*list)[i].params.psi3,
				  			(*list)[i].nLayer, 
							(*list)[i].params.totalMass,
						       	(*list)[i].params.fFinal);
	} 
      else
	{

      		fprintf(output, "%e %e %e %e\n", 	(*list)[i].params.t0, 
				  			(*list)[i].params.t3, 
							(*list)[i].params.mass1, 
							(*list)[i].params.mass2);
	}
    }	   
  fprintf(output,"&\n");
  fclose(output);
}

/* personal function. Should not be used. output hte bank  overlaps in a binary
 * file to be used with DES3.
 * */

void
PrintBankOverlap(
		 InspiralTemplateList 	**list,
		 int 			sizeBank,
		 float 			*overlap,
		 InspiralCoarseBankIn 	coarseBankIn
		)
{


/*to avoid warnings */
int temp, temp3;
float temp2,temp1;

temp = sizeBank;
temp1=*overlap;
temp2 = (*list)[0].metric.g00;
temp3 = coarseBankIn.numFcutTemplates;
}

#if 0
FILE 		*output1;
  FILE 		*output2;
  
  long Npsi0, Npsi3;

  double dx0, dx3;
  double psi0Min = coarseBankIn.psi0Min;
  double psi0Max = coarseBankIn.psi0Max;
  double psi3Min = coarseBankIn.psi3Min;
  double psi3Max = coarseBankIn.psi3Max;
  double psi0, psi3;

  int    numfcut = coarseBankIn.numFcutTemplates;
  int    i,j,l,n;

  float  *a;

  double minimalMatch = coarseBankIn.mmCoarse;
  double theta, myphi, fac;


  output1 = fopen("FF.sr4", "a+");
  output2 = fopen("FF.dim", "w");
   dx0 = sqrt(2.L * (1.L - minimalMatch)/(*list)[0].metric.g00 );
   dx3 = sqrt(2.L * (1.L - minimalMatch)/(*list)[0].metric.g11 );


   fprintf(stderr, "%f %f\n", dx0, dx3);
   
     if ((*list)[0].metric.theta!=0.L)
   {
     myphi = atan2(dx3, dx0);
     theta = fabs((*list)[0].metric.theta);
     if (theta <= myphi) {
       fac = cos(theta);
       dx0  /= fac;
       dx3 *=  fac;
     } 
     else {
       fac = sin(theta);
       dx0 = dx3 / fac;
       dx3 = dx0 * fac;
     }
   }
   
     fprintf(stderr, "%f %f\n", dx0, dx3);
     fprintf(stderr,"Approxi= %d\n", coarseBankIn.approximant); 
     a =( float*)malloc(sizeof(float) ); 

     switch( coarseBankIn.approximant ){
	case BCV: 
       /*some data*/
       
       
       Npsi0 = floor(( psi0Max - psi0Min ) / dx0) + 1 ;
       Npsi3 = floor(-( psi3Min - psi3Max ) / dx3) + 1;

       printf("%f %f %ld %ld\n", dx0, dx3, Npsi0, Npsi3);     
       
/*	Npsi0 = 56; */
	Npsi3 = sizeBank/Npsi0/5;  
	dx0  =  ( psi0Max - psi0Min)/(Npsi0);
	dx3  =  ( psi3Max - psi3Min)/(Npsi3);
	
       printf("%f %f %ld %ld\n", dx0, dx3, Npsi0, Npsi3);     
       fflush(stdout); 
       /* The dimension file */
       fprintf(output2,"%7ld %14.8f %14.8f  %s\n",Npsi3, -psi3Max+dx3, dx3,"psi3");
       fprintf(output2,"%7ld %14.8f %14.8f  %s\n",Npsi0, psi0Min+dx0, dx0,"psi0");
       fprintf(output2,"%7d %14.8f %14.8f  %s\n",numfcut,1.,1.,"layers");
       
       fclose(output2);
       float tab[Npsi0][Npsi3][numfcut];
       for (l = 0; l < numfcut; l++){
	 for (i = 0; i < Npsi0; i++){
	   for (j = 0; j < Npsi3; j++){
	     tab[i][j][l]=0;
	   }
	 }
       }
       n = 0;
       /*fill to zero */
       while (n< sizeBank)
	 {
	   psi0 = (*list)[n].params.psi0;
	   psi3 = (*list)[n].params.psi3;
	   
	   i = (int)(( psi0 - psi0Min ) / dx0);
           j = (int)(-( psi3 - psi3Max) / dx3);;
	 if (i > Npsi0 || (long)j > Npsi3 || (*list)[n].nLayer > (unsigned int)numfcut)
		fprintf(stderr," %d %d %d %f %f \n", n, i, j, psi0, psi3); 
	  
  	  tab[i][j][(*list)[n].nLayer-1] = overlap[n];
	  n++;
	}
/*fprintf(stderr, "ok %d %d %d\n",numfcut, Npsi0, Npsi3);*/
      for (l = 1; l <= numfcut; l++){
       for (i = 0; i < Npsi0; i++){
	for (j = 0; j < Npsi3; j++){
/*        fprintf(stderr,"%ld %ld %ld %lf\n", i , j , l , tab[i][j][l]); */
       if (tab[i][j][l-1]!=0)
	a[0] = tab[i][j][l-1];	
	else a[0] = 0;
/*fprintf(stderr,"#%d %d %d %lf\n",i, j, l-1, a[0]);	*/
	fwrite(a,4,1,output1);
	 }
	}	 	
      }
      
    fclose(output1);
      break;
    case BCVSpin: 
    case SpinTaylorT3: 
    case TaylorT1: 
    case TaylorT2: 
    case TaylorT3: 
    case TaylorF1: 
    case TaylorF2: 
    case PadeT1: 
    case PadeF1: 
    case EOB:
    case SpinTaylor: 
      break;      
  }

}
#endif





/* routine to print the bank in a xml file*/
/* should add the status variable since we call sone lal functions here 
 * this routine is not well commented and won't be in close future.
 * */
/* !! tau2 is used to store the Layer number */
void 
BEPrintBankXml(
	       InspiralTemplateList *coarseList, 
	       UINT4 numCoarse,
	       InspiralCoarseBankIn   coarseBankIn,
	       RandomInspiralSignalIn randIn,
	       OtherParamIn           otherIn)
{
#define MAXIFO 2
  LALStatus             status = blank_status;
  MetadataTable         proctable;

  MetadataTable         templateBank;
  SnglInspiralTable     *tmplt  = NULL;
  CHAR  ifo[3];                           /* two character ifo code       */
  CHAR *channelName = NULL;               /* channel string               */
  UINT4 	i;  
  LIGOLwXMLStream       xmlStream;
  CHAR  fname[256];
  LIGOTimeGPS gpsStartTime 	= { 0, 0 };    /* input data GPS start time    */
  LIGOTimeGPS gpsEndTime 	= { 0, 0 };      /* input data GPS end time      */
  LALLeapSecAccuracy    accuracy = 1;
  CHAR  comment[LIGOMETA_COMMENT_MAX];
  CHAR  ifoName[MAXIFO][LIGOMETA_IFO_MAX];

  MetadataTable         processParamsTable;
  ProcessParamsTable   *this_proc_param = NULL;

  
  strncpy( ifoName[0], "no", LIGOMETA_IFO_MAX * sizeof(CHAR) );
  strncpy( ifoName[1], "ne", LIGOMETA_IFO_MAX * sizeof(CHAR) );
  memset( ifo, 0, sizeof(ifo) );
  memcpy( ifo, "MC", sizeof(ifo) - 1 );


  /* --- first we create the filename --- */
  LALSnprintf( fname, sizeof(fname), BANKEFFICIENCY_PRINTBANK_FILEXML ,
	       ifo, gpsStartTime.gpsSeconds,
	       gpsEndTime.gpsSeconds - gpsStartTime.gpsSeconds );
  
  /* --- let's save the bank itself --- */
  templateBank.snglInspiralTable = NULL;

  

  /* --- convert the templates to sngl_inspiral structures 
     in order to  write results in the XML file --- */
  if ( numCoarse )
    {
      /* --- we sae one line --- */
      tmplt = templateBank.snglInspiralTable = (SnglInspiralTable *)
	LALCalloc( 1, sizeof(SnglInspiralTable) );
      LALSnprintf( tmplt->ifo, LIGOMETA_IFO_MAX * sizeof(CHAR), ifo );
      LALSnprintf( tmplt->search, LIGOMETA_SEARCH_MAX * sizeof(CHAR), 
		   "BankEfficiency" );
      LALSnprintf( tmplt->channel, LIGOMETA_CHANNEL_MAX * sizeof(CHAR),
		   channelName );
      tmplt->mass1   = (REAL4) coarseList[0].params.mass1;
      tmplt->mass2   = (REAL4) coarseList[0].params.mass2;
      tmplt->mchirp  = (REAL4) coarseList[0].params.chirpMass;
      tmplt->eta     = (REAL4) coarseList[0].params.eta;
      tmplt->tau0    = (REAL4) coarseList[0].params.t0;
      tmplt->tau2    = (REAL4) coarseList[0].nLayer;
      tmplt->tau3    = (REAL4) coarseList[0].params.t3;
      tmplt->tau4    = (REAL4) coarseList[0].params.t4;
      tmplt->tau5    = (REAL4) coarseList[0].params.t5;
      tmplt->ttotal  = (REAL4) coarseList[0].params.tC;
      tmplt->psi0    = (REAL4) coarseList[0].params.psi0;
      tmplt->psi3    = (REAL4) coarseList[0].params.psi3;
      tmplt->f_final = (REAL4) coarseList[0].params.fFinal;
      
      /* --- then the others --- */
      for ( i = 1; i < numCoarse; ++i )
	{
	  tmplt = tmplt->next = (SnglInspiralTable *)
	    LALCalloc( 1, sizeof(SnglInspiralTable) );
	  LALSnprintf( tmplt->ifo, LIGOMETA_IFO_MAX * sizeof(CHAR), ifo );
	  LALSnprintf( tmplt->search, LIGOMETA_SEARCH_MAX * sizeof(CHAR), 
		       "BankEfficiency" );
	  LALSnprintf( tmplt->channel, LIGOMETA_CHANNEL_MAX * sizeof(CHAR),
		       channelName );
	  tmplt->mass1   = (REAL4) coarseList[i].params.mass1;
	  tmplt->mass2   = (REAL4) coarseList[i].params.mass2;
	  tmplt->mchirp  = (REAL4) coarseList[i].params.chirpMass;
	  tmplt->eta     = (REAL4) coarseList[i].params.eta;
	  tmplt->tau0    = (REAL4) coarseList[i].params.t0;
	  tmplt->tau2    = (REAL4) coarseList[i].nLayer;
	  tmplt->tau3    = (REAL4) coarseList[i].params.t3;
	  tmplt->tau4    = (REAL4) coarseList[i].params.t4;
	  tmplt->tau5    = (REAL4) coarseList[i].params.t5;
	  tmplt->ttotal  = (REAL4) coarseList[i].params.tC;
	  tmplt->psi0    = (REAL4) coarseList[i].params.psi0;
	  tmplt->psi3    = (REAL4) coarseList[i].params.psi3;
	  tmplt->f_final = (REAL4) coarseList[i].params.fFinal;
	}
    }
  
  /* -- we start to fill the xml file here --- */
  memset( &xmlStream, 0, sizeof(LIGOLwXMLStream) );
  LAL_CALL( LALOpenLIGOLwXMLFile( &status, &xmlStream, fname), &status );
   

  /* create the process and process params tables */
  proctable.processTable = (ProcessTable *) calloc( 1, sizeof(ProcessTable) );
  LAL_CALL( LALGPSTimeNow ( &status, &(proctable.processTable->start_time),
			    &accuracy ), &status );
  LAL_CALL( populate_process_table( &status, proctable.processTable, 
				    PROGRAM_NAME, CVS_REVISION, CVS_SOURCE, CVS_DATE ), &status );
  this_proc_param = processParamsTable.processParamsTable = 
    (ProcessParamsTable *) calloc( 1, sizeof(ProcessParamsTable) );

  BEFillProc(this_proc_param, coarseBankIn, randIn, otherIn);




  memset( comment, 0, LIGOMETA_COMMENT_MAX * sizeof(CHAR) );



  /* write process table */
  LALSnprintf( proctable.processTable->ifos, LIGOMETA_IFOS_MAX, "%s%s", 
	       ifoName[0], ifoName[1] );
  LAL_CALL( LALGPSTimeNow ( &status, &(proctable.processTable->end_time),
			    &accuracy ), &status );
  
  LAL_CALL( LALBeginLIGOLwXMLTable( &status, &xmlStream, process_table ), 
	    &status );
  LAL_CALL( LALWriteLIGOLwXMLTable( &status, &xmlStream, proctable, 
				    process_table ), &status );
  LAL_CALL( LALEndLIGOLwXMLTable ( &status, &xmlStream ), &status );
  
  /* write process params table */
  LAL_CALL( LALBeginLIGOLwXMLTable( &status, &xmlStream, 
				    process_params_table ), &status );
  LAL_CALL( LALWriteLIGOLwXMLTable( &status, &xmlStream, processParamsTable, 
				    process_params_table ), &status );
  LAL_CALL( LALEndLIGOLwXMLTable ( &status, &xmlStream ), &status );
  
  /* finally write sngl inspiral table */
  if ( templateBank.snglInspiralTable )
    {
      LAL_CALL( LALBeginLIGOLwXMLTable( &status, &xmlStream, sngl_inspiral_table ), 
		&status );
      LAL_CALL( LALWriteLIGOLwXMLTable( &status, &xmlStream, templateBank, 
					sngl_inspiral_table ), &status );
      LAL_CALL( LALEndLIGOLwXMLTable ( &status, &xmlStream ), &status );
    }


  /* --- desallocate memory here --- */
  while ( templateBank.snglInspiralTable )
    {
      tmplt = templateBank.snglInspiralTable;
      templateBank.snglInspiralTable = templateBank.snglInspiralTable->next;
      LALFree( tmplt );
    }
  
  /* close the output xml file */
  LAL_CALL( LALCloseLIGOLwXMLFile ( &status, &xmlStream ), &status );
}





void 
BEFillProc(
	       ProcessParamsTable     *this_proc_param,
	       InspiralCoarseBankIn   coarseBankIn,
	       RandomInspiralSignalIn randIn,
	       OtherParamIn           otherIn)
{


#define ADD_PROCESS_PARAM( pptype, format, ppname, ppvalue ) \
this_proc_param = this_proc_param->next = (ProcessParamsTable *) \
  calloc( 1, sizeof(ProcessParamsTable) ); \
  LALSnprintf( this_proc_param->program, LIGOMETA_PROGRAM_MAX, "%s", PROGRAM_NAME ); \
  LALSnprintf( this_proc_param->param,   LIGOMETA_PARAM_MAX,   "%-20s", ppname); \
  LALSnprintf( this_proc_param->type,    LIGOMETA_TYPE_MAX,    "%-10s",   pptype ); \
  LALSnprintf( this_proc_param->value,   LIGOMETA_VALUE_MAX,   format, ppvalue );



  ADD_PROCESS_PARAM("float",	"%12.5f",	"--bank-alpha",	coarseBankIn.alpha);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--bank-mass-range",	coarseBankIn.mMin);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--bank-mass-range",	coarseBankIn.mMax);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--bank-psi0-range",	coarseBankIn.psi0Min);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--bank-psi0-range",	coarseBankIn.psi0Max);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--bank-psi3-range",	coarseBankIn.psi3Min);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--bank-psi3-range",	coarseBankIn.psi3Max);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--bank-ffinal",	coarseBankIn.fUpper);
  ADD_PROCESS_PARAM("float",	"%12.5d",        "--bank-grid-type",	coarseBankIn.gridType);

  ADD_PROCESS_PARAM("float",	"%12.5f",	"--fl",		coarseBankIn.fLower);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--fendbcv",		coarseBankIn.LowGM);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--fendbcv",		coarseBankIn.HighGM);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--fl-signal",	randIn.param.fLower);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--fl-template",      coarseBankIn.fLower);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--freq-moment-bank",	coarseBankIn.fUpper);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--mass-range-bank",	coarseBankIn.mMin);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--mass-range-bank",	coarseBankIn.mMax);
  ADD_PROCESS_PARAM("float",	"%12.5f",       "--max-totalmass",    otherIn.maxTotalMass);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--m1",		otherIn.m1);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--m2",		otherIn.m2);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--mm",		coarseBankIn.mmCoarse);
  ADD_PROCESS_PARAM("int",	"%6d",		"--ntrial",		otherIn.ntrials);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--noise-amplitude",	randIn.NoiseAmp);
  ADD_PROCESS_PARAM("int",	"%6d",		"--number-fcut",	coarseBankIn.numFcutTemplates);
  ADD_PROCESS_PARAM("int",	"%6d",		"--numSeconds",	otherIn.numSeconds);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--psi0",		otherIn.psi0);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--psi3",		otherIn.psi3);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--sampling",		coarseBankIn.tSampling);
  ADD_PROCESS_PARAM("int",	"%6d",		"--simulation-type",	randIn.type);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-amplitude",	randIn.SignalAmp);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-alpha",	randIn.param.alpha);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-ffinal",	otherIn.signalfFinal);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-mass-range",randIn.mMin);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-mass-range",randIn.mMax);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-psi0-range",randIn.psi0Min);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-psi0-range",randIn.psi0Max);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-psi3-range",randIn.psi3Min);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--signal-psi3-range",randIn.psi3Max);
  ADD_PROCESS_PARAM("int",	"%6d",		"--seed",		randIn.useed);
  switch (otherIn.NoiseModel){
  case LIGOI:
    ADD_PROCESS_PARAM("string",	"%6s",	"--noisemodel","LIGOI");
    break;
  case LIGOA:
    ADD_PROCESS_PARAM("string",	"%6s",	"--noisemodel", "LIGOA");
    break;
  case VIRGO:
    ADD_PROCESS_PARAM("string",	"%6s",	"--noisemodel", "VIRGO");
    break;
  case GEO:
    ADD_PROCESS_PARAM("string",	"%6s",	"--noisemodel","GEO");
    break;
  case TAMA:    
    ADD_PROCESS_PARAM("string",	"%6s",	"--noisemodel","TAMA");    
    break; 
  case UNITY:    
    ADD_PROCESS_PARAM("string",	"%6s",	"--noisemodel","UNITY");    
    break;
  case REALPSD:
    ADD_PROCESS_PARAM("string", "%6s",	"--noisemodel","REALPSD");    
     switch (otherIn.detector){
     case L1: 
       ADD_PROCESS_PARAM("string", "%12s",	"--detector","L1");    
       break;
     case H1: 
       ADD_PROCESS_PARAM("string", "%12s",	"--detector","H1");    
       break;
     case H2: 
       ADD_PROCESS_PARAM("string", "%12s",	"--detector","H2");    
       break;
     case V1: 
     case G1: 
       break;
     }
     switch (otherIn.run){
     case S2: 
       ADD_PROCESS_PARAM("string", "%12s",	"--run","S2");    
       break;
     case S3: 
       ADD_PROCESS_PARAM("string", "%12s",	"--run","S3");    
       break;
     case S1: 
     case S4:
     case S5:
     case S6:
       break;
     }
     ADD_PROCESS_PARAM("string", "%s",	"--channel",otherIn.chanName);    
     ADD_PROCESS_PARAM("string", "%12.9d",	"--gps-start-time",otherIn.startTime);     
     break;
  } 
  ADD_PROCESS_PARAM("int",	"%6d",		"--signal",		otherIn.signal);
  ADD_PROCESS_PARAM("int",	"%6d",		"--signal-order",	randIn.param.order);
  ADD_PROCESS_PARAM("int",	"%6d",		"--template",		otherIn.template);
  ADD_PROCESS_PARAM("int",	"%6d",		"--template-order",	coarseBankIn.order);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--tau0",		otherIn.tau0);
  ADD_PROCESS_PARAM("float",	"%12.5f",	"--tau3",		otherIn.tau3);
  if (otherIn.alphaFConstraint )
    ADD_PROCESS_PARAM("float", "%12.5d", "--alpha-constraint",1);
  if (!otherIn.alphaFConstraint )
    ADD_PROCESS_PARAM("float", "%12.5d", "--no-alpha-constraint",0);
  if (!otherIn.FastSimulation )
    ADD_PROCESS_PARAM("float", "%12.5d", "--fast-simulation",1);
  if (otherIn.binaryInjection == BHNS)
    ADD_PROCESS_PARAM("float", "%12.5d", "--bhns-injection", BHNS);
  	  
#undef ADD_PROCESS_PARAM
}


/* xml file for the standalone code */
void 
BEPrintResultsXml( InspiralCoarseBankIn   coarseBankIn,
		   RandomInspiralSignalIn randIn,
		   OtherParamIn           otherIn,
		   ResultIn trigger
		  )
{
  LALStatus             status = blank_status;


  MetadataTable         templateBank;
  CHAR                  ifo[3];                           /* two character ifo code       */
  LIGOLwXMLStream       xmlStream;
  CHAR                  fname[256];
  LIGOTimeGPS           gpsStartTime 	= { 0, 0 };    /* input data GPS start time    */
  LIGOTimeGPS           gpsEndTime 	= { 0, 0 };      /* input data GPS end time      */
  LALLeapSecAccuracy    accuracy = 1;
  CHAR                  comment[LIGOMETA_COMMENT_MAX];
  CHAR                  ifoName[MAXIFO][LIGOMETA_IFO_MAX];

  MetadataTable         processParamsTable;
  ProcessParamsTable   *this_proc_param = NULL;

  LALSnprintf( fname, sizeof(fname), 
	       BANKEFFICIENCY_PRINTRESULT_FILEXML ,
	       ifo,
	       gpsStartTime.gpsSeconds,
	       gpsEndTime.gpsSeconds - gpsStartTime.gpsSeconds );

  if (trigger.ntrial == 1){
    strncpy( ifoName[0], "no", LIGOMETA_IFO_MAX * sizeof(CHAR) );
    strncpy( ifoName[1], "ne", LIGOMETA_IFO_MAX * sizeof(CHAR) );
    memset( ifo, 0, sizeof(ifo) );
    memcpy( ifo, "MC", sizeof(ifo) - 1 );
    
    /* -- we start to fill the xml file here --- */
    memset( &xmlStream, 0, sizeof(LIGOLwXMLStream) );
    LAL_CALL( LALOpenLIGOLwXMLFile( &status, &xmlStream, fname), &status );
    
    /* create the process and process params tables */
    templateBank.processTable = (ProcessTable *) calloc( 1, sizeof(ProcessTable) );
    LAL_CALL( LALGPSTimeNow ( &status, &(templateBank.processTable->start_time),
			      &accuracy ), &status );
    LAL_CALL( populate_process_table( &status, 
				      templateBank.processTable, 
				      PROGRAM_NAME, 
				      CVS_REVISION,
				      CVS_SOURCE,
				      CVS_DATE ), &status );
    this_proc_param = processParamsTable.processParamsTable = 
      (ProcessParamsTable *) calloc( 1, sizeof(ProcessParamsTable) );
    
    BEFillProc(this_proc_param, coarseBankIn, randIn, otherIn);
    
    memset( comment, 0, LIGOMETA_COMMENT_MAX * sizeof(CHAR) );
    
    /* write process table */
    LALSnprintf( templateBank.processTable->ifos, LIGOMETA_IFOS_MAX, "%s%s", 
		 ifoName[0], ifoName[1] );
    LAL_CALL( LALGPSTimeNow ( &status, &(templateBank.processTable->end_time),
			      &accuracy ), &status );
    
    LAL_CALL( LALBeginLIGOLwXMLTable( &status, &xmlStream, process_table ), 
	      &status );
    LAL_CALL( LALWriteLIGOLwXMLTable( &status, &xmlStream, templateBank, 
				      process_table ), &status );
    LAL_CALL( LALEndLIGOLwXMLTable ( &status, &xmlStream ), &status );
    
    /* write process params table */
    LAL_CALL( LALBeginLIGOLwXMLTable( &status, &xmlStream, 
				      process_params_table ), &status );
    LAL_CALL( LALWriteLIGOLwXMLTable( &status, &xmlStream, processParamsTable, 
				      process_params_table ), &status );
    LAL_CALL( LALEndLIGOLwXMLTable ( &status, &xmlStream ), &status );
    
    /* finally write sngl inspiral table */
    
    PRINT_LIGOLW_XML_BANKEFFICIENCY(xmlStream.fp);
    
    fprintf(xmlStream.fp,BANKEFFICIENCY_PARAMS_ROW"\n",
	    trigger.psi0_trigger,
	    trigger.psi3_trigger,
	    trigger.psi0_triggerC,
	    trigger.psi3_triggerC,
	    trigger.psi0_inject, 
	    trigger.psi3_inject,
	    trigger.fend_trigger, 
	    trigger.fend_triggerC, 
	    trigger.fend_inject,
	    trigger.totalMass_trigger,
	    trigger.eta_trigger,
	    trigger.totalMass_triggerC,
	    trigger.eta_triggerC,
	    trigger.mass1_inject,
	    trigger.mass2_inject,
	    trigger.rho_final,
	    trigger.phase,
	    trigger.alpha,
	    trigger.alpha_f, 
	    trigger.layer,
	    trigger.bin,
	    trigger.rho_finalC,
	    trigger.phaseC,
	    trigger.alphaC,
	    trigger.alpha_fC, 
	    trigger.layerC,
	    trigger.binC,
	    trigger.coaTime
	    );
	    
     if (trigger.ntrial == (UINT4)otherIn.ntrials){
       PRINT_LIGOLW_XML_TABLE_FOOTER(xmlStream.fp);
       PRINT_LIGOLW_XML_FOOTER(xmlStream.fp);
     }
     fclose( xmlStream.fp );
     xmlStream.fp = NULL;
  }
  else 
    {
      xmlStream.fp = fopen(fname,"a+");
      
      if (trigger.ntrial == (UINT4)otherIn.ntrials){
	
	fprintf(xmlStream.fp,BANKEFFICIENCY_PARAMS_ROW,
		trigger.psi0_trigger,
		trigger.psi3_trigger,
		trigger.psi0_triggerC,
		trigger.psi3_triggerC,
		trigger.psi0_inject, 
		trigger.psi3_inject,
		trigger.fend_trigger, 
		trigger.fend_triggerC, 
		trigger.fend_inject,
		trigger.totalMass_trigger,
		trigger.eta_trigger,
		trigger.totalMass_triggerC,
		trigger.eta_triggerC,
		trigger.mass1_inject,
		trigger.mass2_inject,
		trigger.rho_final,
		trigger.phase,
		trigger.alpha,
		trigger.alpha_f, 
		trigger.layer,
		trigger.bin,
		trigger.rho_finalC,
		trigger.phaseC,
		trigger.alphaC,
		trigger.alpha_fC, 
		trigger.layerC,
		trigger.binC, 
		trigger.coaTime);
	PRINT_LIGOLW_XML_TABLE_FOOTER(xmlStream.fp );
	PRINT_LIGOLW_XML_FOOTER(xmlStream.fp );
      }
      else
	{
	  fprintf(xmlStream.fp,BANKEFFICIENCY_PARAMS_ROW"\n",
		  trigger.psi0_trigger,
		  trigger.psi3_trigger,
		  trigger.psi0_triggerC,
		  trigger.psi3_triggerC,
		  trigger.psi0_inject, 
		  trigger.psi3_inject,
		  trigger.fend_trigger, 
		  trigger.fend_triggerC, 
		  trigger.fend_inject,
		  trigger.totalMass_trigger,
		  trigger.eta_trigger,
		  trigger.totalMass_triggerC,
		  trigger.eta_triggerC,
		  trigger.mass1_inject,
		  trigger.mass2_inject,
		  trigger.rho_final,
		  trigger.phase,
		  trigger.alpha,
		  trigger.alpha_f, 
		  trigger.layer,
		  trigger.bin,
		  trigger.rho_finalC,
		  trigger.phaseC,
		  trigger.alphaC,
		  trigger.alpha_fC, 
		  trigger.layerC,
		  trigger.binC, 
		  trigger.coaTime);
	}
     
      fclose( xmlStream.fp );
      xmlStream.fp = NULL;
    }  

  /* close the output xml file */

}


/* print prototype for condor code (option --print-result-prototype)*/
void 
BEPrintProtoXml(InspiralCoarseBankIn   coarseBankIn,
		  RandomInspiralSignalIn randIn,
		  OtherParamIn           otherIn
		  )
{
  LALStatus             status = blank_status;


  MetadataTable         templateBank;
  CHAR                  ifo[3];                           /* two character ifo code       */
  LIGOLwXMLStream       xmlStream;
  CHAR                  fname[256];
  LIGOTimeGPS           gpsStartTime 	= { 0, 0 };    /* input data GPS start time    */
  LIGOTimeGPS           gpsEndTime 	= { 0, 0 };      /* input data GPS end time      */
  LALLeapSecAccuracy    accuracy = 1;
  CHAR                  comment[LIGOMETA_COMMENT_MAX];
  CHAR                  ifoName[MAXIFO][LIGOMETA_IFO_MAX];

  MetadataTable         processParamsTable;
  ProcessParamsTable   *this_proc_param = NULL;

  LALSnprintf( fname, sizeof(fname), BANKEFFICIENCY_PRINTPROTO_FILEXML ,
	       ifo, gpsStartTime.gpsSeconds,
	       gpsEndTime.gpsSeconds - gpsStartTime.gpsSeconds );




    strncpy( ifoName[0], "no", LIGOMETA_IFO_MAX * sizeof(CHAR) );
    strncpy( ifoName[1], "ne", LIGOMETA_IFO_MAX * sizeof(CHAR) );
    memset( ifo, 0, sizeof(ifo) );
    memcpy( ifo, "MC", sizeof(ifo) - 1 );
    
    
    
    /* -- we start to fill the xml file here --- */
    memset( &xmlStream, 0, sizeof(LIGOLwXMLStream) );
    LAL_CALL( LALOpenLIGOLwXMLFile( &status, &xmlStream, fname), &status );
    
    
    /* create the process and process params tables */
    templateBank.processTable = (ProcessTable *) calloc( 1, sizeof(ProcessTable) );
    LAL_CALL( LALGPSTimeNow ( &status, &(templateBank.processTable->start_time),
			      &accuracy ), &status );
    LAL_CALL( populate_process_table( &status, templateBank.processTable, 
				      PROGRAM_NAME, CVS_REVISION, CVS_SOURCE, CVS_DATE ), &status );
    this_proc_param = processParamsTable.processParamsTable = 
      (ProcessParamsTable *) calloc( 1, sizeof(ProcessParamsTable) );
    
    BEFillProc(this_proc_param, coarseBankIn, randIn, otherIn);
    
    memset( comment, 0, LIGOMETA_COMMENT_MAX * sizeof(CHAR) );
    
    
    
    /* write process table */
    LALSnprintf( templateBank.processTable->ifos, LIGOMETA_IFOS_MAX, "%s%s", 
		 ifoName[0], ifoName[1] );
    LAL_CALL( LALGPSTimeNow ( &status, &(templateBank.processTable->end_time),
			      &accuracy ), &status );
    
    LAL_CALL( LALBeginLIGOLwXMLTable( &status, &xmlStream, process_table ), 
	      &status );
    LAL_CALL( LALWriteLIGOLwXMLTable( &status, &xmlStream, templateBank, 
				      process_table ), &status );
    LAL_CALL( LALEndLIGOLwXMLTable ( &status, &xmlStream ), &status );
    
    /* write process params table */
    LAL_CALL( LALBeginLIGOLwXMLTable( &status, &xmlStream, 
				      process_params_table ), &status );
    LAL_CALL( LALWriteLIGOLwXMLTable( &status, &xmlStream, processParamsTable, 
				      process_params_table ), &status );
    LAL_CALL( LALEndLIGOLwXMLTable ( &status, &xmlStream ), &status );
    
    /* finally write sngl inspiral table */
    
     
      fclose( xmlStream.fp );
      xmlStream.fp = NULL;
#undef MAXIFO 


}



void
BEInitOverlapOutputIn(OverlapOutputIn *this){
  this->rhoMaxConstraint   = 0.;
  this->phaseConstraint    = 0.;
  this->rhoBinConstraint   = 0;
  this->templateNumberConstraint        = 0;
  this->alphaConstraint   = -1.;
  this->layerConstraint    = -1;
  this->freqConstraint     = -1.;

  this->rhoMaxUnconstraint = 0.;  
  this->phaseUnconstraint  = 0.;  
  this->rhoBinUnconstraint = 0;  
  this->templateNumberUnconstraint      = 0;
  this->alphaUnconstraint = -1.;
  this->layerUnconstraint  = -1;
   this->freqUnconstraint  = -1.;
 /*I do not think we need any other  iniitalisation */
}

void
BEFillOverlapOutput(InspiralWaveOverlapOut overlapout, 
		    OverlapOutputIn *this)
{

  this->rhoMaxConstraint         = overlapout.max;
  this->phaseConstraint          = overlapout.phase;
  this->rhoBinConstraint         = overlapout.bin;
  this->alphaConstraint         = -1.;
  this->freqConstraint           = -1;
  this->layerConstraint          = -1;
  this->templateNumberConstraint = -1;
  
  this->rhoMaxUnconstraint       = overlapout.max;
  this->phaseUnconstraint        = overlapout.phase;
  this->rhoBinUnconstraint       = overlapout.bin;
  this->alphaUnconstraint       = -1.;
  this->freqConstraint           = -1;
  this->layerConstraint          = -1;
  this->templateNumberConstraint = -1;
      
}



/* Estimate the size of the longest template */
void BEGetMaximumSize(LALStatus  *status, 		      
		      RandomInspiralSignalIn  randIn, 
		      UINT4 *length)
{

  INITSTATUS( status, "BEGetMaximumSize", BANKEFFICIENCYC );
  ATTATCHSTATUSPTR( status );
  
  randIn.param.massChoice 	= m1Andm2;
  randIn.param.approximant 	= EOB;
  *length = 0;
 
  LAL_CALL(LALInspiralWaveLength(status->statusPtr, length, randIn.param), 
	   status->statusPtr);

  *length*=2; /* for safety */

  DETATCHSTATUSPTR( status );
  RETURN( status );  

}



void BECreatePsd(LALStatus                *status, 
		   InspiralCoarseBankIn   *coarseBankIn, 
		   RandomInspiralSignalIn *randIn,
		   OtherParamIn           otherIn)
{
  UINT4 i;
  REAL4 df; 
  FILE  *Foutput;

  INITSTATUS( status, "BECreatePsd", BANKEFFICIENCYC );
  ATTATCHSTATUSPTR( status );


  memset( &(coarseBankIn->shf), 0, sizeof(REAL8FrequencySeries) );
  coarseBankIn->shf.f0 	= 0;
  LAL_CALL(LALDCreateVector( status->statusPtr, &(coarseBankIn->shf.data), randIn->psd.length ),
	   status->statusPtr);
  
  coarseBankIn->shf.deltaF 	= randIn->param.tSampling / ((randIn->psd.length-1)*2);
  
  /* --- Compute Noise Spectral Density --- */
  df = randIn->param.tSampling/(float) ((randIn->psd.length-1)*2);	 

  switch (otherIn.NoiseModel)						
    {
    case UNITY:
      LAL_CALL(LALNoiseSpectralDensity (status->statusPtr, coarseBankIn->shf.data, &LALLIGOIPsd, df),
	       status->statusPtr);
      for (i = 0; i < coarseBankIn->shf.data->length; i++)
	coarseBankIn->shf.data->data[i] = 1;
      break;
    case LIGOI:  
      LAL_CALL(LALNoiseSpectralDensity (status->statusPtr, coarseBankIn->shf.data, &LALLIGOIPsd, df), 
	       status->statusPtr);
      break;
    case LIGOA:  
      LAL_CALL(LALNoiseSpectralDensity (status->statusPtr, coarseBankIn->shf.data, &LALAdvLIGOPsd, df),
	       status->statusPtr);
      break;
    case VIRGO: 
      LAL_CALL(LALNoiseSpectralDensity (status->statusPtr, coarseBankIn->shf.data, &LALVIRGOPsd, df),
	       status->statusPtr);;
      break;
    case GEO:
      LAL_CALL(LALNoiseSpectralDensity (status->statusPtr, coarseBankIn->shf.data, &LALGEOPsd, df), 
	       status->statusPtr);
      break;
    case TAMA:
      LAL_CALL(LALNoiseSpectralDensity (status->statusPtr, coarseBankIn->shf.data, &LALTAMAPsd, df), 
	       status->statusPtr);
      break;
    case REALPSD:	
      /* Read psd dans le fichier InputPSD.dat */
      LAL_CALL(LALCreateRealPsd (status->statusPtr, coarseBankIn, *randIn , otherIn), 
	       status->statusPtr);
      break;
     }

  /* --- save the psd in the file psd.dat if requested --- */
  if (otherIn.PrintPsd)
   {
     Foutput= fopen(BANKEFFICIENCY_PRINTPSD_FILE,"w");
     for (i = 1; i < coarseBankIn->shf.data->length; i++)
       fprintf(Foutput, "%f %e\n",(float)i*df, coarseBankIn->shf.data->data[i]);  
     fclose(Foutput);
   }


  /* copy the psd in RandIn.psd */
  for (i = 0; i< coarseBankIn->shf.data->length; i++){
    randIn->psd.data[i] = coarseBankIn->shf.data->data[i];
  }


  

  DETATCHSTATUSPTR( status );
  RETURN( status );  
}



void BEGenerateInputData(LALStatus *status,
			 REAL4Vector * signal,
			 RandomInspiralSignalIn  *randIn,
			 OtherParamIn           otherIn)
{
  UINT4  trial ;
  UINT4 success ;


  INITSTATUS( status, "BEGenerateInputData", BANKEFFICIENCYC );
  ATTATCHSTATUSPTR( status );
  
  
  trial =0 ;
  success =0 ;
  /*  for (i = 0; i < signal->length; i++) 
    signal->data[i] = 0.;       

  */
  
  /* we might force to compute a non random waveform giving the two
     input parameters m1 and m2 */      
  randIn->param.approximant = otherIn.signal; 

  if (randIn->type != 1){
    if (otherIn.signal == BCV)
      {
	/* user parameters*/
	if (otherIn.psi0!=-1 && otherIn.psi3!=-1)
	  {
	    randIn->param.massChoice = fixedPsi;
	    randIn->param.psi0 = otherIn.psi0;
	    randIn->param.psi3 = otherIn.psi3;
	    LALRandomInspiralSignal(status->statusPtr, signal, randIn);
	    CHECKSTATUSPTR(status);
	  }
	else /* random parameters*/
	  {
	    randIn->param.massChoice = fixedPsi;
	    {
	      int valid = 0 ;
	      
	      while (!valid)
		{
		  REAL8 epsilon1 = (float) rand()/(float)RAND_MAX;
		  REAL8 epsilon2 = (float) rand()/(float)RAND_MAX;
		  REAL8 fend = 10000000.;
		  int trial = 0; 
		  
		  randIn->param.psi0 = randIn->psi0Min + epsilon1 * (randIn->psi0Max - randIn->psi0Min);
		  randIn->param.psi3 = randIn->psi3Min + epsilon2 * (randIn->psi3Max - randIn->psi3Min);
		  
		  
		  while (((fend > randIn->param.tSampling/2.) || ( fend < randIn->param.fLower)) && (trial < 10))
		    { 	     
		      
		      REAL8 fLR, fLSO;
		      REAL8 epsilon3 ;	
		      epsilon3 = (float) rand()/(float)RAND_MAX;
		      
		      randIn->param.totalMass = -randIn->param.psi3/(16.L*LAL_PI * LAL_PI * randIn->param.psi0);
		      randIn->param.totalMass *= 2 / LAL_MTSUN_SI;
		      
		      
		      fLR = 1.L/(LAL_PI * pow (3.L,1.5) * randIn->param.totalMass * LAL_MTSUN_SI); 
		      fLSO = 1.L/(LAL_PI * pow (6.L,1.5) * randIn->param.totalMass * LAL_MTSUN_SI);		
		      
		      fend = fLSO + (fLR - fLSO) * epsilon3;
		      fend = fLSO + (fLR - fLSO) * epsilon3;
		      
		      randIn->param.fFinal = fend;		  
		      randIn->param.fCutoff = fend;		  
		      
		      trial++;
		    }
		  if (trial == 10 ) valid = 0;
		  else valid = 1;
		}
	    }
	    
	    LALRandomInspiralSignal(status->statusPtr, signal, randIn);
	    CHECKSTATUSPTR(status);	    	    	    
	}
    }
  else /* EOB , T1 and so on*/
    {
      if (otherIn.m1!=-1 && otherIn.m2!=-1)
	{
	  randIn->param.massChoice = fixedMasses;
	  randIn->param.mass1 = otherIn.m1;
	  randIn->param.mass2 = otherIn.m2;
	  LALRandomInspiralSignal(status->statusPtr, signal, randIn);
	  CHECKSTATUSPTR(status);	  
	}
      else if (otherIn.tau0!=-1 && otherIn.tau3!=-1) 
	{
	  randIn->param.massChoice = fixedTau;
	  randIn->param.t0 = otherIn.tau0;
	  randIn->param.t3 = otherIn.tau3; 
	  LALRandomInspiralSignal(status->statusPtr, signal, randIn);
	  CHECKSTATUSPTR(status);
	}
      else if (otherIn.binaryInjection == BHNS)
	{
	  randIn->param.massChoice = bhns;
	  LALRandomInspiralSignal(status->statusPtr, signal, randIn);
	  CHECKSTATUSPTR(status);
	}
      else
	{
	  randIn->param.massChoice = m1Andm2;
	  randIn->param.massChoice = totalMassUAndEta;

	  LALRandomInspiralSignal(status->statusPtr, signal, randIn);
	  CHECKSTATUSPTR(status);
	}    
    }
  }


    if (randIn->type == 1)
      {
	randIn->param.massChoice = m1Andm2;
	
	LALRandomInspiralSignal(status->statusPtr, signal, randIn);
	CHECKSTATUSPTR(status);
      }    
    

     
  DETATCHSTATUSPTR(status);
  RETURN (status);

}

void 
LALCreateRealPsd(LALStatus *status, 
		 InspiralCoarseBankIn *bankIn,
		 RandomInspiralSignalIn randIn, 
		 OtherParamIn userParam)
{




  enum
    {
      undefined,
      real_4,
      real_8
    } calData = undefined;

  enum { unset, urandom, user } randSeedType = user;    /* sim seed type */
  INT4  randomSeed        = 1;            /* value of sim rand seed       */
  REAL4 gaussVar          = 1.0;         /* variance of gaussian noise   */
  INT4  gaussianNoise     = 0;            /* make input data gaussian   0 non , 1 oui  */
  RandomParams *randParams = NULL;

                 /* verbocity of lal function    */


  
  int i; FILE *Foutput;
  /* lal function variables */

  /*LALLeapSecAccuracy    accuracy = LALLEAPSEC_LOOSE;*/

  /* frame input data */
  FrCache      *frInCache = NULL;
  FrCache      *calCache = NULL;
  FrStream     *frStream = NULL;
  FrChanIn      frChan;
  CHAR tmpChName[LALNameLength];

  COMPLEX8FrequencySeries       injResp;
  COMPLEX8FrequencySeries      *injRespPtr;
  LIGOTimeGPS slideData   = {0,0};        /* slide data for time shifting */

  INT8  durationNS = 0;  
  INT8  gpsEndTimeNS     = 0;             /* input data GPS end time ns   */
  INT8  gpsStartTimeNS   = 0;             /* input data GPS start time ns */

  /* parameters used to generate calibrated power spectrum */
  LIGOTimeGPS gpsStartTime = { 0, 0 };    /* input data GPS start time    */
  LIGOTimeGPS gpsEndTime = { 0, 0 };      /* input data GPS end time      */
  INT4  padData = 8;                      /* saftety margin on input data */
  CHAR  *fqChanName       = NULL;


  CHAR *injectionFile = "/home/cokelaer/Work/TestWaveOverlap/HL-INJECTIONS_1-732005208-2048.xml";
  CHAR  *frInCacheName = NULL;

  INT4  numPoints         = (randIn.psd.length-1)*2;           /* points in a segment          */
  INT4  numSegments       = 15;           /* number of segments           */
  CHAR  ifo[3];                           /* two character ifo code       */

  INT4  inputDataLength =  -1;              /* number of points in input    */
  INT4   resampFiltType   = 0;           /* low pass filter used for res 0 = = ldas*/
  INT4   sampleRate       = randIn.param.tSampling;           /* sample rate of filter data   */
  INT4   highPass         = 1;           /* enable high pass on raw data */
  REAL4  highPassFreq     = bankIn->fLower;            /* high pass frequency          */
  INT4   highPassOrder    = 8;           /* order of the td iir filter   */
  REAL4  highPassAtten    = 0.1;           /* attenuation of the td filter */
  REAL4  fLow             = bankIn->fLower;           /* low frequency cutoff         */
  INT4   specType         = 1;           /* use median (1)or mean(0) psd   or gauss    */

  CHAR  *calCacheName     = NULL;
  INT4   pointCal         = 0;            /* don't average cal over chunk */
  REAL4  dynRangeExponent = 40;            /*onent of dynamic range    */
  REAL4 geoHighPassFreq = -1;             /* GEO high pass frequency      */
  INT4  geoHighPassOrder = -1;            /* GEO high pass filter order   */
  REAL4 geoHighPassAtten = -1;            /* GEO high pass attenuation    */  
  /* raw iut data storage */
  REAL4TimeSeries               chan;
  REAL8TimeSeries               geoChan;
  REAL4FrequencySeries          spec;
  
  /* structures for preconditioning */
  ResampleTSParams              resampleParams;
    
  
  /* counters and other variables */
  UINT4 cut,  j, k;
  const LALUnit strainPerCount = {0,{0,0,0,0,0,1,-1},{0,0,0,0,0,0,0}};
  
  LALUnitPair pair;
  REAL8 respRe, respIm;
  REAL8 shf;
  REAL8 inputLengthNS;
  UINT4 numInputPoints;
  const REAL8 epsilon = 1.0e-8;
  UINT4 resampleChan = 0;
  CalibrationUpdateParams calfacts, inj_calfacts;
  REAL8 dynRange = 0;
  REAL8 inputDeltaT;


  WindowSpectrumIn windowSpectrumParam;
  /*InspiralPipelineIn inspiralPipelineIn;*/

  CHAR         *calGlobPattern;

  INITSTATUS( status, "LALCreatRealPsd", BANKEFFICIENCYC );
  ATTATCHSTATUSPTR( status );

  
  /*  SetInspiralPipelineParams(&inspiralPipelineIn, randIn);*/


  /*  fprintf(stderr,"Generating real PSD\n");*/
  calData = undefined;

  memset( ifo, 0, sizeof(ifo));

  switch (userParam.detector){
  case L1: 
    memcpy( ifo, "L1", sizeof(ifo) - 1 );
    break;
  case H1: 
    memcpy( ifo, "H1", sizeof(ifo) - 1 );
    break;
  case H2: 
    memcpy( ifo, "H2", sizeof(ifo) - 1 );
    break;
  case V1: 
  case G1: 
    break;
  }


  if ( dynRangeExponent )
  {
    /* compute the dynamic range scaling for the psd computation */
    dynRange = (REAL8) pow( 2.0, dynRangeExponent );
  }
  else
  {
    dynRange = 1.0;
  }
  if ( vrbflg )
    fprintf( stdout, "using dynamic range scaling %e\n", dynRange );




  /** Add on Thomas BEGIN */ 
  
  

  /* S3 */
  /*gpsStartTimeNS += (INT8) (752578908) * 1000000000LL;*/

  /* S2 */
   /*   gpsStartTimeNS += (INT8) (732005200 +padData) * 1000000000LL ;
  gpsStartTimeNS += (INT8) (0);
  */

  gpsStartTimeNS += (INT8) (userParam.startTime) *  1000000000LL;
  fqChanName    = userParam.chanName; 
  calCacheName  = userParam.calCacheName;
  frInCacheName = userParam.frInCacheName;

  inputDataLength = numPoints   * ( numSegments + 1) / 2 ;


  
  
  gpsEndTimeNS = gpsStartTimeNS;
  gpsEndTimeNS += (INT8) ((UINT8)inputDataLength /(UINT8)sampleRate) 
    * 1000000000LL;
  gpsEndTimeNS += (INT8) (0);
 
  LAL_CALL( LALINT8toGPS( status->statusPtr, &(gpsStartTime), &(gpsStartTimeNS) ), 
	    status->statusPtr ); 
  LAL_CALL( LALINT8toGPS( status->statusPtr, &(gpsEndTime), &(gpsEndTimeNS) ), 
	    status->statusPtr );
  /** End Thomas */

  /*
   *
   * read in the input data channel
   *
   */
 

  /* set the time series parameters of the input data and resample params */
  memset( &resampleParams, 0, sizeof(ResampleTSParams) );
  resampleParams.deltaT = 1.0 / (REAL8) sampleRate;
  
  /* set the params of the input data time series */
  memset( &chan, 0, sizeof(REAL4TimeSeries) );
  memset( &geoChan, 0, sizeof(REAL8TimeSeries) );
  chan.epoch = gpsStartTime;
  chan.epoch.gpsSeconds -= padData; /* subtract pad seconds from start */
  /* subtract slide from start */
  chan.epoch.gpsSeconds -= slideData.gpsSeconds; 
  chan.epoch.gpsNanoSeconds -= slideData.gpsNanoSeconds;
  /* copy the start time into the GEO time series */
  geoChan.epoch = chan.epoch;


  
  if ( vrbflg ) 
    fprintf( stdout,  "reading frame file locations from cache file: %s\n", frInCacheName );

  /* read a frame cache from the specified file */
  LAL_CALL( LALFrCacheImport( status->statusPtr, &frInCache, 
			      frInCacheName), status->statusPtr );
  

  /* open the input data frame stream from the frame cache */
  LAL_CALL( LALFrCacheOpen( status->statusPtr, &frStream,frInCache ), status->statusPtr );

  /* set the mode of the frame stream to fail on gaps or time errors */
  frStream->mode = LAL_FR_VERBOSE_MODE;

  /* seek to required epoch and set chan name */
  LAL_CALL( LALFrSeek( status->statusPtr, &(chan.epoch), frStream ), status->statusPtr );
  frChan.name = fqChanName;

  if ( calData == real_8 )
  {
    /* determine the sample rate of the raw data */
    LAL_CALL( LALFrGetREAL8TimeSeries( status->statusPtr, &geoChan, &frChan, frStream ),
        status->statusPtr );

    /* copy the data paramaters from the GEO channel to input data channel */
    LALSnprintf( chan.name, LALNameLength * sizeof(CHAR), "%s", geoChan.name );
    chan.epoch          = geoChan.epoch;
    chan.deltaT         = geoChan.deltaT;
    chan.f0             = geoChan.f0;
    chan.sampleUnits    = geoChan.sampleUnits;
  }
  else
  {
    /* determine the sample rate of the raw data and allocate enough memory */
    LAL_CALL( LALFrGetREAL4TimeSeries( status->statusPtr, &chan, &frChan, frStream ),
        status->statusPtr );
  }


  inputDeltaT = chan.deltaT;



  /* determine if we need to resample the channel */
  if ( vrbflg )
  {
    fprintf( stdout, "resampleParams.deltaT = %e\n", resampleParams.deltaT );
    fprintf( stdout, "chan.deltaT = %e\n", chan.deltaT );
  }
  if ( ! ( fabs( resampleParams.deltaT - chan.deltaT ) < epsilon ) )
  {
    resampleChan = 1;
    if ( vrbflg )
      fprintf( stdout, "input channel will be resampled\n" );

    if ( resampFiltType == 0 )
    {
      resampleParams.filterType = LDASfirLP;
    }
    else if ( resampFiltType == 1 )
    {
      resampleParams.filterType = defaultButterworth;
    }
  }

  /* determine the number of points to get and create storage for the data */
  inputLengthNS = 
    (REAL8) ( gpsEndTimeNS - gpsStartTimeNS + 2000000000LL * padData );
  numInputPoints = (UINT4) floor( inputLengthNS / (chan.deltaT * 1.0e9) + 0.5 );
  if ( calData == real_8 )
    {
    /* create storage for the GEO input data */
      LAL_CALL( LALDCreateVector( status->statusPtr, &(geoChan.data), numInputPoints ), 
		status->statusPtr );
    }
  LAL_CALL( LALSCreateVector( status->statusPtr, &(chan.data), numInputPoints ), 
	    status->statusPtr );

  if ( vrbflg ) fprintf( stdout, "input channel %s has sample interval "
      "(deltaT) = %e\nreading %d points from frame stream\n", fqChanName, 
      chan.deltaT, numInputPoints );

  if ( calData == real_8 )
  {
    /* read in the GEO data here */
    PassBandParamStruc geoHighpassParam;

    /* read the GEO data from the time series into geoChan      */
    /* which already has the correct amount of memory allocated */
    if ( vrbflg ) fprintf( stdout, "reading GEO data from frames... " );

    LAL_CALL( LALFrGetREAL8TimeSeries( status->statusPtr, &geoChan, &frChan, frStream ),
        status->statusPtr);

    if ( vrbflg ) fprintf( stdout, "done\n" );

    /* high pass the GEO data using the parameters specified on the cmd line */
    geoHighpassParam.nMax = geoHighPassOrder;
    geoHighpassParam.f1 = -1.0;
    geoHighpassParam.f2 = (REAL8) geoHighPassFreq;
    geoHighpassParam.a1 = -1.0;
    geoHighpassParam.a2 = (REAL8)(1.0 - geoHighPassAtten);
    if ( vrbflg ) fprintf( stdout, "applying %d order high pass to GEO data: "
        "%3.2f of signal passes at %4.2f Hz\n", 
			   geoHighpassParam.nMax, 
			   geoHighpassParam.a2,
			   geoHighpassParam.f2 );

    LAL_CALL( LALButterworthREAL8TimeSeries( status->statusPtr, &geoChan, 
          &geoHighpassParam ), status->statusPtr );

    /* cast the GEO data to REAL4 in the chan time series       */
    /* which already has the correct amount of memory allocated */
    for ( j = 0 ; j <numInputPoints ; ++j )
    {
      chan.data->data[j] = (REAL4) ( geoChan.data->data[j] * dynRange );
    }

    /* re-copy the data paramaters from the GEO channel to input data channel */
    LALSnprintf( chan.name, LALNameLength * sizeof(CHAR), "%s", geoChan.name );
    chan.epoch          = geoChan.epoch;
    chan.deltaT         = geoChan.deltaT;
    chan.f0             = geoChan.f0;
    chan.sampleUnits    = geoChan.sampleUnits;

    /* free the REAL8 GEO input data */
    LAL_CALL( LALDDestroyVector( status->statusPtr, &(geoChan.data) ), status->statusPtr );
    geoChan.data = NULL;
  }
  else if ( calData == real_4)
    {
      /* read the data channel time series from frames */
      LAL_CALL( LALFrGetREAL4TimeSeries( status->statusPtr, &chan, &frChan, frStream ),
		status->statusPtr );
      
      /* multiply the input data by dynRange */
      for ( j = 0 ; j < numInputPoints ; ++j )
	{
	  chan.data->data[j] *= dynRange;           
	}
    }
  else
    {
      /* read the data channel time series from frames */
      LAL_CALL( LALFrGetREAL4TimeSeries( status->statusPtr, &chan, &frChan, frStream ),
		status->statusPtr );	
    }



  memcpy( &(chan.sampleUnits), &lalADCCountUnit, sizeof(LALUnit) );


  /* close the frame file stream and destroy the cache */
  LAL_CALL( LALFrClose( status->statusPtr, &frStream ), status->statusPtr );


  LAL_CALL( LALDestroyFrCache( status->statusPtr, &frInCache ), status->statusPtr );


  if ( vrbflg ) fprintf( stdout, "read channel %s from frame stream\n"
      "got %d points with deltaT %e\nstarting at GPS time %d sec %d ns\n", 
      chan.name, chan.data->length, chan.deltaT, 
      chan.epoch.gpsSeconds, chan.epoch.gpsNanoSeconds );



  /*
   *
   * create the random seed if it will be needed
   *
   */


  if ( randSeedType != unset )
    {
      
    if ( randSeedType == urandom )
    {
      FILE   *fpRand = NULL;
      INT4    randByte;

      if ( vrbflg ) 
        fprintf( stdout, "obtaining random seed from /dev/urandom: " );

      randomSeed = 0;
      fpRand = fopen( "/dev/urandom", "r" );
      if ( fpRand )
      {
        for ( randByte = 0; randByte < 4 ; ++randByte )
        {
          INT4 tmpSeed = (INT4) fgetc( fpRand );
          randomSeed += tmpSeed << ( randByte * 8 );
        }
        fclose( fpRand );
      }
      else
      {
        perror( "error obtaining random seed from /dev/urandom" );
        exit( 1 );
      }
    }
    else if ( randSeedType == user )
    {
      if ( vrbflg ) 
        fprintf( stdout, "using user specified random seed: " );
    }
    else
    {
      /* should never get here */
      fprintf( stderr, "error obtaining random seed\n" );
      exit( 1 );
    }

    if ( vrbflg ) fprintf( stdout, "%d\n", randomSeed );

    /* create the tmplt bank random parameter structure */
    LAL_CALL( LALCreateRandomParams( status->statusPtr, &randParams, randomSeed ),
        status->statusPtr );
  }

  /* replace the input data with gaussian noise if necessary */
  if ( gaussianNoise )
  {
    if ( vrbflg ) fprintf( stdout, 
        "setting input data to gaussian noise with variance %e... ", gaussVar );
    memset( chan.data->data, 0, chan.data->length * sizeof(REAL4) );
    LAL_CALL( LALNormalDeviates(status->statusPtr, chan.data, randParams ), 
	      status->statusPtr );
    for ( j = 0; j < chan.data->length; ++j )
    {
      chan.data->data[j] *= gaussVar;
    }
    if ( vrbflg ) fprintf( stdout, "done\n" );

  }


  

  /*
   *
   * generate the response function for the requested time
   *
   */
  
  memset( &resp, 0, sizeof(COMPLEX8FrequencySeries) );
  LAL_CALL( LALCCreateVector( status->statusPtr, &(resp.data), numPoints / 2 + 1 ), 
      status->statusPtr );
  
  /* set the parameters of the response to match the data */
  resp.epoch.gpsSeconds = chan.epoch.gpsSeconds + padData;
  resp.epoch.gpsNanoSeconds = chan.epoch.gpsNanoSeconds;
  resp.deltaF = (REAL8) sampleRate / (REAL8) numPoints;
  resp.sampleUnits = strainPerCount;
  strcpy( resp.name, chan.name );

  /* generate the response function for the current time */
  if ( vrbflg ) fprintf( stdout, "generating response at time %d sec %d ns\n",
			 resp.epoch.gpsSeconds, resp.epoch.gpsNanoSeconds );
  
  /* initialize the calfacts */
  memset( &calfacts, 0, sizeof(CalibrationUpdateParams) );
  calfacts.ifo = ifo;

  /* determine length of chunk */
  if ( pointCal )
    {
    calfacts.duration.gpsSeconds = 1;
    calfacts.duration.gpsNanoSeconds = 0;
    }
  else
    {
     durationNS = gpsEndTimeNS - gpsStartTimeNS;
      LAL_CALL( LALINT8toGPS( status->statusPtr, &(calfacts.duration), 
			      &durationNS ), status->statusPtr );
    }

  
  if ( calData )
  {
    /* if we are using calibrated data set the response to unity */

    for( k = 0; k < resp.data->length; ++k )
    {
      resp.data->data[k].re = (REAL4) (1.0 / dynRange);
      resp.data->data[k].im = 0.0;
    }
  }
  else
  {
    calGlobPattern = NULL;        
    if ( vrbflg ) fprintf( stdout, 
          "reading calibration data from cache: %s ....", calCacheName );
    LAL_CALL( LALCreateCalibFrCache( status->statusPtr, &calCache, calCacheName, 
          NULL, calGlobPattern ), status->statusPtr );
  
    
    if ( calGlobPattern ) LALFree( calGlobPattern );


    /* get the response from the frame data */
    LAL_CALL( LALExtractFrameResponse( status->statusPtr, &resp, calCache, 
				       &calfacts), status->statusPtr );
    
    LAL_CALL( LALDestroyFrCache( status->statusPtr, &calCache), status->statusPtr );
    if ( vrbflg ) fprintf( stdout, 
			   "for calibration of data, alpha = %f and alphabeta = %f\n",
			   (REAL4) calfacts.alpha.re, (REAL4) calfacts.alphabeta.re);
  }


  if ( gaussianNoise )
    {
      /* replace the response function with unity if */
      /* we are filtering gaussian noise             */
      if ( vrbflg ) fprintf( stdout, "setting response to unity... " );
      for ( k = 0; k < resp.data->length; ++k )
	{
	  resp.data->data[k].re = 1.0;
	  resp.data->data[k].im = 0;
	}
      if ( vrbflg ) fprintf( stdout, "done\n" );
      
  
    }
  
  injectionFile = NULL;
    


  if ( injectionFile )
  {
    /* get injections within 500 seconds of either end of the segment.   */
    /* a 0.4,0.4 MACHO starting at 30.0 Hz has length 435.374683 seconds */
    /* so this should be plenty of safety. better to waste cpu than miss */
    /* injected signals...                                               */
    INT4 injSafety = 500;
    int  numInjections = 0;
    SimInspiralTable    *injections = NULL;
    SimInspiralTable    *thisInj = NULL;

    /* read in the injection data from XML */
    numInjections = SimInspiralTableFromLIGOLw( &injections, injectionFile,
        gpsStartTime.gpsSeconds - injSafety, 
        gpsEndTime.gpsSeconds + injSafety );

    if ( numInjections < 0 )
    {
      fprintf( stderr, "error: cannot read injection file" );
      exit( 1 );
    }
    else if ( numInjections )
    {

      fprintf(stderr, "resample injection\n");

      /* see if we need a higher resolution response to do the injections */
      if ( resampleChan )	
      {


        /* we need a different resolution of response function for injections */
        UINT4 rateRatio = floor( resampleParams.deltaT / chan.deltaT + 0.5 );
        UINT4 rawNumPoints = rateRatio * numPoints;

        if ( vrbflg ) fprintf( stdout, "rateRatio = %d\nrawNumPoints = %d\n"
            "chan.deltaT = %e\n", rateRatio, rawNumPoints, chan.deltaT );

        memset( &injResp, 0, sizeof(COMPLEX8FrequencySeries) );
        LAL_CALL( LALCCreateVector( status->statusPtr, &(injResp.data), 
              rawNumPoints / 2 + 1 ), status->statusPtr );
        injResp.epoch = resp.epoch;
        injResp.deltaF = 1.0 / ( rawNumPoints * chan.deltaT );
        injResp.sampleUnits = strainPerCount;
        strcpy( injResp.name, chan.name );

	
        if ( calData )
        {
          /* if we are using calibrated data set the response to unity */
          if ( vrbflg ) fprintf( stdout, 
              "setting injection response to inverse dynRange... " );
	  dynRange = (REAL8) pow( 2.0, 40);
          for ( k = 0; k < injResp.data->length; ++k )
          {
            injResp.data->data[k].re = (REAL4)(1.0/dynRange);
            injResp.data->data[k].im = 0.0;
          }
          injRespPtr = &injResp;
        }
        else
        {
          /* generate the response function for the current time */
          if ( vrbflg ) fprintf( stdout, 
              "generating high resolution response at time %d sec %d ns\n"
              "length = %d points, deltaF = %e Hz\n",
              resp.epoch.gpsSeconds, resp.epoch.gpsNanoSeconds,
              injResp.data->length, injResp.deltaF );

          /* initialize the inj_calfacts */
          memset( &inj_calfacts, 0, sizeof(CalibrationUpdateParams) );
          inj_calfacts.ifo = ifo;

	  LAL_CALL( LALINT8toGPS( status->statusPtr, &(inj_calfacts.duration), 
				  &durationNS ), status->statusPtr );
	  
          /* create the lal calibration frame cache */
	  calGlobPattern = NULL;
	  if ( vrbflg ) fprintf( stdout, 
				 "reading calibration data from cache: %s\n", calCacheName );
	  
	  LAL_CALL( LALCreateCalibFrCache( status->statusPtr, &calCache, calCacheName, 
                NULL, calGlobPattern ), status->statusPtr );
          if ( calGlobPattern ) LALFree( calGlobPattern );


	  /* extract the calibration from frames */
          LAL_CALL( LALExtractFrameResponse( status->statusPtr, &injResp, calCache, 
                &inj_calfacts ), status->statusPtr );

	  
	  LAL_CALL( LALDestroyFrCache( status->statusPtr, &calCache), status->statusPtr );
	  

          injRespPtr = &injResp;	  
	}
	if ( gaussianNoise )
	  {
	    /* replace the response function with unity if */
	    /* we are filtering gaussian noise             */
	    if ( vrbflg ) fprintf( stdout, "setting response to unity... " );
	    for ( k = 0; k < injResp.data->length; ++k )
	      {
		injResp.data->data[k].re = 1.0;
		injResp.data->data[k].im = 0;
	      }
	    if ( vrbflg ) fprintf( stdout, "done\n" );
	    
        }

      }
      else
      {
        /* the data is already at the correct sample rate, just do injections */
        injRespPtr = &resp;
        memset( &injResp, 0, sizeof(COMPLEX8FrequencySeries) );
      }

      /* inject the signals, preserving the channel name (Tev mangles it) */


      /* if injectOverhead option, then set chan.name to "ZENITH".  
       * This causes no detector site to be found in the injection code so
       * that the injection is done directly overhead (i.e. with a response 
       * function of F+ = 1; Fx = 0) */
      LALSnprintf( tmpChName, LALNameLength * sizeof(CHAR), "%s", chan.name );
      
      LALSnprintf( chan.name, LALNameLength * sizeof(CHAR), "ZENITH" );


      LAL_CALL( LALFindChirpInjectSignals( status->statusPtr, &chan, injections, 
            injRespPtr ), status->statusPtr );
      LALSnprintf( chan.name,  LALNameLength * sizeof(CHAR), "%s", tmpChName );

      if ( vrbflg ) fprintf( stdout, "injected %d signals from %s into %s\n", 
          numInjections, injectionFile, chan.name );

      while ( injections )
      {
        thisInj = injections;
        injections = injections->next;
        LALFree( thisInj );
      }

      /* write the raw channel data plus injections to the output frame file */

      if ( injResp.data )
        LAL_CALL( LALCDestroyVector( status->statusPtr, &(injResp.data) ), status->statusPtr );
    }
    else
    {
      if ( vrbflg ) fprintf( stdout, "no injections in this chunk\n" );
    }
  }


  /* resample the input data */
  if ( resampleChan )
  {
    if (vrbflg) fprintf( stdout, "resampling input data from %e to %e\n",
        chan.deltaT, resampleParams.deltaT );

    LAL_CALL( LALResampleREAL4TimeSeries( status->statusPtr, &chan, &resampleParams ),
        status->statusPtr );

    if ( vrbflg ) fprintf( stdout, "channel %s resampled:\n"
        "%d points with deltaT %e\nstarting at GPS time %d sec %d ns\n", 
        chan.name, chan.data->length, chan.deltaT, 
        chan.epoch.gpsSeconds, chan.epoch.gpsNanoSeconds );
  }


  /* 
   *
   * high pass the data, removed pad from time series and check length of data
   *
   */
  
  /* iir filter to remove low frequencies from data channel */
  if ( highPass )
  {
    PassBandParamStruc highpassParam;
    highpassParam.nMax = highPassOrder;
    highpassParam.f1 = -1.0;
    highpassParam.f2 = (REAL8) highPassFreq;
    highpassParam.a1 = -1.0;
    highpassParam.a2 = (REAL8)(1.0 - highPassAtten); /* a2 is not attenuation */

    if ( vrbflg ) fprintf( stdout, "applying %d order high pass: "
        "%3.2f of signal passes at %4.2f Hz\n", 
        highpassParam.nMax, highpassParam.a2, highpassParam.f2 );

    LAL_CALL( LALButterworthREAL4TimeSeries( status->statusPtr, &chan, &highpassParam ),
        status->statusPtr );
  }

  /* remove pad from requested data from start and end of time series */
  memmove( chan.data->data, chan.data->data + padData * sampleRate, 
      (chan.data->length - 2 * padData * sampleRate) * sizeof(REAL4) );
  LALRealloc( chan.data->data, 
      (chan.data->length - 2 * padData * sampleRate) * sizeof(REAL4) );
  chan.data->length -= 2 * padData * sampleRate;
  chan.epoch.gpsSeconds += padData;

  if ( vrbflg ) fprintf( stdout, "after removal of %d second padding at "
      "start and end:\ndata channel sample interval (deltaT) = %e\n"
      "data channel length = %d\nstarting at %d sec %d ns\n", 
      padData , chan.deltaT , chan.data->length, 
      chan.epoch.gpsSeconds, chan.epoch.gpsNanoSeconds );


  /*
   *
   * power spectrum estimation
   *
   */


  /* create storage for the power spectral estimate */
  memset( &spec, 0, sizeof(REAL4FrequencySeries) );
  LAL_CALL( LALSCreateVector( status->statusPtr, &(spec.data), numPoints / 2 + 1 ), 
      status->statusPtr );


  windowSpectrumParam.numPoints =  numPoints ;
  windowSpectrumParam.gaussVar =  gaussVar ;
  windowSpectrumParam.inputDeltaT =  inputDeltaT ;
  windowSpectrumParam.specType =  specType ;


  LAL_CALL( LALComputeWindowSpectrum( status->statusPtr, 
				      &windowSpectrumParam,
				      &spec, 
				      &chan ),
	    status->statusPtr );




  /* set low frequency cutoff of power spectrum */
  cut = fLow / spec.deltaF > 1 ?  fLow / spec.deltaF : 1;

  /* compute a calibrated strain power spectrum */
  
  bankIn->shf.epoch = spec.epoch;
  memcpy( bankIn->shf.name, spec.name, LALNameLength * sizeof(CHAR) );
  bankIn->shf.deltaF = spec.deltaF;
  bankIn->shf.f0 = spec.f0;
  bankIn->shf.data = NULL;
  
  pair.unitOne = &(spec.sampleUnits);
  pair.unitTwo = &(resp.sampleUnits);
  LAL_CALL( LALUnitMultiply( status->statusPtr, &(bankIn->shf.sampleUnits), &pair ), 
      status->statusPtr );
  LAL_CALL( LALDCreateVector( status->statusPtr, &(bankIn->shf.data), spec.data->length ),
      status->statusPtr );
  memset( bankIn->shf.data->data, 0, 
      bankIn->shf.data->length * sizeof(COMPLEX8) ); 
  
  shf = spec.data->data[cut] * 
    ( resp.data->data[cut].re * resp.data->data[cut].re +
      resp.data->data[cut].im * resp.data->data[cut].im );

  for ( k = 1; k < cut ; ++k )
  {
    bankIn->shf.data->data[k] = shf;
  }

  for ( k = cut; k < bankIn->shf.data->length; ++k )
  {
    respRe = (REAL8) resp.data->data[k].re;
    respIm = (REAL8) resp.data->data[k].im;
    bankIn->shf.data->data[k] = (REAL8) spec.data->data[k] *
      ( respRe * respRe + respIm * respIm );

    
  }
  

  Foutput= fopen("spec.dat","w");

  for (i = 1; i < (int)bankIn->shf.data->length; i++)
    fprintf(Foutput, "%15.10e\n", bankIn->shf.data->data[i]);  
  fclose(Foutput); 
  /*
   Foutput= fopen("uncalibratedpsd.dat","w");

  for (i = 1; i < spec.data->length; i++)
    fprintf(Foutput, "%15.10e\n",spec.data->data[i]);  
  fclose(Foutput); 


  Foutput= fopen("respre.dat","w");

  for (i = 0; i < spec.data->length; i++){
    respRe = (REAL8) resp.data->data[i].re;
    fprintf(Foutput, "%e\n",respRe);  
  }
  fclose(Foutput); 
  
  Foutput= fopen("respim.dat","w");

  for (i = 1; i < spec.data->length; i++){
    respIm = (REAL8) resp.data->data[i].im;
    fprintf(Foutput, "%e\n",respIm);  
  }
  fclose(Foutput); 

  */

  /* ADD on Thomas Create strain data to be process by Overlap or something else */

  {
    REAL4       *dataPtr;
    RealFFTPlan                  *fwdPlan = NULL;


    REAL4Vector    *rawSegment = NULL ; 


    INT4 length;

    length = numPoints/2 + 1;
    strainSegment = NULL;

    LALCreateVector( status->statusPtr, &rawSegment,
		     numPoints);
    CHECKSTATUSPTR( status );
    
    LALCCreateVector( status->statusPtr, &strainSegment,
		      numPoints/2+1);    
    CHECKSTATUSPTR( status );
 

     
    dataPtr = chan.data->data;
    rawSegment->data = dataPtr;
   
    
    LALCreateForwardRealFFTPlan( status->statusPtr, &fwdPlan, 
				 numPoints, 0);
    CHECKSTATUSPTR( status );

    /*     Foutput= fopen("raw0.dat","w");
    for (i = 1; i < rawSegment->length; i++)
      fprintf(Foutput, "%e\n", rawSegment->data[i]);  
    fclose(Foutput); 
    */
   
    
    
    LALForwardRealFFT( status->statusPtr, strainSegment,
		       rawSegment, fwdPlan );
    CHECKSTATUSPTR( status );



    dynRange = (REAL8) pow( 2.0, 0);
    
    
    for ( k = 0; k < strainSegment->length; ++k )
      {
	
	REAL4 p = strainSegment->data[k].re;
	REAL4 q = strainSegment->data[k].im;
	REAL4 x = resp.data->data[k].re * dynRange;
	REAL4 y = resp.data->data[k].im * dynRange;
	
	strainSegment->data[k].re =  p*x - q*y;
	strainSegment->data[k].im =  p*y + q*x;
      }
    
    LALDestroyRealFFTPlan(status->statusPtr,&fwdPlan);
       

    for ( k = 0; k < cut; ++k )
      {
	strainSegment->data[k].re = 0.0;
	strainSegment->data[k].im = 0.0;
      }
      
    dynRange = (REAL8) pow( 2.0, 40);/* pour avoir meme resultat que LAL avant filtrage*/
    /*
    Foutput= fopen("finalstrainre.dat","w");
    
    for (i = 0; i < strainSegment->length; i++)
      fprintf(Foutput, "%15.10e\n", strainSegment->data[i].re*dynRange );  
    fclose(Foutput); 
      
      Foutput= fopen("finalstrainim.dat","w");
      
      for (i = 0; i < strainSegment->length; i++)
	fprintf(Foutput, "%15.10e\n", strainSegment->data[i].im*dynRange );  
	
	fclose(Foutput); 
    */
    
  
  }
  /* END ADD on Thomas*/                                                                                                                        


  /* spec, uncalibrated spec, and raw0.dat identical to 
     inspiral in FindChipSPData. */


#if 0
 { 
   REAL4Vector            signal;
   RandomInspiralSignalIn randIn;
   UINT4 n,k;
  RealFFTPlan 			*fwdp = NULL;


   n 	= (strainSegment->length-1)*2;
   signal.length = n;
   signal.data 		= (REAL4*) LALCalloc(1, sizeof(REAL4) * signal.length);

   LALCreateForwardRealFFTPlan(status->statusPtr, &fwdp, signal.length, 0);

   randIn.param.tSampling  = sampleRate;
   randIn.param.massChoice = fixedPsi;
   randIn.param.psi0 = 226783;
   randIn.param.psi3 = -943;
   randIn.param.fCutoff = 620;
   randIn.param.alpha = 1.1;
   randIn.param.approximant = BCV;
   randIn.param.massChoice = fixedPsi;
   randIn.param.fLower  = 40 ; 
   randIn.psd.length 	= strainSegment->length;
   randIn.psd.data 	= (REAL8*) LALMalloc(sizeof(REAL8) * randIn.psd.length); 
   randIn.type = 0;
   randIn.param.order = twoPN;
   randIn.fwdp = fwdp;
   randIn.param.signalAmplitude = 1;
   randIn.param.startPhase = 0;
   randIn.param.nStartPad =  16384;


   fprintf(stderr, "%d %d %d\n",n,bankIn->shf.data->length, strainSegment->length);
   for ( k = 1; k < bankIn->shf.data->length; ++k )
     {    
       randIn.psd.data[k] =  bankIn->shf.data->data[k];
     }
      

      
   LALRandomInspiralSignal(status->statusPtr, &signal, &randIn);
   CHECKSTATUSPTR(status);
   
   


   for ( k = 0; k < n/2; ++k )
     {
       
       strainSegment->data[k].re = signal.data[k] ;
       strainSegment->data[k].im = signal.data[n - k];
     }
   
      
 }
#endif



  LALCheckMemoryLeaks();    




  DETATCHSTATUSPTR( status );
  RETURN( status );  
}



void LALComputeWindowSpectrum(LALStatus *status, 
			      WindowSpectrumIn *param,
			      REAL4FrequencySeries  *spec,
			      REAL4TimeSeries *chan)
{
  
  LALWindowParams               wpars;
  AverageSpectrumParams         avgSpecParams;
  INT4 k;
  
  INITSTATUS( status, "LALCreatRealPsd", BANKEFFICIENCYC );
  ATTATCHSTATUSPTR( status );
  
  /* compute the windowed power spectrum for the data channel */
  avgSpecParams.window = NULL;
  avgSpecParams.plan   = NULL; 
  
  LAL_CALL( LALCreateForwardRealFFTPlan( status->statusPtr, 
					 &(avgSpecParams.plan), 
					 param->numPoints, 0 ),
	    status->statusPtr );
  
  
  switch ( param->specType )
    {
    case 0:
      avgSpecParams.method = useMean;
      if ( vrbflg ) fprintf( stdout, "computing mean psd" );
      break;
    case 1:
      avgSpecParams.method = useMedian;
      if ( vrbflg ) fprintf( stdout, "computing median psd" );
      break;
    case 2:
      avgSpecParams.method = useUnity;
      if ( vrbflg ) fprintf( stdout, "simulation gaussian noise psd" );
      break;
    }
  
  
  wpars.type = Hann;
  wpars.length = param->numPoints;
  avgSpecParams.overlap = param->numPoints / 2;
  if ( vrbflg ) 
    fprintf( stdout, " with overlap %d\n", avgSpecParams.overlap );
  
  LAL_CALL( LALCreateREAL4Window( status->statusPtr, &(avgSpecParams.window),
				  &wpars ), status->statusPtr );
  LAL_CALL( LALREAL4AverageSpectrum( status->statusPtr, spec, chan, &avgSpecParams ),
	    status->statusPtr );
  LAL_CALL( LALDestroyREAL4Window( status->statusPtr, &(avgSpecParams.window) ), 
	    status->statusPtr );
  LAL_CALL( LALDestroyRealFFTPlan( status->statusPtr, &(avgSpecParams.plan) ), status->statusPtr );

  strcpy( spec->name, chan->name );

  if ( param->specType == 2 )
  {
    /* multiply the unit power spectrum to get a gaussian psd */
    REAL4 gaussVarSq = param->gaussVar * param->gaussVar;
    if ( param->inputDeltaT != chan->deltaT )
    {
      /* reduce the variance as we have resampled the data */
      gaussVarSq *= param->inputDeltaT / chan->deltaT;
    }
    for ( k = 0; k < (int)spec->data->length; ++k )
    {
      spec->data->data[k] *= 2.0 * gaussVarSq * (REAL4) chan->deltaT;
    }

    if ( vrbflg ) 
      fprintf( stdout, "set psd to constant value = %e\n", spec->data->data[0] );
  }


  DETATCHSTATUSPTR( status );
  RETURN( status );  

}








void SetInspiralPipelineParam(InspiralPipelineIn *param,
			      RandomInspiralSignalIn randIn)
{

  /* parameters used to generate calibrated power spectrum */
  /*  param->gpsStartTime = {0,0};
    param->gpsEndTime = 0;
  */
  param->padData = 8;                
  param->fqChanName       = NULL;


  param->injectionFile = "/home/cokelaer/Work/TestWaveOverlap/HL-INJECTIONS_1-732005208-2048.xml";
  param->frInCacheName = NULL;

  param->numPoints         = (randIn.psd.length-1)*2;           /* points in a segment          */
  param->numSegments       = 15;           /* number of segments           */
  

  param->inputDataLength =  -1;            
  param->resampFiltType   = 0;           
  param->sampleRate       = randIn.param.tSampling;    
  param->highPass         = 1;           
  param->highPassFreq     = randIn.param.fLower;            
  param->highPassOrder    = 8;           
  param->highPassAtten    = 0.1;         
  param->fLow             = randIn.param.fLower;
  param->specType         = 1;           

  param->calCacheName     = NULL;
  param->pointCal         = 0;            
  param->dynRangeExponent = 40;            
  param->geoHighPassFreq = -1;     
  param->geoHighPassOrder = -1;    
  param->geoHighPassAtten = -1;    
}




void BECreateBank(LALStatus *status, 
		  InspiralCoarseBankIn   *coarseBankIn,	
		  InspiralTemplateList   **list,
		  INT4                   *sizeBank
		  )
{
  Order temp;
  INT4 i;


  INITSTATUS (status, "BECreateBank", BANKEFFICIENCYC);
  ATTATCHSTATUSPTR(status);

  temp  = coarseBankIn->order;
  coarseBankIn->order = 4; 
  
  LAL_CALL(LALInspiralCreateCoarseBank(status->statusPtr, &(*list), sizeBank, *coarseBankIn),
	   status->statusPtr);


  
  if (sizeBank == 0) {
    fprintf(stderr, "BankEfficiency Error :: bank is empty\n");
    exit(0);
  }

  for (i = 0; i < (*sizeBank); i++) {
    (*list)[i].params.order = temp;
  }
  

  DETATCHSTATUSPTR( status );
  RETURN( status );  
}




void BECreatePowerVector(LALStatus              *status, 
			 BEPowerVector          *powerVector,
			 RandomInspiralSignalIn  randIn, 
			 INT4                    length)
{


  INITSTATUS (status, "BECreatePowerVector", BANKEFFICIENCYC);
  ATTATCHSTATUSPTR(status);

  powerVector->fm2_3.length = length / 2 ;
  powerVector->fm5_3.length = length / 2;       
  powerVector->fm7_6.length = length / 2 ;
  powerVector->fm1_2.length = length / 2;       
  powerVector->fm2_3.data   = (REAL4*) LALCalloc(1, sizeof(REAL4) * powerVector->fm2_3.length);
  powerVector->fm5_3.data   = (REAL4*) LALCalloc(1, sizeof(REAL4) * powerVector->fm5_3.length);
  powerVector->fm7_6.data   = (REAL4*) LALCalloc(1, sizeof(REAL4) * powerVector->fm7_6.length);
  powerVector->fm1_2.data   = (REAL4*) LALCalloc(1, sizeof(REAL4) * powerVector->fm1_2.length);
   
  
  /* --- Create useful vector once for all --- */
  LAL_CALL(LALCreateVectorFreqPower(&powerVector->fm5_3, randIn.param, -5, 3), 
	   status->statusPtr);
  LAL_CALL(LALCreateVectorFreqPower(&powerVector->fm2_3, randIn.param, -2, 3), 
	   status->statusPtr);
  LAL_CALL(LALCreateVectorFreqPower(&powerVector->fm7_6, randIn.param, -7, 6), 
	   status->statusPtr);
  LAL_CALL(LALCreateVectorFreqPower(&powerVector->fm1_2, randIn.param, -1, 2), 
	   status->statusPtr);


  DETATCHSTATUSPTR( status );
  RETURN( status );  
}



void LALInspiralOverlapBCV(LALStatus *status,
			   InspiralTemplateList   **list,
			   BEPowerVector          *powerVector,
			   OtherParamIn           *otherIn, 
			   RandomInspiralSignalIn *randIn,
			   INT4                    templateNumber, 
			   REAL4Vector            *FilterBCV1,
			   REAL4Vector            *FilterBCV2,
			   InspiralWaveOverlapIn  *overlapin,
			   OverlapOutputIn        *output,
			   REAL4Vector            *correlation,
			   BEMoments              *moments)
			   

{
  REAL4 fendBCV ;
  REAL4 df;
  INT4 n, kMin, kMax; 

  INITSTATUS (status, "LALInspiralOverlapBCV", BANKEFFICIENCYC);
  ATTATCHSTATUSPTR(status);

  n    = FilterBCV1->length;
  df = randIn->param.tSampling / (REAL8)(n/2) / 2.;      

  kMin  = floor(randIn->param.fLower /df );



  fendBCV = (*list)[templateNumber].params.fFinal ;
  

  /* if we want to test the BCV metric by injectd BCV signal, we 
   * have to get the same final frequnecy for both template and signal */
  if (otherIn->signal == BCV) 
    fendBCV = (*list)[templateNumber].params.fFinal =  randIn->param.fFinal; 
  
  
  /*                 if (otherIn.NoiseModel == REALPSD)
		     {
		     sizeBank =1 ; 
		     list[currentTemplateNumber].params.fFinal =  620;; 
		     list[currentTemplateNumber].params.psi0 =  226783;; 
		     list[currentTemplateNumber].params.psi3 =  -943;; 
		     fendBCV = 620;
		     }
		  */
  overlapin->ifExtOutput = 0;
  overlapin->param.fFinal   =  fendBCV;
  overlapin->param.fCutoff  =  fendBCV;               
  kMax = floor(fendBCV / df);
  
  /* Extract some values a11,a21,a22 parameter for template creation*/

  
  /* Template creation */
  LAL_CALL(LALCreateFilters(FilterBCV1,
			    FilterBCV2,
			    powerVector,
			    moments,
			    kMin,kMax,     
			    (*list)[templateNumber].params.psi0,
			    (*list)[templateNumber].params.psi3
			    ), status->statusPtr);
  
  
  /* The overlap given two filters and the input signal*/
  /*BEInitOverlapOutputIn(&OverlapOutputThisTemplate); */
  /*be sure rhomax = 0 and so on*/
  LAL_CALL(LALWaveOverlapBCV(status->statusPtr, 
			     correlation,
			     overlapin,
			     FilterBCV1, 
			     FilterBCV2, 
			     *otherIn, 
			     output, 
			     moments), 
	   status->statusPtr);
  
  
  DETATCHSTATUSPTR( status );
  RETURN( status );  

}


void
PrintParameters(InspiralCoarseBankIn 	coarse,
		RandomInspiralSignalIn 	rand,
		OtherParamIn    	other)
{

  /*  fprintf(stdout, "coarseBankIn parameters\n"
	          "-----------------------\n");

  fprintf(stdout, "fLower\t\t=\t%f\n"
	  "fUpper\t\t=\t%f\n"
	  "tSampling\t=\t%f\n"
	  "space\t\t=\t%d\n"
	  "mmCoarse\t=\t%f\n" 
	  "mmFine\t\t=\t%f\n" 
	  "iflso\t\t=\t%d\n" 
	  "mMin\t\t=\t%f\n" 
	  "mMax\t\t=\t%f\n" 
	  "MMax\t\t=\t%f\n" 
	  "massRange\t=\t%d\n" 
	  "etamin\t\t=\t%f\n" 
	  "psi0Min\t\t=\t%f\n" 
	  "psi0Max\t\t=\t%f\n" 
	  "psi3Min\t\t=\t%f\n" 
	  "psi3Max\t\t=\t%f\n" 
	  "alpha\t\t=\t%f\n" 
	  "numFcut\t\t=\t%d\n" 
	  "approximant\t=\t%d\n" 
	  "order\t\t=\t%d\n" 
	  "LowGM\t\t=\t%f\n" 
	  "HighGM\t\t=\t%f\n" 
	  "gridType\t=\t%d\n",
	  coarse.fLower,    	
	  coarse.fUpper,    	
	  coarse.tSampling, 	
	  (INT4)(coarse.space),     	
	  coarse.mmCoarse,
	  coarse.mmFine,
	  coarse.iflso,           
	  coarse.mMin,      	
	  coarse.mMax,      	
	  coarse.MMax,      	
	  coarse.massRange,       
	  coarse.etamin, 	        
	  coarse.psi0Min,   	
	  coarse.psi0Max,   	
	  coarse.psi3Min,   	
	  coarse.psi3Max,   	
	  coarse.alpha,     	
	  coarse.numFcutTemplates, 
	  coarse.approximant,     
	  coarse.order,           
	  (REAL8)coarse.LowGM,     	
	  (REAL8)coarse.HighGM,    	
	  (INT4)coarse.gridType);

  
  fprintf(stdout, "bankIn parameters\n"
	  "------------------------\n");

  fprintf(stdout, "useed\t\t=\t%f\n"
	  "type\t\t=\t%f\n"
	  "SignalAmp\t=\t%f\n"
	  "param.order\t=\t%f\n"
	  "param.alpha\t=\t%f\n"
	  "param.ieta\t=\t%f\n"
	  "param.mass1\t=\t%f\n"
	  "param.mass2\t=\t%f\n"
	  "param.fLower\t=\t%f\n"
	  "param.OmegaS\t=\t%f\n"
	  "param.Theta\t=\t%f\n"
	  "mMin\t\t=\t%f\n"
	  "mMax\t\t=\t%f\n"
	  "MMax\t\t=\t%f\n"
	  "etaMin\t\t=\t%f\n"
	  "psi0Min\t\t=\t%f\n"
	  "psi0Max\t\t=\t%f\n"
	  "psi3Min\t\t=\t%f\n"
	  "psi3Max\t\t=\t%f\n"
	  "param.approximant=\t%f\n"
	  "param.tSampling\t=\t%f\n"
	  "param.fCutoff\t=\t%f\n"
	  "param.startTime\t=\t%f\n"
	  "param.startPhase=\t%f\n"
	  "param.nStartPad\t=\t%d\n"
	  "param.signalAmplitude\t=\t%f\n"
	  "param.nEndPad\t=\t%d\n"
	  "NoiseAmp\t=\t%f\n",
	  rand.useed,
	  rand.type,
	  rand.SignalAmp,
	  rand.param.order,
	  rand.param.alpha,
	  rand.param.ieta,
	  rand.param.mass1,
	  rand.param.mass2,
	  rand.param.fLower,
	  rand.param.OmegaS,
	  rand.param.Theta,
	  rand.mMin,
	  rand.mMax,
	  rand.MMax,
	  rand.etaMin,
	  rand.psi0Min,
	  rand.psi0Max,
	  rand.psi3Min,
	  rand.psi3Max,
	  rand.param.approximant,
	  rand.param.tSampling,
	  rand.param.fCutoff,
	  rand.param.startTime,
	  rand.param.startPhase,
	  rand.param.nStartPad,
	  rand.param.signalAmplitude,
	  rand.param.nEndPad,
	  rand.NoiseAmp);

  */

  fprintf(stdout, "otherIn parameters\n"
	  "------------------------\n");
  
   fprintf(stdout,
	  "alphaFConstraint\t%d\t\n"
	  "extraFinalPrinting\t%d\t\n"
	  "template\t\t%d\t\n"
	  "signalfFinal\t\t%f\t\n"
	  "lalDebug\t\t%d\t\n"
	  "signal\t\t\t%d\t\n"
	  "m1\t\t\t%f\t\n"
	  "m2\t\t\t%f\t\n"
	  "numSeconds\t\t%d\t\n"
	  "psi0\t\t%f\t\n"
	  "psi3\t\t%f\t\n"
	  "tau0\t\t%f\t\n"
	  "tau3\t\t%f\t\n" 
	  "PrintBestOverlap\t%d\t\n"	
	  "PrintBestTemplate\t%d\t\n" 	
	  "PrintSNRHisto\t\t%d\t\n"        
	  "PrintPsd\t\t%d\t\n"             
	  "PrintBank\t\t%d\t\n"    	
	  "PrintResultXml\t\t%d\t\n"    	
	  "PrintPrototype\t\t%d\t\n"    	
	  "PrintBankOverlap\t\t%d\t\n"	
	  "faithfulness\t\t%d\t\n"        	
	  "ntrials\t\t%d\t\n" 	     	
	  "FastSimulation\t\t%d\t\n"       
	  "NoiseModel\t\t%d\t\n"           
	  "binaryInjection\t\t%d\t\n"      
	  "maxTotalMass\t\t%f\t\n"         
	  "detector\t\t%d\t\n" 
	  "run\t\t%d\t\n"      
	  "chanName\t\t%s\t\n" 
	  "calCacheName\t\t%s\t\n" 
	  "startTime\t\t%d\t\n"  
	  "numSeconds\t\t%d\t\n" 
	  "L1.chanName\t\t%s\t\n"             
	  "H1.chanName\t\t%s\t\n"             
	  "H2.chanName\t\t%s\t\n"               
	  "L1.dataFile.S3.calCacheName\t%s\t\n"         
	  "L1.dataFile.S3.frInCacheName\t%s\t\n"        
	  "H1.dataFile.S3.calCacheName\t%s\t\n"         
	  "H1.dataFile.S3.frInCacheName\t%s\t\n"        
	  "H2.dataFile.S3.calCacheName\t%s\t\n"         
	  "H2.dataFile.S3.frInCacheName\t%s\t\n"        
	  "L1.dataFile.S2.calCacheName\t%s\t\n"         
	  "L1.dataFile.S2.frInCacheName\t%s\t\n"        
	  "H1.dataFile.S2.calCacheName\t%s\t\n"         
	  "H1.dataFile.S2.frInCacheName\t%s\t\n"        
	  "H2.dataFile.S2.calCacheName\t%s\t\n"         
	  "H2.dataFile.S2.frInCacheName\t%s\t\n",
	  other.alphaFConstraint,
	  other.extraFinalPrinting,
	  other.template,
	  other.signalfFinal,
	  lalDebugLevel,
	  other.signal,
	  other.m1,
	  other.m2,
	  other.numSeconds,
	  other.psi0,
	  other.psi3,
	  other.tau0,
	  other.tau3, 
	  other.PrintBestOverlap,	
	  other.PrintBestTemplate, 	
	  other.PrintSNRHisto,        
	  other.PrintPsd,             
	  other.PrintBank,    	
	  other.PrintResultXml,    	
	  other.PrintPrototype,    	
	  other.PrintBankOverlap,	
	  other.faithfulness,        	
	  other.ntrials, 	     	
	  other.FastSimulation,       
	  other.NoiseModel,           
	  other.binaryInjection,      
	  other.maxTotalMass,         
	  other.detector, 
	  other.run,      
	  other.chanName, 
	  other.calCacheName, 
	  other.startTime,  
	  other.numSeconds, 
	  other.L1.chanName,             
	  other.H1.chanName,             
	  other.H2.chanName,               
	  other.L1.dataFile.S3.calCacheName,         
	  other.L1.dataFile.S3.frInCacheName,        
	  other.H1.dataFile.S3.calCacheName,         
	  other.H1.dataFile.S3.frInCacheName,        
	  other.H2.dataFile.S3.calCacheName,         
	  other.H2.dataFile.S3.frInCacheName,        
	  other.L1.dataFile.S2.calCacheName,         
	  other.L1.dataFile.S2.frInCacheName,        
	  other.H1.dataFile.S2.calCacheName,         
	  other.H1.dataFile.S2.frInCacheName,        
	  other.H2.dataFile.S2.calCacheName,         
	  other.H2.dataFile.S2.frInCacheName);
  



}
