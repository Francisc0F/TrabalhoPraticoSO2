#include <tchar.h>
#include <windows.h>
#include "../utils.h"
#include "aviao_utils.h"

void menuAviao() {
	_putws(TEXT("\nprox <destino> - Proximo destino"));
	//_putws(TEXT("emb - embarcar passageiros"));
	_putws(TEXT("init - iniciar viagem"));
	_putws(TEXT("quit - terminar instancia de aviao"));
}


void printMSG(MSGcel cel) {
	_tprintf(TEXT("info %s, x %d, y %d, \n"), cel.info, cel.x, cel.y);
}

void proxDestino(int idAeroportoDestino) {
	// todo 
}

void preparaLeituraMSGdoAviao(HANDLE* hFileMap, ControllerToPlane* ler) {
	hFileMap = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		sizeof(MSGCtrlToPlane), // alterar o tamanho do filemapping
		TEXT(FILE_MAP_MSG_TO_PLANES)); //nome do file mapping, tem de ser único

	if (hFileMap == NULL) {
		_tprintf(TEXT("Erro no CreateFileMapping\n"));
		//CloseHandle(hFile);
		return 1;
	}

	//mapeia bloco de memoria para espaço de endereçamento
	ler->fileViewMap = (MSGCtrlToPlane*)MapViewOfFile(
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

void preparaThreadDeGestaoViagens(ThreadGerirViagens * control) {
	control->hEventNovaViagem = CreateEvent(
		NULL,
		TRUE,
		FALSE, NULL);

	if (control->hEventNovaViagem == NULL) {
		_tprintf(TEXT("Erro no CreateEvent hEventEnviarMSG\n"));
		return 1;
	}
	control->terminar = 0;
}

void enviarMensagemParaControlador(MSGThread* escreve, TCHAR* info) {
	_tcscpy_s(escreve->info, _countof(escreve->info), info);
	SetEvent(escreve->hEventEnviarMSG);
	Sleep(100);
	ResetEvent(escreve->hEventEnviarMSG); //bloqueia evento
}


void setupAviao(Aviao* aviao ,ThreadsControlerAviao*  control) {
	_tprintf(TEXT("Indique cap maxima, numero posicoes por segundo -> <NcapMaxima> <NposicoesSegundo>\n"));
	TCHAR info[100];
	_fgetts(info, 100, stdin);
	info[_tcslen(info) - 1] = '\0';

	TCHAR* nextToken;
	TCHAR delim[] = L" ";
	
	TCHAR* token = _tcstok_s(info, delim, &nextToken);
	if (token != NULL) {
		aviao->max_passag = _tstoi(token);
		if (nextToken != NULL) {
			aviao->posPorSegundo = _tstoi(nextToken);
		}
	}
	// envio de info do aeroporto do aviao
	_tprintf(TEXT("Escolha da Lista, o numero do aeroporto onde comecar\n"));
	enviarMensagemParaControlador(control->escrita, TEXT("aero"));
	WaitForSingleObject(control->leitura->hEvent, INFINITE);
	_tprintf(TEXT("controlador: %s\n"), control->leitura->ultimaMsg);

	TCHAR idAero[100]; 
	_fgetts(idAero, 100, stdin);
	idAero[_tcslen(idAero) - 1] = '\0';

	aviao->idAeroporto = _tstoi(idAero);
	// envio de info do aviao  -> adiciona aviao no controlador
	control->escrita->idAeroporto = aviao->idAeroporto;
	control->escrita->capacidadePassageiros = aviao->max_passag;
	control->escrita->posPorSegundo = aviao->posPorSegundo;
	enviarMensagemParaControlador(control->escrita, TEXT("info"));
	
	WaitForSingleObject(control->leitura->hEvent, INFINITE);
	obterCordsDeString(control->leitura->ultimaMsg, &aviao->x, &aviao->y);

	printAviao(aviao, NULL);
}

int abrirMapaPartilhado(HANDLE* hMapaDePosicoesPartilhada) {
	hMapaDePosicoesPartilhada = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, MAPA_PARTILHADO);
	if (hMapaDePosicoesPartilhada == NULL) {
			_tprintf(TEXT("Controlador nao esta disponivel.\n"));
			return 1;
	}
	return 0;
}


int viajarPara(Aviao * aviao) {
	// prox posicao 	 dll
	//TCHAR dll[MAX] = TEXT("C:\\Users\\Francisco\\source\\repos\\TrabalhoPraticoSO2\\SO2_TP_DLL_2021\\x64\\SO2_TP_DLL_2021.dll");
	TCHAR dll[100] = TEXT(DLL);
	HINSTANCE hinstLib = NULL;
	hinstLib = LoadLibrary(dll);

	

	MYPROC ProcAdd = NULL;
	BOOL fFreeResult, fRunTimeLinkSuccess = FALSE;

	if (hinstLib != NULL)
	{
		ProcAdd = (MYPROC)GetProcAddress(hinstLib, "move");
		
		if (NULL != ProcAdd)
		{
			fRunTimeLinkSuccess = TRUE;
			int nextX = aviao->proxDestinoX;
			int nextY = aviao->proxDestinoY;

			int currX = aviao->x;
			int currY = aviao->x;

			aviao->statusViagem = 1;
			while (aviao->statusViagem == 1) {
				//int move(int cur_x, int cur_y, int final_dest_x, int final_dest_y, int * next_x, int* next_y)
				aviao->statusViagem = (ProcAdd)(currX, currY, aviao->proxDestinoX, aviao->proxDestinoY, &nextX, &nextY);
				// status 1 mov correta, 2 erro, 0 chegou 
				_tprintf(TEXT("\nprev pos (%d,%d)  -> (%d,%d) status %d"), currX, currY, nextX, nextY, aviao->statusViagem);
				currX = nextX;
				currY = nextY;
				Sleep(aviao->posPorSegundo * 1000);
			}
			if (aviao->statusViagem == 0) {
				fFreeResult = FreeLibrary(hinstLib);
				_tprintf(TEXT("Free lib"));
				return aviao->statusViagem;
			}
		
		}

	
	}
	fFreeResult = FreeLibrary(hinstLib);
	_tprintf(TEXT("Free lib"));
}

void disparaEventoDeInicioViagem(ThreadGerirViagens * control) {
	SetEvent(control->hEventNovaViagem);
	Sleep(20);
	ResetEvent(control->hEventNovaViagem);
}
