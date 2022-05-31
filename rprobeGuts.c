/*

   rprobeGuts.c - send and receive stream of ICMP packets

   $Header: /home/roycroft/carter/Ping+/rpr/src/Release/RCS/rprobeGuts.c,v 1.3 1996/06/21 15:53:40 carter Exp carter $

*/

#include <fcntl.h>
#include <sys/mman.h>
//#include <sys/syssgi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define VERSION "1.2"

#include "ping.h"

#include "rprobe.h"

//#include "raisePriority.h"

static char rcsid[] = "$Id: rprobeGuts.c,v 1.3 1996/06/21 15:53:40 carter Exp carter $";

struct sockaddr_in whereto;		/* Who to ping */
struct sockaddr_in from;		/* Our (source) address */

static int icmp_socket;			/* Socket file descriptor */

char *my_name;
char *my_source = 0;

int datalen = 1480 - ICMP_MINLEN;	/* How much data (was 64) */

/* Incoming and outgoing data packets */
u_char packet[MAXPACKET];		/* Incoming packet */
u_char outpack[MAXPACKET];		/* Outgoing packet */

struct timeval starttv;
struct timezone tz;			/* leftover */

static int n_outstanding = 0;	/* sequence # for outbound packets = #sent */
int packet_id;


unsigned long getTime() {
    unsigned long cur_time;
    struct timespec tstart;
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    cur_time = tstart.tv_sec*1000000 + tstart.tv_nsec/1000;
    return cur_time;
}

/* SGI specific 40 ns accuracy timer; rolls over every 40*2^32 ns */
//volatile hw_time_t *iotimer_addr;
//unsigned cntr_res;
#define TIMENOW getTime()
int TIMER_MAPPED = 1;

hw_time_t last_time;
hw_time_t this_time;
hw_time_t recv_time;

hw_time_t rtt = (hw_time_t) 0;        /* round trip time of packet */
hw_time_t rtts[MAX_PROBES];           /* rtt of each packet */
hw_time_t depart[MAX_PROBES];         /* dparture time of each packet */
hw_time_t arrive[MAX_PROBES];         /* arrival time of each packet */
int  bytes[MAX_PROBES];		      /* size of each packet */
/* did this packet come in order ?*/
int  orderOK[MAX_PROBES] = {0,0,0,0,0,0,0,0,0,0};
int  returned[MAX_PROBES] = {0,0,0,0,0,0,0,0,0,0};
int npackets;
int numPaths = 0;  /* how many paths did the packets take ? */
int routeData = 0; /* did we find RR data ? */
int inOrder = 1;   /* were the packets in order ? */
int duplicatePkts = 0; /* were there any duplicate packets ? */

optstr *sumopt = NULL, *curopt;         /* BC for RR option */

volatile int TimeOutFlag;

int OldMain( char *HostName, int numPkts, int pktSizes[], int timeout);


/* add timeout parameter 1/4/96 */
int rprobe(char *host, int npackets, int packetSize[],
	   hw_time_t ReturnDepart[], hw_time_t ReturnArrive[],
	   int *ReturnBytes, int *ReturnOrderOK,
	   int *returnCodes, int *ReturnNumPaths,
	   int timeout) {
  int i;

  TimeOutFlag = 0;
  numPaths = 0;
  routeData = 0;
  inOrder = 1;
  duplicatePkts = 0;

  n_outstanding = 0;

#ifdef DEBUG
  printf("parameter echoing --  host: %s, numPackets: %d\n",
	 host, npackets);
  printf("size\tdepart\tarrive\n");
  for (i=0; i<npackets; i++) {
    printf("%d\t%lu\t%lu\n",
	   packetSize[i],depart[i],arrive[i]);
  }

  printf("incoming codes: %X, ReturnNumPaths: %d\n", *returnCodes, *ReturnNumPaths);
#endif

  for (i=0; i<npackets; i++) {
    bytes[i] = depart[i] = (i + 1) * 100; 
    arrive[i] = depart[i] + (i + 1) * 100;
  }


  //setRealTimePriority();


  if (0 != OldMain(host, npackets, packetSize, timeout)) {
    printf("OldMain complained about something\n");
  }

  //clearRealTimePriority();

  for (i=0; i<npackets; i++) {
    ReturnBytes[i] = bytes[i];
    ReturnDepart[i] =  depart[i];
    ReturnArrive[i] = arrive[i];
    ReturnOrderOK[i] = orderOK[i];
  }
  

  returnCodes[0] = 0;
  if (routeData) {
    returnCodes[0] |= RecordRouteData;
  }
  if (inOrder) {
    returnCodes[0] |= PacketsInOrder;
  }
  if (duplicatePkts) {
    returnCodes[0] |= DuplicatePackets;
  }
  *ReturnNumPaths = numPaths;
#ifdef DEBUG
  printf("outgoing codes: %X, ReturnNumPaths: %d\n", 
	 *returnCodes, *ReturnNumPaths);
#endif
  return(0);
}

