#pragma once

/*
 * Forced into the ESP-IDF FreeRTOS component by the DOMES firmware build.
 * Keep this header C-compatible and dependency-free: the kernel expands these
 * macros in private implementation contexts before application headers exist.
 */

#ifndef __ASSEMBLER__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void domes_trace_hook_task_switch_in(void);
void domes_trace_hook_task_switch_out(void);
void domes_trace_hook_task_ready(const volatile void* task);
void domes_trace_hook_task_delete(const volatile void* task);
void domes_trace_hook_task_block(const volatile void* object, uint32_t timeout_ticks);
void domes_trace_hook_queue_send(const volatile void* object, uint32_t from_isr);
void domes_trace_hook_queue_receive(const volatile void* object, uint32_t from_isr);
void domes_trace_hook_isr_enter(uint32_t interrupt_id);
void domes_trace_hook_isr_exit(void);

#ifdef __cplusplus
}
#endif

#define traceTASK_SWITCHED_IN() domes_trace_hook_task_switch_in()
#define traceTASK_SWITCHED_OUT() domes_trace_hook_task_switch_out()
#define traceMOVED_TASK_TO_READY_STATE(pxTCB) domes_trace_hook_task_ready((pxTCB))
#define traceTASK_DELETE(pxTCB) domes_trace_hook_task_delete((pxTCB))

#define traceBLOCKING_ON_QUEUE_RECEIVE(pxQueue) domes_trace_hook_task_block((pxQueue), xTicksToWait)
#define traceBLOCKING_ON_QUEUE_SEND(pxQueue) domes_trace_hook_task_block((pxQueue), xTicksToWait)

#define traceQUEUE_SEND(pxQueue) domes_trace_hook_queue_send((pxQueue), 0U)
#define traceQUEUE_SEND_FROM_ISR(pxQueue) domes_trace_hook_queue_send((pxQueue), 1U)
#define traceQUEUE_GIVE_FROM_ISR(pxQueue) domes_trace_hook_queue_send((pxQueue), 1U)
#define traceQUEUE_RECEIVE(pxQueue) domes_trace_hook_queue_receive((pxQueue), 0U)
#define traceQUEUE_SEMAPHORE_RECEIVE(pxQueue) domes_trace_hook_queue_receive((pxQueue), 0U)
#define traceQUEUE_RECEIVE_FROM_ISR(pxQueue) domes_trace_hook_queue_receive((pxQueue), 1U)

#define traceISR_ENTER(interrupt_id) domes_trace_hook_isr_enter((uint32_t)(interrupt_id))
#define traceISR_EXIT() domes_trace_hook_isr_exit()
#define traceISR_EXIT_TO_SCHEDULER() domes_trace_hook_isr_exit()

#endif
