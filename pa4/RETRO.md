# PA4 Retrospective

## What went well
- Reusing PA1 tokenization and PA2 post-token emission reduced duplication and kept behavior aligned with earlier assignments.
- Implementing expansion around per-token hide/unavailable metadata made recursion and nested-invocation cases tractable.
- Building argument substitution with separate raw/expanded paths handled `#`, `##`, and variadic behavior in one consistent pipeline.

## What went poorly
- The nesting semantics are subtle; several recursion cases required iterative fixes before matching the checked-in tests.
- Stringization required preserving token-boundary whitespace metadata to avoid over- or under-spacing in emitted string literals.
- Re-tokenizing preprocessed output through the full PA2 pipeline caused an extra phase-1 pass and introduced trigraph regressions.

## Suggestions
- Provide a small shared preprocessor-token utility layer (token model, argument parser, hide-set helpers) to reduce PA4 boilerplate.
- Add one dedicated assignment note warning against re-applying phase 1-3 after macro expansion when emitting PA2-style output.
- Include a compact “nasty recursion and stringize” starter fixture that combines `#`, `##`, variadics, and nested invocation in one file for faster debugging.
