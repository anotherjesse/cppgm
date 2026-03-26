# PA1 Retrospective

## What went well
- Implementing the translation phases as a pull-based stream pipeline (`IPullStream`) made handling complex features like Trigraphs, UCN, and line splicing remarkably elegant. It allowed precise tracking of state without needing complex lookahead management since each stream handled its own bounded lookahead via `unget()`.
- Leveraging greedy longest-match with a robust backtrack enabled the correct tokenization of edge cases like raw string literals and overlapping tokens.
- Using Python locally to iterate and iteratively refine the generated C++ skeleton proved to be an effective strategy to iterate on the tokenizer state machine.

## What went poorly
- Navigating the exact ordering of translation phases 1 and 2—specifically regarding how `\u` escapes should be processed relative to line splicing—required a bit of experimentation. The standard describes them somewhat independently, but checking the reference implementation helped clarify the exact sequence (UTF8 -> Trigraph -> UCN -> Line Splice -> EOF Newline).
- Hand-rolling a deterministic finite automaton (DFA) approach for string literal parsing became verbose, specifically dealing with the complex prefix rules (`u8`, `R`, `L`, etc.) and the fallback to `identifier` when parsing fails.

## Suggestions for improving the assignment
- The assignment document states `convert any universal-character-names (escape sequences \uXXXX and \UXXXXXXXX) into their unicode code point` as part of the translation pipeline. However, standard N3485 phase 1 replaces characters NOT in the basic source character set WITH UCNs, not the other way around. It would be helpful to explicitly clarify that for this assignment, we are decoding UCNs to characters, diverging slightly from standard Phase 1 literal interpretation.
- The assignment text contains a typo for the bitwise XOR operator, listing it as `ˆ` (U+02C6) and `ˆ=` instead of the standard ASCII caret `^` and `^=`. Updating this would prevent confusion.