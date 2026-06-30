/**
 ******************************************************************************
 * @file:   heap.h
 * @author: GaoDen
 * @date:   10/01/2026
 * @brief:  heap memory management for dynamic memory allocation
 ******************************************************************************
**/

#ifndef __HEAP_H__
#define __HEAP_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Heap block status
 */
typedef enum {
    HEAP_BLOCK_STATE_FREE = 0x00,
    HEAP_BLOCK_STATE_ALLOCATED,
} heap_block_state_t;

/**
 * @brief Heap control block structure
 */
typedef struct heap_block_t {
    struct heap_block_t* next;      /* heap block next pointer */
    struct heap_block_t* prev;      /* heap block prev pointer */
    heap_block_state_t state;       /* heap block state (free, allocated) */
    size_t size;                    /* heap block size (payload size) */
} heap_block_t;

/**
 * @brief Heap memory initialization
 * @param[in] none
 * @return none
 * @note This function will be called by the kernel
 */
extern void heap_init();

/**
 * @brief Heap memory allocation
 * @param[in] size size of memory to allocate
 * @return
 *          - pointer to allocated memory (allocated successfully)
 *          - NULL if memory allocation fails
 */
extern void* heap_malloc(size_t size);

/**
 * @brief Heap memory free
 * @param[in] addr pointer to memory to free (MUST-BE not NULL)
 * @return none
 */
extern void heap_free(void* addr);

/**
 * @brief Heap usage information
 * 
 * This function prints the current status of the heap, including total size, used size, free size, and fragmentation level.
 *
 * @param[in] none
 * @return none
 */
extern void heap_info();

#ifdef __cplusplus
}
#endif

#endif /* __HEAP_H__ */
