/*

   congtool.c - main routines for cprobe tool.

   $Header: /home/roycroft/carter/Ping+/rpr/src/Release/RCS/congtool.c,v 1.5 1996/07/10 18:56:07 carter Exp $

*/
#include "congtool.h"
#include <stdlib.h>

static char rcsid[] = "$Id: congtool.c,v 1.5 1996/07/10 18:56:07 carter Exp $";

char hostname[MAXHOSTNAMELEN];

void usage(){fprintf(stderr,"Usage: cprobe hostname\n");}


/* forward declarations */


void sortGaps(int gapCount, int *gaps, int *departGaps,
	      int *sortedGaps, int *sortedDGaps);

float probeLoop(char * hostName, int packetCount);

#ifndef NOMAIN
/* conditional compilation for use as stanalone probe
   and as part of SONAR server */
int main(int argc, char *argv[])
{
  int npackets = 0;                         /* number of packets to send */
  float bandwidth;                      /* available bandwidth estimate */

  int i;

  if (argc != 2)
    {
      usage();
      exit(1);
    }

  printf("CPROBE (%s) %s\n",
	 VERSION, argv[1]);

  bandwidth = -1.0;

  bandwidth = (float)cprobe(argv[1]);

  if (bandwidth > 0.0) {
    printf("Available Bandwidth: %10.5f\n",bandwidth);
  }
  else {
    printf("Available Bandwidth: 0.0\n");
  }

  exit(0);
}		
#endif

int cprobe(char *host) {
  float result;
  result =  probeLoop(host, 10);
  return((int) result);
}

/* print stats for each received packet */
void reportResults(int npackets,
	      hw_time_t depart[], hw_time_t arrive[], int bytes[],
	      int returnCodes, int numPaths) {     
     int i = 0;


#ifdef VERBOSE 
     for (i=0; i< npackets; i++){

       printf("pkt %d: %d bytes %d us", i, bytes[i], arrive[i] - depart[i]);
       if (arrive[i] > 0)
	 printf("\n");
       else
	 printf(" ++ lost ++\n");
     }
#endif
     printf("* ");
     for (i=0; i< npackets; i++){
	  printf("%d %lu %lu ", bytes[i], depart[i], arrive[i]);
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
  int probeSize = 600;
  int numPaths = 0;  /* how many paths did the packets take ? */
  int returnCodes = 0;
  hw_time_t depart[MAX_PROBES];         /* dparture time of each packet */
  hw_time_t arrive[MAX_PROBES];         /* arrival time of each packet */
  int  packetSize[MAX_PROBES];
  int  bytes[MAX_PROBES];		      /* size of each packet */
  int  orderOK[MAX_PROBES];
  int  gaps[MAX_PROBES];
  int  departGaps[MAX_PROBES];
  int  sortedGaps[MAX_PROBES]; 
  int  sortedDGaps[MAX_PROBES]; 
  
  int  gapCount = 0;

  float estimate1 = 0.0, estimate2 = 0.0;


  float bandwidth = 0.0;
  float bandwidthVector[NUM_ROUNDS];
  float probeSizeVector[NUM_ROUNDS];
  int pass = 0;
  int  i = 0, j = 0;
  int validObservations = 0;
  int badIteration = 0;


  packetCount = 8;

  for(j=0;j<MAX_PROBES;j++) {
    bandwidthVector[j] = 0.0;
  }
  

  while (probeSize <= 900) {

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
	    depart, arrive, bytes, orderOK, &returnCodes, &numPaths,
		 1 /* minimum timeout in seconds */)) {
       /* successful probe, report results */
#ifdef VERBOSE
       reportResults(packetCount, depart, arrive, bytes, returnCodes, numPaths);
#endif
     }
     else {
       printf("FATAL ERROR\n");
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
					gaps, departGaps);
    if ((packetCount -1 ) == gapCount) {
      int timeSpent;
      int bytesReceived;
#ifdef VERBOSE
      printf("exactly %d gaps\n",packetCount - 1);
      printf("took %d microseconds\n", timeSpent = arrive[gapCount] - arrive[0]);
      printf("sent %d bytes\n", bytesReceived = (probeSize + 42) * gapCount);
#endif
    }
     if (gapCount < 2) {
       printf("not enough valid gaps, going on to next iteration\n");
       badIteration = 1;
     }
     if (badIteration)  {
       estimate1 = estimate2 = 0.0;
       bandwidth = 0.0;
     }
     else {
       /* we have enough gaps to be getting on with */
#ifdef DEBUG
       displayBothGaps(gapCount, gaps, departGaps);
#endif
       sortGaps(gapCount, gaps, departGaps, sortedGaps, sortedDGaps);
       if ((packetCount -1 ) == gapCount) {
	 int timeSpent;
	 int bytesReceived;
	 int i;

	 timeSpent = 0;
	 bytesReceived = 0;
	 /* remove hi and low outliers */
	 /* add up the amount of time per packet and the amount of bytes
	    per packet */
	 for (i=1; i < gapCount; i++) {
	   timeSpent += sortedGaps[i];
	   bytesReceived += probeSize + 42;
	 }
#ifdef VERBOSE
	 printf("WITHOUT OUTLIERS\n");
	 printf("took %d microseconds\n", timeSpent);
	 printf("sent %d bytes\n", bytesReceived);
#endif
	 /* now, just calculate available bandwidth directly */

	 bandwidthVector[pass] = ((float) bytesReceived * 8.0 * 1000000.0) 
	   / (float) timeSpent;
#ifdef VERBOSE
	 printf("available bandwidth estimate: %f\n", bandwidthVector[pass]);
#endif

       }

     }/* end procesing of sufficient gaps */

    /* increment pass counter and adjust packet size for next round */
    pass++;
    probeSize += 100;
  } /* end loop over varied probe sizes */

  { int i; float sum = 0.0; int validReadings = 0;
    /* compute average of readings for return */
    for (i=0;i<pass;i++) {
      if (bandwidthVector[i] > 0.0) {
	sum += bandwidthVector[i];
	validReadings++;
      }
    }
    if (validReadings > 0) {
      return(sum/validReadings);
    }
    else {
      return(0.0);
    }
  }
} /* end probeLoop */

