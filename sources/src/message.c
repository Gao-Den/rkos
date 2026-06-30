/**
 ******************************************************************************
 * @file:   message.c
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  message package management
 ******************************************************************************
**/

#include "message.h"
#include "heap.h"

#if defined (MESSAGE_LOG_EN)
#define MESSAGE_LOG(fmt, ...)           OS_LOG((const char*)fmt, ##__VA_ARGS__)
#else
#define MESSAGE_LOG(fmt, ...)
#endif

/* pure message pool memory */
static rk_pure_msg_t pure_msg_pool[RK_PURE_MSG_POOL_SIZE];
static rk_msg_t* free_list_pure_msg_pool;
static uint32_t free_list_pure_msg_used;

/* common message pool memory */
static rk_common_msg_t common_msg_pool[RK_COMMON_MSG_POOL_SIZE];
static rk_msg_t* free_list_common_msg_pool;
static uint32_t free_list_common_used;

/* dynamic message pool memory */
static rk_dynamic_msg_t dynamic_msg_pool[RK_DYNAMIC_MSG_POOL_SIZE];
static rk_msg_t* free_list_dynamic_msg_pool;
static uint32_t free_list_dynamic_msg_used;

/* messages init (private kernel) */
static void pure_msg_init();
static void common_msg_init();
static void dynamic_msg_init();

/**
 * @brief Pure message initialization
 * @note This function is used to initialize the memory pool pure_msg_pool[RK_PURE_MSG_POOL_SIZE]
 */
void pure_msg_init() {

    OS_ENTRY_CRITICAL();

    free_list_pure_msg_pool = (rk_msg_t*)pure_msg_pool;
     
    for (uint32_t index = 0; index < RK_PURE_MSG_POOL_SIZE; index++) {
        pure_msg_pool[index].msg_header.msg_type = PURE_MSG_TYPE;
        if (index == (RK_PURE_MSG_POOL_SIZE - 1)) {
            pure_msg_pool[index].msg_header.next = RK_MSG_NULL;
        }
        else {
            pure_msg_pool[index].msg_header.next = (rk_msg_t*)&pure_msg_pool[index + 1];
        }
    }

    free_list_pure_msg_used = 0;

    OS_EXIT_CRITICAL();
}

/**
 * @brief Take a pure message from the pool
 * @param[in] none
 * @return pointer to the allocated pure message
 */
rk_msg_t* get_pure_msg() {
    rk_msg_t* get_msg;

    OS_ENTRY_CRITICAL();

    get_msg = free_list_pure_msg_pool;

    if (get_msg == RK_MSG_NULL) {
        OS_FATAL("MSG", 0x01);
    }

    free_list_pure_msg_pool = get_msg->next;
    free_list_pure_msg_used++;
    
    get_msg->src_task_id = 0;
    get_msg->des_task_id = 0;
    get_msg->signal = 0;
    
    OS_EXIT_CRITICAL();

    return get_msg;
}

/**
 * @brief Give a pure message return to the pool
 * @param[in] message pointer to the pure message to be freed (MUST-BE not NULL)
 * @return none
 */
void free_pure_msg(rk_msg_t* msg) {
    OS_ENTRY_CRITICAL();

    msg->next = free_list_pure_msg_pool;
    free_list_pure_msg_pool = msg;
    free_list_pure_msg_used--;

    OS_EXIT_CRITICAL();
}

/**
 * @brief Get a available pure message (Message free in the pool)
 * @param[in] none
 * @return number of free pure messages in the pool
 */
uint8_t get_pure_msg_free() {
    return (RK_PURE_MSG_POOL_SIZE - free_list_pure_msg_used);
}

/**
 * @brief Common message init function
 * @note This function is used to initialize the memory pool common_msg_pool[RK_COMMON_MSG_POOL_SIZE]
 */
