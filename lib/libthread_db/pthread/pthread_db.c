/*
 * Copyright (c) 2004 David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <thr_private.h>
#include <sys/types.h>
#include <sys/kse.h>
#include <sys/ptrace.h>
#include <proc_service.h>
#include <thread_db.h>

#include "pthread_db.h"

struct pt_map {
	enum {
		PT_NONE,
		PT_USER,
		PT_LWP
	} type;

	union {
		lwpid_t  lwp;
		psaddr_t thr; 
	};
};

#define P2T(c) ps2td(c)

static td_err_e pt_ta_activated(pt_thragent_t *ta, int *a);
static long pt_map_thread(const pt_thragent_t *ta, psaddr_t pt);
static long pt_map_lwp(const pt_thragent_t *ta, lwpid_t lwp);
static void pt_unmap_lwp(const pt_thragent_t *ta, lwpid_t lwp);
static int pt_validate(const td_thrhandle_t *th);

static int
ps2td(int c)
{
	switch (c) {
	case PS_OK:
		return TD_OK;
	case PS_ERR:
		return TD_ERR;
	case PS_BADPID:
		return TD_BADPH;
	case PS_BADLID:
		return TD_NOLWP;
	case PS_BADADDR:
		return TD_ERR;
	case PS_NOSYM:
		return TD_NOLIBTHREAD;
	case PS_NOFREGS:
		return TD_NOFPREGS;
	default:
		return TD_ERR;
	}
}

static td_err_e
pt_init(void)
{
	pt_md_init();
	return (0);
}

static td_err_e
pt_ta_new(struct ps_prochandle *ph, pt_thragent_t **pta)
{
#define LOOKUP_SYM(proc, sym, addr) 			\
	ret = ps_pglobal_lookup(proc, NULL, sym, addr);	\
	if (ret != 0) {					\
		TDBG("can not find symbol: %s\n", sym);	\
		ret = TD_NOLIBTHREAD;			\
		goto error;				\
	}

	pt_thragent_t *ta;
	int dbg;
	int ret;

	TDBG_FUNC();

	ta = malloc(sizeof(pt_thragent_t));
	if (ta == NULL)
		return (TD_MALLOC);

	ta->ph = ph;
	ta->thread_activated = 0;
	ta->map = NULL;
	ta->map_len = 0;

	LOOKUP_SYM(ph, "_libkse_debug",		&ta->libkse_debug_addr);
	LOOKUP_SYM(ph, "_thread_list",		&ta->thread_list_addr);
	LOOKUP_SYM(ph, "_thread_activated",	&ta->thread_activated_addr);
	LOOKUP_SYM(ph, "_thread_active_threads",&ta->thread_active_threads_addr);
	LOOKUP_SYM(ph, "_thread_keytable",	&ta->thread_keytable_addr);

	dbg = getpid();
	/*
	 * If this fails it probably means we're debugging a core file and
	 * can't write to it.
	 */
	ps_pdwrite(ph, ta->libkse_debug_addr, &dbg, sizeof(int));
	*pta = ta;
	return (0);

error:
	free(ta);
	return (ret);
}

static td_err_e
pt_ta_delete(pt_thragent_t *ta)
{
	int dbg;

	TDBG_FUNC();

	dbg = 0;
	/*
	 * Error returns from this write are not really a problem;
	 * the process doesn't exist any more.
	 */
	ps_pdwrite(ta->ph, ta->libkse_debug_addr, &dbg, sizeof(int));
	if (ta->map)
		free(ta->map);
	free(ta);
	return (TD_OK);
}

static td_err_e
pt_ta_get_nthreads (const pt_thragent_t *ta, int *np)
{
	int ret;

	TDBG_FUNC();

	ret = ps_pdread(ta->ph, ta->thread_active_threads_addr, np,
	                sizeof(int));
	return (P2T(ret));
}

static td_err_e
pt_ta_get_ph(const pt_thragent_t *ta, struct ps_prochandle **ph)
{
	TDBG_FUNC();

	*ph = ta->ph;
	return (TD_OK);
}

