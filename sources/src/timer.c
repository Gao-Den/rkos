/**
 ******************************************************************************
 * @file:   timer.c
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  timer service (timer os and timer event)
 ******************************************************************************
**/

#include "timer.h"

#include "rk.h"
#include "mailbox.h"
#include "message.h"
#include "heap.h"

#if defined (TIMER_LOG_EN)
#define TIMER_LOG(fmt, ...)                  OS_LOG((const char*)fmt, ##__VA_ARGS__)
#else
#define TIMER_LOG(fmt, ...)
#endif

/**
 * @brief Timer os management
 */
timer_service_t* timer_list_head = TIMER_NULL;
mailbox_t timer_mailbox;
static volatile timer_tick_t timer_tick_irq = {0, RK_DISABLE};

/**
 * @brief Timer event memory pool
 */
static uint16_t timer_node_used = 0;
static uint16_t timer_node_used_max = 0;
static timer_event_t* timer_event_list = TIMER_EVENT_NULL;
static timer_event_t* timer_pool_free = TIMER_EVENT_NULL;
static timer_event_t timer_node_pool[TIMER_EVENT_POOL_MAX_SIZE];

/**
 * @brief Timer event callback
 * @param[in] timer pointer to the timer service control block of the timer event
 */
void timer_event_callback(void* timer);

/**
 * @brief Timer event pool initialization
 * @param[in] none
 * @return none
 * @note This function will be called by the kernel to initialize the timer event memory pool
 */
void timer_event_pool_init() {
    OS_ENTRY_CRITICAL();

    timer_pool_free = (timer_event_t*)timer_node_pool;
     
    for (uint32_t index = 0; index < TIMER_EVENT_POOL_MAX_SIZE; index++) {
        if (index == (TIMER_EVENT_POOL_MAX_SIZE - 1)) {
            timer_node_pool[index].next = TIMER_EVENT_NULL;
        }
        else {
            timer_node_pool[index].next = &timer_node_pool[index + 1];
        }
    }

    OS_EXIT_CRITICAL();
}

/**
 * @brief Get a timer event from the pool
 * @param[in] none
 * @return pointer to the timer event
 */
timer_event_t* timer_event_pool_get() {
    OS_ENTRY_CRITICAL();

    timer_event_t* ret = TIMER_EVENT_NULL;
    ret = timer_pool_free;

    if (ret == TIMER_EVENT_NULL) {
        OS_FATAL("TIMER", 0x01);
    }

    timer_pool_free = ret->next;
    timer_node_used++;
    
    if (timer_node_used > timer_node_used_max) {
        timer_node_used_max = timer_node_used;
    }

    OS_EXIT_CRITICAL();

    return ret;
}

/**
 * @brief Give a timer event return to the pool
 * @param[in] node pointer to the timer event to be freed (MUST-BE not NULL)
 * @return none
 */
void timer_event_pool_free(timer_event_t* node) {
    OS_ENTRY_CRITICAL();
    node->next = timer_pool_free;
    timer_pool_free = node;
    timer_node_used--;
    OS_EXIT_CRITICAL();
}

/**
 * @brief Timer service initialization
 * @param[in] none
 * @return
 *          - TIMER_OK: timer service init success
 *          - TIMER_NG: timer service init fail
 * @note This function will be called by the kernel to initialize the timer service
 */
timer_status_t timer_init() {
    /* timer event pool init */
    timer_event_pool_init();

    /* timer os mailbox init */
    if (mailbox_create(&timer_mailbox, TIMER_TASK_QUEUE_MAX_SIZE, sizeof(rk_msg_t*)) == MAILBOX_OK) {
        return TIMER_OK;
    }
    
    return TIMER_NG;
}

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
timer_service_t* timer_create(const uint8_t* timer_name, uint64_t period, timer_type_t type, void* message, pf_callback callback) {
    OS_ENTRY_CRITICAL();

    timer_service_t* timer = (timer_service_t*)heap_malloc(sizeof(timer_service_t));

    if (timer == TIMER_NULL) {
        OS_FATAL("TIMER", 0x02);
    }

    timer->timer_name = (const uint8_t*)timer_name;
    timer->timer_counter = period;
    timer->timer_period = period;
    timer->timer_type = type;
    timer->callback = callback;
    timer->message = message;

    OS_EXIT_CRITICAL();

    return timer;
}

