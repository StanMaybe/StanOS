bits 16
org 0x7C00

KERNEL_LOAD equ 0x1000
KERNEL_SECTORS equ 62
VBE_INFO equ 0x9000
FONT_BUFFER equ 0x0A00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; Reset disk
    xor ah, ah
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; Load kernel from sectors 2 through 63
    xor ax, ax
    mov es, ax
    mov bx, KERNEL_LOAD
    mov byte [sectors_read], 0

.load_loop:
    cmp byte [sectors_read], KERNEL_SECTORS
    je .load_done

    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, [sectors_read]
    add cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    add bx, 512
    inc byte [sectors_read]
    jmp .load_loop

.load_done:
    ; Get the BIOS 8x16 font
    mov ax, 0x1130
    mov bh, 0x06
    int 0x10

    mov [font_segment], es
    mov [font_offset], bp

    ; Copy 256 glyphs, 16 bytes per glyph, to physical 0xA000
    push ds
    mov ax, [font_segment]
    mov ds, ax
    mov si, [font_offset]

    mov ax, FONT_BUFFER
    mov es, ax
    xor di, di
    mov cx, 2048
    cld
    rep movsw
    pop ds

    xor ax, ax
    mov es, ax

    ; Query VESA mode 0x118 (1024x768, usually 24-bit)
    mov ax, 0x4F01
    mov cx, 0x0118
    mov di, VBE_INFO
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; Require supported mode + linear framebuffer
    mov ax, [VBE_INFO]
    and ax, 0x0081
    cmp ax, 0x0081
    jne vbe_error

    ; Set mode with the linear framebuffer enabled
    mov ax, 0x4F02
    mov bx, 0x4118
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_mode

bits 32

protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000

    jmp 0x08:KERNEL_LOAD

.hang:
    cli
    hlt
    jmp .hang

bits 16

disk_error:
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    jmp error_halt

vbe_error:
    mov ah, 0x0E
    mov al, 'V'
    int 0x10

error_halt:
    cli
    hlt
    jmp error_halt

gdt_start:
    dq 0

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

boot_drive:
    db 0

sectors_read:
    db 0

font_segment:
    dw 0

font_offset:
    dw 0

times 510 - ($ - $$) db 0
dw 0xAA55