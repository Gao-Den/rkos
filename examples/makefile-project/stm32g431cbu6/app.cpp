/**
 ******************************************************************************
 * @author: GaoDen
 * @date:   02/02/2026
 ******************************************************************************
**/

#include "app.h"

/* system include */
#include "sys.h"
#include "io_cfg.h"

/* os include */
#include "rk_port.h"
#include "task.h"
#include "message.h"
#include "mailbox.h"

void task_a();
void task_b();
void task_c();

void app() {
    /******************************************************************************
    * hardware init
    *******************************************************************************/

    /******************************************************************************
    * kernel start
    *******************************************************************************/
    /* task os init */
    task_os_init();
    task_os_create(0x01, task_a, TASK_PRIORITY_1, 128, MAILBOX_NULL, 0, (const uint8_t*)"task_a");
    task_os_create(0x02, task_b, TASK_PRIORITY_2, 128, MAILBOX_NULL, 0, (const uint8_t*)"task_b");
    task_os_create(0x03, task_c, TASK_PRIORITY_3, 128, MAILBOX_NULL, 0, (const uint8_t*)"task_c");
    task_os_run();
}

void task_a() {
    while (1) {
        led_pc6_on();
        task_os_delay(500);
        led_pc6_off();
        task_os_delay(500);
    }
}

void task_b() {
    while (1) {
        led_pb10_on();
        task_os_delay(500);
        led_pb10_off();
        task_os_delay(500);
    }
}

void task_c() {
    while (1) {
        led_pb11_on();
        xprintf("[task_b] system milis: %ld ms\n", sys_ctrl_millis());
        task_os_delay(500);
        led_pb11_off();
        task_os_delay(500);
    }
}
