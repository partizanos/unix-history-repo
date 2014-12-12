/*-
 * Copyright (c) 2012 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef __T4_TOM_H__
#define __T4_TOM_H__

#define LISTEN_HASH_SIZE 32

/*
 * Min receive window.  We want it to be large enough to accommodate receive
 * coalescing, handle jumbo frames, and not trigger sender SWS avoidance.
 */
#define MIN_RCV_WND (24 * 1024U)

/*
 * Max receive window supported by HW in bytes.  Only a small part of it can
 * be set through option0, the rest needs to be set through RX_DATA_ACK.
 */
#define MAX_RCV_WND ((1U << 27) - 1)

#define	DDP_RSVD_WIN (16 * 1024U)
#define	SB_DDP_INDICATE	SB_IN_TOE	/* soreceive must respond to indicate */

#define USE_DDP_RX_FLOW_CONTROL

/* TOE PCB flags */
enum {
	TPF_ATTACHED	   = (1 << 0),	/* a tcpcb refers to this toepcb */
	TPF_FLOWC_WR_SENT  = (1 << 1),	/* firmware flow context WR sent */
	TPF_TX_DATA_SENT   = (1 << 2),	/* some data sent */
	TPF_TX_SUSPENDED   = (1 << 3),	/* tx suspended for lack of resources */
	TPF_SEND_FIN	   = (1 << 4),	/* send FIN after all pending data */
	TPF_FIN_SENT	   = (1 << 5),	/* FIN has been sent */
	TPF_ABORT_SHUTDOWN = (1 << 6),	/* connection abort is in progress */
	TPF_CPL_PENDING    = (1 << 7),	/* haven't received the last CPL */
	TPF_SYNQE	   = (1 << 8),	/* synq_entry, not really a toepcb */
	TPF_SYNQE_NEEDFREE = (1 << 9),	/* synq_entry was malloc'd separately */
	TPF_SYNQE_TCPDDP   = (1 << 10),	/* ulp_mode TCPDDP in toepcb */
	TPF_SYNQE_EXPANDED = (1 << 11),	/* toepcb ready, tid context updated */
	TPF_SYNQE_HAS_L2TE = (1 << 12),	/* we've replied to PASS_ACCEPT_REQ */
};

enum {
	DDP_OK		= (1 << 0),	/* OK to turn on DDP */
	DDP_SC_REQ	= (1 << 1),	/* state change (on/off) requested */
	DDP_ON		= (1 << 2),	/* DDP is turned on */
	DDP_BUF0_ACTIVE	= (1 << 3),	/* buffer 0 in use (not invalidated) */
	DDP_BUF1_ACTIVE	= (1 << 4),	/* buffer 1 in use (not invalidated) */
};

struct ofld_tx_sdesc {
	uint32_t plen;		/* payload length */
	uint8_t tx_credits;	/* firmware tx credits (unit is 16B) */
};

struct ppod_region {
	TAILQ_ENTRY(ppod_region) link;
	int used;	/* # of pods used by this region */
	int free;	/* # of contiguous pods free right after this region */
};

struct ddp_buffer {
	uint32_t tag;	/* includes color, page pod addr, and DDP page size */
	int nppods;
	int offset;
	int len;
	struct ppod_region ppod_region;
	int npages;
	vm_page_t *pages;
};

struct toepcb {
	TAILQ_ENTRY(toepcb) link; /* toep_list */
	u_int flags;		/* miscellaneous flags */
	struct tom_data *td;
	struct inpcb *inp;	/* backpointer to host stack's PCB */
	struct port_info *port;	/* physical port */
	struct sge_wrq *ofld_txq;
	struct sge_ofld_rxq *ofld_rxq;
	struct sge_wrq *ctrlq;
	struct l2t_entry *l2te;	/* L2 table entry used by this connection */
	struct clip_entry *ce;	/* CLIP table entry used by this tid */
	int tid;		/* Connection identifier */

