#include <tchar.h>
#include <windows.h>
#include "utils.h"

void printAviao(Aviao* aviao, TCHAR* out) {
	TCHAR aux[100];
	if (out != NULL) {
		_stprintf_s(aux, 100, L"\n id: [%d]\n IdAeroPorto atual: %d (%d, %d)\n n_passag: %d\n max_passag: %d\n posPorSegundo: %d\n",
			aviao->id, aviao->idAeroporto,aviao->x, aviao->y, aviao->n_passag, aviao->max_passag, aviao->posPorSegundo);
		_tcscat_s(out, 300, aux);
	}
	else {
		_tprintf(TEXT("\n id: [%d]\n IdAeroPorto atual: %d (%d, %d)\n n_passag: %d\n max_passag: %d\n posPorSegundo: %d\n"),
			aviao->id, aviao->idAeroporto, aviao->x, aviao->y, aviao->n_passag, aviao->max_passag, aviao->posPorSegundo);
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