static td_err_e
pt_ta_map_id2thr(const pt_thragent_t *ta, thread_t id, td_thrhandle_t *th)
{
	prgregset_t gregs;
	TAILQ_HEAD(, pthread) thread_list;
	psaddr_t pt, tcb_addr;
	lwpid_t lwp;
	int ret;

	TDBG_FUNC();

	if (id < 0 || id >= ta->map_len || ta->map[id].type == PT_NONE)
		return (TD_NOTHR);
	ret = ps_pdread(ta->ph, ta->thread_list_addr, &thread_list,
			sizeof(thread_list));
	if (ret != 0)
		return (P2T(ret));
	pt = (psaddr_t)thread_list.tqh_first;
	if (ta->map[id].type == PT_LWP) {
		/*
		 * if we are referencing a lwp, make sure it was not already
		 * mapped to user thread.
		 */
		while (pt != 0) {
			ret = ps_pdread(ta->ph,
			        pt + offsetof(struct pthread, tcb),
			        &tcb_addr, sizeof(tcb_addr));
			if (ret != 0)
				return (P2T(ret));
			ret = ps_pdread(ta->ph,
			        tcb_addr + offsetof(struct tcb,
				  tcb_tmbx.tm_lwp),
				&lwp, sizeof(lwp));
			if (ret != 0)
				return (P2T(ret));
			/*
			 * If the lwp was already mapped to userland thread,
			 * we shouldn't reference it directly in future.
			 */
			if (lwp == ta->map[id].lwp) {
				ta->map[id].type = PT_NONE;
				return (TD_NOTHR);
			}
			/* get next thread */
			ret = ps_pdread(ta->ph,
			        pt + offsetof(struct pthread, tle.tqe_next),
			        &pt, sizeof(pt));
			if (ret != 0)
				return (P2T(ret));
		}
		/* check lwp */
		ret = ptrace(PT_GETREGS, ta->map[id].lwp, (caddr_t)&gregs, 0);
		if (ret != 0) {
			/* no longer exists */
			ta->map[id].type = PT_NONE;
			return (TD_NOTHR);
		}
	} else {
		while (pt != 0 && ta->map[id].thr != pt) {
			ret = ps_pdread(ta->ph,
				pt + offsetof(struct pthread, tcb),
				&tcb_addr, sizeof(tcb_addr));
			if (ret != 0)
				return (P2T(ret));
			/* get next thread */
			ret = ps_pdread(ta->ph,
				pt + offsetof(struct pthread, tle.tqe_next),
				&pt, sizeof(pt));
			if (ret != 0)
				return (P2T(ret));
		}

		if (pt == 0) {
			/* no longer exists */
			ta->map[id].type = PT_NONE;
			return (TD_NOTHR);
		}
	}
	th->th_ta_p = (td_thragent_t *)ta;
	th->th_unique = id;
	return (TD_OK);
}

static td_err_e
pt_ta_map_lwp2thr(const pt_thragent_t *ta, lwpid_t lwp, td_thrhandle_t *th)
{
	TAILQ_HEAD(, pthread) thread_list;
	psaddr_t pt, ptr;
	lwpid_t tmp_lwp;
	int ret;
	
	TDBG_FUNC();

	ret = ps_pdread(ta->ph, ta->thread_list_addr, &thread_list,
	                sizeof(thread_list));
	if (ret != 0)
		return (P2T(ret));
	pt = (psaddr_t)thread_list.tqh_first;
	while (pt != 0) {
		ret = ps_pdread(ta->ph, pt + offsetof(struct pthread, tcb),
				&ptr, sizeof(ptr));
		if (ret != 0)
			return (P2T(ret));
		ptr += offsetof(struct tcb, tcb_tmbx.tm_lwp);
		ret = ps_pdread(ta->ph, ptr, &tmp_lwp, sizeof(lwpid_t));
		if (ret != 0)
			return (P2T(ret));
		if (tmp_lwp == lwp) {
			th->th_ta_p = (td_thragent_t *)ta;
			th->th_unique = pt_map_thread(ta, pt);
			if (th->th_unique == -1)
				return (TD_MALLOC);
			pt_unmap_lwp(ta, lwp);
			return (TD_OK);
		}

		/* get next thread */
		ret = ps_pdread(ta->ph,
		           pt + offsetof(struct pthread, tle.tqe_next), 
		           &pt, sizeof(pt));
		if (ret != 0)
			return (P2T(ret));
	}

	return (TD_NOTHR);
}

