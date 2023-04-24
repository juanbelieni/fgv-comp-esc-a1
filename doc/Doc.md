## Informações contidas em highway-simulator/args.py:
Parâmetros de entrada do programa.
```python
- -n (str): nome da rodovia;
- -l (int): número de faixas;
- -s (int): velocidada limite;
- -sl (int): limite de velocidade;
- -pv (float): probabilidade de um novo veículo ser criado;
- -pl (float): probabilidade de mudar de faixa;
- -pc (float): probabilidade de colisão;
- -cd (int): duração da colisão;
- -vmax (int): velocidade máxima;
- -vmin (int): velocidade mínima;
- -amax (int): aceleração máxima;
- -amin (int): aceleração mínima;
- -d (float): duração de cada ciclo;
- -o (str): nome do diretório de saída;
- -p (None): imprime a rodovia no console;
```

## Informações contidas em highway-simulator/main.py:
Mock que simula a dinâmica de funcionamento de uma rodovia.

```python
class VehiclePosition:  
"""Classe que representa a posição de um veículo na rodovia"""  

- lane (int): faixa em que o veículo está;
- dist (int): distância do início da rodovia;
```

```python
class Vehicle:  
"""Classe que representa um veículo"""  

- id (str): placa;  
- pos (VehiclePosition): posição (faixa e distância do início do percurso);  
- speed (int): velocidade;  
- acceleration (int): aceleração;  
- collision_time (Optional[int]): instante da colisão, se tiver acontecido;  
- collide(self, cycle: int) -> (None): define parametros de colisão;  
```

```python
class Highway:  
"""Classe que representa uma rodovia"""  

- name (str): nome da rodovia;
- speed_limit (int): velocidade máxima permitida;  
- lanes (int): número de faixas;  
- size (int): tamanho da rodovia;  
- outgoing_vehicles: veículos da andam da esquerda para a direita;  
- incoming_vehicles: veículos que andam da direita para a esquerda;  
- vehicles(self) -> (list[Vehicle]): lista com todos os veículos que estão na rodovia;  
```

```python  
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
```

```python
class Simulation:  
"""Classe que representa a simulação"""  

- highway (Highway): rodovia;
- params (SimulationParams): parâmetros da simulação;
- cycle (int): ciclo atual;
- run(self) -> (None): executa a simulação;
- __generate_vehicles(self) -> (None): gera novos veículos;
- __move_vehicles(self) -> (None): move os veículos;
- __remove_collisions(self) -> (None): remove veículos que colidiram;
- __print_status(self) -> (None): imprime uma atualização dos parâmetros relacionados à rodovia;
- __report_vehicles(self) -> (None): salva os dados dos veículos em txt;
```

## Informações contidas em ETL/conversions.py:
Conversões de tipos de dados para posterior uso.

```cpp
inline int str_to_int(const char* str, int& index, char end);
/***
Retorna a representação em inteiro de uma string.
***/

- @param (str) O array de caracteres a ser convertido.
- @param (index) O índice do primeiro caractere.
- @param (end) O caractere que termina a parte convertida da string.
- @return O inteiro representado pela string.
```

```cpp
inline double str_to_double(const char* str, int& index, char end);
/***
Retorna a representação em float de uma string.
***/

 - @param (str) O array de caracteres a ser convertido.
 - @param (index) O índice do primeiro caractere.
 - @param (end) O caractere que termina a parte convertida da string.
 - @return O double representado pela string.
```

```cpp
struct Plate {};
/***
Uma estrutura para a alocação apropriada de espaço para a placa de um veículo.

OBS: A princípio a placa tem 7 caracteres, mas como a alocação é feita para 8 o último é definido como 0.
***/
```

```cpp
struct Hash {};
/***
Cria um hash para a estrutura Plate reinterpretando o array de char como um inteiro.
***/
```

## Informações contidas em ETL/external.py:

```cpp
class SlowService {};
/***
Classe que representa o funcionamento de um serviço lento
***/
- std::queue<Plate> queue: fila com as placas a serem consultadas;
- std::condition_variable cv: variável de condição para a fila;
- std::mutex processing_mutex.
- std::mutex queue_mutex. 
- int max_queue_size: tamanho máximo da fila;
- int nap_time: tempo de espera entre cada consulta;
- std::vector<std::string> first_names: nomes próprios;
- std::vector<std::string> last_names: sobrenomes;
- std::vector<std::string> models: vetor com modelo dos carros;
- std::string name: nome do motorista;
- std::string model: modelo do carro;
- int year = -1: ano do carro;
```

```cpp
SlowService(int max_queue_size, int nap_time);
/***
Construtor da classe SlowService
***/

- int max_queue_size: tamanho máximo da fila;
- int nap_time: tempo de espera entre cada consulta, default = 10;
```

```cpp
bool query_vehicle(const Plate& plate);
/***
Chamada de query_vehicle que coloca a placa informada na fila de espera e aguarda até que ela esteja no primeiro lugar da fila, então os valores são computados e a função retorna.
Os valores são obtidos de arquivos que eu peguei por scraping com nomes comuns de brasileiros e modelos de carro em 2012 ou algo assim, aí são selecionados aleatoriamente pra cada placa
Importante ressaltar que travar a computação não trava a fila de espera
***/

- const Plate& plate: placa do veículo.
```

## Informações contidas em ETL/ETL.py:

### Parte de extração, transformação e carregamento que começa a partir da linha 19

```cpp
class ETL;
/***
Classe que representa o processo de extração, transformação e carregamento dos dados.
***/

- static const int buffer_size: tamanho do buffer de leitura.
- static const int default_map_size: tamanho inicial do mapa de veículos.
- static const int n_files = 5: número de arquivos de dados.
```

