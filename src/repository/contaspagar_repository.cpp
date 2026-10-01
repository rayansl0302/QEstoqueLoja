#include "contaspagar_repository.h"
#include "../infra/databaseconnection_service.h"
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QMetaType>

namespace {
const char *kColunas =
    "id, id_empresa, descricao, fornecedor, categoria, documento, valor, vencimento, status, valor_pago, "
    "forma_pagamento, pago_em, parcela, total_parcelas, grupo, observacao, motivo_cancelamento, "
    "id_operador_sessao, criado_em";

ContaPagarDTO ler(QSqlQuery &q)
{
    ContaPagarDTO c;
    c.id = q.value("id").toLongLong();
    c.idEmpresa = q.value("id_empresa").toLongLong();
    c.descricao = q.value("descricao").toString();
    c.fornecedor = q.value("fornecedor").toString();
    c.categoria = q.value("categoria").toString();
    c.documento = q.value("documento").toString();
    c.valor = q.value("valor").toDouble();
    c.vencimento = q.value("vencimento").toString();
    c.status = q.value("status").toString();
    c.valorPago = q.value("valor_pago").toDouble();
    c.formaPagamento = q.value("forma_pagamento").toString();
    c.pagoEm = q.value("pago_em").toString();
    c.parcela = q.value("parcela").toInt();
    c.totalParcelas = q.value("total_parcelas").toInt();
    c.grupo = q.value("grupo").toString();
    c.observacao = q.value("observacao").toString();
    c.motivoCancelamento = q.value("motivo_cancelamento").toString();
    c.idOperadorSessao = q.value("id_operador_sessao").isNull() ? -1 : q.value("id_operador_sessao").toLongLong();
    c.criadoEm = q.value("criado_em").toString();
    return c;
}
void setErro(QString *erro, const QString &t) { if (erro) *erro = t; }
QVariant nulo() { return QVariant(QMetaType(QMetaType::QString)); }
}

ContasPagar_repository::ContasPagar_repository(QObject *parent)
    : QObject(parent)
    , db(DatabaseConnection_service::db())
{
}

bool ContasPagar_repository::inserirVarias(const QList<ContaPagarDTO> &contas, QList<qlonglong> *ids, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    if (!db.transaction()) {
        setErro(erro, db.lastError().text());
        return false;
    }
    const QString agora = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    for (const ContaPagarDTO &c : contas) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO contas_pagar (id_empresa, descricao, fornecedor, categoria, documento, valor, "
                  "vencimento, status, parcela, total_parcelas, grupo, observacao, id_operador_sessao, criado_em) "
                  "VALUES (:emp, :desc, :forn, :cat, :doc, :valor, :venc, :status, :parc, :total, :grupo, :obs, "
                  ":opsessao, :criado)");
        q.bindValue(":emp", c.idEmpresa);
        q.bindValue(":desc", c.descricao);
        q.bindValue(":forn", c.fornecedor);
        q.bindValue(":cat", c.categoria);
        q.bindValue(":doc", c.documento);
        q.bindValue(":valor", c.valor);
        q.bindValue(":venc", c.vencimento);
        q.bindValue(":status", c.status);
        q.bindValue(":parc", c.parcela);
        q.bindValue(":total", c.totalParcelas);
        q.bindValue(":grupo", c.grupo.isEmpty() ? nulo() : QVariant(c.grupo));
        q.bindValue(":obs", c.observacao);
        q.bindValue(":opsessao", c.idOperadorSessao >= 0 ? QVariant(c.idOperadorSessao)
                                                         : QVariant(QMetaType(QMetaType::LongLong)));
        q.bindValue(":criado", agora);
        if (!q.exec()) {
            setErro(erro, q.lastError().text());
            db.rollback();
            return false;
        }
        QVariant id = q.lastInsertId();
        if (!id.isValid()) {                   // PostgreSQL sem RETURNING
            QSqlQuery q2(db);
            if (q2.exec("SELECT MAX(id) FROM contas_pagar") && q2.next())
                id = q2.value(0);
        }
        if (ids)
            ids->append(id.toLongLong());
    }
    if (!db.commit()) {
        setErro(erro, db.lastError().text());
        db.rollback();
        return false;
    }
    return true;
}

