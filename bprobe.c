/*

   main routine for bprobe tool
   calls rprobe to do actuall packet transmissions
   save results and does analysis

*/
#include "bprobe.h"

static char rcsid[] = "$Id: bprobe.c,v 1.5 1996/07/10 18:56:46 carter Exp $";

char hostname[MAXHOSTNAMELEN];

void usage(){fprintf(stderr,"Usage: bprobe hostname \n");}


/* forward declarations */
int *freeBins(int *bins);
float applyMethod(int occupiedBinCount, float binSize,  
		  float smallGap, int *bins,
		  int *fullBins);

float applyNewGraphicalMethod(float **bws, float *probeSizes,  
			      int rows, int cols,
			      int minPeak, int minNumSets);

void histogramGaps(float smallGap, float bigGap, float gapRange, 
		   int *gaps, int gapCount,
		   float *RETURNbinSize, int *RETURNoccupiedBinCount,
		   int **RETURNoccupiedBins, int **RETURNbins);

float reportBW(float estimate, int probeSize);
void reportBandwidthForGaps(int count, int gaps[], int Dgaps[], int size,
			    float *bwEsts, float * interDepartureTimes,
			    float *packetSizes);


void sortGaps(int gapCount, int *gaps, int *departGaps,
	      int *sortedGaps, int *sortedDGaps);

void gapStats(int gapCount, int *gaps, 
	 float *minGap, float *maxGap, 
	 float *gapRange, float *medianGap);

float probeLoop(char * hostName, int packetCount);

void calculateUtilization(float **BwEstimateMatrix, 
			  float **interDepartureMatrix,
			  float **packetSizeMatrix,
			  int rows, int cols,
			  float baseBW, 
			  float *returnAvg, float *returnMedian);

#ifndef NOMAIN
/* conditional compilation for use as stanalone probe
   and as part of SONAR server */
int main(int argc, char *argv[])
{
  int numPaths = 0;  /* how many paths did the packets take ? */
  int returnCodes = 0;
  hw_time_t depart[MAX_PROBES];         /* departure time of each packet */
  hw_time_t arrive[MAX_PROBES];         /* arrival time of each packet */
  int  packetSize[MAX_PROBES];
  int  bytes[MAX_PROBES];		      /* size of each packet */
  int npackets;
  float bandwidth;
  int retry;


     char *cp;
     char *ap;
     int fp;
     int i;
     struct protoent *proto;
     hw_time_t start_time, end_time;

     /* for high-res timer */
     unsigned hw_clock, raddr;
     int fd;


     if (argc != 2)
     {
	  usage();
	  exit(1);
     }

     if (argc > (2 + MAX_PROBES))
     {
	  fprintf(stderr, "Too many probes requested; only doing first %d\n",
		  MAX_PROBES);
	  argc = 2 + MAX_PROBES;
     }
     npackets = argc - 2;
     setuid(getuid());
     /* initialize Arrival vector */
     for (i = 0; i< npackets; i++) {
       arrive[i] = -1;
     }
          

     printf("BRPROBE (%s) %s\n",
	    VERSION, argv[1]);


  bandwidth = -1.0;
  retry = 4; /* DISABLE RETRIES */
  while ((bandwidth < 0.0) && (retry++ < 5)) {
    bandwidth = (float)bprobe(argv[1]);
  }
  if (bandwidth > 0.0) {
    printf("Final Bandwidth: %10.5e\n",
	   bandwidth);
  }
  else {
    printf("Final Bandwidth: 0.0\n");
  }


     exit(0);
}		
#endif

int bprobe(char *host) {
  float result;
  result =  probeLoop(host, 10);
  return((int) result);
}


/* print stats for each received packet */
void reportResults(int npackets,
	      hw_time_t depart[], hw_time_t arrive[], int bytes[],
	      int orderOK[],
	      int returnCodes, int numPaths) {     
     int i = 0;


     printf("* ");
     for (i=0; i< npackets; i++){
	  printf("%lu %lu %lu ", bytes[i], depart[i], arrive[i]);
	}

     if (returnCodes & PacketsInOrder) {
       printf(" SEQOK ");
     }
     else {
       printf(" NOTSEQOK ");
     }
     if (returnCodes & RecordRouteData) {
       printf(" RR");
     }
     else {
       printf(" NORR");
     }
     if (returnCodes & DuplicatePackets) {
       /* tack this string onto the end of the record route info flag
	  that way, it doesn't disrupt the processing by other scripts */
       printf("_DUP ");
     }
     else {
       printf(" ");
     }
       
     if (numPaths > 1) {
       printf("multipath: %d", numPaths);
     }
     printf("\n");

   }


