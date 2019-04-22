/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * main program
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include <windows.h>

#include <windows.h>
#include <winsvc.h>
#include <winsock2.h>
//#include "wine/debug.h"

#include "xrdp.h"

static const WCHAR xrdpW[] = {'x','r','d','p','d',0};

static struct xrdp_listen *g_listen = 0;
static long g_threadid = 0; /* main threadid */

static SERVICE_STATUS_HANDLE g_ssh = 0;
static SERVICE_STATUS g_service_status;
static SERVICE_STATUS_HANDLE service_handle;

static long g_sync_mutex = 0;
static long g_sync1_mutex = 0;
static intptr_t g_term_event = 0;
static intptr_t g_sync_event = 0;
/* synchronize stuff */
static int g_sync_command = 0;
static long g_sync_result = 0;
static long g_sync_param1 = 0;
static long g_sync_param2 = 0;
static long (*g_sync_func)(long param1, long param2);

void pipe_sig(int sig_num);

/*****************************************************************************/
long g_xrdp_sync(long (*sync_func)(long param1, long param2), long sync_param1, long sync_param2);
void xrdp_shutdown(int sig);
int g_is_term(void);
void g_set_term(int in_val);
intptr_t g_get_term_event(void);
intptr_t g_get_sync_event(void);
void g_process_waiting_function(void);

/* helper function to update service status */
static void set_service_status( SERVICE_STATUS_HANDLE handle, DWORD state, DWORD accepted )
{
    SERVICE_STATUS status;
    status.dwServiceType             = SERVICE_WIN32;
    status.dwCurrentState            = state;
    status.dwControlsAccepted        = accepted;
    status.dwWin32ExitCode           = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint              = 0;
    status.dwWaitHint                = (state == SERVICE_START_PENDING) ? 10000 : 0;
    SetServiceStatus( handle, &status );
}

static VOID WINAPI service_handler( DWORD ctrl)
{
    switch (ctrl)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
    //    set_service_status( service_handle, SERVICE_STOP_PENDING, 0 );
        return;
    default:
        set_service_status( service_handle, SERVICE_RUNNING,
                            SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN );
        return;
    }
}

/*****************************************************************************/
static void
log_event(HANDLE han, char *msg)
{
    ReportEventA(han, EVENTLOG_INFORMATION_TYPE, 0, 0, 0, 1, 0, &msg, 0);
}


static void WINAPI MyServiceMain( DWORD argc, LPWSTR *argv )
/*****************************************************************************/
//VOID WINAPI
//MyServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
    WSADATA w;
    char text[256];
    int pid;
    //HANDLE event_han;
    //  int fd;
    //  char text[256];

    //  fd = g_file_open("c:\\temp\\xrdp\\log.txt");
    //  g_file_write(fd, "hi\r\n", 4);
    //event_han = RegisterEventSource(0, "xrdp");
    //log_event(event_han, "hi xrdp log");
    g_threadid = tc_get_threadid();
    g_set_current_dir("c:\\temp\\xrdp");
    g_listen = 0;
    WSAStartup(2, &w);
    g_sync_mutex = tc_mutex_create();
    g_sync1_mutex = tc_mutex_create();
    pid = g_getpid();
    g_snprintf(text, 255, "xrdp_%8.8x_main_term", pid);
    g_term_event = g_create_wait_obj(text);
    g_snprintf(text, 255, "xrdp_%8.8x_main_sync", pid);
    g_sync_event = g_create_wait_obj(text);
    g_memset(&g_service_status, 0, sizeof(SERVICE_STATUS));
    g_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_service_status.dwCurrentState = SERVICE_RUNNING;
    g_service_status.dwControlsAccepted = SERVICE_CONTROL_INTERROGATE |
                                          SERVICE_ACCEPT_STOP |
                                          SERVICE_ACCEPT_SHUTDOWN;
    g_service_status.dwWin32ExitCode = NO_ERROR;
    g_service_status.dwServiceSpecificExitCode = 0;
    g_service_status.dwCheckPoint = 0;
    g_service_status.dwWaitHint = 0;
    //  g_sprintf(text, "calling RegisterServiceCtrlHandler\r\n");
    //  g_file_write(fd, text, g_strlen(text));
    service_handle = RegisterServiceCtrlHandlerExW((void *)xrdpW, service_handler, NULL);

    if (!service_handle)
        return;

    //if (g_ssh != 0)
    //{
        //    g_sprintf(text, "ok\r\n");
        //    g_file_write(fd, text, g_strlen(text));
        SetServiceStatus(g_ssh, &g_service_status);
        g_listen = xrdp_listen_create();
        xrdp_listen_main_loop(g_listen);
        g_sleep(100);
        g_service_status.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_ssh, &g_service_status);
    //}
    //else
    //{
        //g_sprintf(text, "RegisterServiceCtrlHandler failed\r\n");
        //g_file_write(fd, text, g_strlen(text));
    //}

    xrdp_listen_delete(g_listen);
    tc_mutex_delete(g_sync_mutex);
    tc_mutex_delete(g_sync1_mutex);
    //g_destroy_wait_obj(g_term_event);
    //g_destroy_wait_obj(g_sync_event);
    WSACleanup();
    //CloseHandle(event_han);
}

