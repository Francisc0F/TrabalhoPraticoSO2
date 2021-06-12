#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include "../utils.h"
#include "aviao_utils.h"

#pragma region aviao threads

// ouvir mensagens do controller
DWORD WINAPI ThreadReader(LPVOID param) {
	ThreadsControlerAviao* threadControl = (ThreadsControlerAviao*)param;
	ControllerToPlane* dados = threadControl->leitura;
	MSGThread* escreve = threadControl->escrita;

	while (1) {
		//esperar até que evento desbloqueie , evento que avisa todos os subscritores para ler
		WaitForSingleObject(dados->hEvent, INFINITE);
		if (dados->terminar)
			break;

		WaitForSingleObject(dados->hMutex, INFINITE);
		// escreve->id é unico e seguro de usar pois passa num mutex ao arrancar do programa 
		if (dados->fileViewMap->idAviao == escreve->idAviao) {
			// guarda mensagem em variavel local
			_tcscpy_s(dados->ultimaMsg, _countof(dados->ultimaMsg), dados->fileViewMap->info);
			//_tprintf(TEXT("MSG: id: %d \nmsg: %s\n"), dados->fileViewMap->idAviao, dados->fileViewMap->info);
			//_tprintf(TEXT("MSG\n"));
		}
		else if (dados->fileViewMap->idAviao == -1) {
			_tprintf(TEXT("Notificação Geral: %s\n"), dados->fileViewMap->info);
			if ((_tcscmp(dados->fileViewMap->info, L"terminar") == 0)) {
				_tprintf(TEXT("Aviao Terminou.\n"));
				TCHAR cmd[23];
				_fgetts(cmd, 23, stdin);
				exit(0);
			}
		
		}
		ReleaseMutex(dados->hMutex);

		Sleep(200);
	}

	return 0;
}

// escrever mensagens para o controller
DWORD WINAPI ThreadWriter(LPVOID param) {
	MSGThread* dados = (MSGThread*)param;
	MSGcel cel;
	int contador = 0;

	while (!dados->terminar) {
		WaitForSingleObject(dados->hEventEnviarMSG, INFINITE);

		cel.id = dados->idAviao;
		cel.x = dados->x;
		cel.y = dados->y;
		cel.aviao.max_passag = dados->capacidadePassageiros;
		cel.aviao.posPorSegundo = dados->posPorSegundo;
		cel.aviao.idAeroporto = dados->idAeroporto;
		_tcscpy_s(cel.info, _countof(cel.info), dados->info);

		//esperar posicao escrita
		WaitForSingleObject(dados->hSemEscrita, INFINITE);

		WaitForSingleObject(dados->hMutex, INFINITE);
		CopyMemory(&dados->bufferPartilhado->buffer[dados->bufferPartilhado->posE], &cel, sizeof(MSGcel));
		dados->bufferPartilhado->posE++; 
		if (dados->bufferPartilhado->posE == TAM_BUFFER_CIRCULAR)
			dados->bufferPartilhado->posE = 0;
		ReleaseMutex(dados->hMutex);

		ReleaseSemaphore(dados->hSemLeitura, 1, NULL);
		Sleep(100);
	}

	return 0;
}

DWORD WINAPI GestoraDeViagens(LPVOID param) {
	ThreadGerirViagens* dados = (ThreadGerirViagens*)param;
	
	while (!dados->terminar) {
		WaitForSingleObject(dados->hEventNovaViagem, INFINITE);
		int x = viajar(dados);
		if (x == 0) {
			enviarMensagemParaControlador(dados->escrita, TEXT("CHEGOU AO NOVO DESTINO"));
		}
	}
	return 0;
}

DWORD WINAPI EstouAquiPing(LPVOID param) {
	ThreadPingControler* threadControl = (ThreadPingControler*)param;
	MapaPartilhado* partilhado = threadControl->MapaPartilhado;
	Aviao* aviaoLocal = threadControl->AviaoLocal;

	int contador = 0;
	while (!threadControl->terminar) {

		if (WaitForSingleObject(threadControl->hTimer, INFINITE) != WAIT_OBJECT_0) {
			_tprintf(L"WaitForSingleObject failed (%d)\n", GetLastError());
			break;
		}
		else {
			WaitForSingleObject(threadControl->hMutexAcessoAMapaPartilhado, INFINITE);
			int index = getAviao(aviaoLocal->id, partilhado->avioesMapa);
			Aviao* aux = &partilhado->avioesMapa[index];
			aux->segundosVivo += 3;
			ReleaseMutex(threadControl->hMutexAcessoAMapaPartilhado);
		}
		
	}

	return 0;
}
#pragma endregion declaracao threads para fluxo de mensagens  aviao -> controlador , controlador -> aviao




