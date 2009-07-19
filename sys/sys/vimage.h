/*-
 * Copyright (c) 2006-2009 University of Zagreb
 * Copyright (c) 2006-2009 FreeBSD Foundation
 *
 * This software was developed by the University of Zagreb and the
 * FreeBSD Foundation under sponsorship by the Stichting NLnet and the
 * FreeBSD Foundation.
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
 *
 * $FreeBSD$
 */

#ifndef	_SYS_VIMAGE_H_
#define	_SYS_VIMAGE_H_

#include <sys/proc.h>
#include <sys/queue.h>

#ifdef _KERNEL

#include <sys/lock.h>
#include <sys/sx.h>

#ifdef INVARIANTS
#define	VNET_DEBUG
#endif

struct vnet;
struct ifnet;

typedef int vnet_attach_fn(const void *);
typedef int vnet_detach_fn(const void *);

#ifdef VIMAGE

struct vnet_modinfo {
	u_int				 vmi_id;
	u_int				 vmi_dependson;
	char				*vmi_name;
	vnet_attach_fn			*vmi_iattach;
	vnet_detach_fn			*vmi_idetach;
};
typedef struct vnet_modinfo vnet_modinfo_t;

struct vnet_modlink {
	TAILQ_ENTRY(vnet_modlink)	 vml_mod_le;
	const struct vnet_modinfo	*vml_modinfo;
	const void			*vml_iarg;
	const char			*vml_iname;
};

/* Stateful modules. */
#define	VNET_MOD_NET		 0	/* MUST be 0 - implicit dependency */
#define	VNET_MOD_NETGRAPH	 1
#define	VNET_MOD_INET		 2
#define	VNET_MOD_INET6		 3
#define	VNET_MOD_IPSEC		 4
#define	VNET_MOD_IPFW		 5
#define	VNET_MOD_DUMMYNET	 6
#define	VNET_MOD_PF		 7
#define	VNET_MOD_ALTQ		 8
#define	VNET_MOD_IPX		 9
#define	VNET_MOD_ATALK		10
#define	VNET_MOD_ACCF_HTTP	11
#define	VNET_MOD_IGMP		12
#define	VNET_MOD_MLD		13
#define	VNET_MOD_RTABLE		14

/* Stateless modules. */
#define	VNET_MOD_IF_CLONE	19
#define	VNET_MOD_NG_ETHER	20
#define	VNET_MOD_NG_IFACE	21
#define	VNET_MOD_NG_EIFACE	22
#define	VNET_MOD_ESP		23
#define	VNET_MOD_IPIP		24
#define	VNET_MOD_AH		25
#define	VNET_MOD_IPCOMP	 	26	
#define	VNET_MOD_GIF		27
	/*	 		28 */
#define	VNET_MOD_FLOWTABLE	29
#define	VNET_MOD_LOIF		30
#define	VNET_MOD_DOMAIN		31
#define	VNET_MOD_DYNAMIC_START	32
#define	VNET_MOD_MAX		64

int	vi_if_move(struct thread *, struct ifnet *, char *, int);
void	vnet_mod_register(const struct vnet_modinfo *);
void	vnet_mod_register_multi(const struct vnet_modinfo *, void *, char *);
void	vnet_mod_deregister(const struct vnet_modinfo *);
void	vnet_mod_deregister_multi(const struct vnet_modinfo *, void *, char *);
struct vnet *vnet_alloc(void);
void	vnet_destroy(struct vnet *);
void	vnet_foreach(void (*vnet_foreach_fn)(struct vnet *, void *),
	    void *arg);

#endif /* VIMAGE */

struct vnet {
	LIST_ENTRY(vnet)	 vnet_le;	/* all vnets list */
	u_int			 vnet_magic_n;
	u_int			 ifcnt;
	u_int			 sockcnt;
	void			*vnet_data_mem;
	uintptr_t		 vnet_data_base;
};

#define	curvnet curthread->td_vnet

#define	VNET_MAGIC_N 0x3e0d8f29

