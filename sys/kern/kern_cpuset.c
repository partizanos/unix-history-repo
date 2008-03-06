/*-
 * Copyright (c) 2008,  Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/refcount.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/syscallsubr.h>
#include <sys/cpuset.h>
#include <sys/sx.h>
#include <sys/refcount.h>
#include <sys/queue.h>
#include <sys/limits.h>

#include <vm/uma.h>

/*
 * cpusets provide a mechanism for creating and manipulating sets of
 * processors for the purpose of constraining the scheduling of threads to
 * specific processors.
 *
 * Each process belongs to an identified set, by default this is set 1.  Each
 * thread may further restrict the cpus it may run on to a subset of this
 * named set.  This creates an anonymous set which other threads and processes
 * may not join by number.
 *
 * The named set is referred to herein as the 'base' set to avoid ambiguity.
 * This set is usually a child of a 'root' set while the anonymous set may
 * simply be referred to as a mask.  In the syscall api these are referred to
 * as the ROOT, CPUSET, and MASK levels where CPUSET is called 'base' here.
 *
 * Threads inherit their set from their creator whether it be anonymous or
 * not.  This means that anonymous sets are immutable because they may be
 * shared.  To modify an anonymous set a new set is created with the desired
 * mask and the same parent as the existing anonymous set.  This gives the
 * illusion of each thread having a private mask.A
 *
 * Via the syscall apis a user may ask to retrieve or modify the root, base,
 * or mask that is discovered via a pid, tid, or setid.  Modifying a set
 * modifies all numbered and anonymous child sets to comply with the new mask.
 * Modifying a pid or tid's mask applies only to that tid but must still
 * exist within the assigned parent set.
 *
 * A thread may not be assigned to a a group seperate from other threads in
 * the process.  This is to remove ambiguity when the setid is queried with
 * a pid argument.  There is no other technical limitation.
 *
 * This somewhat complex arrangement is intended to make it easy for
 * applications to query available processors and bind their threads to
 * specific processors while also allowing administrators to dynamically
 * reprovision by changing sets which apply to groups of processes.
 *
 * A simple application should not concern itself with sets at all and
 * rather apply masks to its own threads via CPU_WHICH_TID and a -1 id
 * meaning 'curthread'.  It may query availble cpus for that tid with a
 * getaffinity call using (CPU_LEVEL_CPUSET, CPU_WHICH_PID, -1, ...).
 */
static uma_zone_t cpuset_zone;
static struct mtx cpuset_lock;
static struct setlist cpuset_ids;
struct cpuset *cpuset_zero;
static struct unrhdr *cpuset_unr;

/*
 * Acquire a reference to a cpuset, all pointers must be tracked with refs.
 */
struct cpuset *
cpuset_ref(struct cpuset *set)
{

	refcount_acquire(&set->cs_ref);
	return (set);
}

/*
 * Release a reference in a context where it is safe to allocte.
 */
void
cpuset_rel(struct cpuset *set)
{
	cpusetid_t id;

	if (refcount_release(&set->cs_ref) == 0)
		return;
	mtx_lock_spin(&cpuset_lock);
	LIST_REMOVE(set, cs_siblings);
	id = set->cs_id;
	if (id != CPUSET_INVALID)
		LIST_REMOVE(set, cs_link);
	mtx_unlock_spin(&cpuset_lock);
	cpuset_rel(set->cs_parent);
	uma_zfree(cpuset_zone, set);
	if (id != CPUSET_INVALID)
		free_unr(cpuset_unr, id);
}

/*
 * Deferred release must be used when in a context that is not safe to
 * allocate/free.  This places any unreferenced sets on the list 'head'.
 */
static void
cpuset_rel_defer(struct setlist *head, struct cpuset *set)
{

	if (refcount_release(&set->cs_ref) == 0)
		return;
	mtx_lock_spin(&cpuset_lock);
	LIST_REMOVE(set, cs_siblings);
	if (set->cs_id != CPUSET_INVALID)
		LIST_REMOVE(set, cs_link);
	LIST_INSERT_HEAD(head, set, cs_link);
	mtx_unlock_spin(&cpuset_lock);
}

