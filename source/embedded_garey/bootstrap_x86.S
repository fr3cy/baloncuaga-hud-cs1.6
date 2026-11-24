.intel_syntax noprefix
.code32
.text
.Lstart:
.equ CODE_RVA, 0x00a87000
.equ DATA_RVA, 0x00a88000
.equ FPS_STATE, DATA_RVA
.equ VOICE_STATE, DATA_RVA+4
.equ GET_MODULE, 0x00a7206c
.equ GET_PROC, 0x00a72080
.equ CREATE_THREAD, 0x00a72058
.equ CLOSE_HANDLE, 0x00a72054
.equ VIRTUAL_QUERY, 0x00a72074
.equ SLEEP, 0x00a72094

.globl fps_adapter
fps_adapter:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    and esp, -16
    sub esp, 12
    push dword ptr [ebp+8]
.globl pointer_read_call
pointer_read_call:
    .byte 0xe8
    .long 0
    add esp, 16
    mov esi, eax
    call .Lpc_bx
.Lfps_pc:
    sub ebx, CODE_RVA + (.Lfps_pc-.Lstart)
    xor eax, eax
    mov edx, 1
    lock cmpxchg dword ptr [ebx+FPS_STATE], edx
    jne .Lfps_done
    lea eax, [ebx+HW_NAME_RVA]
    sub esp, 12
    push eax
    call dword ptr [ebx+GET_MODULE]
    add esp, 12
    mov edx, 0x5f3d8dcc
    mov ecx, 0x01259000
    call .Lcompatible_image
    test eax, eax
    jz .Lfps_unsupported
.globl native_fps_call
native_fps_call:
    .byte 0xe8
    .long 0
    mov dword ptr [ebx+FPS_STATE], 2
    jmp .Lfps_done
.Lfps_unsupported:
    mov dword ptr [ebx+FPS_STATE], 4
.Lfps_done:
    mov eax, esi
    lea esp, [ebp-8]
    pop esi
    pop ebx
    pop ebp
    ret

.globl voice_adapter
voice_adapter:
    push ebp
    mov ebp, esp
    push ebx
    and esp, -16
.globl original_destructor_call
original_destructor_call:
    .byte 0xe8
    .long 0
    call .Lpc_bx
.Lvoice_pc:
    sub ebx, CODE_RVA+(.Lvoice_pc-.Lstart)
    xor eax, eax
    mov edx, 1
    lock cmpxchg dword ptr [ebx+VOICE_STATE], edx
    jne .Lvoice_adapter_done
    cmp dword ptr [ebx+FPS_STATE], 2
    jne .Lvoice_create_failed
    lea eax, [ebx+VOICE_THREAD_RVA]
    sub esp, 8
    push 0
    push 0
    push 0
    push eax
    push 0
    push 0
    call dword ptr [ebx+CREATE_THREAD]
    add esp, 8
    test eax, eax
    jz .Lvoice_create_failed
    sub esp, 12
    push eax
    call dword ptr [ebx+CLOSE_HANDLE]
    add esp, 12
    jmp .Lvoice_adapter_done
.Lvoice_create_failed:
    mov dword ptr [ebx+VOICE_STATE], 4
.Lvoice_adapter_done:
    lea esp, [ebp-4]
    pop ebx
    pop ebp
    ret

.globl voice_thread
voice_thread:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    and esp, -16
    sub esp, 16
    call .Lpc_bx
.Lvoice_thread_pc:
    sub ebx, CODE_RVA+(.Lvoice_thread_pc-.Lstart)
    mov dword ptr [ebp-16], 100
