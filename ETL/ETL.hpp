#ifndef ETL_HPP_
#define ETL_HPP_

// Fixes some IntelliSense errors in the IDE
#define GRPC_CALLBACK_API_NONEXPERIMENTAL

#include <locale.h>
#include <ncurses.h>
#undef OK  // macro de péssimo nome definido como `(0)` que quebra o gRPC
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "./external.hpp"
#include "proto/simulation.grpc.pb.h"

namespace sim = simulation;

template<>
struct std::hash<sim::Highway> {
    size_t operator()(const sim::Highway& highway) const noexcept {
        return std::hash<std::string>()(highway.name());
    }
};

class ETL {
    static const int default_map_size = 4096;

    struct Position {
        uint32_t lane;
        uint32_t distance;
    };

    // Armazena os dados instantâneos mais recentes de um veículo para o cálculo do risco
    struct Vehicle {
        std::string name;
        std::string model;
        int year = -1;

        int highway_index;
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

    struct HighwayData {
        sim::Highway highway;
        std::vector<uint32_t> cycles;
        // Armazena o instante de tempo de realização de cada ciclo da simulação
        std::vector<double> times;
        // Armazena o tempo decorrido desde o início da simulação até a chegada no dashboard
        double time_elapsed;
    };

    struct ThreadData {
        std::thread thread;
        // Armazena as placas de veículos que tiveram informações modificadas e ainda não foram processadas
        std::vector<std::string> modified;
        // Armazena os veículos com dados computados antes que sejam exibidos no terminal
        std::vector<std::pair<std::string, Vehicle>> vehicles_processing;
        // Recebe os dados do vetor anterior e é atualizado dinamicamente enquanto é usado pelo dashboard
        std::vector<std::pair<std::string, Vehicle>> vehicles_processed;
    };

    class SimulationServiceImpl final : public sim::SimulationService::Service {
        grpc::Status ReportCycle(grpc::ServerContext* context, const sim::SimulationCycle* cycle,
                                 sim::Empty* response) override {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push(*cycle);
            cv.notify_one();
            return grpc::Status::OK;
        }

     public:
        std::queue<sim::SimulationCycle> queue;
        std::condition_variable cv;
        std::mutex mutex;

        SimulationServiceImpl() {}

        /// Retorna um valor que pode ou não conter um ciclo de simulação. Caso a fila
        /// esteja vazia, espera pelo número fornecido de milissegundos ou até que os
        /// dados sejam recebidos.
        std::optional<sim::SimulationCycle> get_data(int64_t timeout = 500) {
            std::unique_lock<std::mutex> lock(mutex);
            // Se a fila estiver vazia, espera até que não esteja
            if (queue.empty()) {
                cv.wait_for(lock, std::chrono::milliseconds(timeout),
                    [this] { return !queue.empty(); });
                if (queue.empty())
                    return {};
            }
            auto cycle = std::move(queue.front());
            queue.pop();
            return cycle;
        }
    };

 public:
    ETL(int num_threads, int external_queue_size) : num_threads(num_threads), service(external_queue_size) {
        // O serviço deve ser inicializado junto da classe atual, pois ele não possui construtor padrão
        vehicles.reserve(default_map_size);
        highways.reserve(100);
    }

    ~ETL() {
        if (timeout_thread.joinable())
            timeout_thread.join();
    }

    void set_thread_count(int num_threads) {
        this->num_threads = num_threads;
    }

    double summary() const {
        return info.num_runs ? (info.total_time / info.num_runs) : std::numeric_limits<double>::infinity();
    }

    /// Inicia a execução do ETL. O comportamento padrão com `timeout` igual a 0 é de
    /// esperar o usuário pressionar a tecla 'q' para encerrar a execução. Caso haja um
    /// valor maior (em segundos), o servidor interrompe a execução após o tempo especificado.
    void run(double timeout = 0.0) {
        /*
         *  Mínimo de 5 threads além daquela que recebeu a chamada de `run`:
         *  1 thread para a verificação de mensagens dos clientes.
         *  Pelo menos 1 thread para executar Extract: a atual gerencia a chegada de novos
         *  dados por RPC e a(s) outra(s) processa(m) os dados recebidos.
         *  Pelo menos 1 thread para executar Transform.
         *  2 threads para executar Load: uma para receber requisições de atualização
         *  do dashboard e uma para verificação de input do usuário.
         *  Esse número não leva em consideração threads que ficarão em espera na maioria
         *  do tempo, como a thread de timeout, que apenas acorda para encerrar o programa.
        */
        if (num_threads < 5)
            throw std::runtime_error("Número de threads deve ser maior que ou igual a 5.");

        // Subtrai do número de threads para manter apenas a quantidade que é flexível
        num_threads -= 3;
        // Inicializa o dashboard
        {
            setlocale(LC_ALL, "");
            initscr();
            noecho();
            keypad(stdscr, true);
            std::thread dashboard(&ETL::load, this);
            dashboard.detach();
        }
        std::thread orchestrator_thread(&ETL::orchestrator, this);

        this->listen(timeout);
        orchestrator_thread.join();
        num_threads += 3;
    }

