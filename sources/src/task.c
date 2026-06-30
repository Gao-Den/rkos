/**
 ******************************************************************************
 * @file:   task.c
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  task os service (management, scheduling & synchronization)
 ******************************************************************************
**/

#include "task.h"

#include "rk.h"
#include "timer.h"
#include "mailbox.h"
#include "heap.h"

#if defined (TASK_LOG_EN)
#define TASK_LOG(fmt, ...)                  OS_LOG((const char*)fmt, ##__VA_ARGS__)
#else
#define TASK_LOG(fmt, ...)
#endif

#define TASK_STACK_MASK                     (0xBEEFC0DE)
#define TASK_STACK_ALIGNMENT                (4)
#define TASK_STACK_WATERMARK_SIZE           (4 * 32) /* bytes */
#define PMASK(val)                          ((val) ? (uint_fast8_t)(32U - __builtin_clz(val)) : 0) /* depending on the architecture */

typedef void (*pf_func_tick)(uint32_t);

/**
 * @brief Task os current running task and next task to run
 * @note Used for context switching (Handle by the pendsv)
 */
tcb_t* task_os_current;
tcb_t* task_os_next;

/**
 * @brief List of all tasks in the system (Used for task monitoring purposes)    
 */
static tcb_t* task_link = TCB_NULL;

/**
 * @brief User application task table
 * @note User application should define a table of tasks and call task_create_table to create tasks
 */
static task_t* task_list_table;
uint8_t task_table_size;

/**
 * @brief System os status (State machine)
 */
static os_state_t os_state;
os_state_t os_get_state();
void os_set_state(os_state_t state);

/**
 * @brief Task memory pool service
 * @note Memory pool for task control blocks (static allocation).
 *       Configurable by TASK_POOL_MAX_SIZE, if TASK_POOL_MAX_SIZE is set to 0, task control blocks will be allocated through dynamic allocation
 */
static tcb_t task_pool[TASK_POOL_MAX_SIZE];
static tcb_t* task_pool_free_list;
static uint32_t task_pool_used;
static void task_pool_init();
static tcb_t* task_pool_get();
static void task_pool_free(tcb_t* tcb);

/**
 * @brief Task os priority management
 * @note Used for task scheduling purposes
 */
static uint8_t task_ready_mask = 0x00;
static tcb_ready_t task_list_ready[TASK_PRIORITY_MAX_SIZE];
static task_list_t task_list_delay;
static task_list_t task_list_suspend;
void task_os_set_mask(uint8_t mask);
void task_os_clear_mask(uint8_t mask);

/**
 * @brief Task os list management
 * @note Managing related tasks: put, get, remove.
 *       Operation based on the list task: ready, delay, mutex wait, mailbox wait, ...
 */
void task_list_init(task_list_t* list);
void task_list_put(task_list_t* list, tcb_t* tcb);
tcb_t* task_list_get(task_list_t* list);
void task_list_remove(task_list_t* list, tcb_t* tcb);
void task_list_force_remove(tcb_t* tcb);

/**
 * @brief Task os scheduler tick
 * @note Management related to the os tick: task delay tick, task queue tick, ...
 */
static void task_os_tick_idle(uint32_t tick);
static void task_os_tick_run(uint32_t tick);
static void task_os_delay_tick(uint32_t tick);
static pf_func_tick task_os_tick_state = task_os_tick_idle;

/**
 * @brief Task os ultility
 * @note Ultility: task stack overflow, get current active object
 */
void task_stack_overflow();
tcb_t* task_os_get_current_tcb();
uint8_t task_os_get_current_task_id();

/**
 * @brief Task os idle task (Task os default)
 * @param[in] none
 * @return none
 * @note This function will be called by the kernel when there is no ready task to run */
static void task_os_idle();

/**
 * @brief Task control block memory pool init
 * @param[in] none
 * @return none
 * @note This function will be called by the kernel
 */
void task_pool_init() {
    OS_ENTRY_CRITICAL();

    task_pool_free_list = (tcb_t*)&task_pool[0];

    for (uint8_t i = 0; i < TASK_POOL_MAX_SIZE; i++) {
        task_pool[i].task_stack = (uint32_t*)0;
        task_pool[i].state = TASK_STATE_FREE;
        task_pool[i].delay_counter = 0;
        memset((task_t*)&task_pool[i].task_ref, 0, sizeof(task_t));

        if (i == (TASK_POOL_MAX_SIZE - 1)) {
            task_pool[i].next = TCB_NULL;
        }
        else {
            task_pool[i].prev = TCB_NULL;
            task_pool[i].next = (tcb_t*)&task_pool[i + 1];
        }
    }

    task_pool_used = 0;

    OS_EXIT_CRITICAL();
}

/**
 * @brief Task control block pool get
 * @param[in] none
 * @return pointer to the allocated task control block
 */
