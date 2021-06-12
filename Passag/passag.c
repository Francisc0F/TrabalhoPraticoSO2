#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include <fcntl.h>
#include "../utils.h"

BOOL validaArgs(int argc, LPTSTR argv[]) {
	TCHAR origem[100] = { 0 };
	TCHAR destino[100] = { 0 };
	TCHAR nome[100] = { 0 };
	int tempoEspera = -1;
	if (argc >= 3) {
		if (tokenValid(argv[0]) && tokenValid(argv[1]) && tokenValid(argv[2])) {
			_tcscpy_s(origem, _countof(origem), argv[0]);
			_tcscpy_s(destino, _countof(destino), argv[1]);
			_tcscpy_s(nome, _countof(nome), argv[2]);
			if (tokenValid(argv[3]) && isNumber(argv[3])) {
				tempoEspera = _tstoi(argv[3]);
			}
			return TRUE;
		}

	}
	return FALSE;
}

int _tmain(int argc, LPTSTR argv[])
{
#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif
	HANDLE hPipe;
	MensagemPipe Message;
	Message.autor.pid = GetCurrentProcessId();

	BOOL   fSuccess = FALSE;
	DWORD  cbRead, cbToWrite, cbWritten, dwMode;

	if (validaArgs(argc, argv)) {
		_tprintf(TEXT("Dados VALIDOS.\n"));


		// Try to open a named pipe; wait for it, if necessary. 

		while (1)
		{
			hPipe = CreateFile(
				PIPEPASSAG,   // pipe name 
				GENERIC_READ |  // read and write access 
				GENERIC_WRITE,
				0,              // no sharing 
				NULL,           // default security attributes
				OPEN_EXISTING,  // opens existing pipe 
				0,              // default attributes 
				NULL);          // no template file 

		  // Break if the pipe handle is valid. 

			if (hPipe != INVALID_HANDLE_VALUE)
				break;

			// Exit if an error other than ERROR_PIPE_BUSY occurs. 

			if (GetLastError() != ERROR_PIPE_BUSY)
			{
				_tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
				return -1;
			}

			// All pipe instances are busy, so wait for 20 seconds. 

			if (!WaitNamedPipe(PIPEPASSAG, 20000))
			{
				_tprintf("Could not open pipe: 20 second wait timed out.");
				return -1;
			}
		}

		// The pipe connected; change to message-read mode. 

		dwMode = PIPE_READMODE_MESSAGE;
		fSuccess = SetNamedPipeHandleState(
			hPipe,    // pipe handle 
			&dwMode,  // new pipe mode 
			NULL,     // don't set maximum bytes 
			NULL);    // don't set maximum time 
		if (!fSuccess)
		{
			_tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
			return -1;
		}


		// Send a message to the pipe server. 
		while (1) {

			_tprintf(TEXT("Envia: "));
			_fgetts(Message.mensagem, 100, stdin);
			Message.mensagem[_tcslen(Message.mensagem) - 1] = '\0';

			cbToWrite = sizeof(MensagemPipe);
			_tprintf(TEXT("Sending %d byte message: \"%s\"\n"), cbToWrite, Message.mensagem);

			fSuccess = WriteFile(
				hPipe,                  // pipe handle 
				&Message,				// message 
				cbToWrite,              // message length 
				&cbWritten,             // bytes written 
				NULL);                  // not overlapped 

			if (!fSuccess)
			{
				_tprintf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
				return -1;
			}

			_tprintf(L"\nMessage sent to server, receiving reply as follows:\n");

			do
			{
				// Read from the pipe. 

				fSuccess = ReadFile(
					hPipe,    // pipe handle 
					&Message,    // buffer to receive reply 
					sizeof(MensagemPipe),  // size of buffer 
					&cbRead,  // number of bytes read 
					NULL);    // not overlapped 

				if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
					break;

				_tprintf(TEXT("\"server: %s\"\n"), Message.mensagem);
			} while (!fSuccess);  // repeat loop if ERROR_MORE_DATA 

			if (!fSuccess)
			{
				_tprintf(TEXT("ReadFile from pipe failed. GLE=%d\n"), GetLastError());
				return -1;
			}
		}

		CloseHandle(hPipe);


	}
	else {
		_tprintf(TEXT("Dados Invalidos passageiro Terminou.\n"));
		return -1;
	}
}
