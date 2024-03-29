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

#include "psm_user.h"
#include "psm_mq_internal.h"

static void __recvpath
psmi_mq_req_copy(psm_mq_req_t req, psm_epaddr_t epaddr, const void *buf, 
		 uint32_t nbytes)
{
    // recv_msglen may be changed by unexpected receive buf.
    uint32_t msglen_left = req->recv_msglen - req->recv_msgoff;
    uint32_t msglen_this = min(msglen_left, nbytes);
    uint8_t *msgptr = (uint8_t *)req->buf + req->recv_msgoff;
    
    VALGRIND_MAKE_MEM_DEFINED(msgptr, msglen_this);
    psmi_mq_mtucpy(msgptr, buf, msglen_this);
    
    req->recv_msgoff += msglen_this;
    req->send_msgoff += nbytes;
    return;
}

int __recvpath
psmi_mq_handle_data(psm_mq_req_t req, psm_epaddr_t epaddr, 
		    const void *buf, uint32_t nbytes)
{
    psm_mq_t mq = req->mq;
    int rc;
    
    if (req->state == MQ_STATE_MATCHED)
	rc = MQ_RET_MATCH_OK;
    else {
	psmi_assert(req->state == MQ_STATE_UNEXP);
	rc = MQ_RET_UNEXP_OK;
    }

    psmi_mq_req_copy(req, epaddr, buf, nbytes);

    if (req->send_msgoff == req->send_msglen) {
	if (req->type & MQE_TYPE_EGRLONG) {
	    int flowid = req->egrid.egr_flowid;
	    psmi_assert(STAILQ_FIRST(&epaddr->egrlong[flowid]) == req);
	    STAILQ_REMOVE_HEAD(&epaddr->egrlong[flowid], nextq);
	}
	    
	/* Whatever is leftover in the posted message should be now marked as
	 * undefined.
	 * XXX Sends not supported yet.
	 */
#if 0
#ifdef PSM_VALGRIND
	if (req->send_msglen < req->buf_len)
	    VALGRIND_MAKE_MEM_UNDEFINED(
		(void *) ((uintptr_t) req->buf + req->send_msglen), 
		req->buf_len - req->send_msglen);
#endif
#endif
	if (req->state == MQ_STATE_MATCHED) {
	    req->state = MQ_STATE_COMPLETE;
	    mq_qq_append(&mq->completed_q, req);
	}
	else { /* MQ_STATE_UNEXP */
	    req->state = MQ_STATE_COMPLETE;
	}
	_IPATH_VDBG("epaddr=%s completed %d byte send, state=%d\n", 
		    psmi_epaddr_get_name(epaddr->epid),
		    (int)req->send_msglen, req->state);
    }

    return rc;
}

int __recvpath
psmi_mq_handle_rts(psm_mq_t mq, uint64_t tag, 
		   uintptr_t send_buf, uint32_t send_msglen, 
		   psm_epaddr_t peer, mq_rts_callback_fn_t cb, 
		   psm_mq_req_t *req_o)
{
    psm_mq_req_t req;
    uint32_t msglen;
    int rc;

    PSMI_PLOCK_ASSERT();

    req = mq_req_match(&(mq->expected_q), tag, 1);

    if (req) { /* we have a match, no need to callback */
	msglen = mq_set_msglen(req, req->buf_len, send_msglen);
	req->type = MQE_TYPE_RECV;
	req->state = MQ_STATE_MATCHED;
	req->tag = tag;
	req->recv_msgoff = 0;
	req->rts_peer = peer;
	req->rts_sbuf = send_buf;
	*req_o = req; /* yes match */
	rc = MQ_RET_MATCH_OK;
    }
    else { /* No match, keep track of callback */
	req = psmi_mq_req_alloc(mq, MQE_TYPE_RECV);
	psmi_assert(req != NULL);

	req->type = MQE_TYPE_RECV;
	/* We don't know recv_msglen yet but we set it here for
	 * mq_iprobe */
	req->send_msglen = req->recv_msglen = send_msglen;
	req->state = MQ_STATE_UNEXP_RV;
	req->tag = tag;
	req->rts_callback = cb;
	req->recv_msgoff = 0;
	req->rts_peer = peer;
	req->rts_sbuf = send_buf;
	mq_sq_append(&mq->unexpected_q, req);
	*req_o = req; /* no match, will callback */
	rc = MQ_RET_UNEXP_OK;
    }

    _IPATH_VDBG("from=%s match=%s (req=%p) mqtag=%" PRIx64" recvlen=%d "
		"sendlen=%d errcode=%d\n", psmi_epaddr_get_name(peer->epid), 
		rc == MQ_RET_MATCH_OK ? "YES" : "NO", req, req->tag, 
		req->recv_msglen, req->send_msglen, req->error_code);
    return rc;
}

