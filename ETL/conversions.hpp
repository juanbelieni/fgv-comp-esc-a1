#ifndef CONVERSIONS_HPP_
#define CONVERSIONS_HPP_

#include <iostream>

// Observação: sim, eu sei que existem funções que fazem isso e fiz o benchmark,
//              então sei que escrever as minhas dá mais controle e é mais rápido.

/**
 *  @brief Retorna a representação em inteiro de uma string.
 *  
 *  @param str O array de caracteres a ser convertido.
 *  @param index O índice do primeiro caractere.
 *  @param end O caractere que termina a parte convertida da string.
 *  @return O inteiro representado pela string.
 */
inline int str_to_int(const char* str, int& index, char end) {
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
 *  @param str O array de caracteres a ser convertido.
 *  @param index O índice do primeiro caractere.
 *  @param end O caractere que termina a parte convertida da string.
 *  @return O double representado pela string.
 */
inline double str_to_double(const char* str, int& index, char end) {
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

// Uma placa tem 7 caracteres, mas o compilador usaria 8 de qualquer jeito, então
// é mais fácil alocar 8 bytes e usar o último para o '\0', assim o cout funciona
struct Plate {
    char plate[8];  // 7 caracteres + '\0'

    Plate() {
        plate[7] = '\0';
    }

    Plate(char* symbols) {
        for (int i = 0; i < 7; i++) {
            plate[i] = symbols[i];
        }
        plate[7] = '\0';
    }

    bool operator==(const Plate& other) const {
        for (int i = 0; i < 7; i++) {
            if (plate[i] != other.plate[i])
                return false;
        }
        return true;
    }

    bool operator==(const char* const other) const {
        for (int i = 0; i < 7; i++) {
            if (plate[i] != other[i])
                return false;
        }
        return true;
    }
};

// Cria um hash para a estrutura Plate reinterpretando o array de char como um inteiro
template<typename T>
struct Hash {
    std::size_t operator()(const T& plate) const noexcept {
        return *reinterpret_cast<const size_t*>(plate.plate);
    }
};

std::ostream &operator<<(std::ostream &os, const Plate &plate) {
    os << plate.plate;
    return os;
}

#endif  // CONVERSIONS_HPP_
