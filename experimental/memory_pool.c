#include "arc_runtime.h"
#include "memory.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

// T2.3.5: Memory Pool Optimization
// Size-class based allocation pools for reduced fragmentation and fast paths

// Size classes for memory pools (powers of 2 + some common sizes)
static const size_t SIZE_CLASSES[] = {
    8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048
};
static const size_t NUM_SIZE_CLASSES = sizeof(SIZE_CLASSES) / sizeof(SIZE_CLASSES[0]);
static const size_t MAX_POOLED_SIZE = 2048;

// Memory block for pool allocation
typedef struct PoolBlock {
    struct PoolBlock* next;
    uint8_t data[];
} PoolBlock;

// Memory pool for a specific size class
typedef struct {
    size_t block_size;
    PoolBlock* free_list;
    size_t total_blocks;
    size_t free_blocks;
    size_t allocations;
    size_t deallocations;
    pthread_mutex_t lock;
} MemoryPool;

// Pool manager
typedef struct {
    MemoryPool pools[NUM_SIZE_CLASSES];
    size_t large_allocations;
    size_t large_deallocations;
    size_t total_memory_used;
    size_t peak_memory_used;
    pthread_mutex_t stats_lock;
    bool initialized;
} PoolManager;

static PoolManager g_pool_manager = {0};

// Find appropriate size class for allocation
static int find_size_class(size_t size) {
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        if (size <= SIZE_CLASSES[i]) {
            return (int)i;
        }
    }
    return -1; // Too large for pooling
}

// Initialize memory pools
void wyn_pool_init(void) {
    if (g_pool_manager.initialized) return;
    
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        MemoryPool* pool = &g_pool_manager.pools[i];
        pool->block_size = SIZE_CLASSES[i];
        pool->free_list = NULL;
        pool->total_blocks = 0;
        pool->free_blocks = 0;
        pool->allocations = 0;
        pool->deallocations = 0;
        pthread_mutex_init(&pool->lock, NULL);
    }
    
    g_pool_manager.large_allocations = 0;
    g_pool_manager.large_deallocations = 0;
    g_pool_manager.total_memory_used = 0;
    g_pool_manager.peak_memory_used = 0;
    pthread_mutex_init(&g_pool_manager.stats_lock, NULL);
    g_pool_manager.initialized = true;
}

// Allocate a block from pool
static PoolBlock* allocate_pool_block(MemoryPool* pool) {
    size_t total_size = sizeof(PoolBlock) + pool->block_size;
    PoolBlock* block = malloc(total_size);
    if (!block) return NULL;
    
    block->next = NULL;
    pool->total_blocks++;
    
    // Update global statistics
    pthread_mutex_lock(&g_pool_manager.stats_lock);
    g_pool_manager.total_memory_used += total_size;
    if (g_pool_manager.total_memory_used > g_pool_manager.peak_memory_used) {
        g_pool_manager.peak_memory_used = g_pool_manager.total_memory_used;
    }
    pthread_mutex_unlock(&g_pool_manager.stats_lock);
    
    return block;
}

// Fast pooled allocation
void* wyn_pool_alloc(size_t size) {
    wyn_pool_init();
    
    int class_index = find_size_class(size);
    if (class_index == -1) {
        // Large allocation - use regular malloc
        pthread_mutex_lock(&g_pool_manager.stats_lock);
        g_pool_manager.large_allocations++;
        g_pool_manager.total_memory_used += size;
        if (g_pool_manager.total_memory_used > g_pool_manager.peak_memory_used) {
            g_pool_manager.peak_memory_used = g_pool_manager.total_memory_used;
        }
        pthread_mutex_unlock(&g_pool_manager.stats_lock);
        
        return malloc(size);
    }
    
    MemoryPool* pool = &g_pool_manager.pools[class_index];
    pthread_mutex_lock(&pool->lock);
    
    PoolBlock* block;
    if (pool->free_list) {
        // Fast path: reuse existing block
        block = pool->free_list;
        pool->free_list = block->next;
        pool->free_blocks--;
    } else {
        // Slow path: allocate new block
        block = allocate_pool_block(pool);
        if (!block) {
            pthread_mutex_unlock(&pool->lock);
            return NULL;
        }
    }
    
    pool->allocations++;
    pthread_mutex_unlock(&pool->lock);
    
    return block->data;
}

