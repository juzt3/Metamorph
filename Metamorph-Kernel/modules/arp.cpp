
#include "arp.h"
#include "adapters.h"

#define IOCTL_NSI_GET_PARAMETER (0x00120001)
#define IOCTL_NSI_GET_ALL_PARAMETERS (0x00120003)

PDRIVER_DISPATCH NsiControlOriginal = 0;

NTSTATUS NsiIoc(PDEVICE_OBJECT device, PIRP irp, PVOID context) {
	if (context) {
		IOC_REQUEST request = *(PIOC_REQUEST)context;
		ExFreePool(context);

		auto* buffer = (UCHAR*)request.Buffer;
		if (!buffer)
			buffer = (UCHAR*)irp->UserBuffer;

		if (buffer && request.BufferLength)
			Adapters::PatchBuffer(buffer, request.BufferLength);

		if (request.OldRoutine && irp->StackCount > 1) {
			return request.OldRoutine(device, irp, request.OldContext);
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS NsiControl(PDEVICE_OBJECT device, PIRP irp) {

	PIO_STACK_LOCATION ioc = IoGetCurrentIrpStackLocation(irp);
	switch (ioc->Parameters.DeviceIoControl.IoControlCode) {
	case IOCTL_NSI_GET_PARAMETER:
	case IOCTL_NSI_GET_ALL_PARAMETERS: {
		Utils::ChangeIoc(ioc, irp, NsiIoc);

		return NsiControlOriginal(device, irp);
	}
	case IOCTL_NSI_PROXY_ARP: {
		DWORD length = ioc->Parameters.DeviceIoControl.OutputBufferLength;

		NTSTATUS status = NsiControlOriginal(device, irp);

		PNSI_STRUCTURE_1 params = (PNSI_STRUCTURE_1)irp->UserBuffer;

		if (params && NSI_PARAMS_ARP == params->Type) {

			params->NumberOfEntries = 0;

			//LOG(XOR("Type: %d\n"), params->Type);
			//LOG(XOR("Length: %d\n"), length);

			//LOG(XOR("After\n"));

			//PrintBufferAsHex(irp->UserBuffer, 0xFF);

			// memset(irp->UserBuffer, 0xFF, length);

			// memset( (PVOID)((DWORD64)irp->UserBuffer + 112), 0x00, 255-112);

			// params->NumberOfEntries = 0;

			// LOG(XOR("Count: %d\n"), params->NumberOfEntries);


			//LOG(XOR("Handled ARP table (Length: %d)\n"), length);
		} else {
			// memset(irp->UserBuffer, 0x00, length);
		}

		return status;
	}
	}

	return NsiControlOriginal(device, irp);
}

NTSTATUS ARP::Spoof() {
	ControlHook::SwapControl(RTL_CONSTANT_STRING(L"\\Driver\\nsiproxy"), NsiControl, &NsiControlOriginal);

	return STATUS_SUCCESS;
}