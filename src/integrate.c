
/*
 Copyright (C) 2001-2004  the R Development Core Team
 Copyright (C) 2009 M.A.L. Marques

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.
  
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
  
 You should have received a copy of the GNU Lesser General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
  DAG integration from QUADPACK

  This code has been adapted from
  R : A Computer Language for Statistical Data Analysis
*/

#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <util.h>


#define FALSE 0
#define TRUE 1

FLOAT integrate(integr_fn func, void *ex, FLOAT a, FLOAT b)
{
  FLOAT epsabs, epsrel, result, abserr, *alist, *blist, *rlist, *elist;
  int limit, neval, ierr, *iord, last;

#if SINGLE_PRECISION
  epsabs = 1e-5;
  epsrel = 1e-5;
#else
  epsabs = 1e-10;
  epsrel = 1e-10;
#endif

  limit  = 1000;
  alist = (FLOAT *)malloc(limit*sizeof(FLOAT));
  blist = (FLOAT *)malloc(limit*sizeof(FLOAT));
  rlist = (FLOAT *)malloc(limit*sizeof(FLOAT));
  elist = (FLOAT *)malloc(limit*sizeof(FLOAT));
  iord  = (int   *)malloc(limit*sizeof(int));

  rdqagse(func, ex, &a, &b, &epsabs, &epsrel, &limit, &result, &abserr, &neval, &ierr,
	    alist, blist, rlist, elist, iord, &last);

  return result;
}

/* f2c-ed translations + modifications of QUADPACK functions from here down */

static void rdqk21(integr_fn f, void *ex,
		   FLOAT *, FLOAT *, FLOAT *, FLOAT *, FLOAT *, FLOAT *);

static void rdqpsrt(int *, int *, int *, FLOAT *, FLOAT *, int *, int *);

static void rdqelg(int *, FLOAT *, FLOAT *, FLOAT *, FLOAT *, int *);

