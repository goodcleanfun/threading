
install:
	clib install --dev

test:
	@$(CC) test.c -std=c11 -I src -I deps -I deps/greatest -o $@
	@./$@

.PHONY: install test