void
psmi_mq_handle_rts_complete(psm_mq_req_t req) 
{
    psm_mq_t mq = req->mq;

    /* Stats on rendez-vous messages */
    psmi_mq_stats_rts_account(req);
    req->state = MQ_STATE_COMPLETE;
    mq_qq_append(&mq->completed_q, req);
#ifdef PSM_VALGRIND
    if (MQE_TYPE_IS_RECV(req->type))
	PSM_VALGRIND_DEFINE_MQ_RECV(req->buf, req->buf_len, req->recv_msglen);
    else
	VALGRIND_MAKE_MEM_DEFINED(req->buf, req->buf_len);
#endif
    _IPATH_VDBG("RTS complete, req=%p, recv_msglen = %d\n", 
		    req, req->recv_msglen);
    return;
}

/* Not exposed in public psm, but may extend parts of PSM 2.1 to support
 * this feature before 2.3 */
psm_mq_unexpected_callback_fn_t
psmi_mq_register_unexpected_callback(psm_mq_t mq, 
				     psm_mq_unexpected_callback_fn_t fn)
{
    psm_mq_unexpected_callback_fn_t old_fn = mq->unexpected_callback;
    mq->unexpected_callback = fn;
    return old_fn;
}

int __recvpath
psmi_mq_handle_envelope_unexpected(
	psm_mq_t mq, uint16_t mode, psm_epaddr_t epaddr,
	uint64_t tag, psmi_egrid_t egrid, uint32_t send_msglen, 
	const void *payload, uint32_t paylen)
{
    psm_mq_req_t req;
    uint32_t msglen;

    /* 
     * Keep a callback here in case we want to fit some other high-level
     * protocols over MQ (i.e. shmem).  These protocols would bypass the
     * normal mesage handling and go to higher-level message handlers.
     */
    if (mode >= MQ_MSG_USER_FIRST && mq->unexpected_callback) {
	mq->unexpected_callback(mq,mode,epaddr,tag,send_msglen,payload,paylen);
	return MQ_RET_UNEXP_OK;
    }
    req = psmi_mq_req_alloc(mq, MQE_TYPE_RECV);
    psmi_assert(req != NULL);

    req->tag = tag;
    req->recv_msgoff = 0;
    req->recv_msglen = req->send_msglen = req->buf_len = msglen = send_msglen;

    _IPATH_VDBG(
		"from=%s match=NO (req=%p) mode=%x mqtag=%" PRIx64
		" send_msglen=%d\n", psmi_epaddr_get_name(epaddr->epid), 
		req, mode, tag, send_msglen);
#if 0
    if (mq->cur_sysbuf_bytes+msglen > mq->max_sysbuf_bytes) {
		_IPATH_VDBG("req=%p with len=%d exceeds limit of %llu sysbuf_bytes\n",
			req, msglen, (unsigned long long) mq->max_sysbuf_bytes);
		return MQ_RET_UNEXP_NO_RESOURCES;
    }
#endif
    switch (mode) {
	case MQ_MSG_TINY:
	    if (msglen > 0) {
		req->buf = psmi_mq_sysbuf_alloc(mq, msglen);
		mq_copy_tiny((uint32_t *)req->buf, (uint32_t *)payload, msglen);
	    }
	    else
		req->buf = NULL;
	    req->state = MQ_STATE_COMPLETE;
	    break;

	case MQ_MSG_SHORT:
	    req->buf = psmi_mq_sysbuf_alloc(mq, msglen);
	    psmi_mq_mtucpy(req->buf, payload, msglen);
	    req->state = MQ_STATE_COMPLETE;
	    break;

	case MQ_MSG_LONG:
	    req->egrid = egrid;
	    req->send_msgoff = 0;
	    req->buf = psmi_mq_sysbuf_alloc(mq, msglen);
	    req->state = MQ_STATE_UNEXP;
	    req->type |= MQE_TYPE_EGRLONG;
	    STAILQ_INSERT_TAIL(&epaddr->egrlong[egrid.egr_flowid], req, nextq);
	    _IPATH_VDBG("unexp MSG_LONG %d of length %d bytes pay=%d\n", 
			egrid.egr_msgno, msglen, paylen);
	    psmi_mq_handle_data(req, epaddr, payload, paylen);
	    break;

	default:
	    psmi_handle_error(PSMI_EP_NORETURN, PSM_INTERNAL_ERR,
			    "Internal error, unknown packet 0x%x", mode);
    }
    mq_sq_append(&mq->unexpected_q, req);
    mq->stats.rx_sys_bytes += msglen;
    mq->stats.rx_sys_num++;

    return MQ_RET_UNEXP_OK;
}

