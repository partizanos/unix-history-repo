/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_hp300.c	8.1 (Berkeley) 6/4/93";
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * AMD64 machine dependent routines for kvm.  Hopefully, the forthcoming
 * vm code will one day obsolete this module.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <limits.h>

#include "kvm_private.h"

#ifndef btop
#define	btop(x)		(amd64_btop(x))
#define	ptob(x)		(amd64_ptob(x))
#endif

struct vmstate {
	pml4_entry_t	*PML4;
};

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != 0) {
		if (kd->vmst->PML4) {
			free(kd->vmst->PML4);
		}
		free(kd->vmst);
	}
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct vmstate *vm;
	struct nlist nlist[2];
	u_long pa;
	u_long kernbase;
	pml4_entry_t	*PML4;

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == 0) {
		_kvm_err(kd, kd->program, "cannot allocate vm");
		return (-1);
	}
	kd->vmst = vm;
	vm->PML4 = 0;

	nlist[0].n_name = "kernbase";
	nlist[1].n_name = 0;

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "bad namelist - no kernbase");
		return (-1);
	}
	kernbase = nlist[0].n_value;

	nlist[0].n_name = "KPML4phys";
	nlist[1].n_name = 0;

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "bad namelist - no KPML4phys");
		return (-1);
	}
	if (kvm_read(kd, (nlist[0].n_value - kernbase), &pa, sizeof(pa)) !=
	    sizeof(pa)) {
		_kvm_err(kd, kd->program, "cannot read KPML4phys");
		return (-1);
	}
	PML4 = _kvm_malloc(kd, PAGE_SIZE);
	if (kvm_read(kd, pa, PML4, PAGE_SIZE) != PAGE_SIZE) {
		_kvm_err(kd, kd->program, "cannot read KPML4phys");
		return (-1);
	}
	vm->PML4 = PML4;
	return (0);
}

static int
_kvm_vatop(kvm_t *kd, u_long va, u_long *pa)
{
	struct vmstate *vm;
	u_long offset;
	u_long pdpe_pa;
	u_long pde_pa;
	u_long pte_pa;
	pml4_entry_t pml4e;
	pdp_entry_t pdpe;
	pd_entry_t pde;
	pt_entry_t pte;
	u_long pml4eindex;
	u_long pdpeindex;
	u_long pdeindex;
	u_long pteindex;
	int i;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "kvm_vatop called in live kernel!");
		return((off_t)0);
	}

	vm = kd->vmst;
	offset = va & (PAGE_SIZE - 1);

	/*
	 * If we are initializing (kernel page table descriptor pointer
	 * not yet set) then return pa == va to avoid infinite recursion.
	 */
	if (vm->PML4 == 0) {
		*pa = va;
		return (PAGE_SIZE - offset);
	}

	pml4eindex = (va >> PML4SHIFT) & (NPML4EPG - 1);
	pml4e = vm->PML4[pml4eindex];
	if (((u_long)pml4e & PG_V) == 0)
		goto invalid;

	pdpeindex = (va >> PDPSHIFT) & (NPDPEPG-1);
	pdpe_pa = ((u_long)pml4e & PG_FRAME) + (pdpeindex * sizeof(pdp_entry_t));

	/* XXX This has to be a physical address read, kvm_read is virtual */
	if (lseek(kd->pmfd, pdpe_pa, 0) == -1) {
		_kvm_syserr(kd, kd->program, "_kvm_vatop: lseek pdpe_pa");
		goto invalid;
	}
	if (read(kd->pmfd, &pdpe, sizeof pdpe) != sizeof pdpe) {
		_kvm_syserr(kd, kd->program, "_kvm_vatop: read pdpe");
		goto invalid;
	}
	if (((u_long)pdpe & PG_V) == 0)
		goto invalid;


	pdeindex = (va >> PDRSHIFT) & (NPDEPG-1);
	pde_pa = ((u_long)pdpe & PG_FRAME) + (pdeindex * sizeof(pd_entry_t));

	/* XXX This has to be a physical address read, kvm_read is virtual */
	if (lseek(kd->pmfd, pde_pa, 0) == -1) {
		_kvm_syserr(kd, kd->program, "_kvm_vatop: lseek pde_pa");
		goto invalid;
	}
	if (read(kd->pmfd, &pde, sizeof pde) != sizeof pde) {
		_kvm_syserr(kd, kd->program, "_kvm_vatop: read pde");
		goto invalid;
	}
	if (((u_long)pde & PG_V) == 0)
		goto invalid;

	if ((u_long)pde & PG_PS) {
	      /*
	       * No final-level page table; ptd describes one 2MB page.
	       */
#define	PAGE2M_MASK	(NBPDR - 1)
#define	PG_FRAME2M	(~PAGE2M_MASK)
		*pa = ((u_long)pde & PG_FRAME2M) + (va & PAGE2M_MASK);
		return (NBPDR - (va & PAGE2M_MASK));
	}

	pteindex = (va >> PAGE_SHIFT) & (NPTEPG-1);
	pte_pa = ((u_long)pde & PG_FRAME) + (pteindex * sizeof(pt_entry_t));

	/* XXX This has to be a physical address read, kvm_read is virtual */
	if (lseek(kd->pmfd, pte_pa, 0) == -1) {
		_kvm_syserr(kd, kd->program, "_kvm_vatop: lseek");
		goto invalid;
	}
	if (read(kd->pmfd, &pte, sizeof pte) != sizeof pte) {
		_kvm_syserr(kd, kd->program, "_kvm_vatop: read");
		goto invalid;
	}
	if (((u_long)pte & PG_V) == 0)
		goto invalid;

	*pa = ((u_long)pte & PG_FRAME) + offset;
	return (PAGE_SIZE - offset);

invalid:
	_kvm_err(kd, 0, "invalid address (%x)", va);
	return (0);
}

int
_kvm_kvatop(kvm_t *kd, u_long va, u_long *pa)
{
	return (_kvm_vatop(kd, va, pa));
}
