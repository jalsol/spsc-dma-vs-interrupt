obj-m += device_interrupt.o
obj-m += device_dma.o

# Use available kernel headers (module version mismatch handled with -f flag)
KDIR := /lib/modules/6.19.8-arch1-1/build
PWD := $(shell pwd)

all: modules bench

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

bench:
	gcc -O3 -Wall -o bench_interrupt bench_interrupt.c
	gcc -O3 -Wall -o bench_dma bench_dma.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f bench_interrupt bench_dma

load:
	sudo insmod -f device_interrupt.ko
	sudo insmod -f device_dma.ko
	sudo chmod 666 /dev/dev_interrupt /dev/dev_dma

unload:
	-sudo rmmod device_interrupt 2>/dev/null || true
	-sudo rmmod device_dma 2>/dev/null || true

run: 
	@echo "=== Cleaning up old modules ==="
	-sudo rmmod device_interrupt 2>/dev/null || true
	-sudo rmmod device_dma 2>/dev/null || true
	@echo "=== Loading modules ==="
	sudo insmod -f device_interrupt.ko
	sudo insmod -f device_dma.ko
	sudo chmod 666 /dev/dev_interrupt /dev/dev_dma
	@echo ""
	@echo "=== Interrupt-driven ==="
	./bench_interrupt
	@echo ""
	@echo "=== DMA with polling ==="
	./bench_dma
	@echo ""
	@echo "=== Cleaning up ==="
	-sudo rmmod device_interrupt device_dma 2>/dev/null || true

.PHONY: all modules bench clean load unload run