float probeLoop(char * hostName, int packetCount) {
  int probeSize = 124;
  int numPaths = 0;  /* how many paths did the packets take ? */
  int returnCodes = 0;
  hw_time_t depart[MAX_PROBES];         /* dparture time of each packet */
  hw_time_t arrive[MAX_PROBES];         /* arrival time of each packet */
  int  packetSize[MAX_PROBES];
  int  bytes[MAX_PROBES];		      /* size of each packet */
  int  orderOK[MAX_PROBES];		      /* is this packet in order? */
  int  gaps[MAX_PROBES];
  int  departGaps[MAX_PROBES];
  int  sortedGaps[MAX_PROBES]; 
  int  sortedDGaps[MAX_PROBES]; 
  int  occupiedBinCount = 0;
  int  *occupiedBins;
  int  *bins;
  int  gapCount = 0;
  float  binSize = 0;
  float  minGap = 0.0;
  float  maxGap = 0.0;
  float  gapRange = 0.0;
  float  medianGap = 0.0;

  float estimate1 = 0.0, estimate6 = 0.0;

  float avgUtil6 = 0.0;

  float medUtil6 = 0.0;

  float bandwidth = 0.0;
  float gapVector[NUM_ROUNDS];
  float bandwidthVector[NUM_ROUNDS];
  float probeSizeVector[NUM_ROUNDS];
  float **bwsToPass;
  float **interDepartureMatrix;
  float **packetSizeMatrix;
  int pass = 0;
  int  i = 0, j = 0;
  int validObservations = 0;
  int badIteration = 0;

  packetCount = 10;

  bwsToPass = matrix(0,NUM_ROUNDS-1,0,MAX_PROBES-1);
  interDepartureMatrix = matrix(0,NUM_ROUNDS-1,0,MAX_PROBES-1);
  packetSizeMatrix = matrix(0,NUM_ROUNDS-1,0,MAX_PROBES-1);
  for(i=0;i<NUM_ROUNDS;i++)
    for(j=0;j<MAX_PROBES;j++) {
      bwsToPass[i][j] = 0.0;
      interDepartureMatrix[i][j] = 0.0;
      packetSizeMatrix[i][j] = 0.0;
      gapVector[j] = bandwidthVector[j] = probeSizeVector[j] = 0.0;
    }
  

  //while (probeSize < 8192) {
  while (probeSize < 1400) {
/*
    printf("\n\nTry probing with %d byte probes...\n", probeSize);
*/
    badIteration = 0; /* clear trouble flag */
     /* initialize Arrival vector */
     for (i = 0; i< packetCount; i++) {
       arrive[i] = -1;
     }
     /* marshal the packet size parameters for the call */

     for (i=0; i<packetCount; i++)
     {
       packetSize[i] = probeSize;
     }

     /* do the call */
     if (!rprobe(hostName, packetCount, packetSize,
	    depart, arrive, bytes, orderOK,
		 &returnCodes, &numPaths,
		 /* for now, hard code so it works the old way
		    may want to adjust based on packet size eventually */
		 30)) {
#ifdef DEBUG
       /* successful probe, report results */
       reportResults(packetCount, depart, arrive, bytes, 
		     orderOK,
		     returnCodes, numPaths);
#endif
     }
     else {
       printf("bprobe: rprobe failure.\n");
       exit(-1);
     }
     if (!(returnCodes & PacketsInOrder))
       {
	 printf("LOGME Packets out of order. Cannot continue\n");
	 badIteration = 1;
       }
     if (returnCodes & DuplicatePackets) {
       printf("LOGME Duplicate Packets received. Cannot continue\n");
	 badIteration = 1;
     }
     gapCount = computeInterArrivalGaps(packetCount, arrive, depart,
					orderOK,
					gaps, departGaps);
     if (gapCount < 2) {
       printf("not enough valid gaps (%d), going on to next iteration\n",
	      gapCount);
       badIteration = 1;
     }
     if (badIteration)  {
       estimate1 = 0.0;
       bandwidth = 0.0;
     }
     else {
       /* we have enough gaps to be getting on with */
#ifdef DEBUG
       displayBothGaps(gapCount, gaps, departGaps);
#endif
       sortGaps(gapCount, gaps, departGaps, sortedGaps, sortedDGaps);

#ifdef DEBUG
       displayBothGaps(gapCount, gaps, sortedGaps, sortedDGaps);
#endif
       reportBandwidthForGaps(gapCount, sortedGaps, sortedDGaps, probeSize,
			      bwsToPass[pass], interDepartureMatrix[pass],
			      packetSizeMatrix[pass]);
       gapStats(gapCount, sortedGaps, &minGap, &maxGap, 
		&gapRange, &medianGap);
       histogramGaps(minGap, maxGap, gapRange, 
		     gaps, gapCount,
		     &binSize, &occupiedBinCount,
		     &occupiedBins, &bins);
       /* this is just shoehorned in */
       estimate1 = applyMethod(occupiedBinCount, binSize,
			      minGap,
			      bins,
			      occupiedBins);
       bandwidth = reportBW(estimate1, probeSize);
       bins = freeBins(bins);
       occupiedBins = freeBins(occupiedBins);
       

     }/* end procesing of sufficient gaps */
    /* double the probe size for the next iteration */
    gapVector[pass] = estimate1;
    bandwidthVector[pass] = bandwidth;
    probeSizeVector[pass] = (float)probeSize;
    pass++;
    /* new probe size is 1.5 times old on odd passes
       ans 2.5 times old on even passes */
    if ((pass % 2) == 1) {
      //int halfProbe = probeSize >> 1;
      //probeSize = probeSize + halfProbe;
      // Changing probesize = 1.3 times old for odd passes
      probeSize = 1.3*probeSize;
    }
    else {
      //int halfProbe = probeSize >> 1;
      //probeSize = 2 * probeSize + halfProbe;
      // Changing probesize = 1.7 times old for even passes
      probeSize = 1.7*probeSize;
    }
    /* round up to next even number */
    if ((probeSize % 2) == 1)
      probeSize++;

  } /* end loop over varied probe sizes */

  validObservations = cleanData(probeSizeVector, gapVector, bwsToPass, 
				MAX_PROBES);
  estimate6 = applyNewGraphicalMethod(bwsToPass, probeSizeVector,
				   validObservations, MAX_PROBES,
				      7, 2);
  calculateUtilization(bwsToPass, interDepartureMatrix, packetSizeMatrix,
		       validObservations, MAX_PROBES,
		       estimate6, &avgUtil6,&medUtil6);

  free_matrix(bwsToPass,0,NUM_ROUNDS-1,0,MAX_PROBES-1);
  free_matrix(interDepartureMatrix,0,NUM_ROUNDS-1,0,MAX_PROBES-1);
  free_matrix(packetSizeMatrix,0,NUM_ROUNDS-1,0,MAX_PROBES-1);
  return(estimate6);
} /* end probeLoop */