// Fast pooled deallocation
void wyn_pool_free(void* ptr, size_t size) {
    if (!ptr) return;
    
    int class_index = find_size_class(size);
    if (class_index == -1) {
        // Large allocation - use regular free
        pthread_mutex_lock(&g_pool_manager.stats_lock);
        g_pool_manager.large_deallocations++;
        g_pool_manager.total_memory_used -= size;
        pthread_mutex_unlock(&g_pool_manager.stats_lock);
        
        free(ptr);
        return;
    }
    
    // Calculate block address from data pointer
    PoolBlock* block = (PoolBlock*)((uint8_t*)ptr - sizeof(PoolBlock));
    
    MemoryPool* pool = &g_pool_manager.pools[class_index];
    pthread_mutex_lock(&pool->lock);
    
    // Add to free list
    block->next = pool->free_list;
    pool->free_list = block;
    pool->free_blocks++;
    pool->deallocations++;
    
    pthread_mutex_unlock(&pool->lock);
}

// Optimized ARC allocation using pools
WynObject* wyn_arc_alloc_pooled(size_t size, uint32_t type_id, void (*destructor)(void*)) {
    if (size == 0) {
        report_error(ERR_INVALID_EXPRESSION, __FILE__, __LINE__, 0, "Cannot allocate zero-sized object");
        return NULL;
    }
    
    // Calculate total size including header
    size_t total_size = sizeof(WynObjectHeader) + size;
    
    // Use pooled allocation
    WynObject* obj = (WynObject*)wyn_pool_alloc(total_size);
    if (!obj) {
        report_error(ERR_OUT_OF_MEMORY, __FILE__, __LINE__, 0, "Failed to allocate ARC object from pool");
        return NULL;
    }
    
    // Initialize header
    obj->header.magic = ARC_MAGIC;
    atomic_store(&obj->header.ref_count, 1);
    obj->header.type_id = type_id;
    obj->header.size = (uint32_t)size;
    obj->header.destructor = destructor;
    
    // Zero-initialize data area
    memset(obj->data, 0, size);
    
    return obj;
}

// Optimized ARC deallocation using pools
void wyn_arc_deallocate_pooled(WynObject* obj) {
    if (!obj) return;
    
    // Call custom destructor if provided
    if (obj->header.destructor) {
        obj->header.destructor(obj->data);
    }
    
    // Calculate total size for pool deallocation
    size_t total_size = sizeof(WynObjectHeader) + obj->header.size;
    
    // Clear magic number for safety
    obj->header.magic = 0;
    
    // Use pooled deallocation
    wyn_pool_free(obj, total_size);
}

// Pool statistics
// (Defined in arc_runtime.h)
WynPoolStats wyn_pool_get_stats(void) {
    wyn_pool_init();
    
    WynPoolStats stats = {0};
    stats.total_pools = NUM_SIZE_CLASSES;
    
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        MemoryPool* pool = &g_pool_manager.pools[i];
        pthread_mutex_lock(&pool->lock);
        
        stats.total_blocks += pool->total_blocks;
        stats.free_blocks += pool->free_blocks;
        stats.total_allocations += pool->allocations;
        stats.total_deallocations += pool->deallocations;
        
        pthread_mutex_unlock(&pool->lock);
    }
    
    pthread_mutex_lock(&g_pool_manager.stats_lock);
    stats.large_allocations = g_pool_manager.large_allocations;
    stats.large_deallocations = g_pool_manager.large_deallocations;
    stats.total_memory_used = g_pool_manager.total_memory_used;
    stats.peak_memory_used = g_pool_manager.peak_memory_used;
    pthread_mutex_unlock(&g_pool_manager.stats_lock);
    
    // Calculate derived statistics
    if (stats.total_blocks > 0) {
        stats.fragmentation_ratio = (double)stats.free_blocks / stats.total_blocks;
        stats.pool_utilization = 1.0 - stats.fragmentation_ratio;
    }
    
    return stats;
}

