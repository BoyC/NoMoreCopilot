#include "NoMoreCopilot.h"

PFILTER_CONTEXT gCtx = NULL;

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT  DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
  UNREFERENCED_PARAMETER(RegistryPath);

  WDF_DRIVER_CONFIG config;
  WDF_DRIVER_CONFIG_INIT(&config, EvtDeviceAdd);

  return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}

NTSTATUS EvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
  UNREFERENCED_PARAMETER(Driver);

  WdfFdoInitSetFilter(DeviceInit);

  WDF_OBJECT_ATTRIBUTES devAttr;
  WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&devAttr, FILTER_CONTEXT);

  WDFDEVICE device;
  NTSTATUS status = WdfDeviceCreate(&DeviceInit, &devAttr, &device);
  if (!NT_SUCCESS(status)) 
    return status;

  PFILTER_CONTEXT ctx = FilterGetContext(device);
  RtlZeroMemory(ctx, sizeof(*ctx));
  KeInitializeSpinLock(&ctx->SpinLock);
  gCtx = ctx;

  WDF_IO_QUEUE_CONFIG qcfg;
  WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qcfg, WdfIoQueueDispatchParallel);
  qcfg.EvtIoInternalDeviceControl = EvtIoInternalDeviceControl;

  WDFQUEUE queue;
  status = WdfIoQueueCreate(device, &qcfg, WDF_NO_OBJECT_ATTRIBUTES, &queue);
  return status;
}

static __forceinline BOOLEAN IsConnectIoctl(_In_ ULONG code) 
{ 
  return code == IOCTL_INTERNAL_KEYBOARD_CONNECT; 
}
static __forceinline BOOLEAN IsDisconnectIoctl(_In_ ULONG code) 
{ 
  return code == IOCTL_INTERNAL_KEYBOARD_DISCONNECT; 
}

VOID EvtIoInternalDeviceControl(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t OutputBufferLength, _In_ size_t InputBufferLength, _In_ ULONG IoControlCode)
{
  UNREFERENCED_PARAMETER(OutputBufferLength);
  UNREFERENCED_PARAMETER(InputBufferLength);

  PFILTER_CONTEXT ctx = gCtx;
  NTSTATUS status = STATUS_SUCCESS;

  if (IsConnectIoctl(IoControlCode)) 
  {
    PCONNECT_DATA connect = NULL; size_t len = 0;
    status = WdfRequestRetrieveInputBuffer(Request, sizeof(CONNECT_DATA), (PVOID*)&connect, &len);
    if (!NT_SUCCESS(status) || !connect)
    {
      WdfRequestComplete(Request, status);
      return;
    }
    if (ctx->UpperConnectData.ClassService != NULL) 
    { 
      WdfRequestComplete(Request, STATUS_SHARING_VIOLATION); 
      return; 
    }

    // Stash original, replace only ClassService
    ctx->UpperConnectData = *connect;
    connect->ClassService = (PVOID)(ULONG_PTR)KbFilter_ServiceCallback;

    WDF_REQUEST_SEND_OPTIONS opts;
    WDF_REQUEST_SEND_OPTIONS_INIT(&opts, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
    if (!WdfRequestSend(Request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)), &opts)) 
    {
      status = WdfRequestGetStatus(Request);
      WdfRequestComplete(Request, status);
    }
    return;
  }

  if (IsDisconnectIoctl(IoControlCode)) 
  {
    WDF_REQUEST_SEND_OPTIONS opts;
    WDF_REQUEST_SEND_OPTIONS_INIT(&opts, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
    if (!WdfRequestSend(Request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)), &opts)) 
    {
      status = WdfRequestGetStatus(Request);
      WdfRequestComplete(Request, status);
    }
    ctx->UpperConnectData.ClassService = NULL;
    ctx->UpperConnectData.ClassDeviceObject = NULL;
    return;
  }

  // Default: forward
  WDF_REQUEST_SEND_OPTIONS opts;
  WDF_REQUEST_SEND_OPTIONS_INIT(&opts, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
  if (!WdfRequestSend(Request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)), &opts)) 
  {
    status = WdfRequestGetStatus(Request);
    WdfRequestComplete(Request, status);
  }
}
