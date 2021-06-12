#include <tchar.h>
#include <windows.h>
#include <strsafe.h>
#include "../utils.h"
#include "controlador_utils.h"



void menuControlador() {
	_putws(TEXT("\naddAero <nome> <cordX> <cordY> - Adicionar aeroporto"));
	_putws(TEXT("lista - lista toda a informação do aeroporto"));
	_putws(TEXT("suspender ou ativar - aceitação de novos aviões por parte dos utilizadores"));
	_putws(TEXT("end - Encerrar sistema, notifica todos os processos"));
}

BOOL verificaAeroCords(int x, int y, Aeroporto lista[]) {
	int raio = 10;
	int limInfY = (y - raio);
	int limSupY = (y + raio);
	int limInfX = (x - raio);
	int limSupX = (x + raio);
	for (int i = 0; i < MAXAEROPORTOS; i++) {
		pAeroporto aux = &lista[i];
		if (aux->id > 0) {
			if (aux->y >= limInfY && aux->y <= limSupY &&
				aux->x >= limInfX && aux->x <= limSupX) {
				return FALSE;
			}
		}
	}
	return TRUE;
}

BOOL verificaAeroExiste(TCHAR* nome, Aeroporto lista[]) {
	for (int i = 0; i < MAXAEROPORTOS; i++) {
		pAeroporto aux = &lista[i];
		if (_tcscmp(aux->nome, nome) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOL verificaPassagExiste(TCHAR* nome, Passag lista[]) {
	for (int i = 0; i < MAXPASSAGEIROS; i++) {
		pPassag aux = &lista[i];
		if (_tcscmp(aux->nome, nome) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}


int adicionarAeroporto(TCHAR* nome, int x, int y, Aeroporto lista[]) {
	pAeroporto aux = NULL;
	int i;
	for (i = 0; i < MAXAEROPORTOS; i++) {
		aux = &lista[i];
		if (_tcscmp(aux->nome, L"") == 0) {
			_tcscpy_s(aux->nome, _countof(aux->nome), nome);
			aux->id = i + 1;
			aux->x = x;
			aux->y = y;
			aux->xBM = aux->x * MAPFACTOR;
			aux->yBM = aux->y * MAPFACTOR;
			break;
		}
	}
	return i;
}

void printAeroporto(pAeroporto aero, TCHAR* out) {
	TCHAR* text = L"\nid: [%d]\nNome: [%s]\nPosicao: (%d, %d)\n";
	TCHAR aux[100];
	if (out != NULL) {
		_stprintf_s(aux, 100, text, aero->id, aero->nome, aero->x, aero->y);
		_tcscat_s(out, 300, aux);
	}
	else {
		_tprintf(text, aero->id, aero->nome, aero->x, aero->y);
	}
}

void printPassag(pPassag p, TCHAR* out) {
	TCHAR* text = PASSAGFORMAT;
	TCHAR aux[100];
	if (out != NULL) {
		_stprintf_s(aux, 100, text, p->pid, p->nome, p->origem, p->destino, p->tempEspera);
		_tcscat_s(out, 300, aux);
	}
	else {
		_tprintf(text, p->pid, p->nome, p->origem, p->destino, p->tempEspera);
	}
}

void adicionarAviao(Aviao* a, Aviao lista[]) {
	// int id, int n_passag, int max_passag, int posPorSegundo, int idAero,
	for (int i = 0; i < MAXAVIOES; i++) {
		if (lista[i].id == 0) {
			lista[i].id = a->id;
			lista[i].idAeroporto = a->idAeroporto;
			lista[i].n_passag = a->n_passag;
			lista[i].max_passag = a->max_passag;
			lista[i].posPorSegundo = a->posPorSegundo;
			lista[i].x = a->x;
			lista[i].y = a->x;

			lista[i].segundosVivo = 0;

			lista[i].proxDestinoId = -1;
			lista[i].proxDestinoX = -1;
			lista[i].proxDestinoY = -1;
			lista[i].statusViagem = -1;

			break;
		}
	}
}

void adicionarPassag(pPassag a, Passag lista[]) {
	pPassag aux = NULL;
	for (int i = 0; i < MAXPASSAGEIROS; i++) {
		aux = &lista[i];
		if (aux->pid == 0) {
			aux->pid = a->pid;
			aux->tempEspera = a->tempEspera;
			_tcscpy_s(aux->destino, _countof(aux->destino), a->destino);
			_tcscpy_s(aux->origem, _countof(aux->origem), a->origem);
			_tcscpy_s(aux->nome, _countof(aux->nome), a->nome);
			break;
		}
	}
}


int getAeroporto(int id, Aeroporto lista[]) {
	for (int i = 0; i < MAXAEROPORTOS; i++) {
		if (lista[i].id == id) {
			return i;
		}
	}
	return -1;
}

void removerEm(int index, Aviao lista[]) {
	if (index + 1 > MAXAVIOES) {
		_tprintf(L"Exedeu Boundries");
	}
	else {
		for (int i = index + 1; i < MAXAVIOES; i++) {
			lista[i - 1] = lista[i];
		}
	}

}

void removerTodos(Aviao lista[]) {
	for (int i = 0; i < MAXAVIOES; i++) {
		apagaDoSistema(&lista[i]);
	}
}

void listaAvioes(Aviao lista[], TCHAR* out) {
	TCHAR* titulo = L"[Aviões]\n";
	if (out != NULL) {
		_tcscat_s(out, 100, titulo);
	}
	else {
		_tprintf(titulo);
	}

	for (int i = 0; i < MAXAVIOES; i++) {
		if (lista[i].id != 0) {
			printAviao(&lista[i], out);
		}
	}
}

void listaAvioesEmAero(Aviao lista[], int idAero, TCHAR* out) {
	TCHAR* titulo = L"[Aviões]\n";
	if (out != NULL) {
		_tcscat_s(out, 100, titulo);
	}
	else {
		_tprintf(titulo);
	}

	for (int i = 0; i < MAXAVIOES; i++) {
		if (lista[i].id < 0) {
			break;
		}
		
		if ( lista[i].idAeroporto == idAero) {
			printAviao(&lista[i], out);
		}
	}
}




void listaAeroportos(Aeroporto lista[], TCHAR* out) {
	TCHAR* titulo = L"[Aeroportos]\n";
	if (out != NULL) {
		_tcscat_s(out, 100, titulo);
	}
	else {
		_tprintf(titulo);
	}

	for (int i = 0; i < MAXAEROPORTOS; i++) {
		if (_tcscmp(lista[i].nome, L"") != 0) {
			printAeroporto(&lista[i], out);
		}
	}
}

void listaPassageiros(Passag lista[], TCHAR* out) {
	TCHAR* titulo = L"[Passageiros]\n";
	if (out != NULL) {
		_tcscat_s(out, 100, titulo);
	}
	else {
		_tprintf(titulo);
	}

	for (int i = 0; i < MAXPASSAGEIROS; i++) {
		if (lista[i].pid > 0) {
			printPassag(&lista[i], out);
		}
	}
}

void listaPassageirosEmAeroporto(Passag lista[], TCHAR* aero,  TCHAR* out) {
	TCHAR* titulo = TEXT("\n[Passageiros]");
	if (out != NULL) {
		_tcscat_s(out, 400, titulo);
	}
	else {
		_tprintf(titulo);
	}

	for (int i = 0; i < MAXPASSAGEIROS; i++) {
		if (lista[i].pid == 0) {
			break;
		}
		if (_tcscmp(aero, lista[i].origem) == 0) {
			printPassag(&lista[i], out);
		}
	}
}


int ThreadEnvioDeMsgParaAvioes(ControllerToPlane* escrever, HANDLE* hFileMap, HANDLE* hEscrita) {

	//mapeia ficheiro num bloco de memoria
	*hFileMap = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		sizeof(MSGCtrlToPlane), // alterar o tamanho do filemapping
		TEXT(FILE_MAP_MSG_TO_PLANES)); //nome do file mapping, tem de ser único

	if (*hFileMap == NULL) {
		_tprintf(TEXT("Erro no CreateFileMapping\n"));
		//CloseHandle(hFile); //recebe um handle e fecha esse handle , no entanto o handle é limpo sempre que o processo termina
		return -1;
	}

	//mapeia bloco de memoria para espaço de endereçamento
	escrever->fileViewMap = (MSGCtrlToPlane*)MapViewOfFile(
		*hFileMap,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		0);

	if (escrever->fileViewMap == NULL) {
		_tprintf(TEXT("Erro no MapViewOfFile\n"));
		return 1;
	}


	escrever->hEvent = CreateEvent(
		NULL,
		TRUE,
		FALSE,
		TEXT(EVENT_MSG_TO_PLANES));

	if (escrever->hEvent == NULL) {
		_tprintf(TEXT("Erro no CreateEvent\n"));
		UnmapViewOfFile(escrever->fileViewMap);
		return -2;
	}

	// evento pra gerir ordem de escrita
	escrever->hEventOrdemDeEscrever = CreateEvent(
		NULL,
		TRUE,
		FALSE, NULL);

	if (escrever->hEventOrdemDeEscrever == NULL) {
		_tprintf(TEXT("Erro no CreateEvent hEventOrdemDeEscrever\n"));
		return -3;
	}

	escrever->hMutex = CreateMutex(
		NULL,
		FALSE,
		TEXT(MUTEX_MSG_TO_PLANES));

	if (escrever->hMutex == NULL) {
		_tprintf(TEXT("Erro no CreateMutex\n"));
		UnmapViewOfFile(escrever->fileViewMap);
		return -1;
	}
	escrever->terminar = 0;

	return 0;
}

int checkRegEditKeys(TCHAR* key_dir, HKEY handle, DWORD handleRes, TCHAR* key_name, int* maxAvioes) {

	// open Or create
	if (RegCreateKeyEx(
		HKEY_CURRENT_USER,
		key_dir,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&handle,
		&handleRes
	) != ERROR_SUCCESS) {
		_tprintf(TEXT("Chave N_avioes nao foi nem criada nem aberta.\n"));
		return -1;
	}

	if (handleRes == REG_CREATED_NEW_KEY)
		_tprintf(TEXT("Chave criada: %s\n"), key_dir);
	else
		_tprintf(TEXT("Chave aberta: %s\n"), key_dir);

	TCHAR key_value[TAM];

	DWORD tamanho = sizeof(key_value);
	if (RegQueryValueEx(
		handle,
		key_name,
		0,
		NULL,
		(LPBYTE)key_value,
		&tamanho
	) != ERROR_SUCCESS) {
		_tprintf(TEXT("Numero de Avioes maximo nao esta definido.\n"));
		_tprintf(TEXT("Numero de Avioes maximo: "));
		_tscanf_s(TEXT("%s"), key_value, TAM);

		if (RegSetValueEx(
			handle,
			key_name,
			0,
			REG_SZ,
			(LPCBYTE)&key_value,
			sizeof(TCHAR) * (_tcslen(key_value) + 1) //lê o buffer todo
		) != ERROR_SUCCESS) {
			_tprintf(TEXT("O atributo nao foi alterado nem criado! ERRO! %s"), key_name);
			return -1;
		}
		*maxAvioes = _tstoi(key_value);
	}
	else {
		*maxAvioes = _tstoi(key_value);
		///_tprintf(TEXT("Atributo [%s] foi encontrado! value [%d]\n"), key_name, *maxAvioes);
	}
	_tprintf(TEXT("Max avioes: %d\n"), *maxAvioes);
	RegCloseKey(handle);
	return 0;
}

int setupMapaPartilhado(HANDLE* hMapaDePosicoesPartilhada, HANDLE* mutexAcesso) {
	*hMapaDePosicoesPartilhada = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, MAPA_PARTILHADO);
	if (*hMapaDePosicoesPartilhada == NULL) {

		*hMapaDePosicoesPartilhada = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			sizeof(MapaPartilhado),
			MAPA_PARTILHADO);

		if (*hMapaDePosicoesPartilhada == NULL) {
			_tprintf(TEXT("Erro no hMapaDePosicoesPartilhada\n"));
			return -1;
		}
	}
	else {
		_tprintf(TEXT("Controlador ja em Execucao\n"));
		return 1;
	}
	*mutexAcesso = CreateMutex(NULL, FALSE, MUTEX_MAPA_PARTILHADO);

	if (*mutexAcesso == NULL) {
		_tprintf(TEXT("Erro no mutexAcesso hMapaDePosicoesPartilhada\n"));
		return -1;
	}

	return 0;

}

int preparaParaLerInfoDeAvioes(MSGThread* ler, HANDLE* hLerFileMap) {

	ler->hSemEscrita = CreateSemaphore(NULL, TAM_BUFFER_CIRCULAR, TAM_BUFFER_CIRCULAR, SEMAPHORE_ESCRITA_MSG_TO_CONTROLER);
	ler->hSemLeitura = CreateSemaphore(NULL, 0, TAM_BUFFER_CIRCULAR, SEMAPHORE_LEITURA_MSG_TO_CONTROLER);

	//criar mutex para os consumidores neste caso so ha 1, o Controlador
	ler->hMutex = CreateMutex(NULL, FALSE, MUTEX_CONSUMIDOR_MSG_TO_CONTROLER);

	if (ler->hSemEscrita == NULL || ler->hSemLeitura == NULL || ler->hMutex == NULL) {
		_tprintf(TEXT("Erro no CreateSemaphore hSemLeitura hSemEscrita ou no CreateMutex\n"));
		return -1;
	}

	*hLerFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, FILE_MAP_MSG_TO_CONTROLER);
	if (*hLerFileMap == NULL) {
		*hLerFileMap = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			sizeof(BufferCircular),
			FILE_MAP_MSG_TO_CONTROLER);

		if (*hLerFileMap == NULL) {
			_tprintf(TEXT("Erro no CreateFileMapping\n"));
			return -1;
		}
	}

	//mapeamos o bloco de memoria para o espaco de enderaçamento do nosso processo
	ler->bufferPartilhado = (BufferCircular*)MapViewOfFile(*hLerFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);


	if (ler->bufferPartilhado == NULL) {
		_tprintf(TEXT("Erro no MapViewOfFile\n"));
		return -1;
	}
	// inicializar buffer circular partilhado 
	ler->bufferPartilhado->nConsumidores = 0;
	ler->bufferPartilhado->nProdutores = 0;
	ler->bufferPartilhado->posE = 0;
	ler->bufferPartilhado->posL = 0;

	ler->terminar = 0;

	return 0;
}

