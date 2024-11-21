module;

#include <print>

#include "..\shared.h"

export module driver;

/*
* This is not a dependency of the project, its a palceholder that allows us to achieve the initial page table manipulation. Can be done through anything really.
*/

namespace driver {
	HANDLE handle = nullptr;

	export void initialize() {
		handle = CreateFileA("\\\\.\\UMPM", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (handle == INVALID_HANDLE_VALUE) {
			std::println("Failed to open driver handle: {}", std::error_code(GetLastError(), std::system_category()).message());
			return;
		}
	}

	export bool io(control_codes c, void* input, size_t input_size, void* output = nullptr, size_t output_size = 0) {
		if (handle == nullptr) {
			std::println("Driver handle is not initialized");
			return false;
		}

		if (!DeviceIoControl(handle, std::to_underlying(c), input, input_size, output, output_size, nullptr, nullptr)) {
			std::println("Failed to send IOCTL: {}", std::error_code(GetLastError(), std::system_category()).message());
			return false;
		}

		return true;
	}
}