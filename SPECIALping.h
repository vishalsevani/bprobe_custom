/*
 *  static char sccsid[] = "@(#)ping.h";
 */

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <signal.h>

#ifndef	__STDC__
#define	const
#endif	/* __STDC__ */

#ifdef	MORESTATS
#include <math.h>
#endif	/* MORESTATS */

#define	MAXWAIT		10		/* max time to wait for response, sec. */
#define	MAXPACKET	65536		/* max packet size */
#define	MAXDATA		MAXPACKET-sizeof(struct timeval)-ICMP_MINLEN	/* max data length */
#define	NROUTES		9		/* number of record route slots */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	64
#endif

#define MAX_PROBES      10

#define	RECV_BUF	48*1024		/* Size of receive buffer to specify */

#define OF_VERBOSE		0x01	/* verbose flag */
#define OF_QUIET		0x02	/* quiet flag */
#define OF_FLOOD		0x04	/* floodping flag */
#define	OF_RROUTE		0x08	/* record route flag */
#define	OF_CISCO		0x10	/* Cisco style pings */
#define	OF_NUMERIC		0x20	/* Numeric IP addresses only */
#define	OF_MISSING		0x40	/* Print table of missing responses */
#define	OF_FAST			0x80	/* Send an echo request whenever we get a response */
#define	OF_FILLED		0x0100	/* Fill packets with specified data */
#define	OF_LSROUTE		0x0200	/* Loose source routing */
#define	OF_SSROUTE		0x0400	/* Strict source routing */
#define	OF_DONTROUTE		0x0800	/* Don't use routing table */
#define	OF_DEBUG		0x1000	/* Turn on socket debugging */
#define	OF_SRCADDR		0x2000	/* Turn on socket debugging */

/* time in microseconds */
typedef unsigned long hw_time_t;

/*
 *	Structure for storing responses for a specific route
 */
typedef struct _optstr {
    struct _optstr *next;
/*     stats optstats;  BC - don't need this, do we?*/
    char optval[MAX_IPOPTLEN];
} optstr;

struct opt_names {
    char op_letter;
    const char *op_arg;
    const char *op_name;
};

/* References to packet fields */
#define	out_icmp	((struct icmp *) outpack)
#define	out_tp		((hw_time_t *) (outpack + ICMP_MINLEN))
#define	out_pattern	((u_char *) out_tp + sizeof(*out_tp))

/*
 *	Shared data
 */
extern int timing;		/* Packets are long enough to time */
extern int got_one;		/* Received a packet this period */
extern int datalen;
extern int column;		/* For keeping track of output column */
extern u_long pingflags;	/* User specified options */
extern struct timezone tz;
extern u_char outpack[];	/* Outgoing packet buffer */
extern u_char packet[];		/* Incoming packet buffer */
extern int mx_dup_ck;
extern char rcvd_tbl[];

/*
 *	Routine type declarations
 */
void recv_alarm();
void prefinish();
void finish();
void record_stats();
void tvsub();
void usage();

#ifdef	__STDC__
void pr_retip(struct ip *ip);
u_short in_cksum(u_short *, int);
hw_time_t timesub(hw_time_t *first, hw_time_t *second);
char *a2sockaddr(struct sockaddr_in *, char *, char *);
int recv_icmp(int, u_char *, int);
void send_icmp(int, int, struct sockaddr_in *);
u_short in_cksum(u_short *, int);
void pr_icmph(struct icmp *);
void pr_iph(struct ip *);
void pr_retip(struct ip *);
void pr_rroption(char *);
char *pr_addr(struct in_addr);
char *pr_quad_addr(struct in_addr);
#else	/* __STDC__ */
int recv_icmp();
void send_icmp();
u_short in_cksum();
void pr_icmph();
void pr_iph();
void pr_retip();
void pr_rroption();
void pr_stats();
char *pr_addr();
char *pr_quad_addr();
#endif	/* __STDC__ */

/* System routines */
extern char *inet_ntoa();
extern int optind;
extern int errno;
