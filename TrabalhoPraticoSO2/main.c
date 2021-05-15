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


#pragma region threads declaration

#pragma region thread de controlo de escrita de msg para todos os avioes
DWORD WINAPI ThreadEscrever(LPVOID param) {
	ThreadsControlerControlador* threadControl = (ThreadsControlerControlador*)param;
	ControllerToPlane* dados = threadControl->escrita;

	while (!(dados->terminar)) {
		// espera evento ordem de enviar mensagem
		WaitForSingleObject(dados->hEventOrdemDeEscrever, INFINITE);

		WaitForSingleObject(dados->hMutex, INFINITE);
		ZeroMemory(dados->fileViewMap, sizeof(MSGCtrlToPlane));
		CopyMemory(dados->fileViewMap, &dados->msgToSend, sizeof(MSGCtrlToPlane));
		ReleaseMutex(dados->hMutex);
		
		
		//evento ordem para "ir ler" mensagem, para todos os avioes
		SetEvent(dados->hEvent);// desbloqueia evento   
		Sleep(100);
		ResetEvent(dados->hEvent); //bloqueia evento
	}
	return 0;
}
#pragma endregion 
	
#pragma region buffer circular leitura 
DWORD WINAPI ThreadLerBufferCircular(LPVOID param) {
	ThreadsControlerControlador* threadControl = (ThreadsControlerControlador*)param;
	MSGThread* dados = threadControl->leitura;
	BufferCircular* bufferPartilhado = dados->bufferPartilhado;
	MSGcel celLocal;
	int soma = 0;
	TCHAR* garbage = NULL;
	while (!dados->terminar) {

		WaitForSingleObject(dados->hSemLeitura, INFINITE);

		WaitForSingleObject(dados->hMutex, INFINITE);

		CopyMemory(&celLocal, &bufferPartilhado->buffer[bufferPartilhado->posL], sizeof(MSGcel));
		bufferPartilhado->posL++;
		if (bufferPartilhado->posL == TAM_BUFFER_CIRCULAR)
			bufferPartilhado->posL = 0;

		ReleaseMutex(dados->hMutex);

		ReleaseSemaphore(dados->hSemEscrita, 1, NULL);

		_tprintf(TEXT("Aviao: %d msg: %s\n"), celLocal.id, celLocal.info);

		if (_tcscmp(celLocal.info, TEXT("info") ) == 0) {
			Aeroporto* aux = &threadControl->listaAeroportos[celLocal.aviao.idAeroporto - 1];
			celLocal.aviao.id = celLocal.id;
			celLocal.aviao.x = aux->x;
			celLocal.aviao.y = aux->y;

			WaitForSingleObject(*threadControl->hMutexAcessoMapa, INFINITE);
			adicionarAviao(&celLocal.aviao, threadControl->avioes);
			_tprintf(TEXT("adicionou aviao\n"));
			(*threadControl->numAtualAvioes)++;
			ReleaseMutex(*threadControl->hMutexAcessoMapa);
			
			if ((*threadControl->numAtualAvioes) == 1) {
				//iniciar timer de aviso periodico de avioes
				LARGE_INTEGER liDueTime;
				liDueTime.QuadPart = -30000000LL; // comeca daqui a 3 
				if (!SetWaitableTimer(threadControl->hTimerPing, 
					&liDueTime, 
					3000,  // fica periodic a cada 3 secs 
					NULL, 
					NULL, 
					0))
				{
					_tprintf(L"SetWaitableTimer failed (%d)\n", GetLastError());
				}
			}
			else if((*threadControl->numAtualAvioes) == 0){
				// todo cancel waitabletimer
			}
		


			TCHAR send[100] = { 0 };
			preparaStringdeCords(send, aux->x, aux->y);
			enviarMensagemParaAviao(celLocal.id, threadControl->escrita, send);

		}if (_tcscmp(celLocal.info, TEXT("aero")) == 0) {
			_tprintf(TEXT("Enviou %s.\n"), celLocal.info);
			TCHAR info[400]= TEXT("");
			listaAeroportos(threadControl->listaAeroportos, info);
			enviarMensagemParaAviao(celLocal.id, threadControl->escrita, info);
		}
		else {
			TCHAR* nextToken;
			TCHAR* delim = L" ";
			TCHAR* token = _tcstok_s(celLocal.info, delim, &nextToken);
			if (_tcscmp(token, TEXT("prox")) == 0) {
				if (nextToken != NULL) {
					int proxDestino = _tcstol(nextToken, &garbage, 0);
					int index = getAeroporto(proxDestino, threadControl->listaAeroportos);
					if (index >= 0) {
						_tprintf(TEXT("Aviao %d vai partir para %s.\n"), celLocal.id, threadControl->listaAeroportos[index].nome);
						
						TCHAR send[100] = { 0 };
						preparaStringdeCords(send, threadControl->listaAeroportos[index].x, threadControl->listaAeroportos[index].y);
						enviarMensagemParaAviao(celLocal.id, threadControl->escrita, send);
					}
					else {
						enviarMensagemParaAviao(celLocal.id, threadControl->escrita, TEXT("erro"));
					}
				}
				else {
					enviarMensagemParaAviao(celLocal.id, threadControl->escrita, TEXT("erro"));
				}
			}
			else if (_tcscmp(token, TEXT("idAero")) == 0) {
				int index = getAeroporto(_tstoi(nextToken), threadControl->listaAeroportos);
				TCHAR send[100] = { 0 };
				if (index >= 0) {
					 _tcscpy_s(send, _countof(send), L"sucesso");
					enviarMensagemParaAviao(celLocal.id, threadControl->escrita, send);
				}
				else {
					_tcscpy_s(send, _countof(send), L"erro");
					enviarMensagemParaAviao(celLocal.id, threadControl->escrita, send);
				}
			}
		}
		
	}
	
	return 0;
}
#pragma endregion


