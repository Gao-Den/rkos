/**
 ******************************************************************************
 * @file:   mailbox.c
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  mailbox service (task communication)
 ******************************************************************************
**/

#include "mailbox.h"

#include "rk.h"
#include "task.h"
#include "heap.h"

#if defined (MAILBOX_LOG_EN)
#define MAILBOX_LOG(fmt, ...)           OS_LOG((const char*)fmt, ##__VA_ARGS__)
#else
#define MAILBOX_LOG(fmt, ...)
#endif

/* mailbox list management */
mailbox_t* mailbox_list_head = MAILBOX_NULL;

/**
 * @brief Create a mailbox
 * @param[in] mailbox pointer to the mailbox object (MUST-BE not NULL)
 * @param[in] queue_size size of the mailbox queue
 * @param[in] element_size size of each element in the mailbox
 * @return
 *          - MAILBOX_OK: mailbox create success
 *          - MAILBOX_NG: mailbox create fail
 */
mailbox_status_t mailbox_create(mailbox_t* mailbox, uint16_t queue_size, uint32_t element_size) {
    if (mailbox == MAILBOX_NULL || queue_size == 0 || element_size == 0) {
        return MAILBOX_NG;
    }

    OS_ENTRY_CRITICAL();

    /* mailbox list */
    mailbox->next = mailbox_list_head;
    mailbox_list_head = mailbox;

    /* mailbox task wait */
    task_list_init(&mailbox->task_send);
    task_list_init(&mailbox->task_recv);

    /* mailbox init */
    mailbox->element_size = element_size;
    mailbox->queue_size = queue_size;
    mailbox->head_index = 0;
    mailbox->tail_index = 0;
    mailbox->fill_size = 0;
    mailbox->buffer = (uint8_t*)heap_malloc(queue_size * element_size);
    memset((uint8_t*)mailbox->buffer, 0, queue_size * element_size);

    OS_EXIT_CRITICAL();

    if (mailbox->buffer == NULL) {
        OS_FATAL("MAILBOX", 0x01);
        return MAILBOX_NG;
    }

    MAILBOX_LOG("\n");
    MAILBOX_LOG("[MAILBOX] mailbox created\n");
    MAILBOX_LOG("[MAILBOX] mailbox buffer: 0x%08X\n", mailbox->buffer);
    MAILBOX_LOG("[MAILBOX] mailbox queue_size: %d\n", mailbox->queue_size);
    MAILBOX_LOG("[MAILBOX] mailbox element_size: %d\n", mailbox->element_size);
    MAILBOX_LOG("\n");

    return MAILBOX_OK;
}

/**
 * @brief Delete a mailbox
 * @param[in] mailbox pointer to the mailbox object (MUST-BE not NULL)
 * @return
 *          - MAILBOX_OK: mailbox delete success
 *          - MAILBOX_NG: mailbox delete fail
 */
mailbox_status_t mailbox_delete(mailbox_t* mailbox) {
    if (mailbox == MAILBOX_NULL) {
        OS_FATAL("MAILBOX", 0x02);
        return MAILBOX_NG;
    }

    OS_ENTRY_CRITICAL();

    mailbox_t* prev_mailbox = MAILBOX_NULL;
    mailbox_t* current_mailbox = mailbox_list_head;

    while (current_mailbox != MAILBOX_NULL) {
        if (current_mailbox == mailbox) {
            if (prev_mailbox == MAILBOX_NULL) {
                mailbox_list_head = current_mailbox->next;
            }
            else {
                prev_mailbox->next = current_mailbox->next;
            }

            heap_free(current_mailbox->buffer);

            OS_EXIT_CRITICAL();

            return MAILBOX_OK;
        }

        prev_mailbox = current_mailbox;
        current_mailbox = current_mailbox->next;
    }

    OS_EXIT_CRITICAL();

    return MAILBOX_NG;
}

/**
 * @brief Send a message to the mailbox
 * @param[in] mailbox pointer to the mailbox object (MUST-BE not NULL)
 * @param[in] element pointer to the element to send (MUST-BE not NULL)
 * @param[in] timeout_ms timeout in milliseconds for sending (MAILBOX_NULL for no wait, TASK_DELAY_INFINITE for wait indefinitely)
 * @return
 *          - MAILBOX_OK: mailbox send success
 *          - MAILBOX_NG: mailbox send fail
 */
mailbox_status_t mailbox_send(mailbox_t* mailbox, void* element, uint32_t timeout_ms) {
    if (mailbox == MAILBOX_NULL || element == NULL) {
        return MAILBOX_NG;
    }

    OS_ENTRY_CRITICAL();

    if (mailbox->fill_size >= mailbox->queue_size) {
        if (timeout_ms > 0) {
            /* task os ready remove */
            tcb_t* tcb_current = task_os_get_current_tcb();
            task_ready_remove(tcb_current);

            /* task os block */
            tcb_current->delay_counter = timeout_ms;
            task_list_put(&mailbox->task_send, tcb_current);
            tcb_current->state = TASK_STATE_BLOCKED;

            /* task scheduler */
            OS_EXIT_CRITICAL();
            task_os_scheduler();
            OS_ENTRY_CRITICAL();

            if (tcb_current->delay_counter == 0) {
                OS_EXIT_CRITICAL();
                return MAILBOX_NG;
            }
            else {
                /* task mailbox send */
                memcpy((uint8_t*)(mailbox->buffer + mailbox->tail_index * mailbox->element_size), (uint8_t*)element, mailbox->element_size);

                uint32_t next_tail_index = (++mailbox->tail_index) % mailbox->queue_size;
                mailbox->tail_index = next_tail_index;
                mailbox->fill_size++;

                OS_EXIT_CRITICAL();
                return MAILBOX_OK;
            }
        }
        else {
            OS_EXIT_CRITICAL();
            return MAILBOX_NG;
        }
    }
    else {
        OS_ENTRY_CRITICAL();

        memcpy((uint8_t*)(mailbox->buffer + mailbox->tail_index * mailbox->element_size), (uint8_t*)element, mailbox->element_size);

        uint32_t next_tail_index = (++mailbox->tail_index) % mailbox->queue_size;
        mailbox->tail_index = next_tail_index;
        mailbox->fill_size++;

        OS_EXIT_CRITICAL();
    }

    OS_EXIT_CRITICAL();

    return MAILBOX_OK;
}

