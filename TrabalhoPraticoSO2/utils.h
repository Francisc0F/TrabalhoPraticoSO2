#pragma once
typedef struct t AviaoMsg, * AviaoMsg;
struct t {
	int id;
	char msg[100];
};


typedef struct c Aviao, * Aviao;
struct c {
	int id;
	int n_passag;

};