#include "Driver.h"

#define DevName L"\\Device\\MyDDKDevice"
#define SymLinkName L"\\??\\HelloDDK"

extern "C" NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT pDriverObject,
	IN PUNICODE_STRING pRegistryPath)
{
	NTSTATUS status;
	KdPrint(("Enter DriverEntry\n"));

	//设置卸载函数
	pDriverObject->DriverUnload = HelloDDKUnload;

	//设置派遣函数
	for (int i = 0; i < arraysize(pDriverObject->MajorFunction); ++i)
		pDriverObject->MajorFunction[i] = HelloDDKDispatchRoutin;

	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HelloDDKDeviceIOControl;

	//创建驱动设备对象
	status = CreateDevice(pDriverObject);

	KdPrint(("Leave DriverEntry\n"));
	return status;
}


NTSTATUS CreateDevice(
	IN PDRIVER_OBJECT	pDriverObject)
{
	NTSTATUS status;
	PDEVICE_OBJECT pDevObj;
	PDEVICE_EXTENSION pDevExt;

	//创建设备名称
	UNICODE_STRING devName;
	RtlInitUnicodeString(&devName, DevName);

	//创建设备
	status = IoCreateDevice(pDriverObject,
		sizeof(DEVICE_EXTENSION),
		&(UNICODE_STRING)devName,
		FILE_DEVICE_UNKNOWN,
		0, TRUE,
		&pDevObj);
	if (!NT_SUCCESS(status))
		return status;

	pDevObj->Flags |= DO_DIRECT_IO;
	pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	pDevExt->pDevice = pDevObj;
	pDevExt->ustrDeviceName = devName;

	//申请模拟文件的缓冲区
	pDevExt->buffer = (PUCHAR)ExAllocatePool(PagedPool, MAX_FILE_LENGTH);
	//设置模拟文件大小
	pDevExt->file_length = 0;

	//创建符号链接
	UNICODE_STRING symLinkName;
	RtlInitUnicodeString(&symLinkName, SymLinkName);
	pDevExt->ustrSymLinkName = symLinkName;
	status = IoCreateSymbolicLink(&symLinkName, &devName);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(pDevObj);
		return status;
	}
	return STATUS_SUCCESS;
}


VOID HelloDDKUnload(IN PDRIVER_OBJECT pDriverObject)
{
	PDEVICE_OBJECT	pNextObj;
	KdPrint(("Enter DriverUnload\n"));
	pNextObj = pDriverObject->DeviceObject;
	while (pNextObj != NULL)
	{
		PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
			pNextObj->DeviceExtension;
		if (pDevExt->buffer)
		{
			ExFreePool(pDevExt->buffer);
			pDevExt->buffer = NULL;
		}

		//删除符号链接
		UNICODE_STRING pLinkName = pDevExt->ustrSymLinkName;
		IoDeleteSymbolicLink(&pLinkName);
		pNextObj = pNextObj->NextDevice;
		IoDeleteDevice(pDevExt->pDevice);
	}
}


NTSTATUS HelloDDKDispatchRoutin(IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp)
{
	KdPrint(("Enter HelloDDKDispatchRoutin\n"));

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
	//建立一个字符串数组与IRP类型对应起来
	static char* irpname[] =
	{
		"IRP_MJ_CREATE",
		"IRP_MJ_CREATE_NAMED_PIPE",
		"IRP_MJ_CLOSE",
		"IRP_MJ_READ",
		"IRP_MJ_WRITE",
		"IRP_MJ_QUERY_INFORMATION",
		"IRP_MJ_SET_INFORMATION",
		"IRP_MJ_QUERY_EA",
		"IRP_MJ_SET_EA",
		"IRP_MJ_FLUSH_BUFFERS",
		"IRP_MJ_QUERY_VOLUME_INFORMATION",
		"IRP_MJ_SET_VOLUME_INFORMATION",
		"IRP_MJ_DIRECTORY_CONTROL",
		"IRP_MJ_FILE_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CONTROL",
		"IRP_MJ_INTERNAL_DEVICE_CONTROL",
		"IRP_MJ_SHUTDOWN",
		"IRP_MJ_LOCK_CONTROL",
		"IRP_MJ_CLEANUP",
		"IRP_MJ_CREATE_MAILSLOT",
		"IRP_MJ_QUERY_SECURITY",
		"IRP_MJ_SET_SECURITY",
		"IRP_MJ_POWER",
		"IRP_MJ_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CHANGE",
		"IRP_MJ_QUERY_QUOTA",
		"IRP_MJ_SET_QUOTA",
		"IRP_MJ_PNP",
	};

	UCHAR type = stack->MajorFunction;
	if (type >= arraysize(irpname))
		KdPrint((" - Unknown IRP, major type %X\n", type));
	else
		KdPrint(("\t%s\n", irpname[type]));

	NTSTATUS status = STATUS_SUCCESS;
	// 完成IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;	// bytes xfered
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	KdPrint(("Leave HelloDDKDispatchRoutin\n"));

	return status;
}

