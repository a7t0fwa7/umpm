#include <print>
#include <iostream>

#include "..\shared.h"
#include <Windows.h>

import memory_manager;

// vectord exception handler
LONG vectored_handler(PEXCEPTION_POINTERS exception) {
	std::println("Exception caught: {:#x}", exception->ExceptionRecord->ExceptionCode);

	mm::cleanup();
	std::cin.get();
	ExitProcess(1);
}

int main() {
	AddVectoredExceptionHandler(1, vectored_handler);

	HANDLE driver = CreateFileA("\\\\.\\UMPM", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (driver == INVALID_HANDLE_VALUE) {
		std::println("Failed to open driver handle: {}", std::error_code(GetLastError(), std::system_category()).message());
		return 1;
	}

	auto allocation = VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	memset(allocation, 0xFF, 0x1000);
	if (!VirtualLock(allocation, 0x1000))
		std::println("failed to lock");

	// we place a magic value at the start, necessary for us and the driver
	*reinterpret_cast<std::uint64_t*>(allocation) = reinterpret_cast<std::uint64_t>(allocation);
	std::uint64_t old_pfn{};
	DWORD bytes_returned = 0;
	auto success = DeviceIoControl(driver, static_cast<std::uint32_t>(control_codes::create_recursive_pte), allocation, 0x1000, &old_pfn, sizeof(old_pfn), &bytes_returned, nullptr);

	if (!success) {
		std::println("{}", std::error_code(GetLastError(), std::system_category()).message());
		CloseHandle(driver);
		return 1;
	}
	CloseHandle(driver);



	mm::initialize(allocation, old_pfn);

	//void* base = GetModuleHandleA(nullptr);
	//mm::process p{
	//	.base = reinterpret_cast<std::uint64_t>(base),
	//};
	//std::println("Found: {} - {}", p.find_cr3(), p.cr3);

	// Example
	mm::map_page(0x1ad000, [](std::uint8_t* va) -> int {
		for (int i = 256; i < 512; ++i) {
			std::println("[{}] = {:#x}", i, static_cast<std::uint64_t>(reinterpret_cast<pml4e_t*>(va)[i].page_pa));
		}
		return 69;
		});


	mm::cleanup();
}