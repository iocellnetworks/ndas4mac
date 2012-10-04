/***************************************************************************
*
*  Utilities.c
*
*	Utilities for Debugging.
*	Contains the copy of source codes of bsd/net in xnu about socket functions
*	from http://opensource.apple.com which is not exported & is unsupported   
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/protosw.h>
#include <sys/domain.h>

#include <libkern/OSAtomic.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include "lpx.h"
#include <Utilities.h>
#include <sys/queue.h>

uint32_t	debugLevel = 0x33333333; 

/************************************************************************
 * Copied from xnu-1228/bsd/kern/uipc_mbuf.c
 ************************************************************************/

struct mbuf *
m_dtom(void *x)
{
        return ((struct mbuf *)((u_long)(x) & ~(MSIZE-1)));
}

/************************************************************************
 * Copied from xnu-1228/bsd/kern/uipc_socket2.c
 ************************************************************************/

extern void postevent(struct socket *, struct sockbuf *, int);

/*
 * Drop a record off the front of a sockbuf
 * and move the next record to the front.
 */
void
sbdroprecord(struct sockbuf *sb)
{
        struct mbuf *m, *mn;

        m = sb->sb_mb;
        if (m) {
                sb->sb_mb = m->m_nextpkt;
                do {
                        sbfree(sb, m);
                        MFREE(m, mn);
                        m = mn;
                } while (m);
        }
        SB_EMPTY_FIXUP(sb);
        postevent(0, sb, EV_RWBYTES);
}

/* adjust counters in sb reflecting freeing of m */
void
sbfree(struct sockbuf *sb, struct mbuf *m)
{
        int cnt = -1;
        sb->sb_cc -= m->m_len;
        if (m->m_type != MT_DATA && m->m_type != MT_HEADER &&
                m->m_type != MT_OOBDATA)
                sb->sb_ctl -= m->m_len;
        sb->sb_mbcnt -= MSIZE;
        if (m->m_flags & M_EXT) {
                sb->sb_mbcnt -= m->m_ext.ext_size;
                cnt -= m->m_ext.ext_size / MSIZE ;
        }
//        OSAddAtomic(cnt, (SInt32*)&total_mb_cnt);
}

/*
 * Wakeup processes waiting on a socket buffer.
 * Do asynchronous notification via SIGIO
 * if the socket has the SS_ASYNC flag set.
 */
void
sowakeup(struct socket *so, struct sockbuf *sb)
{
        sb->sb_flags &= ~SB_SEL;
        selwakeup(&sb->sb_sel);
        if (sb->sb_flags & SB_WAIT) {
                sb->sb_flags &= ~SB_WAIT;
                wakeup((caddr_t)&sb->sb_cc);
        }
#if 0 // Intentially commented
        if (so->so_state & SS_ASYNC) {
                if (so->so_pgid < 0)
                        gsignal(-so->so_pgid, SIGIO);
                else if (so->so_pgid > 0)
                        proc_signal(so->so_pgid, SIGIO);
        }
        if (sb->sb_flags & SB_KNOTE) {
                KNOTE(&sb->sb_sel.si_note, SO_FILT_HINT_LOCKED);
        }
        if (sb->sb_flags & SB_UPCALL) {
                void (*so_upcall)(struct socket *, caddr_t, int);
                caddr_t so_upcallarg;

                so_upcall = so->so_upcall;
                so_upcallarg = so->so_upcallarg;
                /* Let close know that we're about to do an upcall */
                so->so_flags |= SOF_UPCALLINUSE;

                socket_unlock(so, 0);
                (*so_upcall)(so, so_upcallarg, M_DONTWAIT);
                socket_lock(so, 0);

                so->so_flags &= ~SOF_UPCALLINUSE;
                /* Tell close that it's safe to proceed */
                if (so->so_flags & SOF_CLOSEWAIT)
                        wakeup((caddr_t)&so->so_upcall);
        }
#endif
}

void
sowwakeup(struct socket *so)
{
        if (sb_notify(&so->so_snd))
                sowakeup(so, &so->so_snd);
}

/*
 * Do we need to notify the other side when I/O is possible?
 */

int
sb_notify(struct sockbuf *sb)
{
        return ((sb->sb_flags &
            (SB_WAIT|SB_SEL|SB_ASYNC|SB_UPCALL|SB_KNOTE)) != 0);
}
