#!/bin/bash

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo ./run_benchmark.sh)"
    exit 1
fi

# Clean up any existing modules
rmmod device_interrupt 2>/dev/null
rmmod device_dma 2>/dev/null

echo "=== Testing Interrupt-Driven Device ==="
echo ""

insmod device_interrupt.ko
if [ $? -ne 0 ]; then
    echo "Failed to load interrupt module"
    exit 1
fi

chmod 666 /dev/dev_interrupt
./bench_interrupt

rmmod device_interrupt
echo ""
echo "=========================================="
echo ""

sleep 1

echo "=== Testing DMA Device with Polling ==="
echo ""

insmod device_dma.ko
if [ $? -ne 0 ]; then
    echo "Failed to load DMA module"
    exit 1
fi

chmod 666 /dev/dev_dma
./bench_dma

rmmod device_dma
