CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
SODIUM_LIBS := $(shell pkg-config --libs libsodium 2>/dev/null || echo -Wl,-l:libsodium.so.23)
BUILD_DIR := build
PROGRAM := $(BUILD_DIR)/snake
TEST_PROGRAM := $(BUILD_DIR)/snake_tests

.PHONY: all clean run test

all: $(PROGRAM)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(PROGRAM): src/snake.c include/sodium_compat.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/snake.c -o $(PROGRAM) $(SODIUM_LIBS)

test: $(PROGRAM) $(TEST_PROGRAM)
	./$(TEST_PROGRAM)
	@if strings $(PROGRAM) | grep -F 'CodeX' >/dev/null; then \
		echo "Feil: lesbart nøkkelmateriale funnet i programfilen"; exit 1; \
	else \
		echo "Ingen lesbar nøkkelstreng funnet i programfilen."; \
	fi

$(TEST_PROGRAM): tests/tests.c src/snake.c include/sodium_compat.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/tests.c -o $(TEST_PROGRAM) $(SODIUM_LIBS)

run: $(PROGRAM)
	./$(PROGRAM)

clean:
	rm -f $(PROGRAM) $(TEST_PROGRAM)
