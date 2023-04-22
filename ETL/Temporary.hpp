#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Observação: sim, eu sei que existem funções que fazem isso e fiz o benchmark,
//              então sei que escrever as minhas dá mais controle e é mais rápido.

/**
 *  @brief Retorna a representação em inteiro de uma string.
 *  
 *  @param str O array de caracteres terminado em '\n' (por padrão) a ser convertido.
 *  @param index O índice do primeiro caractere.
 *  @param end O caractere que termina a parte convertida da string.
 *  @return O inteiro representado pela string.
 */
inline int str_to_int(const char* str, int& index, char end = '\n') {
    int result = 0;
    bool negative = false;
    if (str[0] == '-') {
        negative = true;
        index++;
    }
    for (; str[index] != end; index++) {
        result *= 10;
        result += str[index] - '0';
    }
    return negative ? -result : result;
}

/**
 *  @brief Retorna a representação em float de uma string.
 *  
 *  @param str O array de caracteres terminado em '\n' a ser convertido.
 *  @param index O índice do primeiro caractere.
 *  @param end O caractere que termina a parte convertida da string.
 *  @return O double representado pela string.
 */
inline double str_to_double(const char* str, int& index, char end = '\n') {
    int integer = 0;
    bool negative = false;
    if (str[0] == '-') {
        negative = true;
        index++;
    }
    for (; str[index] != '.' && str[index] != end; index++) {
        integer *= 10;
        integer += str[index] - '0';
    }
    double result = integer;
    if (str[index] == end)
        return result;

    int divisor = 1;
    for (++index; str[index] != end; index++) {
        divisor *= 10;
        if (str[index] == '0')
            continue;
        result += static_cast<double>(str[index] - '0') / divisor;
    }
    return negative ? -result : result;
}



// Cria um hash para a estrutura Plate reinterpretando o array de char como um inteiro
template<typename T>
struct Hash {
    std::size_t operator()(const T& plate) const noexcept {
        return *reinterpret_cast<const size_t*>(plate.plate);
    }
};

class ETL {
    static const int buffer_size = 4096;
    static const int default_map_size = 1024;
    static const int n_files = 5;
    static const int n_lanes = 8;
    static const int lane_size = 200;
    
 public:
    struct Position {
        int lane;
        int distance;
        int cycle;

        Position(int lane, int distance, int cycle) : lane(lane), distance(distance), cycle(cycle) {}
    };

    // Armazena os dados instantâneos mais recentes de um veículo para o cálculo do risco e
    // a posição desse veículo em diferentes instantes, incluindo deslocamento e sua via.
    struct Data {
        std::vector<Position> positions;
        float velocity;
        float acceleration;
        float risk;
    };

    // Uma placa tem 7 caracteres, mas o compilador usaria 8 de qualquer jeito, então
    // é mais fácil alocar 8 bytes e usar o último para o '\0', assim o cout funciona
    struct Plate {
        char plate[8]{};  // 7 caracteres + '\0'

        bool operator==(const Plate& other) const {
            for (int i = 0; i < 7; i++) {
                if (plate[i] != other.plate[i]) {
                    return false;
                }
            }
            return true;
        }
    };

    ETL(int n_threads) : n_threads(n_threads) {
        vehicles.reserve(default_map_size);
        cycle_times.reserve(512);
        threads.reserve(n_threads);
        modified.reserve(n_threads);
    }

    ~ETL() {
        delete[] data;
    }

