#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include "../utils.h"
#include "controlador_utils.h"
#define MAX 1000
#define MAP 1000
#define MAXAVIOES 100

typedef int(__cdecl* MYPROC)(LPWSTR);

DWORD WINAPI ThreadEscrever(LPVOID param) {
	TCHAR msg[NUM_CHAR_FILE_MAP];
	ThreadController* dados = (ThreadController*)param;

	while (!(dados->terminar)) {
		_tprintf(TEXT("Escreve \n"));

		_fgetts(msg, NUM_CHAR_FILE_MAP, stdin);
		msg[_tcslen(msg) - 1] = '\0'; //terminamos a string de maneira correta

		if (_tcscmp(msg, TEXT("fim")) == 0)
			dados->terminar = 1;


		//faço lock ao mutex
		WaitForSingleObject(dados->hMutex, INFINITE);

		//limpa memoria antes de fazer a copia
		ZeroMemory(dados->fileViewMap, NUM_CHAR_FILE_MAP * sizeof(TCHAR));

		//copia memoria de um sitio para outro (aqui copia a mensagem escrita no terminal para o fileViewMap)
		CopyMemory(dados->fileViewMap, msg, _tcslen(msg) * sizeof(TCHAR));

		//liberto mutex
		ReleaseMutex(dados->hMutex);

		//desbloqueia evento
		SetEvent(dados->hEvent);
		Sleep(500);

		ResetEvent(dados->hEvent); //bloqueia evento
	}
	return 0;
}


//DWORD WINAPI ThreadLerBufferCircular(LPVOID param) {
//	BufferCircularLer* dados = (BufferCircularLer*)param;
//	CelulaBuffer cel;
//	int contador = 0;
//	int soma = 0;
//
//	while (!dados->terminar) {
//
//
//		//aqui entramos na logica da aula teorica
//
//		//esperamos por uma posicao para lermos
//		WaitForSingleObject(dados->hSemLeitura, INFINITE);
//
//		//esperamos que o mutex esteja livre
//		WaitForSingleObject(dados->hMutex, INFINITE);
//
//
//		//vamos copiar da proxima posicao de leitura do buffer circular para a nossa variavel cel
//		CopyMemory(&cel, &dados->memPar->buffer[dados->memPar->posL], sizeof(CelulaBuffer));
//		dados->memPar->posL++; //incrementamos a posicao de leitura para o proximo consumidor ler na posicao seguinte
//
//		//se apos o incremento a posicao de leitura chegar ao fim, tenho de voltar ao inicio
//		if (dados->memPar->posL == TAM_BUFFER)
//			dados->memPar->posL = 0;
//
//		//libertamos o mutex
//		ReleaseMutex(dados->hMutex);
//
//		//libertamos o semaforo. temos de libertar uma posicao de escrita
//		ReleaseSemaphore(dados->hSemEscrita, 1, NULL);
//
//		contador++;
//		soma += cel.val;
//		_tprintf(TEXT("C%d consumiu %d.\n"), dados->id, cel.val);
//	}
//	_tprintf(TEXT("C%d consumiu %d items.\n"), dados->id, soma);
//
//	return 0;
//}


int _tmain(int argc, TCHAR* argv[]) {
	//UNICODE: Por defeito, a consola Windows não processa caracteres wide. 
	//A maneira mais fácil para ter esta funcionalidade é chamar _setmode:
#ifdef UNICODE 
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif

	// regedit keys setup 
	TCHAR key_dir[TAM] = TEXT("Software\\TRABALHOSO2\\");
	HKEY handle = NULL; // handle para chave depois de aberta ou criada
	DWORD handleRes = NULL;
	TCHAR key_name[TAM] = TEXT("N_avioes"); //nome do par-valor
	int maxAvioes;
	checkRegEditKeys(key_dir, handle, handleRes, TEXT("N_avioes"), &maxAvioes);

	

	Aeroporto listaAeroportos[MAXAEROPORTOS] = { 0 };


	//ThreadController escrever;
	//HANDLE hFileMap;
	//HANDLE hEscrita;
	//ThreadEnvioDeMsgParaAvioes(&escrever, &hFileMap, &hEscrita);
	//hEscrita = CreateThread(NULL, 0, ThreadEscrever, &escrever, 0, NULL);


	// menu  
	//while (1) {
	//	menuControlador();
	//	TCHAR tokenstring[50] = { 0 };
	//	_fgetts(tokenstring, 50, stdin);
	//	tokenstring[_tcslen(tokenstring) - 1] = '\0';
	//	TCHAR* ptr;
	//	TCHAR delim[] = L" ";
	//	TCHAR* token = wcstok_s(tokenstring, delim, &ptr);

	//	TCHAR nome[100];
	//	int y;
	//	int x;
	//	while (token != NULL)
	//	{
	//		//_tprintf(L"%ls\n", token);
	//		if (_tcscmp(token, L"addAero") == 0) {
	//			token = wcstok_s(NULL, delim, &ptr);
	//			if (token != NULL) {
	//				_tcscpy_s(nome, _countof(nome), token);
	//				token = wcstok_s(NULL, delim, &ptr);
	//				if (token != NULL) {
	//					x = _tstoi(token);
	//					token = wcstok_s(NULL, delim, &ptr);
	//					if (token != NULL) {
	//						y = _tstoi(token);
	//						adicionarAeroporto(nome, x, y, listaAeroportos);
	//					}
	//				}
	//			}
	//		}
	//		else if (_tcscmp(token, L"lista") == 0) {
	//			listaTudo(listaAeroportos);
	//		}
	//		else if (_tcscmp(token, L"suspender") == 0) {
	//			_putws(TEXT("suspende aceitação de novos aviões por parte dos utilizadores"));
	//		}
	//		else if (_tcscmp(token, L"ativar") == 0) {
	//			_putws(TEXT("ativa aceitação de novos aviões por parte dos utilizadores"));
	//		}
	//		else if (_tcscmp(token, L"end") == 0) {
	//			_tprintf(TEXT("Encerrar sistema, todos os processos serão notificados.\n"));
	//		}
	//		token = wcstok_s(NULL, delim, &ptr);
	//	}
	//}






	//if (hEscrita != NULL)
	//	WaitForSingleObject(hEscrita, INFINITE);

}