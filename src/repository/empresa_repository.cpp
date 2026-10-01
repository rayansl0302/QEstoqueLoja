#include "empresa_repository.h"
#include "../infra/databaseconnection_service.h"
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

namespace {
EmpresaDTO ler(QSqlQuery &q)
{
    EmpresaDTO e;
    e.id = q.value("id").toLongLong();
    e.apelido = q.value("apelido").toString();
    e.cnpj = q.value("cnpj").toString();
    e.razaoSocial = q.value("razao_social").toString();
    e.ativa = q.value("ativa").toBool();
    return e;
}
void setErro(QString *erro, const QString &t) { if (erro) *erro = t; }
}

Empresa_repository::Empresa_repository(QObject *parent)
    : QObject(parent)
    , db(DatabaseConnection_service::db())
{
}

QList<EmpresaDTO> Empresa_repository::listar(bool somenteAtivas)
{
    QList<EmpresaDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery q(db);
    q.prepare(QString("SELECT id, apelido, cnpj, razao_social, ativa FROM empresas %1 ORDER BY id")
                  .arg(somenteAtivas ? "WHERE ativa = :a" : ""));
    if (somenteAtivas)
        q.bindValue(":a", true);
    if (!q.exec()) {
        qDebug() << "Empresa_repository::listar:" << q.lastError().text();
        return lista;
    }
    while (q.next())
        lista.append(ler(q));
    return lista;
}

EmpresaDTO Empresa_repository::getPorId(qlonglong id)
{
    EmpresaDTO e;
    if (!DatabaseConnection_service::open())
        return e;
    QSqlQuery q(db);
    q.prepare("SELECT id, apelido, cnpj, razao_social, ativa FROM empresas WHERE id = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        e = ler(q);
    return e;
}

EmpresaDTO Empresa_repository::getPorCnpj(const QString &cnpj)
{
    EmpresaDTO e;
    if (cnpj.isEmpty() || !DatabaseConnection_service::open())
        return e;
    QSqlQuery q(db);
    q.prepare("SELECT id, apelido, cnpj, razao_social, ativa FROM empresas WHERE cnpj = :c");
    q.bindValue(":c", cnpj);
    if (q.exec() && q.next())
        e = ler(q);
    return e;
}

qlonglong Empresa_repository::inserir(const EmpresaDTO &empresa, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return -1;
    }
    QSqlQuery q(db);
    q.prepare("INSERT INTO empresas (apelido, cnpj, razao_social, ativa, criado_em) "
              "VALUES (:ap, :cnpj, :razao, :ativa, :agora)");
    q.bindValue(":ap", empresa.apelido);
    q.bindValue(":cnpj", empresa.cnpj);
    q.bindValue(":razao", empresa.razaoSocial);
    q.bindValue(":ativa", empresa.ativa);
    q.bindValue(":agora", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return -1;
    }
    QVariant id = q.lastInsertId();
    if (!id.isValid()) {                       // PostgreSQL sem RETURNING
        QSqlQuery q2(db);
        if (q2.exec("SELECT MAX(id) FROM empresas") && q2.next())
            id = q2.value(0);
    }
    return id.toLongLong();
}

bool Empresa_repository::atualizar(const EmpresaDTO &empresa, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE empresas SET apelido = :ap, cnpj = :cnpj, razao_social = :razao WHERE id = :id");
    q.bindValue(":ap", empresa.apelido);
    q.bindValue(":cnpj", empresa.cnpj);
    q.bindValue(":razao", empresa.razaoSocial);
    q.bindValue(":id", empresa.id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    return true;
}

bool Empresa_repository::definirAtiva(qlonglong id, bool ativa, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE empresas SET ativa = :a WHERE id = :id");
    q.bindValue(":a", ativa);
    q.bindValue(":id", id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    return true;
}
