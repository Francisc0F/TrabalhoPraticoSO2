#pragma once
#define MAXAVIOESAEROPORTO 100
#define MAXAEROPORTOS 100
#define MAXAVIOES 100
#define	READMAP "READMAP"

#define FILE_MAP_MESSEGER_TO_PLANES "MESSEGER_TO_PLANES"
#define EVENT_MESSEGER_TO_PLANES "EVENT_MESSEGER_TO_PLANES"
#define MUTEX_MESSEGER_TO_PLANES "MUTEX_MESSEGER_TO_PLANES"
#define NUM_CHAR_FILE_MAP 200

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
	Aviao listaAvioes[MAXAVIOESAEROPORTO];
};


typedef struct {
	TCHAR* fileViewMap;
	HANDLE hEvent;
	HANDLE hMutex;
	int terminar;
}ThreadController;

