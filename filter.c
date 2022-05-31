/*

   filter.c - filtering mechanism for bandwidth estimates

*/
#include "bprobe.h"

static char rcsid[] = "$Id: filter.c,v 1.2 1996/06/21 18:22:26 carter Exp $";


void dumpBWMatrixPrivateNW( float **bws, int rows, int cols) {
  int i, j;

  for(i=0;i<rows;i++) {
    printf("LOGME logme {");
    for(j=0;j<cols;j++) {
      printf("%15.5e, ", bws[i][j]);
    }
    printf("},\n");
  }
}


void dumpBWMatrixPrivate( float **bws, int rows, int cols) {
  int i, j;
  FILE *fp;

  fp = fopen("DATA.DATBERT","a");

  
  fprintf(fp, "%d\n", rows);
  for(i=0;i<rows;i++) {
    printf("logme {");
    for(j=0;j<cols;j++) {
      printf("%15.5e, ", bws[i][j]);
      fprintf(fp,"%15.5e ", bws[i][j]);
    }
    printf("},\n");
    fprintf(fp,"\n");
  }
  fclose(fp);
}

int readBWMatrixPrivate(FILE *fp, float **bws, int *rows, int cols) {
  int i, j, count = 0, res;
  char c = 'a';
  float alpha;
  
  res = fscanf(fp, "%d\n", rows);
  if (EOF != res)  {
    for(i=0;i<*rows;i++) {
      for(j=0;j<cols;j++) {
	res = fscanf(fp,"%f", &alpha /*&(bws[i][j])*/);
	bws[i][j] = alpha;
	printf("(al)%15.5e ", alpha ); /*bws[i][j]); */
	if (bws[i][j] > 0.0)
	  count++;
      }
      printf("\n");
    }
    return(count);
  }
  else {
    return(0);
  }
  
}

float applyNewGraphicalMethod(float **bwEstimates, float *probeSizes,
			   int rows, int cols, int minPeakSize, int minNumSets) {
  /* takes a matrix of bandwidth estimates

     gap1  gap2  gap3 ...
     --------------------
small|
     |
probe|
size |
     |
large|

     a vector of probe sizes

     and the dimensions of the matrix

RETURNS:  a bandwidth estimate (0.0 if no estimate can be made)

*******************************************/

  int i,j, count,setNumber;
  int fuzzLimit, redoLimit;
  int redoFlag;
  int validProbeCount = 0;
  int setsMerged = 0;
  int initialSetCount = 0;
  float fuzzFactor = (1.0/(float)NEW_FUZZ_ROUNDS);  /* percent */
  SetPtr sets[NUM_ROUNDS] = {NULL};
  SetPtr mergedSet = NULL;
  SetPtr newMergedSet = NULL;
  SetPtr peakSet = NULL;
  float orderedBws[MAX_PROBES];
  int peakSize = 0;
  int peakNumSets = 0;
  int iterationCount = 0;
  

  if (rows <=0) {
    return(0.0);
  }



  /* loop over sets of estimates from large probes to small */
  for (i=rows-1; i>=0; i--) {
#ifdef DEBUG
    printf("**** ROW %d **** SIZE %d\n",i, (int)probeSizes[i]);
#endif
    count = 0;
    /* since gaps are sorted by increasing size, bw's need to be reversed */
    for (j=MAX_PROBES-1;j>=0;j--) {
      if (bwEstimates[i][j] > 0) {
	orderedBws[count] = bwEstimates[i][j];
	count++;
      }
    }

    setNumber = (rows-1) - i;
#ifdef DEBUG
    printf("size %d stored in SET %d\n",(int)probeSizes[i],setNumber);
#endif
    
    /* should this be a higher limit? do we trust one reading, for instance? */
    /* well, with the new pruning scheme, yes */
    /* on the other hand, NO */
    if (count>0) {
      sets[initialSetCount] = createSet(orderedBws, count, 0.0);
      initialSetCount++;
    }
    else {
      sets[setNumber] = NULL;
    }
  }

#ifdef DEBUG
  printf("after set creation from bandwidths rows = %d\n",rows);
  for (i=0; i<rows; i++) {
    printf("set[%d] : size %d \n",i,(int)probeSizes[(rows-1)-i]);
    if (sets[i])  {
      dumpSet(sets[i], 1);
    }
    else {
      printf(" EMPTY SET \n");
    }
  }
#endif
  
  if (initialSetCount < 2) {
#ifdef DEBUG
    printf("Bailing. Not enough sets to consider. %d", initialSetCount);
#endif
    return(0.0);
  }
  
  redoFlag = 1;
  redoLimit = 0;
  /* allow at most 10 outside the loop retries */
  while (redoFlag && redoLimit < NEW_FUZZ_ROUNDS) {
    iterationCount++;
#ifdef CDEBUG
    printf("Major grind redo number %d\n", redoLimit);
#endif

    setsMerged = 0;
    redoLimit++;
    fuzzLimit = 0;
    adjustFuzz(sets[0],fuzzFactor);
    adjustFuzz(sets[1],fuzzFactor);
    mergedSet  = unionSets(sets[0], sets[1]);

    if (mergedSet) {
      setsMerged = 2;
      /* mergedSet is the merged of the two largest probe sizes.
	 Try to merge in the successively smaller
	 probe size data sets until only one peak remains.
	 If all peaks disappear (i.e. we go from n>1 to 0 peaks
	 in one round. quit and remake the initial merger of 0 and 1.
	 */
      i = 2;  /* start with set 2 */
      /* keep merging in each smaller set (up to FUZZ_ROUNDS tries apiece)
	 until we get to one peak or we run out of sets
	 */
      while ((i < rows) && 
	     sets[i]) {
	adjustFuzz(sets[i],fuzzFactor);
#ifdef CDEBUG
	printf("merge in set %d\n", i);
#endif
	newMergedSet = unionSets(mergedSet, sets[i]);
	if (newMergedSet) {
	  setsMerged++;
	  destroySet(mergedSet);
	  mergedSet = newMergedSet;
	}
	i++; /* try the next smaller set size */
      } /* end loop over smaller sets */
    } /* have a mergedSet from sets 0 and 1 */

    /* at this point we have merger of sets 0,1... 
       until all peaks were lost and we backed up
       one step in time */

    findPeakInSet(mergedSet, &peakSet, &peakSize, &peakNumSets);

    if (setSize(peakSet) == 1 && peakSize >= minPeakSize  && peakNumSets >= minNumSets ) {
      /* we have resolved on one peak */
#ifdef DEBUG
      printf("Success. One peak found in %d iterations\n", redoLimit);
      printf("logme");
      dumpSet(peakSet,1);
#endif
      redoFlag = 0;
      break;
    }
    else {
      redoFlag = 1;
#ifdef CDEBUG
      printf("too many [%d] peaks still after %d tries, Try once more\n", setSize(peakSet), redoLimit);
      dumpSet(peakSet,1);
#endif
      mergedSet = destroySet(mergedSet);
      peakSet = destroySet(peakSet);
    }
  } /* end loop over total allowed retries */

  /* OK, now we tried everything and we are ready to give up
     if we must */
  /*        clean up the oritginal setts */
  while(initialSetCount--) {
    sets[initialSetCount] = destroySet(sets[initialSetCount]);
  }
  if (peakSet && 
      setSize(peakSet) == 1 && 
      peakSet->first) {
    float estimate = (peakSet->first->itemLow +  
	    peakSet->first->itemHigh)/2.0;
    /* shoot the bastards - plug the storage leak */
    mergedSet = destroySet(mergedSet);
    peakSet = destroySet(peakSet);
#ifdef DEBUG
    printf("**Merged Sets: %d\n",setsMerged);
#endif
    return(estimate);
  }
  else return(0.0);
  
} /* end applyNewGraphicalMethod */