/* 
 * This handles the regular (i.e. non-rendezvous MPI envelopes) 
 */
int __recvpath
psmi_mq_handle_envelope(psm_mq_t mq, uint16_t mode, psm_epaddr_t epaddr,
		   uint64_t tag, psmi_egrid_t egrid, uint32_t send_msglen, 
		   const void *payload, uint32_t paylen)
{
    psm_mq_req_t req;
    uint32_t msglen;
    int rc;

    psmi_assert(epaddr != NULL);

    req = mq_req_match(&(mq->expected_q), tag, 1);

    if (req) { /* we have a match */
	psmi_assert(MQE_TYPE_IS_RECV(req->type));
	req->tag = tag;
	msglen = mq_set_msglen(req, req->buf_len, send_msglen);

	_IPATH_VDBG("from=%s match=YES (req=%p) mode=%x mqtag=%"
		PRIx64" msglen=%d paylen=%d\n", psmi_epaddr_get_name(epaddr->epid), 
		req, mode, tag, msglen, paylen);

	switch(mode) {
	    case MQ_MSG_TINY:
		PSM_VALGRIND_DEFINE_MQ_RECV(req->buf, req->buf_len, msglen);
		mq_copy_tiny((uint32_t *)req->buf, (uint32_t *)payload, msglen);
		req->state = MQ_STATE_COMPLETE;
		mq_qq_append(&mq->completed_q, req);
		break;

	    case MQ_MSG_SHORT: /* message fits in 1 payload */
		PSM_VALGRIND_DEFINE_MQ_RECV(req->buf, req->buf_len, msglen);
		psmi_mq_mtucpy(req->buf, payload, msglen);
		req->state = MQ_STATE_COMPLETE;
		mq_qq_append(&mq->completed_q, req);
		break;

	    case MQ_MSG_LONG:
		req->egrid = egrid;
		req->state = MQ_STATE_MATCHED;
		req->type |= MQE_TYPE_EGRLONG;
		req->send_msgoff = req->recv_msgoff = 0;
		STAILQ_INSERT_TAIL(&epaddr->egrlong[egrid.egr_flowid], req, nextq);
		_IPATH_VDBG("exp MSG_LONG %d of length %d bytes pay=%d\n", 
			egrid.egr_msgno, msglen, paylen);
		psmi_mq_handle_data(req, epaddr, payload, paylen);
		break;

	    default:
		psmi_handle_error(PSMI_EP_NORETURN, PSM_INTERNAL_ERR,
			    "Internal error, unknown packet 0x%x", mode);
	}

	mq->stats.rx_user_bytes += msglen;
	mq->stats.rx_user_num++;

	rc = MQ_RET_MATCH_OK;
	if (mode == MQ_MSG_LONG)
	    return rc;
    }
    else
	rc =  psmi_mq_handle_envelope_unexpected(mq, mode, epaddr, tag,
		    egrid, send_msglen, payload, paylen);

    return rc;
}
