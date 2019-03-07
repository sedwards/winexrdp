
#include "config.h"
#include "wine/port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <windows.h>

#include "wine/debug.h"
#include "wine/heap.h"


#define BUF_SIZE 1024 
#define BUF_SIZE2 4096 
const WCHAR szName[]={'G','l','o','b','a','l','\\','M','y','F','i','l','e','M','a','p','p','i','n','g','O','b','j','e','c','t',0};
//const WCHAR szMsg[BUF_SIZE2]={'M','e','s','s','a','g','e','f','r','o','m','f','i','r','s','t','p','r','o','c','e','s','s',0};
WCHAR buf[BUF_SIZE2]={'d','s','t',0};
const WCHAR pbData[BUF_SIZE2]={'M','e','s','s','a','g','e','f','r','o','m','f','i','r','s','t','p','r','o','c','e','s','s',0};
//const WCHAR pbData[BUF_SIZE2]={'M','e','s','s','a','g','e','f','r','o','m','f','i','r','s','t','p','r','o','c','e','s','s',0};
//char szName[]={"Global\\MyFileMappingObject"};
const char szMsg[]={"Message from first process."};

#define BUFFER_SIZE 1024
#define COPY_SIZE 1024 

/*
   MyCopyMemory - A wrapper for CopyMemory

   buf     - destination buffer
   pbData  - source buffer
   cbData  - size of block to copy, in bytes
   bufsize - size of the destination buffer
*/

void MyCopyMemory(WCHAR *buf, WCHAR *pbData, SIZE_T cbData, SIZE_T bufsize) 
{
    CopyMemory(buf, pbData, min(cbData,bufsize));
}

static int mywprintf(const WCHAR *format, ...)
{
    static char output_bufA[65536];
    static WCHAR output_bufW[sizeof(output_bufA)];
    va_list             parms;
    DWORD               nOut;
    BOOL                res = FALSE;
    HANDLE              hout = GetStdHandle(STD_OUTPUT_HANDLE);

    va_start(parms, format);
    vsnprintfW(output_bufW, ARRAY_SIZE(output_bufW), format, parms);
    va_end(parms);

//    wprintf("output %S", *format);
//    /* Try to write as unicode whenever we think it's a console */
//    if (((DWORD_PTR)hout & 3) == 3)
//    {
//        res = WriteConsoleW(hout, output_bufW, strlenW(output_bufW), &nOut, NULL);
//    }
//    else
//    {
        DWORD   convertedChars;

        /* Convert to OEM, then output */
        convertedChars = WideCharToMultiByte(GetConsoleOutputCP(), 0, output_bufW, -1,
                                             output_bufA, sizeof(output_bufA),
                                             NULL, NULL);
	printf("%s\n",output_bufA);
//        res = WriteFile(hout, output_bufA, convertedChars, &nOut, FALSE);
//    }

    return res ? nOut : 0;
}

int MyReadConsoleInput(VOID);

void main()
{
   HANDLE hMapFile;
   LPCWSTR pBuf;

   hMapFile = CreateFileMappingW(
                 INVALID_HANDLE_VALUE,    // use paging file
                 NULL,                    // default security
                 PAGE_READWRITE,          // read/write access
                 0,                       // maximum object size (high-order DWORD)
                 BUF_SIZE,                // maximum object size (low-order DWORD)
                 szName);                 // name of mapping object

   if (hMapFile == NULL)
   {
	   printf("Error 1");
      fprintf("Could not create file mapping object (%d).\n",
             GetLastError());
      return 1;
   }
   pBuf = (LPWSTR) MapViewOfFile(hMapFile,   // handle to map object
                        FILE_MAP_ALL_ACCESS, // read/write permission
                        0,
                        0,
                        BUF_SIZE);

   if (pBuf == NULL)
   {
      printf("Error 2");
      fprintf("Could not map view of file (%d).\n", GetLastError());

      CloseHandle(hMapFile);

      return 1;
   }


//   CopyMemory((PVOID)pBuf, szMsg, min(COPY_SIZE*sizeof(WCHAR), BUFFER_SIZE*sizeof(WCHAR)));

//#if 0   
    /* This works */
    //mywprintf(buf);
    MyCopyMemory(buf, pbData, COPY_SIZE*sizeof(WCHAR), BUFFER_SIZE*sizeof(WCHAR));
    MyCopyMemory(pBuf, pbData, COPY_SIZE*sizeof(WCHAR), BUFFER_SIZE*sizeof(WCHAR));
    //mywprintf(buf);
    mywprintf(pBuf);
//#endif

    for(;;) {
      MyReadConsoleInput();
    }
   //getch();
   //MyReadConsoleInput();
   UnmapViewOfFile(pBuf);

   CloseHandle(hMapFile);
}


