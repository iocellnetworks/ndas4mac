#include <sys/kernel.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/time.h>
//#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
//#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
//#include <sys/sx.h>
#include <sys/systm.h>
#include <sys/domain.h>

#include <net/route.h>
#include <netinet/tcp_fsm.h>

#include "lpx.h"
#include "lpx_if.h"
#include "lpx_pcb.h"
#include "lpx_var.h"
#include "lpx_stream_var.h"
#include "lpx_stream.h"
#include "lpx_base.h"
#include "lpx_stream_timer.h"

#include "Utilities.h"

#include <sys/proc.h>

#include <machine/spl.h>
#include <net/ethernet.h>
#include <string.h>

extern struct  domain lpxdomain;

void lpx_stream_fasttimo()
{
    register struct lpxpcb *lpxp;
    register struct stream_pcb *cb;
//    int s = splnet();
	
//    DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("smp_fasttimo\n"));
	
	lck_mtx_assert(lpxdomain.dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
	lck_mtx_lock(lpxdomain.dom_mtx);

    lpxp = lpx_stream_pcb.lpxp_next;
	for (; lpxp != &lpx_stream_pcb; lpxp = lpxp->lpxp_next) {
		if ((cb = lpxpcbtostreampcb(lpxp)) != NULL &&
			(cb->s_flags & SF_DELACK)) {
			cb->s_flags &= ~SF_DELACK;
			cb->s_flags |= SF_ACKNOW;
			lpx_stream_stat.smps_delack++;
			lpx_stream_output(cb, (struct mbuf *)NULL);
		}
	}
 	
	lck_mtx_assert(lpxdomain.dom_mtx, LCK_MTX_ASSERT_OWNED);
	lck_mtx_unlock(lpxdomain.dom_mtx);

//    splx(s);
}

/*
 * smp protocol timeout routine called every 500 ms.
 * Updates the timers in all active pcb's and
 * causes finite state machine actions if timers expire.
 */
void lpx_stream_slowtimo()
{
    register struct lpxpcb *lpxp, *lpxpnxt;
    register struct stream_pcb *cb;
//    int s = splnet();
    register int i;
	
//    DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("smp_slowtimo\n"));

	lck_mtx_assert(lpxdomain.dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
	lck_mtx_lock(lpxdomain.dom_mtx);
	
    /*
     * Search through tcb's and update active timers.
     */
    lpxp = lpx_stream_pcb.lpxp_next;
    if (lpxp == NULL) {
 //       splx(s);
		lck_mtx_assert(lpxdomain.dom_mtx, LCK_MTX_ASSERT_OWNED);
		lck_mtx_unlock(lpxdomain.dom_mtx);
		
        return;
    }
	
	//lpx_stream_lock(lpxp->lpxp_socket, 0, 0);
	
    while (lpxp != &lpx_stream_pcb) {
				
        cb = lpxpcbtostreampcb(lpxp);
        lpxpnxt = lpxp->lpxp_next;

        if (cb == NULL) {
						
            goto tpgone;
		}

		if (cb->s_flags_control & SF_NEEDFREE) {
				
			cb->s_flags_control &= ~SF_NEEDFREE;
			
			 DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("smp_slowtimo: Call dispense lpxp %p\n", lpxp));
			
			Lpx_PCB_dispense(lpxp);
	
			goto tpgone;
		}

        for (i = 0; i < SMPT_NTIMERS; i++) {
			//            if (i == SMPT_KEEP)
			//                DEBUG_PRINT(2, ("cb = %p time = %d\n", cb, cb->s_timer[SMPT_KEEP]));
            if (cb->s_timer[i] && --cb->s_timer[i] == 0) {
                lpx_stream_timers(cb, i);
                if (lpxpnxt->lpxp_prev != lpxp)
                    goto tpgone;
            }
        }
		
		cb->s_idle++;
        
		if (cb->s_rtt)
            cb->s_rtt++;
tpgone:
		
		lpxp = lpxpnxt;
    }
	
    lpx_stream_iss += SMP_ISSINCR/PR_SLOWHZ;       /* increment iss */
    
	lck_mtx_assert(lpxdomain.dom_mtx, LCK_MTX_ASSERT_OWNED);
	lck_mtx_unlock(lpxdomain.dom_mtx);
	
	//lpx_stream_unlock(lpxp->lpxp_socket, 0, 0);
	//splx(s);
}
