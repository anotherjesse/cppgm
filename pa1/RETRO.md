## What went well

- The checked-in fixtures were enough to pin down the course-defined behavior for UCNs, trigraphs, raw strings, header names, and the preprocessing operator set.
- Rebuilding the tokenizer around a single decoded source stream made it easier to keep the translation phases in the right order.

## What went poorly

- The comment handling behavior was not obvious from the summary alone. Block comments suppress internal newlines in the token stream, which was easy to miss until I compared against the reference binary.
- Raw-string token data needs UTF-8 re-encoding from code points, not byte casts. That was subtle because most fixtures are ASCII-only.
- The Annex E identifier rules need the E.2 exclusion at start position, not just the E.1 allow-list.

## Suggestions

- Add one or two tiny fixtures that isolate multi-line block comments and raw strings containing non-ASCII characters.
- Call out the identifier-start vs identifier-continue distinction for Annex E more explicitly in the assignment notes.
- A short phase-order table in the README would help, especially for the trigraph/UCN/line-splicing interactions.
