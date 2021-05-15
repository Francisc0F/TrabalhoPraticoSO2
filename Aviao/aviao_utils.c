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

void preparaLeituraMSGdoAviao(HANDLE* hFileMap, ControllerToPlane* ler) {
	*hFileMap = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		sizeof(MSGCtrlToPlane), // alterar o tamanho do filemapping
		TEXT(FILE_MAP_MSG_TO_PLANES)); //nome do file mapping, tem de ser �nico

	if (*hFileMap == NULL) {
		_tprintf(TEXT("Erro no CreateFileMapping\n"));
		//CloseHandle(hFile);
		return 1;
	}

	//mapeia bloco de memoria para espa�o de endere�amento
	ler->fileViewMap = (MSGCtrlToPlane*)MapViewOfFile(
		*hFileMap,
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
	escreve->hSemEscrita = CreateSemaphore(NULL, TAM_BUFFER_CIRCULAR, TAM_BUFFER_CIRCULAR, SEMAPHORE_ESCRITA_MSG_TO_CONTROLER);

	escreve->hSemLeitura = CreateSemaphore(NULL, 0, TAM_BUFFER_CIRCULAR, SEMAPHORE_LEITURA_MSG_TO_CONTROLER);

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


	*hFileEscritaMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, FILE_MAP_MSG_TO_CONTROLER);
	if (*hFileEscritaMap == NULL) {
		primeiroProcesso = TRUE;
		//criamos o bloco de memoria partilhada
		*hFileEscritaMap = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			sizeof(BufferCircular), 
			FILE_MAP_MSG_TO_CONTROLER);

		if (*hFileEscritaMap == NULL) {
			_tprintf(TEXT("Erro no CreateFileMapping\n"));
			return -1;
		}
	}

	escreve->bufferPartilhado = (BufferCircular*)MapViewOfFile(*hFileEscritaMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (escreve->bufferPartilhado == NULL) {
		_tprintf(TEXT("Erro no MapViewOfFile\n"));
		return -1;
	}

	escreve->terminar = 0;

	//nProdutores, sao o que define os ids dos avioes
	WaitForSingleObject(escreve->hMutex, INFINITE);
	escreve->bufferPartilhado->nProdutores++;
	escreve->idAviao = escreve->bufferPartilhado->nProdutores;
	ReleaseMutex(escreve->hMutex);
}

void preparaThreadDeGestaoViagens(ThreadGerirViagens* control) {
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


void setupAviao(Aviao* aviao, ThreadsControlerAviao* control) {
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
	_tprintf(TEXT("a espera de resposta...\n"));
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
	_tprintf(TEXT("a espera de resposta...\n"));
	WaitForSingleObject(control->leitura->hEvent, INFINITE);
	obterCordsDeString(control->leitura->ultimaMsg, &aviao->x, &aviao->y);

	printAviao(aviao, NULL);
}

int abrirMapaPartilhado(HANDLE* hMapaDePosicoesPartilhada, HANDLE* mutexAcesso) {
	*hMapaDePosicoesPartilhada = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, MAPA_PARTILHADO);
	if (*hMapaDePosicoesPartilhada == NULL) {
		_tprintf(TEXT("Controlador nao esta disponivel.\n"));
		return 1;
	}
	return 0;

	*mutexAcesso = CreateMutex(NULL, FALSE, MUTEX_MAPA_PARTILHADO);

	if (*mutexAcesso == NULL) {
		_tprintf(TEXT("Erro no mutexAcesso hMapaDePosicoesPartilhada\n"));
		return -1;
	}
}

void atualizaPosicaoAviao(Aviao* a, int x, int y) {
	a->x = x;
	a->y = y;
}

void updateAeroportoAviao(Aviao* a, int idAero) {
	a->idAeroporto = idAero;
}

void updateAviao(Aviao* a,int idAero, int statusViagem, int x, int y) {
	a->statusViagem = statusViagem;
	updateAeroportoAviao(a, idAero);
	atualizaPosicaoAviao(a, x, y);
}

void reCalcularRota(MapaPartilhado* partilhado, int currX, int currY, int * nextX, int* nextY) {
	int quatroVizinhos[4];
	quatroVizinhos[0] = verificaPosLivre(partilhado->avioesMapa, currX + 1, currY);
	quatroVizinhos[1] = verificaPosLivre(partilhado->avioesMapa, currX - 1, currY);
	quatroVizinhos[2] = verificaPosLivre(partilhado->avioesMapa, currX, currY + 1);
	quatroVizinhos[3] = verificaPosLivre(partilhado->avioesMapa, currX, currY - 1);
	int i = 0;
	while (quatroVizinhos[i] == 0) {
		i++;
	}

	switch (i)
	{
	case 0: {
		*nextX = currX + 1;
		break;
	}
	case 1: {
		*nextX = currX - 1;
		break;
	}
	case 2: {
		*nextY = currY + 1;
		break;
	}
	case 3: {
		*nextY = currY - 1;
		break;
	}
	}
}

