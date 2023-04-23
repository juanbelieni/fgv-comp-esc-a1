#ifndef ETL_HPP_
#define ETL_HPP_

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "./external.hpp"
#include "./conversions.hpp"

class ETL {
    static const int buffer_size = 65536;
    static const int default_map_size = 4096;
    static const int n_files = 5;
    
 public:
    struct Position {
        int lane;
        int distance;
        int cycle;

        Position(int lane, int distance, int cycle) : lane(lane), distance(distance), cycle(cycle) {}
    };

    // Armazena os dados instantâneos mais recentes de um veículo para o cálculo do risco
    struct Vehicle {
        std::string name;
        std::string model;
        int year = -1;
        // Velocidade do veículo em unidades deconhecidas de distância (talvez carro) por segundo
        float speed;
        float acceleration;
        // Domínio: [0, 1]
        float risk;
    };

    // Armazena as posições de um veículo em diferentes instantes, incluindo deslocamento e sua via
    struct VehicleData {
        std::vector<Position> positions;
        Vehicle vehicle;
    };

    ETL(int n_threads, int external_queue_size) : n_threads(n_threads), service(external_queue_size) {
        // O serviço deve ser inicializado junto da classe atual, pois ele não possui construtor padrão
        vehicles.reserve(default_map_size);
        threads.reserve(n_threads);
        modified.reserve(n_threads);
        data = new char[buffer_size];
    }

    ~ETL() {
        delete[] data;
    }

    void set_thread_count(int n_threads) {
        this->n_threads = n_threads;
    }

    void run(const std::vector<std::string>& folders) {
        if (n_threads < 3)
            throw std::runtime_error("Número de threads deve ser maior que ou igual a 3");

        // Para facilitar o uso da variável, subtrai duas threads que serão dedicadas ao dashboard
        n_threads -= 2;
        const std::string path_end = ".csv";
        const std::string temp_end = ".tmp";

        std::ifstream file;
        // Usa um índice para identificar o próximo arquivo a ser lido para a etapa de
        // Extract do ETL, que é incrementado a cada ciclo de simulação e retorna ao 0
        // após atingir uma constante para evitar que muitos arquivos sejam criados
        std::vector<int> file_indices(folders.size(), 0);
        cycle_times.resize(folders.size());
        bool first_time = true;

        // Procura arquivos para processar indefinidamente
        while (true) {
            // Define o caminho do próximo arquivo com base num índice que é incrementado
            // Antes de abrir o arquivo, espera até que um arquivo temporário seja criado,
            // o que indica que o arquivo está totalmente preenchido com os dados
            int folder = 0;
            std::string path;
            std::string temp_path;
            while (true) {
                // Verifica em cada uma das pastas analisadas se o arquivo existe
                temp_path = folders[folder] + std::to_string(file_indices[folder]) + temp_end;
                if (std::filesystem::exists(temp_path)) {
                    // Ao encontrar um arquivo para análise, começa o ETL
                    path = folders[folder] + std::to_string(file_indices[folder]) + path_end;
                    break;
                }

                folder = (folder + 1) % folders.size();
            }
            highway = folder;

            file.open(path);
            file.getline(data, buffer_size);
            // Obtém o ponto de início dos dados, que é depois do header do CSV
            int start = file.tellg();
            int index = 0;
            // Obtém o número do ciclo de simulação atual
            cycle = str_to_int(data, index, ',');
            // Salva o instante de tempo em que o ciclo foi computado
            cycle_times[highway][cycle] = str_to_double(data, index, ',');
            int n_lanes = str_to_int(data, index, ',');
            int extension = str_to_int(data, index, ',');
            int max_speed = str_to_int(data, index, '\n');
            // Lê o restante dos dados do arquivo, deixando o primeiro e o último caracteres
            // do buffer como '\n' para facilitar verificações mais à frente
            file.read(data + 1, buffer_size - 2);
            int total_characters = file.gcount();
            file.close();

            data[0] = '\n';
            data[buffer_size - 1] = '\n';

            // Apaga o arquivo temporário
            std::filesystem::remove(temp_path);

            threads_working = n_threads + 1;
            int new_plates = 0;
            int chunk_size = total_characters / n_threads;
            std::unique_lock<std::mutex> lock(mutex);
            for (int i = 0; i < n_threads; i++) {
                // Não há necessidade de chamar o destrutor das placas, pois elas não possuem
                // memória dinâmica, então basta "redimensionar" o vetor para 0 antes de começar
                modified[i].resize(0);
                threads.emplace_back(&ETL::extract, this, i,
                    start + i * chunk_size, start + (i + 1) * chunk_size, n_lanes, &new_plates);
            }
            // Espera até que todas as threads tenham lido a quantidade de novas linhas do arquivo,
            // pois criar todas as threads duas vezes para o extract seria muito custoso
            cv.wait(lock, [this] { return is_only_main_working(); });

            // Calcula o novo tamanho da tabela de veículos e redimensiona se necessário
            int new_size = vehicles.size() + new_plates;
            int capacity = vehicles.bucket_count();
            while (capacity < 3 * new_size / 4)
                capacity *= 2;
            if (capacity > vehicles.bucket_count())
                vehicles.reserve(capacity);
            
            threads_working = 0;
            lock.unlock();
            cv.notify_all();
            
            // Espera até que todas as threads terminem esta etapa
            for (auto& thread : threads)
                thread.join();
            threads.clear();

            for (int i = 0; i < n_threads; i++)
                threads.emplace_back(&ETL::transform, this, i, max_speed);

            for (auto& thread : threads)
                thread.join();
            threads.clear();

            int vehicle_count = 0;
            for (int i = 0; i < n_threads; ++i)
                vehicle_count += modified[i].size();
            
            // Previne a execução da thread que atualiza os dados no terminal
            std::lock_guard<std::mutex> load_lock(load_mutex);
            if (first_time) {
                std::thread dashboard(&ETL::load, this);
                dashboard.detach();
                std::thread input(&ETL::input, this);
                first_time = false;
            }

            file_indices[highway] = (file_indices[highway] + 1) % n_files;
        }
    }

