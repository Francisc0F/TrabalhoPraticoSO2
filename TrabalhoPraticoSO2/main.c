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

typedef int(__cdecl* MYPROC)(LPWSTR);

#pragma region threads declaration

#pragma region escrever na memoria partilhada
DWORD WINAPI ThreadEscrever(LPVOID param) {
	ThreadsControler* threadControl = (ThreadsControler*)param;
	ControllerToPlane* dados = threadControl->escrita;

	while (!(dados->terminar)) {
		// espera evento ordem de enviar mensagem
		WaitForSingleObject(dados->hEventOrdemDeEscrever, INFINITE);

		//faço lock ao mutex
		WaitForSingleObject(dados->hMutex, INFINITE);

		//limpa memoria antes de fazer a copia
		ZeroMemory(dados->fileViewMap, sizeof(MSGCtrlToPlane));

		//copia memoria de um sitio para outro 
		CopyMemory(dados->fileViewMap, &dados->msgToSend, sizeof(MSGCtrlToPlane));

		//liberto mutex
		ReleaseMutex(dados->hMutex);

		//evento ordem de leitura
		//desbloqueia evento   
		SetEvent(dados->hEvent);
		Sleep(500);

		ResetEvent(dados->hEvent); //bloqueia evento
	}
	return 0;
}

#pragma endregion 

#pragma region buffer circular leitura 
DWORD WINAPI ThreadLerBufferCircular(LPVOID param) {
	ThreadsControler* threadControl = (ThreadsControler*)param;
	MSGThread* dados = threadControl->leitura;
	MSGcel cel;
	int soma = 0;
	TCHAR* garbage = NULL;
	while (!dados->terminar) {
		//aqui entramos na logica da aula teorica

		//esperamos por uma posicao para lermos
		WaitForSingleObject(dados->hSemLeitura, INFINITE);

		//esperamos que o mutex esteja livre
		WaitForSingleObject(dados->hMutex, INFINITE);


		//vamos copiar da proxima posicao de leitura do buffer circular para a nossa variavel cel
		CopyMemory(&cel, &dados->memPar->buffer[dados->memPar->posL], sizeof(MSGcel));
		dados->memPar->posL++; //incrementamos a posicao de leitura para o proximo consumidor ler na posicao seguinte

		//se apos o incremento a posicao de leitura chegar ao fim, tenho de voltar ao inicio
		if (dados->memPar->posL == TAM_BUFFER)
			dados->memPar->posL = 0;

		//libertamos o mutex
		ReleaseMutex(dados->hMutex);

		//libertamos o semaforo. temos de libertar uma posicao de escrita
		ReleaseSemaphore(dados->hSemEscrita, 1, NULL);

		// trata mensagem
		_tprintf(TEXT("C%d consumiu %s.\n"), dados->id, cel.info);

		if (_tcscmp(cel.info, L"info") == 0) {
			adicionarAviao(cel.id, 0, cel.aviao.max_passag, cel.aviao.posPorSegundo, cel.aviao.idAeroporto, threadControl->avioes);
			enviarMensagemParaAviao(cel.id, threadControl->escrita, TEXT("sucesso"));
		}if (_tcscmp(cel.info, L"aero") == 0) {
			_tprintf(TEXT("Enviou %s.\n"), cel.info);
			TCHAR info[400]= TEXT("");
			listaTudo(threadControl->listaAeroportos, info);
			enviarMensagemParaAviao(cel.id, threadControl->escrita, info);
		}
		else {
			TCHAR* ptr;
			TCHAR delim[] = L" ";
			TCHAR* token = wcstok_s(cel.info, delim, &ptr);
			if (_tcscmp(token, L"prox") == 0) {
				token = wcstok_s(cel.info, delim, &ptr);
				if (token != NULL) {
					int proxDestino = _tcstol(token, &garbage, 0);
					switch (errno) {
					case ERANGE:
						enviarMensagemParaAviao(cel.id, threadControl->escrita, TEXT("ERANGE"));
						return 1;
						// host-specific (GNU/Linux in my case)
					case EINVAL:
						enviarMensagemParaAviao(cel.id, threadControl->escrita, TEXT("EINVAL"));
						return 1;
					}
				
					if (getAeroporto(proxDestino, threadControl->listaAeroportos) > 0) {
						enviarMensagemParaAviao(cel.id, threadControl->escrita, TEXT("sucesso"));
					}
					else {
						enviarMensagemParaAviao(cel.id, threadControl->escrita, TEXT("erro"));
					}
				}
				else {
					enviarMensagemParaAviao(cel.id, threadControl->escrita, TEXT("erro"));
				}
			}
		}
		
	}
	_tprintf(TEXT("fim thread leitura \n"));
	return 0;
}
#pragma endregion 

