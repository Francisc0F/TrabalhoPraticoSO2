#pragma once
#include <tchar.h>
#include "../utils.h"
#define TAM 200

void menuControlador();
void adicionarAeroporto(TCHAR* nome, int x, int y, Aeroporto lista[]);
void listaTudo(Aeroporto lista[]);
void inicializarLista(Aeroporto lista[]);
void ThreadEnvioDeMsgParaAvioes(ThreadControllerToPlane* escrever, HANDLE* hFileMap, HANDLE* hEscrita);
void checkRegEditKeys(TCHAR* key_dir, HKEY handle, DWORD handleRes, TCHAR* key_name, int* maxAvioes);
void preparaParaLerInfoDeAvioes(MSGThread* ler, HANDLE* hLerFileMap);