static td_err_e
pt_ta_thr_iter(const pt_thragent_t *ta,
               td_thr_iter_f *callback, void *cbdata_p,
               td_thr_state_e state, int ti_pri,
               sigset_t *ti_sigmask_p,
               unsigned int ti_user_flags)
{
	TAILQ_HEAD(, pthread) thread_list;
	td_thrhandle_t th;
	psaddr_t pt;
	int ret, activated;
	
	TDBG_FUNC();

	ret = pt_ta_activated((pt_thragent_t *)ta, &activated);
	if (ret != 0)
		return (P2T(ret));
	if (!activated)
		return (0);
	ret = ps_pdread(ta->ph, ta->thread_list_addr, &thread_list,
	                sizeof(thread_list));
	if (ret != 0)
		return (P2T(ret));
	pt = (psaddr_t)thread_list.tqh_first;
	while (pt != 0) {
		th.th_ta_p = (td_thragent_t *)ta;
		th.th_unique = pt_map_thread(ta, pt);
		/* should we unmap lwp here ? */
		if (th.th_unique == -1)
			return (TD_MALLOC);
		if ((*callback)(&th, cbdata_p))
			return (TD_DBERR);
		/* get next thread */
		ret = ps_pdread(ta->ph,
		                pt + offsetof(struct pthread, tle.tqe_next),
	                        &pt, sizeof(pt));
		if (ret != 0)
			return (P2T(ret));
	}
	return (TD_OK);
}

static td_err_e
pt_ta_tsd_iter(const pt_thragent_t *ta, td_key_iter_f *ki, void *arg)
{
	struct pthread_key keytable[PTHREAD_KEYS_MAX];
	int i, ret;

	TDBG_FUNC();

	ret = ps_pdread(ta->ph, (psaddr_t)ta->thread_keytable_addr, keytable,
	                sizeof(keytable));
	if (ret != 0)
		return (P2T(ret));

	for (i = 0; i < PTHREAD_KEYS_MAX; i++) {
		if (keytable[i].allocated) {
			ret = (ki)(i, keytable[i].destructor, arg);
			if (ret != 0)
				return (TD_DBERR);
		}
	}
	return (TD_OK);
}

static td_err_e
pt_ta_event_addr(const pt_thragent_t *ta, td_event_e event, td_notify_t *ptr)
{
	TDBG_FUNC();
	return (TD_NOEVENT);
}

static td_err_e
pt_ta_set_event(const pt_thragent_t *ta, td_thr_events_t *events)
{
	TDBG_FUNC();
	return (TD_ERR);
}

static td_err_e
pt_ta_clear_event(const pt_thragent_t *ta, td_thr_events_t *events)
{
	TDBG_FUNC();
	return (TD_ERR);
}

static td_err_e
pt_ta_event_getmsg(const pt_thragent_t *ta, td_event_msg_t *msg)
{
	TDBG_FUNC();
	return (TD_NOMSG);
}

static td_err_e
pt_ta_setconcurrency(const pt_thragent_t *ta, int level)
{
	TDBG_FUNC();
	return (TD_OK);
}

static td_err_e
pt_ta_enable_stats(const pt_thragent_t *ta, int enable)
{
	TDBG_FUNC();
	return (TD_OK);
}

static td_err_e
pt_ta_reset_stats(const pt_thragent_t *ta)
{
	TDBG_FUNC();
	return (TD_OK);
}

static td_err_e
pt_ta_get_stats(const pt_thragent_t *ta, td_ta_stats_t *statsp)
{
	TDBG_FUNC();
	return (TD_OK);
}

static td_err_e
pt_thr_validate(const td_thrhandle_t *th)
{
	td_thrhandle_t temp;
	int ret;

	TDBG_FUNC();

	ret = pt_ta_map_id2thr((pt_thragent_t *)th->th_ta_p, th->th_unique,
	                       &temp);
	return (P2T(ret));
}

