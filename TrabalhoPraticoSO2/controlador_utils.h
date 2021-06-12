#pragma once
#include <tchar.h>
#include "../utils.h"
#define TAM 200
#define MAPFACTOR 10
#define PASSAGFORMAT TEXT("\nid: [%d] Nome: [%s] Origem: %s Destino: %s Tempo espera: %d\n");

typedef struct {
	MSGThread* leitura;
	ControllerToPlane* escrita;
	Aeroporto* listaAeroportos;
	Aviao* avioes;
	int* numAtualAvioes;
	HANDLE* hMutexAcessoMapa;
	HANDLE hTimerPing;
}ThreadsControlerControlador;

typedef struct {
	MapaPartilhado* MapaPartilhado;
	MapaPartilhado MapaPartilhadoLocal;
	HANDLE* hMutexAcessoMapa;
	HANDLE hControloDeNumeroDeAvioes;
	HANDLE hTimer;
	int terminar;
}ThreadGestaoDeMapa;

typedef struct {
	HINSTANCE hInst;
	HINSTANCE hPrevInst;
	LPSTR lpCmdLine;
	int* nCmdShow;
	MapaPartilhado* MapaPartilhado;
	MapaPartilhado MapaPartilhadoLocal;
	HANDLE* hMutexAcessoMapa;
	HANDLE hControloDeNumeroDeAvioes;
	HANDLE hTimer;
	int terminar;
}ThreadUI;


typedef struct {
	MapaPartilhado* MapaPartilhado;
	HANDLE hMutex;
	Aeroporto* aeroportos;
	int terminar;
}ThreadAtualizaUI;

#define CONNECTING_STATE 0
#define READING_STATE 1
#define WRITING_STATE 2
#define INSTANCES 4
#define PIPE_TIMEOUT 5000

typedef struct
{
	OVERLAPPED oOverlap;
	HANDLE hPipeInst;
	MensagemPipe chRequest;
	DWORD cbRead;
	MensagemPipe chReply;
	DWORD cbToWrite;
	DWORD dwState;
	BOOL fPendingIO;
} PIPEINST, * LPPIPEINST;

typedef struct {
	PIPEINST* hPipes;
	HANDLE* hEvents;
	HANDLE hMutex;
	int terminar;
}ThreadCriadorPipes;





void menuControlador();

BOOL verificaAeroExiste(TCHAR* nome, Aeroporto lista[]);
BOOL verificaPassagExiste(TCHAR* nome, Passag lista[]);
BOOL verificaAeroCords(int x, int y, Aeroporto lista[]);
void adicionarAviao(Aviao* a, Aviao lista[]);

void removerEm(int index, Aviao lista[]);
void removerTodos(Aviao lista[]);

void listaAvioes(Aviao lista[], TCHAR* out);
void listaAvioesEmAero(Aviao lista[], int idAero, TCHAR* out);


int getAeroporto(int id, Aeroporto lista[]);
void listaPassageiros(Passag lista[], TCHAR* out);
void listaPassageirosEmAeroporto(Passag lista[], TCHAR* aero, TCHAR* out);
void adicionarPassag(pPassag a, Passag lista[]);

int adicionarAeroporto(TCHAR* nome, int x, int y, Aeroporto lista[]);
void listaAeroportos(Aeroporto lista[], TCHAR* out);

int ThreadEnvioDeMsgParaAvioes(ControllerToPlane* escrever, HANDLE* hFileMap, HANDLE* hEscrita);
int checkRegEditKeys(TCHAR* key_dir, HKEY handle, DWORD handleRes, TCHAR* key_name, int* maxAvioes);
int preparaParaLerInfoDeAvioes(MSGThread* ler, HANDLE* hLerFileMap);

void enviarMensagemParaAviao(int id, ControllerToPlane* escreve, TCHAR* info);
void enviarMensagemBroadCast(ControllerToPlane* escreve, TCHAR* info);

void printAeroporto(pAeroporto aero, TCHAR* out);

int setupMapaPartilhado(HANDLE* hMapaDePosicoesPartilhada, HANDLE* mutexAcesso);

void interacaoConsolaControlador(Aeroporto* aeroportos, MapaPartilhado* mapaPartilhadoAvioes, HANDLE* hmutexMapaPartilhado, ControllerToPlane* escrita);

VOID DisconnectAndReconnect(LPPIPEINST Pipe);
BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo);