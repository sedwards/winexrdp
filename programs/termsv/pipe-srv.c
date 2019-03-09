#if 0
#include <windows.h> 
#include <stdio.h> 
//#include <tchar.h>
#include <strsafe.h>

#define BUFSIZE 512
 
DWORD WINAPI InstanceThread(LPVOID); 
VOID GetAnswerToRequest(LPSTR, LPSTR, LPDWORD); 

#define THREAD_RV void*
#define THREAD_CC __stdcall

HANDLE hPipe; 

THREAD_RV THREAD_CC wine_named_pipe(VOID) 
{ 
   BOOL   fConnected = FALSE; 
   DWORD  dwThreadId = 0; 
   HANDLE hThread = NULL; 
   LPSTR lpszPipename = "\\\\.\\pipe\\rdp_server"; 
   hPipe = INVALID_HANDLE_VALUE;
 
// The main loop creates an instance of the named pipe and 
// then waits for a client to connect to it. When the client 
// connects, a thread is created to handle communications 
// with that client, and this loop is free to wait for the
// next client connect request. It is an infinite loop.
 
   for (;;) 
   { 
      printf( ("\nPipe Server: Main thread awaiting client connection on %s\n"), lpszPipename);
      hPipe = CreateNamedPipeA( 
          lpszPipename,             // pipe name 
          PIPE_ACCESS_DUPLEX,       // read/write access 
          PIPE_TYPE_MESSAGE |       // message type pipe 
          PIPE_READMODE_MESSAGE |   // message-read mode 
          PIPE_WAIT,                // blocking mode 
          PIPE_UNLIMITED_INSTANCES, // max. instances  
          BUFSIZE,                  // output buffer size 
          BUFSIZE,                  // input buffer size 
          0,                        // client time-out 
          NULL);                    // default security attribute 

      if (hPipe == INVALID_HANDLE_VALUE) 
      {
          printf(("CreateNamedPipe failed, GLE=%d.\n"), GetLastError()); 
      //    return -1;
      }
 
      // Wait for the client to connect; if it succeeds, 
      // the function returns a nonzero value. If the function
      // returns zero, GetLastError returns ERROR_PIPE_CONNECTED. 
 
      fConnected = ConnectNamedPipe(hPipe, NULL) ? 
         TRUE : (GetLastError() == ERROR_PIPE_CONNECTED); 
 
      if (fConnected) 
      { 
         printf("Client connected, creating a processing thread.\n"); 
      
         // Create a thread for this client. 
         hThread = CreateThread( 
            NULL,              // no security attribute 
            0,                 // default stack size 
            InstanceThread,    // thread proc
            (LPVOID) hPipe,    // thread parameter 
            0,                 // not suspended 
            &dwThreadId);      // returns thread ID 

         if (hThread == NULL) 
         {
            printf(("CreateThread failed, GLE=%d.\n"), GetLastError()); 
            return -1;
         }
         else CloseHandle(hThread); 
       } 
      else 
        // The client could not connect, so close the pipe. 
         CloseHandle(hPipe); 
   } 

   //return 0; 
} 
 
