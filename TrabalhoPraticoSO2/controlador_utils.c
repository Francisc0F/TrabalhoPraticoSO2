#include <tchar.h>
#include <windows.h>
#include "../utils.h"



void menuControlador() {
	_putws(TEXT("\n addAero <nome> <cordX> <cordY> - Adicionar aeroporto"));
	_putws(TEXT("lista - lista toda a informação do aeroporto"));
	_putws(TEXT("suspender ou ativar - aceitação de novos aviões por parte dos utilizadores"));
	_putws(TEXT("end - Encerrar sistema, notifica todos os processos"));
}


void adicionarAeroporto(TCHAR* nome, int x, int y, Aeroporto lista[]) {
	pAeroporto aux = NULL;
	for (int i = 0; i < MAXAEROPORTOS; i++) {
		if (_tcscmp(lista[i].nome, L"") == 0) {
			_tcscpy_s(lista[i].nome, _countof(lista[i].nome), nome);
			break;
		}
		else {
			break;
		}
	}
}

void listaTudo(Aeroporto lista[]) {
	for (int i = 0; i < MAXAEROPORTOS; i++) {
		if (_tcscmp(lista[i].nome, L"") != 0) {
			_tprintf(TEXT("Nome: [%s]\n"), lista[i].nome);
		}
	}
}

void inicializarLista(Aeroporto lista[]) {
	/*for (int i = 0; i < MAXAEROPORTOS; i++) {
		memset(&lista[i], 0, sizeof(lista[i]));
	}*/
}

//
//void pesquisaAeroporto(TCHAR nome, Aviao lista[]) {
//	for (int i = 0; i < MAXAVIOES; i++) {
//		if (lista[i] != NULL) {
//			_tcscpy(lista[i].nome, nome);
//		}
//		else {
//			break;
//		}
//	}
//}
//

