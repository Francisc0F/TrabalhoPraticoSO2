#pragma once
#include <tchar.h>
#include "../utils.h"
#define TAM 200


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



void menuControlador();


void adicionarAviao(Aviao* a, Aviao lista[]);

void removerEm(int index, Aviao lista[]);
void removerTodos(Aviao lista[]);

void listaAvioes(Aviao lista[], TCHAR* out);

int getAeroporto(int id, Aeroporto lista[]);

void adicionarAeroporto(TCHAR* nome, int x, int y, Aeroporto lista[]);
void listaAeroportos(Aeroporto lista[], TCHAR* out);

int ThreadEnvioDeMsgParaAvioes(ControllerToPlane* escrever, HANDLE* hFileMap, HANDLE* hEscrita);
int checkRegEditKeys(TCHAR* key_dir, HKEY handle, DWORD handleRes, TCHAR* key_name, int* maxAvioes);
int preparaParaLerInfoDeAvioes(MSGThread* ler, HANDLE* hLerFileMap);

void enviarMensagemParaAviao(int id, ControllerToPlane* escreve, TCHAR* info);
void enviarMensagemBroadCast(ControllerToPlane* escreve, TCHAR* info);

void printAeroporto(pAeroporto aero, TCHAR* out);

int setupMapaPartilhado(HANDLE* hMapaDePosicoesPartilhada, HANDLE* mutexAcesso);

void interacaoConsolaControlador(Aeroporto* aeroportos, MapaPartilhado* mapaPartilhadoAvioes, HANDLE * hmutexMapaPartilhado, ControllerToPlane* escrita);

