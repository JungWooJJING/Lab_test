.section .data
test_pipe:
    .long 0x74657374

.section .bss
    .lcomm fs_temp,8

.section .init_array,"aw"
    .quad __init_forkserver

.section .text
.global __init_forkserver
.type __init_forkserver,@function
__init_forkserver:
    mov $199, %rdi
    lea test_pipe(%rip), %rsi
    mov $4, %rdx
    mov $1, %rax
    syscall

    jmp __fork_loop

__fork_loop:
    mov $0, %rax
    mov $198, %rdi
    lea fs_temp(%rip), %rsi
    mov $4, %rdx
    syscall

    cmp $4, %rax
    jne __fork_exit
    
    mov $57, %rax
    syscall

    cmp $0, %rax
    jl __fork_exit

    je __child_process

__parent_process:
    mov %rax, fs_temp(%rip)
    mov $199, %rdi
    lea fs_temp(%rip), %rsi
    mov $4, %rdx
    mov $1, %rax
    syscall

    mov fs_temp(%rip), %rdi
    lea fs_temp(%rip), %rsi
    mov $0, %rdx
    mov $0, %r10
    mov $61, %rax
    syscall

    mov $199, %rdi
    lea fs_temp(%rip), %rsi
    mov $4, %rdx
    mov $1, %rax
    syscall

    jmp __fork_loop

__child_process:
    ret

__fork_exit:
    mov $60, %rax
    mov $1, %rdi
    syscall

.size __init_forkserver, .-__init_forkserver
