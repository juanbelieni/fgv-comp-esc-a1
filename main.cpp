#include "ETL/Temporary.hpp"

using std::cout;
using std::endl;
using std::string;
using std::vector;

/*
 *  TODO:
    - pedir e implementar a verificação de um inteiro no header do arquivo que indica
 *  o número de novas placas por ciclo, assim a hash table pode ser expandida para um
 *  tamanho suficiente antes que o ETL comece a rodar e evitamos muita dor de cabeça.
 * 
 *  - Chegar em um consenso sobre o tamanho da placa.
 * 
 *  Também não testei nada do que foi escrito, então tenha paciência, mas reclame à vontade.
 */

int main() {
    ETL etl(4);
    // Espera um enter
    std::cin.get();
    etl.run();
}
