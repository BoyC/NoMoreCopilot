#include "NoMoreCopilot.h"

static __forceinline BOOLEAN IsWinKey(USHORT sc, USHORT fl)
{
  return IS_E0(fl) && (sc == SC_LWIN || sc == SC_RWIN);
}
static __forceinline BOOLEAN IsShiftKey(USHORT sc, USHORT fl)
{
  return !IS_E0(fl) && (sc == SC_LSHIFT || sc == SC_RSHIFT);
}
static __forceinline BOOLEAN IsF23(USHORT sc, USHORT fl)
{
  return sc == SC_F23 && ((!!IS_E0(fl)) == (!!F23_HAS_E0));
}

VOID SendKeys(PFILTER_CONTEXT ctx, PKEYBOARD_INPUT_DATA keys, ULONG count)
{
  if (!ctx || !ctx->UpperConnectData.ClassService)
    return;
  ULONG consumed = 0;
  PFN_CLASS_SERVICE fn = (PFN_CLASS_SERVICE)(ULONG_PTR)ctx->UpperConnectData.ClassService;
  fn(ctx->UpperConnectData.ClassDeviceObject, keys, keys + count, &consumed);
}

static __forceinline VOID EmitKey(PKEYBOARD_INPUT_DATA outBuf, PULONG pCount, USHORT unit, USHORT sc, BOOLEAN e0, BOOLEAN make)
{
  KEYBOARD_INPUT_DATA k; RtlZeroMemory(&k, sizeof(k));
  k.UnitId = unit;
  k.MakeCode = sc;
  k.Flags = (e0 ? KEY_E0 : 0) | (make ? 0 : KEY_BREAK);
  outBuf[(*pCount)++] = k;
}

static __forceinline VOID EmitRCtrl(PKEYBOARD_INPUT_DATA outBuf, PULONG pCount, USHORT unit, BOOLEAN make)
{
  EmitKey(outBuf, pCount, unit, SC_RCTRL, TRUE, make);
}

static BOOLEAN g_UserShiftDown = FALSE;

static __forceinline VOID TrackWhenForwarded(const KEYBOARD_INPUT_DATA* ev)
{
  if (IsShiftKey(ev->MakeCode, ev->Flags))
    g_UserShiftDown = !IS_BREAK(ev->Flags);
}

static __forceinline VOID TrackWhenSynthesized(USHORT sc, BOOLEAN make)
{
  if (sc == SC_LSHIFT || sc == SC_RSHIFT)
    g_UserShiftDown = make;
}