#ifdef TESTPKG
void main() {
  int rows = 10, columns = 10, dataPoints = 0;
  FILE *fp;
  FILE *fpo;

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

//float probeSizeVector[NUM_ROUNDS] = {124, 248, 496, 992, 1984, 3968, 7936, 0,	 0 , 0};
float probeSizeVector[NUM_ROUNDS] = {124, 248, 496, 992, 1012, 800, 730, 0,	 0 , 0};
  float **barf, answer;
  int i,j;

barf = matrix(0,rows,0,columns);

  for (i=0;i<rows;i++)
    for (j=0;j<columns;j++)
      barf[i][j] = -1.0;
 


fp = fopen("DATA.DATBERT","r");

fpo = fopen("DATA.MATRIX", "w");


while (dataPoints = readBWMatrixPrivate(fp,barf,&rows,columns)) {
     int peakSize, numSets;
     fprintf(fpo, "%d data points\n",dataPoints);
     fprintf(fpo, "\t\t\tNumber of Sets\n");
     fprintf(fpo, "           ");
     for (numSets=1; numSets <= rows; numSets++) {
       fprintf(fpo, "%10d ",numSets);
     }
     fprintf(fpo,"\n");
     for (peakSize=1; peakSize <= dataPoints; peakSize++) {
       fprintf(fpo, "%10d ",peakSize);
       for (numSets=1; numSets <= rows; numSets++) {
	 answer = applyNewGraphicalMethod(barf, probeSizeVector, 
					  rows, MAX_PROBES, peakSize,numSets);
	 fprintf(fpo,"%10.2e ",answer);
       }
       fprintf(fpo,"\n");
     }
     fclose(fpo);
     fpo = fopen("DATA.MATRIX", "a");

   }
     fclose(fp);
     fclose((((((((((fpo))))))))));
     printf("BRRRRRRRRRRRRRRRRRRRRR the answer is %10.4g\n", answer);
     
   }
#endif
