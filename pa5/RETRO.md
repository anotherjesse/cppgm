what went well

- Reusing the PA4 tokenizer and macro engine kept the PA5 implementation incremental instead of becoming a separate preprocessor rewrite.
- Carrying real source-location metadata through active text sequences made `__LINE__`, multiline macro calls, and `#line` fit together cleanly.
- Working in small checkpoints made it easier to isolate regressions when PA5 changes exposed PA4 macro edge cases.

what went poorly

- The starter left nearly all PA5 behavior unimplemented, so the early work was mostly building infrastructure before any interesting tests could pass.
- The original line tracking was too tied to logical lines from tokenization, which broke physical-line expectations around comments and took extra refactoring to unwind.
- `_Pragma("once")` and `#pragma once` were easy to under-scope at first because the tests only expose the real requirement once includes are already working.

suggestions

- Add an earlier assignment note that PA5 line numbering is based on physical source lines, not just the logical lines left after comment replacement.
- Include one small checked-in fixture that mixes empty-argument `##` pasting with later PA5 work, because shared macro regressions can otherwise hide until much later.
- The PA5 starter would be easier to extend if it came with a minimal preprocessing context object for include and pragma-once state instead of only a `main` stub.
