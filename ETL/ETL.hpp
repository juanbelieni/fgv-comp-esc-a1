#ifndef ETL_HPP_
#define ETL_HPP_

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale.h>
#include <ncurses.h>
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
    };

    // Armazena os dados instantâneos mais recentes de um veículo para o cálculo do risco
    struct Vehicle {
        std::string name;
        std::string model;
        int year = -1;

        Position last_pos;
        // Velocidade do veículo em unidades deconhecidas de distância (talvez carro) por segundo
        float speed;
        float acceleration;
        // Domínio: [0, 1]
        float risk;
        bool flags[3];
    };

    // Armazena as posições de um veículo em diferentes instantes, incluindo deslocamento e sua faixa
    struct VehicleData {
        std::vector<Position> positions;
        Vehicle vehicle;
    };

    ETL(int n_threads, int external_queue_size) : n_threads(n_threads), service(external_queue_size) {
        // O serviço deve ser inicializado junto da classe atual, pois ele não possui construtor padrão
        vehicles.reserve(default_map_size);
        data = new char[buffer_size];
        current_filter = ALL;
        current_absolute_value = 1;
        current_vehicle = std::make_pair(0, 0);
        should_exit = false;
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

        // Para facilitar o uso da variável, subtrai uma thread que será dedicada ao dashboard
        n_threads -= 2;
        const std::string path_end = ".csv";
        const std::string temp_end = ".tmp";

        std::ifstream file;
        // Usa um índice para identificar o próximo arquivo a ser lido para a etapa de
        // Extract do ETL, que é incrementado a cada ciclo de simulação e retorna ao 0
        // após atingir uma constante para evitar que muitos arquivos sejam criados
        std::vector<int> file_indices(folders.size(), 0);
        cycle_times.resize(folders.size());
        double now_ = now();
        for (auto& times : cycle_times) {
            times.resize(default_map_size / 4);  // valor arbitrário
            times.push_back(now_);
        }
        bool first_time = true;

        // Procura arquivos para processar indefinidamente
        while (true) {
            // Define o caminho do próximo arquivo com base num índice que é incrementado
            // Antes de abrir o arquivo, espera até que um arquivo temporário seja criado,
            // o que indica que o arquivo está totalmente preenchido com os dados
            int folder = 0;
            std::string path;
            std::string temp_path;
            std::cout << "Procurando um arquivo para iniciar..." << std::endl;

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
            int index = 0;
            // Obtém o número do ciclo de simulação atual e soma um para poder acessar o elemento
            // anterior mesmo quando o índice for 0
            cycle = str_to_int(data, index, ' ') + 1;
            // Salva o instante de tempo em que o ciclo foi computado
            cycle_times[highway][cycle] = str_to_double(data, index, ' ');
            int n_lanes = str_to_int(data, index, ' ');
            int extension = str_to_int(data, index, ' ');
            // Obtém a velocidade em unidades de deslocamento e converte para float
            float max_speed = str_to_int(data, index, '\n');
            // Coloca a velocidade máxima na mesma escala da velocidade computada para os veículos
            // max_speed /= cycle_times[highway][cycle] - cycle_times[highway][cycle - 1];
            // Lê o restante dos dados do arquivo, deixando o primeiro e o último caracteres
            // do buffer como '\n' para facilitar verificações mais à frente
            file.read(data + 1, buffer_size - 2);
            int total_characters = file.gcount();
            file.close();

            data[0] = '\n';
            data[total_characters + 1] = '\n';

            // Apaga o arquivo temporário
            std::filesystem::remove(temp_path);

            threads.resize(0);
            modified.resize(n_threads);
            new_processed.resize(n_threads);
            threads_working = n_threads + 1;
            int new_plates = 0;
            int chunk_size = total_characters / n_threads;
            std::unique_lock<std::mutex> lock(mutex);
            for (int i = 0; i < n_threads; i++) {
                // Não há necessidade de chamar o destrutor das placas, pois elas não possuem
                // memória dinâmica, então basta "redimensionar" o vetor para 0 antes de começar
                modified[i].resize(0);
                threads.emplace_back(&ETL::extract, this, i,
                    1 + i * chunk_size, 1 + (i + 1) * chunk_size,
                    n_lanes, max_speed, &new_plates);
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
            
            // Espera até que todas as threads terminem a extração
            for (auto& thread : threads)
                thread.join();
            threads.clear();
            lock.lock();

            vehicle_counts[ALL] = 0;
            vehicle_counts[COLLISION_RISK] = 0;
            vehicle_counts[ABOVE_SPEED_LIMIT] = 0;
            threads_working = n_threads + 1;
            for (int i = 0; i < n_threads; i++)
                threads.emplace_back(&ETL::transform, this, i, max_speed);
            
            // Espera até que todos os dados processados estejam prontos
            cv.wait(lock, [this] { return is_only_main_working(); });

            // Impede que o dashboard seja atualizado
            std::unique_lock<std::mutex> load_lock(load_mutex);

            // Se o usuário tentou encerrar, encerra o programa
            int stop = should_exit;

            info.num_vehicles[ALL] = vehicle_counts[ALL];
            info.num_vehicles[COLLISION_RISK] = vehicle_counts[COLLISION_RISK];
            info.num_vehicles[ABOVE_SPEED_LIMIT] = vehicle_counts[ABOVE_SPEED_LIMIT];
            info.num_lanes = n_lanes;
            info.time_elapsed = now() - cycle_times[highway][cycle];
            // Move os dados para um vetor que será usado pelo dashboard
            auto temp = std::move(processed);
            processed = std::move(new_processed);
            new_processed = std::move(temp);

            // Se é a primeira execução, inicia a thread que estará em execução independente
            if (first_time) {
                setlocale(LC_ALL, "");
                initscr();
                noecho();
                keypad(stdscr, true);
                std::thread dashboard(&ETL::load, this);
                dashboard.detach();
                first_time = false;
            }

            // Notifica as threads para que elas obtenham os dados não prioritários
            threads_working = 0;
            lock.unlock();
            cv.notify_all();

            should_draw = true;
            update_filter(current_filter, true);
            load_lock.unlock();
            load_cv.notify_one();

            // Espera até que a comunicação com o serviço externo seja encerrada
            for (auto& thread : threads)
                thread.join();
            threads.clear();

            if (stop)
                break;

            // Força uma atualização do dashboard com as respostas do serviço externo mesmo
            // sem input do usuário
            load_lock.lock();
            should_draw = true;
            load_lock.unlock();
            load_cv.notify_one();
            
            // Descomente para dormir por 2 segundos entre cada ciclo
            // std::this_thread::sleep_for(std::chrono::seconds(2));
            file_indices[highway] = (file_indices[highway] + 1) % n_files;
        }
        n_threads += 2;
    }

 private:
    // Mapeia a placa para os dados do veículo
    std::unordered_map<Plate, VehicleData, Hash<Plate>> vehicles;
    std::condition_variable cv;
    // Mutex "principal" que vai impedir concorrência entre as threads em E e T
    std::mutex mutex;
    // Serviço que ainda está me assombrando
    SlowService service;
    // Armazena as placas de veículos que tiveram informações modificadas e ainda não foram processadas
    std::vector<std::vector<Plate>> modified;
    // Armazena os veículos com dados computados antse que sejam exibidos no terminal (adoro templates)
    std::vector<std::vector<std::pair<Plate, Vehicle>>> new_processed;
    // Mesmo que o anterior, mas estes estão prontos e são usados pelo load
    std::vector<std::vector<std::pair<Plate, Vehicle>>> processed;
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

    void extract(int thread_num, int start, int end, int n_lanes, int max_speed, int* sum) {
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
            i += 8;

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
            
            // Transforma os dois índices da faixa em um só para facilitar acesso ao array
            int lane = (data[i] - '0') * n_lanes / 2;
            i += 2;
            lane += data[i] - '0';
            i += 2;
            int distance = str_to_int(data, i, '\n');
            current->vehicle.last_pos = {lane, distance, cycle};
            current->positions.push_back(current->vehicle.last_pos);
            modified[thread_num].push_back(plate);
        }
        // Garante que o vetor que recebe os dados do transform está preparado para recebê-los
        new_processed[thread_num].reserve(modified[thread_num].size());
        new_processed[thread_num].resize(0);
    }

    void transform(int thread_num, int max_speed) {
        int risk_count = 0;
        int speed_count = 0;
        for (Plate& plate : modified[thread_num]) {
            VehicleData* current = &vehicles[plate];
            Vehicle* car = &current->vehicle;
            std::vector<Position>& positions = current->positions;

            // Efetua o cálculo da velocidade e aceleração apenas se houver mais de uma posição
            if (positions.size() > 1) {
                float prev_speed = car->speed;

                int last = positions.size() - 1;
                int distance = positions[last].distance - positions[last - 1].distance;

                // Calcula velocidade como deslocamento dividido por tempo decorrido
                car->speed = static_cast<float>(distance);
                    // / (cycle_times[highway][positions[last].cycle] - cycle_times[highway][positions[last - 1].cycle]);
                if (car->speed == -0.0f)
                    car->speed = 0.0f;
                
                if (positions.size() > 2) {
                    float prev_acceleration = car->acceleration;
                    // Calcula aceleração como velocidade dividida por tempo decorrido
                    car->acceleration = (car->speed - prev_speed);
                        // / (cycle_times[highway][positions[last].cycle] - cycle_times[highway][positions[last - 1].cycle]);
                    if (car->acceleration == -0.0f)
                        car->acceleration = 0.0f;
                    
                    if (positions.size() > 3) {
                        // Cálculo do risco de colisão usando a função sigmoide
                        float x = 16 * (car->speed + car->speed * std::abs(car->acceleration)) / max_speed - 6;
                        car->risk = 1.0f / (1.0f + std::exp(-x));
                    } else {
                        car->risk = -1.0f;
                    }
                } else {
                    car->acceleration = 0.0f;
                    car->risk = -1.0f;    
                }
            // Se não há dados suficientes, define como valores negativos para que possam ser
            // descartados facilmente na análise posterior
            } else {
                car->speed = -1.0f;
                car->acceleration = 0.0f;
                car->risk = -1.0f;
            }

            car->flags[ALL] = true;
            car->flags[COLLISION_RISK] = car->risk >= 0.5f;
            car->flags[ABOVE_SPEED_LIMIT] = car->speed > max_speed;
            risk_count += car->flags[COLLISION_RISK];  // booleano é igual a 1 ou 0
            speed_count += car->flags[ABOVE_SPEED_LIMIT];
            new_processed[thread_num].push_back(std::make_pair(plate, *car));
        }

        // Incrementa os contadores de veículos em risco e acima da velocidade máxima
        std::unique_lock<std::mutex> lock(mutex);
        int car_count = modified[thread_num].size();
        vehicle_counts[ALL] += car_count;
        vehicle_counts[COLLISION_RISK] += risk_count;
        vehicle_counts[ABOVE_SPEED_LIMIT] += speed_count;
        threads_working--;
        cv.notify_all();
        // Espera até que todas as threads tenham terminado essa parte do cálculo
        cv.wait(lock, [this] { return is_all_done(); });
        lock.unlock();

        // Passa a usar dados movidos para outro vetor, deixando new_processed para os próximos
        // ciclos e atualizando os dados atualmente no dashboard conforme o serviço externo responde
        for (auto& [plate, vehicle] : processed[thread_num]) {
            // Tentamos obter as informações do veículo pelo serviço externo lento e atualizamos
            // tanto no dashboard quanto no registro geral de veículos
            if (service.query_vehicle(plate)) {
                // Atualiza no dashboard
                vehicle.name = service.get_name();
                vehicle.model = service.get_model();
                vehicle.year = service.get_year();
                
                // Atualiza no registro geral
                Vehicle* car = &vehicles[plate].vehicle;
                car->name = vehicle.name;
                car->model = vehicle.model;
                car->year = vehicle.year;
            }
        }
    }

    void load() {
        {
            std::thread input_thread(&ETL::handle_input, this);
            input_thread.detach();
        }
        while (true) {
            std::unique_lock<std::mutex> load_lock(load_mutex);
            // Se não tem uma "ordem de desenho", espera até que ela chegue
            if (!should_draw)
                load_cv.wait(load_lock, [this] { return should_draw; });
            if (should_exit)
                break;
            should_draw = false;
            draw();
        }
        endwin();
        std::cout << "Encerrando todas as threads..." << std::endl;
    }

    /*
     *
     *  Aqui começam as variáveis, funções e estruturas relacionadas à parte gráfica do programa.
     * 
    */

    enum VehicleFilter : int {
        ALL,
        COLLISION_RISK,
        ABOVE_SPEED_LIMIT,
    };

    struct DashboardInfo {
        // Armazena o tempo decorrido desde o início da simulação até a chegada no dashboard
        double time_elapsed;
        // Armazena o número de veículos para cada categoria de filtro
        int num_vehicles[3];
        int num_lanes;
    };

    std::condition_variable load_cv;
    std::mutex load_mutex;
    DashboardInfo info;
    std::pair<int, int> current_vehicle;
    int current_absolute_value;
    int current_filter;
    int vehicle_counts[3];
    // Indica se o programa deve ser encerrado (ao receber 'q' como input)
    bool should_exit;
    bool should_draw;

    /// Usado para computar a diferença de tempo entre a simulação e a chegada no dashboard.
    double now() {
        unsigned long nanoseconds = std::chrono::system_clock::now().time_since_epoch().count();
        // Converte para segundos
        return static_cast<double>(nanoseconds) / 1e9;
    }
    
    /// Retorna true se houve mudança no valor do veículo atual.
    bool find_previous() {
        for (int i = current_vehicle.first; i >= 0; i--) {
            int j = i == current_vehicle.first ? current_vehicle.second - 1 : processed[i].size() - 1;
            for (; j >= 0; j--) {
                if (processed[i][j].second.flags[current_filter]) {
                    current_vehicle = std::make_pair(i, j);
                    current_absolute_value--;
                    return true;
                }
            }
        }
        return false;
    }

    /// Retorna true se houve mudança no valor do veículo atual.
    bool find_next() {
        for (int i = current_vehicle.first; i < processed.size(); i++) {
            int j = i == current_vehicle.first ? current_vehicle.second + 1 : 0;
            for (; j < processed[i].size(); j++) {
                if (processed[i][j].second.flags[current_filter]) {
                    current_vehicle = std::make_pair(i, j);
                    current_absolute_value++;
                    return true;
                }
            }
        }
        return false;
    }

    /// Retorna true se o filtro foi atualizado. Define o veículo selecionado como
    /// o primeiro que se adequa ao filtro especificado.
    bool update_filter(int new_value, bool force = false) {
        if (new_value != current_filter || force) {
            current_filter = new_value;
            current_vehicle = std::make_pair(0, 0);
            if (info.num_vehicles[current_filter] > 1) {
                if (!processed[0][0].second.flags[current_filter])
                    find_next();
                current_absolute_value = 1;
            } else {
                current_absolute_value = 0;
            }
            return true;
        }
        return false;
    }

    void handle_input() {
        while (true) {
            bool changed = false;
            switch (getch()) {
                case KEY_LEFT:
                    changed = find_previous();
                    break;
                case KEY_RIGHT:
                    changed = find_next();
                    break;
                case 'q':
                    load_mutex.lock();
                    // Necessário para que a thread de desenho pare de esperar
                    should_draw = true;
                    should_exit = true;
                    load_mutex.unlock();
                    load_cv.notify_one();
                    return;
                case 't':
                    changed = update_filter(VehicleFilter::ALL);
                    break;
                case 'r':
                    changed = update_filter(VehicleFilter::COLLISION_RISK);
                    break;
                case 'v':
                    changed = update_filter(VehicleFilter::ABOVE_SPEED_LIMIT);
                    break;
                default:
                    break;
            }
            if (changed) {
                load_mutex.lock();
                should_draw = true;
                load_mutex.unlock();
                load_cv.notify_one();
            }
        }
    }

    void draw() {
        static const std::pair<Plate, Vehicle> default_car = std::make_pair(
            Plate{"-------"}, Vehicle{"", "", -1, {0, 0}, -1, 0, -1});
        clear();
        printw("Dashboard\n\n");

        printw("Número de rodovias: %d\n", static_cast<int>(cycle_times.size()));
        printw("Número de veículos: %d\n", info.num_vehicles[ALL]);
        printw("Número de veículos acima do limite de velocidade: %d\n",
            info.num_vehicles[ABOVE_SPEED_LIMIT]);
        printw("Tempo entre simulação e análise: %.6f segundos;\n\n", info.time_elapsed);

        std::string filter_name;

        switch (current_filter) {
            case VehicleFilter::ALL:
                filter_name = "Todos os veículos";
                break;
            case VehicleFilter::COLLISION_RISK:
                filter_name = "Veículos com risco de colisão";
                break;
            case VehicleFilter::ABOVE_SPEED_LIMIT:
                filter_name = "Veículos acima do limite de velocidade";
                break;
        }

        const std::pair<Plate, Vehicle>* data;
        if (info.num_vehicles[current_filter] == 0)
            data = &default_car;
        else
            data = &processed[current_vehicle.first][current_vehicle.second];
        const Vehicle& v = data->second;

        printw("< %s (%d/%d) >\n\n", filter_name.c_str(), current_absolute_value,
            info.num_vehicles[current_filter]);

        printw("Placa: %s\n", data->first.plate);
        printw("\tPosição: (%d, %d)\n", data->second.last_pos.lane, data->second.last_pos.distance);
        if (v.speed >= 0)
            printw("\tVelocidade: %.2f\n", v.speed);
        else
            printw("\tVelocidade: -\n");
        printw("\tAceleração: %.2f\n", v.acceleration);
        if (v.risk >= 0)
            printw("\tRisco de colisão: %.2f\n", v.risk);
        else
            printw("\tRisco de colisão: -\n");
        if (v.year >= 0) {
            printw("\tProprietário: %s\n", v.name.c_str());
            printw("\tModelo: %s\n", v.model.c_str());
            printw("\tAno de fabricação: %d\n\n", v.year);
        } else {
            printw("\tProprietário: -\n");
            printw("\tModelo: -\n");
            printw("\tAno de fabricação: -\n\n");
        }

        printw("Comandos:\n");
        printw("\t<: anterior\n");
        printw("\t>: próximo\n");
        printw("\tq: sair\n");
        printw("\tt: todos os veículos\n");
        printw("\tr: veículos com risco de colisão\n");
        printw("\tv: veículos acima do limite de velocidade\n");

        refresh();
    }
};

#endif  // ETL_HPP_
