#pragma once

#define DLL "SO2_TP_DLL_2021.dll"
#define NThreads 4
typedef struct {
	MSGThread* escrita;
	ControllerToPlane* leitura;
}ThreadsControlerAviao;

typedef struct {
	HANDLE hTimer;
	MapaPartilhado* MapaPartilhado;
	Aviao* AviaoLocal;
	HANDLE *hMutexAcessoAMapaPartilhado;
	int terminar;
}ThreadPingControler;



typedef int(__cdecl* MYPROC)(LPWSTR);

void menuAviao();
void preparaLeituraMSGdoAviao(HANDLE* hFileMap, ControllerToPlane * ler);
void preparaEnvioDeMensagensParaOControlador(HANDLE* hFileEscritaMap, MSGThread* escreve, BOOL* primeiroProcesso);
void enviarMensagemParaControlador(MSGThread* escreve, TCHAR* info);
void setupAviao(Aviao * aviao, ThreadsControlerAviao* escreve);
int abrirMapaPartilhado(HANDLE* hMapaDePosicoesPartilhada, HANDLE* mutexAcesso);
int viajar(ThreadGerirViagens* dados);
void preparaThreadDeGestaoViagens(ThreadGerirViagens* control);
void disparaEventoDeInicioViagem(ThreadGerirViagens* control);
int verificaPosLivre(Aviao* lista, int x, int y);
void interacaoComConsolaAviao(Aviao* aviao, ControllerToPlane* ler, MSGThread* escreve, ThreadGerirViagens* ThreadViagens);