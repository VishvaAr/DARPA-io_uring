# DARPA-io_uring


## Overview

This repository contains the methodology and symbolic execution harnesses used to verify a chain of Weakness-Method-Impact (WMI) vulnerabilities in the io_uring module of the demo3_linux-main kernel.


## Methodology: grep

Initial vulnerability discovery was performed via manual static analysis and source code auditing, avoiding reliance on external AI models.

Mapping the Attack Surface: I utilized grep -rn "free", across the io_uring directory to identify how the kernel handles object teardowns.

Identifying the Anomaly: The audit revealed that io_uring/rsrc.c handles high-traffic resource nodes differently than standard objects. Instead of being destroyed, they are recycled via a custom cache (rsrc_node_cache).

By comparing the deallocation logic (io_rsrc_node_destroy) with the reallocation logic (io_rsrc_node_alloc), I confirmed that while the node's reference counts were reset, the item union containing critical file pointers was never cleared. This creates the foundational Stale Reference (WMI-1) flaw.



