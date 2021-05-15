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
			lista[i].id = i + 1;
			lista[i].x = x;
			lista[i].y = y;
			break;
		}
	}
}



void printAeroporto(pAeroporto aero, TCHAR* out) {
	TCHAR aux[100];
	if (out != NULL) {
		_stprintf_s(aux, 100, L"\nid: [%d] \nNome: [%s]\nPosicao: (%d, %d)\n",aero->id, aero->nome, aero->x, aero->y);
		_tcscat_s(out, 300, aux);
	}
	else {
		_tprintf(TEXT("\nid: [%d]\n"), aero->id);
		_tprintf(TEXT("Nome: [%s]\n"), aero->nome);
		_tprintf(TEXT("Posicao: (%d, %d)\n"), aero->x, aero->y);
	}
}

void adicionarAviao(Aviao *a, Aviao lista[]) {
	// int id, int n_passag, int max_passag, int posPorSegundo, int idAero,
	for (int i = 0; i < MAXAVIOES; i++) {
		if(lista[i].id == 0){
			lista[i].id = a->id;
			lista[i].idAeroporto = a->idAeroporto;
			lista[i].n_passag = a->n_passag;
			lista[i].max_passag = a->max_passag;
			lista[i].posPorSegundo = a->posPorSegundo;
			lista[i].x = a->x;
			lista[i].y = a->x;


			lista[i].proxDestinoId = -1;
			lista[i].proxDestinoX = -1;
			lista[i].proxDestinoY = -1;
			lista[i].statusViagem = -1;
	
			break;
		}
	}
}

int getAeroporto(int id, Aeroporto lista[]) {
	for (int i = 0; i < MAXAEROPORTOS; i++) {
		if (lista[i].id == id) {
			return i;
		}
	}
	return -1;
}





void listaAvioes(Aviao lista[], TCHAR* out) {
	for (int i = 0; i < MAXAVIOES; i++) {
		if (lista[i].id != 0 ) {
			if (out != NULL) {
				printAviao(&lista[i], out);
			}
			else {
				printAviao(&lista[i], NULL);
			}

		}
	}
	//_tprintf(TEXT("out ate agr [%s]\n"), out);
}


void listaTudo(Aeroporto lista[], TCHAR* out) {
	for (int i = 0; i < MAXAEROPORTOS; i++) {
		if (_tcscmp(lista[i].nome, L"") != 0) {
			if (out != NULL) {
				printAeroporto(&lista[i], out);
			}
			else {
				printAeroporto(&lista[i], NULL);
			}

		}
	}
	//_tprintf(TEXT("out ate agr [%s]\n"), out);
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

void ThreadEnvioDeMsgParaAvioes(ControllerToPlane* escrever, HANDLE* hFileMap, HANDLE* hEscrita) {

	//mapeia ficheiro num bloco de memoria
	hFileMap = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		sizeof(MSGCtrlToPlane), // alterar o tamanho do filemapping
		TEXT(FILE_MAP_MSG_TO_PLANES)); //nome do file mapping, tem de ser único

	if (hFileMap == NULL) {
		_tprintf(TEXT("Erro no CreateFileMapping\n"));
		//CloseHandle(hFile); //recebe um handle e fecha esse handle , no entanto o handle é limpo sempre que o processo termina
		return 1;
	}

	//mapeia bloco de memoria para espaço de endereçamento
	escrever->fileViewMap = (MSGCtrlToPlane*)MapViewOfFile(
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

	// evento pra gerir ordem de escrita
	escrever->hEventOrdemDeEscrever = CreateEvent(
		NULL,
		TRUE,
		FALSE, NULL);

	if (escrever->hEventOrdemDeEscrever == NULL) {
		_tprintf(TEXT("Erro no CreateEvent hEventOrdemDeEscrever\n"));
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

void checkRegEditKeys(TCHAR* key_dir, HKEY handle, DWORD handleRes, TCHAR* key_name, int* maxAvioes) {

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
		*maxAvioes = _tstoi(key_value);
	}
	else {
		*maxAvioes = _tstoi(key_value);
		///_tprintf(TEXT("Atributo [%s] foi encontrado! value [%d]\n"), key_name, *maxAvioes);
	}
	_tprintf(TEXT("Max avioes: %d\n"), *maxAvioes);
	RegCloseKey(handle);
}

void setupMapaPartilhado(HANDLE * hMapaDePosicoesPartilhada, HANDLE * mutexAcesso) {
	 *hMapaDePosicoesPartilhada = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, MAPA_PARTILHADO);
	if (*hMapaDePosicoesPartilhada == NULL) {

		*hMapaDePosicoesPartilhada = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			sizeof(MapaPartilhado),
			MAPA_PARTILHADO);

		if (*hMapaDePosicoesPartilhada == NULL) {
			_tprintf(TEXT("Erro no hMapaDePosicoesPartilhada\n"));
			return -1;
		}
	}
	*mutexAcesso = CreateMutex(NULL, FALSE, MUTEX_MAPA_PARTILHADO);

	if (*mutexAcesso == NULL) {
		_tprintf(TEXT("Erro no mutexAcesso hMapaDePosicoesPartilhada\n"));
		return -1;
	}

}

