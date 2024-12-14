
install:
	clib install --dev

test:
	@$(CC) test.c -I src -I deps -I deps/greatest -o $@
	@./$@

.PHONY: install test
