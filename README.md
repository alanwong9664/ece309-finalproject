# ECE 309 · Final Project starter — Skills, Memory, and Command Policy

Read `Final_spec.md` (in this folder) first. This file only covers setting up the repo.

## 1. Bring your P2 and P3 code

Copy eight files from your P3 repo (details in `src/core/README.md` and `src/tools/README.md`):

```
include/core/conversation.h        include/core/sentinel_scanner.h
src/core/conversation.cpp          src/core/sentinel_scanner.cpp
include/tools/tool_registry.h      src/tools/tool_registry.cpp
src/tools/workspace.cpp            src/tools/calculator_eval.cpp
```

Do **not** copy `message.h`, `tool.h`, `builtin_tools.*`, `harness.*` or `main.cpp`; the
starter's versions are newer.

If your P2 or P3 code doesn't work, build against the instructor's references instead:

```bash
cmake -S . -B build -DUSE_P2_REFERENCE=ON -DUSE_P3_REFERENCE=ON
```

## 2. Build and test (Linux / WSL2)

```bash
cmake -S . -B build              # ASan + UBSan are on by default
cmake --build build -j
./build/test_p2 && ./build/test_loop   # earlier projects: must pass before you start
./build/test_final                     # your tests (8 examples to start)
cd build && ctest --output-on-failure   # everything, plus the end-to-end goldens
```

To check that you compile with zero warnings, as the grader does, add
`-DMH_WERROR=ON` to the configure line.

## 3. Try the harness

```bash
./build/miniharness --script scripts/memory_demo.script     # needs your BST + top_k
./build/miniharness --script scripts/policy_demo.script     # needs your policies + chain
./build/miniharness --script scripts/skills_demo.script     # needs DepGraph, SkillIndex, LruCache
./build/miniharness --script scripts/context_demo.script --context-budget 1450 --keep-recent 2
./build/miniharness --script scripts/memory_demo.script --memory my_memories.txt   # run it twice: memories persist
```

`policy_demo` asks you two y/n questions. Commands are simulated: nothing is ever run.

## 4. What is yours

| File | Status |
|---|---|
| `include/util/min_heap.h` (`MinHeap`, `top_k`) | **you implement** (9 pts) |
| `include/util/lru_cache.h` (`LruCache`) | **you implement** (14 pts) |
| `src/memory/memory_index.cpp` + private section of `include/memory/memory_index.h` | **you implement** (14 pts) |
| `src/memory/context_builder.cpp` (`select_context`) | **you implement** (6 pts) |
| `src/policy/policies.cpp`, `src/policy/policy_chain.cpp` | **you implement** (12 pts) |
| `src/skills/dep_graph.cpp` + private section of `include/skills/dep_graph.h` | **you implement** (12 pts) |
| `src/skills/skill_index.cpp` | **you implement** (5 pts) |
| private sections of `include/policy/policies.h` and `include/skills/skill_index.h` | yours to change if you want |
| `tests/final/test_final.cpp` (≥ 20 tests: 8 given, ≥ 12 yours) | **you write** |
| `docs/design-log-final.md` | **you write** |
| everything else | provided, do not modify |
