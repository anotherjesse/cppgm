## What Went Well

- Reusing the PA1 preprocessing-token lexer through `dev/src/PPLexer.*` kept the assignment focused on literal semantics instead of reimplementing scanning.
- The checked-in PA2 fixtures cover the real trouble spots well: integer range selection, invalid user-defined suffix placement, raw string decoding, and string-literal concatenation.
- Keeping post-token parsing in small literal-specific helpers made it practical to iterate on the integer, character, floating, and string rules independently.

## What Went Poorly

- The starter `posttoken.cpp` mixed useful reference tables with placeholder demo behavior, so it took extra care to preserve the reusable pieces while replacing the fake pipeline.
- A few important behaviors are only obvious from the checked-in refs, especially huge integer UDL prefixes and the exact string-prefix compatibility matrix.
- The shared build layout makes it easy to accidentally test a stale `posttoken` binary if the rebuild and the test run overlap.

## Suggestions

- Add a short note in the assignment handout that integer user-defined literals validate the core syntactically, even when the builtin integer literal would overflow.
- Document the string-literal concatenation prefix rules expected by the harness, especially the distinction between ordinary strings being neutral and `u8` being incompatible with `u`, `U`, and `L`.
- Replace the demo `main` in the starter with a minimal real token loop so students can extend working behavior instead of first deleting placeholder output.
