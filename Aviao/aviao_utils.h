#pragma once



typedef struct {
	TCHAR* fileViewMap;
	HANDLE hSemEscrita;
	HANDLE hSemLeitura;
	HANDLE hEventEnviarMSG;

	HANDLE hMutex;

	int terminar;
	TCHAR info[100];
	int x;
	int y;
	int id;

	BufferCircular* memPar;
}MSGThread;

void menuAviao();

void printMSG(MSGcel cel);

void preparaLeituraMSGdoAviao(HANDLE* hFileMap, ThreadController* ler);

void preparaEnvioDeMensagensParaOControlador(HANDLE* hFileEscritaMap, MSGThread* escreve, BOOL* primeiroProcesso);
