#include "module.h"

DEFINE_MODULE(Memory,
	{
		MEMORYSTATUSEX status = { 0 };
		status.dwLength = sizeof(status);

		GlobalMemoryStatusEx(&status);

		data["total"] = status.ullTotalPhys;

		WMIC::DoRequest(XOR("SELECT * FROM Win32_PhysicalMemory"), XOR("Manufacturer"), [&data](const char* value) {
			data["manufacturer"].push_back(value);
		});

		WMIC::DoRequest(XOR("SELECT * FROM Win32_PhysicalMemory"), XOR("SerialNumber"), [&data](const char* value) {
			data["serial"].push_back(value);
		});

		WMIC::DoRequest(XOR("SELECT * FROM Win32_PhysicalMemory"), XOR("Capacity"), [&data](const char* value) {
			data["capacity"].push_back(value);
		});
	}
)