.Lvoice_retry:
    lea eax, [ebx+STEAM_NAME_RVA]
    sub esp, 12
    push eax
    call dword ptr [ebx+GET_MODULE]
    add esp, 12
    test eax, eax
    jz .Lvoice_wait
    mov esi, eax
    mov edx, 0x50c7d858
    mov ecx, 0x0001d000
    call .Lcompatible_image
    test eax, eax
    jz .Lvoice_unsupported
    lea eax, [ebx+STEAM_CLIENT_RVA]
    sub esp, 8
    push eax
    push esi
    call dword ptr [ebx+GET_PROC]
    add esp, 8
    test eax, eax
    jz .Lvoice_unsupported
    call eax
    test eax, eax
    jz .Lvoice_wait
    lea eax, [ebx+STEAM_USER_RVA]
    sub esp, 8
    push eax
    push esi
    call dword ptr [ebx+GET_PROC]
    add esp, 8
    test eax, eax
    jz .Lvoice_unsupported
    call eax
    test eax, eax
    jz .Lvoice_wait
    mov edi, eax
    call .Lreadable_pointer
    test eax, eax
    jz .Lvoice_unsupported
    mov eax, dword ptr [edi]
    mov edi, eax
    call .Lreadable_pointer
    test eax, eax
    jz .Lvoice_unsupported
    mov eax, dword ptr [edi+44]
    call .Lexecutable_pointer
    test eax, eax
    jz .Lvoice_unsupported
    mov eax, dword ptr [edi+52]
    call .Lexecutable_pointer
    test eax, eax
    jz .Lvoice_unsupported
    lea eax, [ebx+VOICE_ENABLE_RVA]
    sub esp, 12
    push eax
    call dword ptr [ebx+0x0021fdc8]
    add esp, 16
    test eax, eax
    jz .Lvoice_unsupported
    lea eax, [ebx+VOICE_SCALE_RVA]
    sub esp, 12
    push eax
    call dword ptr [ebx+0x0021fdc8]
    add esp, 16
    test eax, eax
    jz .Lvoice_unsupported
.globl native_voice_call
native_voice_call:
    .byte 0xe8
    .long 0
    mov dword ptr [ebx+VOICE_STATE], 2
    jmp .Lvoice_thread_done
.Lvoice_wait:
    sub esp, 12
    push 100
    call dword ptr [ebx+SLEEP]
    add esp, 12
    dec dword ptr [ebp-16]
    jnz .Lvoice_retry
.Lvoice_unsupported:
    mov dword ptr [ebx+VOICE_STATE], 4
.Lvoice_thread_done:
    xor eax, eax
    lea esp, [ebp-12]
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret 4

# Validate engine/client compatibility before the original module scanner
# inspects their private structures. A skipped CS Fix must leave HUD usable.
.globl guarded_worker
guarded_worker:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    and esp, -16
    call .Lpc_bx
.Lworker_pc:
    sub ebx, CODE_RVA+(.Lworker_pc-.Lstart)
    mov dword ptr [ebx+DATA_RVA+8], 1
    mov esi, 600
.Lworker_poll:
    sub esp, 12
    lea eax, [ebx+HW_NAME_RVA]
    push eax
    call dword ptr [ebx+GET_MODULE]
    add esp, 12
    test eax, eax
    jz .Lworker_wait
    mov edx, 0x5f3d8dcc
    mov ecx, 0x01259000
    call .Lcompatible_image
    test eax, eax
    jz .Lworker_skip
    sub esp, 12
    lea eax, [ebx+CLIENT_NAME_RVA]
    push eax
    call dword ptr [ebx+GET_MODULE]
    add esp, 12
    test eax, eax
    jz .Lworker_wait
    mov edx, 0x5f28cde0
    mov ecx, 0x0015f000
    call .Lcompatible_image
    test eax, eax
    jz .Lworker_skip
    sub esp, 12
    push dword ptr [ebp+8]
.globl original_worker_call
original_worker_call:
    .byte 0xe8
    .long 0
    add esp, 12
    jmp .Lworker_done
.Lworker_wait:
    sub esp, 12
    push 100
    call dword ptr [ebx+SLEEP]
    add esp, 12
    dec esi
    jnz .Lworker_poll
.Lworker_skip:
    mov dword ptr [ebx+FPS_STATE], 4
    mov dword ptr [ebx+VOICE_STATE], 4
    xor eax, eax
