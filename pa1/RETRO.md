# PA1 Retrospective

## What Went Well
- Building the tokenizer as a clear pipeline (UTF-8 decode, trigraphs, line splicing, UCN replacement, tokenization) made debugging individual behaviors much faster.
- Driving implementation directly from checked-in `pa1/tests` and `course/pa1` fixtures kept behavior aligned with assignment expectations.
- Using explicit state for include-directive context (`#include`/`%:include`) made header-name tokenization deterministic.

## What Went Poorly
- Raw string literal handling required an extra iteration because trigraph replacement must not affect raw-string contents in emitted token text.
- Phase interactions (trigraphs, splicing, comments, literals) are easy to implement in the wrong order without targeted regression checks.

## Suggestions
- Add a short starter note in PA1 about raw-string interaction with phase-1/phase-2 transformations, since this is a common source of subtle mismatches.
- Include a small reference section in the starter with a minimal “tokenization order of operations” checklist.
- Add one additional explicit test that combines UCN, trigraphs, and raw strings in one file to make expected ordering even clearer.
