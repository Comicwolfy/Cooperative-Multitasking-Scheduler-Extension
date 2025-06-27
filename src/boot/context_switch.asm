section .text
bits 32

global context_switch
context_switch:
    pushf
    push eax
    push ecx
    push edx
    push ebx
    push esi
    push edi
    push ebp

    mov eax, [esp + 36]
    mov [eax], esp

    mov esp, [esp + 40]

    pop ebp
    pop edi
    pop esi
    pop ebx
    pop edx
    pop ecx
    pop eax
    popf

    ret
