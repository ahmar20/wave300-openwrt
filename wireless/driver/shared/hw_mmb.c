/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"

#define SAFE_PLACE_TO_DEFINE_CHIP_INFO
#include "hw_mmb.h"

#include "mtlk_df.h"
#include "mtlkmsg.h"

#include "shram.h"
#include "mtlklist.h"

#include "mtlk_dfg.h"
#include "mtlk_df_nbuf.h"
#include "mtlk_df_fw.h"
#include "mtlk_gpl_helper.h"
#include "mtlk_vap_manager.h"
#include "mtlkhal.h"
#include "mtlk_snprintf.h"
#include "mtlkwlanirbdefs.h"
#include "mtlkwssa_drvinfo.h"
#include "mtlk_wssd.h"
#include "mtlk_bist.h"
#include "fw_recovery.h"
#include "drvver.h"

#include "mmb_ops.h"

#define LOG_LOCAL_GID   GID_HWMMB
#define LOG_LOCAL_FID   1

#define MTLK_FRMW_LOAD_CHUNK_TIMEOUT   2000 /* ms */
#define MTLK_MAC_BOOT_TIMEOUT          2000 /* ms */
#define MTLK_CHI_MAGIC_TIMEOUT         2000 /* ms */
#define MTLK_READY_CFM_TIMEOUT         5000 /* ms */
#define MTLK_SW_RESET_CFM_TIMEOUT      5000 /* ms */
#define MTLK_PRGMDL_LOAD_CHUNK_TIMEOUT 5000 /* ms */
#define MTLK_RX_BUFFS_RECOVERY_PERIOD  5000 /* ms */

#define MTLK_MAX_RX_BUFFS_TO_RECOVER   ((uint16)-1) /* no limit */

#define RX_MAX_MSG_OFFSET       2    /* alignment offset from MAC. TODO: ???? */

#define HW_PCI_TXMM_MAX_MSGS 32
#define HW_PCI_TXDM_MAX_MSGS 2
#define HW_PCI_TXMM_GRANULARITY 1000
#define HW_PCI_TXDM_GRANULARITY 1000

#define HW_PCI_TXM_MAX_FAILS 5

#define DAT_CFM_ID_NONE      0xFF

#ifndef MTLK_RX_BUFF_ALIGNMENT
#define MTLK_RX_BUFF_ALIGNMENT 0     /* No special alignment required */
#endif

#ifndef HIBYTE
#define HIBYTE(s) ((uint8)((uint16)(s) >> 8))
#endif
#ifndef LOBYTE
#define LOBYTE(s) ((uint8)(s))
#endif

#define bswap_32(x) __swab32(x)

#define MTLK_HW_MAX_RX_DATA_QUEUES MAX_RX_DATA_QUEUES

#define STRING_IF_VERSIONS_SIZE 512
#define STRING_PROGMODEL_SIZE   128

typedef struct
{
  uint8  percentage;  /* percent */
  uint8  min_buffers; /* nof     */
  uint16 data_size;   /* bytes   */
} mtlk_hw_rx_queue_cfg_t;

typedef struct
{
  uint32                 nof_queues_enabled;
  mtlk_hw_rx_queue_cfg_t queue[MTLK_HW_MAX_RX_DATA_QUEUES];
} mtlk_hw_rx_queues_cfg_t;

static mtlk_hw_rx_queue_cfg_t default_rx_queues_cfg[] = {
  /* %%,  nof, bytes - MUST BE ARRANGED BY BUFFER SIZE (increasing) */
  {  40,  10,   100 },
  {  40,  10,  1600 },
  {  20,   2,  4082 /* 4096 (page size) - 14 (overhead size) */ }
};

typedef struct
{
  uint16 max_que_size;  /* Maximum queue size */
  uint16 que_size;      /* Current queue size */
  uint16 buf_size;      /* Queue buffer size  */
} mtlk_hw_rx_queue_t;

typedef struct
{
  uint16             nof_in_use; /* Number of queues in use */
  mtlk_hw_rx_queue_t queue[MTLK_HW_MAX_RX_DATA_QUEUES];
} mtlk_hw_rx_queues_t;


/*****************************************************
 * IND/REQ BD-related definitions
 *****************************************************/
typedef struct
{
  uint32 offset; /* BD offset (PAS) */
  uint16 size;   /* BD size         */
  uint16 idx;    /* BD access index */
} mtlk_hw_bd_t;

typedef struct
{
  mtlk_hw_bd_t ind;
  mtlk_hw_bd_t req;
} mtlk_hw_ind_req_bd_t;
/*****************************************************/

typedef struct
{
  uint8              index;      /* index in mirror array */
  mtlk_dlist_entry_t list_entry; /* for mirror elements list */
} mtlk_hw_mirror_hdr_t;

/*****************************************************
 * Data Tx-related definitions
 *****************************************************/
typedef struct
{
  mtlk_hw_mirror_hdr_t hdr;        /* Header */
  mtlk_nbuf_t         *nbuf;       /* Network buffer   */
  uint32               dma_addr;   /* DMA mapped address */
  uint32               size;       /* Data buffer size */
  uint32               sta_id;     /* Station ID */
  uint8                ac;         /* Packet's access category */
  mtlk_osal_timestamp_t ts;        /* Timestamp (used for TX monitoring) */
  mtlk_vap_handle_t    vap_handle;
} mtlk_hw_data_req_mirror_t;
/*****************************************************/

/*****************************************************
 * Data Rx-related definitions
 *****************************************************/
typedef struct
{
  mtlk_hw_mirror_hdr_t hdr;     /* Header */
  mtlk_nbuf_t         *nbuf;    /* Network buffer   */
  uint32               dma_addr;/* DMA mapped address */
  uint32               size;    /* Data buffer size */
  uint8                que_idx; /* Rx Queue Index */
  mtlk_lslist_entry_t  pend_l;  /* Pending list entry */
} mtlk_hw_data_ind_mirror_t;
/*****************************************************/

/*****************************************************
 * Logger-related definitions
 *****************************************************/
typedef struct
{
  mtlk_hw_mirror_hdr_t hdr;         /* Header */
  void*                virt_addr;   /* data buffer virtual address    */
  uint32               dma_addr;    /* data buffer DMA mapped address */
  uint16               bufsize;
} mtlk_hw_log_ind_mirror_t;
/*****************************************************/

/*****************************************************
 * Control Messages (CM = MM and DM) Tx-related definitions
 *****************************************************/
typedef struct _mtlk_hw_cm_req_obj_t
{
  mtlk_hw_mirror_hdr_t hdr;        /* Header */
#ifdef MTCFG_DEBUG
  mtlk_atomic_t        usage_cnt;  /* message usage counter */
#endif
  UMI_MSG_HEADER       msg_hdr;
} mtlk_hw_cm_req_mirror_t;
/*****************************************************/

/*****************************************************
 * Control Messages (CM = MM and DM) Rx-related definitions
 * NOTE: msg member must be 1st in these structures because
 *       it is used for copying messages to/from PAS and
 *       buffers that are used for copying to/from PAS must
 *       be aligned to 32 bits boundary (see 
 *       _mtlk_mmb_memcpy...() functions)
 *****************************************************/
typedef struct
{
  mtlk_hw_mirror_hdr_t hdr;
  UMI_MSG_HEADER       msg_hdr;
} mtlk_hw_cm_ind_mirror_t;
/*****************************************************/

/**************************************************************/
/* TODO: DEV_DF made external for init in DFG will be fixed */
mtlk_hw_mmb_t mtlk_mmb_obj;

/*****************************************************
 * Auxiliary BD ring-related definitions
 *****************************************************/
/********************************************************************
 * Number of BD descriptors
 * PAS offset of BD array (ring)
 * Local BD mirror (array)
*********************************************************************/
typedef struct {
  uint8   nof_bds;
  void   *iom_bdr_pos;
  uint16  iom_bd_size;
  void   *hst_bdr_mirror;
  uint16  hst_bd_size;
  MTLK_DECLARE_INIT_STATUS;
} mtlk_mmb_basic_bdr_t;

MTLK_INIT_STEPS_LIST_BEGIN(mmb_basic_bdr)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_basic_bdr, ALLOC_HST_BDR_MIRROR)
MTLK_INIT_INNER_STEPS_BEGIN(mmb_basic_bdr)
MTLK_INIT_STEPS_LIST_END(mmb_basic_bdr);

static mtlk_hw_msg_t* _mtlk_hw_get_msg_to_send(mtlk_vap_handle_t vap_handle, uint32* nof_free_tx_msgs);
static int            _mtlk_hw_send_data(mtlk_vap_handle_t vap_handle, const mtlk_hw_send_data_t *data);
static int            _mtlk_hw_release_msg_to_send(mtlk_vap_handle_t vap_handle,mtlk_hw_msg_t *msg);
static int            _mtlk_hw_set_prop(mtlk_vap_handle_t vap_handle, mtlk_hw_prop_e prop_id, void *buffer, uint32 size);
static int            _mtlk_hw_get_prop(mtlk_vap_handle_t vap_handle, mtlk_hw_prop_e prop_id, void *buffer, uint32 size);
static void*          _mtlk_hw_get_device(mtlk_vap_handle_t vap_handle);

static int            _mtlk_mmb_init_fw_logger(mtlk_hw_t *hw);

static __INLINE mtlk_hw_mirror_hdr_t *
_mtlk_basic_bdr_get_mirror_bd_hdr (mtlk_mmb_basic_bdr_t *bbdr, uint8 index)
{
  MTLK_ASSERT(bbdr != NULL);
  MTLK_ASSERT(bbdr->nof_bds >= index);

  return (mtlk_hw_mirror_hdr_t *)&((uint8 *)bbdr->hst_bdr_mirror)[index * bbdr->hst_bd_size];
}
 
static __INLINE void*
__mtlk_basic_bdr_get_mirror_bd_safe (mtlk_mmb_basic_bdr_t *bbdr, uint8 index)
{
  MTLK_ASSERT(bbdr != NULL);
  MTLK_ASSERT(bbdr->nof_bds >= index);

  return &((uint8 *)bbdr->hst_bdr_mirror)[index * bbdr->hst_bd_size];
}

#define _mtlk_basic_bdr_get_mirror_bd(bbdr, index, type) \
  ((type *)__mtlk_basic_bdr_get_mirror_bd_safe((bbdr), (index)))

static __INLINE void *
__mtlk_basic_bdr_get_iom_bd_safe (mtlk_mmb_basic_bdr_t *bbdr, uint8 index, uint32 expected_iom_bd_size)
{
  MTLK_ASSERT(bbdr != NULL);
  MTLK_ASSERT(bbdr->nof_bds >= index);
  MTLK_ASSERT(bbdr->iom_bd_size == expected_iom_bd_size);

  return &((uint8 *)bbdr->iom_bdr_pos)[index * bbdr->iom_bd_size];
}

#define _mtlk_basic_bdr_get_iom_bd(bbdr, index, type) \
  __mtlk_basic_bdr_get_iom_bd_safe((bbdr), (index), sizeof(type))

static __INLINE BOOL
_mtlk_basic_bdr_contains_hst_bd (mtlk_mmb_basic_bdr_t *bbdr, const void *hst_bd)
{
  MTLK_ASSERT(bbdr != NULL);

  return (bbdr->hst_bdr_mirror <= hst_bd && 
          (bbdr->hst_bdr_mirror + bbdr->nof_bds * bbdr->hst_bd_size) > hst_bd);
}

static void
_mtlk_basic_bdr_cleanup (mtlk_mmb_basic_bdr_t *bbdr)
{
  MTLK_CLEANUP_BEGIN(mmb_basic_bdr, MTLK_OBJ_PTR(bbdr))
    MTLK_CLEANUP_STEP(mmb_basic_bdr, ALLOC_HST_BDR_MIRROR, MTLK_OBJ_PTR(bbdr),
                      mtlk_osal_mem_free, (bbdr->hst_bdr_mirror));
  MTLK_CLEANUP_END(mmb_basic_bdr, MTLK_OBJ_PTR(bbdr))
}

static int
_mtlk_basic_bdr_init (mtlk_mmb_basic_bdr_t *bbdr,
                      uint8                 nof_bds,
                      uint8                *iom_bdr_pos,
                      uint16                iom_bd_size,
                      uint16                hst_bd_size)
{
  uint8 i;

  MTLK_ASSERT(bbdr != NULL);
  MTLK_ASSERT(nof_bds != 0);
  MTLK_ASSERT(iom_bdr_pos != 0);
  MTLK_ASSERT(iom_bd_size != 0);
  MTLK_ASSERT(hst_bd_size != 0);

  MTLK_INIT_TRY(mmb_basic_bdr, MTLK_OBJ_PTR(bbdr))
    MTLK_INIT_STEP_EX(mmb_basic_bdr, ALLOC_HST_BDR_MIRROR, MTLK_OBJ_PTR(bbdr),
                      mtlk_osal_mem_alloc, (nof_bds * hst_bd_size, MTLK_MEM_TAG_HW),
                      bbdr->hst_bdr_mirror, bbdr->hst_bdr_mirror != NULL, MTLK_ERR_NO_MEM);
    
    memset(bbdr->hst_bdr_mirror, 0, nof_bds * hst_bd_size);
    bbdr->nof_bds     = nof_bds;
    bbdr->iom_bdr_pos = iom_bdr_pos;
    bbdr->iom_bd_size = iom_bd_size;
    bbdr->hst_bd_size = hst_bd_size;
    for (i = 0; i < nof_bds; i++) {
      mtlk_hw_mirror_hdr_t *hdr = _mtlk_basic_bdr_get_mirror_bd_hdr(bbdr, i);
      hdr->index = i;
    }
  MTLK_INIT_FINALLY(mmb_basic_bdr, MTLK_OBJ_PTR(bbdr))
  MTLK_INIT_RETURN(mmb_basic_bdr, MTLK_OBJ_PTR(bbdr), _mtlk_basic_bdr_cleanup, (bbdr));
}

typedef struct {
  mtlk_mmb_basic_bdr_t basic;
  mtlk_dlist_t         free_list;
  mtlk_dlist_t         used_list;
  mtlk_osal_spinlock_t lock;
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_INIT_LOOP(REG_FREE);
} mtlk_mmb_advanced_bdr_t;

MTLK_INIT_STEPS_LIST_BEGIN(mmb_advanced_bdr)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_advanced_bdr, BASIC_BDR)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_advanced_bdr, LIST_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_advanced_bdr, FREE_LIST)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_advanced_bdr, REG_FREE)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_advanced_bdr, USED_LIST)
MTLK_INIT_INNER_STEPS_BEGIN(mmb_advanced_bdr)
MTLK_INIT_STEPS_LIST_END(mmb_advanced_bdr);

static void
_mtlk_advanced_bdr_cleanup (mtlk_mmb_advanced_bdr_t *abdr)
{
  int i;
  MTLK_CLEANUP_BEGIN(mmb_advanced_bdr, MTLK_OBJ_PTR(abdr))
    MTLK_CLEANUP_STEP(mmb_advanced_bdr, USED_LIST, MTLK_OBJ_PTR(abdr),
                      mtlk_dlist_cleanup, (&abdr->used_list));
    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(abdr), REG_FREE) > 0; i++) {
      /* Empty list to prevent ASSERT on cleanup */
      MTLK_CLEANUP_STEP_LOOP(mmb_advanced_bdr, REG_FREE, MTLK_OBJ_PTR(abdr),
                             mtlk_dlist_pop_front, (&abdr->free_list));
    }
    MTLK_CLEANUP_STEP(mmb_advanced_bdr, FREE_LIST, MTLK_OBJ_PTR(abdr),
                      mtlk_dlist_cleanup, (&abdr->free_list));
    MTLK_CLEANUP_STEP(mmb_advanced_bdr, LIST_LOCK, MTLK_OBJ_PTR(abdr),
                      mtlk_osal_lock_cleanup, (&abdr->lock));
    MTLK_CLEANUP_STEP(mmb_advanced_bdr, BASIC_BDR, MTLK_OBJ_PTR(abdr),
                      _mtlk_basic_bdr_cleanup, (&abdr->basic));
  MTLK_CLEANUP_END(mmb_advanced_bdr, MTLK_OBJ_PTR(abdr))
}

static int
_mtlk_advanced_bdr_init (mtlk_mmb_advanced_bdr_t *abdr,
                         uint8                    nof_bds,
                         uint8                   *iom_bdr_pos,
                         uint16                   iom_bd_size,
                         uint16                   hst_bd_size)
{
  uint8 i = 0;

  MTLK_ASSERT(abdr != NULL);

  MTLK_INIT_TRY(mmb_advanced_bdr, MTLK_OBJ_PTR(abdr))
    MTLK_INIT_STEP(mmb_advanced_bdr, BASIC_BDR, MTLK_OBJ_PTR(abdr),
                   _mtlk_basic_bdr_init, (&abdr->basic, nof_bds, iom_bdr_pos, iom_bd_size, hst_bd_size))
    MTLK_INIT_STEP(mmb_advanced_bdr, LIST_LOCK, MTLK_OBJ_PTR(abdr),
                   mtlk_osal_lock_init, (&abdr->lock));
    MTLK_INIT_STEP_VOID(mmb_advanced_bdr, FREE_LIST, MTLK_OBJ_PTR(abdr),
                        mtlk_dlist_init, (&abdr->free_list));
    for (i = 0; i < abdr->basic.nof_bds; i++) {
      mtlk_hw_mirror_hdr_t *hdr = _mtlk_basic_bdr_get_mirror_bd_hdr(&abdr->basic, i);
      MTLK_INIT_STEP_VOID_LOOP(mmb_advanced_bdr, REG_FREE, MTLK_OBJ_PTR(abdr),
                               mtlk_dlist_push_back, (&abdr->free_list, &hdr->list_entry));
   }
    MTLK_INIT_STEP_VOID(mmb_advanced_bdr, USED_LIST, MTLK_OBJ_PTR(abdr),
                        mtlk_dlist_init, (&abdr->used_list));
  MTLK_INIT_FINALLY(mmb_advanced_bdr, MTLK_OBJ_PTR(abdr))
  MTLK_INIT_RETURN(mmb_advanced_bdr, MTLK_OBJ_PTR(abdr), _mtlk_advanced_bdr_cleanup, (abdr));
}
/*****************************************************/

typedef struct
{
  mtlk_lslist_t     lbufs; /* Rx Data Buffers to be re-allocated */
  mtlk_osal_timer_t timer; /* Recovery Timer */
} mtlk_hw_rx_pbufs_t; /* failed RX buffers allocations recovery */

typedef enum
{
  MTLK_ISR_NONE,
  MTLK_ISR_INIT_EVT,
  MTLK_ISR_MSGS_PUMP,
  MTLK_ISR_LAST
} mtlk_hw_mmb_isr_type_e;

typedef enum
{
  MTLK_HW_CNT_TX_PACKETS_DISCARDED_FW,
  MTLK_HW_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD,
  MTLK_HW_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE,
  MTLK_HW_CNT_PACKETS_SENT,
  MTLK_HW_CNT_PACKETS_RECEIVED,
  MTLK_HW_CNT_BYTES_RECEIVED,
  MTLK_HW_CNT_BYTES_SENT,
  MTLK_HW_CNT_802_1X_PACKETS_RECEIVED,
  MTLK_HW_CNT_802_1X_PACKETS_SENT,
  MTLK_HW_CNT_PAIRWISE_MIC_FAILURE_PACKETS,
  MTLK_HW_CNT_GROUP_MIC_FAILURE_PACKETS,
  MTLK_HW_CNT_UNICAST_REPLAYED_PACKETS,
  MTLK_HW_CNT_MULTICAST_REPLAYED_PACKETS,
  MTLK_HW_CNT_FWD_RX_PACKETS,
  MTLK_HW_CNT_FWD_RX_BYTES,
  MTLK_HW_CNT_UNICAST_PACKETS_SENT,
  MTLK_HW_CNT_UNICAST_PACKETS_RECEIVED,
  MTLK_HW_CNT_MULTICAST_PACKETS_SENT,
  MTLK_HW_CNT_MULTICAST_PACKETS_RECEIVED,
  MTLK_HW_CNT_BROADCAST_PACKETS_SENT,
  MTLK_HW_CNT_BROADCAST_PACKETS_RECEIVED,
  MTLK_HW_CNT_MULTICAST_BYTES_SENT,
  MTLK_HW_CNT_MULTICAST_BYTES_RECEIVED,
  MTLK_HW_CNT_BROADCAST_BYTES_SENT,
  MTLK_HW_CNT_BROADCAST_BYTES_RECEIVED,
  MTLK_HW_CNT_FW_LOGGER_PACKETS_PROCESSED,
  MTLK_HW_CNT_FW_LOGGER_PACKETS_DROPPED,
  MTLK_HW_CNT_DAT_FRAMES_RECEIVED,
  MTLK_HW_CNT_CTL_FRAMES_RECEIVED,
  MTLK_HW_CNT_MAN_FRAMES_RECEIVED,
  MTLK_HW_CNT_AGGR_ACTIVE,
  MTLK_HW_CNT_REORD_ACTIVE,
  MTLK_HW_CNT_ISRS_TOTAL,
  MTLK_HW_CNT_ISRS_FOREIGN,
  MTLK_HW_CNT_ISRS_NOT_PENDING,
  MTLK_HW_CNT_ISRS_HALTED,
  MTLK_HW_CNT_ISRS_INIT,
  MTLK_HW_CNT_ISRS_TO_DPC,
  MTLK_HW_CNT_ISRS_UNKNOWN,
  MTLK_HW_CNT_POST_ISR_DPCS,
  MTLK_HW_CNT_FW_MSGS_HANDLED,
  MTLK_HW_CNT_SQ_DPCS_SCHEDULED,
  MTLK_HW_CNT_SQ_DPCS_ARRIVED,
  MTLK_HW_CNT_RX_BUF_ALLOC_FAILED,
  MTLK_HW_CNT_RX_BUF_REALLOC_FAILED,
  MTLK_HW_CNT_RX_BUF_REALLOCATED,
  MTLK_HW_CNT_RX_ADDBA_REQ_RECEIVED,
  MTLK_HW_CNT_RX_ADDBA_RES_CFMD_FAIL,
  MTLK_HW_CNT_RX_ADDBA_RES_CFMD_SUCCESS,
  MTLK_HW_CNT_RX_ADDBA_RES_LOST,
  MTLK_HW_CNT_RX_ADDBA_RES_NEGATIVE_SENT,
  MTLK_HW_CNT_RX_ADDBA_RES_NOT_CFMD,
  MTLK_HW_CNT_RX_ADDBA_RES_POSITIVE_SENT,
  MTLK_HW_CNT_RX_ADDBA_RES_REACHED,
  MTLK_HW_CNT_RX_ADDBA_RES_RETRANSMISSIONS,
  MTLK_HW_CNT_RX_BAR_WITHOUT_REORDERING,
  MTLK_HW_CNT_RX_AGGR_PKT_WITHOUT_REORDERING,
  MTLK_HW_CNT_RX_DELBA_REQ_CFMD_FAIL,
  MTLK_HW_CNT_RX_DELBA_REQ_CFMD_SUCCESS,
  MTLK_HW_CNT_RX_DELBA_REQ_LOST,
  MTLK_HW_CNT_RX_DELBA_REQ_NOT_CFMD,
  MTLK_HW_CNT_RX_DELBA_REQ_RCV,
  MTLK_HW_CNT_RX_DELBA_REQ_REACHED,
  MTLK_HW_CNT_RX_DELBA_REQ_RETRANSMISSIONS,
  MTLK_HW_CNT_RX_DELBA_REQ_SENT,
  MTLK_HW_CNT_RX_DELBA_SENT_BY_TIMEOUT,
  MTLK_HW_CNT_TX_ACK_ON_BAR_DETECTED,
  MTLK_HW_CNT_TX_ADDBA_REQ_CFMD_FAIL,
  MTLK_HW_CNT_TX_ADDBA_REQ_CFMD_SUCCESS,
  MTLK_HW_CNT_TX_ADDBA_REQ_NOT_CFMD,
  MTLK_HW_CNT_TX_ADDBA_REQ_SENT,
  MTLK_HW_CNT_TX_ADDBA_RES_RCV_NEGATIVE,
  MTLK_HW_CNT_TX_ADDBA_RES_RCV_POSITIVE,
  MTLK_HW_CNT_TX_ADDBA_RES_TIMEOUT,
  MTLK_HW_CNT_TX_CLOSE_AGGR_CFMD_FAIL,
  MTLK_HW_CNT_TX_CLOSE_AGGR_CFMD_SUCCESS,
  MTLK_HW_CNT_TX_CLOSE_AGGR_NOT_CFMD,
  MTLK_HW_CNT_TX_CLOSE_AGGR_SENT,
  MTLK_HW_CNT_TX_DELBA_REQ_CFMD_FAIL,
  MTLK_HW_CNT_TX_DELBA_REQ_CFMD_SUCCESS,
  MTLK_HW_CNT_TX_DELBA_REQ_LOST,
  MTLK_HW_CNT_TX_DELBA_REQ_NOT_CFMD,
  MTLK_HW_CNT_TX_DELBA_REQ_RCV,
  MTLK_HW_CNT_TX_DELBA_REQ_REACHED,
  MTLK_HW_CNT_TX_DELBA_REQ_RETRANSMISSIONS,
  MTLK_HW_CNT_TX_DELBA_REQ_SENT,
  MTLK_HW_CNT_TX_OPEN_AGGR_CFMD_FAIL,
  MTLK_HW_CNT_TX_OPEN_AGGR_CFMD_SUCCESS,
  MTLK_HW_CNT_TX_OPEN_AGGR_NOT_CFMD,
  MTLK_HW_CNT_TX_OPEN_AGGR_SENT,
  MTLK_HW_CNT_NOF_FAST_RCVRY_PROCESSED,
  MTLK_HW_CNT_NOF_FULL_RCVRY_PROCESSED,
  MTLK_HW_CNT_NOF_FAST_RCVRY_FAILED,
  MTLK_HW_CNT_NOF_FULL_RCVRY_FAILED,
  MTLK_HW_CNT_LAST
} mtlk_hw_wss_cnt_id_e;

typedef enum
{
  MTLK_HW_SOURCE_CNT_FW_LOGGER_PACKETS_PROCESSED,
  MTLK_HW_SOURCE_CNT_FW_LOGGER_PACKETS_DROPPED,
  MTLK_HW_SOURCE_CNT_ISRS_TOTAL,
  MTLK_HW_SOURCE_CNT_ISRS_FOREIGN,
  MTLK_HW_SOURCE_CNT_ISRS_NOT_PENDING,
  MTLK_HW_SOURCE_CNT_ISRS_HALTED,
  MTLK_HW_SOURCE_CNT_ISRS_INIT,
  MTLK_HW_SOURCE_CNT_ISRS_TO_DPC,
  MTLK_HW_SOURCE_CNT_ISRS_UNKNOWN,
  MTLK_HW_SOURCE_CNT_POST_ISR_DPCS,
  MTLK_HW_SOURCE_CNT_FW_MSGS_HANDLED,
  MTLK_HW_SOURCE_CNT_RX_BUF_ALLOC_FAILED,
  MTLK_HW_SOURCE_CNT_RX_BUF_REALLOC_FAILED,
  MTLK_HW_SOURCE_CNT_RX_BUF_REALLOCATED,
  MTLK_HW_SOURCE_CNT_LAST
} mtlk_hw_source_wss_cnt_id_e;

/* Statistic ALLOWED flags */
#define MTLK_HW_SOURCE_CNT_FW_LOGGER_PACKETS_PROCESSED_ALLOWED      MTLK_WWSS_WLAN_STAT_ID_FW_LOGGER_PACKETS_PROCESSED_ALLOWED
#define MTLK_HW_SOURCE_CNT_FW_LOGGER_PACKETS_DROPPED_ALLOWED        MTLK_WWSS_WLAN_STAT_ID_FW_LOGGER_PACKETS_DROPPED_ALLOWED
#define MTLK_HW_SOURCE_CNT_ISRS_TOTAL_ALLOWED                       MTLK_WWSS_WLAN_STAT_ID_ISRS_TOTAL_ALLOWED
#define MTLK_HW_SOURCE_CNT_ISRS_FOREIGN_ALLOWED                     MTLK_WWSS_WLAN_STAT_ID_ISRS_FOREIGN_ALLOWED
#define MTLK_HW_SOURCE_CNT_ISRS_NOT_PENDING_ALLOWED                 MTLK_WWSS_WLAN_STAT_ID_ISRS_NOT_PENDING_ALLOWED
#define MTLK_HW_SOURCE_CNT_ISRS_HALTED_ALLOWED                      MTLK_WWSS_WLAN_STAT_ID_ISRS_HALTED_ALLOWED
#define MTLK_HW_SOURCE_CNT_ISRS_INIT_ALLOWED                        MTLK_WWSS_WLAN_STAT_ID_ISRS_INIT_ALLOWED
#define MTLK_HW_SOURCE_CNT_ISRS_TO_DPC_ALLOWED                      MTLK_WWSS_WLAN_STAT_ID_ISRS_TO_DPC_ALLOWED
#define MTLK_HW_SOURCE_CNT_ISRS_UNKNOWN_ALLOWED                     MTLK_WWSS_WLAN_STAT_ID_ISRS_UNKNOWN_ALLOWED
#define MTLK_HW_SOURCE_CNT_POST_ISR_DPCS_ALLOWED                    MTLK_WWSS_WLAN_STAT_ID_POST_ISR_DPCS_ALLOWED
#define MTLK_HW_SOURCE_CNT_FW_MSGS_HANDLED_ALLOWED                  MTLK_WWSS_WLAN_STAT_ID_FW_MSGS_HANDLED_ALLOWED
#define MTLK_HW_SOURCE_CNT_RX_BUF_ALLOC_FAILED_ALLOWED              MTLK_WWSS_WLAN_STAT_ID_RX_BUF_ALLOC_FAILED_ALLOWED
#define MTLK_HW_SOURCE_CNT_RX_BUF_REALLOC_FAILED_ALLOWED            MTLK_WWSS_WLAN_STAT_ID_RX_BUF_REALLOC_FAILED_ALLOWED
#define MTLK_HW_SOURCE_CNT_RX_BUF_REALLOCATED_ALLOWED               MTLK_WWSS_WLAN_STAT_ID_RX_BUF_REALLOCATED_ALLOWED

