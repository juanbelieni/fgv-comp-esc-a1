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
  
OBS: Dependendo do tamanho de fonte da sua IDE, pode ser necessário diminuir ou aumentar o tamanho para que a simulação seja visualizada corretamente.

## Manual de instruções
Para utilizar o programa de simulação, basta executar pelo terminal o arquivo ```main.py``` usando Python em versão compatível, definir pelo argumento ```-n``` o nome da rodovia e, opcionalmente, escolher valores para outros argumentos.

Exemplo de linha da comando para executar o programa:  
```bash
py -3.10 main.py -n "Nova Iorque" -l 3 -s 5 -pv 0.1 -pl 0.20 -pc 0.15
```
Onde:
- -n: nome da rodovia;
- -l: número de faixas;
- -s: velocidada limite;
- -pv: probabilidade de um novo veículo ser criado;
- -pl: probabilidade de mudar de faixa;
- -pc: probabilidade de colisão;
- -cd: duração da colisão;
- -vmax: velocidade máxima;
- -vmin: velocidade mínima;
- -amax: aceleração máxima;
- -amin: aceleração mínima;
  
Todos os parâmetros são opcionais, exceto o nome da rodovia. Caso algum parâmetro não seja passado, o programa irá utilizar os valores padrão.