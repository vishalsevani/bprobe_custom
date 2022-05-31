/*

   utility routines - 

   allocate and free matricies and vectors (from Numerical Recipes)

*/

#include <stdio.h>
#include <stdlib.h>

static char rcsid[] = "$Id: util.c,v 1.4 1996/07/09 15:52:44 carter Exp $";

float **matrix(nrl, nrh, ncl, nch)
int nrl, nrh, ncl, nch;
/* Allocates a float matrix with range [nrl..nrh][ncl..nch] */
{
     int i;
     float **m;

     /* allocate pointers to rows */
     m = (float **) malloc ((unsigned) (nrh-nrl+1)*sizeof(float*));
     if (!m) printf("Allocation failure 1 in matrix()");
     m -= nrl;
     
     /* allocate rows and set pointers to them */
     for (i=nrl; i<=nrh; i++)
     {
	  m[i] = (float *) malloc((unsigned) (nch-ncl+1)*sizeof(float));
	  if (!m[i]) printf("Allocation failure 2 in matrix()");
	  m[i] -= ncl;
     }
     return m;
}

void free_matrix(m, nrl, nrh, ncl, nch)
float **m;
int nrl, nrh, ncl, nch;
/* Frees a matrix allocated with matrix() */
{
     int i;
     
     for (i=nrh; i>=nrl; i--)
	  free((char*) (m[i]+ncl));
     free((char *) (m+nrl));
}

int compare_floats(const void *vp, const void *vq) {
  float *p = (float *)vp, *q = (float *)vq;

  return((*p == *q) ? 0 : (*p < *q) ? -1 : +1);
}

void calculateUtilization(float **BwEstimateMatrix, 
			  float **interDepartureMatrix,
			  float **packetSizeMatrix,
			  int rows, int cols,
			   float baseBW, 
			   float *returnAvg, float *returnMedian) {
/* takes a matrix of bandwidth estimates

     gap1  gap2  gap3 ...
     --------------------
small|
     |
probe|
size |
     |
large|

     the best base bandwidth estimate

     and the dimensions of the matrix

RETURNS:  a utilization estimate (0.0 if no estimate can be made)

*******************************************/

  int i, j, numUtilMeasurements = 0;
  int center;
  float *Utils, sumUtils, avgUtil, medianUtil = 0.0f;

  *returnAvg = *returnMedian = 0.0;

  Utils = (float *) malloc (rows * cols * sizeof(float));

  for (i=0; i< rows; i++) {
    for (j=0; j< cols; j++) {

#ifdef DEBUG
      printf("barf[%d][%d] (%10.4f,%10.4f,%10.4f)", 
	     i, j,
	     BwEstimateMatrix[i][j],
	     interDepartureMatrix[i][j],
	     packetSizeMatrix[i][j]); 
#endif	   

      if ((BwEstimateMatrix[i][j] > 0.0) &&
	  /* throw out any obvious over-estimates */
	  (BwEstimateMatrix[i][j] < baseBW) &&
	  /* check that offered load was high enough to saturate */
	  (((packetSizeMatrix[i][j] * 8.0 * 1000000.0) / 
	    interDepartureMatrix[i][j]) > baseBW)) {
	/* compute the harmonic mean */
	/* save all the reciprocals */
#ifdef DEBUG	
	printf("->accepted\n");
#endif
	Utils[numUtilMeasurements++] = 1.0/BwEstimateMatrix[i][j];
      }
      else {
#ifdef DEBUG	
	printf("->REJECTED\n");
#endif
      }
    } /* end loop over columns of bw estimates matrix */
  } /* end loop over rows of bw estimates matrix */

  if (numUtilMeasurements > 1) { 
    qsort(Utils, numUtilMeasurements, sizeof(float),compare_floats); 
    
    /* sum the reciprocals */
    sumUtils = 0.0;
    for (i = 0; i < numUtilMeasurements; i++) {
/*      printf("utils[%d]: %10.8f\n", i, Utils[i]);*/
      sumUtils += Utils[i];
    }

    /* this is now the Harmonic mean */
    avgUtil = (float)numUtilMeasurements/sumUtils;

#if MEDIAN    
    center = (int) numUtilMeasurements / 2;
    if ((numUtilMeasurements % 2) == 0) {
      medianUtil = (Utils[center] + Utils[center-1]) / 2.0;
    }
    else { 
      medianUtil = Utils[center];
    }
#endif
    

    free(Utils);
    
    *returnAvg = avgUtil;
    *returnMedian = medianUtil;
  }
}


