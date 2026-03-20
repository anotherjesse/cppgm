# Project Layout and Development Workflow

This project is a series of assignments (PA1 to PA9) aimed at building a self-hosting C++11 compiler for Linux x86_64.

## Project Structure

- `pa1/`: Programming Assignment 1 - Preprocessing Tokenizer (`pptoken`)
- `pa2/`: Programming Assignment 2 - Post-Tokenizer (`posttoken`)
- `pa3/`: Programming Assignment 3 - Constant Expressions (`ctrlexpr`)
- `pa4/`: Programming Assignment 4 - Macro Processor (`macro`)
- `pa5/`: Programming Assignment 5 - Preprocessor (`preproc`)
- `pa6/`: Programming Assignment 6 - C++ Grammar Recognizer (`recog`)
- `pa7/`: Programming Assignment 7 - Namespace Declarations (`nsdecl`)
- `pa8/`: Programming Assignment 8 - Namespace Initialization (`nsinit`)
- `pa9/`: Programming Assignment 9 - Code Generation (`cy86`)
- `doc/`: Documentation and reference materials.

## Grammar Files and Extras

Assignments PA6 through PA9 include grammar files (`.gram`) and sometimes additional resources in `grammar/` or `extras/` directories. These are provided as part of the assignment specification and should be referenced during implementation.

## Development Workflow

To facilitate code reuse and maintain a single source of truth for the compiler's implementation, the `dev/` directory is structured to isolate assignment entry points from shared logic.

- `dev/assignments/`: Contains the `main` entry point for each assignment (e.g., `pptoken.cpp`, `posttoken.cpp`).
- `dev/`: The root of the `dev` directory should contain all **shared** compiler components (e.g., `tokenizer.cpp`, `parser.cpp`).

### Working on an Assignment

1. **Implement Features**: 
   - Modify the corresponding entry point in `dev/assignments/`.
   - Add new shared components directly into `dev/`.
2. **Automatic Shared Code Discovery**:
   - The Makefiles automatically find **all** `.cpp` files in the root of `dev/` and link them into your executable.
   - There is no need to update the Makefiles when you add new shared `.cpp` files to `dev/`.
   - The compiled objects are stored in a shared `obj/` directory for fast builds across assignments.
3. **Automatic Dependency Tracking**: 
   - Header changes (`.h` files) are automatically detected, ensuring that only the necessary source files are recompiled when you modify an interface.
4. **Build and Verify**:
   From within the specific assignment directory (e.g., `pa1/`):
   ```bash
   make       # Build the application
   make test  # Run the test suite
   ```
   Each assignment directory also contains a committed `course` symlink to
   `../tests/course`, so supplemental shared tests are available as
   `course/paN/` from inside `paN/`.

### Regression Testing

As you progress through the assignments, it is crucial to ensure that changes
made for a new assignment do not break previous ones.

- After completing `paN`, run `make test-through-paN` from the **project
  root** to build and test all assignments from `pa1` through `paN`.
- Example: after finishing `pa5`, run `make test-through-pa5`.
- `make test` from the **project root** still runs the full suite (`pa1`
  through `pa9`) and is useful for final verification.
- Treat this regression run as part of finishing the assignment, not as an
  optional cleanup step.

### Version Control (Git)

It is highly recommended to track your progress using Git. Committing
frequently lets you refactor shared code in `dev/` with less risk and makes it
much easier to recover from regressions.

**Commit Strategy**: 
- Commit after successfully implementing a new feature or passing a new test case.
- Always commit before starting the next assignment (`pa(N+1)`), ensuring your
  workspace is clean and `make test-through-paN` passes from the root.
- Example: `git commit -am "pa1: implement trigraph replacement"`

### Assignment Retrospectives

After completing each assignment, add a `RETRO.md` file in the corresponding
`paN/` directory and commit it with the assignment work or immediately after.
Each retrospective should cover:

- What went well.
- What went poorly.
- Suggestions for improving the assignment, starter code, tests, or reference material.

### Suggestions for Success

- Reuse and extend code from previous assignments instead of starting each
  assignment from scratch. The project is designed to build cumulatively.
- Include extra context in error cases when practical, especially filenames,
  line numbers, and the specific condition that failed. Better diagnostics make
  debugging and regression triage much easier.
- Periodically simplify, reorganize, and refactor shared code as the project
  grows so the implementation stays manageable across later assignments.
- Follow the checked-in tests closely. In this project, passing the provided
  tests is often a better guide than trying to implement a broader or newer
  interpretation of the language.
- Do not implement features "ahead" of the assignment if doing so changes
  behavior that the current tests expect.
- If a conflict between the tests, reference behavior, and the intended design
  truly cannot be resolved cleanly, choose the path that keeps the assignment
  moving forward and document the conflict in `RETRO.md`.

## Tools and References

- **Reference Implementations**: Each `paN` directory contains a compiled
  reference binary (e.g., `pptoken-ref`). Use it to inspect expected behavior
  and to regenerate assignment-local fixtures in `paN/tests/` via
  `make ref-test`.
- **Local Test Suites**: The original assignment test suites live in
  `paN/tests/`. They include input files (`.t`), reference outputs (`.ref`),
  and exit statuses. `make ref-test` only regenerates these local fixtures.
- **Shared Supplemental Tests**: Additional shared tests live in
  `tests/course/paN/` and are reachable from inside each assignment directory
  as `course/paN/`. `make test` runs both `paN/tests/` and `course/paN/`.
- **Where to Add New Shared Tests**: If you add new user-authored or
  supplemental tests, put them in `tests/course/paN/` rather than
  `paN/tests/`.
- **Test Authority**: Prefer the checked-in tests for the current assignment
  over speculative improvements, broader feature support, or newer-standard
  behavior when those would cause regressions.
- **Authoring Rule for Shared Tests**: Do not bulk-regenerate the checked-in
  `.ref` files under `tests/course/paN/` from the local `*-ref` binaries.
  Those imported fixtures are authoritative and may intentionally cover edge
  cases where the reference binaries are not the oracle. Update them only with
  an intentional content change.
- **C++ Standard**: The project targets the C++11 standard (N3485), available in the `doc/` directory.
