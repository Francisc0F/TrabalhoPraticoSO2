#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include <fcntl.h>
#include "../utils.h"

#define BUFFER 200

BOOL validaArgs(int argc, LPTSTR argv[], TCHAR origem[], TCHAR destino[], TCHAR nome[], int* tempoEspera) {
	*tempoEspera = -1;

	if (argc >= 4) {
		if (tokenValid(argv[1]) && tokenValid(argv[2]) && tokenValid(argv[3])) {
			_tcscpy_s(origem, BUFFER, argv[1]);
			_tcscpy_s(destino, BUFFER, argv[2]);
			_tcscpy_s(nome, BUFFER, argv[3]);
			if (tokenValid(argv[4]) && isNumber(argv[4])) {
				*tempoEspera = _tstoi(argv[4]);
			}
			return TRUE;
		}

	}
	return FALSE;
}


BOOL WriteToPIPE(DWORD * cbToWrite, BOOL *fSuccess, MensagemPipe* Message, HANDLE * hPipe, DWORD* cbWritten) {
	*cbToWrite = sizeof(MensagemPipe);
	*fSuccess = WriteFile(
		*hPipe,                  // pipe handle 
		Message,				// message 
		*cbToWrite,              // message length 
		cbWritten,             // bytes written 
		NULL);                  // not overlapped 

	if (!*fSuccess)
	{
		_tprintf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
		return FALSE;
	}
	return TRUE;
}

BOOL ReadFromPIPE(DWORD* cbRead, BOOL* fSuccess, MensagemPipe* Message, HANDLE* hPipe) {
	do
	{
		*fSuccess = ReadFile(
			*hPipe,    // pipe handle 
			Message,    // buffer to receive reply 
			sizeof(MensagemPipe),  // size of buffer 
			cbRead,  // number of bytes read 
			NULL);    // not overlapped 

		if (!*fSuccess && GetLastError() != ERROR_MORE_DATA)
			return FALSE;

		
	} while (!*fSuccess);  // repeat loop if ERROR_MORE_DATA 

	if (!*fSuccess)
	{
		_tprintf(TEXT("ReadFile from pipe failed. GLE=%d\n"), GetLastError());
		return FALSE;
	}

	return TRUE;
}



int _tmain(int argc, LPTSTR argv[])
{
#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif
	
	TCHAR* erro = TEXT("Dados Invalidos passageiro Terminou.\n");


	HANDLE hPipe;
	MensagemPipe Message;
	ZeroMemory(&Message, sizeof(MensagemPipe));
	Message.autor.pid = GetCurrentProcessId();

	BOOL   fSuccess = FALSE;
	DWORD  cbRead, cbToWrite, cbWritten, dwMode;

	TCHAR origem[BUFFER] = { 0 };
	TCHAR destino[BUFFER] = { 0 };
	TCHAR nome[BUFFER] = { 0 };
	int tempoEspera = -1;

	if (validaArgs(argc, argv, origem, destino, nome, &tempoEspera)) {
		_tprintf(TEXT("Dados VALIDOS.\n"));
		_tcscpy_s(Message.autor.destino, _countof(Message.autor.destino), destino);
		_tcscpy_s(Message.autor.origem, _countof(Message.autor.origem), origem);
		_tcscpy_s(Message.autor.nome, _countof(Message.autor.nome), nome);
		_tcscpy_s(Message.mensagem, _countof(Message.mensagem), TEXT("1"));

		// Try to open a named pipe; wait for it, if necessary. 
#pragma region connect to pipe
		while (1)
		{
			hPipe = CreateFile(
				PIPEPASSAG,   // pipe name 
				GENERIC_READ |  // read and write access 
				GENERIC_WRITE,
				0,              // no sharing 
				NULL,           // default security attributes
				OPEN_EXISTING,  // opens existing pipe 
				FILE_FLAG_OVERLAPPED,           
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
#pragma endregion
		// The pipe connected; change to message-read mode. 

		// config pipe
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


		while (1) {
			//WaitForSingleObject(hPipe., INFINITE);
			if (!WriteToPIPE(&cbToWrite, &fSuccess, &Message, &hPipe, &cbWritten)) {
				return -1;
			}

			if (!ReadFromPIPE(&cbRead, &fSuccess, &Message, &hPipe)) {
				return -1;
			}
			else {
				_tprintf(TEXT("Controlador: %s\n"), Message.mensagem);
				if (_tcscmp(Message.mensagem, TEXT("erro")) == 0) {
					_tprintf(erro);
					return -1;
				}
			}
		}
	


		CloseHandle(hPipe);

	}
	else {
		_tprintf(erro);
		return -1;
	}
}
