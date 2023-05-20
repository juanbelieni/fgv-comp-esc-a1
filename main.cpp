#include <iostream>
#include <string>
#include <vector>
#include <cmath>

#include "ETL/ETL.hpp"

using std::cout;
using std::endl;

int main(int argc, char** argv) {
    // Argumentos: número de threads (mínimo 3) e tamanho da fila do serviço externo
    ETL etl(6, 10);
    std::vector<std::string> folders;
    // O comportamento padrão é verificar apenas a pasta data/
    if (argc == 1)
        folders.push_back("data/");
    
    for (int i = 1; i < argc; i++) {
        folders.push_back(argv[i]);
        folders.back() += "/";
    }
    etl.run(folders);
}
