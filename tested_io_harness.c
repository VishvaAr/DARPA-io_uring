#include <klee/klee.h>
#include <stdlib.h>
#include <stdio.h>

/* ==========================================================
 * 1. THE REAL DARPA STRUCT (From demo3_linux-main)
 * ========================================================== */
struct io_rsrc_node {
    int refs;
    int empty;
    int type;
    union {
        void *file;
        void *buf;
        void *rsrc;
    } item;
};

/* ==========================================================
 * 2. THE EXPLOIT TARGET
 * ========================================================== */
struct fake_file {
    void (*f_op_release)(void);
};

void safe_release() {
    printf("[SAFE] Normal cleanup executed.\n");
}

void hijacked_release() {
    printf("[WMI-4 SUCCESS] Attacker code executed!\n");
}

/* ==========================================================
 * 3. THE EXTRACTED DARPA CACHE (The actual vulnerable code)
 * ========================================================== */
#define CACHE_MAX 8
struct io_rsrc_node *fake_cache[CACHE_MAX];
int cache_count = 0;

void extracted_io_rsrc_node_destroy(struct io_rsrc_node *node) {
    if (cache_count < CACHE_MAX) {
        fake_cache[cache_count++] = node;
    } else {
        free(node);
    }
}

struct io_rsrc_node *extracted_io_rsrc_node_alloc(void) {
    struct io_rsrc_node *node;
    if (cache_count > 0) {
        // THE DARPA BUG: Pulls from cache, fails to wipe item.file
        node = fake_cache[--cache_count];
    } else {
        node = calloc(1, sizeof(*node));
    }
    node->refs = 1;
    node->empty = 0;
    return node;
}

/* ==========================================================
 * 4. THE WMI 1-4 EXPLOIT CHAIN
 * ========================================================== */
int main() {
    printf("--- Phase 1: Normal Allocation ---\n");
    struct io_rsrc_node *node1 = extracted_io_rsrc_node_alloc();
    struct fake_file *good_file = malloc(sizeof(struct fake_file));
    good_file->f_op_release = safe_release;

    node1->item.file = good_file;
    node1->type = 0;

    printf("--- Phase 2: Teardown (Trigger WMI-1) ---\n");
    extracted_io_rsrc_node_destroy(node1);

    printf("--- Phase 3: Memory Reclaim (Trigger WMI-2) ---\n");
    free(good_file);
    struct fake_file *evil_file = malloc(sizeof(struct fake_file));
    evil_file->f_op_release = hijacked_release;

    printf("--- Phase 4: Cache Reuse & Execution (Trigger WMI-4) ---\n");
    struct io_rsrc_node *node2 = extracted_io_rsrc_node_alloc();

    if (node2->item.file != NULL) {
        struct fake_file *stale_ptr = (struct fake_file *)node2->item.file;

        if (stale_ptr->f_op_release == hijacked_release) {
            klee_warning("[WMI-4 DETECTED] DARPA Cache bug led to Function Pointer Hijack!");
            stale_ptr->f_op_release(); // Call the hijacked function
        }
    }

    // Force KLEE to flag the error for the final report
    klee_assert(((struct fake_file *)node2->item.file)->f_op_release != hijacked_release);

    return 0;
}