ContaPagarDTO ContasPagar_repository::getPorId(qlonglong id)
{
    ContaPagarDTO c;
    if (!DatabaseConnection_service::open())
        return c;
    QSqlQuery q(db);
    q.prepare(QString("SELECT %1 FROM contas_pagar WHERE id = :id").arg(kColunas));
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        c = ler(q);
    return c;
}

QList<ContaPagarDTO> ContasPagar_repository::listar(const FiltroContasPagarDTO &f)
{
    QList<ContaPagarDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;

    QString sql = QString("SELECT %1 FROM contas_pagar WHERE 1 = 1 ").arg(kColunas);
    if (f.idEmpresa > 0)
        sql += "AND id_empresa = :emp ";
    if (f.status == QLatin1String("VENCIDA"))
        sql += "AND status = 'ABERTA' AND vencimento < :hoje ";
    else if (!f.status.isEmpty())
        sql += "AND status = :status ";
    if (!f.vencimentoDe.isEmpty())
        sql += "AND vencimento >= :de ";
    if (!f.vencimentoAte.isEmpty())
        sql += "AND vencimento <= :ate ";
    if (!f.texto.trimmed().isEmpty())
        sql += "AND (LOWER(descricao) LIKE :txt OR LOWER(fornecedor) LIKE :txt OR LOWER(documento) LIKE :txt) ";
    sql += "ORDER BY vencimento, id";

    QSqlQuery q(db);
    q.prepare(sql);
    if (f.idEmpresa > 0)
        q.bindValue(":emp", f.idEmpresa);
    if (f.status == QLatin1String("VENCIDA"))
        q.bindValue(":hoje", QDate::currentDate().toString(Qt::ISODate));
    else if (!f.status.isEmpty())
        q.bindValue(":status", f.status);
    if (!f.vencimentoDe.isEmpty())
        q.bindValue(":de", f.vencimentoDe);
    if (!f.vencimentoAte.isEmpty())
        q.bindValue(":ate", f.vencimentoAte);
    if (!f.texto.trimmed().isEmpty())
        q.bindValue(":txt", "%" + f.texto.trimmed().toLower() + "%");
    if (!q.exec()) {
        qDebug() << "ContasPagar_repository::listar:" << q.lastError().text();
        return lista;
    }
    while (q.next())
        lista.append(ler(q));
    return lista;
}

QList<ContaPagarDTO> ContasPagar_repository::listarDoGrupo(const QString &grupo)
{
    QList<ContaPagarDTO> lista;
    if (grupo.isEmpty() || !DatabaseConnection_service::open())
        return lista;
    QSqlQuery q(db);
    q.prepare(QString("SELECT %1 FROM contas_pagar WHERE grupo = :g ORDER BY parcela").arg(kColunas));
    q.bindValue(":g", grupo);
    if (q.exec())
        while (q.next())
            lista.append(ler(q));
    return lista;
}

bool ContasPagar_repository::baixar(qlonglong id, double valorPago, const QString &forma, const QString &pagoEm,
                                    QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE contas_pagar SET status = 'PAGA', valor_pago = :vp, forma_pagamento = :forma, pago_em = :em "
              "WHERE id = :id AND status = 'ABERTA'");
    q.bindValue(":vp", valorPago);
    q.bindValue(":forma", forma);
    q.bindValue(":em", pagoEm);
    q.bindValue(":id", id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() != 1) {
        setErro(erro, "A conta não está mais em aberto (outra pessoa pode ter baixado ou cancelado).");
        return false;
    }
    return true;
}

bool ContasPagar_repository::estornarBaixa(qlonglong id, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE contas_pagar SET status = 'ABERTA', valor_pago = NULL, forma_pagamento = NULL, pago_em = NULL "
              "WHERE id = :id AND status = 'PAGA'");
    q.bindValue(":id", id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() != 1) {
        setErro(erro, "A conta não está paga.");
        return false;
    }
    return true;
}