void wyn_pool_reset_stats(void) {
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        MemoryPool* pool = &g_pool_manager.pools[i];
        pthread_mutex_lock(&pool->lock);
        pool->allocations = 0;
        pool->deallocations = 0;
        pthread_mutex_unlock(&pool->lock);
    }
    
    pthread_mutex_lock(&g_pool_manager.stats_lock);
    g_pool_manager.large_allocations = 0;
    g_pool_manager.large_deallocations = 0;
    pthread_mutex_unlock(&g_pool_manager.stats_lock);
}

void wyn_pool_print_stats(void) {
    WynPoolStats stats = wyn_pool_get_stats();
    
    printf("=== Memory Pool Statistics ===\n");
    printf("Total pools: %zu\n", stats.total_pools);
    printf("Total blocks: %zu\n", stats.total_blocks);
    printf("Free blocks: %zu\n", stats.free_blocks);
    printf("Pool allocations: %zu\n", stats.total_allocations);
    printf("Pool deallocations: %zu\n", stats.total_deallocations);
    printf("Large allocations: %zu\n", stats.large_allocations);
    printf("Large deallocations: %zu\n", stats.large_deallocations);
    printf("Total memory used: %zu bytes\n", stats.total_memory_used);
    printf("Peak memory used: %zu bytes\n", stats.peak_memory_used);
    printf("Fragmentation ratio: %.2f%%\n", stats.fragmentation_ratio * 100);
    printf("Pool utilization: %.2f%%\n", stats.pool_utilization * 100);
    printf("==============================\n");
}

// Detailed pool information
void wyn_pool_print_detailed_stats(void) {
    wyn_pool_init();
    
    printf("=== Detailed Pool Statistics ===\n");
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        MemoryPool* pool = &g_pool_manager.pools[i];
        pthread_mutex_lock(&pool->lock);
        
        printf("Pool %zu (size %zu): %zu total, %zu free, %zu allocs, %zu deallocs\n",
               i, pool->block_size, pool->total_blocks, pool->free_blocks,
               pool->allocations, pool->deallocations);
        
        pthread_mutex_unlock(&pool->lock);
    }
    printf("================================\n");
}

// Pool cleanup
void wyn_pool_cleanup(void) {
    if (!g_pool_manager.initialized) return;
    
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        MemoryPool* pool = &g_pool_manager.pools[i];
        pthread_mutex_lock(&pool->lock);
        
        // Free all blocks in the pool
        PoolBlock* current = pool->free_list;
        while (current) {
            PoolBlock* next = current->next;
            free(current);
            current = next;
        }
        
        pool->free_list = NULL;
        pool->total_blocks = 0;
        pool->free_blocks = 0;
        
        pthread_mutex_unlock(&pool->lock);
        pthread_mutex_destroy(&pool->lock);
    }
    
    pthread_mutex_destroy(&g_pool_manager.stats_lock);
    g_pool_manager.initialized = false;
}

// Batch allocation for improved performance
void wyn_pool_alloc_batch(void** ptrs, size_t* sizes, size_t count) {
    if (!ptrs || !sizes || count == 0) return;
    
    for (size_t i = 0; i < count; i++) {
        ptrs[i] = wyn_pool_alloc(sizes[i]);
    }
}

void wyn_pool_free_batch(void** ptrs, size_t* sizes, size_t count) {
    if (!ptrs || !sizes || count == 0) return;
    
    for (size_t i = 0; i < count; i++) {
        if (ptrs[i]) {
            wyn_pool_free(ptrs[i], sizes[i]);
            ptrs[i] = NULL;
        }
    }
}
