#pragma once

#ifdef _KERNEL_MODE
#include <ntifs.h>
namespace std {
    typedef signed char        int8_t;
    typedef short              int16_t;
    typedef int                int32_t;
    typedef long long          int64_t;
    typedef unsigned char      uint8_t;
    typedef unsigned short     uint16_t;
    typedef unsigned int       uint32_t;
    typedef unsigned long long uint64_t;
}
#else
#define NOMINMAX
#include <Windows.h>
#include <cstdint>
#endif


#ifndef _KERNEL_MODE
namespace driver {
#endif

    enum class control_codes : std::uint32_t {
        create_recursive_pte = CTL_CODE(FILE_DEVICE_UNKNOWN, 999, METHOD_BUFFERED, FILE_SPECIAL_ACCESS),
        invlpg = CTL_CODE(FILE_DEVICE_UNKNOWN, 1000, METHOD_BUFFERED, FILE_SPECIAL_ACCESS),
    };

#ifndef _KERNEL_MODE
}
#endif

#pragma warning(disable: 4201)
union pml4e_t {
    std::uint64_t value;

    struct {
        std::uint64_t present : 1;
        std::uint64_t write : 1;
        std::uint64_t supervisor : 1;
        std::uint64_t page_level_write_through : 1;
        std::uint64_t page_level_cache_disable : 1;
        std::uint64_t accessed : 1;
        std::uint64_t reserved_1 : 1;
        std::uint64_t must_be_zero : 1;
        std::uint64_t ignored_1 : 3;
        std::uint64_t restart : 1;
        std::uint64_t page_pa : 36;
        std::uint64_t reserved_2 : 4;
        std::uint64_t ignored_2 : 11;
        std::uint64_t execute_disable : 1;
    };
};

union pdpte_1gb_t {
    std::uint64_t value;

    struct {
        std::uint64_t present : 1;
        std::uint64_t write : 1;
        std::uint64_t supervisor : 1;
        std::uint64_t page_level_write_through : 1;
        std::uint64_t page_level_cache_disable : 1;
        std::uint64_t accessed : 1;
        std::uint64_t dirty : 1;
        std::uint64_t large_page : 1;
        std::uint64_t global : 1;
        std::uint64_t ignored_1 : 2;
        std::uint64_t restart : 1;
        std::uint64_t pat : 1;
        std::uint64_t reserved_1 : 17;
        std::uint64_t page_pa : 18;
        std::uint64_t reserved_2 : 4;
        std::uint64_t ignored_2 : 7;
        std::uint64_t protection_key : 4;
        std::uint64_t execute_disable : 1;
    };
};

union pdpte_t {
    std::uint64_t value;

    struct {
        std::uint64_t present : 1;
        std::uint64_t write : 1;
        std::uint64_t supervisor : 1;
        std::uint64_t page_level_write_through : 1;
        std::uint64_t page_level_cache_disable : 1;
        std::uint64_t accessed : 1;
        std::uint64_t reserved_1 : 1;
        std::uint64_t large_page : 1;
        std::uint64_t ignored_1 : 3;
        std::uint64_t restart : 1;
        std::uint64_t page_pa : 36;
        std::uint64_t reserved_2 : 4;
        std::uint64_t ignored_2 : 11;
        std::uint64_t execute_disable : 1;
    };
};

union pde_2mb_t {
    std::uint64_t value;

    struct {
        std::uint64_t present : 1;
        std::uint64_t write : 1;
        std::uint64_t supervisor : 1;
        std::uint64_t page_level_write_through : 1;
        std::uint64_t page_level_cache_disable : 1;
        std::uint64_t accessed : 1;
        std::uint64_t dirty : 1;
        std::uint64_t large_page : 1;
        std::uint64_t global : 1;
        std::uint64_t ignored_1 : 2;
        std::uint64_t restart : 1;
        std::uint64_t pat : 1;
        std::uint64_t reserved_1 : 8;
        std::uint64_t page_pa : 27;
        std::uint64_t reserved_2 : 4;
        std::uint64_t ignored_2 : 7;
        std::uint64_t protection_key : 4;
        std::uint64_t execute_disable : 1;
    };
};

union pde_t {
    std::uint64_t value;

    struct {
        std::uint64_t present : 1;
        std::uint64_t write : 1;
        std::uint64_t supervisor : 1;
        std::uint64_t page_level_write_through : 1;
        std::uint64_t page_level_cache_disable : 1;
        std::uint64_t accessed : 1;
        std::uint64_t reserved_1 : 1;
        std::uint64_t large_page : 1;
        std::uint64_t ignored_1 : 3;
        std::uint64_t restart : 1;
        std::uint64_t page_pa : 36;
        std::uint64_t reserved_2 : 4;
        std::uint64_t ignored_2 : 11;
        std::uint64_t execute_disable : 1;
    };
};

union pte_t {
    std::uint64_t value;

    struct {
        std::uint64_t present : 1;
        std::uint64_t write : 1;
        std::uint64_t supervisor : 1;
        std::uint64_t page_level_write_through : 1;
        std::uint64_t page_level_cache_disable : 1;
        std::uint64_t accessed : 1;
        std::uint64_t dirty : 1;
        std::uint64_t pat : 1;
        std::uint64_t global : 1;
        std::uint64_t ignored_1 : 2;
        std::uint64_t restart : 1;
        std::uint64_t page_pa : 36;
        std::uint64_t reserved_1 : 4;
        std::uint64_t ignored_2 : 7;
        std::uint64_t protection_key : 4;
        std::uint64_t execute_disable : 1;
    };
};

union virtual_address_t {
    std::uint64_t address;
    struct {
        std::uint64_t offset : 12;
        std::uint64_t pt_index : 9;
        std::uint64_t pd_index : 9;
        std::uint64_t pdpt_index : 9;
        std::uint64_t pml4_index : 9;
        std::uint64_t reserved : 16;
    };
};

union self_ref_t {
    std::uint64_t address;

    struct {
        std::uint64_t offset : 12;
        std::uint64_t pml4_index : 9;
        std::uint64_t reserved : 43;
    } first;

    struct {
        std::uint64_t offset : 12;
        std::uint64_t pdpt_index : 9;
        std::uint64_t pml4_index : 9;
        std::uint64_t reserved : 34;
    } second;

    struct {
        std::uint64_t offset : 12;
        std::uint64_t pd_index : 9;
        std::uint64_t pdpt_index : 9;
        std::uint64_t pml4_index : 9;
        std::uint64_t reserved : 25;
    } third;
};