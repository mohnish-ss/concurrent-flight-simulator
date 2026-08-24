CC ?= cc
CPPFLAGS ?= -Iinclude -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -g
LDLIBS ?= -pthread

TARGET = flight-simulator
TEST_TARGET = test-alert-buffer
ASAN_TARGET = flight-simulator-asan
TSAN_TARGET = flight-simulator-tsan

COMMON_SRC = src/sync_lock.c src/alert_buffer.c
SIM_SRC = src/main.c src/simulation.c $(COMMON_SRC)
TEST_SRC = tests/test_alert_buffer.c $(COMMON_SRC)

$(TARGET): $(SIM_SRC) include/flight_sim.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SIM_SRC) -o $@ $(LDLIBS)

.PHONY: test asan tsan clean
test: $(TEST_SRC) include/flight_sim.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_SRC) -o $(TEST_TARGET) $(LDLIBS)
	./$(TEST_TARGET)

asan: $(SIM_SRC) include/flight_sim.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fno-omit-frame-pointer -fsanitize=address,undefined $(SIM_SRC) -o $(ASAN_TARGET) $(LDLIBS)
	./$(ASAN_TARGET)

tsan: $(SIM_SRC) include/flight_sim.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fno-omit-frame-pointer -fsanitize=thread $(SIM_SRC) -o $(TSAN_TARGET) $(LDLIBS)
	./$(TSAN_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET) $(ASAN_TARGET) $(TSAN_TARGET)
	rm -rf $(TARGET).dSYM $(TEST_TARGET).dSYM $(ASAN_TARGET).dSYM $(TSAN_TARGET).dSYM
