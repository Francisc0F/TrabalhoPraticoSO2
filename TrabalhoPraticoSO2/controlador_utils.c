#include <tchar.h>
#include <windows.h>
#include "../utils.h"
#include "controlador_utils.h"



void menuControlador() {
	_putws(TEXT("\naddAero <nome> <cordX> <cordY> - Adicionar aeroporto"));
	_putws(TEXT("lista - lista toda a informação do aeroporto"));
	_putws(TEXT("suspender ou ativar - aceitação de novos aviões por parte dos utilizadores"));
	_putws(TEXT("end - Encerrar sistema, notifica todos os processos"));
}


void adicionarAeroporto(TCHAR* nome, int x, int y, Aeroporto lista[]) {
	pAeroporto aux = NULL;
	for (int i = 0; i < MAXAEROPORTOS; i++) {
		if (_tcscmp(lista[i].nome, L"") == 0) {
			_tcscpy_s(lista[i].nome, _countof(lista[i].nome), nome);
			lista[i].x = x;
			lista[i].y = y;
			break;
		}
	}
}



void printAeroporto(pAeroporto aero) {
		_tprintf(TEXT("Nome: [%s]\n"), aero->nome);
		_tprintf(TEXT("Posicao: (%d, %d)\n"), aero->x, aero->y);
}



void listaTudo(Aeroporto lista[]) {
	for (int i = 0; i < MAXAEROPORTOS; i++) {
		if (_tcscmp(lista[i].nome, L"") != 0) {
			printAeroporto(&lista[i]);
		}
	}
}

void inicializarLista(Aeroporto lista[]) {
	/*for (int i = 0; i < MAXAEROPORTOS; i++) {
		memset(&lista[i], 0, sizeof(lista[i]));
	}*/
}

//
//void pesquisaAeroporto(TCHAR nome, Aviao lista[]) {
//	for (int i = 0; i < MAXAVIOES; i++) {
//		if (lista[i] != NULL) {
//			_tcscpy(lista[i].nome, nome);
//		}
//		else {
//			break;
//		}
//	}
//}
//

void ThreadEnvioDeMsgParaAvioes(ThreadController * escrever, HANDLE *hFileMap, HANDLE *hEscrita) {


	//mapeia ficheiro num bloco de memoria
	hFileMap = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		NUM_CHAR_FILE_MAP * sizeof(TCHAR), // alterar o tamanho do filemapping
		TEXT(FILE_MAP_MSG_TO_PLANES)); //nome do file mapping, tem de ser único

	if (hFileMap == NULL) {
		_tprintf(TEXT("Erro no CreateFileMapping\n"));
		//CloseHandle(hFile); //recebe um handle e fecha esse handle , no entanto o handle é limpo sempre que o processo termina
		return 1;
	}

	//mapeia bloco de memoria para espaço de endereçamento
	escrever->fileViewMap = (TCHAR*)MapViewOfFile(
		hFileMap,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		0);

	if (escrever->fileViewMap == NULL) {
		_tprintf(TEXT("Erro no MapViewOfFile\n"));
		return 1;
	}


	escrever->hEvent = CreateEvent(
		NULL,
		TRUE,
		FALSE,
		TEXT(EVENT_MSG_TO_PLANES));

	if (escrever->hEvent == NULL) {
		_tprintf(TEXT("Erro no CreateEvent\n"));
		UnmapViewOfFile(escrever->fileViewMap);
		return 1;
	}

	escrever->hMutex = CreateMutex(
		NULL,
		FALSE,
		TEXT(MUTEX_MSG_TO_PLANES));

	if (escrever->hMutex == NULL) {
		_tprintf(TEXT("Erro no CreateMutex\n"));
		UnmapViewOfFile(escrever->fileViewMap);
		return 1;
	}
	escrever->terminar = 0;

}


void checkRegEditKeys(TCHAR* key_dir, HKEY handle, DWORD handleRes, TCHAR* key_name, int* maxAvioes){

	// open Or create
	if (RegCreateKeyEx(
		HKEY_CURRENT_USER,
		key_dir,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&handle,
		&handleRes
	) != ERROR_SUCCESS) {
		_tprintf(TEXT("Chave N_avioes nao foi nem criada nem aberta.\n"));
		return -1;
	}

	if (handleRes == REG_CREATED_NEW_KEY)
		_tprintf(TEXT("Chave criada: %s\n"), key_dir);
	else
		_tprintf(TEXT("Chave aberta: %s\n"), key_dir);

	TCHAR key_value[TAM];

	DWORD tamanho = sizeof(key_value);
	if (RegQueryValueEx(
		handle,
		key_name,
		0,
		NULL,
		(LPBYTE)key_value,
		&tamanho
	) != ERROR_SUCCESS) {
		_tprintf(TEXT("Numero de Avioes maximo nao esta definido.\n"));
		_tprintf(TEXT("Numero de Avioes maximo: "));
		_tscanf_s(TEXT("%s"), key_value, TAM);

		if (RegSetValueEx(
			handle,
			key_name,
			0,
			REG_SZ,
			(LPCBYTE)&key_value,
			sizeof(TCHAR) * (_tcslen(key_value) + 1) //lê o buffer todo
		) != ERROR_SUCCESS) {
			_tprintf(TEXT("O atributo nao foi alterado nem criado! ERRO! %s"), key_name);
		}

	}
	else {
		*maxAvioes = _tstoi(key_value);
		_tprintf(TEXT("Atributo [%s] foi encontrado! value [%d]\n"), key_name, *maxAvioes);
		_tprintf(TEXT("Max avioes: %d"), *maxAvioes);
	}

	RegCloseKey(handle);
}