/*
 * Complete a deferred release.  Removes the set from the list provided to
 * cpuset_rel_defer.
 */
static void
cpuset_rel_complete(struct cpuset *set)
{
	LIST_REMOVE(set, cs_link);
	cpuset_rel(set->cs_parent);
	uma_zfree(cpuset_zone, set);
}

/*
 * Find a set based on an id.  Returns it with a ref.
 */
static struct cpuset *
cpuset_lookup(cpusetid_t setid)
{
	struct cpuset *set;

	if (setid == CPUSET_INVALID)
		return (NULL);
	mtx_lock_spin(&cpuset_lock);
	LIST_FOREACH(set, &cpuset_ids, cs_link)
		if (set->cs_id == setid)
			break;
	if (set)
		cpuset_ref(set);
	mtx_unlock_spin(&cpuset_lock);
	return (set);
}

/*
 * Create a set in the space provided in 'set' with the provided parameters.
 * The set is returned with a single ref.  May return EDEADLK if the set
 * will have no valid cpu based on restrictions from the parent.
 */
static int
_cpuset_create(struct cpuset *set, struct cpuset *parent, cpuset_t *mask,
    cpusetid_t id)
{

	if (!CPU_OVERLAP(&parent->cs_mask, mask))
		return (EDEADLK);
	CPU_COPY(mask, &set->cs_mask);
	LIST_INIT(&set->cs_children);
	refcount_init(&set->cs_ref, 1);
	set->cs_flags = 0;
	mtx_lock_spin(&cpuset_lock);
	CPU_AND(mask, &parent->cs_mask);
	set->cs_id = id;
	set->cs_parent = cpuset_ref(parent);
	LIST_INSERT_HEAD(&parent->cs_children, set, cs_siblings);
	if (set->cs_id != CPUSET_INVALID)
		LIST_INSERT_HEAD(&cpuset_ids, set, cs_link);
	mtx_unlock_spin(&cpuset_lock);

	return (0);
}

/*
 * Create a new non-anonymous set with the requested parent and mask.  May
 * return failures if the mask is invalid or a new number can not be
 * allocated.
 */
static int
cpuset_create(struct cpuset **setp, struct cpuset *parent, cpuset_t *mask)
{
	struct cpuset *set;
	cpusetid_t id;
	int error;

	id = alloc_unr(cpuset_unr);
	if (id == -1)
		return (ENFILE);
	*setp = set = uma_zalloc(cpuset_zone, M_WAITOK);
	error = _cpuset_create(set, parent, mask, id);
	if (error == 0)
		return (0);
	free_unr(cpuset_unr, id);
	uma_zfree(cpuset_zone, set);

	return (error);
}

/*
 * Recursively check for errors that would occur from applying mask to
 * the tree of sets starting at 'set'.  Checks for sets that would become
 * empty as well as RDONLY flags.
 */
static int
cpuset_testupdate(struct cpuset *set, cpuset_t *mask)
{
	struct cpuset *nset;
	cpuset_t newmask;
	int error;

	mtx_assert(&cpuset_lock, MA_OWNED);
	if (set->cs_flags & CPU_SET_RDONLY)
		return (EPERM);
	if (!CPU_OVERLAP(&set->cs_mask, mask))
		return (EDEADLK);
	CPU_COPY(&set->cs_mask, &newmask);
	CPU_AND(&newmask, mask);
	error = 0;
	LIST_FOREACH(nset, &set->cs_children, cs_siblings) 
		if ((error = cpuset_testupdate(nset, &newmask)) != 0)
			break;
	return (error);
}

/*
 * Applies the mask 'mask' without checking for empty sets or permissions.
 */
static void
cpuset_update(struct cpuset *set, cpuset_t *mask)
{
	struct cpuset *nset;

	mtx_assert(&cpuset_lock, MA_OWNED);
	CPU_AND(&set->cs_mask, mask);
	LIST_FOREACH(nset, &set->cs_children, cs_siblings) 
		cpuset_update(nset, &set->cs_mask);

	return;
}