typedef struct
{
  VECTOR_AREA_CALIBR_EXTENSION_DATA ext_data;
  void                             *buffer;
  uint32                            dma_addr;
} mtlk_calibr_data_t;

typedef struct
{
  VECTOR_AREA_MIPS_CONTROL_EXTENSION_DATA ext_data;
} mtlk_mips_ctrl_data_t;

typedef struct
{
  VECTOR_AREA_LOGGER_EXTENSION_DATA ext_data;
  VECTOR_AREA_LOGGER_BUF_INFO_EXTENSION_DATA buf_info_ext_data;
  uint8 is_supported;
  uint8 is_supported_ex;
  mtlk_mmb_basic_bdr_t log_buffers; /* FW Logger Buffers Queue */
} mtlk_fw_logger_data_t;

typedef struct
{
  VECTOR_AREA_FW_CAPABILITIES_NUM_OF_STATIONS_EXTENSION_DATA nof_stas;
  VECTOR_AREA_FW_CAPABILITIES_NUM_OF_VAPS_EXTENSION_DATA     nof_vaps;
} mtlk_fw_capabilities_t;

#define HW_FLAGS_PREPARED_MAN_REQ_BDR     0x00000001
#define HW_FLAGS_PREPARED_MAN_IND_BDR     0x00000002
#define HW_FLAGS_PREPARED_DBG_REQ_BDR     0x00000004
#define HW_FLAGS_PREPARED_DBG_IND_BDR     0x00000008
#define HW_FLAGS_PREPARED_DATA_REQ_BDR    0x00000010
#define HW_FLAGS_PREPARED_DATA_IND_BDR    0x00000020
#define HW_FLAGS_PREPARED_LOG_IND_BDR     0x00000040
#define HW_FLAGS_PREPARED_RX_BUFFERS      0x00000080

struct _mtlk_hw_t 
{
  mtlk_ccr_t            *ccr;
  mtlk_hw_mmb_t         *mmb;
  unsigned char         *mmb_pas;
  uint8                 card_idx;

  char                  if_version[STRING_IF_VERSIONS_SIZE];
  char                  progmodel[STRING_PROGMODEL_SIZE];

  mtlk_eeprom_data_t*   ee_data; /* EEPROM parsed data */

  mtlk_vap_manager_t    *vap_manager;

  mtlk_hw_state_e        state;

  VECTOR_AREA_BASIC      chi_data; /* Can be removed? */
  VECTOR_AREA_INIT_DATA  chi_init; 
  mtlk_calibr_data_t     calibr;   /* Calibration Extension related */
  mtlk_mips_ctrl_data_t  mips_ctrl;/* MIPS Control Extension related */
  mtlk_fw_logger_data_t  fw_logger;/* FW Logger Extension related */
  mtlk_fw_capabilities_t fw_capabilities;

  mtlk_osal_spinlock_t   reg_lock;
  volatile int           init_evt; /* used during the initialization */
  mtlk_hw_mmb_isr_type_e isr_type;

  mtlk_hw_ind_req_bd_t   bds;     /* IND/REQ BD */

  mtlk_mmb_advanced_bdr_t tx_data; /* Tx Data related */
  uint16                  tx_data_nof_free_bds; /* Number of free REQ BD descriptors */
  uint16                  tx_data_max_used_bds; /* Maximal number of used REQ BD descriptors */

  mtlk_mmb_basic_bdr_t    rx_data; /* Rx Data related */
  mtlk_hw_rx_queues_t     rx_data_queues; /* Dynamic Rx Buffer Queues */
  mtlk_hw_rx_pbufs_t      rx_data_pending; /* Rx Data Buffers recovery */

  mtlk_mmb_advanced_bdr_t tx_man;  /* Tx MM related */
  mtlk_mmb_basic_bdr_t    rx_man;  /* Rx MM related */
  mtlk_mmb_advanced_bdr_t tx_dbg;  /* Tx DM related */
  mtlk_mmb_basic_bdr_t    rx_dbg;  /* Rx DM related */

  mtlk_txmm_t            master_txmm_mirror;
  mtlk_txmm_t            master_txdm_mirror;

  mtlk_txmm_base_t       txmm_base;
  mtlk_txmm_base_t       txdm_base;

  int                    mac_events_stopped; /* No INDs must be passed to Core except those needed to perform cleanup */
  int                    mac_events_stopped_completely; /* No INDs must be passed to Core at all*/
  BOOL                   mac_reset_logic_initialized;

  mtlk_irbd_t           *irbd;
  mtlk_wss_t            *wss;
  mtlk_wss_cntr_handle_t  *wss_hcntrs[MTLK_HW_SOURCE_CNT_LAST];
  mtlk_irbd_handle_t    *stat_irb_handle;

  uint32                flags;

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
};

static const uint32 _mtlk_hw_listener_wss_id_map[] =
{
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_FW,     /* MTLK_HW_CNT_TX_PACKETS_DISCARDED_FW */
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_TOO_OLD,  /* MTLK_HW_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD */
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_DUPLICATE,/* MTLK_HW_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE */
  MTLK_WWSS_WLAN_STAT_ID_PACKETS_SENT,                /* MTLK_HW_CNT_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_PACKETS_RECEIVED,            /* MTLK_HW_CNT_PACKETS_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_BYTES_RECEIVED,              /* MTLK_HW_CNT_BYTES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_BYTES_SENT,                   /* MTLK_HW_CNT_PACKETS_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_802_1X_PACKETS_RECEIVED,      /* MTLK_HW_CNT_802_1X_PACKETS_RECEIVED                                         */
  MTLK_WWSS_WLAN_STAT_ID_802_1X_PACKETS_SENT,          /* MTLK_HW_CNT_802_1X_PACKETS_SENT                                             */
  MTLK_WWSS_WLAN_STAT_ID_PAIRWISE_MIC_FAILURE_PACKETS,/* MTLK_HW_CNT_PAIRWISE_MIC_FAILURE_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_GROUP_MIC_FAILURE_PACKETS,   /* MTLK_HW_CNT_GROUP_MIC_FAILURE_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_REPLAYED_PACKETS,    /* MTLK_HW_CNT_UNICAST_REPLAYED_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_REPLAYED_PACKETS,  /* MTLK_HW_CNT_MULTICAST_REPLAYED_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_FWD_RX_PACKETS,              /* MTLK_HW_CNT_FWD_RX_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_FWD_RX_BYTES,                /* MTLK_HW_CNT_FWD_RX_BYTES */
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_SENT,        /* MTLK_HW_CNT_UNICAST_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_RECEIVED,    /* MTLK_HW_CNT_UNICAST_PACKETS_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_PACKETS_SENT,      /* MTLK_HW_CNT_MULTICAST_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_PACKETS_RECEIVED,  /* MTLK_HW_CNT_MULTICAST_PACKETS_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_PACKETS_SENT,      /* MTLK_HW_CNT_BROADCAST_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_PACKETS_RECEIVED,  /* MTLK_HW_CNT_BROADCAST_PACKETS_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_BYTES_SENT,        /* MTLK_HW_CNT_MULTICAST_BYTES_SENT */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_BYTES_RECEIVED,    /* MTLK_HW_CNT_MULTICAST_BYTES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_BYTES_SENT,        /* MTLK_HW_CNT_BROADCAST_BYTES_SENT */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_BYTES_RECEIVED,    /* MTLK_HW_CNT_BROADCAST_BYTES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_FW_LOGGER_PACKETS_PROCESSED, /* MTLK_HW_CNT_FW_LOGGER_PACKETS_PROCESSED */
  MTLK_WWSS_WLAN_STAT_ID_FW_LOGGER_PACKETS_DROPPED,   /* MTLK_HW_CNT_FW_LOGGER_PACKETS_DROPPED */
  MTLK_WWSS_WLAN_STAT_ID_DAT_FRAMES_RECEIVED,         /* MTLK_HW_CNT_DAT_FRAMES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_CTL_FRAMES_RECEIVED,         /* MTLK_HW_CNT_CTL_FRAMES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_MAN_FRAMES_RECEIVED,         /* MTLK_HW_CNT_MAN_FRAMES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_AGGR_ACTIVE,                 /* MTLK_HW_CNT_AGGR_ACTIVE  */
  MTLK_WWSS_WLAN_STAT_ID_REORD_ACTIVE,                /* MTLK_HW_CNT_REORD_ACTIVE */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_TOTAL,                  /* MTLK_HW_CNT_ISRS_TOTAL */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_FOREIGN,                /* MTLK_HW_CNT_ISRS_FOREIGN */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_NOT_PENDING,            /* MTLK_HW_CNT_ISRS_NOT_PENDING */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_HALTED,                 /* MTLK_HW_CNT_ISRS_HALTED */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_INIT,                   /* MTLK_HW_CNT_ISRS_INIT */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_TO_DPC,                 /* MTLK_HW_CNT_ISRS_TO_DPC */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_UNKNOWN,                /* MTLK_HW_CNT_ISRS_UNKNOWN */
  MTLK_WWSS_WLAN_STAT_ID_POST_ISR_DPCS,               /* MTLK_HW_CNT_POST_ISR_DPCS */
  MTLK_WWSS_WLAN_STAT_ID_FW_MSGS_HANDLED,             /* MTLK_HW_CNT_FW_MSGS_HANDLED */
  MTLK_WWSS_WLAN_STAT_ID_SQ_DPCS_SCHEDULED,           /* MTLK_HW_CNT_SQ_DPCS_SCHEDULED */
  MTLK_WWSS_WLAN_STAT_ID_SQ_DPCS_ARRIVED,             /* MTLK_HW_CNT_SQ_DPCS_ARRIVED */
  MTLK_WWSS_WLAN_STAT_ID_RX_BUF_ALLOC_FAILED,         /* MTLK_HW_CNT_RX_BUF_ALLOC_FAILED */
  MTLK_WWSS_WLAN_STAT_ID_RX_BUF_REALLOC_FAILED,       /* MTLK_HW_CNT_RX_BUF_REALLOC_FAILED */
  MTLK_WWSS_WLAN_STAT_ID_RX_BUF_REALLOCATED,          /* MTLK_HW_CNT_RX_BUF_REALLOCATED */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_REQ_RECEIVED,       /* MTLK_HW_CNT_RX_ADDBA_REQ_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_FAIL,      /* MTLK_HW_CNT_RX_ADDBA_RES_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_SUCCESS,   /* MTLK_HW_CNT_RX_ADDBA_RES_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_LOST,           /* MTLK_HW_CNT_RX_ADDBA_RES_LOST */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NEGATIVE_SENT,  /* MTLK_HW_CNT_RX_ADDBA_RES_NEGATIVE_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NOT_CFMD,       /* MTLK_HW_CNT_RX_ADDBA_RES_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_POSITIVE_SENT,  /* MTLK_HW_CNT_RX_ADDBA_RES_POSITIVE_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_REACHED,        /* MTLK_HW_CNT_RX_ADDBA_RES_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_RETRANSMISSIONS,/* MTLK_HW_CNT_RX_ADDBA_RES_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_RX_BAR_WITHOUT_REORDERING,   /* MTLK_HW_CNT_RX_BAR_WITHOUT_REORDERING */
  MTLK_WWSS_WLAN_STAT_ID_RX_AGGR_PKT_WITHOUT_REORDERING, /* MTLK_HW_CNT_RX_AGGR_PKT_WITHOUT_REORDERING */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_FAIL,      /* MTLK_HW_CNT_RX_DELBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_SUCCESS,   /* MTLK_HW_CNT_RX_DELBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_LOST,           /* MTLK_HW_CNT_RX_DELBA_REQ_LOST */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_NOT_CFMD,       /* MTLK_HW_CNT_RX_DELBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RCV,            /* MTLK_HW_CNT_RX_DELBA_REQ_RCV */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_REACHED,        /* MTLK_HW_CNT_RX_DELBA_REQ_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RETRANSMISSIONS,/* MTLK_HW_CNT_RX_DELBA_REQ_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_SENT,           /* MTLK_HW_CNT_RX_DELBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_SENT_BY_TIMEOUT,    /* MTLK_HW_CNT_RX_DELBA_SENT_BY_TIMEOUT */
  MTLK_WWSS_WLAN_STAT_ID_TX_ACK_ON_BAR_DETECTED,      /* MTLK_HW_CNT_TX_ACK_ON_BAR_DETECTED */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_FAIL,      /* MTLK_HW_CNT_TX_ADDBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_SUCCESS,   /* MTLK_HW_CNT_TX_ADDBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_NOT_CFMD,       /* MTLK_HW_CNT_TX_ADDBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_SENT,           /* MTLK_HW_CNT_TX_ADDBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_NEGATIVE,   /* MTLK_HW_CNT_TX_ADDBA_RES_RCV_NEGATIVE */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_POSITIVE,   /* MTLK_HW_CNT_TX_ADDBA_RES_RCV_POSITIVE */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_TIMEOUT,        /* MTLK_HW_CNT_TX_ADDBA_RES_TIMEOUT */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_FAIL,     /* MTLK_HW_CNT_TX_CLOSE_AGGR_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_SUCCESS,  /* MTLK_HW_CNT_TX_CLOSE_AGGR_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_NOT_CFMD,      /* MTLK_HW_CNT_TX_CLOSE_AGGR_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_SENT,          /* MTLK_HW_CNT_TX_CLOSE_AGGR_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_FAIL,      /* MTLK_HW_CNT_TX_DELBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_SUCCESS,   /* MTLK_HW_CNT_TX_DELBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_LOST,           /* MTLK_HW_CNT_TX_DELBA_REQ_LOST */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_NOT_CFMD,       /* MTLK_HW_CNT_TX_DELBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RCV,            /* MTLK_HW_CNT_TX_DELBA_REQ_RCV */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_REACHED,        /* MTLK_HW_CNT_TX_DELBA_REQ_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RETRANSMISSIONS,/* MTLK_HW_CNT_TX_DELBA_REQ_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_SENT,           /* MTLK_HW_CNT_TX_DELBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_FAIL,      /* MTLK_HW_CNT_TX_OPEN_AGGR_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_SUCCESS,   /* MTLK_HW_CNT_TX_OPEN_AGGR_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_NOT_CFMD,       /* MTLK_HW_CNT_TX_OPEN_AGGR_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_SENT,           /* MTLK_HW_CNT_TX_OPEN_AGGR_SENT */
  MTLK_WWSS_WLAN_STAT_ID_NOF_FAST_RCVRY_PROCESSED,    /* MTLK_HW_CNT_NOF_FAST_RCVRY_PROCESSED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_FULL_RCVRY_PROCESSED,    /* MTLK_HW_CNT_NOF_FULL_RCVRY_PROCESSED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_FAST_RCVRY_FAILED,       /* MTLK_HW_CNT_NOF_FAST_RCVRY_FAILED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_FULL_RCVRY_FAILED        /* MTLK_HW_CNT_NOF_FULL_RCVRY_FAILED */
};

static const uint32 _mtlk_hw_source_wss_id_map[] =
{
  MTLK_WWSS_WLAN_STAT_ID_FW_LOGGER_PACKETS_PROCESSED, /* MTLK_HW_SOURCE_CNT_FW_LOGGER_PACKETS_PROCESSED */
  MTLK_WWSS_WLAN_STAT_ID_FW_LOGGER_PACKETS_DROPPED,   /* MTLK_HW_SOURCE_CNT_FW_LOGGER_PACKETS_DROPPED */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_TOTAL,                  /* MTLK_HW_SOURCE_CNT_ISRS_TOTAL */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_FOREIGN,                /* MTLK_HW_SOURCE_CNT_ISRS_FOREIGN */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_NOT_PENDING,            /* MTLK_HW_SOURCE_CNT_ISRS_NOT_PENDING */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_HALTED,                 /* MTLK_HW_SOURCE_CNT_ISRS_HALTED */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_INIT,                   /* MTLK_HW_SOURCE_CNT_ISRS_INIT */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_TO_DPC,                 /* MTLK_HW_SOURCE_CNT_ISRS_TO_DPC */
  MTLK_WWSS_WLAN_STAT_ID_ISRS_UNKNOWN,                /* MTLK_HW_SOURCE_CNT_ISRS_UNKNOWN */
  MTLK_WWSS_WLAN_STAT_ID_POST_ISR_DPCS,               /* MTLK_HW_SOURCE_CNT_POST_ISR_DPCS */
  MTLK_WWSS_WLAN_STAT_ID_FW_MSGS_HANDLED,             /* MTLK_HW_SOURCE_CNT_FW_MSGS_HANDLED */
  MTLK_WWSS_WLAN_STAT_ID_RX_BUF_ALLOC_FAILED,         /* MTLK_HW_SOURCE_CNT_RX_BUF_ALLOC_FAILED */
  MTLK_WWSS_WLAN_STAT_ID_RX_BUF_REALLOC_FAILED,       /* MTLK_HW_SOURCE_CNT_RX_BUF_REALLOC_FAILED */
  MTLK_WWSS_WLAN_STAT_ID_RX_BUF_REALLOCATED,          /* MTLK_HW_SOURCE_CNT_RX_BUF_REALLOCATED */
};

/**********************************************************************
 **********************************************************************/
/* with checking ALLOWED */
#define _mtlk_mmb_hw_inc_cnt(hw, id)    { if (id##_ALLOWED) __mtlk_mmb_hw_inc_cnt(hw, id); }

static __INLINE void
__mtlk_mmb_hw_inc_cnt(mtlk_hw_t                  *hw,
                     mtlk_hw_source_wss_cnt_id_e cnt_id)
{
  MTLK_ASSERT(cnt_id >= 0 && cnt_id < MTLK_HW_SOURCE_CNT_LAST);

  mtlk_wss_cntr_inc(hw->wss_hcntrs[cnt_id]);
}

/**********************************************************************
 * INIT event: impemented as flag + sleep
 * NOTE: can't be OSAL event because of SET from ISR (OSAL limitation)
 **********************************************************************/
#define MTLK_HW_INIT_EVT_STEP_MS      20 /* INIT_EVT WAIT resolution */

static __INLINE int
MTLK_HW_INIT_EVT_INIT (mtlk_hw_t *hw)
{
  hw->init_evt = 0;
  return MTLK_ERR_OK;
}

static __INLINE void
MTLK_HW_INIT_EVT_SET (mtlk_hw_t *hw)
{
  hw->init_evt = 1;
}

static __INLINE void
MTLK_HW_INIT_EVT_RESET (mtlk_hw_t *hw)
{
  hw->init_evt = 0;
}

static __INLINE void
MTLK_HW_INIT_EVT_CLEANUP (mtlk_hw_t *hw)
{
}

static __INLINE int 
MTLK_HW_INIT_EVT_WAIT (mtlk_hw_t *hw, uint32 msec)
{
  int res = MTLK_ERR_UNKNOWN;

  while (1) {
    if (hw->init_evt) {
      res = MTLK_ERR_OK;
      break;
    }
    else if (msec < MTLK_HW_INIT_EVT_STEP_MS) {
      res = MTLK_ERR_TIMEOUT;
      break;
    }
    else {
      mtlk_osal_msleep(MTLK_HW_INIT_EVT_STEP_MS);
      msec -= MTLK_HW_INIT_EVT_STEP_MS;
    }
  }

  return res;
}
/**********************************************************************/

#define _mtlk_mmb_pas_writel(hw, comment, index, v)                 \
  for(;;) {                                                         \
    ILOG6_S("Write PAS: %s", comment);                    \
    mtlk_writel((v),(hw)->mmb_pas + (uint32)(index));     \
	break;															\
  }

#define _mtlk_mmb_pas_readl(hw, comment, index, v)                  \
  for(;;) {                                                         \
    ILOG6_S("Read PAS: %s", comment);                     \
    (v) = mtlk_readl((hw)->mmb_pas + (uint32)(index));    \
	break;														    \
  }

static __INLINE int
_mtlk_mmb_memcpy_fromio (void       *to,
                         const void *from,
                         uint32      count)
{
  if ((((unsigned long)to | (unsigned long)from | count) & 0x3) == 0) {
    while (count) {
      *((uint32 *)to) = mtlk_raw_readl(from);
      from   = ((uint8 *)from) + 4;
      to     = ((uint8 *)to) + 4;
      count -= 4;
    }
    return 1;
  }
  else {
    ELOG_PPD("Unaligned access (to=0x%p, from=0x%p, size=%d)",
          to, from, count);
    MTLK_ASSERT(FALSE);
    return 0;
  }
}

static __INLINE mtlk_vap_handle_t 
_mtlk_mmb_get_vap_handle_from_msg_info (mtlk_hw_t *hw, uint16 msg_info, uint8 *vap_id)
{
  mtlk_vap_handle_t vap_handle;
  int               res;
  *vap_id = (uint8)MTLK_BFIELD_GET(msg_info, IND_REQ_INFO_BSS_IDX);
  res = mtlk_vap_manager_get_vap_handle(hw->vap_manager, *vap_id, &vap_handle);
  
  MTLK_ASSERT(res == MTLK_ERR_OK);
  MTLK_UNREFERENCED_PARAM(res);

  return vap_handle;
}

static __INLINE uint16 
_mtlk_mmb_vap_handle_to_msg_info (mtlk_hw_t *hw, mtlk_vap_handle_t vap_handle)
{
  uint16 msg_info = 0;
  uint8  vap_index = mtlk_vap_get_id(vap_handle);
  MTLK_UNREFERENCED_PARAM(hw);

  MTLK_ASSERT(vap_index <= MTLK_BFIELD_VALUE(IND_REQ_INFO_BSS_IDX, -1, uint8));

  MTLK_BFIELD_SET(msg_info, IND_REQ_INFO_BSS_IDX, vap_index);
  return msg_info;
}


#define _mtlk_mmb_memcpy_toio(to, from, count) \
        _mtlk_mmb_memcpy_toio_swap(to, from, count, FALSE)

static __INLINE int
_mtlk_mmb_memcpy_toio_swap (void *to, const void *from, uint32 count, BOOL swap)
{
  MTLK_ASSERT(NULL != to);
  MTLK_ASSERT(NULL != from);

  if ((((unsigned long)to | (unsigned long)from | count) & 0x3) == 0) {
    while (count) {
      if (swap) {
        mtlk_raw_writel(bswap_32(*(uint32 *)from), to);
      } else {
        mtlk_raw_writel(*(uint32 *)from, to);
      }
      from   = ((uint8 *)from) + 4;
      to     = ((uint8 *)to) + 4;
      count -= 4;
    }
    return 1;
  }
  else {
    ELOG_PPD("Unaligned access (to=0x%p, from=0x%p, size=%d)",
          to, from, count);
    MTLK_ASSERT(FALSE);
    return 0;
  }
}

#ifdef MTCFG_FW_WRITE_VALIDATION

static __INLINE int
_mtlk_mmb_memcpy_validate_toio (void *to, const void *from, uint32 count, int swap)
{
  uint32 test_value;
  uint8 validation_errors = 0;

  ILOG0_DPP("Validating memory chunk write of size %d, from %p, to %p", count, from, to);

  if(_mtlk_mmb_memcpy_toio_swap(to, from, count, swap)) {
    while (count) {
      test_value = mtlk_raw_readl(to);
      if (swap) {
        test_value = bswap_32(test_value);
      }

      if(test_value != *(uint32 *)from) {
        ILOG0_PDD("Write validation error at %p: written %d, read %d", to, *(uint32 *)from, test_value);
        validation_errors++;
      }

      from   = ((uint8 *)from) + 4;
      to     = ((uint8 *)to) + 4;
      count -= 4;
    }

    ILOG0_D("Amount of write validation errors: %d", validation_errors);
    return 1;
  }

  return 0;
}

#endif /* MTCFG_FW_WRITE_VALIDATION */

static __INLINE int
_mtlk_mmb_memcpy_toio_no_pll (mtlk_hw_t  *hw,
                              void       *to,
                              const void *from,
                              uint32     count)
{
  if ((((unsigned long)to | (unsigned long)from | count) & 0x3) == 0) {
    while (count) {
      if (hw->mmb->cfg.no_pll_write_delay_us) {
        mtlk_udelay(hw->mmb->cfg.no_pll_write_delay_us);
      }
      mtlk_raw_writel(*(uint32 *)from, to);
      from   = ((uint8 *)from) + 4;
      to     = ((uint8 *)to) + 4;
      count -= 4;
    }
    return 1;
  }
  else {
    ELOG_PPD("Unaligned access (to=0x%p, from=0x%p, size=%d)",
          to, from, count);
    MTLK_ASSERT(FALSE);
    return 0;
  }
}

#define _mtlk_mmb_pas_get(hw, comment, index, ptr, n) \
  _mtlk_mmb_memcpy_fromio((ptr), (hw)->mmb_pas + (index), (n))
#define _mtlk_mmb_pas_put(hw, comment, index, ptr, n) \
  _mtlk_mmb_memcpy_toio((hw)->mmb_pas + (index), (ptr), (n))

/* currently not used; please note byte swapping parameter is off */
#ifdef MTCFG_FW_WRITE_VALIDATION
#define _mtlk_mmb_pas_put_validate(hw, comment, index, ptr, n) \
  _mtlk_mmb_memcpy_validate_toio((hw)->mmb_pas + (index), (ptr), (n), FALSE)
#endif

static int _mtlk_mmb_send_msg (mtlk_hw_t *hw, 
                               uint8      msg_type,
                               uint8      msg_index,
                               uint16     msg_info);

static void txmm_on_cfm(mtlk_hw_t *hw, PMSG_OBJ pmsg);
static void txdm_on_cfm(mtlk_hw_t *hw, PMSG_OBJ pmsg);

static void _mtlk_mmb_free_unconfirmed_tx_buffers(mtlk_hw_t *hw);

#define HW_MSG_PTR(msg)          ((mtlk_hw_msg_t *)(msg))
#define DATA_REQ_MIRROR_PTR(msg) ((mtlk_hw_data_req_mirror_t *)(msg))
#define MAN_IND_MIRROR_PTR(msg)  ((mtlk_hw_man_ind_mirror_t *)(msg))
#define MAN_DBG_MIRROR_PTR(msg)  ((mtlk_hw_dbg_ind_mirror_t *)(msg))

#if MTLK_RX_BUFF_ALIGNMENT
static __INLINE mtlk_nbuf_t *
_mtlk_mmb_nbuf_alloc (mtlk_hw_t *hw,
                      uint32     size)
{
  mtlk_nbuf_t *nbuf = mtlk_df_nbuf_alloc(hw->vap_manager, size);
  if (nbuf) {
    /* Align skbuffer if required by HW */
    unsigned long tail = ((unsigned long)mtlk_df_nbuf_get_virt_addr(nbuf)) & 
                         (MTLK_RX_BUFF_ALIGNMENT - 1);
    if (tail) {
      unsigned long nof_pad_bytes = MTLK_RX_BUFF_ALIGNMENT - tail;
      mtlk_df_nbuf_reserve(nbuf, nof_pad_bytes);
    }
  }
  
  return nbuf;
}
#else
#define _mtlk_mmb_nbuf_alloc(hw, size)  mtlk_df_nbuf_alloc((hw)->vap_manager, (size))
#endif
#define _mtlk_mmb_nbuf_free(hw, nbuf)   mtlk_df_nbuf_free((hw)->vap_manager, nbuf)

static __INLINE uint16
_mtlk_mmb_get_rx_res_info (uint16 que_idx)
{
  return (MTLK_BFIELD_VALUE(IND_REQ_INFO_BSS_IDX, 0, uint16) |
          MTLK_BFIELD_VALUE(RX_RES_BUF_QUE_IDX, que_idx, uint16));
}

static __INLINE uint16
_mtlk_mmb_get_log_ind_data_size (uint16 msg_info)
{
  return MTLK_BFIELD_GET(msg_info, LOG_IND_DATA_SIZE);
}

static __INLINE uint32
_mtlk_mmb_get_rx_ind_data_size (uint16 msg_info)
{
  return (uint32)MTLK_BFIELD_GET(msg_info, RX_IND_DATA_SIZE);
}

static __INLINE uint8
_mtlk_mmb_get_rx_ind_data_offset (uint16 msg_info)
{
  return MTLK_BFIELD_GET(msg_info, RX_IND_DATA_OFFSET)?2:0;
}

static __INLINE void 
_mtlk_mmb_set_rx_payload_addr (mtlk_hw_t *hw, 
                               uint16     ind_idx, 
                               uint32     dma_addr)
{
  RXDAT_IND_MSG_DESC bd;

  bd.u32HostPayloadAddr = HOST_TO_MAC32(dma_addr);
  
  /* Rx IND descriptor (DMA address) */
  _mtlk_mmb_memcpy_toio(_mtlk_basic_bdr_get_iom_bd(&hw->rx_data, ind_idx, RXDAT_IND_MSG_DESC),
                        &bd,
                        sizeof(bd));
}

