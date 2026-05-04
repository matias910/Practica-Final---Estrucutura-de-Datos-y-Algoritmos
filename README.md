# Práctica II — DialSort vs TieredSort
### ST0245 - SI001 · Data Structures and Algorithms · EAFIT University
**Lecturer:** Alexander Narváez Berrío

---

## Team Members

| Name |
|------|
| Matias Zapata Rojas | 
| Samuel Valencia Montoya |

---

## Description

This project implements and experimentally compares **DialSort** — a non-comparative integer sorting algorithm based on the self-indexing principle — against **TieredSort**, a two-level alternative proposed by the team.

### DialSort
DialSort builds a flat histogram `H[0..U-1]` in a single ingestion pass over the input, then reconstructs the sorted output by scanning the histogram from index 0 to U. It requires no comparisons and no prefix-sum.

- **Time:** O(n + U) · **Space:** O(U) · **Non-comparative** · **2 passes**

### TieredSort (Student Proposal)
TieredSort is a two-level non-comparative integer sort. It partitions the universe `[mn, mx]` into **K = ⌈√U⌉** coarse tiers of equal width, then within each tier applies a local histogram of size `tier_width = ⌈U/K⌉`.

**Analogy:** sorting mail by city first, then by street within each city.

- **Pass 1 — Scatter:** each key is placed into its coarse tier via integer division. O(n).
- **Pass 2 — Fine histogram + emit:** local histogram built per tier, values emitted in order. O(n + U) total.
- **Time:** O(n + U) · **Space:** O(n + √U) · **Non-comparative** · **3 passes**
- **Key advantage:** active working set per pass is O(√U) instead of O(U), improving cache locality for large U.

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
├── datasets/
│   └── dataset_n<N>_U<U>_<dist>.csv   (24 files, auto-generated)
└── results/
    └── results.txt
```

---

## How to Compile and Run

### Requirements
- GCC 11+ with C++17 support
- CMake 3.27+
- pthreads (Linux/macOS)

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

### Run benchmark and generate datasets
```bash
mkdir -p datasets
./build/bench_tiered > results/results.txt
```

The benchmark automatically exports 24 CSV dataset files into `datasets/` during the run.

---

## Simulator

Open `simulators/simulator.html` in any modern browser. No installation required.

Shows **DialSort** and **TieredSort** side by side, stepping through each internal phase:
- Input key injection / coarse scatter
- Flat histogram build / fine histogram per tier
- Scan wavefront / tier-by-tier emit
- Sorted output

Use **▶ Step** to advance manually or **▶▶ Auto** to animate both in sync.

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
| Total configurations | 48 |

Algorithms compared: `std::sort` · `DialSort` · `DialSort-Parallel` · `TieredSort`

Memory consumption measured via `/proc/self/status` (VmRSS delta in KB) on Linux.

---

## Key Results

| Metric | Value |
|--------|-------|
| Total configurations | 48 |
| DialSort faster than TieredSort | 46 / 48 |
| TieredSort faster than DialSort | 2 / 48 |
| Avg ratio DialSort / TieredSort | 0.367x |
| Max ratio (TieredSort wins) | 2.126x |
| Correctness checks | PASSED 48/48 |

**Main findings:**
- DialSort dominates for small/medium U where its flat histogram fits in L1/L2 cache. At n=10M, U=1024 it reaches **47x speedup** over std::sort.
- TieredSort wins in 2 configurations at large U (65,536) confirming the two-level cache hypothesis.
- DialSort-Parallel pays off at n ≥ 1,000,000; at small n thread overhead dominates.

---

## Algorithmic Complexity

| Algorithm | Time | Space | Comparative? |
|-----------|------|-------|--------------|
| std::sort | O(n log n) | O(log n) | Yes |
| DialSort | O(n + U) | O(U) | No |
| DialSort-Parallel | O(n/p + U) | O(p·U) | No |
| TieredSort | O(n + U) | O(n + √U) | No |

---

## Datasets

The benchmark auto-generates 24 CSV files in `datasets/` covering:
- **Sizes:** n = 100,000 and n = 1,000,000
- **Universe sizes:** U = 256, 1,024, 65,536
- **Distributions:** uniform, skewed, sorted, reverse

Each file has `index,value` columns with the exact input used in the benchmark (same seed).

---

## License

Academic use only — ST0245, EAFIT University, 2026.
