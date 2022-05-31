/* error codes that the probe can report */

/* these are all 1 == TRUE and 0 == FALSE */

#define RecordRouteData 1
#define PacketsInOrder  2
#define DuplicatePackets 4

int rprobe(char *host, int npackets, int packetSize[],
	   hw_time_t depart[], hw_time_t arrive[],
	   int *bytes, int *orderOK,
	   int *returnCodes, int *numPaths,
	   int timeout);

