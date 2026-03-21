# agent-cppgm

This repository is a cleaned and consolidated version of the original C++
Grandmaster Challenge, `cppgm`, a staged course for building a C++11 compiler.
The original site is no longer live, but it is preserved on the Internet
Archive at
`https://web.archive.org/web/20221225011253/http://www.cppgm.org/index.html`.
The broader Wayback capture for `cppgm.org` is at
`https://web.archive.org/web/*/http://www.cppgm.org/*`.

This repository contains the original `PA1` through `PA9` sequence and the
community tests from https://github.com/danilchap/cppgm.tests, packaged in a
form that is easier to use with modern coding agents. The starter kits are
collected into one repository, assignment entrypoints live in `dev/`, shared
implementation lives in `dev/src/`, supplemental tests live under `tests/`, and
the expected workflow is documented in `AGENTS.md`. In my experimentation,
current coding agents have been able to progress through all nine lessons
autonomously in yolo mode when given the repository, `AGENTS.md`, a suitable
execution environment, and no more guidance than being told to continue.

This is intended to be run inside an isolated sandbox on Linux x86_64. For best
results, use a tool-rich environment with `bash`, `make`, `git`, a working C++
compiler, binutils, Perl, Python, `diff`, `grep`, `sed`, `awk`, `xxd`,
`objdump`, and debugging tools such as `gdb` and `strace`. **Do not run an
autonomous agent on an unsandboxed workstation.**

The simplest prompt is to tell the agent to follow `AGENTS.md`, continue the
assignment sequence in order, reuse prior code, follow the checked-in tests,
update `RETRO.md` after each assignment, and keep going until the full suite
passes from the repository root.

```text
Read AGENTS.md and follow it exactly. Starting from the current repository
state, continue the PA1 through PA9 assignment sequence in order. Reuse and
extend existing code instead of starting over. Follow the checked-in tests and
assignment instructions, even when they differ from newer-standard behavior.
After each completed assignment, update paN/RETRO.md and keep going. Stop only
when the repository root make test passes.
```

From the repository root, a few workable starting points are:

```bash
gemini -s -i "Read AGENTS.md and follow it exactly. Starting from the current repository state, continue the PA1 through PA9 assignment sequence in order. Reuse and extend existing code instead of starting over. Follow the checked-in tests and assignment instructions, even when they differ from newer-standard behavior. After each completed assignment, update paN/RETRO.md and keep going. Stop only when the repository root make test passes."
```

```bash
claude --permission-mode acceptEdits "Read AGENTS.md and follow it exactly. Starting from the current repository state, continue the PA1 through PA9 assignment sequence in order. Reuse and extend existing code instead of starting over. Follow the checked-in tests and assignment instructions, even when they differ from newer-standard behavior. After each completed assignment, update paN/RETRO.md and keep going. Stop only when the repository root make test passes."
```

```bash
codex --full-auto "Read AGENTS.md and follow it exactly. Starting from the current repository state, continue the PA1 through PA9 assignment sequence in order. Reuse and extend existing code instead of starting over. Follow the checked-in tests and assignment instructions, even when they differ from newer-standard behavior. After each completed assignment, update paN/RETRO.md and keep going. Stop only when the repository root make test passes."
```