int cleanData(float *probeSizes, float *gapSizes, float **bwsToPass,
	      int numberOfProbeRounds) {
  int i, j, k;
  int valid = numberOfProbeRounds;  /* initially, assume all are valid */

  for (i=0; i<numberOfProbeRounds; i++) {
    if (gapSizes[i] == 0.0)  {
      valid--;  /* remove this entry from the count of valid data */
      for (j = i+1; j<numberOfProbeRounds; j++) {
	/* move the rest of the data down one position */
	probeSizes[j-1] = probeSizes[j];
	gapSizes[j-1] = gapSizes[j];
	for (k=0; k<MAX_PROBES; k++) {
	  bwsToPass[j-1][k] = bwsToPass[j][k];
	}
      }
      i--; /* want to look again at the *new* i'th entry */
      numberOfProbeRounds--; /* one fewer we need to look at at the end */
    }
  }
  return(valid);
}

int *freeBins(int *bins) {
  free(bins);
  return(NULL);
}

float applyMethod(int occupiedBinCount, float binSize,
		  float smallGap, int *bins,
		  int *fullBins) {

  float sum = 0.0;
  float count = 0.0;
  int index = 0;
  float estimate = 0.0;

  if (occupiedBinCount > 3) {
    /* throw out the low and high bins */
    /* take average of remainder */
    sum = 0.0;
    count = 0.0;
    for(index = 1; index < occupiedBinCount - 1; index++) {
      /* compute the contribution of this bin my multiplying
	 the value of the bin by the number of items in it */
      sum += (smallGap + binSize * fullBins[index]) * 
	bins[fullBins[index]];
      /* count the number of items in the running sum */
      count += bins[fullBins[index]];
    }
    estimate = sum / count;
  }
  else {
    if (occupiedBinCount == 3) {
      /* take the median bin size as the estimate */
      estimate = smallGap + binSize * fullBins[1];
    }
    else {
      if (occupiedBinCount > 0) {
	/* take average of remainder */
	sum = 0.0;
	count = 0.0;
	for(index = 0; index < occupiedBinCount ; index++) {
	  /* compute the contribution of this bin my multiplying
	     the value of the bin by the number of items in it */
	  sum += (smallGap + binSize * fullBins[index]) * 
	    bins[fullBins[index]];
	  /* count the number of items in the running sum */
	  count += bins[fullBins[index]];
	}
	estimate = sum / count;
      }
      else {
	printf("ERROR - no bins occupied\n");
	estimate = 1.0;
      }
    }
  }
  return(estimate);
} /* end applyMethod */