tcb_t* task_pool_get() {
    tcb_t* ret;

    OS_ENTRY_CRITICAL();

    ret = task_pool_free_list;

    if (ret == TCB_NULL) {
        OS_FATAL("OS", 0x01);
    }

    task_pool_free_list = ret->next;
    task_pool_used++;

    ret->prev = TCB_NULL;
    ret->next = TCB_NULL;

    OS_EXIT_CRITICAL();

    return ret;
}

/**
 * @brief Task control block pool free
 * @param[in] tcb pointer to the task control block to be freed (MUST-BE not NULL)
 * @return none
 */
void task_pool_free(tcb_t* tcb) {
    OS_ENTRY_CRITICAL();

    tcb->state = TASK_STATE_FREE;
    tcb->delay_counter = 0;
    memset((task_t*)&tcb->task_ref, 0, sizeof(task_t));
    tcb->prev = TCB_NULL;
    tcb->next = task_pool_free_list;
    task_pool_free_list = tcb;
    task_pool_used--;

    OS_EXIT_CRITICAL();
}

/**
 * @brief Get the number of free task control blocks in the pool
 * @param[in] none
 * @return number of free task control blocks in the pool
 */
uint8_t task_pool_get_free_size() {
    uint8_t ret;

    OS_ENTRY_CRITICAL();
    ret = TASK_POOL_MAX_SIZE - task_pool_used;
    OS_EXIT_CRITICAL();

    return ret;
}

/**
 * @brief Initialize a task list
 * @param[in] list pointer to the task list to be initialized (MUST-BE not NULL)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void task_list_init(task_list_t* list) {
    list->head = TCB_NULL;
    list->tail = TCB_NULL;
}

/**
 * @brief Put the specified task control block to the end of the given task list
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @param[in] tcb pointer to the task control block to be added (MUST-BE not NULL)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void task_list_put(task_list_t* list, tcb_t* tcb) {
    OS_ENTRY_CRITICAL();

    if (tcb == TCB_NULL) {
        OS_FATAL("FIFO", 0x01);
    }

    tcb->task_list_owner = list;
    tcb->next = TCB_NULL;
    tcb->prev = TCB_NULL;

    if (list->head == TCB_NULL) {
        list->head = tcb;
        list->tail = tcb;
    }
    else {
        list->tail->next = tcb;
        tcb->prev = list->tail;
        list->tail = tcb;
    }

    OS_EXIT_CRITICAL();
}

/**
 * @brief Remove and return the head task control block from the given task list
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @return pointer to the task control block of the head task list. If the list is empty, it will return NULL
 * @warning kernel private (should not be called by user applications directly)
 */
tcb_t* task_list_get(task_list_t* list) {
    OS_ENTRY_CRITICAL();

    tcb_t* ret;

    if (list->head == TCB_NULL) {
        OS_FATAL("FIFO", 0x02);
    }

    ret = list->head;
    list->head = ret->next;

    if (list->head != TCB_NULL) {
        list->head->prev = TCB_NULL;
    }
    else {
        list->tail = TCB_NULL;
    }

    ret->next = TCB_NULL;
    ret->prev = TCB_NULL;

    OS_EXIT_CRITICAL();

    return ret;
}

/**
 * @brief Return the head task control block from the given task list without removing it
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @return pointer to the task control block of the head task (not removed from the list). If the list is empty, it will return NULL
 * @warning kernel private (should not be called by user applications directly)
 */
tcb_t* task_list_get_head(task_list_t* list) {
    OS_ENTRY_CRITICAL();
    tcb_t* ret = list->head;
    OS_EXIT_CRITICAL();

    return ret;
}

/**
 * @brief Remove the specified task control block from the given task list
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @param[in] tcb pointer to the task control block to be removed (MUST-BE not NULL)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void task_list_remove(task_list_t* list, tcb_t* tcb) {
    OS_ENTRY_CRITICAL();

    if ((tcb == TCB_NULL) || (list->head == TCB_NULL)) {
        OS_FATAL("FIFO", 0x03);
    }

    /* last node */
    if ((list->head == tcb) && (list->tail == tcb)) {
        list->head = TCB_NULL;
        list->tail = TCB_NULL;
    }
    /* head node */
    else if (tcb == list->head) {
        list->head = tcb->next;
        list->head->prev = TCB_NULL;
    }
    /* tail node */
    else if (tcb == list->tail) {
        list->tail = tcb->prev;
        list->tail->next = TCB_NULL;
    }
    /* middle node */
    else {
        if ((tcb->prev == TCB_NULL) || (tcb->next == TCB_NULL)) {
            OS_FATAL("FIFO", 0x04);
        }

        tcb->prev->next = tcb->next;
        tcb->next->prev = tcb->prev;
    }

    tcb->next = TCB_NULL;
    tcb->prev = TCB_NULL;
    tcb->task_list_owner = TCB_NULL;

    OS_EXIT_CRITICAL();
}

