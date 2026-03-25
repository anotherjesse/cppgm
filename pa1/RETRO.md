## What Went Well

- Building the tokenizer around the checked-in fixtures gave fast feedback on course-specific behavior such as header-name recognition, raw-string exceptions, and identifier handling for Unicode.
- Keeping the implementation in `dev/pptoken.cpp` preserved the expected shared-code layout for later assignments.

## What Went Poorly

- The starter repository was still at the stub stage, so there was no existing lexical infrastructure to extend.
- A few behaviors that differ from a naive standard reading, especially around `u8` character literals and raw-string/trigraph interaction, required multiple rounds against the supplemental course tests.

## Suggestions

- Add an explicit note in the PA1 starter about which literal-prefix combinations are intentionally accepted or rejected by the checked-in tests.
- Call out raw-string interaction with translation phases more directly in the assignment notes, since it drives several non-obvious edge cases.
