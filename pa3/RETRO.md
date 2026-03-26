# PA3 Retrospective

## What went well
- Building the recursive descent parser by converting `posttoken` token classifications into `Value` evaluating AST nodes was straightforward.
- Adopting a simplified evaluation pipeline that evaluates on the fly without full AST allocations (`LineTokenStream`) allowed us to optimize memory usage perfectly.
- Avoiding `std::regex` compilation per number evaluation and just utilizing the existing `parse_integer` significantly optimized parsing time.

## What went poorly
- Due to the massive number of expressions generated in tests like `300-triple.t`, performance scaling was initially catastrophic. Over-allocating `std::vector<PPToken>` across the entire file and instantiating too many virtual stream class objects was inefficient. Refactoring `PPTokenizer` to bypass vectorization memory pressure and passing `-O3` resolved the timeout issues.
- Missing `char16_t` overflow assertions (`u` prefixed strings bounded by `0xFFFF`) required patching after course-provided tests flagged deviations.

## Suggestions for improving the assignment
- Given that the evaluation can take dozens of seconds under `O0` for the half-million lines `300-triple.t` test, consider recommending optimizations explicitly like bypassing full-file vectorization in favor of streaming to students. 
- Explicitly emphasize the `char16_t` `0xFFFF` constraint and how the `long long int` MIN / -1 division overflow is to be reported as an `error`.