bool ContasPagar_repository::cancelar(qlonglong id, const QString &motivo, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE contas_pagar SET status = 'CANCELADA', motivo_cancelamento = :m "
              "WHERE id = :id AND status = 'ABERTA'");
    q.bindValue(":m", motivo);
    q.bindValue(":id", id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() != 1) {
        setErro(erro, "Só é possível cancelar conta em aberto.");
        return false;
    }
    return true;
}

bool ContasPagar_repository::atualizarAberta(const ContaPagarDTO &c, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE contas_pagar SET descricao = :desc, fornecedor = :forn, categoria = :cat, documento = :doc, "
              "valor = :valor, vencimento = :venc, observacao = :obs WHERE id = :id AND status = 'ABERTA'");
    q.bindValue(":desc", c.descricao);
    q.bindValue(":forn", c.fornecedor);
    q.bindValue(":cat", c.categoria);
    q.bindValue(":doc", c.documento);
    q.bindValue(":valor", c.valor);
    q.bindValue(":venc", c.vencimento);
    q.bindValue(":obs", c.observacao);
    q.bindValue(":id", c.id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() != 1) {
        setErro(erro, "Só é possível alterar conta em aberto.");
        return false;
    }
    return true;
}

bool ContasPagar_repository::atualizarTextos(const ContaPagarDTO &c, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE contas_pagar SET descricao = :desc, fornecedor = :forn, categoria = :cat, documento = :doc, "
              "observacao = :obs WHERE id = :id AND status <> 'CANCELADA'");
    q.bindValue(":desc", c.descricao);
    q.bindValue(":forn", c.fornecedor);
    q.bindValue(":cat", c.categoria);
    q.bindValue(":doc", c.documento);
    q.bindValue(":obs", c.observacao);
    q.bindValue(":id", c.id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() != 1) {
        setErro(erro, "Conta cancelada não pode ser alterada.");
        return false;
    }
    return true;
}

ResumoContasPagarDTO ContasPagar_repository::resumo(qlonglong idEmpresa, const QString &hoje)
{
    ResumoContasPagarDTO r;
    if (!DatabaseConnection_service::open())
        return r;
    const QDate d = QDate::fromString(hoje, Qt::ISODate);
    const QString amanha = d.addDays(1).toString(Qt::ISODate);
    const QString em7 = d.addDays(7).toString(Qt::ISODate);

    QSqlQuery q(db);
    q.prepare(QString(
        "SELECT "
        "COALESCE(SUM(CASE WHEN vencimento < :hoje THEN 1 ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN vencimento < :hoje THEN valor ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN vencimento = :hoje THEN 1 ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN vencimento = :hoje THEN valor ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN vencimento >= :amanha AND vencimento <= :em7 THEN 1 ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN vencimento >= :amanha AND vencimento <= :em7 THEN valor ELSE 0 END), 0), "
        "COUNT(*), COALESCE(SUM(valor), 0) "
        "FROM contas_pagar WHERE status = 'ABERTA' %1").arg(idEmpresa > 0 ? "AND id_empresa = :emp" : ""));
    q.bindValue(":hoje", hoje);
    q.bindValue(":amanha", amanha);
    q.bindValue(":em7", em7);
    if (idEmpresa > 0)
        q.bindValue(":emp", idEmpresa);
    if (q.exec() && q.next()) {
        r.qtdVencidas = q.value(0).toInt();
        r.valorVencidas = q.value(1).toDouble();
        r.qtdHoje = q.value(2).toInt();
        r.valorHoje = q.value(3).toDouble();
        r.qtdProximos7Dias = q.value(4).toInt();
        r.valorProximos7Dias = q.value(5).toDouble();
        r.qtdAbertas = q.value(6).toInt();
        r.valorAbertas = q.value(7).toDouble();
    } else {
        qDebug() << "ContasPagar_repository::resumo:" << q.lastError().text();
    }
    return r;
}
