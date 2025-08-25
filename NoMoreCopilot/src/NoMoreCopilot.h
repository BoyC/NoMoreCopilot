#pragma once
#include <ntddk.h>
#include <wdf.h>
#include <kbdmou.h>
#include <ntddkbd.h>

typedef VOID( *PFN_CLASS_SERVICE )( _In_ PDEVICE_OBJECT DeviceObject, _In_ PKEYBOARD_INPUT_DATA InputDataStart, _In_ PKEYBOARD_INPUT_DATA InputDataEnd, _Out_ PULONG InputDataConsumed );

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

typedef struct _FILTER_CONTEXT
{
  CONNECT_DATA UpperConnectData;   // original class service + device object
  KSPIN_LOCK   SpinLock;

  // Session state
  BOOLEAN      CopilotActive;
  BOOLEAN      InReleasePhase;     // after F23 BREAK until pending* break counts drain

  // Pending chord BREAK counters (swallow exactly these later)
  USHORT       PendingLShiftBreaks;
  USHORT       PendingRShiftBreaks;
  USHORT       PendingLWinBreaks;
  USHORT       PendingRWinBreaks;

  USHORT       LastUnitId;
} FILTER_CONTEXT, * PFILTER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME( FILTER_CONTEXT, FilterGetContext );

extern PFILTER_CONTEXT gCtx;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD EvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL EvtIoInternalDeviceControl;

VOID
KbFilter_ServiceCallback( _In_ PDEVICE_OBJECT DeviceObject, _In_ PKEYBOARD_INPUT_DATA InputDataStart, _In_ PKEYBOARD_INPUT_DATA InputDataEnd, _Out_ PULONG InputDataConsumed );

// --- Scancodes we care about (found by the probe) ---
#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_LWIN   0x5B   // E0
#define SC_RWIN   0x5C   // E0
#define SC_LCTRL  0x1D
#define SC_RCTRL  0x1D   // E0

#define SC_F23    0x6E
#define F23_HAS_E0 0

#ifndef KEY_BREAK
#define KEY_BREAK   0x0001
#endif
#ifndef KEY_E0
#define KEY_E0      0x0002
#endif
#ifndef KEY_E1
#define KEY_E1      0x0004
#endif

#define IS_BREAK(f) ((f) & KEY_BREAK)
#define IS_E0(f)    ((f) & KEY_E0)

VOID InjectRightCtrlUnit( _In_ USHORT unit, _In_ BOOLEAN make );
VOID SendKeys( _In_ PFILTER_CONTEXT ctx, _In_reads_( count ) PKEYBOARD_INPUT_DATA keys, _In_ ULONG count );
