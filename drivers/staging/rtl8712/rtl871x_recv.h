#ifndef _RTL871X_RECV_H_
#define _RTL871X_RECV_H_

#include "osdep_service.h"
#include "drv_types.h"

#define NR_RECVFRAME 256

#define RXFRAME_ALIGN	8
#define RXFRAME_ALIGN_SZ	(1 << RXFRAME_ALIGN)

#define MAX_RXFRAME_CNT	512
#define MAX_RX_NUMBLKS		(32)
#define RECVFRAME_HDR_ALIGN 128
#define MAX_SUBFRAME_COUNT	64

#define SNAP_SIZE sizeof(struct ieee80211_snap_hdr)

struct recv_reorder_ctrl {
	struct _adapter	*padapter;
	u16 indicate_seq; 
	u16 wend_b;
	u8 wsize_b;
	struct  __queue pending_recvframe_queue;
	struct timer_list reordering_ctrl_timer;
};

struct	stainfo_rxcache	{
	u16	tid_rxseq[16];
};

#define		PHY_RSSI_SLID_WIN_MAX			100
#define		PHY_LINKQUALITY_SLID_WIN_MAX		20


struct smooth_rssi_data {
	u32	elements[100];	
	u32	index;		
	u32	total_num;	
	u32	total_val;	
};

struct rx_pkt_attrib {

	u8	amsdu;
	u8	order;
	u8	qos;
	u8	to_fr_ds;
	u8	frag_num;
	u16	seq_num;
	u8   pw_save;
	u8    mfrag;
	u8    mdata;
	u8	privacy; 
	u8	bdecrypted;
	int	hdrlen;	 
	int	encrypt; 
	int	iv_len;
	int	icv_len;
	int	priority;
	int	ack_policy;
	u8	crc_err;
	u8	dst[ETH_ALEN];
	u8	src[ETH_ALEN];
	u8	ta[ETH_ALEN];
	u8	ra[ETH_ALEN];
	u8	bssid[ETH_ALEN];
	u8	tcpchk_valid; 
	u8	ip_chkrpt; 
	u8	tcp_chkrpt; 
	u8	signal_qual;
	s8	rx_mimo_signal_qual[2];
	u8	mcs_rate;
	u8	htc;
	u8	signal_strength;
};

struct recv_priv {
	spinlock_t lock;
	struct  __queue	free_recv_queue;
	struct  __queue	recv_pending_queue;
	u8 *pallocated_frame_buf;
	u8 *precv_frame_buf;
	uint free_recvframe_cnt;
	struct _adapter	*adapter;
	uint	rx_bytes;
	uint	rx_pkts;
	uint	rx_drop;
	uint  rx_icv_err;
	uint  rx_largepacket_crcerr;
	uint  rx_smallpacket_crcerr;
	uint  rx_middlepacket_crcerr;
	u8  rx_pending_cnt;
	uint	ff_hwaddr;
	struct tasklet_struct recv_tasklet;
	struct sk_buff_head free_recv_skb_queue;
	struct sk_buff_head rx_skb_queue;
	u8 *pallocated_recv_buf;
	u8 *precv_buf;    
	struct  __queue	free_recv_buf_queue;
	u32	free_recv_buf_queue_cnt;
	
	s8 rssi;
	u8 signal;
	u8 noise;
	u8 fw_rssi;
	struct smooth_rssi_data signal_qual_data;
	struct smooth_rssi_data signal_strength_data;
};

struct sta_recv_priv {
	spinlock_t lock;
	sint	option;
	struct  __queue defrag_q; 
	struct	stainfo_rxcache rxcache;
	uint	sta_rx_bytes;
	uint	sta_rx_pkts;
	uint	sta_rx_fail;
};

#include "rtl8712_recv.h"

union recv_frame *r8712_alloc_recvframe(struct  __queue *pfree_recv_queue);
union recv_frame *r8712_dequeue_recvframe(struct  __queue *queue);
int r8712_enqueue_recvframe(union recv_frame *precvframe,
			     struct  __queue *queue);
int r8712_free_recvframe(union recv_frame *precvframe,
			  struct  __queue *pfree_recv_queue);
void r8712_free_recvframe_queue(struct  __queue *pframequeue,
				 struct  __queue *pfree_recv_queue);
void r8712_init_recvframe(union recv_frame *precvframe,
			   struct recv_priv *precvpriv);
int r8712_wlanhdr_to_ethhdr(union recv_frame *precvframe);
int recv_func(struct _adapter *padapter, void *pcontext);

static inline u8 *get_rxmem(union recv_frame *precvframe)
{
	
	if (precvframe == NULL)
		return NULL;
	return precvframe->u.hdr.rx_head;
}

static inline u8 *get_rx_status(union recv_frame *precvframe)
{
	return get_rxmem(precvframe);
}

static inline u8 *get_recvframe_data(union recv_frame *precvframe)
{
	
	if (precvframe == NULL)
		return NULL;
	return precvframe->u.hdr.rx_data;
}

