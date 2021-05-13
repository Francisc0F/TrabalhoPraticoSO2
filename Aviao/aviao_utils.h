#pragma once

#define DLL "SO2_TP_DLL_2021.dll"
typedef struct {
	MSGThread* escrita;
	ControllerToPlane* leitura;
}ThreadsControlerAviao;

typedef struct {
	HANDLE hEventNovaViagem;
	int terminar;
	Aviao * aviao; //buffer circular em si (array de estruturas)
}ThreadGerirViagens;


typedef int(__cdecl* MYPROC)(LPWSTR);

void menuAviao();
void printMSG(MSGcel cel);
void preparaLeituraMSGdoAviao(HANDLE* hFileMap, ControllerToPlane * ler);
void preparaEnvioDeMensagensParaOControlador(HANDLE* hFileEscritaMap, MSGThread* escreve, BOOL* primeiroProcesso);
void enviarMensagemParaControlador(MSGThread* escreve, TCHAR* info);
void setupAviao(Aviao * aviao, ThreadsControlerAviao* escreve);
int abrirMapaPartilhado(HANDLE* hMapaDePosicoesPartilhada);
int viajarPara(Aviao* aviao);
void preparaThreadDeGestaoViagens(ThreadGerirViagens* control);
void disparaEventoDeInicioViagem(ThreadGerirViagens* control);