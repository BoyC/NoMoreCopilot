#include "NoMoreCopilot.h"

typedef VOID(*PFN_CLASS_SERVICE)(_In_ PDEVICE_OBJECT DeviceObject, _In_ PKEYBOARD_INPUT_DATA InputDataStart, _In_ PKEYBOARD_INPUT_DATA InputDataEnd, _Out_ PULONG InputDataConsumed);

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

#define CHORD_WINDOW_100MS   (1000000ULL) // 100 ms in KeQueryInterruptTime units

static __forceinline BOOLEAN IsLWin(USHORT sc, USHORT fl) { return HAS_E0(fl) && sc == SC_LWIN; }
static __forceinline BOOLEAN IsLShift(USHORT sc, USHORT fl) { return !HAS_E0(fl) && sc == SC_LSHIFT; }
static __forceinline BOOLEAN IsF23(USHORT sc, USHORT fl) { return sc == SC_F23 && (HAS_E0(fl) == (!!F23_HAS_E0)); }

static __forceinline VOID EmitKey(_Out_ PKEYBOARD_INPUT_DATA out, USHORT unit, USHORT sc, BOOLEAN e0, BOOLEAN make)
{
  RtlZeroMemory(out, sizeof(*out));
  out->UnitId = unit;
  out->MakeCode = sc;
  out->Flags = (e0 ? KEY_E0 : 0) | (make ? 0 : KEY_BREAK);
}
static __forceinline VOID EmitLWin(PKEYBOARD_INPUT_DATA o, USHORT u, BOOLEAN m) { EmitKey(o, u, SC_LWIN, TRUE, m); }
static __forceinline VOID EmitLShift(PKEYBOARD_INPUT_DATA o, USHORT u, BOOLEAN m) { EmitKey(o, u, SC_LSHIFT, FALSE, m); }
static __forceinline VOID EmitRCtrl(PKEYBOARD_INPUT_DATA o, USHORT u, BOOLEAN m) { EmitKey(o, u, SC_RCTRL, TRUE, m); }

static __forceinline VOID SendKeys(_In_ PFILTER_CONTEXT ctx, _In_reads_(n) PKEYBOARD_INPUT_DATA keys, _In_ ULONG n)
{
  if (!ctx || !ctx->upperConnectData.ClassService || n == 0) 
    return;
  ULONG consumed = 0;
  ((PFN_CLASS_SERVICE)(ULONG_PTR)ctx->upperConnectData.ClassService)(ctx->upperConnectData.ClassDeviceObject, keys, keys + n, &consumed);
}

typedef enum { ST_IDLE = 0, ST_EXPECT_SHIFT, ST_EXPECT_F23, ST_CHORD } STAGE;

typedef struct _CHORD_STATE
{
  STAGE   stage;
  USHORT  unit;

  BOOLEAN swallowNextLWinUp;
  BOOLEAN swallowNextLShiftUp;

  ULONGLONG tWinDown;
  ULONGLONG tShiftDown;
} CHORD_STATE;

static CHORD_STATE g = { 0 };

static __forceinline VOID ResetState(VOID)
{
  g.stage = ST_IDLE;
  g.unit = 0;
  g.swallowNextLWinUp = g.swallowNextLShiftUp = FALSE;
  g.tWinDown = g.tShiftDown = 0;
}

VOID KbFilter_ServiceCallback(_In_ PDEVICE_OBJECT DevObj, _In_ PKEYBOARD_INPUT_DATA inStart, _In_ PKEYBOARD_INPUT_DATA inEnd, _Out_ PULONG inConsumed)
{
  UNREFERENCED_PARAMETER(DevObj);
  PFILTER_CONTEXT ctx = filterContext;

  KEYBOARD_INPUT_DATA inBuf[128];
  ULONG inCnt = (ULONG)(inEnd - inStart);
  if (inCnt > RTL_NUMBER_OF(inBuf)) 
    inCnt = RTL_NUMBER_OF(inBuf);
  for (ULONG i = 0; i < inCnt; ++i) 
    inBuf[i] = inStart[i];

  KEYBOARD_INPUT_DATA outBuf[256];
  ULONG outCount = 0;

  KIRQL irql;
  KeAcquireSpinLock(&ctx->spinLock, &irql);

  for (ULONG i = 0; i < inCnt; ++i)
  {
    const KEYBOARD_INPUT_DATA* ev = &inBuf[i];
    const USHORT sc = ev->MakeCode;
    const USHORT fl = ev->Flags;
    const BOOLEAN make = !IS_BREAK(fl);
    const USHORT unit = ev->UnitId;
    const ULONGLONG now = KeQueryInterruptTime();

    switch (g.stage)
    {
    case ST_IDLE:
      if (make && IsLWin(sc, fl))
      {
        g.stage = ST_EXPECT_SHIFT;
        g.unit = unit;
        g.tWinDown = now;
        g.tShiftDown = 0;
        continue;
      }
      break;

    case ST_EXPECT_SHIFT:
      if (make && unit == g.unit && IsLShift(sc, fl) && (now - g.tWinDown) <= CHORD_WINDOW_100MS)
      {
        g.tShiftDown = now;
        g.stage = ST_EXPECT_F23;
        continue;
      }
      
      EmitLWin(&outBuf[outCount++], g.unit, TRUE);
      ResetState();
      break;

    case ST_EXPECT_F23:
      if (make && unit == g.unit && IsF23(sc, fl) && (now - g.tWinDown) <= CHORD_WINDOW_100MS && (now - g.tShiftDown) <= CHORD_WINDOW_100MS)
      {
        EmitRCtrl(&outBuf[outCount++], g.unit, TRUE);
        g.stage = ST_CHORD;
        continue;
      }

      EmitLWin(&outBuf[outCount++], g.unit, TRUE);
      EmitLShift(&outBuf[outCount++], g.unit, TRUE);
      ResetState();
      break;

    case ST_CHORD:
      if (unit == g.unit && IsF23(sc, fl))
      {
        if (!make)
        {
          EmitRCtrl(&outBuf[outCount++], g.unit, FALSE);
          g.swallowNextLShiftUp = TRUE;
          g.swallowNextLWinUp = TRUE;
        }
        continue;
      }

      if (!make && unit == g.unit)
      {
        if (IsLShift(sc, fl) && g.swallowNextLShiftUp)
        {
          g.swallowNextLShiftUp = FALSE;
          if (!g.swallowNextLWinUp) 
            ResetState();
          continue;
        }
        if (IsLWin(sc, fl) && g.swallowNextLWinUp)
        {
          g.swallowNextLWinUp = FALSE;
          if (!g.swallowNextLShiftUp) 
            ResetState();
          continue;
        }
      }

      break;
    }

    outBuf[outCount++] = *ev;
  }

  if (outCount) 
    SendKeys(ctx, outBuf, outCount);
  *inConsumed += inCnt;
  KeReleaseSpinLock(&ctx->spinLock, irql);
}