NTSTATUS HelloDDKDeviceIOControl(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
	//DeviceIoControl调用来自用户线程，因此处于PASSIVE_LEVEL
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	KIRQL oldirql;
	KeAcquireSpinLock(&pdx->My_SpinLock, &oldirql);
	//==================================================锁定区
	NTSTATUS status = STATUS_SUCCESS;
	KdPrint(("Enter HelloDDKDeviceIoControl\n"));
	//使用自旋锁后IRQL提升至DISPATCH_LEVEL
	ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);
	//设置IRP完成状态
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave HelloDDKDeviceIoControl\n"));

	//========================================================
	KeReleaseSpinLock(&pdx->My_SpinLock, oldirql);

	return STATUS_SUCCESS;
}




//NTSTATUS HelloDDKDeviceIOControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
//{
//	NTSTATUS status = STATUS_SUCCESS;
//	KdPrint(("Enter HelloDDKDeviceIoControl\n"));
//
//	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
//	//得到输入缓冲区大小
//	ULONG cbin = stack->Parameters.DeviceIoControl.InputBufferLength;
//	//得到输出缓冲区大小
//	ULONG cbout = stack->Parameters.DeviceIoControl.OutputBufferLength;
//	//得到ioctl码
//	ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
//
//	ULONG info = 0;
//
//	switch (code)
//	{
//	case IOCTL_TEST1:
//	{
//		KdPrint(("IOCTL_TEST1\n"));
//		//缓冲区方式IOCTL
//		UCHAR* InputBuffer = (UCHAR*)pIrp->AssociatedIrp.SystemBuffer;
//		for (ULONG i = 0; i < cbin; ++i)
//		{
//			KdPrint(("%X\n", InputBuffer[i]));
//		}
//
//		//操作输出缓冲区
//		UCHAR* OutputBuffer = (UCHAR*)pIrp->AssociatedIrp.SystemBuffer;
//		memset(OutputBuffer, 0xAA, cbout);
//
//		//设置实际操作输出缓冲区长度
//		info = cbout;
//		break;
//	}
//	case IOCTL_TEST2:
//	{
//		KdPrint(("IOCTL_TEST2\n"));
//		//直接方式IOCTL
//		UCHAR* InputBuffer = (UCHAR*)pIrp->AssociatedIrp.SystemBuffer;
//		for (ULONG i = 0; i < cbin; ++i)
//		{
//			KdPrint(("%X\n", InputBuffer[i]));
//		}
//
//		KdPrint(("User Address:0X%08X\n", MmGetMdlVirtualAddress(pIrp->MdlAddress)));
//
//		UCHAR* OutputBuffer = (UCHAR*)MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);
//
//		memset(OutputBuffer, 0xAA, cbout);
//
//		info = cbout;
//		break;
//	}
//	case IOCTL_TEST3:
//	{
//		//其他方式
//		KdPrint(("IOCTL_TEST3\n"));
//
//		UCHAR* UserInputBuffer = (UCHAR*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
//		KdPrint(("UserInputBuffer:0X%0X\n", UserInputBuffer));
//
//		//得到用户模式地址
//		PVOID UserOutputBuffer = pIrp->UserBuffer;
//
//		KdPrint(("UserOutputBuffer: 0X%0X\n", UserOutputBuffer));
//
//		__try
//		{
//			KdPrint(("Enter __try block\n"));
//
//			//判断指针是否可读
//			ProbeForRead(UserInputBuffer, cbin, 4);
//			//显示输入缓冲区内容
//			for (ULONG i = 0; i < cbin; ++i)
//			{
//				KdPrint(("%X\n", UserInputBuffer[i]));
//			}
//
//			//判断指针是否可写
//			ProbeForWrite(UserOutputBuffer, cbout, 4);
//
//			//操作输出缓冲区
//			memset(UserOutputBuffer, 0xAA, cbout);
//
//			info = cbout;
//			KdPrint(("Leave __try block\n"));
//		}
//		__except (EXCEPTION_EXECUTE_HANDLER)
//		{
//			KdPrint(("Catch the exception\n"));
//			KdPrint(("The program will keep goint\n"));
//			status = STATUS_UNSUCCESSFUL;
//		}
//		info = cbout;
//		break;
//	}
//	default:
//		status = STATUS_INVALID_VARIANT;
//	}
//
//	//完成IRP
//	pIrp->IoStatus.Status = status;
//	pIrp->IoStatus.Information = info;
//	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
//
//	KdPrint(("Leave HelloDDKDeviceIOControl\n"));
//
//	return status;
//}
