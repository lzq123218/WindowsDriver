// Test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include <winsvc.h>
#include <conio.h>
#include <winioctl.h>

#define DEVICE_NAME		L"NtModeDrv"
#define DRIVER_PATH		L".\\ntmodeldrv.sys"

#define IOCTRL_BASE 0x800

#define MYIOCTRL_CODE(i) \
    CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTRL_BASE+i, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define PRINT_CODE		MYIOCTRL_CODE(0)
#define BYE_CODE		MYIOCTRL_CODE(1)
#define WRITE_HELLO		MYIOCTRL_CODE(2)

BOOL LoadDriver(TCHAR* lpszDriverName, TCHAR* lpszDriverPath)
{
	//char szDriverImagePath[256] = "D:\\DriverTest\\ntmodelDrv.sys";
	TCHAR szDriverImagePath[256] = { 0 };
	//得到完成的驱动路径
	GetFullPathName(lpszDriverPath, 256, szDriverImagePath, NULL);

	BOOL bRet = FALSE;

	SC_HANDLE hServiceMgr = NULL;    //SCM管理器的句柄
	SC_HANDLE hServiceDDK = NULL;    //NT驱动程序的服务句柄

									 //打开服务控制管理器
	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (hServiceMgr == NULL)
	{
		//OpenSCManager失败
		printf("OpenSCManager() Failed %d!\n", GetLastError());
		bRet = FALSE;
		goto BeforeLeave;
	}
	else
	{
		//OpenSCManager成功
		printf("OpenSCManager() Ok 1!\n");
	}

	//创建驱动所对应的服务
	hServiceDDK = CreateService(hServiceMgr,
		lpszDriverName,     //驱动在注册表中的名字
		lpszDriverName,     //注册表驱动程序的DisplayName值
		SERVICE_ALL_ACCESS, //加载驱动程序的访问权限
		SERVICE_KERNEL_DRIVER,  //表示加载的服务是驱动
		SERVICE_DEMAND_START,   //注册表驱动程序的 Start 值
		SERVICE_ERROR_IGNORE,   //注册表驱动程序的 ErrorControl 值
		szDriverImagePath,      //注册表驱动程序的 ImagePath 值
		NULL,   //GroupOrder  HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GroupOrderList
		NULL,
		NULL,
		NULL,
		NULL);
	DWORD dwRtn;
	//判断服务是否失败
	if (hServiceDDK == NULL)
	{
		dwRtn = GetLastError();
		if (dwRtn != ERROR_IO_PENDING && dwRtn != ERROR_SERVICE_EXISTS)
		{
			//由于其他原因创建服务失败
			printf("CreateService() Failed %d !\n", dwRtn);
			bRet = FALSE;
			goto BeforeLeave;
		}
		else
		{
			//服务创建失败，是由于服务已经创建过
			printf("CreateService() Failed Service is ERROR_IO_PENDING or ERROR_SERVICE_EXISTS!\n");
		}

		//驱动程序已经加载，只需要打开
		hServiceDDK = OpenService(hServiceMgr, lpszDriverName, SERVICE_ALL_ACCESS);
		if (hServiceDDK == NULL)
		{
			//如果打开服务也失败，则意味错误
			dwRtn = GetLastError();
			printf("OpenService() Failed %d!\n", dwRtn);
			bRet = FALSE;
			goto BeforeLeave;
		}
		else
		{
			printf("OpenService() ok !\n");
		}
	}
	else
	{
		printf("CreateService() ok !\n");
	}

	//开启此项服务
	bRet = StartService(hServiceDDK, NULL, NULL);
	if (!bRet)
	{
		DWORD dwRtn = GetLastError();
		if (dwRtn != ERROR_IO_PENDING && dwRtn != ERROR_SERVICE_ALREADY_RUNNING)
		{
			printf("StartService() Failed %d !\n", dwRtn);
			bRet = FALSE;
			goto BeforeLeave;
		}
		else
		{
			if (dwRtn == ERROR_IO_PENDING)
			{
				//设备被挂住
				printf("StartService() Failed ERROR_IO_PENDING!\n");
				bRet = FALSE;
				goto BeforeLeave;
			}
			else
			{
				//服务已经开启
				printf("StartService() Failed ERROR_SERVICE_ALREADY_RUNNING !\n");
				bRet = TRUE;
				goto BeforeLeave;
			}
		}
	}
	bRet = TRUE;
	//离开前关闭句柄
BeforeLeave:
	if (hServiceDDK)
	{
		CloseServiceHandle(hServiceDDK);
	}
	if (hServiceMgr)
	{
		CloseServiceHandle(hServiceMgr);
	}
	return bRet;
}

