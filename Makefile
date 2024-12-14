
test:
	clib install --dev
	@$(CC) test.c -std=c11 -I src -I deps -I deps/greatest -o $@
	@./$@

.PHONY: test