/*****************************************************************************/
//int
//main(int argc, char **argv)
int wwmain(int argc, WCHAR *argv[])
{
    int test;
    int host_be;
    WSADATA w;
    SC_HANDLE sc_man;
    SC_HANDLE sc_ser;
    int run_as_service;
    SERVICE_TABLE_ENTRYW te[2];

    g_init("dummy");
    ssl_init();

    run_as_service = 1;

    if (argc == 2)
    {
        if (g_strncasecmp(argv[1], "-help", 255) == 0 ||
                g_strncasecmp(argv[1], "--help", 255) == 0 ||
                g_strncasecmp(argv[1], "-h", 255) == 0)
        {
            g_writeln("%s", "");
            g_writeln("xrdp: A Remote Desktop Protocol server.");
            g_writeln("Copyright (C) Jay Sorg 2004-2011");
            g_writeln("See http://xrdp.sourceforge.net for more information.");
            g_writeln("%s", "");
            g_writeln("Usage: xrdp [options]");
            g_writeln("   -h: show help");
            g_writeln("   -install: install service");
            g_writeln("   -remove: remove service");
            g_writeln("%s", "");
            g_exit(0);
        }
        else if (g_strncasecmp(argv[1], "-install", 255) == 0 ||
                 g_strncasecmp(argv[1], "--install", 255) == 0 ||
                 g_strncasecmp(argv[1], "-i", 255) == 0)
        {
            /* open service manager */
            sc_man = OpenSCManagerA(0, 0, GENERIC_WRITE);

            if (sc_man == 0)
            {
                g_writeln("error OpenSCManager, do you have rights?");
                g_exit(0);
            }

            /* check if service is already installed */
            sc_ser = OpenServiceA(sc_man, "xrdp", SERVICE_ALL_ACCESS);

            if (sc_ser == 0)
            {
                /* install service */
                CreateServiceA(sc_man, "xrdp", "xrdp", SERVICE_ALL_ACCESS,
                              SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
                              SERVICE_ERROR_IGNORE, "c:\\temp\\xrdp\\xrdp.exe",
                              0, 0, 0, 0, 0);

            }
            else
            {
                g_writeln("error service is already installed");
                CloseServiceHandle(sc_ser);
                CloseServiceHandle(sc_man);
                g_exit(0);
            }

            CloseServiceHandle(sc_man);
            g_exit(0);
        }
        else if (g_strncasecmp(argv[1], "-remove", 255) == 0 ||
                 g_strncasecmp(argv[1], "--remove", 255) == 0 ||
                 g_strncasecmp(argv[1], "-r", 255) == 0)
        {
            /* open service manager */
            sc_man = OpenSCManagerA(0, 0, GENERIC_WRITE);

            if (sc_man == 0)
            {
                g_writeln("error OpenSCManager, do you have rights?");
                g_exit(0);
            }

            /* check if service is already installed */
            sc_ser = OpenServiceA(sc_man, "xrdp", SERVICE_ALL_ACCESS);

            if (sc_ser == 0)
            {
                g_writeln("error service is not installed");
                CloseServiceHandle(sc_man);
                g_exit(0);
            }

            DeleteService(sc_ser);
            CloseServiceHandle(sc_man);
            g_exit(0);
        }
        else
        {
            g_writeln("Unknown Parameter");
            g_writeln("xrdp -h for help");
            g_writeln("%s", "");
            g_exit(0);
        }
    }
    else if (argc > 1)
    {
        g_writeln("Unknown Parameter");
        g_writeln("xrdp -h for help");
        g_writeln("%s", "");
        g_exit(0);
    }

    if (run_as_service)
    {
        g_memset(&te, 0, sizeof(te));
        te[0].lpServiceName = (void *)xrdpW;
        te[0].lpServiceProc = MyServiceMain;
        StartServiceCtrlDispatcherW(&te);
        g_exit(0);
    }

    WSAStartup(2, &w);

    g_threadid = tc_get_threadid();
    g_listen = xrdp_listen_create();
    g_signal_user_interrupt(xrdp_shutdown); /* SIGINT */
    g_signal_pipe(pipe_sig); /* SIGPIPE */
    g_signal_terminate(xrdp_shutdown); /* SIGTERM */
    g_sync_mutex = tc_mutex_create();
    g_sync1_mutex = tc_mutex_create();
    //pid = g_getpid();
    //g_snprintf(text, 255, "xrdp_%8.8x_main_term", pid);
    //g_term_event = g_create_wait_obj(text);
    //g_snprintf(text, 255, "xrdp_%8.8x_main_sync", pid);
    //g_sync_event = g_create_wait_obj(text);

    xrdp_listen_main_loop(g_listen);
    xrdp_listen_delete(g_listen);
    tc_mutex_delete(g_sync_mutex);
    tc_mutex_delete(g_sync1_mutex);
    g_delete_wait_obj(g_term_event);
    g_delete_wait_obj(g_sync_event);
    /* I don't think it ever gets here */
    /* when running in win32 app mode, control c exits right away */
    WSACleanup();
    return 0;
}