int _tmain(int argc, LPTSTR argv[]) {

	HANDLE hMapaDePosicoesPartilhada;
	HANDLE hMutexMapaDePosicoesPartilhada;
	int controladorDisponivel = abrirMapaPartilhado(&hMapaDePosicoesPartilhada, &hMutexMapaDePosicoesPartilhada);
	if (controladorDisponivel == 1) {
		_tprintf(TEXT("Aviao terminou.\n"));
		TCHAR cmd[23];
		_fgetts(cmd, 23, stdin);
		return -1;
	}
	MapaPartilhado* mapaAvioesPartilhado = (MapaPartilhado*)MapViewOfFile(hMapaDePosicoesPartilhada, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	

	HANDLE hSem = CreateSemaphore(NULL, 1, 1, SEMAPHORE_NUM_AVIOES);
	if (hSem == NULL) {
		_tprintf(TEXT("Erro no CreateSemaphore controloDeNumeroDeAvioes\n"));
		return -1;
	}

	_tprintf(TEXT("Aguarda ordem de entrada do controlador...\n"));
	WaitForSingleObject(hSem, INFINITE);

	//UNICODE: Por defeito, a consola Windows não processa caracteres wide. 
	//A maneira mais fácil para ter esta funcionalidade é chamar _setmode:
#ifdef UNICODE 
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif
	// TODO  fazer validacao com open no mutex do controlador para saber se o controlador esta vivo

#pragma region init

	HANDLE hThreads[4];
	ThreadsControlerAviao controler;

	// thread para enviar mensagens pro controlador -> buffer circular
	HANDLE hFileEscritaMap;
	MSGThread escreve;
	BOOL primeiroProcesso = FALSE;
	preparaEnvioDeMensagensParaOControlador(&hFileEscritaMap, &escreve, &primeiroProcesso);
	hThreads[0] = CreateThread(NULL, 0, ThreadWriter, &escreve, 0, NULL);

	// thread para receber mensagens do controlador -> memoria partilhada acesso direto
	ControllerToPlane ler;
	HANDLE hFileLeituraMap;
	controler.escrita = &escreve;
	controler.leitura = &ler;
	preparaLeituraMSGdoAviao(&hFileLeituraMap, &ler);
	hThreads[1] = CreateThread(NULL, 0, ThreadReader, &controler, 0, NULL);

	Aviao aviao;
	aviao.id = escreve.idAviao;
	aviao.proxDestinoId = -1;
	aviao.x = -1;
	aviao.y = -1;
	aviao.proxDestinoX = -1;
	aviao.proxDestinoY = -1;
	aviao.n_passag = -1;

	ThreadGerirViagens ThreadViagens;
	preparaThreadDeGestaoViagens(&ThreadViagens);

	ThreadViagens.escrita = &escreve;
	ThreadViagens.hMutexAcessoAMapaPartilhado = hMutexMapaDePosicoesPartilhada;
	ThreadViagens.aviaoMemLocal = &aviao;
	ThreadViagens.MapaPartilhado = mapaAvioesPartilhado;
	hThreads[2] = CreateThread(NULL, 0, GestoraDeViagens, &ThreadViagens, 0, NULL);

	aviao.processId = GetCurrentProcessId();
	//obter dados inicias
	setupAviao(&aviao, &controler);

	ThreadPingControler ThreadPing;
	ThreadPing.hTimer = OpenWaitableTimer(TIMER_ALL_ACCESS , NULL, PINGTIMER);
	if (ThreadPing.hTimer == NULL) {
		_tprintf(L"OpenWaitableTimer failed (%d)\n", GetLastError());
		TCHAR cmd[23];
		_fgetts(cmd, 23, stdin);
		return -2;
	}
	ThreadPing.AviaoLocal = &aviao;
	ThreadPing.MapaPartilhado = mapaAvioesPartilhado;
	ThreadPing.terminar = 0;
	hThreads[3] = CreateThread(NULL, 0, EstouAquiPing, &ThreadPing, 0,NULL);
#pragma endregion inicializar variaveis de controlo de threads e inicio de threads leitura e escrita

#pragma region menu aviao
	interacaoComConsolaAviao(&aviao, &ler, &escreve, &ThreadViagens);
#pragma endregion 

	if (hThreads[0] != NULL && hThreads[1] != NULL && hThreads[2] != NULL && hThreads[3] != NULL)
		WaitForMultipleObjects(4, hThreads, TRUE, INFINITE);
}