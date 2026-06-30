/**
 ******************************************************************************
 * @file:   task.h
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  task os service (management, scheduling & synchronization)
 ******************************************************************************
**/

#ifndef __TASK_H__
#define __TASK_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "message.h"

#define TCB_NULL                            ((tcb_t*)0)
#define TASK_DELAY_INFINITE                 (0xFFFFFFFF)

typedef void (*pf_task)();

/**
 * @brief System os status (State machine)
 */
typedef enum {
    OS_STATE_INIT = 0x00,
    OS_STATE_READY,
    OS_STATE_RUNNING,
} os_state_t;

/**
 * @brief Task os status
 */
typedef enum {
    TASK_STATE_FREE = 0x00,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SUSPENDED,
} task_state_t;

/**
 * @brief Task priority (Higher value means higher priority)
 */
typedef enum {
    TASK_PRIORITY_0 = 0x00,
    TASK_PRIORITY_1,
    TASK_PRIORITY_2,
    TASK_PRIORITY_3,
    TASK_PRIORITY_4,
    TASK_PRIORITY_5,
    TASK_PRIORITY_6,
    TASK_PRIORITY_7,
} task_priority_t;

/**
 * @brief Task definition (Public for user application)
 */
typedef struct {
    uint8_t task_id;                /* task id (MUST-BE unique and less than TASK_POOL_MAX_SIZE) */
    pf_task task_handler;           /* task handler function pointer */
    task_priority_t task_priority;  /* task priority (reference task_priority_t) */
    uint32_t task_stack_size;       /* task stack size (unit: uint32_t, MUST-BE greater than 0) */
    void* mailbox;                  /* pointer to the mailbox object (set MAILBOX_NULL if the task does not require a mailbox) */
    uint16_t mailbox_size;          /* mailbox size (set the value to 0 if the task does not require a mailbox) */
    const uint8_t* task_name;       /* pointer to the task name string */
} task_t;

/**
 * @brief Task control block
 * @warning kernel private (should not be called by user applications directly)
 */
typedef struct tcb_t {
    uint32_t* task_stack;           /* task stack pointer */
    uint32_t* task_stack_base;      /* task stack buffer */
    uint32_t task_stack_end;        /* task stack end address */
    uint32_t task_stack_limit;      /* task stack limit address (task stack overflow protection) */
    task_state_t state;             /* task status */
    task_t task_ref;                /* task reference */
    uint32_t delay_counter;         /* delay tick: task delay, queue, ... */

    struct tcb_t* next;
    struct tcb_t* prev;
    struct tcb_t* next_link;        /* task management link */
    void* task_list_owner;          /* task list where the tcb_t is currently in: ready, delay, suspend, ... */
} tcb_t;

/**
 * @brief Common list used: ready, delay, mutex wait, mailbox wait, ...
 * @warning kernel private (should not be called by user applications directly)
 */
typedef struct {
    tcb_t* head;
    tcb_t* tail;
} task_list_t;

/**
 * @brief Task ready list
 * @warning kernel private (should not be called by user applications directly)
 */
typedef struct {
    uint8_t mask;
    task_list_t list_ready;
} tcb_ready_t;

/**
 * @brief Initialize the task management system: Memory pool, Message package, Heap system, Timer service
 * @param[in] none
 * @return none
 * @note MUST-BE called before any other task os function
 */
extern void task_os_init();

/**
 * @brief Create a single task
 * @param[in] task_id task id (MUST-BE unique and less than TASK_POOL_MAX_SIZE)
 * @param[in] task_handler task handler function pointer
 * @param[in] task_priority task priority (reference task_priority_t)
 * @param[in] task_stack_size task stack size (unit: uint32_t, MUST-BE greater than 0)
 * @param[in] mailbox pointer to the mailbox object (set MAILBOX_NULL if the task does not require a mailbox)
 * @param[in] mailbox_size mailbox size (set the value to 0 if the task does not require a mailbox)
 * @param[in] task_name pointer to the task name string
 * @return none
 * @note MUST-BE called after task_os_init and before task_os_run
 */
extern void task_os_create(uint8_t task_id, pf_task task_handler, uint8_t task_priority, uint32_t task_stack_size, void* mailbox, uint16_t mailbox_size, const uint8_t* task_name);

/**
 * @brief Create tasks based on the provided task table (by user application)
 * @param[in] table pointer to the task table
 * @return none
 */
extern void task_os_create_table(task_t* table);

/**
 * @brief Delete the specified task
 * @param[in] task_id task id to be deleted
 * @return none
 */
extern void task_os_delete(uint8_t task_id);

/**
 * @brief Suspend the specified task
 * @param[in] task_id task id to be suspended
 * @return none
 */
extern void task_os_suspend(uint8_t task_id);

/**
 * @brief Resume the specified task
 * @param[in] task_id task id to be resumed
 * @return none
 */
extern void task_os_resume(uint8_t task_id);

/**
 * @brief Start the os scheduler
 * @param[in] none
 * @return none
 * @note MUST-BE called after task_os_init and task_os_create
 */
extern void task_os_run();

