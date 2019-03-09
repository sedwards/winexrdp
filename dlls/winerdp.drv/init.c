/*
 * Android driver initialisation functions
 *
 * Copyright 1996, 2013, 2017 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define NONAMELESSSTRUCT
#define NONAMELESSUNION
#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <string.h>
#include <link.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winreg.h"
#include "wine/gdi_driver.h"
#include "wine/server.h"
#include "wine/library.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(rdp);

extern MONITORINFOEXW default_monitor DECLSPEC_HIDDEN;

unsigned int screen_width = 0;
unsigned int screen_height = 0;
RECT virtual_screen_rect = { 0, 0, 0, 0 };

MONITORINFOEXW default_monitor =
{
    sizeof(default_monitor),    /* cbSize */
    { 0, 0, 0, 0 },             /* rcMonitor */
    { 0, 0, 0, 0 },             /* rcWork */
    MONITORINFOF_PRIMARY,       /* dwFlags */
    { '\\','\\','.','\\','D','I','S','P','L','A','Y','1',0 }   /* szDevice */
};

/* FIXME: Detect this from the RDP Channel */
static const unsigned int screen_bpp = 32;  /* we don't support other modes */

int device_init_done;

typedef struct
{
    struct gdi_physdev dev;
} RDP_PDEVICE;

static const struct gdi_dc_funcs android_drv_funcs;
int pipe_thread();

int delay_load_rdpdrv(void)
{
    if(!device_init_done)
    {
       if(!pipe_thread())
       {
           FIXME("winerdp driver - termsrv has not Opened file for communication yet\n");
           return 0;
       } else {
           device_init();
           FIXME("RDP_create_desktop no device_init_done starting now\n");
	   return 1;
       }
    }
}

/******************************************************************************
 *           init_monitors
 */
void init_monitors( int width, int height )
{
    static const WCHAR trayW[] = {'S','h','e','l','l','_','T','r','a','y','W','n','d',0};
    RECT rect;
    HWND hwnd = FindWindowW( trayW, NULL );

    virtual_screen_rect.right = width;
    virtual_screen_rect.bottom = height;
    default_monitor.rcMonitor = default_monitor.rcWork = virtual_screen_rect;

    if (!hwnd || !IsWindowVisible( hwnd )) return;
    if (!GetWindowRect( hwnd, &rect )) return;
    if (rect.top) default_monitor.rcWork.bottom = rect.top;
    else default_monitor.rcWork.top = rect.bottom;
    TRACE( "found tray %p %s work area %s\n", hwnd,
           wine_dbgstr_rect( &rect ), wine_dbgstr_rect( &default_monitor.rcWork ));
}

/******************************************************************************
 *           set_screen_dpi
 */
void set_screen_dpi( DWORD dpi )
{
    static const WCHAR dpi_key_name[] = {'S','o','f','t','w','a','r','e','\\','F','o','n','t','s',0};
    static const WCHAR dpi_value_name[] = {'L','o','g','P','i','x','e','l','s',0};
    HKEY hkey;

    if (!RegCreateKeyW( HKEY_CURRENT_CONFIG, dpi_key_name, &hkey ))
    {
        RegSetValueExW( hkey, dpi_value_name, 0, REG_DWORD, (void *)&dpi, sizeof(DWORD) );
        RegCloseKey( hkey );
    }
}

/**********************************************************************
 *	     fetch_display_metrics
 */
static void fetch_display_metrics(void)
{
    FIXME("fetch_display_metrics\n");
    SERVER_START_REQ( get_window_rectangles )
    {
        req->handle = wine_server_user_handle( GetDesktopWindow() );
        req->relative = COORDS_CLIENT;
        if (!wine_server_call( req ))
        {
            screen_width  = reply->window.right;
            screen_height = reply->window.bottom;
        }
    }
    SERVER_END_REQ;

    init_monitors( screen_width, screen_height );
    FIXME( "fetch_display_metrics - screen %ux%u\n", screen_width, screen_height );
}


/**********************************************************************
 *           device_init
 *
 * Perform initializations needed upon creation of the first device.
 */
void device_init(void)
{
    device_init_done = TRUE;
    fetch_display_metrics();
    FIXME("device_init\n");
}


/******************************************************************************
 *           create_android_physdev
 */