static int
_mtlk_mmb_rxque_set_default_cfg (mtlk_hw_t *hw)
{
  int    res              = MTLK_ERR_UNKNOWN;
  int    i                = 0;
  uint16 total_percentage = 0;
  uint16 total_bds        = 0;

  memset(&hw->rx_data_queues, 0, sizeof(hw->rx_data_queues));
 
  for (; i < ARRAY_SIZE(default_rx_queues_cfg); i++) {
    mtlk_hw_rx_queue_t *queue = &hw->rx_data_queues.queue[i];

    if (i == ARRAY_SIZE(default_rx_queues_cfg) - 1) {
      /* Last queue - take all the rest (round percentage) */
      queue->max_que_size = (uint16)(hw->rx_data.nof_bds - total_bds);
    }
    else {
      /* Take by percentage */
      queue->max_que_size =
        (uint16)(hw->rx_data.nof_bds * default_rx_queues_cfg[i].percentage / 100);
    }

    queue->buf_size = default_rx_queues_cfg[i].data_size +
                      sizeof(MAC_RX_ADDITIONAL_INFO_T) + RX_MAX_MSG_OFFSET;

    hw->rx_data_queues.nof_in_use++;

    ILOG2_DD("Rx Queue#d: size = %d, buffer = %d",
             (int)queue->max_que_size,
             (int)queue->buf_size);

    total_percentage = total_percentage + default_rx_queues_cfg[i].percentage;
    total_bds        = total_bds + queue->max_que_size;
  }

  if (total_percentage > 100) {
    ELOG_D("Incorrect Rx Queues Percentage Table (total=%d)", 
          (int)total_percentage);
    res = MTLK_ERR_PARAMS;
  }
  else if (total_bds != hw->rx_data.nof_bds) {
    ELOG_DD("Incorrect Rx Queues total (%d!=%d)", 
          (int)total_bds,
          (int)hw->rx_data.nof_bds);
    res = MTLK_ERR_UNKNOWN;
  }
  else if (hw->rx_data_queues.nof_in_use) {
    res = MTLK_ERR_OK;
  }

  return res;
}

static void
get_firmware_version (mtlk_hw_t       *hw,
                      const char      *fname,
                      const char      *data,
                      unsigned long    size)
{
  static const char MAC_VERSION_SIGNATURE[] = "@@@ VERSION INFO @@@";
  const char *border = data + size;
  char *text = hw->if_version;
  char *print_position = NULL;
  int res, bytes_printed = 0, len;
  int bytes_left = STRING_IF_VERSIONS_SIZE;
  mtlk_vap_handle_t master_vap_handle;

  mtlk_osal_lock_acquire(&hw->reg_lock);
  data = mtlk_osal_str_memchr(data, '@', border - data);
  while (data) {
    if (strncmp(data, MAC_VERSION_SIGNATURE, ARRAY_SIZE(MAC_VERSION_SIGNATURE)-1) == 0) {
      len = strlen(text);
      print_position = text + len;
      bytes_left -= len;
      if(bytes_left <= 0)
      {
        break;
      }
      bytes_printed = mtlk_snprintf(print_position, bytes_left, "%s: %s\n", fname, data);
      text[STRING_IF_VERSIONS_SIZE - 1] = '\0';
      break;
    }
    data = mtlk_osal_str_memchr(data + 1, '@', border - data - 1);
  }
  mtlk_osal_lock_release(&hw->reg_lock);

  res = mtlk_vap_manager_get_master_vap(hw->vap_manager, &master_vap_handle);
  if (bytes_printed > 0 && res == MTLK_ERR_OK)
  {
    ILOG0_DS("CID-%04x: %s", mtlk_vap_get_oid(master_vap_handle), print_position);
  }
}

static int
_mtlk_mmb_notify_firmware(mtlk_hw_t *hw,
                          const char *fname,
                          const mtlk_df_fw_file_buf_t* fb)
{
  int res = MTLK_ERR_OK;
  mtlk_vap_handle_t master_vap_handle;
  mtlk_core_firmware_file_t fw_buffer;

  strcpy(fw_buffer.fname, fname);
  fw_buffer.fcontents = *fb;

  res = mtlk_vap_manager_get_master_vap(hw->vap_manager, &master_vap_handle);
  if (MTLK_ERR_OK == res)
  {
    get_firmware_version(hw,
                         fname,
                         fb->buffer,
                         fb->size);

    res = MTLK_ERR_OK;
  }

  return res;
}


static mtlk_hw_data_req_mirror_t *
_mtlk_mmb_get_msg_from_data_pool(mtlk_hw_t *hw, mtlk_vap_handle_t vap_handle)
{ 
  mtlk_hw_data_req_mirror_t *data_req = NULL;
  uint16 nof_used_bds;
  
  mtlk_osal_lock_acquire(&hw->tx_data.lock);
  if (mtlk_dlist_size(&hw->tx_data.free_list)) {
    mtlk_dlist_entry_t *node = mtlk_dlist_pop_front(&hw->tx_data.free_list);
    
    data_req = MTLK_LIST_GET_CONTAINING_RECORD(node, 
                                               mtlk_hw_data_req_mirror_t,
                                               hdr.list_entry);
    hw->tx_data_nof_free_bds--;

    nof_used_bds = hw->tx_data.basic.nof_bds - hw->tx_data_nof_free_bds;
    if (nof_used_bds > hw->tx_data_max_used_bds)
      hw->tx_data_max_used_bds = nof_used_bds;

    data_req->vap_handle = vap_handle;

    /* add to the "used" list */
    mtlk_dlist_push_back(&hw->tx_data.used_list, &data_req->hdr.list_entry);
  }
  mtlk_osal_lock_release(&hw->tx_data.lock);

  ILOG4_PD("got msg %p, %d free msgs", data_req, hw->tx_data_nof_free_bds);

  return data_req;
}

static int
_mtlk_mmb_free_sent_msg_to_data_pool(mtlk_hw_t                 *hw, 
                                     mtlk_hw_data_req_mirror_t *data_req)
{
  mtlk_osal_lock_acquire(&hw->tx_data.lock);
  /* remove from the "used" list */
  mtlk_dlist_remove(&hw->tx_data.used_list,
                    &data_req->hdr.list_entry);
  /* add to the "free" list */
  mtlk_dlist_push_back(&hw->tx_data.free_list,
                       &data_req->hdr.list_entry);
  hw->tx_data_nof_free_bds++;
  mtlk_osal_lock_release(&hw->tx_data.lock);

  ILOG4_DD("%d msg freed, %d free msgs", data_req->hdr.index, hw->tx_data_nof_free_bds);

  return hw->tx_data_nof_free_bds;
}

static __INLINE uint32
_mtlk_mmb_cm_bdr_get_iom_bd_size (mtlk_hw_t *hw,
                                  BOOL       is_man)
{
  return sizeof(UMI_MSG_HEADER) + (is_man?hw->mmb->cfg.man_msg_size:hw->mmb->cfg.dbg_msg_size);
}

static __INLINE mtlk_mmb_basic_bdr_t *
_mtlk_mmb_cm_get_ind_bbdr (mtlk_hw_t *hw,
                           BOOL       is_man)
{
  return is_man?&hw->rx_man:&hw->rx_dbg;
}

static __INLINE mtlk_mmb_basic_bdr_t *
_mtlk_mmb_cm_get_req_bbdr (mtlk_hw_t *hw,
                           BOOL       is_man)
{
  return is_man?&hw->tx_man.basic:&hw->tx_dbg.basic;
}

static __INLINE void *
_mtlk_mmb_cm_bdr_get_iom_bd (mtlk_mmb_basic_bdr_t *bdr,
                             uint8                 index,
                             uint32                iom_size)
{
  return __mtlk_basic_bdr_get_iom_bd_safe(bdr, index, iom_size);
}

static __INLINE void *
_mtlk_mmb_cm_get_mirror_bd (mtlk_mmb_basic_bdr_t *bdr,
                            uint8                 index)
{
  return __mtlk_basic_bdr_get_mirror_bd_safe(bdr, index);
}

#define _mtlk_mmb_cm_ind_get_mirror_bd(b, i) \
  ((mtlk_hw_cm_ind_mirror_t *)_mtlk_mmb_cm_get_mirror_bd((b), (i)))

#define _mtlk_mmb_cm_req_get_mirror_bd(b, i) \
  ((mtlk_hw_cm_req_mirror_t*)_mtlk_mmb_cm_get_mirror_bd((b), (i)))

static void
_mtlk_mmb_resp_cm_ind (mtlk_hw_t                     *hw,
                       BOOL                           is_man,
                       const mtlk_hw_cm_ind_mirror_t *cm_ind)
{
  uint32 iom_size = _mtlk_mmb_cm_bdr_get_iom_bd_size(hw, is_man);
  void  *iom      = _mtlk_mmb_cm_bdr_get_iom_bd(_mtlk_mmb_cm_get_ind_bbdr(hw, is_man), cm_ind->hdr.index, iom_size);

  _mtlk_mmb_memcpy_toio(iom, &cm_ind->msg_hdr, iom_size);

  _mtlk_mmb_send_msg(hw, is_man?ARRAY_MAN_IND:ARRAY_DBG_IND, cm_ind->hdr.index, 0);
}

static __INLINE void
_mtlk_mmb_handle_dat_cfm (mtlk_hw_t *hw, 
                          uint8      index,
                          uint16     info)
{
  mtlk_core_release_tx_data_t data;
  mtlk_hw_data_req_mirror_t  *data_req;

  data_req = _mtlk_basic_bdr_get_mirror_bd(&hw->tx_data.basic, index, mtlk_hw_data_req_mirror_t);

  if (data_req->dma_addr) { /* NULL keep-alive packets are not mapped */
      mtlk_df_nbuf_unmap_phys_addr(hw->vap_manager,
                                   data_req->nbuf,
                                   data_req->dma_addr,
                                   data_req->size,
                                   MTLK_DATA_TO_DEVICE);
  }

  data.msg             = HW_MSG_PTR(data_req);
  data.nbuf            = data_req->nbuf;
  data.size            = data_req->size;
  data.access_category = data_req->ac;
  data.status          = (UMI_STATUS)MTLK_BFIELD_GET(info, IND_REQ_TX_STATUS);
  data.resources_free  = hw->tx_data_nof_free_bds;
  data.nof_retries     = MTLK_BFIELD_GET(info, IND_REQ_NUM_RETRANSMISSIONS);

  mtlk_vap_get_core_vft(data_req->vap_handle)->release_tx_data(data_req->vap_handle, &data);
   
  _mtlk_mmb_free_sent_msg_to_data_pool(hw, data_req);
}

#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)

static __inline int
_mtlk_mmb_push_logger_buf(mtlk_hw_t *hw,
                          void* buff_addr,
                          uint16 buff_size)
{
  uint8 *pdata;
  mtlk_log_buf_entry_t *pbuf = mtlk_log_new_pkt_reserve(buff_size, &pdata);

  if (NULL != pbuf) {
    MTLK_ASSERT(NULL != pdata);
    memcpy(pdata, buff_addr, buff_size);
    mtlk_log_new_pkt_release(pbuf);
    _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_FW_LOGGER_PACKETS_PROCESSED);
    return MTLK_ERR_OK;
  }
  else {
    _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_FW_LOGGER_PACKETS_DROPPED);
    return MTLK_ERR_NO_MEM;
  }
}

#endif

static __INLINE int
_mtlk_mmb_handle_logger_buf_indication(mtlk_hw_t *hw,
                                       uint8      msg_index,
                                       uint16     msg_info)
{
  int res = MTLK_ERR_OK;
#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
  mtlk_hw_log_ind_mirror_t *log_ind = 
    _mtlk_basic_bdr_get_mirror_bd(&hw->fw_logger.log_buffers, msg_index, mtlk_hw_log_ind_mirror_t);
  uint16 log_data_size = _mtlk_mmb_get_log_ind_data_size(msg_info);
#endif

  MTLK_ASSERT(RTLOG_FLAGS & RTLF_REMOTE_ENABLED);

#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)

  mtlk_unmap_phys_addr(hw->vap_manager, log_ind->dma_addr,
                       log_data_size, MTLK_DATA_FROM_DEVICE);

  res = _mtlk_mmb_push_logger_buf(hw, log_ind->virt_addr, log_data_size);

  MTLK_ASSERT(0 != log_ind->bufsize);

  log_ind->dma_addr = mtlk_map_to_phys_addr(hw->vap_manager, log_ind->virt_addr,
                                            log_ind->bufsize, MTLK_DATA_FROM_DEVICE);

  MTLK_ASSERT(0 != log_ind->dma_addr);

#endif
  /* Send response */
  _mtlk_mmb_send_msg(hw, ARRAY_DAT_LOGGER_IND, msg_index, 0);
  return res;
}

static __INLINE int
_mtlk_mmb_handle_dat_ind (mtlk_hw_t *hw, 
                          uint8      msg_index,
                          uint16     msg_info)
{
  int                        res      = MTLK_ERR_UNKNOWN;
  uint8                      offset   = _mtlk_mmb_get_rx_ind_data_offset(msg_info);
  uint32                     data_size= _mtlk_mmb_get_rx_ind_data_size(msg_info);
  mtlk_core_handle_rx_data_t data;
  mtlk_hw_data_ind_mirror_t *data_ind = 
    _mtlk_basic_bdr_get_mirror_bd(&hw->rx_data, msg_index, mtlk_hw_data_ind_mirror_t);
  uint8                     *buffer   =
    (uint8 *)mtlk_df_nbuf_get_virt_addr(data_ind->nbuf);
  mtlk_vap_handle_t          vap_handle;
  uint8                      vap_id = 0;

  data.info = (MAC_RX_ADDITIONAL_INFO_T *)(buffer + offset);
  offset += sizeof(*data.info); /* skip meta-data header */

  mtlk_df_nbuf_unmap_phys_addr(hw->vap_manager,
                               data_ind->nbuf,
                               data_ind->dma_addr,
                               data_size + offset,
                               MTLK_DATA_FROM_DEVICE);

  hw->rx_data_queues.queue[data_ind->que_idx].que_size--;
  data.nbuf     = data_ind->nbuf;
  data.size     = data_size; /* size of data in buffer */
  data.offset   = offset;

  ILOG6_DDD("RX IND: al=%d o=%d ro=%d",
       (int)data.size,
       (int)msg_info,
       (int)data.offset);

  vap_handle = _mtlk_mmb_get_vap_handle_from_msg_info(hw, msg_info, &vap_id);

  res = mtlk_vap_get_core_vft(vap_handle)->handle_rx_data(vap_handle, &data);

  ILOG6_D("vap_index %d",  vap_id);

  if (res == MTLK_ERR_NOT_IN_USE) {
    mtlk_df_nbuf_free_sub_frames(hw->vap_manager, data_ind->nbuf);
  }

  data_ind->nbuf = _mtlk_mmb_nbuf_alloc(hw, data_ind->size);
  if (__UNLIKELY(data_ind->nbuf == NULL)) { 
    /* Handler failed. Fill requested buffer size and put the 
       RX Data Ind mirror element to Pending list to allow 
       recovery (reallocation) later.
     */
    _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_RX_BUF_ALLOC_FAILED);
    mtlk_lslist_push(&hw->rx_data_pending.lbufs, &data_ind->pend_l);
    ILOG2_DD("RX Data HANDLE_REALLOC failed! Slot#%d (%d bytes) added to pending list", 
         (int)msg_index,
         (int)data_ind->size);
    goto FINISH;
  }

  data_ind->dma_addr = mtlk_df_nbuf_map_to_phys_addr(hw->vap_manager,
                                                     data_ind->nbuf, 
                                                     data_ind->size,
                                                     MTLK_DATA_FROM_DEVICE);

  hw->rx_data_queues.queue[data_ind->que_idx].que_size++;

  /* Set new payload buffer address */
  _mtlk_mmb_set_rx_payload_addr(hw, msg_index, data_ind->dma_addr);
  
  /* Send response */
  _mtlk_mmb_send_msg(hw, ARRAY_DAT_IND, msg_index, 
                     _mtlk_mmb_get_rx_res_info(data_ind->que_idx));

FINISH:
  return res;
}

static __INLINE int
_mtlk_mmb_handle_dat_ind_on_stop (mtlk_hw_t *hw, 
                                  uint8      msg_index,
                                  uint16     msg_info)
{
  mtlk_hw_data_ind_mirror_t *data_ind = 
    _mtlk_basic_bdr_get_mirror_bd(&hw->rx_data, msg_index, mtlk_hw_data_ind_mirror_t);

  /* Send response */
  _mtlk_mmb_send_msg(hw, ARRAY_DAT_IND, msg_index, 
                     _mtlk_mmb_get_rx_res_info(data_ind->que_idx));

  return MTLK_ERR_OK;
}

static void
_mtlk_mmb_handle_cm_ind (mtlk_hw_t *hw,
                         BOOL       is_man,
                         uint8      msg_index,
                         uint16     msg_info)
{
  uint32                    msg_id    = 0;
  mtlk_mmb_basic_bdr_t     *bbdr      = _mtlk_mmb_cm_get_ind_bbdr(hw, is_man);
  mtlk_hw_cm_ind_mirror_t  *ind_obj   = _mtlk_mmb_cm_ind_get_mirror_bd(bbdr, msg_index);
  uint32                    iom_size  = _mtlk_mmb_cm_bdr_get_iom_bd_size(hw, is_man);
  void                     *iom       = _mtlk_mmb_cm_bdr_get_iom_bd(bbdr, msg_index, iom_size);
  mtlk_vap_handle_t         vap_handle;
  uint8                     vap_id;

  /* get MAN ind header + data */
  _mtlk_mmb_memcpy_fromio(&ind_obj->msg_hdr,
                          iom,
                          iom_size);

  msg_id = (uint32)MAC_TO_HOST16(ind_obj->msg_hdr.u16MsgId);

  vap_handle = _mtlk_mmb_get_vap_handle_from_msg_info(hw, msg_info, &vap_id);
  mtlk_vap_get_core_vft(vap_handle)->handle_rx_ctrl(vap_handle,
                           msg_id,
                           ((uint8 *)&ind_obj->msg_hdr) + sizeof(ind_obj->msg_hdr),
                           iom_size - sizeof(ind_obj->msg_hdr));

  ILOG2_DD("msg_id 0x%x, vap_index %d", 
           msg_id, vap_id);

  _mtlk_mmb_resp_cm_ind(hw, is_man, ind_obj);
}

static __INLINE void
_mtlk_mmb_dbg_verify_msg_send(mtlk_hw_cm_req_mirror_t *obj)
{
#ifdef MTCFG_DEBUG
  if (1 != mtlk_osal_atomic_inc(&obj->usage_cnt)) {
    ELOG_D("Message being sent twice, msg id 0x%x",
          MAC_TO_HOST16(MSG_OBJ_GET_ID(&obj->msg_hdr)));
    MTLK_ASSERT(FALSE);
  }
#else
  MTLK_UNREFERENCED_PARAM(obj);
#endif
}

static __INLINE void
_mtlk_mmb_dbg_verify_msg_recv(mtlk_hw_cm_req_mirror_t *obj)
{
#ifdef MTCFG_DEBUG
  if (0 != mtlk_osal_atomic_dec(&obj->usage_cnt)) {
    ELOG_D("Message received from HW twice, msg id 0x%x",
          MAC_TO_HOST16(MSG_OBJ_GET_ID(&obj->msg_hdr)));
    MTLK_ASSERT(FALSE);
  }
#else
  MTLK_UNREFERENCED_PARAM(obj);
#endif
}

static __INLINE void
_mtlk_mmb_dbg_init_msg_verifier(mtlk_mmb_basic_bdr_t *bbdr)
{
#ifdef MTCFG_DEBUG
  uint8 i;
  for (i = 0; i < bbdr->nof_bds; i++) {
    mtlk_hw_cm_req_mirror_t *obj = _mtlk_basic_bdr_get_mirror_bd(bbdr, i, mtlk_hw_cm_req_mirror_t);
    mtlk_osal_atomic_set(&obj->usage_cnt, 0);
  }
#else
  MTLK_UNREFERENCED_PARAM(bbdr);
#endif
}

static __INLINE void
_mtlk_mmb_handle_cm_cfm (mtlk_hw_t *hw, 
                         BOOL       is_man,
                         uint8      msg_index,
                         uint16     msg_info)
{
  mtlk_mmb_basic_bdr_t    *bbdr     = _mtlk_mmb_cm_get_req_bbdr (hw, is_man);
  mtlk_hw_cm_req_mirror_t *req_obj  = _mtlk_mmb_cm_req_get_mirror_bd(bbdr, msg_index);
  uint32                   iom_size = _mtlk_mmb_cm_bdr_get_iom_bd_size(hw, is_man);
  void                    *iom      = _mtlk_mmb_cm_bdr_get_iom_bd(bbdr, msg_index, iom_size);

  MTLK_UNREFERENCED_PARAM(msg_info);
  
  _mtlk_mmb_dbg_verify_msg_recv(req_obj);

  /* get data */
  _mtlk_mmb_memcpy_fromio(MSG_OBJ_PAYLOAD(&req_obj->msg_hdr),
                          ((uint8 *)iom) + sizeof(req_obj->msg_hdr),
                          iom_size - sizeof(req_obj->msg_hdr));

  /*send it to TXMM */
  if (is_man) {
    txmm_on_cfm(hw, &req_obj->msg_hdr);
  }
  else {
    txdm_on_cfm(hw, &req_obj->msg_hdr);
  }
}

static void
_mtlk_mmb_handle_received_msg (mtlk_hw_t *hw, 
                               uint8      type, 
                               uint8      index, 
                               uint16     info)
{
  switch (type) {
  case ARRAY_DAT_IND:
    if (__LIKELY(!hw->mac_events_stopped))
      _mtlk_mmb_handle_dat_ind(hw, index, info);
    else
      _mtlk_mmb_handle_dat_ind_on_stop(hw, index, info);
    break;
  case ARRAY_DAT_REQ:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_TX_CFM);
    if (__LIKELY(!hw->mac_events_stopped))
      _mtlk_mmb_handle_dat_cfm(hw, index, info);
    break;
  case ARRAY_MAN_IND:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_MM);
    if (__LIKELY(!hw->mac_events_stopped))
      _mtlk_mmb_handle_cm_ind(hw, TRUE, index, info);
    break;
  case ARRAY_MAN_REQ:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_TX_MM_CFM);
    if (__LIKELY(!hw->mac_events_stopped_completely))
      _mtlk_mmb_handle_cm_cfm(hw, TRUE, index, info);
    break;
  case ARRAY_DBG_IND:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_MM);
    if (__LIKELY(!hw->mac_events_stopped))
      _mtlk_mmb_handle_cm_ind(hw, FALSE, index, info);
    break;
  case ARRAY_DBG_REQ:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_TX_MM_CFM);
    if (__LIKELY(!hw->mac_events_stopped_completely))
      _mtlk_mmb_handle_cm_cfm(hw, FALSE, index, info);
    break;
  case ARRAY_DAT_LOGGER_IND:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_LOG);
    if (__LIKELY(!hw->mac_events_stopped_completely))
      _mtlk_mmb_handle_logger_buf_indication(hw, index, info);
    break;
  case ARRAY_NULL:
  default:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_MM);
    ELOG_D("Wrong message type (%d)", type);
    break;
  }
}

static __INLINE void
_mtlk_mmb_read_ind_or_cfm(mtlk_hw_t *hw)
{
  int processed_count = 0;
#ifdef MTCFG_CPU_STAT
  static const cpu_stat_track_id_e ids[] = {
    CPU_STAT_ID_RX_DAT,
    CPU_STAT_ID_RX_MM,
    CPU_STAT_ID_RX_EMPTY,
    CPU_STAT_ID_TX_CFM,
    CPU_STAT_ID_RX_LOG,
    CPU_STAT_ID_TX_MM_CFM,
    CPU_STAT_ID_RX_MGMT_BEACON,
    CPU_STAT_ID_RX_MGMT_ACTION,
    CPU_STAT_ID_RX_MGMT_OTHER,
    CPU_STAT_ID_RX_CTL
  };
  cpu_stat_track_id_e ts_handle;
#endif

  MTLK_ASSERT(NULL != hw->ccr);

  while (processed_count++ < hw->bds.ind.size) {
    IND_REQ_BUF_DESC_ELEM elem;

    /* WLS-2479 zero_elem must be 4-aligned.                   */
    /* Considering variety of kernel configuration options     */
    /* related to packing and alignment, the only fool proof   */
    /* way to secure this requirement is to declare it as 4    */
    /* bytes integer type.                                     */
    static const uint32 zero_elem = 0;
    MTLK_ASSERT(sizeof(zero_elem) == sizeof(IND_REQ_BUF_DESC_ELEM));

    CPU_STAT_BEGIN_TRACK_SET(ids, ARRAY_SIZE(ids), &ts_handle);

    CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_RX_HW);
    _mtlk_mmb_pas_get(hw,
                      "get next message",
                      hw->bds.ind.offset + hw->bds.ind.idx * sizeof(elem),
                      &elem,
                      sizeof(elem));

    ILOG5_DDD("MSG READ: t=%d idx=%d inf=%d",
         (int)elem.u8Type,
         (int)elem.u8Index,
         (int)MAC_TO_HOST16(elem.u16Info));

    if (elem.u8Type == 0) /* NULL type means empty descriptor */
    {
      CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_EMPTY);
      CPU_STAT_END_TRACK_SET(ts_handle);
      CPU_STAT_END_TRACK(CPU_STAT_ID_RX_HW);

      goto _finish;     /* all packets are processed */
    }

    /***********************************************************************
     * Zero handled BD
     ***********************************************************************/
    _mtlk_mmb_pas_put(hw,
                      "zero received message",
                      hw->bds.ind.offset + hw->bds.ind.idx * sizeof(IND_REQ_BUF_DESC_ELEM),
                      &zero_elem,
                      sizeof(IND_REQ_BUF_DESC_ELEM));
    /***********************************************************************/
    CPU_STAT_END_TRACK(CPU_STAT_ID_RX_HW);

    _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_FW_MSGS_HANDLED);

#ifdef MTLK_CCR_CLEAR_INT_UNCONDIT
    /* clear interrupts unconditionally */
    mtlk_ccr_clear_interrupts(hw->ccr);
#endif

    _mtlk_mmb_handle_received_msg(hw, 
                                  elem.u8Type, 
                                  elem.u8Index, 
                                  MAC_TO_HOST16(elem.u16Info));

    hw->bds.ind.idx++;
    if (hw->bds.ind.idx >= hw->bds.ind.size)
      hw->bds.ind.idx = 0;

    CPU_STAT_END_TRACK_SET(ts_handle);
  }

  /* not all packets are processed -> reschedule tasklet */
  (void)mtlk_ccr_postpone_irq_handler(hw->ccr);
  return;   /* don't enable interrupts */

_finish:
  mtlk_ccr_enable_interrupts(hw->ccr);
}

static int
_mtlk_mmb_cause_mac_assert (mtlk_hw_t *hw, uint32 mips_no)
{
  uint32 pas_offset;
  uint32 val = 0;

  MTLK_ASSERT(mips_no < NUM_OF_MIPS);

  if (!hw->mips_ctrl.ext_data.u32DescriptorLocation) {
    return MTLK_ERR_NOT_SUPPORTED;
  }

  pas_offset = 
    hw->mips_ctrl.ext_data.u32DescriptorLocation + 
    MTLK_OFFSET_OF(MIPS_CONTROL_DESCRIPTOR, u32MIPSctrl[mips_no]);

  _mtlk_mmb_pas_readl(hw,
                      "MIPS Ctrl",
                      pas_offset,
                      val);

  MTLK_BFIELD_SET(val, MIPS_CTRL_DO_ASSERT, 1);

  _mtlk_mmb_pas_writel(hw,
                       "MIPS Ctrl",
                       pas_offset,
                       val);

  WLOG_V("FW MAC assert is done");
  return MTLK_ERR_OK;
}

static int
_mtlk_mmb_handle_sw_trap (mtlk_hw_t *hw)
{
  int res = MTLK_ERR_UNKNOWN;
  uint32 mips_no = UMIPS;
  mtlk_vap_handle_t master_vap_handle;

  if ((hw->mips_ctrl.ext_data.u32DescriptorLocation) &&
      (hw->state != MTLK_HW_STATE_MAC_ASSERTED) &&
      (hw->state != MTLK_HW_STATE_APPFATAL)) {
    /* MIPS Ctrl extension supported => cause MAC assert => 
     * Core will receive and handle it in regular way
     */
    res = _mtlk_mmb_cause_mac_assert(hw, mips_no);
    hw->state = MTLK_HW_STATE_MAC_ASSERTED;
  }
  else {
    /* MIPS Ctrl extension NOT supported => notify Core => 
     * Core will "simulate" MAC assertion
     * OR
     * MAC assertion was sent, but FW didn't send
     * neither EVENT_APP_FATAL nor EVENT_EXCEPTION in response
     */ 
    res = mtlk_vap_manager_get_master_vap(hw->vap_manager, &master_vap_handle);
    if (MTLK_ERR_OK == res)
    {
      res = mtlk_vap_get_core_vft(master_vap_handle)->set_prop(master_vap_handle,
                             MTLK_CORE_PROP_MAC_STUCK_DETECTED, 
                             &mips_no,
                             sizeof(mips_no));
    }
  }
  
  return res;
}

