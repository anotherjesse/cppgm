## PA9 Retro

### What Went Well

- Reusing the PA5 preprocessor and PA2 literal parsing kept the CY86 front end small and consistent with the earlier assignments.
- A flat encoded CY86 image plus an embedded native runtime avoided a much larger direct x86 code generator while still matching the checked-in execution tests.
- The checked-in PA9 programs exercised the important semantic corners quickly, especially stringized literals and unsigned 64-bit float conversions.

### What Went Poorly

- The starter stub leaves a large design gap between "write ELF" and "execute the full CY86 language", so backend direction had to be chosen before much code could be reused.
- Float correctness debugging was expensive because failures only showed up after generating and running a second executable.
- CY86’s use of PA2 literal encodings in non-obvious places, like truncated stringized macro literals in integer comparisons, was easy to over-restrict at first.

### Suggestions

- Add a small checked-in unit test set for literal-to-immediate truncation and extension rules independently of the large calculator fixtures.
- Document more explicitly that PA9 label values are virtual addresses and that PA2 literal encodings are used directly for immediates and data.
- Provide one reference note or starter helper for the runtime/backend boundary so students do not have to rediscover the entire execution strategy from scratch.
