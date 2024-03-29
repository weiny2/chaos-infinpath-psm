/*
 * Copyright (c) 2006-2010. QLogic Corporation. All rights reserved.
 * Copyright (c) 2003-2006, PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _IPS_EPSTATE_H
#define _IPS_EPSTATE_H

#include "psm_user.h"

typedef uint16_t ips_epstate_idx;
#define IPS_EPSTATE_COMMIDX_MAX 65535

struct ptl_epaddr;

struct ips_epstate_entry {
    uint64_t            epid;
    struct ptl_epaddr	*ipsaddr;
};

struct ips_epstate {
    const psmi_context_t	*context;
    ips_epstate_idx	eps_base_idx;
    int			eps_tabsize;
    int			eps_tabsizeused;
    int			eps_tab_nextidx;

    struct ips_epstate_entry *eps_tab;
};

psm_error_t ips_epstate_init(struct ips_epstate *eps, const psmi_context_t *contextj);
psm_error_t ips_epstate_fini(struct ips_epstate *eps);

psm_error_t  ips_epstate_add(struct ips_epstate *eps, 
			     struct ptl_epaddr *ipsaddr,
			     ips_epstate_idx *commidx);
psm_error_t  ips_epstate_del(struct ips_epstate *eps, ips_epstate_idx commidx);

PSMI_INLINE(
struct ips_epstate_entry *
ips_epstate_lookup(const struct ips_epstate *eps, ips_epstate_idx idx))
{
    idx += eps->eps_base_idx;
    if (idx < eps->eps_tabsize)
	return &eps->eps_tab[idx];
    else
	return NULL;
}

#endif /* _IPS_EPSTATE_H */
