module;
#include <iostream>
#include <unordered_map>
#include <map>
#include <vector>
#include <expected>

#include <Windows.h>
#include <winternl.h>
#pragma comment(lib, "ntdll.lib")

export module superfetch;
/*
* This is a modified version of `https://github.com/jonomango/superfetch`, credits to the original author.
* It has been modified to work with C++20 modules and allow iterating over the physical memory ranges just like through MmGetPhysicalMemoryRanges in the kernel.
*/

export namespace spf {
    enum SUPERFETCH_INFORMATION_CLASS {
        SuperfetchRetrieveTrace = 1, // Query
        SuperfetchSystemParameters = 2, // Query
        SuperfetchLogEvent = 3, // Set
        SuperfetchGenerateTrace = 4, // Set
        SuperfetchPrefetch = 5, // Set
        SuperfetchPfnQuery = 6, // Query
        SuperfetchPfnSetPriority = 7, // Set
        SuperfetchPrivSourceQuery = 8, // Query
        SuperfetchSequenceNumberQuery = 9, // Query
        SuperfetchScenarioPhase = 10, // Set
        SuperfetchWorkerPriority = 11, // Set
        SuperfetchScenarioQuery = 12, // Query
        SuperfetchScenarioPrefetch = 13, // Set
        SuperfetchRobustnessControl = 14, // Set
        SuperfetchTimeControl = 15, // Set
        SuperfetchMemoryListQuery = 16, // Query
        SuperfetchMemoryRangesQuery = 17, // Query
        SuperfetchTracingControl = 18, // Set
        SuperfetchTrimWhileAgingControl = 19,
        SuperfetchInformationMax = 20
    };

    struct SUPERFETCH_INFORMATION {
        ULONG Version = 45;
        ULONG Magic = 'kuhC';
        SUPERFETCH_INFORMATION_CLASS InfoClass;
        PVOID Data;
        ULONG Length;
    };

    struct MEMORY_FRAME_INFORMATION {
        ULONGLONG UseDescription : 4;
        ULONGLONG ListDescription : 3;
        ULONGLONG Reserved0 : 1;
        ULONGLONG Pinned : 1;
        ULONGLONG DontUse : 48;
        ULONGLONG Priority : 3;
        ULONGLONG Reserved : 4;
    };

    struct FILEOFFSET_INFORMATION {
        ULONGLONG DontUse : 9;
        ULONGLONG Offset : 48;
        ULONGLONG Reserved : 7;
    };

    struct PAGEDIR_INFORMATION {
        ULONGLONG DontUse : 9;
        ULONGLONG PageDirectoryBase : 48;
        ULONGLONG Reserved : 7;
    };

    struct UNIQUE_PROCESS_INFORMATION {
        ULONGLONG DontUse : 9;
        ULONGLONG UniqueProcessKey : 48;
        ULONGLONG Reserved : 7;
    };

    struct MMPFN_IDENTITY {
        union {
            MEMORY_FRAME_INFORMATION e1;
            FILEOFFSET_INFORMATION e2;
            PAGEDIR_INFORMATION e3;
            UNIQUE_PROCESS_INFORMATION e4;
        } u1;

        SIZE_T PageFrameIndex;

        union {
            struct {
                ULONG Image : 1;
                ULONG Mismatch : 1;
            } e1;

            PVOID FileObject;
            PVOID UniqueFileObjectKey;
            PVOID ProtoPteAddress;
            PVOID VirtualAddress;
        } u2;
    };

    struct SYSTEM_MEMORY_LIST_INFORMATION {
        SIZE_T ZeroPageCount;
        SIZE_T FreePageCount;
        SIZE_T ModifiedPageCount;
        SIZE_T ModifiedNoWritePageCount;
        SIZE_T BadPageCount;
        SIZE_T PageCountByPriority[8];
        SIZE_T RepurposedPagesByPriority[8];
        ULONG_PTR ModifiedPageCountPageFile;
    };

    struct PF_PFN_PRIO_REQUEST {
        ULONG Version;
        ULONG RequestFlags;
        SIZE_T PfnCount;
        SYSTEM_MEMORY_LIST_INFORMATION MemInfo;
        MMPFN_IDENTITY PageData[ANYSIZE_ARRAY];
    };