.Lworker_done:
    lea esp, [ebp-8]
    pop esi
    pop ebx
    pop ebp
    ret 4

.Lpc_bx:
    mov ebx, dword ptr [esp]
    ret

# EAX = trusted module handle, EDX = supported timestamp, ECX = image size.
.Lcompatible_image:
    test eax, eax
    jz .Limage_no
    cmp word ptr [eax], 0x5a4d
    jne .Limage_no
    push esi
    mov esi, dword ptr [eax+60]
    cmp esi, 0x1000
    ja .Limage_pop_no
    add esi, eax
    cmp dword ptr [esi], 0x4550
    jne .Limage_pop_no
    cmp word ptr [esi+24], 0x10b
    jne .Limage_pop_no
    cmp dword ptr [esi+8], edx
    jne .Limage_pop_no
    cmp dword ptr [esi+80], ecx
    jne .Limage_pop_no
    pop esi
    mov eax, 1
    ret
.Limage_pop_no:
    pop esi
.Limage_no:
    xor eax, eax
    ret

# Validate the entire region of a Steam vtable before reading its slots.
.Lreadable_pointer:
    push ebp
    mov ebp, esp
    push esi
    mov esi, eax
    and esp, -16
    sub esp, 32
    mov edx, esp
    sub esp, 4
    push 28
    push edx
    push eax
    call dword ptr [ebx+VIRTUAL_QUERY]
    add esp, 4
    test eax, eax
    jz .Lread_no
    cmp dword ptr [esp+16], 0x1000
    jne .Lread_no
    mov eax, dword ptr [esp+20]
    test eax, 0x101
    jnz .Lread_no
    mov eax, dword ptr [esp+12]
    add eax, dword ptr [esp]
    jc .Lread_no
    sub eax, esi
    jc .Lread_no
    cmp eax, 64
    jb .Lread_no
    mov eax, 1
    mov esi, dword ptr [ebp-4]
    leave
    ret
.Lread_no:
    xor eax, eax
    mov esi, dword ptr [ebp-4]
    leave
    ret

.Lexecutable_pointer:
    push ebp
    mov ebp, esp
    and esp, -16
    sub esp, 32
    mov edx, esp
    sub esp, 4
    push 28
    push edx
    push eax
    call dword ptr [ebx+VIRTUAL_QUERY]
    add esp, 4
    test eax, eax
    jz .Lexec_no
    cmp dword ptr [esp+16], 0x1000
    jne .Lexec_no
    mov eax, dword ptr [esp+20]
    test eax, 0x101
    jnz .Lexec_no
    test eax, 0xf0
    jz .Lexec_no
    mov eax, 1
    leave
    ret
.Lexec_no:
    xor eax, eax
    leave
    ret
.Lhw_name: .asciz "hw.dll"
.Lclient_name: .asciz "client.dll"
.Lsteam_name: .asciz "steam_api.dll"
.Lsteam_user: .asciz "SteamUser"
.Lsteam_client: .asciz "SteamClient"
.Lvoice_enable: .asciz "voice_enable"
.Lvoice_scale: .asciz "voice_scale"
.equ HW_NAME_RVA, CODE_RVA+(.Lhw_name-.Lstart)
.equ CLIENT_NAME_RVA, CODE_RVA+(.Lclient_name-.Lstart)
.equ VOICE_THREAD_RVA, CODE_RVA+(voice_thread-.Lstart)
.equ STEAM_NAME_RVA, CODE_RVA+(.Lsteam_name-.Lstart)
.equ STEAM_CLIENT_RVA, CODE_RVA+(.Lsteam_client-.Lstart)
.equ STEAM_USER_RVA, CODE_RVA+(.Lsteam_user-.Lstart)
.equ VOICE_ENABLE_RVA, CODE_RVA+(.Lvoice_enable-.Lstart)
.equ VOICE_SCALE_RVA, CODE_RVA+(.Lvoice_scale-.Lstart)