void enviarMensagemBroadCast(ControllerToPlane* escreve, TCHAR* info) {
	_tcscpy_s(escreve->msgToSend.info, _countof(escreve->msgToSend.info), info);
	escreve->msgToSend.idAviao = -1;
	SetEvent(escreve->hEventOrdemDeEscrever);
	Sleep(50);
	ResetEvent(escreve->hEventOrdemDeEscrever); //bloqueia evento
}

void enviarMensagemParaAviao(int id, ControllerToPlane* escreve, TCHAR* info) {
	_tcscpy_s(escreve->msgToSend.info, _countof(escreve->msgToSend.info), info);
	escreve->msgToSend.idAviao = id;
	SetEvent(escreve->hEventOrdemDeEscrever);
	Sleep(50);
	ResetEvent(escreve->hEventOrdemDeEscrever); //bloqueia evento
}

void interacaoConsolaControlador(Aeroporto* aeroportos, MapaPartilhado* mapaPartilhadoAvioes, HANDLE* mutexAccessoMapa, ControllerToPlane* escrita) {
	// menu  
	while (1) {
		menuControlador();
		TCHAR tokenstring[50] = { 0 };
		_fgetts(tokenstring, 50, stdin);
		tokenstring[_tcslen(tokenstring) - 1] = '\0';
		TCHAR* ptr = NULL;
		TCHAR delim[] = L" ";
		TCHAR* token = wcstok_s(tokenstring, delim, &ptr);

		TCHAR nome[100];
		int y;
		int x;
		while (token != NULL)
		{
			//_tprintf(L"%ls\n", token);
			if (_tcscmp(token, L"addAero") == 0) {
				token = wcstok_s(NULL, delim, &ptr);
				if (tokenValid(token)) {
					_tcscpy_s(nome, _countof(nome), token);
					token = wcstok_s(NULL, delim, &ptr);
					if (tokenValid(token) && isNumber(token)) {
						x = _tstoi(token);
						token = wcstok_s(NULL, delim, &ptr);
						if (tokenValid(token) && isNumber(token)) {
							y = _tstoi(token);
							adicionarAeroporto(nome, x, y, aeroportos);
						}
						else {
							_tprintf(TEXT("Erro Param cord y invalido.\n"));
							break;
						}
					}
					else {
						_tprintf(TEXT("Erro Param cord x invalido.\n"));
						break;
					}
				}
				else {
					_tprintf(TEXT("Erro Param nome invalido.\n"));
					break;
				}
			}
			else if (_tcscmp(token, L"lista") == 0) {
				listaAeroportos(aeroportos, NULL);
				_tprintf(TEXT("\n"));
				WaitForSingleObject(*mutexAccessoMapa, INFINITE);
				listaAvioes(mapaPartilhadoAvioes->avioesMapa, NULL);
				ReleaseMutex(*mutexAccessoMapa);
				break;
			}
			else if (_tcscmp(token, L"suspender") == 0) {
				_putws(TEXT("suspende aceitação de novos aviões por parte dos utilizadores"));
			}
			else if (_tcscmp(token, L"ativar") == 0) {
				_putws(TEXT("ativa aceitação de novos aviões por parte dos utilizadores"));
			}
			else if (_tcscmp(token, L"end") == 0) {
				WaitForSingleObject(*mutexAccessoMapa, INFINITE);
				if (mapaPartilhadoAvioes->numAtualAvioes > 0) {
					removerTodos(mapaPartilhadoAvioes->avioesMapa);
				}
				ReleaseMutex(*mutexAccessoMapa);
				enviarMensagemBroadCast(escrita, TEXT("terminar"));
				_tprintf(TEXT("A Encerrar sistema...\nTodos os processos serão notificados.\n"));
				Sleep(1000);

				_tprintf(TEXT("Controlador terminou.\n"));
				TCHAR cmd[23];
				_fgetts(cmd, 23, stdin);
				exit(4);
			}
			token = wcstok_s(NULL, delim, &ptr);
		}
	}

}

