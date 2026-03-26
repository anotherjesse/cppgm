# PA6 Retrospective

## What Went Well

- Reused the PA5 preprocessor pipeline directly in `recog`, so token preparation and diagnostics stayed consistent with earlier assignments.
- Implemented grammar loading from `pa6.gram` with support for grouped and quantified EBNF forms (`()`, `?`, `*`, `+`), which let the recognizer follow the checked-in grammar without hand-flattening rules.
- Added identifier-category predicates (`class-name`, `template-name`, `typedef-name`, `enum-name`, `namespace-name`) and integrated them in parse completion, matching PA6 name-based disambiguation expectations.

## What Went Poorly

- Template close-angle behavior was initially too permissive. The recognizer accepted parses that consumed `>` as part of an argument expression instead of closing the template argument list.
- Ambiguity between declaration and expression paths required several iterations to align with PA6 fixture behavior, especially for the `500-closing-angle-bracket-*` cases.
- Embedding PA5 into PA6 required an extra compile guard in `preproc.cpp` to avoid duplicate `main` symbols, which was easy to miss until integration.

## Suggestions

- Add a short PA6 note that explicitly states the intended close-angle disambiguation rule with one BAD and one OK example, matching the `500-closing-angle-bracket` fixtures.
- Include one small implementation hint in the README about where predicate checks should be applied (token match vs production completion), since placement materially changes ambiguity outcomes.
- Add one focused supplemental test that isolates declaration-vs-expression ambiguity with template-like identifiers but without nested templates, to reduce debugging breadth.