void rdqagse(integr_fn f, void *ex, FLOAT *a, FLOAT *b, 
	     FLOAT *epsabs, FLOAT *epsrel, int *limit, FLOAT *result,
	     FLOAT *abserr, int *neval, int *ier, FLOAT *alist__,
	     FLOAT *blist, FLOAT *rlist, FLOAT *elist, int *iord, int *last)
{
  /* Local variables */
  int noext, extrap;
  int k,ksgn, nres;
  int ierro;
  int ktmin, nrmax;
  int iroff1, iroff2, iroff3;
  int id;
  int numrl2;
  int jupbnd;
  int maxerr;
  FLOAT res3la[3];
  FLOAT rlist2[52];
  FLOAT abseps, area, area1, area2, area12, dres, epmach;
  FLOAT a1, a2, b1, b2, defabs, defab1, defab2, oflow, uflow, resabs, reseps;
  FLOAT error1, error2, erro12, errbnd, erlast, errmax, errsum;
  
  FLOAT correc = 0.0, erlarg = 0.0, ertest = 0.0, small = 0.0;
  /*
 ***begin prologue  dqagse
 ***date written   800101   (yymmdd)
 ***revision date  830518   (yymmdd)
 ***category no.  h2a1a1
 ***keywords  automatic integrator, general-purpose,
              (end point) singularities, extrapolation,
              globally adaptive
 ***author  piessens,robert,appl. math. & progr. div. - k.u.leuven
            de doncker,elise,appl. math. & progr. div. - k.u.leuven
 ***purpose  the routine calculates an approximation result to a given
             definite integral i = integral of f over (a,b),
             hopefully satisfying following claim for accuracy
             abs(i-result) <= max(epsabs,epsrel*abs(i)).
 ***description

       computation of a definite integral
       standard fortran subroutine
       FLOAT precision version

       parameters
        on entry
           f      - FLOAT precision
                    function subprogram defining the integrand
                    function f(x). the actual name for f needs to be
                    declared e x t e r n a l in the driver program.

           a      - FLOAT precision
                    lower limit of integration

           b      - FLOAT precision
                    upper limit of integration

           epsabs - FLOAT precision
                    absolute accuracy requested
           epsrel - FLOAT precision
                    relative accuracy requested
                    if  epsabs <= 0
                    and epsrel < max(50*rel.mach.acc.,0.5d-28),
                    the routine will end with ier = 6.

           limit  - int
                    gives an upperbound on the number of subintervals
                    in the partition of (a,b)

        on return
           result - FLOAT precision
                    approximation to the integral

           abserr - FLOAT precision
                    estimate of the modulus of the absolute error,
                    which should equal or exceed abs(i-result)

           neval  - int
                    number of integrand evaluations

           ier    - int
                    ier = 0 normal and reliable termination of the
                            routine. it is assumed that the requested
                            accuracy has been achieved.
                    ier > 0 abnormal termination of the routine
                            the estimates for integral and error are
                            less reliable. it is assumed that the
                            requested accuracy has not been achieved.
           error messages
                        = 1 maximum number of subdivisions allowed
                            has been achieved. one can allow more sub-
                            divisions by increasing the value of limit
                            (and taking the according dimension
                            adjustments into account). however, if
                            this yields no improvement it is advised
                            to analyze the integrand in order to
                            determine the integration difficulties. if
                            the position of a local difficulty can be
                            determined (e.g. singularity,
                            discontinuity within the interval) one
                            will probably gain from splitting up the
                            interval at this point and calling the
                            integrator on the subranges. if possible,
                            an appropriate special-purpose integrator
                            should be used, which is designed for
                            handling the type of difficulty involved.
                        = 2 the occurrence of roundoff error is detec-
                            ted, which prevents the requested
                            tolerance from being achieved.
                            the error may be under-estimated.
                        = 3 extremely bad integrand behaviour
                            occurs at some points of the integration
                            interval.
                        = 4 the algorithm does not converge.
                            roundoff error is detected in the
                            extrapolation table.
                            it is presumed that the requested
                            tolerance cannot be achieved, and that the
                            returned result is the best which can be
                            obtained.
                        = 5 the integral is probably divergent, or
                            slowly convergent. it must be noted that
                            divergence can occur with any other value
                            of ier.
                        = 6 the input is invalid, because
                            epsabs <= 0 and
                            epsrel < max(50*rel.mach.acc.,0.5d-28).
                            result, abserr, neval, last, rlist(1),
                            iord(1) and elist(1) are set to zero.
                            alist(1) and blist(1) are set to a and b
                            respectively.

           alist  - FLOAT precision
                    vector of dimension at least limit, the first
                     last  elements of which are the left end points
                    of the subintervals in the partition of the
                    given integration range (a,b)

           blist  - FLOAT precision
                    vector of dimension at least limit, the first
                     last  elements of which are the right end points
                    of the subintervals in the partition of the given
                    integration range (a,b)

           rlist  - FLOAT precision
                    vector of dimension at least limit, the first
                     last  elements of which are the integral
                    approximations on the subintervals

           elist  - FLOAT precision
                    vector of dimension at least limit, the first
                     last  elements of which are the moduli of the
                    absolute error estimates on the subintervals

           iord   - int
                    vector of dimension at least limit, the first k
                    elements of which are pointers to the
                    error estimates over the subintervals,
                    such that elist(iord(1)), ..., elist(iord(k))
                    form a decreasing sequence, with k = last
                    if last <= (limit/2+2), and k = limit+1-last
                    otherwise

           last   - int
                    number of subintervals actually produced in the
                    subdivision process

 ***references  (none)
 ***routines called  dqelg,dqk21,dqpsrt
 ***end prologue  dqagse



           the dimension of rlist2 is determined by the value of
           limexp in subroutine dqelg (rlist2 should be of dimension
           (limexp+2) at least).

           list of major variables
           -----------------------

          alist     - list of left end points of all subintervals
                      considered up to now
          blist     - list of right end points of all subintervals
                      considered up to now
          rlist(i)  - approximation to the integral over
                      (alist(i),blist(i))
          rlist2    - array of dimension at least limexp+2 containing
                      the part of the epsilon table which is still
                      needed for further computations
          elist(i)  - error estimate applying to rlist(i)
          maxerr    - pointer to the interval with largest error
                      estimate
          errmax    - elist(maxerr)
          erlast    - error on the interval currently subdivided
                      (before that subdivision has taken place)
          area      - sum of the integrals over the subintervals
          errsum    - sum of the errors over the subintervals
          errbnd    - requested accuracy max(epsabs,epsrel*
                      abs(result))
          *****1    - variable for the left interval
          *****2    - variable for the right interval
          last      - index for subdivision
          nres      - number of calls to the extrapolation routine
          numrl2    - number of elements currently in rlist2. if an
                      appropriate approximation to the compounded
                      integral has been obtained it is put in
                      rlist2(numrl2) after numrl2 has been increased
                      by one.
          small     - length of the smallest interval considered up
                      to now, multiplied by 1.5
          erlarg    - sum of the errors over the intervals larger
                      than the smallest interval considered up to now
          extrap    - logical variable denoting that the routine is
                      attempting to perform extrapolation i.e. before
                      subdividing the smallest interval we try to
                      decrease the value of erlarg.
          noext     - logical variable denoting that extrapolation
                      is no longer allowed (true value)

           machine dependent constants
           ---------------------------

          epmach is the largest relative spacing.
          uflow is the smallest positive magnitude.
          oflow is the largest positive magnitude. */

  /* ***first executable statement  dqagse */
  /* Parameter adjustments */
  --iord;
  --elist;
  --rlist;
  --blist;
  --alist__;

  /* Function Body */
  epmach = FLOAT_EPSILON;

  /*            test on validity of parameters */
  /*            ------------------------------ */
  *ier = 0;
  *neval = 0;
  *last = 0;
  *result = 0.;
  *abserr = 0.;
  alist__[1] = *a;
  blist[1] = *b;
  rlist[1] = 0.;
  elist[1] = 0.;
  if (*epsabs <= 0. && *epsrel < max(epmach * 50., 5e-29)) {
    *ier = 6;
    return;
  }
  
  /*           first approximation to the integral */
  /*           ----------------------------------- */
  
  uflow = FLOAT_MIN;
  oflow = FLOAT_MAX;
  ierro = 0;
  rdqk21(f, ex, a, b, result, abserr, &defabs, &resabs);
  
  /*           test on accuracy. */
  
  dres = ABS(*result);
  errbnd = max(*epsabs, *epsrel * dres);
  *last = 1;
  rlist[1] = *result;
  elist[1] = *abserr;
  iord[1] = 1;
  if (*abserr <= epmach * 100. * defabs && *abserr > errbnd)
    *ier = 2;
  if (*limit == 1)
    *ier = 1;
  if (*ier != 0 || (*abserr <= errbnd && *abserr != resabs)
      || *abserr == 0.) goto L140;
  
  /*           initialization */
  /*           -------------- */
  
  rlist2[0] = *result;
  errmax = *abserr;
  maxerr = 1;
  area = *result;
  errsum = *abserr;
  *abserr = oflow;
  nrmax = 1;
  nres = 0;
  numrl2 = 2;
  ktmin = 0;
  extrap = FALSE;
  noext = FALSE;
  iroff1 = 0;
  iroff2 = 0;
  iroff3 = 0;
  ksgn = -1;
  if (dres >= (1. - epmach * 50.) * defabs) {
    ksgn = 1;
  }

  /*           main do-loop */
  /*           ------------ */
  
  for (*last = 2; *last <= *limit; ++(*last)) {

    /*           bisect the subinterval with the nrmax-th largest error estimate. */
    
    a1 = alist__[maxerr];
    b1 = (alist__[maxerr] + blist[maxerr]) * .5;
    a2 = b1;
    b2 = blist[maxerr];
    erlast = errmax;
    rdqk21(f, ex, &a1, &b1, &area1, &error1, &resabs, &defab1);
    rdqk21(f, ex, &a2, &b2, &area2, &error2, &resabs, &defab2);
    
    /*           improve previous approximations to integral
		 and error and test for accuracy. */
    
    area12 = area1 + area2;
    erro12 = error1 + error2;
    errsum = errsum + erro12 - errmax;
    area = area + area12 - rlist[maxerr];
    if (defab1 == error1 || defab2 == error2) {
      goto L15;
    }
    if (ABS(rlist[maxerr] - area12) > ABS(area12) * 1e-5 ||
	erro12 < errmax * .99) {
      goto L10;
    }
    if (extrap) {
      ++iroff2;
    }
    if (! extrap) {
      ++iroff1;
    }
  L10:
    if (*last > 10 && erro12 > errmax) {
      ++iroff3;
    }
  L15:
    rlist[maxerr] = area1;
    rlist[*last] = area2;
    errbnd = max(*epsabs, *epsrel * ABS(area));
    
    /*           test for roundoff error and eventually set error flag. */
    
    if (iroff1 + iroff2 >= 10 || iroff3 >= 20)
      *ier = 2;
    if (iroff2 >= 5)
      ierro = 3;
    
    /* set error flag in the case that the number of subintervals equals limit. */
    if (*last == *limit)
      *ier = 1;
    
    /*           set error flag in the case of bad integrand behaviour
		 at a point of the integration range. */
    
    if (max(ABS(a1), ABS(b2)) <=
	(epmach * 100. + 1.) * (ABS(a2) + uflow * 1e3)) {
      *ier = 4;
    }
    
    /*           append the newly-created intervals to the list. */
    
    if (error2 > error1) {
      alist__[maxerr] = a2;
      alist__[*last] = a1;
      blist[*last] = b1;
      rlist[maxerr] = area2;
      rlist[*last] = area1;
      elist[maxerr] = error2;
      elist[*last] = error1;
    } else {
      alist__[*last] = a2;
      blist[maxerr] = b1;
      blist[*last] = b2;
      elist[maxerr] = error1;
      elist[*last] = error2;
    }
    
    /*           call subroutine dqpsrt to maintain the descending ordering
		 in the list of error estimates and select the subinterval
		 with nrmax-th largest error estimate (to be bisected next). */
    
    /*L30:*/
    rdqpsrt(limit, last, &maxerr, &errmax, &elist[1], &iord[1], &nrmax);
    
    if (errsum <= errbnd)   goto L115;/* ***jump out of do-loop */
    if (*ier != 0)	    goto L100;/* ***jump out of do-loop */
    
    if (*last == 2)	    goto L80;
    if (noext)		    goto L90;
    
    erlarg -= erlast;
    if (ABS(b1 - a1) > small) {
      erlarg += erro12;
    }
    if (extrap) {
      goto L40;
    }

    /*           test whether the interval to be bisected next is the
		 smallest interval. */
    
    if (ABS(blist[maxerr] - alist__[maxerr]) > small) {
      goto L90;
    }
    extrap = TRUE;
    nrmax = 2;
  L40:
    if (ierro == 3 || erlarg <= ertest) {
      goto L60;
    }
    
    /*           the smallest interval has the largest error.
		 before bisecting decrease the sum of the errors over the
		 larger intervals (erlarg) and perform extrapolation. */
    
    id = nrmax;
    jupbnd = *last;
    if (*last > *limit / 2 + 2) {
      jupbnd = *limit + 3 - *last;
    }
    for (k = id; k <= jupbnd; ++k) {
      maxerr = iord[nrmax];
      errmax = elist[maxerr];
      if (ABS(blist[maxerr] - alist__[maxerr]) > small) {
	goto L90;/* ***jump out of do-loop */
      }
      ++nrmax;
      /* L50: */
    }
    
    /*           perform extrapolation. */
    
  L60:
    ++numrl2;
    rlist2[numrl2 - 1] = area;
    rdqelg(&numrl2, rlist2, &reseps, &abseps, res3la, &nres);
    ++ktmin;
    if (ktmin > 5 && *abserr < errsum * .001) {
      *ier = 5;
    }
    if (abseps >= *abserr) {
      goto L70;
    }
    ktmin = 0;
    *abserr = abseps;
    *result = reseps;
    correc = erlarg;
    ertest = max(*epsabs, *epsrel * ABS(reseps));
    if (*abserr <= ertest) {
      goto L100;/* ***jump out of do-loop */
    }
    
    /*           prepare bisection of the smallest interval. */
    
  L70:
    if (numrl2 == 1) {
      noext = TRUE;
    }
    if (*ier == 5) {
      goto L100;
    }
    maxerr = iord[1];
    errmax = elist[maxerr];
    nrmax = 1;
    extrap = FALSE;
    small *= .5;
    erlarg = errsum;
    goto L90;
  L80:
    small = ABS(*b - *a) * .375;
    erlarg = errsum;
    ertest = errbnd;
    rlist2[1] = area;
  L90:
    ;
  }
  
  
 L100:/*		set final result and error estimate. */
      /*		------------------------------------ */
  if (*abserr == oflow) 	goto L115;
  if (*ier + ierro == 0) 	goto L110;
  if (ierro == 3)
    *abserr += correc;
  if (*ier == 0)
    *ier = 3;
  if (*result != 0. && area != 0.) goto L105;
  if (*abserr > errsum) 	goto L115;
  if (area == 0.) 		goto L130;
  goto L110;
  
 L105:
  if (*abserr / ABS(*result) > errsum / ABS(area)) {
    goto L115;
  }
  
 L110:/*		test on divergence. */
  if (ksgn == -1 && max(ABS(*result), ABS(area)) <= defabs * .01) {
    goto L130;
  }
  if (.01 > *result / area || *result / area > 100. || errsum > ABS(area)) {
    *ier = 5;
  }
  goto L130;
  
 L115:/*		compute global integral sum. */
  *result = 0.;
  for (k = 1; k <= *last; ++k)
    *result += rlist[k];
  *abserr = errsum;
 L130:
  if (*ier > 2)
  L140:
    *neval = *last * 42 - 21;

  return;
} /* rdqagse_ */


