#include "module.h"

#include <intrin.h>

DEFINE_MODULE(Processor,
    {
        int regs[4] = { 0 };

        __cpuid(regs, 0);

        char vendor[13] = { 0 };
        memcpy(vendor, &regs[1], 4);
        memcpy(vendor + 4, &regs[3], 4);
        memcpy(vendor + 8, &regs[2], 4);

        data["vendor"] = vendor;

        char brand[49] = { 0 };

        __cpuid(regs, 0x80000000);
        if ((unsigned int)regs[0] >= 0x80000004) {
            for (int i = 0; i < 3; i++) {
                __cpuid(regs, 0x80000002 + i);
                memcpy(brand + i * 16, regs, 16);
            }
        }

        data["brand"] = brand;

        WMIC::DoRequest(XOR("SELECT * FROM Win32_Processor WHERE Name IS NOT NULL"), XOR("Name"), [&data](const char* value) {
            data["name"] = value;
        });

        WMIC::DoRequest(XOR("SELECT * FROM Win32_Processor WHERE Name IS NOT NULL"), XOR("Manufacturer"), [&data](const char* value) {
            data["manufacturer"] = value;
        });

        WMIC::DoRequest(XOR("SELECT * FROM Win32_Processor WHERE Name IS NOT NULL"), XOR("NumberOfCores"), [&data](const char* value) {
            data["cores"] = value;
        });

        WMIC::DoRequest(XOR("SELECT * FROM Win32_Processor WHERE Name IS NOT NULL"), XOR("NumberOfLogicalProcessors"), [&data](const char* value) {
            data["logical"] = value;
        });
    }
)