static int
_mtlk_mmb_process_bcl(mtlk_hw_t *hw, UMI_BCL_REQUEST* preq, int get_req)
{
  int res     = MTLK_ERR_OK;
  int bcl_ctl = 0;
  int i       = 0;

  /* WARNING: _mtlk_mmb_pas_writel can't be used here since both
   * header and data should came in the same endianness
   */
  _mtlk_mmb_pas_put(hw, "Write unit", SHARED_RAM_BCL_ON_EXCEPTION_UNIT, 
                    &preq->Unit, sizeof(preq->Unit));
  _mtlk_mmb_pas_put(hw, "Write size", SHARED_RAM_BCL_ON_EXCEPTION_SIZE, 
                    &preq->Size, sizeof(preq->Size));
  _mtlk_mmb_pas_put(hw, "Write adress", SHARED_RAM_BCL_ON_EXCEPTION_ADDR, 
                    &preq->Address, sizeof(preq->Address));

  if (get_req)
  {
    bcl_ctl = BCL_READ;
  }
  else
  {
    _mtlk_mmb_pas_put(hw, "", SHARED_RAM_BCL_ON_EXCEPTION_DATA, preq->Data, sizeof(preq->Data));
    bcl_ctl = BCL_WRITE;
  }

  _mtlk_mmb_pas_writel(hw, "Write request", SHARED_RAM_BCL_ON_EXCEPTION_CTL, bcl_ctl);
  /* wait at least 1ms */
  mtlk_udelay(1000);

  // need to wait 150ms
  for (i = 0; i < 15; i++)
  {
    _mtlk_mmb_pas_readl(hw, "Reading BCL request status", SHARED_RAM_BCL_ON_EXCEPTION_CTL, bcl_ctl);
    //      bcl_ctl = le32_to_cpu(bcl_ctl);
    if (bcl_ctl == BCL_IDLE)
      break;
    /* if data are not ready, wait additional time */
    mtlk_osal_msleep(10);
  }

  if (bcl_ctl != BCL_IDLE)
  {
    WLOG_V("Timeout on BCL request");
    res = MTLK_ERR_TIMEOUT;
  }

  if (get_req)
  {
    _mtlk_mmb_pas_get(hw, "", SHARED_RAM_BCL_ON_EXCEPTION_DATA, preq->Data, sizeof(preq->Data));
  }

  return res;
}

static int
_mtlk_mmb_alloc_and_set_rx_buffer (mtlk_hw_t                 *hw, 
                                   mtlk_hw_data_ind_mirror_t *data_ind)
{
  int res  = MTLK_ERR_NO_MEM;

  data_ind->nbuf = _mtlk_mmb_nbuf_alloc(hw, data_ind->size);
  if (!data_ind->nbuf) {
    ILOG2_D("WARNING: failed to allocate buffer of %d bytes",
            (int)data_ind->size);
    goto FINISH;
  }

  data_ind->dma_addr = mtlk_df_nbuf_map_to_phys_addr(hw->vap_manager,
                                                     data_ind->nbuf,
                                                     data_ind->size,
                                                     MTLK_DATA_FROM_DEVICE);

  ILOG3_PD("hbuff: p=0x%p l=%d", 
           data_ind->nbuf,
           (int)data_ind->size);
    
  _mtlk_mmb_set_rx_payload_addr(hw, data_ind->hdr.index, data_ind->dma_addr);

  res = MTLK_ERR_OK;

FINISH:
  return res;
}

static void
_mtlk_mmb_recover_rx_buffers (mtlk_hw_t *hw, uint16 max_buffers)
{
  uint16 i = 0;
  for (; i < max_buffers; i++) {
    int                        ares     = MTLK_ERR_UNKNOWN;
    mtlk_lslist_entry_t       *lentry   = mtlk_lslist_pop(&hw->rx_data_pending.lbufs);
    mtlk_hw_data_ind_mirror_t *data_ind = NULL;

    if (!lentry) /* no more pending entries */
      break;

    data_ind = MTLK_LIST_GET_CONTAINING_RECORD(lentry, 
                                               mtlk_hw_data_ind_mirror_t, 
                                               pend_l);

    ares = _mtlk_mmb_alloc_and_set_rx_buffer(hw, data_ind);
    if (ares != MTLK_ERR_OK) {
      _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_RX_BUF_REALLOC_FAILED);
      /* Failed again. Put it back to the pending list and stop recovery. */
      mtlk_lslist_push(&hw->rx_data_pending.lbufs, &data_ind->pend_l);
      break;
    }

    _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_RX_BUF_REALLOCATED);

    /* Succeeded. Send it to MAC as response. */
    hw->rx_data_queues.queue[data_ind->que_idx].que_size++;
    _mtlk_mmb_send_msg(hw, ARRAY_DAT_IND, data_ind->hdr.index, 
                       _mtlk_mmb_get_rx_res_info(data_ind->que_idx));
    ILOG2_DD("Slot#%d (%d bytes) returned to MAC", 
         (int)data_ind->hdr.index,
         (int)data_ind->size);
  }
}

static uint32 __MTLK_IFUNC 
_mtlk_mmb_on_rx_buffs_recovery_timer (mtlk_osal_timer_t *timer, 
                                      mtlk_handle_t      clb_usr_data)
{
  mtlk_hw_t *hw = (mtlk_hw_t*)clb_usr_data;
  
  MTLK_UNREFERENCED_PARAM(timer);
  _mtlk_mmb_recover_rx_buffers(hw, MTLK_MAX_RX_BUFFS_TO_RECOVER);

  return MTLK_RX_BUFFS_RECOVERY_PERIOD;
}

static void
_mtlk_mmb_power_on (mtlk_hw_t *hw)
{
  uint32 val = 0;

  MTLK_ASSERT(NULL != hw->ccr);

  mtlk_ccr_release_ctl_from_reset(hw->ccr);

  hw->mmb->bist_passed = 1;
  if (hw->mmb->cfg.bist_check_permitted) {
    if (!mtlk_ccr_check_bist(hw->ccr, &val)) {
      ILOG0_D("WARNING: Device self test status: 0x%08lu", (unsigned long)val);
      hw->mmb->bist_passed = 0;
    }
  }

  mtlk_ccr_boot_from_bus(hw->ccr);
  mtlk_ccr_exit_debug_mode(hw->ccr);
  mtlk_ccr_power_on_cpus(hw->ccr);
}

static int
_mtlk_mmb_load_fw2mem(mtlk_cpu_mem_t *mem, mtlk_df_fw_file_buf_t *buf)
{
  uint32 bytes_written = 0;
  int i;

  for(i = 0; (0 != mem[i].length) && (bytes_written < buf->size); i++ )
  {

#ifdef MTCFG_FW_WRITE_VALIDATION
  #define FW_WRITE_FUNC _mtlk_mmb_memcpy_validate_toio
#else
  #define FW_WRITE_FUNC _mtlk_mmb_memcpy_toio_swap
#endif
    if(!FW_WRITE_FUNC(mem[i].start, buf->buffer + bytes_written,
                      MIN(mem[i].length, buf->size - bytes_written), mem[i].swap))
    {
        ELOG_V("Failed to put firmware to shared memory");
        return MTLK_ERR_FW;
    }
#undef FW_WRITE_FUNC     

    bytes_written += MIN(mem[i].length, buf->size - bytes_written);
    MTLK_ASSERT(bytes_written <= buf->size);
  }
  
  if (bytes_written != buf->size)
  {
    ELOG_DD("Firmware file is to big to fit into the cpu memory (%d > %d)",
         buf->size, bytes_written);
    return MTLK_ERR_FW;
  }

  return MTLK_ERR_OK; 
}

static int
_mtlk_mmb_load_firmware(mtlk_hw_t* hw)
{
  mtlk_df_fw_file_buf_t fb;
  mtlk_work_mode_e mode;
  mtlk_fw_info_t fw_info;
  int cpu = 0;
  int res = MTLK_ERR_FW;

  mode = mtlk_vap_manager_get_mode(hw->vap_manager);

  mtlk_osal_lock_acquire(&hw->reg_lock);
  hw->if_version[0] = 0;
  mtlk_osal_lock_release(&hw->reg_lock);

  for(cpu = 0; cpu < CHI_CPU_NUM_CNT; cpu++)
  {
    if (MTLK_ERR_OK != mtlk_ccr_get_fw_info(hw->ccr, cpu, mode, &fw_info))
      break;

    res = mtlk_df_fw_load_file(hw->vap_manager, fw_info.fname, &fb);
    if (res != MTLK_ERR_OK)
    {
      ELOG_S("can not load file '%s'", fw_info.fname);
      break;
    }

    ILOG2_SD("Loading '%s' of %d bytes", fw_info.fname, fb.size);

    res = _mtlk_mmb_load_fw2mem(fw_info.mem, &fb);

    if (MTLK_ERR_OK == res)
      _mtlk_mmb_notify_firmware(hw, fw_info.fname, &fb);

    mtlk_df_fw_unload_file(hw->vap_manager, &fb);

    if (res != MTLK_ERR_OK)
    {
      ELOG_S("'%s' load timed out or interrupted", fw_info.fname);
      break;
    }
  }

  return res;
}

#define PROGMODEL_MODIFIED_MASK 0x80000000u
#define PROGMODEL_VERSION_MASK  0x0000ffffu

static void prepare_progmodel_string(mtlk_hw_t *hw, const mtlk_core_firmware_file_t *ff)
{
  uint32 version = 0;
  int res = MTLK_ERR_FW;
  int len = 0;
  BOOL modified = FALSE;
  mtlk_vap_handle_t master_vap_handle;

  version = mtlk_ccr_get_progmodel_version(hw->ccr);
  modified = !!(version & PROGMODEL_MODIFIED_MASK);
  version &= PROGMODEL_VERSION_MASK;

  mtlk_osal_lock_acquire(&hw->reg_lock);

  len = mtlk_snprintf(hw->progmodel, STRING_PROGMODEL_SIZE, "ProgModel: %s version %d%s\n",
                  ff->fname, version, modified ? " (modified)" : "");

  hw->progmodel[STRING_PROGMODEL_SIZE - 1] = '\0';

  mtlk_osal_lock_release(&hw->reg_lock);

  res = mtlk_vap_manager_get_master_vap(hw->vap_manager, &master_vap_handle);
  if (len > 0 && res == MTLK_ERR_OK)
  {
    ILOG0_DS("CID-%04x: %s", mtlk_vap_get_oid(master_vap_handle), hw->progmodel);
  }
}

static int
_mtlk_mmb_load_progmodel_to_hw (mtlk_hw_t   *hw,
                                const mtlk_core_firmware_file_t *ff)
{
  int                    res       = MTLK_ERR_FW;
  unsigned int           loc       = 0;
  mtlk_txmm_msg_t        man_msg;
  mtlk_txmm_data_t      *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txmm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("can not get TXMM slot");
    goto FINISH;
  }

  while (loc < ff->fcontents.size) {
    unsigned int left;

    if ((ff->fcontents.size - loc) >  PROGMODEL_CHUNK_SIZE)
      left = PROGMODEL_CHUNK_SIZE;
    else
      left = ff->fcontents.size - loc;

    // XXX: what is 4*5 here? (ant)
    _mtlk_mmb_pas_put(hw, "",  4*5, ff->fcontents.buffer + loc, left);
    ILOG4_DD("wrote %d bytes to PAS offset 0x%x\n",
        (int)left, 4*5);

    man_entry->id           = UM_DOWNLOAD_PROG_MODEL_REQ;
    man_entry->payload_size = 0;

    res = mtlk_txmm_msg_send_blocked(&man_msg,
                                     MTLK_PRGMDL_LOAD_CHUNK_TIMEOUT);

    if (res != MTLK_ERR_OK) {
      ELOG_D("Can't download programming model, timed-out. Err#%d", res);
#if 1
      /* a2k - do not exit - allow to connect to driver through BCL for debugging */
      res = MTLK_ERR_OK;
#endif
      goto FINISH;
    }

    loc+= left;
    ILOG3_DD("loc %d, left %d", loc, left);
  }

  ILOG3_V("End program mode");

  if(ff->update_version)
  {
    prepare_progmodel_string(hw, ff);
  }

  res = MTLK_ERR_OK;

FINISH:

  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static void
_mtlk_mmb_run_firmware(mtlk_hw_t *hw)
{
  MTLK_ASSERT(NULL != hw->ccr);

  mtlk_ccr_disable_interrupts(hw->ccr);
  mtlk_ccr_switch_to_iram_boot(hw->ccr);
  mtlk_ccr_release_cpus_reset(hw->ccr);
}

static void
_mtlk_mmb_parse_chi_extensions (mtlk_hw_t *hw)
{
  uint32 offset = CHI_ADDR + sizeof(hw->chi_data);

  ILOG2_DDD("offset = %d, CHI_ADDR = 0x%08x, sizeof(hw->chi_data) = %lu",offset,CHI_ADDR,(unsigned long)sizeof(hw->chi_data));

  while (1) {
    VECTOR_AREA_EXTENSION_HEADER ext_hdr;

    _mtlk_mmb_pas_get(hw,
                      "CHI Vector Extension Header",
                      offset,
                      &ext_hdr,
                      sizeof(ext_hdr));

    ILOG2_D("HOST_EXTENSION_MAGIC = 0x%08x",HOST_EXTENSION_MAGIC);
    ILOG2_D("ext_hdr.u32ExtensionMagic = 0x%08x",ext_hdr.u32ExtensionMagic);
    
    if (MAC_TO_HOST32(ext_hdr.u32ExtensionMagic) != HOST_EXTENSION_MAGIC)
      break; /* No more extensions in CHI Area */

    ext_hdr.u32ExtensionID       = MAC_TO_HOST32(ext_hdr.u32ExtensionID);
    ext_hdr.u32ExtensionDataSize = MAC_TO_HOST32(ext_hdr.u32ExtensionDataSize);
    
    ILOG2_D("ext_hdr.u32ExtensionID = 0x%08x",ext_hdr.u32ExtensionID);
    ILOG2_D("ext_hdr.u32ExtensionDataSize = %d",ext_hdr.u32ExtensionDataSize);

    offset += sizeof(ext_hdr); /* skip to extension data */

    switch (ext_hdr.u32ExtensionID) {
    case VECTOR_AREA_CALIBR_EXTENSION_ID:
      if (ext_hdr.u32ExtensionDataSize == sizeof(VECTOR_AREA_CALIBR_EXTENSION_DATA)) {
        _mtlk_mmb_pas_get(hw,
                          "CHI Vector Extension Data",
                          offset,
                          &hw->calibr.ext_data,
                          sizeof(hw->calibr.ext_data));
      }
      else {
        WLOG_DD("Invalid Calibration Extension Data size (%d != %d)",
             (int)ext_hdr.u32ExtensionDataSize,
             (int)sizeof(VECTOR_AREA_CALIBR_EXTENSION_DATA));
        memset(&hw->calibr, 0, sizeof(hw->calibr));
      }
      break;
    case VECTOR_AREA_MIPS_CONTROL_EXTENSION_ID:
      if (ext_hdr.u32ExtensionDataSize == sizeof(VECTOR_AREA_MIPS_CONTROL_EXTENSION_DATA)) {
        _mtlk_mmb_pas_get(hw,
                          "CHI Vector Extension Data",
                          offset,
                          &hw->mips_ctrl.ext_data,
                          sizeof(hw->mips_ctrl.ext_data));
      }
      else {
        WLOG_DD("Invalid MIPS Control Extension Data size (%d != %d)",
             (int)ext_hdr.u32ExtensionDataSize,
             (int)sizeof(VECTOR_AREA_MIPS_CONTROL_EXTENSION_DATA));
        memset(&hw->mips_ctrl, 0, sizeof(hw->mips_ctrl));
      }
      break;
    case VECTOR_AREA_LOGGER_EXTENSION_ID:
#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
      if (ext_hdr.u32ExtensionDataSize == sizeof(VECTOR_AREA_LOGGER_EXTENSION_DATA)) {
        _mtlk_mmb_pas_get(hw,
                          "CHI Vector FW Logger Extension Data",
                          offset,
                          &hw->fw_logger.ext_data,
                          sizeof(hw->fw_logger.ext_data));
        hw->fw_logger.is_supported = 1;
      }
      else {
        WLOG_DD("Invalid FW Logger Extension Data size (%d != %d)",
                (int)ext_hdr.u32ExtensionDataSize,
                (int)sizeof(VECTOR_AREA_LOGGER_EXTENSION_DATA));
        memset(&hw->fw_logger, 0, sizeof(hw->fw_logger));
      }
#else //#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
      WLOG_V("Runtime logger is supported by firmware but not by driver. Firmware logging suppressed.");
      memset(&hw->fw_logger, 0, sizeof(hw->fw_logger));
#endif //#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
      break;
    case VECTOR_AREA_LOGGER_BUF_INFO_EXTENSION_ID:
#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
      if (ext_hdr.u32ExtensionDataSize == sizeof(VECTOR_AREA_LOGGER_BUF_INFO_EXTENSION_DATA)) {
        _mtlk_mmb_pas_get(hw,
                          "CHI Vector FW Logger Buffer Info Extension Data",
                          offset,
                          &hw->fw_logger.buf_info_ext_data,
                          sizeof(hw->fw_logger.buf_info_ext_data));
        hw->fw_logger.is_supported_ex = 1;
      }
      else {
        WLOG_DD("Invalid FW Logger Buffer Info Extension Data size (%d != %d)",
                (int)ext_hdr.u32ExtensionDataSize,
                (int)sizeof(VECTOR_AREA_LOGGER_BUF_INFO_EXTENSION_DATA));
        memset(&hw->fw_logger, 0, sizeof(hw->fw_logger));
      }
#else //#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
      WLOG_V("Extended Runtime logger is supported by firmware but not by driver. Extended Firmware logging suppressed.");
#endif //#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
      break;
    case VECTOR_AREA_FW_CAPABILITIES_NUM_OF_STATIONS_EXTENSION_ID:
      if (ext_hdr.u32ExtensionDataSize == sizeof(hw->fw_capabilities.nof_stas)) {
        _mtlk_mmb_pas_get(hw,
                          "FW Caps Nof STAs Extension Data",
                          offset,
                          &hw->fw_capabilities.nof_stas,
                          sizeof(hw->fw_capabilities.nof_stas));
      }
      else {
        WLOG_DD("Invalid FW Caps Nof STA Extension Data size (%d != %d)",
                ext_hdr.u32ExtensionDataSize,
                sizeof(hw->fw_capabilities.nof_stas));
      }
      break;
    case VECTOR_AREA_FW_CAPABILITIES_NUM_OF_VAPS_EXTENSION_ID:
      if (ext_hdr.u32ExtensionDataSize == sizeof(hw->fw_capabilities.nof_vaps)) {
        _mtlk_mmb_pas_get(hw,
                          "FW Caps Nof VAPs Extension Data",
                          offset,
                          &hw->fw_capabilities.nof_vaps,
                          sizeof(hw->fw_capabilities.nof_vaps));
      }
      else {
        WLOG_DD("Invalid FW Caps Nof STA Extension Data size (%d != %d)",
                ext_hdr.u32ExtensionDataSize,
                sizeof(hw->fw_capabilities.nof_vaps));
      }
      break;
    default:
      ILOG2_D("Unrecognized extension#%d", (int)ext_hdr.u32ExtensionID);
      break;
    }

    offset += ext_hdr.u32ExtensionDataSize; /* skip to next extension */
  }
}

static void
_mtlk_mmb_chi_init(mtlk_hw_t *hw)
{
  /* cleanup cached values */
  memset (&hw->chi_data, 0, sizeof(hw->chi_data));

  /* cleanup FW memory */
  _mtlk_mmb_pas_put(hw,
                    "CHI Vector Area",
                    CHI_ADDR,
                    &hw->chi_data,
                    sizeof(hw->chi_data));

  /* Fill up FW memory with intial configuration. 
     Both structures chi_data and chi_init are mapped to the 
     same mem region 
  */
  hw->chi_init.mipsWorkingFreq = 
    HOST_TO_MAC32(mtlk_ccr_get_mips_freq(hw->ccr));
  
  _mtlk_mmb_pas_put(hw,
                    "CHI Vector Area Init Data",
                    CHI_ADDR,
                    &hw->chi_init,
                    sizeof(hw->chi_init));
  return;
}

static int
_mtlk_mmb_wait_chi_magic(mtlk_hw_t *hw)
{
  typedef enum
  {
    MTLK_HW_INIT_EVT_WAIT_RES_OK,
    MTLK_HW_INIT_EVT_WAIT_RES_FAILED,
    MTLK_HW_INIT_EVT_WAIT_RES_POLLING,
    MTLK_HW_INIT_EVT_WAIT_RES_LAST
  } mtlk_hw_init_evt_wait_res_e;

  mtlk_hw_init_evt_wait_res_e wait_res = MTLK_HW_INIT_EVT_WAIT_RES_LAST;

  unsigned int *tptr;
  int i;

#ifdef MTCFG_USE_INTERRUPT_POLLING
  mtlk_osal_timestamp_t start_ts = mtlk_osal_timestamp();
#endif

  MTLK_ASSERT(NULL != hw->ccr);

  /* Check for the magic value and then get the base address and length of the CHI area */
  hw->isr_type = MTLK_ISR_INIT_EVT;
  MTLK_HW_INIT_EVT_RESET(hw);
  mtlk_ccr_enable_interrupts(hw->ccr);

  wait_res = 
    (MTLK_HW_INIT_EVT_WAIT(hw, MTLK_CHI_MAGIC_TIMEOUT) == MTLK_ERR_OK)?
      MTLK_HW_INIT_EVT_WAIT_RES_OK:MTLK_HW_INIT_EVT_WAIT_RES_FAILED;

tptr=(void*)&hw->chi_data;

#ifdef MTCFG_USE_INTERRUPT_POLLING
  wait_res = MTLK_HW_INIT_EVT_WAIT_RES_POLLING;

  do
  {
#endif

    _mtlk_mmb_pas_get(hw,
                      "CHI Vector Area",
                      CHI_ADDR,
                      &hw->chi_data,
                      sizeof(hw->chi_data));

pr_info("command buffer dump %px:\n",tptr);

for (i=0;i<0x50;i++) {
pr_info("%04x: %08x\n",i,tptr[i]);
}

    if (wait_res != MTLK_HW_INIT_EVT_WAIT_RES_FAILED &&
        MAC_TO_HOST32(hw->chi_data.u32Magic) == HOST_MAGIC) {
      _mtlk_mmb_parse_chi_extensions(hw);
      return MTLK_ERR_OK;
    }

#ifdef MTCFG_USE_INTERRUPT_POLLING
    mtlk_osal_msleep(100);
  }
  while( mtlk_osal_time_passed_ms(start_ts) <= MTLK_CHI_MAGIC_TIMEOUT );
#endif

  ELOG_DD("Wait for CHI Magic failed (wait_res=%d value=0x%08x)",
          wait_res, MAC_TO_HOST32(hw->chi_data.u32Magic));

  return MTLK_ERR_HW;
}

static int
_mtlk_mmb_send_ready_blocked (mtlk_hw_t *hw)
{
  int               res       = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry;
  READY_REQ*        ready_req = NULL;
  uint16            nque      = 0;

  MTLK_ASSERT(NULL != hw->ccr);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txmm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("can not get TXMM slot");
    goto FINISH;
  }

  man_entry->id           = UM_MAN_READY_REQ;
  man_entry->payload_size = 0;

  ready_req = (READY_REQ *) man_entry->payload;

  memset(ready_req, 0, sizeof(*ready_req));


  for (nque = 0; nque < hw->rx_data_queues.nof_in_use; nque++) {
    /* NOTE: we should pass queues to MAC (fill the READY request):
       - arrenged by size (growing)
       - with whole buffer sizes (including max offset, header size etc.) 
    */
    ready_req->asQueueParams[nque].u16QueueSize  = 
      HOST_TO_MAC16(hw->rx_data_queues.queue[nque].que_size);
    ready_req->asQueueParams[nque].u16BufferSize = 
      HOST_TO_MAC16(hw->rx_data_queues.queue[nque].buf_size);
  }

  ready_req->FWinterface = HOST_TO_MAC32(mtlk_hw_mmb_get_card_idx(hw));

  mtlk_ccr_enable_interrupts(hw->ccr);
  res = mtlk_txmm_msg_send_blocked(&man_msg, 
                                   MTLK_READY_CFM_TIMEOUT);

  if (res != MTLK_ERR_OK) {
    ELOG_D("MAC initialization failed (err=%d)", res);
    goto FINISH;
  }

  res = MTLK_ERR_OK;

FINISH:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

int
 _mtlk_mmb_send_hw_dep_cfg (mtlk_hw_t *hw)
{
  int                           res;
  mtlk_txmm_msg_t               man_msg;
  mtlk_txmm_data_t              *man_entry;
  UmiHwDependentConfig_t        *hw_cfg_msg;
  uint32                        afe_value;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txmm_mirror, NULL);

  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_MAN_HW_DEPENDENT_CONFIG_REQ;
  man_entry->payload_size = sizeof(UmiHwDependentConfig_t);
  hw_cfg_msg = (UmiHwDependentConfig_t *) man_entry->payload;

  afe_value = mtlk_ccr_get_afe_value(hw->ccr);
  if (0 != afe_value) {
    WLOG_D("AFE value: 0x%08X", afe_value);
  }
  hw_cfg_msg->AFEvalue     = HOST_TO_MAC32(afe_value);
  hw_cfg_msg->xtal         = HOST_TO_MAC32(mtlk_eeprom_get_xtal_value(hw->ee_data));
  hw_cfg_msg->extLNAbypass = mtlk_eeprom_get_lna_gain_bypass(hw->ee_data); /* extLNAbypass is int8 */
  hw_cfg_msg->extLNAhigh   = mtlk_eeprom_get_lna_gain_high(hw->ee_data);   /* extLNAhigh is int8 */

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  mtlk_txmm_msg_cleanup(&man_msg);

  if (res != MTLK_ERR_OK) {
    ELOG_D("FW set HW dependent configuration failed (err=%d)", res);
  }

  return res;
}

