/*----------------------------------------------------------------------- 
 * 
 * File Name: VectorOpsTest.c
 * 
 * Author: Creighton, J. D. E., Sintes, A. M.
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
 *   Test suite for vector operations
 * 
 * DIAGNOSTICS
 * 
 * CALLS
 *   CCVectorDivide
 *   CCVectorMultiply
 *   CCVectorMultiplyConjugate
 *   SCVectorMultiply
 *   SSVectorMultiply
 * 
 * NOTES
 * 
 *-----------------------------------------------------------------------
 */

#include <stdio.h>
#include <math.h>
#include "LALStdlib.h"
#include "LALConstants.h"
#include "AVFactories.h"
#include "VectorOps.h"

NRCSID (MAIN, "$Id$");

int debuglevel = 2;

int
main ()
{
  const int size = 8;
  COMPLEX8Vector *z1 = NULL;
  COMPLEX8Vector *z2 = NULL;
  COMPLEX8Vector *z3 = NULL;
  REAL4Vector    *x1 = NULL;
  REAL4Vector    *x2 = NULL;
  REAL4Vector    *x3 = NULL;
  REAL4Vector    *y1 = NULL;
  REAL4Vector    *y2 = NULL;
  REAL4Vector    *y3 = NULL;
  static Status   status;
  INT4            i;

  CCreateVector(&status, &z1, size);
  CCreateVector(&status, &z2, size);
  CCreateVector(&status, &z3, size);
  SCreateVector(&status, &x1, size);
  SCreateVector(&status, &x2, size);
  SCreateVector(&status, &x3, size);
  SCreateVector(&status, &y1, size/2);
  y2         = (REAL4Vector *)LALMalloc(sizeof(REAL4Vector));
  y2->data   = NULL;
  y2->length = size;
  y3         = (REAL4Vector *)LALMalloc(sizeof(REAL4Vector));
  y3->data   = (REAL4 *)LALMalloc(size*sizeof(REAL4));
  y3->length = 0;

  for (i = 0; i < size; ++i)
  {
    z1->data[i].re = 1 + i;
    z1->data[i].im = 2 + i*i;
    z2->data[i].re = 3 + i + i*i*i;
    z2->data[i].im = 4 + i*i + i*i*i;
    x1->data[i]    = 5 + i + i*i;
    x2->data[i]    = 6 + i + i*i + i*i*i;
  }

  printf("\n");
  CCVectorMultiply(&status, z3, z1, z2);
  for (i = 0; i < size; ++i)
    printf("(% 6.0f,% 6.0f) x (% 6.0f,% 6.0f) = (% 6.0f,% 6.0f)\n",
           z1->data[i].re, z1->data[i].im,
           z2->data[i].re, z2->data[i].im,
           z3->data[i].re, z3->data[i].im);

  printf("\n");
  CCVectorMultiplyConjugate(&status, z3, z1, z2);
  for (i = 0; i < size; ++i)
    printf("(% 6.0f,% 6.0f) x (% 6.0f,% 6.0f)* = (% 6.0f,% 6.0f)\n",
           z1->data[i].re, z1->data[i].im,
           z2->data[i].re, z2->data[i].im,
           z3->data[i].re, z3->data[i].im);

  printf("\n");
  CCVectorDivide(&status, z3, z1, z2);
  for (i = 0; i < size; ++i)
    printf("(% 6.0f,% 6.0f) / (% 6.0f,% 6.0f) = (% 9.6f,% 9.6f)\n",
           z1->data[i].re, z1->data[i].im,
           z2->data[i].re, z2->data[i].im,
           z3->data[i].re, z3->data[i].im);

  printf("\n");
  SCVectorMultiply(&status, z3, x1, z1);
  for (i = 0; i < size; ++i)
    printf("% 6.0f x (% 6.0f,% 6.0f) = (% 6.0f,% 6.0f)\n",
           x1->data[i],
           z1->data[i].re, z1->data[i].im,
           z3->data[i].re, z3->data[i].im);

  printf("\n");
  SSVectorMultiply(&status, x3, x1, x2);
  for (i = 0; i < size; ++i)
    printf("% 6.0f x % 6.0f = % 6.0f\n",
           x1->data[i], x2->data[i], x3->data[i]);

  printf("\n");
  SSVectorMultiply(&status, x3, x1, NULL);
  SSVectorMultiply(&status, x3, y2, x2);
  SSVectorMultiply(&status, y3, x1, x2);
  SSVectorMultiply(&status, x3, x1, y1);

  CDestroyVector(&status, &z1);
  CDestroyVector(&status, &z2);
  CDestroyVector(&status, &z3);
  SDestroyVector(&status, &x1);
  SDestroyVector(&status, &x2);
  SDestroyVector(&status, &x3);
  SDestroyVector(&status, &y1);
  LALFree(y2);
  LALFree(y3->data);
  LALFree(y3);

  x1 = x2 = x3 = y1 = y2 = y3 = NULL;
  z1 = z2 = z3 = NULL;
  
  
  CCreateVector(&status, &z1, size);
  
  SCreateVector(&status, &x1, size);
  SCreateVector(&status, &x2, size);
  SCreateVector(&status, &x3, size);
  
  
   for (i = 0; i < size; ++i)
  {
    z1->data[i].re = (12.0 + i) *cos(LAL_PI/3.0*i);
    z1->data[i].im = (12.0 + i )*sin(LAL_PI/3.0*i);
  }
  
   printf("\n"); 
   CVectorAbs(&status, x1, z1);  
   for (i = 0; i < size; ++i)
    printf(" Abs(% f,%f)  = %f \n",
           z1->data[i].re, z1->data[i].im,
           x1->data[i]);
           
    CVectorAngle(&status, x2, z1);
    for (i = 0; i < size; ++i)    
     printf(" Angle(%f,%f)  = %f \n",
           z1->data[i].re, z1->data[i].im,
           x2->data[i]);
 
    UnwrapREAL4Angle(&status, x3, x2); 
     for (i = 0; i < size; ++i)    
     printf(" Unwrap Phase Angle ( %f )  = %f \n",
           x2->data[i],
           x3->data[i]);
  
  
  SCreateVector(&status, &y1, size/2);
  
  y2         = (REAL4Vector *)LALMalloc(sizeof(REAL4Vector));
  y2->data   = NULL;
  y2->length = size;
  
  y3         = (REAL4Vector *)LALMalloc(sizeof(REAL4Vector));
  y3->data   = (REAL4 *)LALMalloc(size*sizeof(REAL4));
  y3->length = 0;

  printf("\n");
  
    CVectorAbs(&status, x1, NULL);
    CVectorAbs(&status, NULL, z1);
    CVectorAbs(&status, y1, z1);
    CVectorAbs(&status, y2, z1);
    CVectorAbs(&status, y3, z1);
    
    
    CVectorAngle(&status, x2, NULL);
    CVectorAngle(&status, NULL, z1);
    CVectorAngle(&status, y1, z1);
    CVectorAngle(&status, y2, z1);
    CVectorAngle(&status, y3, z1);
    
    UnwrapREAL4Angle(&status, x3, NULL);   
    UnwrapREAL4Angle(&status, NULL, x2);   
    UnwrapREAL4Angle(&status, y1, x2);   
    UnwrapREAL4Angle(&status, y2, x2);   
    UnwrapREAL4Angle(&status, y3, x2);   
    UnwrapREAL4Angle(&status, x2, x2);   
 

  CDestroyVector(&status, &z1);
 
  SDestroyVector(&status, &x1);
  SDestroyVector(&status, &x2);
  SDestroyVector(&status, &x3);
  
  SDestroyVector(&status, &y1);
  LALFree(y2);
  LALFree(y3->data);
  LALFree(y3);

  return 0;
}