/*
 * Modify the set 'set' to use a copy of the mask provided.  Apply this new
 * mask to restrict all children in the tree.  Checks for validity before
 * applying the changes.
 */
static int
cpuset_modify(struct cpuset *set, cpuset_t *mask)
{
	struct cpuset *root;
	int error;

	error = suser(curthread);
	if (error)
		return (error);
	/*
	 * Verify that we have access to this set of
	 * cpus.
	 */
	root = set->cs_parent;
	if (root && !CPU_SUBSET(&root->cs_mask, mask))
		return (EINVAL);
	mtx_lock_spin(&cpuset_lock);
	error = cpuset_testupdate(set, mask);
	if (error)
		goto out;
	cpuset_update(set, mask);
	CPU_COPY(mask, &set->cs_mask);
out:
	mtx_unlock_spin(&cpuset_lock);

	return (error);
}

/*
 * Walks up the tree from 'set' to find the root.  Returns the root
 * referenced.
 */
static struct cpuset *
cpuset_root(struct cpuset *set)
{

	for (; set->cs_parent != NULL; set = set->cs_parent)
		if (set->cs_flags & CPU_SET_ROOT)
			break;
	cpuset_ref(set);

	return (set);
}

/*
 * Find the first non-anonymous set starting from 'set'.  Returns this set
 * referenced.  May return the passed in set with an extra ref if it is
 * not anonymous. 
 */
static struct cpuset *
cpuset_base(struct cpuset *set)
{

	if (set->cs_id == CPUSET_INVALID)
		set = set->cs_parent;
	cpuset_ref(set);

	return (set);
}

/*
 * Resolve the 'which' parameter of several cpuset apis.
 *
 * For WHICH_PID and WHICH_TID return a locked proc and valid proc/tid.  Also
 * checks for permission via p_cansched().
 *
 * For WHICH_SET returns a valid set with a new reference.
 *
 * -1 may be supplied for any argument to mean the current proc/thread or
 * the base set of the current thread.  May fail with ESRCH/EPERM.
 */
static int
cpuset_which(cpuwhich_t which, id_t id, struct proc **pp, struct thread **tdp,
    struct cpuset **setp)
{
	struct cpuset *set;
	struct thread *td;
	struct proc *p;
	int error;

	*pp = p = NULL;
	*tdp = td = NULL;
	*setp = set = NULL;
	switch (which) {
	case CPU_WHICH_PID:
		if (id == -1) {
			PROC_LOCK(curproc);
			p = curproc;
			break;
		}
		if ((p = pfind(id)) == NULL)
			return (ESRCH);
		break;
	case CPU_WHICH_TID:
		if (id == -1) {
			PROC_LOCK(curproc);
			p = curproc;
			td = curthread;
			break;
		}
		sx_slock(&allproc_lock);
		FOREACH_PROC_IN_SYSTEM(p) {
			PROC_LOCK(p);
			PROC_SLOCK(p);
			FOREACH_THREAD_IN_PROC(p, td)
				if (td->td_tid == id)
					break;
			PROC_SUNLOCK(p);
			if (td != NULL)
				break;
			PROC_UNLOCK(p);
		}
		sx_sunlock(&allproc_lock);
		if (td == NULL)
			return (ESRCH);
		break;
	case CPU_WHICH_CPUSET:
		if (id == -1) {
			thread_lock(curthread);
			set = cpuset_base(curthread->td_cpuset);
			thread_unlock(curthread);
		} else
			set = cpuset_lookup(id);
		if (set) {
			*setp = set;
			return (0);
		}
		return (ESRCH);
	default:
		return (EINVAL);
	}
	error = p_cansched(curthread, p);
	if (error) {
		PROC_UNLOCK(p);
		return (error);
	}
	if (td == NULL)
		td = FIRST_THREAD_IN_PROC(p);
	*pp = p;
	*tdp = td;
	return (0);
}

/*
 * Create an anonymous set with the provided mask in the space provided by
 * 'fset'.  If the passed in set is anonymous we use its parent otherwise
 * the new set is a child of 'set'.
 */
