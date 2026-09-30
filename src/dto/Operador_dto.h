#ifndef OPERADOR_DTO_H
#define OPERADOR_DTO_H
#include <QString>

struct OperadorDTO {
    qlonglong id = 0;
    QString nome;
    QString pinHash;
    QString pinSalt;
    bool ativo = true;
    int tentativasFalhas = 0;
    bool bloqueado = false;
    QString adicionadoEm;
    QString atualizadoEm;
};

#endif // OPERADOR_DTO_H