/**
 * @brief Remove the specified task control block from its current task list (ready, delay, suspend, ...)
 * @param[in] tcb pointer to the task control block to be removed (MUST-BE not NULL)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void task_list_force_remove(tcb_t* tcb) {
    OS_ENTRY_CRITICAL();

    if ((tcb == TCB_NULL) || (tcb->task_list_owner == TCB_NULL)) {
        OS_FATAL("FIFO", 0x05);
    }

    task_list_t* list = (task_list_t*)tcb->task_list_owner;
    task_list_remove(list, tcb);

    OS_EXIT_CRITICAL();
}

/**
 * @brief Remove and return the highest priority task control block from the given task list
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @return pointer to the task control block of the highest priority task (removed from the list). If the list is empty, it will return NULL
 * @warning kernel private (should not be called by user applications directly)
 */
tcb_t* task_list_get_higher_priority(task_list_t* list) {
    OS_ENTRY_CRITICAL();

    if (list->head == TCB_NULL) {
        OS_FATAL("FIFO", 0x05);
    }

    tcb_t* p_tcb = list->head;
    tcb_t* ret = p_tcb;
    uint8_t priority = p_tcb->task_ref.task_priority;

    while ((p_tcb != TCB_NULL)) {
        if ((priority < p_tcb->task_ref.task_priority)) {
            priority = p_tcb->task_ref.task_priority;
            ret = p_tcb;
        }

        p_tcb = p_tcb->next;
    }

    OS_EXIT_CRITICAL();

    return ret;
}

/**
 * @brief Check the availability of the given task list
 * @param[in] list pointer to the task list (MUST-BE not NULL)
 * @return TRUE if the list contain task, FALSE if the list is empty
 * @warning kernel private (should not be called by user applications directly)
 */
bool task_list_is_available(task_list_t* list) {
    if (list->head != TCB_NULL) {
        return true;
    }
    
    return false;
}

/**
 * @brief Put the specified task control block to the ready list based on its priority
 * @param[in] tcb pointer to the task control block to be put in the ready list (MUST-BE not NULL)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void task_ready_put(tcb_t* tcb) {
    OS_ENTRY_CRITICAL();

    task_list_put(&task_list_ready[tcb->task_ref.task_priority].list_ready, tcb);
    tcb->state = TASK_STATE_READY;
    task_os_set_mask(task_list_ready[tcb->task_ref.task_priority].mask);

    OS_EXIT_CRITICAL();
}

/**
 * @brief Remove the specified task control block from the ready list based on its priority
 * @param[in] tcb pointer to the task control block to be removed from the ready list (MUST-BE not NULL)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void task_ready_remove(tcb_t* tcb) {
    OS_ENTRY_CRITICAL();

    task_list_remove(&task_list_ready[tcb->task_ref.task_priority].list_ready, tcb);
    if (!task_list_is_available(&task_list_ready[tcb->task_ref.task_priority].list_ready)) {
        task_os_clear_mask(task_list_ready[tcb->task_ref.task_priority].mask);
    }

    OS_EXIT_CRITICAL();
}

/**
 * @brief Get the current os state
 * @param[in] none
 * @return current os state
 * @warning kernel private (should not be called by user applications directly)
 */
os_state_t os_get_state() {
    return os_state;
}

/**
 * @brief Set the current os state
 * @param[in] state os state to be set
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void os_set_state(os_state_t state) {
    os_state = state;
}

/**
 * @brief RK banner
 * @param[in] none
 * @return none
 */
void os_banner() {
    OS_LOG("\n");
    OS_LOG(" ____  _  _\n");
    OS_LOG("(  _ \\( )/ )\n");
    OS_LOG(" )   /(   (\n");
    OS_LOG("(_)\\_)(_)\\_)\n");
    OS_LOG("\n");
    OS_LOG("Kernel version: %s\n", RK_VERSION);
    OS_LOG("Author: %s\n", "GaoDen");
    OS_LOG("Build date: Jan 10 2026\n");
    OS_LOG("\n");
}

/**
 * @brief This function will initialize the task management system: memory pool, message package, heap system, timer service
 * @param[in] none
 * @return none
 * @note MUST-BE called before any other task os function
 */