static RDP_PDEVICE *create_android_physdev(void)
{
    FIXME("create_android_physdev\n");
    RDP_PDEVICE *physdev;

    if (!device_init_done) 
    {
	FIXME("create_android_physdev no device_init_done\n");
	    device_init();
    }

    if (!(physdev = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*physdev) ))) return NULL;
    return physdev;
}

/**********************************************************************
 *           RDP_CreateDC
 */
static BOOL RDP_CreateDC( PHYSDEV *pdev, LPCWSTR driver, LPCWSTR device,
                              LPCWSTR output, const DEVMODEW* initData )
{
    FIXME("RDP_CreateDC\n");

    if(!delay_load_rdpdrv())
    {
       FIXME("termsrv not loaded, delaying dll initialization");
       return FALSE;
    }

#if 0
    RDP_PDEVICE *physdev = create_android_physdev();

    if (!physdev){
	FIXME("RDP_CreateDC - failed\n");
	    return FALSE;
    }

    push_dc_driver( pdev, &physdev->dev, &android_drv_funcs );
#endif
    return TRUE;
}


/**********************************************************************
 *           RDP_CreateCompatibleDC
 */
static BOOL RDP_CreateCompatibleDC( PHYSDEV orig, PHYSDEV *pdev )
{
    FIXME("RDP_CreateCompatibleDC\n");

    if(!delay_load_rdpdrv())
    {
       FIXME("termsrv not loaded, delaying dll initialization");
       return FALSE;
    }

#if 0
    RDP_PDEVICE *physdev = create_android_physdev();

    if (!physdev){ 
	FIXME("RDP_CreateCompatibleDC - failed\n");
	    return FALSE;
    }

    push_dc_driver( pdev, &physdev->dev, &android_drv_funcs );
#endif
    return TRUE;
}


/**********************************************************************
 *           RDP_DeleteDC
 */
static BOOL RDP_DeleteDC( PHYSDEV dev )
{
    //FIXME("RDP_DeleteDC\n");
    HeapFree( GetProcessHeap(), 0, dev );
    return TRUE;
}


/***********************************************************************
 *           RDP_ChangeDisplaySettingsEx
 */
LONG CDECL RDP_ChangeDisplaySettingsEx( LPCWSTR devname, LPDEVMODEW devmode,
                                            HWND hwnd, DWORD flags, LPVOID lpvoid )
{
    FIXME( "(%s,%p,%p,0x%08x,%p)\n", debugstr_w( devname ), devmode, hwnd, flags, lpvoid );
    return DISP_CHANGE_SUCCESSFUL;
}


/***********************************************************************
 *           RDP_GetMonitorInfo
 */
BOOL CDECL RDP_GetMonitorInfo( HMONITOR handle, LPMONITORINFO info )
{
	FIXME("RDP_GetMonitorInfo\n");
    if (handle != (HMONITOR)1)
    {
	FIXME("RDP_GetMonitorInfo - Error\n");
        SetLastError( ERROR_INVALID_HANDLE );
        return FALSE;
    }
    info->rcMonitor = default_monitor.rcMonitor;
    info->rcWork = default_monitor.rcWork;
    info->dwFlags = default_monitor.dwFlags;
    if (info->cbSize >= sizeof(MONITORINFOEXW))
        lstrcpyW( ((MONITORINFOEXW *)info)->szDevice, default_monitor.szDevice );
    return TRUE;
}


/***********************************************************************
 *           RDP_EnumDisplayMonitors
 */
BOOL CDECL RDP_EnumDisplayMonitors( HDC hdc, LPRECT rect, MONITORENUMPROC proc, LPARAM lp )
{
	FIXME("RDP_EnumDisplayMonitors\n");
    return proc( (HMONITOR)1, 0, &default_monitor.rcMonitor, lp );
}


/***********************************************************************
 *           RDP_EnumDisplaySettingsEx
 */
