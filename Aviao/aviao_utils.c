#include <tchar.h>
#include <windows.h>
#include "../utils.h"
#include "aviao_utils.h"

void menuAviao() {
	_putws(TEXT("\nprox <destino> - Proximo destino"));
	_putws(TEXT("emb - embarcar passageiros"));
	_putws(TEXT("init - iniciar viagem"));
	_putws(TEXT("quit - terminar instancia de aviao"));
}


void printMSG(MSGcel cel) {
	_tprintf(TEXT("info %s, x %d, y %d, \n"), cel.info, cel.x, cel.y);
}

void proxDestino(int idAeroportoDestino) {
	// todo 
}

void preparaLeituraMSGdoAviao(HANDLE* hFileMap, ThreadControllerToPlane* ler) {
	hFileMap = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		NUM_CHAR_FILE_MAP * sizeof(TCHAR),
		TEXT(FILE_MAP_MSG_TO_PLANES));

	if (hFileMap == NULL) {
		_tprintf(TEXT("Erro no CreateFileMapping\n"));
		//CloseHandle(hFile);
		return 1;
	}

	//mapeia bloco de memoria para espaço de endereçamento
	ler->fileViewMap = (TCHAR*)MapViewOfFile(
		hFileMap,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		0);

	if (ler->fileViewMap == NULL) {
		_tprintf(TEXT("Erro no MapViewOfFile\n"));
		return 1;
	}

	ler->hEvent = CreateEvent(
		NULL,
		TRUE,
		FALSE,
		TEXT(EVENT_MSG_TO_PLANES));

	if (ler->hEvent == NULL) {
		_tprintf(TEXT("Erro no CreateEvent\n"));
		UnmapViewOfFile(ler->fileViewMap);
		return 1;
	}

	ler->hMutex = CreateMutex(
		NULL,
		FALSE,
		TEXT(MUTEX_MSG_TO_PLANES));

	if (ler->hMutex == NULL) {
		_tprintf(TEXT("Erro no CreateMutex\n"));
		UnmapViewOfFile(ler->fileViewMap);
		return 1;
	}
	ler->terminar = 0;

}

void preparaEnvioDeMensagensParaOControlador(HANDLE* hFileEscritaMap, MSGThread* escreve, BOOL* primeiroProcesso) {
	//criar semaforo que conta as escritas
	escreve->hSemEscrita = CreateSemaphore(NULL, TAM_BUFFER, TAM_BUFFER, SEMAPHORE_ESCRITA_MSG_TO_CONTROLER);

	//criar semaforo que conta as leituras
	//0 porque nao ha nada para ser lido e depois podemos ir até um maximo de 10 posicoes para serem lidas
	escreve->hSemLeitura = CreateSemaphore(NULL, 0, TAM_BUFFER, SEMAPHORE_LEITURA_MSG_TO_CONTROLER);

	//criar mutex para os produtores
	escreve->hMutex = CreateMutex(NULL, FALSE, MUTEX_PRODUTOR_MSG_TO_CONTROLER);

	if (escreve->hSemEscrita == NULL || escreve->hSemLeitura == NULL || escreve->hMutex == NULL) {
		_tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex\n"));
		return -1;
	}

	escreve->hEventEnviarMSG = CreateEvent(
		NULL,
		TRUE,
		FALSE, NULL);

	if (escreve->hEventEnviarMSG == NULL) {
		_tprintf(TEXT("Erro no CreateEvent hEventEnviarMSG\n"));
		return 1;
	}


	//o openfilemapping vai abrir um filemapping com o nome que passamos no lpName
	//se devolver um HANDLE ja existe e nao fazemos a inicializacao
	//se devolver NULL nao existe e vamos fazer a inicializacao

	hFileEscritaMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, FILE_MAP_MSG_TO_CONTROLER);
	if (hFileEscritaMap == NULL) {
		primeiroProcesso = TRUE;
		//criamos o bloco de memoria partilhada
		hFileEscritaMap = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			sizeof(BufferCircular), //tamanho da memoria partilhada
			FILE_MAP_MSG_TO_CONTROLER);//nome do filemapping. nome que vai ser usado para partilha entre processos

		if (hFileEscritaMap == NULL) {
			_tprintf(TEXT("Erro no CreateFileMapping\n"));
			return -1;
		}
	}

	//mapeamos o bloco de memoria para o espaco de enderaçamento do nosso processo
	escreve->memPar = (BufferCircular*)MapViewOfFile(hFileEscritaMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);


	if (escreve->memPar == NULL) {
		_tprintf(TEXT("Erro no MapViewOfFile\n"));
		return -1;
	}

	if (primeiroProcesso == TRUE) {
		escreve->memPar->nConsumidores = 0;
		escreve->memPar->nProdutores = 0;
		escreve->memPar->posE = 0;
		escreve->memPar->posL = 0;
	}

	escreve->terminar = 0;
	//temos de usar o mutex para aumentar o nProdutores para termos os ids corretos
	WaitForSingleObject(escreve->hMutex, INFINITE);
	escreve->memPar->nProdutores++;
	escreve->id = escreve->memPar->nProdutores;
	ReleaseMutex(escreve->hMutex);

}


void enviarMensagemParaControlador(MSGThread* escreve, TCHAR* info) {
	_tcscpy_s(escreve->info, _countof(escreve->info), info);
	SetEvent(escreve->hEventEnviarMSG);
	Sleep(100);
	ResetEvent(escreve->hEventEnviarMSG); //bloqueia evento
}

void setupAviao(int* capacidadePassageiros, int* posPorSegundo) {
	_tprintf(TEXT("Indique cap maxima, numero posicoes por segundo -> <NcapMaxima> <NposicoesSegundo>\n"));
	TCHAR info[100];
	_fgetts(info, 100, stdin);
	info[_tcslen(info) - 1] = '\0';

	TCHAR* ptr;
	TCHAR delim[] = L" ";
	TCHAR* token = wcstok_s(info, delim, &ptr);
	if (token != NULL) {
		*capacidadePassageiros = _tstoi(token);
		token = wcstok_s(info, delim, &ptr);
		if (token != NULL) {
			*posPorSegundo = _tstoi(token);
		}
	}
}