/**
 ******************************************************************************
 * @file:   message.h
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  message package management
 ******************************************************************************
**/

#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rk.h"

#define PURE_MSG_TYPE 					(0x01)
#define COMMON_MSG_TYPE                 (0x02)
#define DYNAMIC_MSG_TYPE                (0x04)
#define RK_MSG_NULL                     ((rk_msg_t*)0)

/**
 * @brief Message header structure (Common for pure, common, dynamic message)
 */
typedef struct rk_msg_t {
    /* private for kernel */
    struct rk_msg_t* next;      /* message management (pool memory) */
    uint8_t msg_type;           /* message type (pure, common, dynamic) */

    /* public for user application */
    uint8_t src_task_id;        /* source task id (where message is sent) */
    uint8_t des_task_id;        /* destination task id (where the message will be sent) */
    uint8_t signal;             /* message execution signal */

    /* link layer (network) */
    uint8_t if_src_task_id;
    uint8_t if_des_task_id;
    uint8_t if_signal;
    uint8_t if_msg_type;
} rk_msg_t;

/**
 * @brief Message pure type (Only message header)
 */
typedef struct {
    rk_msg_t msg_header;
} rk_pure_msg_t;

/**
 * @brief Message common type (Message contain static data, data size is limited by RK_COMMON_MSG_DATA_SIZE)
 */
typedef struct {
    rk_msg_t msg_header;
    uint8_t data_size;
    uint8_t data[RK_COMMON_MSG_DATA_SIZE];
} rk_common_msg_t;

/**
 * @brief Message dynamic type (Message contain dynamic data, data size is dynamic allocated by user application)
 */
typedef struct {
    rk_msg_t msg_header;
    uint8_t data_size;
    uint8_t* data;
} rk_dynamic_msg_t;

/**
 * @brief Message initialization (Message pool)
 * @param[in] none
 * @return none
 * @note This function will be called by the kernel
 */
extern void msg_init();

/**
 * @brief Free a message (Any type of message)
 * @param[in] msg pointer to the message to be freed (MUST-BE not NULL)
 * @return none
 */
extern void free_msg(rk_msg_t* msg);

/**
 * @brief Take a pure message from the pool
 * @param[in] none
 * @return pointer to the allocated pure message
 */
extern rk_msg_t* get_pure_msg();

/**
 * @brief Get a available pure message (Message free in the pool)
 * @param[in] none
 * @return number of free pure messages in the pool
 */
extern uint8_t get_pure_msg_free();

/**
 * @brief Take a common message from the pool
 * @param[in] none
 * @return pointer to the allocated common message
 */
extern rk_msg_t* get_common_msg();

/**
 * @brief Set data for a common message
 * @param[in] msg pointer to the common message (MUST-BE not NULL)
 * @param[in] data pointer to the data to be set (MUST-BE not NULL)
 * @param[in] size size of the data to be set (MUST-BE less than or equal to RK_COMMON_MSG_DATA_SIZE)
 * @return none
 */
extern void set_data_common_msg(rk_msg_t* msg, uint8_t* data, uint8_t size);

/**
 * @brief Get data from a common message
 * @param[in] msg pointer to the common message (MUST-BE not NULL)
 * @return pointer to the data in the common message
 */
extern uint8_t* get_data_common_msg(rk_msg_t* msg);

/**
 * @brief Get a available common message (Message free in the pool)
 * @param[in] none
 * @return number of free common messages in the pool
 */
extern uint8_t get_common_msg_free();

/**
 * @brief Take a dynamic message from the pool
 * @param[in] none
 * @return pointer to the allocated dynamic message
 */
extern rk_msg_t* get_dynamic_msg();

/**
 * @brief Set data for a dynamic message
 * @param[in] msg pointer to the dynamic message (MUST-BE not NULL)
 * @param[in] data pointer to the data to be set (MUST-BE not NULL)
 * @param[in] size size of the data to be set
 * @return none
 */
extern void set_data_dynamic_msg(rk_msg_t* msg, uint8_t* data, uint32_t size);

/**
 * @brief Get data from a dynamic message
 * @param[in] msg pointer to the dynamic message (MUST-BE not NULL)
 * @return pointer to the data in the dynamic message
 */
extern uint8_t* get_data_dynamic_msg(rk_msg_t* msg);

/**
 * @brief Get a available dynamic message (Message free in the pool)
 * @param[in] none
 * @return number of free dynamic messages in the pool
 */
extern uint8_t get_dynamic_msg_free();

#ifdef __cplusplus
}
#endif

#endif /* __MESSAGE_H__ */
