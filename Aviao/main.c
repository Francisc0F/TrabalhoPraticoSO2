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
		//esperar até que evento desbloqueie
		WaitForSingleObject(dados->hEvent, INFINITE);

		//verifica se é preciso terminar a thread ou nao
		if (dados->terminar)
			break;

		//faço o lock para o mutex
		WaitForSingleObject(dados->hMutex, INFINITE);
		_tprintf(TEXT("Nova msg: %s\n"), dados->fileViewMap);

		//faço unlock do mutex
		ReleaseMutex(dados->hMutex);

		Sleep(1000);
	}

	return 0;
}

DWORD WINAPI ThreadWriter(LPVOID param) {
	MSGThread* dados = (MSGThread*)param;
	MSGcel cel;
	int contador = 0;

	while (!dados->terminar) {
		WaitForSingleObject(dados->hEventEnviarMSG, INFINITE);

		cel.id = dados->id;
		cel.x = dados->x;
		cel.y = dados->y;
		_tcscpy_s(cel.info, _countof(cel.info), dados->info);

		//aqui entramos na logica da aula teorica

		//esperamos por uma posicao para escrevermos
		WaitForSingleObject(dados->hSemEscrita, INFINITE);

		//esperamos que o mutex esteja livre
		WaitForSingleObject(dados->hMutex, INFINITE);

		//vamos copiar a variavel cel para a memoria partilhada (para a posição de escrita)
		CopyMemory(&dados->memPar->buffer[dados->memPar->posE], &cel, sizeof(MSGcel));
		dados->memPar->posE++; //incrementamos a posicao de escrita para o proximo produtor escrever na posicao seguinte

		//se apos o incremento a posicao de escrita chegar ao fim, tenho de voltar ao inicio
		if (dados->memPar->posE == TAM_BUFFER)
			dados->memPar->posE = 0;

		//libertamos o mutex
		ReleaseMutex(dados->hMutex);

		//libertamos o semaforo. temos de libertar uma posicao de leitura
		ReleaseSemaphore(dados->hSemLeitura, 1, NULL);

		_tprintf(TEXT("Aviao %d enviou \n"), dados->id);
		printMSG(cel);
		Sleep(1000);
	}

	return 0;
}


int _tmain(int argc, LPTSTR argv[]) {
	//UNICODE: Por defeito, a consola Windows não processa caracteres wide. 
	//A maneira mais fácil para ter esta funcionalidade é chamar _setmode:
#ifdef UNICODE 
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif
	//HANDLE hThreadReader = NULL;
	//ThreadController ler;
	//HANDLE hFileLeituraMap;

	//preparaLeituraMSGdoAviao(&hFileLeituraMap, &ler);
	//hThreadReader = CreateThread(NULL, 0, ThreadReader, &ler, 0, NULL);


	HANDLE hThreadWriter = NULL;
	HANDLE hFileEscritaMap;
	MSGThread escreve;
	BOOL  primeiroProcesso = FALSE;

	preparaEnvioDeMensagensParaOControlador(&hFileEscritaMap, &escreve, &primeiroProcesso);

	_tprintf(TEXT("Antes de lancar thread Escreva qualquer coisa para enviar\n"));
	_getts_s(escreve.info, 99);

	hThreadWriter = CreateThread(NULL, 0, ThreadWriter, &escreve, 0, NULL);
	

	// method enviar mensagem para controlador
	TCHAR comando[100];
	while (1) {
		_tprintf(TEXT("Escreva qualquer coisa para enviar\n"));
		_getts_s(escreve.info, 99);
		//desbloqueia evento
		SetEvent(escreve.hEventEnviarMSG);
		Sleep(100);
		ResetEvent(escreve.hEventEnviarMSG); //bloqueia evento
	}

	if (hThreadWriter != NULL) {
		//_tprintf(TEXT("Escreva qualquer coisa para sair ...\n"));
		//_getts_s(comando, 100);
	//	escreve.terminar = 1;

		//esperar que a thread termine
		WaitForSingleObject(hThreadWriter, INFINITE);
	}

	
	//while (1) {
	//	menuAviao();
	//	TCHAR tokenstring[50] = { 0 };
	//	_fgetts(tokenstring, 50, stdin);
	//	tokenstring[_tcslen(tokenstring) - 1] = '\0';
	//	TCHAR* ptr;
	//	TCHAR delim[] = L" ";
	//	TCHAR* token = wcstok_s(tokenstring, delim, &ptr);
	//	while (token != NULL)
	//	{
	//		//_tprintf(L"%ls\n", token);
	//		if (_tcscmp(token, L"prox") == 0) {
	//			_tprintf(TEXT("Próximo destino definido.\n"));
	//		}
	//		else if (_tcscmp(token, L"emb") == 0) {
	//			_tprintf(TEXT("Embarcar passageiros.\n"));
	//		}
	//		else if (_tcscmp(token, L"init") == 0) {
	//			_tprintf(TEXT("Iniciar viagem.\n"));
	//		}
	//		else if (_tcscmp(token, L"quit") == 0) {
	//			_tprintf(TEXT("Sair de instância de avião.\n"));
	//		}
	//		token = wcstok_s(NULL, delim, &ptr);
	//	}
	//}

	//if (hThreadReader != NULL)
	//	WaitForSingleObject(hThreadReader, INFINITE);



	// prox posicao
		 //dll
	/*_tprintf(TEXT("Hello"));
	TCHAR dll[MAX] = TEXT("C:\\Users\\Francisco\\source\\repos\\TrabalhoPraticoSO2\\SO2_TP_DLL_2021\\x64\\SO2_TP_DLL_2021.dll");
	TCHAR dll[MAX] = TEXT("SO2_TP_DLL_2021.dll");
	HINSTANCE hinstLib = NULL;
	hinstLib = LoadLibrary(dll);

	MYPROC ProcAdd = NULL;
	BOOL fFreeResult, fRunTimeLinkSuccess = FALSE;*/

	//if (hinstLib != NULL)
	//{
	//	
	//	ProcAdd = (MYPROC)GetProcAddress(hinstLib, "move");

	//	// If the function address is valid, call the function.
	//	if (NULL != ProcAdd)
	//	{
	//		fRunTimeLinkSuccess = TRUE;
	//		int nextX = 15;
	//		int nextY = 10;
	//		/*int currX = 15;
	//		int currY = 10;*/
	//		int status = 1;
	//		while (status == 1) {
	//			//int move(int cur_x, int cur_y, int final_dest_x, int final_dest_y, int * next_x, int* next_y)
	//			status = (ProcAdd)(nextX, nextY, 50, 50, &nextX, &nextY);
	//			// status 1 mov correta, 2 erro, 0 chegou 
	//			_tprintf(TEXT("\nprev pos (%d,%d)  -> (%d,%d) status %d"), nextX, nextY, nextX, nextY, status);
	//		}
	//	}
	// //Free the DLL module.

	//	fFreeResult = FreeLibrary(hinstLib);
	//	_tprintf(TEXT("Free lib"));
	//}

	//unsigned int map[MAX][MAX] = { 0 };
	//unsigned int i;





}