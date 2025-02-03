# 1. `shared.h`

### Path:

```
c:\Users\a7t0fwa7\Documents\Github-Work\umpm\shared.h
```

### Purpose:

This header file contains shared definitions and declarations used by both the user-mode and kernel-mode components of your project. It sets up the necessary types, includes, and namespaces based on whether the code is being compiled for kernel-mode or user-mode.

### Breakdown:

1. **Include Guards:**

   - `#pragma once` ensures the file is included only once during compilation.
2. **Mode-Specific Includes and Definitions:**

   - **Kernel Mode (`_KERNEL_MODE` defined):**
     - Includes `<ntifs.h>`, a Windows Driver Kit header for file system drivers.
     - Defines fixed-width integer types (`int8_t`, `uint8_t`, etc.) within the `std` namespace.
   - **User Mode (`_KERNEL_MODE` not defined):**
     - Defines `NOMINMAX` to prevent Windows headers from defining `min` and `max` macros.
     - Includes `<Windows.h>` and `<cstdint>` for Windows API and fixed-width integer types.
3. **Namespace `driver`:**

   - Wrapped in `#ifndef _KERNEL_MODE` to ensure it's only available in user-mode.
   - Defines an `enum class control_codes` with custom IOCTL codes (`create_recursive_pte`, `invlpg`) using the `CTL_CODE` macro.
4. **Disable Specific Compiler Warning:**

   - `#pragma warning(disable: 4201)` disables warning 4201, which relates to nameless struct/union usage.
5. **Union Definitions for Page Table Entries:**

   - Defines several `union` types (`pml4e_t`, `pdpte_1gb_t`, `pdpte_t`, `pde_2mb_t`, `pde_t`, `pte_t`, `virtual_address_t`, `self_ref_t`) representing different levels and types of page table entries in the x86-64 architecture.
   - Each union provides both a 64-bit `value` and a structured view of individual bit fields, allowing for easy manipulation and access to specific parts of the page table entries.
   - `virtual_address_t` and `self_ref_t` are used to dissect virtual addresses and self-referencing addresses, respectively.

### Usage:

This header serves as a foundational piece, providing necessary types and structures for memory management and driver communication. Both UM and KM components include this to ensure consistency in data structures and communication protocols.

---

## 2. Kernel-Mode Driver: `main.cpp`

### Path:

```
c:\Users\a7t0fwa7\Documents\Github-Work\umpm\km\main.cpp
```

### Purpose:

This is the entry point and main implementation of a Windows kernel-mode driver. The driver facilitates low-level memory manipulation by exposing IOCTLs (Input/Output Controls) that can be invoked from user-mode applications.

### Breakdown:

1. **Includes and Definitions:**

   - Includes `<ntifs.h>` for kernel-mode functions and structures.
   - Includes `"../shared.h"` for shared definitions.
   - Includes `<intrin.h>` for compiler intrinsics like `__invlpg`.
2. **Custom `print` Function:**

   - `print`: A helper function to print debug messages using `vDbgPrintEx`.
3. **IOCTL Dispatch Function (`dispatch`):**

   - Handles incoming IOCTL requests.
   - **Supported Control Codes:**
     - `create_recursive_pte`: Creates a recursive Page Table Entry (PTE) for manipulating page tables.
     - `invlpg`: Invalidates a page in the Translation Lookaside Buffer (TLB) for a given virtual address.
   - **Operation for `create_recursive_pte`:**
     - Retrieves the current Process Environment Block (PEB) and translates a given virtual address.
     - Validates and manipulates page table entries to establish a self-referencing PTE.
     - Copies the old Page Frame Number (PFN) to allow restoration.
     - Flushes the TLB entry for the modified virtual address using `__invlpg`.
   - **Operation for `invlpg`:**
     - Invokes `__invlpg` to invalidate a specific virtual address in the TLB.
4. **Driver Entry Point (`DriverEntry`):**

   - Sets up the driver, creating a device (`\\Device\\UMPM`) and a symbolic link (`\\DosDevices\\UMPM`).
   - Sets up major function handlers for `IRP_MJ_CREATE`, `IRP_MJ_CLOSE`, and `IRP_MJ_DEVICE_CONTROL`.
   - Configures the device for buffered I/O and finalizes initialization.