static int
cpuset_shadow(struct cpuset *set, struct cpuset *fset, cpuset_t *mask)
{
	struct cpuset *parent;

	if (set->cs_id == CPUSET_INVALID)
		parent = set->cs_parent;
	else
		parent = set;
	if (!CPU_SUBSET(&parent->cs_mask, mask))
		return (EINVAL);
	return (_cpuset_create(fset, parent, mask, CPUSET_INVALID));
}

/*
 * Handle two cases for replacing the base set or mask of an entire process.
 *
 * 1) Set is non-null and mask is null.  This reparents all anonymous sets
 *    to the provided set and replaces all non-anonymous td_cpusets with the
 *    provided set.
 * 2) Mask is non-null and set is null.  This replaces or creates anonymous
 *    sets for every thread with the existing base as a parent.
 *
 * This is overly complicated because we can't allocate while holding a 
 * spinlock and spinlocks must be held while changing and examining thread
 * state.
 */
static int
cpuset_setproc(pid_t pid, struct cpuset *set, cpuset_t *mask)
{
	struct setlist freelist;
	struct setlist droplist;
	struct cpuset *tdset;
	struct cpuset *nset;
	struct thread *td;
	struct proc *p;
	int threads;
	int nfree;
	int error;
	/*
	 * The algorithm requires two passes due to locking considerations.
	 * 
	 * 1) Lookup the process and acquire the locks in the required order.
	 * 2) If enough cpusets have not been allocated release the locks and
	 *    allocate them.  Loop.
	 */
	LIST_INIT(&freelist);
	LIST_INIT(&droplist);
	nfree = 0;
	for (;;) {
		error = cpuset_which(CPU_WHICH_PID, pid, &p, &td, &nset);
		if (error)
			goto out;
		PROC_SLOCK(p);
		if (nfree >= p->p_numthreads)
			break;
		threads = p->p_numthreads;
		PROC_SUNLOCK(p);
		PROC_UNLOCK(p);
		for (; nfree < threads; nfree++) {
			nset = uma_zalloc(cpuset_zone, M_WAITOK);
			LIST_INSERT_HEAD(&freelist, nset, cs_link);
		}
	}
	PROC_LOCK_ASSERT(p, MA_OWNED);
	PROC_SLOCK_ASSERT(p, MA_OWNED);
	/*
	 * Now that the appropriate locks are held and we have enough cpusets,
	 * make sure the operation will succeed before applying changes.  The
	 * proc lock prevents td_cpuset from changing between calls.
	 */
	error = 0;
	FOREACH_THREAD_IN_PROC(p, td) {
		thread_lock(td);
		tdset = td->td_cpuset;
		/*
		 * Verify that a new mask doesn't specify cpus outside of
		 * the set the thread is a member of.
		 */
		if (mask) {
			if (tdset->cs_id == CPUSET_INVALID)
				tdset = tdset->cs_parent;
			if (!CPU_SUBSET(&tdset->cs_mask, mask))
				error = EINVAL;
		/*
		 * Verify that a new set won't leave an existing thread
		 * mask without a cpu to run on.  It can, however, restrict
		 * the set.
		 */
		} else if (tdset->cs_id == CPUSET_INVALID) {
			if (!CPU_OVERLAP(&set->cs_mask, &tdset->cs_mask))
				error = EINVAL;
		}
		thread_unlock(td);
		if (error)
			goto unlock_out;
	}
	/*
	 * Replace each thread's cpuset while using deferred release.  We
	 * must do this because the PROC_SLOCK has to be held while traversing
	 * the thread list and this limits the type of operations allowed.
	 */
	FOREACH_THREAD_IN_PROC(p, td) {
		thread_lock(td);
		/*
		 * If we presently have an anonymous set or are applying a
		 * mask we must create an anonymous shadow set.  That is
		 * either parented to our existing base or the supplied set.
		 *
		 * If we have a base set with no anonymous shadow we simply
		 * replace it outright.
		 */
		tdset = td->td_cpuset;
		if (tdset->cs_id == CPUSET_INVALID || mask) {
			nset = LIST_FIRST(&freelist);
			LIST_REMOVE(nset, cs_link);
			if (mask)
				error = cpuset_shadow(tdset, nset, mask);
			else
				error = _cpuset_create(nset, set,
				    &tdset->cs_mask, CPUSET_INVALID);
			if (error) {
				LIST_INSERT_HEAD(&freelist, nset, cs_link);
				thread_unlock(td);
				break;
			}
		} else
			nset = cpuset_ref(set);
		cpuset_rel_defer(&droplist, tdset);
		td->td_cpuset = nset;
		sched_affinity(td);
		thread_unlock(td);
	}
unlock_out:
	PROC_SUNLOCK(p);
	PROC_UNLOCK(p);
out:
	while ((nset = LIST_FIRST(&droplist)) != NULL)
		cpuset_rel_complete(nset);
	while ((nset = LIST_FIRST(&freelist)) != NULL) {
		LIST_REMOVE(nset, cs_link);
		uma_zfree(cpuset_zone, nset);
	}
	return (error);
}

