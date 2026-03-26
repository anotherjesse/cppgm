# PA4 Retrospective

## What Went Well

- Reused PA1/PA2 tokenization and posttoken emission directly by embedding `posttoken.cpp`, which kept output behavior aligned with checked-in fixtures.
- Implemented directive parsing and expansion in `dev/macro.cpp` incrementally against failing tests, which made edge-case fixes (`#`, `##`, varargs, recursion suppression) straightforward to validate.
- Added a token-level expansion model with blacklist/non-invokable state that matched the course recursion semantics used by PA4 tests.

## What Went Poorly

- Initial implementation dropped replacement-list whitespace too aggressively, which broke stringizing details and some token-paste expectations.
- `##` handling required multiple iterations to distinguish replacement-list concatenation from literal `##` tokens passed through macro arguments.
- Early argument expansion for all parameters caused a false error in stringized-argument cases; expansion needed to be lazy.

## Suggestions

- Add a short required section in the assignment text clarifying whitespace preservation expectations specifically for `#` stringizing and `##` around parameters.
- Include one canonical pseudo-code expansion order for: parse invocation, decide raw vs expanded arguments per parameter use, apply `#`/`##`, then rescan.
- Add one focused supplemental test that contrasts `##` inside replacement lists vs `##` appearing only inside actual macro arguments.
