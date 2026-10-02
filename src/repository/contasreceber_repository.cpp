#include "contasreceber_repository.h"
#include "../infra/databaseconnection_service.h"
#include "../infra/empresaativa.h"
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QMetaType>

namespace {
// saldo de uma dívida manual / de uma venda a prazo do PDV (mesma conta da tela de pagamentos à prazo)
const char *kSaldoDivida =
    "d.valor_total - COALESCE((SELECT SUM(p.valor) FROM pagamentos_divida p "
    "WHERE p.id_divida = d.id AND p.cancelado = 0), 0)";
const char *kSaldoPrazo =
    "v.valor_final - COALESCE((SELECT SUM(e.total) FROM entradas_vendas e WHERE e.id_venda = v.id), 0)";

void setErro(QString *erro, const QString &t) { if (erro) *erro = t; }
QVariant nuloInt() { return QVariant(QMetaType(QMetaType::LongLong)); }

QString normalizaDataHora(const QString &texto)
{
    QDateTime d = QDateTime::fromString(texto, "yyyy-MM-dd HH:mm:ss");
    if (!d.isValid())
        d = QDateTime::fromString(texto, Qt::ISODate);
    return d.isValid() ? d.toString("yyyy-MM-dd HH:mm:ss") : texto;
}

DividaDTO lerDivida(QSqlQuery &q)
{
    DividaDTO d;
    d.id = q.value("id").toLongLong();
    d.idCliente = q.value("id_cliente").toLongLong();
    d.idEmpresa = q.value("id_empresa").toLongLong();
    d.nomeCliente = q.value("nome_cliente").toString();
    d.dataCompra = q.value("data_compra").toString();
    d.descricao = q.value("descricao").toString();
    d.valorTotal = q.value("valor_total").toDouble();
    d.saldo = q.value("saldo").toDouble();
    d.observacao = q.value("observacao").toString();
    d.status = q.value("status").toString();
    d.motivoCancelamento = q.value("motivo_cancelamento").toString();
    d.idOperadorSessao = q.value("id_operador_sessao").isNull() ? -1 : q.value("id_operador_sessao").toLongLong();
    d.criadoEm = q.value("criado_em").toString();
    return d;
}

PagamentoDividaDTO lerPagamento(QSqlQuery &q)
{
    PagamentoDividaDTO p;
    p.id = q.value("id").toLongLong();
    p.idDivida = q.value("id_divida").toLongLong();
    p.dataPagamento = q.value("data_pagamento").toString();
    p.valor = q.value("valor").toDouble();
    p.formaPagamento = q.value("forma_pagamento").toString();
    p.observacao = q.value("observacao").toString();
    p.idOperadorSessao = q.value("id_operador_sessao").isNull() ? -1 : q.value("id_operador_sessao").toLongLong();
    p.idCaixa = q.value("id_caixa").toLongLong();
    p.cancelado = q.value("cancelado").toInt() != 0;
    p.motivoCancelamento = q.value("motivo_cancelamento").toString();
    p.criadoEm = q.value("criado_em").toString();
    return p;
}
}

ContasReceber_repository::ContasReceber_repository(QObject *parent)
    : QObject(parent)
    , db(DatabaseConnection_service::db())
{
}

// ------------------------------------------------------------------ dívidas manuais

qlonglong ContasReceber_repository::inserirDivida(const DividaDTO &d, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return -1;
    }
    QSqlQuery q(db);
    q.prepare("INSERT INTO dividas (id_cliente, id_empresa, data_compra, descricao, valor_total, observacao, status, "
              "id_operador_sessao, criado_em) VALUES (:cli, :emp, :data, :desc, :valor, :obs, :status, :op, :criado)");
    q.bindValue(":cli", d.idCliente);
    q.bindValue(":emp", d.idEmpresa > 0 ? d.idEmpresa : EmpresaAtiva::id());
    q.bindValue(":data", d.dataCompra);
    q.bindValue(":desc", d.descricao);
    q.bindValue(":valor", d.valorTotal);
    q.bindValue(":obs", d.observacao);
    q.bindValue(":status", d.status);
    q.bindValue(":op", d.idOperadorSessao >= 0 ? QVariant(d.idOperadorSessao) : nuloInt());
    q.bindValue(":criado", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return -1;
    }
    QVariant id = q.lastInsertId();
    if (!id.isValid()) {                       // PostgreSQL sem RETURNING
        QSqlQuery q2(db);
        if (q2.exec("SELECT MAX(id) FROM dividas") && q2.next())
            id = q2.value(0);
    }
    return id.toLongLong();
}

