#include <locale.h>
#include <ncurses.h>

#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <optional>

#define clamp(x, min, max) (x < min ? min : (x > max ? max : x))

using namespace std;

struct Vehicle {
  string license_plate;
  pair<int, int> position;
  int speed;
  int acceleration;
  float collision_risk;

  string owner_name;
  string model;
  int manufacture_year;
};

enum VehicleFilter {
  ALL,
  COLLISION_RISK,
  ABOVE_SPEED_LIMIT,
};

struct DashboardInfo {
  int num_highways;
  int num_vehicles;
  int num_vehicles_with_collision_risk;
  int num_vehicles_above_speed_limit;
  vector<Vehicle> vehicles;
  vector<Vehicle> vehicles_with_collision_risk;
  vector<Vehicle> vehicles_above_speed_limit;
};

DashboardInfo info;

auto current_filter = VehicleFilter::ALL;
auto current_vehicle = 0;
auto current_number_of_cars = 0;

mutex close_mutex;
mutex draw_mutex;

void draw() {
  draw_mutex.lock();

  clear();
  printw("Dashboard\n\n");

  printw("Número de rodovias: %d\n", info.num_highways);
  printw("Número de veículos: %d\n", info.num_vehicles);
  printw("Número de veículos acima do limite de velocidade: %d\n\n",
         info.num_vehicles_above_speed_limit);

  string filter_name;
  vector<Vehicle>* vehicles;

  switch (current_filter) {
    case VehicleFilter::ALL:
      filter_name = "Todos os veículos";
      vehicles = &info.vehicles;
      break;
    case VehicleFilter::COLLISION_RISK:
      filter_name = "Veículos com risco de colisão";
      vehicles = &info.vehicles_with_collision_risk;
      break;
    case VehicleFilter::ABOVE_SPEED_LIMIT:
      filter_name = "Veículos acima do limite de velocidade";
      vehicles = &info.vehicles_above_speed_limit;
      break;
  }

  auto& v = (*vehicles)[current_vehicle];

  printw("< %s (%d/%d) >\n\n", filter_name.c_str(), current_vehicle + 1,
         vehicles->size());

  printw("%s\n", v.license_plate.c_str());
  printw("\tPosição: (%d, %d)\n", v.position.first, v.position.second);
  printw("\tVelocidade: %d\n", v.speed);
  printw("\tAceleração: %d\n", v.acceleration);
  printw("\tRisco de colisão: %.2f\n", v.collision_risk);
  printw("\tProprietário: %s\n", v.owner_name.c_str());
  printw("\tModelo: %s\n", v.model.c_str());
  printw("\tAno de fabricação: %d\n\n", v.manufacture_year);

  printw("Comandos:\n");
  printw("\t<: anterior\n");
  printw("\t>: próximo\n");
  printw("\tq: sair\n");
  printw("\tt: todos os veículos\n");
  printw("\tr: veículos com risco de colisão\n");
  printw("\tv: veículos acima do limite de velocidade\n");

  refresh();

  draw_mutex.unlock();
}

void handle_input() {
  current_number_of_cars = info.num_vehicles;
  while (true) {
    switch (getch()) {
      case KEY_LEFT:
        current_vehicle = clamp(current_vehicle - 1, 0, current_number_of_cars - 1);
        break;
      case KEY_RIGHT:
        current_vehicle = clamp(current_vehicle + 1, 0, current_number_of_cars - 1);
        break;
      case 'q':
        close_mutex.unlock();
        break;
      case 't':
        current_filter = VehicleFilter::ALL;
        current_number_of_cars = info.num_vehicles;
        current_vehicle = 0;
        break;
      case 'r':
        current_filter = VehicleFilter::COLLISION_RISK;
        current_number_of_cars = info.num_vehicles_with_collision_risk;
        current_vehicle = 0;
        break;
      case 'v':
        current_filter = VehicleFilter::ABOVE_SPEED_LIMIT;
        current_number_of_cars = info.num_vehicles_above_speed_limit;
        current_vehicle = 0;
        break;
    }

    draw();
  }
}

int main() {
  info.num_highways = 1;
  info.num_vehicles = 3;
  info.num_vehicles_above_speed_limit = 1;
  info.num_vehicles_with_collision_risk = 2;
  info.vehicles.resize(info.num_vehicles);

  info.vehicles[0].license_plate = "ABC-1234";
  info.vehicles[0].position = make_pair(0, 0);
  info.vehicles[0].speed = 5;
  info.vehicles[0].acceleration = 1;
  info.vehicles[0].collision_risk = 0.2;
  info.vehicles[0].owner_name = "John Doe";
  info.vehicles[0].model = "Ferrari";
  info.vehicles[0].manufacture_year = 2019;

  info.vehicles[1].license_plate = "XYZ-5678";
  info.vehicles[1].position = make_pair(10, 20);
  info.vehicles[1].speed = 6;
  info.vehicles[1].acceleration = 2;
  info.vehicles[1].collision_risk = 0.3;
  info.vehicles[1].owner_name = "Jane Smith";
  info.vehicles[1].model = "Lamborghini";
  info.vehicles[1].manufacture_year = 2021;

  info.vehicles[2].license_plate = "DEF-9012";
  info.vehicles[2].position = make_pair(-5, 10);
  info.vehicles[2].speed = 4;
  info.vehicles[2].acceleration = 1.5;
  info.vehicles[2].collision_risk = 0.1;
  info.vehicles[2].owner_name = "Bob Johnson";
  info.vehicles[2].model = "Porsche";
  info.vehicles[2].manufacture_year = 2018;

  info.vehicles_with_collision_risk = {info.vehicles[0], info.vehicles[1]};
  info.vehicles_above_speed_limit = {info.vehicles[1]};

  close_mutex.lock();

  setlocale(LC_ALL, "");
  initscr();
  noecho();
  keypad(stdscr, true);

  thread input_thread(handle_input);
  draw();

  close_mutex.lock();
  input_thread.detach();
  endwin();

  return 0;
}