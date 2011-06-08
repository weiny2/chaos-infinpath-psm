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

#ifndef _PSMI_IN_USER_H
#error psm_ep.h not meant to be included directly, include psm_user.h instead
#endif

#ifndef _PSMI_EP_H
#define _PSMI_EP_H

/* 
 * EPIDs encode the following information:
 * 
 * LID:16 bits - LID for endpoint
 * SUBCONTEXT:2 bits - Subcontext used for endpoint
 * CONTEXT:6 bits - Context used for bits (upto 64 contexts)
 * IBA_SL: 4 bits - Default SL to use for endpoint
 * HCATYPE: 3 bits - QLE71XX, QLE72XX, QLE73XX ....
 */

#define PSMI_HCA_TYPE_UNKNOWN 0
#define PSMI_HCA_TYPE_QLE71XX 1
#define PSMI_HCA_TYPE_QLE72XX 2
#define PSMI_HCA_TYPE_QLE73XX 3
#define PSMI_HCA_TYPE_DEFAULT PSMI_HCA_TYPE_UNKNOWN

#define PSMI_SL_DEFAULT 0
#define PSMI_VL_DEFAULT 0

#define PSMI_EPID_PACK_EXT(lid,context,subcontext,hca_type,sl) \
  ( ((((uint64_t)lid)&0xffff)<<16) |			       \
    ((((uint64_t)subcontext)&0x3)<<14) |		       \
    ((((uint64_t)context)&0x3f)<<8) |			       \
    ((((uint64_t)sl)&0xf)<<4) |				       \
    (((uint64_t)hca_type)&0x7) )

#define PSMI_EPID_UNPACK_EXT(epid,lid,context,subcontext,hca_type,sl) do { \
    (lid) = ((epid)>>16)&0xffff;					\
    (subcontext) = ((epid)>>14)&0x3;					\
    (context) = ((epid)>>8)&0x3f;					\
    (sl) = ((epid)>>4)&0xf;						\
    (hca_type) = ((epid)&0x7);						\
  } while (0)

#define PSMI_EPID_PACK(lid,context,subcontext)	\
  PSMI_EPID_PACK_EXT(lid,context,subcontext,PSMI_HCA_TYPE_DEFAULT, PSMI_SL_DEFAULT)

#define PSMI_EPID_UNPACK(epid,lid,context,subcontext) do {	\
    uint32_t hca_type, sl;					\
    PSMI_EPID_UNPACK_EXT(epid,lid,context,subcontext,hca_type,sl);	\
  } while (0)

#define PSMI_MIN_EP_CONNECT_TIMEOUT (2 * SEC_ULL)
#define PSMI_MIN_EP_CLOSE_TIMEOUT   (2 * SEC_ULL)
#define PSMI_MAX_EP_CLOSE_TIMEOUT   (60 * SEC_ULL)

#define PSMI_MIN_EP_CLOSE_GRACE_INTERVAL (1 * SEC_ULL)
#define PSMI_MAX_EP_CLOSE_GRACE_INTERVAL (10 * SEC_ULL)

struct psm_ep {
    psm_epid_t		epid;	    /**> This endpoint's Endpoint ID */
    psm_epaddr_t	epaddr;	    /**> This ep's ep address */
    psm_mq_t		mq;	    /**> only 1 MQ */
    int			unit_id;
    uint16_t		portnum;
    uint8_t		out_sl;
    uint8_t             pad;
    int			did_syslog;
    psm_uuid_t		key;
    uint16_t		network_pkey; /**> InfiniBand Pkey */
    uint64_t            service_id;   /* Infiniband service ID */
    psm_path_res_t      path_res_type;/* Path resolution for endpoint */
    psm_ep_errhandler_t	errh;
    int			devid_enabled[PTL_MAX_INIT];
    int			memmode;    /**> min, normal, large memory mode */

    uint32_t	ipath_num_sendbufs; /**> Number of allocated send buffers */
    uint32_t    ipath_num_descriptors; /** Number of allocated scb descriptors*/
    uint32_t    ipath_imm_size;     /** Immediate data size */
    uint32_t	shm_mbytes;	    /**> Number of shared memory pages */
    uint32_t	connections;	    /**> Number of connections */	

    psmi_context_t	context;
    char	*context_mylabel;
    uint32_t	yield_spin_cnt;

    /* Active Message handler table */
    void    **am_htable;

    uint64_t    gid_hi;
    uint64_t    gid_lo;

    ptl_ctl_t	ptl_amsh;
    ptl_ctl_t	ptl_ips;
    ptl_ctl_t	ptl_self;

    /* All ptl data is allocated inline below */
    uint8_t ptl_base_data[0] __attribute__((aligned(8)));
};

#define PSMI_EGRLONG_FLOWS_MAX	4   /* must be same as EP_FLOW_LAST */

typedef
union psmi_egrid {
    struct {
	uint32_t	egr_flowid : 8;
	uint32_t	egr_msgno  : 24;
    };
    uint32_t	egr_data;
}
psmi_egrid_t;

#define PSMI_EGRID_NULL	{ .egr_flowid = 0, .egr_msgno = 0 }

typedef 
union psmi_seqnum {
  struct {
    uint32_t seq:11;
    uint32_t gen:8;
    uint32_t flow:5;
  };
  struct {
    uint32_t psn:24;
    uint32_t pad:8;
  };
  uint32_t val;
} psmi_seqnum_t;

struct psm_epaddr {
    struct ptl	    *ptl;	   /* Which ptl owns this epaddress */
    ptl_ctl_t	    *ptlctl;	   /* The control structure for the ptl */
    psm_epid_t	    epid;	   
    psm_ep_t	    ep;
  
    void           *usr_ep_ctxt;   /* User context associated with endpoint */

    STAILQ_HEAD(, psm_mq_req)	egrlong[PSMI_EGRLONG_FLOWS_MAX];
    //psm_mq_req_t    next_egrlong[PSMI_EGRLONG_FLOWS_MAX];

    /* PTLs have a few ways to initialize the ptl address */
    union {
	ptl_epaddr_t    *ptladdr;
	uint32_t	 _ptladdr_u32[2];
	uint64_t	 _ptladdr_u64;
	uint8_t		 _ptladdr_data[0];
    };
};

#ifndef PSMI_BLOCKUNTIL_POLLS_BEFORE_YIELD
#  define PSMI_BLOCKUNTIL_POLLS_BEFORE_YIELD  250
#endif

/*
 * Users of BLOCKUNTIL should check the value of err upon return 
 */
#define PSMI_BLOCKUNTIL(ep,err,cond)	do {			\
	    int spin_cnt = 0;					\
	    PSMI_PROFILE_BLOCK();				\
	    while (!(cond)) {					\
		err = psmi_poll_internal(ep, 1);		\
		if (err == PSM_OK_NO_PROGRESS) {		\
		    PSMI_PROFILE_REBLOCK(1);			\
		    if (++spin_cnt == (ep)->yield_spin_cnt) {   \
			spin_cnt = 0;				\
			PSMI_PYIELD();				\
		    }						\
		}						\
		else if (err == PSM_OK) {			\
		    PSMI_PROFILE_REBLOCK(0);			\
		    spin_cnt = 0;				\
		}						\
		else						\
		    break;					\
	    }							\
	    PSMI_PROFILE_UNBLOCK();				\
	} while(0)

#endif /* _PSMI_EP_H */