DividaDTO ContasReceber_repository::getDivida(qlonglong id)
{
    DividaDTO d;
    if (!DatabaseConnection_service::open())
        return d;
    QSqlQuery q(db);
    q.prepare(QString("SELECT d.*, c.nome AS nome_cliente, %1 AS saldo FROM dividas d "
                      "LEFT JOIN clientes c ON c.id = d.id_cliente WHERE d.id = :id").arg(kSaldoDivida));
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        d = lerDivida(q);
    return d;
}

QList<DividaDTO> ContasReceber_repository::listarDividasDoCliente(qlonglong idCliente, qlonglong idEmpresa,
                                                                  bool somenteAbertas)
{
    QList<DividaDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QString sql = QString("SELECT d.*, c.nome AS nome_cliente, %1 AS saldo FROM dividas d "
                          "LEFT JOIN clientes c ON c.id = d.id_cliente WHERE d.id_cliente = :cli ").arg(kSaldoDivida);
    if (idEmpresa > 0)
        sql += "AND d.id_empresa = :emp ";
    if (somenteAbertas)
        sql += "AND d.status = 'ABERTA' ";
    sql += "ORDER BY d.data_compra, d.id";
    QSqlQuery q(db);
    q.prepare(sql);
    q.bindValue(":cli", idCliente);
    if (idEmpresa > 0)
        q.bindValue(":emp", idEmpresa);
    if (!q.exec()) {
        qDebug() << "listarDividasDoCliente:" << q.lastError().text();
        return lista;
    }
    while (q.next())
        lista.append(lerDivida(q));
    return lista;
}

bool ContasReceber_repository::atualizarStatusDivida(qlonglong id, const QString &status, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE dividas SET status = :s WHERE id = :id AND status <> 'CANCELADA'");
    q.bindValue(":s", status);
    q.bindValue(":id", id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    return true;
}

bool ContasReceber_repository::cancelarDivida(qlonglong id, const QString &motivo, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE dividas SET status = 'CANCELADA', motivo_cancelamento = :m WHERE id = :id AND status = 'ABERTA'");
    q.bindValue(":m", motivo);
    q.bindValue(":id", id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() != 1) {
        setErro(erro, "Só é possível cancelar lançamento em aberto.");
        return false;
    }
    return true;
}

// ------------------------------------------------------------------ pagamentos

qlonglong ContasReceber_repository::inserirPagamento(const PagamentoDividaDTO &p, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return -1;
    }
    QSqlQuery q(db);
    q.prepare("INSERT INTO pagamentos_divida (id_divida, data_pagamento, valor, forma_pagamento, observacao, "
              "id_operador_sessao, id_caixa, cancelado, criado_em) "
              "VALUES (:div, :data, :valor, :forma, :obs, :op, :caixa, 0, :criado)");
    q.bindValue(":div", p.idDivida);
    q.bindValue(":data", p.dataPagamento);
    q.bindValue(":valor", p.valor);
    q.bindValue(":forma", p.formaPagamento);
    q.bindValue(":obs", p.observacao);
    q.bindValue(":op", p.idOperadorSessao >= 0 ? QVariant(p.idOperadorSessao) : nuloInt());
    q.bindValue(":caixa", p.idCaixa > 0 ? QVariant(p.idCaixa) : nuloInt());
    q.bindValue(":criado", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return -1;
    }
    QVariant id = q.lastInsertId();
    if (!id.isValid()) {
        QSqlQuery q2(db);
        if (q2.exec("SELECT MAX(id) FROM pagamentos_divida") && q2.next())
            id = q2.value(0);
    }
    return id.toLongLong();
}

