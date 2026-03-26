# PA2 Retrospective

What went well:
- Reusing the PA1 tokenizer kept the PA2 implementation focused on post-token classification instead of rebuilding the front end.
- The checked-in PA2 fixtures were precise about the expected output format, especially for literal types and user-defined suffixes.
- The assignment-local and course tests together covered the tricky cases: suffix rules, raw strings, mixed encodings, and concatenation.

What went poorly:
- I initially treated trailing whitespace/newlines as part of a concatenated string literal's rendered source, which caused subtle output mismatches.
- Some of the literal rules in this assignment differ from newer-standard expectations, so a few assumptions had to be corrected against the fixtures.

Suggestions:
- Add one or two explicit tests that show how string-literal concatenation should spell the combined source when separators are present.
- Call out in the starter notes that PA2 ignores whitespace/newlines in output, but still preserves a single separator between adjacent string literal tokens.
- A short table of the assignment-specific suffix rules for string, char, integer, and floating literals would reduce backtracking.