static void rdqelg(int *n, FLOAT *epstab, FLOAT *
		   result, FLOAT *abserr, FLOAT *res3la, int *nres)
{
  /* Local variables */
  int i__, indx, ib, ib2, ie, k1, k2, k3, num, newelm, limexp;
  FLOAT delta1, delta2, delta3, e0, e1, e1abs, e2, e3, epmach, epsinf;
  FLOAT oflow, ss, res;
  FLOAT errA, err1, err2, err3, tol1, tol2, tol3;
  
  /* ***begin prologue  dqelg
 ***refer to  dqagie,dqagoe,dqagpe,dqagse
 ***revision date  830518   (yymmdd)
 ***keywords  epsilon algorithm, convergence acceleration,
            extrapolation
***author  piessens,robert,appl. math. & progr. div. - k.u.leuven
          de doncker,elise,appl. math & progr. div. - k.u.leuven
***purpose  the routine determines the limit of a given sequence of
           approximations, by means of the epsilon algorithm of
           p.wynn. an estimate of the absolute error is also given.
           the condensed epsilon table is computed. only those
           elements needed for the computation of the next diagonal
           are preserved.
***description

          epsilon algorithm
          standard fortran subroutine
          FLOAT precision version

          parameters
             n      - int
                      epstab(n) contains the new element in the
                      first column of the epsilon table.

             epstab - FLOAT precision
                      vector of dimension 52 containing the elements
                      of the two lower diagonals of the triangular
                      epsilon table. the elements are numbered
                      starting at the right-hand corner of the
                      triangle.

             result - FLOAT precision
                      resulting approximation to the integral

             abserr - FLOAT precision
                      estimate of the absolute error computed from
                      result and the 3 previous results

             res3la - FLOAT precision
                      vector of dimension 3 containing the last 3
                      results

             nres   - int
                      number of calls to the routine
                      (should be zero at first call)

***end prologue  dqelg


          list of major variables
          -----------------------

          e0     - the 4 elements on which the computation of a new
          e1       element in the epsilon table is based
          e2
          e3                 e0
                       e3    e1    new
                             e2

          newelm - number of elements to be computed in the new diagonal
          errA   - errA = abs(e1-e0)+abs(e2-e1)+abs(new-e2)
          result - the element in the new diagonal with least value of errA

          machine dependent constants
          ---------------------------

          epmach is the largest relative spacing.
          oflow is the largest positive magnitude.
          limexp is the maximum number of elements the epsilon
          table can contain. if this number is reached, the upper
          diagonal of the epsilon table is deleted. */

/* ***first executable statement  dqelg */
  /* Parameter adjustments */
  --res3la;
  --epstab;

  /* Function Body */
  epmach = FLOAT_EPSILON;
  oflow = FLOAT_MAX;
  ++(*nres);
  *abserr = oflow;
  *result = epstab[*n];
  if (*n < 3) {
    goto L100;
  }
  limexp = 50;
  epstab[*n + 2] = epstab[*n];
  newelm = (*n - 1) / 2;
  epstab[*n] = oflow;
  num = *n;
  k1 = *n;
  for (i__ = 1; i__ <= newelm; ++i__) {
    k2 = k1 - 1;
    k3 = k1 - 2;
    res = epstab[k1 + 2];
    e0 = epstab[k3];
    e1 = epstab[k2];
    e2 = res;
    e1abs = ABS(e1);
    delta2 = e2 - e1;
    err2 = ABS(delta2);
    tol2 = max(ABS(e2), e1abs) * epmach;
    delta3 = e1 - e0;
    err3 = ABS(delta3);
    tol3 = max(e1abs, ABS(e0)) * epmach;
    if (err2 <= tol2 && err3 <= tol3) {
      /*           if e0, e1 and e2 are equal to within machine
		   accuracy, convergence is assumed. */
      *result = res;/*		result = e2 */
      *abserr = err2 + err3;/*	abserr = ABS(e1-e0)+ABS(e2-e1) */
      
      goto L100;	/* ***jump out of do-loop */
    }
    
    e3 = epstab[k1];
    epstab[k1] = e1;
    delta1 = e1 - e3;
    err1 = ABS(delta1);
    tol1 = max(e1abs, ABS(e3)) * epmach;
    
    /*           if two elements are very close to each other, omit
		 a part of the table by adjusting the value of n */
    
    if (err1 > tol1 && err2 > tol2 && err3 > tol3) {
      ss = 1. / delta1 + 1. / delta2 - 1. / delta3;
      epsinf = ABS(ss * e1);
      
      /*           test to detect irregular behaviour in the table, and
		   eventually omit a part of the table adjusting the value of n. */
      
      if (epsinf > 1e-4) {
	goto L30;
      }
    }
    
    *n = i__ + i__ - 1;
    goto L50;/* ***jump out of do-loop */
    
    
  L30:/* compute a new element and eventually adjust the value of result. */
    
    res = e1 + 1. / ss;
    epstab[k1] = res;
    k1 += -2;
    errA = err2 + ABS(res - e2) + err3;
    if (errA <= *abserr) {
      *abserr = errA;
      *result = res;
    }
  }
  
  /*           shift the table. */
  
 L50:
  if (*n == limexp) {
    *n = (limexp / 2 << 1) - 1;
  }
  
  if (num / 2 << 1 == num) ib = 2; else ib = 1;
  ie = newelm + 1;
  for (i__ = 1; i__ <= ie; ++i__) {
    ib2 = ib + 2;
    epstab[ib] = epstab[ib2];
    ib = ib2;
  }
  if (num != *n) {
    indx = num - *n + 1;
    for (i__ = 1; i__ <= *n; ++i__) {
      epstab[i__] = epstab[indx];
      ++indx;
    }
  }
  /*L80:*/
  if (*nres >= 4) {
    /* L90: */
    *abserr = ABS(*result - res3la[3]) +
      ABS(*result - res3la[2]) +
      ABS(*result - res3la[1]);
    res3la[1] = res3la[2];
    res3la[2] = res3la[3];
    res3la[3] = *result;
  } else {
    res3la[*nres] = *result;
    *abserr = oflow;
  }
  
 L100:/* compute error estimate */
  *abserr = max(*abserr, epmach * 5. * ABS(*result));
  return;
} /* rdqelg_ */

