/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 *	@(#)config.h	8.1 (Berkeley) 6/6/93
 */

/*
 * Config.
 */
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#define	NODEV	((dev_t)-1)

struct file_list {
	struct	file_list *f_next;
	char	*f_fn;			/* the name */
	int     f_type;                 /* type or count */
	u_char	f_flags;		/* see below */
	char	*f_special;		/* special make rule if present */
	char	*f_depends;		/* additional dependancies */
	char	*f_clean;		/* File list to add to clean rule */
	char	*f_needs;
	/*
	 * Random values:
	 *	swap space parameters for swap areas
	 *	root device, etc. for system specifications
	 */
	union {
		struct {		/* when swap specification */
			dev_t	fuw_swapdev;
			int	fuw_swapsize;
			int	fuw_swapflag;
		} fuw;
		struct {		/* when system specification */
			dev_t	fus_rootdev;
			dev_t	fus_dumpdev;
		} fus;
		struct {		/* when component dev specification */
			dev_t	fup_compdev;
			int	fup_compinfo;
		} fup;
	} fun;
#define	f_swapdev	fun.fuw.fuw_swapdev
#define	f_swapsize	fun.fuw.fuw_swapsize
#define	f_swapflag	fun.fuw.fuw_swapflag
#define	f_rootdev	fun.fus.fus_rootdev
#define	f_dumpdev	fun.fus.fus_dumpdev
#define f_compdev	fun.fup.fup_compdev
#define f_compinfo	fun.fup.fup_compinfo
};

/*
 * Types.
 */
#define DRIVER		1
#define NORMAL		2
#define	INVISIBLE	3
#define	PROFILING	4
#define	SYSTEMSPEC	5
#define	SWAPSPEC	6
#define COMPDEVICE	7
#define COMPSPEC	8
#define NODEPEND	9
#define LOCAL		10
#define DEVDONE		0x80000000
#define TYPEMASK	0x7fffffff

/*
 * Attributes (flags).
 */
#define	CONFIGDEP	1
#define NO_IMPLCT_RULE	2
#define NO_OBJ		4
#define BEFORE_DEPEND	8

struct	idlst {
	char	*id;
	struct	idlst *id_next;
};

struct device {
	int	d_type;			/* CONTROLLER, DEVICE, bus adaptor */
	struct	device *d_conn;		/* what it is connected to */
	char	*d_name;		/* name of device (e.g. rk11) */
	struct	idlst *d_vec;		/* interrupt vectors */
	int	d_pri;			/* interrupt priority */
	int	d_addr;			/* address of csr */
	int	d_unit;			/* unit number */
	int	d_drive;		/* drive number */
	int	d_target;		/* target number */
	int	d_lun;			/* unit number */
	int	d_slave;		/* slave number */
#define QUES	-1	/* -1 means '?' */
#define	UNKNOWN -2	/* -2 means not set yet */
	int	d_dk;			/* if init 1 set to number for iostat */
	int	d_flags;		/* flags for device init */
	int	d_conflicts;		/* I'm allowed to conflict */
	int	d_disabled;		/* nonzero to skip probe/attach */
	char	*d_port;		/* io port base manifest constant */
	int	d_portn;	/* io port base (if number not manifest) */
	char	*d_mask;		/* interrupt mask */
	int	d_maddr;		/* io memory base */
	int	d_msize;		/* io memory size */
	int	d_drq;			/* DMA request  */
	int	d_irq;			/* interrupt request  */
	struct	device *d_next;		/* Next one in list */
};
#define TO_NEXUS	(struct device *)-1
#define TO_VBA		(struct device *)-2

struct config {
	char	*c_dev;
	char	*s_sysname;
};

/*
 * Config has a global notion of which machine type is
 * being used.  It uses the name of the machine in choosing
 * files and directories.  Thus if the name of the machine is ``vax'',
 * it will build from ``Makefile.vax'' and use ``../vax/inline''
 * in the makerules, etc.
 */
int	machine;
char	*machinename;
#define	MACHINE_I386	1
#define MACHINE_PC98	2
#define MACHINE_ALPHA	3

/*
 * For each machine, a set of CPU's may be specified as supported.
 * These and the options (below) are put in the C flags in the makefile.
 */
struct cputype {
	char	*cpu_name;
	struct	cputype *cpu_next;
} *cputype;

/*
 * A set of options may also be specified which are like CPU types,
 * but which may also specify values for the options.
 * A separate set of options may be defined for make-style options.
 */
struct opt {
	char	*op_name;
	char	*op_value;
	int	op_line;	/* line number for error-reporting */
	int	op_ownfile;	/* true = own file, false = makefile */
	struct	opt *op_next;
} *opt, *mkopt;

struct opt_list {
	char *o_name;
	char *o_file;
	struct opt_list *o_next;
} *otab;

extern char	*ident;
extern int	do_trace;

char	*ns();
char	*tc();
char	*get_word();
char	*get_quoted_word();
char	*path();
char	*raisestr();
void	moveifchanged();
dev_t	nametodev();
char	*devtoname();
void	init_dev __P((struct device *));       


#if MACHINE_I386
extern int	seen_isa;
extern int	seen_scbus;
#endif

extern struct	device *dtab;

extern char	errbuf[80];
extern int	yyline;

extern struct	file_list *ftab, *conf_list, **confp, *comp_list, **compp;

extern int	profiling;
extern int	debugging;

extern int	maxusers;
extern u_int	loadaddress;

extern	int old_config_present;	/* Old config/build directory still there */

extern char *PREFIX;		/* Config file name - for error messages */

#define eq(a,b)	(!strcmp(a,b))
