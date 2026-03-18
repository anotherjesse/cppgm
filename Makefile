PAS = pa1 pa2 pa3 pa4 pa5 pa6 pa7 pa8 pa9

.PHONY: all test $(PAS)

all: $(PAS)

$(PAS):
	$(MAKE) -C $@

test:
	@for dir in $(PAS); do \
		echo "========================================"; \
		echo "Building and testing $$dir..."; \
		echo "========================================"; \
		$(MAKE) -C $$dir test || exit 1; \
	done
	@echo "========================================"
	@echo "ALL TESTS PASSED SUCCESSFULLY!"
	@echo "========================================"