	/* tx credit handling */
	u_int tx_total;		/* total tx WR credits (in 16B units) */
	u_int tx_credits;	/* tx WR credits (in 16B units) available */
	u_int tx_nocompl;	/* tx WR credits since last compl request */
	u_int plen_nocompl;	/* payload since last compl request */

	/* rx credit handling */
	u_int sb_cc;		/* last noted value of so_rcv->sb_cc */
	int rx_credits;		/* rx credits (in bytes) to be returned to hw */

	u_int ulp_mode;	/* ULP mode */

	u_int ddp_flags;
	struct ddp_buffer *db[2];
	time_t ddp_disabled;
	uint8_t ddp_score;

	/* Tx software descriptor */
	uint8_t txsd_total;
	uint8_t txsd_pidx;
	uint8_t txsd_cidx;
	uint8_t txsd_avail;
	struct ofld_tx_sdesc txsd[];
};

struct flowc_tx_params {
	uint32_t snd_nxt;
	uint32_t rcv_nxt;
	unsigned int snd_space;
	unsigned int mss;
};

#define	DDP_RETRY_WAIT	5	/* seconds to wait before re-enabling DDP */
#define	DDP_LOW_SCORE	1
#define	DDP_HIGH_SCORE	3

/*
 * Compressed state for embryonic connections for a listener.  Barely fits in
 * 64B, try not to grow it further.
 */
struct synq_entry {
	TAILQ_ENTRY(synq_entry) link;	/* listen_ctx's synq link */
	int flags;			/* same as toepcb's tp_flags */
	int tid;
	struct listen_ctx *lctx;	/* backpointer to listen ctx */
	struct mbuf *syn;
	uint32_t iss;
	uint32_t ts;
	volatile uintptr_t wr;
	volatile u_int refcnt;
	uint16_t l2e_idx;
	uint16_t rcv_bufsize;
};

/* listen_ctx flags */
#define LCTX_RPL_PENDING 1	/* waiting for a CPL_PASS_OPEN_RPL */

struct listen_ctx {
	LIST_ENTRY(listen_ctx) link;	/* listen hash linkage */
	volatile int refcount;
	int stid;
	struct stid_region stid_region;
	int flags;
	struct inpcb *inp;		/* listening socket's inp */
	struct sge_wrq *ctrlq;
	struct sge_ofld_rxq *ofld_rxq;
	struct clip_entry *ce;
	TAILQ_HEAD(, synq_entry) synq;
};

TAILQ_HEAD(ppod_head, ppod_region);

struct clip_entry {
	TAILQ_ENTRY(clip_entry) link;
	struct in6_addr lip;	/* local IPv6 address */
	u_int refcount;
};

TAILQ_HEAD(clip_head, clip_entry);
struct tom_data {
	struct toedev tod;

	/* toepcb's associated with this TOE device */
	struct mtx toep_list_lock;
	TAILQ_HEAD(, toepcb) toep_list;

	struct mtx lctx_hash_lock;
	LIST_HEAD(, listen_ctx) *listen_hash;
	u_long listen_mask;
	int lctx_count;		/* # of lctx in the hash table */

	struct mtx ppod_lock;
	int nppods;
	int nppods_free;	/* # of available ppods */
	int nppods_free_head;	/* # of available ppods at the begining */
	struct ppod_head ppods;

	struct mtx clip_table_lock;
	struct clip_head clip_table;
	int clip_gen;

	/* WRs that will not be sent to the chip because L2 resolution failed */
	struct mtx unsent_wr_lock;
	STAILQ_HEAD(, wrqe) unsent_wr_list;
	struct task reclaim_wr_resources;
};

static inline struct tom_data *
tod_td(struct toedev *tod)
{

	return (__containerof(tod, struct tom_data, tod));
}

static inline struct adapter *
td_adapter(struct tom_data *td)
{

	return (td->tod.tod_softc);
}

