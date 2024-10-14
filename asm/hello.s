global _start

section .text

; arg regs: RDI, RSI, RDX, RCX, R8, R9

_start:
  mov rsi, msg      ;   "Hello, world!\n",
  mov rdx, msglen   ;   sizeof("Hello, world!\n")
  call print

  call exit

print:
  mov rax, 1        ; write(
  mov rdi, 1        ;   STDOUT_FILENO,
  mov rsi, msg      ;   str,
  mov rdx, msglen   ;   sizeof(str)
  syscall           ; );
  ret

exit:
  mov rax, 60       ; exit(
  mov rdi, 0        ;   EXIT_SUCCESS
  syscall           ; );

section .rodata
  msg: db "Hello, worlds!", 10
  msglen: equ $ - msg