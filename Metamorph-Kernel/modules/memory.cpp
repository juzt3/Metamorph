#include "memory.h"

NTSTATUS Memory::Spoof() {
	auto* shared = reinterpret_cast<PKUSER_SHARED_DATA>(0xFFFFF78000000000);
	if (!shared || !shared->NumberOfPhysicalPages)
		return STATUS_UNSUCCESSFUL;

	const auto physical = MmGetPhysicalAddress(shared);
	if (!physical.QuadPart)
		return STATUS_UNSUCCESSFUL;

	auto* mapped = static_cast<PKUSER_SHARED_DATA>(MmMapIoSpace(physical, PAGE_SIZE, MmCached));
	if (!mapped)
		return STATUS_UNSUCCESSFUL;

	ULONG seed = KeQueryTimeIncrement();
	const auto delta = 0x40000 + (RtlRandomEx(&seed) % 0x40000); // 1-2 GiB in pages

	mapped->NumberOfPhysicalPages -= delta;

	MmUnmapIoSpace(mapped, PAGE_SIZE);

	return STATUS_SUCCESS;
}