/**
 * @brief Timer service delete the specified timer
 * @param[in] timer pointer to the timer service control block
 * @return 
 *          - TIMER_OK: timer delete success
 *          - TIMER_NG: timer remove fail
 */
timer_status_t timer_delete(timer_service_t* timer) {

    OS_ENTRY_CRITICAL();

    timer_service_t* current_node = timer_list_head;
    timer_service_t* prev_node = timer_list_head;

    while (current_node != TIMER_NULL) {
        if (current_node == timer) {
            if (current_node == timer_list_head) {
                timer_list_head = current_node->next;
            }
            else {
                prev_node->next = current_node->next;
            }

            heap_free(current_node);

            OS_EXIT_CRITICAL();

            return TIMER_OK;
        }
        else {
            prev_node = current_node;
            current_node = current_node->next;
        }
    }

    OS_EXIT_CRITICAL();

    return TIMER_NG;
}

/**
 * @brief Timer service change the period of the specified timer
 * @param[in] timer pointer to the timer service control block
 * @param[in] period new timer period in milliseconds
 * @return none
 */
void timer_change_period(timer_service_t* timer, uint64_t period) {
    OS_ENTRY_CRITICAL();
    timer->timer_period = period;
    timer->timer_counter = period;
    OS_EXIT_CRITICAL();
}

/**
 * @brief Timer service start the specified timer
 * @param[in] timer pointer to the timer service control block
 * @return none
 */
void timer_start(timer_service_t* timer) {
    OS_ENTRY_CRITICAL();
    timer->next = timer_list_head;
    timer_list_head = timer;
    OS_EXIT_CRITICAL();
}

/**
 * @brief Timer service tick
 * @param[in] tick timer tick heart counter to decrement (unit: milliseconds)
 * @return none
 */
void timer_tick(uint32_t tick) {
    if (timer_list_head != TIMER_NULL) {
        
        timer_tick_irq.tick += tick;

        if (timer_tick_irq.state == RK_ENABLE) {
            OS_ENTRY_CRITICAL();

            timer_tick_irq.state = RK_DISABLE;

            rk_msg_t* msg = get_pure_msg();
            msg->src_task_id = 0xFF;
            msg->des_task_id = TIMER_TASK_ID;
            msg->signal = TIMER_TICK_SIG;

            OS_EXIT_CRITICAL();

            if (mailbox_send(&timer_mailbox, (uint8_t*)&msg, TASK_DELAY_INFINITE) != MAILBOX_OK) {
                OS_FATAL("TIMER", 0x03);
            }
        }
    }
}

/**
 * @brief Timer service task
 * @param[in] none
 * @return none
 */