/* t4_tom.c */
struct toepcb *alloc_toepcb(struct port_info *, int, int, int);
void free_toepcb(struct toepcb *);
void offload_socket(struct socket *, struct toepcb *);
void undo_offload_socket(struct socket *);
void final_cpl_received(struct toepcb *);
void insert_tid(struct adapter *, int, void *);
void *lookup_tid(struct adapter *, int);
void update_tid(struct adapter *, int, void *);
void remove_tid(struct adapter *, int);
void release_tid(struct adapter *, int, struct sge_wrq *);
int find_best_mtu_idx(struct adapter *, struct in_conninfo *, int);
u_long select_rcv_wnd(struct socket *);
int select_rcv_wscale(void);
uint64_t calc_opt0(struct socket *, struct port_info *, struct l2t_entry *,
    int, int, int, int);
uint64_t select_ntuple(struct port_info *, struct l2t_entry *);
void set_tcpddp_ulp_mode(struct toepcb *);
int negative_advice(int);
struct clip_entry *hold_lip(struct tom_data *, struct in6_addr *);
void release_lip(struct tom_data *, struct clip_entry *);

/* t4_connect.c */
void t4_init_connect_cpl_handlers(struct adapter *);
int t4_connect(struct toedev *, struct socket *, struct rtentry *,
    struct sockaddr *);
void act_open_failure_cleanup(struct adapter *, u_int, u_int);

/* t4_listen.c */
void t4_init_listen_cpl_handlers(struct adapter *);
int t4_listen_start(struct toedev *, struct tcpcb *);
int t4_listen_stop(struct toedev *, struct tcpcb *);
void t4_syncache_added(struct toedev *, void *);
void t4_syncache_removed(struct toedev *, void *);
int t4_syncache_respond(struct toedev *, void *, struct mbuf *);
int do_abort_req_synqe(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
int do_abort_rpl_synqe(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
void t4_offload_socket(struct toedev *, void *, struct socket *);

/* t4_cpl_io.c */
void t4_init_cpl_io_handlers(struct adapter *);
void t4_uninit_cpl_io_handlers(struct adapter *);
void send_abort_rpl(struct adapter *, struct sge_wrq *, int , int);
void send_flowc_wr(struct toepcb *, struct flowc_tx_params *);
void send_reset(struct adapter *, struct toepcb *, uint32_t);
void make_established(struct toepcb *, uint32_t, uint32_t, uint16_t);
void t4_rcvd(struct toedev *, struct tcpcb *);
int t4_tod_output(struct toedev *, struct tcpcb *);
int t4_send_fin(struct toedev *, struct tcpcb *);
int t4_send_rst(struct toedev *, struct tcpcb *);
void t4_set_tcb_field(struct adapter *, struct toepcb *, int, uint16_t,
    uint64_t, uint64_t);
void t4_push_frames(struct adapter *sc, struct toepcb *toep, int drop);

/* t4_ddp.c */
void t4_init_ddp(struct adapter *, struct tom_data *);
void t4_uninit_ddp(struct adapter *, struct tom_data *);
int t4_soreceive_ddp(struct socket *, struct sockaddr **, struct uio *,
    struct mbuf **, struct mbuf **, int *);
struct mbuf *get_ddp_mbuf(int);
void enable_ddp(struct adapter *, struct toepcb *toep);
void release_ddp_resources(struct toepcb *toep);
void insert_ddp_data(struct toepcb *, uint32_t);

/* ULP related */
#define CXGBE_ISCSI_MBUF_TAG          50
int t4tom_cpl_handler_registered(struct adapter *, unsigned int);
void t4tom_register_cpl_iscsi_callback(void (*fp)(struct tom_data *,
    struct socket *, void *, unsigned int));
void t4tom_register_queue_iscsi_callback(struct mbuf *(*fp)(struct socket *,
    unsigned int, int *));
void t4_ulp_push_frames(struct adapter *sc, struct toepcb *toep, int);
int t4_cpl_iscsi_callback(struct tom_data *, struct toepcb *, void *, uint32_t);
struct mbuf *t4_queue_iscsi_callback(struct socket *, struct toepcb *, uint32_t,
    int *);
extern void (*tom_cpl_iscsi_callback)(struct tom_data *, struct socket *,
    void *, unsigned int);
extern struct mbuf *(*tom_queue_iscsi_callback)(struct socket*, unsigned int,
    int *);
#endif