/**
 * @brief Task os tick (System os tick base)
 * @param tick tick heart (unit: milliseconds)
 * @return none
 * @note MUST-BE called in systick interrupt, timer interrupt, ...
 */
extern void task_os_tick(uint32_t tick);

/**
 * @brief Task scheduling based on the ready task list
 * @param[in] none
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
extern void task_os_scheduler();

/**
 * @brief Task context switching service
 * 
 * User porting this function need to know: Initialize and trigger pendsv concept
 * 
 * @param[in] none
 * @return none
 */
extern void task_os_pendsv_handler() __attribute__((naked));

/**
 * @brief Task post a message
 * @param[in] task_id destination task id
 * @param[in] msg pointer to the message to be sent (MUST-BE not NULL)
 */
extern void task_post(uint8_t task_id, rk_msg_t* msg);

/**
 * @brief Task post a pure message
 * @param[in] des_task_id destination task id
 * @param[in] signal message execution signal
 */
extern void task_post_pure_msg(uint8_t des_task_id, uint8_t signal);

/**
 * @brief Task post a common message
 * @param[in] des_task_id destination task id
 * @param[in] signal message execution signal
 * @param[in] data pointer to the data to be sent (MUST-BE not NULL)
 * @param[in] size size of the data to be sent (unit: byte, MUST-BE less than or equal to RK_COMMON_MSG_DATA_SIZE)
 */
extern void task_post_common_msg(uint8_t des_task_id, uint8_t signal, uint8_t* data, uint32_t size);

/**
 * @brief Task post a dynamic message
 * @param[in] des_task_id destination task id
 * @param[in] signal message execution signal
 * @param[in] data pointer to the data to be sent (MUST-BE not NULL)
 * @param[in] size size of the data to be sent (unit: byte)
 */
extern void task_post_dynamic_msg(uint8_t des_task_id, uint8_t signal, uint8_t* data, uint32_t size);

/**
 * @brief Task receive the message
 * @param[in] task_id task id to receive message
 * @return pointer to the received message
 */
extern rk_msg_t* task_receive_msg(uint8_t task_id);

/**
 * @brief Task free the message
 * @param[in] msg pointer to the message to be freed (MUST-BE not NULL)
 * @return none
 */
extern void task_free_msg(rk_msg_t* msg);

/**
 * @brief Show information about all tasks in the system (id, priority, stack size, stack used, name)
 * @param[in] none
 * @return none
 */
extern void task_info();

/**
 * @brief Get current task control block active at the time the function is called
 * @param[in] none
 * @return pointer to the current task control block
 */
extern tcb_t* task_os_get_current_tcb();

/**
 * @brief Block the current task for the specified number of milliseconds, allowing other tasks to run during the delay period
 * @param[in] ms delay time in milliseconds
 * @return none
 * @note MUST-BE called in task handler, user application MUST-NOT call this function in interrupt handler
 */
extern void task_os_delay(uint32_t ms);

/**
 * @brief Initialize a task list
 * @param[in] list pointer to the task list to be initialized (MUST-BE not NULL)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
extern void task_list_init(task_list_t* list);

/**
 * @brief Put the specified task control block to the end of the given task list
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @param[in] tcb pointer to the task control block to be added (MUST-BE not NULL)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
extern void task_list_put(task_list_t* list, tcb_t* tcb);

/**
 * @brief Remove the specified task control block from the given task list
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @param[in] tcb pointer to the task control block to be removed (MUST-BE not NULL)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
extern void task_list_remove(task_list_t* list, tcb_t* tcb);

/**
 * @brief Remove and return the head task control block from the given task list
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @return pointer to the task control block of the head task list. If the list is empty, it will return NULL
 * @warning kernel private (should not be called by user applications directly)
 */
extern tcb_t* task_list_get(task_list_t* list);

/**
 * @brief Return the head task control block from the given task list without removing it
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @return pointer to the task control block of the head task (not removed from the list). If the list is empty, it will return NULL
 * @warning kernel private (should not be called by user applications directly)
 */
extern tcb_t* task_list_get_head(task_list_t* list);

/**
 * @brief Remove and return the highest priority task control block from the given task list
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @return pointer to the task control block of the highest priority task (removed from the list). If the list is empty, it will return NULL
 * @warning kernel private (should not be called by user applications directly)
 */
extern tcb_t* task_list_get_higher_priority(task_list_t* list);

/**
 * @brief Check the availability of the given task list
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @return TRUE if the list contain task, FALSE if the list is empty
 * @warning kernel private (should not be called by user applications directly)
 */
extern bool task_list_is_available(task_list_t* list);

/**
 * @brief Put the specified task control block to the ready list based on its priority
 * @param[in] tcb pointer to the task control block to be put in the ready list (MUST-BE not NULL)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
extern void task_ready_put(tcb_t* tcb);

/**
 * @brief Remove the specified task control block from the ready list based on its priority
 * @param[in] tcb pointer to the task control block to be removed from the ready list (MUST-BE not NULL)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
extern void task_ready_remove(tcb_t* tcb);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_H__ */
