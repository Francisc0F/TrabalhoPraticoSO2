#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#define MAX 256



int _tmain(int argc, LPTSTR argv[]) {
	//UNICODE: Por defeito, a consola Windows não processa caracteres wide. 
	//A maneira mais fácil para ter esta funcionalidade é chamar _setmode:
#ifdef UNICODE 
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
#endif


	TCHAR str[MAX], result[MAX] = TEXT("Olá! Aviao\n");
	unsigned int i;

	do {
		_tprintf(result);
		fflush(stdin);
		_fgetts(str, MAX, stdin);
		//Retirar \n
		str[_tcslen(str) - 1] = '\0';
		//Maiúsculas
		for (i = 0; i < _tcslen(str); i++)
			str[i] = _totupper(str[i]);
		_stprintf_s(result, MAX, TEXT("Frase:%s, Tamanho:%d\n"), str, _tcslen(str));
	} while (_tcsicmp(TEXT("FIM"), str));
	return 0;
}