/**
 ******************************************************************************
 * @file:   heap.c
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  heap memory management for dynamic memory allocation
 ******************************************************************************
**/

#include "heap.h"
#include "rk.h"

#if defined (HEAP_LOG_EN)
#define HEAP_LOG(fmt, ...)              OS_LOG((const char*)fmt, ##__VA_ARGS__)
#else
#define HEAP_LOG(fmt, ...)
#endif

#define HEAP_BLOCK_NULL                 ((heap_block_t*)0)
#define HEAP_ALIGNMENT_SIZE             (4)
#define HEAP_ALIGNMENT(size)            (size_t)(((size) + (HEAP_ALIGNMENT_SIZE - 1)) & ~(HEAP_ALIGNMENT_SIZE - 1))
#define HEAP_BLOCK_HEADER_SIZE          ((size_t)HEAP_ALIGNMENT(sizeof(heap_block_t)))
#define HEAP_MINIMUM_SPLIT_SIZE         (8)

/* heap memory pool */
static uint8_t heap_buffer_block_pool[HEAP_BUFFER_MAX_SIZE] __attribute__((aligned(HEAP_ALIGNMENT_SIZE)));
static heap_block_t* heap_list_head = HEAP_BLOCK_NULL;
static uint32_t heap_available_size = 0;
static uint32_t heap_free_minimum_size = HEAP_BUFFER_MAX_SIZE;

/**
 * @brief Heap memory initialization
 * @param[in] none
 * @return none
 * @note This function will be called by the kernel
 */
void heap_init() {
    uint32_t heap_total_size = HEAP_BUFFER_MAX_SIZE;
    uint32_t heap_start_addr = (uint32_t)heap_buffer_block_pool;

    HEAP_LOG("[HEAP] heap static buffer: 0x%08X\n", (uint32_t)heap_start_addr);

    if ((heap_start_addr & (HEAP_ALIGNMENT_SIZE - 1)) != 0) {
        heap_start_addr += (HEAP_ALIGNMENT_SIZE - 1);
        heap_start_addr &= ~(HEAP_ALIGNMENT_SIZE - 1);
        heap_total_size -= (heap_start_addr - (uint32_t)heap_buffer_block_pool);
    }

    heap_free_minimum_size = heap_total_size;

    HEAP_LOG("[HEAP] heap start buffer (aligned): 0x%08X\n", (uint32_t)heap_start_addr);
    HEAP_LOG("[HEAP] heap total size: %d bytes\n", (uint32_t)heap_total_size);
    HEAP_LOG("[HEAP] heap block header size: %d bytes\n", HEAP_BLOCK_HEADER_SIZE);

    heap_list_head = (heap_block_t*)heap_start_addr;
    heap_list_head->state = HEAP_BLOCK_STATE_FREE;
    heap_list_head->size = heap_total_size - HEAP_BLOCK_HEADER_SIZE;
    heap_list_head->next = HEAP_BLOCK_NULL;
    heap_list_head->prev = HEAP_BLOCK_NULL;

    heap_available_size = heap_total_size - HEAP_BLOCK_HEADER_SIZE;
}

/**
 * @brief Heap memory allocation
 * @param[in] size size of memory to allocate
 * @return
 *          - pointer to allocated memory (allocated successfully)
 *          - NULL if memory allocation fails
 */
