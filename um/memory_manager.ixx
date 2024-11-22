module;
#include <iostream>
#include <print>
#include <vector>
#include <expected>
#include <optional>
#include <limits>
#include <chrono>
#include <type_traits>

#include "..\shared.h"

export module memory_manager;
export import superfetch;

namespace mm {
	export spf::memory_map map{};

	pte_t* self = nullptr;
	int pt_index{}; // the index of the self ref pte
	std::uint64_t old_pfn{}; // the old pfn of the self ref pte which was overwritten by us

	// the page that will be remapped to a given physical address
	// TODO: allow as many dummy pages as threads that need access to physical memory
	void* dummy = nullptr;
	int dummy_index{};
	std::uint64_t dummy_pfn{};

	export std::expected<void, spf::error> initialize(void* pte, std::uint64_t pfn) {
		self = reinterpret_cast<pte_t*>(pte);
		pt_index = virtual_address_t{ .address = reinterpret_cast<std::uint64_t>(mm::self) }.pt_index;
		old_pfn = pfn;

		if (auto result = map.snapshot(); !result)
			return std::unexpected(result.error());

		dummy = VirtualAlloc((uint8_t*)(pte)+0x1000, 0x1000, MEM_COMMIT, PAGE_READWRITE);
		if (!dummy) {
			std::println("Failed to allocate the dummy page: {}", std::error_code(GetLastError(), std::system_category()).message());
		}

		if (!VirtualLock(dummy, 0x1000)) {
			std::println("Failed to lock the memory: {}", std::error_code(GetLastError(), std::system_category()).message());
		}

		dummy_index = virtual_address_t{ .address = reinterpret_cast<std::uint64_t>(dummy) }.pt_index;
		dummy_pfn = self[dummy_index].page_pa;
		std::println("Dummy: {:#x}, Dummy PFN: {:#x}", reinterpret_cast<std::uint64_t>(dummy), dummy_pfn);


		return {}; // success
	}

	export void cleanup() {
		if (!self || !old_pfn || !pt_index) return;
		std::println("Self: {:#x}, Old PFN: {}", reinterpret_cast<std::uint64_t>(self), old_pfn);

		// the magic value we placed initially should still be there at the original pfn, 
		// if we can read it then we know the old_pfn has been restored and were good to exit the process
		while (*reinterpret_cast<std::uint64_t*>(self) != reinterpret_cast<std::uint64_t>(self)) {
			self[dummy_index].page_pa = dummy_pfn;
			self[pt_index].page_pa = old_pfn;
			FlushProcessWriteBuffers();
		}

		std::println("Everything is restored: {}", self->value == reinterpret_cast<std::uint64_t>(self));
		if (!VirtualUnlock(self, 0x2000)) {
			std::println("Failed to unlock the memory: {}", std::error_code(GetLastError(), std::system_category()).message());
		}
		if (!VirtualFree(self, 0, MEM_RELEASE)) {
			std::println("Failed to free the memory: {}", std::error_code(GetLastError(), std::system_category()).message());
		}

		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		std::cin.get();
	}

	export template <typename F>
		requires (std::is_invocable_v<F, std::uint8_t*> && !std::is_void_v<std::invoke_result_t<F, std::uint8_t*>>)
	auto map_page(std::uint64_t physical_address, F&& func) -> std::optional<std::invoke_result_t<F, std::uint8_t*>> {
		if (!map.is_valid_pa(physical_address)) {
			return std::nullopt;
		}

		auto offset = physical_address & 0xfff;
		auto free_index = dummy_index;
		auto va = reinterpret_cast<std::uint8_t*>(dummy);

		// the self referencing pte has all the necessary attributes, we just copy it and modify the pfn
		auto copy = self[pt_index];
		copy.page_pa = physical_address >> 12;

		volatile auto& pte = self[free_index];
		auto old_val = pte.value;

		// these protection flags do not affect our access to the dummy page, its always read/write 
		static bool write{};
		DWORD old_protect{};
		DWORD new_protect = write ? PAGE_READONLY : PAGE_READWRITE;

		// force an `invlpg` on the dummy page which will invalidate the cached translation within the tlb, 
		// ensuring our modifications to the page tables are respected by the processor
		if (!VirtualProtect(va, 0x1000, new_protect, &old_protect)) {
			std::println("Failed to change the protection of the page: {}",
				std::error_code(GetLastError(), std::system_category()).message());

			std::unreachable();
			return false;
		}
		write = !write;

		// map, callback, unmap
		pte.value = copy.value;

		va += offset; // add the offset back incase the physical address wasnt page aligned
		auto result = func(va);

		pte.value = old_val;

		return result;
	}