static void  rdqk21(integr_fn f, void *ex, FLOAT *a, FLOAT *b, FLOAT *result,
		    FLOAT *abserr, FLOAT *resabs, FLOAT *resasc)
{
  /* Initialized data */
  
  static FLOAT wg[5] = { .066671344308688137593568809893332,
			  .149451349150580593145776339657697,
			  .219086362515982043995534934228163,
			  .269266719309996355091226921569469,
			  .295524224714752870173892994651338 };
  static FLOAT xgk[11] = { .995657163025808080735527280689003,
			    .973906528517171720077964012084452,
			    .930157491355708226001207180059508,
			    .865063366688984510732096688423493,
			    .780817726586416897063717578345042,
			    .679409568299024406234327365114874,
			    .562757134668604683339000099272694,
			    .433395394129247190799265943165784,
			    .294392862701460198131126603103866,
			    .14887433898163121088482600112972,0. };
  static FLOAT wgk[11] = { .011694638867371874278064396062192,
			    .03255816230796472747881897245939,
			    .05475589657435199603138130024458,
			    .07503967481091995276704314091619,
			    .093125454583697605535065465083366,
			    .109387158802297641899210590325805,
			    .123491976262065851077958109831074,
			    .134709217311473325928054001771707,
			    .142775938577060080797094273138717,
			    .147739104901338491374841515972068,
			    .149445554002916905664936468389821 };


  /* Local variables */
  FLOAT fv1[10], fv2[10], vec[21];
  FLOAT absc, resg, resk, fsum, fval1, fval2;
  FLOAT hlgth, centr, reskh, uflow;
  FLOAT fc, epmach, dhlgth;
  int j, jtw, jtwm1;
  
/* ***begin prologue  dqk21
***date written   800101   (yymmdd)
***revision date  830518   (yymmdd)
***category no.  h2a1a2
***keywords  21-point gauss-kronrod rules
***author  piessens,robert,appl. math. & progr. div. - k.u.leuven
          de doncker,elise,appl. math. & progr. div. - k.u.leuven
***purpose  to compute i = integral of f over (a,b), with error
                          estimate
                      j = integral of abs(f) over (a,b)
***description

          integration rules
          standard fortran subroutine
          FLOAT precision version

          parameters
           on entry
             f      - FLOAT precision
                      function subprogram defining the integrand
                      function f(x). the actual name for f needs to be
                      declared e x t e r n a l in the driver program.

             a      - FLOAT precision
                      lower limit of integration

             b      - FLOAT precision
                      upper limit of integration

           on return
             result - FLOAT precision
                      approximation to the integral i
                      result is computed by applying the 21-point
                      kronrod rule (resk) obtained by optimal addition
                      of abscissae to the 10-point gauss rule (resg).

             abserr - FLOAT precision
                      estimate of the modulus of the absolute error,
                      which should not exceed abs(i-result)

             resabs - FLOAT precision
                      approximation to the integral j

             resasc - FLOAT precision
                      approximation to the integral of abs(f-i/(b-a))
                      over (a,b)

***references  (none)
***end prologue  dqk21



          the abscissae and weights are given for the interval (-1,1).
          because of symmetry only the positive abscissae and their
          corresponding weights are given.

          xgk    - abscissae of the 21-point kronrod rule
                   xgk(2), xgk(4), ...  abscissae of the 10-point
                   gauss rule
                   xgk(1), xgk(3), ...  abscissae which are optimally
                   added to the 10-point gauss rule

          wgk    - weights of the 21-point kronrod rule

          wg     - weights of the 10-point gauss rule


gauss quadrature weights and kronron quadrature abscissae and weights
as evaluated with 80 decimal digit arithmetic by l. w. fullerton,
bell labs, nov. 1981.





          list of major variables
          -----------------------

          centr  - mid point of the interval
          hlgth  - half-length of the interval
          absc   - abscissa
          fval*  - function value
          resg   - result of the 10-point gauss formula
          resk   - result of the 21-point kronrod formula
          reskh  - approximation to the mean value of f over (a,b),
                   i.e. to i/(b-a)


          machine dependent constants
          ---------------------------

          epmach is the largest relative spacing.
          uflow is the smallest positive magnitude. */

/* ***first executable statement  dqk21 */
  epmach = FLOAT_EPSILON;
  uflow = FLOAT_MIN;
  
  centr = (*a + *b) * .5;
  hlgth = (*b - *a) * .5;
  dhlgth = ABS(hlgth);

  /*           compute the 21-point kronrod approximation to
	       the integral, and estimate the absolute error. */
  
  resg = 0.;
  vec[0] = centr;
  for (j = 1; j <= 5; ++j) {
    jtw = j << 1;
    absc = hlgth * xgk[jtw - 1];
    vec[(j << 1) - 1] = centr - absc;
    /* L5: */
    vec[j * 2] = centr + absc;
  }
  for (j = 1; j <= 5; ++j) {
    jtwm1 = (j << 1) - 1;
    absc = hlgth * xgk[jtwm1 - 1];
    vec[(j << 1) + 9] = centr - absc;
    vec[(j << 1) + 10] = centr + absc;
  }
  f(vec, 21, ex);
  fc = vec[0];
  resk = wgk[10] * fc;
  *resabs = ABS(resk);
  for (j = 1; j <= 5; ++j) {
    jtw = j << 1;
    absc = hlgth * xgk[jtw - 1];
    fval1 = vec[(j << 1) - 1];
    fval2 = vec[j * 2];
    fv1[jtw - 1] = fval1;
    fv2[jtw - 1] = fval2;
    fsum = fval1 + fval2;
    resg += wg[j - 1] * fsum;
    resk += wgk[jtw - 1] * fsum;
    *resabs += wgk[jtw - 1] * (ABS(fval1) + ABS(fval2));
    /* L10: */
  }
  for (j = 1; j <= 5; ++j) {
    jtwm1 = (j << 1) - 1;
    absc = hlgth * xgk[jtwm1 - 1];
    fval1 = vec[(j << 1) + 9];
    fval2 = vec[(j << 1) + 10];
    fv1[jtwm1 - 1] = fval1;
    fv2[jtwm1 - 1] = fval2;
    fsum = fval1 + fval2;
    resk += wgk[jtwm1 - 1] * fsum;
    *resabs += wgk[jtwm1 - 1] * (ABS(fval1) + ABS(fval2));
    /* L15: */
  }
  reskh = resk * .5;
  *resasc = wgk[10] * ABS(fc - reskh);
  for (j = 1; j <= 10; ++j) {
    *resasc += wgk[j - 1] * (ABS(fv1[j - 1] - reskh) +
			     ABS(fv2[j - 1] - reskh));
    /* L20: */
  }
  *result = resk * hlgth;
  *resabs *= dhlgth;
  *resasc *= dhlgth;
  *abserr = ABS((resk - resg) * hlgth);
  if (*resasc != 0. && *abserr != 0.) {
    *abserr = *resasc * min(1., pow(*abserr * 200. / *resasc, 1.5));
  }
  if (*resabs > uflow / (epmach * 50.)) {
    *abserr = max(epmach * 50. * *resabs, *abserr);
  }
  return;
} /* rdqk21_ */


