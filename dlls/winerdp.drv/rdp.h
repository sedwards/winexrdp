/*
 * Android driver definitions
 *
 * Copyright 2013 Alexandre Julliard
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

#ifndef __WINE_RDP_H
#define __WINE_RDP_H

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/gdi_driver.h"
#include "rdp_native.h"

/**************************************************************************
 * OpenGL driver
 */

//extern void update_gl_drawable( HWND hwnd ) DECLSPEC_HIDDEN;
//extern void destroy_gl_drawable( HWND hwnd ) DECLSPEC_HIDDEN;
//extern struct opengl_funcs *get_wgl_driver( UINT version ) DECLSPEC_HIDDEN;


/**************************************************************************
 * Android pseudo-device
 */

extern void start_rdp_device(void) DECLSPEC_HIDDEN;
//extern void register_native_window( HWND hwnd, struct HANDLE *win, BOOL client ) DECLSPEC_HIDDEN;
extern struct HANDLE *create_ioctl_window( HWND hwnd, BOOL opengl, float scale ) DECLSPEC_HIDDEN;
extern struct HANDLE *grab_ioctl_window( struct HANDLE *window ) DECLSPEC_HIDDEN;
extern void release_ioctl_window( struct HANDLE *window ) DECLSPEC_HIDDEN;
extern void destroy_ioctl_window( HWND hwnd, BOOL opengl ) DECLSPEC_HIDDEN;
extern int ioctl_window_pos_changed( HWND hwnd, const RECT *window_rect, const RECT *client_rect,
                                     const RECT *visible_rect, UINT style, UINT flags,
                                     HWND after, HWND owner ) DECLSPEC_HIDDEN;
extern int ioctl_set_window_parent( HWND hwnd, HWND parent, float scale ) DECLSPEC_HIDDEN;
extern int ioctl_set_capture( HWND hwnd ) DECLSPEC_HIDDEN;
extern int ioctl_set_cursor( int id, int width, int height,
                             int hotspotx, int hotspoty, const unsigned int *bits ) DECLSPEC_HIDDEN;


/**************************************************************************
 * USER driver
 */

extern unsigned int screen_width DECLSPEC_HIDDEN;
extern unsigned int screen_height DECLSPEC_HIDDEN;
extern RECT virtual_screen_rect DECLSPEC_HIDDEN;
extern MONITORINFOEXW default_monitor DECLSPEC_HIDDEN;

enum rdp_window_messages
{
    WM_RDP_REFRESH = 0x80001000,
};

extern void init_gralloc( const struct hw_module_t *module ) DECLSPEC_HIDDEN;
extern HWND get_capture_window(void) DECLSPEC_HIDDEN;
extern void init_monitors( int width, int height ) DECLSPEC_HIDDEN;
extern void set_screen_dpi( DWORD dpi ) DECLSPEC_HIDDEN;
extern void update_keyboard_lock_state( WORD vkey, UINT state ) DECLSPEC_HIDDEN;

/* JNI entry points */
extern void desktop_changed( PVOID *env, PVOID obj, int width, int height ) DECLSPEC_HIDDEN;
extern void config_changed( PVOID *env, PVOID obj, int dpi ) DECLSPEC_HIDDEN;
extern void surface_changed( PVOID *env, PVOID obj, int win, PVOID surface,
                             BOOL client ) DECLSPEC_HIDDEN;
extern BOOL motion_event( PVOID *env, PVOID obj, int win, int action,
                              int x, int y, int state, int vscroll ) DECLSPEC_HIDDEN;
extern BOOL keyboard_event( PVOID *env, PVOID obj, int win, int action,
                                int keycode, int state ) DECLSPEC_HIDDEN;

enum event_type
{
    DESKTOP_CHANGED,
    CONFIG_CHANGED,
    SURFACE_CHANGED,
    MOTION_EVENT,
    KEYBOARD_EVENT,
};

union event_data
{
    enum event_type type;
    struct
    {
        enum event_type type;
        unsigned int    width;
        unsigned int    height;
    } desktop;
    struct
    {
        enum event_type type;
        unsigned int    dpi;
    } cfg;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        HANDLE          *window;
        BOOL            client;
        unsigned int    width;
        unsigned int    height;
    } surface;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        INPUT           input;
    } motion;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        UINT            lock_state;
        INPUT           input;
    } kbd;
};

//int send_event( const union event_data *data ) DECLSPEC_HIDDEN;

#endif  /* __WINE_RDP_H */