static inline u8 *recvframe_push(union recv_frame *precvframe, sint sz)
{
	


	if (precvframe == NULL)
		return NULL;
	precvframe->u.hdr.rx_data -= sz ;
	if (precvframe->u.hdr.rx_data < precvframe->u.hdr.rx_head) {
		precvframe->u.hdr.rx_data += sz ;
		return NULL;
	}
	precvframe->u.hdr.len += sz;
	return precvframe->u.hdr.rx_data;
}

static inline u8 *recvframe_pull(union recv_frame *precvframe, sint sz)
{
	if (precvframe == NULL)
		return NULL;
	precvframe->u.hdr.rx_data += sz;
	if (precvframe->u.hdr.rx_data > precvframe->u.hdr.rx_tail) {
		precvframe->u.hdr.rx_data -= sz;
		return NULL;
	}
	precvframe->u.hdr.len -= sz;
	return precvframe->u.hdr.rx_data;
}

static inline u8 *recvframe_put(union recv_frame *precvframe, sint sz)
{
	unsigned char *prev_rx_tail;

	if (precvframe == NULL)
		return NULL;
	prev_rx_tail = precvframe->u.hdr.rx_tail;
	precvframe->u.hdr.rx_tail += sz;
	if (precvframe->u.hdr.rx_tail > precvframe->u.hdr.rx_end) {
		precvframe->u.hdr.rx_tail -= sz;
		return NULL;
	}
	precvframe->u.hdr.len += sz;
	return precvframe->u.hdr.rx_tail;
}

static inline u8 *recvframe_pull_tail(union recv_frame *precvframe, sint sz)
{
	if (precvframe == NULL)
		return NULL;
	precvframe->u.hdr.rx_tail -= sz;
	if (precvframe->u.hdr.rx_tail < precvframe->u.hdr.rx_data) {
		precvframe->u.hdr.rx_tail += sz;
		return NULL;
	}
	precvframe->u.hdr.len -= sz;
	return precvframe->u.hdr.rx_tail;
}

static inline _buffer *get_rxbuf_desc(union recv_frame *precvframe)
{
	_buffer *buf_desc;
	if (precvframe == NULL)
		return NULL;
	return buf_desc;
}

static inline union recv_frame *rxmem_to_recvframe(u8 *rxmem)
{
	return (union recv_frame *)(((addr_t)rxmem >> RXFRAME_ALIGN) <<
				  RXFRAME_ALIGN);
}

static inline union recv_frame *pkt_to_recvframe(_pkt *pkt)
{
	u8 *buf_star;
	union recv_frame *precv_frame;

	precv_frame = rxmem_to_recvframe((unsigned char *)buf_star);
	return precv_frame;
}

static inline u8 *pkt_to_recvmem(_pkt *pkt)
{
	
	union recv_frame *precv_frame = pkt_to_recvframe(pkt);

	return	precv_frame->u.hdr.rx_head;
}

static inline u8 *pkt_to_recvdata(_pkt *pkt)
{
	
	union recv_frame *precv_frame = pkt_to_recvframe(pkt);

	return	precv_frame->u.hdr.rx_data;
}

static inline sint get_recvframe_len(union recv_frame *precvframe)
{
	return precvframe->u.hdr.len;
}

struct sta_info;

void	_r8712_init_sta_recv_priv(struct sta_recv_priv *psta_recvpriv);
sint r8712_recvframe_chkmic(struct _adapter *adapter,
			    union recv_frame *precvframe);
union recv_frame *r8712_decryptor(struct _adapter *adapter,
				  union recv_frame *precv_frame);
union recv_frame *r8712_recvframe_chk_defrag(struct _adapter *adapter,
					     union recv_frame *precv_frame);
union recv_frame *r8712_recvframe_defrag(struct _adapter *adapter,
					 struct  __queue *defrag_q);
union recv_frame *r8712_recvframe_chk_defrag_new(struct _adapter *adapter,
					union recv_frame *precv_frame);
union recv_frame *r8712_recvframe_defrag_new(struct _adapter *adapter,
					struct  __queue *defrag_q,
					union recv_frame *precv_frame);
int r8712_recv_decache(union recv_frame *precv_frame, u8 bretry,
		       struct stainfo_rxcache *prxcache);
int r8712_sta2sta_data_frame(struct _adapter *adapter,
			     union recv_frame *precv_frame,
			     struct sta_info **psta);
int r8712_ap2sta_data_frame(struct _adapter *adapter,
			    union recv_frame *precv_frame,
			    struct sta_info **psta);
int r8712_sta2ap_data_frame(struct _adapter *adapter,
			    union recv_frame *precv_frame,
			    struct sta_info **psta);
int r8712_validate_recv_ctrl_frame(struct _adapter *adapter,
				   union recv_frame *precv_frame);
int r8712_validate_recv_mgnt_frame(struct _adapter *adapter,
				   union recv_frame *precv_frame);
int r8712_validate_recv_data_frame(struct _adapter *adapter,
				   union recv_frame *precv_frame);
int r8712_validate_recv_frame(struct _adapter *adapter,
			      union recv_frame *precv_frame);
union recv_frame *r8712_portctrl(struct _adapter *adapter,
				 union recv_frame *precv_frame);
void  r8712_mgt_dispatcher(struct _adapter *padapter, u8 *pframe, uint len);
int r8712_amsdu_to_msdu(struct _adapter *padapter, union recv_frame *prframe);

#endif

