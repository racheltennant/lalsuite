/*Need to include the appropiate headers*/

/*We have found that LALSimIMR.h is in the same directory*/
#include "LALSimIMR.h"
#include <lal/SphericalHarmonics.h>
#include <lal/Sequence.h>
#include <lal/Date.h>
#include <lal/Units.h>
#include <lal/LALConstants.h>
#include <lal/FrequencySeries.h>
#include <lal/LALDatatypes.h>
#include <lal/LALStdlib.h> 

/*I think this is not the right way of specifying the path*/
#include <lal/XLALError.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*#include "LALSimIMRPhenomXHM_internals.h"*/
/*#include "LALSimIMRPhenomXHM_internals.c"*/

/*#include "LALSimIMRPhenomXHM_structs.h"
#include "LALSimIMRPhenomXHM_qnm.h"
#include "LALSimIMRPhenomXHM_multiband.c"

#include "LALSimIMRPhenomXPHM.c" */

/*Is this what we meant by Debug function?*/
#ifndef _OPENMP
#define omp ignore
#endif

#define L_MAX 4

#ifndef PHENOMXHMDEBUG
#define DEBUG 0
#else
#define DEBUG 1 //print debugging info
#endif

#ifndef PHENOMXHMMBAND
#define MBAND 0
#else
#define MBAND PHENOMXHMMBAND //use multibanding
#endif


/*Need to initialize LALDict sturcture before using it*/
int main() {
    LALDict *lalParams = XLALCreateDict();
    XLALSimInspiralWaveformParamsInsert(params, "lambda_grav", 5.0);

    return 0;
}


/*XLALSimInspiralWaveformParamsInsertModeArray(lalParams, ) */

/*LALDict *params = XLALSimInspiralWaveformParamsFromLALDict(LAL_SIM_INSPIRAL_APPROXIMANT_IMRPhenomXHM);
/** Return hptilde and hctilde from a sum of modes */
/*int main() {
    /*Declare input variables
    REAL8 m1_SI = 2.6;                        /**< primary mass [kg] */
    /*double m2_SI = 23.0;    */                    /**< secondary mass [kg] */
    /*double chi1z = 0.4;                        /**< TO CHECK aligned spin of primary */
    /*double chi2z = 0.3;                        /**< TO CHECK aligned spin of secondary */
    /*double f_min = 50;                         /**< Starting GW frequency (Hz) */
    /*double f_max = 300;                         /**< End frequency; 0 defaults to Mf = 0.3 */
    /*double deltaF = 2048;                       /**< Sampling frequency (Hz) */
    /*double distance = 3.086e+22;                /**< distance of source (m) */
    /*double inclination = 0;                  /**< inclination of source (rad) */
    /*double phiRef = 1.3;                       /**< reference orbital phase (rad) */
    /*double fRef_In = 50;                      /**< Reference frequency */
    /*LALDict *lalParams                  /**< LALDict struct */ 
  /*Declare output variable*/
  /*  MultiModeWaveform *waveform;  *//*I think that this line is not necessary?*/

  /*Call the function*/

  /*I think that we need something like: 
  COMPLEX16FrequencySeries *hptilde = 
  COMPLEX16FrequencySeries *hctilde =*/
   /*MultiModeWaveform *waveform = IMRPhenomXHM_MultiMode(m1_SI, m2_SI, chi1z, chi2z, f_min, f_max, deltaF, distance, inclination, phiRef, fRef_In, *lalparams);

    return 0;
} */

/*Question*/
/*What about IMRPhenomXHM Parameters defined in line 180 in LALSimInspiralWaveformParams*/