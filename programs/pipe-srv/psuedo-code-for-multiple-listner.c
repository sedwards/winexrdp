Main Thread
{
    CreateListenerThread();
    WaitForQuitEvent();
}

ListenerThread
{
    ConnectNamedPipe();
    if (no error)
    {
        CreateListenerThread();
        if( PeekNamedPipe() has a message )
        {
            ReadFile();
            ProcessReceivedMessage(); // if -quit signal quit event
        }
        FileFlushBuffers();
        DisconnectNamedPipe();
        CloseHandle();
    }
    else
    {
        // handle/report error
    }
}