PagamentoDividaDTO ContasReceber_repository::getPagamento(qlonglong id)
{
    PagamentoDividaDTO p;
    if (!DatabaseConnection_service::open())
        return p;
    QSqlQuery q(db);
    q.prepare("SELECT * FROM pagamentos_divida WHERE id = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        p = lerPagamento(q);
    return p;
}

QList<PagamentoDividaDTO> ContasReceber_repository::listarPagamentos(qlonglong idDivida)
{
    QList<PagamentoDividaDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery q(db);
    q.prepare("SELECT * FROM pagamentos_divida WHERE id_divida = :d ORDER BY data_pagamento, id");
    q.bindValue(":d", idDivida);
    if (q.exec())
        while (q.next())
            lista.append(lerPagamento(q));
    return lista;
}

bool ContasReceber_repository::cancelarPagamento(qlonglong id, const QString &motivo, qlonglong idOperadorSessao,
                                                 QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE pagamentos_divida SET cancelado = 1, motivo_cancelamento = :m, cancelado_por = :op, "
              "cancelado_em = :em WHERE id = :id AND cancelado = 0");
    q.bindValue(":m", motivo);
    q.bindValue(":op", idOperadorSessao >= 0 ? QVariant(idOperadorSessao) : nuloInt());
    q.bindValue(":em", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    q.bindValue(":id", id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() != 1) {
        setErro(erro, "Este pagamento já foi estornado.");
        return false;
    }
    return true;
}

// ------------------------------------------------------------------ fiado do PDV

QList<VendaPrazoAbertaDTO> ContasReceber_repository::listarVendasPrazoAbertas(qlonglong idCliente, qlonglong idEmpresa)
{
    QList<VendaPrazoAbertaDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QString sql = QString("SELECT v.id, v.id_empresa, v.data_hora, v.valor_final, %1 AS saldo FROM vendas2 v "
                          "WHERE v.id_cliente = :cli AND v.forma_pagamento = 'Prazo' ").arg(kSaldoPrazo);
    if (idEmpresa > 0)
        sql += "AND v.id_empresa = :emp ";
    sql += "ORDER BY v.data_hora, v.id";
    QSqlQuery q(db);
    q.prepare(sql);
    q.bindValue(":cli", idCliente);
    if (idEmpresa > 0)
        q.bindValue(":emp", idEmpresa);
    if (!q.exec()) {
        qDebug() << "listarVendasPrazoAbertas:" << q.lastError().text();
        return lista;
    }
    while (q.next()) {
        VendaPrazoAbertaDTO v;
        v.idVenda = q.value(0).toLongLong();
        v.idEmpresa = q.value(1).toLongLong();
        v.dataHora = normalizaDataHora(q.value(2).toString());
        v.valorFinal = q.value(3).toDouble();
        v.saldo = q.value(4).toDouble();
        if (v.saldo > 0.004)
            lista.append(v);
    }
    return lista;
}

qlonglong ContasReceber_repository::inserirEntradaVenda(qlonglong idVenda, double valor, const QString &forma,
                                                        const QString &dataHora, qlonglong idCaixa,
                                                        qlonglong idOperadorSessao, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return -1;
    }
    QSqlQuery q(db);
    q.prepare("INSERT INTO entradas_vendas (id_venda, total, data_hora, forma_pagamento, valor_recebido, troco, taxa, "
              "valor_final, desconto, id_caixa, id_operador_sessao) "
              "VALUES (:venda, :valor, :data, :forma, :valor, 0, 0, :valor, 0, :caixa, :op)");
    q.bindValue(":venda", idVenda);
    q.bindValue(":valor", valor);
    q.bindValue(":data", dataHora);
    q.bindValue(":forma", forma);
    q.bindValue(":caixa", idCaixa > 0 ? QVariant(idCaixa) : nuloInt());
    q.bindValue(":op", idOperadorSessao >= 0 ? QVariant(idOperadorSessao) : nuloInt());
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return -1;
    }
    QVariant id = q.lastInsertId();
    if (!id.isValid()) {
        QSqlQuery q2(db);
        if (q2.exec("SELECT MAX(id) FROM entradas_vendas") && q2.next())
            id = q2.value(0);
    }
    return id.toLongLong();
}

bool ContasReceber_repository::atualizarEstaPago(qlonglong idVenda, bool pago)
{
    if (!DatabaseConnection_service::open())
        return false;
    QSqlQuery q(db);
    q.prepare("UPDATE vendas2 SET esta_pago = :v WHERE id = :id");
    q.bindValue(":v", pago ? 1 : 0);
    q.bindValue(":id", idVenda);
    return q.exec();
}