```cpp
struct Position;
/***
Posição do veículo (faixa de trânsito e distância até o início da faixa) e o ciclo em que foi obtida.
***/

- int lane: inteiro que representa uma faixa de trânsito.
- int distance: distância até o início da faixa.
- int cycle: ciclo em que a posição foi obtida.
```

```cpp
struct Vehicle;
/***
Estrutura para representar o nome do proprietário, nome do modelo, ano de fabricação; referentes à última leitura: posição, aceleração, risco e 3 booleanos indicando se ele pertence a cada filtro do dashboard
***/

- std::string name: nome do proprietário.
- std::string model: nome do modelo.
- int year = -1: ano de fabricação.
- Position last_pos: ultima posição gravada do veículo.
- float speed: velocidade do veículo.
- float acceleration: aceleção do veículo.
- float risk: probalidade/risco de colisão do veículo.
- bool flags[3]: 3 booleanos indicando se ele pertence a cada filtro do dashboard.
```

```cpp
struct VehicleData;
/***
O veículo e um vetor de todas as suas posições lidas.
***/

- std::vector<Position> positions: vetor das posições
- Vehicle vehicle: veículo em si.
```

```cpp
ETL(int n_threads, int external_queue_size) : n_threads(n_threads), service(external_queue_size);
/***
Construtor da classe ETL.
***/

- int n_threads: número de threads fora a principal, pois ela coordena tudo, das quais 2 ficam reservadas para o dashboard, então o mínimo é de 3.
- int external_queue_size: tamanho da fila do serviço externo, pois ele é parte da classe.
```

```cpp
void run(const std::vector<std::string>& folders);
/***
Começa a rodar o ETL e recebe um vetor de strings que são os caminhos das pastas a serem verificadas, cada pasta representando uma rodovia; também chama as 3 etapas do ETL e só para de rodar por interrupção do usuário
***/

- const std::vector<std::string>& folders: vetor de strings das pastas.
```

```cpp
bool is_all_done() -> (boll)
e
bool is_only_main_thread()  -> (bool);
/***
Funções usadas pela variável de condição para acordar uma thread que está dormindo baseado em quantas threads terminaram suas tarefas
***/
```

```cpp
void set_thread_count(int n_threads);
/***
Define o número de threads que serão usadas.
***/

int n_threads: número de threads a serem usadas.
```

```cpp
void find_next_line(int& i);
/***
Encontra a próxima nova linha no buffer que armazena os dados para manter a composição dos dados.
***/
```

```cpp
void extract(int thread_num, int start, int end, int n_lanes, int max_speed, int* sum);
/***
Soma a quantidade de novas placas e aloca a memória necessária, então recomeça a leitura do arquivo extraindo os dados de fato.
***/

- int thread_num: número da thread.
- int start: índice de início da leitura.
- int end: índice de fim da leitura.
- int n_lanes: número de faixas da rodovia.
- int max_speed: velocidade máxima da rodovia.
- int* sum: ponteiro para a variável que armazena a quantidade de novas placas.
```

```cpp
void transform(int thread_num, int max_speed);
/***
Calcula velocidade, aceleração e risco (na medida do possível) e dorme até que os dados sejam passados para o load. Em seguida, passa por todos os dados novamente tentando uma comunicação com o serviço externo, assim mantendo a prioridade do risco.
***/
- int thread_num: número da thread.
- int max_speed: velocidade máxima da rodovia.
```

```cpp
void load();
/***
Aguarda uma comunicação de outra thread especificando que o dashboard deve ser redesenhado, seja por input do usuário ou por leitura de dados atualizados
***/

```


### Parte gráfica que começa a partir da linha 462

```cpp
enum VehicleFilter : int;
/***
Serve apenas para servir como tag para cada veículo, sendo todos,ou os que tem risco de colisão, ou os que estão acima da velocidade limite.
***/

- ALL
- COLLISION_RISK
- ABOVE_SPEED_LIMIT
```

```cpp
struct DashboardInfo;
/***
Serve para armazenar as informações que serão mostradas no dashboard. Ex: informação de tempo passado desde a geração do CSV até a chegada no dashboard, número de veículos em cada categoria do filtro e número de faixas.
***/

- double time_elapsed: tempo decorrido desde o início da simulação.
- int num_vehicles[3]: número de veículos de cada tipo.
- int num_lanes: número de faixas da rodovia.
- std::condition_variable load_cv: variável de condição para a thread de carregamento.
- std::mutex load_mutex: mutex para a thread de carregamento.
- DashboardInfo info: informações que serão mostradas no dashboard.
- std::pair<int, int> current_vehicle: veículo atual que está sendo mostrado.   
- int current_absolute_value: valor absoluto sobre o veículo atual que está sendo mostrado.
- int current_filter: filtro selecionado atual
- int vehicle_counts[3]: contagem de veículos de cada tipo.
- bool should_exit: variável que indica se a simulação deve ser encerrada.
- bool should_draw: variável que indica se a interface deve ser desenhada.
```

```cpp
double now();
/***
Função para computar a diferença de tempo entre a simulação e a chegada no dashboard, obtendo o tempo Unix atual.
***/

```

```cpp	
bool find_previous()
e
bool find_next()
;
/***
Iteram sobre o vetor de veículos procurando aqueles que se encaixam no filtro.
***/
```

```cpp
bool update_filter(int new_value);
/***
Atualiza o filtro usado para a visualização de veículos.
***/
```

```cpp
void handle_input();
/***
Função que verifica se houve alguma interação com relação usuário na interface.
***/
```

```cpp
void draw();
/***
Função que desenha a interface mostrando o número de rodovias, veículos, tempo decorrido, informações relacionadas aos veículos em si, etc.
***/
```