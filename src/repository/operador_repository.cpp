#include "operador_repository.h"
#include "../infra/databaseconnection_service.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

namespace {
void setErro(QString *erro, const QString &texto)
{
    if (erro)
        *erro = texto;
}
}

Operador_repository::Operador_repository(QObject *parent)
    : QObject{parent}
{
    db = DatabaseConnection_service::db();
}

OperadorDTO Operador_repository::lerLinha(QSqlQuery &query)
{
    OperadorDTO op;
    op.id = query.value("id").toLongLong();
    op.nome = query.value("nome").toString();
    op.pinHash = query.value("pin_hash").toString();
    op.pinSalt = query.value("pin_salt").toString();
    op.ativo = query.value("ativo").toBool();
    op.tentativasFalhas = query.value("tentativas_falhas").toInt();
    op.bloqueado = query.value("bloqueado").toBool();
    op.adicionadoEm = query.value("adicionado_em").toString();
    op.atualizadoEm = query.value("atualizado_em").toString();
    return op;
}

qlonglong Operador_repository::inserir(const OperadorDTO &op, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return -1;
    }
    QSqlQuery query(db);
    query.prepare("INSERT INTO operadores (nome, pin_hash, pin_salt, ativo, tentativas_falhas, bloqueado, "
                  "adicionado_em, atualizado_em) "
                  "VALUES (:nome, :hash, :salt, :ativo, 0, :bloq, :agora, :agora2)");
    const QDateTime agora = QDateTime::currentDateTime();
    query.bindValue(":nome", op.nome);
    query.bindValue(":hash", op.pinHash);
    query.bindValue(":salt", op.pinSalt);
    query.bindValue(":ativo", op.ativo);
    query.bindValue(":bloq", false);
    query.bindValue(":agora", agora);
    query.bindValue(":agora2", agora);
    if (!query.exec()) {
        setErro(erro, query.lastError().text());
        qDebug() << "inserir operador falhou:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool Operador_repository::atualizar(const OperadorDTO &op, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery query(db);
    query.prepare("UPDATE operadores SET nome = :nome, ativo = :ativo, atualizado_em = :agora WHERE id = :id");
    query.bindValue(":nome", op.nome);
    query.bindValue(":ativo", op.ativo);
    query.bindValue(":agora", QDateTime::currentDateTime());
    query.bindValue(":id", op.id);
    if (!query.exec()) {
        setErro(erro, query.lastError().text());
        return false;
    }
    return true;
}

bool Operador_repository::atualizarPin(qlonglong id, const QString &pinHash, const QString &pinSalt, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery query(db);
    query.prepare("UPDATE operadores SET pin_hash = :hash, pin_salt = :salt, tentativas_falhas = 0, "
                  "bloqueado = :bloq, atualizado_em = :agora WHERE id = :id");
    query.bindValue(":hash", pinHash);
    query.bindValue(":salt", pinSalt);
    query.bindValue(":bloq", false);
    query.bindValue(":agora", QDateTime::currentDateTime());
    query.bindValue(":id", id);
    if (!query.exec()) {
        setErro(erro, query.lastError().text());
        return false;
    }
    return true;
}

bool Operador_repository::atualizarAtivo(qlonglong id, bool ativo, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery query(db);
    query.prepare("UPDATE operadores SET ativo = :ativo, atualizado_em = :agora WHERE id = :id");
    query.bindValue(":ativo", ativo);
    query.bindValue(":agora", QDateTime::currentDateTime());
    query.bindValue(":id", id);
    if (!query.exec()) {
        setErro(erro, query.lastError().text());
        return false;
    }
    return true;
}

bool Operador_repository::registrarTentativa(qlonglong id, int tentativas, bool bloqueado, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery query(db);
    query.prepare("UPDATE operadores SET tentativas_falhas = :t, bloqueado = :b, atualizado_em = :agora "
                  "WHERE id = :id");
    query.bindValue(":t", tentativas);
    query.bindValue(":b", bloqueado);
    query.bindValue(":agora", QDateTime::currentDateTime());
    query.bindValue(":id", id);
    if (!query.exec()) {
        setErro(erro, query.lastError().text());
        return false;
    }
    return true;
}

OperadorDTO Operador_repository::getPorId(qlonglong id)
{
    OperadorDTO op;
    if (!DatabaseConnection_service::open())
        return op;
    QSqlQuery query(db);
    query.prepare("SELECT * FROM operadores WHERE id = :id");
    query.bindValue(":id", id);
    if (!query.exec() || !query.next())
        return op;
    return lerLinha(query);
}

QList<OperadorDTO> Operador_repository::listar(bool somenteAtivos)
{
    QList<OperadorDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery query(db);
    QString sql = "SELECT * FROM operadores ";
    if (somenteAtivos)
        sql += "WHERE ativo = :ativo ";
    sql += "ORDER BY nome";
    query.prepare(sql);
    if (somenteAtivos)
        query.bindValue(":ativo", true);
    if (!query.exec()) {
        qDebug() << "listar operadores falhou:" << query.lastError().text();
        return lista;
    }
    while (query.next())
        lista << lerLinha(query);
    return lista;
}

void Operador_repository::listar(QSqlQueryModel *model)
{
    if (!model || !DatabaseConnection_service::open())
        return;
    QSqlQuery query(db);
    query.prepare("SELECT id, nome, "
                  "CASE WHEN ativo THEN 'Sim' ELSE 'Não' END AS ativo, "
                  "CASE WHEN bloqueado THEN 'Sim' ELSE 'Não' END AS bloqueado, "
                  "tentativas_falhas FROM operadores ORDER BY nome");
    if (!query.exec()) {
        qDebug() << "listar operadores (model) falhou:" << query.lastError().text();
        return;
    }
    model->setQuery(std::move(query));
}

bool Operador_repository::nomeExiste(const QString &nome, qlonglong ignorarId)
{
    if (!DatabaseConnection_service::open())
        return false;
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM operadores WHERE UPPER(TRIM(nome)) = UPPER(TRIM(:nome)) AND id <> :id");
    query.bindValue(":nome", nome);
    query.bindValue(":id", ignorarId);
    if (!query.exec() || !query.next())
        return false;
    return query.value(0).toInt() > 0;
}

QString Operador_repository::getConfigCaixa(const QString &chave)
{
    if (!DatabaseConnection_service::open())
        return QString();
    QSqlQuery query(db);
    query.prepare("SELECT valor FROM config_caixa WHERE chave = :c");
    query.bindValue(":c", chave);
    if (!query.exec() || !query.next())
        return QString();
    return query.value(0).toString();
}

bool Operador_repository::setConfigCaixa(const QString &chave, const QString &valor, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery up(db);
    up.prepare("UPDATE config_caixa SET valor = :v WHERE chave = :c");
    up.bindValue(":v", valor);
    up.bindValue(":c", chave);
    if (!up.exec()) {
        setErro(erro, up.lastError().text());
        return false;
    }
    if (up.numRowsAffected() > 0)
        return true;

    QSqlQuery ins(db);
    ins.prepare("INSERT INTO config_caixa (chave, valor) VALUES (:c, :v)");
    ins.bindValue(":c", chave);
    ins.bindValue(":v", valor);
    if (!ins.exec()) {
        setErro(erro, ins.lastError().text());
        return false;
    }
    return true;
}
