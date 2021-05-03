#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include "../utils.h"
#include "aviao_utils.h"

#define MAX 1000

typedef int(__cdecl* MYPROC)(LPWSTR);

// ouvir mensagens do controller
DWORD WINAPI ThreadReader(LPVOID param) {
    ThreadController* dados = (ThreadController*)param;
 
    while (1) { 
        _tprintf(TEXT("A ler \n"));
        //esperar at� que evento desbloqueie
        WaitForSingleObject(dados->hEvent, INFINITE);

        //verifica se � preciso terminar a thread ou nao
        if (dados->terminar)
            break;

        //fa�o o lock para o mutex
        WaitForSingleObject(dados->hMutex, INFINITE);
        _tprintf(TEXT("Nova msg: %s\n"), dados->fileViewMap);

        //fa�o unlock do mutex
        ReleaseMutex(dados->hMutex);

        Sleep(1000);
    }

    return 0;
}

int _tmain(int argc, LPTSTR argv[]) {
	//UNICODE: Por defeito, a consola Windows n�o processa caracteres wide. 
	//A maneira mais f�cil para ter esta funcionalidade � chamar _setmode:
#ifdef UNICODE 
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif
    HANDLE hThreadReader = NULL;
    ThreadController ler;
 	HANDLE hFileMap;

    hFileMap = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        NUM_CHAR_FILE_MAP * sizeof(TCHAR), // alterar o tamanho do filemapping
        TEXT(FILE_MAP_MESSEGER_TO_PLANES)); //nome do file mapping, tem de ser �nico

    if (hFileMap == NULL) {
        _tprintf(TEXT("Erro no CreateFileMapping\n"));
        //CloseHandle(hFile); //recebe um handle e fecha esse handle , no entanto o handle � limpo sempre que o processo termina
        return 1;
    }

    //mapeia bloco de memoria para espa�o de endere�amento
    ler.fileViewMap = (TCHAR*)MapViewOfFile(
        hFileMap,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        0);

    if (ler.fileViewMap == NULL) {
        _tprintf(TEXT("Erro no MapViewOfFile\n"));
        return 1;
    }

    ler.hEvent = CreateEvent(
        NULL,
        TRUE,
        FALSE,
        TEXT(EVENT_MESSEGER_TO_PLANES));

    if (ler.hEvent == NULL) {
        _tprintf(TEXT("Erro no CreateEvent\n"));
        UnmapViewOfFile(ler.fileViewMap);
        return 1;
    }

    ler.hMutex = CreateMutex(
        NULL,
        FALSE,
        TEXT(MUTEX_MESSEGER_TO_PLANES));

    if (ler.hMutex == NULL) {
        _tprintf(TEXT("Erro no CreateMutex\n"));
        UnmapViewOfFile(ler.fileViewMap);
        return 1;
    }
    ler.terminar = 0;


    //cria threads
    hThreadReader = CreateThread(NULL, 0, ThreadReader, &ler, 0, NULL);

  
    while (1) {
        menuAviao();
        TCHAR tokenstring[50] = { 0 };
        _fgetts(tokenstring, 50, stdin);
        tokenstring[_tcslen(tokenstring) - 1] = '\0';
        TCHAR* ptr;
        TCHAR delim[] = L" ";
        TCHAR* token = wcstok_s(tokenstring, delim, &ptr);
        while (token != NULL)
        {
            //_tprintf(L"%ls\n", token);
            if (_tcscmp(token, L"prox") == 0) {
                _tprintf(TEXT("Pr�ximo destino definido.\n"));
            }
            else if (_tcscmp(token, L"emb") == 0) {
                _tprintf(TEXT("Embarcar passageiros.\n"));
            }
            else if (_tcscmp(token, L"init") == 0) {
                _tprintf(TEXT("Iniciar viagem.\n"));
            }
            else if (_tcscmp(token, L"quit") == 0) {
                _tprintf(TEXT("Sair de inst�ncia de avi�o.\n"));
            }
            token = wcstok_s(NULL, delim, &ptr);
        }
    }


    if (hThreadReader != NULL)
        WaitForSingleObject(hThreadReader, INFINITE);

}