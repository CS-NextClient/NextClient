#pragma once

#include <csetjmp>

namespace frame_abort_test
{
#ifdef _MSC_VER
    // Match the engine's longjmp without C++ stack unwinding.
    __declspec(naked) inline int Save(std::jmp_buf)
    {
        __asm {
            mov edx, [esp + 4]
            mov [edx], ebp
            mov [edx + 4], ebx
            mov [edx + 8], edi
            mov [edx + 12], esi
            mov [edx + 16], esp
            mov eax, [esp]
            mov [edx + 20], eax
            mov eax, fs:[0]
            mov [edx + 24], eax
            xor eax, eax
            ret
        }
    }

    __declspec(naked) inline void Abort(std::jmp_buf, int)
    {
        __asm {
            mov edx, [esp + 4]
            mov ebp, [edx]
            mov ebx, [edx + 4]
            mov edi, [edx + 8]
            mov esi, [edx + 12]
            mov eax, [edx + 24]
            mov fs:[0], eax
            mov eax, [esp + 8]
            mov esp, [edx + 16]
            add esp, 4
            jmp dword ptr [edx + 20]
        }
    }
#else
    inline void Abort(std::jmp_buf buffer, int value)
    {
        std::longjmp(buffer, value);
    }
#endif
} // namespace frame_abort_test
