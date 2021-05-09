#pragma once

#define DLL "SO2_TP_DLL_2021.dll"


void menuAviao();
void printMSG(MSGcel cel);
void preparaLeituraMSGdoAviao(HANDLE* hFileMap, ThreadController* ler);
void preparaEnvioDeMensagensParaOControlador(HANDLE* hFileEscritaMap, MSGThread* escreve, BOOL* primeiroProcesso);
void enviarMensagemParaControlador(MSGThread* escreve, TCHAR* info);
void setupAviao(int* capacidadePassageiros, int* posPorSegundo);