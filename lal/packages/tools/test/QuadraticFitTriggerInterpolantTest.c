/*
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
 * along with with program; see the file COPYING. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * Copyright (C) 2012 Leo Singer
 */


#include <complex.h>
#include <stdlib.h>

#include <lal/TriggerInterpolation.h>

int main(__attribute__((unused)) int argc, __attribute__((unused)) char **argv)
{
    int result;
    double tmax;
    double complex ymax;
    const double complex y[] = {0, 3, 4, 3, 0};

    QuadraticFitTriggerInterpolant *interp = XLALCreateQuadraticFitTriggerInterpolant(2);
    if (!interp)
        exit(EXIT_FAILURE);

    result = XLALApplyQuadraticFitTriggerInterpolant(interp, &tmax, &ymax, &y[2]);
    if (result)
        exit(EXIT_FAILURE);

    if (tmax != 0)
        exit(EXIT_FAILURE);
    if (creal(ymax) != 4)
        exit(EXIT_FAILURE);
    if (cimag(ymax) != 0)
        exit(EXIT_FAILURE);

    XLALDestroyQuadraticFitTriggerInterpolant(interp);
    exit(EXIT_SUCCESS);
}