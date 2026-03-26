# PA3 Retro

What went well:
- Reusing the PA2 tokenizer and literal parsers kept the implementation focused on the controlling-expression grammar instead of redoing lexical work.
- The checked-in tests were good at pinning down assignment-specific behavior, especially around `defined`, mixed signed/unsigned arithmetic, and short-circuiting.

What went poorly:
- The starter layout made it awkward to reuse `posttoken.cpp` directly from `ctrlexpr.cpp`, so I had to add a small hook to rename the PA2 entrypoint.
- `300-triple` was slow enough in the default debug build that it hit the harness timeout until I optimized the shared build.

Suggestions:
- Expose a documented reuse hook for the shared PA2 parser/literal code so PA3 can include it without touching the entrypoint logic.
- Consider building the shared tools with optimization enabled by default, or call out that PA3 has a large throughput-sensitive test.
- A short note in the assignment handout about the intended signed/unsigned promotion rules would reduce the time spent reverse-engineering the checked-in fixtures.
