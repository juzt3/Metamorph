#include "service.hpp"

NTSTATUS service::RegisterAndStart(const std::wstring& driver_path) {
	const static DWORD ServiceTypeKernel = 1;
	const std::wstring driver_name = intel_driver::GetDriverNameW();
	const std::wstring servicesPath = L"SYSTEM\\CurrentControlSet\\Services\\" + driver_name;
	const std::wstring nPath = L"\\??\\" + driver_path;

	HKEY dservice;
	LSTATUS status = RegCreateKeyW(HKEY_LOCAL_MACHINE, servicesPath.c_str(), &dservice); //Returns Ok if already exists
	if (status != ERROR_SUCCESS) {
		Log(L"[-] RegCreateKeyW failed: " << status << std::endl);
		return (NTSTATUS)0xC0000001;
	}

	status = RegSetKeyValueW(dservice, NULL, L"ImagePath", REG_EXPAND_SZ, nPath.c_str(), (DWORD)(nPath.size()*sizeof(wchar_t)));
	if (status != ERROR_SUCCESS) {
		RegCloseKey(dservice);
		RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		Log(L"[-] RegSetKeyValueW(ImagePath) failed: " << status << std::endl);
		return (NTSTATUS)0xC0000001;
	}
	
	status = RegSetKeyValueW(dservice, NULL, L"Type", REG_DWORD, &ServiceTypeKernel, sizeof(DWORD));
	if (status != ERROR_SUCCESS) {
		RegCloseKey(dservice);
		RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		Log(L"[-] RegSetKeyValueW(Type) failed: " << status << std::endl);
		return (NTSTATUS)0xC0000001;
	}
	
	RegCloseKey(dservice);

	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	if (ntdll == NULL) {
		RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		Log(L"[-] GetModuleHandleA(ntdll.dll) failed" << std::endl);
		return (NTSTATUS)0xC0000001;
	}

	auto RtlAdjustPrivilege = (nt::RtlAdjustPrivilege)GetProcAddress(ntdll, "RtlAdjustPrivilege");
	auto NtLoadDriver = (nt::NtLoadDriver)GetProcAddress(ntdll, "NtLoadDriver");
	if (!RtlAdjustPrivilege || !NtLoadDriver) {
		RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		Log(L"[-] Failed to resolve RtlAdjustPrivilege/NtLoadDriver" << std::endl);
		return (NTSTATUS)0xC0000001;
	}

	ULONG SE_LOAD_DRIVER_PRIVILEGE = 10UL;
	BOOLEAN SeLoadDriverWasEnabled;
	NTSTATUS ntStatus = RtlAdjustPrivilege(SE_LOAD_DRIVER_PRIVILEGE, TRUE, FALSE, &SeLoadDriverWasEnabled);
	if (!NT_SUCCESS(ntStatus)) {
		RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		Log(L"[-] RtlAdjustPrivilege(SeLoadDriverPrivilege) failed: 0x" << std::hex << ntStatus << std::endl);
		return ntStatus;
	}

	std::wstring wdriver_reg_path = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" + driver_name;
	UNICODE_STRING serviceStr;
	RtlInitUnicodeString(&serviceStr, wdriver_reg_path.c_str());

	ntStatus = NtLoadDriver(&serviceStr);
	Log(L"[+] NtLoadDriver status: 0x" << std::hex << ntStatus << std::endl);

	if (ntStatus == 0xC000010E) { // STATUS_IMAGE_ALREADY_LOADED
		return 0;
	}

	if (!NT_SUCCESS(ntStatus)) {
		if (ntStatus == 0xC0000603) { // STATUS_IMAGE_CERT_REVOKED
			Log(L"[-] Code Integrity / vulnerable driver blocklist rechazo iqvw64e.sys." << std::endl);
			Log(L"[-] VulnerableDriverBlocklistEnable=0, HVCI off y reinicia antes de reintentar." << std::endl);
		}
		else if (ntStatus == 0xC0000428) { // STATUS_INVALID_IMAGE_HASH
			Log(L"[-] Firma del driver no aceptada por Code Integrity." << std::endl);
		}
		else if (ntStatus == 0xC0000022 || ntStatus == 0xC000009A) { // ACCESS_DENIED / INSUFFICIENT_RESOURCES
			Log(L"[-] Antivirus/anticheat bloqueando la carga del driver vulnerable." << std::endl);
		}

		RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		return ntStatus;
	}

	return ntStatus;
}

bool service::StopAndRemove(const std::wstring& driver_name) {
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	if (ntdll == NULL)
		return false;

	std::wstring wdriver_reg_path = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" + driver_name;
	UNICODE_STRING serviceStr;
	RtlInitUnicodeString(&serviceStr, wdriver_reg_path.c_str());

	HKEY driver_service;
	std::wstring servicesPath = L"SYSTEM\\CurrentControlSet\\Services\\" + driver_name;
	LSTATUS status = RegOpenKeyW(HKEY_LOCAL_MACHINE, servicesPath.c_str(), &driver_service);
	if (status != ERROR_SUCCESS) {
		if (status == ERROR_FILE_NOT_FOUND) {
			return true;
		}
		return false;
	}
	RegCloseKey(driver_service);

	auto NtUnloadDriver = (nt::NtUnloadDriver)GetProcAddress(ntdll, "NtUnloadDriver");
	NTSTATUS st = NtUnloadDriver(&serviceStr);
	if (st != 0x0) {
		status = RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		return false; //lets consider unload fail as error because can cause problems with anti cheats later
	}
	

	status = RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
	if (status != ERROR_SUCCESS) {
		return false;
	}
	return true;
}