void task_os_init() {
    OS_ENTRY_CRITICAL();

    tcb_ready_t* task;

    /* os init state */
    os_set_state(OS_STATE_INIT);

    /* heap init */
    heap_init();

    /* task pool init */
    task_pool_init();

    /* message pool init */
    msg_init();

    /* task init active object */
    for (uint8_t i = 0; i < TASK_PRIORITY_MAX_SIZE; i++) {
        task = &task_list_ready[i];
        task->mask = (1 << i);

        /* task list ready */
        task_list_init(&task_list_ready[i].list_ready);
    }

    /* task list delay */
    task_list_init(&task_list_delay);

    /* task list suspend */
    task_list_init(&task_list_suspend);

    task_ready_mask = 0x00;
    task_os_current = TCB_NULL;
    task_os_next = TCB_NULL;

    /* os update state */
    os_set_state(OS_STATE_READY);

    OS_EXIT_CRITICAL();

    /* task os default */
    task_os_create(TIMER_TASK_ID, timer_task, TASK_PRIORITY_7, 1024, MAILBOX_NULL, 0, (const uint8_t*)"timer_service");
    task_os_create(0xFE, task_os_idle, TASK_PRIORITY_0, 512, MAILBOX_NULL, 0, (const uint8_t*)"task_os_idle");

    /* rk banner */
    os_banner();
}

/**
 * @brief Create a single task
 * @param[in] task_id task id (MUST-BE unique and less than TASK_POOL_MAX_SIZE)
 * @param[in] task_handler task handler function pointer
 * @param[in] task_priority task priority (reference task_priority_t)
 * @param[in] task_stack_size task stack size (unit: uint32_t, MUST-BE greater than 0)
 * @param[in] mailbox pointer to the mailbox object (set MAILBOX_NULL if the task does not require a mailbox)
 * @param[in] mailbox_size mailbox size (set the value to 0 if the task does not require a mailbox)
 * @param[in] task_name pointer to the task name string
 * @return none
 * @note MUST-BE called after task_os_init and before task_os_run
 */
void task_os_create(uint8_t task_id, pf_task task_handler, uint8_t task_priority, uint32_t task_stack_size, void* mailbox, uint16_t mailbox_size, const uint8_t* task_name) {

    tcb_t* tcb = (tcb_t*)task_pool_get();

    OS_ENTRY_CRITICAL();

    /* task mailbox init */
    if (mailbox != MAILBOX_NULL && mailbox_size > 0) {
        tcb->task_ref.mailbox = mailbox;
        tcb->task_ref.mailbox_size = mailbox_size;
        mailbox_create((mailbox_t*)tcb->task_ref.mailbox, tcb->task_ref.mailbox_size, sizeof(rk_msg_t*));
    }

    /* task handler init */
    tcb->task_ref.task_id = task_id;
    tcb->task_ref.task_handler = task_handler;
    tcb->task_ref.task_priority = (task_priority_t)task_priority;
    tcb->task_ref.task_name = task_name;
    tcb->task_ref.task_stack_size = task_stack_size;

    /* task state init */
    tcb->state = TASK_STATE_READY;

    /* task stack memory allocation */
    uint32_t* task_stack_buffer = (uint32_t*)heap_malloc((task_stack_size * sizeof(uint32_t)) + (32 * sizeof(uint32_t))); /* task stack reserved */
    uint32_t* p_stack = &task_stack_buffer[task_stack_size - 1];
    p_stack = (uint32_t*)(((uint32_t)p_stack) & (~((uint32_t)(TASK_STACK_ALIGNMENT - 1)))); /* task stack alignment */

    tcb->task_stack = (uint32_t*)p_stack;
    tcb->task_stack_base = task_stack_buffer;
    tcb->task_stack_end = ((uint32_t)p_stack - ((task_stack_size - 1) * sizeof(uint32_t)));

    TASK_LOG("\n");
    TASK_LOG("[TASK] task created name: %s\n", (const char*)task_name);
    TASK_LOG("[TASK] task created priority: %d\n", task_priority);
    TASK_LOG("[TASK] task created stack allocated size: %d\n", (task_stack_size * 4));
    TASK_LOG("[TASK] task created stack buffer: 0x%08X\n", task_stack_buffer);
    TASK_LOG("[TASK] task created stack buffer end: 0x%08X\n", (uint32_t)&task_stack_buffer[task_stack_size - 1]);
    TASK_LOG("[TASK] task created stack pointer: 0x%08X\n", tcb->task_stack);
    TASK_LOG("[TASK] task created stack end: 0x%08X\n", tcb->task_stack_end);
    TASK_LOG("\n");

    /* task stack watermark */
    tcb->task_stack_limit = tcb->task_stack_end + TASK_STACK_WATERMARK_SIZE;
    uint32_t* p_start = (uint32_t*)tcb->task_stack_end;
    uint32_t* p_end = (uint32_t*)(tcb->task_stack);
    while (p_start < p_end) {
        *p_start++ = TASK_STACK_MASK;
    }

    /* task priority queue */
    task_ready_put(tcb);

    /* task management link */
    if (task_link == TCB_NULL) {
        task_link = tcb;
    }
    else {
        tcb->next_link = task_link;
        task_link = tcb;
    }

    /* task hardware stack frame */
    tcb->task_stack--;
    *tcb->task_stack = 0x01000000;              /* xPSR */
    tcb->task_stack--;
    *tcb->task_stack = (uint32_t)task_handler;  /* PC */
    tcb->task_stack--;
    *tcb->task_stack = 0xFFFFFFFD;              /* LR (EXC_RETURN) */
    tcb->task_stack--;
    *tcb->task_stack = 0;                       /* R12 */
    tcb->task_stack--;
    *tcb->task_stack = 0;                       /* R3 */
    tcb->task_stack--;
    *tcb->task_stack = 0;                       /* R2 */
    tcb->task_stack--;
    *tcb->task_stack = 0;                       /* R1 */
    tcb->task_stack--;
    *tcb->task_stack = 0;                       /* R0 */

    tcb->task_stack -= 8;                       /* space for R4-R11 (saved by pendsv handler) */

    OS_EXIT_CRITICAL();
}