	export class process {
	public:
		std::uint64_t base{};
		std::uint64_t cr3{};

		std::optional<std::uint64_t> translate(std::uint64_t va) {
			virtual_address_t address{ va };
			std::uint64_t result{};

			auto pml4e = map_page(cr3,
				[&](std::uint8_t* va) -> std::uint64_t {
					volatile auto& _pml4e = reinterpret_cast<pml4e_t*>(va)[address.pml4_index];
					if (!_pml4e.present || _pml4e.page_level_write_through || _pml4e.page_level_cache_disable) {

						return 0;
					}
					return static_cast<std::uint64_t>(_pml4e.page_pa) << 12;
				});

			if (!pml4e || !pml4e.value()) return std::nullopt;

			auto pdpte = map_page(pml4e.value(),
				[&](std::uint8_t* va) -> std::uint64_t {
					volatile auto& _pdpte = reinterpret_cast<pdpte_1gb_t*>(va)[address.pdpt_index];
					if (!_pdpte.present || _pdpte.page_level_write_through || _pdpte.page_level_cache_disable) {
						return 0;
					}

					// 1gb
					if (_pdpte.large_page) {
						result =
							(static_cast<std::uint64_t>(_pdpte.page_pa) << 30) +
							(static_cast<std::uint64_t>(address.pd_index) << 21) +
							(static_cast<std::uint64_t>(address.pt_index) << 12) +
							address.offset;
						return 0;
					}

					return static_cast<std::uint64_t>(reinterpret_cast<pdpte_t*>(va)[address.pdpt_index].page_pa) << 12;
				});

			if (result) return result;
			if (!pdpte || !pdpte.value()) return std::nullopt;


			auto pde = map_page(pdpte.value(),
				[&](std::uint8_t* va) -> std::uint64_t {
					volatile auto& _pde = reinterpret_cast<pde_2mb_t*>(va)[address.pd_index];
					if (!_pde.present || _pde.page_level_write_through || _pde.page_level_cache_disable) {
						return 0;
					}

					// 2mb
					if (_pde.large_page) {
						result =
							(static_cast<std::uint64_t>(_pde.page_pa) << 21) +
							(static_cast<std::uint64_t>(address.pt_index) << 12) + address.offset;
						return 0;
					}

					return static_cast<std::uint64_t>(reinterpret_cast<pde_t*>(va)[address.pd_index].page_pa) << 12;
				});

			if (result) return result;
			if (!pde || !pde.value()) return std::nullopt;


			auto pte = map_page(pde.value(),
				[&](std::uint8_t* va) -> std::uint64_t {
					volatile auto& _pte = reinterpret_cast<pte_t*>(va)[address.pt_index];
					if (!_pte.present || _pte.page_level_write_through || _pte.page_level_cache_disable) {
						return 0;
					}
					return static_cast<std::uint64_t>(_pte.page_pa) << 12;
				});

			if (!pte || !pte.value()) return std::nullopt;


			return pte.value() + address.offset;
		}

		bool find_cr3() {
			virtual_address_t base_address{ base };
			for (auto& [start, end] : map.ranges()) {
				//std::println("{:#x} - {:#x}", start, end);

				for (auto current = start; current < end; current += 0x1000) {
					cr3 = current;

					auto result = translate(base);
					if (!result || !result.value()) continue;

					auto dos = map_page(result.value(),
						[](std::uint8_t* va) -> bool {
							return *reinterpret_cast<std::uint16_t*>(va) == IMAGE_DOS_SIGNATURE;
						});

					if (dos && dos.value()) {
						return true;
					}
				}
			}
			cr3 = 0;
			return false;
		}
	};
}