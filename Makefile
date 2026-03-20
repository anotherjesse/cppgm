PAS = $(patsubst %/Makefile,%,$(wildcard pa*/Makefile))
TEST_THROUGH_TARGETS = $(addprefix test-through-,$(PAS))

.PHONY: all test $(PAS) $(TEST_THROUGH_TARGETS)

all: $(PAS)

$(PAS):
	$(MAKE) -C $@

test:
	@for dir in $$(printf '%s\n' $(PAS) | sort -t a -k 2,2n); do \
		echo "========================================"; \
		echo "Building and testing $$dir..."; \
		echo "========================================"; \
		$(MAKE) -C $$dir test || exit 1; \
	done
	@echo "========================================"
	@echo "ALL TESTS PASSED SUCCESSFULLY!"
	@echo "========================================"

$(TEST_THROUGH_TARGETS):
	@target=$(@:test-through-%=%); \
	for dir in $$(printf '%s\n' $(PAS) | sort -t a -k 2,2n); do \
		echo "========================================"; \
		echo "Building and testing $$dir..."; \
		echo "========================================"; \
		$(MAKE) -C $$dir test || exit 1; \
		if [ "$$dir" = "$$target" ]; then break; fi; \
	done
	@echo "========================================"
	@echo "ALL TESTS PASSED SUCCESSFULLY!"
	@echo "========================================"