void* heap_malloc(size_t size) {
    OS_ENTRY_CRITICAL();

    uint8_t* ret = NULL;

    /* heap size alignment */
    size = HEAP_ALIGNMENT(size);
    HEAP_LOG("[HEAP] heap_malloc(%d) bytes\n", size);

    if ((size == 0) || (size > heap_available_size)) {
        OS_FATAL("HEAP", 0x01);
        return ret;
    }

    heap_block_t* p_block = heap_list_head;

    while (p_block != HEAP_BLOCK_NULL) {
        if ((p_block->size >= size) && (p_block->state == HEAP_BLOCK_STATE_FREE)) {
            break;
        }

        p_block = p_block->next;
    }

    if (p_block != HEAP_BLOCK_NULL) {
        if ((p_block->size - size) > (HEAP_BLOCK_HEADER_SIZE + HEAP_MINIMUM_SPLIT_SIZE)) {

            size_t remaining = p_block->size - HEAP_BLOCK_HEADER_SIZE - size;

            if (remaining < HEAP_BLOCK_HEADER_SIZE) {
                EXIT_CRITICAL();
                OS_FATAL("HEAP", 0x03);
            }

            /* heap malloc (payload address) */
            ret = (uint8_t*)p_block + HEAP_BLOCK_HEADER_SIZE;

            /* heap split block */
            heap_block_t* heap_new_block = (heap_block_t*)(((uint8_t*)p_block) + HEAP_BLOCK_HEADER_SIZE + size);
            heap_new_block->state = HEAP_BLOCK_STATE_FREE;
            heap_new_block->size = remaining;
            heap_new_block->next = p_block->next;
            heap_new_block->prev = p_block;

            /* heap block allocated */
            p_block->size = size;
            p_block->state = HEAP_BLOCK_STATE_ALLOCATED;
            if (p_block->next != HEAP_BLOCK_NULL) {
                p_block->next->prev = heap_new_block;
            }
            p_block->next = heap_new_block;

            heap_available_size -= (HEAP_BLOCK_HEADER_SIZE + size);
        }
        else {
            ret = (uint8_t*)p_block + HEAP_BLOCK_HEADER_SIZE;

            /* heap block allocated */
            p_block->state = HEAP_BLOCK_STATE_ALLOCATED;

            heap_available_size -= (HEAP_BLOCK_HEADER_SIZE + p_block->size);
        }

        if (heap_available_size < heap_free_minimum_size) {
            heap_free_minimum_size = heap_available_size;
        }
    }
    else {
        OS_FATAL("HEAP", 0x02);
    }

    OS_EXIT_CRITICAL();

    if (ret == NULL) {
        OS_FATAL("HEAP", 0x04);
    }

    return (void*)ret;
}

/**
 * @brief Heap memory free
 * @param[in] addr pointer to memory to free (MUST-BE not NULL)
 * @return none
 */
void heap_free(void* addr) {
    if (addr == NULL) {
        OS_FATAL("HEAP", 0x05);
    }

    OS_ENTRY_CRITICAL();

    heap_block_t* p_block = (heap_block_t*)((uint8_t*)addr - HEAP_BLOCK_HEADER_SIZE);
    p_block->state = HEAP_BLOCK_STATE_FREE;
    heap_available_size += (HEAP_BLOCK_HEADER_SIZE + p_block->size);

    /* heap merge (prev block) */
    if ((p_block->prev != HEAP_BLOCK_NULL) && (p_block->prev->state == HEAP_BLOCK_STATE_FREE)) {
        p_block->prev->size += (HEAP_BLOCK_HEADER_SIZE + p_block->size);
        p_block->prev->next = p_block->next;

        if (p_block->next != HEAP_BLOCK_NULL) {
            p_block->next->prev = p_block->prev;
        }

        p_block = p_block->prev;
    }

    /* heap merge (next block) */
    if ((p_block->next != HEAP_BLOCK_NULL) && (p_block->next->state == HEAP_BLOCK_STATE_FREE)) {
        p_block->size += (HEAP_BLOCK_HEADER_SIZE + p_block->next->size);
        p_block->next = p_block->next->next;

        if (p_block->next != HEAP_BLOCK_NULL) {
            p_block->next->prev = p_block;
        }
    }

    OS_EXIT_CRITICAL();
}

/**
 * @brief Heap usage information
 * 
 * This function prints the current status of the heap, including total size, used size, free size, and fragmentation level.
 *
 * @param[in] none
 * @return none
 */
void heap_info() {
    heap_block_t* p_block = heap_list_head;
    OS_LOG("\n");
    OS_LOG("heap informations:\n");
    OS_LOG("heap available size: %ld bytes\n", heap_available_size);
    OS_LOG("heap minimum free: %ld bytes\n", heap_free_minimum_size);
    uint32_t heap_usage_percentage = (uint32_t)(((HEAP_BUFFER_MAX_SIZE - heap_available_size) * 30) / HEAP_BUFFER_MAX_SIZE);
    OS_LOG("heap usage: [");

    for (uint32_t i = 0; i < 30; i++) {
        if (i < heap_usage_percentage) {
            OS_LOG("#");
        }
        else {
            OS_LOG("-");
        }
    }

    OS_LOG("] %ld / %ld (bytes)\n", (HEAP_BUFFER_MAX_SIZE - heap_available_size),HEAP_BUFFER_MAX_SIZE);
    OS_LOG("heap block:\n");

    uint32_t block_index = 0;

    while (p_block != HEAP_BLOCK_NULL) {
        OS_LOG("[block]: %d \t\t[block size]: %ld \t\t[block state]: %s\n", block_index, p_block->size, (p_block->state == HEAP_BLOCK_STATE_ALLOCATED) ? "ALLOCATED" : "FREE");
        block_index++;
        p_block = p_block->next;
    }

    OS_LOG("\n");
}