// ------------------------------------------------------------------ totais

QMap<qlonglong, double> ContasReceber_repository::saldosManuaisPorCliente(qlonglong idEmpresa)
{
    QMap<qlonglong, double> mapa;
    if (!DatabaseConnection_service::open())
        return mapa;
    QSqlQuery q(db);
    q.prepare(QString("SELECT d.id_cliente, SUM(%1) FROM dividas d WHERE d.status = 'ABERTA' %2 GROUP BY d.id_cliente")
                  .arg(kSaldoDivida, idEmpresa > 0 ? "AND d.id_empresa = :emp" : ""));
    if (idEmpresa > 0)
        q.bindValue(":emp", idEmpresa);
    if (q.exec())
        while (q.next())
            mapa[q.value(0).toLongLong()] = q.value(1).toDouble();
    return mapa;
}

QMap<qlonglong, double> ContasReceber_repository::saldosPrazoPorCliente(qlonglong idEmpresa)
{
    QMap<qlonglong, double> mapa;
    if (!DatabaseConnection_service::open())
        return mapa;
    QSqlQuery q(db);
    // só saldos positivos entram (venda paga a mais não "abate" a dívida de outra)
    q.prepare(QString("SELECT cliente_id, SUM(saldo) FROM ("
                      "SELECT v.id_cliente AS cliente_id, %1 AS saldo FROM vendas2 v "
                      "WHERE v.forma_pagamento = 'Prazo' %2) t WHERE saldo > 0.004 GROUP BY cliente_id")
                  .arg(kSaldoPrazo, idEmpresa > 0 ? "AND v.id_empresa = :emp" : ""));
    if (idEmpresa > 0)
        q.bindValue(":emp", idEmpresa);
    if (q.exec())
        while (q.next())
            mapa[q.value(0).toLongLong()] = q.value(1).toDouble();
    return mapa;
}

QMap<qlonglong, QString> ContasReceber_repository::ultimaCompraPorCliente(qlonglong idEmpresa)
{
    QMap<qlonglong, QString> mapa;
    if (!DatabaseConnection_service::open())
        return mapa;
    auto juntar = [&](const QString &sql) {
        QSqlQuery q(db);
        q.prepare(sql);
        if (idEmpresa > 0)
            q.bindValue(":emp", idEmpresa);
        if (!q.exec())
            return;
        while (q.next()) {
            const qlonglong cli = q.value(0).toLongLong();
            const QString d = normalizaDataHora(q.value(1).toString()).left(10);
            if (!d.isEmpty() && d > mapa.value(cli))
                mapa[cli] = d;
        }
    };
    juntar(QString("SELECT id_cliente, MAX(data_compra) FROM dividas WHERE status <> 'CANCELADA' %1 GROUP BY id_cliente")
               .arg(idEmpresa > 0 ? "AND id_empresa = :emp" : ""));
    juntar(QString("SELECT id_cliente, MAX(data_hora) FROM vendas2 WHERE forma_pagamento = 'Prazo' %1 GROUP BY id_cliente")
               .arg(idEmpresa > 0 ? "AND id_empresa = :emp" : ""));
    return mapa;
}

// ------------------------------------------------------------------ extrato

namespace {
QString nomeOperador(QSqlDatabase &db, qlonglong id, QMap<qlonglong, QString> &cache)
{
    if (id < 0)
        return QString();
    if (id == 0)
        return QStringLiteral("Gerente");
    if (cache.contains(id))
        return cache.value(id);
    QSqlQuery q(db);
    q.prepare("SELECT nome FROM operadores WHERE id = :id");
    q.bindValue(":id", id);
    const QString nome = (q.exec() && q.next()) ? q.value(0).toString() : QString();
    cache.insert(id, nome);
    return nome;
}
}

