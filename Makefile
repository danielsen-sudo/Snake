CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
SODIUM_LIBS := $(shell pkg-config --libs libsodium 2>/dev/null || echo -Wl,-l:libsodium.so.23)
BUILD_DIR := build
PROGRAM := $(BUILD_DIR)/snake
GUI_PROGRAM := $(BUILD_DIR)/snake_gui
TEST_PROGRAM := $(BUILD_DIR)/snake_tests
GUI_TEST_PROGRAM := $(BUILD_DIR)/snake_gui_tests
SANITIZE_SCORE_TEST := $(BUILD_DIR)/snake_tests_sanitize
SANITIZE_GAME_TEST := $(BUILD_DIR)/snake_game_tests_sanitize
SANITIZE_FLAGS := -std=c11 -O1 -g -Wall -Wextra -Wpedantic \
	-fsanitize=address,undefined -fno-omit-frame-pointer
SDL3_PREFIX ?= $(CURDIR)/.deps/sdl3
SDL3_PKG_CONFIG := PKG_CONFIG_PATH="$(SDL3_PREFIX)/lib/pkgconfig" pkg-config
SDL3_CFLAGS := $(shell $(SDL3_PKG_CONFIG) --cflags sdl3 2>/dev/null)
SDL3_LIBS := $(shell $(SDL3_PKG_CONFIG) --libs sdl3 2>/dev/null)
SDL3_TTF_CFLAGS := $(shell $(SDL3_PKG_CONFIG) --cflags sdl3-ttf 2>/dev/null)
SDL3_TTF_LIBS := $(shell $(SDL3_PKG_CONFIG) --libs sdl3-ttf 2>/dev/null)
SDL3_RPATH := -Wl,-rpath,'$$ORIGIN/../.deps/sdl3/lib'

.PHONY: all clean gui run run-gui test test-gui test-sanitize package

all: $(PROGRAM)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

COMMON_SOURCES := src/snake.c src/game.c src/score_storage.c src/score_crypto.c

$(PROGRAM): src/snake_terminal.c $(COMMON_SOURCES) \
		include/game.h \
		include/scores.h include/snake_shared.h \
		include/sodium_compat.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) src/snake_terminal.c $(COMMON_SOURCES) \
		-o $(PROGRAM) $(SODIUM_LIBS)

gui: $(GUI_PROGRAM)

$(GUI_PROGRAM): src/snake_gui.c $(COMMON_SOURCES) \
		include/game.h \
		include/scores.h include/snake_shared.h \
		include/sodium_compat.h assets/fonts/NotoSans-Regular.ttf | $(BUILD_DIR)
	@if [ -z "$(SDL3_CFLAGS)" ]; then \
		echo "Feil: SDL3 ble ikke funnet i $(SDL3_PREFIX)."; exit 1; \
	fi
	@if [ -z "$(SDL3_TTF_CFLAGS)" ]; then \
		echo "Feil: SDL3_ttf ble ikke funnet i $(SDL3_PREFIX)."; exit 1; \
	fi
	$(CC) $(CFLAGS) $(SDL3_CFLAGS) $(SDL3_TTF_CFLAGS) \
		src/snake_gui.c $(COMMON_SOURCES) \
		-o $(GUI_PROGRAM) \
		$(SDL3_TTF_LIBS) $(SDL3_LIBS) $(SDL3_RPATH) $(SODIUM_LIBS)

test: $(PROGRAM) $(TEST_PROGRAM)
	./$(TEST_PROGRAM)
	@if strings $(PROGRAM) | grep -F 'CodeX' >/dev/null; then \
		echo "Feil: lesbart nøkkelmateriale funnet i programfilen"; exit 1; \
	else \
		echo "Ingen lesbar nøkkelstreng funnet i programfilen."; \
	fi

test-gui: $(GUI_PROGRAM) $(GUI_TEST_PROGRAM)
	SNAKE_DATA_DIR=$(BUILD_DIR)/test-data SDL_VIDEODRIVER=dummy \
		./$(GUI_PROGRAM) --smoke-test
	./$(GUI_TEST_PROGRAM)

$(GUI_TEST_PROGRAM): tests/gui_logic_tests.c $(COMMON_SOURCES) \
		include/game.h include/scores.h include/snake_shared.h \
		include/sodium_compat.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		tests/gui_logic_tests.c $(COMMON_SOURCES) \
		-o $(GUI_TEST_PROGRAM) $(SODIUM_LIBS)

test-sanitize: $(SANITIZE_SCORE_TEST) $(SANITIZE_GAME_TEST)
	ASAN_OPTIONS=detect_leaks=0 ./$(SANITIZE_SCORE_TEST)
	ASAN_OPTIONS=detect_leaks=0 ./$(SANITIZE_GAME_TEST)

$(SANITIZE_SCORE_TEST): tests/tests.c $(COMMON_SOURCES) | $(BUILD_DIR)
	$(CC) $(SANITIZE_FLAGS) tests/tests.c $(COMMON_SOURCES) \
		-o $(SANITIZE_SCORE_TEST) $(SODIUM_LIBS)

$(SANITIZE_GAME_TEST): tests/gui_logic_tests.c $(COMMON_SOURCES) | $(BUILD_DIR)
	$(CC) $(SANITIZE_FLAGS) tests/gui_logic_tests.c $(COMMON_SOURCES) \
		-o $(SANITIZE_GAME_TEST) $(SODIUM_LIBS)

$(TEST_PROGRAM): tests/tests.c $(COMMON_SOURCES) \
		include/game.h \
		include/scores.h include/sodium_compat.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/tests.c $(COMMON_SOURCES) \
		-o $(TEST_PROGRAM) $(SODIUM_LIBS)

run: $(PROGRAM)
	./$(PROGRAM)

run-gui: $(GUI_PROGRAM)
	./$(GUI_PROGRAM)

package: all gui
	./scripts/package-release.sh

clean:
	rm -f $(PROGRAM) $(GUI_PROGRAM) $(TEST_PROGRAM) $(GUI_TEST_PROGRAM) \
		$(SANITIZE_SCORE_TEST) $(SANITIZE_GAME_TEST)