DWORD WINAPI ThreadGestorDeMapa(LPVOID param) {
	ThreadGestaoDeMapa* threadControl = (ThreadGestaoDeMapa*)param;

	while (!threadControl->terminar) {
		if (WaitForSingleObject(threadControl->hTimer, INFINITE) != WAIT_OBJECT_0) {
			_tprintf(L"WaitForSingleObject failed (%d)\n", GetLastError());
			break;
		}
		else {
			Sleep(100); // espera atualizacao dos avioes
			WaitForSingleObject(threadControl->hMutexAcessoMapa, INFINITE);
			if (threadControl->MapaPartilhadoLocal.numAtualAvioes == -1) {
				CopyMemory(&threadControl->MapaPartilhadoLocal, threadControl->MapaPartilhado, sizeof(MapaPartilhado));
			}
			else {
				Aviao * listaPartilhada = threadControl->MapaPartilhado->avioesMapa;
				for (int i = 0; i < MAXAVIOES; i++) {
					Aviao* local = &threadControl->MapaPartilhadoLocal.avioesMapa[i];
					int index = getAviao(local->id, listaPartilhada);
					if (index > -1) {
						if (listaPartilhada[index].segundosVivo == local->segundosVivo && local->id > 0) {
							_tprintf(L"Aviao %d Desapareceu no radar.", local->id);
							removerEm(index, listaPartilhada);
							// pode entrar mais um 
							ReleaseSemaphore(threadControl->hControloDeNumeroDeAvioes, 1, NULL);
						}
					}
					else {
						_tprintf(L"Erro nao encontrou aviao na mem partilhada 173");
					}
					
				}
				CopyMemory(&threadControl->MapaPartilhadoLocal, threadControl->MapaPartilhado, sizeof(MapaPartilhado));
				
				//Aviao* aux = &partilhado->avioesMapa[index];
			}
			ReleaseMutex(threadControl->hMutexAcessoMapa);
		}
	
	}

	return 0;
}




#pragma endregion 

