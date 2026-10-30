# Unit tests that run on the PC (no hardware needed). Usage: make -C test
CC      ?= gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -g -I../Core/Inc -Iunity

# AddressSanitizer and UBSan are not available in the Windows (MinGW) gcc.
ifneq ($(OS),Windows_NT)
CFLAGS += -fsanitize=address,undefined -fno-sanitize-recover=undefined
endif

BUILD = build

all: run

$(BUILD)/test_sensor_math: test_sensor_math.c ../Core/Src/sensor_math.c unity/unity.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^

run: $(BUILD)/test_sensor_math
	./$(BUILD)/test_sensor_math

clean:
	rm -rf $(BUILD)

.PHONY: all run clean