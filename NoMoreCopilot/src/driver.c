#include "NoMoreCopilot.h"

PFILTER_CONTEXT filterContext = NULL;

static __forceinline BOOLEAN IsConnectIoctl(_In_ ULONG code)
{
  return code == IOCTL_INTERNAL_KEYBOARD_CONNECT;
}
static __forceinline BOOLEAN IsDisconnectIoctl(_In_ ULONG code)
{
  return code == IOCTL_INTERNAL_KEYBOARD_DISCONNECT;
}

VOID EvtIoInternalDeviceControl(_In_ WDFQUEUE queue, _In_ WDFREQUEST request, _In_ size_t outputBufferLength, _In_ size_t inputBufferLength, _In_ ULONG ioControlCode)
{
  UNREFERENCED_PARAMETER(outputBufferLength);
  UNREFERENCED_PARAMETER(inputBufferLength);

  PFILTER_CONTEXT ctx = filterContext;
  NTSTATUS status = STATUS_SUCCESS;

  if (IsConnectIoctl(ioControlCode))
  {
    PCONNECT_DATA connect = NULL; size_t len = 0;
    status = WdfRequestRetrieveInputBuffer(request, sizeof(CONNECT_DATA), (PVOID*)&connect, &len);
    if (!NT_SUCCESS(status) || !connect)
    {
      WdfRequestComplete(request, status);
      return;
    }
    if (ctx->upperConnectData.ClassService != NULL)
    {
      WdfRequestComplete(request, STATUS_SHARING_VIOLATION);
      return;
    }

    ctx->upperConnectData = *connect;
    connect->ClassService = (PVOID)(ULONG_PTR)KbFilter_ServiceCallback;

    WDF_REQUEST_SEND_OPTIONS opts;
    WDF_REQUEST_SEND_OPTIONS_INIT(&opts, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
    if (!WdfRequestSend(request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(queue)), &opts))
    {
      status = WdfRequestGetStatus(request);
      WdfRequestComplete(request, status);
    }
    return;
  }

  if (IsDisconnectIoctl(ioControlCode))
  {
    WDF_REQUEST_SEND_OPTIONS opts;
    WDF_REQUEST_SEND_OPTIONS_INIT(&opts, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
    if (!WdfRequestSend(request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(queue)), &opts))
    {
      status = WdfRequestGetStatus(request);
      WdfRequestComplete(request, status);
    }
    ctx->upperConnectData.ClassService = NULL;
    ctx->upperConnectData.ClassDeviceObject = NULL;
    return;
  }

  WDF_REQUEST_SEND_OPTIONS opts;
  WDF_REQUEST_SEND_OPTIONS_INIT(&opts, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
  if (!WdfRequestSend(request, WdfDeviceGetIoTarget(WdfIoQueueGetDevice(queue)), &opts))
  {
    status = WdfRequestGetStatus(request);
    WdfRequestComplete(request, status);
  }
}

NTSTATUS EvtDeviceAdd(_In_ WDFDRIVER driver, _Inout_ PWDFDEVICE_INIT deviceInit)
{
  UNREFERENCED_PARAMETER(driver);

  WdfFdoInitSetFilter(deviceInit);

  WDF_OBJECT_ATTRIBUTES devAttr;
  WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&devAttr, FILTER_CONTEXT);

  WDFDEVICE device;
  NTSTATUS status = WdfDeviceCreate(&deviceInit, &devAttr, &device);
  if (!NT_SUCCESS(status))
    return status;

  PFILTER_CONTEXT ctx = FilterGetContext(device);
  RtlZeroMemory(ctx, sizeof(*ctx));
  KeInitializeSpinLock(&ctx->spinLock);
  filterContext = ctx;

  WDF_IO_QUEUE_CONFIG qcfg;
  WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qcfg, WdfIoQueueDispatchParallel);
  qcfg.EvtIoInternalDeviceControl = EvtIoInternalDeviceControl;

  WDFQUEUE queue;
  status = WdfIoQueueCreate(device, &qcfg, WDF_NO_OBJECT_ATTRIBUTES, &queue);
  return status;
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT  driverObject, _In_ PUNICODE_STRING registryPath)
{
  UNREFERENCED_PARAMETER(registryPath);

  WDF_DRIVER_CONFIG config;
  WDF_DRIVER_CONFIG_INIT(&config, EvtDeviceAdd);

  return WdfDriverCreate(driverObject, registryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}