DWORD WINAPI InstanceThread(LPVOID lpvParam)
// This routine is a thread processing function to read from and reply to a client
// via the open pipe connection passed from the main loop. Note this allows
// the main loop to continue executing, potentially creating more threads of
// of this procedure to run concurrently, depending on the number of incoming
// client connections.
{ 
   HANDLE hHeap      = GetProcessHeap();
   CHAR* pchRequest = (CHAR*)HeapAlloc(hHeap, 0, BUFSIZE*sizeof(CHAR));
   CHAR* pchReply   = (CHAR*)HeapAlloc(hHeap, 0, BUFSIZE*sizeof(CHAR));

   DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0; 
   BOOL fSuccess = FALSE;
   HANDLE hPipe  = NULL;

   // Do some extra error checking since the app will keep running even if this
   // thread fails.

   if (lpvParam == NULL)
   {
       printf( "\nERROR - Pipe Server Failure:\n");
       printf( "   InstanceThread got an unexpected NULL value in lpvParam.\n");
       printf( "   InstanceThread exitting.\n");
       if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
       if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
       return (DWORD)-1;
   }

   if (pchRequest == NULL)
   {
       printf( "\nERROR - Pipe Server Failure:\n");
       printf( "   InstanceThread got an unexpected NULL heap allocation.\n");
       printf( "   InstanceThread exitting.\n");
       if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
       return (DWORD)-1;
   }

   if (pchReply == NULL)
   {
       printf( "\nERROR - Pipe Server Failure:\n");
       printf( "   InstanceThread got an unexpected NULL heap allocation.\n");
       printf( "   InstanceThread exitting.\n");
       if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
       return (DWORD)-1;
   }

   // Print verbose messages. In production code, this should be for debugging only.
   printf("InstanceThread created, receiving and processing messages.\n");

// The thread's parameter is a handle to a pipe object instance. 
 
   hPipe = (HANDLE) lpvParam; 

// Loop until done reading
   while (1) 
   { 
   // Read client requests from the pipe. This simplistic code only allows messages
   // up to BUFSIZE characters in length.
      fSuccess = ReadFile( 
         hPipe,        // handle to pipe 
         pchRequest,    // buffer to receive data 
         BUFSIZE*sizeof(CHAR), // size of buffer 
         &cbBytesRead, // number of bytes read 
         NULL);        // not overlapped I/O 

      if (!fSuccess || cbBytesRead == 0)
      {   
          if (GetLastError() == ERROR_BROKEN_PIPE)
          {
              printf(("InstanceThread: client disconnected.\n"), GetLastError()); 
          }
          else
          {
              printf(("InstanceThread ReadFile failed, GLE=%d.\n"), GetLastError()); 
          }
          break;
      }

   // Process the incoming message.
      GetAnswerToRequest(pchRequest, pchReply, &cbReplyBytes); 
 
   // Write the reply to the pipe. 
      fSuccess = WriteFile( 
         hPipe,        // handle to pipe 
         pchReply,     // buffer to write from 
         cbReplyBytes, // number of bytes to write 
         &cbWritten,   // number of bytes written 
         NULL);        // not overlapped I/O 

      if (!fSuccess || cbReplyBytes != cbWritten)
      {   
          printf(("InstanceThread WriteFile failed, GLE=%d.\n"), GetLastError()); 
          break;
      }
  }

// Flush the pipe to allow the client to read the pipe's contents 
// before disconnecting. Then disconnect the pipe, and close the 
// handle to this pipe instance. 
 
   FlushFileBuffers(hPipe); 
   DisconnectNamedPipe(hPipe); 
   CloseHandle(hPipe); 

   HeapFree(hHeap, 0, pchRequest);
   HeapFree(hHeap, 0, pchReply);

   printf("InstanceThread exitting.\n");
   return 1;
}

VOID GetAnswerToRequest( LPSTR pchRequest, 
                         LPSTR pchReply, 
                         LPDWORD pchBytes )
// This routine is a simple function to print the client request to the console
// and populate the reply buffer with a default data string. This is where you
// would put the actual client request processing code that runs in the context
// of an instance thread. Keep in mind the main thread will continue to wait for
// and receive other client connections while the instance thread is working.
{
    printf( ("Client Request String:\"%s\"\n"), pchRequest );

    // Check the outgoing message to make sure it's not too long for the buffer.
    //if (FAILED(StringCchCopy( pchReply, BUFSIZE, ("default answer from server") )))
    if (!lstrcpyA( pchReply, ("default answer from server") ))
    {
        *pchBytes = 0;
        pchReply[0] = 0;
        printf("StringCchCopy failed, no outgoing message.\n");
        return;
    }
    *pchBytes = (lstrlenA(pchReply)+1)*sizeof(CHAR);
}

void termsv_msg_pipe(char *msg)
{
   CHAR  chBuf[BUFSIZE];
   BOOL   fSuccess = FALSE;
   DWORD  cbRead, cbToWrite, cbWritten, dwMode;
   LPSTR lpvMessage="termsv_send_msg_pipe: Test Message\n";

   cbToWrite = (lstrlenA(lpvMessage)+1)*sizeof(CHAR);
   printf( ("termsv_msg_pipe: sending %d byte message from RDP Driver: \"%s\"\n"), cbToWrite, lpvMessage);

   fSuccess = WriteFile(
      hPipe,                  // pipe handle
      lpvMessage,             // message
      cbToWrite,              // message length
      &cbWritten,             // bytes written
      NULL);                  // not overlapped

   if ( ! fSuccess)
   {
      printf( ("writing message to pipe failed. GLE=%d\n"), GetLastError() );
   }
}

