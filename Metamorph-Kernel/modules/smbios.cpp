
#include "smbios.h"

/**
 * \brief Get's the string from SMBIOS table
 * \param header Table header
 * \param string String itself
 * \return Pointer to the null terminated string
 */
char* SMBios::GetString(SMBIOS_HEADER* header, SMBIOS_STRING string)
{
	const auto* start = reinterpret_cast<const char*>(header) + header->Length;

	if (!string || *start == 0)
		return nullptr;

	while (--string)
	{
		start += strlen(start) + 1;
	}

	return const_cast<char*>(start);
}

/**
 * \brief Replace string at a given location by randomized string with same length
 * \param string Pointer to string (has to be null terminated)
 */
void SMBios::RandomizeString(char* string)
{
	if (!string)
		return;

	const auto length = static_cast<int>(strlen(string));
	if (!length)
		return;

	auto* buffer = static_cast<char*>(ExAllocatePoolWithTag(NonPagedPool, length + 1, POOL_TAG));
	Utils::RandomText(buffer, length);
	buffer[length] = '\0';

	memcpy(string, buffer, length);

	ExFreePool(buffer);
}

/**
 * \brief Modify information in the table of given header
 * \param header Table header (types 0-4 and 17 implemented)
 * \return 
 */
NTSTATUS SMBios::ProcessTable(SMBIOS_HEADER* header)
{
	if (!header->Length)
		return STATUS_UNSUCCESSFUL;

	if (header->Type == 0)
	{
		auto* type0 = reinterpret_cast<SMBIOS_TYPE0*>(header);

		auto* vendor = GetString(header, type0->Vendor);
		RandomizeString(vendor);

		auto* biosVersion = GetString(header, type0->BiosVersion);
		RandomizeString(biosVersion);

		auto* biosReleaseDate = GetString(header, type0->BiosReleaseDate);
		RandomizeString(biosReleaseDate);
	}

	if (header->Type == 1)
	{
		auto* type1 = reinterpret_cast<SMBIOS_TYPE1*>(header);

		auto* manufacturer = GetString(header, type1->Manufacturer);
		RandomizeString(manufacturer);

		auto* productName = GetString(header, type1->ProductName);
		RandomizeString(productName);

		auto* serialNumber = GetString(header, type1->SerialNumber);
		RandomizeString(serialNumber);
	}

	if (header->Type == 2)
	{
		auto* type2 = reinterpret_cast<SMBIOS_TYPE2*>(header);

		auto* manufacturer = GetString(header, type2->Manufacturer);
		RandomizeString(manufacturer);

		auto* productName = GetString(header, type2->ProductName);
		RandomizeString(productName);

		auto* serialNumber = GetString(header, type2->SerialNumber);
		RandomizeString(serialNumber);
	}

	if (header->Type == 3)
	{
		auto* type3 = reinterpret_cast<SMBIOS_TYPE3*>(header);

		auto* manufacturer = GetString(header, type3->Manufacturer);
		RandomizeString(manufacturer);

		auto* serialNumber = GetString(header, type3->SerialNumber);
		RandomizeString(serialNumber);

		static const UINT8 chassisTypes[] = { 3, 4, 5, 6, 7, 10, 23 };
		ULONG seed = KeQueryTimeIncrement();
		type3->Type = chassisTypes[RtlRandomEx(&seed) % sizeof(chassisTypes)];
	}

	if (header->Type == 4)
	{
		auto* type4 = reinterpret_cast<SMBIOS_TYPE4*>(header);

		Utils::RandomBytes(reinterpret_cast<char*>(type4->ProcessorId), sizeof(type4->ProcessorId) - 1);
	}

	if (header->Type == 17)
	{
		auto* type17 = reinterpret_cast<SMBIOS_TYPE17*>(header);

		auto* manufacturer = GetString(header, type17->Manufacturer);
		RandomizeString(manufacturer);

		auto* serialNumber = GetString(header, type17->SerialNumber);
		RandomizeString(serialNumber);

		auto* partNumber = GetString(header, type17->PartNumber);
		RandomizeString(partNumber);
	}
	
	return STATUS_SUCCESS;
}

/**
 * \brief Loop through SMBIOS tables with provided first table header
 * \param mapped Header of the first table
 * \param size Size of all tables including strings
 * \return 
 */
NTSTATUS SMBios::LoopTables(void* mapped, ULONG size)
{
	auto* start = static_cast<char*>(mapped);
	auto* endAddress = start + size;
	auto* current = start;

	while (current + sizeof(SMBIOS_HEADER) <= endAddress)
	{
		auto* header = reinterpret_cast<SMBIOS_HEADER*>(current);
		if (header->Type == 127 && header->Length == 4)
			break;

		if (header->Length < 4 || current + header->Length > endAddress)
			break;

		ProcessTable(header);

		auto* end = current + header->Length;
		while (end + 1 < endAddress && (*end || *(end + 1))) end++;
		if (end + 1 >= endAddress)
			break;

		current = end + 2;
	}

	return STATUS_SUCCESS;
}

/**
 * \brief Find the SMBIOS entry point, map the table and modify possible identifiable information
 * \return Status of the change
 */
NTSTATUS SMBios::Spoof()
{
	PHYSICAL_ADDRESS biosAddress = { 0 };
	biosAddress.LowPart = 0xF0000;

	auto* bios = static_cast<char*>(MmMapIoSpace(biosAddress, 0x10000, MmNonCached));
	if (!bios)
		return STATUS_UNSUCCESSFUL;

	PHYSICAL_ADDRESS tableAddress = { 0 };
	ULONG size = 0;

	for (ULONG offset = 0; offset + 0x20 <= 0x10000; offset += 16)
	{
		auto* entry = bios + offset;

		if (memcmp(entry, "_SM3_", 5) == 0)
		{
			tableAddress.QuadPart = *reinterpret_cast<ULONGLONG*>(entry + 0x10);
			size = *reinterpret_cast<ULONG*>(entry + 0x0C);
			break;
		}

		if (memcmp(entry, "_SM_", 4) == 0)
		{
			tableAddress.LowPart = *reinterpret_cast<ULONG*>(entry + 0x18);
			size = *reinterpret_cast<BYTE*>(entry + 0x16);
			break;
		}
	}

	MmUnmapIoSpace(bios, 0x10000);

	if (!tableAddress.QuadPart || !size || size > 0x10000)
		return STATUS_UNSUCCESSFUL;

	auto* table = static_cast<char*>(MmMapIoSpace(tableAddress, size, MmNonCached));
	if (!table)
		return STATUS_UNSUCCESSFUL;

	LoopTables(table, size);

	MmUnmapIoSpace(table, size);

	return STATUS_SUCCESS;
}