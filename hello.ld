MEMORY {
  boot_space (rwx) : ORIGIN = 0x80200000, LENGTH = 0x100000
}

SECTIONS {
  .text : {
   hello.o(.text*l)
   hello.o(.rodata*)
   hello.o(.data*)
   devtree.o(.text*l)
   devtree.o(.rodata*)
   devtree.o(.data*)
   main.o(.text*l)
   main.o(.rodata*)
   main.o(.data*)
} > boot_space
 }
