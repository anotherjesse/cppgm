## What Went Well

- Reusing the PA2 tokenizer and literal/output pipeline let `macro` focus on directive parsing and macro expansion instead of rebuilding phase 7 again.
- The checked-in PA4 tests covered the hard semantics well: stringizing, placemarkers, recursion blocking, whitespace-sensitive behavior, and token-paste retokenization.

## What Went Poorly

- The PA4 starter was completely empty, so the assignment still required building a full macro engine from scratch on top of reused lexical code.
- Several course-specific behaviors were only obvious after reading the tests closely, especially whitespace-tolerant function-like invocation, lazy argument prescan, and how `##` results should be rescanned.

## Suggestions

- Call out explicitly in the PA4 README that function-like macro invocation in the checked-in tests accepts intervening whitespace inside a text-sequence, since that differs from many first-pass implementations.
- Add a short note that `##` paste results may rescan into multiple preprocessing tokens, not just one, because that affects both invalid-token handling and user-defined-literal cases.
