# Host-side test Makefile for Magic Lantern 70D development
# This allows testing logic without ARM hardware

CC = gcc
CFLAGS = -Wall -Wextra -g -DCONFIG_70D -DCONFIG_70D_112 -I../src -I../platform/70D.112/include -Ihost-stubs
LDFLAGS =

TESTS = test_focus

all: $(TESTS)

test_focus: test_focus.c host-stubs/host_stubs.h
	$(CC) $(CFLAGS) -o $@ $<

# Check all built modules have resolvable symbols (catches TCC load errors)
.PHONY: check-symbols
check-symbols:
	@echo "=== Checking module symbols ==="
	@SYM=../platform/70D.112/build/70D_112.sym; \
	if [ ! -f "$$SYM" ]; then echo "  ERROR: Build the firmware first"; exit 1; fi; \
	all_ok=1; \
	mo_dir=../modules/build; \
	included=$$(sort ../platform/70D.112/modules.included | grep -v '^#' | grep -v '^$$'); \
	all_mod_syms=$$(for mo in $$mo_dir/*.mo; do \
		arm-none-eabi-readelf -sW "$$mo" 2>/dev/null | awk '$$5=="GLOBAL" && $$7!="UND" && $$7!="ABS" {print $$8}'; \
	done | sort -u); \
	for mo in $$mo_dir/*.mo; do \
		name=$$(basename $$mo .mo); \
		if ! echo "$$included" | grep -qw "$$name"; then continue; fi; \
		name=$$(basename $$mo .mo); \
		undef=$$(arm-none-eabi-readelf -sW "$$mo" 2>/dev/null | awk '{if($$7=="UND") print $$8}' | grep -v '^__module_' | sort -u); \
		if [ -z "$$undef" ]; then continue; fi; \
		missing=""; \
		for sym in $$undef; do \
			if ! grep -qw "$$sym" "$$SYM"; then \
				if ! echo "$$all_mod_syms" | grep -qw "$$sym"; then \
					case "$$sym" in init_log|send_log_data_str) ;; \
					*) missing="$$missing $$sym" ;; esac; \
				fi; \
			fi; \
		done; \
		if [ -n "$$missing" ]; then \
			echo "  FAIL: $$name — unresolved:$$missing"; \
			all_ok=0; \
		else \
			echo "  OK:   $$name"; \
		fi; \
	done; \
	if [ "$$all_ok" = "1" ]; then echo "  All module symbols OK"; \
	else echo "  Some modules have unresolved symbols!"; exit 1; fi

clean:
	rm -f $(TESTS) *.o

run: all check-symbols
	./test_focus

.PHONY: all clean run check-symbols