static td_err_e
pt_thr_get_info(const td_thrhandle_t *th, td_thrinfo_t *info)
{
	struct pthread pt;
	pt_thragent_t *ta = (pt_thragent_t *)th->th_ta_p;
	psaddr_t tcb_addr;
	uint32_t dflags;
	int ret;

	TDBG_FUNC();

	ret = pt_validate(th);
	if (ret)
		return (ret);

	memset(info, 0, sizeof(*info));
	if (ta->map[th->th_unique].type == PT_LWP) {
		info->ti_type = TD_THR_SYSTEM;
		info->ti_lid = ta->map[th->th_unique].lwp;
		info->ti_tid = th->th_unique;
		info->ti_state = TD_THR_RUN;
		info->ti_type = TD_THR_SYSTEM;
		return (TD_OK);
	}

	ret = ps_pdread(ta->ph, (psaddr_t)(ta->map[th->th_unique].thr),
	                &pt, sizeof(pt));
	if (ret != 0)
		return (P2T(ret));
	if (pt.magic != THR_MAGIC)
		return (TD_BADTH);
	tcb_addr = (psaddr_t) pt.tcb;
	ret = ps_pdread(ta->ph,
	        tcb_addr + offsetof(struct tcb, tcb_tmbx.tm_lwp),
	        &info->ti_lid, sizeof(lwpid_t));
	if (ret != 0)
		return (P2T(ret));
	ret = ps_pdread(ta->ph,
		tcb_addr + offsetof(struct tcb, tcb_tmbx.tm_dflags),
		&dflags, sizeof(dflags));
	info->ti_ta_p = th->th_ta_p;
	info->ti_tid = th->th_unique;
	info->ti_tls = (char *)pt.specific; 
	info->ti_startfunc = (psaddr_t)pt.start_routine;
	info->ti_stkbase = (psaddr_t) pt.attr.stackaddr_attr;
	info->ti_stksize = pt.attr.stacksize_attr;
	switch (pt.state) {
	case PS_RUNNING:
		info->ti_state = TD_THR_RUN;
		break;
	case PS_LOCKWAIT:
	case PS_MUTEX_WAIT:
	case PS_COND_WAIT:
	case PS_SIGSUSPEND:
	case PS_SIGWAIT:
	case PS_JOIN:
	case PS_SUSPENDED:
	case PS_DEADLOCK:
	case PS_SLEEP_WAIT:
		info->ti_state = TD_THR_SLEEP;
		break;
	case PS_DEAD:
		info->ti_state = TD_THR_ZOMBIE;
		break;
	default:
		info->ti_state = TD_THR_UNKNOWN;
		break;
	}

	info->ti_db_suspended = ((dflags & TMDF_DONOTRUNUSER) != 0);
	info->ti_type = TD_THR_USER;
	info->ti_pri = pt.active_priority;
	info->ti_sigmask = pt.sigmask;
	info->ti_traceme = 0; 
	info->ti_pending = pt.sigpend;
	info->ti_events = 0;
	return (0);
}

static td_err_e
pt_thr_getfpregs(const td_thrhandle_t *th, prfpregset_t *fpregs)
{
	struct kse_thr_mailbox tmbx;
	pt_thragent_t *ta = (pt_thragent_t *)th->th_ta_p;
	psaddr_t tcb_addr, tmbx_addr, ptr;
	lwpid_t lwp;
	int ret;

	TDBG_FUNC();

	ret = pt_validate(th);
	if (ret)
		return (ret);

	if (ta->map[th->th_unique].type == PT_LWP) {
		ret = ps_lgetfpregs(ta->ph, ta->map[th->th_unique].lwp, fpregs);
		return (P2T(ret));
	}

	ret = ps_pdread(ta->ph, ta->map[th->th_unique].thr +
 	                offsetof(struct pthread, tcb),
                        &tcb_addr, sizeof(tcb_addr));
	if (ret != 0)
		return (P2T(ret));
	tmbx_addr = tcb_addr + offsetof(struct tcb, tcb_tmbx);
	ptr = tmbx_addr + offsetof(struct kse_thr_mailbox, tm_lwp);
	ret = ps_pdread(ta->ph, ptr, &lwp, sizeof(lwpid_t));
	if (ret != 0)
		return (P2T(ret));
	if (lwp != 0) {
		ret = ps_lgetfpregs(ta->ph, lwp, fpregs);
		return (P2T(ret));
	}

	ret = ps_pdread(ta->ph, tmbx_addr, &tmbx, sizeof(tmbx));
	if (ret != 0)
		return (P2T(ret));
	pt_ucontext_to_fpreg(&tmbx.tm_context, fpregs);
	return (0);
}

static td_err_e
pt_thr_getgregs(const td_thrhandle_t *th, prgregset_t gregs)
{
	struct kse_thr_mailbox tmbx;
	pt_thragent_t *ta = (pt_thragent_t *)th->th_ta_p;
	psaddr_t tcb_addr, tmbx_addr, ptr;
	lwpid_t lwp;
	int ret;

	TDBG_FUNC();

	ret = pt_validate(th);
	if (ret)
		return (ret);

	if (ta->map[th->th_unique].type == PT_LWP) {
		ret = ps_lgetregs(ta->ph,
		                  ta->map[th->th_unique].lwp, gregs);
		return (P2T(ret));
	}

	ret = ps_pdread(ta->ph, ta->map[th->th_unique].thr +
	                offsetof(struct pthread, tcb),
			&tcb_addr, sizeof(tcb_addr));
	if (ret != 0)
		return (P2T(ret));
	tmbx_addr = tcb_addr + offsetof(struct tcb, tcb_tmbx);
	ptr = tmbx_addr + offsetof(struct kse_thr_mailbox, tm_lwp);
	ret = ps_pdread(ta->ph, ptr, &lwp, sizeof(lwpid_t));
	if (ret != 0)
		return (P2T(ret));
	if (lwp != 0) {
		ret = ps_lgetregs(ta->ph, lwp, gregs);
		return (P2T(ret));
	}
	ret = ps_pdread(ta->ph, tmbx_addr, &tmbx, sizeof(tmbx));
	if (ret != 0)
		return (P2T(ret));
	pt_ucontext_to_reg(&tmbx.tm_context, gregs);
	return (0);
}

