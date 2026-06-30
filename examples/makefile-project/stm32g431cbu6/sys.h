/**
 ******************************************************************************
 * @author: GaoDen
 * @date:   16/10/2024
 ******************************************************************************
**/

#ifndef __SYS_STARTUP_H__
#define __SYS_STARTUP_H__

#ifdef __cplusplus
 extern "C" {
#endif 

#include <stdint.h>
#include <stdio.h>

#include "xprintf.h"

#define ENTRY_CRITICAL()            enter_critical()
#define EXIT_CRITICAL()             exit_critical()

#define SYS_FATAL(s, c)                                                     \
    do {                                                                    \
        xprintf("FATAL ERROR\n");                                           \
        xprintf("fatal_type: %s \t fatal_code: 0x%02X\n", (s), (c));        \
        while (1);                                                          \
    } while (0)

/* system ultility */
extern uint32_t sys_ctrl_millis();
extern void sys_ctrl_delay_ms(volatile uint32_t count);
extern void enable_interrupts();
extern void disable_interrupts();
extern void enter_critical();
extern void exit_critical();

/* system pendsv trigger */
extern void pendsv_trigger();

#ifdef __cplusplus
}
#endif

#endif /* __SYS_STARTUP_H__ */
