#include "contasreceber_service.h"
#include "caixa_service.h"
#include "sessao_service.h"
#include "../infra/databaseconnection_service.h"
#include "../infra/empresaativa.h"
#include <QDateTime>
#include <QLocale>
#include <QSqlDatabase>
#include <algorithm>
#include <cmath>

std::function<bool()> ContasReceber_service::autorizador;

namespace {
double arredondar2(double v) { return std::round(v * 100.0) / 100.0; }
qint64 centavos(double v) { return qRound64(v * 100.0); }
QString dinheiro(double v) { return QLocale(QLocale::Portuguese, QLocale::Brazil).toCurrencyString(v, "R$ "); }
constexpr double kTolerancia = 0.004;
}

ContasReceber_service::ContasReceber_service(QObject *parent)
    : QObject(parent)
{
}

void ContasReceber_service::definirAutorizador(std::function<bool()> a)
{
    autorizador = std::move(a);
}

bool ContasReceber_service::autorizado()
{
    return !autorizador || autorizador();
}

QStringList ContasReceber_service::formasDePagamento()
{
    return {"Dinheiro", "Crédito", "Débito", "Pix"};
}

ContasReceber_service::Resultado ContasReceber_service::verificarSessao() const
{
    Sessao_service *s = Sessao_service::instancia();
    if (!s->ativa())
        return {false, "Entre com um operador antes de usar o contas a receber (não há sessão ativa)."};
    return {true};
}

ContasReceber_service::Resultado ContasReceber_service::validarPagamento(double valor, const QString &forma,
                                                                         const QDate &data) const
{
    if (valor <= 0)
        return {false, "Informe um valor maior que zero."};
    if (!formasDePagamento().contains(forma))
        return {false, "Escolha a forma de pagamento (Dinheiro, Crédito, Débito ou Pix)."};
    if (!data.isValid())
        return {false, "Informe uma data de pagamento válida."};
    if (data > QDate::currentDate())
        return {false, "A data do pagamento não pode ser no futuro."};
    return {true};
}

// ------------------------------------------------------------------ consultas

double ContasReceber_service::totalDevido(qlonglong idCliente, qlonglong idEmpresa)
{
    double total = 0;
    for (const DividaDTO &d : repo.listarDividasDoCliente(idCliente, idEmpresa, true))
        total += d.saldo;
    for (const VendaPrazoAbertaDTO &v : repo.listarVendasPrazoAbertas(idCliente, idEmpresa))
        total += v.saldo;
    return arredondar2(total);
}

ContasReceber_service::SituacaoLimite ContasReceber_service::situacaoLimite(qlonglong idCliente, double valorNovo)
{
    SituacaoLimite s;
    const ClienteCreditoDTO c = repo.getCredito(idCliente);
    s.devido = totalDevido(idCliente);     // o limite é do cliente: vale para todas as empresas somadas
    s.temLimite = c.temLimite;
    s.limite = c.limite;
    if (c.temLimite) {
        s.disponivel = qMax(0.0, arredondar2(c.limite - s.devido));
        s.excedente = qMax(0.0, arredondar2(s.devido + valorNovo - c.limite));
        s.excede = s.excedente > kTolerancia;
    }
    return s;
}

ClienteCreditoDTO ContasReceber_service::getCredito(qlonglong idCliente)
{
    return repo.getCredito(idCliente);
}

DividaDTO ContasReceber_service::getDivida(qlonglong id)
{
    return repo.getDivida(id);
}

QList<DividaDTO> ContasReceber_service::dividasAbertas(qlonglong idCliente, qlonglong idEmpresa)
{
    return repo.listarDividasDoCliente(idCliente, idEmpresa, true);
}

