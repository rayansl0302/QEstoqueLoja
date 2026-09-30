#ifndef SESSAO_REPOSITORY_H
#define SESSAO_REPOSITORY_H

#include <QObject>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QList>
#include "../dto/Sessao_dto.h"

// Log de sessões do operador (tabela sessoes_operador). Append-only: a entrada é gravada
// no login e a saída no logout/troca. Sessão sem saída é o sinal de "o programa foi
// encerrado sem logout", que fica igual ao estado de um caixa aberto depois de um crash.
class Sessao_repository : public QObject
{
    Q_OBJECT
public:
    explicit Sessao_repository(QObject *parent = nullptr);

    // Grava a entrada da sessão e devolve o id do log (para fechar depois).
    qlonglong registrarEntrada(const SessaoDTO &sessao, qlonglong idCaixaNoMomento, QString *erro = nullptr);
    bool registrarSaida(qlonglong idLog, const QString &motivo, QString *erro = nullptr);

    // Fecha sem motivo as sessões que ficaram abertas neste terminal (programa anterior
    // encerrado sem logout).
    int fecharSessoesPendentes(const QString &terminal, QString *erro = nullptr);

    QList<LogSessaoDTO> listar(int limite = 100);

    // auditoria de ações sensíveis (tabela auditoria_acesso, migração 17)
    bool registrarAcao(const LogAcaoDTO &acao, QString *erro = nullptr);
    QList<LogAcaoDTO> listarAcoes(int limite = 200);

private:
    QSqlDatabase db;
    LogSessaoDTO lerLinha(QSqlQuery &query);
};

#endif // SESSAO_REPOSITORY_H