 private:
    // Mapeia a placa para os dados do veículo
    std::unordered_map<Plate, VehicleData, Hash<Plate>> vehicles;
    std::condition_variable cv;
    // Mutex "principal" que vai impedir concorrência entre as threads em E e T
    std::mutex mutex;
    // Mutex que vai funcionar como indicador booleano para o processamento do dashboard
    std::mutex load_mutex;
    // Serviço que ainda está me assombrando
    SlowService service;
    // Armazena as placas que sofreram modificação em cada ciclo
    std::vector<std::vector<Plate>> modified;
    std::vector<std::thread> threads;
    // Armazena o instante de tempo de realização de cada ciclo da simulação
    std::vector<std::vector<double>> cycle_times;
    // Dados que serão lidos do arquivo de texto
    char* data;
    // Número atual de threads trabalhando, usado para sincronização
    int threads_working;
    // Gostaria de fazer com que isso fosse dinâmico, se sobrar tempo
    int n_threads;
    // Rodovia que está sendo processada atualmente
    int highway;
    // Número do ciclo de simulação atual
    int cycle;

    bool is_all_done() {
        return threads_working == 0;
    }

    bool is_only_main_working() {
        return threads_working == 1;
    }

    void find_next_line(int& i) {
        while (data[i] != '\n')
            i++;
        i++;
    }

