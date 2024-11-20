module;
#include <iostream>
#include <print>
#include <vector>
#include <optional>

#include "..\..\shared.h"

export module memory_manager;

export namespace mm {
	pte_t* self = nullptr;
	uint64_t old_pfn = 0;
	int pt_index = 0;

	std::vector<int> mappings{};

	void cleanup() {
		if (!self || !old_pfn || !pt_index) return;

		// restore all the mappings, by default we set them to 0
		for (auto index : mappings) {
			self[index].value = 0;
		}

		// copy the old_pfn into the self referencing pte so the windows vmm doesnt bugcheck
		self[pt_index].page_pa = old_pfn;
	}

	std::optional<int> find_free_pte() {
		for (int i = 0; i < 256; i++) {
			auto& pte = self[i];
			if (!pte.present) {
				std::println("Found free PTE at [{}]", i);
				return i;
			}
		}
		return std::nullopt;
	}

	uint8_t* get_pte_va(int pt_index) {
		virtual_address_t va{ .address = reinterpret_cast<uint64_t>(self) };
		va.pt_index = pt_index;
		return reinterpret_cast<uint8_t*>(va.address);
	}

	template <typename T>
	requires (std::is_invocable_v<T, uint8_t*>)
	void map_page(uint64_t physical_address, T&& func) {
		auto free_index = find_free_pte();
		if (!free_index) {
			std::println("Failed to find a free PTE");
			return;
		}

		// insert the modification into a list so it can restored in the cleanup function incase theres an early return here
		mappings.push_back(free_index.value());
		auto& pte = self[free_index.value()];

		// the self referencing pte has all the necessary attributes, we just copy it and modify the pfn
		pte.value = self[pt_index].value;
		pte.page_pa = physical_address >> 12;

		// this is the virtual address mapped by the pte
		auto va = get_pte_va(free_index.value());
		func(va);

		pte.value = 0;
		mappings.pop_back();
	}
}