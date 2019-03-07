
#include <windows.h> 
#include <stdio.h>

#if 0

#define BUFSIZE 512
 
HANDLE hPipe;

void pipe_thread(void) 
{ 
   LPSTR lpvMessage="New RDPdrv Client Connection to server\n"; 
   CHAR  chBuf[BUFSIZE]; 
   BOOL   fSuccess = FALSE; 
   DWORD  cbRead, cbToWrite, cbWritten, dwMode; 
   LPSTR lpszPipename = "\\\\.\\pipe\\rdp_server";

 //  if( argc > 1 )
 //     lpvMessage = argv[1];
 
// Try to open a named pipe; wait for it, if necessary. 
 
//   while (1) 
//   { 
      hPipe = CreateFileA( 
         lpszPipename,   // pipe name 
         GENERIC_READ |  // read and write access 
         GENERIC_WRITE, 
         0,              // no sharing 
         NULL,           // default security attributes
         OPEN_EXISTING,  // opens existing pipe 
         0,              // default attributes 
         NULL);          // no template file 
 
   // Break if the pipe handle is valid. 
 
      if (hPipe != INVALID_HANDLE_VALUE) 
      {
         printf( ("Could not open pipe. - INVALID_HANDLE_VALUE GLE=%d\n"), GetLastError() ); 
      }

      // Exit if an error other than ERROR_PIPE_BUSY occurs. 
 
      if (GetLastError() != ERROR_PIPE_BUSY) 
      {
         printf( ("Could not open pipe. - ERROR_PIPE_BUSY GLE=%d\n"), GetLastError() ); 
      }
 
      // All pipe instances are busy, so wait for 20 seconds. 
 
      if ( ! WaitNamedPipeA(lpszPipename, 20000)) 
      { 
         printf("Could not open pipe: 20 second wait timed out."); 
      } 
 
// The pipe connected; change to message-read mode. 
 
   dwMode = PIPE_READMODE_MESSAGE; 
   fSuccess = SetNamedPipeHandleState( 
      hPipe,    // pipe handle 
      &dwMode,  // new pipe mode 
      NULL,     // don't set maximum bytes 
      NULL);    // don't set maximum time 
   if ( ! fSuccess) 
   {
      printf( ("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError() ); 
   }

   /* Leave our thread around for as long as the process is attached */
 //  for(;;) {}

// Send a message to the pipe server. 
 
   cbToWrite = (lstrlenA(lpvMessage)+1)*sizeof(CHAR);
   printf( ("Sending %d byte message: \"%s\"\n"), cbToWrite, lpvMessage); 

   fSuccess = WriteFile( 
      hPipe,                  // pipe handle 
      lpvMessage,             // message 
      cbToWrite,              // message length 
      &cbWritten,             // bytes written 
      NULL);                  // not overlapped 

   if ( ! fSuccess) 
   {
      printf( ("WriteFile to pipe failed. GLE=%d\n"), GetLastError() ); 
   }

   printf("\nMessage sent to server, receiving reply as follows:\n");

   // * For Debugging
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
 
      printf( ("\"%s\"\n"), chBuf ); 
    } while ( ! fSuccess);  // repeat loop if ERROR_MORE_DATA 

    if ( ! fSuccess)
    {
      printf( ("ReadFile from pipe failed. GLE=%d\n"), GetLastError() );
      //ExitProcess(GetLastError()); 
      //return -1;
    }

    printf("RDP Client Pipe Initialization Complete");
}

// We know this function is good, break it up in to something useful
#if 0
void send_msg_pipe(char *msg)
{
   CHAR  chBuf[BUFSIZE];
   BOOL   fSuccess = FALSE;
   DWORD  cbRead, cbToWrite, cbWritten, dwMode;
   LPSTR lpvMessage="send_msg_pipe: Test Message\n"; 

   cbToWrite = (lstrlenA(lpvMessage)+1)*sizeof(CHAR);
   printf( ("send_msg_pipe: sending %d byte message: \"%s\"\n"), cbToWrite, lpvMessage);

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

   printf("\nMessage sent to server, receiving reply as follows:\n");

   // * For Debugging
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

      printf( ("\"%s\"\n"), chBuf );
    } while ( ! fSuccess);  // repeat loop if ERROR_MORE_DATA 

    if ( ! fSuccess)
    {
      printf( ("Message Confirmation from pipe failed. GLE=%d\n"), GetLastError() );
      return -1;
    }
}
#endif

void rdpdrv_send_msg_pipe(char *msg)
{
   CHAR  chBuf[BUFSIZE];
   BOOL   fSuccess = FALSE;
   DWORD  cbRead, cbToWrite, cbWritten, dwMode;
   LPSTR lpvMessage="rdpdrv_send_msg_pipe: Test Message\n";

   cbToWrite = (lstrlenA(lpvMessage)+1)*sizeof(CHAR);
   printf( ("rdpdrv_msg_pipe: sending %d byte message from RDP Driver: \"%s\"\n"), cbToWrite, lpvMessage);

   fSuccess = WriteFile(
      hPipe,                  // pipe handle
      lpvMessage,             // message
      cbToWrite,              // message length
      &cbWritten,             // bytes written
      NULL);                  // not overlapped

   if ( ! fSuccess)
   {
      printf( ("rdpdrv_send_msg_pipe pipe failed. GLE=%d\n"), GetLastError() );
   }
}

#if 0
void rdpdrv_msg_pipe()
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

      printf( ("rdpdrv_read_msg_pipe: \"%s\"\n"), chBuf );
    } while ( ! fSuccess);  // repeat loop if ERROR_MORE_DATA

    if ( ! fSuccess)
    {
      printf( ("Message Confirmation from pipe failed. GLE=%d\n"), GetLastError() );
      return -1;
    }
}
#endif
#endif


static int rdpdrv_printfA(const char *format, ...)
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


#include "wine/debug.h"

#define BUF_SIZE 4096 

WINE_DEFAULT_DEBUG_CHANNEL(rdp);

const WCHAR szName[]={'G','l','o','b','a','l','\\','M','y','F','i','l','e','M','a','p','p','i','n','g','O','b','j','e','c','t',0};

HANDLE hMapFile;
LPCSTR pBuf;

void pipe_thread(void)
{
//   HANDLE hMapFile;
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
      FIXME("RDPdrv - Could not create file mapping object (%d).\n",
             GetLastError());
   }
/*
   pBuf = (LPSTR) MapViewOfFile(hMapFile,   // handle to map object
                        FILE_MAP_ALL_ACCESS, // read/write permission
                        0,
                        0,
                        BUF_SIZE);

   if (pBuf == NULL)
   {
      FIXME("RDPdrv - Could not map view of file (%d).\n", GetLastError());
      CloseHandle(hMapFile);
   }
 */
}

void rdpdrv_read_shm_msg(void)
{
   rdpdrv_printfA("rdpdrv_read_shm_msg: Attempting to read message sent by Termsv.exe.so\n");

   pBuf = (LPSTR) MapViewOfFile(hMapFile,   // handle to map object
                        FILE_MAP_ALL_ACCESS, // read/write permission
                        0,
                        0,
                        BUF_SIZE);

   if (pBuf == NULL)
   {
      FIXME("RDPdrv - Could not map view of file (%d).\n", GetLastError());
      CloseHandle(hMapFile);
   }

    mywprintfA("rdpdrv------------read:\n");
    mywprintfA(pBuf);
    mywprintfA("rdpdrv-------- From shared memory\n");
}