5. **Driver Unload Function (`DriverUnload`):**

   - Cleans up by deleting the symbolic link and the device upon driver unload.

### Usage:

This kernel-mode driver provides the backend for memory manipulation functionalities exposed to user-mode applications. It allows user-mode code to create and manage recursive PTEs, enabling direct interaction with system page tables—a powerful capability often used in advanced memory management tools or systems programming.

---

## 3. User-Mode Driver Interaction: `driver.ixx`

### Path:

```
c:\Users\a7t0fwa7\Documents\Github-Work\umpm\um\driver.ixx
```

### Purpose:

This C++ module (`.ixx` indicates a C++20 module) encapsulates interactions with the kernel-mode driver. It provides functions to initialize communication with the driver and perform IOCTL operations.

### Breakdown:

1. **Module Declaration and Imports:**

   - Declares the module and specifies it as `export module driver;`.
   - Includes `"../shared.h"` for shared definitions.
   - Includes `<print>` and likely other necessary headers (assuming `<print>` is a custom or C++23 header).
2. **Namespace `driver`:**

   - **Handle Management:**
     - `HANDLE handle`: A global handle to the driver device (`\\.\UMPM`).
   - **Initialization Function (`initialize`):**
     - Opens a handle to the driver using `CreateFileA`.
     - Validates the handle and prints an error message if the operation fails.
   - **IOCTL Function (`io`):**
     - Sends IOCTL requests to the driver using `DeviceIoControl`.
     - Accepts a control code (`control_codes`), input buffer, input size, and optional output buffer and size.
     - Prints error messages if the IOCTL call fails.
     - Returns `true` on success and `false` on failure.

### Usage:

This module abstracts the complexity of interacting with the kernel-mode driver. It allows other user-mode components to initialize communication with the driver and send IOCTLs without dealing directly with Windows API calls. This abstraction simplifies the code in other parts of the user-mode application.

---

## 4. Superfetch Interaction: `superfetch.ixx`

### Path:

```
c:\Users\a7t0fwa7\Documents\Github-Work\umpm\um\superfetch.ixx
```

### Purpose:

This C++ module interacts with the Windows Superfetch service to retrieve and manipulate physical memory ranges and page frame data. It's a modified version of an existing project to work with C++20 modules and facilitate physical memory range iteration.

### Breakdown:

1. **Module Declaration and Imports:**

   - Declares the module as `export module superfetch;`.
   - Includes necessary headers like `<Windows.h>`, `<winternl.h>`, and C++ standard headers.
   - Imports the previously defined `shared.h` and `memory_manager.ixx` modules.
2. **Namespace `spf`:**

   - **Enumerations and Structures:**

     - `SUPERFETCH_INFORMATION_CLASS`: Enumerates different Superfetch information classes for querying and setting parameters.
     - Various structures (`SUPERFETCH_INFORMATION`, `MEMORY_FRAME_INFORMATION`, `FILEOFFSET_INFORMATION`, etc.) represent different data structures used in Superfetch interactions.
   - **Constants:**

     - Defines privilege constants like `SE_PROF_SINGLE_PROCESS_PRIVILEGE` and `SE_DEBUG_PRIVILEGE`.
     - Defines `SystemSuperfetchInformation` as a specific system information class.
   - **External Function Declaration:**

     - `RtlAdjustPrivilege`: Adjusts the privileges of the current process, required to interact with Superfetch.
   - **Utility Structures:**

     - `memory_range`: Represents a range of physical memory with a base PFN and page count.
   - **Error Handling:**

     - Defines an `enum class error` for various error types (`raise_privilege`, `query_ranges`, `query_pfn`).
   - **Type Aliases:**

     - `spf_error`, `memory_ranges`, `memory_translations` for better code readability.
   - **Class `memory_map`:**

     - **Public Methods:**
       - `snapshot()`: Takes a snapshot of the current memory map by querying Superfetch.
       - `va_to_pa()`, `pa_to_va()`: Convert virtual addresses to physical addresses and vice versa.
       - `is_valid_pa()`: Validates if a physical address is within known ranges.
       - Accessors for memory ranges and translations.
     - **Private Methods:**
       - `raise_privilege()`: Adjusts process privileges to interact with Superfetch.
       - `query_memory_ranges()`, `query_memory_ranges_v1()`, `query_memory_ranges_v2()`: Query different versions of memory range information from Superfetch.
       - `query_superfetch_info()`: Helper to send queries to Superfetch using `NtQuerySystemInformation`.
     - **Private Members:**
       - `ranges_`, `physical_memory_ranges`, `translations_`: Store memory range and translation data.
   - **Implementation of `memory_map` Methods:**

     - **`snapshot()`:** Raises necessary privileges, queries memory ranges, and populates the translations map by querying PFN data.
     - **`translate()`:** Translates a virtual address to a physical address using the memory map.
     - **`find_cr3()`:** Attempts to find the `CR3` register value (which points to the base of the page tables) for a given process base address.
   - **Helper Functions and Constants:**

     - Functions for adjusting privileges, querying Superfetch, and handling different Superfetch information classes.

