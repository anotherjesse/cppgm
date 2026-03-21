## What Went Well

- Reusing the PA1 lexer and the PA2 literal-decoding logic made it practical to focus the assignment on parsing and evaluation instead of rebuilding token infrastructure again.
- The recursive-descent parser matched the assignment grammar directly, which made precedence, associativity, short-circuiting, and `defined` handling straightforward to validate against the refs.
- The checked-in supplemental tests were especially useful for the non-obvious semantic cases: inactive-branch suppression, `defined` syntax errors, signed division overflow, and Unicode character literals.

## What Went Poorly

- The starter for `ctrlexpr` is effectively empty, so the first working version requires implementing token conversion, parsing, and evaluation all at once.
- The behavior of the alternative operator keywords is easy to misread because this harness treats them as preprocessing operators earlier than a naive PA2-style identifier path would suggest.
- Several important semantics are only really clear after reading both the README and the refs together, especially the course-defined 64-bit promotion model and the conditional operator result type.

## Suggestions

- Add one small starter note that the alternative operator spellings such as `not`, `and`, and `bitand` arrive from phase 3 as preprocessing operators, not plain identifiers.
- Include an explicit fixture showing that `?:` still determines a common signed/unsigned result type using both branches even when one branch is not evaluated.
- Provide one short recommended architecture sketch in the handout, for example phase-3 tokenization, line splitting, line-local token conversion, then recursive-descent evaluation with an `evaluate` flag for short-circuit contexts.
