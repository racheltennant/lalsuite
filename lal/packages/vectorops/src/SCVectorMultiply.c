/*----------------------------------------------------------------------- 
 * 
 * File Name: SCVectorMultiply.c
 * 
 * Author: Creighton, J. D. E.
 * 
 * Revision: $Id$
 * 
 *----------------------------------------------------------------------- 
 * 
 * NAME 
 * SCVectorMultiply
 * 
 * SYNOPSIS 
 *
 *
 * DESCRIPTION 
 * Multiply a real vector by a complex vectors.
 * 
 * DIAGNOSTICS 
 * Null pointer, invalid input size, size mismatch.
 *
 * CALLS
 * 
 * NOTES
 * 
 *-----------------------------------------------------------------------
 */

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

#ifndef _VECTOROPS_H
#include "VectorOps.h"
#ifndef _VECTOROPS_H
#define _VECTOROPS_H
#endif
#endif

NRCSID (SCVECTORMULTIPLYC, "$Id$");

void
SCVectorMultiply (
    Status               *status,
    COMPLEX8Vector       *out,
    const REAL4Vector    *in1,
    const COMPLEX8Vector *in2
    )
{
  REAL4    *a;
  COMPLEX8 *b;
  COMPLEX8 *c;
  INT4      n;

  INITSTATUS (status, SCVECTORMULTIPLYC);

  ASSERT (out, status, VECTOROPS_ENULL, VECTOROPS_MSGENULL);
  ASSERT (in1, status, VECTOROPS_ENULL, VECTOROPS_MSGENULL);

  ASSERT (in2, status, VECTOROPS_ENULL, VECTOROPS_MSGENULL);

  ASSERT (out->data, status, VECTOROPS_ENULL, VECTOROPS_MSGENULL);
  ASSERT (in1->data, status, VECTOROPS_ENULL, VECTOROPS_MSGENULL);
  ASSERT (in2->data, status, VECTOROPS_ENULL, VECTOROPS_MSGENULL);

  ASSERT (out->length > 0, status, VECTOROPS_ESIZE, VECTOROPS_MSGESIZE);
  ASSERT (in1->length == out->length, status,
          VECTOROPS_ESZMM, VECTOROPS_MSGESZMM);
  ASSERT (in2->length == out->length, status,
          VECTOROPS_ESZMM, VECTOROPS_MSGESZMM);

  a = in1->data;
  b = in2->data;
  c = out->data;
  n = out->length;

  while (n-- > 0)
  {
    REAL4 fac = *a;
    REAL4 br  = b->re;
    REAL4 bi  = b->im;

    c->re = fac*br;
    c->im = fac*bi;

    ++a;
    ++b;
    ++c;
  }

  RETURN (status);
}

