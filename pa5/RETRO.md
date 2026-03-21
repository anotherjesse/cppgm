What went well

- Reusing the PA4 macro-expansion machinery made the directive layer much smaller than a full restart would have.
- Making `preproc` use the PA2 post-tokenizer in a strict mode kept the phase-7 output aligned with earlier assignments and caught invalid-token regressions automatically.
- The checked-in tests covered the important integration points well: predefined macros, pragma handling, include search, `#line`, and no-shared-state processing across multiple source files.

What went poorly

- Source line tracking was the trickiest part of the assignment. Counting only logical token lines was wrong once block comments and line splices started affecting `__LINE__`.
- `defined` in controlling expressions needed special handling to prevent premature macro expansion of its operand, which was easy to miss in an otherwise generic expansion pipeline.
- The PA5 surface area is much larger than PA4, so keeping the implementation inside a single entry file started to show strain.

Suggestions

- The starter material should emphasize earlier that PA5 needs physical-line-aware bookkeeping, not just the line breaks left after tokenization.
- A short note that `defined(__FILE__)` and similar cases must protect the operand from macro expansion would save a debugging round.
- By this point the course would benefit from a starter shared preprocessor core in `dev/src/`, since PA4 and PA5 naturally want to share most of the same machinery.
