#include "module.h"

DEFINE_MODULE(Volume,
	{
		DWORD serial = 0;

		if (GetVolumeInformationA(XOR("C:\\"), NULL, 0, &serial, NULL, NULL, NULL, 0)) {
			data = serial;
		}
	}
)
