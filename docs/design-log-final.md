# Final Project Design Log

**Project owner:** Alan Wong (alanwong9664). **P2:** I used the instructor reference in the carry-over paths. **P3:** I used my existing implementation. I requested comment removal during cleanup.

**Tools used:** OpenAI Codex generated the final-project implementations, added tests, and drafted this log; a separate Codex agent reviewed the code and tests. GCC, CMake, and CTest were used to build and run tests, ASan and UBSan to check memory safety and undefined behavior, Git for version control, and Ubuntu WSL for Linux validation.

## 1. LRU cache

The hash table maps keys to nodes in a manually linked list, with the most recent node at the head. A middle-node get finds X through the table. With predecessor P, successor N, and old head H, unlinking writes P->next = N and N->prev = P. Reinsertion writes X->prev = nullptr, X->next = H, H->prev = X, and head_ = X. That is six pointer assignments, independent of cache length. Getting the current head skips relinking; successful and unsuccessful lookups increment their respective counters.

For a full-cache put, this implementation first prepares a new node under unique ownership and inserts its pointer into the hash table. This protects against leaking the allocation if insertion throws. It then attaches the node at the front: new->prev = nullptr, new->next = old_head, old_head->prev = new, and head_ = new. The temporary extra entry is immediately removed by evicting the old tail T. Since the cache previously contained an entry, T now has predecessor P; unlinking writes P->next = nullptr and tail_ = P. The table entry is erased before T is deleted, and the eviction counter increases once.

For an initially empty cache, attaching the first node also sets tail_. Capacity zero returns without allocation; capacity one uses the same insertion and eviction logic. Erase does not count as eviction, and clear preserves statistics. List operations take constant time; hash operations are expected amortized O(1). A 20,000-operation vector-model comparison checks ordering and counters, and a 300,000-entry replacement workload exercises repeated allocation and eviction.

## 2. Heap and top-k

The heap stores a complete tree in a vector and uses the supplied comparator for every ordering decision. Push follows parent indices upward. Pop replaces the root with the last item and follows the smaller child downward. Empty access throws, and singleton removal avoids moving an element onto itself.

Top-k retains at most k candidates, so insertion and replacement traverse at most log k levels. The scan therefore costs O(n log k), with O(k) auxiliary storage, rather than sorting all n values. For k = 1, the scan is linear; for k greater than n, storage never exceeds n. Extracting and reversing retained entries produces the requested largest-first order.

An optimized run on 100,000 seeded random integers measured **0.113337 ms** for k = 10 and **22.218211 ms** for a full descending sort. The outputs matched. This single timing sample depends on scheduling and hardware.

## 3. BST height

Sequential insertion of ids 1 through 1,000 produced height **1,000**. Shuffling the same ids with seed 312 produced height **21**. The test tree rooted at 50, with children 30/70 and grandchildren 20/40/60/80, has height three.

Increasing ids always follow the right branch, producing a linked list. Find, insert, and remove can consequently take O(n), and constructing the sequential tree takes O(n squared). Shuffling improves this particular shape without providing a worst-case guarantee. AVL balancing would provide logarithmic height, but the core implementation deliberately remains unbalanced.

Traversal, height calculation, insertion, and removal use iteration. Destruction rotates left children upward only while dismantling the tree, avoiding recursive stack growth and allocation in the destructor. A 5,000-node sequential test checks the deep shape and removes every record. Randomized operations are also compared against std::map.

## 4. DFS colors

White means unseen, gray means currently on the active path, and black means fully processed. Only an edge to gray identifies a cycle. In the diamond a->b, a->c, b->d, c->d, d is black when c reaches it. Treating every previously visited node as a cycle would incorrectly reject this graph.

The implementation uses explicit DFS frames and post-order output, preserving requirement insertion order without recursive calls. A gray target selects only the cycle's active-path suffix for the exact error message. Successful traversal deliberately leaves the caller's error string unchanged. Reverse-edge breadth-first traversal sorts each distance level alphabetically and excludes the starting name, including when cycles or missing requirements exist.

## 5. Inverted index

Each distinct document word contributes one posting. Search visits only postings for distinct query words, accumulates scores, and uses top-k with smaller document ids winning ties.

| Documents | Index ms/query | Linear ms/query |
| --- | ---: | ---: |
| 100 | 0.000405 | 0.049640 |
| 1,000 | 0.000880 | 1.132712 |
| 5,000 | 0.002903 | 2.402655 |

These measurements used GCC 13.3.0 Release optimization without sanitizers, steady_clock, 20 repetitions, generated documents, and query "topic17 topic43". Index construction was excluded. The linear method re-tokenizes the collection; sparse posting lookup grows much more slowly here. Common-word queries would touch more postings and narrow the advantage.

## 6. Threat model

The default chain rejects unlisted program names and recognizable dangerous substrings, requests confirmation for selected programs, and limits request frequency. It is not a general sandbox or a parser for every command language. In particular, allowing python3 does not restrict what Python code could do if real execution were introduced. Currently commands are simulated, so the chain's decisions do not execute operating-system commands.

## 7. Design improvement and validation

A future version should separate simulation policy from an actual execution sandbox and structured argument validation. Substring checks alone cannot enforce interpreter behavior.

A clean default Linux build passed with warnings as errors and ASan/UBSan enabled: P2 14/14, P3 9/9, final tests 38/38, and end-to-end sessions 11/11. No sanitizer diagnostics appeared in that final run. Hidden grading tests were unavailable. Benchmark measurements were collected separately from sanitizer validation.
