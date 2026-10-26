#include "volume.h"

static PDRIVER_DISPATCH VolumeOriginal = 0;

typedef struct
{
	ULONG Original;
	ULONG Fake;
} VOLUME_SERIAL;

static VOLUME_SERIAL VolumeSerials[16] = { 0 };
static ULONG VolumeSerialCount = 0;

static ULONG GetFakeSerial(ULONG original) {
	for (ULONG i = 0; i < VolumeSerialCount; i++) {
		if (VolumeSerials[i].Original == original)
			return VolumeSerials[i].Fake;
	}

	auto fake = original ^ 0x5A5A5A5A;
	if (!fake)
		fake = 0x12345678;

	if (VolumeSerialCount < ARRAYSIZE(VolumeSerials)) {
		VolumeSerials[VolumeSerialCount].Original = original;
		VolumeSerials[VolumeSerialCount].Fake = fake;
		VolumeSerialCount++;
	}

	return fake;
}

static NTSTATUS VolumeIoc(PDEVICE_OBJECT device, PIRP irp, PVOID context) {
	if (context) {
		IOC_REQUEST request = *(PIOC_REQUEST)context;
		ExFreePool(context);

		auto* volume = (PFILE_FS_VOLUME_INFORMATION)irp->AssociatedIrp.SystemBuffer;
		if (volume)
			volume->VolumeSerialNumber = GetFakeSerial(volume->VolumeSerialNumber);

		if (request.OldRoutine && irp->StackCount > 1) {
			return request.OldRoutine(device, irp, request.OldContext);
		}
	}

	return STATUS_SUCCESS;
}

static NTSTATUS VolumeControl(PDEVICE_OBJECT device, PIRP irp) {
	PIO_STACK_LOCATION ioc = IoGetCurrentIrpStackLocation(irp);

	if (ioc->Parameters.QueryVolume.FsInformationClass == FileFsVolumeInformation
		&& ioc->Parameters.QueryVolume.Length >= sizeof(FILE_FS_VOLUME_INFORMATION)) {
		Utils::ChangeIoc(ioc, irp, VolumeIoc);
	}

	return VolumeOriginal(device, irp);
}

NTSTATUS Volume::Spoof() {
	return ControlHook::Swap(RTL_CONSTANT_STRING(L"\\FileSystem\\Ntfs"), IRP_MJ_QUERY_VOLUME_INFORMATION, VolumeControl, &VolumeOriginal);
}
