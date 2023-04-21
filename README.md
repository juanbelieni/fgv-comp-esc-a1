# Pipeline ETL para o monitoramento de rodovias
Trabalho de A1 para a matéria de Computação Escalável, do 5° período do curso de Ciência de Dados da FGV/EMAp.

**Grupo:** 
> * Amanda Perez
> 
> * Eduardo Adame
> 
> * Juan Belieni
> 
> * Kayo Yokoyama
>
> * Marcelo Amaral

## Introdução:
O principal objetivo deste projeto é implementar, utilizando a linguagem C++, um pipeline de ETL para um sistema
de monitoramento de rodovias, aplicando conceitos estudados em aula para lidar com os problemas de concorrência envolvidos.
Além da implementação do ETL em si, buscou-se também implementar uma simulação de rodovia em Python, a fim de gerar os dados
necessários ao sistema.

---

## Manual de instruções:

## Simulação da rodovia

### Modelagem

A programação dessa simulação se baseia numa modelagem discreta dos espaço e do tempo. I.e., a cada ciclo, o programa calcula a próxima posição que o veículo deve estar baseado em sua posição e velocidade, com base nas informações de aceleração e movimentação de outros carros. Desta maneira, ao final do ciclo, a posição dos carros é armazenada em disco para uso futuro do ETL.  
  
Voltando a simulação em si, temos as seguintes informações:  
class VehiclePosition:
"""Classe que representa a posição de um veículo na rodovia"""  

- lane (int): faixa em que o veículo está;
- dist (int): distância do início da rodovia;

class Vehicle:  
"""Classe que representa um veículo"""  

- id (str): placa;  
- pos (VehiclePosition): posição (faixa e distância do início do percurso);  
- speed (int): velocidade;  
- acceleration (int): aceleração;  
- collision_time (Optional[int]): instante da colisão, se tiver acontecido;  
- collide (Null): define parametros de colisão;  
  

class Highway:  
"""Classe que representa uma rodovia"""  

- name (str):  
- speed_limit (int): velocidade máxima permitida;  
- lanes (int): número de faixas;  
- size (int): tamanho da rodovia;  
- outgoing_vehicles: veículos da andam da esquerda para a direita;  
- incoming_vehicles: veículos que andam da direita para a esquerda;  
- vehicles (list[Vehicle]): lista com todos os veículos que estão na rodovia;  
  
  
class SimulationParams:
"""Classe que representa os parâmetros da simulação"""  

- new_vehicle_probability (float): probabilidade de um novo veículo ser criado;  
- change_lane_probability (float): probabilidade de mudar de faixa;  
- collision_probability (float): probabilidade de colisão;  
- collision_duration (int): duração da colisão;  
- max_speed (float): velocidade máxima;  
- min_speed (float): velocidade mínima;  
- max_acceleration (float): aceleração máxima;  
- min_acceleration (float): aceleração mínima;  
  

class Simulation:
"""Classe que representa a simulação"""  

- highway (Highway): rodovia;
- params (SimulationParams): parâmetros da simulação;
- cycle (int): ciclo atual;
- run (método): executa a simulação;
- generate_vehicles (Null): gera novos veículos;
- move_vehicles (Null): move os veículos;
- remove_collisions (Null): remove veículos que colidiram;
- print_status (Null): imprime uma atualização dos parâmetros relacionados à rodovia;
- report_vehicles (Null): salva os dados dos veículos em txt;