QList<DevedorDTO> ContasReceber_service::listarDevedores(const FiltroDevedoresDTO &f)
{
    const QMap<qlonglong, double> manuais = repo.saldosManuaisPorCliente(f.idEmpresa);
    const QMap<qlonglong, double> prazo = repo.saldosPrazoPorCliente(f.idEmpresa);
    const QMap<qlonglong, QString> ultima = repo.ultimaCompraPorCliente(f.idEmpresa);
    const QString busca = f.texto.trimmed().toLower();

    QList<DevedorDTO> lista;
    for (const ClienteCreditoDTO &c : repo.listarClientes(true)) {
        DevedorDTO d;
        d.idCliente = c.id;
        d.nome = c.nome;
        d.telefone = c.telefone;
        d.whatsapp = c.whatsapp;
        d.ativo = c.ativo;
        d.totalDevido = arredondar2(manuais.value(c.id) + prazo.value(c.id));
        d.ultimaCompra = ultima.value(c.id);
        d.temLimite = c.temLimite;
        d.limite = c.limite;

        switch (f.modo) {
        case ModoDevedores::ComDivida:
            if (d.totalDevido <= kTolerancia) continue;
            break;
        case ModoDevedores::Todos:
            if (!d.ativo && d.totalDevido <= kTolerancia) continue;
            break;
        case ModoDevedores::Inativos:
            if (d.ativo) continue;
            break;
        }
        if (!busca.isEmpty() && !d.nome.toLower().contains(busca) && !d.telefone.contains(busca) &&
            !d.whatsapp.contains(busca))
            continue;
        lista.append(d);
    }
    std::sort(lista.begin(), lista.end(), [](const DevedorDTO &a, const DevedorDTO &b) {
        if (!qFuzzyCompare(a.totalDevido + 1, b.totalDevido + 1))
            return a.totalDevido > b.totalDevido;
        return a.nome.compare(b.nome, Qt::CaseInsensitive) < 0;
    });
    return lista;
}

QList<LinhaExtratoDTO> ContasReceber_service::extrato(qlonglong idCliente, qlonglong idEmpresa)
{
    QList<LinhaExtratoDTO> linhas = repo.linhasManuais(idCliente, idEmpresa);
    linhas.append(repo.linhasDoPdv(idCliente, idEmpresa));
    // na mesma data a compra vem antes do pagamento
    auto ordem = [](const LinhaExtratoDTO &l) {
        return (l.tipo == LinhaExtratoDTO::Compra || l.tipo == LinhaExtratoDTO::VendaPrazo ||
                l.tipo == LinhaExtratoDTO::DividaCancelada) ? 0 : 1;
    };
    std::stable_sort(linhas.begin(), linhas.end(), [&](const LinhaExtratoDTO &a, const LinhaExtratoDTO &b) {
        if (a.data != b.data)
            return a.data < b.data;
        return ordem(a) < ordem(b);
    });
    double saldo = 0;
    for (LinhaExtratoDTO &l : linhas) {
        saldo = arredondar2(saldo + l.efeito);
        l.saldoApos = saldo;
    }
    return linhas;
}

ResumoReceberDTO ContasReceber_service::resumo(qlonglong idEmpresa)
{
    ResumoReceberDTO r;
    FiltroDevedoresDTO f;
    f.idEmpresa = idEmpresa;
    f.modo = ModoDevedores::ComDivida;
    for (const DevedorDTO &d : listarDevedores(f)) {
        r.totalReceber += d.totalDevido;
        ++r.clientesDevendo;
        if (d.totalDevido > r.maiorDivida) {
            r.maiorDivida = d.totalDevido;
            r.maiorDevedor = d.nome;
        }
    }
    r.totalReceber = arredondar2(r.totalReceber);
    return r;
}

// ------------------------------------------------------------------ venda a prazo / lançamento

ContasReceber_service::Resultado ContasReceber_service::validarVendaPrazo(qlonglong idCliente, double valor,
                                                                          bool limiteLiberado)
{
    if (idCliente <= 1)
        return {false, "Venda a prazo precisa de um cliente identificado (não use o Consumidor)."};
    const ClienteCreditoDTO c = repo.getCredito(idCliente);
    if (c.id <= 0)
        return {false, "Cliente não encontrado."};
    if (!c.ativo)
        return {false, QString("O cliente %1 está inativo. Reative-o para vender a prazo.").arg(c.nome)};

    const SituacaoLimite s = situacaoLimite(idCliente, valor);
    if (s.excede) {
        const QString msg = QString("Limite de crédito excedido.\nLimite: %1 · já deve: %2 · esta compra: %3 · passa em %4.")
                                .arg(dinheiro(s.limite), dinheiro(s.devido), dinheiro(valor), dinheiro(s.excedente));
        if (!limiteLiberado) {
            Resultado r{false, msg};
            r.limiteExcedido = true;
            return r;
        }
        if (!autorizado())
            return {false, "Liberar o limite de crédito é restrito a gerentes."};
        Sessao_service::instancia()->registrarAcao(
            QStringLiteral("LIMITE_LIBERADO"),
            QStringLiteral("Cliente %1: limite %2, devia %3, compra de %4 liberada acima do limite")
                .arg(c.nome, dinheiro(s.limite), dinheiro(s.devido), dinheiro(valor)));
    }
    return {true};
}