/*
 * Apply an anonymous mask to a single thread.
 */
static int
cpuset_setthread(lwpid_t id, cpuset_t *mask)
{
	struct cpuset *nset;
	struct cpuset *set;
	struct thread *td;
	struct proc *p;
	int error;

	nset = uma_zalloc(cpuset_zone, M_WAITOK);
	error = cpuset_which(CPU_WHICH_TID, id, &p, &td, &set);
	if (error)
		goto out;
	thread_lock(td);
	set = td->td_cpuset;
	error = cpuset_shadow(set, nset, mask);
	if (error == 0) {
		cpuset_rel(td->td_cpuset);
		td->td_cpuset = nset;
		sched_affinity(td);
		nset = NULL;
	}
	thread_unlock(td);
	PROC_UNLOCK(p);
out:
	if (nset)
		uma_zfree(cpuset_zone, nset);
	return (error);
}

/*
 * Creates the cpuset for thread0.  We make two sets:
 * 
 * 0 - The root set which should represent all valid processors in the
 *     system.  It is initially created with a mask of all processors
 *     because we don't know what processors are valid until cpuset_init()
 *     runs.  This set is immutable.
 * 1 - The default set which all processes are a member of until changed.
 *     This allows an administrator to move all threads off of given cpus to
 *     dedicate them to high priority tasks or save power etc.
 */
struct cpuset *
cpuset_thread0(void)
{
	struct cpuset *set;
	int error;

	cpuset_zone = uma_zcreate("cpuset", sizeof(struct cpuset), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, 0);
	mtx_init(&cpuset_lock, "cpuset", NULL, MTX_SPIN | MTX_RECURSE);
	/*
	 * Create the root system set for the whole machine.  Doesn't use
	 * cpuset_create() due to NULL parent.
	 */
	set = uma_zalloc(cpuset_zone, M_WAITOK | M_ZERO);
	set->cs_mask.__bits[0] = -1;
	LIST_INIT(&set->cs_children);
	LIST_INSERT_HEAD(&cpuset_ids, set, cs_link);
	set->cs_ref = 1;
	set->cs_flags = CPU_SET_ROOT;
	cpuset_zero = set;
	/*
	 * Now derive a default, modifiable set from that to give out.
	 */
	set = uma_zalloc(cpuset_zone, M_WAITOK);
	error = _cpuset_create(set, cpuset_zero, &cpuset_zero->cs_mask, 1);
	KASSERT(error == 0, ("Error creating default set: %d\n", error));
	/*
	 * Initialize the unit allocator. 0 and 1 are allocated above.
	 */
	cpuset_unr = new_unrhdr(2, INT_MAX, NULL);

	return (set);
}

/*
 * This is called once the final set of system cpus is known.  Modifies
 * the root set and all children and mark the root readonly.  
 */
