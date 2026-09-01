all:
	@gcc src/main.c -o src/a.out
	@./src/a.out
	@rm src/a.out