VOID DisconnectAndReconnect(LPPIPEINST Pipe)
{
	// Disconnect the pipe instance. 

	if (!DisconnectNamedPipe(Pipe->hPipeInst))
	{
		_tprintf(TEXT("DisconnectNamedPipe failed with %d\n"), GetLastError());
	}

	// Call a subroutine to connect to the new client. 

	Pipe->fPendingIO = ConnectToNewClient(
		Pipe->hPipeInst,
		&Pipe->oOverlap);

	Pipe->dwState = Pipe->fPendingIO ?
		CONNECTING_STATE : // still connecting 
		READING_STATE;     // ready to read 
}

BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
	BOOL fConnected, fPendingIO = FALSE;

	// Start an overlapped connection for this pipe instance. 
	fConnected = ConnectNamedPipe(hPipe, lpo);

	// Overlapped ConnectNamedPipe should return zero. 
	if (fConnected)
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	switch (GetLastError())
	{
		// The overlapped connection in progress. 
	case ERROR_IO_PENDING:
		fPendingIO = TRUE;
		break;

		// Client is already connected, so signal an event. 

	case ERROR_PIPE_CONNECTED:
		if (SetEvent(lpo->hEvent))
			break;

		// If an error occurs during the connect operation... 
	default:
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}
	}

	return fPendingIO;
}