void gapStats(int gapCount, int *gaps, 
	 float *minGap, float *maxGap, 
	 float *gapRange, float *medianGap) {
  /* # find the range of the gaps */
  int   smallGapIndex = 0;
  int   bigGapIndex = gapCount - 1;
	   
  *minGap = gaps[0];
  *maxGap = gaps[bigGapIndex];

  *gapRange = *maxGap - *minGap;
#ifdef DEBUG  
  printf("minGap: %10.2f, maxGap: %10.2f, gapRange: %10.2f\n", 
	 *minGap, *maxGap, *gapRange);
#endif
  /* # find the median gap */
  if ((bigGapIndex % 2) == 0) {
    *medianGap = gaps[(bigGapIndex >>= 1)];
  }
  else {
    int leftOfCenter = (int)(bigGapIndex/2.0);
    int rightOfCenter = leftOfCenter++;
    *medianGap = ((gaps[leftOfCenter] + gaps[rightOfCenter]) / 2.0);
  }
#ifdef DEBUG  
  printf( "medianGap: %f\n", *medianGap);
#endif
}/* end gapStats */

void histogramGaps(float smallGap, float bigGap, float gapRange, 
	      int *gaps, int gapCount,
	      float *RETURNbinSize, int *RETURNoccupiedBinCount,
	      int **RETURNoccupiedBins, int **RETURNbins) {
  int numberOfBins = 100;
  int index = 0;
  int *occupiedBins, *bins;
  float binSize;
  int occupiedBinCount;


  binSize = gapRange/(float)numberOfBins;

  /* don't want too fine a grain */
  while ((binSize < 0.1)) {
   binSize = binSize * 2.0;
  }

  occupiedBinCount = 10;
  while (occupiedBinCount > 7) {
    occupiedBinCount = 0;
    numberOfBins = (int)(gapRange/binSize + 0.5); 
    numberOfBins++; /* safety margin. think about this someday */

    /* allocate space for bins and occupiedBins */
    bins = (int *)malloc(numberOfBins * sizeof(int));
    occupiedBins = (int *) malloc(numberOfBins * sizeof(int));

    /* zero out these arrays */
    /* can thse be done faster. bzero?? */
    for (index = 0; index < numberOfBins; index++) {
      bins[index] = occupiedBins[index] = 0;
    }

    /* # assign the gaps to bins */
    for (index = 0; index < gapCount; index++) {
      int binNumber = (int)((gaps[index]-smallGap)/binSize + 0.5);
      bins[binNumber]++;
    }
	
    /* figure out how many bins are occupied and which ones are */
    for(index = 0; index < numberOfBins; index++) {
      if (bins[index]) {
	occupiedBins[occupiedBinCount++] = index;
#ifdef DEBUG
	printf("bin[%d] : %d\n",index, bins[index]);
#endif
      }
    }

#ifdef DEBUG	
    printf("full bins: %d\n",occupiedBinCount);
    for(index = 0; index < occupiedBinCount; index++) {
      printf("occupied bin[%d] : %d\n",index, occupiedBins[index]);
    }
#endif

    if (occupiedBinCount > 7) {
      /* free storage for next round */
      free(bins);
      free(occupiedBins);
      /* undef(@bin); */
#ifdef DEBUG
      printf("too many bins, another round\n");
#endif
      binSize = binSize * 2;
    } /* cleanup for next iteration */
  }		/* 	# loop to compress bins */
  /* return these values */
  *RETURNbinSize = binSize;
  *RETURNoccupiedBinCount = occupiedBinCount;
  *RETURNoccupiedBins = occupiedBins;
  *RETURNbins = bins;
} /* end histogramGaps */

void displayGaps(int gapCount, int *gaps) {
  int index = 0 ;
  
  for (index = 0; index < gapCount; index++) {
    printf("gaps[%d] = %d\n", index, gaps[index]);
  }	

}

void displayBothGaps(int gapCount, int *gaps, int *Dgaps) {
  int index = 0 ;
  
  for (index = 0; index < gapCount; index++) {
    printf("gaps[%d] = %d\t%d\n", index, gaps[index], Dgaps[index]);
  }	

}