#pragma endregion 

int _tmain(int argc, TCHAR* argv[]) {

#pragma region unicode setup

	//UNICODE: Por defeito, a consola Windows não processa caracteres wide. 
	//A maneira mais fácil para ter esta funcionalidade é chamar _setmode:
#ifdef UNICODE 
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif
#pragma endregion 

	// TODO fazer validacao com open no mutex do controlador para saber se ha outro controlador vivo

#pragma region regedit keys setup
	TCHAR key_dir[TAM] = TEXT("Software\\TRABALHOSO2\\");
	HKEY handle = NULL; // handle para chave depois de aberta ou criada
	DWORD handleRes = NULL;
	TCHAR key_name[TAM] = TEXT("N_avioes"); //nome do par-valor

	int maxAvioes;
	checkRegEditKeys(key_dir, handle, handleRes, TEXT("N_avioes"), &maxAvioes);
#pragma endregion 

#pragma region threads setup
	Aeroporto aeroportos[MAXAEROPORTOS] = { 0 };
	Aviao avioes[MAXAVIOES] = { 0 };

	ThreadsControler controler;
	controler.listaAeroportos = aeroportos;
	controler.avioes = avioes;

	// ouvir mensagens dos avioes
	MSGThread ler;
	HANDLE hLerFileMap;
	HANDLE hThreadLeitura;

	controler.leitura = &ler;
	preparaParaLerInfoDeAvioes(controler.leitura, &hLerFileMap);
	hThreadLeitura = CreateThread(NULL, 0, ThreadLerBufferCircular, &controler, 0, NULL);

	ControllerToPlane escrever;
	HANDLE hFileMap;
	HANDLE hEscrita;
	controler.escrita = &escrever;
	ThreadEnvioDeMsgParaAvioes(controler.escrita, &hFileMap, &hEscrita);
	hEscrita = CreateThread(NULL, 0, ThreadEscrever, &controler, 0, NULL);

#pragma endregion 
	// setup aeroportos inicias

	adicionarAeroporto(TEXT("Lisbon"), 2, 2, aeroportos);
	adicionarAeroporto(TEXT("Madrid"), 10, 10, aeroportos);
	adicionarAeroporto(TEXT("Paris"), 20, 10, aeroportos);
	adicionarAeroporto(TEXT("Moscovo, Russia"), 30, 18, aeroportos);

#pragma region menu interface
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
				if (token != NULL) {
					_tcscpy_s(nome, _countof(nome), token);
					token = wcstok_s(NULL, delim, &ptr);
					if (token != NULL) {
						x = _tstoi(token);
						token = wcstok_s(NULL, delim, &ptr);
						if (token != NULL) {
							y = _tstoi(token);
							adicionarAeroporto(nome, x, y, aeroportos);
						}
					}
				}
			}
			else if (_tcscmp(token, L"lista") == 0) {
				//listaTudo(aeroportos, NULL);
				listaAvioes(avioes, NULL);
				break;
			}
			else if (_tcscmp(token, L"suspender") == 0) {
				_putws(TEXT("suspende aceitação de novos aviões por parte dos utilizadores"));
			}
			else if (_tcscmp(token, L"ativar") == 0) {
				_putws(TEXT("ativa aceitação de novos aviões por parte dos utilizadores"));
			}
			else if (_tcscmp(token, L"end") == 0) {
				_tprintf(TEXT("Encerrar sistema, todos os processos serão notificados.\n"));
			}
			token = wcstok_s(NULL, delim, &ptr);
		}
	}

#pragma endregion 


	if (hEscrita != NULL)
		WaitForSingleObject(hEscrita, INFINITE);

	if (hThreadLeitura != NULL) {
		WaitForSingleObject(hThreadLeitura, INFINITE);
	}


	//UnmapViewOfFile(ler.memPar);
	//CloseHandles ... mas é feito automaticamente quando o processo termina

	return 0;

}