int __MTLK_IFUNC 
_mtlk_mmb_send_fw_log_severity(mtlk_hw_t *hw,
                               uint32 newLevel,
                               uint32 targetCPU)
{
  int                           res;
  mtlk_txmm_msg_t               man_msg;
  mtlk_txmm_data_t              *man_entry;

  if (targetCPU >= UMI_CPU_ID_MAX) {
    ELOG_D("Incorrect targetCPU=%d", targetCPU);
    return MTLK_ERR_PARAMS;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txdm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  ILOG2_DD("newLevel=%d, targetCPU=%d.", newLevel, targetCPU);

  man_entry->id = UM_DBG_LOGGER_SET_SEVERITY_REQ;
  man_entry->payload_size = sizeof(UmiLoggerMsgSetSeverity_t);
  ((UmiLoggerMsgSetSeverity_t *) man_entry->payload)->newLevel  = HOST_TO_MAC32(newLevel);
  ((UmiLoggerMsgSetSeverity_t *) man_entry->payload)->targetCPU = HOST_TO_MAC32(targetCPU);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  mtlk_txmm_msg_cleanup(&man_msg);

  if (res != MTLK_ERR_OK) {
    ELOG_D("FW logger severity configuration failed (err=%d)", res);
  }

  return res;
}

int __MTLK_IFUNC
mtlk_mmb_send_debug_tpc(mtlk_hw_t *hw, uint8 value)
{
  int                           res;
  mtlk_txmm_msg_t               man_msg;
  mtlk_txmm_data_t              *man_entry;
  UMI_DEBUG_TPC                 *msg;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txmm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_MAN_SET_DEBUG_TPC_REQ;
  man_entry->payload_size = sizeof(*msg);

  msg = (UMI_DEBUG_TPC *) man_entry->payload;
  memset(msg, 0, sizeof(*msg));
  msg->value = value;

  ILOG2_D("Set DEBUG_TPC value %d", msg->value);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  mtlk_txmm_msg_cleanup(&man_msg);

  if (res != MTLK_ERR_OK) {
    ELOG_D("Setting DEBUG_TPC failed (err=%d)", res);
  }

  return res;
}

static int
_mtlk_mmb_send_fw_log_mode(mtlk_hw_t *hw)
{
  int                           res;
  mtlk_txmm_msg_t               man_msg;
  mtlk_txmm_data_t              *man_entry;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txdm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_DBG_LOGGER_SET_MODE_REQ;
  man_entry->payload_size = sizeof(UmiLoggerMsgSetMode_t);
  ((UmiLoggerMsgSetMode_t *) man_entry->payload)->modeReq = HOST_TO_MAC32(LOGGER_STATE_ACTIVE);
  ((UmiLoggerMsgSetMode_t *) man_entry->payload)->targetCPU = HOST_TO_MAC32(UMI_CPU_ID_UM);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  mtlk_txmm_msg_cleanup(&man_msg);

  if (res != MTLK_ERR_OK) {
    ELOG_D("FW logger activation failed (err=%d)", res);
  }

  return res;
}

static int
_mtlk_mmb_send_fw_log_mode_off(mtlk_hw_t *hw)
{
  int                           res;
  mtlk_txmm_msg_t               man_msg;
  mtlk_txmm_data_t              *man_entry;

  if((hw->state == MTLK_HW_STATE_EXCEPTION) ||
     (hw->state == MTLK_HW_STATE_APPFATAL) ||
     (hw->state == MTLK_HW_STATE_MAC_ASSERTED))
  {
    /* We do not stop FW logger. Let's hope it will not overwrite our memory. */
    return MTLK_ERR_OK;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txdm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_DBG_LOGGER_SET_MODE_REQ;
  man_entry->payload_size = sizeof(UmiLoggerMsgSetMode_t);
  ((UmiLoggerMsgSetMode_t *) man_entry->payload)->modeReq = HOST_TO_MAC32(LOGGER_STATE_CYCLIC_MODE);
  ((UmiLoggerMsgSetMode_t *) man_entry->payload)->targetCPU = HOST_TO_MAC32(UMI_CPU_ID_ALL);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  mtlk_txmm_msg_cleanup(&man_msg);

  if (res != MTLK_ERR_OK) {
    ELOG_D("FW logger deactivation failed (err=%d)", res);
  }

  return res;
}

int __MTLK_IFUNC
mtlk_mmb_send_agg_rate_limit(mtlk_hw_t *hw, uint8 mode, uint16 maxRate)
{
  int                           res;
  mtlk_txmm_msg_t               man_msg;
  mtlk_txmm_data_t              *man_entry;
  UMI_AGG_RATE_LIMIT            *msg_limit;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txmm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_MAN_SET_AGG_RATE_LIMIT_REQ;
  man_entry->payload_size = sizeof(*msg_limit);

  msg_limit = (UMI_AGG_RATE_LIMIT *)man_entry->payload;
  memset(msg_limit, 0, sizeof(*msg_limit));
  msg_limit->mode    = mode;
  msg_limit->maxRate = HOST_TO_MAC16(maxRate);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK || msg_limit->status != UMI_OK) {
    ELOG_DD("AGG_RATE_LIMIT configuration failed (err=%d, status=%d)", res, msg_limit->status);
    if (res == MTLK_ERR_OK) {
        res = MTLK_ERR_UNKNOWN;
    }
  }

  mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

int __MTLK_IFUNC
mtlk_mmb_send_rx_high_threshold(mtlk_hw_t *hw, int8 value)
{
  int                           res;
  mtlk_txmm_msg_t               man_msg;
  mtlk_txmm_data_t              *man_entry;
  UMI_RX_TH                     *msg;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txmm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_MAN_SET_RX_TH_REQ;
  man_entry->payload_size = sizeof(*msg);

  msg = (UMI_RX_TH *)man_entry->payload;
  memset(msg, 0, sizeof(*msg));
  msg->value = value;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("RX_HIGH_THRESHOLD configuration failed (err=%d)", res);
    res = MTLK_ERR_UNKNOWN;
  }

  mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

int __MTLK_IFUNC
mtlk_mmb_send_rx_duty_cycle(mtlk_hw_t *hw, uint32 onTime, uint32 offTime)
{
  int                           res;
  mtlk_txmm_msg_t               man_msg;
  mtlk_txmm_data_t              *man_entry;
  UMI_RX_DUTY_CYCLE             *msg;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txmm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_MAN_SET_RX_DUTY_CYCLE_REQ;
  man_entry->payload_size = sizeof(*msg);

  msg = (UMI_RX_DUTY_CYCLE *)man_entry->payload;
  memset(msg, 0, sizeof(*msg));
  msg->onTime  = HOST_TO_MAC32(onTime);
  msg->offTime = HOST_TO_MAC32(offTime);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("RX_DUTY_CYCLE configuration failed (err=%d)", res);
    res = MTLK_ERR_UNKNOWN;
  }

  mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

int __MTLK_IFUNC
mtlk_mmb_send_lna_gains(mtlk_hw_t *hw, int8 lna_gain_bypass, int8 lna_gain_high)
{
  int res = MTLK_ERR_OK;
  mtlk_txmm_msg_t    man_msg;
  mtlk_txmm_data_t   *man_entry;
  UMI_EXT_LNA_PARAMS *lna_gains;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txmm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_MAN_SET_EXT_LNA_PARAMS_REQ;
  man_entry->payload_size = sizeof(*lna_gains);

  lna_gains = (UMI_EXT_LNA_PARAMS *)man_entry->payload;
  memset(lna_gains, 0, sizeof(*lna_gains));
  lna_gains->extLnaGainBypass = lna_gain_bypass;
  lna_gains->extLnaGainHigh   = lna_gain_high;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);

  if (res != MTLK_ERR_OK || lna_gains->status != UMI_OK) {
    ELOG_DD("UM_MAN_SET_EXT_LNA_PARAMS_REQ configuration failed (err=%d, status=%d)", res, lna_gains->status);
    if (res == MTLK_ERR_OK) {
      res = MTLK_ERR_UNKNOWN;
    }
  }

  mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

int __MTLK_IFUNC
mtlk_mmb_send_cca_threshold(mtlk_hw_t *hw, int8 value)
{
  int                           res;
  mtlk_txmm_msg_t               man_msg;
  mtlk_txmm_data_t              *man_entry;
  UMI_CCA_TH                    *msg;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txmm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_MAN_SET_CCA_TH_REQ;
  man_entry->payload_size = sizeof(*msg);

  msg = (UMI_CCA_TH *)man_entry->payload;
  memset(msg, 0, sizeof(*msg));
  msg->value = value;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("CCA_TH_REQ configuration failed (err=%d)", res);
    res = MTLK_ERR_UNKNOWN;
  }

  mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static void
_mtlk_mmb_cleanup_man_req_bdr (mtlk_hw_t *hw)
{
  if (hw->flags & HW_FLAGS_PREPARED_MAN_REQ_BDR) {
    _mtlk_advanced_bdr_cleanup(&hw->tx_man);
    hw->flags &= ~HW_FLAGS_PREPARED_MAN_REQ_BDR;
  }
}

static void
_mtlk_mmb_cleanup_man_ind_bdr (mtlk_hw_t *hw)
{
  if (hw->flags & HW_FLAGS_PREPARED_MAN_IND_BDR) {
    _mtlk_basic_bdr_cleanup(&hw->rx_man);
    hw->flags &= ~HW_FLAGS_PREPARED_MAN_IND_BDR;
  }
}

static void
_mtlk_mmb_cleanup_dbg_req_bdr (mtlk_hw_t *hw)
{
  if (hw->flags & HW_FLAGS_PREPARED_DBG_REQ_BDR) {
    _mtlk_advanced_bdr_cleanup(&hw->tx_dbg);
    hw->flags &= ~HW_FLAGS_PREPARED_DBG_REQ_BDR;
  }
}

static void
_mtlk_mmb_cleanup_dbg_ind_bdr (mtlk_hw_t *hw)
{
  if (hw->flags & HW_FLAGS_PREPARED_DBG_IND_BDR) {
    _mtlk_basic_bdr_cleanup(&hw->rx_dbg);
    hw->flags &= ~HW_FLAGS_PREPARED_DBG_IND_BDR;
  }
}

static void
_mtlk_mmb_cleanup_data_req_bdr (mtlk_hw_t *hw)
{
  if (hw->flags & HW_FLAGS_PREPARED_DATA_REQ_BDR) {
    _mtlk_mmb_free_unconfirmed_tx_buffers(hw);
    _mtlk_advanced_bdr_cleanup(&hw->tx_data);
    hw->flags &= ~HW_FLAGS_PREPARED_DATA_REQ_BDR;
  }
}

static void
_mtlk_mmb_cleanup_data_ind_bdr (mtlk_hw_t *hw)
{
  if (hw->flags & HW_FLAGS_PREPARED_DATA_IND_BDR) {
    _mtlk_basic_bdr_cleanup(&hw->rx_data);
    hw->flags &= ~HW_FLAGS_PREPARED_DATA_IND_BDR;
  }
}

static void
_mtlk_mmb_cleanup_log_ind_bdr (mtlk_hw_t *hw)
{
  MTLK_ASSERT(hw->fw_logger.is_supported);

  if (hw->flags & HW_FLAGS_PREPARED_LOG_IND_BDR) {
    _mtlk_basic_bdr_cleanup(&hw->fw_logger.log_buffers);
    hw->flags &= ~HW_FLAGS_PREPARED_LOG_IND_BDR;
  }
}

#define LOG_CHI_AREA(d)                                    \
  ILOG2_DDDD("CHI: " #d ": is=0x%x in=%d rs=0x%x rn=%d", \
       MAC_TO_HOST32(hw->chi_data.d.u32IndStartOffset),   \
       MAC_TO_HOST32(hw->chi_data.d.u32IndNumOfElements), \
       MAC_TO_HOST32(hw->chi_data.d.u32ReqStartOffset),   \
       MAC_TO_HOST32(hw->chi_data.d.u32ReqNumOfElements))

static int
_mtlk_mmb_prepare_man_req_bdr(mtlk_hw_t *hw)
{
  /* Management Requests BD initialization */
  int    res     = MTLK_ERR_UNKNOWN;
  uint32 nof_bds = MAC_TO_HOST32(hw->chi_data.sMAN.u32ReqNumOfElements);

  MTLK_ASSERT(nof_bds < ((uint8)-1));

  res = _mtlk_advanced_bdr_init(&hw->tx_man, 
                                (uint8)nof_bds, 
                                hw->mmb_pas + MAC_TO_HOST32(hw->chi_data.sMAN.u32ReqStartOffset),
                                _mtlk_mmb_cm_bdr_get_iom_bd_size(hw, TRUE),
                                sizeof(mtlk_hw_cm_req_mirror_t) + hw->mmb->cfg.man_msg_size);
  if (res == MTLK_ERR_OK) {
    _mtlk_mmb_dbg_init_msg_verifier(&hw->tx_man.basic);
    hw->flags |= HW_FLAGS_PREPARED_MAN_REQ_BDR;
  }

  return res;
}

static int
_mtlk_mmb_prepare_man_ind_bdr(mtlk_hw_t *hw)
{
  int res = MTLK_ERR_UNKNOWN;

  /* Management Indications BD initialization */
  uint32 nof_bds = MAC_TO_HOST32(hw->chi_data.sMAN.u32IndNumOfElements);

  MTLK_ASSERT(nof_bds < ((uint8)-1));

  res = _mtlk_basic_bdr_init(&hw->rx_man,
                             (uint8)nof_bds,
                             hw->mmb_pas + MAC_TO_HOST32(hw->chi_data.sMAN.u32IndStartOffset),
                             _mtlk_mmb_cm_bdr_get_iom_bd_size(hw, TRUE),
                             sizeof(mtlk_hw_cm_ind_mirror_t) + hw->mmb->cfg.man_msg_size);
  if (res == MTLK_ERR_OK) {
    hw->flags |= HW_FLAGS_PREPARED_MAN_IND_BDR;
  }

  return res;
}

static int
_mtlk_mmb_prepare_dbg_req_bdr(mtlk_hw_t *hw)
{
  /* DBG Requests BD initialization */
  int    res     = MTLK_ERR_UNKNOWN;
  uint32 nof_bds = MAC_TO_HOST32(hw->chi_data.sDBG.u32ReqNumOfElements);

  MTLK_ASSERT(nof_bds < ((uint8)-1));

  res = _mtlk_advanced_bdr_init(&hw->tx_dbg,
                                (uint8)nof_bds, 
                                hw->mmb_pas + MAC_TO_HOST32(hw->chi_data.sDBG.u32ReqStartOffset),
                                _mtlk_mmb_cm_bdr_get_iom_bd_size(hw, FALSE),
                                sizeof(mtlk_hw_cm_req_mirror_t) + hw->mmb->cfg.dbg_msg_size);

  if (res == MTLK_ERR_OK) {
    _mtlk_mmb_dbg_init_msg_verifier(&hw->tx_dbg.basic);
    hw->flags |= HW_FLAGS_PREPARED_DBG_REQ_BDR;
  }

  return res;
}

static int
_mtlk_mmb_prepare_dbg_ind_bdr(mtlk_hw_t *hw)
{
  int res = MTLK_ERR_UNKNOWN;
  /* DBG Indications BD initialization */
  uint32 nof_bds = MAC_TO_HOST32(hw->chi_data.sDBG.u32IndNumOfElements);

  MTLK_ASSERT(nof_bds < ((uint8)-1));

  res = _mtlk_basic_bdr_init(&hw->rx_dbg,
                             (uint8)nof_bds,
                             hw->mmb_pas + MAC_TO_HOST32(hw->chi_data.sDBG.u32IndStartOffset),
                             _mtlk_mmb_cm_bdr_get_iom_bd_size(hw, FALSE),
                             sizeof(mtlk_hw_cm_ind_mirror_t) + hw->mmb->cfg.dbg_msg_size);
  if (res == MTLK_ERR_OK) {
    hw->flags |= HW_FLAGS_PREPARED_DBG_IND_BDR;
  }

  return res;
}

static int
_mtlk_mmb_prepare_data_req_bdr(mtlk_hw_t *hw)
{
  /* Data Requests BD initialization */
  int    res     = MTLK_ERR_UNKNOWN;
  uint32 nof_bds = MAC_TO_HOST32(hw->chi_data.sDAT.u32ReqNumOfElements);

  MTLK_ASSERT(nof_bds < ((uint8)-1));

  res = _mtlk_advanced_bdr_init(&hw->tx_data,
                                (uint8)nof_bds, 
                                hw->mmb_pas + MAC_TO_HOST32(hw->chi_data.sDAT.u32ReqStartOffset),
                                sizeof(SHRAM_DAT_REQ_MSG),
                                sizeof(mtlk_hw_data_req_mirror_t));
  if (res == MTLK_ERR_OK) {
    hw->tx_data_nof_free_bds = nof_bds;
    hw->tx_data_max_used_bds = 0;
    hw->flags |= HW_FLAGS_PREPARED_DATA_REQ_BDR;
  }

  return res;
}

static int
_mtlk_mmb_prepare_data_ind_bdr(mtlk_hw_t *hw)
{
  int res = MTLK_ERR_UNKNOWN;
  /* Data Indications BD initialization */
  uint32 nof_bds = MAC_TO_HOST32(hw->chi_data.sDAT.u32IndNumOfElements);

  MTLK_ASSERT(nof_bds < ((uint8)-1));

  res = _mtlk_basic_bdr_init(&hw->rx_data,
                             (uint8)nof_bds,
                             hw->mmb_pas + MAC_TO_HOST32(hw->chi_data.sDAT.u32IndStartOffset),
                             sizeof(RXDAT_IND_MSG_DESC),
                             sizeof(mtlk_hw_data_ind_mirror_t));
  if (res == MTLK_ERR_OK) {
    hw->flags |= HW_FLAGS_PREPARED_DATA_IND_BDR;
  }

  return res;
}

static int
_mtlk_mmb_prepare_log_ind_bdr(mtlk_hw_t *hw)
{
  int res = MTLK_ERR_UNKNOWN;
  uint32 nof_bds  = MAC_TO_HOST32(hw->fw_logger.ext_data.u32NumOfBufferDescriptors);
  uint32 bdr_offs = MAC_TO_HOST32(hw->fw_logger.ext_data.u32BufferDescriptorsLocation);

  /* Logger data Indications BD initialization */
  MTLK_ASSERT(hw->fw_logger.is_supported);

  res = _mtlk_basic_bdr_init(&hw->fw_logger.log_buffers,
                             nof_bds,
                             hw->mmb_pas + bdr_offs,
                             sizeof(BUFFER_DAT_IND_MSG_DESC),
                             sizeof(mtlk_hw_log_ind_mirror_t));
  if (res == MTLK_ERR_OK) {
    hw->flags |= HW_FLAGS_PREPARED_LOG_IND_BDR;
  }

  return res;
}

static void
_mtlk_mmb_free_unconfirmed_tx_buffers(mtlk_hw_t *hw)
{
  ILOG3_V("Freeing unconfirmed TX buffers");

  while (TRUE) {
    mtlk_hw_data_req_mirror_t  *data_req;
    mtlk_dlist_entry_t         *node = 
      mtlk_dlist_pop_front(&hw->tx_data.used_list);

    if (!node) {
      break; /* No more buffers */
    }

    data_req = MTLK_LIST_GET_CONTAINING_RECORD(node, 
                                               mtlk_hw_data_req_mirror_t,
                                               hdr.list_entry);

    if (data_req->dma_addr) {
      mtlk_df_nbuf_unmap_phys_addr(hw->vap_manager,
                                   data_req->nbuf,
                                   data_req->dma_addr,
                                   data_req->size,
                                   MTLK_DATA_TO_DEVICE);
    }

    /* Simply free the buffers without any statistic update due to with next steps
       card will be completely deleted. */
    mtlk_df_nbuf_free(hw->vap_manager, data_req->nbuf);

  }
}

static int
_mtlk_mmb_txmm_restore (mtlk_hw_t *hw)
{
  int res;
  mtlk_txmm_restore(&hw->txmm_base);
  res = mtlk_txmm_start(&hw->txmm_base);
  return res;
}

static int
_mtlk_mmb_txdm_restore (mtlk_hw_t *hw)
{
  int res;
  mtlk_txmm_restore(&hw->txdm_base);
  res = mtlk_txmm_start(&hw->txdm_base);
  return res;
}

static void
_mtlk_mmb_set_fw_logger_buffers (mtlk_hw_t *hw){

  int i;

  for (i = 0; i < hw->fw_logger.log_buffers.nof_bds; i++){
    BUFFER_DAT_IND_MSG_DESC bd;
    mtlk_hw_log_ind_mirror_t *log_ind =
      _mtlk_basic_bdr_get_mirror_bd(&hw->fw_logger.log_buffers, i, mtlk_hw_log_ind_mirror_t);

    MTLK_ASSERT(0 != log_ind->dma_addr);
    bd.u32HostPayloadAddr = HOST_TO_MAC32(log_ind->dma_addr);

    _mtlk_mmb_memcpy_toio(_mtlk_basic_bdr_get_iom_bd(&hw->fw_logger.log_buffers, i, BUFFER_DAT_IND_MSG_DESC),
                          &bd,
                          sizeof(bd));
  }
}

static void
_mtlk_mmb_empty_pending_rx_buffers (mtlk_hw_t *hw)
{
  int que_idx;
  mtlk_slist_entry_t *list_entry;

  while (1) {
    mtlk_hw_data_ind_mirror_t *data_ind;

    list_entry = mtlk_lslist_pop(&hw->rx_data_pending.lbufs);
    if (NULL == list_entry)
      break;

    data_ind = MTLK_LIST_GET_CONTAINING_RECORD(list_entry,
                                               mtlk_hw_data_ind_mirror_t,
                                               pend_l);

    hw->rx_data_queues.queue[data_ind->que_idx].que_size++;
  }

  /* Ensure no leaks in queues */
  for (que_idx = 0; que_idx < hw->rx_data_queues.nof_in_use; que_idx ++) {
    MTLK_ASSERT(hw->rx_data_queues.queue[que_idx].que_size ==
                hw->rx_data_queues.queue[que_idx].max_que_size);
  }
}

static void
_mtlk_mmb_free_preallocated_rx_buffers (mtlk_hw_t *hw)
{
  uint16 i = 0;

  if (!(hw->flags & HW_FLAGS_PREPARED_RX_BUFFERS)) {
    return;
  }

  for (i = 0; i < hw->rx_data.nof_bds; i++) {
    mtlk_hw_data_ind_mirror_t *data_ind =
      _mtlk_basic_bdr_get_mirror_bd(&hw->rx_data, i, mtlk_hw_data_ind_mirror_t);

    if (!data_ind->nbuf)
      continue;

    mtlk_df_nbuf_unmap_phys_addr(hw->vap_manager,
                                 data_ind->nbuf,
                                 data_ind->dma_addr,
                                 data_ind->size,
                                 MTLK_DATA_FROM_DEVICE);

    _mtlk_mmb_nbuf_free(hw, data_ind->nbuf);
  }

  hw->flags &= ~HW_FLAGS_PREPARED_RX_BUFFERS;
}

static int
_mtlk_mmb_prealloc_rx_buffers (mtlk_hw_t *hw)
{
  uint8  nque      = 0;
  uint16 bd_index  = 0;

  /* TODO: alloc buffers in the reverse order (bigger first) */
  for (nque = 0; nque < hw->rx_data_queues.nof_in_use; nque++) {
    mtlk_hw_rx_queue_t *queue = &hw->rx_data_queues.queue[nque];
    uint16              i     = 0;
    int                 ares  = MTLK_ERR_OK;
    queue->que_size = 0;
    for (i = 0; i < queue->max_que_size ; i++) {
      mtlk_hw_data_ind_mirror_t *data_ind =
        _mtlk_basic_bdr_get_mirror_bd(&hw->rx_data, bd_index + i, mtlk_hw_data_ind_mirror_t);

      data_ind->que_idx = nque;
      data_ind->size = queue->buf_size;

      if (ares == MTLK_ERR_OK) {
         /* No holes in BDR pointers yet: 1st allocation, 
            or all the previous allocations succeeded 
         */
        ares = _mtlk_mmb_alloc_and_set_rx_buffer(hw, data_ind);
        if (ares != MTLK_ERR_OK) {
          /* BRD pointer hole produced here (NULL-pointer) */
           ILOG2_D("WARNING: Can't preallocate buffer of %d bytes.",
               (int)queue->buf_size);
        }
        else {
          queue->que_size++;
        }
      }

      if (ares != MTLK_ERR_OK) {
        /* Some allocation has failed.
           The rest of allocations for this queue will be inserted to
           pending list, since the MAC can't handle holes and runs untill
           1st NULL.
           Then we'll try to recover (reallocate)
         */
        _mtlk_mmb_set_rx_payload_addr(hw, data_ind->hdr.index, (uint32)NULL);
        mtlk_lslist_push(&hw->rx_data_pending.lbufs, &data_ind->pend_l);
      }
    }

    ILOG2_DDDD("Total %d from %d buffers allocated for queue#%d (%d bytes each)",
         (int)queue->que_size,
         (int)queue->max_que_size,
         (int)nque,
         (int)queue->buf_size);

    bd_index = bd_index + queue->max_que_size;
  }

  hw->flags |= HW_FLAGS_PREPARED_RX_BUFFERS;
  return MTLK_ERR_OK;
}

static int
_mtlk_mmb_send_msg (mtlk_hw_t *hw, 
                    uint8      msg_type,
                    uint8      msg_index,
                    uint16     msg_info)
{
  IND_REQ_BUF_DESC_ELEM elem;
  mtlk_handle_t         lock_val;

  MTLK_ASSERT(NULL != hw->ccr);

  elem.u8Type  = msg_type;
  elem.u8Index = msg_index;
  elem.u16Info = HOST_TO_MAC16(msg_info);

  ILOG5_DDD("MSG WRITE: t=%d idx=%d inf=%d", 
       (int)msg_type, (int)msg_index, (int)msg_info);

  lock_val = mtlk_osal_lock_acquire_irq(&hw->reg_lock);

  _mtlk_mmb_pas_put(hw, 
                    "new BD descriptor",
                    hw->bds.req.offset + (hw->bds.req.idx * sizeof(elem)), /* DB Array [BD Idx] */
                    &elem,
                    sizeof(elem));

  hw->bds.req.idx++;
  if (hw->bds.req.idx >= hw->bds.req.size)
    hw->bds.req.idx = 0;  

  mtlk_ccr_initiate_doorbell_inerrupt(hw->ccr);

  mtlk_osal_lock_release_irq(&hw->reg_lock, lock_val);

  return MTLK_ERR_OK;
}

static int
_mtlk_mmb_send_sw_reset_mac_req(mtlk_hw_t *hw)
{
  int               res       = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txmm_mirror, NULL);
  if (!man_entry) {
    ELOG_V("Can't send request to MAC due to the lack of MAN_MSG");
    goto FINISH;
  }

  man_entry->id           = UM_MAN_SW_RESET_MAC_REQ;
  man_entry->payload_size = 0;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_SW_RESET_CFM_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_V("Can't send sw reset request to MAC, timed-out");
    goto FINISH;
  }

  res = MTLK_ERR_OK;

FINISH:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);
  return res;
}

static void
_mtlk_mmb_cleanup_reset_mac(mtlk_hw_t *hw)
{
  MTLK_ASSERT(NULL != hw->ccr);

  WLOG_V("FW CPU reset is done");

  mtlk_ccr_put_cpus_to_reset(hw->ccr);
  mtlk_ccr_clear_boot_from_bus(hw->ccr);
  mtlk_ccr_put_ctl_to_reset(hw->ccr);
}

static void
_mtlk_mmb_stop_events_completely(mtlk_hw_t *hw)
{
  /* NOTE: mac_events_stopped must be also set here to avoid additional checks
   * in _mtlk_mmb_handle_received_msg() (hw->mac_events_stopped || hw->mac_events_stopped_completely) */
  hw->mac_events_stopped            = 1;
  hw->mac_events_stopped_completely = 1;
}

static void
_mtlk_mmb_reset_all_events(mtlk_hw_t *hw)
{
    hw->mac_events_stopped            = 0;
    hw->mac_events_stopped_completely = 0;
}

static void
_mtlk_mmb_cleanup_calibration_cache(mtlk_hw_t *hw)
{
  mtlk_unmap_phys_addr(hw->vap_manager,
                       hw->calibr.dma_addr,
                       hw->calibr.ext_data.u32BufferRequestedSize,
                       MTLK_DATA_FROM_DEVICE);
  mtlk_osal_mem_free(hw->calibr.buffer);
  memset(&hw->calibr, 0, sizeof(hw->calibr));
}

static void _mtlk_mmb_init_calibration_ext(mtlk_hw_t *hw)
{
  /* SWAP extension's fields filled by FW to HOST format */
  hw->calibr.ext_data.u32BufferRequestedSize =
    MAC_TO_HOST32(hw->calibr.ext_data.u32BufferRequestedSize);
  hw->calibr.ext_data.u32DescriptorLocation =
    MAC_TO_HOST32(hw->calibr.ext_data.u32DescriptorLocation);

  ILOG2_DD("DEBUG: Calibration Cache req_size=%d location=0x%08x",
        (int)hw->calibr.ext_data.u32BufferRequestedSize,
        hw->calibr.ext_data.u32DescriptorLocation);
}

static void _mtlk_mmb_set_calibration_buffers (mtlk_hw_t *hw)
{
  /* Get DMA address of calibration buffer and write it to
     the dedicated location */
  hw->calibr.dma_addr = mtlk_map_to_phys_addr(hw->vap_manager,
                                              hw->calibr.buffer,
                                              hw->calibr.ext_data.u32BufferRequestedSize,
                                              MTLK_DATA_FROM_DEVICE);
  MTLK_ASSERT(0 != hw->calibr.dma_addr);

  ILOG2_D("hw->calibr.dma_addr = 0x%08x",hw->calibr.dma_addr);

  _mtlk_mmb_pas_writel(hw, "Calibration Cache buffer pointer",
    hw->calibr.ext_data.u32DescriptorLocation, hw->calibr.dma_addr);

  ILOG2_PDDD("DEBUG: Calibration Cache buffer pointer (v=0x%p, d=0x%08x, s=%d) written to 0x%08x",
        hw->calibr.buffer,
        hw->calibr.dma_addr,
        (int)hw->calibr.ext_data.u32BufferRequestedSize,
        hw->calibr.ext_data.u32DescriptorLocation);
}

static int
_mtlk_mmb_init_calibration_cache (mtlk_hw_t *hw)
{
  _mtlk_mmb_init_calibration_ext(hw);

  hw->calibr.buffer = mtlk_osal_mem_dma_alloc(hw->calibr.ext_data.u32BufferRequestedSize,
                                              MTLK_MEM_TAG_EXTENSION);
  if (hw->calibr.buffer) {
    _mtlk_mmb_set_calibration_buffers(hw);
    return MTLK_ERR_OK;
  }
  else {
    WLOG_D("Can't allocate Calibration buffer of %d bytes",
           (int)hw->calibr.ext_data.u32BufferRequestedSize);
    return MTLK_ERR_NO_MEM;
  }
}

static void
_mtlk_mmb_restore_calibration_cache (mtlk_hw_t *hw)
{
  _mtlk_mmb_init_calibration_ext(hw);

  mtlk_unmap_phys_addr(hw->vap_manager,
                       hw->calibr.dma_addr,
                       hw->calibr.ext_data.u32BufferRequestedSize,
                       MTLK_DATA_FROM_DEVICE);

  _mtlk_mmb_set_calibration_buffers(hw);
}

static void
_mtlk_mmb_init_mips_control(mtlk_hw_t *hw)
{
  hw->mips_ctrl.ext_data.u32DescriptorLocation = 
    MAC_TO_HOST32(hw->mips_ctrl.ext_data.u32DescriptorLocation);
  ILOG2_D("MIPS Ctrl Descriptor PAS offset: 0x%x", 
        hw->mips_ctrl.ext_data.u32DescriptorLocation);
}

static void
_mtlk_mmb_cleanup_fw_logger(mtlk_hw_t *hw)
{
  int i;

  for(i = 0; i < MAC_TO_HOST32(hw->fw_logger.ext_data.u32NumOfBufferDescriptors); i++) {
    mtlk_hw_log_ind_mirror_t *log_ind = 
      _mtlk_basic_bdr_get_mirror_bd(&hw->fw_logger.log_buffers, i, mtlk_hw_log_ind_mirror_t);

    if(NULL == log_ind->virt_addr)
      continue;

    MTLK_ASSERT(log_ind->dma_addr);

    mtlk_unmap_phys_addr(hw->vap_manager, log_ind->dma_addr,
                         log_ind->bufsize, MTLK_DATA_FROM_DEVICE);
    mtlk_osal_mem_free(log_ind->virt_addr);
    log_ind->virt_addr = NULL;
  }

  _mtlk_mmb_cleanup_log_ind_bdr(hw);
}

