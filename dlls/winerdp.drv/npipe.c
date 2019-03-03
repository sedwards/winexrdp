#include <windows.h> 
#include <stdio.h>
//#include <conio.h>
//#include <tchar.h>

#define BUFSIZE 512
 
HANDLE hPipe;

void pipe_thread(void) 
{ 
   LPSTR lpvMessage="New RDP Client Connection to server\n"; 
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
      return -1;
    }

    printf("RDP Client Pipe Initialization Complete");
}


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

