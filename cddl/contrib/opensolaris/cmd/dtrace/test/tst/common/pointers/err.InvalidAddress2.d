/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2012 by Delphix. All rights reserved.
 */

/*
 * ASSERTION: D pointers do not allow invalid pointer accesses.
 *
 * SECTION: Pointers and Arrays/Pointer Safety
 *
 * NOTES:
 *
 */

#pragma D option quiet

BEGIN
{
	x = (int *)alloca(sizeof (int));
	trace(x);
	y = (int *) (x - 3300778156056);
	*y = 3;
	trace(*y);
	exit(0);
}

ERROR
{
	exit(1);
}
