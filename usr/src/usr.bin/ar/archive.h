/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Hugh Smith at The University of Guelph.
 *
 *	@(#)archive.h	5.3 (Berkeley) %G%
 */

/* Ar(1) options. */
#define	AR_A	0x0001
#define	AR_B	0x0002
#define	AR_C	0x0004
#define	AR_D	0x0008
#define	AR_M	0x0010
#define	AR_O	0x0020
#define	AR_P	0x0040
#define	AR_Q	0x0080
#define	AR_R	0x0100
#define	AR_S	0x0200
#define	AR_T	0x0400
#define	AR_U	0x0800
#define	AR_V	0x1000
#define	AR_X	0x2000
extern u_int options;

/* Set up file copy. */
#define	SETCF(from, fromname, to, toname, pad) { \
	cf.rfd = from; \
	cf.rname = fromname; \
	cf.wfd = to; \
	cf.wname = toname; \
	cf.flags = pad; \
}

/* File copy structure. */
typedef struct {
	int rfd;			/* read file descriptor */
	char *rname;			/* read name */
	int wfd;			/* write file descriptor */
	char *wname;			/* write name */
#define	NOPAD	0x00			/* don't pad */
#define	RPAD	0x01			/* pad on reads */
#define	WPAD	0x02			/* pad on writes */
	u_int flags;			/* pad flags */
} CF;

/* Header structure internal format. */
typedef struct {
	off_t size;			/* size of the object in bytes */
	long date;			/* date */
	int lname;			/* size of the long name in bytes */
	int gid;			/* group */
	int uid;			/* owner */
	u_short mode;			/* permissions */
	char name[MAXNAMLEN + 1];	/* name */
} CHDR;

/* Print out any files that weren't in the archive. */
#define	ORPHANS { \
	if (*argv) { \
		eval = 1; \
		do { \
			(void)fprintf(stderr, \
			    "ar: %s: not found in archive.\n", *argv); \
		} while (*++argv); \
	} \
}

/* Old archive format name limit. */
#define	ARNAMEMAX	16

/* Header format strings. */
#define	HDR1	"%s%-13d%-12ld%-6u%-6u%-8o%-10ld%2s"
#define	HDR2	"%-16.16s%-12ld%-6u%-6u%-8o%-10ld%2s"

#include <stdlib.h>
#include <string.h>