#ifdef VIMAGE
#ifdef VNET_DEBUG
#define	VNET_ASSERT(condition)						\
	if (!(condition)) {						\
		printf("VNET_ASSERT @ %s:%d %s():\n",			\
			__FILE__, __LINE__, __FUNCTION__);		\
		panic(#condition);					\
	}

#define	CURVNET_SET_QUIET(arg)						\
	VNET_ASSERT((arg)->vnet_magic_n == VNET_MAGIC_N);		\
	struct vnet *saved_vnet = curvnet;				\
	const char *saved_vnet_lpush = curthread->td_vnet_lpush;	\
	curvnet = arg;							\
	curthread->td_vnet_lpush = __FUNCTION__;
 
#define	CURVNET_SET_VERBOSE(arg)					\
	CURVNET_SET_QUIET(arg)						\
	if (saved_vnet)							\
		printf("CURVNET_SET(%p) in %s() on cpu %d, prev %p in %s()\n", \
		       curvnet,	curthread->td_vnet_lpush, curcpu,	\
		       saved_vnet, saved_vnet_lpush);

#define	CURVNET_SET(arg)	CURVNET_SET_VERBOSE(arg)
 
#define	CURVNET_RESTORE()						\
	VNET_ASSERT(saved_vnet == NULL ||				\
		    saved_vnet->vnet_magic_n == VNET_MAGIC_N);		\
	curvnet = saved_vnet;						\
	curthread->td_vnet_lpush = saved_vnet_lpush;
#else /* !VNET_DEBUG */
#define	VNET_ASSERT(condition)

#define	CURVNET_SET(arg)						\
	struct vnet *saved_vnet = curvnet;				\
	curvnet = arg;	
 
#define	CURVNET_SET_VERBOSE(arg)	CURVNET_SET(arg)
#define	CURVNET_SET_QUIET(arg)		CURVNET_SET(arg)
 
#define	CURVNET_RESTORE()						\
	curvnet = saved_vnet;
#endif /* VNET_DEBUG */
#else /* !VIMAGE */
#define	VNET_ASSERT(condition)
#define	CURVNET_SET(arg)
#define	CURVNET_SET_QUIET(arg)
#define	CURVNET_RESTORE()
#endif /* !VIMAGE */

#ifdef VIMAGE
/*
 * Global linked list of all virtual network stacks, along with read locks to
 * access it.  If a caller may sleep while accessing the list, it must use
 * the sleepable lock macros.
 */
LIST_HEAD(vnet_list_head, vnet);
extern struct vnet_list_head vnet_head;
extern struct rwlock vnet_rwlock;
extern struct sx vnet_sxlock;

#define	VNET_LIST_RLOCK()		sx_slock(&vnet_sxlock)
#define	VNET_LIST_RLOCK_NOSLEEP()	rw_rlock(&vnet_rwlock)
#define	VNET_LIST_RUNLOCK()		sx_sunlock(&vnet_sxlock)
#define	VNET_LIST_RUNLOCK_NOSLEEP()	rw_runlock(&vnet_rwlock)

/*
 * Iteration macros to walk the global list of virtual network stacks.
 */
#define	VNET_ITERATOR_DECL(arg) struct vnet *arg
#define	VNET_FOREACH(arg)	LIST_FOREACH((arg), &vnet_head, vnet_le)

#else /* !VIMAGE */
/*
 * No-op macros for the !VIMAGE case.
 */
#define	VNET_LIST_RLOCK()
#define	VNET_LIST_RLOCK_NOSLEEP()
#define	VNET_LIST_RUNLOCK()
#define	VNET_LIST_RUNLOCK_NOSLEEP()
#define	VNET_ITERATOR_DECL(arg)
#define	VNET_FOREACH(arg)

#endif /* VIMAGE */

#ifdef VIMAGE
extern struct vnet *vnet0;
#define	IS_DEFAULT_VNET(arg)	((arg) == vnet0)
#else
#define	IS_DEFAULT_VNET(arg)	1
#endif

#ifdef VIMAGE
#define	CRED_TO_VNET(cr)	(cr)->cr_prison->pr_vnet
#define	TD_TO_VNET(td)		CRED_TO_VNET((td)->td_ucred)
#define	P_TO_VNET(p)		CRED_TO_VNET((p)->p_ucred)
#else /* !VIMAGE */
#define	CRED_TO_VNET(cr)	NULL
#define	TD_TO_VNET(td)		NULL
#define	P_TO_VNET(p)		NULL
#endif /* VIMAGE */

#endif /* _KERNEL */

#endif /* !_SYS_VIMAGE_H_ */
