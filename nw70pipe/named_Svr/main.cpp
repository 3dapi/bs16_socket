//
//
////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable:4996)

#include <windows.h>

#include <stdio.h>
#include <time.h>

#define CONNECTING_STATE 0
#define READING_STATE 1
#define WRITING_STATE 2
#define INSTANCES 4
#define PIPE_TIMEOUT 5000
#define MAX_BUF 8192

typedef struct
{
	OVERLAPPED oOverlap;
	HANDLE hPipeInst;
	char chRequest[MAX_BUF];
	DWORD cbRead;
	char chReply[MAX_BUF];
	DWORD cbToWrite;
	DWORD dwState;
	int fPendingIO;
} PIPEINST, *LPPIPEINST;


VOID DisconnectAndReconnect(DWORD);
int ConnectToNewClient(HANDLE, LPOVERLAPPED);
VOID GetAnswerToRequest(LPPIPEINST);

PIPEINST Pipe[INSTANCES];
HANDLE hEvents[INSTANCES];


char g_sPipeName[] = "\\\\.\\pipe\\mynamedpipe";


int main()
{
	DWORD cbRet, dwErr;
	int i;
	int hr;
	HANDLE hPipe = nullptr;


	for(i = 0; i < INSTANCES; ++i)
	{
		hEvents[i] = CreateEvent(nullptr, TRUE, TRUE, nullptr);

		Pipe[i].oOverlap.hEvent = hEvents[i];

		hPipe = CreateNamedPipe(g_sPipeName
							,	PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED	// overlapped mode
							,	PIPE_TYPE_MESSAGE |			// message type pipe
								PIPE_READMODE_MESSAGE |		// message-read mode
								PIPE_WAIT					// blocking mode
							,	PIPE_UNLIMITED_INSTANCES	// max. instances
							,	MAX_BUF						// output buffer size
							,	MAX_BUF						// input buffer size
							,	NMPWAIT_USE_DEFAULT_WAIT	// client time-out
							,	nullptr);						// default security attribute

		if(Pipe[i].hPipeInst == INVALID_HANDLE_VALUE)
		{
			printf("CreateNamedPipe failed with %d.\n", GetLastError());
			return 0;
		}


		Pipe[i].hPipeInst = hPipe;
		// Call the subroutine to connect to the new client

		Pipe[i].fPendingIO = ConnectToNewClient(Pipe[i].hPipeInst, &Pipe[i].oOverlap);

		Pipe[i].dwState = Pipe[i].fPendingIO ?
			CONNECTING_STATE : // still connecting
			READING_STATE;	 // ready to read
	}

	while(1)
	{
	// Wait for the event object to be signaled, indicating
	// completion of an overlapped read, write, or
	// connect operation.

	i = WaitForMultipleObjects(INSTANCES, hEvents, FALSE, INFINITE);	// waits indefinitely

	i -= WAIT_OBJECT_0;

	if(i < 0 || i >(INSTANCES - 1))
	{
	printf("Index out of range.\n");
	return 0;
	}

	// Get the result if the operation was pending.

	if(Pipe[i].fPendingIO)
	{
	hr = GetOverlappedResult(
	Pipe[i].hPipeInst, // handle to pipe
	&Pipe[i].oOverlap, // OVERLAPPED structure
	&cbRet,			// bytes transferred
	FALSE);			// do not wait

	switch(Pipe[i].dwState)
	{
	// Pending connect operation
	case CONNECTING_STATE:
	if(! hr)
	{
	printf("Error %d.\n", GetLastError());
	return 0;
	}
	Pipe[i].dwState = READING_STATE;
	break;

	// Pending read operation
	case READING_STATE:
	if(! hr || cbRet == 0)
	{
	DisconnectAndReconnect(i);
	continue;
	}
	Pipe[i].cbRead = cbRet;
	Pipe[i].dwState = WRITING_STATE;
	break;

	// Pending write operation
	case WRITING_STATE:
	if(! hr || cbRet != Pipe[i].cbToWrite)
	{
	DisconnectAndReconnect(i);
	continue;
	}
	Pipe[i].dwState = READING_STATE;
	break;

	default:
	{
	printf("Invalid pipe state.\n");
	return 0;
	}
	}
	}

	// The pipe state determines which operation to do next.

	switch(Pipe[i].dwState)
	{
	// READING_STATE:
	// The pipe instance is connected to the client
	// and is ready to read a request from the client.

	case READING_STATE:
	hr = ReadFile(
	Pipe[i].hPipeInst,
	Pipe[i].chRequest,
	MAX_BUF*sizeof(char),
	&Pipe[i].cbRead,
	&Pipe[i].oOverlap);

	// The read operation completed successfully.

	if(hr && Pipe[i].cbRead != 0)
	{
	Pipe[i].fPendingIO = FALSE;
	Pipe[i].dwState = WRITING_STATE;
	continue;
	}

	// The read operation is still pending.

	dwErr = GetLastError();
	if(! hr &&(dwErr == ERROR_IO_PENDING))
	{
	Pipe[i].fPendingIO = TRUE;
	continue;
	}

	// An error occurred; disconnect from the client.

	DisconnectAndReconnect(i);
	break;

	// WRITING_STATE:
	// The request was successfully read from the client.
	// Get the reply data and write it to the client.

	case WRITING_STATE:
	GetAnswerToRequest(&Pipe[i]);

	hr = WriteFile(
	Pipe[i].hPipeInst,
	Pipe[i].chReply,
	Pipe[i].cbToWrite,
	&cbRet,
	&Pipe[i].oOverlap);

	// The write operation completed successfully.

	if(hr && cbRet == Pipe[i].cbToWrite)
	{
	Pipe[i].fPendingIO = FALSE;
	Pipe[i].dwState = READING_STATE;
	continue;
	}

	// The write operation is still pending.

	dwErr = GetLastError();
	if(! hr &&(dwErr == ERROR_IO_PENDING))
	{
	Pipe[i].fPendingIO = TRUE;
	continue;
	}

	// An error occurred; disconnect from the client.

	DisconnectAndReconnect(i);
	break;

	default:
	{
	printf("Invalid pipe state.\n");
	return 0;
	}
	}
	}

	return 0;
}


