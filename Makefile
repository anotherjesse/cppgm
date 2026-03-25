# macOS ships with an ancient version of GNU Make (3.81).
# If the user has installed a modern GNU Make via Homebrew,
# this overrides the default `make` command to use it instead.
# Includes both Intel (/usr/local) and Apple Silicon (/opt/homebrew) paths.
GNUMAKE = $(firstword $(wildcard /opt/homebrew/opt/make/libexec/gnubin/make /usr/local/opt/make/libexec/gnubin/make))
ifneq ($(GNUMAKE),)
MAKE := $(GNUMAKE)
endif
MAKEFLAGS += --no-print-directory

PAS = $(patsubst %/Makefile,%,$(wildcard pa*/Makefile))
SORTED_PAS = $(shell printf '%s\n' $(PAS) | sort -t a -k 2,2n)
DEV_BUILD_LOCK = obj/.dev-build.lock

.PHONY: all build test test-report $(PAS)

all: build

build:
	@mkdir -p obj
	@lockdir=$(DEV_BUILD_LOCK); \
	while ! mkdir $$lockdir 2>/dev/null; do sleep 1; done; \
	trap 'rmdir "$$lockdir"' EXIT HUP INT TERM; \
	$(MAKE) -s -C dev

test: build
	@for dir in $(SORTED_PAS); do \
		echo "===== $$dir ====="; \
		$(MAKE) -C $$dir CPGM_SKIP_DEV_REBUILD=1 test || exit 1; \
	done
	@echo "===== ALL TESTS PASSED SUCCESSFULLY! ====="

test-report: build
	@export KEEP_GOING=1; \
	tmpdir=$$(mktemp -d); \
	trap "rm -f pa*/.test_failed .test_counts; rm -rf $$tmpdir" EXIT INT TERM; \
	rm -f pa*/.test_failed .test_counts; \
	for dir in $(SORTED_PAS); do \
		( \
			echo "===== $$dir =====" > "$$tmpdir/$$dir.out"; \
			$(MAKE) -C $$dir CPGM_SKIP_DEV_REBUILD=1 test >> "$$tmpdir/$$dir.out" 2>&1 \
		) & \
	done; \
	wait; \
	for dir in $(SORTED_PAS); do \
		cat "$$tmpdir/$$dir.out"; \
	done; \
	passed=$$(awk '{s+=$$1} END {print s}' .test_counts 2>/dev/null || echo 0); \
	total=$$(awk '{s+=$$2} END {print s}' .test_counts 2>/dev/null || echo 0); \
	if ls pa*/.test_failed 1> /dev/null 2>&1; then \
		echo "===== TEST SUMMARY: $$passed / $$total TESTS PASSED ====="; \
		exit 1; \
	else \
		echo "===== ALL TESTS PASSED SUCCESSFULLY! ($$passed / $$total) ====="; \
	fi

$(PAS):
	$(MAKE) build
	$(MAKE) -C $@ CPGM_SKIP_DEV_REBUILD=1 test
