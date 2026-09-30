#ifndef OPERADOR_SERVICE_H
#define OPERADOR_SERVICE_H

#include <QObject>
#include <QSqlQueryModel>
#include "../repository/operador_repository.h"
#include "../repository/caixa_repository.h"

class Operador_service : public QObject
{
    Q_OBJECT
public:
    struct Resultado {
        bool ok;
        QString msg;
        qlonglong id = 0;
    };
    static constexpr int MaxTentativas = 3;

    explicit Operador_service(QObject *parent = nullptr);

    Resultado cadastrar(const QString &nome, const QString &pin);
    Resultado renomear(qlonglong id, const QString &nome);
    Resultado redefinirPin(qlonglong id, const QString &pin);
    Resultado definirAtivo(qlonglong id, bool ativo);
    Resultado desbloquear(qlonglong id);

    // Confere o PIN; a cada erro soma uma tentativa e, na terceira, bloqueia o operador.
    Resultado validarPin(qlonglong id, const QString &pin);

    // PIN do gerente (compartilhado entre os terminais): protege o cadastro de operadores,
    // para que um operador não consiga redefinir/desbloquear o PIN de outro.
    bool gerentePinDefinido();
    Resultado definirGerentePin(const QString &pin);
    Resultado validarGerentePin(const QString &pin);

    OperadorDTO getPorId(qlonglong id);
    QList<OperadorDTO> listar(bool somenteAtivos);
    void listar(QSqlQueryModel *model);

    static bool pinValido(const QString &pin);

private:
    Operador_repository repo;
    Caixa_repository caixaRepo;
    static QString gerarSalt();
    static QString hashPin(const QString &pin, const QString &salt);
};

#endif // OPERADOR_SERVICE_H