// DisconnectAndReconnect(DWORD)
// This function is called when an error occurs or when the client
// closes its handle to the pipe. Disconnect from this client, then
// call ConnectNamedPipe to wait for another client to connect.

VOID DisconnectAndReconnect(DWORD i)
{
	// Disconnect the pipe instance.

	if(! DisconnectNamedPipe(Pipe[i].hPipeInst) )
	{
		printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
	}

	// Call a subroutine to connect to the new client.

	Pipe[i].fPendingIO = ConnectToNewClient(Pipe[i].hPipeInst, &Pipe[i].oOverlap);

	Pipe[i].dwState = Pipe[i].fPendingIO ?
	CONNECTING_STATE : // still connecting
	READING_STATE;	 // ready to read
}

// ConnectToNewClient(HANDLE, LPOVERLAPPED)
// This function is called to start an overlapped connect operation.
// It returns TRUE if an operation is pending or FALSE if the
// connection has been completed.

int ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
	int fConnected, fPendingIO = FALSE;
	lpo->Offset = 0;		 //MISSING!
	lpo->OffsetHigh = 0;	 //MISSING!

	// Start an overlapped connection for this pipe instance.
	fConnected = ConnectNamedPipe(hPipe, lpo);

	// Overlapped ConnectNamedPipe should return zero.
	if(fConnected)
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	switch(GetLastError())
	{
		// The overlapped connection in progress.
		case ERROR_IO_PENDING:
			fPendingIO = TRUE;
			break;

		// Client is already connected, so signal an event.

		case ERROR_PIPE_CONNECTED:
			if(SetEvent(lpo->hEvent))
				break;

		// If an error occurs during the connect operation...
		default:
		{
			printf("ConnectNamedPipe failed with %d.\n", GetLastError());
			return 0;
		}
	}

	return fPendingIO;
}


VOID GetAnswerToRequest(LPPIPEINST pipe)
{
	printf( "[%d] %s\n", pipe->hPipeInst, pipe->chRequest);

	strcpy( pipe->chReply, "Default answer from server");

	pipe->cbToWrite = strlen(pipe->chReply);
}

