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

LRESULT CALLBACK TrataEventos(HWND, UINT, WPARAM, LPARAM);

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

DWORD WINAPI UIThread(LPVOID param) {
	HWND hWnd;		// hWnd é o handler da janela, gerado mais abaixo por CreateWindow()
	MSG lpMsg;		// MSG é uma estrutura definida no Windows para as mensagens
	WNDCLASSEX wcApp;	// WNDCLASSEX é uma estrutura cujos membros servem para 
			  // definir as características da classe da janela
	ThreadUI* dados = (ThreadUI*)param;

	TCHAR szProgName[] = TEXT("Controlador");
	// ============================================================================
	// 1. Definição das características da janela "wcApp" 
	//    (Valores dos elementos da estrutura "wcApp" do tipo WNDCLASSEX)
	// ============================================================================
	wcApp.cbSize = sizeof(WNDCLASSEX);      // Tamanho da estrutura WNDCLASSEX
	wcApp.hInstance = dados->hInst; // Instância da janela actualmente exibida 
								   // ("hInst" é parâmetro de WinMain e vem 
										 // inicializada daí)
	wcApp.lpszClassName = szProgName; // Nome da janela (neste caso = nome do programa)
	wcApp.lpfnWndProc = TrataEventos;       // Endereço da função de processamento da janela
										
	wcApp.style = CS_HREDRAW | CS_VREDRAW;  // Estilo da janela: Fazer o redraw se for
											// modificada horizontal ou verticalmente

	wcApp.hIcon = LoadIcon(NULL, IDI_APPLICATION);   // "hIcon" = handler do ícon normal
										   // "NULL" = Icon definido no Windows
										   // "IDI_AP..." Ícone "aplicação"
	wcApp.hIconSm = LoadIcon(NULL, IDI_INFORMATION); // "hIconSm" = handler do ícon pequeno
										   // "NULL" = Icon definido no Windows
										   // "IDI_INF..." Ícon de informação
	wcApp.hCursor = LoadCursor(NULL, IDC_ARROW);	// "hCursor" = handler do cursor (rato) 
							  // "NULL" = Forma definida no Windows
							  // "IDC_ARROW" Aspecto "seta" 
	wcApp.lpszMenuName = NULL;			// Classe do menu que a janela pode ter
							  // (NULL = não tem menu)
	wcApp.cbClsExtra = 0;				// Livre, para uso particular
	wcApp.cbWndExtra = 0;				// Livre, para uso particular
	wcApp.hbrBackground = CreateSolidBrush(RGB(125, 125, 125));
	//(HBRUSH)GetStockObject(WHITE_BRUSH);
// "hbrBackground" = handler para "brush" de pintura do fundo da janela. Devolvido por
// "GetStockObject".Neste caso o fundo será branco

// ============================================================================
// 2. Registar a classe "wcApp" no Windows
// ============================================================================
	if (!RegisterClassEx(&wcApp))
		return(0);

	// ============================================================================
	// 3. Criar a janela
	// ============================================================================
	hWnd = CreateWindow(
		szProgName,			// Nome da janela (programa) definido acima
		TEXT("Janelaaaa bruh"),// Texto que figura na barra do título
		WS_OVERLAPPEDWINDOW,	// Estilo da janela (WS_OVERLAPPED= normal)
		CW_USEDEFAULT,		// Posição x pixels (default=à direita da última)
		CW_USEDEFAULT,		// Posição y pixels (default=abaixo da última)
		CW_USEDEFAULT,		// Largura da janela (em pixels)
		CW_USEDEFAULT,		// Altura da janela (em pixels)
		(HWND)HWND_DESKTOP,	// handle da janela pai (se se criar uma a partir de
						// outra) ou HWND_DESKTOP se a janela for a primeira, 
						// criada a partir do "desktop"
		(HMENU)NULL,			// handle do menu da janela (se tiver menu)
		(HINSTANCE)wcApp.hInstance,		// handle da instância do programa actual ("hInst" é 
						// passado num dos parâmetros de WinMain()
		0);				// Não há parâmetros adicionais para a janela
	  // ============================================================================
	  // 4. Mostrar a janela
	  // ============================================================================
	ShowWindow(hWnd, *dados->nCmdShow);	// "hWnd"= handler da janela, devolvido por 
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

}
#pragma endregion 

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {

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
	//Aviao avioes[MAXAVIOES] = { 0 };

#pragma region lista de posicoes em mapa partilhado
	HANDLE hMapaDePosicoesPartilhada = NULL;
	HANDLE hMutexAcessoMapa = NULL;
	check = setupMapaPartilhado(&hMapaDePosicoesPartilhada, &hMutexAcessoMapa);
	if (check != 0) {
		_tprintf(erroControl);
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

	HANDLE hThreads[4];

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


	// UI thread
	ThreadUI UiThread;
	
	UiThread.hInst = hInst;
	UiThread.hPrevInst = hPrevInst;
	UiThread.lpCmdLine = lpCmdLine;
	UiThread.nCmdShow = &nCmdShow;
	hThreads[3] = CreateThread(NULL, 0, UIThread, &UiThread, 0, NULL);


	// setup aeroportos inicias
	adicionarAeroporto(TEXT("Lisbon"), 2, 2, aeroportos);
	adicionarAeroporto(TEXT("Madrid"), 10, 10, aeroportos);
	adicionarAeroporto(TEXT("Paris"), 20, 10, aeroportos);
	adicionarAeroporto(TEXT("Moscovo, Russia"), 30, 18, aeroportos);

	//interacaoConsolaControlador(aeroportos, mapaPartilhadoAvioes, &hMutexAcessoMapa, &escrever);

	if (hThreads[0] != NULL && hThreads[1] != NULL && hThreads[2] != NULL && hThreads[3] != NULL)
		WaitForMultipleObjects(4, hThreads, TRUE, INFINITE);
}

LRESULT CALLBACK TrataEventos(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam) {
	//handle para o device context
	HDC hdc;
	RECT rect; // representa um retangulo
	static TCHAR c = '?';
	int xPos, yPos;
	static int times = 0; // meti a var a static

	switch (messg)
	{
		// detetar o clique esquerdo do rato
	case WM_LBUTTONDOWN:
		times += 1;
		c = times + '0';

		break;
	case WM_RBUTTONDOWN:
		xPos = GET_X_LPARAM(lParam); // guarda a posição do rato
		yPos = GET_Y_LPARAM(lParam);

		// temos de ir buscar o handle para o device context
		hdc = GetDC(hWnd);

		// operações de configuração do retangulo
		GetClientRect(hWnd, &rect);
		rect.left = xPos; // indicamos um desvio da esquerda para a direita no x
		rect.top = yPos; // indicamos um desvio do topo da janela para o fundo da janela no y
		SetTextColor(hdc, RGB(255, 255, 255)); // cor do texto
		SetBkMode(hdc, TRANSPARENT); // quero que o fundo por trás do texto seja transparente. podemos usar o SetBkColor e temos uma cor concreta
		// operações de escrita no entanto de forma incorreta porque isto devia de estar em eventos de refrescamento
		DrawText(hdc, &c, 1, &rect, DT_NOCLIP | DT_SINGLELINE);

		// temos de fazer release do device context
		ReleaseDC(hWnd, hdc);
		break;

	case WM_KEYDOWN:
		if (wParam == VK_SPACE) {
			times = 0;
			c = times + '0';
		}

		break;

		// evento que o windows envia para  a funcao de callback quando alguem carregou no botao de fecho da janela
	case WM_CLOSE:
		// handle , texto da janela, titulo da janela, configurações da MessageBox(botoes e icons)
		if (MessageBox(hWnd, TEXT("Tem a certeza que quer sair?"), TEXT("Confirmação"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
			// o utilizador disse que queria sair da aplicação
			DestroyWindow(hWnd);
		}
		break;

	case WM_DESTROY:	// Destruir a janela e terminar o programa 
						// "PostQuitMessage(Exit Status)"		
		PostQuitMessage(0);
		break;
	default:
		// Neste exemplo, para qualquer outra mensagem (p.e. "minimizar","maximizar","restaurar")
		// não é efectuado nenhum processamento, apenas se segue o "default" do Windows
		return(DefWindowProc(hWnd, messg, wParam, lParam));
		break;  // break tecnicamente desnecessário por causa do return
	}
	return(0);
}
