/**
 ******************************************************************************
 * @author: GaoDen
 * @date:   16/10/2024
 ******************************************************************************
**/

#include "sys.h"

#include "stm32g4xx.h"
#include "stm32g431xx.h"
#include "stm32g4xx_ll_system.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_pwr.h"
#include "stm32g4xx_ll_utils.h"

#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_conf.h"
#include "stm32g4xx_hal_pwr_ex.h"
#include "stm32g4xx_hal_spi.h"

#include "io_cfg.h"
#include "app.h"
#include "xprintf.h"

/* os include */
#include "task.h"

/* define the addresses of data and bss sections */
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern void (*__preinit_array_start[])();
extern void (*__preinit_array_end[])();
extern void (*__init_array_start[])();
extern void (*__init_array_end[])();

/* system configuration */
static void sys_cfg_tick();
static void sys_cfg_clock();
static void sys_cfg_pendsv();

/* system interrupts (core-exception) */
static void default_handler();
static void nmi_handler();
static void hardfault_handler();
static void mem_manage_handler();
static void bus_fault_handler();
static void usage_fault_handler();
static void svc_handler();
static void debug_mon_handler();
static void pendsv_handler();
static void system_tick_handler();

/* external interrupt */
static void usart1_irq_handler();

/* system tick counter */
volatile uint32_t millis_current;

/* nested critical section */
static int32_t nest_entry_critical_counter = 0;

/* reset handler */
void reset_handler(void) {
    /* MUST BE disable isr */
    __disable_irq();

    /* call the system init function */
    SystemInit();

    /*****************************************************************************/
    /* copy the data segment initializers from flash to SRAM
    ******************************************************************************/
    volatile uint32_t *src = &_sidata; /* pointer start address in flash */
    volatile uint32_t *dest = &_sdata; /* pointer start address in sram */
    volatile unsigned i, cnt;
    while (dest < &_edata) {
        *dest++ = *src++;
    }

    /* zero fill to .bss section */
    dest = &_sbss;
    while (dest < &_ebss) {
        *dest++ = 0;
    }

    /*****************************************************************************/
    /* system config
    ******************************************************************************/
    sys_cfg_tick();
    sys_cfg_clock();
    sys_cfg_pendsv();

    /* invoke all static constructors */
    cnt = __preinit_array_end - __preinit_array_start;
    for (i = 0; i < cnt; i++)
        __preinit_array_start[i]();

    cnt = __init_array_end - __init_array_start;
    for (i = 0; i < cnt; i++)
        __init_array_start[i]();

    usart1_init(115200);
    xfunc_output = (void(*)(int))usart1_put_char;

    /* io init */
    io_init();

    __enable_irq();

    /* go to the main application */
    app();
}

