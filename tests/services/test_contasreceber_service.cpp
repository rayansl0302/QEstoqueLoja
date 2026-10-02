#include <QtTest>
#include <QUuid>
#include <QSqlQuery>
#include <QSqlError>
#include "test_contasreceber_service.h"
#include "services/contasreceber_service.h"
#include "services/cliente_service.h"
#include "services/caixa_service.h"
#include "services/operador_service.h"
#include "services/sessao_service.h"
#include "infra/databaseconnection_service.h"
#include "infra/empresaativa.h"
#include "dto/Sessao_dto.h"

namespace {
QSqlDatabase banco() { return DatabaseConnection_service::db(); }

QString sufixo() { return QUuid::createUuid().toString(QUuid::Id128).left(8); }

void fecharCaixaPorSql(qlonglong id)
{
    QSqlQuery q(banco());
    q.prepare("UPDATE caixas SET status = 'FECHADO', fechado_em = :f WHERE id = :id");
    q.bindValue(":f", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    q.bindValue(":id", id);
    q.exec();
}

qlonglong somaMovimentosDePagamentos()
{
    QSqlQuery q(banco());
    q.exec("SELECT COUNT(*) FROM movimentacoes_caixa WHERE tipo = 'RECEBIMENTO' AND id_pagamento_divida IS NOT NULL");
    return q.next() ? q.value(0).toLongLong() : 0;
}

// venda a prazo do PDV (vendas2 "Prazo"), com data própria
qlonglong vendaPrazo(qlonglong cliente, double valor, const QString &dataHora)
{
    QSqlQuery q(banco());
    q.prepare("INSERT INTO vendas2 (cliente, total, data_hora, forma_pagamento, valor_recebido, troco, taxa, valor_final, "
              "desconto, id_cliente, esta_pago, id_empresa) VALUES ('Cliente', :v, :d, 'Prazo', 0, 0, 0, :v, 0, :c, 0, :e)");
    q.bindValue(":v", valor);
    q.bindValue(":d", dataHora);
    q.bindValue(":c", cliente);
    q.bindValue(":e", EmpresaAtiva::id());
    [&]() { QVERIFY2(q.exec(), qPrintable(q.lastError().text())); }();
    return q.lastInsertId().toLongLong();
}
}

void TestContasReceber::init()
{
    // operador novo, logado, com o caixa dele aberto
    ContasReceber_service::definirAutorizador(nullptr);
    Operador_service os;
    const auto op = os.cadastrar("Recebe " + sufixo(), "1357");
    QVERIFY2(op.ok, qPrintable(op.msg));
    idOperador = op.id;

    SessaoDTO s;
    s.idOperador = op.id;
    s.nomeOperador = os.getPorId(op.id).nome;
    s.terminal = Caixa_service::terminalAtual();
    QString erro;
    Sessao_service::instancia()->abrir(s, &erro);
    QVERIFY2(Sessao_service::instancia()->ativa(), qPrintable(erro));

    Caixa_service cs;
    const auto cx = cs.abrirCaixa(op.id, QString(), 0, 0);
    QVERIFY2(cx.ok, qPrintable(cx.msg));
    idCaixa = cx.id;
    caixasAbertos.append(idCaixa);
}

void TestContasReceber::cleanup()
{
    for (qlonglong id : std::as_const(caixasAbertos))
        fecharCaixaPorSql(id);
    caixasAbertos.clear();
    Sessao_service::instancia()->encerrar("LOGOUT");
    ContasReceber_service::definirAutorizador(nullptr);
    EmpresaAtiva::definir(1);
}

void TestContasReceber::cleanupTestCase()
{
    Sessao_service::instancia()->encerrar("LOGOUT");
}

qlonglong TestContasReceber::novoCliente(const QString &nome)
{
    ClienteDTO c;
    c.nome = nome.isEmpty() ? "Cliente " + sufixo() : nome;
    c.ehPf = true;
    c.indIeDest = 0;
    Cliente_service cs;
    const auto r = cs.inserirCliente(c);
    [&]() { QVERIFY2(r.ok, qPrintable(r.msg)); }();
    QSqlQuery q(banco());
    q.prepare("SELECT id FROM clientes WHERE nome = :n");
    q.bindValue(":n", c.nome);
    return (q.exec() && q.next()) ? q.value(0).toLongLong() : 0;
}

DividaDTO TestContasReceber::divida(qlonglong cliente, double valor, const QString &descricao, int diasAtras)
{
    DividaDTO d;
    d.idCliente = cliente;
    d.valorTotal = valor;
    d.descricao = descricao;
    d.dataCompra = QDate::currentDate().addDays(-diasAtras).toString(Qt::ISODate);
    return d;
}

void TestContasReceber::nome_do_cliente_e_unico_sem_diferenciar_espacos_e_maiusculas()
{
    const QString base = "Maria Unica " + sufixo();
    QVERIFY(novoCliente(base) > 0);
    Cliente_service cs;
    ClienteDTO igual;
    igual.nome = "  " + base.toUpper().replace(" ", "   ") + " ";
    igual.ehPf = true;
    igual.indIeDest = 0;
    const auto r = cs.inserirCliente(igual);
    QVERIFY(!r.ok);
    QVERIFY(r.msg.contains("mesmo nome") || r.msg.contains("com este nome"));
}

void TestContasReceber::lancar_divida_exige_sessao()
{
    const qlonglong cli = novoCliente();
    ContasReceber_service cr;
    Sessao_service::instancia()->encerrar("LOGOUT");
    const auto r = cr.lancarDivida(divida(cli, 50));
    QVERIFY(!r.ok);
    QVERIFY(r.msg.contains("sessão"));
}

void TestContasReceber::lancar_divida_valida_cliente_data_e_valor()
{
    ContasReceber_service cr;
    const qlonglong cli = novoCliente();
    QVERIFY(!cr.lancarDivida(divida(1, 50)).ok);                       // Consumidor não tem fiado
    QVERIFY(!cr.lancarDivida(divida(99999999, 50)).ok);               // cliente que não existe
    QVERIFY(!cr.lancarDivida(divida(cli, 0)).ok);
    QVERIFY(!cr.lancarDivida(divida(cli, 10, "x")).ok);                // descrição curta
    DividaDTO futura = divida(cli, 10);
    futura.dataCompra = QDate::currentDate().addDays(2).toString(Qt::ISODate);
    QVERIFY(!cr.lancarDivida(futura).ok);

    const auto ok = cr.lancarDivida(divida(cli, 80.505, "Compras", 3));    // arredonda para centavos
    QVERIFY2(ok.ok, qPrintable(ok.msg));
    const DividaDTO salva = cr.getDivida(ok.id);
    QCOMPARE(salva.status, QStringLiteral("ABERTA"));
    QCOMPARE(salva.idOperadorSessao, idOperador);
    QCOMPARE(salva.idEmpresa, EmpresaAtiva::id());
    QCOMPARE(qRound(salva.valorTotal * 100), 8051);
    QCOMPARE(salva.saldo, salva.valorTotal);
}

void TestContasReceber::pagamento_parcial_abate_o_saldo_e_quita_sozinho()
{
    ContasReceber_service cr;
    const qlonglong cli = novoCliente();
    const auto l = cr.lancarDivida(divida(cli, 100));
    QVERIFY(l.ok);
    QCOMPARE(cr.totalDevido(cli), 100.0);

    const auto p1 = cr.receberPagamento(l.id, 30, "Pix", QDate::currentDate());
    QVERIFY2(p1.ok, qPrintable(p1.msg));
    DividaDTO d = cr.getDivida(l.id);
    QCOMPARE(d.saldo, 70.0);
    QCOMPARE(d.status, QStringLiteral("ABERTA"));
    QCOMPARE(cr.totalDevido(cli), 70.0);

    // várias parcelas até quitar; ao zerar vira QUITADA
    QVERIFY(cr.receberPagamento(l.id, 20, "Dinheiro", QDate::currentDate()).ok);
    const auto ultimo = cr.receberPagamento(l.id, 50, "Débito", QDate::currentDate());
    QVERIFY2(ultimo.ok, qPrintable(ultimo.msg));
    QVERIFY(ultimo.msg.contains("quitada"));
    d = cr.getDivida(l.id);
    QCOMPARE(d.status, QStringLiteral("QUITADA"));
    QCOMPARE(d.saldo, 0.0);
    QCOMPARE(cr.totalDevido(cli), 0.0);
    QVERIFY(!cr.receberPagamento(l.id, 1, "Pix", QDate::currentDate()).ok);   // já quitada
}

void TestContasReceber::pagamento_maior_que_o_saldo_e_recusado()
{
    ContasReceber_service cr;
    const qlonglong cli = novoCliente();
    const auto l = cr.lancarDivida(divida(cli, 40));
    QVERIFY(l.ok);
    const auto r = cr.receberPagamento(l.id, 40.01, "Pix", QDate::currentDate());
    QVERIFY(!r.ok);
    QVERIFY(r.msg.contains("saldo"));
    QCOMPARE(cr.getDivida(l.id).saldo, 40.0);               // nada foi gravado
    QVERIFY(!cr.receberPagamento(l.id, 10, "Cheque", QDate::currentDate()).ok);      // forma inválida
    QVERIFY(!cr.receberPagamento(l.id, 10, "Pix", QDate::currentDate().addDays(1)).ok);
    QVERIFY(!cr.receberDoCliente(cli, 41, "Pix", QDate::currentDate()).ok);          // passa do total
}

void TestContasReceber::pagamento_exige_caixa_aberto_e_entra_no_caixa()
{
    ContasReceber_service cr;
    const qlonglong cli = novoCliente();
    const auto l = cr.lancarDivida(divida(cli, 60));
    QVERIFY(l.ok);

    const qlonglong antes = somaMovimentosDePagamentos();
    QVERIFY(cr.receberPagamento(l.id, 25, "Pix", QDate::currentDate()).ok);
    QCOMPARE(somaMovimentosDePagamentos(), antes + 1);      // virou RECEBIMENTO no caixa do operador

    // com o caixa fechado o pagamento é recusado e nada fica gravado
    fecharCaixaPorSql(idCaixa);
    const auto r = cr.receberPagamento(l.id, 10, "Pix", QDate::currentDate());
    QVERIFY(!r.ok);
    QVERIFY(r.msg.contains("caixa", Qt::CaseInsensitive));
    QCOMPARE(cr.getDivida(l.id).saldo, 35.0);
    QCOMPARE(somaMovimentosDePagamentos(), antes + 1);
}

void TestContasReceber::receber_do_cliente_abate_da_mais_antiga_e_junta_o_fiado_do_pdv()
{
    ContasReceber_service cr;
    const qlonglong cli = novoCliente();
    // do mais antigo para o mais novo: venda do PDV (20 dias), dívida manual (10), dívida manual (2)
    const qlonglong venda = vendaPrazo(cli, 50, QDate::currentDate().addDays(-20).toString("yyyy-MM-dd") + " 10:00:00");
    const auto d1 = cr.lancarDivida(divida(cli, 40, "Segunda compra", 10));
    const auto d2 = cr.lancarDivida(divida(cli, 30, "Terceira compra", 2));
    QVERIFY(venda > 0 && d1.ok && d2.ok);
    QCOMPARE(cr.totalDevido(cli), 120.0);

    // 70: quita a venda do PDV (50) e abate 20 da dívida seguinte
    const auto r = cr.receberDoCliente(cli, 70, "Dinheiro", QDate::currentDate());
    QVERIFY2(r.ok, qPrintable(r.msg));
    QCOMPARE(r.quantidade, 2);
    QCOMPARE(cr.getDivida(d1.id).saldo, 20.0);
    QCOMPARE(cr.getDivida(d2.id).saldo, 30.0);
    QCOMPARE(cr.totalDevido(cli), 50.0);
    QSqlQuery q(banco());
    q.prepare("SELECT esta_pago FROM vendas2 WHERE id = :id");
    q.bindValue(":id", venda);
    QVERIFY(q.exec() && q.next());
    QVERIFY(q.value(0).toInt() != 0);                      // a venda do PDV foi quitada

    // o resto quita tudo
    const auto fim = cr.receberDoCliente(cli, 50, "Pix", QDate::currentDate());
    QVERIFY2(fim.ok, qPrintable(fim.msg));
    QCOMPARE(cr.totalDevido(cli), 0.0);
    QCOMPARE(cr.getDivida(d2.id).status, QStringLiteral("QUITADA"));
    QVERIFY(!cr.receberDoCliente(cli, 1, "Pix", QDate::currentDate()).ok);     // nada mais a receber
}

void TestContasReceber::limite_de_credito_bloqueia_e_o_gerente_libera()
{
    ContasReceber_service cr;
    const qlonglong cli = novoCliente();
    QVERIFY(cr.definirCredito(cli, true, 100, "", "").ok);
    QVERIFY(cr.lancarDivida(divida(cli, 70)).ok);
    QCOMPARE(cr.situacaoLimite(cli).disponivel, 30.0);

    const auto bloqueada = cr.lancarDivida(divida(cli, 40));
    QVERIFY(!bloqueada.ok);
    QVERIFY(bloqueada.limiteExcedido);
    QCOMPARE(cr.totalDevido(cli), 70.0);
    QVERIFY(cr.lancarDivida(divida(cli, 30)).ok);          // até o limite pode

    // o fiado do PDV também conta no limite
    const auto pdv = cr.validarVendaPrazo(cli, 1);
    QVERIFY(!pdv.ok && pdv.limiteExcedido);

    // exceção: só com autorização de gerente
    bool gerente = false;
    ContasReceber_service::definirAutorizador([&]() { return gerente; });
    QVERIFY(!cr.lancarDivida(divida(cli, 40), true).ok);
    gerente = true;
    QVERIFY(cr.lancarDivida(divida(cli, 40), true).ok);
    QCOMPARE(cr.totalDevido(cli), 140.0);

    // sem limite cadastrado não bloqueia
    const qlonglong livre = novoCliente();
    QVERIFY(cr.lancarDivida(divida(livre, 100000)).ok);
}

void TestContasReceber::cancelar_lancamento_so_sem_pagamento_e_estorno_so_gerente()
{
    ContasReceber_service cr;
    const qlonglong cli = novoCliente();
    const auto l = cr.lancarDivida(divida(cli, 90));
    QVERIFY(l.ok);
    const auto pag = cr.receberPagamento(l.id, 30, "Pix", QDate::currentDate());
    QVERIFY(pag.ok);

    QVERIFY(!cr.cancelarDivida(l.id, "lançada errado").ok);    // tem pagamento vinculado

    bool gerente = false;
    ContasReceber_service::definirAutorizador([&]() { return gerente; });
    QVERIFY(!cr.estornarPagamento(pag.id, "valor errado").ok);
    gerente = true;
    QVERIFY(!cr.estornarPagamento(pag.id, "ab").ok);           // motivo curto
    QVERIFY(cr.estornarPagamento(pag.id, "valor errado").ok);
    QVERIFY(!cr.estornarPagamento(pag.id, "valor errado").ok); // já estornado
    QCOMPARE(cr.getDivida(l.id).saldo, 90.0);                   // voltou ao saldo cheio

    // sem pagamento válido o gerente cancela, com motivo
    gerente = false;
    QVERIFY(!cr.cancelarDivida(l.id, "lançada errado").ok);
    gerente = true;
    QVERIFY(!cr.cancelarDivida(l.id, "").ok);
    QVERIFY(cr.cancelarDivida(l.id, "lançada errado").ok);
    QCOMPARE(cr.getDivida(l.id).status, QStringLiteral("CANCELADA"));
    QCOMPARE(cr.totalDevido(cli), 0.0);
    QVERIFY(!cr.receberPagamento(l.id, 10, "Pix", QDate::currentDate()).ok);
}

void TestContasReceber::estorno_com_caixa_fechado_e_recusado()
{
    ContasReceber_service cr;
    const qlonglong cli = novoCliente();
    const auto l = cr.lancarDivida(divida(cli, 50));
    const auto pag = cr.receberPagamento(l.id, 50, "Dinheiro", QDate::currentDate());
    QVERIFY(pag.ok);
    QCOMPARE(cr.getDivida(l.id).status, QStringLiteral("QUITADA"));

    fecharCaixaPorSql(idCaixa);                                // o caixa do recebimento já fechou
    const auto r = cr.estornarPagamento(pag.id, "engano");
    QVERIFY(!r.ok);
    QVERIFY(r.msg.contains("fechado"));
    QCOMPARE(cr.getDivida(l.id).status, QStringLiteral("QUITADA"));
}

void TestContasReceber::cliente_com_divida_nao_e_excluido_e_inativo_nao_compra_a_prazo()
{
    ContasReceber_service cr;
    Cliente_service cs;
    const qlonglong cli = novoCliente();
    QVERIFY(cr.lancarDivida(divida(cli, 25)).ok);

    const auto negado = cs.deletarCliente(cli);
    QVERIFY(!negado.ok);
    QVERIFY(negado.msg.contains("dívida"));

    // inativar: a dívida continua, mas ele sai da lista de venda e não compra mais a prazo
    QVERIFY(cs.listarClientesParaCompleter().filter(QString("(ID: %1)").arg(cli)).size() == 1);
    QVERIFY(cr.inativarCliente(cli).ok);
    QVERIFY(cs.listarClientesParaCompleter().filter(QString("(ID: %1)").arg(cli)).isEmpty());
    QVERIFY(!cr.lancarDivida(divida(cli, 10)).ok);
    QVERIFY(!cr.validarVendaPrazo(cli, 10).ok);
    QCOMPARE(cr.totalDevido(cli), 25.0);

    FiltroDevedoresDTO f;
    f.modo = ModoDevedores::Inativos;
    bool achou = false;
    for (const DevedorDTO &d : cr.listarDevedores(f))
        achou = achou || d.idCliente == cli;
    QVERIFY(achou);
    QVERIFY(!cr.inativarCliente(1).ok);                        // Consumidor nunca

    QVERIFY(cr.reativarCliente(cli).ok);
    QVERIFY(cr.lancarDivida(divida(cli, 10)).ok);

    // sem dívida e sem histórico pode excluir; com histórico só inativa
    const qlonglong limpo = novoCliente();
    QVERIFY(cs.deletarCliente(limpo).ok);
    const qlonglong comHistorico = novoCliente();
    const auto l = cr.lancarDivida(divida(comHistorico, 15));
    QVERIFY(cr.receberPagamento(l.id, 15, "Pix", QDate::currentDate()).ok);
    const auto semDivida = cs.deletarCliente(comHistorico);
    QVERIFY(!semDivida.ok);
    QVERIFY(semDivida.msg.contains("histórico"));
}

void TestContasReceber::extrato_junta_as_duas_fontes_com_saldo_acumulado()
{
    ContasReceber_service cr;
    const qlonglong cli = novoCliente();
    vendaPrazo(cli, 100, QDate::currentDate().addDays(-30).toString("yyyy-MM-dd") + " 09:00:00");
    const auto l = cr.lancarDivida(divida(cli, 60, "Caderneta", 5));
    QVERIFY(cr.receberPagamento(l.id, 20, "Pix", QDate::currentDate()).ok);

    const QList<LinhaExtratoDTO> linhas = cr.extrato(cli);
    QCOMPARE(linhas.size(), 3);
    QCOMPARE(linhas.at(0).tipo, LinhaExtratoDTO::VendaPrazo);
    QCOMPARE(linhas.at(0).saldoApos, 100.0);
    QCOMPARE(linhas.at(1).tipo, LinhaExtratoDTO::Compra);
    QCOMPARE(linhas.at(1).saldoApos, 160.0);
    QCOMPARE(linhas.at(2).tipo, LinhaExtratoDTO::Pagamento);
    QCOMPARE(linhas.at(2).saldoApos, 140.0);
    QVERIFY(linhas.at(2).estornavel);
    QVERIFY(!linhas.at(1).cancelavel);                         // já tem pagamento
    QCOMPARE(cr.totalDevido(cli), 140.0);

    // filtro por empresa: a venda e a dívida são da empresa em uso
    QCOMPARE(cr.extrato(cli, EmpresaAtiva::id()).size(), 3);
    QCOMPARE(cr.extrato(cli, 999999).size(), 0);

    // a lista de devedores soma as duas fontes e mostra a última compra
    FiltroDevedoresDTO f;
    f.texto = cr.getCredito(cli).nome;
    const QList<DevedorDTO> devedores = cr.listarDevedores(f);
    QCOMPARE(devedores.size(), 1);
    QCOMPARE(devedores.first().totalDevido, 140.0);
    QCOMPARE(devedores.first().ultimaCompra, QDate::currentDate().addDays(-5).toString(Qt::ISODate));
}

void TestContasReceber::operacoes_de_gerente_respeitam_o_autorizador()
{
    ContasReceber_service cr;
    const qlonglong cli = novoCliente();
    bool gerente = false;
    ContasReceber_service::definirAutorizador([&]() { return gerente; });

    // observação e WhatsApp qualquer operador altera; o limite, só gerente
    QVERIFY(cr.definirCredito(cli, false, 0, "paga sempre no dia 5", "11999990000").ok);
    QVERIFY(!cr.definirCredito(cli, true, 300, "", "").ok);
    QVERIFY(!cr.inativarCliente(cli).ok);
    gerente = true;
    QVERIFY(cr.definirCredito(cli, true, 300, "paga sempre no dia 5", "11999990000").ok);
    QVERIFY(cr.definirCredito(cli, true, -1, "", "").msg.contains("negativo"));
    const ClienteCreditoDTO c = cr.getCredito(cli);
    QVERIFY(c.temLimite);
    QCOMPARE(c.limite, 300.0);
    QCOMPARE(c.whatsapp, QStringLiteral("11999990000"));
    QCOMPARE(c.observacao, QStringLiteral("paga sempre no dia 5"));
    QVERIFY(cr.inativarCliente(cli).ok);
    QVERIFY(cr.definirCredito(1, true, 10, "", "").msg.contains("Consumidor"));

    // o resumo conta quem deve
    gerente = true;
    const qlonglong devedor = novoCliente();
    QVERIFY(cr.lancarDivida(divida(devedor, 77)).ok);
    const ResumoReceberDTO r = cr.resumo(EmpresaAtiva::id());
    QVERIFY(r.totalReceber >= 77.0);
    QVERIFY(r.clientesDevendo >= 1);
    QVERIFY(r.maiorDivida >= 77.0);
}
