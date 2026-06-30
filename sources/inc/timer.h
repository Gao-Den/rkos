/**
 ******************************************************************************
 * @file:   timer.h
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  timer service (timer os and timer event)
 ******************************************************************************
**/

#ifndef __TIMER_H__
#define __TIMER_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define TIMER_NULL                          ((timer_service_t*)0)
#define TIMER_EVENT_NULL                    ((timer_event_t*)0)

#define TIMER_TASK_ID                       (0xFD)
#define TIMER_TICK_SIG                      (0xFD)

typedef void (*pf_callback)(void*);

/**
 * @brief Timer execution result status
 */
typedef enum {
    TIMER_OK = 0x00,
    TIMER_NG,
} timer_status_t;

/**
 * @brief Timer type (One-shot, periodically)
 */
typedef enum {
    TIMER_ONE_SHOT = 0x00,
    TIMER_PERIODIC,
} timer_type_t;

/**
 * @brief Timer service control block
 */
typedef struct timer_service_t {
    struct timer_service_t* next;       /* timer service next pointer (timer list management) */

    const uint8_t* timer_name;          /* timer name string pointer (for debugging and monitoring purposes) */
    timer_type_t timer_type;            /* timer type (TIMER_ONE_SHOT, TIMER_PERIODIC) */
    uint64_t timer_counter;             /* timer tick counter */
    uint64_t timer_period;              /* timer period */
    pf_callback callback;               /* timer callback function pointer (MUST-BE not NULL) */
    void* message;                      /* pointer to the message to be sent when the timer expires (can be NULL) */
} timer_service_t;

/**
 * @brief Timer event control block
 */
typedef struct timer_event_t {
    struct timer_event_t* next;         /* timer event next pointer (timer event list management) */

    timer_service_t* timer_os;          /* pointer to the timer service control block created by this timer event (for timer management) */

    uint8_t src_task_id;                /* source task id (where timer event is set) */
    uint8_t des_task_id;                /* destination task id (where the timer event will be sent) */
    uint8_t signal;                     /* timer event signal */
} timer_event_t;

/**
 * @brief Timer tick control block
 */
typedef struct {
    uint64_t tick;
    uint8_t state;
} timer_tick_t;

/**
 * @brief Timer service create a new timer
 * 
 * The created timer will not start automatically, user application must call timer_start() to start the timer
 * 
 * @param[in] timer_name pointer to the timer name string
 * @param[in] period timer period in milliseconds
 * @param[in] type timer type (TIMER_ONE_SHOT, TIMER_PERIODIC)
 * @param[in] message pointer to the message to be sent when the timer expires
 * @param[in] callback timer callback function pointer (MUST-BE not NULL)
 * @return pointer to the created timer service control block
 */
extern timer_service_t* timer_create(const uint8_t* timer_name, uint64_t period, timer_type_t type, void* message, pf_callback callback);

/**
 * @brief Timer service start the specified timer
 * @param[in] timer pointer to the timer service control block
 * @return none
 */
extern void timer_start(timer_service_t* timer);

/**
 * @brief Timer service delete the specified timer
 * @param[in] timer pointer to the timer service control block
 * @return 
 *          - TIMER_OK: timer delete success
 *          - TIMER_NG: timer remove fail
 */
extern timer_status_t timer_delete(timer_service_t* timer);

/**
 * @brief Timer service change the period of the specified timer
 * @param[in] timer pointer to the timer service control block
 * @param[in] period new timer period in milliseconds
 * @return none
 */
extern void timer_change_period(timer_service_t* timer, uint64_t period);

/**
 * @brief Timer service tick
 * @param[in] tick timer tick heart counter to decrement (unit: milliseconds)
 * @return none
 */
extern void timer_tick(uint32_t tick);

/**
 * @brief Timer service task
 * @param[in] none
 * @return none
 */
extern void timer_task();

/**
 * @brief Timer event set signal
 * @param[in] task_id destination task id
 * @param[in] signal signal event
 * @param[in] period timer period in milliseconds
 * @param[in] type timer type (TIMER_ONE_SHOT, TIMER_PERIODIC)
 * @return
 *          - TIMER_OK: timer set success
 *          - TIMER_NG: timer set fail
 */
extern timer_status_t timer_set(uint8_t task_id, uint8_t signal, uint64_t period, timer_type_t type);

/**
 * @brief Timer event remove signal
 * @param[in] task_id destination task id
 * @param[in] signal signal event
 * @return 
 *          - TIMER_OK: timer remove success
 *          - TIMER_NG: timer remove fail
 */
extern timer_status_t timer_remove(uint8_t task_id, uint8_t signal);

#ifdef __cplusplus
}
#endif

#endif /* __TIMER_H__ */
