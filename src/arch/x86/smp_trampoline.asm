; SMP AP real-mode trampoline
; Assembled as flat binary (ORG 0), copied to page-aligned low memory
; Entry point at offset 0 within the page

[ORG 0]
[BITS 16]

%define MAILBOX 0x1000
%define MB_ENTRY 0x00
%define MB_STACK 0x04
%define MB_CR3   0x08
%define MB_CPU   0x0C
%define MB_GO    0x18

start:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor sp, sp

    ; Enable A20
    mov ax, 0x2401
    int 0x15
    jnc .a20_ok
    call a20_kbc
.a20_ok:

    ; Load GDT (CS-relative addressing gives correct real-mode address)
    lgdt [cs:gdt_desc]

    ; Enter protected mode
    mov eax, cr0
    or al, 1
    mov cr0, eax

    ; Far jump to 32-bit code: selector 0x08, offset pm_entry
    jmp dword 0x08:pm_entry

[BITS 32]
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov ax, 0x38
    mov gs, ax

    ; Load page directory from mailbox
    mov eax, [MAILBOX + MB_CR3]
    mov cr3, eax

    ; Enable paging
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; Set up stack
    mov esp, [MAILBOX + MB_STACK]

    ; Signal ready
    mov dword [MAILBOX + MB_GO], 0

    ; Jump to kernel entry point (virtual address, paging enabled)
    jmp [MAILBOX + MB_ENTRY]

; Real-mode helpers
a20_kbc:
    push ax
    call a20_wait
    mov al, 0xD1
    out 0x64, al
    call a20_wait
    mov al, 0xDF
    out 0x60, al
    call a20_wait
    pop ax
    ret

a20_wait:
    in al, 0x64
    test al, 2
    jnz a20_wait
    ret

align 4
gdt:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00CFFA000000FFFF
    dq 0x00CFF2000000FFFF
    dq 0
    dq 0
    dq 0x00CF92000000FFFF
gdt_desc:
    dw (gdt_desc - gdt - 1)
    dd gdt
