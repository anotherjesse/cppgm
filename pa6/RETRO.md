What went well

- Reusing PA5 as a library made PA6 much simpler: the recognizer could consume the fully preprocessed token stream instead of reimplementing translation phases again.
- A grammar-driven recognizer was fast to iterate on because most fixes were token classification or ambiguity handling, not large parser rewrites.
- The mock-name lookup rules fit cleanly as parser hooks for `class-name`, `template-name`, `typedef-name`, `enum-name`, and `namespace-name`.

What went poorly

- Pure grammar recognition was still not quite enough for the close-angle-bracket ambiguity. The course-specific template-closing rule needed an extra validation layer.
- It was easy to get subtle token-classification bugs, especially for `ST_EMPTYSTR` and the split `ST_RSHIFT_1` / `ST_RSHIFT_2` handling.
- Building the runtime grammar AST with raw pointers was fragile; the first version had a lifetime bug before parsing semantics were even exercised.

Suggestions

- The starter kit would benefit from a small machine-readable grammar export instead of only the raw `.gram` file and HTML explorer.
- A note in the assignment text that some ambiguities still need semantic bias beyond the context-free grammar would make the close-angle requirement clearer.
- The PA6 special-token mapping rules deserve a compact checklist in the README, since getting those terminals wrong cascades into otherwise confusing parse failures.
