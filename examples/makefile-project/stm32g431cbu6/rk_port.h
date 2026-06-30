/**
 ******************************************************************************
 * @file:   rk_port.h
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  rk porting interface
 ******************************************************************************
**/

#ifndef __RK_PORT_H__
#define __RK_PORT_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sys.h"
#include "xprintf.h"

#define port_entry_critical()           ENTRY_CRITICAL()
#define port_exit_critical()            EXIT_CRITICAL()

#define port_os_printf(fmt, ...)        xprintf(fmt, ##__VA_ARGS__)
#define port_os_pendsv_trigger()        pendsv_trigger()

#define port_fatal_error(c, m)          SYS_FATAL(c, m)

#ifdef __cplusplus
}
#endif

#endif /* __RK_PORT_H__ */