ContasReceber_service::Resultado ContasReceber_service::lancarDivida(const DividaDTO &d, bool limiteLiberado)
{
    const Resultado sessao = verificarSessao();
    if (!sessao.ok)
        return sessao;
    if (d.descricao.trimmed().size() < 2)
        return {false, "Descreva o que o cliente comprou."};
    const double valor = arredondar2(d.valorTotal);
    if (valor <= 0)
        return {false, "Informe um valor maior que zero."};
    const QDate data = QDate::fromString(d.dataCompra, Qt::ISODate);
    if (!data.isValid())
        return {false, "Informe uma data de compra válida."};
    if (data > QDate::currentDate())
        return {false, "A data da compra não pode ser no futuro."};

    const Resultado cliente = validarVendaPrazo(d.idCliente, valor, limiteLiberado);
    if (!cliente.ok)
        return cliente;

    DividaDTO nova = d;
    nova.id = 0;
    nova.idEmpresa = d.idEmpresa > 0 ? d.idEmpresa : EmpresaAtiva::id();
    nova.descricao = d.descricao.trimmed();
    nova.observacao = d.observacao.trimmed();
    nova.valorTotal = valor;
    nova.status = kDividaAberta;
    nova.idOperadorSessao = Sessao_service::instancia()->idOperador();

    QString erro;
    const qlonglong id = repo.inserirDivida(nova, &erro);
    if (id <= 0)
        return {false, "Não foi possível lançar a dívida: " + erro};
    Resultado r{true, "Dívida lançada.", id};
    r.valor = valor;
    r.quantidade = 1;
    return r;
}

// ------------------------------------------------------------------ pagamentos

ContasReceber_service::Resultado ContasReceber_service::receberPagamento(qlonglong idDivida, double valor,
                                                                         const QString &forma, const QDate &data,
                                                                         const QString &observacao)
{
    const Resultado sessao = verificarSessao();
    if (!sessao.ok)
        return sessao;
    const Resultado v = validarPagamento(valor, forma, data);
    if (!v.ok)
        return v;

    const DividaDTO divida = repo.getDivida(idDivida);
    if (divida.id <= 0)
        return {false, "Dívida não encontrada."};
    if (!divida.aberta())
        return {false, "Esta dívida já foi quitada ou cancelada."};
    const double pago = arredondar2(valor);
    if (pago > divida.saldo + kTolerancia)
        return {false, QString("O pagamento (%1) passa do saldo devedor desta dívida (%2).")
                           .arg(dinheiro(pago), dinheiro(divida.saldo))};

    Caixa_service caixa;
    const auto caixaAberto = caixa.exigirCaixaAberto();
    if (!caixaAberto.ok)
        return {false, caixaAberto.msg};

    QSqlDatabase db = DatabaseConnection_service::db();
    if (!db.transaction())
        return {false, "Não foi possível iniciar o registro do pagamento."};

    PagamentoDividaDTO p;
    p.idDivida = idDivida;
    p.dataPagamento = QDateTime(data, QTime::currentTime()).toString("yyyy-MM-dd HH:mm:ss");
    p.valor = pago;
    p.formaPagamento = forma;
    p.observacao = observacao.trimmed();
    p.idOperadorSessao = Sessao_service::instancia()->idOperador();
    p.idCaixa = caixaAberto.id;

    QString erro;
    const qlonglong idPag = repo.inserirPagamento(p, &erro);
    if (idPag <= 0) {
        db.rollback();
        return {false, "Não foi possível registrar o pagamento: " + erro};
    }
    const auto mov = caixa.registrarRecebimentoDivida(
        idDivida, idPag, forma, pago, QString("Recebimento de %1 — %2").arg(divida.nomeCliente, divida.descricao));
    if (!mov.ok) {
        db.rollback();
        return {false, mov.msg};
    }
    if (divida.saldo - pago <= kTolerancia && !repo.atualizarStatusDivida(idDivida, kDividaQuitada, &erro)) {
        db.rollback();
        return {false, "Não foi possível quitar a dívida: " + erro};
    }
    if (!db.commit()) {
        db.rollback();
        return {false, "Não foi possível confirmar o pagamento."};
    }
    Resultado r{true, divida.saldo - pago <= kTolerancia ? "Pagamento registrado: dívida quitada."
                                                         : "Pagamento registrado.", idPag};
    r.valor = pago;
    r.quantidade = 1;
    return r;
}

