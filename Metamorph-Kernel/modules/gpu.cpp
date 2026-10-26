#include "gpu.h"

static PDRIVER_DISPATCH NpfsOriginal = 0;
static PDRIVER_DISPATCH NpfsWriteOriginal = 0;
static PFAST_IO_READ NpfsFastReadOriginal = 0;
static PFAST_IO_WRITE NpfsFastWriteOriginal = 0;
static char FakeGpuUuid[37] = { 0 };

static void PatchGpuUuid(char* data, ULONG length) {
	for (ULONG i = 0; i + 4 + 36 <= length; ++i) {
		if (memcmp(data + i, "GPU-", 4) != 0)
			continue;

		memcpy(data + i + 4, FakeGpuUuid, 36);
		i += 4 + 36 - 1;
	}
}

static NTSTATUS NpfsIoc(PDEVICE_OBJECT device, PIRP irp, PVOID context) {
	if (!context)
		context = irp->Tail.Overlay.DriverContext[3];

	if (context) {
		IOC_REQUEST request = *(PIOC_REQUEST)context;
		ExFreePool(context);

		auto* buffer = static_cast<PFILE_PIPE_PEEK_BUFFER>(request.Buffer);
		if (!buffer && irp->MdlAddress)
			buffer = static_cast<PFILE_PIPE_PEEK_BUFFER>(MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority));
		if (!buffer)
			buffer = static_cast<PFILE_PIPE_PEEK_BUFFER>(irp->UserBuffer);
		if (!buffer)
			buffer = static_cast<PFILE_PIPE_PEEK_BUFFER>(IoGetCurrentIrpStackLocation(irp)->Parameters.FileSystemControl.Type3InputBuffer);

		if (buffer && request.BufferLength > FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data)) {
			ULONG length = request.BufferLength - FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data);
			if (length > buffer->ReadDataAvailable)
				length = buffer->ReadDataAvailable;

			PatchGpuUuid(buffer->Data, length);
		}

		if (request.OldRoutine && irp->StackCount > 1) {
			return request.OldRoutine(device, irp, request.OldContext);
		}
	}

	return STATUS_SUCCESS;
}

static NTSTATUS NpfsControl(PDEVICE_OBJECT device, PIRP irp) {
	PIO_STACK_LOCATION ioc = IoGetCurrentIrpStackLocation(irp);

	if (ioc->Parameters.FileSystemControl.FsControlCode == FSCTL_PIPE_PEEK)
		Utils::ChangeIoc(ioc, irp, NpfsIoc);

	return NpfsOriginal(device, irp);
}

static NTSTATUS NpfsWrite(PDEVICE_OBJECT device, PIRP irp) {
	PIO_STACK_LOCATION ioc = IoGetCurrentIrpStackLocation(irp);

	auto* buffer = static_cast<char*>(irp->UserBuffer);
	if (!buffer)
		buffer = static_cast<char*>(irp->AssociatedIrp.SystemBuffer);

	if (buffer && ioc->Parameters.Write.Length)
		PatchGpuUuid(buffer, ioc->Parameters.Write.Length);

	return NpfsWriteOriginal(device, irp);
}

static BOOLEAN NpfsFastRead(PFILE_OBJECT fileObject, PLARGE_INTEGER offset, ULONG length, BOOLEAN wait, ULONG lockKey, PVOID buffer, PIO_STATUS_BLOCK ioStatus, PDEVICE_OBJECT device) {
	BOOLEAN result = NpfsFastReadOriginal(fileObject, offset, length, wait, lockKey, buffer, ioStatus, device);

	if (result && buffer && ioStatus && ioStatus->Information)
		PatchGpuUuid(static_cast<char*>(buffer), static_cast<ULONG>(ioStatus->Information));

	return result;
}

static BOOLEAN NpfsFastWrite(PFILE_OBJECT fileObject, PLARGE_INTEGER offset, ULONG length, BOOLEAN wait, ULONG lockKey, PVOID buffer, PIO_STATUS_BLOCK ioStatus, PDEVICE_OBJECT device) {
	if (buffer && length)
		PatchGpuUuid(static_cast<char*>(buffer), length);

	return NpfsFastWriteOriginal(fileObject, offset, length, wait, lockKey, buffer, ioStatus, device);
}

NTSTATUS Gpu::Spoof() {
	ULONG seed = KeQueryTimeIncrement();

	RtlStringCbPrintfA(FakeGpuUuid, sizeof(FakeGpuUuid), "%08X-%04X-%04X-%04X-%08X%04X",
		RtlRandomEx(&seed), RtlRandomEx(&seed) & 0xFFFF, RtlRandomEx(&seed) & 0xFFFF,
		RtlRandomEx(&seed) & 0xFFFF, RtlRandomEx(&seed), RtlRandomEx(&seed) & 0xFFFF);

	NTSTATUS status = ControlHook::Swap(RTL_CONSTANT_STRING(L"\\FileSystem\\npfs"), IRP_MJ_FILE_SYSTEM_CONTROL, NpfsControl, &NpfsOriginal);
	ControlHook::Swap(RTL_CONSTANT_STRING(L"\\FileSystem\\npfs"), IRP_MJ_WRITE, NpfsWrite, &NpfsWriteOriginal);

	UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\FileSystem\\npfs");
	PDRIVER_OBJECT driver = 0;
	if (NT_SUCCESS(ObReferenceObjectByName(&name, OBJ_CASE_INSENSITIVE, 0, 0, *IoDriverObjectType, KernelMode, 0, (PVOID*)&driver))) {
		if (driver->FastIoDispatch) {
			NpfsFastReadOriginal = driver->FastIoDispatch->FastIoRead;
			driver->FastIoDispatch->FastIoRead = NpfsFastRead;
			NpfsFastWriteOriginal = driver->FastIoDispatch->FastIoWrite;
			driver->FastIoDispatch->FastIoWrite = NpfsFastWrite;
		}

		ObDereferenceObject(driver);
	}

	return status;
}
