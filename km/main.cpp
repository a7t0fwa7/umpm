#include <ntifs.h>
#include "../shared.h"

#include <intrin.h>

static inline void print(const char* fmt, ...) {
	va_list args;
	__va_start(&args, fmt);
	vDbgPrintEx(0, 0, fmt, args);
}

static NTSTATUS dispatch(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;

	if (!stack) {
		status = STATUS_INVALID_PARAMETER;
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	if (stack->MajorFunction != IRP_MJ_DEVICE_CONTROL) {
		status = STATUS_INVALID_DEVICE_REQUEST;
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	auto code = stack->Parameters.DeviceIoControl.IoControlCode;
	switch (static_cast<control_codes>(code)) {
	case control_codes::create_recursive_pte: {
		virtual_address_t self_copy{ .address = reinterpret_cast<std::uint64_t>(MmGetVirtualForPhysical({.QuadPart = static_cast<std::int64_t>(__readcr3())})) };
		self_copy.pdpt_index = self_copy.pml4_index;
		self_copy.pd_index = self_copy.pml4_index;
		self_copy.pt_index = self_copy.pml4_index;

		// va is a 4kb page allocated by the usermode application
		virtual_address_t va{ .address = *static_cast<std::uint64_t*>(Irp->AssociatedIrp.SystemBuffer) };

		// attempt to translate the given virtual address using the self referencing pml4e that windows places in the pml4 table
		// since the index is unknown, we initially use MmGetVirtualForPhysical, the self referencing pml4e index changes everytime u boot into windows but it stays the same for every process until u reboot again
		self_ref_t self{ .address = self_copy.address };
		auto& pml4e = reinterpret_cast<pml4e_t*>(self.address)[va.pml4_index];
		if (!pml4e.present) {
			status = STATUS_NO_MEMORY;
			break;
		}

		self.first.pml4_index = va.pml4_index;
		auto& pdpte = reinterpret_cast<pdpte_1gb_t*>(self.address)[va.pdpt_index];
		if (!pdpte.present || pdpte.large_page) {
			status = STATUS_NO_MEMORY;
			break;
		}

		self.second.pml4_index = va.pml4_index;
		self.second.pdpt_index = va.pdpt_index;
		auto& pde = reinterpret_cast<pde_t*>(self.address)[va.pd_index];
		if (!pde.present || pde.large_page) {
			status = STATUS_NO_MEMORY;
			break;
		}

		self.third.pml4_index = va.pml4_index;
		self.third.pdpt_index = va.pdpt_index;
		self.third.pd_index = va.pd_index;
		auto& pte = reinterpret_cast<pte_t*>(self.address)[va.pt_index];
		if (!pte.present) {
			status = STATUS_NO_MEMORY;
			break;
		}

		// copy the old pfn so the usermode process can restore it before exiting, this way the windows vmm doesnt cause a bugcheck
		std::uint64_t old_pfn = pte.page_pa;
		pte.page_pa = pde.page_pa; // map the given virtual address to the array of ptes it resides in, creating a self referencing pte

		*static_cast<std::uint64_t*>(Irp->AssociatedIrp.SystemBuffer) = old_pfn;
		Irp->IoStatus.Information = sizeof(std::uint64_t);

		// force a tlb flush just incase, alternatively use invlpg on the single entry above
		__writecr3(__readcr3());
		break;
	}
	default: {
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	}

	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

void DriverUnload(PDRIVER_OBJECT);
NTSTATUS entry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	DriverObject->DriverUnload = DriverUnload;

	UNICODE_STRING device_name = RTL_CONSTANT_STRING(L"\\Device\\UMPM");
	UNICODE_STRING sym_link = RTL_CONSTANT_STRING(L"\\DosDevices\\UMPM");

	PDEVICE_OBJECT device = nullptr;
	auto status = IoCreateDevice(DriverObject, 0, &device_name, FILE_DEVICE_UNKNOWN, 0, FALSE, &device);
	if (!NT_SUCCESS(status)) {
		print("Failed to create device\n");
		return status;
	}

	status = IoCreateSymbolicLink(&sym_link, &device_name);
	if (!NT_SUCCESS(status)) {
		print("Failed to create symbolic link\n");
		IoDeleteDevice(device);
		return status;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = [](PDEVICE_OBJECT, PIRP Irp) {
		Irp->IoStatus.Status = STATUS_SUCCESS;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
		};

	DriverObject->MajorFunction[IRP_MJ_CLOSE] = [](PDEVICE_OBJECT, PIRP Irp) {
		Irp->IoStatus.Status = STATUS_SUCCESS;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
		};

	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatch;

	SetFlag(device->Flags, DO_BUFFERED_IO);
	ClearFlag(device->Flags, DO_DEVICE_INITIALIZING);

	return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING sym_link = RTL_CONSTANT_STRING(L"\\DosDevices\\UMPM");
	IoDeleteSymbolicLink(&sym_link);
	IoDeleteDevice(DriverObject->DeviceObject);
}

extern "C" {
	NTKERNELAPI NTSTATUS IoCreateDriver(PUNICODE_STRING DriverName,
		PDRIVER_INITIALIZE InitializationFunction);
};

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING) {
	UNICODE_STRING driver_name = {};
	RtlInitUnicodeString(&driver_name, L"\\Driver\\UMPM");

	return IoCreateDriver(&driver_name, entry);
}