ContasReceber_service::Resultado ContasReceber_service::receberDoCliente(qlonglong idCliente, double valor,
                                                                         const QString &forma, const QDate &data,
                                                                         const QString &observacao)
{
    const Resultado sessao = verificarSessao();
    if (!sessao.ok)
        return sessao;
    const Resultado v = validarPagamento(valor, forma, data);
    if (!v.ok)
        return v;
    const ClienteCreditoDTO cliente = repo.getCredito(idCliente);
    if (cliente.id <= 0)
        return {false, "Cliente não encontrado."};

    // tudo que está em aberto, das duas fontes
    struct Item {
        bool manual = false;
        qlonglong id = 0;          // dívida manual ou venda
        qlonglong idEmpresa = 0;
        QString data;
        qint64 saldo = 0;          // em centavos
        QString descricao;
    };
    QList<Item> itens;
    for (const DividaDTO &d : repo.listarDividasDoCliente(idCliente, 0, true))
        if (d.saldo > kTolerancia)
            itens.append({true, d.id, d.idEmpresa, d.dataCompra + " 00:00:00", centavos(d.saldo), d.descricao});
    for (const VendaPrazoAbertaDTO &vp : repo.listarVendasPrazoAbertas(idCliente, 0))
        itens.append({false, vp.idVenda, vp.idEmpresa, vp.dataHora, centavos(vp.saldo),
                      QString("Venda a prazo #%1").arg(vp.idVenda)});

    qint64 totalDevido = 0;
    for (const Item &i : itens)
        totalDevido += i.saldo;
    if (totalDevido <= 0)
        return {false, QString("%1 não tem dívida em aberto.").arg(cliente.nome)};
    const qint64 aPagar = centavos(valor);
    if (aPagar > totalDevido)
        return {false, QString("O pagamento (%1) passa do total devido (%2).")
                           .arg(dinheiro(aPagar / 100.0), dinheiro(totalDevido / 100.0))};

    // primeiro a empresa em uso, depois da mais antiga para a mais nova
    const qlonglong ativa = EmpresaAtiva::id();
    std::stable_sort(itens.begin(), itens.end(), [ativa](const Item &a, const Item &b) {
        const bool aa = a.idEmpresa == ativa, bb = b.idEmpresa == ativa;
        if (aa != bb)
            return aa;
        if (a.data != b.data)
            return a.data < b.data;
        return a.id < b.id;
    });

    Caixa_service caixa;
    const auto caixaAberto = caixa.exigirCaixaAberto();
    if (!caixaAberto.ok)
        return {false, caixaAberto.msg};

    QSqlDatabase db = DatabaseConnection_service::db();
    if (!db.transaction())
        return {false, "Não foi possível iniciar o registro do pagamento."};

    const qlonglong sessaoId = Sessao_service::instancia()->idOperador();
    const QString quando = QDateTime(data, QTime::currentTime()).toString("yyyy-MM-dd HH:mm:ss");
    qint64 restante = aPagar;
    int aplicados = 0;
    QString erro;

    for (const Item &i : std::as_const(itens)) {
        if (restante <= 0)
            break;
        const qint64 aplicar = qMin(restante, i.saldo);
        const double valorAplicado = aplicar / 100.0;
        const bool quita = aplicar >= i.saldo;

        if (i.manual) {
            PagamentoDividaDTO p;
            p.idDivida = i.id;
            p.dataPagamento = quando;
            p.valor = valorAplicado;
            p.formaPagamento = forma;
            p.observacao = observacao.trimmed();
            p.idOperadorSessao = sessaoId;
            p.idCaixa = caixaAberto.id;
            const qlonglong idPag = repo.inserirPagamento(p, &erro);
            if (idPag <= 0) {
                db.rollback();
                return {false, "Não foi possível registrar o pagamento: " + erro};
            }
            const auto mov = caixa.registrarRecebimentoDivida(
                i.id, idPag, forma, valorAplicado, QString("Recebimento de %1 — %2").arg(cliente.nome, i.descricao));
            if (!mov.ok) {
                db.rollback();
                return {false, mov.msg};
            }
            if (quita && !repo.atualizarStatusDivida(i.id, kDividaQuitada, &erro)) {
                db.rollback();
                return {false, "Não foi possível quitar a dívida: " + erro};
            }
        } else {
            const qlonglong idEntrada = repo.inserirEntradaVenda(i.id, valorAplicado, forma, quando,
                                                                 caixaAberto.id, sessaoId, &erro);
            if (idEntrada <= 0) {
                db.rollback();
                return {false, "Não foi possível registrar o pagamento da venda: " + erro};
            }
            const auto mov = caixa.registrarRecebimento(i.id, idEntrada, forma, valorAplicado);
            if (!mov.ok) {
                db.rollback();
                return {false, mov.msg};
            }
            if (quita)
                repo.atualizarEstaPago(i.id, true);
        }
        restante -= aplicar;
        ++aplicados;
    }
    if (!db.commit()) {
        db.rollback();
        return {false, "Não foi possível confirmar o pagamento."};
    }
    Resultado r{true, aPagar == totalDevido ? "Pagamento registrado: o cliente quitou tudo."
                                            : "Pagamento registrado.", 0};
    r.valor = aPagar / 100.0;
    r.quantidade = aplicados;
    return r;
}

