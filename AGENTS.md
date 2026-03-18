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

### Regression Testing

As you progress through the assignments, it is crucial to ensure that changes made for a new assignment do not break previous ones. After making significant changes in `dev/`, you can run `make test` from the **project root** to automatically build and test all assignments (`pa1` through `pa9`) sequentially using the top-level `Makefile`.

### Version Control (Git)

It is highly recommended to track your progress using Git. Committing frequently allows you to confidently refactor shared code in `dev/` and easily roll back if a change breaks previous assignments.

**Commit Strategy**: 
- Commit after successfully implementing a new feature or passing a new test case.
- Always commit before starting the next assignment (`pa(N+1)`), ensuring your workspace is clean and `make test` passes from the root.
- Example: `git commit -am "pa1: implement trigraph replacement"`

## Tools and References

- **Reference Implementations**: Each `paN` directory contains a compiled reference binary (e.g., `pptoken-ref`). You can use this binary to see the expected correct behavior or to generate the correct `.ref` and `.ref.exit_status` outputs for any newly created test cases.
- **Test Suites**: The original test suites are located in `paN/tests/`. They include input files (`.t`), reference outputs (`.ref`), and exit statuses.
- **Extra User-Supplied Tests**: Additional tests are located in `cppgm.tests/course/paN/`. **If you need to add your own new tests, you should add them to the `cppgm.tests/course/paN/` directory** rather than the original `paN/tests/` directory. The `make test` command is configured to automatically run both.
- **C++ Standard**: The project targets the C++11 standard (N3485), available in the `doc/` directory.