void timer_task() {
    /* timer init */
    if (timer_init() != TIMER_OK) {
        OS_FATAL("TIMER", 0x04);
    }

    rk_msg_t* msg = RK_MSG_NULL;
    timer_tick_irq.state = RK_ENABLE;

    while (1) {
        if (mailbox_receive(&timer_mailbox, (uint8_t*)&msg, TASK_DELAY_INFINITE) != MAILBOX_OK) {
            OS_FATAL("TIMER", 0x05);
        }

        timer_service_t* current_node;
        timer_service_t* p_timer_delete = TIMER_NULL; /* must-be assign TIMER_NULL */
        uint32_t irq_counter;

        OS_ENTRY_CRITICAL();

        current_node = timer_list_head;
        irq_counter = timer_tick_irq.tick;

        timer_tick_irq.tick = 0;
        timer_tick_irq.state = RK_ENABLE;

        OS_EXIT_CRITICAL();

        switch (msg->signal) {
        case TIMER_TICK_SIG: {
            while (current_node != TIMER_NULL) {
                OS_ENTRY_CRITICAL();
                
                if (irq_counter < (current_node->timer_counter)) {
                    current_node->timer_counter -= irq_counter;
                }
                else {
                    current_node->timer_counter = 0;
                }
                
                uint32_t ref_counter = current_node->timer_counter;

                OS_EXIT_CRITICAL();

                if (ref_counter == 0) {
                    /* timer callback */
                    current_node->callback((void*)current_node->message);

                    OS_ENTRY_CRITICAL();

                    if (current_node->timer_type == TIMER_PERIODIC) {
                        current_node->timer_counter = current_node->timer_period;
                    }
                    else {
                        p_timer_delete = current_node;
                    }

                    OS_EXIT_CRITICAL();
                }

                current_node = current_node->next;

                if (p_timer_delete) {
                    if (timer_delete(p_timer_delete) != TIMER_OK) {
                        OS_FATAL("TIMER", 0x06);
                    }

                    p_timer_delete = TIMER_NULL;
                }
            }
        }
            break;

        default: {
            OS_FATAL("TIMER", 0x07);
        }
            break;
        }

        free_msg(msg);
    }
}

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
timer_status_t timer_set(uint8_t task_id, uint8_t signal, uint64_t period, timer_type_t type) {
    OS_ENTRY_CRITICAL();
    timer_event_t* p_timer = timer_event_list;
    OS_EXIT_CRITICAL();

    while (p_timer != TIMER_EVENT_NULL) {
        if ((p_timer->des_task_id == task_id) && (p_timer->signal == signal)) {
            timer_change_period(p_timer->timer_os, period);
            return TIMER_OK;
        }
        
        p_timer = p_timer->next;
    }

    OS_ENTRY_CRITICAL();

    /* timer event create */
    timer_event_t* timer_event = timer_event_pool_get();
    timer_event->src_task_id = TIMER_TASK_ID;
    timer_event->des_task_id = task_id;
    timer_event->signal = signal;

    /* timer os create */
    timer_service_t* ret = timer_create((const uint8_t*)"timer_event", period, type, (void*)timer_event, timer_event_callback);
    timer_event->timer_os = ret;
    timer_start(timer_event->timer_os);

    /* timer event list */
    timer_event->next = timer_event_list;
    timer_event_list = timer_event;

    OS_EXIT_CRITICAL();

    return TIMER_OK;
}

/**
 * @brief Timer event remove signal
 * @param[in] task_id destination task id
 * @param[in] signal signal event
 * @return 
 *          - TIMER_OK: timer remove success
 *          - TIMER_NG: timer remove fail
 */
timer_status_t timer_remove(uint8_t task_id, uint8_t signal) {
    OS_ENTRY_CRITICAL();
    timer_event_t* current_node = timer_event_list;
    timer_event_t* previous_node = TIMER_EVENT_NULL;
    OS_EXIT_CRITICAL();

    while (current_node != TIMER_EVENT_NULL) {
        if ((current_node->des_task_id == task_id) && (current_node->signal == signal)) {
            OS_ENTRY_CRITICAL();

            if (current_node == timer_event_list) {
                timer_event_list = current_node->next;
            } 
            else {
                previous_node->next = current_node->next;
            }

            OS_EXIT_CRITICAL();

            /* timer delete */
            timer_delete(current_node->timer_os);
            timer_event_pool_free(current_node);

            return TIMER_OK;
        }

        OS_ENTRY_CRITICAL();
        previous_node = current_node;
        current_node = current_node->next;
        OS_EXIT_CRITICAL();
    }

    return TIMER_NG;
}

/**
 * @brief Timer event callback
 * @param[in] timer pointer to the timer service control block of the timer event
 */
void timer_event_callback(void* timer) {
    OS_ENTRY_CRITICAL();

    timer_event_t* timer_event = (timer_event_t*)timer;

    if (timer_event == TIMER_EVENT_NULL) {
        OS_EXIT_CRITICAL();
        OS_FATAL("TIMER", 0x08);
    }

    rk_msg_t* msg = get_pure_msg();
    msg->src_task_id = timer_event->src_task_id;
    msg->des_task_id = timer_event->des_task_id;
    msg->signal = timer_event->signal;
    task_post(msg->des_task_id, msg);

    /* timer list update */
    if (timer_event->timer_os->timer_type == TIMER_ONE_SHOT) {
        timer_event_pool_free(timer_event);
    }

    OS_EXIT_CRITICAL();
}