BOOL CDECL RDP_EnumDisplaySettingsEx( LPCWSTR name, DWORD n, LPDEVMODEW devmode, DWORD flags)
{
	FIXME("RDP_EnumDisplaySettingsEx\n");
    static const WCHAR dev_name[CCHDEVICENAME] =
        { 'W','i','n','e',' ','A','n','d','r','o','i','d',' ','d','r','i','v','e','r',0 };

    devmode->dmSize = offsetof( DEVMODEW, dmICMMethod );
    devmode->dmSpecVersion = DM_SPECVERSION;
    devmode->dmDriverVersion = DM_SPECVERSION;
    memcpy( devmode->dmDeviceName, dev_name, sizeof(dev_name) );
    devmode->dmDriverExtra = 0;
    devmode->u2.dmDisplayFlags = 0;
    devmode->dmDisplayFrequency = 0;
    devmode->u1.s2.dmPosition.x = 0;
    devmode->u1.s2.dmPosition.y = 0;
    devmode->u1.s2.dmDisplayOrientation = 0;
    devmode->u1.s2.dmDisplayFixedOutput = 0;

    if (n == ENUM_CURRENT_SETTINGS || n == ENUM_REGISTRY_SETTINGS) n = 0;
    if (n == 0)
    {
        devmode->dmPelsWidth = screen_width;
        devmode->dmPelsHeight = screen_height;
        devmode->dmBitsPerPel = screen_bpp;
        devmode->dmDisplayFrequency = 60;
        devmode->dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY;
        FIXME( "mode %d -- %dx%d %d bpp @%d Hz\n", n,
               devmode->dmPelsWidth, devmode->dmPelsHeight,
               devmode->dmBitsPerPel, devmode->dmDisplayFrequency );
        return TRUE;
    }
    FIXME( "mode %d -- not present\n", n );
    SetLastError( ERROR_NO_MORE_FILES );
    return FALSE;
}

