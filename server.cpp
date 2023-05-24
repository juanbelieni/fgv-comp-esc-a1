#include <iostream>
#include <fstream>

#include "ETL/ETL.hpp"

int main(int argc, char** argv) {
    int num_runs = 50;
    if (argc != 1)
        num_runs = std::atoi(argv[1]);

    std::vector<double> results(num_runs);

    for (int i = 0; i < num_runs; i++) {
        // Argumentos: número de threads (mínimo 5) e tamanho da fila do serviço externo
        // O argumento para run é o tempo até que o servidor seja interrompido
        ETL etl(10, 5);
        etl.run(0);
        results.push_back(etl.summary());
    }

    std::ofstream file;
    file.open("results.csv");
    file << "instances,avg_time\n";
    for (int i = 0; i < results.size(); ++i)
        file << (i + 1) << ',' << results[i] << '\n';
    file.close();
}
