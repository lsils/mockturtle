---
name: Bug report
about: Report a bug if something doesn't work as expected
title: ''
labels: ''
assignees: ''

---

**Describe the bug**
What went wrong? (e.g. the optimized circuit was not equivalent to the original one, an assertion failed, etc.)

**To Reproduce**
Steps to reproduce the behavior:
1. Which version of `mockturtle` (commit or PR number) are you using? (Preferably the latest, unless there are special reasons.)
2. A complete snippet of your code (usually a cpp file including `main`).
3. The benchmark circuit for which the error occurs. Please try to minimize it first by using the testcase minimizer ([docs](https://mockturtle.readthedocs.io/en/latest/debugging.html#testcase-minimizer), [example code](https://github.com/lsils/mockturtle/blob/master/examples/minimize.cpp)).
4. Error messages or print-outs you see (if any) (preferably copy-pasted as-is).

**Environment**
 - OS: [e.g. Linux]
 - Compiler: [e.g. GCC 12]
 - Compilation mode: [DEBUG or RELEASE]

**Additional context**
Add any other context about the problem here.

**Check list**
 * [ ] I have tried to run in DEBUG mode and there was no assertion failure (or the reported bug is an assertion failure).
 * [ ] I have made sure that the provided code compiles and the testcase reproduces the error.
 * [ ] I have minimized the testcase.
