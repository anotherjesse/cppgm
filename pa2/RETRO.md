## What Went Well

- Reusing the PA1 lexical behavior inside `posttoken` kept the PP-token stream aligned with the checked-in tests.
- The PA2 tests were strong enough to drive the tricky rules: integer type selection, raw strings, UTF-16 surrogate handling in strings, and concatenation invalidation.

## What Went Poorly

- The starter for `posttoken` had lookup tables and output helpers, but none of the actual implementation, so the first PA2 checkpoint was still a full assignment build.
- Several course-defined behaviors differ across character literals, string literals, and concatenation, which made the first implementation pass too permissive.

## Suggestions

- Document the PA2 concatenation rules more explicitly, especially the interaction between ordinary, `u8`, `u`, `U`, and `L` strings.
- Call out that `char16_t` strings accept non-BMP code points via surrogate pairs while `char16_t` character literals do not.
