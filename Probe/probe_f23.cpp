#include <windows.h>
#include <stdio.h>

HHOOK g_hHook;

LRESULT CALLBACK Hook( int nCode, WPARAM wParam, LPARAM lParam )
{
  if ( nCode == HC_ACTION )
  {
    const KBDLLHOOKSTRUCT* k = reinterpret_cast<KBDLLHOOKSTRUCT*>( lParam );
    bool pressed = ( wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN );
    printf( "%s: vk=0x%02X sc=0x%02X flags=0x%X e0=%d e1=%d time=%u\n",
            pressed ? "DOWN" : "UP",
            (unsigned)k->vkCode,
            (unsigned)k->scanCode,
            (unsigned)k->flags,
            !!( k->flags & LLKHF_EXTENDED ),
            !!( k->flags & 0x02 ),
            (unsigned)GetTickCount() );
    fflush( stdout );
  }
  return CallNextHookEx( g_hHook, nCode, wParam, lParam );
}

int wmain()
{
  g_hHook = SetWindowsHookExW( WH_KEYBOARD_LL, Hook, nullptr, 0 );
  if ( !g_hHook )
  {
    printf( "SetWindowsHookEx failed: %lu\n", GetLastError() );
    return 1;
  }
  printf( "Press Ctrl+C to exit. Press the Copilot key to see vk/sc/e0.\n" );
  MSG msg;
  while ( GetMessageW( &msg, nullptr, 0, 0 ) ) {}
  return 0;
}
