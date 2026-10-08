bits 16
org 0x7C00

start:
    mov [boot_drive], dl
    mov ax, 0x0003
    int 0x10
    cli
    mov ax, 0x0000
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x7C00
    mov si, kmsg
    call print

    mov bx, 0x7E00
    mov ah, 0x02
    mov al, 128
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc .error

    call enable_a20

    ; ===== VGA graphics mode 640x480x16 =====
    mov ax, 0x0012
    int 0x10

    lgdt [gdt_desc]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode_32

.error:
    mov si, err
    call print
    cli
    hlt

enable_a20:
    mov ax, 0x2401
    int 0x15
    jnc .a20_ok
    in  al, 0x92
    test al, 2
    jnz .a20_ok
    or  al, 2
    out 0x92, al
.a20_ok:
    ret

[bits 32]
protected_mode_32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp 0x08:0x7E00

[bits 16]
print:
    lodsb
    or  al, al
    jz  .done
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10
    jmp print
.done:
    ret

gdt_start:
    dq 0x0000000000000000
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A
    db 0xCF
    db 0x00
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

err        db "Err.", 0x0D, 0x0A, 0x00
kmsg       db "loading kernel...", 0x0D, 0x0A, 0x00
boot_drive db 0

times 510 - ($ - $$) db 0
dw 0xAA55
