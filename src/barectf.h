#ifndef _BARECTF_H
#define _BARECTF_H

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2020 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * The following code was generated by barectf v3.1.2
 * on 2023-08-23T14:46:34.595726.
 *
 * For more details, see <https://barectf.org/>.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define barectf_trace_job_arrival barectf_default_trace_job_arrival
#define barectf_trace_job_finished barectf_default_trace_job_finished
#define barectf_trace_proc_activated barectf_default_trace_proc_activated
#define barectf_trace_proc_idle barectf_default_trace_proc_idle
#define barectf_trace_remaining_execution_time barectf_default_trace_remaining_execution_time
#define barectf_trace_resched barectf_default_trace_resched
#define barectf_trace_serv_budget_exhausted barectf_default_trace_serv_budget_exhausted
#define barectf_trace_serv_budget_replenished barectf_default_trace_serv_budget_replenished
#define barectf_trace_serv_inactive barectf_default_trace_serv_inactive
#define barectf_trace_serv_non_cont barectf_default_trace_serv_non_cont
#define barectf_trace_serv_postpone barectf_default_trace_serv_postpone
#define barectf_trace_serv_preempted barectf_default_trace_serv_preempted
#define barectf_trace_serv_ready barectf_default_trace_serv_ready
#define barectf_trace_serv_running barectf_default_trace_serv_running
#define barectf_trace_serv_scheduled barectf_default_trace_serv_scheduled
#define barectf_trace_sim_finished barectf_default_trace_sim_finished
#define barectf_trace_virtual_time barectf_default_trace_virtual_time

struct barectf_ctx;

uint32_t barectf_packet_size(const void *vctx);
int barectf_packet_is_full(const void *vctx);
int barectf_packet_is_empty(const void *vctx);
uint32_t barectf_packet_events_discarded(const void *vctx);
uint32_t barectf_discarded_event_records_count(const void * const vctx);
uint32_t barectf_packet_sequence_number(const void * const vctx);
uint8_t *barectf_packet_buf(const void *vctx);
uint8_t *barectf_packet_buf_addr(const void * const vctx);
void barectf_packet_set_buf(void *vctx, uint8_t *buf, uint32_t buf_size);
uint32_t barectf_packet_buf_size(const void *vctx);
int barectf_packet_is_open(const void *vctx);
int barectf_is_in_tracing_section(const void *vctx);
volatile const int *barectf_is_in_tracing_section_ptr(const void *vctx);
int barectf_is_tracing_enabled(const void *vctx);
void barectf_enable_tracing(void *vctx, int enable);

/* barectf platform callbacks */
struct barectf_platform_callbacks {
	/* Clock source callbacks */
	uint64_t (*default_clock_get_value)(void *);

	/* Is the back end full? */
	int (*is_backend_full)(void *);

	/* Open packet */
	void (*open_packet)(void *);

	/* Close packet */
	void (*close_packet)(void *);
};

/* Common barectf context */
struct barectf_ctx {
	/* Platform callbacks */
	struct barectf_platform_callbacks cbs;

	/* Platform data (passed to callbacks) */
	void *data;

	/* Output buffer (will contain a CTF binary packet) */
	uint8_t *buf;

	/* Packet's total size (bits) */
	uint32_t packet_size;

	/* Packet's content size (bits) */
	uint32_t content_size;

	/* Current position from beginning of packet (bits) */
	uint32_t at;

	/* Size of packet header + context fields (content offset) */
	uint32_t off_content;

	/* Discarded event records counter snapshot */
	uint32_t events_discarded;

	/* Packet's sequence number */
	uint32_t sequence_number;

	/* Current packet is open? */
	int packet_is_open;

	/* In tracing code? */
	volatile int in_tracing_section;

	/* Tracing is enabled? */
	volatile int is_tracing_enabled;

	/* Use current/last event record timestamp when opening/closing packets */
	int use_cur_last_event_ts;
};

/* Context for data stream type `default` */
struct barectf_default_ctx {
	/* Parent */
	struct barectf_ctx parent;