HANDLE hStdin;
DWORD fdwSaveOldMode;

VOID ErrorExit(LPSTR);
VOID KeyEventProc(KEY_EVENT_RECORD);
VOID MouseEventProc(MOUSE_EVENT_RECORD);
VOID ResizeEventProc(WINDOW_BUFFER_SIZE_RECORD);

int MyReadConsoleInput(VOID)
{
    DWORD cNumRead, fdwMode, i;
    INPUT_RECORD irInBuf[128];
    int counter=0;

    // Get the standard input handle.

    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE)
        ErrorExit("GetStdHandle");

    // Save the current input mode, to be restored on exit.

    if (! GetConsoleMode(hStdin, &fdwSaveOldMode) )
        ErrorExit("GetConsoleMode");

    // Enable the window and mouse input events.

    fdwMode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
    if (! SetConsoleMode(hStdin, fdwMode) )
        ErrorExit("SetConsoleMode");

    // Loop to read and handle the next 100 input events.

    while (counter++ <= 2)
    {
        // Wait for the events.

        if (! ReadConsoleInputW(
                hStdin,      // input buffer handle
                irInBuf,     // buffer to read into
                128,         // size of read buffer
                &cNumRead) ) // number of records read
            ErrorExit("ReadConsoleInput");

        // Dispatch the events to the appropriate handler.

        for (i = 0; i < cNumRead; i++)
        {
            switch(irInBuf[i].EventType)
            {
                case KEY_EVENT: // keyboard input
                    KeyEventProc(irInBuf[i].Event.KeyEvent);
                    break;

                case MOUSE_EVENT: // mouse input
                    MouseEventProc(irInBuf[i].Event.MouseEvent);
                    break;

                case WINDOW_BUFFER_SIZE_EVENT: // scrn buf. resizing
                    ResizeEventProc( irInBuf[i].Event.WindowBufferSizeEvent );
                    break;

                case FOCUS_EVENT:  // disregard focus events

                case MENU_EVENT:   // disregard menu events
                    break;

                default:
                    ErrorExit("Unknown event type");
                    break;
            }
        }
    }

    // Restore input mode on exit.

    SetConsoleMode(hStdin, fdwSaveOldMode);

    return 0;
}

VOID ErrorExit (LPSTR lpszMessage)
{
    fprintf(stderr, "%s\n", lpszMessage);

    // Restore input mode on exit.

    SetConsoleMode(hStdin, fdwSaveOldMode);

    ExitProcess(0);
}

VOID KeyEventProc(KEY_EVENT_RECORD ker)
{
    printf("Key event: ");

    if(ker.bKeyDown)
        printf("key pressed\n");
    else printf("key released\n");
}

VOID MouseEventProc(MOUSE_EVENT_RECORD mer)
{
#ifndef MOUSE_HWHEELED
#define MOUSE_HWHEELED 0x0008
#endif
    printf("Mouse event: ");

    switch(mer.dwEventFlags)
    {
        case 0:

            if(mer.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED)
            {
                printf("left button press \n");
            }
            else if(mer.dwButtonState == RIGHTMOST_BUTTON_PRESSED)
            {
                printf("right button press \n");
            }
            else
            {
                printf("button press\n");
            }
            break;
        case DOUBLE_CLICK:
            printf("double click\n");
            break;
        case MOUSE_HWHEELED:
            printf("horizontal mouse wheel\n");
            break;
        case MOUSE_MOVED:
            printf("mouse moved\n");
            break;
        case MOUSE_WHEELED:
            printf("vertical mouse wheel\n");
            break;
        default:
            printf("unknown\n");
            break;
    }
}

VOID ResizeEventProc(WINDOW_BUFFER_SIZE_RECORD wbsr)
{
    printf("Resize event\n");
    printf("Console screen buffer is %d columns by %d rows.\n", wbsr.dwSize.X, wbsr.dwSize.Y);
}