ContasReceber_service::Resultado ContasReceber_service::estornarPagamento(qlonglong idPagamento, const QString &motivo)
{
    const Resultado sessao = verificarSessao();
    if (!sessao.ok)
        return sessao;
    if (!autorizado())
        return {false, "Estornar um pagamento é restrito a gerentes."};
    if (motivo.trimmed().size() < 3)
        return {false, "Informe o motivo do estorno."};
    const PagamentoDividaDTO p = repo.getPagamento(idPagamento);
    if (p.id <= 0)
        return {false, "Pagamento não encontrado."};
    if (p.cancelado)
        return {false, "Este pagamento já foi estornado."};

    Caixa_service caixa;
    const auto pode = caixa.podeEstornarRecebimentoDivida(idPagamento);
    if (!pode.ok)
        return {false, pode.msg};

    QSqlDatabase db = DatabaseConnection_service::db();
    if (!db.transaction())
        return {false, "Não foi possível iniciar o estorno."};
    QString erro;
    if (!repo.cancelarPagamento(idPagamento, motivo.trimmed(), Sessao_service::instancia()->idOperador(), &erro)) {
        db.rollback();
        return {false, erro};
    }
    const auto mov = caixa.estornarRecebimentoDivida(idPagamento);
    if (!mov.ok) {
        db.rollback();
        return {false, mov.msg};
    }
    const DividaDTO divida = repo.getDivida(p.idDivida);
    if (divida.status == kDividaQuitada)
        repo.atualizarStatusDivida(p.idDivida, kDividaAberta, &erro);     // voltou a ter saldo
    if (!db.commit()) {
        db.rollback();
        return {false, "Não foi possível confirmar o estorno."};
    }
    Sessao_service::instancia()->registrarAcao(
        QStringLiteral("PAGAMENTO_ESTORNADO"),
        QStringLiteral("Pagamento #%1 de %2 (%3) da dívida #%4 estornado: %5")
            .arg(idPagamento).arg(dinheiro(p.valor), p.formaPagamento).arg(p.idDivida).arg(motivo.trimmed()));
    Resultado r{true, "Pagamento estornado.", idPagamento};
    r.valor = p.valor;
    return r;
}

