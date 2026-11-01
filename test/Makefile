# Unit tests that run on the PC (no hardware needed). Usage: make -C test
CC      ?= gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -g -I../Core/Inc -Iunity

# AddressSanitizer and UBSan are not available in the Windows (MinGW) gcc.
ifneq ($(OS),Windows_NT)
CFLAGS += -fsanitize=address,undefined -fno-sanitize-recover=undefined
endif

BUILD = build
TESTS = test_sensor_math test_watchdog_core

all: run

$(BUILD)/test_sensor_math: test_sensor_math.c ../Core/Src/sensor_math.c unity/unity.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/test_watchdog_core: test_watchdog_core.c ../Core/Src/watchdog_core.c unity/unity.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

run: $(addprefix $(BUILD)/,$(TESTS))
	@set -e; for t in $(TESTS); do echo "== $$t"; ./$(BUILD)/$$t; done

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
