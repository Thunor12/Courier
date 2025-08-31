# Simple build with tests for Courier v0.1
# Requires: gcc, pthreads, POSIX mqueues (usually link with -lrt on older glibc)

CC ?= gcc
CFLAGS ?= -Os -Wall -Wextra -Wpedantic -Werror -std=c11
LDFLAGS ?= -Os -fpic -pthread -lrt

INCDIR := include
SRCDIR := src
TESTDIR := tests
EXAMPLEDIR := examples
BUILD := build

LIBOBJS := $(BUILD)/courier.o
LIBA := $(BUILD)/courier.a

TESTS := \
  $(BUILD)/test_queue_basic \
  $(BUILD)/test_actor_basic

EXAMPLES := \
  $(BUILD)/example_thermostat

.PHONY: all clean test

all: $(TESTS) $(EXAMPLES) $(LIBA)

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/courier.o: $(SRCDIR)/courier.c $(INCDIR)/courier.h | $(BUILD)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(LIBA): $(LIBOBJS) | $(BUILD)
	ar rcs $@ $^

$(BUILD)/test_queue_basic: $(TESTDIR)/test_queue_basic.c $(LIBOBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) $^ -o $@ $(LDFLAGS)

$(BUILD)/test_actor_basic: $(TESTDIR)/test_actor_basic.c $(LIBOBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) $^ -o $@ $(LDFLAGS)

$(BUILD)/example_thermostat: $(EXAMPLEDIR)/example_thermostat.c $(LIBOBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD)

# Run both tests
test: all
	@echo "Running test_queue_basic..." && $(BUILD)/test_queue_basic
	@echo "Running test_actor_basic..." && $(BUILD)/test_actor_basic