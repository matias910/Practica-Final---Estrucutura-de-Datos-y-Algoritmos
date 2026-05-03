# Práctica II — DialSort vs TieredSort
### ST0245 - SI001 · Data Structures and Algorithms · EAFIT University
**Lecturer:** Alexander Narváez Berrío

---

## Team Members

| Name |
|------|
| Matias Zapata Rojas 
| Samuel Valencia Montoya

---

## Description

This project implements and experimentally compares **DialSort** — a non-comparative integer sorting algorithm based on the self-indexing principle — against **TieredSort**, a two-level alternative proposed by the team.

### DialSort
DialSort builds a flat histogram `H[0..U-1]` in a single ingestion pass over the input, then reconstructs the sorted output by scanning the histogram from index 0 to U. It requires no comparisons and no prefix-sum. Time complexity: **O(n + U)**. Space: **O(U)**.

### TieredSort (Student Proposal)
TieredSort is a two-level non-comparative integer sort. It partitions the universe `[mn, mx]` into **K = ⌈√U⌉** coarse tiers of equal width, then within each tier applies a local histogram of size `tier_width = ⌈U/K⌉`.

**Analogy:** sorting mail by city first, then by street within each city.

- **Pass 1 — Scatter:** each key is placed into its coarse tier bucket via integer division. O(n).
- **Pass 2 — Fine histogram + emit:** for each tier, a local histogram is built and values are emitted in order. O(n + U) total.
- **Time:** O(n + U) — same asymptotic class as DialSort.
- **Space:** O(n + K + tier_width) — active working set per pass is O(√U) instead of O(U), improving cache locality for large U.

---

## Repository Structure

```
.
├── CMakeLists.txt
├── README.md
├── src/
│   └── DialsortVsTieredSort.cpp
├── simulators/
│   └── simulator.html
└── results/
    └── results.txt
```

---

## How to Compile and Run

### Requirements
- GCC 11+ with C++17 support
- CMake 3.27+
- pthreads (included in Linux/macOS)

### Fedora / RHEL
```bash
sudo dnf install gcc-c++ cmake
```

### Ubuntu / Debian
```bash
sudo apt install build-essential cmake
```

### Build
```bash
cmake -B build
cmake --build build
```

### Run benchmark
```bash
./build/bench_tiered
```

### Save results to file
```bash
./build/bench_tiered > results/results.txt
```

---

## Simulator

Open `simulators/simulator.html` in any modern browser. No installation required.

The simulator shows **DialSort** and **TieredSort** side by side, stepping through each internal phase:
- Input key injection
- Histogram / tier scatter
- Scan wavefront / fine histogram
- Sorted output

Use **▶ Step** to advance manually or **▶▶ Auto** to animate both algorithms in sync.

---

## Benchmark Design

| Parameter | Values |
|-----------|--------|
| Input size (n) | 10,000 · 100,000 · 1,000,000 · 10,000,000 |
| Universe size (U) | 256 · 1,024 · 65,536 |
| Distributions | uniform · skewed · sorted · reverse |
| Warmup rounds | 3 (discarded) |
| Measurement | best of 7 runs |
| Seed | 20260321 (fixed) |

Algorithms compared per configuration:
1. `std::sort` — GCC introsort baseline
2. `DialSort` — sequential
3. `DialSort-Parallel` — 8-thread parallel ingestion
4. `TieredSort` — student proposal

---

## Key Results

| Metric | Value |
|--------|-------|
| Total configurations | 48 |
| DialSort faster than TieredSort | 45 / 48 |
| TieredSort faster than DialSort | 3 / 48 |
| Avg ratio DialSort / TieredSort | 0.613x |

**Main finding:** DialSort dominates for small and medium U where its flat histogram fits in L1/L2 cache. TieredSort's cache advantage becomes relevant at large U (65,536) with skewed or non-uniform distributions, where it outperforms sequential DialSort in 3 configurations.

---

## Algorithmic Complexity

| Algorithm | Time | Space | Comparative? |
|-----------|------|-------|--------------|
| std::sort | O(n log n) | O(log n) | Yes |
| DialSort | O(n + U) | O(U) | No |
| DialSort-Parallel | O(n/p + U) | O(p·U) | No |
| TieredSort | O(n + U) | O(n + √U) | No |

---

## License

Academic use only — ST0245, EAFIT University, 2026.
