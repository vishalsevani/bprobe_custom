/*

   SGI specific routines to adjust the priority of process
   so that packets timers are accurate.
   
   $Header: /home/roycroft/carter/Ping+/rpr/src/Release/RCS/raisePriority.c,v 1.3 1996/06/21 15:55:22 carter Exp $

*/
   

//#include <sys/schedctl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

static char rcsid[] = "$Id: raisePriority.c,v 1.3 1996/06/21 15:55:22 carter Exp $";

void setRealTimePriority (void) {
  int OldPri, NewPri;
  /* go to real-time priority */
  /* this also allows 1ms granularity for setitimer */
  if ((OldPri = schedctl(NDPRI, 0, NDPHIMAX)) < 0)
    {
      printf("setRealTimePriority: Couldn't set NDPRI %d\n",
	     NDPHIMAX);
      exit(1);
    }
  
  NewPri = schedctl(GETNDPRI, 0);
#ifdef VERBOSE
  printf("priority raised from %d to %d [%d]\n", OldPri, NewPri, NewPri == NDPHIMAX);
#endif
   } /* end setRealTimePriority */

void clearRealTimePriority (void) {
  int OldPri;
  int NewPri = 0;

  /* leave real-time priority */
  OldPri = schedctl(NDPRI, 0, NewPri);
#ifdef VERBOSE
  printf("priority lowered from %d to %d [%d]\n", OldPri, NewPri, NewPri == 0);
#endif
} /* end clearRealTimePriority */