/* interrupt vector table */
__attribute__((section(".isr_vector"))) void (*const g_pfnVectors[])(void) = {
    /* system interrupt */
    (void (*)(void))&_estack,                        /* stack pointer */
    reset_handler,                                   /* reset handler */
    nmi_handler,                                     /* nmi handler */
    hardfault_handler,                               /* hard fault handler */
    mem_manage_handler,                              /* mem manage handler */
    bus_fault_handler,                               /* bus fault handler */
    usage_fault_handler,                             /* usage fault handler */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    svc_handler,                                     /* svc handler */
    debug_mon_handler,                               /* debug monitor handler */
    0,                                               /* reserved */
    task_os_pendsv_handler,                          /* pend sv handler */
    system_tick_handler,                             /* systick handler */
    
    /* peripheral interrupts */
    default_handler,                                 /* window watchdog interrupt */
    default_handler,                                 /* pvd/pvm interrupt */
    default_handler,                                 /* rtc tamp lsecss interrupt */
    default_handler,                                 /* rtc wakeup interrupt */
    default_handler,                                 /* flash interrupt */
    default_handler,                                 /* rcc interrupt */
    default_handler,                                 /* exti line0 interrupt */
    default_handler,                                 /* exti line1 interrupt */
    default_handler,                                 /* exti line2 interrupt */
    default_handler,                                 /* exti line3 interrupt */
    default_handler,                                 /* exti line4 interrupt */
    default_handler,                                 /* dma1 channel1 interrupt */
    default_handler,                                 /* dma1 channel2 interrupt */
    default_handler,                                 /* dma1 channel3 interrupt */
    default_handler,                                 /* dma1 channel4 interrupt */
    default_handler,                                 /* dma1 channel5 interrupt */
    default_handler,                                 /* dma1 channel6 interrupt */
    0,                                               /* reserved */
    default_handler,                                 /* adc1 and adc2 interrupt */
    default_handler,                                 /* usb high priority interrupt */
    default_handler,                                 /* usb low priority interrupt */
    default_handler,                                 /* fdcan1 interrupt 0 */
    default_handler,                                 /* fdcan1 interrupt 1 */
    default_handler,                                 /* exti line[9:5] interrupt */
    default_handler,                                 /* tim1 break and tim15 interrupt */
    default_handler,                                 /* tim1 update and tim16 interrupt */
    default_handler,                                 /* tim1 trigger and comm/tim17 interrupt */
    default_handler,                                 /* tim1 capture compare interrupt */
    default_handler,                                 /* tim2 interrupt */
    default_handler,                                 /* tim3 interrupt */
    default_handler,                                 /* tim4 interrupt */
    default_handler,                                 /* i2c1 event interrupt */
    default_handler,                                 /* i2c1 error interrupt */
    default_handler,                                 /* i2c2 event interrupt */
    default_handler,                                 /* i2c2 error interrupt */
    default_handler,                                 /* spi1 interrupt */
    default_handler,                                 /* spi2 interrupt */
    usart1_irq_handler,                              /* usart1 interrupt */
    default_handler,                                 /* usart2 interrupt */
    default_handler,                                 /* usart3 interrupt */
    default_handler,                                 /* exti line[15:10] interrupt */
    default_handler,                                 /* rtc alarm interrupt */
    default_handler,                                 /* usb wakeup interrupt */
    default_handler,                                 /* tim8 break interrupt */
    default_handler,                                 /* tim8 update interrupt */
    default_handler,                                 /* tim8 trigger and comm interrupt */
    default_handler,                                 /* tim8 capture compare interrupt */
    0,                                               /* reserved */
    0,                                               /* reserved */
    default_handler,                                 /* low power timer1 interrupt */
    0,                                               /* reserved */
    default_handler,                                 /* spi3 interrupt */
    default_handler,                                 /* uart4 interrupt */
    0,                                               /* reserved */
    default_handler,                                 /* tim6 and dac interrupt */
    default_handler,                                 /* tim7 interrupt */
    default_handler,                                 /* dma2 channel1 interrupt */
    default_handler,                                 /* dma2 channel2 interrupt */
    default_handler,                                 /* dma2 channel3 interrupt */
    default_handler,                                 /* dma2 channel4 interrupt */
    default_handler,                                 /* dma2 channel5 interrupt */
    0,                                               /* reserved */
    0,                                               /* reserved */
    default_handler,                                 /* ucpd1 interrupt */
    default_handler,                                 /* comparator 1, 2, and 3 interrupt */
    default_handler,                                 /* comparator 4 interrupt */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    default_handler,                                 /* clock recovery system interrupt */
    default_handler,                                 /* sai1 interrupt */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    default_handler,                                 /* floating point unit interrupt */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    0,                                               /* reserved */
    default_handler,                                 /* random number generator interrupt */
    default_handler,                                 /* low power uart1 interrupt */
    default_handler,                                 /* i2c3 event interrupt */
    default_handler,                                 /* i2c3 error interrupt */
    default_handler,                                 /* dma mux overrun interrupt */
    0,                                               /* reserved */
    0,                                               /* reserved */
    default_handler,                                 /* dma2 channel6 interrupt */
    0,                                               /* reserved */
    0,                                               /* reserved */
    default_handler,                                 /* cordic interrupt */
    default_handler                                  /* fmac interrupt */
};

