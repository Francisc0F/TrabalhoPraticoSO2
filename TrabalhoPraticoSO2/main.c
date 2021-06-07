#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include "../utils.h"
#include "controlador_utils.h"
#define MAX 1000
#define MAP 1000
#define MAXAVIOES 100

HBITMAP hBmpAviao;
HBITMAP hBmpAero;
HDC bmpAviaoDC; // hdc do bitmap
HDC bmpAeroDC; // hdc do bitmap
BITMAP bmpAviao; // informação sobre o bitmap
BITMAP bmpAero; // informação sobre o bitmap

int limDir; // limite direito
HWND hWndGlobal; // handle para a janela
HINSTANCE hInstGlobal; // winman instance
HANDLE hMutexPintura;

HDC memDC = NULL; // copia do device context que esta em memoria, tem de ser inicializado a null
HBITMAP hBitmapDB; // copia as caracteristicas da janela original para a janela que vai estar em memoria
HANDLE hConsola;


Aeroporto* aeroGlobal = NULL;
Aviao* aviaoGlobal = NULL;

#pragma regio declaracaoFuncoes 
BOOL GerarConsola();
BOOL GerarWindow(WNDCLASSEX* wcApp);
BOOL GerarJanelaUI(HWND* hWnd, WNDCLASSEX* wcApp);
BOOL carregaBitmaps(HWND* hWnd);

LRESULT CALLBACK TrataEventos(HWND, UINT, WPARAM, LPARAM);
#pragma endregion



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

		if (_tcscmp(celLocal.info, TEXT("info")) == 0) {
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
			else if ((*threadControl->numAtualAvioes) == 0) {
				// todo cancel waitabletimer
			}



			TCHAR send[100] = { 0 };
			preparaStringdeCords(send, aux->x, aux->y);
			enviarMensagemParaAviao(celLocal.id, threadControl->escrita, send);

		}if (_tcscmp(celLocal.info, TEXT("aero")) == 0) {
			_tprintf(TEXT("Enviou %s.\n"), celLocal.info);
			TCHAR info[400] = TEXT("");
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
				Aviao* listaPartilhada = threadControl->MapaPartilhado->avioesMapa;
				for (int i = 0; i < MAXAVIOES; i++) {
					Aviao* local = &threadControl->MapaPartilhadoLocal.avioesMapa[i];
					int index = getAviao(local->id, listaPartilhada);
					if (index > -1) {
						if (listaPartilhada[index].segundosVivo == local->segundosVivo && local->id > 0) {
							_tprintf(L"Aviao %d Desapareceu no radar.\n", local->id);
							removerEm(index, listaPartilhada);
							threadControl->MapaPartilhadoLocal.numAtualAvioes--;
							// pode entrar mais um 
							ReleaseSemaphore(threadControl->hControloDeNumeroDeAvioes, 1, NULL);
						}
					}
					else {
						_tprintf(L"Erro nao encontrou aviao na mem partilhada 173");
					}

				}
				CopyMemory(&threadControl->MapaPartilhadoLocal, threadControl->MapaPartilhado, sizeof(MapaPartilhado));
			}
			ReleaseMutex(threadControl->hMutexAcessoMapa);
		}

	}

	return 0;
}


DWORD WINAPI atualizaUI(LPVOID param) {
	ThreadAtualizaUI* dados = (ThreadAtualizaUI*)param;
	int dir = 1; // 1 para a direita, -1 para a esquerda
	int salto = 1; // quantidade de pixeis que a imagem salta de cada vez
	Aeroporto* lista = dados->aeroportos;
	MapaPartilhado* mapa = dados->MapaPartilhado;
	Aviao* listaAvi = mapa->avioesMapa;
	int cordsAeroportos = 0;
	while (1) {
		// Aguarda que o mutex esteja livre
		WaitForSingleObject(hMutexPintura, INFINITE);

		if (!cordsAeroportos) {
			cordsAeroportos = 1;
			for (int i = 0; i < MAXAEROPORTOS; i++) {
				Aeroporto* aux = &lista[i];
				if (_tcscmp(aux->nome, L"") != 0) {
					aux->xBM = aux->x * 10;
					aux->yBM = aux->y * 10;
					//_tprintf(TEXT("aux->yBM %d\n", aux->yBM));
					//_tprintf(TEXT("aux->xBM %d\n\n", aux->xBM));
				}
			}
		}

		for (int i = 0; i < MAXAVIOES; i++) {
			if (lista[i].id != 0) {
				Aviao* aux = &listaAvi[i];
				aux->xBM = aux->x * 10;
				aux->yBM = aux->y * 10;
			}
		}


		//liberta mutex
		ReleaseMutex(hMutexPintura);

		// dizemos ao sistema que a posição da imagem mudou e temos entao de fazer o refresh da janela
		InvalidateRect(hWndGlobal, NULL, FALSE);
		Sleep(1);
	}
	return 0;

}

