/*
 * <security/pam_appl.h>
 * 
 * This header file collects definitions for the PAM API --- that is,
 * public interface between the PAM library and an application program
 * that wishes to use it.
 *
 * Note, the copyright information is at end of file.
 *
 * Created: 15-Jan-96 by TYT
 * Last modified: 1996/3/5 by AGM
 *
 * $Id: pam_appl.h,v 1.3 2000/11/19 23:54:02 agmorgan Exp $
 * $FreeBSD$
 */

#ifndef _SECURITY_PAM_APPL_H
#define _SECURITY_PAM_APPL_H

#ifdef __cplusplus
extern "C" {
#endif
 
#include <security/_pam_types.h>      /* Linux-PAM common defined types */

/* -------------- The Linux-PAM Framework layer API ------------- */

extern int pam_start(const char *_service_name, const char *_user,
		     const struct pam_conv *_pam_conversation,
		     pam_handle_t **_pamh);
extern int pam_end(pam_handle_t *_pamh, int _pam_status);

/* Authentication API's */

extern int pam_authenticate(pam_handle_t *_pamh, int _flags);
extern int pam_setcred(pam_handle_t *_pamh, int _flags);

/* Account Management API's */

extern int pam_acct_mgmt(pam_handle_t *_pamh, int _flags);

/* Session Management API's */

extern int pam_open_session(pam_handle_t *_pamh, int _flags);
extern int pam_close_session(pam_handle_t *_pamh, int _flags);

/* Password Management API's */

extern int pam_chauthtok(pam_handle_t *_pamh, int _flags);

#ifdef __cplusplus
}
#endif

/* take care of any compatibility issues */
#include <security/_pam_compat.h>

/*
 * Copyright Theodore Ts'o, 1996.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#endif /* _SECURITY_PAM_APPL_H */