#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
static void
__mtlk_mmb_prepare_data_fw_logger (mtlk_hw_t *hw, uint32 bp_addr, int buffer_cnt, int buffer_size)
{
  int i;
  uint8 *data_start = NULL;
  uint32 *val4bytes = NULL;
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(0 != buffer_size);
  MTLK_ASSERT(0 != buffer_cnt);

  data_start = mtlk_osal_mem_alloc(MTLK_IPAD4(buffer_size + sizeof(uint32)), MTLK_MEM_TAG_FW_LOGGER);
  if (NULL == data_start) {
    ELOG_V("Can't allocate memory for FW Logger. No data is parsed");
    goto finish;
  }

  val4bytes = mtlk_osal_mem_alloc(sizeof(uint32), MTLK_MEM_TAG_FW_LOGGER);
  if (NULL == val4bytes) {
    ELOG_V("Can't allocate memory for FW Logger. No data is parsed");
    goto finish;
  }

  for (i = 0; i < buffer_cnt; i++) {
    int offset = 0;
    _mtlk_mmb_pas_get(hw, "FW logger buffer", bp_addr + (i * (buffer_size + sizeof(uint32))), (void *)data_start, MTLK_IPAD4(buffer_size + sizeof(uint32)));

    memcpy(val4bytes, data_start, sizeof(uint32));
    *val4bytes = MAC_TO_HOST32(*val4bytes);

    if (*val4bytes != USED_NSSBUF)
      continue;

    offset += sizeof(uint32);
    while ((offset <= buffer_size) && (*val4bytes != LOGGER_END_OF_BUFFER_MARK)) {
      memcpy(val4bytes, &data_start[offset], sizeof(uint32));
      *val4bytes = MAC_TO_HOST32(*val4bytes);
      offset += sizeof(uint8);
    }
    offset -= sizeof(uint8);
    if (*val4bytes == LOGGER_END_OF_BUFFER_MARK)
      offset -= sizeof(uint32);
    if (offset > buffer_size)
      offset = buffer_size;
    if (offset) {
      res = _mtlk_mmb_push_logger_buf(hw, data_start + sizeof(uint32), offset);
    }
  }

finish:
  if (val4bytes) {
    mtlk_osal_mem_free(val4bytes);
  }
  if (data_start) {
    mtlk_osal_mem_free(data_start);
  }
}

static void
_mtlk_mmb_parse_fw_logger (mtlk_hw_t *hw)
{  
  uint32 bp_addr = 0;
  uint16 nof_bdr = 0;
  uint16 size = 0;

  bp_addr = MAC_TO_HOST32(hw->fw_logger.buf_info_ext_data.lmBufferInfo.u32BufferPoolAddress);
  if (bp_addr) {
    nof_bdr = MAC_TO_HOST16(hw->fw_logger.buf_info_ext_data.lmBufferInfo.u16NumberOfBuffers);
    size = MAC_TO_HOST16(hw->fw_logger.buf_info_ext_data.lmBufferInfo.u16Size);
    __mtlk_mmb_prepare_data_fw_logger(hw, bp_addr, nof_bdr, size);
  }

  bp_addr = MAC_TO_HOST32(hw->fw_logger.buf_info_ext_data.umBufferInfo.u32BufferPoolAddress);
  if (bp_addr) {
    nof_bdr = MAC_TO_HOST16(hw->fw_logger.buf_info_ext_data.umBufferInfo.u16NumberOfBuffers);
    size = MAC_TO_HOST16(hw->fw_logger.buf_info_ext_data.umBufferInfo.u16Size);
    __mtlk_mmb_prepare_data_fw_logger(hw, bp_addr, nof_bdr, size);
  }
}
#endif /* (RTLOG_FLAGS & RTLF_REMOTE_ENABLED) */

static int
_mtlk_mmb_alloc_fw_logger_buffers (mtlk_hw_t *hw, int start_idx, int count, uint16 bufsize)
{
  int res = MTLK_ERR_OK;
  int i;

  for (i = start_idx; i < (start_idx + count); i++) {
    mtlk_hw_log_ind_mirror_t *log_ind = 
      _mtlk_basic_bdr_get_mirror_bd(&hw->fw_logger.log_buffers, i, mtlk_hw_log_ind_mirror_t);

    log_ind->virt_addr = mtlk_osal_mem_dma_alloc(bufsize, MTLK_MEM_TAG_FW_LOGGER);
    if(NULL == log_ind->virt_addr) {
      res = MTLK_ERR_NO_MEM;
      break;
    }

    if (NULL != log_ind->virt_addr) {
      log_ind->bufsize = bufsize;
      log_ind->dma_addr = mtlk_map_to_phys_addr(hw->vap_manager, log_ind->virt_addr,
                                                bufsize, MTLK_DATA_FROM_DEVICE);
    }
  }

  return res;
}

static int
_mtlk_mmb_init_fw_logger (mtlk_hw_t *hw)
{
  int res = MTLK_ERR_OK;
  uint32 nof_bdr = 0;
  uint16 nof_bdr_lm = 0;
  uint16 nof_bdr_um = 0;
  uint16 bufsize_lm = 0;
  uint16 bufsize_um = 0;

  res = _mtlk_mmb_prepare_log_ind_bdr(hw);
  if (MTLK_ERR_OK != res)
    return res;

  nof_bdr = MAC_TO_HOST32(hw->fw_logger.ext_data.u32NumOfBufferDescriptors);

  if (hw->fw_logger.is_supported_ex) {
    nof_bdr_lm = MAC_TO_HOST16(hw->fw_logger.buf_info_ext_data.lmBufferInfo.u16NumberOfBuffers);
    nof_bdr_um = MAC_TO_HOST16(hw->fw_logger.buf_info_ext_data.umBufferInfo.u16NumberOfBuffers);

    if (nof_bdr != (nof_bdr_lm + nof_bdr_um)) {
      ELOG_DDD("Incorrect FW logger capabilities: %d != %d + %d", nof_bdr, nof_bdr_lm, nof_bdr_um);
      return MTLK_ERR_PARAMS;
    }

    bufsize_lm = MAC_TO_HOST16(hw->fw_logger.buf_info_ext_data.lmBufferInfo.u16Size);
    bufsize_um = MAC_TO_HOST16(hw->fw_logger.buf_info_ext_data.umBufferInfo.u16Size);

    if ((nof_bdr_lm) && (bufsize_lm)) {
      res |= _mtlk_mmb_alloc_fw_logger_buffers(hw, 0, nof_bdr_lm, bufsize_lm);
    }
    if ((nof_bdr_um) && (bufsize_um)) {
      res |= _mtlk_mmb_alloc_fw_logger_buffers(hw, nof_bdr_lm, nof_bdr_um, bufsize_um);
    }
  }
  else {
    res = _mtlk_mmb_alloc_fw_logger_buffers(hw, 0, nof_bdr, 120);
  }

  if (MTLK_ERR_OK == res) {
    _mtlk_mmb_set_fw_logger_buffers(hw);
  }
  else {
    _mtlk_mmb_cleanup_fw_logger(hw);
  }

  return res;
}

static void
_mtlk_mmb_init_fw_capabilities (mtlk_hw_t *hw)
{
  hw->fw_capabilities.nof_stas.u32NumOfStations = 
    MAC_TO_HOST32(hw->fw_capabilities.nof_stas.u32NumOfStations);
  hw->fw_capabilities.nof_vaps.u32NumOfVaps = 
    MAC_TO_HOST32(hw->fw_capabilities.nof_vaps.u32NumOfVaps);
  ILOG1_DD("FW supports %d STAs %d VAPs", 
        hw->fw_capabilities.nof_stas.u32NumOfStations,
        hw->fw_capabilities.nof_vaps.u32NumOfVaps);
}

MTLK_INIT_STEPS_LIST_BEGIN(hw_mmb_card)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_IRB_ALLOC)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_IRB_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_WSS_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_SOURCE_CNTRs)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_RX_DATA_LIST)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_REQ_BD_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_INIT_EVT)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_RX_PEND_TIMER)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_EEPROM)
MTLK_INIT_INNER_STEPS_BEGIN(hw_mmb_card)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb_card, HW_IRB_NAME)
MTLK_INIT_STEPS_LIST_END(hw_mmb_card);


MTLK_START_STEPS_LIST_BEGIN(hw_mmb_card)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_MASTER_VAP)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_POWER_ON)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_BIST_READ)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_EEPROM_READ)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_CHI_INIT)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_LOAD_FIRMWARE)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_RUN_FIRMWARE)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_WAIT_CHI_MAGIC)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_MAN_REQ_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_MAN_IND_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_DBG_REQ_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_DBG_IND_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_DAT_REQ_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_DAT_IND_BDR)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_RX_QUEUES)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_RX_BUFFERS)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_CALIBRATION_CACHE)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_MIPS_CONTROL)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_FW_LOGGER)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_FW_CAPABILITIES)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_TXMM)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_TXDM)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_SEND_READY)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_HW_DEP_CFG)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_FW_LOG_SEVERITY)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_FW_LOG_MODE)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_RX_PEND_TIMER)
  MTLK_START_STEPS_LIST_ENTRY(hw_mmb_card, HW_STAT_REQ_HANDLER)
MTLK_START_INNER_STEPS_BEGIN(hw_mmb_card)
MTLK_START_STEPS_LIST_END(hw_mmb_card);

void __MTLK_IFUNC 
mtlk_hw_mmb_stop_card(mtlk_hw_t *hw)
{
  int res;
  mtlk_vap_handle_t master_vap_handle;
  uint32 mac_soft_reset_enable = 0;
  int exception = (hw->state == MTLK_HW_STATE_EXCEPTION) || 
                  (hw->state == MTLK_HW_STATE_APPFATAL) ||
                  (hw->state == MTLK_HW_STATE_MAC_ASSERTED);
  
  MTLK_ASSERT(NULL != hw->ccr);

  hw->state = MTLK_HW_STATE_UNLOADING;

  res = mtlk_vap_manager_get_master_vap(hw->vap_manager, &master_vap_handle);
  if (MTLK_ERR_OK == res)
  {
    res = mtlk_vap_get_core_vft(master_vap_handle)->get_prop(master_vap_handle,
                           MTLK_CORE_PROP_MAC_SW_RESET_ENABLED,
                           &mac_soft_reset_enable,
                           sizeof(mac_soft_reset_enable));
  }
  if (res != MTLK_ERR_OK) {
    mac_soft_reset_enable = 0;
  }

  MTLK_STOP_BEGIN(hw_mmb_card, MTLK_OBJ_PTR(hw))
    MTLK_STOP_STEP(hw_mmb_card, HW_STAT_REQ_HANDLER, MTLK_OBJ_PTR(hw), 
                   mtlk_wssd_unregister_request_handler, (hw->irbd, hw->stat_irb_handle));

    MTLK_STOP_STEP(hw_mmb_card, HW_RX_PEND_TIMER, MTLK_OBJ_PTR(hw), 
                   mtlk_osal_timer_cancel_sync, (&hw->rx_data_pending.timer));

    MTLK_STOP_STEP(hw_mmb_card, HW_FW_LOG_MODE, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_send_fw_log_mode_off, (hw));

    MTLK_STOP_STEP(hw_mmb_card, HW_FW_LOG_SEVERITY, MTLK_OBJ_PTR(hw), MTLK_NOACTION,());

    MTLK_STOP_STEP(hw_mmb_card, HW_HW_DEP_CFG, MTLK_OBJ_PTR(hw), MTLK_NOACTION,());

    MTLK_STOP_STEP(hw_mmb_card, HW_SEND_READY, MTLK_OBJ_PTR(hw), MTLK_NOACTION,());

    if (hw->mac_reset_logic_initialized && !exception) {
      ILOG3_V("Calling _mtlk_pci_send_sw_reset_mac_req");
      if (_mtlk_mmb_send_sw_reset_mac_req(hw) != MTLK_ERR_OK) {
        hw->mac_reset_logic_initialized = FALSE;
      }
    } 
    else if (exception && (mac_soft_reset_enable == 0)) {
      hw->mac_reset_logic_initialized = FALSE;
    }

    mtlk_ccr_disable_interrupts(hw->ccr);

    _mtlk_mmb_stop_events_completely(hw);
    hw->isr_type = MTLK_ISR_NONE;

    MTLK_STOP_STEP(hw_mmb_card, HW_TXDM, MTLK_OBJ_PTR(hw), 
                   mtlk_txmm_stop, (&hw->txdm_base));
    MTLK_STOP_STEP(hw_mmb_card, HW_TXMM, MTLK_OBJ_PTR(hw), 
                   mtlk_txmm_stop, (&hw->txmm_base));
    MTLK_STOP_STEP(hw_mmb_card, HW_FW_CAPABILITIES, MTLK_OBJ_PTR(hw),
                   MTLK_NOACTION, ());
    MTLK_STOP_STEP(hw_mmb_card, HW_FW_LOGGER, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_cleanup_fw_logger, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_MIPS_CONTROL, MTLK_OBJ_PTR(hw),
                   MTLK_NOACTION, ());
    MTLK_STOP_STEP(hw_mmb_card, HW_CALIBRATION_CACHE, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_cleanup_calibration_cache, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_RX_BUFFERS, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_free_preallocated_rx_buffers, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_RX_QUEUES, MTLK_OBJ_PTR(hw), 
                   MTLK_NOACTION, ());
    MTLK_STOP_STEP(hw_mmb_card, HW_DAT_IND_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_cleanup_data_ind_bdr, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_DAT_REQ_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_cleanup_data_req_bdr, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_DBG_IND_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_cleanup_dbg_ind_bdr, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_DBG_REQ_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_cleanup_dbg_req_bdr, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_MAN_IND_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_cleanup_man_ind_bdr, (hw));
    MTLK_STOP_STEP(hw_mmb_card, HW_MAN_REQ_BDR, MTLK_OBJ_PTR(hw),
                   _mtlk_mmb_cleanup_man_req_bdr, (hw));

    MTLK_STOP_STEP(hw_mmb_card, HW_WAIT_CHI_MAGIC, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());
    MTLK_STOP_STEP(hw_mmb_card, HW_RUN_FIRMWARE, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());
    MTLK_STOP_STEP(hw_mmb_card, HW_LOAD_FIRMWARE, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());
    MTLK_STOP_STEP(hw_mmb_card, HW_CHI_INIT, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());

    MTLK_STOP_STEP(hw_mmb_card, HW_EEPROM_READ, MTLK_OBJ_PTR(hw),
                   mtlk_clean_eeprom_data, (master_vap_handle, hw->ee_data));

    MTLK_STOP_STEP(hw_mmb_card, HW_BIST_READ, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());

    MTLK_STOP_STEP(hw_mmb_card, HW_POWER_ON, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());

#ifndef MTCFG_NO_FW_RESET_ON_STOP
    _mtlk_mmb_cleanup_reset_mac(hw);
#endif

    MTLK_STOP_STEP(hw_mmb_card, HW_MASTER_VAP, MTLK_OBJ_PTR(hw), MTLK_NOACTION, ());

  MTLK_STOP_END(hw_mmb_card, MTLK_OBJ_PTR(hw))
}

void __MTLK_IFUNC 
mtlk_hw_mmb_cleanup_card(mtlk_hw_t *hw)
{
  MTLK_CLEANUP_BEGIN(hw_mmb_card, MTLK_OBJ_PTR(hw))
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_EEPROM, MTLK_OBJ_PTR(hw),
                      mtlk_eeprom_delete, (hw->ee_data));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_RX_PEND_TIMER, MTLK_OBJ_PTR(hw),
                      mtlk_osal_timer_cleanup, (&hw->rx_data_pending.timer));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_INIT_EVT, MTLK_OBJ_PTR(hw),
                      MTLK_HW_INIT_EVT_CLEANUP, (hw));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_REQ_BD_LOCK, MTLK_OBJ_PTR(hw),
                      mtlk_osal_lock_cleanup, (&hw->reg_lock));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_RX_DATA_LIST, MTLK_OBJ_PTR(hw),
                      mtlk_lslist_cleanup, (&hw->rx_data_pending.lbufs));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_SOURCE_CNTRs, MTLK_OBJ_PTR(hw),
                      mtlk_wss_cntrs_close, (hw->wss, hw->wss_hcntrs,
                      MTLK_HW_SOURCE_CNT_LAST));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_WSS_CREATE, MTLK_OBJ_PTR(hw),
                      mtlk_wss_delete, (hw->wss));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_IRB_INIT, MTLK_OBJ_PTR(hw),
                      mtlk_irbd_cleanup, (hw->irbd));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_IRB_ALLOC, MTLK_OBJ_PTR(hw),
                      mtlk_irbd_free, (hw->irbd));
    hw->irbd = NULL;
  MTLK_CLEANUP_END(hw_mmb_card, MTLK_OBJ_PTR(hw));
}

int __MTLK_IFUNC 
mtlk_hw_mmb_init_card(mtlk_hw_t *hw, mtlk_ccr_t *ccr, unsigned char *mmb_pas,
                      mtlk_vap_manager_t *vap_manager)
{
  char irb_node_name[sizeof(MTLK_IRB_HW_NAME) + 3]; /* 3 chars for card index */
  int  tmp;

  MTLK_ASSERT(NULL != hw);
  MTLK_ASSERT(NULL != ccr);
  MTLK_ASSERT(NULL != mmb_pas);
  MTLK_ASSERT(NULL != vap_manager);
  MTLK_ASSERT(ARRAY_SIZE(_mtlk_hw_source_wss_id_map) == MTLK_HW_SOURCE_CNT_LAST);
  MTLK_ASSERT(ARRAY_SIZE(hw->wss_hcntrs) == MTLK_HW_SOURCE_CNT_LAST);

  hw->ccr = ccr;
  hw->mmb_pas = mmb_pas;
  hw->vap_manager = vap_manager;
  hw->state = MTLK_HW_STATE_INITIATING;

  MTLK_ASSERT(ARRAY_SIZE(_mtlk_hw_listener_wss_id_map) == MTLK_HW_CNT_LAST);

  MTLK_INIT_TRY(hw_mmb_card, MTLK_OBJ_PTR(hw))
    MTLK_INIT_STEP_EX(hw_mmb_card, HW_IRB_ALLOC, MTLK_OBJ_PTR(hw),
                      mtlk_irbd_alloc, (),
                      hw->irbd, hw->irbd != NULL, MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP_EX(hw_mmb_card, HW_IRB_NAME, MTLK_OBJ_NONE,
                      mtlk_snprintf, (irb_node_name, sizeof(irb_node_name), "%s%d", MTLK_IRB_HW_NAME, hw->card_idx),
                      tmp, tmp > 0 && tmp < sizeof(irb_node_name), MTLK_ERR_NO_RESOURCES);
    MTLK_INIT_STEP(hw_mmb_card, HW_IRB_INIT, MTLK_OBJ_PTR(hw),
                   mtlk_irbd_init, (hw->irbd, mtlk_dfg_get_driver_irbd(), irb_node_name));
    MTLK_INIT_STEP_EX(hw_mmb_card, HW_WSS_CREATE, MTLK_OBJ_PTR(hw),
                      mtlk_wss_create, (mtlk_dfg_get_driver_wss(), _mtlk_hw_listener_wss_id_map, ARRAY_SIZE(_mtlk_hw_listener_wss_id_map)),
                      hw->wss, hw->wss != NULL, MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP(hw_mmb_card, HW_SOURCE_CNTRs, MTLK_OBJ_PTR(hw),
                   mtlk_wss_cntrs_open, (hw->wss, _mtlk_hw_source_wss_id_map,
                   hw->wss_hcntrs, MTLK_HW_SOURCE_CNT_LAST));
    MTLK_INIT_STEP_VOID(hw_mmb_card, HW_RX_DATA_LIST, MTLK_OBJ_PTR(hw),
                        mtlk_lslist_init, (&hw->rx_data_pending.lbufs));
    MTLK_INIT_STEP(hw_mmb_card, HW_REQ_BD_LOCK, MTLK_OBJ_PTR(hw),
                   mtlk_osal_lock_init, (&hw->reg_lock));
    MTLK_INIT_STEP(hw_mmb_card, HW_INIT_EVT, MTLK_OBJ_PTR(hw),
                   MTLK_HW_INIT_EVT_INIT, (hw));
    MTLK_INIT_STEP(hw_mmb_card, HW_RX_PEND_TIMER, MTLK_OBJ_PTR(hw),
                   mtlk_osal_timer_init, (&hw->rx_data_pending.timer,
                                          _mtlk_mmb_on_rx_buffs_recovery_timer,
                                          HANDLE_T(hw)));
    MTLK_INIT_STEP_EX(hw_mmb_card, HW_EEPROM, MTLK_OBJ_PTR(hw),
                      mtlk_eeprom_create, (),
                      hw->ee_data, NULL != hw->ee_data, MTLK_ERR_NO_MEM);

    _mtlk_mmb_reset_all_events(hw);

  MTLK_INIT_FINALLY(hw_mmb_card, MTLK_OBJ_PTR(hw));
    MTLK_CLEANUP_STEP(hw_mmb_card, HW_IRB_NAME, MTLK_OBJ_NONE, 
                      MTLK_NOACTION, ());
  MTLK_INIT_RETURN(hw_mmb_card, MTLK_OBJ_PTR(hw), mtlk_hw_mmb_cleanup_card, (hw));
}

static void
_mtlk_mmb_get_recovery_stats (mtlk_hw_t *hw, mtlk_wssa_drv_recovery_stats_t *stats)
{
  stats->FastRcvryProcessed = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_NOF_FAST_RCVRY_PROCESSED);
  stats->FullRcvryProcessed = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_NOF_FULL_RCVRY_PROCESSED);
  stats->FastRcvryFailed    = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_NOF_FAST_RCVRY_FAILED);
  stats->FullRcvryFailed    = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_NOF_FULL_RCVRY_FAILED);
}

static void
_mtlk_mmb_get_hw_stats(mtlk_hw_t* hw, mtlk_wssa_drv_hw_stats_t* stats)
{
  mtlk_txmm_stats_t txmm_stats;

  /* minimal statistic level */

  /* TXMM stats */
  mtlk_txmm_base_get_stats(&hw->txmm_base, &txmm_stats);
  stats->txmm_sent = txmm_stats.nof_sent;
  stats->txmm_cfmd = txmm_stats.nof_cfmed;
  stats->txmm_peak = txmm_stats.used_peak;

  /* TXDM stats */
  mtlk_txmm_base_get_stats(&hw->txdm_base, &txmm_stats);
  stats->txdm_sent = txmm_stats.nof_sent;
  stats->txdm_cfmd = txmm_stats.nof_cfmed;
  stats->txdm_peak = txmm_stats.used_peak;

  stats->RxPacketsSucceeded = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_PACKETS_RECEIVED);
  stats->RxBytesSucceeded = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_BYTES_RECEIVED);
  stats->TxPacketsSucceeded = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_PACKETS_SENT);
  stats->TxBytesSucceeded = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_BYTES_SENT);
  stats->UnicastPacketsSent = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_UNICAST_PACKETS_SENT);
  stats->UnicastPacketsReceived = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_UNICAST_PACKETS_RECEIVED);
  stats->MulticastPacketsSent = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_MULTICAST_PACKETS_SENT);
  stats->MulticastPacketsReceived = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_MULTICAST_PACKETS_RECEIVED);
  stats->BroadcastPacketsSent = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_BROADCAST_PACKETS_SENT);
  stats->BroadcastPacketsReceived = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_BROADCAST_PACKETS_RECEIVED);
}

#if MTLK_MTIDL_HW_STAT_FULL
static void
_mtlk_mmb_get_debug_hw_stats(mtlk_hw_t* hw, mtlk_wssa_drv_debug_hw_stats_t* stats)
{
  mtlk_txmm_stats_t txmm_stats;

  /* minimal statistic level */

  /* TXMM stats */
  mtlk_txmm_base_get_stats(&hw->txmm_base, &txmm_stats);
  stats->txmm_sent = txmm_stats.nof_sent;
  stats->txmm_cfmd = txmm_stats.nof_cfmed;
  stats->txmm_peak = txmm_stats.used_peak;

  /* TXDM stats */
  mtlk_txmm_base_get_stats(&hw->txdm_base, &txmm_stats);
  stats->txdm_sent = txmm_stats.nof_sent;
  stats->txdm_cfmd = txmm_stats.nof_cfmed;
  stats->txdm_peak = txmm_stats.used_peak;

  stats->RxPacketsSucceeded = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_PACKETS_RECEIVED);
  stats->RxBytesSucceeded = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_BYTES_RECEIVED);
  stats->TxPacketsSucceeded = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_PACKETS_SENT);
  stats->TxBytesSucceeded = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_BYTES_SENT);
  stats->UnicastPacketsSent = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_UNICAST_PACKETS_SENT);
  stats->UnicastPacketsReceived = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_UNICAST_PACKETS_RECEIVED);
  stats->MulticastPacketsSent = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_MULTICAST_PACKETS_SENT);
  stats->MulticastPacketsReceived = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_MULTICAST_PACKETS_RECEIVED);
  stats->BroadcastPacketsSent = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_BROADCAST_PACKETS_SENT);
  stats->BroadcastPacketsReceived = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_BROADCAST_PACKETS_RECEIVED);

  /* add FULL statistic level */

  stats->RxPacketsDiscardedDrvTooOld = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD);
  stats->RxPacketsDiscardedDrvDuplicate = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE);

  stats->RxPackets802_1x = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_802_1X_PACKETS_RECEIVED);
  stats->TxPackets802_1x = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_802_1X_PACKETS_SENT);

  stats->PairwiseMICFailurePackets = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_PAIRWISE_MIC_FAILURE_PACKETS);
  stats->GroupMICFailurePackets = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_GROUP_MIC_FAILURE_PACKETS);
  stats->UnicastReplayedPackets = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_UNICAST_REPLAYED_PACKETS);
  stats->MulticastReplayedPackets = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_MULTICAST_REPLAYED_PACKETS);
  stats->FwdRxPackets = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_FWD_RX_PACKETS);
  stats->FwdRxBytes = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_FWD_RX_BYTES);
  stats->MulticastBytesSent = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_MULTICAST_BYTES_SENT);
  stats->MulticastBytesReceived = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_MULTICAST_BYTES_RECEIVED);
  stats->BroadcastBytesSent = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_BROADCAST_BYTES_SENT);
  stats->BroadcastBytesReceived = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_BROADCAST_BYTES_RECEIVED);

  stats->FreeTxMSDUs = hw->tx_data_nof_free_bds;
  stats->TxMSDUsUsagePeak = hw->tx_data_max_used_bds;
  stats->BISTCheckPassed = hw->mmb->bist_passed;

  stats->DATFramesReceived = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_DAT_FRAMES_RECEIVED);
  stats->CTLFramesReceived = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_CTL_FRAMES_RECEIVED);
  stats->MANFramesReceived = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_MAN_FRAMES_RECEIVED);
  stats->FWLoggerPacketsProcessed = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_FW_LOGGER_PACKETS_PROCESSED);
  stats->FWLoggerPacketsDropped = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_FW_LOGGER_PACKETS_DROPPED);

  stats->ISRsTotal      = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_ISRS_TOTAL);
  stats->ISRsForeign    = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_ISRS_FOREIGN);
  stats->ISRsNotPending = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_ISRS_NOT_PENDING);
  stats->ISRsHalted     = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_ISRS_HALTED);
  stats->ISRsInit       = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_ISRS_INIT);
  stats->ISRsToDPC      = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_ISRS_TO_DPC);
  stats->ISRsUnknown    = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_ISRS_UNKNOWN);
  stats->PostISRDPCs    = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_POST_ISR_DPCS);
  stats->FWMsgsHandled  = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_FW_MSGS_HANDLED);
  stats->SqDPCsScheduled= mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_SQ_DPCS_SCHEDULED);
  stats->SqDPCsArrived  = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_SQ_DPCS_ARRIVED);

  stats->RxAllocFailed  = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_BUF_ALLOC_FAILED);
  stats->RxReAllocFailed= mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_BUF_REALLOC_FAILED);
  stats->RxReAllocated  = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_BUF_REALLOCATED);

  /* Aggregation info */
  stats->Aggregation.AggrActive                 = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_AGGR_ACTIVE);
  stats->Aggregation.ReordActive                = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_REORD_ACTIVE);
  stats->Aggregation.RxAddbaReqReceived         = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_ADDBA_REQ_RECEIVED);
  stats->Aggregation.RxAddbaResCfmdFail         = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_ADDBA_RES_CFMD_FAIL);
  stats->Aggregation.RxAddbaResCfmdSuccess      = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_ADDBA_RES_CFMD_SUCCESS);
  stats->Aggregation.RxAddbaResLost             = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_ADDBA_RES_LOST);
  stats->Aggregation.RxAddbaResNegativeSent     = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_ADDBA_RES_NEGATIVE_SENT);
  stats->Aggregation.RxAddbaResNotCfmd          = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_ADDBA_RES_NOT_CFMD);
  stats->Aggregation.RxAddbaResPositiveSent     = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_ADDBA_RES_POSITIVE_SENT);
  stats->Aggregation.RxAddbaResReached          = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_ADDBA_RES_REACHED);
  stats->Aggregation.RxAddbaResRetransmissions  = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_ADDBA_RES_RETRANSMISSIONS);
  stats->Aggregation.RxBarWithoutReordering     = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_BAR_WITHOUT_REORDERING);
  stats->Aggregation.RxPktWithoutReordering     = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_AGGR_PKT_WITHOUT_REORDERING);
  stats->Aggregation.RxDelbaReqCfmdFail         = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_DELBA_REQ_CFMD_FAIL);
  stats->Aggregation.RxDelbaReqCfmdSuccess      = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_DELBA_REQ_CFMD_SUCCESS);
  stats->Aggregation.RxDelbaReqLost             = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_DELBA_REQ_LOST);
  stats->Aggregation.RxDelbaReqNotCfmd          = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_DELBA_REQ_NOT_CFMD);
  stats->Aggregation.RxDelbaReqRcv              = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_DELBA_REQ_RCV);
  stats->Aggregation.RxDelbaReqReached          = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_DELBA_REQ_REACHED);
  stats->Aggregation.RxDelbaReqRetransmissions  = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_DELBA_REQ_RETRANSMISSIONS);
  stats->Aggregation.RxDelbaReqSent             = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_DELBA_REQ_SENT);
  stats->Aggregation.RxDelbaSentByTimeout       = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_RX_DELBA_SENT_BY_TIMEOUT);

  stats->Aggregation.TxAckOnBarDetected         = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_ACK_ON_BAR_DETECTED);
  stats->Aggregation.TxAddbaReqCfmdFail         = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_ADDBA_REQ_CFMD_FAIL);
  stats->Aggregation.TxAddbaReqCfmdSuccess      = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_ADDBA_REQ_CFMD_SUCCESS);
  stats->Aggregation.TxAddbaReqNotCfmd          = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_ADDBA_REQ_NOT_CFMD);
  stats->Aggregation.TxAddbaReqSent             = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_ADDBA_REQ_SENT);
  stats->Aggregation.TxAddbaResRcvNegative      = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_ADDBA_RES_RCV_NEGATIVE);
  stats->Aggregation.TxAddbaResRcvPositive      = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_ADDBA_RES_RCV_POSITIVE);
  stats->Aggregation.TxAddbaResTimeout          = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_ADDBA_RES_TIMEOUT);
  stats->Aggregation.TxCloseAggrCfmdFail        = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_CLOSE_AGGR_CFMD_FAIL);
  stats->Aggregation.TxCloseAggrCfmdSuccess     = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_CLOSE_AGGR_CFMD_SUCCESS);
  stats->Aggregation.TxCloseAggrNotCfmd         = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_CLOSE_AGGR_NOT_CFMD);
  stats->Aggregation.TxCloseAggrSent            = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_CLOSE_AGGR_SENT);
  stats->Aggregation.TxDelbaReqCfmdFail         = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_DELBA_REQ_CFMD_FAIL);
  stats->Aggregation.TxDelbaReqCfmdSuccess      = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_DELBA_REQ_CFMD_SUCCESS);
  stats->Aggregation.TxDelbaReqLost             = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_DELBA_REQ_LOST);
  stats->Aggregation.TxDelbaReqNotCfmd          = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_DELBA_REQ_NOT_CFMD);
  stats->Aggregation.TxDelbaReqRcv              = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_DELBA_REQ_RCV);
  stats->Aggregation.TxDelbaReqReached          = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_DELBA_REQ_REACHED);
  stats->Aggregation.TxDelbaReqRetransmissions  = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_DELBA_REQ_RETRANSMISSIONS);
  stats->Aggregation.TxDelbaReqSent             = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_DELBA_REQ_SENT);
  stats->Aggregation.TxOpenAggrCfmdFail         = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_OPEN_AGGR_CFMD_FAIL);
  stats->Aggregation.TxOpenAggrCfmdSuccess      = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_OPEN_AGGR_CFMD_SUCCESS);
  stats->Aggregation.TxOpenAggrNotCfmd          = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_OPEN_AGGR_NOT_CFMD);
  stats->Aggregation.TxOpenAggrSent             = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_TX_OPEN_AGGR_SENT);
}
#endif /* MTLK_MTIDL_HW_STAT_FULL */