QList<LinhaExtratoDTO> ContasReceber_repository::linhasManuais(qlonglong idCliente, qlonglong idEmpresa)
{
    QList<LinhaExtratoDTO> linhas;
    QMap<qlonglong, QString> ops;
    for (const DividaDTO &d : listarDividasDoCliente(idCliente, idEmpresa, false)) {
        const QList<PagamentoDividaDTO> pagamentos = listarPagamentos(d.id);
        bool temPagamentoValido = false;
        for (const PagamentoDividaDTO &p : pagamentos)
            if (!p.cancelado)
                temPagamentoValido = true;

        LinhaExtratoDTO l;
        l.tipo = d.status == kDividaCancelada ? LinhaExtratoDTO::DividaCancelada : LinhaExtratoDTO::Compra;
        l.data = d.dataCompra + " 00:00:00";
        l.descricao = d.descricao + (d.status == kDividaCancelada && !d.motivoCancelamento.isEmpty()
                                         ? " — cancelada: " + d.motivoCancelamento : QString());
        l.valor = d.valorTotal;
        l.efeito = d.status == kDividaCancelada ? 0 : d.valorTotal;
        l.idEmpresa = d.idEmpresa;
        l.idDivida = d.id;
        l.operador = nomeOperador(db, d.idOperadorSessao, ops);
        l.observacao = d.observacao;
        l.cancelavel = d.aberta() && !temPagamentoValido;
        linhas.append(l);

        for (const PagamentoDividaDTO &p : pagamentos) {
            LinhaExtratoDTO lp;
            lp.tipo = p.cancelado ? LinhaExtratoDTO::PagamentoEstornado : LinhaExtratoDTO::Pagamento;
            lp.data = normalizaDataHora(p.dataPagamento);
            lp.descricao = QStringLiteral("Pagamento (%1) — %2").arg(p.formaPagamento, d.descricao)
                           + (p.cancelado && !p.motivoCancelamento.isEmpty() ? " — estornado: " + p.motivoCancelamento : QString());
            lp.valor = p.valor;
            lp.efeito = p.cancelado ? 0 : -p.valor;
            lp.idEmpresa = d.idEmpresa;
            lp.idDivida = d.id;
            lp.idPagamento = p.id;
            lp.operador = nomeOperador(db, p.idOperadorSessao, ops);
            lp.observacao = p.observacao;
            lp.estornavel = !p.cancelado;
            linhas.append(lp);
        }
    }
    return linhas;
}

QList<LinhaExtratoDTO> ContasReceber_repository::linhasDoPdv(qlonglong idCliente, qlonglong idEmpresa)
{
    QList<LinhaExtratoDTO> linhas;
    if (!DatabaseConnection_service::open())
        return linhas;
    QMap<qlonglong, QString> ops;

    QSqlQuery q(db);
    q.prepare(QString("SELECT v.id, v.data_hora, v.valor_final, v.id_empresa, v.id_operador_sessao FROM vendas2 v "
                      "WHERE v.id_cliente = :cli AND v.forma_pagamento = 'Prazo' %1")
                  .arg(idEmpresa > 0 ? "AND v.id_empresa = :emp" : ""));
    q.bindValue(":cli", idCliente);
    if (idEmpresa > 0)
        q.bindValue(":emp", idEmpresa);
    if (q.exec()) {
        while (q.next()) {
            LinhaExtratoDTO l;
            l.tipo = LinhaExtratoDTO::VendaPrazo;
            l.idVenda = q.value(0).toLongLong();
            l.data = normalizaDataHora(q.value(1).toString());
            l.descricao = QStringLiteral("Venda a prazo #%1 (PDV)").arg(l.idVenda);
            l.valor = q.value(2).toDouble();
            l.efeito = l.valor;
            l.idEmpresa = q.value(3).toLongLong();
            l.operador = nomeOperador(db, q.value(4).isNull() ? -1 : q.value(4).toLongLong(), ops);
            linhas.append(l);
        }
    }

    QSqlQuery e(db);
    e.prepare(QString("SELECT e.id, e.id_venda, e.data_hora, e.total, e.forma_pagamento, e.id_operador_sessao, v.id_empresa "
                      "FROM entradas_vendas e JOIN vendas2 v ON v.id = e.id_venda "
                      "WHERE v.id_cliente = :cli AND v.forma_pagamento = 'Prazo' %1")
                  .arg(idEmpresa > 0 ? "AND v.id_empresa = :emp" : ""));
    e.bindValue(":cli", idCliente);
    if (idEmpresa > 0)
        e.bindValue(":emp", idEmpresa);
    if (e.exec()) {
        while (e.next()) {
            LinhaExtratoDTO l;
            l.tipo = LinhaExtratoDTO::PagamentoVenda;
            l.idEntrada = e.value(0).toLongLong();
            l.idVenda = e.value(1).toLongLong();
            l.data = normalizaDataHora(e.value(2).toString());
            l.valor = e.value(3).toDouble();
            l.efeito = -l.valor;
            l.descricao = QStringLiteral("Pagamento da venda #%1 (%2)").arg(l.idVenda).arg(e.value(4).toString());
            l.idEmpresa = e.value(6).toLongLong();
            l.operador = nomeOperador(db, e.value(5).isNull() ? -1 : e.value(5).toLongLong(), ops);
            linhas.append(l);
        }
    }
    return linhas;
}