/**
 * @brief Create tasks based on the provided task table (by user application)
 * @param[in] table pointer to the task table
 * @return none
 */
void task_os_create_table(task_t* table) {
    OS_ENTRY_CRITICAL();

    task_list_table = table;
    while (table[task_table_size].task_handler != (pf_task)0) {
        task_os_create(table[task_table_size].task_id,
                       table[task_table_size].task_handler,
                       table[task_table_size].task_priority,
                       table[task_table_size].task_stack_size,
                       table[task_table_size].mailbox,
                       table[task_table_size].mailbox_size,
                       table[task_table_size].task_name);

        task_table_size++;
    }

    OS_EXIT_CRITICAL();
}

/**
 * @brief Delete the specified task
 * @param[in] task_id task id to be deleted
 * @return none
 */
void task_os_delete(uint8_t task_id) {
    OS_ENTRY_CRITICAL();

    tcb_t* p_tcb = task_link;
    tcb_t* p_tcb_prev = TCB_NULL;

    while (p_tcb != TCB_NULL) {
        if (p_tcb->task_ref.task_id == task_id) {
            break;
        }

        p_tcb_prev = p_tcb;
        p_tcb = p_tcb->next_link;
    }

    if (p_tcb == TCB_NULL) {
        OS_FATAL("OS", 0x03);
    }

    /* task link update */
    if (p_tcb_prev != TCB_NULL) {
        p_tcb_prev->next_link = p_tcb->next_link;
    }
    else {
        task_link = p_tcb->next_link;
    }

    /* task list force remove */
    task_list_force_remove(p_tcb);

    /* task stack memory free */
    heap_free((void*)p_tcb->task_stack_base);

    /* task pool free */
    task_pool_free(p_tcb);

    OS_EXIT_CRITICAL();

    /* task scheduler update */
    task_os_scheduler();
}

/**
 * @brief Suspend the specified task
 * @param[in] task_id task id to be suspended
 * @return none
 */
void task_os_suspend(uint8_t task_id) {
    OS_ENTRY_CRITICAL();

    tcb_t* p_tcb = task_link;

    while (p_tcb != TCB_NULL) {
        if (p_tcb->task_ref.task_id == task_id) {
            break;
        }

        p_tcb = p_tcb->next_link;
    }

    if (p_tcb == TCB_NULL) {
        OS_FATAL("OS", 0x03);
    }

    /* task list force remove */
    task_list_force_remove(p_tcb);
    p_tcb->state = TASK_STATE_SUSPENDED;
    task_list_put(&task_list_suspend, p_tcb);

    OS_EXIT_CRITICAL();

    /* task scheduler update */
    task_os_scheduler();
}

/**
 * @brief Resume the specified task
 * @param[in] task_id task id to be resumed
 * @return none
 */
void task_os_resume(uint8_t task_id) {
    OS_ENTRY_CRITICAL();

    tcb_t* p_tcb = task_link;

    while (p_tcb != TCB_NULL) {
        if (p_tcb->task_ref.task_id == task_id) {
            break;
        }

        p_tcb = p_tcb->next_link;
    }

    if (p_tcb == TCB_NULL) {
        OS_FATAL("OS", 0x04);
    }

    if (p_tcb->state == TASK_STATE_SUSPENDED) {
        task_list_remove(&task_list_suspend, p_tcb);
        task_ready_put(p_tcb);
    }
    else {
        OS_FATAL("OS", 0x05);
    }

    OS_EXIT_CRITICAL();
}

/**
 * @brief Trigger the pendsv exception
 * @param[in] none
 * @return none
 */
void task_os_pendsv_trigger() {
    os_pendsv_trigger();
}

/**
 * @brief Task context switching service
 * 
 * User porting this function need to know: Initialize and trigger pendsv concept
 * 
 * @param[in] none
 * @return none
 */
