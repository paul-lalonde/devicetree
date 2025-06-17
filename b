#!/bin/sh
riscv64-unknown-elf-as -march=rv64ic -o hello.o hello.s
riscv64-unknown-elf-gcc -g -mcmodel=medany -march=rv64i -mabi=lp64 -c -o main.o main.c
riscv64-unknown-elf-gcc -g -mcmodel=medany -march=rv64i -mabi=lp64 -c -o devtree.o devtree.c
riscv64-unknown-elf-ld -o kern -T hello.ld hello.o main.o devtree.o
#riscv64-unknown-elf-objdump -D kern
