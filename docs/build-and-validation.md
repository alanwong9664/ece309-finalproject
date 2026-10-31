# Build, provenance, and validation

## Code provenance

- P2 is the **instructor reference**. Its four files were copied unchanged from `reference/p2/` into `include/core/` and `src/core/` so the default CMake configuration works. The original reference files are retained. CMake labels these paths as "your own" based on file location; that label does not describe their authorship.
- The four P3 carry-over files were copied unchanged from the owner's existing P3 project: `tool_registry.h`, `tool_registry.cpp`, `workspace.cpp`, and `calculator_eval.cpp`.
- OpenAI Codex generated the final-project implementations, added tests, and prepared the design log and this document. An independent Codex agent performed read-only implementation and test review. This goes beyond the AI assistance permitted by the supplied assignment; it must not be represented as solely student-written work.

## Required Linux build

From the repository root on Linux, WSL2, or the department machines:

```bash
cmake -S . -B build -DMH_WERROR=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

ASan and UBSan are enabled by default. No reference flags are required because the instructor's P2 files are present in the carry-over paths. Selecting `-DUSE_P2_REFERENCE=ON` also uses the same reference implementation directly. The supplied P3 reference remains available via `-DUSE_P3_REFERENCE=ON`.

## Verified results

Verified on 2026-10-31 using Ubuntu WSL, GCC 13.3.0, CMake 3.28.3, C++17, warnings as errors, AddressSanitizer, and UndefinedBehaviorSanitizer:

| Check | Result |
| --- | --- |
| Clean build, default P2/P3 selection | Passed without compiler warnings |
| P2 regression | 14/14 |
| P3 tool loop | 9/9 |
| Final test suite | 38/38, including the original 8 examples |
| End-to-end sessions | 11/11 |
| Sanitizer diagnostics in final run | None |

Hidden grading tests were not available. Passing the included and added tests does not establish a hidden-test score or compliance with the assignment's AI-use policy.

## Performance measurements

The tests print the required timings and BST heights. For measurements without sanitizer overhead:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DMH_SANITIZE=OFF -DMH_WERROR=ON
cmake --build build-release --target test_final --parallel 4
./build-release/test_final =heap_top_k_edges_random_and_benchmark
./build-release/test_final =bst_sequential_and_shuffled_heights
./build-release/test_final =index_benchmark_sizes
```

The index test generates 100, 1,000, and 5,000 documents and reports the mean over 20 queries per implementation. Fixed random seeds make correctness workloads reproducible; elapsed times vary with hardware and system load. The design log records an actual optimized run, separately from the sanitizer validation.

## Submission files

`github.txt` identifies the owner's final-project repository. A submission ZIP must contain the same committed source, documentation, tests, and sample inputs, while excluding `.git/`, build directories, and assistant configuration. No agent instructions or coordination notes are part of this repository.
