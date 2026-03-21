## What Went Well

- The starter `pptoken.cpp` already included the Annex E identifier ranges and the token output interface, which made it possible to focus on the translation and tokenization rules instead of scaffolding.
- A single scanner that handled trigraphs, line splicing, UCN decoding, comments, and tokenization together matched the assignment tests well and kept the edge cases localized.
- The checked-in course tests were especially useful for pinning down the non-obvious behaviors around raw strings, `%:include`, `<::`, and pp-number greediness.

## What Went Poorly

- The assignment still requires a lot of reverse engineering from fixtures, especially for comment/newline behavior and raw string corner cases.
- Because `dev/Makefile` only rebuilt the touched target when asked directly, it was easy to momentarily test a stale `pptoken` binary after editing the source.
- The starter comments point in the right direction, but they do not narrow the implementation enough to avoid several false starts on phase-order details.

## Suggestions

- Add a short note in the starter kit that multiline block comments in this harness collapse to one whitespace token without preserving interior newlines.
- Add one explicit fixture for the `#` / `%:` / `include` directive state machine, including comments and whitespace between the directive marker and `include`.
- Add a tiny smoke target or README note reminding contributors to run `make -C dev <tool>` or the root `make build` after editing shared sources so stale binaries are less likely.