int viajar(ThreadGerirViagens* dados) {

	Aviao* aviaoLocal = dados->aviaoMemLocal;
	MapaPartilhado * partilhado = dados->MapaPartilhado;

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
			int nextX = aviaoLocal->proxDestinoX;
			int nextY = aviaoLocal->proxDestinoY;
			
			int currX = aviaoLocal->x;
			int currY = aviaoLocal->x;

			aviaoLocal->statusViagem = 1;
			while (aviaoLocal->statusViagem == 1) {
				//int move(int cur_x, int cur_y, int final_dest_x, int final_dest_y, int * next_x, int* next_y)
				aviaoLocal->statusViagem = (ProcAdd)(currX, currY, aviaoLocal->proxDestinoX, aviaoLocal->proxDestinoY, &nextX, &nextY);
				// status 1 mov correta, 2 erro, 0 chegou 
				// 
				//_tprintf(TEXT("\n (x: %d, y: %d) status %d"), nextX, nextY, aviaoLocal->statusViagem);

				WaitForSingleObject(dados->hMutexAcessoAMapaPartilhado, INFINITE);
				//CopyMemory(&local, partilhado, sizeof(MapaPartilhado)); TODO avaliar acesso mais performante com um CopyMemory
#pragma region atualiza memoria partilhada direto -> live -> fica atualizado para todos os avioes e controlador
				int index = getAviao(aviaoLocal->id, partilhado->avioesMapa);
				Aviao* aux = &partilhado->avioesMapa[index];
				int livre = -1;
				if (aviaoLocal->statusViagem == 0 && aviaoLocal->proxDestinoX == nextX && aviaoLocal->proxDestinoY == nextY) {
					_tprintf(TEXT("\nChegou"));
					updateAviao(aux, aviaoLocal->proxDestinoId, aviaoLocal->statusViagem, nextX, nextY);
				}
				else {
					livre = verificaPosLivre(partilhado->avioesMapa, nextX, nextY);
					if (livre == 1) {
						updateAviao(aux, -1, aviaoLocal->statusViagem, nextX, nextY);
					}
					else {
						_tprintf(TEXT("\n A recalcular rota"));
						reCalcularRota(partilhado, currX, currY, &nextX, &nextY);
						updateAviao(aux, -1, aviaoLocal->statusViagem, nextX, nextY);
					}
				}
			
#pragma endregion 
				ReleaseMutex(dados->hMutexAcessoAMapaPartilhado);
				_tprintf(TEXT("\n (x: %d, y: %d) status %d"), nextX, nextY, aviaoLocal->statusViagem);

				if (livre == 1) {
					atualizaPosicaoAviao(aviaoLocal, nextX, nextY);
				}

				currX = nextX;
				currY = nextY;
				if (aviaoLocal->statusViagem != 0) {
					Sleep(aviaoLocal->posPorSegundo * 1000);
				}
				else {
					break;
				}
			}
			if (aviaoLocal->statusViagem == 0) {
				fFreeResult = FreeLibrary(hinstLib);
				return aviaoLocal->statusViagem;
			}

		}


	}

}

void disparaEventoDeInicioViagem(ThreadGerirViagens* control) {
	SetEvent(control->hEventNovaViagem);
	Sleep(20);
	ResetEvent(control->hEventNovaViagem);
}

int verificaPosLivre(Aviao* lista, int x, int y) {
	for (int i = 0; i < MAXAVIOES; i++) {
		if (lista[i].id != 0) {
			if (lista[i].x == x && lista[i].y == y) {
				return 0;
			}
		}
	}
	return 1;
}


void interacaoComConsolaAviao(Aviao * aviao, ControllerToPlane * ler , MSGThread * escreve, ThreadGerirViagens * ThreadViagens) {
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
					aviao->proxDestinoId = _tstoi(nextToken);
					TCHAR msg[100] = TEXT("prox ");
					_tcscat_s(msg, 100, nextToken);

					enviarMensagemParaControlador(escreve, msg);

					WaitForSingleObject(ler->hEvent, INFINITE);
					obterCordsDeString(ler->ultimaMsg, &aviao->proxDestinoX, &aviao->proxDestinoY);
					_tprintf(TEXT("controlador: %s\n"), ler->ultimaMsg);

					_tprintf(TEXT("X:%d  Y:%d\n"), aviao->proxDestinoX, aviao->proxDestinoY);
					if (_tcscmp(ler->ultimaMsg, L"erro") > 0) {
						_tprintf(TEXT("Pr�ximo destino definido.\n"));
					}
				}
			}
			else if (_tcscmp(token, TEXT("emb")) == 0) {
				// todo
				_tprintf(TEXT("Embarcar passageiros.\n"));
			}
			else if (_tcscmp(token, L"init") == 0) {
				_tprintf(TEXT("A tra�ar rota.\n"));
				TCHAR info[100] = TEXT("init ");
				//_tcscat_s(info, 300, _tstoi(proxDestino));
				disparaEventoDeInicioViagem(ThreadViagens);

				//_tprintf(TEXT("Iniciou viagem.\n"));
			}
			else if (_tcscmp(token, L"quit") == 0) {
				// todo
				_tprintf(TEXT("Sair de inst�ncia de avi�o.\n"));
			}
			token = wcstok_s(NULL, delim, &nextToken);
		}
	}

}