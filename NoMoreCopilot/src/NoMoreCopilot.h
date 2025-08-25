#pragma once
#include <ntddk.h>
#include <wdf.h>
#include <kbdmou.h>
#include <ntddkbd.h>

typedef struct _FILTER_CONTEXT
{
  CONNECT_DATA upperConnectData; // original class service + device object
  KSPIN_LOCK   spinLock;
} FILTER_CONTEXT, * PFILTER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILTER_CONTEXT, FilterGetContext);

// Provided elsewhere in your driver (same as your current codebase)
extern PFILTER_CONTEXT filterContext;

// --- Scancodes we care about ---
#define SC_LSHIFT 0x2A
#define SC_LWIN   0x5B   // E0
#define SC_RCTRL  0x1D   // E0
#define SC_F23    0x6E
#define F23_HAS_E0 0     // set to 1 if your F23 reports with E0

#ifndef KEY_BREAK
#define KEY_BREAK   0x0001
#endif
#ifndef KEY_E0
#define KEY_E0      0x0002
#endif
#ifndef KEY_E1
#define KEY_E1      0x0004
#endif

#define IS_BREAK(f) (((f) & KEY_BREAK) != 0)
#define HAS_E0(f)   (((f) & KEY_E0) != 0)

// Service callback exported to KbdClass via ConnectData
VOID KbFilter_ServiceCallback(
  _In_ PDEVICE_OBJECT DeviceObject,
  _In_ PKEYBOARD_INPUT_DATA InputDataStart,
  _In_ PKEYBOARD_INPUT_DATA InputDataEnd,
  _Out_ PULONG InputDataConsumed);