#pragma endregion 

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {

	AllocConsole();
	GerarConsola();

	TCHAR* erroControl = TEXT("Controlador vai Terminar.\n");

#pragma region regedit keys setup
	TCHAR key_dir[TAM] = TEXT("Software\\TRABALHOSO2\\");
	HKEY handle = NULL; // handle para chave depois de aberta ou criada
	DWORD handleRes = NULL;
	TCHAR key_name[TAM] = TEXT("N_avioes"); //nome do par-valor
	int maxAvioes = 0;
	int check = checkRegEditKeys(key_dir, handle, handleRes, TEXT("N_avioes"), &maxAvioes);
	if (check != 0) {
		_tprintf(erroControl);
		return -1;
	}
#pragma endregion 

	Aeroporto aeroportos[MAXAEROPORTOS] = { 0 };
	aeroGlobal = aeroportos;
	Aviao avioes[MAXAVIOES] = { 0 };

#pragma region lista de posicoes em mapa partilhado
	HANDLE hMapaDePosicoesPartilhada = NULL;
	HANDLE hMutexAcessoMapa = NULL;
	check = setupMapaPartilhado(&hMapaDePosicoesPartilhada, &hMutexAcessoMapa);
	if (check != 0) {
		_tprintf(erroControl);
		return -10;
	}
	//mapa de avioes
	MapaPartilhado* mapaPartilhadoAvioes = (MapaPartilhado*)MapViewOfFile(hMapaDePosicoesPartilhada, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	WaitForSingleObject(hMutexAcessoMapa, INFINITE);
	mapaPartilhadoAvioes->numAtualAvioes = 0;
	ReleaseMutex(hMutexAcessoMapa);
#pragma endregion
	aviaoGlobal = mapaPartilhadoAvioes->avioesMapa;


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

	HANDLE hThreads[5];

	// ouvir mensagens dos avioes
	MSGThread ler;
	HANDLE hLerFileMap;
	HANDLE hThreadLeitura;
	controler.leitura = &ler;
	check = preparaParaLerInfoDeAvioes(controler.leitura, &hLerFileMap);
	if (check != 0) {
		_tprintf(erroControl);
		return -1;
	}
	hThreads[0] = CreateThread(NULL, 0, ThreadLerBufferCircular, &controler, 0, NULL);

	// escrever para os avioes
	ControllerToPlane escrever;
	HANDLE hFileMap;
	HANDLE hEscrita;
	controler.escrita = &escrever;
	check = ThreadEnvioDeMsgParaAvioes(controler.escrita, &hFileMap, &hEscrita);
	if (check != 0) {
		_tprintf(erroControl);
		return -1;
	}

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


	//interacaoConsolaControlador(aeroportos, mapaPartilhadoAvioes, &hMutexAcessoMapa, &escrever);

	// UI 

	HWND hWnd;		// hWnd é o handler da janela, gerado mais abaixo por CreateWindow()
	MSG lpMsg;		// MSG é uma estrutura definida no Windows para as mensagens
	WNDCLASSEX wcApp;	// WNDCLASSEX é uma estrutura cujos membros servem para
			  // definir as características da classe da janela


	hInstGlobal = hInst;
	if (!GerarWindow(&wcApp)) {
		_tprintf(TEXT("Erro: GerarWindow(&wcApp\n"));
		return -5;
	}


	// ============================================================================
	// 2. Registar a classe "wcApp" no Windows
	// ============================================================================
	if (!RegisterClassEx(&wcApp))
		return(0);


	if (!GerarJanelaUI(&hWnd, &wcApp)) {
		_tprintf(TEXT("Erro: GerarJanelaUI\n"));
		return -3;
	}

	// ============================================================================
	// 3. Criar a janela
	// ============================================================================


	carregaBitmaps(&hWnd);

	//HDC hdc; // representa a propria janela
	//RECT rect;

	//// carregar o bitmap
	//hBmpAviao = (HBITMAP)LoadImage(NULL, TEXT(BITMAPAVIAO), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	//GetObject(hBmpAviao, sizeof(bmpAviao), &bmpAviao); // vai buscar info sobre o handle do bitmap

	//hdc = GetDC(hWnd);
	//// criamos copia do device context e colocar em memoria
	//bmpAviaoDC = CreateCompatibleDC(hdc);
	//// aplicamos o bitmap ao device context
	//SelectObject(bmpAviaoDC, hBmpAviao);

	//ReleaseDC(hWnd, hdc);

	//// EXEMPLO
	//// 800 px de largura, imagem 40px de largura
	//// ponto central da janela 400 px(800/2)
	//// imagem centrada, começar no 380px e acabar no 420 px
	//// (800/2) - (40/2) = 400 - 20 = 380px

	//// definir as posicoes inicias da imagem
	//GetClientRect(hWnd, &rect);
	//xBitmap = (rect.right / 2) - (bmpAviao.bmWidth / 2);
	//yBitmap = (rect.bottom / 2) - (bmpAviao.bmHeight / 2);

	//// limite direito é a largura da janela - largura da imagem
	//limDir = rect.right - bmpAviao.bmWidth;


	// Cria mutex
	hMutexPintura = CreateMutex(NULL, FALSE, NULL);
	ThreadAtualizaUI atualiza;
	atualiza.aeroportos = aeroportos;
	atualiza.MapaPartilhado = mapaPartilhadoAvioes;
	// Cria a thread de movimentação
	CreateThread(NULL, 0, atualizaUI, &atualiza, 0, NULL);


	// ============================================================================
	// 4. Mostrar a janela
	// ============================================================================
	ShowWindow(hWnd, nCmdShow);	// "hWnd"= handler da janela, devolvido por
					  // "CreateWindow"; "nCmdShow"= modo de exibição (p.e.
					  // normal/modal); é passado como parâmetro de WinMain()
	UpdateWindow(hWnd);		// Refrescar a janela (Windows envia à janela uma
					  // mensagem para pintar, mostrar dados, (refrescar)…
	// ============================================================================
	// 5. Loop de Mensagens
	// ============================================================================
	// O Windows envia mensagens às janelas (programas). Estas mensagens ficam numa fila de
	// espera até que GetMessage(...) possa ler "a mensagem seguinte"
	// Parâmetros de "getMessage":
	// 1)"&lpMsg"=Endereço de uma estrutura do tipo MSG ("MSG lpMsg" ja foi declarada no
	//   início de WinMain()):
	//			HWND hwnd		handler da janela a que se destina a mensagem
	//			UINT message		Identificador da mensagem
	//			WPARAM wParam		Parâmetro, p.e. código da tecla premida
	//			LPARAM lParam		Parâmetro, p.e. se ALT também estava premida
	//			DWORD time		Hora a que a mensagem foi enviada pelo Windows
	//			POINT pt		Localização do mouse (x, y)
	// 2)handle da window para a qual se pretendem receber mensagens (=NULL se se pretendem
	//   receber as mensagens para todas as
	// janelas pertencentes à thread actual)
	// 3)Código limite inferior das mensagens que se pretendem receber
	// 4)Código limite superior das mensagens que se pretendem receber

	// NOTA: GetMessage() devolve 0 quando for recebida a mensagem de fecho da janela,
	// 	  terminando então o loop de recepção de mensagens, e o programa

	while (GetMessage(&lpMsg, NULL, 0, 0)) {
		TranslateMessage(&lpMsg);	// Pré-processamento da mensagem (p.e. obter código
					   // ASCII da tecla premida)
		DispatchMessage(&lpMsg);	// Enviar a mensagem traduzida de volta ao Windows, que
					   // aguarda até que a possa reenviar à função de
					   // tratamento da janela, CALLBACK TrataEventos (abaixo)
	}



	//if (hThreads[3] != NULL) {
	//	WaitForSingleObject(hThreads[3], INFINITE);
	//	CloseHandle(hMapaDePosicoesPartilhada);
	//}

//	if (hThreads[0] != NULL && hThreads[1] != NULL && hThreads[2] != NULL && hThreads[3] != NULL)
//		WaitForMultipleObjects(4, hThreads, TRUE, INFINITE);


		// ============================================================================
	// 6. Fim do programa
	// ============================================================================
	return((int)lpMsg.wParam);	// Retorna sempre o parâmetro wParam da estrutura lpMsg
}


BOOL GerarConsola() {
	HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
	int hCrt = _open_osfhandle((long)handle_out, _O_TEXT);
	FILE* hf_out = _fdopen(hCrt, "w");
	setvbuf(hf_out, NULL, _IONBF, 1);
	*stdout = *hf_out;

	// Create Console Input Handle
	HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
	hCrt = _open_osfhandle((long)handle_in, _O_TEXT);
	FILE* hf_in = _fdopen(hCrt, "r");
	setvbuf(hf_in, NULL, _IONBF, 1);
	*stdin = *hf_in;

	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	
	SetConsoleTitle(L"Controlador Debug");

}

BOOL GerarWindow(WNDCLASSEX* wcApp) {

	wcApp->cbSize = sizeof(WNDCLASSEX);      // Tamanho da estrutura WNDCLASSEX
	wcApp->hInstance = hInstGlobal;		         // Instância da janela actualmente exibida
								   // ("hInst" é parâmetro de WinMain e vem
										 // inicializada daí)
	wcApp->lpszClassName = L"Controlador";       // Nome da janela (neste caso = nome do programa)
	wcApp->lpfnWndProc = TrataEventos;       // Endereço da função de processamento da janela
											// ("TrataEventos" foi declarada no início e
											// encontra-se mais abaixo)
	wcApp->style = CS_HREDRAW | CS_VREDRAW;  // Estilo da janela: Fazer o redraw se for
											// modificada horizontal ou verticalmente

	wcApp->hIcon = LoadIcon(NULL, IDI_APPLICATION);   // "hIcon" = handler do ícon normal
										   // "NULL" = Icon definido no Windows
										   // "IDI_AP..." Ícone "aplicação"
	wcApp->hIconSm = LoadIcon(NULL, IDI_INFORMATION); // "hIconSm" = handler do ícon pequeno
										   // "NULL" = Icon definido no Windows
										   // "IDI_INF..." Ícon de informação
	wcApp->hCursor = LoadCursor(NULL, IDC_ARROW);	// "hCursor" = handler do cursor (rato)
							  // "NULL" = Forma definida no Windows
							  // "IDC_ARROW" Aspecto "seta"
	wcApp->lpszMenuName = NULL;			// Classe do menu que a janela pode ter
							  // (NULL = não tem menu)
	wcApp->cbClsExtra = 0;				// Livre, para uso particular
	wcApp->cbWndExtra = 0;				// Livre, para uso particular
	wcApp->hbrBackground = CreateSolidBrush(RGB(125, 125, 125));
}

BOOL GerarJanelaUI(HWND* hWnd, WNDCLASSEX* wcApp) {

	*hWnd = CreateWindow(
		wcApp->lpszClassName,			// Nome da janela (programa) definido acima
		TEXT("Controlador"),// Texto que figura na barra do título
		WS_OVERLAPPEDWINDOW,	// Estilo da janela (WS_OVERLAPPED= normal)
		CW_USEDEFAULT,		// Posição x pixels (default=à direita da última)
		CW_USEDEFAULT,		// Posição y pixels (default=abaixo da última)
		CW_USEDEFAULT,		// Largura da janela (em pixels)
		CW_USEDEFAULT,		// Altura da janela (em pixels)
		(HWND)HWND_DESKTOP,	// handle da janela pai (se se criar uma a partir de
						// outra) ou HWND_DESKTOP se a janela for a primeira,
						// criada a partir do "desktop"
		(HMENU)NULL,			// handle do menu da janela (se tiver menu)
		(HINSTANCE)hInstGlobal,		// handle da instância do programa actual ("hInst" é
						// passado num dos parâmetros de WinMain()
		0);				// Não há parâmetros adicionais para a janela
	hWndGlobal = *hWnd;
}

BOOL carregaBitmaps(HWND* hWnd) {
	HDC hdc; // representa a propria janela
	RECT rect;

	// carregar bitmaps
	hBmpAviao = (HBITMAP)LoadImage(NULL, TEXT("ola.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	_Post_equals_last_error_ DWORD error = GetLastError();
	if (hBmpAviao == NULL) {
		TCHAR text[100] = { 0 };
		_stprintf_s(text,  _countof(text), L"LoadImage aviao failed(0x%x)\n", error);
		MessageBox(NULL, text, NULL, MB_ICONEXCLAMATION);
	}
	GetObject(hBmpAviao, sizeof(bmpAviao), &bmpAviao); 


	hBmpAero = (HBITMAP)LoadImage(NULL, TEXT("ola.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	error = GetLastError();
	if (hBmpAero == NULL) {
		TCHAR text[100] = { 0 };
		_stprintf_s(text, _countof(text), L"LoadImage aeroporto failed (0x%x)\n", error);
		MessageBox(NULL, text, NULL, MB_ICONEXCLAMATION);
	}
	GetObject(hBmpAero, sizeof(bmpAero), &bmpAero);

	hdc = GetDC(*hWnd);
	bmpAviaoDC = CreateCompatibleDC(hdc);
	SelectObject(bmpAviaoDC, hBmpAviao);

	bmpAeroDC = CreateCompatibleDC(hdc);
	SelectObject(bmpAeroDC, hBmpAero);

	ReleaseDC(*hWnd, hdc);
}

LRESULT CALLBACK TrataEventos(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam) {
	//handle para o device context
	HDC hdc;
	PAINTSTRUCT ps;
	RECT rect;


	switch (messg)
	{
	case WM_PAINT:
		// Inicio da pintura da janela, que substitui o GetDC
		hdc = BeginPaint(hWnd, &ps);
		GetClientRect(hWnd, &rect);

		// se a copia estiver a NULL, significa que é a 1ª vez que estamos a passar no WM_PAINT e estamos a trabalhar com a copia em memoria
		if (memDC == NULL) {
			// cria copia em memoria
			memDC = CreateCompatibleDC(hdc);
			hBitmapDB = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
			// aplicamos na copia em memoria as configs que obtemos com o CreateCompatibleBitmap
			SelectObject(memDC, hBitmapDB);
			DeleteObject(hBitmapDB);
		}
		// operações feitas na copia que é o memDC
		FillRect(memDC, &rect, CreateSolidBrush(RGB(125, 125, 125)));

		WaitForSingleObject(hMutexPintura, INFINITE);
		// operacoes de escrita da imagem - BitBlt
		for (size_t i = 0; i < MAXAEROPORTOS; i++) {
			Aeroporto* aux = &aeroGlobal[i];
			if (_tcscmp(aux->nome, L"") != 0) {
				BitBlt(memDC, aux->xBM, aux->yBM, bmpAero.bmWidth, bmpAero.bmHeight, bmpAeroDC, 0, 0, SRCCOPY);
			}
		}
		for (size_t i = 0; i < MAXAEROPORTOS; i++) {
			Aviao* aux = &aviaoGlobal[i];
			if (aux->id != 0) {
				BitBlt(memDC, aux->xBM, aux->yBM, bmpAviao.bmWidth, bmpAviao.bmHeight, bmpAviaoDC, 0, 0, SRCCOPY);
			}
		}
		//BitBlt(memDC, xBitmap, yBitmap, bmp.bmWidth, bmp.bmHeight, bmpDC, 0, 0, SRCCOPY);
		ReleaseMutex(hMutexPintura);

		// bitblit da copia que esta em memoria para a janela principal - é a unica operação feita na janela principal
		BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);


		// Encerra a pintura, que substitui o ReleaseDC
		EndPaint(hWnd, &ps);
		break;

	case WM_ERASEBKGND:
		return TRUE;

		// redimensiona e calcula novamente o centro
	case WM_SIZE:
		//WaitForSingleObject(hMutexPintura, INFINITE);
		//xBitmap = (LOWORD(lParam) / 2) - (bmp.bmWidth / 2);
		//yBitmap = (HIWORD(lParam) / 2) - (bmp.bmHeight / 2);
		//limDir = LOWORD(lParam) - bmp.bmWidth;
		//memDC = NULL; // metemos novamente a NULL para que caso haja um resize na janela no WM_PAINT a janela em memoria é sempre atualizada com o tamanho novo
		//ReleaseMutex(hMutexPintura);
		break;

	case WM_CLOSE:
		// handle , texto da janela, titulo da janela, configurações da MessageBox(botoes e icons)
		if (MessageBox(hWnd, TEXT("Tem a certeza que quer sair?"), TEXT("Confirmação"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
			// o utilizador disse que queria sair da aplicação
			DestroyWindow(hWnd);
		}
		break;

	case WM_DESTROY:
		// "PostQuitMessage(Exit Status)"
		FreeConsole();
		PostQuitMessage(0);
		break;
	default:
		return(DefWindowProc(hWnd, messg, wParam, lParam));
	}
}