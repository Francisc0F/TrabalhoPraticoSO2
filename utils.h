#pragma once
#define MAXAVIOESAEROPORTO 100
#define MAXAEROPORTOS 100
#define MAXAVIOES 100
#define	READMAP "READMAP"

#define FILE_MAP_MSG_TO_PLANES "FILE_MAP_MSG_TO_PLANES"
#define EVENT_MSG_TO_PLANES "EVENT_MSG_TO_PLANES"
#define MUTEX_MSG_TO_PLANES "MUTEX_MSG_TO_PLANES"
#define NUM_CHAR_FILE_MAP 200

#define FILE_MAP_MSG_TO_CONTROLER "FILE_MAP_MSG_TO_CONTROLER"
#define SEMAPHORE_ESCRITA_MSG_TO_CONTROLER "SEMAPHORE_ESCRITA_MSG_TO_CONTROLER"
#define SEMAPHORE_LEITURA_MSG_TO_CONTROLER "SEMAPHORE_LEITURA_MSG_TO_CONTROLER"
#define MUTEX_PRODUTOR_MSG_TO_CONTROLER "MUTEX_PRODUTOR_MSG_TO_CONTROLER"
#define MUTEX_CONSUMIDOR_MSG_TO_CONTROLER "MUTEX_CONSUMIDOR_MSG_TO_CONTROLER"

#define TAM_BUFFER 20

typedef struct t AviaoMsg, * pAviaoMsg;
struct t {
	int id;
	char msg[100];
};

typedef struct c Aviao, * pAviao;
struct c {
	int id;
	int n_passag;
};

typedef struct x Aeroporto, * pAeroporto;
struct x {
	TCHAR nome[100];
	int x;
	int y;
	Aviao listaAvioes[MAXAVIOESAEROPORTO];
};


typedef struct {
	TCHAR info[500];
	int idAviao;
}MSGCtrlToPlane;


typedef struct {
	MSGCtrlToPlane* fileViewMap;
	MSGCtrlToPlane msgToSend;
	HANDLE hEvent;
	HANDLE hEventOrdemDeEscrever;
	HANDLE hMutex;
	int terminar;
}ControllerToPlane;


typedef struct {
	TCHAR info[100];
	int x;
	int y;
	int id;
}MSGcel;

typedef struct {
	int nProdutores;
	int nConsumidores;
	int posE; //proxima posicao de escrita
	int posL; //proxima posicao de leitura
	MSGcel buffer[TAM_BUFFER]; //buffer circular em si (array de estruturas)
}BufferCircular;




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



typedef struct {
	MSGThread* leitura;
	ControllerToPlane * escrita;
	Aeroporto* listaAeroportos;
}ThreadsControler; 
