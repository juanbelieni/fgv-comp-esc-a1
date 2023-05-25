#include <iostream>
#include <fstream>

#include "ETL/ETL.hpp"

int main(int argc, char** argv) {
    int num_runs = 10;
    int sleep_seconds = 30;
    if (argc > 1)
        num_runs = std::atoi(argv[1]);
    if (argc > 2)
        sleep_seconds = std::atoi(argv[2]);

    std::ofstream file("results.csv");
    // Parâmetros: número de threads (mínimo 5) e tamanho da fila do serviço externo
    ETL etl(10, 5);
    std::thread etl_thread(&ETL::run, &etl, 0.0);

    for (int i = 0; i < num_runs; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
        file << etl.summary() << '\n';
    }
    file.close();

    // Nunca vai acontecer, não conseguimos parar o servidor sem usar ctrl + c
    etl_thread.join();
}
