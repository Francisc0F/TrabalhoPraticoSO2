#include <tchar.h>
#include <windows.h>

void menuControlador(){
    _putws(TEXT("\n addAero - Adicionar aeroporto"));
    _putws(TEXT("lista - lista toda a informação do aeroporto"));
    _putws(TEXT("suspender ou ativar - aceitação de novos aviões por parte dos utilizadores"));
    _putws(TEXT("end - Encerrar sistema, notifica todos os processos"));
}

