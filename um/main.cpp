#include <print>
#include <iostream>
#include <chrono>

#include "..\shared.h"
#include <Windows.h>

import memory_manager;
import driver;

// vectord exception handler
LONG vectored_handler(PEXCEPTION_POINTERS exception) {
	std::println("exception code: {:#x}", exception->ExceptionRecord->ExceptionCode);
	mm::cleanup();
	std::cin.get();
	ExitProcess(1);
}

int main() {
	AddVectoredExceptionHandler(1, vectored_handler);
	driver::initialize();

	// reserve a page for the self referencing pte and another page that gets continiously remapped to the target physical address
	auto allocation = VirtualAlloc(nullptr, 0x2000, MEM_RESERVE, PAGE_NOACCESS);

	allocation = VirtualAlloc(allocation, 0x1000, MEM_COMMIT, PAGE_READWRITE);
	memset(allocation, -1, 0x1000);
	if (!VirtualLock(allocation, 0x1000))
		std::println("failed to lock");

	// we place a magic value at the start, necessary for us and the driver
	*reinterpret_cast<std::uint64_t*>(allocation) = reinterpret_cast<std::uint64_t>(allocation);

	std::uint64_t old_pfn{};
	DWORD bytes_returned = 0;
	auto success = driver::io(driver::control_codes::create_recursive_pte, allocation, sizeof(allocation), &old_pfn, sizeof(old_pfn));
	if (!success) {
		std::println("{}", std::error_code(GetLastError(), std::system_category()).message());
		return 1;
	}

	if (auto r = mm::initialize(allocation, old_pfn); !r)
	{
		std::println("failed to initialize memory manager: {}", std::to_underlying(r.error()));
		return 1;
	}

	// Example
	//mm::map_page(0x1ad000, [](std::uint8_t* va) -> int {
	//	for (int i = 256; i < 512; ++i) {
	//		std::println("[{}] = {:#x}", i, static_cast<std::uint64_t>(reinterpret_cast<pml4e_t*>(va)[i].page_pa));
	//	}
	//	return 69;
	//	});

	//for (auto [start, end] : mm::map.ranges()) {
	//	std::println("start: {:#x} end: {:#x}", start, end);
	//
	//	for (auto current = start; current < end; current += 0x1000) {
	//		std::print("current: {:#x} ", current);
	//		mm::map_page(current,
	//			[](std::uint8_t* va) -> int {
	//				std::println("{:#x}", *reinterpret_cast<std::uint64_t*>(va));
	//				return 69;
	//			});
	//	}
	//}

	// attempt to find the page tables of our own process
	mm::process p{
		.base = reinterpret_cast<std::uint64_t>(GetModuleHandleA(nullptr))
	};

	auto start = std::chrono::high_resolution_clock::now();
	auto r = p.find_cr3();
	auto end = std::chrono::high_resolution_clock::now();
	std::println("Elapsed time: {} | {} | {}",
		(end - start),
		std::chrono::duration_cast<std::chrono::microseconds>(end - start),
		std::chrono::duration_cast<std::chrono::milliseconds>(end - start));
	
	if (r) {
		std::println("found cr3: {:#x}", p.cr3);
		std::println("{:#x}", p.translate(p.base).value_or(-1));
	}
	else
		std::println("failed to find cr3");

	
	

	std::cin.get();
	mm::cleanup();
}