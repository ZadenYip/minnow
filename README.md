# ⚠️ Academic Integrity Warning / 学术诚信警告

This repository contains my personal solutions for the Stanford CS 144 networking labs. I have made it public to showcase my work and document my learning process.

**If you are a student currently taking CS 144 or a similar course, you are strongly advised against referencing, copying, or submitting any part of this code as your own work.**

Doing so constitutes a serious violation of academic integrity and the Stanford Honor Code (or your institution's equivalent).

The true value of these labs comes from the struggle, the debugging, and the satisfaction of building a complex system from the ground up. By looking at a solution, you rob yourself of this invaluable learning experience. I urge you to embrace the challenge and complete the work independently.

---

## Original README Content

Stanford CS 144 Networking Lab
==============================

These labs are open to the public under the (friendly) request that to
preserve their value as a teaching tool, solutions not be posted
publicly by anybody.

Website: https://cs144.stanford.edu

To set up the build system: `cmake -S . -B build`

To compile: `cmake --build build`

To run tests: `cmake --build build --target test`

To run speed benchmarks: `cmake --build build --target speed`

To run clang-tidy (which suggests improvements): `cmake --build build --target tidy`

To format code: `cmake --build build --target format`