void common_msg_init() {

    OS_ENTRY_CRITICAL();

    free_list_common_msg_pool = (rk_msg_t*)common_msg_pool;

    for (uint32_t index = 0; index < RK_COMMON_MSG_POOL_SIZE; index++) {
        common_msg_pool[index].msg_header.msg_type = COMMON_MSG_TYPE;
        if (index == (RK_COMMON_MSG_POOL_SIZE - 1)) {
            common_msg_pool[index].msg_header.next = RK_MSG_NULL;
        }
        else {
            common_msg_pool[index].msg_header.next = (rk_msg_t*)&common_msg_pool[index + 1];
        }
    }

    free_list_common_used = 0;

    OS_EXIT_CRITICAL();
}

/**
 * @brief Take a common message from the pool
 * @param[in] none
 * @return pointer to the allocated common message
 */
rk_msg_t* get_common_msg() {
    rk_msg_t* get_msg;

    OS_ENTRY_CRITICAL();

    get_msg = free_list_common_msg_pool;

    if (get_msg == RK_MSG_NULL) {
        OS_FATAL("MSG", 0x02);
    }

    free_list_common_msg_pool = get_msg->next;
    ((rk_common_msg_t*)get_msg)->data_size = 0;
    free_list_common_used++;

    OS_EXIT_CRITICAL();

    return get_msg;
}

/**
 * @brief Set data for a common message
 * @param[in] msg pointer to the common message (MUST-BE not NULL)
 * @param[in] data pointer to the data to be set (MUST-BE not NULL)
 * @param[in] size size of the data to be set (MUST-BE less than or equal to RK_COMMON_MSG_DATA_SIZE)
 * @return none
 */
void set_data_common_msg(rk_msg_t* msg, uint8_t* data, uint8_t size) {
    if (msg->msg_type != COMMON_MSG_TYPE) {
        OS_FATAL("MSG", 0x03);
    }

    if (size > RK_COMMON_MSG_DATA_SIZE) {
        OS_FATAL("MSG", 0x04);
    }

    ((rk_common_msg_t*)msg)->data_size = size;
    memcpy(((rk_common_msg_t*)msg)->data, data, size);
}

/**
 * @brief Get data from a common message
 * @param[in] msg pointer to the common message (MUST-BE not NULL)
 * @return pointer to the data in the common message
 */
uint8_t* get_data_common_msg(rk_msg_t* msg) {
    if (msg->msg_type != COMMON_MSG_TYPE) {
        OS_FATAL("MSG", 0x05);
    }

    return ((rk_common_msg_t*)msg)->data;
}

/**
 * @brief Give a common message return to the pool
 * @param[in] msg pointer to the common message to be freed (MUST-BE not NULL)
 * @return none
 */
void free_common_msg(rk_msg_t* msg) {
    OS_ENTRY_CRITICAL();

    msg->next = free_list_common_msg_pool;
    free_list_common_msg_pool = msg;
    free_list_common_used--;

    OS_EXIT_CRITICAL();
}

/**
 * @brief Get a available common message (Message free in the pool)
 * @param[in] none
 * @return number of free common messages in the pool
 */
uint8_t get_common_msg_free() {
    return (RK_COMMON_MSG_POOL_SIZE - free_list_common_used);
}

/**
 * @brief Dynamic message init function
 * @note This function is used to initialize the memory pool dynamic_msg_pool[RK_DYNAMIC_MSG_POOL_SIZE]
 */
void dynamic_msg_init() {
    OS_ENTRY_CRITICAL();

    free_list_dynamic_msg_pool = (rk_msg_t*)dynamic_msg_pool;

    for (uint32_t index = 0; index < RK_DYNAMIC_MSG_POOL_SIZE; index++) {
        dynamic_msg_pool[index].msg_header.msg_type = DYNAMIC_MSG_TYPE;
        if (index == (RK_DYNAMIC_MSG_POOL_SIZE - 1)) {
            dynamic_msg_pool[index].msg_header.next = RK_MSG_NULL;
        }
        else {
            dynamic_msg_pool[index].msg_header.next = (rk_msg_t*)&dynamic_msg_pool[index + 1];
        }
    }
    
    free_list_dynamic_msg_used = 0;

    OS_EXIT_CRITICAL();
}

/**
 * @brief Take a dynamic message from the pool
 * @param[in] none
 * @return pointer to the allocated dynamic message
 */