#ifdef TESTPKG
void main() {
  int rows = 7, columns = 10, dataPoints = 0;

  float testData0[7][10] = {
    { 5.16e+06, 1.55e+06, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00 },
    { 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00 },
    { 8.21e+06, 2.63e+06, 1.62e+06, 1.63e+05, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00 },
    { 1.06e+07, 8.98e+06, 4.05e+05, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00 },
    { 9.80e+06, 9.77e+06, 9.47e+06, 9.30e+06, 9.24e+06, 6.49e+05, 3.97e+05, 0.00e+00, 0.00e+00, 0.00e+00 },
    { 1.57e+07, 1.47e+06, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00 },
    { 2.47e+06, 9.13e+05, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00, 0.00e+00 }};

  float testDataA[7][10] = {
    {  8.83436e+06,  8.47059e+06,  8.27586e+06,  7.70054e+06,  7.70054e+06,  4.41718e+06,  5.19856e+05,  0.00000e+00,  0.00000e+00,  0.00000e+00 },
    {  1.26010e+07,  8.90843e+06,  8.27211e+06,  8.16107e+06,  7.94771e+06,  7.81994e+06,  7.41463e+06,  6.98851e+05,  0.00000e+00,  0.00000e+00}, 
    {  9.25786e+06,  9.20000e+06,  9.12397e+06,  9.12397e+06,  9.10516e+06,  8.34783e+06,  1.62652e+06,  1.23697e+06,  6.25762e+05,  0.00000e+00 },
    {  9.93365e+06,  9.91017e+06,  9.62572e+06,  9.55986e+06,  9.49490e+06,  7.53279e+06,  4.35532e+06,  2.88308e+06,  1.52547e+06,  0.00000e+00 },
    {  9.73156e+06,  9.68763e+06,  9.57525e+06,  9.52708e+06,  9.47660e+06,  9.24792e+06,  8.76450e+06,  9.18014e+05,  5.96181e+05,  0.00000e+00 }};
  
  float testData[7][10] = {
    {    8.62275e+06,     8.32370e+06,     8.00000e+06,     3.48668e+06,     2.21538e+06,     7.36950e+05,     0.00000e+00,     0.00000e+00, 
	   0.00000e+00,     0.00000e+00, },
    {    1.26667e+07,     8.47387e+06,     8.47387e+06,     7.67192e+06,     6.98851e+06,     6.87006e+06,     4.78740e+06,     4.02649e+06, 
	   4.92009e+05,     0.00000e+00, },
    {    1.15602e+07,     1.13814e+07,     1.00592e+07,     1.00364e+07,     9.85714e+06,     9.70550e+06,     9.57918e+06,     9.04918e+06, 
	   8.54159e+06,     0.00000e+00, },
    {    1.12688e+07,     1.01256e+07,     9.95725e+06,     9.85194e+06,     9.65899e+06,     9.57078e+06,     9.28461e+06,     8.32572e+06, 
	   7.87748e+05,     0.00000e+00, },
    {    9.98165e+06,     9.83133e+06,     9.63969e+06,     9.54386e+06,     9.31507e+06,     9.18402e+06,     9.13263e+06,     8.96703e+06, 
	   8.59853e+06,     0.00000e+00, },
    {    9.93580e+06,     9.66437e+06,     9.53555e+06,     9.52144e+06,     9.34456e+06,     6.09235e+06,     0.00000e+00,     0.00000e+00, 
	   0.00000e+00,     0.00000e+00, },
    {    9.97130e+06,     9.61299e+06,     9.58561e+06,     9.23797e+06,     0.00000e+00,     0.00000e+00,     0.00000e+00,     0.00000e+00, 
	   0.00000e+00,     0.00000e+00, }};



  int i,j,k,pow2 = 1;
  float avg, med, **barf, **barf2, **barf3;
  
  barf = matrix(0,6,0,9);
  barf2 = matrix(0,6,0,9);
  barf3 = matrix(0,6,0,9);
  for (i=0;i<7;i++) {
    for (j=0;j<10;j++) {
      barf[i][j] = 0.0;
      barf[i][j] = testData[i][j];
      barf2[i][j] = 57.0;
      barf3[i][j] = 124.0 * pow2;
      /*       printf("barf[i][j] %10.4f\n", barf[i][j]); */
    }
    pow2 *= 2;
  }
  
  
  calculateUtilization(barf, barf2, barf3, 7, 10, 10000000.0, &avg, &med);
  printf("avg: %10.4f med: %10.4f\n", avg, med);
  calculateUtilization(barf, barf2, barf3, 7, 10, 9500000.0, &avg, &med);
  printf("avg: %10.4f med: %10.4f\n", avg, med);
  calculateUtilization(barf, barf2, barf3, 7, 10, 8000000.0, &avg, &med);
  printf("avg: %10.4f med: %10.4f\n", avg, med);
  
  free_matrix(barf,0,6,0,9);
  
}
#endif
