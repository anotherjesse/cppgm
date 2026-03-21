What went well

- Reusing the PA1 lexer and PA2 post-token pipeline kept the macro processor aligned with earlier assignments instead of forking token logic again.
- Converting `posttoken` into a reusable library entrypoint paid off immediately for final macro output and reduced duplicated literal handling.
- The checked-in course tests were strong enough to flush out several non-obvious macro edge cases, especially around `##`, whitespace before function-like invocations, and stringization.

What went poorly

- The first expansion model was too eager: expanding all actual arguments up front caused false failures in stringized arguments and made debugging harder.
- `##` needed finer provenance tracking than expected. Generated or argument-sourced `##` tokens cannot be treated like replacement-list paste operators in later passes.
- Some reference behaviors diverged from the current PA1 and PA2 implementation details, especially around pasted tokens and user-defined literal suffixes, so the macro layer needed a few targeted compatibility fixes.

Suggestions

- The starter code should point out earlier that function-like macro invocation in this harness allows whitespace between the macro name and `(`, since that differs from many simplified implementations.
- A short note in the assignment materials about raw versus expanded arguments for `#` and `##` would save time; that distinction drives several of the hardest bugs.
- Adding one or two assignment-local tests for stringized invalid macro invocations and for pasted Unicode UDL suffixes would make the expected behavior clearer without relying on course-side surprises.
