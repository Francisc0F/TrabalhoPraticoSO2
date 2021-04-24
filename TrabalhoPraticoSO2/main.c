#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#define MAX 1000
#define MAXAVIOES 100
#define TAM 200
typedef int(__cdecl* MYPROC)(LPWSTR);

int _tmain(int argc, TCHAR* argv[]) {
	//UNICODE: Por defeito, a consola Windows não processa caracteres wide. 
	//A maneira mais fácil para ter esta funcionalidade é chamar _setmode:
#ifdef UNICODE 
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif
	// registry key
	
	TCHAR key_dir[TAM] = TEXT("Software\\TRABALHOSO2\\");
	//TCHAR key[TAM] = TEXT("Control");
	DWORD key_res;
	HKEY handle; // handle para chave depois de aberta ou criada
	DWORD handleRes;

	TCHAR key_name[TAM] = TEXT("N_avioes"); //nome do par-valor
	TCHAR key_value[TAM]; //nome do par-valor

	
	DWORD tamanho = sizeof(key_value);

	int maxAvioes;

	if (RegCreateKeyEx(
		HKEY_CURRENT_USER,
		key_dir,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&handle,
		&handleRes
	) != ERROR_SUCCESS) {
		_tprintf(TEXT("Chave N_avioes nao foi nem criada nem aberta.\n"));
		return -1;
	}

	if (handleRes == REG_CREATED_NEW_KEY)
		_tprintf(TEXT("Chave criada: %s\n"), key_dir);
	else
		_tprintf(TEXT("Chave aberta: %s\n"), key_dir);

	if (RegQueryValueEx(
		handle,
		key_name,
		0,
		NULL,
		(LPBYTE)key_value,
		&tamanho
	) != ERROR_SUCCESS) {
		_tprintf(TEXT("Numero de Avioes maximo nao esta definido.\n"));
		_tprintf(TEXT("Numero de Avioes maximo: "));
		_tscanf_s(TEXT("%s"), key_value, TAM);

		if (RegSetValueEx(
			handle,
			key_name,
			0,
			REG_SZ,
			(LPCBYTE)&key_value,
			sizeof(TCHAR) * (_tcslen(key_value) + 1) //lê o buffer todo
		) != ERROR_SUCCESS) {
			_tprintf(TEXT("O atributo nao foi alterado nem criado! ERRO! %s"), key_name);
		}

	}
	else {
		maxAvioes = atoi(key_value);
		_tprintf(TEXT("Atributo [%s] foi encontrado! value [%d]\n"), key_name, maxAvioes);
		_tprintf(TEXT("Max avioes: %d"), maxAvioes);
	}






	



	// dll
	//_tprintf(TEXT("Hello"));
	//TCHAR dll[MAX] = TEXT("C:\\Users\\Francisco\\source\\repos\\TrabalhoPraticoSO2\\SO2_TP_DLL_2021\\x64\\SO2_TP_DLL_2021.dll");
	//TCHAR dll[MAX] = TEXT("SO2_TP_DLL_2021.dll");
	//HINSTANCE hinstLib = NULL;
	//hinstLib = LoadLibrary(dll);

	//MYPROC ProcAdd = NULL;
	//BOOL fFreeResult, fRunTimeLinkSuccess = FALSE;





	//if (hinstLib != NULL)
	//{
	//	
	//	ProcAdd = (MYPROC)GetProcAddress(hinstLib, "move");

	//	// If the function address is valid, call the function.
	//	if (NULL != ProcAdd)
	//	{
	//		fRunTimeLinkSuccess = TRUE;
	//		int nextX = 15;
	//		int nextY = 10;
	//		/*int currX = 15;
	//		int currY = 10;*/
	//		int status = 1;
	//		while (status == 1) {
	//			//int move(int cur_x, int cur_y, int final_dest_x, int final_dest_y, int * next_x, int* next_y)
	//			status = (ProcAdd)(nextX, nextY, 50, 50, &nextX, &nextY);
	//			// status 1 mov correta, 2 erro, 0 chegou 
	//			_tprintf(TEXT("\nprev pos (%d,%d)  -> (%d,%d) status %d"), nextX, nextY, nextX, nextY, status);
	//		}
	//	}
	// //Free the DLL module.

	//	fFreeResult = FreeLibrary(hinstLib);
	//	_tprintf(TEXT("Free lib"));
	//}

	//unsigned int map[MAX][MAX] = { 0 };
	//unsigned int i;

}