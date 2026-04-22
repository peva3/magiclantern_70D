# Host-side test Makefile for Magic Lantern 70D development
# This allows testing logic without ARM hardware

CC = gcc
CFLAGS = -Wall -Wextra -g -DCONFIG_70D -DCONFIG_70D_112 -I../src -I../platform/70D.112/include -Ihost-stubs
LDFLAGS =

TESTS = test_focus

all: $(TESTS)

test_focus: test_focus.c host-stubs/host_stubs.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TESTS) *.o

run: all
	./test_focus

.PHONY: all clean run