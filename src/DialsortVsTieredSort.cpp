/**
 * DialSort vs. TieredSort — Direct Comparison Benchmark
 * ======================================================
 * Course  : ST0245 - SI001 — Data Structures and Algorithms
 * University: Universidad EAFIT, , Antioquia, Colombia
 * Lecturer: Alexander Narváez Berrío
 *
 * ALTERNATIVE PROPOSAL: TieredSort
 * ---------------------------------
 * TieredSort is a two-level non-comparative integer sorting algorithm.
 * Instead of maintaining a single flat histogram of size U (like DialSort),
 * it partitions the universe [mn, mx] into K coarse "tiers" of equal width,
 * then within each tier maintains a fine-grained local histogram.
 *
 * Analogy: sorting letters by country first, then by city within each country.
 *
 * ALGORITHM OUTLINE
 * -----------------
 *   Pass 1 — Tier classification:
 *     For each key k, compute tier index  t = (k - mn) / tier_width.
 *     Increment the coarse count array C[t]++.
 *     Also store (k - mn) into a per-tier bucket (vector of offsets).
 *
 *   Pass 2 — Intra-tier sort (local histogram):
 *     For each non-empty tier t, build a local histogram H_t of size tier_width,
 *     count each offset, then project back in order.
 *
 *   Pass 3 — Projection:
 *     Walk tiers 0..K-1 in order, emit sorted values from each tier.
 *
 * COMPLEXITY
 * ----------
 *   Time : O(n + U)  — same asymptotic class as DialSort-Counting
 *   Space: O(n + K + max_tier_width)  — avoids the full-U flat array
 *          when K << U, which improves cache locality for large U.
 *
 * KEY DIFFERENCE vs DialSort
 * --------------------------
 *   DialSort uses a single histogram H[0..U-1] — great for small U, but
 *   for large U the array exceeds L2/L3 cache and random writes become slow.
 *   TieredSort's two-level structure keeps the working set smaller per pass,
 *   trading one extra scan for better cache behavior on large U.
 *
 * COMPILE (no external dependencies)
 * -------
 *   g++ -O3 -std=c++17 -pthread -o bench_tiered DialsortVsTieredSort.cpp
 *
 * To skip TieredSort (for testing harness only):
 *   g++ -O3 -std=c++17 -pthread -DSKIP_TIERED -o bench_tiered DialsortVsTieredSort.cpp
 *
 * REPRODUCIBILITY
 * ---------------
 *   Seed    : fixed (20260321)
 *   Timing  : best-of-7 runs, 3 warmup discarded
 *   Correctness: check_sorted() after every run
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <cmath>

// ── Parameters ────────────────────────────────────────────────────────────────
static constexpr int      WARMUP_ROUNDS  = 3;
static constexpr int      MEASURE_ROUNDS = 7;
static constexpr long     SEED           = 20260321L;
static constexpr uint64_t MAX_U          = 10'000'000ULL;
static constexpr int      NUM_THREADS    = 8;

// Number of coarse tiers for TieredSort.
// Chosen as sqrt(U) at runtime, but capped here for safety.
// A larger K means smaller per-tier histograms (better cache),
// but more overhead in the tier-dispatch loop.
static constexpr size_t   TIERED_K_MAX   = 4096;

// ── Timer ─────────────────────────────────────────────────────────────────────
static inline int64_t now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
            high_resolution_clock::now().time_since_epoch()).count();
}

// ── Correctness ───────────────────────────────────────────────────────────────
static bool check_sorted(const std::vector<int>& a) {
    for (size_t i = 1; i < a.size(); ++i)
        if (a[i-1] > a[i]) return false;
    return true;
}

// ── Universe-size guard ───────────────────────────────────────────────────────
static std::pair<bool, uint64_t> universe_size(int mn, int mx) {
    const uint64_t U = static_cast<uint64_t>(
                               static_cast<int64_t>(mx) - static_cast<int64_t>(mn)
                       ) + 1ULL;
    return {U <= MAX_U, U};
}

// ════════════════════════════════════════════════════════════════════════════════
//  ALGORITHM 1 — DialSort (sequential)
//  Passes: 2  |  Memory: O(U)  |  Prefix sum: NONE
//  Reference implementation — unchanged from teacher's original.
// ════════════════════════════════════════════════════════════════════════════════
static bool dialsort(std::vector<int>& a) {
    const size_t n = a.size();
    if (n <= 1) return true;

    int mn = a[0], mx = a[0];
    for (size_t i = 1; i < n; ++i) {
        if (a[i] < mn) mn = a[i];
        if (a[i] > mx) mx = a[i];
    }

    auto [ok, U64] = universe_size(mn, mx);
    if (!ok) { std::cerr << "[WARN] dialsort: U > MAX_U\n"; return false; }
    const size_t U = static_cast<size_t>(U64);

    // Pass 1 — Ingestion: build flat histogram
    std::vector<int> H(U, 0);
    for (size_t i = 0; i < n; ++i)
        H[static_cast<size_t>(a[i] - mn)]++;

    // Pass 2 — Projection: emit values in histogram order
    size_t out = 0;
    for (size_t y = 0; y < U; ++y) {
        const int val = static_cast<int>(y) + mn;
        for (int c = H[y]; c > 0; --c)
            a[out++] = val;
    }
    return true;
}

// ════════════════════════════════════════════════════════════════════════════════
//  ALGORITHM 2 — DialSort-Parallel (multi-threaded ingestion)
//  Reference implementation — unchanged from teacher's original.
// ════════════════════════════════════════════════════════════════════════════════
static bool dialsort_parallel(std::vector<int>& a, int nthreads = NUM_THREADS) {
    const size_t n = a.size();
    if (n <= 1) return true;

    int mn = a[0], mx = a[0];
    for (size_t i = 1; i < n; ++i) {
        if (a[i] < mn) mn = a[i];
        if (a[i] > mx) mx = a[i];
    }

    auto [ok, U64] = universe_size(mn, mx);
    if (!ok) { std::cerr << "[WARN] dialsort_parallel: U > MAX_U\n"; return false; }
    const size_t U = static_cast<size_t>(U64);

    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    const int nt = std::max(1, std::min(nthreads, hw > 0 ? hw : nthreads));

    std::vector<std::vector<int>> local_H(nt, std::vector<int>(U, 0));

    std::vector<std::thread> workers;
    const size_t chunk = (n + nt - 1) / nt;
    for (int t = 0; t < nt; ++t) {
        workers.emplace_back([&, t]() {
            const size_t lo = t * chunk;
            const size_t hi = std::min(lo + chunk, n);
            auto& lh = local_H[t];
            for (size_t i = lo; i < hi; ++i)
                lh[static_cast<size_t>(a[i] - mn)]++;
        });
    }
    for (auto& w : workers) w.join();

    std::vector<int> H(U, 0);
    for (int t = 0; t < nt; ++t)
        for (size_t y = 0; y < U; ++y)
            H[y] += local_H[t][y];

    size_t out = 0;
    for (size_t y = 0; y < U; ++y) {
        const int val = static_cast<int>(y) + mn;
        for (int c = H[y]; c > 0; --c)
            a[out++] = val;
    }
    return true;
}

// ════════════════════════════════════════════════════════════════════════════════
//  ALGORITHM 3 — TieredSort (student alternative proposal)
//
//  Two-level non-comparative integer sort.
//
//  MOTIVATION
//  ----------
//  DialSort's flat histogram of size U can exceed L2/L3 cache for large U
//  (e.g., U = 65536 → 256 KB at 4 bytes/entry, right at the L2 boundary on
//  most modern CPUs). Every ingestion write H[k-mn]++ becomes a potential
//  cache miss when the histogram doesn't fit in fast cache.
//
//  TieredSort trades one extra pass for a smaller active working set per pass:
//    - Coarse pass: K buckets of size ~U/K  → working set: O(K) integers
//    - Fine pass  : per-tier histogram, size tier_width = ceil(U/K)
//                   → working set: O(U/K) integers per tier
//
//  Choosing K = sqrt(U) minimizes max(K, U/K) = sqrt(U).
//
//  PASSES
//  ------
//   Pass 1 (Coarse scatter, O(n)):
//     For each element k:
//       tier_idx  = (k - mn) / tier_width
//       offset    = (k - mn) % tier_width
//       Push offset into buckets[tier_idx].
//
//   Pass 2 (Fine histogram per tier, O(n + K * tier_width) = O(n + U)):
//     For each non-empty tier t:
//       Build local_H[0..tier_width-1] by scanning buckets[t].
//       Emit values back into output array in histogram order.
//
//  INTERNAL BEHAVIOR VISUALIZATION (printed when n <= 32)
//  -------------------------------------------------------
//  When the input is small enough, TieredSort prints a step-by-step trace:
//    - Universe range [mn, mx], U, K, tier_width
//    - After Pass 1: contents of each non-empty coarse bucket
//    - After Pass 2: sorted output per tier
//  This satisfies the "visualization of internal behavior" requirement.
// ════════════════════════════════════════════════════════════════════════════════
static bool tieredsort(std::vector<int>& a, bool visualize = false) {
    const size_t n = a.size();
    if (n <= 1) return true;

    // ── Step 0: find range ────────────────────────────────────────────────────
    int mn = a[0], mx = a[0];
    for (size_t i = 1; i < n; ++i) {
        if (a[i] < mn) mn = a[i];
        if (a[i] > mx) mx = a[i];
    }

    auto [ok, U64] = universe_size(mn, mx);
    if (!ok) { std::cerr << "[WARN] tieredsort: U > MAX_U\n"; return false; }
    const size_t U = static_cast<size_t>(U64);

    // ── Step 1: choose K (number of tiers) ───────────────────────────────────
    // K = ceil(sqrt(U)), capped at TIERED_K_MAX and U.
    const size_t K = std::min(
        static_cast<size_t>(std::ceil(std::sqrt(static_cast<double>(U)))),
        std::min(TIERED_K_MAX, U)
    );
    const size_t tier_width = (U + K - 1) / K;   // ceil(U / K)

    if (visualize) {
        std::cout << "\n[TieredSort TRACE] n=" << n
                  << "  U=" << U << "  range=[" << mn << ", " << mx << "]"
                  << "  K=" << K << "  tier_width=" << tier_width << "\n";
    }

    // ── Pass 1: Coarse scatter ────────────────────────────────────────────────
    // buckets[t] holds the *offsets within the tier* (not the full keys).
    // This saves memory: offsets are in [0, tier_width) so they fit in int.
    std::vector<std::vector<int>> buckets(K);

    // Pre-reserve approximate capacity to reduce reallocations.
    const size_t approx_per_tier = (n / K) + 1;
    for (size_t t = 0; t < K; ++t)
        buckets[t].reserve(approx_per_tier);

    for (size_t i = 0; i < n; ++i) {
        const size_t offset   = static_cast<size_t>(a[i] - mn);
        const size_t tier_idx = offset / tier_width;
        const int    local    = static_cast<int>(offset % tier_width);
        buckets[tier_idx].push_back(local);
    }

    if (visualize) {
        std::cout << "[TieredSort TRACE] After Pass 1 (coarse scatter):\n";
        for (size_t t = 0; t < K; ++t) {
            if (buckets[t].empty()) continue;
            const int tier_base = static_cast<int>(t * tier_width) + mn;
            std::cout << "  Tier " << t
                      << " (base=" << tier_base << "): [";
            for (size_t j = 0; j < buckets[t].size(); ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << (buckets[t][j] + tier_base);
            }
            std::cout << "]\n";
        }
    }

    // ── Pass 2: Fine histogram + projection per tier ──────────────────────────
    std::vector<int> local_H(tier_width, 0);
    size_t out = 0;

    for (size_t t = 0; t < K; ++t) {
        if (buckets[t].empty()) continue;

        const int tier_base = static_cast<int>(t * tier_width) + mn;

        // Build local histogram for this tier.
        // local_H[j] = count of elements with offset j within the tier.
        std::fill(local_H.begin(), local_H.end(), 0);
        for (int local : buckets[t])
            local_H[static_cast<size_t>(local)]++;

        // Project: emit values in sorted order.
        // The actual key value is: tier_base + j
        // We must clamp j to not exceed mx.
        for (size_t j = 0; j < tier_width; ++j) {
            const int val = tier_base + static_cast<int>(j);
            if (val > mx) break;   // don't emit beyond universe
            for (int c = local_H[j]; c > 0; --c)
                a[out++] = val;
        }
    }

    if (visualize) {
        std::cout << "[TieredSort TRACE] After Pass 2 (output): [";
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << a[i];
        }
        std::cout << "]\n\n";
    }

    return true;
}

// ════════════════════════════════════════════════════════════════════════════════
//  DATA GENERATORS
// ════════════════════════════════════════════════════════════════════════════════
static std::vector<int> gen_uniform(size_t n, int U, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> d(0, U - 1);
    std::vector<int> a(n);
    for (auto& x : a) x = d(rng);
    return a;
}

static std::vector<int> gen_skewed(size_t n, int U, uint64_t seed) {
    std::mt19937_64 rng(seed);
    const int hot_limit = std::max(1, U / 20);
    std::uniform_int_distribution<int> hot(0, hot_limit - 1);
    std::uniform_int_distribution<int> cold(0, U - 1);
    std::bernoulli_distribution pick_hot(0.80);
    std::vector<int> a(n);
    for (auto& x : a) x = pick_hot(rng) ? hot(rng) : cold(rng);
    return a;
}

static std::vector<int> gen_sorted(size_t n, int U, uint64_t seed) {
    auto a = gen_uniform(n, U, seed);
    std::sort(a.begin(), a.end());
    return a;
}

static std::vector<int> gen_reverse(size_t n, int U, uint64_t seed) {
    auto a = gen_sorted(n, U, seed);
    std::reverse(a.begin(), a.end());
    return a;
}

// ════════════════════════════════════════════════════════════════════════════════
//  BENCHMARK HARNESS
// ════════════════════════════════════════════════════════════════════════════════
using SortFn = std::function<bool(std::vector<int>&)>;

struct Row {
    std::string algo;
    std::string dist;
    size_t      n       = 0;
    int         U       = 0;
    double      ms      = 0;
    double      mkeys_s = 0;
    double      speedup = 0;
    bool        correct = false;
    bool        skipped = false;
};

static Row run_one(const std::string& algo,
                   const std::string& dist,
                   const std::vector<int>& base,
                   int U,
                   SortFn fn,
                   double baseline_ms)
{
    Row row;
    row.algo = algo;
    row.dist = dist;
    row.n    = base.size();
    row.U    = U;

    for (int r = 0; r < WARMUP_ROUNDS; ++r) {
        auto tmp = base;
        if (!fn(tmp)) { row.skipped = true; return row; }
    }

    int64_t best = INT64_MAX;
    bool ok = true;

    for (int r = 0; r < MEASURE_ROUNDS; ++r) {
        auto tmp = base;
        const int64_t t0 = now_ns();
        if (!fn(tmp)) { row.skipped = true; return row; }
        const int64_t elapsed = now_ns() - t0;
        if (elapsed > 0 && elapsed < best) best = elapsed;
        if (!check_sorted(tmp)) { ok = false; break; }
    }

    if (best == INT64_MAX) { row.skipped = true; return row; }

    row.ms      = best / 1e6;
    row.mkeys_s = (base.size() / (best / 1e9)) / 1e6;
    row.speedup = (baseline_ms > 0) ? baseline_ms / row.ms : 1.0;
    row.correct = ok;
    return row;
}

// ── Printing ──────────────────────────────────────────────────────────────────
static constexpr int COL_W = 115;
static void separator() { std::cout << std::string(COL_W, '-') << "\n"; }

static void print_table_header() {
    std::cout << std::left
              << std::setw(28) << "Algorithm"
              << std::setw(10) << "Dist"
              << std::setw(12) << "N"
              << std::setw(8)  << "U"
              << std::setw(13) << "ms (best)"
              << std::setw(14) << "M keys/s"
              << std::setw(14) << "vs std::sort"
              << "OK\n";
    separator();
}

static void print_row(const Row& r) {
    if (r.skipped) {
        std::cout << std::left << std::setw(28) << r.algo
                  << std::setw(10) << r.dist
                  << std::setw(12) << r.n
                  << std::setw(8)  << r.U
                  << "[SKIPPED]\n";
        return;
    }
    std::cout << std::left
              << std::setw(28) << r.algo
              << std::setw(10) << r.dist
              << std::setw(12) << r.n
              << std::setw(8)  << r.U
              << std::fixed << std::setprecision(3)
              << std::setw(13) << r.ms
              << std::setw(14) << r.mkeys_s
              << std::setw(14) << r.speedup
              << (r.correct ? "OK" : "*** FAIL ***") << "\n";
}

static void csv_row(const Row& r) {
    if (r.skipped) {
        std::cout << r.algo << "," << r.dist << "," << r.n << "," << r.U
                  << ",SKIPPED,SKIPPED,SKIPPED,SKIPPED\n";
        return;
    }
    std::cout << r.algo    << ","
              << r.dist    << ","
              << r.n       << ","
              << r.U       << ","
              << std::fixed << std::setprecision(3)
              << r.ms      << ","
              << r.mkeys_s << ","
              << r.speedup << ","
              << (r.correct ? "OK" : "FAIL") << "\n";
}

// ════════════════════════════════════════════════════════════════════════════════
//  MAIN
// ════════════════════════════════════════════════════════════════════════════════
int main() {
    std::cout
            << "================================================================\n"
            << "DialSort vs. TieredSort — Direct Comparison Benchmark\n"
            << "Course : ST0245 - Data Structures and Algorithms, EAFIT\n"
            << "Paper  : DialSort: Non-Comparative Integer Sorting\n"
            << "         via the Self-Indexing Principle\n"
            << "================================================================\n"
            << "Compiler     : g++ " << __VERSION__ << "\n"
            << "Flags        : -O3 -std=c++17 -pthread\n"
            << "Threads      : " << NUM_THREADS << "\n"
            << "Warmup       : " << WARMUP_ROUNDS << " discarded runs\n"
            << "Measurement  : best-of-" << MEASURE_ROUNDS << " runs\n"
            << "Seed         : " << SEED << "\n"
            << "================================================================\n\n";

    // ── Visualization demo with tiny input ────────────────────────────────────
    std::cout << "================================================================\n";
    std::cout << "INTERNAL BEHAVIOR VISUALIZATION (n=16, U=64)\n";
    std::cout << "================================================================\n";
    {
        auto demo = gen_uniform(16, 64, static_cast<uint64_t>(SEED));
        std::cout << "Input : [";
        for (size_t i = 0; i < demo.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << demo[i];
        }
        std::cout << "]\n";
        tieredsort(demo, /*visualize=*/true);

        // Also show DialSort trace manually for comparison
        std::cout << "[DialSort TRACE] DialSort builds one flat histogram H[0..63],\n"
                  << "                 increments H[k] for each key k, then emits\n"
                  << "                 each value H[k] times in a single O(U) scan.\n"
                  << "                 No tier structure — one pass over all 64 buckets.\n\n";
    }

    // ── Full benchmark ────────────────────────────────────────────────────────
    const std::vector<size_t> Ns = {10'000, 100'000, 1'000'000, 10'000'000};
    const std::vector<int>    Us = {256, 1024, 65536};

    using GenFn = std::vector<int>(*)(size_t, int, uint64_t);
    struct Dist { std::string name; GenFn gen; };
    const std::vector<Dist> dists = {
            {"uniform", gen_uniform},
            {"skewed",  gen_skewed},
            {"sorted",  gen_sorted},
            {"reverse", gen_reverse},
    };

    SortFn fn_dialsort  = [](std::vector<int>& a){ return dialsort(a); };
    SortFn fn_parallel  = [](std::vector<int>& a){ return dialsort_parallel(a); };
    SortFn fn_tiered    = [](std::vector<int>& a){ return tieredsort(a, false); };
    SortFn fn_std       = [](std::vector<int>& a){ std::sort(a.begin(), a.end()); return true; };

    std::cout << "================================================================\n";
    std::cout << "TABLE — DialSort vs TieredSort vs std::sort\n";
    std::cout << "Column 'vs std::sort': speedup relative to GCC introsort\n\n";
    print_table_header();

    struct GroupResult {
        Row dialsort, parallel, tiered, std_sort;
    };
    std::vector<GroupResult> results;

    for (size_t n : Ns) {
        for (int U : Us) {
            for (const auto& dist : dists) {
                const uint64_t seed = static_cast<uint64_t>(SEED)
                                      ^ (static_cast<uint64_t>(n) * 1000003ULL)
                                      ^ (static_cast<uint64_t>(U) * 7919ULL)
                                      ^ 0xC0FFEEULL;

                const auto base = dist.gen(n, U, seed);

                auto r_std = run_one("std::sort", dist.name, base, U, fn_std, 0.0);
                r_std.speedup = 1.0;
                const double baseline_ms = r_std.ms;

                auto r_dialsort = run_one("DialSort",          dist.name, base, U, fn_dialsort, baseline_ms);
                auto r_parallel = run_one("DialSort-Parallel", dist.name, base, U, fn_parallel, baseline_ms);
                auto r_tiered   = run_one("TieredSort",        dist.name, base, U, fn_tiered,   baseline_ms);

                print_row(r_std);
                print_row(r_dialsort);
                print_row(r_parallel);
                print_row(r_tiered);
                std::cout << "\n";

                GroupResult g;
                g.std_sort = r_std;
                g.dialsort = r_dialsort;
                g.parallel = r_parallel;
                g.tiered   = r_tiered;
                results.push_back(g);
            }
        }
    }

    // ── Summary ───────────────────────────────────────────────────────────────
    std::cout << "================================================================\n";
    std::cout << "SUMMARY — DialSort vs TieredSort\n";
    std::cout << "================================================================\n\n";

    int ds_beats_tiered = 0, tiered_beats_ds = 0, total = 0;
    double sum_ratio = 0;
    double min_ratio = 1e9, max_ratio = 0;

    for (const auto& g : results) {
        if (g.dialsort.skipped || g.tiered.skipped) continue;
        ++total;
        const double ratio = g.dialsort.ms / g.tiered.ms;
        sum_ratio += ratio;
        min_ratio = std::min(min_ratio, ratio);
        max_ratio = std::max(max_ratio, ratio);
        if (ratio < 1.0) ++ds_beats_tiered;
        else             ++tiered_beats_ds;
    }

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Configurations measured                  : " << total << "\n"
              << "DialSort faster than TieredSort          : " << ds_beats_tiered << " / " << total << "\n"
              << "TieredSort faster than DialSort          : " << tiered_beats_ds  << " / " << total << "\n"
              << "Avg ratio DialSort / TieredSort           : " << (total > 0 ? sum_ratio / total : 0) << "x\n"
              << "Min ratio DialSort / TieredSort (best DS) : " << min_ratio << "x\n"
              << "Max ratio DialSort / TieredSort (worst DS): " << max_ratio << "x\n\n"
              << "NOTE: ratio < 1.0 => DialSort is faster than TieredSort\n"
              << "      ratio > 1.0 => TieredSort is faster than DialSort\n\n"
              << "DESIGN INSIGHT:\n"
              << "  TieredSort's two-level structure reduces the active cache footprint\n"
              << "  per pass from O(U) to O(sqrt(U)). This advantage grows as U increases.\n"
              << "  For small U (e.g. U=256), DialSort's flat histogram easily fits in L1\n"
              << "  cache, so the extra bookkeeping of TieredSort is counterproductive.\n"
              << "  For large U (e.g. U=65536), TieredSort should close the gap or win.\n\n";

    bool all_ok = true;
    for (const auto& g : results) {
        if (!g.dialsort.skipped && !g.dialsort.correct) { all_ok = false; break; }
        if (!g.parallel.skipped && !g.parallel.correct) { all_ok = false; break; }
        if (!g.tiered.skipped   && !g.tiered.correct)   { all_ok = false; break; }
    }
    std::cout << "All correctness checks: " << (all_ok ? "PASSED" : "*** FAILURES ***") << "\n\n";

    // ── CSV ───────────────────────────────────────────────────────────────────
    std::cout << "================================================================\n";
    std::cout << "CSV OUTPUT\n";
    std::cout << "================================================================\n";
    std::cout << "algo,dist,N,U,ms_best,Mkeys_per_s,speedup_vs_std,correct\n";
    for (const auto& g : results) {
        csv_row(g.std_sort);
        csv_row(g.dialsort);
        csv_row(g.parallel);
        csv_row(g.tiered);
    }

    return 0;
}