	/* Config-specific members follow */
	uint32_t off_ph_magic;
	uint32_t off_ph_stream_id;
	uint32_t off_pc_packet_size;
	uint32_t off_pc_content_size;
	uint32_t off_pc_timestamp_begin;
	uint32_t off_pc_timestamp_end;
	uint32_t off_pc_events_discarded;
	uint64_t cur_last_event_ts;
};

/* Initialize context */
void barectf_init(void *vctx,
	uint8_t *buf, uint32_t buf_size,
	const struct barectf_platform_callbacks cbs, void *data);

/* Open packet for data stream type `default` */
void barectf_default_open_packet(
	struct barectf_default_ctx *sctx);

/* Close packet for data stream type `default` */
void barectf_default_close_packet(struct barectf_default_ctx *sctx);

/* Trace (data stream type `default`, event record type `job_arrival`) */
void barectf_default_trace_job_arrival(struct barectf_default_ctx *sctx,
	int32_t p_tid,
	int32_t p_virtual_time,
	int32_t p_deadline);

/* Trace (data stream type `default`, event record type `job_finished`) */
void barectf_default_trace_job_finished(struct barectf_default_ctx *sctx,
	int32_t p_tid);

/* Trace (data stream type `default`, event record type `proc_activated`) */
void barectf_default_trace_proc_activated(struct barectf_default_ctx *sctx);

/* Trace (data stream type `default`, event record type `proc_idle`) */
void barectf_default_trace_proc_idle(struct barectf_default_ctx *sctx);

/* Trace (data stream type `default`, event record type `remaining_execution_time`) */
void barectf_default_trace_remaining_execution_time(struct barectf_default_ctx *sctx,
	int32_t p_tid,
	int64_t p_remaining_execution_time);

/* Trace (data stream type `default`, event record type `resched`) */
void barectf_default_trace_resched(struct barectf_default_ctx *sctx);

/* Trace (data stream type `default`, event record type `serv_budget_exhausted`) */
void barectf_default_trace_serv_budget_exhausted(struct barectf_default_ctx *sctx,
	int32_t p_tid);

/* Trace (data stream type `default`, event record type `serv_budget_replenished`) */
void barectf_default_trace_serv_budget_replenished(struct barectf_default_ctx *sctx,
	int32_t p_tid,
	int64_t p_budget);

/* Trace (data stream type `default`, event record type `serv_inactive`) */
void barectf_default_trace_serv_inactive(struct barectf_default_ctx *sctx,
	int32_t p_tid);

/* Trace (data stream type `default`, event record type `serv_non_cont`) */
void barectf_default_trace_serv_non_cont(struct barectf_default_ctx *sctx,
	int32_t p_tid);

/* Trace (data stream type `default`, event record type `serv_postpone`) */
void barectf_default_trace_serv_postpone(struct barectf_default_ctx *sctx,
	int32_t p_tid);

/* Trace (data stream type `default`, event record type `serv_preempted`) */
void barectf_default_trace_serv_preempted(struct barectf_default_ctx *sctx,
	int32_t p_tid);

/* Trace (data stream type `default`, event record type `serv_ready`) */
void barectf_default_trace_serv_ready(struct barectf_default_ctx *sctx,
	int32_t p_tid);

/* Trace (data stream type `default`, event record type `serv_running`) */
void barectf_default_trace_serv_running(struct barectf_default_ctx *sctx,
	int32_t p_tid);

/* Trace (data stream type `default`, event record type `serv_scheduled`) */
void barectf_default_trace_serv_scheduled(struct barectf_default_ctx *sctx,
	int32_t p_tid);

/* Trace (data stream type `default`, event record type `sim_finished`) */
void barectf_default_trace_sim_finished(struct barectf_default_ctx *sctx);

/* Trace (data stream type `default`, event record type `virtual_time`) */
void barectf_default_trace_virtual_time(struct barectf_default_ctx *sctx,
	int32_t p_tid,
	int64_t p_virtual_time);

#ifdef __cplusplus
}
#endif

#endif /* _BARECTF_H */
