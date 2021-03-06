#pragma once
#define MAXAEROPORTOS 100
#define MAXPASSAGEIROS 100
#define MAXAVIOES 100

#define SEMAPHORE_NUM_AVIOES "SEMAPHORE_NUM_AVIOES"
#define MAPA_PARTILHADO "MAPA_PARTILHADO"
#define MUTEX_MAPA_PARTILHADO "MUTEX_MAPA_PARTILHADO"

#define PINGTIMER "PINGTIMER"

#define FILE_MAP_MSG_TO_PLANES "FILE_MAP_MSG_TO_PLANES"
#define EVENT_MSG_TO_PLANES "EVENT_MSG_TO_PLANES"
#define MUTEX_MSG_TO_PLANES "MUTEX_MSG_TO_PLANES"

#define FILE_MAP_MSG_TO_CONTROLER "FILE_MAP_MSG_TO_CONTROLER"
#define SEMAPHORE_ESCRITA_MSG_TO_CONTROLER "SEMAPHORE_ESCRITA_MSG_TO_CONTROLER"
#define SEMAPHORE_LEITURA_MSG_TO_CONTROLER "SEMAPHORE_LEITURA_MSG_TO_CONTROLER"
#define MUTEX_PRODUTOR_MSG_TO_CONTROLER "MUTEX_PRODUTOR_MSG_TO_CONTROLER"

#define BITMAPAVIAO "aviao1.bmp"
#define BITMAPAERO "aeroporto.bmp"

#define TAM_BUFFER_CIRCULAR 100

#define PIPEPASSAG TEXT("\\\\.\\pipe\\passag")

typedef struct t AviaoMsg, * pAviaoMsg;
struct t {
	int id;
	char msg[100];
};

typedef struct c Aviao, * pAviao;
struct c {
	int id;
	int processId;
	int idAeroporto;

	int n_passag;
	int max_passag;
	int posPorSegundo;

	int segundosVivo;

	int proxDestinoId;
	int proxDestinoX;
	int proxDestinoY;
	int x;
	int y;
	int xBM;
	int yBM;
	int hover;

	int statusViagem;//  -1 = nao esta em viagem, != -1 esta em viagem
};

typedef struct x Aeroporto, * pAeroporto;
struct x {
	TCHAR nome[100];
	int id;
	int x;
	int y;
	int xBM;
	int yBM;
	int hover;
};

typedef struct e Passag, * pPassag;
struct e {
	int pid;
	TCHAR origem[100];
	TCHAR destino[100];
	TCHAR nome[100];
	int tempEspera;
};

typedef struct y MensagemPipe, * pMensagemPipe;
struct y {
	Passag autor;
	TCHAR mensagem[400];
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
	TCHAR ultimaMsg[500];
	int terminar;
}ControllerToPlane;


typedef struct {
	TCHAR info[100];
	int x;
	int y;
	int id;
	Aviao aviao;
}MSGcel;

typedef struct {
	int nAvioes;
	int posE;
	int posL;
	MSGcel buffer[TAM_BUFFER_CIRCULAR];
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
	int capacidadePassageiros;
	int posPorSegundo;
	int idAeroporto;
	int idAviao;
	BufferCircular* bufferPartilhado;
}MSGThread;


typedef struct {
	Aviao avioesMapa[MAXAVIOES];
	int numAtualAvioes;
}MapaPartilhado;

typedef struct {
	HANDLE hEventNovaViagem;
	MSGThread* escrita;
	int terminar;
	MapaPartilhado* MapaPartilhado;
	HANDLE *hMutexAcessoAMapaPartilhado;
	Aviao* aviaoMemLocal;
}ThreadGerirViagens;



void preparaStringdeCords(TCHAR* send, int x, int y);
void obterCordsDeString(TCHAR* cords, int* x, int* y);
void printAviao(Aviao* aviao, TCHAR* out);
int getAviao(int id, Aviao lista[]);

BOOL tokenValid(TCHAR* token);
BOOL isNumber(TCHAR* text);

void atualizaPosicaoAviao(Aviao* a, int x, int y);
void updateAviao(Aviao* a, int idAero, int statusViagem, int x, int y);
void updateAviaoCp(Aviao a, int idAero, int statusViagem, int x, int y);
void updateAeroportoAviao(Aviao * a, int idAero);
void apagaDoSistema(Aviao* a);