void displayBothGaps(int gapCount, int *gaps, int *Dgaps) {
  int index = 0 ;
  
  for (index = 0; index < gapCount; index++) {
    printf("gaps[%d] = %d\t%d\n", index, gaps[index], Dgaps[index]);
  }	

}


int computeInterArrivalGaps(
			    int packetCount, unsigned long int *arrive, unsigned long int *depart,
			    int *gaps, int *departGaps) {

  /* loop through vectors of arrival and departute times and
     compute differences between successive entries */

  int index = 0;
  int gapIndex = -1;

  for (index = 0; index < packetCount; index++) {
    /* check that no rollover or timeout occurred */
    if ((arrive[index] > 0 ) &&
	(arrive[index+1] > 0) &&
	(arrive[index+1] > arrive[index])) {
      gaps[++gapIndex] = arrive[index+1] - arrive[index];
      departGaps[gapIndex] = depart[index+1] - depart[index];
    }
  }				/* end loop computing inter-arrival gaps */
  return(gapIndex + 1);         /* return a count of valid gaps */
} /* end computeInterArrivalGaps */


void sortGaps(int gapCount, int *gaps, int *Dgaps,
	      int *sortedGaps, int *sortedDgaps) {
  /* take vectors of gapCount arrival gaps and departure gaps
     and generate sorted vectors sortedGaps and sortedDgaps */
     
  /* do a simple sort since this is so small a list */

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
    /* printf("put %d in position %d\n", gaps[i], position); */
    sortedGaps[position] = gaps[i];
    /* the inter-departure gap goes along for the ride */
    sortedDgaps[position] = Dgaps[i];

  }
} /* end sortGaps */