void preparaParaLerInfoDeAvioes(MSGThread* ler, HANDLE* hLerFileMap) {

	//criar semaforo que conta as escritas
	ler->hSemEscrita = CreateSemaphore(NULL, TAM_BUFFER_CIRCULAR, TAM_BUFFER_CIRCULAR, SEMAPHORE_ESCRITA_MSG_TO_CONTROLER);

	//nao ha nada para ler
	ler->hSemLeitura = CreateSemaphore(NULL, 0, TAM_BUFFER_CIRCULAR, SEMAPHORE_LEITURA_MSG_TO_CONTROLER);

	//criar mutex para os consumidores neste caso so ha 1, o Controlador
	ler->hMutex = CreateMutex(NULL, FALSE, MUTEX_CONSUMIDOR_MSG_TO_CONTROLER);

	if (ler->hSemEscrita == NULL || ler->hSemLeitura == NULL || ler->hMutex == NULL) {
		_tprintf(TEXT("Erro no CreateSemaphore hSemLeitura hSemEscrita ou no CreateMutex\n"));
		return -1;
	}

	//o openfilemapping vai abrir um filemapping com o nome que passamos no lpName
	//se devolver um HANDLE ja existe e nao fazemos a inicializacao
	//se devolver NULL nao existe e vamos fazer a inicializacao

	hLerFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, FILE_MAP_MSG_TO_CONTROLER);
	if (hLerFileMap == NULL) {
		hLerFileMap = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			sizeof(BufferCircular), 
			FILE_MAP_MSG_TO_CONTROLER);

		if (hLerFileMap == NULL) {
			_tprintf(TEXT("Erro no CreateFileMapping\n"));
			return -1;
		}
	}

	//mapeamos o bloco de memoria para o espaco de enderaçamento do nosso processo
	ler->bufferPartilhado = (BufferCircular*)MapViewOfFile(hLerFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);


	if (ler->bufferPartilhado == NULL) {
		_tprintf(TEXT("Erro no MapViewOfFile\n"));
		return -1;
	}
	// inicializar buffer circular partilhado 
	ler->bufferPartilhado->nConsumidores = 0;
	ler->bufferPartilhado->nProdutores = 0;
	ler->bufferPartilhado->posE = 0;
	ler->bufferPartilhado->posL = 0;

	ler->terminar = 0;

	////temos de usar o mutex para aumentar o nConsumidores para termos os ids corretos
	//WaitForSingleObject(ler->hMutex, INFINITE);
	//ler->bufferPartilhado->nConsumidores++;
	//ReleaseMutex(ler->bufferPartilhado);

}

void enviarMensagemBroadCast(ControllerToPlane* escreve, TCHAR* info) {
	_tcscpy_s(escreve->msgToSend.info, _countof(escreve->msgToSend.info), info);
	escreve->msgToSend.idAviao = -1;
	SetEvent(escreve->hEventOrdemDeEscrever);
	Sleep(100);
	ResetEvent(escreve->hEventOrdemDeEscrever); //bloqueia evento
}

void enviarMensagemParaAviao(int id, ControllerToPlane* escreve, TCHAR* info) {
	_tcscpy_s(escreve->msgToSend.info, _countof(escreve->msgToSend.info), info);
	escreve->msgToSend.idAviao = id;
	SetEvent(escreve->hEventOrdemDeEscrever);
	Sleep(100);
	ResetEvent(escreve->hEventOrdemDeEscrever); //bloqueia evento
}