 private:
    // Mapeia os nomes de rodovias para suas filas de dados a processar
    std::unordered_map<std::string, int> highway_idx;
    std::vector<HighwayData> highways;
    // Mapeia a placa para os dados do veículo
    std::unordered_map<std::string, VehicleData> vehicles;
    // Armazena threads e seus respectivos dados em processamento
    std::vector<ThreadData> thread_data;
    // Armazena os índices de ciclos que serão e que estão sendo processados
    std::vector<std::pair<sim::SimulationCycle, int>> cycles_to_process;
    std::vector<std::pair<sim::SimulationCycle, int>> cycles_processing;

    std::condition_variable cv;
    // Mutex "principal" que vai impedir concorrência entre as threads em E e T
    std::mutex mutex;
    // Serviço externo que ainda está me assombrando
    SlowService service;

    // Servidor gRPC
    std::unique_ptr<grpc::Server> server;
    // Serviço que implementa a interface do gRPC e gerencia o recebimento de dados
    SimulationServiceImpl server_service;
    // Thread usada para encerrar o servidor após um tempo
    std::thread timeout_thread;
    bool is_server_running = false;

    int num_threads;
    bool etl_running = false;

    int get_cycle_index(int highway_index) const {
        int i;
        for (i = 0; i < cycles_to_process.size(); i++) {
            if (cycles_to_process[i].second == highway_index)
                return i;
        }
        return -1;
    }

    void expand_map() {
        int max_size = vehicles.size();
        for (const auto& [cycle, highway_index] : cycles_to_process) {
            // Adiciona os dados da simulação aos vetores da rodovia correspondente
            highways[highway_index].cycles.push_back(cycle.cycle());
            highways[highway_index].times.push_back(cycle.timestamp());

            // Computa o número máximo de veículos que podem ser adicionados
            max_size += cycle.vehicles_size();
        }
        int capacity = vehicles.bucket_count();
        // Duplica a capacidade atual até que possa acomodar todos esses veículos
        while (capacity < max_size)
            capacity *= 2;
        // Se a capacidade atual for menor que a necessária, realoca o unordered_map
        if (capacity > vehicles.bucket_count())
            vehicles.reserve(capacity);
    }

    void force_redraw(bool reset = false) {
        std::lock_guard<std::mutex> lock(load_mutex);
        should_draw = true;
        if (reset) {
            update_filter(info.vehicle_filter, false, true);
            for (int i = 0; i < 3; i++)
                info.num_vehicles[i] = vehicle_counts[i];
        }
        load_cv.notify_one();
    }

    void join_all_threads() {
        for (int i = 0; i < num_threads; i++)
            thread_data[i].thread.join();
    }

    void etl() {
        thread_data.resize(num_threads);
        // Armazena o último índice de cada ciclo processado
        std::vector<int> indices;
        indices.reserve(cycles_processing.size());
        int last_index = 0;
        for (int i = 0; i < cycles_processing.size(); i++) {
            last_index += cycles_processing[i].first.vehicles_size();
            indices.push_back(last_index);
        }

        int chunk_size = last_index / num_threads;
        for (int i = 0; i < num_threads; i++) {
            thread_data[i].thread = std::thread(&ETL::extract, this, i,
                i * chunk_size, (i + 1) * chunk_size, indices);
        }
        join_all_threads();

        // Zera os contadores de veículos em cada categoria de filtro
        vehicle_counts[VehicleFilter::ALL] = last_index;
        vehicle_counts[VehicleFilter::COLLISION_RISK] = 0;
        vehicle_counts[VehicleFilter::ABOVE_SPEED_LIMIT] = 0;

        // Faz a transformação prioritária dos dados
        for (int i = 0; i < num_threads; i++)
            thread_data[i].thread = std::thread(&ETL::transform, this, i);
        join_all_threads();
        // Força a atualização do dashboard
        force_redraw(true);

        // Obtém os dados do serviço externo opcionalmente
        for (int i = 0; i < num_threads; i++)
            thread_data[i].thread = std::thread(&ETL::transform_continued, this, i);
        join_all_threads();
        force_redraw();

        std::lock_guard<std::mutex> lock(mutex);
        etl_running = false;
    }

