# PA3 Retrospective

## What went well
- Reusing the PA1 tokenizer pipeline made phase 1-3 handling straightforward and kept UTF-8/trigraph/splice behavior consistent with earlier assignments.
- A direct recursive-descent parser mapped cleanly to the PA3 grammar, so precedence and associativity bugs were easy to avoid.
- Separating parsing/type propagation from evaluation made short-circuit rules for `&&`, `||`, and `?:` easier to implement correctly.

## What went poorly
- Token classification around alternative operator spellings (`not`, `and`, `bitand`, etc.) was initially incomplete because they can arrive as `preprocessing-op-or-punc` tokens.
- Integer and character literal handling required pulling in a non-trivial amount of PA2 logic, which increased implementation size for PA3.

## Suggestions
- Provide a small shared library/header for PA2 literal decoding so PA3 does not need to duplicate literal-parsing code.
- Add one explicit starter test that covers alternative operator spellings emitted as `preprocessing-op-or-punc` to catch that integration issue earlier.
- Consider adding a short PA3 starter note that recommends modeling values as `{signed/unsigned, 64-bit bits}` to avoid accidental signed-overflow pitfalls.