static td_err_e
pt_thr_getxregs (const td_thrhandle_t *th, void *xregs)
{
	return (TD_NOXREGS);
}

static td_err_e
pt_thr_getxregsize (const td_thrhandle_t *th, int *sizep)
{
	return (TD_NOXREGS);
}

static td_err_e
pt_thr_setfpregs(const td_thrhandle_t *th, const prfpregset_t *fpregs)
{
	struct kse_thr_mailbox tmbx;
	pt_thragent_t *ta = (pt_thragent_t *)th->th_ta_p;
	psaddr_t tcb_addr, tmbx_addr, ptr;
	lwpid_t lwp;
	int ret;

	TDBG_FUNC();

	ret = pt_validate(th);
	if (ret)
		return (ret);

	if (ta->map[th->th_unique].type == PT_LWP) {
		ret = ps_lsetfpregs(ta->ph, ta->map[th->th_unique].lwp, fpregs);
		return (P2T(ret));
	}

	ret = ps_pdread(ta->ph, ta->map[th->th_unique].thr +
	                offsetof(struct pthread, tcb),
                        &tcb_addr, sizeof(tcb_addr));
	if (ret != 0)
		return (P2T(ret));
	tmbx_addr = tcb_addr + offsetof(struct tcb, tcb_tmbx);
	ptr = tmbx_addr + offsetof(struct kse_thr_mailbox, tm_lwp);
	ret = ps_pdread(ta->ph, ptr, &lwp, sizeof(lwpid_t));
	if (ret != 0)
		return (P2T(ret));
	if (lwp != 0) {
		ret = ps_lsetfpregs(ta->ph, lwp, fpregs);
		return (P2T(ret));
	}
	/*
	 * Read a copy of context, this makes sure that registers
	 * not covered by structure reg won't be clobbered
	 */
	ret = ps_pdread(ta->ph, tmbx_addr, &tmbx, sizeof(tmbx));
	if (ret != 0)
		return (P2T(ret));

	pt_fpreg_to_ucontext(fpregs, &tmbx.tm_context);
	ret = ps_pdwrite(ta->ph, tmbx_addr, &tmbx, sizeof(tmbx));
	return (P2T(ret));
}

static td_err_e
pt_thr_setgregs(const td_thrhandle_t *th, const prgregset_t gregs)
{
	struct kse_thr_mailbox tmbx;
	pt_thragent_t *ta = (pt_thragent_t *)th->th_ta_p;
	psaddr_t tcb_addr, tmbx_addr, ptr;
	lwpid_t lwp;
	int ret;

	TDBG_FUNC();

	ret = pt_validate(th);
	if (ret)
		return (ret);

	if (ta->map[th->th_unique].type == PT_LWP) {
		ret = ps_lsetregs(ta->ph, ta->map[th->th_unique].lwp, gregs);
		return (P2T(ret));
	}

	ret = ps_pdread(ta->ph, ta->map[th->th_unique].thr +
	                offsetof(struct pthread, tcb),
	                &tcb_addr, sizeof(tcb_addr));
	if (ret != 0)
		return (P2T(ret));
	tmbx_addr = tcb_addr + offsetof(struct tcb, tcb_tmbx);
	ptr = tmbx_addr + offsetof(struct kse_thr_mailbox, tm_lwp);
	ret = ps_pdread(ta->ph, ptr, &lwp, sizeof(lwpid_t));
	if (ret != 0)
		return (P2T(ret));
	if (lwp != 0) {
		ret = ps_lsetregs(ta->ph, lwp, gregs);
		return (P2T(ret));
	}

	/*
	 * Read a copy of context, make sure that registers
	 * not covered by structure reg won't be clobbered
	 */
	ret = ps_pdread(ta->ph, tmbx_addr, &tmbx, sizeof(tmbx));
	if (ret != 0)
		return (P2T(ret));
	pt_reg_to_ucontext(gregs, &tmbx.tm_context);
	ret = ps_pdwrite(ta->ph, tmbx_addr, &tmbx, sizeof(tmbx));
	return (P2T(ret));
}

