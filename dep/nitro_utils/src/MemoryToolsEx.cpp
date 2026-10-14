//#include <cstdint>
//#include <nitro_utils/MemoryToolsEx.h>
//
//bool ValidateAddressing(uint32_t address)
//{
//    char lastByte = *(uint8_t*)(address - 1);
//    char secondByte = *(uint8_t*)(address - 2);
//    char thirdByte = *(uint8_t*)(address - 3);
//
//    // http://sparksandflames.com/files/x86InstructionChart.html
//    if (
//        (lastByte >= '\xB0' && lastByte <= '\xBF')
//        || (lastByte >= '\xA0' && lastByte <= '\xA3')
//        || (thirdByte >= '\x88' && thirdByte <= '\x8E') // mov, lea
//        || (secondByte >= '\x80' && secondByte <= '\x83') // sub, add
//        || (thirdByte >= '\x04' && thirdByte <= '\x05')
//        )
//        return true;
//
//    return false;
//}
//
//void SpoofMemoryStruct(uint32_t base, uint32_t replace, size_t struct_size, uint32_t start, uint32_t end)
//{
//    uint32_t oldProt;
//    for (uint32_t i = start, c; i < end; i++)
//    {
//        c = *(uintptr_t*)i;
//        if (c >= base && c < base + struct_size && ValidateAddressing(i))
//        {
//            VirtualProtect(LPVOID(i), 4, PAGE_EXECUTE_READWRITE, &oldProt);
//            *(uintptr_t*)i = (uintptr_t)(replace + (c - base));
//            VirtualProtect(LPVOID(i), 4, oldProt, &oldProt);
//        }
//    }
//}