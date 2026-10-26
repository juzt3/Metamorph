#include "module.h"

DEFINE_MODULE(Gpu,
	{
		char buffer[4096] = { 0 };

		FILE* pipe = _popen(XOR("nvidia-smi.exe -L"), "r");
		if (pipe) {
			size_t read = fread(buffer, 1, sizeof(buffer) - 1, pipe);
			buffer[read] = '\0';

			_pclose(pipe);

			data = buffer;
		}
	}
)