void finish()
{
     int i;

/*     OldReportResults();  */

     /* set TimeOutFlag to stop spinning on recv socket */
     TimeOutFlag = 1;
     printf("Timeout exceeded.\n");

     /*
     if (returned[0])
     {
	  printf("pkt 1 lat (us)    bandwidth(s) (bytes/sec)\n");
	  printf("* %d ", rtts[0]);
     }
     else
     {
	  printf("bandwidth(s) (bytes/sec)\n");
     }
     for (i=0; i< npackets; i++)
	  if (returned[i])
	       printf("%d: %0.2lf ", i,
		      1000000.0*((double) bytes[i])/((double) rtts[i]));
     printf("\n");
     */

     return;
}

int OldMain( char *HostName, int numPkts, int pktSizes[], int timeout)
{
  char hostname[MAXHOSTNAMELEN];

     char *cp;
     char *ap;
     int fp;
     int i;
     struct protoent *proto;
     hw_time_t start_time, end_time;
     int lastPacketId = -1;
     int thisPacketOutOfOrder = 0;

     /* for high-res timer */
     unsigned hw_clock, raddr;
     int fd;

     unsigned long long tn;
     unsigned long timenow;

     /*	IP_OPTIONS */
    u_char optval[MAX_IPOPTLEN];
    u_char *oix = optval;
    int n_sr = 0;
    struct sockaddr_in gateway;

     /* IP_OPTIONS */

     /* Init packet fields */
     //out_icmp->icmp_id = getpid() & 0xFFFF;
     out_icmp->icmp_id = getpid();
     out_icmp->icmp_type = ICMP_ECHO;
     out_icmp->icmp_code = 0;

/********
  printf("initialize arrival vecetor\n");
********/


     npackets = numPkts;

     /* initialize Arrival vector */
     for (i = 0; i< npackets; i++) {
       arrive[i] = -1;
     }

/********
  printf("get socket address %s\n", HostName);
********/
          
     /* get Internet address from hostname or dot-notation */
     if (cp = a2sockaddr(&whereto, HostName, hostname))
     {
	  fprintf(stderr, "%s: %s\n", my_name, cp);
	  exit(1);
     }

/********
  printf("get socket address again %s\n", HostName);
********/


     if (cp = a2sockaddr(&whereto, HostName, hostname))
     {
	  fprintf(stderr, "%s: %s\n", my_name, cp);
	  exit(1);
     }

/********
  printf("fill packet\n");
********/
     
     /* fill outgoing packet with position info */
     for (fp = 0; fp < MAXDATA; fp++)
     {
	  out_pattern[fp] = fp + sizeof(struct timeval);
     }

/********
  printf("open the socket\n");
********/

     /* open the socket */
     if ((proto = getprotobyname("icmp")) == NULL)
     {
	  fprintf(stderr, "icmp: unknown protocol\n");
	  exit(10);
     }
     if ((icmp_socket = socket(AF_INET, SOCK_RAW, proto->p_proto)) < 0)
     {
	  fprintf(stderr, "rpr: socket");
	  exit(5);
     }

/********
  printf("set RR options\n");
********/

     /* BC 5-30-95 : add record route options from Ping+ */
     /*	RR IP_OPTIONS */
     bzero(optval, sizeof(optval));
     optval[IPOPT_OPTVAL] = (char) IPOPT_RR;
     optval[IPOPT_OLEN] = (char) MAX_IPOPTLEN - 1,
     optval[IPOPT_OFFSET] = (char) IPOPT_MINOFF;
     optval[MAX_IPOPTLEN - 1] = (char) IPOPT_EOL;
     if ((proto = getprotobyname("ip")) == NULL) {
       fprintf(stderr, "ip: unknown protocol\n");
       exit(8);
     }
     /*if (setsockopt(icmp_socket, proto->p_proto, IP_OPTIONS, optval, MAX_IPOPTLEN) < 0) {
       perror("setsockopt: IP_OPTIONS");
       exit(42);
     }*/
     int size = 50 * 1024;
     if (setsockopt(icmp_socket, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0) {
     	perror("setsockopt: size");
	exit(42);
     }
     /* RR IP_OPTIONS */

/********
  printf("map in timer\n");
********/

     /* if (!TIMER_MAPPED) {
       int poffmask; */
       /* map in the hi-res timer */
       /* this is straight from the syssgi man page (new rev?) */
       //poffmask = getpagesize() - 1; /* from syssgi man page */
       
       /* hw_clock = syssgi(SGI_QUERY_CYCLECNTR, &cntr_res);
       if (hw_clock == ENODEV)
	 {
	   fprintf(stderr, "Can't open hi-res timer\n");
	   exit(1);
	 }
       
       raddr = ((unsigned)hw_clock & ~poffmask);
       fd = open("/dev/mmem", O_RDONLY);
       iotimer_addr = (volatile hw_time_t *)
	 mmap(0, poffmask, PROT_READ, MAP_PRIVATE, fd, (int)raddr);
       iotimer_addr = (volatile hw_time_t *)
	 ((unsigned long)iotimer_addr + (hw_clock & poffmask));
       close(fd);
       TIMER_MAPPED = 1;
     }*/

     (void) gettimeofday(&starttv, &tz);
     ap = asctime(localtime((const time_t *)&starttv.tv_sec));
     ap[strlen(ap) -1] = '\0' ;

#ifdef DEBUG
     printf("\nRPR (%s) %s %s\n",
	    VERSION, ap, pr_addr(whereto.sin_addr));
#endif

     /* send the packet */
#ifdef DEBUG
     printf("Intervals between sends (us): ");
#endif
     for (i=0; i<npackets; i++)
     {
	  datalen = pktSizes[i];
	  bytes[n_outstanding] = datalen;
	  returned[n_outstanding] = 0;
	  send_icmp(icmp_socket, n_outstanding++, &whereto);
	  depart[n_outstanding-1] = this_time;
#ifdef DEBUG
	  if (i) printf("%u ",timesub(&this_time, &last_time));
#endif
     }
#ifdef DEBUG
     printf("\n");
#endif

     /* not going to wait forerever here */
     signal(SIGALRM, finish);
     /* update the timeout period for those hard-to-reach sites
	10 packets at 200+ msecs per packets is already 2 seconds */
  /* make this a parameter 1/4/96 */
     alarm(timeout);

     /* busy wait for returning packets */
#ifdef DEBUG
     printf("receiving..");
#endif
     while (n_outstanding && !TimeOutFlag) {
       /* assume the packets come in order */
       thisPacketOutOfOrder = 0;

	  if (recv_icmp(icmp_socket, packet, sizeof (packet)) < 0) {
	       continue;
	  }

	  /* print out rtt here */
#ifdef DEBUG
	  printf(".%d.",packet_id);
#endif
	  if (packet_id != lastPacketId + 1) {
	    /* if the current packet is not the consecutive next packet,
	       set a local flag and the global return flag */
	    thisPacketOutOfOrder = 1;
#ifdef FLAGANYREORDERING
	    inOrder = 0;  /* got a packet out of order, flag it */
#endif
	  } 
	  lastPacketId = packet_id;   /* remember the last packet's id */
	  returned[packet_id]++;
	  /* This is a workaround for a problem on our local net.
	     For some reasone we are getting duplicate packet
	     receptions. So...*/
	  /* only record times and decrement n_outstanding if this is
	     the first time we've seen this packet */
	  if (returned[packet_id] == 1) {
	    rtts[packet_id] = rtt;
	    arrive[packet_id] = recv_time;
	    n_outstanding--;
	    /* flag out of order packets */
	    orderOK[packet_id] = !thisPacketOutOfOrder;
	  }
	  else { /* we've seen this packet before */
	    duplicatePkts = 1;
	  }
     } /* end loop collecting outstanding packets */
#ifdef VERBOSE
     OldReportResults();
#endif

     /*
     printf("pkt 1 lat (us)    bandwidth(s) (bytes/sec)\n");
     printf("* %d ", rtts[0]);
     for (i=0; i< npackets; i++)
	  printf("\t%d: %0.2lf %d", i, 1000000.0*((double) bytes[i])/((double) rtts[i]), orderOK[i]);
     printf("\n");
     */

     /* close socket */
     if (!close(icmp_socket) == 0) {
       printf("couldn't close socket!!!!!!!!\n");
       perror("rprobe: ");
     }
     return(0);
}		


/* print stats for each received packet */
void OldReportResults() {     
     int i = 0;


     printf("\n");
     for (i=0; i< npackets; i++){

       printf("pkt %d: %d bytes %lu us", i, bytes[i], rtts[i]);
       if (returned[i])
	 printf("\n");
       else
	 printf(" ++ lost ++\n");
     }

     printf("* ");
     for (i=0; i< npackets; i++){
	  printf("%d %lu %lu ", bytes[i], depart[i], arrive[i]);
	}

     if (inOrder) {
       printf(" SEQOK ");
     }
     else {
       printf(" NOTSEQOK ");
     }
     if (routeData) {
       printf(" RR");
     }
     else {
       printf(" NORR");
     }
     if (duplicatePkts) {
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

     /* Summarize routes */

     printf("Routes: %d\n", numPaths);
     if (!sumopt) {
       printf("No packets with routing info received\n");
     } else {
       for (curopt = sumopt; curopt; curopt = curopt->next) {
	 printf("via:\n");
	 pr_rroption(curopt->optval);
       }
     }

     /* BC - done interpreting the IP_OPTIONS */

   }


/* Parse a numeric IP address or symbolic host name and return a sockaddr_in */
char *a2sockaddr(struct sockaddr_in *addr, char *cp, char *name)
{
  static struct hostent *hp;
  static char buf[80];

#undef	HOST_NOT_FOUND
#define	HOST_NOT_FOUND	0
#define	_h_errno	0
#define	h_nerr	1
     const char *h_errlist[h_nerr] =
     {
	  "unknown host"
     };


     bzero((caddr_t) addr, sizeof(*addr));
     addr->sin_family = AF_INET;
     addr->sin_addr.s_addr = inet_addr(cp);

     if (addr->sin_addr.s_addr == -1) {
       hp = (struct hostent *)NULL;
	  hp = gethostbyname(cp);
	  if (hp) {
	       addr->sin_family = hp->h_addrtype;
	       bcopy(hp->h_addr, (caddr_t) & addr->sin_addr, hp->h_length);
	       cp = hp->h_name;
	  } else {
	       sprintf(buf, "%s: %s",
		       h_errlist[_h_errno < h_nerr ? _h_errno : HOST_NOT_FOUND],
		       cp);
	       return buf;
	  }
     }
     if (name) {
	  strncpy(name, cp, MAXHOSTNAMELEN);
     }
     return (char *) 0;
}

/*
 *  Return an ascii host address
 *  as a dotted quad and optionally with a hostname
 */
char *pr_addr(struct in_addr addr)
{
     struct hostent *hp;
     static char buf[80];

     if ((hp = gethostbyaddr(&addr.s_addr, 4, AF_INET)) == NULL)
	  sprintf(buf, "%s", inet_ntoa(addr));
     else
	  sprintf(buf, "%s (%s)",	hp->h_name, inet_ntoa(addr));

     return (buf);
}

/*
 * 			P I N G E R
 *
 * Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
void send_icmp(int s, int seq, struct sockaddr_in *addr)
{
     int i, cc;
     int lt;
     struct timeval tv;
     struct tm tvl;

     out_icmp->icmp_cksum = 0;
     out_icmp->icmp_seq = seq;

     cc = datalen + ICMP_MINLEN; /* skips ICMP portion */

     /* put current time into the packet header */
     *out_tp = TIMENOW;

     /* Compute ICMP checksum here */
     out_icmp->icmp_cksum = in_cksum((u_short *)out_icmp, cc);
     last_time = this_time;
     this_time = TIMENOW;

     i = sendto(s, outpack, cc, 0, (struct sockaddr *)addr, sizeof(*addr));

     if (i != cc)
     {
	  fprintf(stderr, "error: tried to write %d chars to %s, ret=%d\n",
		  cc, inet_ntoa(addr->sin_addr), i);
     }
}


/*
 *			P R _ P A C K
 *
 * Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
int recv_icmp(int s, u_char *buf, int len)
{
     int cc;
     struct sockaddr_in from;
     int fromlen = (sizeof from);
     struct ip *ip;
     register struct icmp *icp;
     int hlen, dupflag, matchflag = 1;
     u_char *ipopt;

     if ((cc = recvfrom(s, packet, len, 0, (struct sockaddr *)&from, &fromlen)) < 0) {
	  if (errno != EINTR)
	       fprintf(stderr, "ping: recvfrom");
	  return (cc);
     }

     recv_time = TIMENOW;

     ip = (struct ip *) buf;
     hlen = ip->ip_hl << 2;

     /* Is this packet the right length? */
     if (cc < hlen + ICMP_MINLEN)
     {
	  /* fprintf(stderr, "packet too short (%d bytes) from %s\n", cc,
	     inet_ntoa(from.sin_addr)); */
	  return (-1);
     }
     cc -= hlen;
     icp = (struct icmp *) (buf + hlen);

     /* is this packet the right type? */
     if (icp->icmp_type != ICMP_ECHOREPLY && icp->icmp_type != ICMP_TSTAMPREPLY)
     {
	  /* fprintf(stderr, "wrong type: len=%d addr=%s: ",
	     cc, 
	     pr_addr(from.sin_addr)); */
	  pr_icmph(icp);
	  return (-1);
     }

     /*  was it our ECHO? */
     //if (icp->icmp_id != out_icmp->icmp_id)
	 // return (-1);		

     /* calculate this packet's rtt in ms */
     rtt = timesub(&recv_time, (hw_time_t *) &icp->icmp_data[0]);
     packet_id = icp->icmp_seq;

     /* BC 5/31/95 - interpret the RR results here */
    if ((hlen > sizeof(struct ip))) {
	int ipoptlen;
	int newPath = 0; /* assume we have not got a new path on out hands */
	int matchAny = 0;

	ipopt = buf + sizeof(struct ip);
	/* We only know how to parse RR options */
	if (ipopt[IPOPT_OPTVAL] == IPOPT_RR) {
	  routeData = 1;
	    ipoptlen = ipopt[IPOPT_OLEN];
	    /* loop through all the paths to see if we've used the current
	       onte already */
	    for (curopt = sumopt; curopt; curopt = curopt->next) {
		if (ipoptlen != curopt->optval[IPOPT_OLEN]) {
		  continue;
		}
		if (0 == memcmp(ipopt, curopt->optval, ipoptlen)) {
		  /* this is a not new path, notice that we matched */
		  matchAny = 1;
		}
	    }
	    if (!matchAny) {
	      numPaths++;
	      newPath = 1;
	    }
	    if (newPath || NULL == sumopt) {
		curopt = (optstr *)
		    calloc(1, (sizeof(optstr)));
		curopt->next = sumopt;
		sumopt = curopt;
		bcopy(ipopt, curopt->optval, ipoptlen);
	    }
	  }
      }
     /* End copying of RR IP_OPTIONS */


#ifdef VERBOSE
     /* Now, interpret them */
     if (!sumopt) {
       printf("No packets with routing info received\n");
     } else {
       for (curopt = sumopt; curopt; curopt = curopt->next) {
	 printf("via:\n");
	 pr_rroption(curopt->optval);
       }
     }
     /* BC - done interpreting the IP_OPTIONS */
#endif
     
     
     return (cc+hlen);
}