### Usage:

This module provides comprehensive functionality to interact with Windows Superfetch, allowing the application to retrieve and manage physical memory information. It likely plays a critical role in mapping user-mode virtual addresses to physical addresses by leveraging Superfetch's memory management data.

---

## 5. Memory Management: `memory_manager.ixx`

### Path:

```
c:\Users\a7t0fwa7\Documents\Github-Work\umpm\um\memory_manager.ixx
```

### Purpose:

This C++ module manages memory mappings, translating between virtual and physical addresses, and provides mechanisms to manipulate page tables through the previously defined driver and Superfetch interactions.

### Breakdown:

1. **Module Declaration and Imports:**

   - Declares the module and exports it as `export module memory_manager;`.
   - Imports the `superfetch` module and `"../shared.h"`.
2. **Namespace `mm`:**

   - **Global Variables:**

     - `spf::memory_map map`: An instance of the `memory_map` class from the `superfetch` module.
     - `pte_t* self`: Pointer to the self-referencing Page Table Entry (PTE).
     - `int pt_index`: Index of the self-referencing PTE.
     - `std::uint64_t old_pfn`: Stores the original Page Frame Number (PFN) before it was overwritten.
     - `void* dummy`: Pointer to a dummy page used for remapping.
     - `int dummy_index`: Index of the dummy PTE.
     - `std::uint64_t dummy_pfn`: PFN of the dummy page.
   - **Function `initialize`:**

     - Initializes the memory manager by setting up the self-referencing PTE and taking a snapshot of the memory map.
     - Allocates and locks a dummy page for remapping.
     - Prints debug information about the dummy and self-referencing PTEs.
   - **Function `cleanup`:**

     - Restores the original PFN and cleans up allocated resources.
     - Ensures that the memory mappings are reverted to prevent system instability.
   - **Template Function `map_page`:**

     - Maps a physical address to a user-mode virtual address and allows a callback to operate on the mapped memory.
     - Validates the physical address and applies the desired page table modifications.
     - Invokes the provided function (`func`) with the virtual address corresponding to the physical address.
     - Restores the original page table entry after the callback.
   - **Class `process`:**

     - Represents a process and contains methods to translate virtual addresses to physical addresses.
     - **Members:**
       - `std::uint64_t base`: Base address of the process.
       - `std::uint64_t cr3`: CR3 register value pointing to the process's page tables.
     - **Method `translate`:**
       - Translates a virtual address to a physical address by traversing the page tables.
       - Utilizes the `map_page` function to access different levels of the page tables (PML4E, PDPTE, PDE, PTE).
     - **Method `find_cr3`:**
       - Attempts to locate the CR3 register value for the process by iterating through physical memory ranges.
       - Validates the found CR3 by translating the base address and checking for a valid DOS signature.
3. **Error Handling and Type Definitions:**

   - Utilizes `std::expected` for error handling, allowing functions to return either a value or an error.
   - Defines `memory_ranges` and `memory_translations` as convenient aliases for data structures managing memory mappings.

### Usage:

This module orchestrates the memory mapping and translation processes by interfacing with both the driver and Superfetch modules. It allows the application to perform advanced memory manipulations, such as translating virtual addresses to physical addresses and vice versa, which can be essential for tasks like debugging, memory analysis, or developing low-level system utilities.

