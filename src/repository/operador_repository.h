#ifndef OPERADOR_REPOSITORY_H
#define OPERADOR_REPOSITORY_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQueryModel>
#include <QList>
#include "../dto/Operador_dto.h"

class Operador_repository : public QObject
{
    Q_OBJECT
public:
    explicit Operador_repository(QObject *parent = nullptr);

    qlonglong inserir(const OperadorDTO &op, QString *erro = nullptr);
    bool atualizar(const OperadorDTO &op, QString *erro = nullptr);
    bool atualizarPin(qlonglong id, const QString &pinHash, const QString &pinSalt, QString *erro = nullptr);
    bool atualizarAtivo(qlonglong id, bool ativo, QString *erro = nullptr);
    bool registrarTentativa(qlonglong id, int tentativas, bool bloqueado, QString *erro = nullptr);
    OperadorDTO getPorId(qlonglong id);
    QList<OperadorDTO> listar(bool somenteAtivos);
    void listar(QSqlQueryModel *model);
    bool nomeExiste(const QString &nome, qlonglong ignorarId = 0);
    // chaves do módulo de caixa compartilhadas entre os terminais (ex.: PIN do gerente)
    QString getConfigCaixa(const QString &chave);
    bool setConfigCaixa(const QString &chave, const QString &valor, QString *erro = nullptr);

private:
    QSqlDatabase db;
    OperadorDTO lerLinha(class QSqlQuery &query);
};

#endif // OPERADOR_REPOSITORY_H
