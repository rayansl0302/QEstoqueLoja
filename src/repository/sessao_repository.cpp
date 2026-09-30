#include <QSqlError>
#include "sessao_repository.h"
#include "../infra/databaseconnection_service.h"
#include <QDateTime>
#include <QDebug>
#include <QMetaType>

Sessao_repository::Sessao_repository(QObject *parent)
    : QObject(parent)
    , db(DatabaseConnection_service::db())
{
}

LogSessaoDTO Sessao_repository::lerLinha(QSqlQuery &query)
{
    LogSessaoDTO log;
    log.id = query.value("id").toLongLong();
    log.idOperador = query.value("id_operador").toLongLong();
    log.nomeOperador = query.value("nome_operador").toString();
    log.gerente = query.value("gerente").toBool();
    log.terminal = query.value("terminal").toString();
    log.entradaEm = query.value("entrada_em").toString();
    log.saidaEm = query.value("saida_em").toString();
    log.motivoSaida = query.value("motivo_saida").toString();
    log.idCaixaNoMomento = query.value("id_caixa_no_momento").toLongLong();
    return log;
}

qlonglong Sessao_repository::registrarEntrada(const SessaoDTO &sessao,
                                             qlonglong idCaixaNoMomento,
                                             QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        if (erro) *erro = "Banco de dados indisponível.";
        return 0;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO sessoes_operador (id_operador, nome_operador, gerente, terminal, "
                  "entrada_em, id_caixa_no_momento) "
                  "VALUES (:op, :nome, :gerente, :terminal, :agora, :caixa)");

    query.bindValue(":op", sessao.idOperador);
    query.bindValue(":nome", sessao.nomeOperador);
    query.bindValue(":gerente", sessao.gerente);
    query.bindValue(":terminal", sessao.terminal);
    query.bindValue(":agora", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":caixa", idCaixaNoMomento > 0 ? QVariant(idCaixaNoMomento)
                                                   : QVariant(QMetaType(QMetaType::LongLong)));

    if (!query.exec()) {
        if (erro) *erro = query.lastError().text();
        return 0;
    }

    return query.lastInsertId().toLongLong();
}

bool Sessao_repository::registrarSaida(qlonglong idLog, const QString &motivo, QString *erro)
{
    if (idLog <= 0)
        return true;    // sessão nunca chegou a ser gravada no log

    if (!DatabaseConnection_service::open()) {
        if (erro) *erro = "Banco de dados indisponível.";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE sessoes_operador SET saida_em = :agora, motivo_saida = :motivo "
                  "WHERE id = :id AND saida_em IS NULL");
    query.bindValue(":agora", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":motivo", motivo);
    query.bindValue(":id", idLog);

    if (!query.exec()) {
        if (erro) *erro = query.lastError().text();
        return false;
    }
    return true;
}

int Sessao_repository::fecharSessoesPendentes(const QString &terminal, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        if (erro) *erro = "Banco de dados indisponível.";
        return 0;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE sessoes_operador SET saida_em = :agora, motivo_saida = 'SEM_LOGOUT' "
                  "WHERE saida_em IS NULL AND terminal = :terminal");
    query.bindValue(":agora", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":terminal", terminal);

    if (!query.exec()) {
        if (erro) *erro = query.lastError().text();
        return 0;
    }
    return query.numRowsAffected();
}

QList<LogSessaoDTO> Sessao_repository::listar(int limite)
{
    QList<LogSessaoDTO> lista;

    if (!DatabaseConnection_service::open()) {
        qDebug() << "listar sessoes falhou: banco de dados indisponivel";
        return lista;
    }

    // o limite vai por extenso na string: o PostgreSQL não aceita parâmetro em LIMIT
    const int total = qBound(1, limite, 1000);

    QSqlQuery query(db);
    query.prepare("SELECT id, id_operador, nome_operador, gerente, terminal, entrada_em, saida_em, "
                  "motivo_saida, id_caixa_no_momento FROM sessoes_operador "
                  "ORDER BY id DESC LIMIT " + QString::number(total));

    if (!query.exec()) {
        qDebug() << "listar sessoes falhou:" << query.lastError().text();
        return lista;
    }

    while (query.next())
        lista.append(lerLinha(query));

    return lista;
}

bool Sessao_repository::registrarAcao(const LogAcaoDTO &acao, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        if (erro) *erro = "Banco de dados indisponível.";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO auditoria_acesso (data_hora, id_operador_sessao, nome_operador, terminal, acao, detalhe) "
                  "VALUES (:agora, :op, :nome, :terminal, :acao, :detalhe)");
    query.bindValue(":agora", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":op", acao.idOperadorSessao >= 0 ? QVariant(acao.idOperadorSessao)
                                                      : QVariant(QMetaType(QMetaType::LongLong)));
    query.bindValue(":nome", acao.nomeOperador);
    query.bindValue(":terminal", acao.terminal);
    query.bindValue(":acao", acao.acao);
    query.bindValue(":detalhe", acao.detalhe);

    if (!query.exec()) {
        if (erro) *erro = query.lastError().text();
        return false;
    }
    return true;
}

QList<LogAcaoDTO> Sessao_repository::listarAcoes(int limite)
{
    QList<LogAcaoDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;

    const int total = qBound(1, limite, 1000);
    QSqlQuery query(db);
    query.prepare("SELECT id, data_hora, id_operador_sessao, nome_operador, terminal, acao, detalhe "
                  "FROM auditoria_acesso ORDER BY id DESC LIMIT " + QString::number(total));
    if (!query.exec()) {
        qDebug() << "listar auditoria falhou:" << query.lastError().text();
        return lista;
    }
    while (query.next()) {
        LogAcaoDTO a;
        a.id = query.value("id").toLongLong();
        a.dataHora = query.value("data_hora").toString();
        a.idOperadorSessao = query.value("id_operador_sessao").isNull() ? -1
                                                                       : query.value("id_operador_sessao").toLongLong();
        a.nomeOperador = query.value("nome_operador").toString();
        a.terminal = query.value("terminal").toString();
        a.acao = query.value("acao").toString();
        a.detalhe = query.value("detalhe").toString();
        lista.append(a);
    }
    return lista;
}
