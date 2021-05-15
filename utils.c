#include <tchar.h>
#include <windows.h>
#include "utils.h"

int getAviao(int id, Aviao lista[]) {
	for (int i = 0; i < MAXAVIOES; i++) {
		if (lista[i].id == id) {
			return i;
		}
	}
	return -1;
}

void printAviao(Aviao* aviao, TCHAR* out) {
	TCHAR* txt = TEXT("id: [%d] Em sistema à %d secs\nIdAeroPorto atual: %d (%d, %d)\n n_passag: %d\n max_passag: %d\n posPorSegundo: %d\n");
	TCHAR* txtEmViagem = TEXT("id: [%d] Em sistema à %d secs\n Em viagem (x:%d, y:%d)\n posPorSegundo: %d\n");
	TCHAR aux[100];
	if (out != NULL) {
		if (aviao->idAeroporto == -1) {
			_stprintf_s(aux, 100, txtEmViagem,
				aviao->id,aviao->segundosVivo, aviao->x, aviao->y, aviao->posPorSegundo);
		}
		else {
			_stprintf_s(aux, 100, txt,
				aviao->id, aviao->segundosVivo, aviao->idAeroporto, aviao->x, aviao->y, aviao->n_passag, aviao->max_passag, aviao->posPorSegundo);
		}
		_tcscat_s(out, 300, aux);
	}
	else {
		if (aviao->idAeroporto == -1) {
			_tprintf(txtEmViagem,
				aviao->id, aviao->segundosVivo, aviao->x, aviao->y, aviao->posPorSegundo);
		}
		else {
			_tprintf(txt,
			aviao->id, aviao->segundosVivo, aviao->idAeroporto, aviao->x, aviao->y, aviao->n_passag, aviao->max_passag, aviao->posPorSegundo);
		}
		
	}
}


void preparaStringdeCords(TCHAR* send, int x, int y) {
	TCHAR cordX[100] = { 0 };
	TCHAR cordY[100] = { 0 };

	_itot_s(x, cordX, _countof(cordX), 10);
	_itot_s(y, cordY, _countof(cordY), 10);

	_tcscat_s(send, 100, cordX);
	_tcscat_s(send, 100, TEXT(" "));
	_tcscat_s(send, 100, cordY);
	send[_tcslen(send)] = '\0';
}

void obterCordsDeString(TCHAR* msg, int *x, int *y) {
	TCHAR cords[20];
	_tcscpy_s(cords, _countof(cords), msg);

	TCHAR delim[] = L" ";
	TCHAR* cordY = NULL;
	TCHAR* cordX = _tcstok_s(cords, delim, &cordY);
	*x = _tstoi(cords);
	*y = _tstoi(cordY);
}


BOOL tokenValid(TCHAR* token) {
	return token != NULL && _tcscmp(token, L"") > 0 && token != L"\0" && _tcscmp(token, L" ") > 0;
}

BOOL isNumber(TCHAR* text){
	int j;
	j = _tcslen(text);
	while (j--)
	{
		if (text[j] > 47 && text[j] < 58)
			continue;

		return 0;
	}
	return 1;
}
