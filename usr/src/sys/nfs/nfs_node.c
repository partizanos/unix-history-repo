/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)nfs_node.c	7.39 (Berkeley) %G%
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "mount.h"
#include "namei.h"
#include "vnode.h"
#include "kernel.h"
#include "malloc.h"

#include "rpcv2.h"
#include "nfsv2.h"
#include "nfs.h"
#include "nfsnode.h"
#include "nfsmount.h"
#include "nqnfs.h"

/* The request list head */
extern struct nfsreq nfsreqh;

#define	NFSNOHSZ	512
#if	((NFSNOHSZ&(NFSNOHSZ-1)) == 0)
#define	NFSNOHASH(fhsum)	((fhsum)&(NFSNOHSZ-1))
#else
#define	NFSNOHASH(fhsum)	(((unsigned)(fhsum))%NFSNOHSZ)
#endif

union nhead {
	union  nhead *nh_head[2];
	struct nfsnode *nh_chain[2];
} nhead[NFSNOHSZ];

#define TRUE	1
#define	FALSE	0

/*
 * Initialize hash links for nfsnodes
 * and build nfsnode free list.
 */
nfs_nhinit()
{
	register int i;
	register union  nhead *nh = nhead;

#ifndef lint
	if ((sizeof(struct nfsnode) - 1) & sizeof(struct nfsnode))
		printf("nfs_nhinit: bad size %d\n", sizeof(struct nfsnode));
#endif /* not lint */
	for (i = NFSNOHSZ; --i >= 0; nh++) {
		nh->nh_head[0] = nh;
		nh->nh_head[1] = nh;
	}
}

/*
 * Compute an entry in the NFS hash table structure
 */
union nhead *
nfs_hash(fhp)
	register nfsv2fh_t *fhp;
{
	register u_char *fhpp;
	register u_long fhsum;
	int i;

	fhpp = &fhp->fh_bytes[0];
	fhsum = 0;
	for (i = 0; i < NFSX_FH; i++)
		fhsum += *fhpp++;
	return (&nhead[NFSNOHASH(fhsum)]);
}

/*
 * Look up a vnode/nfsnode by file handle.
 * Callers must check for mount points!!
 * In all cases, a pointer to a
 * nfsnode structure is returned.
 */
nfs_nget(mntp, fhp, npp)
	struct mount *mntp;
	register nfsv2fh_t *fhp;
	struct nfsnode **npp;
{
	register struct nfsnode *np;
	register struct vnode *vp;
	extern int (**nfsv2_vnodeop_p)();
	struct vnode *nvp;
	union nhead *nh;
	int error;

	nh = nfs_hash(fhp);
loop:
	for (np = nh->nh_chain[0]; np != (struct nfsnode *)nh; np = np->n_forw) {
		if (mntp != NFSTOV(np)->v_mount ||
		    bcmp((caddr_t)fhp, (caddr_t)&np->n_fh, NFSX_FH))
			continue;
		vp = NFSTOV(np);
		if (vget(vp))
			goto loop;
		*npp = np;
		return(0);
	}
	if (error = getnewvnode(VT_NFS, mntp, nfsv2_vnodeop_p, &nvp)) {
		*npp = 0;
		return (error);
	}
	vp = nvp;
	MALLOC(np, struct nfsnode *, sizeof *np, M_NFSNODE, M_WAITOK);
	vp->v_data = np;
	np->n_vnode = vp;
	/*
	 * Insert the nfsnode in the hash queue for its new file handle
	 */
	np->n_flag = 0;
	insque(np, nh);
	bcopy((caddr_t)fhp, (caddr_t)&np->n_fh, NFSX_FH);
	np->n_attrstamp = 0;
	np->n_direofoffset = 0;
	np->n_sillyrename = (struct sillyrename *)0;
	np->n_size = 0;
	if (VFSTONFS(mntp)->nm_flag & NFSMNT_NQNFS) {
		ZEROQUAD(np->n_brev);
		ZEROQUAD(np->n_lrev);
		np->n_expiry = (time_t)0;
		np->n_tnext = (struct nfsnode *)0;
	} else
		np->n_mtime = 0;
	*npp = np;
	return (0);
}

nfs_inactive (ap)
	struct vop_inactive_args *ap;
{
	register struct nfsnode *np;
	register struct sillyrename *sp;
	extern int prtactive;

	np = VTONFS(ap->a_vp);
	if (prtactive && ap->a_vp->v_usecount != 0)
		vprint("nfs_inactive: pushing active", ap->a_vp);
	sp = np->n_sillyrename;
	np->n_sillyrename = (struct sillyrename *)0;
	if (sp) {
		/*
		 * Remove the silly file that was rename'd earlier
		 */
		nfs_removeit(sp, ap->a_p);
		crfree(sp->s_cred);
		vrele(sp->s_dvp);
#ifdef SILLYSEPARATE
		free((caddr_t)sp, M_NFSREQ);
#endif
	}
	np->n_flag &= NMODIFIED;
	return (0);
}

/*
 * Reclaim an nfsnode so that it can be used for other purposes.
 */
nfs_reclaim (ap)
	struct vop_reclaim_args *ap;
{
	register struct nfsnode *np = VTONFS(ap->a_vp);
	register struct nfsmount *nmp = VFSTONFS(ap->a_vp->v_mount);
	extern int prtactive;

	if (prtactive && ap->a_vp->v_usecount != 0)
		vprint("nfs_reclaim: pushing active", ap->a_vp);
	/*
	 * Remove the nfsnode from its hash chain.
	 */
	remque(np);

	/*
	 * For nqnfs, take it off the timer queue as required.
	 */
	if ((nmp->nm_flag & NFSMNT_NQNFS) && np->n_tnext) {
		if (np->n_tnext == (struct nfsnode *)nmp)
			nmp->nm_tprev = np->n_tprev;
		else
			np->n_tnext->n_tprev = np->n_tprev;
		if (np->n_tprev == (struct nfsnode *)nmp)
			nmp->nm_tnext = np->n_tnext;
		else
			np->n_tprev->n_tnext = np->n_tnext;
	}
	cache_purge(ap->a_vp);
	FREE(ap->a_vp->v_data, M_NFSNODE);
	ap->a_vp->v_data = (void *)0;
	return (0);
}

/*
 * Lock an nfsnode
 */
nfs_lock (ap)
	struct vop_lock_args *ap;
{

	return (0);
}

/*
 * Unlock an nfsnode
 */
nfs_unlock (ap)
	struct vop_unlock_args *ap;
{

	return (0);
}

/*
 * Check for a locked nfsnode
 */
nfs_islocked (ap)
	struct vop_islocked_args *ap;
{

	return (0);
}

/*
 * Nfs abort op, called after namei() when a CREATE/DELETE isn't actually
 * done. Currently nothing to do.
 */
/* ARGSUSED */
int
nfs_abortop (ap)
	struct vop_abortop_args *ap;
{

	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		FREE(ap->a_cnp->cn_pnbuf, M_NAMEI);
	return (0);
}