/**
 * @brief Receive a message from the mailbox
 * @param[in] mailbox pointer to the mailbox object (MUST-BE not NULL)
 * @param[in] element pointer to the element to receive (MUST-BE not NULL)
 * @param[in] timeout_ms timeout in milliseconds for receiving (MAILBOX_NULL for no wait, TASK_DELAY_INFINITE for wait indefinitely)
 * @return
 *          - MAILBOX_OK: mailbox receive success
 *          - MAILBOX_NG: mailbox receive fail
 */
mailbox_status_t mailbox_receive(mailbox_t* mailbox, void* element, uint32_t timeout_ms) {
    if (mailbox == MAILBOX_NULL || element == NULL) {
        return MAILBOX_NG;
    }

    OS_ENTRY_CRITICAL();

    if (mailbox->fill_size == 0) {
        if (timeout_ms > 0) {
            /* task os ready remove */
            tcb_t* tcb_current = task_os_get_current_tcb();
            task_ready_remove(tcb_current);

            /* task os block */
            tcb_current->delay_counter = timeout_ms;
            task_list_put(&mailbox->task_recv, tcb_current);
            tcb_current->state = TASK_STATE_BLOCKED;

            /* task scheduler */
            OS_EXIT_CRITICAL();
            task_os_scheduler();
            OS_ENTRY_CRITICAL();

            if (tcb_current->delay_counter == 0) {
                OS_EXIT_CRITICAL();
                return MAILBOX_NG;
            }
            else {
                memcpy((uint8_t*)element, (uint8_t*)(mailbox->buffer + mailbox->head_index * mailbox->element_size), mailbox->element_size);

                uint32_t next_head_index = (++mailbox->head_index) % mailbox->queue_size;
                mailbox->head_index = next_head_index;
                mailbox->fill_size--;

                OS_EXIT_CRITICAL();
                return MAILBOX_OK;
            }
        }
        else {
            OS_EXIT_CRITICAL();
            return MAILBOX_NG;
        }
    }
    else {
        OS_ENTRY_CRITICAL();
        memcpy((uint8_t*)element, (uint8_t*)(mailbox->buffer + mailbox->head_index * mailbox->element_size), mailbox->element_size);

        uint32_t next_head_index = (++mailbox->head_index) % mailbox->queue_size;
        mailbox->head_index = next_head_index;
        mailbox->fill_size--;
        OS_EXIT_CRITICAL();
    }

    OS_EXIT_CRITICAL();

    return MAILBOX_OK;
}

/**
 * @brief Mailbox polling service
 * 
 * This function will be called periodically by the kernel to manage mailbox timeouts and unblock tasks waiting on mailboxes
 * 
 * @param[in] tick tick count to decrement (unit: milliseconds)
 * @return none
 * @note This function will be called by the kernel
 */
void mailbox_tick(uint32_t tick) {
    OS_ENTRY_CRITICAL();

    mailbox_t* p_mailbox = mailbox_list_head;

    while (p_mailbox != MAILBOX_NULL) {
        /* mailbox send */
        if (task_list_is_available(&p_mailbox->task_send)) {

            tcb_t* p_tcb = task_list_get_head(&p_mailbox->task_send);

            while (p_tcb != TCB_NULL) {
                if (p_mailbox->fill_size < p_mailbox->queue_size) {
                    /* task unblock */
                    task_list_remove(&p_mailbox->task_send, p_tcb);
                
                    /* task wake-up */
                    task_ready_put(p_tcb);
                }
                else {
                    if (p_tcb->delay_counter == 0) {
                        /* mailbox timeout */
                        task_list_remove(&p_mailbox->task_send, p_tcb);
                
                        /* task wake-up */
                        task_ready_put(p_tcb);
                    }
                    else {
                        if (p_tcb->delay_counter != TASK_DELAY_INFINITE) {
                            p_tcb->delay_counter -= tick;
                        }
                    }
                }

                p_tcb = p_tcb->next;
            }
        }

        /* mailbox receive */
        if (task_list_is_available(&p_mailbox->task_recv)) {

            tcb_t* p_tcb = task_list_get_head(&p_mailbox->task_recv);

            while (p_tcb != TCB_NULL) {
                if (p_mailbox->fill_size > 0) {
                    /* task unblock */
                    task_list_remove(&p_mailbox->task_recv, p_tcb);
                
                    /* task wake-up */
                    task_ready_put(p_tcb);
                }
                else {
                    if (p_tcb->delay_counter == 0) {
                        /* mailbox timeout*/
                        task_list_remove(&p_mailbox->task_recv, p_tcb);
            
                        /* task wake-up */
                        task_ready_put(p_tcb);
                    }
                    else {
                        if (p_tcb->delay_counter != TASK_DELAY_INFINITE) {
                            p_tcb->delay_counter -= tick;
                        }
                    }
                }

                p_tcb = p_tcb->next;
            }
        }

        p_mailbox = p_mailbox->next;
    }

    OS_EXIT_CRITICAL();
}