// ------------------------------------------------------------------ crédito do cliente

ClienteCreditoDTO ContasReceber_repository::getCredito(qlonglong idCliente)
{
    ClienteCreditoDTO c;
    if (!DatabaseConnection_service::open())
        return c;
    QSqlQuery q(db);
    q.prepare("SELECT id, nome, telefone, whatsapp, observacao, ativo, limite_credito FROM clientes WHERE id = :id");
    q.bindValue(":id", idCliente);
    if (q.exec() && q.next()) {
        c.id = q.value(0).toLongLong();
        c.nome = q.value(1).toString();
        c.telefone = q.value(2).toString();
        c.whatsapp = q.value(3).toString();
        c.observacao = q.value(4).toString();
        c.ativo = q.value(5).isNull() ? true : q.value(5).toInt() != 0;
        c.temLimite = !q.value(6).isNull();
        c.limite = c.temLimite ? q.value(6).toDouble() : 0;
    }
    return c;
}

QList<ClienteCreditoDTO> ContasReceber_repository::listarClientes(bool incluirInativos)
{
    QList<ClienteCreditoDTO> lista;
    if (!DatabaseConnection_service::open())
        return lista;
    QSqlQuery q(db);
    q.prepare(QString("SELECT id, nome, telefone, whatsapp, observacao, ativo, limite_credito FROM clientes "
                      "WHERE id > 1 %1 ORDER BY nome").arg(incluirInativos ? "" : "AND ativo = 1"));
    if (!q.exec()) {
        qDebug() << "listarClientes (crédito):" << q.lastError().text();
        return lista;
    }
    while (q.next()) {
        ClienteCreditoDTO c;
        c.id = q.value(0).toLongLong();
        c.nome = q.value(1).toString();
        c.telefone = q.value(2).toString();
        c.whatsapp = q.value(3).toString();
        c.observacao = q.value(4).toString();
        c.ativo = q.value(5).isNull() ? true : q.value(5).toInt() != 0;
        c.temLimite = !q.value(6).isNull();
        c.limite = c.temLimite ? q.value(6).toDouble() : 0;
        lista.append(c);
    }
    return lista;
}

bool ContasReceber_repository::salvarCredito(const ClienteCreditoDTO &c, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE clientes SET limite_credito = :lim, observacao = :obs, whatsapp = :wa WHERE id = :id");
    q.bindValue(":lim", c.temLimite ? QVariant(c.limite) : QVariant(QMetaType(QMetaType::Double)));
    q.bindValue(":obs", c.observacao);
    q.bindValue(":wa", c.whatsapp);
    q.bindValue(":id", c.id);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    return true;
}

bool ContasReceber_repository::definirAtivo(qlonglong idCliente, bool ativo, QString *erro)
{
    if (!DatabaseConnection_service::open()) {
        setErro(erro, "Banco de dados indisponível.");
        return false;
    }
    QSqlQuery q(db);
    q.prepare("UPDATE clientes SET ativo = :a WHERE id = :id");
    q.bindValue(":a", ativo ? 1 : 0);
    q.bindValue(":id", idCliente);
    if (!q.exec()) {
        setErro(erro, q.lastError().text());
        return false;
    }
    return true;
}

int ContasReceber_repository::quantidadeDeHistorico(qlonglong idCliente)
{
    if (!DatabaseConnection_service::open())
        return 0;
    int total = 0;
    for (const char *tabela : {"vendas2", "dividas"}) {
        QSqlQuery q(db);
        q.prepare(QString("SELECT COUNT(*) FROM %1 WHERE id_cliente = :id").arg(tabela));
        q.bindValue(":id", idCliente);
        if (q.exec() && q.next())
            total += q.value(0).toInt();
    }
    return total;
}