/*
 *  Print a descriptive string about an ICMP header.
 */
void pr_icmph(struct icmp *icp)
{
     switch (icp->icmp_type) {
     case ICMP_ECHOREPLY:
	  printf("Echo Reply\n");
	  /* XXX ID + Seq + Data */
	  break;
     case ICMP_UNREACH:
	  switch (icp->icmp_code) {
	  case ICMP_UNREACH_NET:
	       printf("Destination Net Unreachable\n");
	       break;
	  case ICMP_UNREACH_HOST:
	       printf("Destination Host Unreachable\n");
	       break;
	  case ICMP_UNREACH_PROTOCOL:
	       printf("Destination Protocol Unreachable\n");
	       break;
	  case ICMP_UNREACH_PORT:
	       printf("Destination Port Unreachable\n");
	       break;
	  case ICMP_UNREACH_NEEDFRAG:
	       printf("frag needed and DF set\n");
	       break;
	  case ICMP_UNREACH_SRCFAIL:
	       printf("Source Route Failed\n");
	       break;
	  default:
	       printf("Dest Unreachable, Bad Code: %d\n", icp->icmp_code);
	       break;
	  }
	  /* Print returned IP header information */
	  pr_retip((struct ip *)icp->icmp_data);
	  break;
     case ICMP_SOURCEQUENCH:
	  printf("Source Quench\n");
	  pr_retip((struct ip *)icp->icmp_data);
	  break;
     case ICMP_REDIRECT:
	  switch (icp->icmp_code) {
	  case ICMP_REDIRECT_NET:
	       printf("Redirect Network");
	       break;
	  case ICMP_REDIRECT_HOST:
	       printf("Redirect Host");
	       break;
	  case ICMP_REDIRECT_TOSNET:
	       printf("Redirect Type of Service and Network");
	       break;
	  case ICMP_REDIRECT_TOSHOST:
	       printf("Redirect Type of Service and Host");
	       break;
	  default:
	       printf("Redirect, Bad Code: %d", icp->icmp_code);
	       break;
	  }
	  printf(" (New addr: 0x%08x)\n", icp->icmp_hun.ih_gwaddr.s_addr);
	  pr_retip((struct ip *)icp->icmp_data);
	  break;
     case ICMP_ECHO:
	  printf("Echo Request\n");
	  /* XXX ID + Seq + Data */
	  break;
     case ICMP_TIMXCEED:
	  switch (icp->icmp_code) {
	  case ICMP_TIMXCEED_INTRANS:
	       printf("Time to live exceeded\n");
	       break;
	  case ICMP_TIMXCEED_REASS:
	       printf("Frag reassembly time exceeded\n");
	       break;
	  default:
	       printf("Time exceeded, Bad Code: %d\n", icp->icmp_code);
	       break;
	  }
	  pr_retip((struct ip *)icp->icmp_data);
	  break;
     case ICMP_PARAMPROB:
	  printf("Parameter problem: pointer = 0x%02x\n",
		 icp->icmp_hun.ih_pptr);
	  pr_retip((struct ip *)icp->icmp_data);
	  break;
     case ICMP_TSTAMP:
	  printf("Timestamp\n");
	  /* XXX ID + Seq + 3 timestamps */
	  break;
     case ICMP_TSTAMPREPLY:
	  printf("Timestamp Reply\n");
	  /* XXX ID + Seq + 3 timestamps */
	  break;
     case ICMP_IREQ:
	  printf("Information Request\n");
	  /* XXX ID + Seq */
	  break;
     case ICMP_IREQREPLY:
	  printf("Information Reply\n");
	  /* XXX ID + Seq */
	  break;
     case ICMP_MASKREQ:
	  printf("Address Mask Request\n");
	  break;
     case ICMP_MASKREPLY:
	  printf("Address Mask Reply\n");
	  break;
     default:
	  printf("Bad ICMP type: %d\n", icp->icmp_type);
     }
}