static td_err_e
pt_thr_setxregs(const td_thrhandle_t *th, const void *addr)
{
	TDBG_FUNC();
	return (TD_NOXREGS);
}

static td_err_e
pt_thr_event_enable(const td_thrhandle_t *th, int en)
{
	TDBG_FUNC();
	return (TD_ERR);
}

static td_err_e
pt_thr_set_event(const td_thrhandle_t *th, td_thr_events_t *setp)
{
	TDBG_FUNC();
	return (TD_ERR);
}

static td_err_e
pt_thr_clear_event(const td_thrhandle_t *th, td_thr_events_t *setp)
{
	TDBG_FUNC();
	return (TD_ERR);
}

static td_err_e
pt_thr_event_getmsg(const td_thrhandle_t *th, td_event_msg_t *msg)
{
	TDBG_FUNC();
	return (TD_NOMSG);
}

static td_err_e
pt_thr_setprio(const td_thrhandle_t *th, int pri)
{
	TDBG_FUNC();
	return (TD_OK);
}

static td_err_e
pt_thr_setsigpending(const td_thrhandle_t *th, unsigned char n,
	const sigset_t *set)
{
	TDBG_FUNC();
	return (TD_OK);
}

static td_err_e
pt_thr_sigsetmask(const td_thrhandle_t *th, const sigset_t *set)
{
	TDBG_FUNC();
	return (TD_OK);
}

static td_err_e
pt_thr_tsd(const td_thrhandle_t *th, const thread_key_t key, void **data)
{
	pt_thragent_t *ta = (pt_thragent_t *)th->th_ta_p;
	struct pthread_specific_elem *spec, elem;
	int ret;

	TDBG_FUNC();

	ret = pt_validate(th);
	if (ret)
		return (ret);

	if (key < 0 || key >= PTHREAD_KEYS_MAX)
		return (TD_BADKEY);

	if (ta->map[th->th_unique].type == PT_LWP) {
		*data = NULL;
		return (TD_OK);
	}

	ret = ps_pdread(ta->ph,
	                ta->map[th->th_unique].thr +
	                  offsetof(struct pthread, specific),
	                &spec, sizeof(spec));
	if (ret == 0) {
		if (spec == NULL) {
			*data = NULL;
			return (0);
		}
		ret = ps_pdread(ta->ph, (psaddr_t)&spec[key],
			&elem, sizeof(elem));
		if (ret == 0)
			*data = (void *)elem.data;
	}
	return (P2T(ret));
}

static td_err_e
pt_dbsuspend(const td_thrhandle_t *th, int suspend)
{
	pt_thragent_t *ta = (pt_thragent_t *)th->th_ta_p;
	psaddr_t tcb_addr, tmbx_addr, ptr;
	lwpid_t lwp;
	uint32_t dflags;
	int attrflags;
	int ret;

	TDBG_FUNC();

	ret = pt_validate(th);
	if (ret)
		return (ret);

	if (ta->map[th->th_unique].type == PT_LWP) {
		if (suspend)
			ret = ps_lstop(ta->ph, ta->map[th->th_unique].lwp);
		else
			ret = ps_lcontinue(ta->ph, ta->map[th->th_unique].lwp);
		return (P2T(ret));
	}

	ret = ps_pdread(ta->ph, ta->map[th->th_unique].thr +
		offsetof(struct pthread, attr.flags),
		&attrflags, sizeof(attrflags));
	if (ret != 0)
		return (P2T(ret));
	ret = ps_pdread(ta->ph, ta->map[th->th_unique].thr +
	                offsetof(struct pthread, tcb),
	                &tcb_addr, sizeof(tcb_addr));
	if (ret != 0)
		return (P2T(ret));
	tmbx_addr = tcb_addr + offsetof(struct tcb, tcb_tmbx);
	ptr = tmbx_addr + offsetof(struct kse_thr_mailbox, tm_lwp);
	ret = ps_pdread(ta->ph, ptr, &lwp, sizeof(lwpid_t));
	if (ret != 0)
		return (P2T(ret));
	/*
	 * don't stop lwp assigned to a M:N thread, it belongs
	 * to UTS, UTS shouldn't be stopped.
	 */
	if (lwp != 0 && (attrflags & PTHREAD_SCOPE_SYSTEM)) {
		/* dont' suspend signal thread */
		if (attrflags & THR_SIGNAL_THREAD)
			return 0;
		if (suspend)
			ret = ps_lstop(ta->ph, lwp);
		else
			ret = ps_lcontinue(ta->ph, lwp);
		return (P2T(ret));
	}

	ret = ps_pdread(ta->ph,
		tmbx_addr + offsetof(struct kse_thr_mailbox, tm_dflags),
		&dflags, sizeof(dflags));
	if (ret != 0)
		return (P2T(ret));

	if (suspend)
		dflags |= TMDF_DONOTRUNUSER;
	else
		dflags &= ~TMDF_DONOTRUNUSER;
	ret = ps_pdwrite(ta->ph,
	       tmbx_addr + offsetof(struct kse_thr_mailbox, tm_dflags),
	       &dflags, sizeof(dflags));
	return (P2T(ret));
}

