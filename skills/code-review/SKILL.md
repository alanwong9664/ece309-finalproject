---
name: code-review
description: Review C++ code for leaks, the Rule of Five, and const-correctness.
requires: [cpp-style]
---
# Code review
1. Read the files with read_file.
2. For every class that owns memory, check all five special members.
3. Check every getter is const and every loop bound is correct.
4. List problems by file and line, most serious first.