/*
 *
 * Subtract 2 hw_time_t's (which are in microseconds).
 * Clock rolls over every 40*2^32 ns (171 sec) so this
 * routine is not guaranteed if measurements are more
 * than 171 sec apart.
 *
 */
hw_time_t timesub(hw_time_t *first, hw_time_t *second)
{
     hw_time_t result;

     /* since this is a 32 bit counter, wraparound works fine */
     result = *first - *second;
     return result;
}

/*
 *			I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
u_short in_cksum(u_short *addr, int len)
{
     register int nleft = len;
     register u_short *w = addr;
     register u_short answer;
     register int sum = 0;

     /*
      *  Our algorithm is simple, using a 32 bit accumulator (sum),
      *  we add sequential 16 bit words to it, and at the end, fold
      *  back all the carry bits from the top 16 bits into the lower
      *  16 bits.
      */
     while (nleft > 1)
     {
	  sum += *w++;
	  nleft -= 2;
     }

     /* mop up an odd byte, if necessary */
     if (nleft == 1)
     {
	  sum += *(u_char *) w;
     }
     /*
      * add back carry outs from top 16 bits to low 16 bits
      */
     sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
     sum += (sum >> 16);	/* add carry */
     answer = ~sum;		/* truncate to 16 bits */
     return (answer);
}