static td_err_e
pt_thr_dbsuspend(const td_thrhandle_t *th)
{
	TDBG_FUNC();
	return pt_dbsuspend(th, 1);
}

static td_err_e
pt_thr_dbresume(const td_thrhandle_t *th)
{
	TDBG_FUNC();
	return pt_dbsuspend(th, 0);
}

static td_err_e
pt_ta_activated(pt_thragent_t *ta, int *a)
{
	int ret;

	TDBG_FUNC();

	if (ta->thread_activated) {
		*a = ta->thread_activated;
		return (TD_OK);
	}
	ret = ps_pdread(ta->ph, ta->thread_activated_addr,
		  &ta->thread_activated, sizeof(int));
	if (ret == 0)
		*a = ta->thread_activated;
	return (P2T(ret));
}

static td_err_e
pt_thr_sstep(td_thrhandle_t *th, int step)
{
	struct kse_thr_mailbox tmbx;
	struct reg regs;
	pt_thragent_t *ta = (pt_thragent_t *)th->th_ta_p;
	psaddr_t tcb_addr, tmbx_addr;
	uint32_t tmp;
	lwpid_t lwp;
	int ret;

	TDBG_FUNC();

	ret = pt_validate(th);
	if (ret)
		return (ret);

	if (ta->map[th->th_unique].type == PT_LWP) {
		/* Let debugger deal with single step flag in in kernel */
		return (0);
	}

	ret = ps_pdread(ta->ph, ta->map[th->th_unique].thr + 
	                offsetof(struct pthread, tcb),
	                &tcb_addr, sizeof(tcb_addr));
	if (ret != 0)
		return (P2T(ret));

	/* Clear or set single step flag in thread mailbox */
	tmp = step ? TMDF_SSTEP : 0;
	ret = ps_pdwrite(ta->ph, tcb_addr + offsetof(struct tcb,
	                 tcb_tmbx.tm_dflags), &tmp, sizeof(tmp));
	if (ret != 0)
		return (P2T(ret));
	/* Get lwp */
	ret = ps_pdread(ta->ph, tcb_addr + offsetof(struct tcb,
	                tcb_tmbx.tm_lwp), &lwp, sizeof(lwpid_t));
	if (ret != 0)
		return (P2T(ret));
	if (lwp != 0) /* Let debugger deal with single step flag in in kernel */
		return (0);

	tmbx_addr = tcb_addr + offsetof(struct tcb, tcb_tmbx);
	/*
	 * context is in userland, some architectures store
	 * single step status in registers, we should change
	 * these registers.
	 */
	ret = ps_pdread(ta->ph, tmbx_addr, &tmbx, sizeof(tmbx));
	if (ret == 0) {
		pt_ucontext_to_reg(&tmbx.tm_context, &regs);
		/* only write out if it is really changed. */
		if (pt_reg_sstep(&regs, step) != 0) {
			pt_reg_to_ucontext(&regs, &tmbx.tm_context);
			ret = ps_pdwrite(ta->ph, tmbx_addr, &tmbx,
			                 sizeof(tmbx));
		}
	}
	return (P2T(ret));
}

