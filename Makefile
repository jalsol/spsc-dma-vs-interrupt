obj-m += device_interrupt.o
obj-m += device_polling.o

# Build against the currently running kernel by default.
# Can be overridden, e.g. `make KREL=...`.
KREL ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KREL)/build
PWD := $(shell pwd)

all: modules bench

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

bench:
	gcc -O3 -Wall -o bench_interrupt bench_interrupt.c
	gcc -O3 -Wall -o bench_polling bench_polling.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f bench_interrupt bench_polling

load:
	sudo insmod device_interrupt.ko
	sudo insmod device_polling.ko
	sudo chmod 666 /dev/dev_interrupt /dev/dev_polling

unload:
	-sudo rmmod device_interrupt 2>/dev/null || true
	-sudo rmmod device_polling 2>/dev/null || true

run: 
	@echo "=== Cleaning up old modules ==="
	-sudo rmmod device_interrupt 2>/dev/null || true
	-sudo rmmod device_polling 2>/dev/null || true
	@echo "=== Loading modules ==="
	sudo insmod device_interrupt.ko
	sudo insmod device_polling.ko
	sudo chmod 666 /dev/dev_interrupt /dev/dev_polling
	@echo ""
	@echo "=== Interrupt-driven ==="
	./bench_interrupt
	@echo ""
	@echo "=== Polling ==="
	./bench_polling
	@echo ""
	@echo "=== Cleaning up ==="
	-sudo rmmod device_interrupt device_polling 2>/dev/null || true

.PHONY: all modules bench clean load unload run