ContasReceber_service::Resultado ContasReceber_service::cancelarDivida(qlonglong idDivida, const QString &motivo)
{
    const Resultado sessao = verificarSessao();
    if (!sessao.ok)
        return sessao;
    if (!autorizado())
        return {false, "Cancelar um lançamento é restrito a gerentes."};
    if (motivo.trimmed().size() < 3)
        return {false, "Informe o motivo do cancelamento."};
    const DividaDTO d = repo.getDivida(idDivida);
    if (d.id <= 0)
        return {false, "Dívida não encontrada."};
    for (const PagamentoDividaDTO &p : repo.listarPagamentos(idDivida))
        if (!p.cancelado)
            return {false, "Este lançamento tem pagamento vinculado. Estorne os pagamentos antes de cancelar "
                           "(o estorno fica registrado)."};
    QString erro;
    if (!repo.cancelarDivida(idDivida, motivo.trimmed(), &erro))
        return {false, erro};
    Sessao_service::instancia()->registrarAcao(
        QStringLiteral("DIVIDA_CANCELADA"),
        QStringLiteral("Dívida #%1 de %2 (%3) cancelada: %4")
            .arg(idDivida).arg(d.nomeCliente, dinheiro(d.valorTotal), motivo.trimmed()));
    Resultado r{true, "Lançamento cancelado.", idDivida};
    r.valor = d.valorTotal;
    return r;
}

// ------------------------------------------------------------------ cliente

ContasReceber_service::Resultado ContasReceber_service::definirCredito(qlonglong idCliente, bool temLimite,
                                                                       double limite, const QString &observacao,
                                                                       const QString &whatsapp)
{
    if (idCliente <= 1)
        return {false, "O cliente Consumidor não tem crédito."};
    const ClienteCreditoDTO atual = repo.getCredito(idCliente);
    if (atual.id <= 0)
        return {false, "Cliente não encontrado."};
    if (temLimite && limite < 0)
        return {false, "O limite não pode ser negativo."};

    const double novoLimite = temLimite ? arredondar2(limite) : 0;
    const bool limiteMudou = atual.temLimite != temLimite || (temLimite && std::abs(atual.limite - novoLimite) > kTolerancia);
    if (limiteMudou && !autorizado())
        return {false, "Alterar o limite de crédito é restrito a gerentes."};

    ClienteCreditoDTO c = atual;
    c.temLimite = temLimite;
    c.limite = novoLimite;
    c.observacao = observacao.trimmed();
    c.whatsapp = whatsapp.trimmed();
    QString erro;
    if (!repo.salvarCredito(c, &erro))
        return {false, "Não foi possível salvar: " + erro};
    if (limiteMudou)
        Sessao_service::instancia()->registrarAcao(
            QStringLiteral("LIMITE_ALTERADO"),
            QStringLiteral("Cliente %1: limite %2 → %3")
                .arg(atual.nome, atual.temLimite ? dinheiro(atual.limite) : QStringLiteral("sem limite"),
                     temLimite ? dinheiro(novoLimite) : QStringLiteral("sem limite")));
    return {true, "Dados de crédito salvos.", idCliente};
}

ContasReceber_service::Resultado ContasReceber_service::inativarCliente(qlonglong idCliente)
{
    if (idCliente <= 1)
        return {false, "O cliente Consumidor não pode ser inativado."};
    if (!autorizado())
        return {false, "Inativar um cliente é restrito a gerentes."};
    const ClienteCreditoDTO c = repo.getCredito(idCliente);
    if (c.id <= 0)
        return {false, "Cliente não encontrado."};
    QString erro;
    if (!repo.definirAtivo(idCliente, false, &erro))
        return {false, "Não foi possível inativar: " + erro};
    Sessao_service::instancia()->registrarAcao(QStringLiteral("CLIENTE_INATIVADO"),
                                               QStringLiteral("Cliente %1 inativado").arg(c.nome));
    return {true, "Cliente inativado. A dívida continua valendo, mas ele não compra mais a prazo.", idCliente};
}

ContasReceber_service::Resultado ContasReceber_service::reativarCliente(qlonglong idCliente)
{
    if (!autorizado())
        return {false, "Reativar um cliente é restrito a gerentes."};
    const ClienteCreditoDTO c = repo.getCredito(idCliente);
    if (c.id <= 0)
        return {false, "Cliente não encontrado."};
    QString erro;
    if (!repo.definirAtivo(idCliente, true, &erro))
        return {false, "Não foi possível reativar: " + erro};
    Sessao_service::instancia()->registrarAcao(QStringLiteral("CLIENTE_REATIVADO"),
                                               QStringLiteral("Cliente %1 reativado").arg(c.nome));
    return {true, "Cliente reativado.", idCliente};
}
