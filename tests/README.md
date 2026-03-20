cppgm.tests
===========

This directory is imported from:

https://github.com/danilchap/cppgm.tests

Imported upstream commit:

- `f6a9b5df921da768c98fff18f634b81f9e30e093`

Repository-specific changes on top of the imported subtree:

- The canonical location in this repo is `tests/` rather than the legacy
  `cppgm.tests/` path.
- Assignment `Makefile`s look for supplemental course tests under
  `../tests/course/paN`.
- Historical helper patch files that are no longer needed after merging their
  changes locally were removed from this copy.

Structure:

- `course/paN/` contains supplemental course tests keyed by assignment.
- `undefined/` contains tests for undefined or implementation-defined behavior.

This subtree only covers the shared supplemental test corpus. Assignment-local
fixtures and generated reference artifacts remain under each `paN/tests/`
directory in the main repository.