int _tmain(int argc, TCHAR* argv[]) {

	//UNICODE: Por defeito, a consola Windows não processa caracteres wide. 
	//A maneira mais fácil para ter esta funcionalidade é chamar _setmode:
#ifdef UNICODE 
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif

#pragma region regedit keys setup
	TCHAR key_dir[TAM] = TEXT("Software\\TRABALHOSO2\\");
	HKEY handle = NULL; // handle para chave depois de aberta ou criada
	DWORD handleRes = NULL;
	TCHAR key_name[TAM] = TEXT("N_avioes"); //nome do par-valor
	int maxAvioes = 0;
	checkRegEditKeys(key_dir, handle, handleRes, TEXT("N_avioes"), &maxAvioes);
#pragma endregion 

	Aeroporto aeroportos[MAXAEROPORTOS] = { 0 };
	//Aviao avioes[MAXAVIOES] = { 0 };

#pragma region lista de posicoes em mapa partilhado
	HANDLE hMapaDePosicoesPartilhada = NULL;
	HANDLE hMutexAcessoMapa = NULL;
	int x = setupMapaPartilhado(&hMapaDePosicoesPartilhada, &hMutexAcessoMapa);
	if (x != 0) {
		_tprintf(TEXT("Controlador vai Terminar.\n"));
		return -1;
	}
	//mapa de avioes
	MapaPartilhado* mapaPartilhadoAvioes = (MapaPartilhado*)MapViewOfFile(hMapaDePosicoesPartilhada, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	WaitForSingleObject(hMutexAcessoMapa, INFINITE);
	mapaPartilhadoAvioes->numAtualAvioes = 0;
	ReleaseMutex(hMutexAcessoMapa);
#pragma endregion

	ThreadsControlerControlador controler;
	controler.listaAeroportos = aeroportos;
	controler.avioes = mapaPartilhadoAvioes->avioesMapa;
	controler.numAtualAvioes = &mapaPartilhadoAvioes->numAtualAvioes;
	controler.hMutexAcessoMapa = &hMutexAcessoMapa;
	
	controler.hTimerPing = CreateWaitableTimer(NULL, FALSE, PINGTIMER);
	if (NULL == controler.hTimerPing) {
		_tprintf(L"CreateWaitableTimer failed (%d)\n", GetLastError());
		return 1;
	}

#pragma region semaphore de controlo instancias de avioes
	HANDLE controloDeNumeroDeAvioes = CreateSemaphore(NULL, maxAvioes, maxAvioes, SEMAPHORE_NUM_AVIOES);
	if (controloDeNumeroDeAvioes == NULL) {
		_tprintf(TEXT("Erro no CreateSemaphore controloDeNumeroDeAvioes\n"));
		return -1;
	}
#pragma endregion 

	HANDLE hThreads[2];

	// ouvir mensagens dos avioes
	MSGThread ler;
	HANDLE hLerFileMap;
	HANDLE hThreadLeitura;
	controler.leitura = &ler;
	preparaParaLerInfoDeAvioes(controler.leitura, &hLerFileMap);
	hThreads[0] = CreateThread(NULL, 0, ThreadLerBufferCircular, &controler, 0, NULL);

	// escrever para os avioes
	ControllerToPlane escrever;
	HANDLE hFileMap;
	HANDLE hEscrita;
	controler.escrita = &escrever;
	ThreadEnvioDeMsgParaAvioes(controler.escrita, &hFileMap, &hEscrita);
	hThreads[1] = CreateThread(NULL, 0, ThreadEscrever, &controler, 0, NULL);

	// gestor de mapa
	ThreadGestaoDeMapa gestor;
	gestor.hTimer = controler.hTimerPing;
	gestor.MapaPartilhado = mapaPartilhadoAvioes;
	gestor.MapaPartilhadoLocal.numAtualAvioes = -1;
	gestor.hMutexAcessoMapa = &hMutexAcessoMapa;
	gestor.hControloDeNumeroDeAvioes = controloDeNumeroDeAvioes;
	gestor.terminar = 0;
	hThreads[2] = CreateThread(NULL, 0, ThreadGestorDeMapa, &gestor, 0, NULL);

	
	// setup aeroportos inicias
	adicionarAeroporto(TEXT("Lisbon"), 2, 2, aeroportos);
	adicionarAeroporto(TEXT("Madrid"), 10, 10, aeroportos);
	adicionarAeroporto(TEXT("Paris"), 20, 10, aeroportos);
	adicionarAeroporto(TEXT("Moscovo, Russia"), 30, 18, aeroportos);

	interacaoConsolaControlador(aeroportos, mapaPartilhadoAvioes);

	if (hThreads[0] != NULL && hThreads[1] != NULL && hThreads[2] != NULL)
		WaitForMultipleObjects(3, hThreads, TRUE, INFINITE);
}