int computeInterArrivalGaps(
			    int packetCount, hw_time_t *arrive, hw_time_t *depart,
			    int *orderOK,
			    int *gaps, int *departGaps) {
  int index = 0;
  int gapIndex = -1;

  for (index = 0; index < packetCount; index++) {
    /* check that no rollover or timeout occurred */
    if ((arrive[index] > 0 ) &&
	(arrive[index+1] > 0) &&
	(arrive[index+1] > arrive[index]) &&
	/* new scheme, let packets com in out of order but only
	   measure the pairs that ar in order */
	/* check that the next packet is in order relative to this one */
	orderOK[index+1]
	) {
      gaps[++gapIndex] = arrive[index+1] - arrive[index];
      departGaps[gapIndex] = depart[index+1] - depart[index];
    }
  }				/* end loop computing inter-arrival gaps */
  return(gapIndex + 1);         /* return a count of valid gaps */
}


void sortGaps(int gapCount, int *gaps, int *Dgaps,
	      int *sortedGaps, int *sortedDgaps) {
  /* do a stupid sort since this is so small a list */
  int i, j;
  int position;

#if DEBUG
  printf("sorting %d gaps\n", gapCount);
#endif

  for (i = 0; i < gapCount; i++) {
    /* pick up an item */
    /* initially assume it belongs in the first position in the
       sorted list */
    position = 0;
    /* compare it to each other item.
       if the other item is less than incrment the position
       of the item being held to make room for the smaller item
       */
    for (j = 0; j < gapCount; j++) {
      /* if the item is less than the current, current moves over one */
      if (gaps[j] < gaps[i])
	position++;
      /* if the item equals the current, only move over equals to the right */
      if ((gaps[j] == gaps[i]) && j > i) {
	position++;
      }
    }
/*    printf("put %d in position %d\n", gaps[i], position); */
    sortedGaps[position] = gaps[i];
    /* the inter-departure gap goes along for the ride */
    sortedDgaps[position] = Dgaps[i];

  }
} /* end sortGaps */

float reportBW(float estimate, int probeSize) {
  float bandwidth;
  
/*
  printf("estimated interarrivaL GAPOSIS: %10.2f probe size %d\n",estimate,
	 probeSize);
*/
  probeSize += 82;          /* add ethernet penalty */
  if (estimate > 0.0) {
    bandwidth = (float)(probeSize * 8000000.0) / estimate;
  }
  else {
    bandwidth = 0.0;
  }
/*
  printf("estimated Bandwidth: %10.2f bps\n",bandwidth);
*/  
  return(bandwidth);
} /* end reportBW */


void reportBandwidthForGaps(int count, int gaps[], int Dgaps[],
			    int size,
			    float *bwEsts,
			    float *interDepartureTimes,
			    float *packetSizes) {
/*  
    takes vectors of interArrival gaps and interDeparture gaps
    and the packet size from one phase of the probe

    given a set of inter-arrival gaps and the packet sizes
    compute the bw estimates
  
    returns rows of three matrices: bw estimates, interdeparture
    times and packet sizes 

    also dumps a table of this infomation to the stdout.
*/
  int i, validBWIndex;
  float diff, ratio;
  float bw;
  validBWIndex = 0;

#ifdef DEBUG
  printf("Size\tDelArr\tDelDep\t  Diff\t Ratio\tBW EST\n");
#endif

  for (i=0; i<count; i++) {
    bw = reportBW((float)gaps[i], size);
    diff = (float)(gaps[i] - Dgaps[i]);
    ratio = (float)gaps[i]/(float)Dgaps[i];

#ifdef DEBUG
    printf("%d\t%6.0f\t%6.0f\t%6.0f\t%6.2f\t%10.4e", 
	   size, (float)gaps[i], (float)Dgaps[i],
	   diff, ratio, bw);
#endif

    /* weed out bogus entries */
    if ((diff > 0.0) && (ratio <= 10000.0)) {
      interDepartureTimes[validBWIndex] = (float)Dgaps[i];
      packetSizes[validBWIndex] = (float)size;
      bwEsts[validBWIndex++] = bw;
      
      /* terminate table row */
#ifdef DEBUG
      printf("\n");
#endif
    }
    else {
      /* terminate table row (with extreme prejudice ;-)*/
#ifdef DEBUG
      printf(" <discarded>\n");
#endif
    }
  } /* end loop over all valid gaps */
  /* pad the rest of the bw row for this probe size with zeros */
  for (i=validBWIndex; i<MAX_PROBES; i++) {
    interDepartureTimes[i] = 0.0;
    packetSizes[i] = 0.0;
    bwEsts[i] = 0.0;
  }
} /* end reportBandwidthForGaps */
