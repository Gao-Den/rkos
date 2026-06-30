/**
 ******************************************************************************
 * @file:   rk.h
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  rk kernel main header file
 ******************************************************************************
**/

#ifndef __RK_H__
#define __RK_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rk_port.h"

/*****************************************************************************
 * DEFINITION: RK definitions
 *
 *****************************************************************************/
#define RK_VERSION                          "1.0"
#define RK_ENABLE                           (0x01)
#define RK_DISABLE                          (0x00)

/*****************************************************************************
 * DEFINITION: task
 *
 *****************************************************************************/
#define TASK_POOL_MAX_SIZE                  (8)
#define TASK_PRIORITY_MAX_SIZE              (8)

/*****************************************************************************
 * DEFINITION: message
 *
 *****************************************************************************/
#define RK_PURE_MSG_POOL_SIZE               (4)
#define RK_COMMON_MSG_DATA_SIZE             (64)
#define RK_COMMON_MSG_POOL_SIZE             (4)
#define RK_DYNAMIC_MSG_POOL_SIZE            (4)

/*****************************************************************************
 * DEFINITION: timer
 *
 *****************************************************************************/
#define TIMER_TASK_QUEUE_MAX_SIZE           (4)
#define TIMER_EVENT_POOL_MAX_SIZE           (16)

/*****************************************************************************
 * DEFINITION: heap memory
 *
 *****************************************************************************/
#define HEAP_BUFFER_MAX_SIZE                (16 * 1024)

/*****************************************************************************
 * DEFINITION: kernel porting
 *
 *****************************************************************************/
#define OS_ENTRY_CRITICAL()                 port_entry_critical()
#define OS_EXIT_CRITICAL()                  port_exit_critical()
#define os_pendsv_trigger()                 port_os_pendsv_trigger()
#define OS_FATAL(m, c)                      port_fatal_error(m, c)
#define OS_LOG(fmt, ...)                    port_os_printf(fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __RK_H__ */
