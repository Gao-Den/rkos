/**
 ******************************************************************************
 * @author: GaoDen
 * @date:   16/10/2024
 ******************************************************************************
**/

#ifndef __IO_CFG_H__
#define __IO_CFG_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_ll_spi.h"

#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_conf.h"
#include "stm32g4xx_hal_pwr_ex.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_spi.h"

/* led life function */
extern void led_pc6_on();
extern void led_pc6_off();
extern void led_pb10_on();
extern void led_pb10_off();
extern void led_pb11_on();
extern void led_pb11_off();

/* usart1 function */
extern void usart1_init(uint32_t baudrate);
extern void usart1_put_char(uint8_t c);
extern uint8_t usart1_get_char();

/* io initialize */
extern void io_init();

#ifdef __cplusplus
}
#endif

#endif /* __IO_CFG_H__ */