/*
 *  Dump some info on a returned (via ICMP) IP packet.
 */
void pr_retip(struct ip *ip)
{
     int hlen;
     unsigned char *cp;

     pr_iph(ip);
     hlen = ip->ip_hl << 2;
     cp = (unsigned char *) ip + hlen;

     if (ip->ip_p == 6)
     {
	  printf("TCP: from port %d, to port %d (decimal)\n",
		 (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
     }
     else if (ip->ip_p == 17)
     {
	  printf("UDP: from port %d, to port %d (decimal)\n",
		 (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
     }
}

/*
 *  Print an IP header with options.
 */
void pr_iph(struct ip *ip)
{
     int hlen;
     unsigned char *cp;
     struct in_addr bobsInAddr;
     char *bobString1, *bobString2;

     hlen = ip->ip_hl << 2;
     cp = (unsigned char *) ip + 20; /* point to options */
     
     printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src              Dst Data\n");
     printf(" %1d %2d  %02x %04d %04x",
	    ip->ip_v,
	    ip->ip_hl,
	    ip->ip_tos,
	    ip->ip_len,
	    ip->ip_id);
     printf("   %1x %04x",
	    ((ip->ip_off) & 0xe000) >> 13,
	    (ip->ip_off) & 0x1fff);
     printf(" %3d %3d %04x",
	    ip->ip_ttl,
	    ip->ip_p,
	    ip->ip_sum);
     bobsInAddr.s_addr = ip->ip_src.s_addr;
     bobString1 = inet_ntoa(bobsInAddr);
     bobsInAddr.s_addr = ip->ip_dst.s_addr;
     bobString2 = inet_ntoa(bobsInAddr);
     printf(" %-15s %-15s ",
	    bobString1,
	    bobString2);
     
     /*
	printf(" %-15s %-15s ",
	inet_ntoa(ip->ip_src.s_addr),
	inet_ntoa(ip->ip_dst.s_addr));
	*/
     /* dump and option bytes */
     while (hlen-- > 20) {
	  printf("%02x", *cp++);
     }
     printf("\n");
}


/* BC - 5/31/95 - how to print a RR record */
void
pr_rroption(optval)
char *optval;
{
    struct in_addr inaddr;
    char *optptr;
    int optlen;				/* up to last offset */
    int host;

    optptr = &optval[IPOPT_MINOFF - 1];
    optlen = optval[IPOPT_OFFSET];
/*
    printf("rroption code %d length: %d pointer: %d\n",
	   optval[IPOPT_OPTVAL],
	   optval[IPOPT_OLEN],
	   optval[IPOPT_OFFSET]);
*/
    while (optlen > IPOPT_MINOFF) {
	bcopy(optptr, &inaddr, sizeof(inaddr));
	printf("\t%s\n",
	       pr_quad_addr(inaddr));
	optptr += sizeof(inaddr);
	optlen -= sizeof(inaddr);
    }
}

char *pr_quad_addr(struct in_addr addr)
{
     struct hostent *hp;
     static char buf[80];


     sprintf(buf, "%s", inet_ntoa(addr));

     return (buf);
}