rk_msg_t* get_dynamic_msg() {
    rk_msg_t* get_msg;

    OS_ENTRY_CRITICAL();

    get_msg = free_list_dynamic_msg_pool;

    if (get_msg == RK_MSG_NULL) {
        OS_FATAL("MSG", 0x06);
    }

    free_list_dynamic_msg_pool = get_msg->next;
	((rk_dynamic_msg_t*)get_msg)->data_size = 0;
	((rk_dynamic_msg_t*)get_msg)->data = ((uint8_t*)0);
    free_list_dynamic_msg_used++;

    OS_EXIT_CRITICAL();

    return get_msg;
}

/**
 * @brief Set data for a dynamic message
 * @param[in] msg pointer to the dynamic message (MUST-BE not NULL)
 * @param[in] data pointer to the data to be set (MUST-BE not NULL)
 * @param[in] size size of the data to be set
 * @return none
 */
void set_data_dynamic_msg(rk_msg_t* msg, uint8_t* data, uint32_t size) {
    if (msg->msg_type != DYNAMIC_MSG_TYPE) {
        OS_FATAL("MSG", 0x07);
    }

    ((rk_dynamic_msg_t*)msg)->data_size = size;
    ((rk_dynamic_msg_t*)msg)->data = (uint8_t*)heap_malloc(size);
    memcpy(((rk_dynamic_msg_t*)msg)->data, data, size);
}

/**
 * @brief Get data from a dynamic message
 * @param[in] msg pointer to the dynamic message (MUST-BE not NULL)
 * @return pointer to the data in the dynamic message
 */
uint8_t* get_data_dynamic_msg(rk_msg_t* msg) {
    if (msg->msg_type != DYNAMIC_MSG_TYPE) {
        OS_FATAL("MSG", 0x08);
    }

    return ((rk_dynamic_msg_t*)msg)->data;
}

/**
 * @brief Give a dynamic message return to the pool
 * @param[in] msg pointer to the dynamic message to be freed (MUST-BE not NULL)
 * @return none
 */
void free_dynamic_msg(rk_msg_t* msg) {
    OS_ENTRY_CRITICAL();

    msg->next = free_list_dynamic_msg_pool;
    free_list_dynamic_msg_pool = msg;
    free_list_dynamic_msg_used--;

    heap_free(((rk_dynamic_msg_t*)msg)->data);

    OS_EXIT_CRITICAL();
}

/**
 * @brief Get a available dynamic message (Message free in the pool)
 * @param[in] none
 * @return number of free dynamic messages in the pool
 */
uint8_t get_dynamic_msg_free() {
    return (RK_DYNAMIC_MSG_POOL_SIZE - free_list_dynamic_msg_used);
}

/**
 * @brief Message initialization (Message pool)
 * @param[in] none
 * @return none
 * @note This function will be called by the kernel
 */
void msg_init() {
    /* message pool memory init */
    pure_msg_init();
    common_msg_init();
    dynamic_msg_init();

    MESSAGE_LOG("[MESSAGE] message pool initialized successfully\n");
    MESSAGE_LOG("[MESSAGE] pure message pool size: %d\n", RK_PURE_MSG_POOL_SIZE);
    MESSAGE_LOG("[MESSAGE] common message pool size: %d\n", RK_COMMON_MSG_POOL_SIZE);
    MESSAGE_LOG("[MESSAGE] dynamic message pool size: %d\n", RK_DYNAMIC_MSG_POOL_SIZE);
}

/**
 * @brief Free a message (Any type of message)
 * @param[in] msg pointer to the message to be freed (MUST-BE not NULL)
 * @return none
 */
void free_msg(rk_msg_t* msg) {
    switch (msg->msg_type) {
    case PURE_MSG_TYPE: {
        free_pure_msg(msg);
    }
        break;
    
    case COMMON_MSG_TYPE: {
        free_common_msg(msg);
    }
        break;

    case DYNAMIC_MSG_TYPE: {
        free_dynamic_msg(msg);
    }
        break;

    default: {
        OS_FATAL("MSG", 0x0A);
    }
        break;
    }
}
