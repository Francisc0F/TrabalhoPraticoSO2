#include <Windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <strsafe.h>
#include <stdio.h>
#include "../utils.h"
#include "controlador_utils.h"
#include "resource.h"
#define MAX 1000
#define MAP 1000
#define MAXAVIOES 100

#define TID_POLLMOUSE 100
#define MOUSE_POLL_DELAY 500

POINT pt = { 1,2 };

#pragma region declaracaoGlobais para UI
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
Passag* passagGlobal = NULL;

#pragma endregion

#pragma region declaracaoFuncoes
BOOL GerarConsola();
BOOL GerarWindow(WNDCLASSEX* wcApp);
BOOL GerarJanelaUI(HWND* hWnd, WNDCLASSEX* wcApp);
BOOL carregaBitmaps(HWND* hWnd);
LRESULT CALLBACK TrataEventos(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK TrataEventosAdministrador(HWND, UINT, WPARAM, LPARAM);
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
			(*threadControl->numAtualAvioes)++;
			ReleaseMutex(*threadControl->hMutexAcessoMapa);
			_tprintf(TEXT("adicionou aviao\n"));
			TCHAR send[100] = { 0 };
			preparaStringdeCords(send, aux->x, aux->y);
			enviarMensagemParaAviao(celLocal.id, threadControl->escrita, send);

			if (*threadControl->numAtualAvioes == 1) {
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
		}
		else if (_tcscmp(celLocal.info, TEXT("aero")) == 0) {
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
	MapaPartilhado* mapa = dados->MapaPartilhado;
	Aviao* listaAvi = mapa->avioesMapa;
	while (1) {

		// Aguarda que o mutex esteja livre
		WaitForSingleObject(hMutexPintura, INFINITE);

		for (int i = 0; i < MAXAVIOES; i++) {
			if (listaAvi[i].id != 0) {
				Aviao* aux = &listaAvi[i];
				aux->xBM = aux->x * MAPFACTOR;
				aux->yBM = aux->y * MAPFACTOR;
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
DWORD MensagensPipes(ThreadCriadorPipes* dados) {
	HANDLE* hEvents = dados->hEvents;
	LPPIPEINST Pipe = dados->hPipes;

	DWORD i, dwWait, cbRet, dwErr;
	BOOL fSuccess;

	TCHAR answer[400] = { 0 };
	while (1)
	{
		// Wait for the event object to be signaled, indicating
		// completion of an overlapped read, write, or
		// connect operation.

		dwWait = WaitForMultipleObjects(
			INSTANCES,    // number of event objects
			hEvents,      // array of event objects
			FALSE,        // does not wait for all
			INFINITE);    // waits indefinitely

	  // dwWait shows which pipe completed the operation.

		i = dwWait - WAIT_OBJECT_0;  // determines which pipe
		if (i < 0 || i >(INSTANCES - 1))
		{
			_tprintf(L"Index out of range.\n");
			return 0;
		}

		// Get the result if the operation was pending.
		if (Pipe[i].fPendingIO)
		{
			fSuccess = GetOverlappedResult(
				Pipe[i].hPipeInst, // handle to pipe
				&Pipe[i].oOverlap, // OVERLAPPED structure
				&cbRet,            // bytes transferred
				FALSE);            // do not wait

			switch (Pipe[i].dwState)
			{
				// Pending connect operation
			case CONNECTING_STATE:
				if (!fSuccess)
				{
					_tprintf(L"Error %d.\n", GetLastError());
					return 0;
				}
				Pipe[i].dwState = READING_STATE;
				break;

				// Pending read operation
			case READING_STATE:
				if (!fSuccess || cbRet == 0)
				{
					DisconnectAndReconnect(&Pipe[i]);
					continue;
				}
				Pipe[i].cbRead = cbRet;
				Pipe[i].dwState = WRITING_STATE;
				break;

				// Pending write operation
			case WRITING_STATE:
				if (!fSuccess || cbRet != Pipe[i].cbToWrite)
				{
					DisconnectAndReconnect(&Pipe[i]);
					continue;
				}
				Pipe[i].dwState = READING_STATE;
				break;

			default:
			{
				_tprintf(L"Invalid State\n");
				return 0;
			}
			}
		}

		// The pipe state determines which operation to do next.

		switch (Pipe[i].dwState)
		{
			// READING_STATE:
			// The pipe instance is connected to the client
			// and is ready to read a request from the client.

		case READING_STATE:
			fSuccess = ReadFile(
				Pipe[i].hPipeInst,
				&Pipe[i].chRequest,
				sizeof(MensagemPipe),
				&Pipe[i].cbRead,
				&Pipe[i].oOverlap);

			// The read operation completed successfully.

			if (fSuccess && Pipe[i].cbRead != 0)
			{
				Pipe[i].fPendingIO = FALSE;
				Pipe[i].dwState = WRITING_STATE;
				continue;
			}

			// The read operation is still pending.

			dwErr = GetLastError();
			if (!fSuccess && (dwErr == ERROR_IO_PENDING))
			{
				Pipe[i].fPendingIO = TRUE;
				continue;
			}

			// An error occurred; disconnect from the client.

			DisconnectAndReconnect(&Pipe[i]);
			break;

			// WRITING_STATE:
			// The request was successfully read from the client.
			// Get the reply data and write it to the client.

		case WRITING_STATE:
			if (_tcscmp(Pipe[i].chRequest.mensagem, L"1") == 0) {
				TCHAR* res = NULL;
				if (verificaAeroExiste(Pipe[i].chRequest.autor.origem, aeroGlobal) &&
					verificaAeroExiste(Pipe[i].chRequest.autor.destino, aeroGlobal) &&
					!verificaPassagExiste(Pipe[i].chRequest.autor.nome, passagGlobal))
				{
					adicionarPassag(&Pipe[i].chRequest.autor, passagGlobal);
					res = TEXT("a espera de viagem");
				}
				else {
					res = TEXT("erro");
				}
				StringCchCopy(Pipe[i].chReply.mensagem, 400, res);
			}

			Pipe[i].cbToWrite = sizeof(MensagemPipe);
			fSuccess = WriteFile(
				Pipe[i].hPipeInst,
				&Pipe[i].chReply,
				Pipe[i].cbToWrite,
				&cbRet,
				&Pipe[i].oOverlap);

			// The write operation completed successfully.

			if (fSuccess && cbRet == Pipe[i].cbToWrite)
			{
				ResetEvent(Pipe[i].oOverlap.hEvent);
				Pipe[i].fPendingIO = FALSE;
				Pipe[i].dwState = READING_STATE;
				continue;
			}

			// The write operation is still pending.

			dwErr = GetLastError();
			if (!fSuccess && (dwErr == ERROR_IO_PENDING))
			{
				Pipe[i].fPendingIO = TRUE;
				continue;
			}

			// An error occurred; disconnect from the client.

			DisconnectAndReconnect(&Pipe[i]);
			break;

		default:
		{
			_tprintf(L"Invalid State\n");
			return 0;
		}
		}
	}
}

DWORD WINAPI CriadorDePIPES(LPVOID param) {
	ThreadCriadorPipes* dados = (ThreadCriadorPipes*)param;
	HANDLE* hEvents = dados->hEvents;
	LPPIPEINST Pipe = dados->hPipes;
	for (int i = 0; i < INSTANCES; i++)
	{
		// Create an event object for this instance.
		hEvents[i] = CreateEvent(
			NULL,    // default security attribute
			TRUE,    // manual-reset event
			TRUE,    // initial state = signaled
			NULL);   // unnamed event object

		if (hEvents[i] == NULL)
		{
			_tprintf(L"CreateEvent failed with %d.\n", GetLastError());
			return 0;
		}
		ZeroMemory(&Pipe[i].oOverlap, sizeof(Pipe[i].oOverlap));
		Pipe[i].oOverlap.hEvent = hEvents[i];

		Pipe[i].hPipeInst = CreateNamedPipe(
			PIPEPASSAG,            // pipe name
			PIPE_ACCESS_DUPLEX |     // read/write access
			FILE_FLAG_OVERLAPPED,    // overlapped mode
			PIPE_TYPE_MESSAGE |      // message-type pipe
			PIPE_READMODE_MESSAGE |  // message-read mode
			PIPE_WAIT,               // blocking mode
			PIPE_UNLIMITED_INSTANCES, // max. instances
			sizeof(MensagemPipe),   // output buffer size
			sizeof(MensagemPipe),   // input buffer size
			PIPE_TIMEOUT,            // client time-out
			NULL);                   // default security attributes

		if (Pipe[i].hPipeInst == INVALID_HANDLE_VALUE)
		{
			_tprintf("CreateNamedPipe failed with %d.\n", GetLastError());
			return 0;
		}

		// Call the subroutine to connect to the new client

		Pipe[i].fPendingIO = ConnectToNewClient(
			Pipe[i].hPipeInst,
			&Pipe[i].oOverlap);

		Pipe[i].dwState = Pipe[i].fPendingIO ?
			CONNECTING_STATE : // still connecting
			READING_STATE;     // ready to read
	}

	MensagensPipes(dados);
}

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
	Passag passag[MAXPASSAGEIROS] = { 0 };
	passagGlobal = passag;
	ZeroMemory(aeroportos, sizeof(Aeroporto) * MAXAEROPORTOS);
	ZeroMemory(passag, sizeof(Passag) * MAXPASSAGEIROS);

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
	if (mapaPartilhadoAvioes != NULL) {
		WaitForSingleObject(hMutexAcessoMapa, INFINITE);
		mapaPartilhadoAvioes->numAtualAvioes = 0;
		ReleaseMutex(hMutexAcessoMapa);
	}
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

//adicionarAeroporto(TEXT("Paris"), 20, 10, aeroportos);
//adicionarAeroporto(TEXT("Moscovo, Russia"), 30, 18, aeroportos);
	adicionarAeroporto(TEXT("Lisbon"), 2, 2, aeroportos);
	adicionarAeroporto(TEXT("Madrid"), 15, 15, aeroportos);

	PIPEINST Pipe[INSTANCES];
	HANDLE hEvents[INSTANCES];
	ThreadCriadorPipes threadCriadorPipes;
	threadCriadorPipes.hPipes = Pipe;
	threadCriadorPipes.hEvents = hEvents;
	hThreads[3] = CreateThread(NULL, 0, CriadorDePIPES, &threadCriadorPipes, 0, NULL);

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

	carregaBitmaps(&hWnd);

	hMutexPintura = CreateMutex(NULL, FALSE, NULL);
	ThreadAtualizaUI atualiza;
	atualiza.MapaPartilhado = mapaPartilhadoAvioes;
	CreateThread(NULL, 0, atualizaUI, &atualiza, 0, NULL);
	ShowWindow(hWnd, nCmdShow);

	HANDLE hAccel = LoadAccelerators(NULL, MAKEINTRESOURCE(IDR_ACCELERATOR2));

	while (GetMessage(&lpMsg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(hWnd, hAccel, &lpMsg))
		{
			TranslateMessage(&lpMsg);
			DispatchMessage(&lpMsg);
		}
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
	FILE* fpstdin = stdin, * fpstdout = stdout, * fpstderr = stderr;

	freopen_s(&fpstdin, "CONIN$", "r", stdin);
	freopen_s(&fpstdout, "CONOUT$", "w", stdout);
	freopen_s(&fpstderr, "CONOUT$", "w", stderr);
	SetConsoleTitle(L"Controlador Debug");
}

BOOL GerarWindow(WNDCLASSEX* wcApp) {
	wcApp->cbSize = sizeof(WNDCLASSEX);      // Tamanho da estrutura WNDCLASSEX
	wcApp->hInstance = hInstGlobal;		         // Instância da janela actualmente exibida
	wcApp->lpszClassName = L"Controlador";       // Nome da janela (neste caso = nome do programa)
	wcApp->lpfnWndProc = TrataEventos;       // Endereço da função de processamento da janela
	wcApp->style = CS_HREDRAW | CS_VREDRAW;  // Estilo da janela: Fazer o redraw se for
	wcApp->hIcon = LoadIcon(NULL, MAKEINTRESOURCE(IDI_ICON1));
	wcApp->hIconSm = LoadIcon(NULL, MAKEINTRESOURCE(IDI_ICON1));
	wcApp->hCursor = LoadCursor(NULL, IDC_ARROW);
	wcApp->lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
	wcApp->cbClsExtra = 0;
	wcApp->cbWndExtra = 0;
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
	hBmpAviao = (HBITMAP)LoadImage(NULL, TEXT(BITMAPAVIAO), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	_Post_equals_last_error_ DWORD error = GetLastError();
	if (hBmpAviao == NULL) {
		TCHAR text[100] = { 0 };
		_stprintf_s(text, _countof(text), L"LoadImage aviao failed(0x%x)\n", error);
		MessageBox(NULL, text, NULL, MB_ICONEXCLAMATION);
		return 0;
	}
	GetObject(hBmpAviao, sizeof(bmpAviao), &bmpAviao);

	hBmpAero = (HBITMAP)LoadImage(NULL, TEXT(BITMAPAERO), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	error = GetLastError();
	if (hBmpAero == NULL) {
		TCHAR text[100] = { 0 };
		_stprintf_s(text, _countof(text), L"LoadImage aeroporto failed (0x%x)\n", error);
		MessageBox(NULL, text, NULL, MB_ICONEXCLAMATION);
		return 0;
	}

	GetObject(hBmpAero, sizeof(bmpAero), &bmpAero);

	hdc = GetDC(*hWnd);
	bmpAviaoDC = CreateCompatibleDC(hdc);
	SelectObject(bmpAviaoDC, hBmpAviao);

	bmpAeroDC = CreateCompatibleDC(hdc);
	SelectObject(bmpAeroDC, hBmpAero);

	ReleaseDC(*hWnd, hdc);
	return 1;
}

LRESULT CALLBACK TrataEventos(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam) {
	//handle para o device context
	HDC hdc;
	PAINTSTRUCT ps;
	RECT rect;


	RECT rc;
	RECT wRc;
	GetWindowRect(hWnd, &rc);
	switch (messg)
	{
		//case WM_CREATE:
		//
		//	break;

	case WM_MOUSEHOVER:
		for (size_t i = 0; i < MAXAEROPORTOS; i++) {
			Aeroporto* aux = &aeroGlobal[i];
			int deCima = rc.top + 50 + aux->yBM;
			int daEsquerda = rc.left + aux->xBM;
			if (_tcscmp(aux->nome, L"") != 0 &&
				deCima  < pt.y &&
				deCima + bmpAero.bmHeight > pt.y &&
				daEsquerda < pt.x &&
				daEsquerda + bmpAero.bmWidth > pt.x) {
				aux->hover = 1;
			}
			else {
				aux->hover = 0;
			}
		}

		for (size_t i = 0; i < MAXAVIOES; i++) {
			Aviao* aux = &aviaoGlobal[i];
			int deCima = rc.top + 50 + aux->yBM;
			int daEsquerda = rc.left + aux->xBM;
			if (aux->id != 0 &&
				aux->idAeroporto != -1 &&
				deCima  < pt.y &&
				deCima + bmpAviao.bmHeight > pt.y &&
				daEsquerda < pt.x &&
				daEsquerda + bmpAviao.bmWidth > pt.x) {
				aux->hover = 1;
			}
			else {
				aux->hover = 0;
			}
		}
		break;
	case WM_MOUSEMOVE:
		SetTimer(hWnd, TID_POLLMOUSE, MOUSE_POLL_DELAY, NULL);
		break;
	case WM_TIMER:

		GetCursorPos(&pt);
		if (PtInRect(&rc, pt))
		{
			PostMessage(hWnd, WM_MOUSEHOVER, 0, 0L);
			break;
		}
		//PostMessage(hWnd, WM_MOUSELEAVE, 0, 0L);
		KillTimer(hWnd, TID_POLLMOUSE);
		break;


	case WM_COMMAND: {
		switch (LOWORD(wParam))
		{
		case ID_ADMIN_GENERAL:
			DialogBox(NULL, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, TrataEventosAdministrador);
			break;
		}
		break;
	}
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
		for (size_t i = 0; i < MAXAEROPORTOS; i++) {
			Aeroporto* aux = &aeroGlobal[i];
			if (_tcscmp(aux->nome, L"") != 0) {
				TCHAR info[200] = { 0 };
				swprintf_s(info, 200, L"Aeroporto: %s", aux->nome);
				BitBlt(memDC, aux->xBM, aux->yBM, bmpAero.bmWidth, bmpAero.bmHeight, bmpAeroDC, 0, 0, SRCCOPY);
				if (aux->hover) {
					int margem = 20;
					rect.left = aux->xBM + bmpAero.bmWidth + margem;
					rect.top = aux->yBM;
					DrawText(memDC, info, -1,&rect, DT_SINGLELINE);
				}
			}
		}
		for (size_t i = 0; i < MAXAVIOES; i++) {
			Aviao* aux = &aviaoGlobal[i];
			if (aux->id != 0) {
				BitBlt(memDC, aux->xBM, aux->yBM, bmpAviao.bmWidth, bmpAviao.bmHeight, bmpAviaoDC, 0, 0, SRCCOPY);
			}
		}
		ReleaseMutex(hMutexPintura);

		// bitblit da copia que esta em memoria para a janela principal - é a unica operação feita na janela principal
		BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

		// Encerra a pintura, que substitui o ReleaseDC
		EndPaint(hWnd, &ps);
		break;

		//case WM_SIZE:
		//
		//	break;

	case WM_CLOSE:

		if (MessageBox(hWnd, TEXT("Tem a certeza que quer sair?"), TEXT("Confirmação"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
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

LRESULT CALLBACK TrataEventosAdministrador(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam)
{
	TCHAR nomeAero[100];
	TCHAR CordX[100];
	TCHAR CordY[100];

	switch (messg)
	{
	case WM_INITDIALOG:
	{
		// Add items to list.
		HWND hwndList = GetDlgItem(hWnd, IDC_LIST1);
		for (int i = 0; i < MAXAEROPORTOS; i++)
		{
			Aeroporto* aux = &aeroGlobal[i];
			if (aux->id > 0) {
				int pos = (int)SendMessage(hwndList, LB_ADDSTRING, 0,
					(LPARAM)aux->nome);
				SendMessage(hwndList, LB_SETITEMDATA, pos, (LPARAM)i);
			}
		}
		// Set input focus to the list box.
		SetFocus(hwndList);
		break;
	}

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case IDOK:
		{
			// GetDlgItemText() vai à dialogbox e vai buscar o input do user
			//
			//MessageBox(hWnd, username, TEXT("IDOK"), MB_OK | MB_ICONINFORMATION);
		}
		case IDCANCEL: {
			EndDialog(hWnd, 0);
			return TRUE;
		}
		case IDC_BUTTON1: {
			GetDlgItemText(hWnd, IDC_EDIT1, nomeAero, 100);
			GetDlgItemText(hWnd, IDC_EDIT2, CordX, 100);
			GetDlgItemText(hWnd, IDC_EDIT3, CordY, 100);
			if (tokenValid(nomeAero) && tokenValid(CordX) && tokenValid(CordY)) {
				if (verificaAeroExiste(nomeAero, aeroGlobal)) {
					MessageBox(hWnd, TEXT("Ja Existe"), TEXT("OK"), MB_ICONERROR);
					break;
				}

				int x = _tstoi(CordX);
				int y = _tstoi(CordY);
				if (verificaAeroCords(x, y, aeroGlobal)) {
					int index = adicionarAeroporto(nomeAero, x, y, aeroGlobal);
					HWND hwndList = GetDlgItem(hWnd, IDC_LIST1);
					int pos = (int)SendMessage(hwndList, LB_ADDSTRING, 0,
						(LPARAM)nomeAero);
					SendMessage(hwndList, LB_SETITEMDATA, pos, index);
				}
				else {
					MessageBox(hWnd, TEXT("Aeroporto muito proximo de outro. tente mais longe"), TEXT("OK"), MB_ICONERROR);
					break;
				}
				MessageBox(hWnd, TEXT("Aeroporto up and running!!"), TEXT("OK"), MB_OK);
				break;
			}

			MessageBox(hWnd, TEXT("Erro dados Invalidos"), TEXT("OK"), MB_ICONERROR);
			break;
		}
		case IDC_LIST1:
		{
			switch (HIWORD(wParam))
			{
			case LBN_SELCHANGE:
			{
				HWND hwndList = GetDlgItem(hWnd, IDC_LIST1);

				int lbItem = (int)SendMessage(hwndList, LB_GETCURSEL, 0, 0);

				int i = (int)SendMessage(hwndList, LB_GETITEMDATA, lbItem, 0);

				Aeroporto* aux = &aeroGlobal[i];
				TCHAR buff[MAX_PATH];
				TCHAR* formatStr = TEXT("id: [%d]\nNome: [%s]\nPosicao: (%d, %d)\n");

				StringCbPrintf(buff, ARRAYSIZE(buff), formatStr, aux->id, aux->nome, aux->x, aux->y);
				TCHAR listaStr[1000] = { 0 };
				listaPassageirosEmAeroporto(passagGlobal, aux->nome, listaStr);
				listaAvioesEmAero(aviaoGlobal, aux->id, listaStr);

				_tcscat_s(buff, MAX_PATH, listaStr);
				SetDlgItemText(hWnd, IDC_STATIC_3, buff);
				return TRUE;
			}
			}
		}
		return TRUE;
		}
		if (LOWORD(wParam) == IDOK)
		{
			// GetDlgItemText() vai à dialogbox e vai buscar o input do user
			//
			//MessageBox(hWnd, username, TEXT("IDOK"), MB_OK | MB_ICONINFORMATION);
		}
		else if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hWnd, 0);
			return TRUE;
		}
		else if (LOWORD(wParam) == IDC_BUTTON1) {
			GetDlgItemText(hWnd, IDC_EDIT1, nomeAero, 100);
			GetDlgItemText(hWnd, IDC_EDIT2, CordX, 100);
			GetDlgItemText(hWnd, IDC_EDIT3, CordY, 100);
			if (tokenValid(nomeAero) && tokenValid(CordX) && tokenValid(CordY)) {
				if (verificaAeroExiste(nomeAero, aeroGlobal)) {
					MessageBox(hWnd, TEXT("Ja Existe"), TEXT("OK"), MB_ICONERROR);
					break;
				}

				int x = _tstoi(CordX);
				int y = _tstoi(CordY);
				if (verificaAeroCords(x, y, aeroGlobal)) {
					int index = adicionarAeroporto(nomeAero, x, y, aeroGlobal);
					HWND hwndList = GetDlgItem(hWnd, IDC_LIST1);
					int pos = (int)SendMessage(hwndList, LB_ADDSTRING, 0,
						(LPARAM)nomeAero);
					SendMessage(hwndList, LB_SETITEMDATA, pos, index);
				}
				else {
					MessageBox(hWnd, TEXT("Aeroporto muito proximo de outro. tente mais longe"), TEXT("OK"), MB_ICONERROR);
					break;
				}
				MessageBox(hWnd, TEXT("Aeroporto up and running!!"), TEXT("OK"), MB_OK);
				break;
			}

			MessageBox(hWnd, TEXT("Erro dados Invalidos"), TEXT("OK"), MB_ICONERROR);
		}
		break;

	case WM_CLOSE:

		EndDialog(hWnd, 0);
		return TRUE;
	}

	return FALSE;
}