#if 0
void termsv_read_msg_pipe()
{
   CHAR  chBuf[BUFSIZE];
   BOOL   fSuccess = FALSE;
   DWORD  cbRead, cbToWrite, cbWritten, dwMode;
   do
   {
   // Read from the pipe.

      fSuccess = ReadFile(
         hPipe,    // pipe handle
         chBuf,    // buffer to receive reply
         BUFSIZE*sizeof(CHAR),  // size of buffer
         &cbRead,  // number of bytes read
         NULL);    // not overlapped

      if ( ! fSuccess && GetLastError() != ERROR_MORE_DATA )
         break;

      printf( ("termsv_read_msg_pipe: \"%s\"\n"), chBuf );
    } while ( ! fSuccess);  // repeat loop if ERROR_MORE_DATA

    if ( ! fSuccess)
    {
      printf( ("Message Confirmation from pipe failed. GLE=%d\n"), GetLastError() );
      return -1;
    }
}
#endif
#endif

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
const WCHAR szName[]={'G','l','o','b','a','l','\\','R','D','P','s','e','r','v','e','r','0',0};
//WCHAR buf[BUF_SIZE2]={'d','s','t',0};
//const WCHAR pbData[BUF_SIZE2]={'M','e','s','s','a','g','e','f','r','o','m','f','i','r','s','t','p','r','o','c','e','s','s',0};
//char szName[]={"Global\\MyFileMappingObject"};
const char szMsg[]={"Message from first process."};

#define BUFFER_SIZE 1024
#define COPY_SIZE 1024 

#define THREAD_RV void*
#define THREAD_CC __stdcall

/*
   MyCopyMemory - A wrapper for CopyMemory

   buf     - destination buffer
   pbData  - source buffer
   cbData  - size of block to copy, in bytes
   bufsize - size of the destination buffer
*/
#if 0
void MyCopyMemory(WCHAR *buf, WCHAR *pbData, SIZE_T cbData, SIZE_T bufsize) 
{
    CopyMemory(buf, pbData, min(cbData,bufsize));
}
#endif

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


    DWORD   convertedChars;

    /* Convert to OEM, then output */
    convertedChars = WideCharToMultiByte(GetConsoleOutputCP(), 0, output_bufW, -1, 
                                        output_bufA, sizeof(output_bufA),NULL, NULL);
	 printf("%s\n",output_bufA);

    return res ? nOut : 0;
}

LPCSTR pBuf;


static int mywprintfA(const char *format, ...)
{
    static char output_bufA[65536];
    va_list             parms;
    DWORD               nOut;
    BOOL                res = FALSE;
    HANDLE              hout = GetStdHandle(STD_OUTPUT_HANDLE);

    va_start(parms, format);
    vsnprintfW(output_bufA, ARRAY_SIZE(output_bufA), format, parms);
    va_end(parms);

    printf("%s\n",output_bufA);

    return res ? nOut : 0;
}


HANDLE hMapFile;

THREAD_RV THREAD_CC wine_rdp_shm_thread(VOID)
{
   printf("printf----------------------Termsv: Entered Shared Memory Thread---------------------------------\n");
   g_writeln("g_writeln----------------------Termsv: Entered Shared Memory Thread---------------------------------\n");
//   LPCWSTR pBuf;

   hMapFile = CreateFileMappingW(
                 INVALID_HANDLE_VALUE,    // use paging file
                 NULL,                    // default security
                 PAGE_READWRITE,          // read/write access
                 0,                       // maximum object size (high-order DWORD)
                 BUF_SIZE,                // maximum object size (low-order DWORD)
                 szName);                 // name of mapping object

   if (hMapFile == NULL)
   {
      fprintf("Termsv: Could not create file mapping object (%d).\n",
             GetLastError());
      return 1;
   }

#if 0   
   pBuf = (LPSTR) MapViewOfFile(hMapFile,   // handle to map object
                        FILE_MAP_ALL_ACCESS, // read/write permission
                        0,
                        0,
                        BUF_SIZE);

   if (pBuf == NULL)
   {
      fprintf("Termsv: Could not map view of file (%d).\n", GetLastError());
      CloseHandle(hMapFile);
      return 1;
   }
#endif
}

void MyCopyMemory(char *buf, char *pbData, SIZE_T cbData, SIZE_T bufsize)
{
    CopyMemory(buf, pbData, min(cbData,bufsize));
}

void termsv_shm_msg(char *msg)
{
   pBuf = (LPSTR) MapViewOfFile(hMapFile,   // handle to map object
                        FILE_MAP_ALL_ACCESS, // read/write permission
                        0,
                        0,
                        BUF_SIZE);

   if (pBuf == NULL)
   {
      fprintf("Termsv: Could not map view of file (%d).\n", GetLastError());
      CloseHandle(hMapFile);
      return 1;
   }

   MyCopyMemory(pBuf, msg, COPY_SIZE*sizeof(char), BUFFER_SIZE*sizeof(char));
   mywprintfA("Writing:\n");
   mywprintfA(pBuf);
}

