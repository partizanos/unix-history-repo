/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)rk.c	7.4 (Berkeley) %G%
 */

/*
 * RK611/RK07
 */
#include "param.h"
#include "inode.h"
#include "fs.h"
#include "disklabel.h"

#include "../vax/pte.h"

#include "../vaxuba/ubareg.h"
#include "../vaxuba/rkreg.h"

#include "saio.h"
#include "savax.h"

#define	SECTSIZ 	512	/* sector size in bytes */

u_short	rkstd[] = { 0777440 };
struct	disklabel rklabel[MAXNUBA*8];
char	lbuf[SECTSIZ];

rkopen(io)
	register struct iob *io;
{
	register struct rkdevice *rkaddr = (struct rkdevice *)ubamem(io->i_unit, rkstd[0]);
	register struct disklabel *lp = &rklabel[io->i_unit];
	struct iob tio;

	if (badaddr((char *)rkaddr, sizeof(short))) {
		printf("nonexistent device");
		return (ENXIO);
	}
	rkaddr->rkcs2 = RKCS2_SCLR;
	rkwait(rkaddr);
	/*
	 * Read in the pack label.
	 */
	lp->d_nsectors = NRKSECT;
	lp->d_secpercyl = NRKTRK*NRKSECT;
	tio = *io;
	tio.i_bn = LABELSECTOR;
	tio.i_ma = lbuf;
	tio.i_cc = SECTSIZ;
	tio.i_flgs |= F_RDDATA;
	if (rkstrategy(&tio, READ) != SECTSIZ) {
		printf("can't read disk label");
		return (EIO);
	}
	*lp = *(struct disklabel *)(lbuf + LABELOFFSET);
	if (lp->d_magic != DISKMAGIC || lp->d_magic2 != DISKMAGIC) {
		printf("hk%d: unlabeled\n", io->i_unit);
#ifdef COMPAT_42
		rkmaptype(io, lp);
#else
		return (ENXIO);
#endif
	}
	if ((unsigned)io->i_boff >= lp->d_npartitions ||
	    (io->i_boff = lp->d_partitions[io->i_boff].p_offset) == -1) {
		printf("rk: bad partition");
		return (EUNIT);
	}
	return (0);
}

#ifdef COMPAT_42
u_long	rk_off[] = { 0, 241, 0, -1, -1, -1, 393, -1 };

rkmaptype(io, lp)
	struct iob *io;
	register struct disklabel *lp;
{
	register struct partition *pp;
	register i;
	register u_long *off = rk_off;

	lp->d_npartitions = 8;
	pp = lp->d_partitions;
	for (i = 0; i < 8; i++, pp++)
		pp->p_offset = *off++;
}
#endif

rkstrategy(io, func)
	register struct iob *io;
{
	register struct rkdevice *rkaddr = (struct rkdevice *)ubamem(io->i_unit, rkstd[0]);
	int com;
	register daddr_t bn;
	short cn, sn, tn;
	int ubinfo, errcnt = 0;

retry:
	ubinfo = ubasetup(io, 1);
	bn = io->i_bn;
	cn = bn / (NRKSECT*NRKTRK);
	sn = bn % NRKSECT;
	tn = (bn / NRKSECT) % NRKTRK;
	rkaddr->rkcs2 = UNITTODRIVE(io->i_unit);
	rkaddr->rkcs1 = RK_CDT|RK_PACK|RK_GO;
	rkwait(rkaddr);
	rkaddr->rkcs1 = RK_CDT|RK_DCLR|RK_GO;
	rkwait(rkaddr);
	rkaddr->rkda = sn | (tn << 8);
	rkaddr->rkcyl = cn;
	rkaddr->rkba = ubinfo;
	rkaddr->rkwc = -(io->i_cc >> 1);
	com = RK_CDT|((ubinfo>>8)&0x300)|RK_GO;
	if (func == READ)
		com |= RK_READ;
	else
		com |= RK_WRITE;
	rkaddr->rkcs1 = com;
	rkwait(rkaddr);
	while ((rkaddr->rkds & RKDS_SVAL) == 0)
		;
	ubafree(io, ubinfo);
	if (rkaddr->rkcs1 & RK_CERR) {
		printf("rk error: (cyl,trk,sec)=(%d,%d,%d) cs2=%b er=%b\n",
		    cn, tn, sn, rkaddr->rkcs2, RKCS2_BITS,
		    rkaddr->rker, RKER_BITS);
		rkaddr->rkcs1 = RK_CDT|RK_DCLR|RK_GO;
		rkwait(rkaddr);
		if (errcnt == 10) {
			printf("rk: unrecovered error\n");
			return (-1);
		}
		errcnt++;
		goto retry;
	}
	if (errcnt)
		printf("rk: recovered by retry\n");
	return (io->i_cc);
}

rkwait(rkaddr)
	register struct rkdevice *rkaddr;
{

	while ((rkaddr->rkcs1 & RK_CRDY) == 0)
		;
}

/*ARGSUSED*/
rkioctl(io, cmd, arg)
	struct iob *io;
	int cmd;
	caddr_t arg;
{

	return (ECMD);
}
