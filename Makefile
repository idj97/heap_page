init_bin_dir:
	@if [ ! -d "./bin" ]; then mkdir bin; fi

clean:
	rm -rf bin

test_build: init_bin_dir
	@gcc \
		-g \
		-Wextra \
		-fsanitize=address \
		-DTEST_MODE \
		-o ./bin/test \
		src/page.c \
		src/test.c \
		lib/unity/unity.c

test: test_build
	./bin/test

coverage:
	@gcc \
		-g \
		-Wextra \
		--coverage \
		--dumpbase '' \
		-DTEST_MODE \
		-o ./bin/test \
		src/page.c \
		src/test.c \
		lib/unity/unity.c
	./bin/test
	cd bin && gcov -o . ../src/page.c

coverage_report:
	cd bin && lcov --capture --directory . --output-file .coverage.info
	genhtml bin/.coverage.info --output-directory bin/out

valgrind: test_build
	@gcc \
		-g \
		-Wextra \
		-DTEST_MODE \
		-o ./bin/valgrind_test \
		src/page.c \
		src/test.c \
		lib/unity/unity.c
	valgrind --track-origins=yes --leak-check=full ./bin/valgrind_test