static int
_mtlk_mmb_request_general_fw_stats(mtlk_hw_t* hw, MTIDL_GeneralStats* stats)
{
   int                       res = MTLK_ERR_OK;
   mtlk_txmm_msg_t           man_msg;
   mtlk_txmm_data_t          *man_entry = NULL;
   UMI_GET_STATISTICS        *mac_stats;

   MTLK_ASSERT(hw != NULL);
   MTLK_ASSERT(stats != NULL);

   man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txdm_mirror, NULL);
   if (!man_entry) {
     ELOG_V("Can't get statistics due to the lack of MAN_MSG");
     res = MTLK_ERR_NO_RESOURCES;
     goto finish;
   }

   mac_stats = (UMI_GET_STATISTICS *)man_entry->payload;

   man_entry->id           = UM_DBG_GET_STATISTICS_REQ;
   man_entry->payload_size = sizeof(*mac_stats);
   mac_stats->u16Status       = 0;
   mac_stats->u16Ident        = HOST_TO_MAC16(GET_ALL_STATS);

   res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
   if (MTLK_ERR_OK != res) {
     ELOG_D("MAC Get Stat sending failure (%i)", res);
   } else if (UMI_OK != MAC_TO_HOST16(mac_stats->u16Status)) {
     ELOG_D("MAC Get Stat failure (%u)", MAC_TO_HOST16(mac_stats->u16Status));
     res = MTLK_ERR_MAC;
   } else {
     memcpy(stats, mac_stats->sStats.au32Statistics, sizeof(*stats));
   }

 finish:
   if (NULL != man_entry) {
     mtlk_txmm_msg_cleanup(&man_msg);
   }
   return res;
}

static int
_mtlk_mmb_request_fw_stats(mtlk_hw_t* hw, uint32 info_id, uint16 info_index, void* stats_buffer, int buffer_size)
{
   int                       res = MTLK_ERR_OK;
   mtlk_txmm_msg_t           man_msg;
   mtlk_txmm_data_t          *man_entry = NULL;
   UMI_GET_FW_STATS          *mac_stats;

   man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, &hw->master_txdm_mirror, NULL);
   if (!man_entry) {
     ELOG_V("Can't get statistics due to the lack of MAN_MSG");
     res = MTLK_ERR_NO_RESOURCES;
     goto finish;
   }

   mac_stats = (UMI_GET_FW_STATS*)man_entry->payload;

   man_entry->id           = UM_DBG_GET_FW_STATS_REQ;
   man_entry->payload_size = sizeof(*mac_stats);

   mac_stats->u32StatId    = HOST_TO_MAC32(info_id);
   mac_stats->u16StatIndex = HOST_TO_MAC16(info_index);
   mac_stats->u16Status    = HOST_TO_MAC16(0);

   res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
   if (MTLK_ERR_OK != res) {
     ELOG_D("Failed to retrieve FW statistics, driver error #%i", res);
   } else if (UMI_OK != MAC_TO_HOST16(mac_stats->u16Status)) {
     ELOG_D("Failed to retrieve FW statistics, FW error #%u", MAC_TO_HOST16(mac_stats->u16Status));
     res = MTLK_ERR_MAC;
   } else {
     memcpy(stats_buffer, &mac_stats->uStatistics, buffer_size);
   }

 finish:
   if (NULL != man_entry) {
     mtlk_txmm_msg_cleanup(&man_msg);
   }
   return res;
}

static void __MTLK_IFUNC
_mtlk_hw_stat_handle_request(mtlk_irbd_t       *irbd,
                             mtlk_handle_t      context,
                             const mtlk_guid_t *evt,
                             void              *buffer,
                             uint32            *size)
{
  mtlk_hw_t            *hw  = HANDLE_T_PTR(mtlk_hw_t, context);
  mtlk_wssa_info_hdr_t *hdr = (mtlk_wssa_info_hdr_t *) buffer;

  MTLK_UNREFERENCED_PARAM(evt);

  if(sizeof(mtlk_wssa_info_hdr_t) > *size)
    return;

  if(MTIDL_SRC_DRV == hdr->info_source)
  {
    switch(hdr->info_id)
    {
    case MTLK_WSSA_DRV_STATUS_HW:
      {
        if(sizeof(mtlk_wssa_drv_hw_stats_t) + sizeof(mtlk_wssa_info_hdr_t) > *size)
        {
          hdr->processing_result = MTLK_ERR_BUF_TOO_SMALL;
        }
        else
        {
          _mtlk_mmb_get_hw_stats(hw, (mtlk_wssa_drv_hw_stats_t*) &hdr[1]);
          hdr->processing_result = MTLK_ERR_OK;
          *size = sizeof(mtlk_wssa_drv_hw_stats_t) + sizeof(mtlk_wssa_info_hdr_t);
        }
      }
      break;
#if MTLK_MTIDL_HW_STAT_FULL
    case MTLK_WSSA_DRV_DEBUG_STATUS_HW:
      {
        if(sizeof(mtlk_wssa_drv_debug_hw_stats_t) + sizeof(mtlk_wssa_info_hdr_t) > *size)
        {
          hdr->processing_result = MTLK_ERR_BUF_TOO_SMALL;
        }
        else
        {
          _mtlk_mmb_get_debug_hw_stats(hw, (mtlk_wssa_drv_debug_hw_stats_t*) &hdr[1]);
          hdr->processing_result = MTLK_ERR_OK;
          *size = sizeof(mtlk_wssa_drv_debug_hw_stats_t) + sizeof(mtlk_wssa_info_hdr_t);
        }
      }
      break;
#endif /* MTLK_MTIDL_HW_STAT_FULL */
    case MTLK_WSSA_DRV_RECOVERY:
      {
        if (sizeof(mtlk_wssa_drv_recovery_stats_t) + sizeof(mtlk_wssa_info_hdr_t) > *size)
        {
          hdr->processing_result = MTLK_ERR_BUF_TOO_SMALL;
        }
        else
        {
          _mtlk_mmb_get_recovery_stats(hw, (mtlk_wssa_drv_recovery_stats_t*) &hdr[1]);
          hdr->processing_result = MTLK_ERR_OK;
          *size = sizeof(mtlk_wssa_drv_recovery_stats_t) + sizeof(mtlk_wssa_info_hdr_t);
        }
      }
      break;
    default:
      {
        hdr->processing_result = MTLK_ERR_NO_ENTRY;
        *size = sizeof(mtlk_wssa_info_hdr_t);
      }
    }
  }
  else if(MTIDL_SRC_FW == hdr->info_source)
  {
    if(hdr->info_id == DBG_STATS_FWSTATUS)
    {
      hdr->processing_result =
        _mtlk_mmb_request_general_fw_stats(hw, (MTIDL_GeneralStats*)&hdr[1]);
    }
    else
    {
      hdr->processing_result =
        _mtlk_mmb_request_fw_stats(hw, hdr->info_id, hdr->info_index, &hdr[1], *size - sizeof(mtlk_wssa_info_hdr_t));
    }
  }
  else
  {
    hdr->processing_result = MTLK_ERR_NO_ENTRY;
    *size = sizeof(mtlk_wssa_info_hdr_t);
  }
}

int __MTLK_IFUNC 
mtlk_hw_mmb_start_card(mtlk_hw_t *hw)
{
  mtlk_vap_handle_t master_vap_handle;
  
  MTLK_ASSERT(hw != NULL);
  MTLK_ASSERT(hw->vap_manager != NULL);

  MTLK_START_TRY(hw_mmb_card, MTLK_OBJ_PTR(hw))

#ifdef MTCFG_NO_FW_RESET_ON_STOP
    /* If MAC reset is not being performed on stop */
    /* it has to be performed on start, otherwise  */
    /* FW will be not restartable.                 */
    _mtlk_mmb_cleanup_reset_mac(hw);
#endif
    MTLK_START_STEP(hw_mmb_card, HW_MASTER_VAP, MTLK_OBJ_PTR(hw),
                    mtlk_vap_manager_get_master_vap, (hw->vap_manager, &master_vap_handle));

    MTLK_START_STEP_VOID(hw_mmb_card, HW_POWER_ON, MTLK_OBJ_PTR(hw),
                         _mtlk_mmb_power_on, (hw));

    MTLK_START_STEP_VOID(hw_mmb_card, HW_BIST_READ, MTLK_OBJ_PTR(hw),
                         mtlk_bist_read_and_write, (hw->ccr));

    MTLK_START_STEP(hw_mmb_card, HW_EEPROM_READ, MTLK_OBJ_PTR(hw),
                    mtlk_eeprom_read_and_parse, (master_vap_handle, hw->ee_data));
    
    MTLK_START_STEP_VOID(hw_mmb_card, HW_CHI_INIT, MTLK_OBJ_PTR(hw),
                         _mtlk_mmb_chi_init, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_LOAD_FIRMWARE, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_load_firmware, (hw));
    MTLK_START_STEP_VOID(hw_mmb_card, HW_RUN_FIRMWARE, MTLK_OBJ_PTR(hw),
                         _mtlk_mmb_run_firmware, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_WAIT_CHI_MAGIC, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_wait_chi_magic, (hw));

    LOG_CHI_AREA(sFifoQ);
    LOG_CHI_AREA(sDAT);
    LOG_CHI_AREA(sMAN);
    LOG_CHI_AREA(sDBG);

    hw->bds.ind.offset = MAC_TO_HOST32(hw->chi_data.sFifoQ.u32IndStartOffset);
    hw->bds.ind.size   = (uint16)MAC_TO_HOST32(hw->chi_data.sFifoQ.u32IndNumOfElements);
    hw->bds.ind.idx    = 0;

    hw->bds.req.offset = MAC_TO_HOST32(hw->chi_data.sFifoQ.u32ReqStartOffset);
    hw->bds.req.size   = (uint16)MAC_TO_HOST32(hw->chi_data.sFifoQ.u32ReqNumOfElements);
    hw->bds.req.idx    = 0;

    MTLK_START_STEP(hw_mmb_card, HW_MAN_REQ_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_man_req_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_MAN_IND_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_man_ind_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_DBG_REQ_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_dbg_req_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_DBG_IND_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_dbg_ind_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_DAT_REQ_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_data_req_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_DAT_IND_BDR, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prepare_data_ind_bdr, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_RX_QUEUES, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_rxque_set_default_cfg, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_RX_BUFFERS, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_prealloc_rx_buffers, (hw));
    MTLK_START_STEP_IF(0 != hw->calibr.ext_data.u32BufferRequestedSize,
                       hw_mmb_card, HW_CALIBRATION_CACHE, MTLK_OBJ_PTR(hw),
                       _mtlk_mmb_init_calibration_cache, (hw));
    MTLK_START_STEP_VOID_IF(0 != hw->mips_ctrl.ext_data.u32DescriptorLocation,
                            hw_mmb_card, HW_MIPS_CONTROL, MTLK_OBJ_PTR(hw),
                            _mtlk_mmb_init_mips_control, (hw));
    MTLK_START_STEP_IF(hw->fw_logger.is_supported,
                       hw_mmb_card, HW_FW_LOGGER, MTLK_OBJ_PTR(hw),
                       _mtlk_mmb_init_fw_logger, (hw));
    MTLK_START_STEP_VOID(hw_mmb_card, HW_FW_CAPABILITIES, MTLK_OBJ_PTR(hw),
                         _mtlk_mmb_init_fw_capabilities, (hw));
    MTLK_START_STEP(hw_mmb_card, HW_TXMM, MTLK_OBJ_PTR(hw),
                    mtlk_txmm_start, (&hw->txmm_base));

    hw->mac_reset_logic_initialized = TRUE;

    MTLK_START_STEP(hw_mmb_card, HW_TXDM, MTLK_OBJ_PTR(hw),
                    mtlk_txmm_start, (&hw->txdm_base));

    hw->state    = MTLK_HW_STATE_WAITING_READY;
    hw->isr_type = MTLK_ISR_MSGS_PUMP;

    MTLK_START_STEP(hw_mmb_card, HW_SEND_READY, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_send_ready_blocked, (hw));

    MTLK_START_STEP(hw_mmb_card, HW_HW_DEP_CFG, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_send_hw_dep_cfg, (hw));

    MTLK_START_STEP(hw_mmb_card, HW_FW_LOG_SEVERITY, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_send_fw_log_severity, (hw, LOGGER_SEVERITY_DEFAULT_LEVEL, UMI_CPU_ID_UM));

    MTLK_START_STEP(hw_mmb_card, HW_FW_LOG_MODE, MTLK_OBJ_PTR(hw),
                    _mtlk_mmb_send_fw_log_mode, (hw));

    /* Must be done after READY message since the recovery may
       send pseudo-responses for non-allocated messages.
       Such pseudo-responses sending is allowed after the MAC has finished 
       its initialization (i.e. after READY CFM from driver's point of view).
     */
    MTLK_START_STEP(hw_mmb_card, HW_RX_PEND_TIMER, MTLK_OBJ_PTR(hw),
                    mtlk_osal_timer_set, (&hw->rx_data_pending.timer,
                                          MTLK_RX_BUFFS_RECOVERY_PERIOD));

    MTLK_START_STEP_EX(hw_mmb_card, HW_STAT_REQ_HANDLER, MTLK_OBJ_PTR(hw),
                       mtlk_wssd_register_request_handler, (hw->irbd, _mtlk_hw_stat_handle_request,
                                                            HANDLE_T(hw)),
                       hw->stat_irb_handle, hw->stat_irb_handle != NULL, MTLK_ERR_UNKNOWN);

    hw->state  = MTLK_HW_STATE_READY;
    ILOG2_V("HW layer activated");
  MTLK_START_FINALLY(hw_mmb_card, MTLK_OBJ_PTR(hw));
  MTLK_START_RETURN(hw_mmb_card, MTLK_OBJ_PTR(hw), mtlk_hw_mmb_stop_card, (hw));
}

/**************************************************************
 * TX MAN MSG module wrapper
 **************************************************************/
#define CM_REQ_MIRROR_BY_MSG_OBJ(pmsg)                                  \
  MTLK_CONTAINER_OF(pmsg, mtlk_hw_cm_req_mirror_t, msg_hdr)

static void 
_txm_send (mtlk_hw_t *hw, BOOL is_man, PMSG_OBJ pmsg, mtlk_vap_handle_t vap_handle)
{
  mtlk_hw_cm_req_mirror_t *req_obj  = CM_REQ_MIRROR_BY_MSG_OBJ(pmsg);
  void                    *msg_hdr  = &req_obj->msg_hdr;
  uint16                   msg_id   = MSG_OBJ_GET_ID(msg_hdr);
  mtlk_mmb_basic_bdr_t    *bbdr     = _mtlk_mmb_cm_get_req_bbdr(hw, is_man);
  uint32                   iom_size = _mtlk_mmb_cm_bdr_get_iom_bd_size(hw, is_man);
  void                    *iom      = _mtlk_mmb_cm_bdr_get_iom_bd(bbdr, req_obj->hdr.index, iom_size);

  _mtlk_mmb_dbg_verify_msg_send(req_obj);

  /* Must do this in order to deal with MsgID endianess */
  MSG_OBJ_SET_ID(msg_hdr, HOST_TO_MAC16(msg_id));

  /* Tx MAN BD */
  _mtlk_mmb_memcpy_toio(iom, msg_hdr, MTLK_OFFSET_OF(UMI_MSG_HEADER,u32MessageRef));
  /* Do copy in two steps: skip the last word of the header, as byte #14 shouldn't be modifyed */
  _mtlk_mmb_memcpy_toio(((char *)iom)+sizeof(UMI_MSG_HEADER), ((char *)msg_hdr)+sizeof(UMI_MSG_HEADER), iom_size-sizeof(UMI_MSG_HEADER));

  MSG_OBJ_SET_ID(msg_hdr, msg_id);

  _mtlk_mmb_send_msg(hw, is_man?ARRAY_MAN_REQ:ARRAY_DBG_REQ, req_obj->hdr.index, _mtlk_mmb_vap_handle_to_msg_info(hw, vap_handle));
}

void __MTLK_IFUNC _txm_msg_timed_out(mtlk_handle_t usr_data, uint16 msg_id)
{
  int res = MTLK_ERR_OK;
  BOOL is_pending = FALSE;
  mtlk_vap_handle_t master_vap;
  mtlk_hw_t* hw = HANDLE_T_PTR(mtlk_hw_t, usr_data);

  mtlk_vap_manager_get_master_vap(hw->vap_manager, &master_vap);
  WLOG_DD("CID-%04x: Resetting FW because of message timeout. Message ID is 0x%X",
         mtlk_vap_get_oid(master_vap), msg_id);

  res = mtlk_vap_get_core_vft(master_vap)->get_prop(master_vap,
                              MTLK_CORE_PROP_IS_MAC_FATAL_PENDING,
                              &is_pending,
                              sizeof(is_pending));
  if (res != MTLK_ERR_OK) {
    is_pending = FALSE;
  }

  if (!is_pending) {
    mtlk_vap_get_hw_vft(master_vap)->set_prop(master_vap, MTLK_HW_RESET, NULL, 0);
  }
}

static PMSG_OBJ
_txm_msg_get_from_pool (mtlk_hw_t *hw, BOOL is_man)
{
  mtlk_mmb_advanced_bdr_t *abdr  = is_man?&hw->tx_man:&hw->tx_dbg;
  PMSG_OBJ                 pmsg  = NULL;

  mtlk_osal_lock_acquire(&abdr->lock);
  if (mtlk_dlist_size(&abdr->free_list))
  {
    mtlk_dlist_entry_t      *node = mtlk_dlist_pop_front(&abdr->free_list);
    mtlk_hw_cm_req_mirror_t *man_req;

    man_req = MTLK_LIST_GET_CONTAINING_RECORD(node,
                                              mtlk_hw_cm_req_mirror_t,
                                              hdr.list_entry);
    pmsg = &man_req->msg_hdr;
  }
  mtlk_osal_lock_release(&abdr->lock);

  return pmsg;
}

static void
_txm_msg_free_to_pool (mtlk_hw_t *hw, BOOL is_man, PMSG_OBJ pmsg)
{
  mtlk_mmb_advanced_bdr_t *abdr    = is_man?&hw->tx_man:&hw->tx_dbg;
  mtlk_hw_cm_req_mirror_t *req_obj = CM_REQ_MIRROR_BY_MSG_OBJ(pmsg);

  mtlk_osal_lock_acquire(&abdr->lock);
  mtlk_dlist_push_back(&abdr->free_list,
                       &req_obj->hdr.list_entry);
  mtlk_osal_lock_release(&abdr->lock);
}

static PMSG_OBJ __MTLK_IFUNC
_txmm_msg_get_from_pool (mtlk_handle_t usr_data)
{
  return _txm_msg_get_from_pool(HANDLE_T_PTR(mtlk_hw_t, usr_data), TRUE);
}

static void __MTLK_IFUNC
_txmm_msg_free_to_pool (mtlk_handle_t usr_data, PMSG_OBJ pmsg)
{
  _txm_msg_free_to_pool(HANDLE_T_PTR(mtlk_hw_t, usr_data), TRUE, pmsg);
}

static void __MTLK_IFUNC
_txmm_send (mtlk_handle_t usr_data, PMSG_OBJ pmsg, mtlk_vap_handle_t vap_handle)
{
  _txm_send(HANDLE_T_PTR(mtlk_hw_t, usr_data), TRUE, pmsg, vap_handle);
}

static PMSG_OBJ __MTLK_IFUNC
_txdm_msg_get_from_pool (mtlk_handle_t usr_data)
{
  return _txm_msg_get_from_pool(HANDLE_T_PTR(mtlk_hw_t, usr_data), FALSE);
}

static void __MTLK_IFUNC
_txdm_msg_free_to_pool (mtlk_handle_t usr_data, PMSG_OBJ pmsg)
{
  _txm_msg_free_to_pool(HANDLE_T_PTR(mtlk_hw_t, usr_data), FALSE, pmsg);
}

static void __MTLK_IFUNC
_txdm_send (mtlk_handle_t usr_data, PMSG_OBJ pmsg, mtlk_vap_handle_t vap_handle)
{
  _txm_send(HANDLE_T_PTR(mtlk_hw_t, usr_data), FALSE, pmsg, vap_handle);
}

static void 
txmm_on_cfm (mtlk_hw_t *hw, PMSG_OBJ pmsg)
{
  mtlk_txmm_on_cfm(&hw->txmm_base, pmsg);
  mtlk_txmm_pump(&hw->txmm_base, pmsg);
}

static void 
txdm_on_cfm(mtlk_hw_t *hw, PMSG_OBJ pmsg)
{
  mtlk_txmm_on_cfm(&hw->txdm_base, pmsg);
  mtlk_txmm_pump(&hw->txdm_base, pmsg);
}

/**************************************************************/

static const mtlk_hw_vft_t hw_mmb_vft =
{
  _mtlk_hw_get_msg_to_send,
  _mtlk_hw_send_data,
  _mtlk_hw_release_msg_to_send,
  _mtlk_hw_set_prop,
  _mtlk_hw_get_prop,
  _mtlk_hw_get_device
};

/**************************************************************
 * HW interface implementation for VFT
 **************************************************************/
static mtlk_hw_msg_t*
_mtlk_hw_get_msg_to_send(mtlk_vap_handle_t vap_handle, uint32* nof_free_tx_msgs)
{
  mtlk_hw_data_req_mirror_t *data_req;
  mtlk_hw_t *hw = mtlk_vap_get_hw(vap_handle);

  data_req = _mtlk_mmb_get_msg_from_data_pool(hw, vap_handle);

  if (nof_free_tx_msgs) {
    *nof_free_tx_msgs = (uint32)hw->tx_data_nof_free_bds;
  }

  return HW_MSG_PTR(data_req);
}

#ifndef MTCFG_RF_MANAGEMENT_MTLK
#define HIDE_PAYLOAD_TYPE_BUG
#endif

#ifdef HIDE_PAYLOAD_TYPE_BUG
/* WARNING: We suspect the PayloadType feature harms the throughput, so
 *          writing the last TXDAT_REQ_MSG_DESC's DWORD including 
 *          u8RFMgmtData and u8PayloadType to Shared RAM is prohibited
 *          until the bug is fixed whether in driver or MAC.
 *          Once the bug is fixed, TXDATA_INFO_SIZE define could be
 *          removed from the code and _mtlk_mmb_memcpy_toio below can
 *          use sizeof(tx_bd) instead.
 */
#define TXDATA_INFO_SIZE offsetof(TXDAT_REQ_MSG_DESC, u8RFMgmtData)
#else
#define TXDATA_INFO_SIZE sizeof(TXDAT_REQ_MSG_DESC)
#endif

static int
_mtlk_hw_send_data(mtlk_vap_handle_t          vap_handle,
                   const mtlk_hw_send_data_t *data)
{
  int                        res      = MTLK_ERR_UNKNOWN;
  mtlk_hw_data_req_mirror_t  *data_req = DATA_REQ_MIRROR_PTR(data->msg);
  TXDAT_REQ_MSG_DESC         tx_bd;
  uint16                     info;
  mtlk_hw_t                  *hw = mtlk_vap_get_hw(vap_handle);

  data_req->sta_id       = data->sta_id;
  data_req->ac           = data->access_category;
  data_req->nbuf         = data->nbuf;
  data_req->size         = data->size;
  data_req->ts           = mtlk_osal_timestamp();
  data_req->vap_handle   = vap_handle;

  if (data->size != 0) { /* not a NULL-packet */
    data_req->dma_addr = 
      mtlk_df_nbuf_map_to_phys_addr(mtlk_vap_get_manager(vap_handle),
                                    data->nbuf,
                                    data->size,
                                    MTLK_DATA_TO_DEVICE);
  }
  else {
    data_req->dma_addr = 0;
  }

  tx_bd.u8ExtraData        = MTLK_BFIELD_VALUE(TX_EXTRA_ENCAP_TYPE,
                                               data->encap_type,
                                               uint8);
  tx_bd.u32HostPayloadAddr = HOST_TO_MAC32(data_req->dma_addr);
  tx_bd.u16Sid             = HOST_TO_MAC16((uint16) data_req->sta_id);
  info                     = MTLK_BFIELD_VALUE(TX_DATA_INFO_LENGTH,
                                               data_req->size,
                                               uint16) +
                             MTLK_BFIELD_VALUE(TX_DATA_INFO_TID,
                                               data_req->ac,
                                               uint16) +
                             MTLK_BFIELD_VALUE(TX_DATA_INFO_WDS,
                                               data->wds,
                                               uint16);
  tx_bd.u16FrameInfo       = HOST_TO_MAC16(info);

#ifdef MTCFG_RF_MANAGEMENT_MTLK
  tx_bd.u8RFMgmtData       = data->rf_mgmt_data;
#endif

  ILOG4_DP("Mapping %08x, data %p", 
       (int)data_req->dma_addr, 
       data_req->nbuf);

  CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_TX_HW);

  /* TX Data BD */
  _mtlk_mmb_memcpy_toio((uint8 *)_mtlk_basic_bdr_get_iom_bd(&hw->tx_data.basic, data_req->hdr.index, SHRAM_DAT_REQ_MSG) + sizeof(UMI_MSG_HEADER),
                        &tx_bd,
                        TXDATA_INFO_SIZE);

  res = _mtlk_mmb_send_msg(hw, 
                           ARRAY_DAT_REQ,
                           data_req->hdr.index,
                           _mtlk_mmb_vap_handle_to_msg_info(hw, vap_handle));

  CPU_STAT_END_TRACK(CPU_STAT_ID_TX_HW);

  if (res != MTLK_ERR_OK)
  {
    if ((data->size != 0) && data_req->dma_addr)
      mtlk_df_nbuf_unmap_phys_addr(mtlk_vap_get_manager(vap_handle),
                                   data->nbuf,
                                   data_req->dma_addr,
                                   data->size,
                                   MTLK_DATA_TO_DEVICE);
  }

  return res;
}

static int
_mtlk_hw_release_msg_to_send (mtlk_vap_handle_t vap_handle,
                              mtlk_hw_msg_t *msg)
{
  mtlk_hw_data_req_mirror_t *data_req = DATA_REQ_MIRROR_PTR(msg);
  mtlk_hw_t *hw = mtlk_vap_get_hw(vap_handle);

  _mtlk_mmb_free_sent_msg_to_data_pool(hw, data_req);

  return MTLK_ERR_OK;
}

static void _mtlk_hw_mmb_version_printout(mtlk_hw_t *hw, char *buffer, uint32 size)
{
  mtlk_osal_lock_acquire(&hw->reg_lock);

  mtlk_snprintf(buffer, size,
                "Driver version:\n%s\nMAC/PHY versions:\n%s%s",
                DRV_VERSION, hw->if_version, hw->progmodel);

  mtlk_osal_lock_release(&hw->reg_lock);
}

