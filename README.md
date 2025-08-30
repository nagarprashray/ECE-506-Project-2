# ECE-506-Project-2
Cache Coherence Simulator – MESI & MOESI

This project implements a cache coherence simulator for a 4-processor shared-memory system, designed to model and compare the behavior of the MESI and MOESI protocols. The goal of the project is to evaluate how coherence mechanisms reduce memory transactions and improve program execution efficiency in parallel architectures.

Features

Models a 4-processor system, each with a private L1 cache.
Supports MESI (Modified, Exclusive, Shared, Invalid) and MOESI (Modified, Owned, Exclusive, Shared, Invalid) coherence protocols.
Simulates cache-to-cache transfers, invalidations, and flushes.

Outputs detailed statistics, including:

Number of reads and writes
Read hits and misses
Write hits and misses
Total miss rate
Memory accesses (excluding writebacks)
Invalidations and flushes
Total program execution time

Learning Outcomes

Understood the role of cache coherence protocols in parallel computer systems.
Explored how MESI reduces unnecessary memory transactions using Exclusive and Shared states.
Analyzed how MOESI extends MESI with the Owned state to optimize bus utilization.
Gained experience in trace-driven simulation and system-level performance evaluation.