void __attribute__((naked)) task_os_pendsv_handler() {
    __asm volatile (
        /* task current save context */
        "   mrs r0, psp                         \n"
        "   ldr r2, =task_os_current            \n"
        "   ldr r1, [r2]                        \n"
        "   stmdb r0!, {r4-r11}                 \n"
        "   str r0, [r1]                        \n"

        /* task stack failure */
        "   push {r0-r3, lr}                    \n"
        "   bl task_stack_overflow              \n"
        "   pop {r0-r3, lr}                     \n"

        /* task next load context */
        "   ldr r2, =task_os_next               \n"
        "   ldr r1, [r2]                        \n"
        "   ldr r0, [r1]                        \n"
        "   ldmia r0!, {r4-r11}                 \n"
        "   msr psp, r0                         \n"
        
        /* task current update */
        "   ldr r2, =task_os_current            \n"
        "   str r1, [r2]                        \n"
        
        "   bx lr                               \n"
    );
}

/**
 * @brief Jump to the first task and start running
 * @param[in] none
 * @return none
 */
void task_os_jump_psp() {
    /* set PSP to first task's stack */
    __asm volatile (
        "   ldr r0, =task_os_current    \n"
        "   ldr r0, [r0]                \n"
        "   ldr r0, [r0]                \n"  /* get stack pointer */
        "   add r0, r0, #32             \n"  /* skip R4-R11 */
        "   msr psp, r0                 \n"

        /* switch to process stack */
        "   mov r0, #2                  \n"
        "   msr control, r0             \n"
        "   isb                         \n"

        /* pop registers and jump to task */
        "   pop {r0-r3, r12, lr}        \n"
        "   pop {r2}                    \n"
        "   bx r2                       \n"
    );
}

/**
 * @brief Start the os scheduler
 * @param[in] none
 * @return none
 * @note MUST-BE called after task_os_init and task_os_create
 */
void task_os_run() {
    OS_ENTRY_CRITICAL();

    uint8_t ready = PMASK(task_ready_mask);

    if (task_list_is_available(&task_list_ready[ready - 1].list_ready)) {
        tcb_t* tcb_run = task_list_get(&task_list_ready[ready - 1].list_ready);
        task_os_current = tcb_run;
        task_list_put((task_list_t*)&task_list_ready[ready - 1].list_ready, tcb_run);
    }
    else {
        OS_EXIT_CRITICAL();
        return;
    }

    if (task_os_current == TCB_NULL) {
        OS_FATAL("OS", 0x03);
    }

    /* os update state */
    os_set_state(OS_STATE_RUNNING);

    OS_EXIT_CRITICAL();

    /* task run */
    task_os_jump_psp();
}

/**
 * @brief Handle operations related to os ticks
 * @param tick tick heart (unit: milliseconds)
 * @return none
 * @note MUST-BE called in systick interrupt, timer interrupt, ...
 */
void task_os_tick(uint32_t tick) {
    task_os_tick_state(tick);
}

/**
 * @brief Task os tick idle state
 * @param[in] none
 * @return none
 */
void task_os_tick_idle(uint32_t tick) {
    OS_ENTRY_CRITICAL();
    if (os_get_state() != OS_STATE_RUNNING) {
        OS_EXIT_CRITICAL();
        return;
    }
    else {
        task_os_tick_state = task_os_tick_run;
    }
    OS_EXIT_CRITICAL();
}

/**
 * @brief Task os tick run state
 * @param tick tick heart (unit: milliseconds)
 * @return none
 */
void task_os_tick_run(uint32_t tick) {
    /* task os delay tick */
    task_os_delay_tick(tick);

    /* task os queue tick */
    mailbox_tick(tick);

    /* timer service */
    timer_tick(tick);

    OS_ENTRY_CRITICAL();

    /* task os get highest priority */
    uint8_t ready = PMASK(task_ready_mask);

    if (task_list_is_available(&task_list_ready[ready - 1].list_ready)) {
        tcb_t* tcb_run = task_list_get(&task_list_ready[ready - 1].list_ready);
        task_os_next = tcb_run;
        task_list_put((task_list_t*)&task_list_ready[ready - 1].list_ready, tcb_run);
    }
    else {
        OS_EXIT_CRITICAL();
        return;
    }

    OS_EXIT_CRITICAL();

    if (task_os_next != task_os_current) {
        task_os_pendsv_trigger();
    }
}

/**
 * @brief Task scheduling based on the ready task list
 * @param[in] none
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void task_os_scheduler() {
    OS_ENTRY_CRITICAL();

    /* task os get highest priority */
    uint8_t ready = PMASK(task_ready_mask);
    
    if (task_list_is_available(&task_list_ready[ready - 1].list_ready)) {
        tcb_t* tcb_run = task_list_get(&task_list_ready[ready - 1].list_ready);
        task_os_next = tcb_run;
        task_list_put((task_list_t*)&task_list_ready[ready - 1].list_ready, tcb_run);
    }
    else {
        OS_EXIT_CRITICAL();
        return;
    }

    OS_EXIT_CRITICAL();

    if (task_os_next != task_os_current) {
        task_os_pendsv_trigger();
    }
}

