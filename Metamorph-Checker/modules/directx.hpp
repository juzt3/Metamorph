#include "module.h"

DEFINE_MODULE(DirectX,
	{
		GetRegDWORD(HKEY_LOCAL_MACHINE, XOR("SOFTWARE\\Microsoft\\DirectX"), XOR("MaxFeatureLevel"), {
			data = value;
		});
	}
)
