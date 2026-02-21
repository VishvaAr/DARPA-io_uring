#include <klee/klee.h>
#include <linux/fs.h>
#include <linux/io_uring.h>
// In STASE, we include the actual kernel headers directly

/* * TARGET: Verified Detection of WMI-1 through WMI-4 in io_uring
 * This harness is designed to run within the STASE environment
 * linked against the full demo3_linux-main source.
 */

int main() {
    // --- STEP 1: Setup io_uring Context ---
    // In STASE, we can call the actual kernel init functions
    struct io_ring_ctx *ctx = io_ring_ctx_alloc(NULL);

    // --- STEP 2: Symbolic Indices (The "Attacker's Choice") ---
    // We let KLEE choose which cache slot to use to find the race
    unsigned int index;
    klee_make_symbolic(&index, sizeof(index), "cache_index");
    klee_assume(index < 8);

    // --- STEP 3: Trigger WMI-1 & WMI-2 (Resource Node Cache) ---
    // We simulate the flow found in rsrc.c:170-215
    struct io_rsrc_node *node = io_rsrc_node_alloc(ctx);
    io_rsrc_node_destroy(node); // Put in cache without clearing

    struct io_rsrc_node *reused_node = io_rsrc_node_alloc(ctx);

    // Verification: Check if the 'item' union survived the recycling
    klee_assert(reused_node->item.file == NULL);

    // --- STEP 4: Trigger WMI-3 (Request Recycling) ---
    // Targeting io_uring.c:1129-1224
    struct io_kiocb *req = io_alloc_req(ctx);
    // Simulate a completed request going to the free_list
    __io_req_complete_post(req);

    struct io_kiocb *reused_req = io_alloc_req(ctx);
    // Verification: Ensure no stale data remains in the union
    klee_assert(reused_req->ptr == NULL);

    // --- STEP 5: Trigger WMI-1 Race (Lock-Drop Quiesce) ---
    // Targeting rsrc.c:217-274
    // We simulate the window where uring_lock is dropped
    mutex_unlock(&ctx->uring_lock);

    // If KLEE can find a path where a background thread modifies
    // the node here, it triggers the Use-After-Free
    io_rsrc_ref_quiesce(ctx, NULL);

    mutex_lock(&ctx->uring_lock);

    return 0;
}