//卸载驱动程序
BOOL UnloadDriver(TCHAR *szSvrName)
{
	BOOL bRet = FALSE;
	SC_HANDLE hServiceMgr = NULL; //SCM管理器的句柄
	SC_HANDLE hServiceDDK = NULL; //NT驱动程序的服务句柄
	SERVICE_STATUS SvrSta;
	//打开SCM管理器
	hServiceMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceMgr == NULL)
	{
		//打开SCM管理器失败
		printf("OpenSCManager() Failed %d!\n", GetLastError());
		bRet = FALSE;
		goto BeforeLeave;
	}
	else
	{
		//打开SCM管理器成功
		printf("OpenSCManager() ok !\n");
	}

	// 打开驱动所对应的服务
	hServiceDDK = OpenService(hServiceMgr, szSvrName, SERVICE_ALL_ACCESS);
	if (hServiceDDK == NULL)
	{
		//打开驱动所对应的服务失败
		printf("OpenService() Failed %d!\n", GetLastError());
		bRet = FALSE;
		goto BeforeLeave;
	}
	else
	{
		printf("OpenService() ok !\n");
	}

	//停止驱动程序，如果停止失败，只有重新启动才能再动态加载
	if (!ControlService(hServiceDDK, SERVICE_CONTROL_STOP, &SvrSta))
	{
		printf("ControlService() Failed %d!\n", GetLastError());
	}
	else
	{
		printf("ControlService() ok !\n");
	}

	//动态卸载驱动程序
	if (!DeleteService(hServiceDDK))
	{
		//卸载失败
		printf("DeleteSrevice() Failed %d!\n", GetLastError());
	}
	else
	{
		//卸载成功
		printf("DelServer:deleteService() ok\n");
	}
	bRet = TRUE;
BeforeLeave:
	//离开前
	if (hServiceDDK)
	{
		CloseServiceHandle(hServiceDDK);
	}
	if (hServiceMgr)
	{
		CloseServiceHandle(hServiceMgr);
	}
	return bRet;
}

void TestDriver()
{
	//测试驱动程序
	HANDLE hDevice = CreateFile(_T("\\\\.\\NtModeDrv"),
		GENERIC_WRITE | GENERIC_READ,
		8,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (hDevice != INVALID_HANDLE_VALUE)
	{
		printf("Create Device ok!\n");
	}
	else
	{
		printf("Create Device Failed %d!\n", GetLastError());
		return;
	}
	CHAR bufRead[1024] = { 0 };
	WCHAR bufWrite[1024] = L"Hello,world";

	DWORD dwRead = 0;
	DWORD dwWrite = 0;

	ReadFile(hDevice, bufRead, 1024, &dwRead, NULL);
	printf("Read done!\n");
	WriteFile(hDevice, bufWrite, (wcslen(bufWrite) + 1) * sizeof(WCHAR), &dwWrite, NULL);

	printf("Write done!\n");

	TCHAR bufInput[1024] = L"Hello,world";
	TCHAR bufOutput[1024] = { 0 };
	DWORD dwRet = 0;

	WCHAR bufFileInput[1024] = L"C:\\docs\\hi.txt";

	DeviceIoControl(
		hDevice,
		PRINT_CODE,
		bufInput,
		sizeof(bufFileInput),
		bufOutput,
		sizeof(bufOutput),
		&dwRet,
		NULL);
	DeviceIoControl(
		hDevice,
		WRITE_HELLO,
		NULL,
		0,
		NULL,
		0,
		&dwRet,
		NULL);
	DeviceIoControl(
		hDevice,
		BYE_CODE,
		NULL,
		0,
		NULL,
		0,
		&dwRet,
		NULL);

	printf("DeviceIoControl done!\n");
	CloseHandle(hDevice);
}

int main()
{
	//加载驱动
	BOOL bRet = LoadDriver(DEVICE_NAME, DRIVER_PATH);
	if (!bRet)
	{
		printf("LoadNTDriver error\n");
		return 0;
	}
	//加载成功
	printf("press any key to create device!\n");
	getchar();
	TestDriver();

	//这时候可通过注册表，或其他查看符号连接的软件验证
	printf("press any key to stop service!\n");
	_getch();

	//卸载驱动
	bRet = UnloadDriver(DEVICE_NAME);
	if (!bRet)
	{
		printf("UnloadNTDriver error!\n");
		return 0;
	}
	return 0;
}

