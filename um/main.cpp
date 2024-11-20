#include <print>
#include <iostream>

#include "..\shared.h"
#include <Windows.h>

import memory_manager;

// vectord exception handler
LONG vectored_handler(PEXCEPTION_POINTERS exception) {
	std::println("Exception caught: {:#x}", exception->ExceptionRecord->ExceptionCode);

	mm::cleanup();
	return EXCEPTION_CONTINUE_SEARCH;
}


int main() {
	AddVectoredExceptionHandler(1, vectored_handler);

	HANDLE driver = CreateFileA("\\\\.\\UMPM", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (driver == INVALID_HANDLE_VALUE) {
		std::println("Failed to open driver handle: {}", std::error_code(GetLastError(), std::system_category()).message());
		return 1;
	}

	mm::self = (pte_t*)VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	memset(mm::self, 0xFF, 0x1000);
	*(void**)mm::self = mm::self;
	std::println("{}", (void*)(mm::self));

	DWORD bytes_returned = 0;
	auto success = DeviceIoControl(driver, static_cast<uint32_t>(control_codes::create_recursive_pte), mm::self, 0x1000, &mm::old_pfn, sizeof(mm::old_pfn), &bytes_returned, nullptr);

	if (!success) {
		std::println("{}", std::error_code(GetLastError(), std::system_category()).message());
		CloseHandle(driver);
		return 1;
	}
	CloseHandle(driver);

	mm::pt_index = virtual_address_t{ .address = reinterpret_cast<uint64_t>(mm::self) }.pt_index;

	mm::map_page(0x1ad000, [](uint8_t* va) {
		for (int i = 256; i < 512; i++) {
			std::println("[{}]: {:#x}", i, static_cast<uint64_t>(reinterpret_cast<pml4e_t*>(va)[i].page_pa) << 12);
		}
		});


	mm::cleanup();
}