/**
 * @brief Block the current task for the specified number of milliseconds, allowing other tasks to run during the delay period
 * @param[in] ms delay time in milliseconds
 * @return none
 * @note MUST-BE called in task handler, user application MUST-NOT call this function in interrupt handler
 */
void task_os_delay(uint32_t ms) {
    OS_ENTRY_CRITICAL();

    tcb_t* tcb = task_os_get_current_tcb();

    tcb->delay_counter = ms;

    if (tcb == TCB_NULL) {
        OS_EXIT_CRITICAL();
        return;
    }

    /* task put to delay list */
    task_ready_remove(tcb);
    task_list_put((task_list_t*)&task_list_delay, tcb);

    task_os_scheduler();

    OS_EXIT_CRITICAL();
}

/**
 * @brief Task delay tick handler, decrease the delay counter of each delayed task and move the task to the ready list when the delay counter reaches 0
 * @param tick tick heart (unit: milliseconds)
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void task_os_delay_tick(uint32_t tick) {
    OS_ENTRY_CRITICAL();

    if (task_list_delay.head == TCB_NULL) {
        OS_EXIT_CRITICAL();
        return;
    }

    tcb_t* p_tcb = task_list_delay.head;
    tcb_t* next_tcb = TCB_NULL;

    while (p_tcb != TCB_NULL) {
        next_tcb = p_tcb->next;
        
        if (p_tcb->delay_counter == 0) {
            task_list_remove((task_list_t*)&task_list_delay, p_tcb);
            task_ready_put(p_tcb);
        }
        else {
            if (p_tcb->delay_counter != TASK_DELAY_INFINITE) {
                p_tcb->delay_counter--;
            }
        }

        p_tcb = next_tcb;
    }

    OS_EXIT_CRITICAL();
}

/**
 * @brief Task post a message
 * @param[in] task_id destination task id
 * @param[in] msg pointer to the message to be sent (MUST-BE not NULL)
 */
void task_post(uint8_t task_id, rk_msg_t* msg) {
    if ((task_id > task_table_size) || (task_list_table[task_id].mailbox == NULL) || (task_list_table[task_id].mailbox_size == 0)) {
        OS_FATAL("OS", 0x04);
    }

    OS_ENTRY_CRITICAL();

    mailbox_t* mailbox = (mailbox_t*)task_list_table[task_id].mailbox;
    if (mailbox != NULL) {
        if (mailbox_send(mailbox, &msg, TASK_DELAY_INFINITE) != MAILBOX_OK) {
            OS_FATAL("OS", 0x05);
        }
    }

    OS_EXIT_CRITICAL();
}

/**
 * @brief Task post a pure message
 * @param[in] des_task_id destination task id
 * @param[in] signal message execution signal
 */
void task_post_pure_msg(uint8_t des_task_id, uint8_t signal) {
    rk_msg_t* msg = get_pure_msg();
    msg->src_task_id = task_os_get_current_task_id();
    msg->des_task_id = des_task_id;
    msg->signal = signal;
    task_post(des_task_id, msg);
}

/**
 * @brief Task post a common message
 * @param[in] des_task_id destination task id
 * @param[in] signal message execution signal
 * @param[in] data pointer to the data to be sent (MUST-BE not NULL)
 * @param[in] size size of the data to be sent (unit: byte, MUST-BE less than or equal to RK_COMMON_MSG_DATA_SIZE)
 */
void task_post_common_msg(uint8_t des_task_id, uint8_t signal, uint8_t* data, uint32_t size) {
    rk_msg_t* msg = get_common_msg();
    msg->src_task_id = task_os_get_current_task_id();
    msg->des_task_id = des_task_id;
    msg->signal = signal;
    set_data_common_msg(msg, data, size);
    task_post(des_task_id, msg);
}

/**
 * @brief Task post a dynamic message
 * @param[in] des_task_id destination task id
 * @param[in] signal message execution signal
 * @param[in] data pointer to the data to be sent (MUST-BE not NULL)
 * @param[in] size size of the data to be sent (unit: byte)
 */
void task_post_dynamic_msg(uint8_t des_task_id, uint8_t signal, uint8_t* data, uint32_t size) {
    rk_msg_t* msg = get_dynamic_msg();
    msg->src_task_id = task_os_get_current_task_id();
    msg->des_task_id = des_task_id;
    msg->signal = signal;
    set_data_dynamic_msg(msg, data, size);
    task_post(des_task_id, msg);
}

/**
 * @brief Task receive the message
 * @param[in] task_id task id to receive message
 * @return pointer to the received message
 */
