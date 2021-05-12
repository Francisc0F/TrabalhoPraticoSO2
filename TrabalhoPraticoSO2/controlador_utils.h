#pragma once
#include <tchar.h>
#include "../utils.h"
#define TAM 200


typedef struct {
	MSGThread* leitura;
	ControllerToPlane* escrita;
	Aeroporto* listaAeroportos;
	Aviao* avioes;
}ThreadsControlerControlador;



void menuControlador();

void adicionarAviao(int id, int n_passag, int max_passag, int posPorSegundo, int idAero, Aviao lista[]);
void listaAvioes(Aviao lista[], TCHAR* out);

int getAeroporto(int id, Aeroporto lista[]);


void adicionarAeroporto(TCHAR* nome, int x, int y, Aeroporto lista[]);
void listaTudo(Aeroporto lista[], TCHAR* out);
void inicializarLista(Aeroporto lista[]);

void ThreadEnvioDeMsgParaAvioes(ControllerToPlane* escrever, HANDLE* hFileMap, HANDLE* hEscrita);
void checkRegEditKeys(TCHAR* key_dir, HKEY handle, DWORD handleRes, TCHAR* key_name, int* maxAvioes);
void preparaParaLerInfoDeAvioes(MSGThread* ler, HANDLE* hLerFileMap);

void enviarMensagemParaAviao(int id, ControllerToPlane* escreve, TCHAR* info);
void printAeroporto(pAeroporto aero, TCHAR* out);