static const struct gdi_dc_funcs android_drv_funcs =
{
    NULL,                               /* pAbortDoc */
    NULL,                               /* pAbortPath */
    NULL,                               /* pAlphaBlend */
    NULL,                               /* pAngleArc */
    NULL,                               /* pArc */
    NULL,                               /* pArcTo */
    NULL,                               /* pBeginPath */
    NULL,                               /* pBlendImage */
    NULL,                               /* pChord */
    NULL,                               /* pCloseFigure */
    RDP_CreateCompatibleDC,         /* pCreateCompatibleDC */
    RDP_CreateDC,                   /* pCreateDC */
    RDP_DeleteDC,                   /* pDeleteDC */
    NULL,                               /* pDeleteObject */
    NULL,                               /* pDeviceCapabilities */
    NULL,                               /* pEllipse */
    NULL,                               /* pEndDoc */
    NULL,                               /* pEndPage */
    NULL,                               /* pEndPath */
    NULL,                               /* pEnumFonts */
    NULL,                               /* pEnumICMProfiles */
    NULL,                               /* pExcludeClipRect */
    NULL,                               /* pExtDeviceMode */
    NULL,                               /* pExtEscape */
    NULL,                               /* pExtFloodFill */
    NULL,                               /* pExtSelectClipRgn */
    NULL,                               /* pExtTextOut */
    NULL,                               /* pFillPath */
    NULL,                               /* pFillRgn */
    NULL,                               /* pFlattenPath */
    NULL,                               /* pFontIsLinked */
    NULL,                               /* pFrameRgn */
    NULL,                               /* pGdiComment */
    NULL,                               /* pGetBoundsRect */
    NULL,                               /* pGetCharABCWidths */
    NULL,                               /* pGetCharABCWidthsI */
    NULL,                               /* pGetCharWidth */
    NULL,                               /* pGetDeviceCaps */
    NULL,                               /* pGetDeviceGammaRamp */
    NULL,                               /* pGetFontData */
    NULL,                               /* pGetFontRealizationInfo */
    NULL,                               /* pGetFontUnicodeRanges */
    NULL,                               /* pGetGlyphIndices */
    NULL,                               /* pGetGlyphOutline */
    NULL,                               /* pGetICMProfile */
    NULL,                               /* pGetImage */
    NULL,                               /* pGetKerningPairs */
    NULL,                               /* pGetNearestColor */
    NULL,                               /* pGetOutlineTextMetrics */
    NULL,                               /* pGetPixel */
    NULL,                               /* pGetSystemPaletteEntries */
    NULL,                               /* pGetTextCharsetInfo */
    NULL,                               /* pGetTextExtentExPoint */
    NULL,                               /* pGetTextExtentExPointI */
    NULL,                               /* pGetTextFace */
    NULL,                               /* pGetTextMetrics */
    NULL,                               /* pGradientFill */
    NULL,                               /* pIntersectClipRect */
    NULL,                               /* pInvertRgn */
    NULL,                               /* pLineTo */
    NULL,                               /* pModifyWorldTransform */
    NULL,                               /* pMoveTo */
    NULL,                               /* pOffsetClipRgn */
    NULL,                               /* pOffsetViewportOrg */
    NULL,                               /* pOffsetWindowOrg */
    NULL,                               /* pPaintRgn */
    NULL,                               /* pPatBlt */
    NULL,                               /* pPie */
    NULL,                               /* pPolyBezier */
    NULL,                               /* pPolyBezierTo */
    NULL,                               /* pPolyDraw */
    NULL,                               /* pPolyPolygon */
    NULL,                               /* pPolyPolyline */
    NULL,                               /* pPolygon */
    NULL,                               /* pPolyline */
    NULL,                               /* pPolylineTo */
    NULL,                               /* pPutImage */
    NULL,                               /* pRealizeDefaultPalette */
    NULL,                               /* pRealizePalette */
    NULL,                               /* pRectangle */
    NULL,                               /* pResetDC */
    NULL,                               /* pRestoreDC */
    NULL,                               /* pRoundRect */
    NULL,                               /* pSaveDC */
    NULL,                               /* pScaleViewportExt */
    NULL,                               /* pScaleWindowExt */
    NULL,                               /* pSelectBitmap */
    NULL,                               /* pSelectBrush */
    NULL,                               /* pSelectClipPath */
    NULL,                               /* pSelectFont */
    NULL,                               /* pSelectPalette */
    NULL,                               /* pSelectPen */
    NULL,                               /* pSetArcDirection */
    NULL,                               /* pSetBkColor */
    NULL,                               /* pSetBkMode */
    NULL,                               /* pSetBoundsRect */
    NULL,                               /* pSetDCBrushColor */
    NULL,                               /* pSetDCPenColor */
    NULL,                               /* pSetDIBitsToDevice */
    NULL,                               /* pSetDeviceClipping */
    NULL,                               /* pSetDeviceGammaRamp */
    NULL,                               /* pSetLayout */
    NULL,                               /* pSetMapMode */
    NULL,                               /* pSetMapperFlags */
    NULL,                               /* pSetPixel */
    NULL,                               /* pSetPolyFillMode */
    NULL,                               /* pSetROP2 */
    NULL,                               /* pSetRelAbs */
    NULL,                               /* pSetStretchBltMode */
    NULL,                               /* pSetTextAlign */
    NULL,                               /* pSetTextCharacterExtra */
    NULL,                               /* pSetTextColor */
    NULL,                               /* pSetTextJustification */
    NULL,                               /* pSetViewportExt */
    NULL,                               /* pSetViewportOrg */
    NULL,                               /* pSetWindowExt */
    NULL,                               /* pSetWindowOrg */
    NULL,                               /* pSetWorldTransform */
    NULL,                               /* pStartDoc */
    NULL,                               /* pStartPage */
    NULL,                               /* pStretchBlt */
    NULL,                               /* pStretchDIBits */
    NULL,                               /* pStrokeAndFillPath */
    NULL,                               /* pStrokePath */
    NULL,                               /* pUnrealizePalette */
    NULL,                               /* pWidenPath */
    NULL,                               /* wine_get_wgl_driver */
    NULL,                               /* wine_get_vulkan_driver */
    GDI_PRIORITY_GRAPHICS_DRV           /* priority */
};


/******************************************************************************
 *           RDP_get_gdi_driver
 */
const struct gdi_dc_funcs * CDECL RDP_get_gdi_driver( unsigned int version )
{
    if (version != WINE_GDI_DRIVER_VERSION)
    {
        ERR( "version mismatch, gdi32 wants %u but wineandroid has %u\n", version, WINE_GDI_DRIVER_VERSION );
        return NULL;
    }
    return &android_drv_funcs;
}

static BOOL process_attach(void)
{
    return TRUE;
}

/***********************************************************************
 *       dll initialisation routine
 */
BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, LPVOID reserved )
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        if(!delay_load_rdpdrv())
	{
             FIXME("termsrv not loaded, delaying dll initialization");
	     return FALSE;
	}
        DisableThreadLibraryCalls( inst );
        return process_attach();
    }
    return TRUE;
}