rk_msg_t* task_receive_msg(uint8_t task_id) {
    rk_msg_t* ret = (rk_msg_t*)0;
    if ((task_id > task_table_size) || (task_list_table[task_id].mailbox == NULL) || (task_list_table[task_id].mailbox_size == 0)) {
        OS_FATAL("OS", 0x06);
    }
    else {
        mailbox_t* mailbox = (mailbox_t*)task_list_table[task_id].mailbox;
        if (mailbox_receive(mailbox, &ret, TASK_DELAY_INFINITE) != MAILBOX_OK) {
            OS_FATAL("OS", 0x07);
        }
    }

    return ret;
}

/**
 * @brief Task free the message
 * @param[in] msg pointer to the message to be freed (MUST-BE not NULL)
 * @return none
 */
void task_free_msg(rk_msg_t* msg) {
    if (msg == NULL) {
        return;
    }

    free_msg(msg);
}

/**
 * @brief Show information about all tasks in the system (id, priority, stack size, stack used, name)
 * @param[in] none
 * @return none
 */
void task_info() {
    OS_ENTRY_CRITICAL();

    tcb_t* p_tcb = task_link;

    OS_LOG("\n\n\n");
    OS_LOG("task infomations\n");
    while (p_tcb != TCB_NULL) {

        uint32_t* p_stack = (uint32_t*)(p_tcb->task_stack_end);
        uint32_t ref_stack = (uint32_t)p_stack;

        while (p_stack < ((uint32_t*)(p_tcb->task_stack_end + (p_tcb->task_ref.task_stack_size * sizeof(uint32_t))))) {
            if (*p_stack != TASK_STACK_MASK) {
                ref_stack = (uint32_t)p_stack;
                break;
            }

            p_stack++;
        }

        uint32_t task_stack_used = (uint32_t)((p_tcb->task_ref.task_stack_size * sizeof(uint32_t)) - (ref_stack - (uint32_t)p_tcb->task_stack_end));
        OS_LOG("[task id]: %d \t[task priority]: %d \t[task stack size]: %d bytes \t[task stack used]: %d bytes \t[task name]: %s\n", p_tcb->task_ref.task_id, p_tcb->task_ref.task_priority, (p_tcb->task_ref.task_stack_size * sizeof(uint32_t)), task_stack_used, p_tcb->task_ref.task_name);

        p_tcb = p_tcb->next_link;
    }
    OS_LOG("\n\n\n");

    OS_EXIT_CRITICAL();
}

/**
 * @brief Get current task control block active at the time the function is called
 * @param[in] none
 * @return pointer to the current task control block
 */
tcb_t* task_os_get_current_tcb() {
    return task_os_current;
}

/**
 * @brief Get current task id
 * @param[in] none
 * @return current task id
 * @note returns task id of the currently active task at the time the function is called
 */
uint8_t task_os_get_current_task_id() {
    OS_ENTRY_CRITICAL();
    uint8_t task_id = task_os_current->task_ref.task_id;
    OS_EXIT_CRITICAL();

    return task_id;
}

/**
 * @brief Task stack overflow monitor
 * @param[in] none
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void task_stack_overflow() {
    uint32_t* p_stack = (uint32_t*)(task_os_current->task_stack_limit - sizeof(uint32_t));

    if (*p_stack != TASK_STACK_MASK) {
        OS_LOG("\n\n\n");
        OS_LOG("-> OS OS_FATAL\n");
        OS_LOG(".fatal info: task stack overflow\n");
        OS_LOG(".task id: %d\n", task_os_current->task_ref.task_id);
        OS_LOG(".task pri: %d\n", task_os_current->task_ref.task_priority);
        OS_LOG(".task name: %s\n", task_os_current->task_ref.task_name);
        OS_LOG(".task stack size: %d bytes\n", (task_os_current->task_ref.task_stack_size * sizeof(uint32_t)));
        OS_FATAL("OS", 0xFE);
    }
}

/**
 * @brief Set the ready mask for specific task priority level
 * @param[in] mask ready mask to be set
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void task_os_set_mask(uint8_t mask) {
    OS_ENTRY_CRITICAL();
    task_ready_mask |= mask;
    OS_EXIT_CRITICAL();
}

/**
 * @brief Clear the ready mask for specific task priority level
 * @param[in] mask ready mask to be cleared
 * @return none
 * @warning kernel private (should not be called by user applications directly)
 */
void task_os_clear_mask(uint8_t mask) {
    OS_ENTRY_CRITICAL();
    task_ready_mask &= ~mask;
    OS_EXIT_CRITICAL();
}

/**
 * @brief Task os idle handler
 * @param[in] none
 * @return none
 * @note This function will be called when no other tasks are ready to run
 */
void task_os_idle() {
    while (1) {
    }
}
