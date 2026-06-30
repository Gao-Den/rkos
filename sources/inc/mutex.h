/**
 ******************************************************************************
 * @file:   mutex.h
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  mutex lock service (task synchronization and mutual exclusion)
 ******************************************************************************
**/

#ifndef __MUTEX_H__
#define __MUTEX_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "task.h"

/**
 * @brief Mutex execution result status
 */
typedef enum {
    MUTEX_OK = 0x00,
    MUTEX_NG,
} mutex_status_t;

/**
 * @brief Mutex control block
 */
typedef struct mutex_t {
    tcb_t* task_owner;                  /* mutex owner task */
    uint8_t task_owner_root_priority;   /* mutex owner task root priority (for priority inheritance) */
    task_list_t task_wait;              /* mutex wait task list */
    uint8_t lock_counter;               /* mutex lock counter (for recursive lock) */
} mutex_t;

/**
 * @brief Create a mutex
 * @param[in] mutex pointer to the mutex object (MUST-BE not NULL)
 * @return none
 */
extern void mutex_init(mutex_t* mutex);

/**
 * @brief Lock a mutex
 * @param[in] mutex pointer to the mutex object (MUST-BE not NULL)
 * @return
 *          - MUTEX_OK: mutex lock success
 *          - MUTEX_NG: mutex lock fail
 */
extern mutex_status_t mutex_lock(mutex_t* mutex);

/**
 * @brief Unlock a mutex
 * @param[in] mutex pointer to the mutex object (MUST-BE not NULL)
 * @return
 *          - MUTEX_OK: mutex unlock success
 *          - MUTEX_NG: mutex unlock fail
 */
extern mutex_status_t mutex_unlock(mutex_t* mutex);

#ifdef __cplusplus
}
#endif

#endif /* __MUTEX_H__ */
