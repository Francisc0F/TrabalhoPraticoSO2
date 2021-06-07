#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include "../utils.h"
#include "controlador_utils.h"
#include "resource.h"
#define MAX 1000
#define MAP 1000
#define MAXAVIOES 100

HBITMAP hBmpAviao;
HBITMAP hBmpAero;
HDC bmpAviaoDC; // hdc do bitmap
HDC bmpAeroDC; // hdc do bitmap
BITMAP bmpAviao; // informa��o sobre o bitmap
BITMAP bmpAero; // informa��o sobre o bitmap

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



		for (int i = 0; i < MAXAVIOES; i++) {
			if (lista[i].id != 0) {
				Aviao* aux = &listaAvi[i];
				aux->xBM = aux->x * 10;
				aux->yBM = aux->y * 10;
			}
		}


		//liberta mutex
		ReleaseMutex(hMutexPintura);

		// dizemos ao sistema que a posi��o da imagem mudou e temos entao de fazer o refresh da janela
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
	//adicionarAeroporto(TEXT("Madrid"), 10, 10, aeroportos);
	//adicionarAeroporto(TEXT("Paris"), 20, 10, aeroportos);
	//adicionarAeroporto(TEXT("Moscovo, Russia"), 30, 18, aeroportos);


	//interacaoConsolaControlador(aeroportos, mapaPartilhadoAvioes, &hMutexAcessoMapa, &escrever);

	// UI 

	HWND hWnd;		// hWnd � o handler da janela, gerado mais abaixo por CreateWindow()
	MSG lpMsg;		// MSG � uma estrutura definida no Windows para as mensagens
	WNDCLASSEX wcApp;	// WNDCLASSEX � uma estrutura cujos membros servem para
			  // definir as caracter�sticas da classe da janela


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


	// Cria mutex
	hMutexPintura = CreateMutex(NULL, FALSE, NULL);
	ThreadAtualizaUI atualiza;
	atualiza.aeroportos = aeroportos;
	atualiza.MapaPartilhado = mapaPartilhadoAvioes;
	// Cria a thread de movimenta��o
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
	return((int)lpMsg.wParam);	// Retorna sempre o par�metro wParam da estrutura lpMsg
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
	wcApp->hInstance = hInstGlobal;		         // Inst�ncia da janela actualmente exibida
	wcApp->lpszClassName = L"Controlador";       // Nome da janela (neste caso = nome do programa)
	wcApp->lpfnWndProc = TrataEventos;       // Endere�o da fun��o de processamento da janela
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
		TEXT("Controlador"),// Texto que figura na barra do t�tulo
		WS_OVERLAPPEDWINDOW,	// Estilo da janela (WS_OVERLAPPED= normal)
		CW_USEDEFAULT,		// Posi��o x pixels (default=� direita da �ltima)
		CW_USEDEFAULT,		// Posi��o y pixels (default=abaixo da �ltima)
		CW_USEDEFAULT,		// Largura da janela (em pixels)
		CW_USEDEFAULT,		// Altura da janela (em pixels)
		(HWND)HWND_DESKTOP,	// handle da janela pai (se se criar uma a partir de
						// outra) ou HWND_DESKTOP se a janela for a primeira,
						// criada a partir do "desktop"
		(HMENU)NULL,			// handle do menu da janela (se tiver menu)
		(HINSTANCE)hInstGlobal,		// handle da inst�ncia do programa actual ("hInst" �
						// passado num dos par�metros de WinMain()
		0);				// N�o h� par�metros adicionais para a janela
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
	bmpAviaoDC = CreateCompatibleDC(hdc)
		;
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


	switch (messg)
	{
		//case WM_CREATE:
		//	
		//	break;

	case WM_COMMAND:

		switch (LOWORD(wParam))
		{
		case ID_ADMIN_GENERAL:
			DialogBox(NULL, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, TrataEventosAdministrador);
			break;

		}
		break;


	case WM_PAINT:
		// Inicio da pintura da janela, que substitui o GetDC
		hdc = BeginPaint(hWnd, &ps);
		GetClientRect(hWnd, &rect);

		// se a copia estiver a NULL, significa que � a 1� vez que estamos a passar no WM_PAINT e estamos a trabalhar com a copia em memoria
		if (memDC == NULL) {
			// cria copia em memoria
			memDC = CreateCompatibleDC(hdc);
			hBitmapDB = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
			// aplicamos na copia em memoria as configs que obtemos com o CreateCompatibleBitmap
			SelectObject(memDC, hBitmapDB);
			DeleteObject(hBitmapDB);
		}
		// opera��es feitas na copia que � o memDC
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

		ReleaseMutex(hMutexPintura);

		// bitblit da copia que esta em memoria para a janela principal - � a unica opera��o feita na janela principal
		BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

		// Encerra a pintura, que substitui o ReleaseDC
		EndPaint(hWnd, &ps);
		break;

		// redimensiona e calcula novamente o centro
	//case WM_SIZE:
	//
	//	break;

	case WM_CLOSE:
		// handle , texto da janela, titulo da janela, configura��es da MessageBox(botoes e icons)
		if (MessageBox(hWnd, TEXT("Tem a certeza que quer sair?"), TEXT("Confirma��o"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
			// o utilizador disse que queria sair da aplica��o
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
			// GetDlgItemText() vai � dialogbox e vai buscar o input do user
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
		//case IDC_LISTBOX_EXAMPLE:
		//{
		//	switch (HIWORD(wParam))
		//	{
		//	case LBN_SELCHANGE:
		//	{
		//		HWND hwndList = GetDlgItem(hDlg, IDC_LISTBOX_EXAMPLE);

		//		// Get selected index.
		//		int lbItem = (int)SendMessage(hwndList, LB_GETCURSEL, 0, 0);

		//		// Get item data.
		//		int i = (int)SendMessage(hwndList, LB_GETITEMDATA, lbItem, 0);

		//		// Do something with the data from Roster[i]
		//		TCHAR buff[MAX_PATH];
		//		StringCbPrintf(buff, ARRAYSIZE(buff),
		//			TEXT("Position: %s\nGames played: %d\nGoals: %d"),
		//			Roster[i].achPosition, Roster[i].nGamesPlayed,
		//			Roster[i].nGoalsScored);

		//		SetDlgItemText(hDlg, IDC_STATISTICS, buff);
		//		return TRUE;
		//	}
		//	}
		//}
		return TRUE;
		}
		// o LOWORD(wParam) traz o ID onde foi carregado 
		// se carregou no OK
		if (LOWORD(wParam) == IDOK)
		{
			// GetDlgItemText() vai � dialogbox e vai buscar o input do user
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
