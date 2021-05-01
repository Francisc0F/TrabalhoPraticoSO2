#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include "../utils.h"
#define MAX 1000
#define MAXAVIOES 100
#define TAM 200


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

int _tmain(int argc, TCHAR* argv[]) {
	//UNICODE: Por defeito, a consola Windows não processa caracteres wide. 
	//A maneira mais fácil para ter esta funcionalidade é chamar _setmode:
#ifdef UNICODE 
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif
	// registry key
	
	TCHAR key_dir[TAM] = TEXT("Software\\TRABALHOSO2\\");
	//TCHAR key[TAM] = TEXT("Control");
	DWORD key_res;
	HKEY handle; // handle para chave depois de aberta ou criada
	DWORD handleRes;

	TCHAR key_name[TAM] = TEXT("N_avioes"); //nome do par-valor
	TCHAR key_value[TAM]; //nome do par-valor

	
	DWORD tamanho = sizeof(key_value);

	int maxAvioes;

	Aviao listAvioes[MAX];


	//if (RegCreateKeyEx(
	//	HKEY_CURRENT_USER,
	//	key_dir,
	//	0,
	//	NULL,
	//	REG_OPTION_NON_VOLATILE,
	//	KEY_ALL_ACCESS,
	//	NULL,
	//	&handle,
	//	&handleRes
	//) != ERROR_SUCCESS) {
	//	_tprintf(TEXT("Chave N_avioes nao foi nem criada nem aberta.\n"));
	//	return -1;
	//}

	//if (handleRes == REG_CREATED_NEW_KEY)
	//	_tprintf(TEXT("Chave criada: %s\n"), key_dir);
	//else
	//	_tprintf(TEXT("Chave aberta: %s\n"), key_dir);

	//if (RegQueryValueEx(
	//	handle,
	//	key_name,
	//	0,
	//	NULL,
	//	(LPBYTE)key_value,
	//	&tamanho
	//) != ERROR_SUCCESS) {
	//	_tprintf(TEXT("Numero de Avioes maximo nao esta definido.\n"));
	//	_tprintf(TEXT("Numero de Avioes maximo: "));
	//	_tscanf_s(TEXT("%s"), key_value, TAM);

	//	if (RegSetValueEx(
	//		handle,
	//		key_name,
	//		0,
	//		REG_SZ,
	//		(LPCBYTE)&key_value,
	//		sizeof(TCHAR) * (_tcslen(key_value) + 1) //lê o buffer todo
	//	) != ERROR_SUCCESS) {
	//		_tprintf(TEXT("O atributo nao foi alterado nem criado! ERRO! %s"), key_name);
	//	}

	//}
	//else {
	//	maxAvioes = atoi(key_value);
	//	_tprintf(TEXT("Atributo [%s] foi encontrado! value [%d]\n"), key_name, maxAvioes);
	//	_tprintf(TEXT("Max avioes: %d"), maxAvioes);
	//}






	



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
	ThreadController escrever;
	HANDLE hFileMap;
	HANDLE hEscrita;


 //mapeia ficheiro num bloco de memoria
	hFileMap = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		NUM_CHAR_FILE_MAP * sizeof(TCHAR), // alterar o tamanho do filemapping
		TEXT(FILE_MAP_MESSEGER_TO_PLANES)); //nome do file mapping, tem de ser único

	if (hFileMap == NULL) {
		_tprintf(TEXT("Erro no CreateFileMapping\n"));
		//CloseHandle(hFile); //recebe um handle e fecha esse handle , no entanto o handle é limpo sempre que o processo termina
		return 1;
	}

	//mapeia bloco de memoria para espaço de endereçamento
	escrever.fileViewMap = (TCHAR*)MapViewOfFile(
		hFileMap,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		0);

	if (escrever.fileViewMap == NULL) {
		_tprintf(TEXT("Erro no MapViewOfFile\n"));
		return 1;
	}


	escrever.hEvent = CreateEvent(
		NULL,
		TRUE,
		FALSE,
		TEXT(EVENT_MESSEGER_TO_PLANES));

	if (escrever.hEvent == NULL) {
		_tprintf(TEXT("Erro no CreateEvent\n"));
		UnmapViewOfFile(escrever.fileViewMap);
		return 1;
	}

	escrever.hMutex = CreateMutex(
		NULL,
		FALSE,
		TEXT(MUTEX_MESSEGER_TO_PLANES));

	if (escrever.hMutex == NULL) {
		_tprintf(TEXT("Erro no CreateMutex\n"));
		UnmapViewOfFile(escrever.fileViewMap);
		return 1;
	}
	escrever.terminar = 0;

	hEscrita = CreateThread(NULL, 0, ThreadEscrever, &escrever, 0, NULL);

	if (hEscrita != NULL)
		WaitForSingleObject(hEscrita, INFINITE);

}