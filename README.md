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

## Requisitos e observações:
Ter instalado em sua máquina:  
- Python 3.10 ou superior
- Biblioteca ncurses para C++

Recomendamos que busque por como proceder com a instalação do ncurses em seu sistema operacional. Acreditamos que os seguintes comandos devem auxiliar:

Ubuntu
```bash
sudo apt-get install libncurses5-dev libncursesw5-dev
```

Arch Linux
```bash
sudo pacman -S ncurses
```

OSX
```bash
brew install ncurses
```
  
OBS: Dependendo do tamanho de fonte da sua IDE ou do seu terminal, pode ser necessário diminuir ou aumentar o tamanho para que a simulação seja visualizada corretamente.

## Manual de instruções
Para utilizar o programa de simulação, basta executar pelo terminal o arquivo ```main.py``` usando Python em versão compatível. Junto a isto, em outro terminal execute o arquivo cpp para a etapa de ETL e de visualização do dashboard. Também é necessário criar uma pasta para a transferência dos dados entre o Mock e o ETL. Por padrão, utilizamos `/data`.

Exemplo de linha da comando para executar o programa no Windows:  
```bash
py -3.10 highway-simulator/main.py -n "Golden Gate Bridge" -l 3 -s 5 -pv 0.1 -pl 0.20 -pc 0.15 -p -o data
```

Exemplo de linha da comando para executar o programa no Linux:  
```bash
python3 highway-simulator/main.py -n "Ponte Rio-Niterói" -l 4 -s 3 -pv 0.2 -pl 0.20 -pc 0.3 -p -o data
```

Para compilar o programa C++:
```bash
g++ main.cpp -std=c++20 -lncurses -o main
```

Para ver no console os parâmetros disponíveis do script python, execute o parâmetro ```-h``` ou ```--help```.

Ou, use esta lista como guia:
- -n: nome da rodovia;
- -l: número de faixas;
- -s: velocidada limite;
- -sl: limite de velocidade;
- -pv: probabilidade de um novo veículo ser criado;
- -pl: probabilidade de mudar de faixa;
- -pc: probabilidade de colisão;
- -cd: duração da colisão;
- -vmax: velocidade máxima;
- -vmin: velocidade mínima;
- -amax: aceleração máxima;
- -amin: aceleração mínima;
- -d: duração em milissegundos de cada iteração;
- -o: diretório do arquivo de saída;
- -p: mostra a simulação no console.
  
Todos os parâmetros são opcionais. Caso algum parâmetro não seja passado, o programa irá utilizar os valores padrão.
