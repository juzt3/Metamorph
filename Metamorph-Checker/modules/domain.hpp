#include "module.h"

DEFINE_MODULE(Domain,
	{
		char domain[256] = { 0 };
		DWORD size = sizeof(domain);

		if (GetComputerNameExA(ComputerNameDnsDomain, domain, &size)) {
			data = domain;
		}
	}
)
