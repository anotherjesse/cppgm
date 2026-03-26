# PA2 Retrospective

## What went well
- Implementing the PA2 architecture by simply wrapping and feeding the token stream extracted from `pa1/pptoken.cpp` proved to be highly effective. This architectural choice isolated the string concatenation and Phase 7 conversion logic from the raw character lexing.
- The use of regex (`std::regex`) for isolating `pp-number` floating-literals worked exceptionally well, given the regular structure of floats compared to the complexity of a manual scan, allowing a clean separation of integers and floats.

## What went poorly
- Distinguishing user-defined-literal identifiers correctly revealed subtle constraints about `pp-number` tokenization. Tokens like `1_e+1` are valid `pp-number`s in PA1, but as Phase 7 tokens, the user-defined suffix `_e+1` is invalid since identifiers cannot contain `+`, `-`, or `.`. Hand-rolling validations to exclude those operators from the extracted ud-suffix required patching the integer/float validation routines after encountering failing course cases.
- The string concatenation phase required extra handling of UTF-8 vs raw char extraction and carefully verifying prefixes. Replicating the decode-encode cycle to enforce standard-compliant Unicode code point boundaries involved complex shift arithmetic that is error-prone.

## Suggestions for improving the assignment
- Clarifying whether `invalid` tokens emitted during concatenation should output their combined raw sources or if there are any formatting limits on error outputs would help reduce guesswork.
- Explicitly reminding students that `pp-number` can aggregate characters that are illegal in Phase 7 `ud-suffix` identifiers (e.g. `+`, `-`, `.`) would prevent a common trap where `1.0_e+1` gets falsely recognized as a user-defined literal.