    struct PF_PHYSICAL_MEMORY_RANGE {
        ULONG_PTR BasePfn;
        ULONG_PTR PageCount;
    };

    struct PF_MEMORY_RANGE_INFO_V1 {
        ULONG Version = 1;
        ULONG RangeCount;
        PF_PHYSICAL_MEMORY_RANGE Ranges[ANYSIZE_ARRAY];
    };

    struct PF_MEMORY_RANGE_INFO_V2 {
        ULONG Version = 2;
        ULONG Flags;
        ULONG RangeCount;
        PF_PHYSICAL_MEMORY_RANGE Ranges[ANYSIZE_ARRAY];
    };

    inline constexpr ULONG SE_PROF_SINGLE_PROCESS_PRIVILEGE = 13;
    inline constexpr ULONG SE_DEBUG_PRIVILEGE = 20;

    inline constexpr SYSTEM_INFORMATION_CLASS SystemSuperfetchInformation = SYSTEM_INFORMATION_CLASS(79);

    extern "C" NTSYSAPI NTSTATUS RtlAdjustPrivilege(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);

    struct memory_range {
        std::uint64_t pfn = 0;
        std::size_t page_count = 0;
    };

    enum class error {
		raise_privilege,
		query_ranges,
		query_pfn
	};

    using spf_error = error;

    using memory_ranges = std::vector<memory_range>;
    using memory_translations = std::unordered_map<void const*, std::uint64_t>;

    class memory_map {
    public:
        std::expected<void, spf_error> snapshot();

        inline std::uint64_t va_to_pa(std::uint64_t virtual_address) const {
            auto aligned_va = reinterpret_cast<void const*>(virtual_address & ~0xFFFull);

            if (auto const it = translations_.find(aligned_va); it != end(translations_))
                return it->second + (virtual_address & 0xFFF);

            return 0;
        }

        inline std::uint64_t pa_to_va(std::uint64_t physical_address) const {
            auto aligned_pa = physical_address & ~0xFFFull;

            for (const auto& [va, pa] : translations_) {
                if (aligned_pa == pa)
                    return reinterpret_cast<std::uint64_t>(va) + (physical_address & 0xFFF);
            }

            return 0;
        }

        inline bool is_valid_pa(std::uint64_t physical_address) const {
            for (const auto& [start, end] : physical_memory_ranges) {
                if (physical_address >= start && physical_address <= end) {
					return true;
				}
			}

			return false;
		}

        const std::map<std::uintptr_t, std::uintptr_t>& ranges();
        memory_translations const& translations() const;

        memory_map() = default;
        ~memory_map() = default;
        memory_map(const memory_map&) = delete;
        memory_map& operator=(const memory_map&) = delete;
        memory_map(memory_map&& other) noexcept = default;
        memory_map& operator=(memory_map&& other) noexcept = default;

    private:
        static bool raise_privilege();

        static memory_ranges query_memory_ranges();

        static memory_ranges query_memory_ranges_v1();

        static memory_ranges query_memory_ranges_v2();

        static NTSTATUS query_superfetch_info(
            SUPERFETCH_INFORMATION_CLASS info_class,
            PVOID buffer,
            ULONG length,
            PULONG return_length = nullptr
        );

    private:
        memory_ranges ranges_ = {};
        std::map<std::uintptr_t, std::uintptr_t> physical_memory_ranges{}; // {start: end} physical address
        memory_translations translations_ = {};
    };

    inline std::expected<void, spf_error> memory_map::snapshot() {
        if (!raise_privilege())
            return std::unexpected(spf_error::raise_privilege);

        this->ranges_ = query_memory_ranges();

        if (ranges_.empty())
            return std::unexpected(spf_error::query_ranges);

        for (auto const& [base_pfn, page_count] : ranges_) {
            std::size_t const buffer_length = sizeof(PF_PFN_PRIO_REQUEST) +
                sizeof(MMPFN_IDENTITY) * page_count;

            auto const buffer = std::make_unique<std::uint8_t[]>(buffer_length);
            auto const request = reinterpret_cast<PF_PFN_PRIO_REQUEST*>(buffer.get());
            request->Version = 1;
            request->RequestFlags = 1;
            request->PfnCount = page_count;

            for (std::uint64_t i = 0; i < page_count; ++i)
                request->PageData[i].PageFrameIndex = base_pfn + i;

            if (!NT_SUCCESS(query_superfetch_info(
                SuperfetchPfnQuery, request, static_cast<ULONG>(buffer_length))))
                return std::unexpected(spf_error::query_pfn);

            for (std::uint64_t i = 0; i < page_count; ++i) {
                if (void const* const virt = request->PageData[i].u2.VirtualAddress)
                    translations_[virt] = (base_pfn + i) << 12;
            }
        }

        return {};
    }