VOID KbFilter_ServiceCallback(PDEVICE_OBJECT DevObj, PKEYBOARD_INPUT_DATA inStart, PKEYBOARD_INPUT_DATA inEnd, PULONG inConsumed)
{
  UNREFERENCED_PARAMETER(DevObj);
  PFILTER_CONTEXT ctx = gCtx;

  KEYBOARD_INPUT_DATA inBuf[128];
  ULONG inCnt = (ULONG)(inEnd - inStart);
  if (inCnt > RTL_NUMBER_OF(inBuf))
    inCnt = RTL_NUMBER_OF(inBuf);
  for (ULONG i = 0; i < inCnt; ++i)
    inBuf[i] = inStart[i];

  KEYBOARD_INPUT_DATA outBuf[128];
  ULONG outCount = 0;

  static BOOLEAN Armed = FALSE;
  static USHORT  ArmedUnit = 0;
  static BOOLEAN SnapshotUserShift = FALSE;
  static BOOLEAN SawChordShiftAfterArm = FALSE;

  BOOLEAN HasF23Make = FALSE;
  for (ULONG i = 0; i < inCnt; ++i)
  {
    if (IsF23(inBuf[i].MakeCode, inBuf[i].Flags) && !IS_BREAK(inBuf[i].Flags))
    {
      HasF23Make = TRUE;
      break;
    }
  }

  KIRQL irql;
  KeAcquireSpinLock(&ctx->SpinLock, &irql);

  if (ctx->InReleasePhase)
  {
    for (ULONG i = 0; i < inCnt; ++i)
    {
      const KEYBOARD_INPUT_DATA* ev = &inBuf[i];
      const USHORT sc = ev->MakeCode;
      const USHORT fl = ev->Flags;
      const BOOLEAN make = !IS_BREAK(fl);

      if (!make)
      {
        if (IsShiftKey(sc, fl))
        {
          if (sc == SC_LSHIFT && ctx->PendingLShiftBreaks)
          {
            ctx->PendingLShiftBreaks--;
            continue;
          }
          if (sc == SC_RSHIFT && ctx->PendingRShiftBreaks)
          {
            ctx->PendingRShiftBreaks--;
            continue;
          }
        }
        else if (IsWinKey(sc, fl))
        {
          if (sc == SC_LWIN && ctx->PendingLWinBreaks)
          {
            ctx->PendingLWinBreaks--;
            continue;
          }
          if (sc == SC_RWIN && ctx->PendingRWinBreaks)
          {
            ctx->PendingRWinBreaks--;
            continue;
          }
        }
      }

      outBuf[outCount++] = *ev;
      TrackWhenForwarded(ev);
    }

    if (ctx->PendingLShiftBreaks == 0 && ctx->PendingRShiftBreaks == 0 && ctx->PendingLWinBreaks == 0 && ctx->PendingRWinBreaks == 0)
      ctx->InReleasePhase = FALSE;

    if (outCount) SendKeys(ctx, outBuf, outCount);
    *inConsumed += inCnt;
    KeReleaseSpinLock(&ctx->SpinLock, irql);
    return;
  }

  for (ULONG i = 0; i < inCnt; ++i)
  {
    KEYBOARD_INPUT_DATA* ev = &inBuf[i];
    const USHORT sc = ev->MakeCode;
    const USHORT fl = ev->Flags;
    const BOOLEAN make = !IS_BREAK(fl);
    const USHORT unit = ev->UnitId;
    ctx->LastUnitId = unit;

    if (ctx->CopilotActive)
    {
      if (IsWinKey(sc, fl))
        continue;

      if (IsF23(sc, fl) && !make)
      {
        EmitRCtrl(outBuf, &outCount, unit, FALSE);
        ctx->CopilotActive = FALSE;
        ctx->InReleasePhase = TRUE;
        continue;
      }

      outBuf[outCount++] = *ev;
      TrackWhenForwarded(ev);
      continue;
    }

    if (!Armed)
    {
      if (make && IsWinKey(sc, fl) && sc == SC_LWIN)
      {
        Armed = TRUE;
        ArmedUnit = unit;
        SnapshotUserShift = g_UserShiftDown;
        SawChordShiftAfterArm = FALSE;
        continue;
      }

      if (make && IsF23(sc, fl))
      {
        EmitKey(outBuf, &outCount, unit, SC_LWIN, TRUE, FALSE);
        ctx->PendingLWinBreaks++;
        EmitKey(outBuf, &outCount, unit, SC_LSHIFT, FALSE, FALSE);
        TrackWhenSynthesized(SC_LSHIFT, FALSE);
        ctx->PendingLShiftBreaks++;
        EmitRCtrl(outBuf, &outCount, unit, TRUE);
        ctx->CopilotActive = TRUE;
        ctx->InReleasePhase = FALSE;
        continue;
      }

      outBuf[outCount++] = *ev;
      TrackWhenForwarded(ev);
      continue;
    }
    else
    {
      if (make && IsShiftKey(sc, fl) && sc == SC_LSHIFT)
      {
        SawChordShiftAfterArm = TRUE;
        continue;
      }

      if (make && IsF23(sc, fl))
      {
        EmitKey(outBuf, &outCount, ArmedUnit, SC_LWIN, TRUE, FALSE);
        ctx->PendingLWinBreaks++;

        if (!SnapshotUserShift)
        {
          EmitKey(outBuf, &outCount, ArmedUnit, SC_LSHIFT, FALSE, FALSE);
          TrackWhenSynthesized(SC_LSHIFT, FALSE);
        }
        ctx->PendingLShiftBreaks++;

        EmitRCtrl(outBuf, &outCount, ArmedUnit, TRUE);
        ctx->CopilotActive = TRUE;
        ctx->InReleasePhase = FALSE;

        Armed = FALSE;
        continue;
      }

      if (!make && IsWinKey(sc, fl) && sc == SC_LWIN)
      {
        EmitKey(outBuf, &outCount, ArmedUnit, SC_LWIN, TRUE, TRUE);
        outBuf[outCount++] = *ev;
        Armed = FALSE;
        continue;
      }

      outBuf[outCount++] = *ev;
      TrackWhenForwarded(ev);
      continue;
    }
  }

  if (outCount) SendKeys(ctx, outBuf, outCount);
  *inConsumed += inCnt;
  KeReleaseSpinLock(&ctx->SpinLock, irql);
}
