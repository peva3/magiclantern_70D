.PHONY: all clean 70D 70D.clean qemu-build modules test help

all: 70D

70D:
	$(MAKE) -C platform/70D.112 -j$$(nproc)

70D.clean:
	$(MAKE) -C platform/70D.112 clean

qemu-build:
	$(MAKE) -C qemu-eos/build -j$$(nproc)

modules:
	$(MAKE) -C modules

test:
	./test_70d_qemu.sh --boot --no-build --timeout 30

help:
	@echo "Targets:"
	@echo "  all / 70D     - Build Magic Lantern for 70D"
	@echo "  70D.clean     - Clean 70D build artifacts"
	@echo "  qemu-build    - Build QEMU-EOS emulator"
	@echo "  modules       - Build all ML modules"
	@echo "  test          - Quick QEMU boot test"
	@echo ""
	@echo "For QEMU builds: make 70D CONFIG_QEMU=y"
	@echo "For parallel builds: make -j$$(nproc)"
