#include "win_serialcomm.h"

void WinSerialComm::BuildDefaultDcb()
{
    memset( &_DCB, 0, sizeof(_DCB) );
    _DCB.DCBlength = sizeof(_DCB);	
    _DCB.BaudRate = CBR_115200;
    _DCB.fBinary = TRUE;
    _DCB.Parity = NOPARITY;
    _DCB.StopBits = ONESTOPBIT;
    _DCB.ByteSize = 8;
    //flow control is none
}

// Init 
void WinSerialComm::Init()
{
    memset(_szCommStr, 0, 20);
    BuildDefaultDcb();
    _hCommHandle = INVALID_HANDLE_VALUE;

    memset(&_ReadOverlapped, 0, sizeof(_ReadOverlapped));
    memset(&_WriteOverlapped, 0, sizeof(_WriteOverlapped));

    _ReadOverlapped.hEvent = ::CreateEvent(NULL, true, false, NULL);
    assert(_ReadOverlapped.hEvent != NULL); 

    _WriteOverlapped.hEvent = ::CreateEvent(NULL, true, false, NULL);
    assert(_WriteOverlapped.hEvent != NULL);

    _dwNotifyNum = 0;
    _dwMaskEvent = DEFAULT_COM_MASK_EVENT;
    _hThreadHandle = NULL;

    memset(&_WaitOverlapped, 0, sizeof(_WaitOverlapped));
    _WaitOverlapped.hEvent = ::CreateEvent(NULL, true, false, NULL);
    assert(_WaitOverlapped.hEvent != NULL); 
} 

// Destroy resource
void WinSerialComm::Destroy()
{
    if(_ReadOverlapped.hEvent != NULL)
        CloseHandle(_ReadOverlapped.hEvent);

    if(_WriteOverlapped.hEvent != NULL)
        CloseHandle(_WriteOverlapped.hEvent);

    if(_WaitOverlapped.hEvent != NULL)
        CloseHandle(_WaitOverlapped.hEvent);

    _Mutex.lock();
    _Abort = true;
    _Condition.wakeOne();
    _Mutex.unlock();

    wait();
}

// Bind Comm port
void WinSerialComm::BindCommPort(DWORD dwPort)
{
    assert(dwPort >= 1 && dwPort <= 1024);

    char p[5];

    _dwPort = dwPort;
    strcpy(_szCommStr, "\\\\.\\COM"); 
    ltoa(_dwPort, p, 10);
    strcat(_szCommStr, p);
}

// Open Comm port
bool WinSerialComm::OpenCommPort()
{
    if(IsOpen())
        Close();

    _hCommHandle = ::CreateFile(
        _szCommStr,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | _dwIOMode, 
        NULL
        );

    if(_fAutoBeginThread)
    {
        start();
        if(IsOpen() && isRunning() )
        {

            return true;
        }
        else
        {
            Close();  // Creating thread fails
            return false;
        }
    }
    return IsOpen();
}

// Setup Comm port
bool WinSerialComm::SetupPort()
{
    if(!IsOpen())
        return false;


    if(!::SetupComm(_hCommHandle, 4096, 4096))
        return false; 


    if(!::GetCommTimeouts(_hCommHandle, &_CO))
        return false;
    _CO.ReadIntervalTimeout = 0;
    _CO.ReadTotalTimeoutMultiplier = 1;
    _CO.ReadTotalTimeoutConstant = 1000;
    _CO.WriteTotalTimeoutMultiplier = 1;
    _CO.WriteTotalTimeoutConstant = 1000;
    if(!::SetCommTimeouts(_hCommHandle, &_CO))
        return false; 


    if(!::PurgeComm(_hCommHandle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ))
        return false; 

    return true;
} 

//---------------------------------------threads callback-----------------------------------------------------

// TODO: replace HWND based message notification by QT signals and slots
// Thread receive data, then call it to send message to hwnd.
void WinSerialComm::OnReceive()//EV_RXCHAR
{
}

void WinSerialComm::OnDSR()
{
}

void WinSerialComm::OnCTS()
{
}

void WinSerialComm::OnBreak()
{
}

void WinSerialComm::OnTXEmpty()
{
}

void WinSerialComm::OnError()
{      
}

void WinSerialComm::OnRing()
{
}

void WinSerialComm::OnRLSD()
{
}

void WinSerialComm::run()
{
    if(!::SetCommMask(_hCommHandle, _dwMaskEvent))
    {
        return ;
    }

    COMSTAT Stat;
    DWORD dwError;

    for(DWORD dwLength, dwMask = 0; isRunning() && IsOpen(); dwMask = 0)
    {
        if(!::WaitCommEvent(_hCommHandle, &dwMask, &_WaitOverlapped))
        {
            if(::GetLastError() == ERROR_IO_PENDING)// asynchronous
                ::GetOverlappedResult(_hCommHandle, &_WaitOverlapped, &dwLength, TRUE);
            else
                continue;
        }

        if(dwMask == 0)
            continue;

        switch( dwMask & 0x1ff )
        {
        case EV_RXCHAR :
            ::ClearCommError(_hCommHandle, &dwError, &Stat);
            if(Stat.cbInQue >= _dwNotifyNum)
                emit OnReceive();
            break;

        case EV_TXEMPTY :
            emit OnTXEmpty();
            break;

        case EV_CTS :
            emit OnCTS();
            break;

        case EV_DSR :
            emit OnDSR();
            break;

        case EV_RING :
            emit OnRing();
            break;

        case EV_RLSD :
            emit OnRLSD();
            break;

        case EV_BREAK:
            emit OnBreak();
            break;

        case EV_ERR :
            emit OnError();
            break;

        }
    }
}
