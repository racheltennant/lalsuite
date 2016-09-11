//
// Copyright (C) 2016 Karl Wette
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with with program; see the file COPYING. If not, write to the
// Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
// MA 02111-1307 USA
//

///
/// \file
/// \ingroup lalapps_pulsar_Weave
///

#include "Weave.h"
#include "LALAppsVCSInfo.h"

#include <lal/LALString.h>
#include <lal/LogPrintf.h>
#include <lal/UserInput.h>
#include <lal/GSLHelpers.h>

int main( int argc, char *argv[] )
{

  // Check VCS information
  XLAL_CHECK_FAIL( XLALAppsVCSInfoCheck() == XLAL_SUCCESS, XLAL_EFUNC );

  // Set help information
  lalUserVarHelpBrief = "compare output files produced by lalapps_Weave";

  ////////// Parse user input //////////

  // Initialise user input variables
  struct uvar_type {
    CHAR *setup_file, *output_file_1, *output_file_2;
    REAL8 param_tol_mism, result_tol_L1, result_tol_L2, result_tol_angle, result_tol_at_max;
  } uvar_struct = {
    .param_tol_mism = 1e-3,
    .result_tol_L1 = 5.5e-2,
    .result_tol_L2 = 4.5e-2,
    .result_tol_angle = 0.04,
    .result_tol_at_max = 5e-2,
  };
  struct uvar_type *const uvar = &uvar_struct;

  // Register user input variables:
  //
  // - Input files
  //
  XLALRegisterUvarMember(
    setup_file, STRING, 'S', REQUIRED,
    "Setup file generated by lalapps_WeaveSetup; the segment list, parameter-space metrics, and other required data. "
    );
  XLALRegisterUvarMember(
    output_file_1, STRING, '1', REQUIRED,
    "First output file produced by lalapps_Weave for comparison. "
    );
  XLALRegisterUvarMember(
    output_file_2, STRING, '2', REQUIRED,
    "Second output file produced by lalapps_Weave for comparison. "
    );
  //
  // - Tolerances
  //
  XLALRegisterUvarMember(
    param_tol_mism, REAL8, 'm', OPTIONAL,
    "Allowed tolerance on mismatch between parameter-space points. "
    "Must be strictly positive. "
    );
  XLALRegisterUvarMember(
    result_tol_L1, REAL8, 'r', OPTIONAL,
    "Allowed tolerance on relative error between mismatch between result vectors using L1 (sum of absolutes) norm. "
    "Must be within range (0,2]. "
    );
  XLALRegisterUvarMember(
    result_tol_L2, REAL8, 's', OPTIONAL,
    "Allowed tolerance on relative error between mismatch between result vectors using L2 (root of sum of squares) norm. "
    "Must be within range (0,2]. "
    );
  XLALRegisterUvarMember(
    result_tol_angle, REAL8, 'a', OPTIONAL,
    "Allowed tolerance on angle between result vectors, in radians. "
    "Must be within range (0,PI]. "
    );
  XLALRegisterUvarMember(
    result_tol_at_max, REAL8, 'x', OPTIONAL,
    "Allowed tolerance on relative errors at maximum component of each result vector. "
    "Must be within range (0,2]. "
    );

  // Parse user input
  XLAL_CHECK_FAIL( xlalErrno == 0, XLAL_EFUNC, "A call to XLALRegisterUvarMember() failed" );
  BOOLEAN should_exit = 0;
  XLAL_CHECK_FAIL( XLALUserVarReadAllInput( &should_exit, argc, argv ) == XLAL_SUCCESS, XLAL_EFUNC );

  // Check user input:
  //
  // - Tolerances
  //
  XLALUserVarCheck( &should_exit,
                    0 < uvar->param_tol_mism,
                    UVAR_STR( mismatch_tol ) " must be strictly positive" );
  XLALUserVarCheck( &should_exit,
                    0 < uvar->result_tol_L1 && uvar->result_tol_L1 <= 2,
                    UVAR_STR( result_tol_L1 ) " must be within range (0,2]" );
  XLALUserVarCheck( &should_exit,
                    0 < uvar->result_tol_L2 && uvar->result_tol_L2 <= 2,
                    UVAR_STR( result_tol_L2 ) " must be within range (0,2]" );
  XLALUserVarCheck( &should_exit,
                    0 < uvar->result_tol_angle && uvar->result_tol_angle <= LAL_PI,
                    UVAR_STR( result_tol_angle ) " must be within range (0,PI]" );
  XLALUserVarCheck( &should_exit,
                    0 < uvar->result_tol_at_max && uvar->result_tol_at_max <= 2,
                    UVAR_STR( result_tol_at_max ) " must be within range (0,2]" );

  // Exit if required
  if ( should_exit ) {
    return EXIT_FAILURE;
  }
  LogPrintf( LOG_NORMAL, "Parsed user input successfully\n" );

  ////////// Load setup data //////////

  // Initialise setup data
  WeaveSetupData XLAL_INIT_DECL( setup );

  {
    // Open setup file
    LogPrintf( LOG_NORMAL, "Opening setup file '%s' for reading ...\n", uvar->setup_file );
    FITSFile *file = XLALFITSFileOpenRead( uvar->setup_file );
    XLAL_CHECK_FAIL( file != NULL, XLAL_EFUNC );

    // Read setup data
    XLAL_CHECK_FAIL( XLALWeaveSetupDataRead( file, &setup ) == XLAL_SUCCESS, XLAL_EFUNC );

    // Close output file
    XLALFITSFileClose( file );
    LogPrintf( LOG_NORMAL, "Closed setup file '%s'\n", uvar->setup_file );
  }

  ////////// Load output results //////////

  // Initialise output results
  WeaveOutputResults *out_1 = NULL, *out_2 = NULL;

  {
    // Open output file #1
    LogPrintf( LOG_NORMAL, "Opening output file '%s' for reading ...\n", uvar->output_file_1 );
    FITSFile *file = XLALFITSFileOpenRead( uvar->output_file_1 );
    XLAL_CHECK_FAIL( file != NULL, XLAL_EFUNC );

    // Read output results
    XLAL_CHECK_FAIL( XLALWeaveOutputResultsReadAppend( file, &out_1 ) == XLAL_SUCCESS, XLAL_EFUNC );

    // Close output file
    XLALFITSFileClose( file );
    LogPrintf( LOG_NORMAL, "Closed output file '%s'\n", uvar->output_file_1 );
  }

  {
    // Open output file #2
    LogPrintf( LOG_NORMAL, "Opening output file '%s' for reading ...\n", uvar->output_file_2 );
    FITSFile *file = XLALFITSFileOpenRead( uvar->output_file_2 );
    XLAL_CHECK_FAIL( file != NULL, XLAL_EFUNC );

    // Read output results
    XLAL_CHECK_FAIL( XLALWeaveOutputResultsReadAppend( file, &out_2 ) == XLAL_SUCCESS, XLAL_EFUNC );

    // Close output file
    XLALFITSFileClose( file );
    LogPrintf( LOG_NORMAL, "Closed output file '%s'\n", uvar->output_file_2 );
  }

  ////////// Compare output results //////////

  // Initalise struct with comparison tolerances
  const VectorComparison result_tol = {
    .relErr_L1 = uvar->result_tol_L1,
    .relErr_L2 = uvar->result_tol_L2,
    .angleV = uvar->result_tol_angle,
    .relErr_atMaxAbsx = uvar->result_tol_at_max,
    .relErr_atMaxAbsy = uvar->result_tol_at_max,
  };

  // Compare output results
  BOOLEAN equal = 0;
  LogPrintf( LOG_NORMAL, "Comparing output files '%s' and '%s ...\n", uvar->output_file_1, uvar->output_file_2 );
  XLAL_CHECK_FAIL( XLALWeaveOutputResultsCompare( &equal, &setup, uvar->param_tol_mism, &result_tol, out_1, out_2 ) == XLAL_SUCCESS, XLAL_EFUNC );
  LogPrintf( LOG_NORMAL, "Output files compare %s\n", equal ? "EQUAL" : "NOT EQUAL" );

  ////////// Cleanup memory and exit //////////

  // Cleanup memory from output results
  XLALWeaveOutputResultsDestroy( out_1 );
  XLALWeaveOutputResultsDestroy( out_2 );

  // Cleanup memory from setup data
  XLALWeaveSetupDataClear( &setup );

  // Cleanup memory from user input
  XLALDestroyUserVars();

  // Check for memory leaks
  LALCheckMemoryLeaks();

  LogPrintf( LOG_NORMAL, "Finished successfully!\n" );

  // Indicate whether output files compared equal
  return equal ? 0 : 1;

XLAL_FAIL:

  // Indicate an error preventing output files from being compared
  return 2;

}

// This function is not required by WeaveCompare.c, but
// must be defined since it is called from OutputResults.c
int XLALWeaveFillOutputToplistItem(
  WeaveOutputToplistItem UNUSED **item,
  BOOLEAN UNUSED *full_init,
  const WeaveSemiResults UNUSED *semi_res,
  const size_t UNUSED freq_idx
  )
{
  XLAL_ERROR( XLAL_EFAILED, "Implementation error" );
}