static int
_mtlk_hw_set_prop(mtlk_vap_handle_t vap_handle, mtlk_hw_prop_e prop_id, void *buffer, uint32 size)
{
  int       res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_hw_t *hw = mtlk_vap_get_hw(vap_handle);

  switch (prop_id)
  {
  case MTLK_HW_PROGMODEL:
    res = _mtlk_mmb_load_progmodel_to_hw(hw, buffer);
    break;
  case MTLK_HW_PROP_STATE:
    if (size == sizeof(mtlk_hw_state_e))
    {
      mtlk_hw_state_e *val = (mtlk_hw_state_e *)buffer;
      hw->state = *val;
      res       = MTLK_ERR_OK;
      if ((hw->state == MTLK_HW_STATE_APPFATAL) ||
          (hw->state == MTLK_HW_STATE_EXCEPTION) ||
          (hw->state == MTLK_HW_STATE_MAC_ASSERTED)) {
        mtlk_txmm_halt(&hw->txmm_base);
        mtlk_txmm_halt(&hw->txdm_base);
      }
    }
    break;
  case MTLK_HW_BCL_ON_EXCEPTION:
    if (size == sizeof(UMI_BCL_REQUEST))
    {
      UMI_BCL_REQUEST *preq = (UMI_BCL_REQUEST *)buffer;
      res = _mtlk_mmb_process_bcl(hw, preq, 0);
    }
    break;
  case MTLK_HW_RESET:
    {
      mtlk_vap_handle_t master_vap_handle = NULL;
      res = mtlk_vap_manager_get_master_vap(hw->vap_manager, &master_vap_handle);

      if (hw->state != MTLK_HW_STATE_EXCEPTION &&
          hw->state != MTLK_HW_STATE_APPFATAL &&
          hw->state != MTLK_HW_STATE_MAC_ASSERTED) {
        if (master_vap_handle &&
            (rcvry_is_initialized(mtlk_vap_get_rcvry(master_vap_handle)))) {
          if (rcvry_is_scheduled(mtlk_vap_get_rcvry(master_vap_handle))) {
            break;
          }
          rcvry_initiate(mtlk_vap_get_rcvry(master_vap_handle));
        }

        ELOG_DD("CID-%04x: Asserting FW: hw_state=%d", mtlk_vap_get_oid(vap_handle), hw->state);
        _mtlk_mmb_handle_sw_trap(hw);
      } else {
        res = MTLK_ERR_NOT_READY;
      }
    }
    break;
  case MTLK_HW_DBG_ASSERT_FW:
    if (buffer && size == sizeof(uint32))
    {
      uint32 *mips_no = (uint32 *)buffer;
      res = _mtlk_mmb_cause_mac_assert(hw, *mips_no);
    }
    break;

#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
  case MTLK_HW_FW_LOG_BUFFER:
    if (buffer && size == sizeof(uint8))
    {
      if (*(uint8 *)buffer)
      {
        _mtlk_mmb_parse_fw_logger(hw);
      }
      else
      {
        ELOG_V("Driver doesn't support logger buffers extension");
        res = MTLK_ERR_PARAMS;
      }
    }
    else if (buffer && size == sizeof(mtlk_core_fw_log_buffer_t))
    {
      void* data;
      mtlk_core_fw_log_buffer_t* descr = (mtlk_core_fw_log_buffer_t*) buffer;

      data = mtlk_osal_mem_alloc(MTLK_IPAD4(descr->length), MTLK_MEM_TAG_FW_LOGGER);
      if(NULL != data)
      {
        _mtlk_mmb_pas_get(hw, "FW logger buffer", descr->addr, data, MTLK_IPAD4(descr->length));
        res = _mtlk_mmb_push_logger_buf(hw, data, descr->length);
        mtlk_osal_mem_free(data);
      }
      else
      {
        ELOG_V("Failed to process logger buffer due to lack of memory");
        res = MTLK_ERR_NO_MEM;
      }
    }
    break;
  case MTLK_HW_LOG:
    if (size >= sizeof(mtlk_log_event_t))
    {
      res = _mtlk_mmb_push_logger_buf(hw, buffer, size);
    }
    break;
#endif

  default:
    break;
  }

  return res;
}

static int
_mtlk_hw_get_prop(mtlk_vap_handle_t vap_handle, mtlk_hw_prop_e prop_id, void *buffer, uint32 size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_hw_t *hw = mtlk_vap_get_hw(vap_handle);

  MTLK_ASSERT(NULL != hw);

  switch (prop_id)
  {
  case MTLK_HW_PROP_STATE:
    if (size == sizeof(mtlk_hw_state_e))
    {
      mtlk_hw_state_e *val = (mtlk_hw_state_e *)buffer;
      *val = hw->state;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_FREE_TX_MSGS:
    if (size == sizeof(uint32))
    {
      uint32 *val = (uint32 *)buffer;
      *val = hw->tx_data_nof_free_bds;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_TX_MSGS_USED_PEAK:
    if (size == sizeof(uint32))
    {
      uint32 *val = (uint32 *)buffer;
      *val = hw->tx_data_max_used_bds;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_DUMP:
    if (size == sizeof(mtlk_hw_dump_t))
    {
      mtlk_hw_dump_t *dump = (mtlk_hw_dump_t *)buffer;
      _mtlk_mmb_pas_get(hw, "dbg dump", dump->addr, dump->buffer, dump->size);
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_BCL_ON_EXCEPTION:
    if (size == sizeof(UMI_BCL_REQUEST))
    {
      UMI_BCL_REQUEST *preq = (UMI_BCL_REQUEST *)buffer;
      res = _mtlk_mmb_process_bcl(hw, preq, 1);
    }
    break;
  case MTLK_HW_PRINT_BUS_INFO:
    {
      MTLK_ASSERT(NULL != hw->ccr);

      mtlk_mmb_drv_get_name(mtlk_ccr_get_bus_drv(hw->ccr),
        (char *)buffer, size - 1);

      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_BIST:
    if (size == sizeof(uint32))
    {
      uint32 *val = (uint32 *)buffer;

      MTLK_ASSERT(NULL != hw->mmb);

      *val = hw->mmb->bist_passed;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_FW_BUFFERS_PROCESSED:
    if (size == sizeof(uint32))
    {
      *(uint32 *)buffer = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_FW_LOGGER_PACKETS_PROCESSED);
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_FW_BUFFERS_DROPPED:
    if (size == sizeof(uint32))
    {
      *(uint32 *)buffer = mtlk_wss_get_stat(hw->wss, MTLK_HW_CNT_FW_LOGGER_PACKETS_DROPPED);
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_FW_CAPS_MAX_STAs:
    if (size != sizeof(uint32))
    {
      res = MTLK_ERR_PARAMS;
    }
    else if (!hw->fw_capabilities.nof_stas.u32NumOfStations)
    {
      res = MTLK_ERR_NOT_SUPPORTED;
    }
    else
    {
      uint32 *val = (uint32 *)buffer;
      *val = hw->fw_capabilities.nof_stas.u32NumOfStations;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_FW_CAPS_MAX_VAPs:
    if (size != sizeof(uint32))
    {
      res = MTLK_ERR_PARAMS;
    }
    else if (!hw->fw_capabilities.nof_vaps.u32NumOfVaps)
    {
      res = MTLK_ERR_NOT_SUPPORTED;
    }
    else
    {
      uint32 *val = (uint32 *)buffer;
      *val = hw->fw_capabilities.nof_vaps.u32NumOfVaps;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_IRBD:
    if (size != sizeof(mtlk_irbd_t**))
    {
      res = MTLK_ERR_PARAMS;
    }
    else
    {
      mtlk_irbd_t** irbd = (mtlk_irbd_t**)buffer;
      *irbd = hw->irbd;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_WSS:
    if (size != sizeof(mtlk_wss_t**))
    {
      res = MTLK_ERR_PARAMS;
    }
    else
    {
      mtlk_wss_t** wss = (mtlk_wss_t**)buffer;
      *wss = hw->wss;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_PROP_EEPROM_DATA:
    if (size != sizeof(mtlk_eeprom_data_t**))
    {
      res = MTLK_ERR_PARAMS;
    }
    else
    {
      mtlk_eeprom_data_t **ee_data = (mtlk_eeprom_data_t**)buffer;
      *ee_data = hw->ee_data;
      res  = MTLK_ERR_OK;
    }
    break;
#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
  case MTLK_HW_FW_LOGGER_IS_EX:
    if (size != sizeof(uint8))
    {
      res = MTLK_ERR_PARAMS;
    }
    else
    {
      uint8 *val = (uint8 *)buffer;
      *val = hw->fw_logger.is_supported_ex;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_LOG_BUFFERS_CNT:
    if (size == sizeof(uint32))
    {
      *(uint32 *)buffer = MAC_TO_HOST32(hw->fw_logger.ext_data.u32NumOfBufferDescriptors);
      res = MTLK_ERR_OK;
    }
    else
    {
      res = MTLK_ERR_PARAMS;
    }
    break;
#endif
  case MTLK_HW_PROP_CCR:
    if (size != sizeof(mtlk_ccr_t**))
    {
      res = MTLK_ERR_PARAMS;
    }
    else
    {
      mtlk_ccr_t **ccr = (mtlk_ccr_t**)buffer;
      *ccr = hw->ccr;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_PROP_CARD_TYPE:
    if (size != sizeof(mtlk_card_type_t*))
    {
      res = MTLK_ERR_PARAMS;
    }
    else
    {
      mtlk_card_type_t *card_type = (mtlk_card_type_t*)buffer;

      MTLK_ASSERT(NULL != hw->ccr);

      *card_type = hw->ccr->hw_type;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_PROP_CHIP_NAME:
    if (size != sizeof(char const **))
    {
      res = MTLK_ERR_PARAMS;
    }
    else
    {
      char const **name = (char const **)buffer;

      MTLK_ASSERT(NULL != hw->ccr);
      MTLK_ASSERT(NULL != hw->ccr->chip_info);

      *name = hw->ccr->chip_info->name;
      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_EEPROM_NAME:
    if (0 == size)
    {
      res = MTLK_ERR_PARAMS;
    }
    else
    {
      char *fname = (char *)buffer;

      /* TODO!!!: use HW serial number if available
          Network device indexes are used for old devices only
      */
      snprintf(fname, size, "cal_%s.bin",
               mtlk_df_get_name(mtlk_vap_manager_get_master_df(
                                mtlk_vap_get_manager(vap_handle))));

      res  = MTLK_ERR_OK;
    }
    break;
  case MTLK_HW_VERSION_PRINTOUT:
    {
      _mtlk_hw_mmb_version_printout(hw, (char *)buffer, size);

      res  = MTLK_ERR_OK;
    }
    break;
  default:
    break;
  }

  return res;
}

static void* 
_mtlk_hw_get_device (mtlk_vap_handle_t vap_handle)
{
  mtlk_hw_t *hw = mtlk_vap_get_hw(vap_handle);

  MTLK_ASSERT(NULL != hw);
  MTLK_ASSERT(NULL != hw->ccr);

  return mtlk_mmb_drv_get_device(mtlk_ccr_get_bus_drv(hw->ccr));
}

static void
__mtlk_print_endianess (void)
{
  uint32 val = HOST_MAGIC;

  ILOG0_SDD("The system is %s endian (0x%08x, 0x%08x)", 
            (val == HOST_TO_MAC32(val))?"Little":"Big", val, HOST_TO_MAC32(val));
}

/**************************************************************/

/**************************************************************
 * MMB interface implementation
 **************************************************************/

MTLK_INIT_STEPS_LIST_BEGIN(hw_mmb)
  MTLK_INIT_STEPS_LIST_ENTRY(hw_mmb, HW_MMB_LOCK)
MTLK_INIT_INNER_STEPS_BEGIN(hw_mmb)
MTLK_INIT_STEPS_LIST_END(hw_mmb);

int __MTLK_IFUNC
mtlk_mmb_txmm_init (mtlk_hw_t *hw, mtlk_vap_handle_t master_vap)
{
  int                  res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_cfg_t      cfg;
  mtlk_txmm_wrap_api_t api;

  memset(&cfg, 0, sizeof(cfg));
  memset(&api, 0, sizeof(api));

  cfg.max_msgs          = HW_PCI_TXMM_MAX_MSGS;
  cfg.max_payload_size  = hw->mmb->cfg.man_msg_size;
  cfg.tmr_granularity   = HW_PCI_TXMM_GRANULARITY;

  api.usr_data          = HANDLE_T(hw);
  api.msg_get_from_pool = _txmm_msg_get_from_pool;
  api.msg_free_to_pool  = _txmm_msg_free_to_pool;
  api.msg_send          = _txmm_send;
  api.msg_timed_out     = _txm_msg_timed_out;

  hw->master_txmm_mirror.vap_handle = master_vap;
  hw->master_txmm_mirror.base = &hw->txmm_base;

  res = mtlk_txmm_init(&hw->txmm_base, &cfg, &api);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Can't init TXMM object (err=%d)", res);
    goto FINISH;
  }

  res = MTLK_ERR_OK;

FINISH:
  return res;
}

int __MTLK_IFUNC
mtlk_mmb_txdm_init (mtlk_hw_t *hw, mtlk_vap_handle_t master_vap)
{
  int                  res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_cfg_t      cfg;
  mtlk_txmm_wrap_api_t api;

  memset(&cfg, 0, sizeof(cfg));
  memset(&api, 0, sizeof(api));

  cfg.max_msgs          = HW_PCI_TXDM_MAX_MSGS;
  cfg.max_payload_size  = hw->mmb->cfg.dbg_msg_size;
  cfg.tmr_granularity   = HW_PCI_TXDM_GRANULARITY;

  api.usr_data          = HANDLE_T(hw);
  api.msg_get_from_pool = _txdm_msg_get_from_pool;
  api.msg_free_to_pool  = _txdm_msg_free_to_pool;
  api.msg_send          = _txdm_send;
  api.msg_timed_out     = _txm_msg_timed_out;

  hw->master_txdm_mirror.vap_handle = master_vap;
  hw->master_txdm_mirror.base = &hw->txdm_base;

  res = mtlk_txmm_init(&hw->txdm_base, &cfg, &api);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Can't init TXDM object (err=%d)", res);
    goto FINISH;
  }

  res = MTLK_ERR_OK;

FINISH:
  return res;
}

int __MTLK_IFUNC 
mtlk_hw_mmb_init (mtlk_hw_mmb_t *mmb, const mtlk_hw_mmb_cfg_t *cfg)
{
  memset(mmb, 0, sizeof(*mmb));
  mmb->cfg = *cfg;

  MTLK_ASSERT(mmb != NULL);
  MTLK_ASSERT(cfg != NULL);
  MTLK_ASSERT(cfg->man_msg_size != 0);
  MTLK_ASSERT(cfg->dbg_msg_size != 0);
  MTLK_ASSERT((cfg->man_msg_size & 0x3) == 0);
  MTLK_ASSERT((cfg->dbg_msg_size & 0x3) == 0);

#if MTLK_RX_BUFF_ALIGNMENT
  ILOG2_DD("HW requires Rx buffer alignment to %d (0x%02x)", 
       MTLK_RX_BUFF_ALIGNMENT,
       MTLK_RX_BUFF_ALIGNMENT);
#endif

  __mtlk_print_endianess();

  MTLK_INIT_TRY(hw_mmb, MTLK_OBJ_PTR(mmb))
    MTLK_INIT_STEP(hw_mmb, HW_MMB_LOCK, MTLK_OBJ_PTR(mmb), 
                   mtlk_osal_lock_init, (&mmb->lock));
  MTLK_INIT_FINALLY(hw_mmb, MTLK_OBJ_PTR(mmb))
  MTLK_INIT_RETURN(hw_mmb, MTLK_OBJ_PTR(mmb),
                   mtlk_hw_mmb_cleanup, (mmb));
}

void __MTLK_IFUNC
mtlk_hw_mmb_cleanup (mtlk_hw_mmb_t *mmb)
{
  MTLK_CLEANUP_BEGIN(hw_mmb, MTLK_OBJ_PTR(mmb))
    MTLK_CLEANUP_STEP(hw_mmb, HW_MMB_LOCK, MTLK_OBJ_PTR(mmb),
                      mtlk_osal_lock_cleanup, (&mmb->lock));
  MTLK_CLEANUP_END(hw_mmb, MTLK_OBJ_PTR(mmb))

  memset(mmb, 0, sizeof(*mmb));
}

mtlk_txmm_base_t *__MTLK_IFUNC
mtlk_hw_mmb_get_txmm (mtlk_hw_t *hw)
{
  return &hw->txmm_base;
}

mtlk_txmm_base_t *__MTLK_IFUNC
mtlk_hw_mmb_get_txdm (mtlk_hw_t *hw)
{
  return &hw->txdm_base;
}

uint8 __MTLK_IFUNC
mtlk_hw_mmb_get_card_idx(mtlk_hw_t *card)
{
  return card->card_idx;
}

void __MTLK_IFUNC
mtlk_hw_mmb_stop_mac_events (mtlk_hw_t *hw)
{
  hw->mac_events_stopped = 1;
}

mtlk_hw_api_t * __MTLK_IFUNC 
mtlk_hw_mmb_add_card (mtlk_hw_mmb_t *mmb)
{
  mtlk_hw_t     *hw     = NULL;
  mtlk_hw_api_t *hw_api = NULL;
  int           i       = 0;

  mtlk_osal_lock_acquire(&mmb->lock);

  hw_api = (mtlk_hw_api_t *)mtlk_osal_mem_alloc(sizeof(*hw_api), MTLK_MEM_TAG_HW);
  if (!hw_api) {
    ELOG_V("Can't allocate HW API object");
    goto FINISH;
  }

  hw = (mtlk_hw_t *)mtlk_osal_mem_alloc(sizeof(*hw), MTLK_MEM_TAG_HW);
  if (!hw) {
    ELOG_V("Can't allocate HW object");
    mtlk_osal_mem_free(hw_api);
    hw_api = NULL;
    goto FINISH;
  }

  if (mmb->nof_cards >= ARRAY_SIZE(mmb->cards)) {
    ELOG_D("Maximum %d boards supported", (int)ARRAY_SIZE(mmb->cards));
    mtlk_osal_mem_free(hw);
    mtlk_osal_mem_free(hw_api);
    hw_api = NULL;
    goto FINISH;
  }

  memset(hw_api, 0, sizeof(*hw_api));
  memset(hw, 0, sizeof(*hw));

  hw->mmb = mmb;

  for (i = 0; i < ARRAY_SIZE(mmb->cards); i++) {
    if (!mmb->cards[i]) {
      mmb->cards[i] = hw;
      mmb->cards[i]->card_idx = i;
      mmb->nof_cards++;
      break;
    }
  }

  hw_api->hw = hw;
  hw_api->vft = &hw_mmb_vft;

FINISH:
  mtlk_osal_lock_release(&mmb->lock);

  return hw_api;
}

void __MTLK_IFUNC 
mtlk_hw_mmb_remove_card (mtlk_hw_mmb_t *mmb,
                         mtlk_hw_api_t *hw_api)
{
  int           i    = 0;

  mtlk_osal_lock_acquire(&mmb->lock);
  for (i = 0; i < ARRAY_SIZE(mmb->cards); i++) {
    if (mmb->cards[i] == hw_api->hw) {
      mmb->cards[i] = NULL;
      mmb->nof_cards--;
      break;
    }
  }
  mtlk_osal_mem_free(hw_api->hw);
  mtlk_osal_mem_free(hw_api);
  mtlk_osal_lock_release(&mmb->lock);
}

void __MTLK_IFUNC
mtlk_hw_mmb_global_version_printout (mtlk_hw_mmb_t *mmb,
                                     char *buffer,
                                     uint32 size)
{
  int i;
  int bytes_left = size - 1;
  int bytes_printed;

  buffer[size - 1] = 0;
  bytes_printed = snprintf(buffer, bytes_left, "Driver version:\n%s\nMAC/PHY versions:\n", DRV_VERSION);
  if(bytes_printed < 0)
  {
    buffer[0] = 0;
    return;
  }
  bytes_left -= bytes_printed;
  buffer += bytes_printed;

  mtlk_osal_lock_acquire(&mmb->lock);

  for (i = 0; i < mmb->nof_cards; i++) {
    mtlk_hw_t *hw = mmb->cards[i];

    mtlk_osal_lock_acquire(&hw->reg_lock);

    if(bytes_left > 0)
    {
      bytes_printed = snprintf(buffer, bytes_left, "%s%s", hw->if_version, hw->progmodel);
      if(bytes_printed < 0)
      {
        buffer[0] = 0;
        bytes_left = -1; /* Break loop, release locks */
        return;
      }
      else
      {
        bytes_left -= bytes_printed;
        buffer += bytes_printed;
      }
    }

    mtlk_osal_lock_release(&hw->reg_lock);
  }

  mtlk_osal_lock_release(&mmb->lock);
}


int __MTLK_IFUNC 
mtlk_hw_mmb_interrupt_handler (mtlk_hw_t *hw)
{
  int ret = MTLK_ERR_OK;
  mtlk_handle_t lock;

  MTLK_ASSERT(NULL != hw);
  MTLK_ASSERT(NULL != hw->ccr);

  lock = mtlk_osal_lock_acquire_irq(&hw->reg_lock);

  CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_ISR);

  do
  {
    ret = mtlk_ccr_on_interrupt(hw->ccr);
    if (MTLK_ERR_PENDING == ret)
    {
      ret = MTLK_ERR_OK;
      break; /* do */
    }
    else if (ret != MTLK_ERR_OK)
    {
      break; /* do */
    }

    _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_ISRS_TOTAL);

    if (!mtlk_ccr_disable_interrupts_if_pending(hw->ccr))
    {
      _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_ISRS_FOREIGN);
      ret = MTLK_ERR_UNKNOWN; /* not an our interrupt */
      break; /* do */
    }

    if (!mtlk_ccr_clear_interrupts_if_pending(hw->ccr))
    {
      _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_ISRS_NOT_PENDING);
      mtlk_ccr_enable_interrupts(hw->ccr);
      break; /* do */
    }

    if (hw->state == MTLK_HW_STATE_HALTED)
    {
      _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_ISRS_HALTED);
      mtlk_osal_emergency_print("Interrupt received while HW is in halted state.");
      mtlk_ccr_enable_interrupts(hw->ccr);
      break; /* do */
    }

    switch (hw->isr_type)
    {
      case MTLK_ISR_INIT_EVT:
        MTLK_HW_INIT_EVT_SET(hw); /* Interrupts will be enabled by bootstrap code */
        _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_ISRS_INIT);
        break; /* switch */
      case MTLK_ISR_MSGS_PUMP:
        _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_ISRS_TO_DPC);

        /* Interrupts will be enabled by tasklet */
        ret = mtlk_ccr_postpone_irq_handler(hw->ccr);
        break; /* switch */
      case MTLK_ISR_NONE:
      case MTLK_ISR_LAST:
      default:
        _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_ISRS_UNKNOWN);
        mtlk_osal_emergency_print("Interrupt of unknown type (%d) received", hw->isr_type);
        mtlk_ccr_enable_interrupts(hw->ccr);
        break; /* switch */
    }
  } while(0);

  CPU_STAT_END_TRACK(CPU_STAT_ID_ISR);

  mtlk_osal_lock_release_irq(&hw->reg_lock, lock);

  return ret;
}

void __MTLK_IFUNC 
mtlk_hw_mmb_deferred_handler (mtlk_hw_t *hw)
{
  _mtlk_mmb_hw_inc_cnt(hw, MTLK_HW_SOURCE_CNT_POST_ISR_DPCS);
  _mtlk_mmb_read_ind_or_cfm(hw);
}

#ifdef MTCFG_TSF_TIMER_ACCESS_ENABLED
uint32 __MTLK_IFUNC
mtlk_hw_get_timestamp(mtlk_vap_handle_t vap_handle)
{
  mtlk_hw_t *hw = mtlk_vap_get_hw(vap_handle);

  uint32 low;
  uint32 high;

  MTLK_ASSERT(NULL != hw);
  MTLK_ASSERT(NULL != hw->ccr);

  /*  Bar1                                  */
  /*  equ mac_pac_tsf_timer_low 0x200738    */
  /*  equ mac_pac_tsf_timer_high 0x20073C   */

    mtlk_ccr_read_hw_timestamp(hw->ccr, &low, &high);

  return low;
}
#endif /* MTCFG_TSF_TIMER_ACCESS_ENABLED */

void __MTLK_IFUNC
mtlk_hw_mmb_dis_interrupts (mtlk_hw_t *hw)
{
  mtlk_ccr_disable_interrupts(hw->ccr);
}

void __MTLK_IFUNC
mtlk_hw_mmb_isolate (mtlk_hw_t *hw)
{
  _mtlk_mmb_stop_events_completely(hw);

  /* Isolate from IRBD event (because of access to FW) */
  mtlk_wssd_unregister_request_handler(hw->irbd, hw->stat_irb_handle);
  hw->stat_irb_handle = NULL;

  /* Stop MMB RX buffer recovery timer */
  mtlk_osal_timer_cancel_sync(&hw->rx_data_pending.timer);

  /* Stop Message sending */
  mtlk_txmm_stop(&hw->txmm_base);
  mtlk_txmm_halt(&hw->txmm_base);

  mtlk_txmm_stop(&hw->txdm_base);
  mtlk_txmm_halt(&hw->txdm_base);
}

int __MTLK_IFUNC
mtlk_hw_mmb_restore (mtlk_hw_t *hw)
{
  int res = MTLK_ERR_OK;
  mtlk_vap_handle_t master_vap;
  BOOL new_mac_fatal_pending = FALSE;

  MTLK_ASSERT(hw != NULL);
  MTLK_ASSERT(hw->vap_manager != NULL);

  _mtlk_mmb_reset_all_events(hw);
  _mtlk_mmb_cleanup_reset_mac(hw);

  mtlk_vap_manager_get_master_vap(hw->vap_manager, &master_vap);
  (void) mtlk_vap_get_core_vft(master_vap)->set_prop(master_vap,
                               MTLK_CORE_PROP_IS_MAC_FATAL_PENDING,
                               &new_mac_fatal_pending,
                               sizeof(new_mac_fatal_pending));

  _mtlk_mmb_power_on(hw);

  _mtlk_mmb_chi_init(hw);
  res = _mtlk_mmb_load_firmware(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  _mtlk_mmb_run_firmware(hw);
  res = _mtlk_mmb_wait_chi_magic(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  LOG_CHI_AREA(sFifoQ);
  LOG_CHI_AREA(sDAT);
  LOG_CHI_AREA(sMAN);
  LOG_CHI_AREA(sDBG);

  MTLK_ASSERT(hw->bds.ind.offset == MAC_TO_HOST32(hw->chi_data.sFifoQ.u32IndStartOffset));
  MTLK_ASSERT(hw->bds.ind.size   == (uint16)MAC_TO_HOST32(hw->chi_data.sFifoQ.u32IndNumOfElements));
  hw->bds.ind.idx = 0;

  MTLK_ASSERT(hw->bds.req.offset == MAC_TO_HOST32(hw->chi_data.sFifoQ.u32ReqStartOffset));
  MTLK_ASSERT(hw->bds.req.size   == (uint16)MAC_TO_HOST32(hw->chi_data.sFifoQ.u32ReqNumOfElements));
  hw->bds.req.idx = 0;

  _mtlk_mmb_empty_pending_rx_buffers(hw);
  _mtlk_mmb_free_preallocated_rx_buffers(hw);

  /* Restore TX BDR */
  _mtlk_mmb_cleanup_man_req_bdr(hw);
  _mtlk_mmb_cleanup_man_ind_bdr(hw);
  _mtlk_mmb_cleanup_dbg_req_bdr(hw);
  _mtlk_mmb_cleanup_dbg_ind_bdr(hw);
  _mtlk_mmb_cleanup_data_req_bdr(hw);
  _mtlk_mmb_cleanup_data_ind_bdr(hw);

  res = _mtlk_mmb_prepare_man_req_bdr(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = _mtlk_mmb_prepare_man_ind_bdr(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = _mtlk_mmb_prepare_dbg_req_bdr(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = _mtlk_mmb_prepare_dbg_ind_bdr(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = _mtlk_mmb_prepare_data_req_bdr(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = _mtlk_mmb_prepare_data_ind_bdr(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = _mtlk_mmb_rxque_set_default_cfg(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = _mtlk_mmb_prealloc_rx_buffers(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  if (hw->fw_logger.is_supported) {
    _mtlk_mmb_cleanup_fw_logger(hw);
    res = _mtlk_mmb_init_fw_logger(hw);
    if (res != MTLK_ERR_OK) {
      ELOG_V("FW log buffers initialization failed");
      return res;
    }
  }

  if (hw->calibr.ext_data.u32BufferRequestedSize) {
    _mtlk_mmb_restore_calibration_cache(hw);
  }

  if (hw->mips_ctrl.ext_data.u32DescriptorLocation) {
    _mtlk_mmb_init_mips_control(hw);
  }

  _mtlk_mmb_init_fw_capabilities(hw);

  res = _mtlk_mmb_txmm_restore(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = _mtlk_mmb_txdm_restore(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  hw->mac_reset_logic_initialized = TRUE;

  hw->state    = MTLK_HW_STATE_WAITING_READY;
  hw->isr_type = MTLK_ISR_MSGS_PUMP;

  res = _mtlk_mmb_send_ready_blocked(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = _mtlk_mmb_send_hw_dep_cfg(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = _mtlk_mmb_send_fw_log_severity(hw, LOGGER_SEVERITY_DEFAULT_LEVEL, UMI_CPU_ID_UM);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = _mtlk_mmb_send_fw_log_mode(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }
  res = mtlk_osal_timer_set(&hw->rx_data_pending.timer, MTLK_RX_BUFFS_RECOVERY_PERIOD);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  hw->stat_irb_handle =
    mtlk_wssd_register_request_handler(hw->irbd, _mtlk_hw_stat_handle_request, HANDLE_T(hw));
  if (hw->stat_irb_handle == NULL) {
    return MTLK_ERR_UNKNOWN;
  }

  hw->state = MTLK_HW_STATE_READY;

  ILOG2_V("HW layer restored!");
  return MTLK_ERR_OK;
}
