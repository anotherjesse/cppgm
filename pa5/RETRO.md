# PA5 Retrospective

## What Went Well

- Reused PA4 macro expansion core directly inside `preproc`, which preserved behavior for define/undef, recursion suppression, varargs, `#`, and `##`.
- Implemented directive state handling (`#if/#elif/#else/#endif`, include recursion, pragma once, line control) in a single streaming pass, which made inactive-group behavior and nesting rules tractable.
- Added strict PA5 phase-7 validation by treating PA2-invalid outputs as fatal, matching assignment expectations.

## What Went Poorly

- `__LINE__` handling took multiple iterations. Correct behavior required a logical-line map aligned to phase 2/3 effects (splicing/comments), not naive newline counting.
- Text-sequence flushing needed careful balancing: flushing too early broke cross-line function-like macro invocations, while flushing too late broke line-sensitive macro expansion.
- Embedding prior assignment code required extra guard hooks in `macro.cpp`/`ctrlexpr.cpp` to avoid duplicate-main translation unit conflicts.

## Suggestions

- Add one explicit PA5 note showing expected `__LINE__` behavior for: plain lines, line-spliced directives, block comments with embedded newlines, and cross-line macro invocations.
- Provide a small reference pseudocode/state table for active vs inactive directive handling, especially around `#elif`/`#else` ordering errors in inactive blocks.
- Include one focused test fixture that isolates `_Pragma` parsing errors vs ignored unknown pragmas to reduce ambiguity during implementation.