static void rdqpsrt(int *limit, int *last, int *maxerr,
		    FLOAT *ermax, FLOAT *elist, int *iord, int *nrmax)
{
  /* Local variables */
  int i, j, k, ido, jbnd, isucc, jupbn;
  FLOAT errmin, errmax;

/* ***begin prologue  dqpsrt
 ***refer to  dqage,dqagie,dqagpe,dqawse
 ***routines called  (none)
 ***revision date  810101   (yymmdd)
 ***keywords  sequential sorting
 ***author  piessens,robert,appl. math. & progr. div. - k.u.leuven
           de doncker,elise,appl. math. & progr. div. - k.u.leuven
 ***purpose  this routine maintains the descending ordering in the
            list of the local error estimated resulting from the
            interval subdivision process. at each call two error
            estimates are inserted using the sequential search
            method, top-down for the largest error estimate and
            bottom-up for the smallest error estimate.
 ***description

           ordering routine
           standard fortran subroutine
           FLOAT precision version

           parameters (meaning at output)
              limit  - int
                       maximum number of error estimates the list
                       can contain

              last   - int
                       number of error estimates currently in the list

              maxerr - int
                       maxerr points to the nrmax-th largest error
                       estimate currently in the list

              ermax  - FLOAT precision
                       nrmax-th largest error estimate
                       ermax = elist(maxerr)

              elist  - FLOAT precision
                       vector of dimension last containing
                       the error estimates

              iord   - int
                       vector of dimension last, the first k elements
                       of which contain pointers to the error
                       estimates, such that
                       elist(iord(1)),...,  elist(iord(k))
                       form a decreasing sequence, with
                       k = last if last <= (limit/2+2), and
                       k = limit+1-last otherwise

              nrmax  - int
                       maxerr = iord(nrmax)

***end prologue  dqpsrt
*/


  /* Parameter adjustments */
  --iord;
  --elist;
  
  /* Function Body */
  
  /*           check whether the list contains more than
	       two error estimates. */
  if (*last <= 2) {
    iord[1] = 1;
    iord[2] = 2;
    goto Last;
  }
  /*           this part of the routine is only executed if, due to a
	       difficult integrand, subdivision increased the error
	       estimate. in the normal case the insert procedure should
	       start after the nrmax-th largest error estimate. */
  
  errmax = elist[*maxerr];
  if (*nrmax > 1) {
    ido = *nrmax - 1;
    for (i = 1; i <= ido; ++i) {
      isucc = iord[*nrmax - 1];
      if (errmax <= elist[isucc])
	break; /* out of for-loop */
      iord[*nrmax] = isucc;
      --(*nrmax);
      /* L20: */
    }
  }
  
  /*L30:       compute the number of elements in the list to be maintained
    in descending order. this number depends on the number of
    subdivisions still allowed. */
  if (*last > *limit / 2 + 2)
    jupbn = *limit + 3 - *last;
  else
    jupbn = *last;
  
  errmin = elist[*last];
  
  /*           insert errmax by traversing the list top-down,
	       starting comparison from the element elist(iord(nrmax+1)). */
  
  jbnd = jupbn - 1;
  for (i = *nrmax + 1; i <= jbnd; ++i) {
    isucc = iord[i];
    if (errmax >= elist[isucc]) {/* ***jump out of do-loop */
      /* L60: insert errmin by traversing the list bottom-up. */
      iord[i - 1] = *maxerr;
      for (j = i, k = jbnd; j <= jbnd; j++, k--) {
	isucc = iord[k];
	if (errmin < elist[isucc]) {
	  /* goto L80; ***jump out of do-loop */
	  iord[k + 1] = *last;
	  goto Last;
	}
	iord[k + 1] = isucc;
      }
      iord[i] = *last;
      goto Last;
    }
    iord[i - 1] = isucc;
  }
  
  iord[jbnd] = *maxerr;
  iord[jupbn] = *last;
  
 Last:/* set maxerr and ermax. */
  
  *maxerr = iord[*nrmax];
  *ermax = elist[*maxerr];
  return;
} /* rdqpsrt_ */