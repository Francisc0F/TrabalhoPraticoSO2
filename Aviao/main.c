#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include "../utils.h"
#include "aviao_utils.h"

#define MAX 1000



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
		if (dados->fileViewMap->idAviao == escreve->id) {
			// guarda mensagem em variavel local
			_tcscpy_s(dados->ultimaMsg, _countof(dados->ultimaMsg), dados->fileViewMap->info);
			//_tprintf(TEXT("MSG: id: %d \nmsg: %s\n"), dados->fileViewMap->idAviao, dados->fileViewMap->info);
		}
		else if (dados->fileViewMap->idAviao == -1) {
			_tprintf(TEXT("Notificação Geral: %s\n"), dados->fileViewMap->info);
		}
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
		cel.aviao.max_passag = dados->capacidadePassageiros;
		cel.aviao.posPorSegundo = dados->posPorSegundo;
		cel.aviao.idAeroporto = dados->idAeroporto;
		_tcscpy_s(cel.info, _countof(cel.info), dados->info);

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

		//_tprintf(TEXT("Aviao %d enviou \n"), dados->id);
		//printMSG(cel);
		Sleep(1000);
	}

	return 0;
}


DWORD WINAPI gestoraDeViagens(LPVOID param) {
	ThreadGerirViagens* dados = (ThreadGerirViagens*)param;

	while (!dados->terminar) {
		WaitForSingleObject(dados->hEventNovaViagem, INFINITE);
		int x = viajarPara(dados->aviao);
		/*if (x == 0) {
			enviarMensagemParaControlador(&escreve, TEXT("CHEGOU AO NOVO DESTINO"));
		}*/
	}

	return 0;
}

#pragma endregion declaracao threads para fluxo de mensagens  aviao -> controlador , controlador -> aviao

int _tmain(int argc, LPTSTR argv[]) {

	HANDLE hMapaDePosicoesPartilhada;
	int controladorDisponivel = abrirMapaPartilhado(&hMapaDePosicoesPartilhada);
	if (controladorDisponivel == 1) {
		_tprintf(TEXT("Aviao terminou.\n"));
		TCHAR cmd[23];
		_fgetts(cmd, 23, stdin);
		return -1;
	}
	Aviao* mapaAvioesPartilhado = (Aviao*)MapViewOfFile(hMapaDePosicoesPartilhada, FILE_MAP_ALL_ACCESS, 0, 0, 0);

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

	
	ThreadsControlerAviao controler;

	// thread para enviar mensagens pro controlador -> buffer circular
	HANDLE hFileEscritaMap;
	MSGThread escreve;
	BOOL primeiroProcesso = FALSE;
	preparaEnvioDeMensagensParaOControlador(&hFileEscritaMap, &escreve, &primeiroProcesso);
	HANDLE hThreadWriter = CreateThread(NULL, 0, ThreadWriter, &escreve, 0, NULL);

	// thread para receber mensagens do controlador -> memoria partilhada acesso direto
	ControllerToPlane ler;
	HANDLE hFileLeituraMap;
	controler.escrita = &escreve;
	controler.leitura = &ler;
	preparaLeituraMSGdoAviao(&hFileLeituraMap, &ler);
	HANDLE hThreadReader = CreateThread(NULL, 0, ThreadReader, &controler, 0, NULL);

	
	


	
	Aviao aviao;
	aviao.proxDestinoId = -1;
	aviao.x = -1;
	aviao.y = -1;
	aviao.proxDestinoX = -1;
	aviao.proxDestinoY = -1;
	aviao.n_passag = -1;

	ThreadGerirViagens ThreadViagens;
	preparaThreadDeGestaoViagens(&ThreadViagens);
	ThreadViagens.aviao = &aviao;
	HANDLE hThreadViagens = CreateThread(NULL, 0, gestoraDeViagens, &ThreadViagens, 0, NULL);

	//obter dados inicias
	setupAviao(&aviao, &controler);
#pragma endregion inicializar variaveis de controlo de threads e inicio de threads leitura e escrita

#pragma region menu aviao

	while (1) {
		menuAviao();
		TCHAR tokenstring[100];
		_fgetts(tokenstring, 100, stdin);
		tokenstring[_tcslen(tokenstring) - 1] = '\0';
		TCHAR* nextToken;
		TCHAR delim[] = L" ";
		TCHAR* token = _tcstok_s(tokenstring, delim, &nextToken);
		while (token != NULL)
		{
			//_tprintf(L"%ls\n", token);
			if (_tcscmp(token, L"prox") == 0) {
				if (nextToken != NULL) {
					aviao.proxDestinoId = _tstoi(nextToken);
					TCHAR msg[100] = TEXT("prox ");
					_tcscat_s(msg, 100, nextToken);

					enviarMensagemParaControlador(&escreve, msg);
					
					WaitForSingleObject(ler.hEvent, INFINITE);
					obterCordsDeString(ler.ultimaMsg, &aviao.proxDestinoX, &aviao.proxDestinoY);
					_tprintf(TEXT("controlador: %s\n"), ler.ultimaMsg);

					_tprintf(TEXT("X:%d  Y:%d\n"), aviao.proxDestinoX, aviao.proxDestinoY);
					if (_tcscmp(ler.ultimaMsg, L"erro") > 0) {
						_tprintf(TEXT("Próximo destino definido.\n"));
					}
				}
			}
			else if (_tcscmp(token, TEXT("emb")) == 0) {
				// todo
				_tprintf(TEXT("Embarcar passageiros.\n"));
			}
			else if (_tcscmp(token, L"init") == 0) {
				_tprintf(TEXT("A traçar rota.\n"));
				TCHAR info[100] = TEXT("init ");
				//_tcscat_s(info, 300, _tstoi(proxDestino));
				disparaEventoDeInicioViagem(&ThreadViagens);
				
				//_tprintf(TEXT("Iniciou viagem.\n"));
			}
			else if (_tcscmp(token, L"quit") == 0) {
				// todo
				_tprintf(TEXT("Sair de instância de avião.\n"));
			}
			token = wcstok_s(NULL, delim, &nextToken);
		}
	}

#pragma endregion 




	if (hThreadReader != NULL)
		WaitForSingleObject(hThreadReader, INFINITE);

	if (hThreadWriter != NULL) {
		WaitForSingleObject(hThreadWriter, INFINITE);
	}


}