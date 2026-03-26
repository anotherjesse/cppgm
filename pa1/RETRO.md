# PA1 Retrospective

## What Went Well

- Built a complete phase 1-3 tokenizer pipeline in `dev/pptoken.cpp` that now passes all checked-in PA1 tests.
- Reused shared identifier range tables from the starter to keep Unicode identifier behavior aligned with assignment expectations.
- Iterated directly against both assignment-local and course supplemental fixtures, which exposed edge cases early (raw strings, include/header context, and escape handling).

## What Went Poorly

- Comment replacement was initially applied too early and affected text inside literals; this caused regressions in mixed comment/string stress tests.
- Raw string detection originally over-eagerly treated `R"` substrings inside other literals as raw-string starts, which required a second pass to constrain context.
- Escape-sequence validation was initially too permissive and had to be tightened to match supplemental error expectations.

## Suggestions

- Add explicit starter guidance for operation ordering around trigraphs, raw strings, comment stripping, and line splicing; most subtle bugs came from ordering ambiguities.
- Include a small mandatory checklist of tricky PA1 corner cases (e.g., `%:` include handling, `u8` with character literals, and invalid escape diagnostics).
- Provide one concise state-machine sketch in the README for phases/tokens interaction to reduce first-implementation churn.
