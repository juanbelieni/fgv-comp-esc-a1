#ifndef EXTERNAL_HPP_
#define EXTERNAL_HPP_

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <queue>

#include "./conversions.hpp"

class SlowService {
    std::queue<Plate> queue;
    std::condition_variable cv;
    std::mutex processing_mutex;
    std::mutex queue_mutex;
    int max_queue_size;
    int nap_time;

    std::vector<std::string> first_names;
    std::vector<std::string> last_names;
    std::vector<std::string> models;

    std::string name;
    std::string model;
    int year = -1;

    int random_number(int limit) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> distrib(0, std::numeric_limits<int>::max());
        return distrib(gen) % limit;
    }

 public:
    /// Tempo de soneca em nanossegundos.
    SlowService(int max_queue_size, int nap_time = 1000) :
                max_queue_size(max_queue_size), nap_time(nap_time) {
        std::ifstream file("ETL/random/first_names.txt");
        char line[64];
        while (file.getline(line, 64))
            first_names.push_back(line);
        file.close();
        
        file.open("ETL/random/last_names.txt");
        while (file.getline(line, 64))
            last_names.push_back(line);
        file.close();
        
        file.open("ETL/random/models.txt");
        while (file.getline(line, 64))
            models.push_back(line);
    }

    /// @brief Adiciona uma placa à fila de espera e descarta a requisição se a fila estiver cheia.
    /// @param plate A placa a ser adicionada à fila.
    /// @return Um booleano indicando se a placa foi adicionada à fila.
    bool query_vehicle(const Plate& plate) {
        std::unique_lock<std::mutex> queue_lock(queue_mutex);
        if (queue.size() == max_queue_size)
            return false;
        queue.push(plate);
        // Libera consultas à fila
        queue_lock.unlock();

        std::unique_lock<std::mutex> processing_lock(processing_mutex);
        // Espera até que a próxima placa a ser processada seja a atual
        if (queue.front() != plate)
            cv.wait(processing_lock, [this, plate] { return queue.front() == plate; });

        // Dorme para ser intencionalmente lento
        std::this_thread::sleep_for(std::chrono::nanoseconds(nap_time));

        /*
        Para que a função seja intencionalmente lenta, uma opção é ler o arquivo com os
        dados todas as vezes, para citar a especificação original:
        "carrega todas as informações de um arquivo e faz a pesquisa pelos dados associados à placa."

        Porém, segundo o professor, "não existe arquivo no contexto desse serviço externo".
        Sendo assim, fui pela abordagem mais simples, que é fazer aleatoriamente.
        */
        name = (first_names[random_number(first_names.size())] + " " +
                last_names[random_number(last_names.size())]);
        model = models[random_number(models.size())];
        year = random_number(23) + 2000;

        queue_lock.lock();
        queue.pop();
        queue_lock.unlock();
        processing_lock.unlock();
        // Notifica a próxima thread que a fila foi modificada
        cv.notify_all();
        return true;
    }

    std::string get_name() const {
        return name;
    }

    std::string get_model() const {
        return model;
    }

    int get_year() const {
        return year;
    }
};

#endif  // EXTERNAL_HPP_