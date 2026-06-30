/**
 ******************************************************************************
 * @file:   mutex.c
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  mutex lock service (task synchronization and mutual exclusion)
 ******************************************************************************
**/

#include "mutex.h"
#include "rk.h"

#if defined (MUTEX_LOG_EN)
#define MUTEX_LOG(fmt, ...)           OS_LOG((const char*)fmt, ##__VA_ARGS__)
#else
#define MUTEX_LOG(fmt, ...)
#endif

/**
 * @brief Create a mutex
 * @param[in] mutex pointer to the mutex object (MUST-BE not NULL)
 * @return none
 */
void mutex_init(mutex_t* mutex) {
    OS_ENTRY_CRITICAL();
    mutex->task_owner = TCB_NULL;
    mutex->task_wait.head = TCB_NULL;
    mutex->task_wait.tail = TCB_NULL;
    mutex->lock_counter = 0;
    OS_EXIT_CRITICAL();
}

/**
 * @brief Lock a mutex
 * @param[in] mutex pointer to the mutex object (MUST-BE not NULL)
 * @return
 *          - MUTEX_OK: mutex lock success
 *          - MUTEX_NG: mutex lock fail
 */
mutex_status_t mutex_lock(mutex_t* mutex) {

    OS_ENTRY_CRITICAL();

    while ((mutex->task_owner != TCB_NULL) && (mutex->task_owner != task_os_get_current_tcb())) {

        tcb_t* tcb_current = task_os_get_current_tcb();

        /* mutex priority inheritance */
        if (tcb_current->task_ref.task_priority > mutex->task_owner->task_ref.task_priority) {
            MUTEX_LOG("[MUTEX] mutex_lock() -> task priority inheritance: %s, new priority: %d\n", mutex->task_owner->task_ref.task_name, tcb_current->task_ref.task_priority);
            if (mutex->task_owner->state == TASK_STATE_READY) {
                task_ready_remove(mutex->task_owner);
                mutex->task_owner->task_ref.task_priority = tcb_current->task_ref.task_priority;
                task_ready_put(mutex->task_owner);
            }
            else {
                mutex->task_owner->task_ref.task_priority = tcb_current->task_ref.task_priority;
            }
        }

        /* task current blocked */
        task_ready_remove(tcb_current);
        task_list_put(&mutex->task_wait, tcb_current);

        /* task scheduler */
        OS_EXIT_CRITICAL();
        task_os_scheduler();
        OS_ENTRY_CRITICAL();
    }

    /* mutex lock */
    mutex->task_owner = task_os_get_current_tcb();
    MUTEX_LOG("[MUTEX] mutex_lock() -> task_owner: %s\n", mutex->task_owner->task_ref.task_name);

    if (mutex->lock_counter == 0) {
        mutex->task_owner_root_priority = mutex->task_owner->task_ref.task_priority;
    }

    mutex->lock_counter++;

    OS_EXIT_CRITICAL();

    return MUTEX_OK;
}

/**
 * @brief Unlock a mutex
 * @param[in] mutex pointer to the mutex object (MUST-BE not NULL)
 * @return
 *          - MUTEX_OK: mutex unlock success
 *          - MUTEX_NG: mutex unlock fail
 */
mutex_status_t mutex_unlock(mutex_t* mutex) {
    OS_ENTRY_CRITICAL();

    if (task_os_get_current_tcb() != mutex->task_owner) {
        OS_FATAL("MUTEX", 0x01);
    }

    mutex->lock_counter--;

    if (mutex->lock_counter == 0) {
        if (mutex->task_owner->task_ref.task_priority > mutex->task_owner_root_priority) {
            /* task owner restore priority */
            MUTEX_LOG("[MUTEX] mutex_unlock() -> task owner: %s, restore priority: %d\n", mutex->task_owner->task_ref.task_name, mutex->task_owner_root_priority);
            if (mutex->task_owner->state == TASK_STATE_READY) {
                task_ready_remove(mutex->task_owner);
                mutex->task_owner->task_ref.task_priority = mutex->task_owner_root_priority;
                task_ready_put(mutex->task_owner);
            }
            else {
                mutex->task_owner->task_ref.task_priority = mutex->task_owner_root_priority;
            }
        }

        mutex->task_owner = TCB_NULL;

        if (task_list_is_available(&mutex->task_wait)) {
            /* task block remove */
            tcb_t* tcb_higher_block = task_list_get_higher_priority(&mutex->task_wait);
            task_list_remove(&mutex->task_wait, tcb_higher_block);

            /* task block wake */
            task_ready_put(tcb_higher_block);
        }

        MUTEX_LOG("[MUTEX] mutex_unlock() -> task owner: %s\n", task_os_get_current_tcb()->task_ref.task_name);
    }

    OS_EXIT_CRITICAL();

    return MUTEX_OK;
}