    void run() {
        const std::string path_start = "data/";
        const std::string path_end = ".csv";
        const std::string temp_end = ".tmp";

        // TODO: decidir qual é o melhor jeito de calcular risco, talvez usando uma "matriz":
        // std::vector<std::vector<int>> pos(n_lanes, std::vector<int>(lane_size, 0));

        std::ifstream file;
        // Usa um índice para identificar o próximo arquivo a ser lido para a etapa de
        // Extract do ETL, que é incrementado a cada ciclo de simulação e retorna ao 0
        // após atingir uma constante para evitar que muitos arquivos sejam criados
        int n = 0;
        while (true) {
            // Define o caminho do próximo arquivo com base num índice que é incrementado
            // Antes de abrir o arquivo, espera até que um arquivo temporário seja criado,
            // o que indica que o arquivo está totalmente preenchido com os dados
            std::string temp_path = path_start + std::to_string(n) + temp_end;
            while (!std::filesystem::exists(temp_path));
            std::string path = path_start + std::to_string(n) + path_end;

            file.open(path);
            file.getline(data, buffer_size);
            // Obtém o ponto de início dos dados, que é depois do header do CSV
            int start = file.tellg();
            int index = 0;
            // Obtém o número do ciclo de simulação atual
            int new_cycle = str_to_int(data, index, ',');
            // Salva o instante de tempo em que o ciclo foi computado
            cycle_times[new_cycle] = str_to_double(data, index);

            if (new_cycle - cycle > 1) {
                std::cout << "Lento demais!" << std::endl;
                std::cout << "Ciclo " << cycle << " demorou " << \
                    cycle_times[new_cycle] - cycle_times[cycle] << " segundos" << std::endl;
            }
            cycle = new_cycle;
            // Lê o restante dos dados do arquivo, deixando o primeiro e o último caracteres
            // do buffer como '\n' para facilitar verificações mais à frente
            file.read(data + 1, buffer_size - 2);
            int total_characters = file.gcount();
            file.close();

            data[0] = '\n';
            data[buffer_size - 1] = '\n';

            // Apaga o arquivo temporário
            std::filesystem::remove(temp_path);

            int chunk_size = total_characters / n_threads;
            for (int i = 0; i < n_threads; i++) {
                threads.emplace_back(&ETL::extract, this, i,
                    start + i * chunk_size, start + (i + 1) * chunk_size);
            }
            // Espera até que todas as threads terminem esta etapa
            for (auto& thread : threads)
                thread.join();

            // TODO: continuar Load aqui, fazer uma função pra cada uma das três etapas

            n = (n + 1) % n_files;
        }
    }

 private:
    // Dados que serão lidos do arquivo de texto
    char* data = new char[buffer_size];
    // Mapeia a placa para os dados do veículo
    std::unordered_map<Plate, Data, Hash<Plate>> vehicles;
    // Armazena as placas que sofreram modificação em cada ciclo
    std::vector<std::vector<Plate>> modified;
    std::vector<std::thread> threads;
    // Armazena o instante de tempo de realização de cada ciclo da simulação
    std::vector<double> cycle_times;
    std::mutex mutex;
    // Gostaria de fazer com que isso fosse dinâmico, se sobrar tempo
    int n_threads;
    int cycle;

    void extract(int thread_num, int start, int end) {
        int i = start;
        // Começa sempre em uma nova linha, assim a divisão de start e end não é
        // exatamente a usada e garantimos que todas as linhas são verificadas por completo
        if (data[i - 1] != '\n') {
            while (data[i] != '\n')
                i++;
            i++;
        }

        while (i < end) {
            Plate plate;
            int i = 0;
            for (; data[i] != ','; i++)
                plate.plate[i] = data[i];
            i++;

            // No código abaixo, como não podemos ter mais de uma thread tentando acessar a
            // mesma placa, o único problema que pode ocorrer é a placa não estar registrada e
            // ser inserida no mesmo espaço de memória em que outra placa que está sendo inserida
            Data* current;
            auto it = vehicles.find(plate);
            // Se a placa ainda não está registrada, cria seu registro sem condição de corrida
            if (it == vehicles.end()) {
                std::lock_guard<std::mutex> lock(mutex);
                current = &vehicles[plate];
            // Se a placa já está registrada, obtém um ponteiro para seus dados e os atualiza
            } else {
                current = &it->second;
            }
            
            // Transforma os dois índices da via em um só para facilitar acesso ao array
            int lane = (data[i] - '0') * n_lanes / 2;
            i += 2;
            lane += data[i] - '0';
            i += 2;
            int distance = str_to_int(data, i);
            current->positions.emplace_back(lane, distance, cycle);
            modified[thread_num].push_back(plate);
        }
    }
};

std::ostream &operator<<(std::ostream &os, const ETL::Plate &plate) {
    os << plate.plate;
    return os;
}