---

## 6. User-Mode Main Application: `main.cpp`

### Path:

```
c:\Users\a7t0fwa7\Documents\Github-Work\umpm\um\main.cpp
```

### Purpose:

This is the entry point of the user-mode application. It initializes the driver, sets up memory management, and provides examples of how to use the memory mapping functionalities.

### Breakdown:

1. **Includes:**

   - `<print>`: Likely a custom or C++23 header for printing.
   - Standard C++ headers: `<iostream>`, `<chrono>`, `<vector>`, `<expected>`, etc.
   - Includes `"../shared.h"` for shared definitions.
   - Includes `<Windows.h>` for Windows API functions.
   - Imports `memory_manager` and `driver` modules.
2. **Vectored Exception Handler (`vectored_handler`):**

   - Catches exceptions during execution.
   - Prints the exception code.
   - Cleans up the memory manager and exits the process gracefully to prevent system instability.
3. **`main` Function:**

   - **Initialization:**
     - Adds the vectored exception handler.
     - Initializes the driver using `driver::initialize()`.
   - **Memory Allocation:**
     - Reserves and commits memory pages for the self-referencing PTE and a dummy page.
     - Locks the allocation in memory and fills it with a magic value (`-1`).
     - Sets up the self-referencing PTE by writing the allocation address into itself.
   - **Driver IOCTL Calls:**
     - Sends an IOCTL to `create_recursive_pte` to set up the recursive PTE with the allocated memory.
     - If successful, initializes the memory manager (`mm::initialize`).
   - **Examples (Commented Out):**
     - Provides examples of how to use `mm::map_page` to iterate over memory or map specific pages.
   - **Process Handling:**
     - Attempts to find the `CR3` register value for the current process.
     - Measures and prints the time taken to find `CR3`.
     - If successful, translates the base address and prints the physical address.
   - **Cleanup:**
     - Waits for user input (`std::cin.get()`) before cleaning up the memory manager.

### Usage:

This application demonstrates how to set up and utilize the memory management functionalities provided by the other modules. It establishes communication with the kernel-mode driver, allocates necessary memory for PTE manipulation, and interacts with the system's memory tables to perform address translations. The commented-out sections suggest that it can be extended to perform more comprehensive memory mapping and analysis tasks.

---

## Overall Project Summary

Your project consists of both user-mode and kernel-mode components designed to interact with and manipulate the system's memory structures at a low level. Here's how the components fit together:

1. **Shared Definitions (`shared.h`):** Provides common types and structures used by both UM and KM components, ensuring consistency in data representation.
2. **Kernel-Mode Driver (`km\main.cpp`):**

   - Exposes IOCTL interfaces to perform memory manipulations.
   - Handles requests to create recursive PTEs and invalidate TLB entries.
   - Facilitates direct interaction with system page tables.
3. **User-Mode Modules:**

   - **Driver Interaction (`driver.ixx`):** Manages communication with the kernel-mode driver, sending IOCTLs as needed.
   - **Superfetch Interaction (`superfetch.ixx`):** Interfaces with Windows Superfetch to retrieve physical memory ranges and page frame information.
   - **Memory Management (`memory_manager.ixx`):** Provides high-level abstractions for memory mapping, translating addresses, and interacting with page tables using the driver and Superfetch data.
4. **User-Mode Application (`um\main.cpp`):**

   - Serves as the main executable that initializes all components.
   - Demonstrates how to set up memory mappings and perform address translations.
   - Includes exception handling to ensure system stability.

### Potential Use Cases:

- **Memory Analysis Tools:** Analyze and map physical memory usage by translating virtual addresses to physical frames.
- **Debugging Utilities:** Enhanced debugging capabilities by directly interacting with page tables.
- **Custom Memory Managers:** Implement specialized memory management strategies beyond what the operating system provides.

### Important Considerations:

- **System Stability:** Manipulating page tables and physical memory can lead to system instability or crashes if not handled correctly.
- **Security Implications:** Such low-level access can be exploited maliciously; ensure appropriate safeguards and permissions.
- **Compatibility:** Ensure compatibility with the target Windows versions and configurations, as page table structures and Superfetch behavior may vary.

---
