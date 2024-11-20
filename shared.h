#pragma once

#ifdef _KERNEL_MODE
#include <ntifs.h>
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
#else
#include <Windows.h>
#include <cstdint>
#endif


enum class control_codes {
	create_recursive_pte = CTL_CODE(FILE_DEVICE_UNKNOWN, 999, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
};

#pragma warning(disable: 4201)
union pml4e_t {
    uint64_t value;

    struct {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t supervisor : 1;
        uint64_t page_level_write_through : 1;
        uint64_t page_level_cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t reserved_1 : 1;
        uint64_t must_be_zero : 1;
        uint64_t ignored_1 : 3;
        uint64_t restart : 1;
        uint64_t page_pa : 36;
        uint64_t reserved_2 : 4;
        uint64_t ignored_2 : 11;
        uint64_t execute_disable : 1;
    };
};

union pdpte_1gb_t {
    uint64_t value;

    struct {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t supervisor : 1;
        uint64_t page_level_write_through : 1;
        uint64_t page_level_cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t large_page : 1;
        uint64_t global : 1;
        uint64_t ignored_1 : 2;
        uint64_t restart : 1;
        uint64_t pat : 1;
        uint64_t reserved_1 : 17;
        uint64_t page_pa : 18;
        uint64_t reserved_2 : 4;
        uint64_t ignored_2 : 7;
        uint64_t protection_key : 4;
        uint64_t execute_disable : 1;
    };
};

union pdpte_t {
    uint64_t value;

    struct {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t supervisor : 1;
        uint64_t page_level_write_through : 1;
        uint64_t page_level_cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t reserved_1 : 1;
        uint64_t large_page : 1;
        uint64_t ignored_1 : 3;
        uint64_t restart : 1;
        uint64_t page_pa : 36;
        uint64_t reserved_2 : 4;
        uint64_t ignored_2 : 11;
        uint64_t execute_disable : 1;
    };
};

union pde_2mb_t {
    uint64_t value;

    struct {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t supervisor : 1;
        uint64_t page_level_write_through : 1;
        uint64_t page_level_cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t large_page : 1;
        uint64_t global : 1;
        uint64_t ignored_1 : 2;
        uint64_t restart : 1;
        uint64_t pat : 1;
        uint64_t reserved_1 : 8;
        uint64_t page_pa : 27;
        uint64_t reserved_2 : 4;
        uint64_t ignored_2 : 7;
        uint64_t protection_key : 4;
        uint64_t execute_disable : 1;
    };
};

union pde_t {
    uint64_t value;

    struct {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t supervisor : 1;
        uint64_t page_level_write_through : 1;
        uint64_t page_level_cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t reserved_1 : 1;
        uint64_t large_page : 1;
        uint64_t ignored_1 : 3;
        uint64_t restart : 1;
        uint64_t page_pa : 36;
        uint64_t reserved_2 : 4;
        uint64_t ignored_2 : 11;
        uint64_t execute_disable : 1;
    };
};

union pte_t {
    uint64_t value;

    struct {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t supervisor : 1;
        uint64_t page_level_write_through : 1;
        uint64_t page_level_cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t pat : 1;
        uint64_t global : 1;
        uint64_t ignored_1 : 2;
        uint64_t restart : 1;
        uint64_t page_pa : 36;
        uint64_t reserved_1 : 4;
        uint64_t ignored_2 : 7;
        uint64_t protection_key : 4;
        uint64_t execute_disable : 1;
    };
};

union virtual_address_t {
    uint64_t address;
    struct {
        uint64_t offset : 12;
        uint64_t pt_index : 9;
        uint64_t pd_index : 9;
        uint64_t pdpt_index : 9;
        uint64_t pml4_index : 9;
        uint64_t reserved : 16;
    };
};

union self_ref_t {
    uint64_t address;

    struct {
        uint64_t offset : 12;
        uint64_t pml4_index : 9;
        uint64_t reserved : 43;
    } first;

    struct {
        uint64_t offset : 12;
        uint64_t pdpt_index : 9;
        uint64_t pml4_index : 9;
        uint64_t reserved : 34;
    } second;

    struct {
        uint64_t offset : 12;
        uint64_t pd_index : 9;
        uint64_t pdpt_index : 9;
        uint64_t pml4_index : 9;
        uint64_t reserved : 25;
    } third;
};