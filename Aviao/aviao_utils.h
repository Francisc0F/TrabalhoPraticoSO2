#pragma once

#define DLL "SO2_TP_DLL_2021.dll"
typedef struct {
	MSGThread* escrita;
	ControllerToPlane* leitura;
}ThreadsControlerAviao;


void menuAviao();
void printMSG(MSGcel cel);
void preparaLeituraMSGdoAviao(HANDLE* hFileMap, ControllerToPlane * ler);
void preparaEnvioDeMensagensParaOControlador(HANDLE* hFileEscritaMap, MSGThread* escreve, BOOL* primeiroProcesso);
void enviarMensagemParaControlador(MSGThread* escreve, TCHAR* info);
void setupAviao(int* capacidadePassageiros, int* posPorSegundo, ThreadsControlerAviao* escreve);