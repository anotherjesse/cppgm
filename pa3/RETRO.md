## What Went Well

- Reusing the PA2 tokenizer and literal-decoding code inside `ctrlexpr` avoided reimplementing the lexical pipeline again.
- The checked-in PA3 tests clearly drove the tricky parts: `defined`, short-circuit evaluation, ternary result typing, and integer-overflow edge cases.

## What Went Poorly

- The PA3 starter was still empty, so even with reuse from PA2 the evaluator still needed a fresh parser and expression engine.
- Signed and unsigned behavior in non-evaluated branches was easy to get subtly wrong without the course tests for conditional result typing.

## Suggestions

- Add a small note in the PA3 README that unevaluated conditional branches still contribute to the static result type.
- Mention the `INT64_MIN / -1` and `INT64_MIN % -1` overflow trap explicitly alongside divide-by-zero as a common implementation hazard.