/******************************************************************************
* system configuration
*******************************************************************************/
void sys_cfg_clock() {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* configure the main internal regulator output voltage */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    /* initializes the RCC Oscillators according to the specified parameters in the RCC_OscInitTypeDef structure */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
    RCC_OscInitStruct.PLL.PLLN = 85;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        /* TODO: fatal error */
    }

    /* initializes the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        /* TODO: fatal error */
    }
}

void sys_cfg_tick() {
    /* set interrupt group priority */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    /* system tick init - irq with 1ms period */
    if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK) {
        /* TODO: fatal error */
    }
    else {
        /* init the low level hardware */
        __HAL_RCC_SYSCFG_CLK_ENABLE();
        __HAL_RCC_PWR_CLK_ENABLE();
        HAL_PWREx_DisableUCPDDeadBattery();
    }
}

void sys_cfg_pendsv() {
    NVIC_SetPriority(PendSV_IRQn, 0xFF);
}

/******************************************************************************
* system ultility
*******************************************************************************/
void sys_ctrl_delay_ms(volatile uint32_t count) {
    volatile uint32_t current_ticks = SysTick->VAL;

    /* number of ticks per millisecond */
    const uint32_t tick_per_ms = SysTick->LOAD + 1;

    /* number of ticks need to count */
    const uint32_t number_of_tick = ((count - ((count > 0) ? 1 : 0)) * tick_per_ms);

    /* number of elapsed ticks */
    uint32_t elapsed_tick = 0;
    volatile uint32_t prev_tick = current_ticks;
    while (number_of_tick > elapsed_tick) {
        current_ticks = SysTick->VAL;
        elapsed_tick += (prev_tick < current_ticks) ? (tick_per_ms + prev_tick - current_ticks) : (prev_tick - current_ticks);
        prev_tick = current_ticks;
    }
}

uint32_t sys_ctrl_millis() {
    volatile uint32_t ret;
    ENTRY_CRITICAL();
    ret = millis_current;
    EXIT_CRITICAL();
    return ret;
}

void usart1_irq_handler() {
    volatile uint8_t c = 0;
    c = usart1_get_char();
    /* TODO */
}

void enable_interrupts() {
    __enable_irq();
}

void disable_interrupts() {
    __disable_irq();
}

void enter_critical() {
    if (nest_entry_critical_counter == 0) {
        __disable_irq();
    }
    nest_entry_critical_counter++;
}

void exit_critical() {
    nest_entry_critical_counter--;
    if (nest_entry_critical_counter == 0) {
        nest_entry_critical_counter = 0;
        __enable_irq();
    }
    else if (nest_entry_critical_counter < 0) {
        SYS_FATAL("ISR", 0x01);
    }
}

/******************************************************************************
* arm core exception
*******************************************************************************/
void default_handler() {
    SYS_FATAL("SYS", 0x01);
}

void nmi_handler() {
    SYS_FATAL("SYS", 0x02);
}

void hardfault_handler() {
    SYS_FATAL("SYS", 0x03);
}

void mem_manage_handler() {
    SYS_FATAL("SYS", 0x04);
}

void bus_fault_handler() {
    SYS_FATAL("SYS", 0x05);
}

void usage_fault_handler() {
    SYS_FATAL("SYS", 0x06);
}

void svc_handler() {
    /* TODO */
}

void debug_mon_handler() {
    /* TODO */
}

void pendsv_trigger() {
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

void pendsv_handler() {
    task_os_pendsv_handler();
}

void system_tick_handler() {
    /* increasing millis counter */
    millis_current++;

    /* task os tick */
    task_os_tick(1);

    /* hal tick */
    HAL_IncTick();
}
