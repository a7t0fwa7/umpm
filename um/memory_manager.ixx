module;
#include <iostream>
#include <print>
#include <vector>
#include <expected>
#include <optional>
#include <thread>
#include <limits>
#include <chrono>
#include <type_traits>

#include "..\shared.h"

export module memory_manager;
export import superfetch;

namespace mm {
	spf::memory_map map{};
	pte_t* self = nullptr;
	std::uint64_t old_pfn = 0; // the old pfn of the self ref pte which was overwritten by us
	int pt_index = 0; // the index of the self ref pte
	std::vector<int> mappings{};

	export std::expected<void, spf::error> initialize(void* pte, std::uint64_t pfn) {
		self = reinterpret_cast<pte_t*>(pte);
		pt_index = virtual_address_t{ .address = reinterpret_cast<std::uint64_t>(mm::self) }.pt_index;
		old_pfn = pfn;

		if (auto result = map.snapshot(); !result)
			return std::unexpected(result.error());

		return {}; // success
	}

	export void cleanup() {
		if (!self || !old_pfn || !pt_index) return;

		// restore all the mappings, by default we set them to 0
		for (auto index : mappings) {
			self[index].value = 0;
		}

		// the magic value we placed initially should still be there at the original pfn, if we can read it then we know the old_pfn has been restored and were good to exit the process
		while (*reinterpret_cast<std::uint64_t*>(self) != reinterpret_cast<std::uint64_t>(self)) {
			self[pt_index].page_pa = old_pfn;
			FlushProcessWriteBuffers();
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			std::println("Restoring pte...");
		}

		std::println("Everything is restored: {}", self->value == reinterpret_cast<std::uint64_t>(self));
		if (!VirtualFree((void*)self, 0, MEM_RELEASE)) {
			std::println("Failed to free memory: {}", std::error_code(GetLastError(), std::system_category()).message());
		}
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		std::cin.get();
	}

	export std::optional<int> find_free_pte() {
		for (int i = 0; i < 256; i++) {
			auto& pte = self[i];
			if (!pte.present) {
				return i;
			}
		}

		for (auto index : mappings) {
			while (self[index].value != 0)
				self[index].value = 0;
		}

		return std::nullopt;
	}

	// get the virtual address mapped by a given pte index within the same table of the self referencing pte obviously
	export std::uint8_t* get_pte_va(int pt_index) {
		virtual_address_t va{ .address = reinterpret_cast<std::uint64_t>(self) };
		va.pt_index = pt_index;
		return reinterpret_cast<std::uint8_t*>(va.address);
	}

	// maybe have an optional RAII class instead of a callback system
	export template <typename F>
		requires (std::is_invocable_v<F, std::uint8_t*> && !std::is_void_v<std::invoke_result_t<F, std::uint8_t*>>)
	auto map_page(std::uint64_t physical_address, F&& func) -> std::optional<std::invoke_result_t<F, std::uint8_t*>> {
		virtual_address_t pa{ physical_address };
		auto offset = pa.offset;
		pa.offset = 0; // page align the physical address incase its not

		auto free_index = find_free_pte();
		if (!free_index) {
			std::println("Failed to find a free PTE");
			return std::nullopt;
		}

		// insert the modification into a list so it can restored in the cleanup function incase theres an early return here
		mappings.push_back(free_index.value());
		volatile auto& pte = self[free_index.value()];

		// the self referencing pte has all the necessary attributes, we just copy it and modify the pfn
		pte.value = self[pt_index].value;
		pte.page_pa = physical_address >> 12;

		// this is the virtual address mapped by the pte
		auto va = get_pte_va(free_index.value());

		va += offset; // add the offset back incase the physical address wasnt page aligned
		auto result = func(va);

		// restore the pte
		pte.value = 0;
		mappings.pop_back();

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
					auto& _pml4e = reinterpret_cast<pml4e_t*>(va)[address.pml4_index];
					if (!_pml4e.present) {
						return 0;
					}
					return static_cast<std::uint64_t>(_pml4e.page_pa) << 12;
				});

			if (!pml4e || !pml4e.value()) return std::nullopt;
			auto pdpte = map_page(pml4e.value(),
				[&](std::uint8_t* va) -> std::uint64_t {
					auto& _pdpte = reinterpret_cast<pdpte_1gb_t*>(va)[address.pdpt_index];
					if (!_pdpte.present) {
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
					auto& _pde = reinterpret_cast<pde_2mb_t*>(va)[address.pd_index];
					if (!_pde.present) {
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
					auto& _pte = reinterpret_cast<pte_t*>(va)[address.pt_index];
					if (!_pte.present) {
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
				std::println("{:#x} - {:#x}", start, end);

				for (auto current = start; current < end; current += 0x1000) {
					std::println("current: {:#x}", current);
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

			return false;
		}
	};
}