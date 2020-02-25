/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 *
 * Metalink Logger
 *
 * Helper macros for generated logging code
 *
 * $Id$
 *
 */
#ifndef __LOGMACRO_MIXINS_H_
#define __LOGMACRO_MIXINS_H_



#ifndef logpkt_memcpy
#define logpkt_memcpy memcpy
#endif

#define LOGPKT_STRING_SIZE(str)                                   \
     (sizeof(mtlk_log_event_data_t) + sizeof(mtlk_log_lstring_t) + str##len__)

#define LOGPKT_SCALAR_SIZE(val)                                   \
     (sizeof(mtlk_log_event_data_t) + sizeof(val))

#define LOGPKT_MACADDR_SIZE(addr)                                 \
     (sizeof(mtlk_log_event_data_t) + MAC_ADDR_LENGTH)

#define LOGPKT_IP6ADDR_SIZE(addr)                                 \
     (sizeof(mtlk_log_event_data_t) + IP6_ADDR_LENGTH)

#define LOGPKT_SIGNAL_STRUCT_SIZE                                 \
  ( sizeof( ((mtlk_log_signal_t*) NULL)->src_task_id) +           \
    sizeof( ((mtlk_log_signal_t*) NULL)->dst_task_id) +           \
    sizeof( ((mtlk_log_signal_t*) NULL)->len) )

#define LOGPKT_SIGNAL_SIZE(message_size)                          \
     (sizeof(mtlk_log_event_data_t) +                             \
     LOGPKT_SIGNAL_STRUCT_SIZE+                                   \
     message_size)

#define LOGPKT_EVENT_HDR_SIZE                                     \
  ( sizeof( ((mtlk_log_event_t*) NULL)->info) +                   \
    sizeof( ((mtlk_log_event_t*) NULL)->info_ex) +                \
    sizeof( ((mtlk_log_event_t*) NULL)->timestamp) )

#define LOGPKT_EVENT_HDR_SIZE_VER2              \
  ( LOGPKT_EVENT_HDR_SIZE +                     \
    sizeof( ((mtlk_log_event_t*) NULL)->size) )

#define LOGPKT_SIGNAL_HDR_SIZE                                    \
     (sizeof(mtlk_log_event_data_t) + LOGPKT_SIGNAL_STRUCT_SIZE)

#define LOGPKT_PUT_STRING(str)                                    \
  {                                                               \
      ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_LSTRING; \
      p__ += sizeof(mtlk_log_event_data_t);                       \
      ((mtlk_log_lstring_t *) p__)->len = str##len__;             \
      p__ += sizeof(mtlk_log_lstring_t);                          \
      logpkt_memcpy(p__, str, str##len__);                        \
      p__ += str##len__;                                          \
  }

#define LOGPKT_PUT_INT32(value)                                   \
  {                                                               \
      ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_INT32;   \
      p__ += sizeof(mtlk_log_event_data_t);                       \
      logpkt_memcpy(p__, &value, sizeof(int32));                  \
      p__ += sizeof(int32);                                       \
  }

#define LOGPKT_PUT_INT8(value)                                    \
  {                                                               \
      ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_INT8;    \
      p__ += sizeof(mtlk_log_event_data_t);                       \
      logpkt_memcpy(p__, &value, sizeof(int8));                   \
      p__ += sizeof(int8);                                        \
  }

#define LOGPKT_PUT_INT64(value)                                   \
  {                                                               \
      ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_INT64;   \
      p__ += sizeof(mtlk_log_event_data_t);                       \
      logpkt_memcpy(p__, &value, sizeof(int64));                  \
      p__ += sizeof(int64);                                       \
  }

#define LOGPKT_PUT_PTR(ptr)                                          \
  {                                                                  \
      if (sizeof(void *) == sizeof(uint32)) {                        \
          ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_INT32;  \
      }                                                              \
      else if (sizeof(void *) == sizeof(uint64)) {                   \
          ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_INT64;  \
      }                                                              \
      else {                                                         \
          MTLK_ASSERT(!"Invalid pointer size");                      \
      }                                                              \
      p__ += sizeof(mtlk_log_event_data_t);                          \
      logpkt_memcpy(p__, &ptr, sizeof(void *));                      \
      p__ += sizeof(void *);                                         \
  }

#define LOGPKT_PUT_MACADDR(addr)                                     \
  {                                                                  \
      ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_MACADDR;    \
      p__ += sizeof(mtlk_log_event_data_t);                          \
      logpkt_memcpy(p__, (void*) addr, MAC_ADDR_LENGTH);             \
      p__ += MAC_ADDR_LENGTH;                                        \
  }

#define LOGPKT_PUT_IP6ADDR(addr)                                     \
  {                                                                  \
      ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_IP6ADDR;    \
      p__ += sizeof(mtlk_log_event_data_t);                          \
      logpkt_memcpy(p__, (void*) addr, IP6_ADDR_LENGTH);             \
      p__ += IP6_ADDR_LENGTH;                                        \
  }

#define LOGPKT_PUT_SIGNAL(mtlk_log_signal, message)                  \
  {                                                                  \
      ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_SIGNAL;     \
      p__ += sizeof(mtlk_log_event_data_t);                          \
      logpkt_memcpy(p__,&mtlk_log_signal,LOGPKT_SIGNAL_STRUCT_SIZE); \
      p__ += LOGPKT_SIGNAL_STRUCT_SIZE;                              \
      logpkt_memcpy(p__, message, mtlk_log_signal.len);              \
      p__ += mtlk_log_signal.len;                                    \
  }

#define LOGPKT_PUT_SIGNAL_HEADER(mtlk_log_signal)			\
  {                                                                     \
      ((mtlk_log_event_data_t *) p__)->datatype = LOG_DT_SIGNAL;	\
      p__ += sizeof(mtlk_log_event_data_t);				\
      logpkt_memcpy(p__, &mtlk_log_signal, LOGPKT_SIGNAL_STRUCT_SIZE);	\
      p__ += LOGPKT_SIGNAL_STRUCT_SIZE;					\
  }

#define LOGPKT_PUT_SIGNAL_BODY(message, message_size)	\
  {							\
      logpkt_memcpy(p__, message, message_size);	\
      p__ += message_size;				\
  }

/* static assert (check during compilation stage) */
#define ct_assert(e) {enum { ct_assert_value = 1/(!!(e)) };}

#endif // __LOGMACRO_MIXINS_H_

