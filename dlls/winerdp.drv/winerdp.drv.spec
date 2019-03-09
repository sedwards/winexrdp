# GDI driver

@ cdecl wine_get_gdi_driver(long) RDP_get_gdi_driver

# USER driver

;@ cdecl GetKeyNameText(long ptr long) RDP_GetKeyNameText
;@ cdecl GetKeyboardLayout(long) RDP_GetKeyboardLayout
;@ cdecl MapVirtualKeyEx(long long long) RDP_MapVirtualKeyEx
;@ cdecl ToUnicodeEx(long long ptr ptr long long long) RDP_ToUnicodeEx
;@ cdecl VkKeyScanEx(long long) RDP_VkKeyScanEx
@ cdecl SetCursor(long) RDP_SetCursor
;@ cdecl ChangeDisplaySettingsEx(ptr ptr long long long) RDP_ChangeDisplaySettingsEx
;@ cdecl EnumDisplayMonitors(long ptr ptr long) RDP_EnumDisplayMonitors
;@ cdecl EnumDisplaySettingsEx(ptr long ptr long) RDP_EnumDisplaySettingsEx
;@ cdecl GetMonitorInfo(long ptr) RDP_GetMonitorInfo
@ cdecl CreateDesktopWindow(long) RDP_CreateDesktopWindow
@ cdecl CreateWindow(long) RDP_CreateWindow
@ cdecl DestroyWindow(long) RDP_DestroyWindow
;@ cdecl MsgWaitForMultipleObjectsEx(long ptr long long long) RDP_MsgWaitForMultipleObjectsEx
@ cdecl SetCapture(long long) RDP_SetCapture
@ cdecl SetLayeredWindowAttributes(long long long long) RDP_SetLayeredWindowAttributes
@ cdecl SetParent(long long long) RDP_SetParent
@ cdecl SetWindowRgn(long long long) RDP_SetWindowRgn
@ cdecl SetWindowStyle(ptr long ptr) RDP_SetWindowStyle
@ cdecl ShowWindow(long long ptr long) RDP_ShowWindow
@ cdecl UpdateLayeredWindow(long ptr ptr) RDP_UpdateLayeredWindow
@ cdecl WindowMessage(long long long long) RDP_WindowMessage
@ cdecl WindowPosChanging(long long long ptr ptr ptr ptr) RDP_WindowPosChanging
@ cdecl WindowPosChanged(long long long ptr ptr ptr ptr ptr) RDP_WindowPosChanged

# Desktop
@ cdecl wine_create_desktop(long long) RDP_create_desktop

# MMDevAPI driver functions
;@ stdcall -private GetPriority() AUDDRV_GetPriority
;@ stdcall -private GetEndpointIDs(long ptr ptr ptr ptr) AUDDRV_GetEndpointIDs
;@ stdcall -private GetAudioEndpoint(ptr ptr ptr) AUDDRV_GetAudioEndpoint
;@ stdcall -private GetAudioSessionManager(ptr ptr) AUDDRV_GetAudioSessionManager
