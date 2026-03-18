obj-m += device_interrupt.o
obj-m += device_dma.o

# Use available kernel headers (may not match running kernel)
KDIR := /lib/modules/6.19.8-arch1-1/build
PWD := $(shell pwd)

all: modules userspace

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

userspace:
	gcc -O3 -Wall -o bench_interrupt bench_interrupt.c
	gcc -O3 -Wall -o bench_dma bench_dma.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f bench_interrupt bench_dma

load_interrupt:
	sudo insmod device_interrupt.ko
	sudo chmod 666 /dev/dev_interrupt

unload_interrupt:
	sudo rmmod device_interrupt

load_dma:
	sudo insmod device_dma.ko
	sudo chmod 666 /dev/dev_dma

unload_dma:
	sudo rmmod device_dma

bench_interrupt: load_interrupt
	./bench_interrupt
	$(MAKE) unload_interrupt

bench_dma: load_dma
	./bench_dma
	$(MAKE) unload_dma

bench_all: all
	@echo "=========================================="
	@echo "Interrupt-Driven Device Benchmark"
	@echo "=========================================="
	-$(MAKE) bench_interrupt
	@echo ""
	@echo "=========================================="
	@echo "DMA with Polling Benchmark"
	@echo "=========================================="
	-$(MAKE) bench_dma
	@echo ""
	@echo "=========================================="
	@echo "Comparison Summary"
	@echo "=========================================="
	@echo "Check the output above to compare:"
	@echo "- Interrupt approach: Higher latency due to interrupts"
	@echo "- DMA polling: Lower latency, no interrupt overhead"

.PHONY: all modules userspace clean load_interrupt unload_interrupt load_dma unload_dma bench_interrupt bench_dma bench_all