    void extract(int thread_num, int start, int end, int n_lanes, int* sum) {
        // Começa sempre em uma nova linha, assim a divisão de start e end não é
        // exatamente a usada e garantimos que todas as linhas são verificadas por completo
        if (data[start - 1] != '\n')
            find_next_line(start);

        int i = start;
        int count = 0;
        auto none = vehicles.end();
        while (i < end) {
            Plate plate(data + i);
            i += 8;
            // Se a placa ainda não está registrada, incrementa o contador para preparar
            // a estrutura de dados para receber o número correto de placas
            auto it = vehicles.find(plate);
            count += (it == none);
            find_next_line(i);
        }
        // Espera até que todas as threads tenham terminado de contar as linhas
        std::unique_lock<std::mutex> lock(mutex);
        *sum += count;
        threads_working--;
        // Gostaria de notificar apenas a thread principal, mas o notify one não preserva a ordem
        cv.notify_all();
        // Espera até que o map tenha sido expandido
        cv.wait(lock, [this] { return is_all_done(); });
        lock.unlock();

        i = start;
        while (i < end) {
            Plate plate(data + i);
            int i = 8;

            // No código abaixo, como não podemos ter mais de uma thread tentando acessar a
            // mesma placa, o único problema que pode ocorrer é a placa não estar registrada e
            // ser inserida no mesmo espaço de memória em que outra placa que está sendo inserida
            VehicleData* current;
            auto it = vehicles.find(plate);
            // Se a placa ainda não está registrada, cria seu registro sem condição de corrida
            if (it == none) {
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
            int distance = str_to_int(data, i, '\n');
            current->positions.emplace_back(lane, distance, cycle);
            modified[thread_num].push_back(plate);
            i++;

            // Tentamos obter as informações do veículo pelo serviço externo lento
            if (service.query_vehicle(plate)) {
                current->vehicle.name = service.get_name();
                current->vehicle.model = service.get_model();
                current->vehicle.year = service.get_year();
            }
        }
    }

    void transform(int thread_num, int max_speed) {
        for (auto& plate : modified[thread_num]) {
            VehicleData* current = &vehicles[plate];
            Vehicle* car = &current->vehicle;
            // Se as informações não foram adquiridas antes, tenta novamente agora
            if (car->year < 0 && service.query_vehicle(plate)) {
                car->name = service.get_name();
                car->model = service.get_model();
                car->year = service.get_year();
            }

            std::vector<Position>& positions = current->positions;
            // Efetua o cálculo da velocidade e aceleração apenas se houver mais de uma posição
            if (positions.size() > 1) {
                float prev_speed = car->speed;

                int last = positions.size() - 1;
                int distance = positions[last].distance - positions[last - 1].distance;

                // Calcula velocidade como deslocamento dividido por tempo decorrido
                car->speed = static_cast<float>(distance) /
                    (cycle_times[highway][positions[last].cycle] - cycle_times[highway][positions[last - 1].cycle]);
                
                if (positions.size() > 2) {
                    float prev_acceleration = car->acceleration;
                    // Calcula aceleração como velocidade dividida por tempo decorrido
                    car->acceleration = (car->speed - prev_speed) /
                        (cycle_times[highway][positions[last].cycle] - cycle_times[highway][positions[last - 1].cycle]);
                    
                    if (positions.size() > 3) {
                        // float time_to_collision = -(current->speed + prev_speed) /
                        //                             (current->acceleration + prev_acceleration);
                        // current->risk = time_to_collision / (prev_speed + prev_acceleration);
                        float x = (car->speed + car->speed * std::abs(car->acceleration)) / max_speed - 5;
                        car->risk = 1.0f / (1.0f + std::exp(-x));
                    } else {
                        car->risk = -1.0f;
                    }
                } else {
                    car->acceleration = -1.0f;
                    car->risk = -1.0f;    
                }
            // Se não há dados suficientes, define como valores negativos para que possam ser
            // descartados facilmente na análise posterior
            } else {
                car->speed = -1.0f;
                car->acceleration = -1.0f;
                car->risk = -1.0f;
            }
        }
    }

    void load() {
        std::unique_lock<std::mutex> lock(load_mutex);
    }

    void input() {

    }

    /// Usado para computar a diferença de tempo entre a simulação e a chegada no dashboard.
    double now() {
        unsigned long nanoseconds = std::chrono::system_clock::now().time_since_epoch().count();
        // Converte para segundos
        return static_cast<double>(nanoseconds) / 1e9;
    }
};

#endif  // ETL_HPP_