    void listen(double timeout = 0.0) {
        std::string server_address("localhost:50051");

        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&server_service);

        server = builder.BuildAndStart();
        is_server_running = true;
        if (timeout != 0.0) {
            timeout_thread = std::thread([this, timeout] {
                std::this_thread::sleep_for(std::chrono::duration<double>(timeout));
                quit();
            });
        }
        // Função que nunca retorna, a não ser que o servidor seja encerrado por quit()
        server->Wait();
    }

    void orchestrator() {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(load_mutex);
                if (should_exit)
                    break;
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (cycles_to_process.size() && !etl_running) {
                    expand_map();
                    cycles_processing = std::move(cycles_to_process);
                    std::thread runner(&ETL::etl, this);
                    runner.detach();
                    etl_running = true;
                }
            }
            std::optional<sim::SimulationCycle> answer = server_service.get_data();
            // Se não houve resposta após 0.5 segundo, tenta novamente
            if (!answer.has_value())
                continue;
            sim::SimulationCycle& cycle = answer.value();
            auto it = highway_idx.find(cycle.highway().name());
            int highway_index;

            // Se a rodovia ainda não está registrada, adiciona ao vetor
            if (it == highway_idx.end()) {
                highway_index = highways.size();
                highways.emplace_back(std::move(cycle.highway()));
                highway_idx.emplace(highways.back().highway.name(), highway_index);
            } else {
                highway_index = it->second;
            }
            int cycle_index = get_cycle_index(highway_index);
            // Se a rodovia não está na fila de processamento, ela é adicionada
            if (cycle_index < 0)
                cycles_to_process.emplace_back(std::move(cycle), highway_index);
            // Se ela está na fila, substitui o ciclo antigo pelo mais recente
            else
                cycles_to_process[cycle_index].first = std::move(cycle);
        }
    }

    void extract(int thread_id, int start, int end, const std::vector<int>& indices) {
        ThreadData& data = thread_data[thread_id];
        // Corrige a divisão imprecisa e garante que todos os veículos serão processados
        if (thread_id == num_threads - 1)
            end = indices.back();
        // Obtém o índice do ciclo que contém o primeiro veículo a ser processado
        int cycle_index;
        for (int i = 0; i < indices.size(); i++) {
            if (indices[i] > start) {
                cycle_index = i;
                break;
            }
        }

        // Variável que armazena o índice do último veículo no vetor local do ciclo,
        // inicializada com o índice do ciclo anterior
        int local_end = indices[std::max(0, cycle_index - 1)];
        while (cycle_index < indices.size()) {
            sim::SimulationCycle& cycle = cycles_processing[cycle_index].first;
            int highway_index = cycles_processing[cycle_index].second;
            int factor = highways[highway_index].highway.lanes() / 2;

            auto none = vehicles.end();
            // Começa pelo último do ciclo anterior e vai até o último veículo do ciclo atual
            int i = local_end;
            local_end = std::min(end, indices[cycle_index]);
            while (i < local_end) {
                const sim::RawVehicle& vehicle = cycle.vehicles(i++);
                // No código abaixo, como não podemos ter mais de uma thread tentando acessar a
                // mesma placa, o único problema que pode ocorrer é a placa não estar registrada e
                // ser inserida no mesmo espaço de memória em que outra placa que está sendo inserida
                VehicleData* current;
                auto it = vehicles.find(vehicle.plate());
                // Se a placa ainda não está registrada, cria seu registro sem condição de corrida
                if (it == none) {
                    std::lock_guard<std::mutex> lock(mutex);
                    current = &vehicles[vehicle.plate()];
                // Se a placa já está registrada, obtém um ponteiro para seus dados e os atualiza
                } else {
                    current = &it->second;
                }

                // Transforma os dois índices da faixa em um só para facilitar acesso ao array
                uint32_t lane = vehicle.lane() + vehicle.direction() * factor;
                current->vehicle.highway_index = highway_index;
                current->vehicle.last_pos = {lane, vehicle.distance()};
                current->positions.push_back(current->vehicle.last_pos);
                data.modified.push_back(vehicle.plate());
            }
            cycle_index++;
        }
        // Garante que o vetor que recebe os dados do transform está preparado
        data.vehicles_processing.resize(0);
        data.vehicles_processing.reserve(data.modified.size());
    }

    void transform(int thread_id) {
        int risk_count = 0;
        int speed_count = 0;
        ThreadData& data = thread_data[thread_id];
        for (const std::string& plate : data.modified) {
            VehicleData* current = &vehicles[plate];
            Vehicle* car = &current->vehicle;
            int highway_index = car->highway_index;

            std::vector<Position>& positions = current->positions;
            std::vector<uint32_t>& cycles = highways[highway_index].cycles;
            float speed_limit = highways[highway_index].highway.speed_limit();

            // Efetua o cálculo da velocidade e aceleração apenas se houver mais de uma posição
            if (positions.size() > 1) {
                float prev_speed = car->speed;
                int last = positions.size() - 1;
                int last_cycle = cycles.size() - 1;

                // Calcula velocidade como deslocamento dividido por tempo decorrido
                car->speed = static_cast<float>(positions[last].distance - positions[last - 1].distance)
                    / (cycles[last_cycle] - cycles[last_cycle - 1]);
                if (car->speed == -0.0f)
                    car->speed = 0.0f;

                if (positions.size() > 2) {
                    // Calcula aceleração como variação de velocidade dividida por tempo decorrido
                    car->acceleration = (car->speed - prev_speed) 
                        / (cycles[last_cycle] - cycles[last_cycle - 1]);
                    if (car->acceleration == -0.0f)
                        car->acceleration = 0.0f;

                    if (positions.size() > 3) {
                        float x = 3.0f * (car->speed + car->speed * std::abs(car->acceleration)) / speed_limit - 5.0f;
                        // Cálculo do risco de colisão usando a função sigmoide
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
            car->flags[ABOVE_SPEED_LIMIT] = car->speed > speed_limit;
            risk_count += car->flags[COLLISION_RISK];  // booleano é igual a 1 ou 0
            speed_count += car->flags[ABOVE_SPEED_LIMIT];
            data.vehicles_processing.emplace_back(plate, *car);
        }

        // Incrementa os contadores de veículos em risco e acima da velocidade máxima
        std::unique_lock<std::mutex> lock(mutex);
        vehicle_counts[COLLISION_RISK] += risk_count;
        vehicle_counts[ABOVE_SPEED_LIMIT] += speed_count;
    }

    void transform_continued(int thread_id) {
        // Passa a usar dados movidos para outro vetor, deixando vehicles_processing para os próximos
        // ciclos e atualizando os dados atualmente no dashboard conforme o serviço externo responde
        for (auto& [plate, vehicle] : thread_data[thread_id].vehicles_processed) {
            // Tentamos obter as informações do veículo pelo serviço externo lento e atualizamos
            // tanto no dashboard quanto no registro geral de veículos
            if (vehicle.year < 0 && service.query_vehicle(plate)) {
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
            // Se não tem um "pedido de desenho", espera até que ele chegue
            if (!should_draw)
                load_cv.wait(load_lock, [this] { return should_draw; });
            if (should_exit)
                break;
            should_draw = false;
            draw();
        }
        endwin();
    }

    void quit() {
        load_mutex.lock();
        if (is_server_running) {
            is_server_running = false;
            server->Shutdown();
        }
        // Necessário para que a thread de desenho pare de esperar
        should_draw = true;
        should_exit = true;
        load_mutex.unlock();
        load_cv.notify_one();
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
        double total_time;
        int num_runs;
        // Índices de acesso dos veículo atual nos vetores analisados
        int vehicle_i;
        int vehicle_j;
        int absolute_value;
        int vehicle_filter;
        int highway_filter;
        // Armazena o número de veículos para cada categoria de filtro
        int num_vehicles[3];
    };

    std::condition_variable load_cv;
    std::mutex load_mutex;
    DashboardInfo info{};
    int vehicle_counts[3];
    // Indica se o programa deve ser encerrado (ao receber 'q' como input)
    bool should_exit = false;
    bool should_draw = false;

    /// Usado para computar a diferença de tempo entre a simulação e a chegada no dashboard.
    double now() {
        unsigned long nanoseconds = std::chrono::system_clock::now().time_since_epoch().count();
        // Converte para segundos
        return static_cast<double>(nanoseconds) / 1e9;
    }

    /// Retorna true se houve mudança no valor do veículo atual.
    bool find_previous() {
        for (int i = info.vehicle_i; i >= 0; i--) {
            int j = i == info.vehicle_i ? info.vehicle_j - 1 : get_processed(i).size() - 1;
            for (j; j >= 0; j--) {
                if (get_processed(i)[j].second.flags[info.vehicle_filter]) {
                    info.vehicle_i = i;
                    info.vehicle_j = j;
                    info.absolute_value--;
                    return true;
                }
            }
        }
        return false;
    }

    const std::vector<std::pair<std::string, Vehicle>>& get_processed(int i) {
        return thread_data[i].vehicles_processed;
    }

    /// Retorna true se houve mudança no valor do veículo atual.
    bool find_next() {
        for (int i = info.vehicle_i; i < get_processed(i).size(); i++) {
            int j = i == info.vehicle_i ? info.vehicle_j + 1 : 0;
            for (j; j < get_processed(i).size(); j++) {
                if (get_processed(i)[j].second.flags[info.vehicle_filter]) {
                    info.vehicle_i = i;
                    info.vehicle_j = j;
                    info.absolute_value++;
                    return true;
                }
            }
        }
        return false;
    }

    /// Retorna true se o filtro foi atualizado. Define o veículo selecionado como
    /// o primeiro que se adequa ao filtro especificado.
    bool update_filter(int new_value, bool highway = false, bool force = false) {
        int& current_filter = highway ? info.highway_filter : info.vehicle_filter;
        if (new_value != current_filter || force) {
            current_filter = new_value;
            info.vehicle_i = 0;
            info.vehicle_j = 0;
            if (info.num_vehicles[info.vehicle_filter] > 0) {
                if (get_processed(0).size() == 0 || !get_processed(0)[0].second.flags[info.vehicle_filter])
                    find_next();
                info.absolute_value = 1;
            } else {
                info.absolute_value = 0;
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
                    quit();
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
                // case 'h':
                //     load_mutex.lock();
                //     int new_value = choose_highway();
                //     changed = update_filter(new_value, true);
                //     load_mutex.unlock();
                //     break;
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
        static const std::pair<std::string, Vehicle> default_car = std::make_pair(
            std::string("-------"), Vehicle{"", "", -1, -1, {0, 0}, -1, 0, -1});
        clear();
        printw("Dashboard\n\n");

        printw("Número de rodovias: %d\n", static_cast<int>(highways.size()));
        printw("Número de veículos: %d\n", info.num_vehicles[ALL]);
        printw("Número de veículos acima do limite de velocidade: %d\n\n",
            info.num_vehicles[ABOVE_SPEED_LIMIT]);

        const char* vehicle_filter_name;
        switch (info.vehicle_filter) {
            case VehicleFilter::ALL:
                vehicle_filter_name = "Todos os veículos";
                break;
            case VehicleFilter::COLLISION_RISK:
                vehicle_filter_name = "Veículos com risco de colisão";
                break;
            case VehicleFilter::ABOVE_SPEED_LIMIT:
                vehicle_filter_name = "Veículos acima do limite de velocidade";
                break;
        }

        const std::pair<std::string, Vehicle>* data;
        if (info.num_vehicles[info.vehicle_filter] == 0)
            data = &default_car;
        else
            data = &thread_data[info.vehicle_i].vehicles_processed[info.vehicle_j];
        const Vehicle& v = data->second;

        printw("< %s (%d/%d) >\n\n", vehicle_filter_name, info.absolute_value,
            info.num_vehicles[info.vehicle_filter]);

        printw("Placa: %s\n", data->first.c_str());

        if (v.highway_index >= 0) {
            printw("Rodovia: %s\n", highways[v.highway_index].highway.name().c_str());
            printw("\tTempo entre simulação e análise: %.6f segundos;\n",
                highways[v.highway_index].time_elapsed);
        } else {
            printw("Rodovia: -\n");
            printw("\tTempo entre simulação e análise: -\n");
        }

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
