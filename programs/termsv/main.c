/*
 * Copyright 2008 Hans Leidekker for CodeWeavers
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

#define WIN32_LEAN_AND_MEAN

#include <windef.h>
#include <winbase.h>
#include <winnt.h>
#include <winsvc.h>
#include <wine/debug.h>

WINE_DEFAULT_DEBUG_CHANNEL(termsv);
static WCHAR termserviceW[] = {'T','e','r','m','S','e','r','v','i','c','e',0};

static SERVICE_STATUS_HANDLE service_handle;
static HANDLE stop_event;
BOOL TERMSV_Initialize(void);

static DWORD WINAPI service_handler( DWORD ctrl, DWORD event_type, LPVOID event_data, LPVOID context )
{
    SERVICE_STATUS status;

    status.dwServiceType             = SERVICE_WIN32;
    status.dwControlsAccepted        = SERVICE_ACCEPT_STOP;
    status.dwWin32ExitCode           = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint              = 0;
    status.dwWaitHint                = 0;

    switch(ctrl)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        WINE_TRACE( "shutting down\n" );
        status.dwCurrentState     = SERVICE_STOP_PENDING;
        status.dwControlsAccepted = 0;
        SetServiceStatus( service_handle, &status );
        SetEvent( stop_event );
        return NO_ERROR;
    default:
        WINE_FIXME( "got service ctrl %x\n", ctrl );
        status.dwCurrentState = SERVICE_RUNNING;
        SetServiceStatus( service_handle, &status );
        return NO_ERROR;
    }
}

static void WINAPI ServiceMain( DWORD argc, LPWSTR *argv )
{
    SERVICE_STATUS status;

    WINE_TRACE( "starting service\n" );

    //if (!TERMSV_Initialize()) return;

    stop_event = CreateEventW( NULL, TRUE, FALSE, NULL );

    service_handle = RegisterServiceCtrlHandlerExW( termserviceW, service_handler, NULL );
    if (!service_handle)
        return;

    status.dwServiceType             = SERVICE_WIN32;
    status.dwCurrentState            = SERVICE_RUNNING;
    status.dwControlsAccepted        = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    status.dwWin32ExitCode           = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint              = 0;
    status.dwWaitHint                = 10000;
    SetServiceStatus( service_handle, &status );

    WaitForSingleObject( stop_event, INFINITE );

    status.dwCurrentState     = SERVICE_STOPPED;
    status.dwControlsAccepted = 0;
    SetServiceStatus( service_handle, &status );
    WINE_TRACE( "service stopped\n" );
}

int twmain( int argc, WCHAR *argv[] )
{
    static const SERVICE_TABLE_ENTRYW service_table[] =
    {
        { termserviceW, ServiceMain },
        { NULL, NULL }
    };

    StartServiceCtrlDispatcherW( service_table );
    return 0;
}

/*****************************************************************************/
long
g_load_library(char *in)
{
    return (long)LoadLibraryA(in);
}

/*****************************************************************************/
int
g_free_library(long lib)
{
    if (lib == 0)
    {
        return 0;
    }

    return FreeLibrary((HMODULE)lib);
}

/*****************************************************************************/
/* returns NULL if not found */
void *
g_get_proc_address(long lib, const char *name)
{
    if (lib == 0)
    {
        return 0;
    }

    return GetProcAddress((HMODULE)lib, name);
}

int
tc_thread_create(unsigned long (__stdcall *start_routine)(void *), void *arg)
{
    int rv = 0;
    DWORD thread_id = 0;
    HANDLE thread = (HANDLE)0;

    /* CreateThread returns handle or zero on error */
    thread = CreateThread(0, 0, start_routine, arg, 0, &thread_id);
    rv = !thread;
    CloseHandle(thread);
    return rv;
}

typedef signed __int64 intptr_t;
typedef intptr_t tbus;

/*****************************************************************************/
tbus
tc_sem_create(int init_count)
{
    HANDLE sem;

    sem = CreateSemaphoreW(0, init_count, init_count + 10, 0);
    return (tbus)sem;
}

/*****************************************************************************/
void
tc_sem_delete(tbus sem)
{
    CloseHandle((HANDLE)sem);
}

/*****************************************************************************/
int
tc_sem_dec(tbus sem)
{
    WaitForSingleObject((HANDLE)sem, INFINITE);
    return 0;
}

/*****************************************************************************/
int
tc_sem_inc(tbus sem)
{
    ReleaseSemaphore((HANDLE)sem, 1, 0);
    return 0;
}