static long
_map_thread(pt_thragent_t *ta, psaddr_t pt, int type)
{
	struct pt_map *new;
	int i, first = -1;

	/* leave zero out */
	for (i = 1; i < ta->map_len; ++i) {
		if (ta->map[i].type == PT_NONE) {
			if (first == -1)
				first = i;
		} else if (ta->map[i].type == type && ta->map[i].thr == pt) {
				return (i);
		}
	}

	if (first == -1) {
		if (ta->map_len == 0) {
			ta->map = calloc(20, sizeof(struct pt_map));
			if (ta->map == NULL)
				return (-1);
			ta->map_len = 20;
			first = 1;
		} else {
			new = realloc(ta->map,
			              sizeof(struct pt_map) * ta->map_len * 2);
			if (new == NULL)
				return (-1);
			memset(new + ta->map_len, '\0', sizeof(struct pt_map) *
			       ta->map_len);
			first = ta->map_len;
			ta->map = new;
			ta->map_len *= 2;
		}
	}

	ta->map[first].type = type;
	ta->map[first].thr = pt;
	return (first);
}

static long
pt_map_thread(const pt_thragent_t *ta, psaddr_t pt)
{
	return _map_thread((pt_thragent_t *)ta, pt, PT_USER);
}

#if 0
static long
pt_map_lwp(const pt_thragent_t *ta, lwpid_t lwp)
{
	return _map_thread((pt_thragent_t *)ta, (psaddr_t)lwp, PT_LWP);
}
#endif

static void
pt_unmap_lwp(const pt_thragent_t *ta, lwpid_t lwp)
{
	int i;

	for (i = 0; i < ta->map_len; ++i) {
		if (ta->map[i].type == PT_LWP && ta->map[i].lwp == lwp) {
			ta->map[i].type = PT_NONE;
			return;
		}
	}
}

static int
pt_validate(const td_thrhandle_t *th)
{
	pt_thragent_t *ta = (pt_thragent_t *)th->th_ta_p;

	if (th->th_unique < 0 || th->th_unique >= ta->map_len ||
	    ta->map[th->th_unique].type == PT_NONE)
		return (TD_NOTHR);
	return (TD_OK);
}

struct ta_ops pthread_ops = {
	.to_init		= pt_init,
	.to_ta_new		= (void *)pt_ta_new,
	.to_ta_delete		= (void *)pt_ta_delete,
	.to_ta_get_nthreads	= (void *)pt_ta_get_nthreads,
	.to_ta_get_ph		= (void *)pt_ta_get_ph,
	.to_ta_map_id2thr	= (void *)pt_ta_map_id2thr,
	.to_ta_map_lwp2thr	= (void *)pt_ta_map_lwp2thr,
	.to_ta_thr_iter		= (void *)pt_ta_thr_iter,
	.to_ta_tsd_iter		= (void *)pt_ta_tsd_iter,
	.to_ta_event_addr	= (void *)pt_ta_event_addr,
	.to_ta_set_event	= (void *)pt_ta_set_event,
	.to_ta_clear_event	= (void *)pt_ta_clear_event,
	.to_ta_event_getmsg	= (void *)pt_ta_event_getmsg,
	.to_ta_setconcurrency	= (void *)pt_ta_setconcurrency,
	.to_ta_enable_stats	= (void *)pt_ta_enable_stats,
	.to_ta_reset_stats	= (void *)pt_ta_reset_stats,
	.to_ta_get_stats	= (void *)pt_ta_get_stats,
	.to_thr_validate	= pt_thr_validate,
	.to_thr_get_info	= pt_thr_get_info,
	.to_thr_getfpregs	= (void *)pt_thr_getfpregs,
	.to_thr_getgregs	= pt_thr_getgregs,
	.to_thr_getxregs	= pt_thr_getxregs,
	.to_thr_getxregsize	= pt_thr_getxregsize,
	.to_thr_setfpregs	= pt_thr_setfpregs,
	.to_thr_setgregs	= pt_thr_setgregs,
	.to_thr_setxregs	= pt_thr_setxregs,
	.to_thr_event_enable	= pt_thr_event_enable,
	.to_thr_set_event	= pt_thr_set_event,
	.to_thr_clear_event	= pt_thr_clear_event,
	.to_thr_event_getmsg	= pt_thr_event_getmsg,
	.to_thr_setprio		= pt_thr_setprio,
	.to_thr_setsigpending	= pt_thr_setsigpending,
	.to_thr_sigsetmask	= pt_thr_sigsetmask,
	.to_thr_tsd		= pt_thr_tsd,
	.to_thr_dbsuspend	= pt_thr_dbsuspend,
	.to_thr_dbresume	= pt_thr_dbresume,
	.to_ta_activated	= (void *)pt_ta_activated,
	.to_thr_sstep		= pt_thr_sstep
};
