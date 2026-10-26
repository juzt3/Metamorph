#include "module.h"

#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

DEFINE_MODULE(Adapters,
	{
		ULONG size = 0;
		GetAdaptersInfo(NULL, &size);

		auto* info = reinterpret_cast<IP_ADAPTER_INFO*>(new char[size]);

		if (GetAdaptersInfo(info, &size) == NO_ERROR) {
			for (auto* adapter = info; adapter; adapter = adapter->Next) {
				if (adapter->AddressLength == 0)
					continue;

				char mac[32] = { 0 };
				snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
					adapter->Address[0], adapter->Address[1], adapter->Address[2],
					adapter->Address[3], adapter->Address[4], adapter->Address[5]);

				data.push_back(mac);
			}
		}

		delete[] reinterpret_cast<char*>(info);
	}
)