    inline const std::map<std::uintptr_t, std::uintptr_t>& memory_map::ranges() {
        for (const auto& range : ranges_) {
            auto start = range.pfn << 12;
            auto bytes = range.page_count * 0x1000;
            auto end = start + bytes;

            physical_memory_ranges[start] = end;
        }

        return physical_memory_ranges;
    }


    inline memory_translations const& memory_map::translations() const {
        return translations_;
    }

    inline bool memory_map::raise_privilege() {
        BOOLEAN old = FALSE;

        if (!NT_SUCCESS(RtlAdjustPrivilege(
            SE_PROF_SINGLE_PROCESS_PRIVILEGE, TRUE, FALSE, &old)))
            return false;

        if (!NT_SUCCESS(RtlAdjustPrivilege(
            SE_DEBUG_PRIVILEGE, TRUE, FALSE, &old)))
            return false;

        return true;
    }

    inline memory_ranges memory_map::query_memory_ranges() {
        auto ranges = query_memory_ranges_v1();
        if (ranges.empty())
            return query_memory_ranges_v2();
        return ranges;
    }

    inline memory_ranges memory_map::query_memory_ranges_v1() {
        ULONG buffer_length = 0;

        if (PF_MEMORY_RANGE_INFO_V1 info = {}; 0xC0000023 != query_superfetch_info(
            SuperfetchMemoryRangesQuery, &info, sizeof(info), &buffer_length))
            return {};

        auto const buffer = std::make_unique<std::uint8_t[]>(buffer_length);
        auto const info = reinterpret_cast<PF_MEMORY_RANGE_INFO_V1*>(buffer.get());
        info->Version = 1;

        if (!NT_SUCCESS(query_superfetch_info(
            SuperfetchMemoryRangesQuery, info, buffer_length)))
            return {};

        memory_ranges ranges = {};

        for (std::uint32_t i = 0; i < info->RangeCount; ++i) {
            ranges.push_back({
                .pfn = info->Ranges[i].BasePfn,
                .page_count = info->Ranges[i].PageCount
                });
        }

        return ranges;
    }

    inline memory_ranges memory_map::query_memory_ranges_v2() {
        ULONG buffer_length = 0;

        if (PF_MEMORY_RANGE_INFO_V2 info = {}; 0xC0000023 != query_superfetch_info(
            SuperfetchMemoryRangesQuery, &info, sizeof(info), &buffer_length))
            return {};

        auto const buffer = std::make_unique<std::uint8_t[]>(buffer_length);
        auto const info = reinterpret_cast<PF_MEMORY_RANGE_INFO_V2*>(buffer.get());
        info->Version = 2;

        if (!NT_SUCCESS(query_superfetch_info(
            SuperfetchMemoryRangesQuery, info, buffer_length)))
            return {};

        memory_ranges ranges = {};

        for (std::uint32_t i = 0; i < info->RangeCount; ++i) {
            ranges.push_back({
                .pfn = info->Ranges[i].BasePfn,
                .page_count = info->Ranges[i].PageCount
                });
        }

        return ranges;
    }

    inline NTSTATUS memory_map::query_superfetch_info(
        SUPERFETCH_INFORMATION_CLASS info_class,
        PVOID buffer,
        ULONG length,
        PULONG return_length
    ) {
        SUPERFETCH_INFORMATION superfetch_info = {
            .InfoClass = info_class,
            .Data = buffer,
            .Length = length
        };

        return NtQuerySystemInformation(SystemSuperfetchInformation,
            &superfetch_info, sizeof(superfetch_info), return_length);
    }
}