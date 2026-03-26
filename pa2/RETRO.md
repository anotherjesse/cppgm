# PA2 Retrospective

## What Went Well
- Reusing PA1 tokenization in-process kept PA2 focused on token analysis instead of re-solving phase 1/2/3 behavior.
- Implementing clear per-token handlers (`pp-number`, character literals, string-literal runs) made it easier to match reference output exactly.
- Building string-literal concatenation around maximal runs and explicit prefix/suffix set checks handled the large `700-hard-string-concat` matrix reliably.

## What Went Poorly
- Raw-string termination originally relied on transformed text and broke when trigraphs appeared in raw content; this surfaced only in PA2 raw tests.
- Keeping PA2-specific user-defined suffix policy (must start with `_`) aligned with tests required explicit validation beyond PA1 token categories.

## Suggestions
- Add a short PA2 starter note explicitly stating that PA2 user-defined literals in this assignment are accepted only with `_`-prefixed suffixes.
- Add one focused PA1/PA2 cross-assignment test for raw-string delimiters with trigraph-like content to catch early-termination bugs earlier.
- Provide a compact integer-literal type-selection table in the PA2 README with decimal vs non-decimal candidate order for quick reference.
