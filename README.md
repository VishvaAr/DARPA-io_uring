# DARPA-io_uring


## Overview

This repository contains the methodology and symbolic execution harnesses used to verify a chain of Weakness-Method-Impact (WMI) vulnerabilities in the io_uring module of the demo3_linux-main kernel.


## Methodology: grep

Initial vulnerability discovery was performed via manual static analysis and source code auditing, avoiding reliance on external AI models.

Mapping the Attack Surface: I utilized grep -rn "free", across the io_uring directory to identify how the kernel handles object teardowns.

Identifying the Anomaly: The audit revealed that io_uring/rsrc.c handles high-traffic resource nodes differently than standard objects. Instead of being destroyed, they are recycled via a custom cache (rsrc_node_cache).

By comparing the deallocation logic (io_rsrc_node_destroy) with the reallocation logic (io_rsrc_node_alloc), I confirmed that while the node's reference counts were reset, the item union containing critical file pointers was never cleared. This creates the foundational Stale Reference (WMI-1) flaw.


## Two Harnesses

File: tested_io_harness.c

Status: Successfully Ran & Verified

Standard symbolic execution engines (like KLEE) cannot easily compile full Linux subsystems on a standard laptop due to missing headers and complex dependencies. To bypass this, I used Function Extraction. I extracted the exact vulnerable allocator logic from rsrc.c and ran it in an isolated standard KLEE environment.

What it proves: This harness mathematically verifies the most critical exploit chain. It proves that the uncleared cache leads directly to a Use-After-Free (WMI-2), which an attacker can leverage to overwrite the f_op_release pointer, resulting in a Function Hijack (WMI-4).

---------------------------------------------------------------------------------------------------------------

File: stase_io_uring_complete.c

Status: Proposed for STASE

To verify the remaining vulnerabilities (such as the io_kiocb request recycling in io_uring.c and the lock-drop race conditions), I asked an LLM to give me a complete harness. 

Why this requires STASE:

This full-module harness cannot be run in standard KLEE on a local machine for three reasons:

Missing Environment: Standard KLEE lacks the specific Linux kernel headers (<linux/io_uring.h>) required to compile the full subsystem.

Concurrency Constraints: Standard KLEE executes sequentially. To verify the lock-drop race condition, the engine needs specialized models for kernel spinlocks and mutexes, which STASE provides.


## Output:

Compile: clang-13 -I ~/klee/include -emit-llvm -c -g -fno-discard-value-names darpa_io_uring_harness.c -o darpa_io.bc

Run: klee darpa_io.bc

