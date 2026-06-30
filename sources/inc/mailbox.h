/**
 ******************************************************************************
 * @file:   mailbox.h
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  mailbox service (task communication)
 ******************************************************************************
**/

#ifndef __MAILBOX_H__
#define __MAILBOX_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "task.h"

#define MAILBOX_NULL                    ((mailbox_t*)0)

/**
 * @brief Mailbox execution result status
 */
typedef enum {
    MAILBOX_OK = 0x00,
    MAILBOX_NG,
} mailbox_status_t;

/**
 * @brief Mailbox control block
 */
typedef struct mailbox_t {
    struct mailbox_t* next;     /* mailbox next (management) */

    task_list_t task_send;      /* task list waiting for sending (mailbox full) */
    task_list_t task_recv;      /* task list waiting for receiving (mailbox empty) */

    uint32_t element_size;      /* mailbox element size */
    uint16_t queue_size;        /* mailbox queue size */
    uint8_t head_index;         /* mailbox head index */
    uint8_t tail_index;         /* mailbox tail index */
    uint8_t fill_size;          /* mailbox fill size (number of elements currently in the mailbox) */
    uint8_t* buffer;            /* mailbox buffer pointer (allocated by user application, MUST-BE not NULL) */
} mailbox_t;

/**
 * @brief Create a mailbox
 * @param[in] mailbox pointer to the mailbox object (MUST-BE not NULL)
 * @param[in] queue_size size of the mailbox queue
 * @param[in] element_size size of each element in the mailbox
 * @return
 *          - MAILBOX_OK: mailbox create success
 *          - MAILBOX_NG: mailbox create fail
 */
extern mailbox_status_t mailbox_create(mailbox_t* mailbox, uint16_t queue_size, uint32_t element_size);

/**
 * @brief Delete a mailbox
 * @param[in] mailbox pointer to the mailbox object (MUST-BE not NULL)
 * @return
 *          - MAILBOX_OK: mailbox delete success
 *          - MAILBOX_NG: mailbox delete fail
 */
extern mailbox_status_t mailbox_delete(mailbox_t* mailbox);

/**
 * @brief Send a message to the mailbox
 * @param[in] mailbox pointer to the mailbox object (MUST-BE not NULL)
 * @param[in] element pointer to the element to send (MUST-BE not NULL)
 * @param[in] timeout_ms timeout in milliseconds for sending (MAILBOX_NULL for no wait, TASK_DELAY_INFINITE for wait indefinitely)
 * @return
 *          - MAILBOX_OK: mailbox send success
 *          - MAILBOX_NG: mailbox send fail
 */
extern mailbox_status_t mailbox_send(mailbox_t* mailbox, void* element, uint32_t timeout_ms);

/**
 * @brief Receive a message from the mailbox
 * @param[in] mailbox pointer to the mailbox object (MUST-BE not NULL)
 * @param[in] element pointer to the element to receive (MUST-BE not NULL)
 * @param[in] timeout_ms timeout in milliseconds for receiving (MAILBOX_NULL for no wait, TASK_DELAY_INFINITE for wait indefinitely)
 * @return
 *          - MAILBOX_OK: mailbox receive success
 *          - MAILBOX_NG: mailbox receive fail
 */
extern mailbox_status_t mailbox_receive(mailbox_t* mailbox, void* element, uint32_t timeout_ms);

/**
 * @brief Mailbox tick (Polling)
 * 
 * This function will be called periodically by the kernel to manage mailbox timeouts and unblock tasks waiting on mailboxes
 * 
 * @param[in] tick tick count to decrement (unit: milliseconds)
 * @return none
 * @note This function will be called by the kernel
 */
extern void mailbox_tick(uint32_t tick);

#ifdef __cplusplus
}
#endif

#endif /* __MAILBOX_H__ */
