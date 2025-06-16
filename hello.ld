MEMORY {
  boot_space (rwx) : ORIGIN = 0x80200000, LENGTH = 0x100000
}

SECTIONS {
  .text : {
   hello.o(.text*l)
   hello.o(.rodata*)
   hello.o(.data*)
   dt.o(.text*l)
   dt.o(.rodata*)
   dt.o(.data*)
 } > boot_space
 }