static void
cpuset_init(void *arg)
{
	cpuset_t mask;

	CPU_ZERO(&mask);
#ifdef SMP
	mask.__bits[0] = all_cpus;
#else
	mask.__bits[0] = 1;
#endif
	if (cpuset_modify(cpuset_zero, &mask))
		panic("Can't set initial cpuset mask.\n");
	cpuset_zero->cs_flags |= CPU_SET_RDONLY;
}
SYSINIT(cpuset, SI_SUB_SMP, SI_ORDER_ANY, cpuset_init, NULL);

#ifndef _SYS_SYSPROTO_H_
struct cpuset_args {
	cpusetid_t	*setid;
};
#endif
int
cpuset(struct thread *td, struct cpuset_args *uap)
{
	struct cpuset *root;
	struct cpuset *set;
	int error;

	thread_lock(td);
	root = cpuset_root(td->td_cpuset);
	thread_unlock(td);
	error = cpuset_create(&set, root, &root->cs_mask);
	cpuset_rel(root);
	if (error)
		return (error);
	error = cpuset_setproc(-1, set, NULL);
	if (error == 0)
		error = copyout(&set->cs_id, uap->setid, sizeof(set->cs_id));
	cpuset_rel(set);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct cpuset_setid_args {
	cpuwhich_t	which;
	id_t		id;
	cpusetid_t	setid;
};
#endif
int
cpuset_setid(struct thread *td, struct cpuset_setid_args *uap)
{
	struct cpuset *set;
	int error;

	/*
	 * Presently we only support per-process sets.
	 */
	if (uap->which != CPU_WHICH_PID)
		return (EINVAL);
	set = cpuset_lookup(uap->setid);
	if (set == NULL)
		return (ESRCH);
	error = cpuset_setproc(uap->id, set, NULL);
	cpuset_rel(set);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct cpuset_getid_args {
	cpulevel_t	level;
	cpuwhich_t	which;
	id_t		id;
	cpusetid_t	*setid;
#endif
int
cpuset_getid(struct thread *td, struct cpuset_getid_args *uap)
{
	struct cpuset *nset;
	struct cpuset *set;
	struct thread *ttd;
	struct proc *p;
	cpusetid_t id;
	int error;

	if (uap->level == CPU_LEVEL_WHICH && uap->which != CPU_WHICH_CPUSET)
		return (EINVAL);
	error = cpuset_which(uap->which, uap->id, &p, &ttd, &set);
	if (error)
		return (error);
	switch (uap->which) {
	case CPU_WHICH_TID:
	case CPU_WHICH_PID:
		thread_lock(ttd);
		set = cpuset_base(ttd->td_cpuset);
		thread_unlock(ttd);
		PROC_UNLOCK(p);
		break;
	case CPU_WHICH_CPUSET:
		break;
	}
	switch (uap->level) {
	case CPU_LEVEL_ROOT:
		nset = cpuset_root(set);
		cpuset_rel(set);
		set = nset;
		break;
	case CPU_LEVEL_CPUSET:
		break;
	case CPU_LEVEL_WHICH:
		break;
	}
	id = set->cs_id;
	cpuset_rel(set);
	if (error == 0)
		error = copyout(&id, uap->setid, sizeof(id));

	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct cpuset_getaffinity_args {
        cpulevel_t	level;
        cpuwhich_t	which;
        int		id;
        int		cpusetsize;
        long 		*mask;
};
#endif
int
cpuset_getaffinity(struct thread *td, struct cpuset_getaffinity_args *uap)
{
	struct thread *ttd;
	struct cpuset *nset;
	struct cpuset *set;
	struct proc *p;
	cpuset_t *mask;
	int error;
	int size;

	if (uap->cpusetsize < sizeof(cpuset_t) ||
	    uap->cpusetsize * NBBY > CPU_MAXSIZE)
		return (ERANGE);
	size = uap->cpusetsize;
	mask = malloc(size, M_TEMP, M_WAITOK | M_ZERO);
	error = cpuset_which(uap->which, uap->id, &p, &ttd, &set);
	if (error)
		goto out;
	error = 0;
	switch (uap->level) {
	case CPU_LEVEL_ROOT:
	case CPU_LEVEL_CPUSET:
		switch (uap->which) {
		case CPU_WHICH_TID:
		case CPU_WHICH_PID:
			thread_lock(ttd);
			set = cpuset_ref(ttd->td_cpuset);
			thread_unlock(ttd);
			break;
		case CPU_WHICH_CPUSET:
			break;
		}
		if (uap->level == CPU_LEVEL_ROOT)
			nset = cpuset_root(set);
		else
			nset = cpuset_base(set);
		CPU_COPY(&nset->cs_mask, mask);
		cpuset_rel(nset);
		break;
	case CPU_LEVEL_WHICH:
		switch (uap->which) {
		case CPU_WHICH_TID:
			thread_lock(ttd);
			CPU_COPY(&ttd->td_cpuset->cs_mask, mask);
			thread_unlock(ttd);
			break;
		case CPU_WHICH_PID:
			PROC_SLOCK(p);
			FOREACH_THREAD_IN_PROC(p, ttd) {
				thread_lock(ttd);
				CPU_OR(mask, &ttd->td_cpuset->cs_mask);
				thread_unlock(ttd);
			}
			PROC_SUNLOCK(p);
			break;
		case CPU_WHICH_CPUSET:
			CPU_COPY(&set->cs_mask, mask);
			break;
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	if (set)
		cpuset_rel(set);
	if (p)
		PROC_UNLOCK(p);
	if (error == 0)
		error = copyout(mask, uap->mask, size);
out:
	free(mask, M_TEMP);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct cpuset_setaffinity_args {
	cpulevel_t	level;
        cpuwhich_t	which;
        int		id;
        int		cpusetsize;
        long 	*	mask;
};
#endif
int
cpuset_setaffinity(struct thread *td, struct cpuset_setaffinity_args *uap)
{
	struct cpuset *nset;
	struct cpuset *set;
	struct thread *ttd;
	struct proc *p;
	cpuset_t *mask;
	int error;

	if (uap->cpusetsize < sizeof(cpuset_t) ||
	    uap->cpusetsize * NBBY > CPU_MAXSIZE)
		return (ERANGE);
	mask = malloc(uap->cpusetsize, M_TEMP, M_WAITOK | M_ZERO);
	error = copyin(uap->mask, mask, uap->cpusetsize);
	if (error)
		goto out;
	/*
	 * Verify that no high bits are set.
	 */
	if (uap->cpusetsize > sizeof(cpuset_t)) {
		char *end;
		char *cp;

		end = cp = (char *)&mask->__bits;
		end += uap->cpusetsize;
		cp += sizeof(cpuset_t);
		while (cp != end)
			if (*cp++ != 0) {
				error = EINVAL;
				goto out;
			}

	}
	switch (uap->level) {
	case CPU_LEVEL_ROOT:
	case CPU_LEVEL_CPUSET:
		error = cpuset_which(uap->which, uap->id, &p, &ttd, &set);
		if (error)
			break;
		switch (uap->which) {
		case CPU_WHICH_TID:
		case CPU_WHICH_PID:
			thread_lock(ttd);
			set = cpuset_ref(ttd->td_cpuset);
			thread_unlock(ttd);
			PROC_UNLOCK(p);
			break;
		case CPU_WHICH_CPUSET:
			break;
		}
		if (uap->level == CPU_LEVEL_ROOT)
			nset = cpuset_root(set);
		else
			nset = cpuset_base(set);
		error = cpuset_modify(nset, mask);
		cpuset_rel(nset);
		cpuset_rel(set);
		break;
	case CPU_LEVEL_WHICH:
		switch (uap->which) {
		case CPU_WHICH_TID:
			error = cpuset_setthread(uap->id, mask);
			break;
		case CPU_WHICH_PID:
			error = cpuset_setproc(uap->id, NULL, mask);
			break;
		case CPU_WHICH_CPUSET:
			error = cpuset_which(CPU_WHICH_CPUSET, uap->id, &p,
			    &ttd, &set);
			if (error == 0) {
				error = cpuset_modify(set, mask);
				cpuset_rel(set);
			}
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	default:
		error = EINVAL;
		break;
	}
out:
	free(mask, M_TEMP);
	return (error);
}
