#include <tchar.h>
#include <windows.h>

void menuAviao() {
    _putws(TEXT("\nprox <destino> - Proximo destino"));
    _putws(TEXT("emb - embarcar passageiros"));
    _putws(TEXT("init - iniciar viagem"));
    _putws(TEXT("quit - terminar instancia de aviao"));
}