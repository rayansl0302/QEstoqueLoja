#ifndef CONTASRECEBER_REPOSITORY_H
#define CONTASRECEBER_REPOSITORY_H

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include <QMap>
#include "../dto/ContasReceber_dto.h"

// Contas a receber (migração 21): dívidas manuais, pagamentos, dados de crédito do cliente e as consultas
// que juntam o fiado do PDV (vendas2 "Prazo" + entradas_vendas).
class ContasReceber_repository : public QObject
{
    Q_OBJECT
public:
    explicit ContasReceber_repository(QObject *parent = nullptr);

    // ---- dívidas manuais
    qlonglong inserirDivida(const DividaDTO &divida, QString *erro = nullptr);
    DividaDTO getDivida(qlonglong id);
    QList<DividaDTO> listarDividasDoCliente(qlonglong idCliente, qlonglong idEmpresa, bool somenteAbertas);
    bool atualizarStatusDivida(qlonglong id, const QString &status, QString *erro = nullptr);
    // só cancela dívida ABERTA (a condição vai no WHERE: dois computadores não se atropelam)
    bool cancelarDivida(qlonglong id, const QString &motivo, QString *erro = nullptr);

    // ---- pagamentos das dívidas manuais
    qlonglong inserirPagamento(const PagamentoDividaDTO &pagamento, QString *erro = nullptr);
    PagamentoDividaDTO getPagamento(qlonglong id);
    QList<PagamentoDividaDTO> listarPagamentos(qlonglong idDivida);
    bool cancelarPagamento(qlonglong id, const QString &motivo, qlonglong idOperadorSessao, QString *erro = nullptr);

    // ---- fiado do PDV
    QList<VendaPrazoAbertaDTO> listarVendasPrazoAbertas(qlonglong idCliente, qlonglong idEmpresa);
    qlonglong inserirEntradaVenda(qlonglong idVenda, double valor, const QString &forma, const QString &dataHora,
                                  qlonglong idCaixa, qlonglong idOperadorSessao, QString *erro = nullptr);
    bool atualizarEstaPago(qlonglong idVenda, bool pago);

    // ---- totais (idEmpresa 0 = todas)
    QMap<qlonglong, double> saldosManuaisPorCliente(qlonglong idEmpresa);
    QMap<qlonglong, double> saldosPrazoPorCliente(qlonglong idEmpresa);
    QMap<qlonglong, QString> ultimaCompraPorCliente(qlonglong idEmpresa);

    // ---- extrato (linhas sem o saldo acumulado: o serviço calcula)
    QList<LinhaExtratoDTO> linhasManuais(qlonglong idCliente, qlonglong idEmpresa);
    QList<LinhaExtratoDTO> linhasDoPdv(qlonglong idCliente, qlonglong idEmpresa);

    // ---- crédito do cliente
    ClienteCreditoDTO getCredito(qlonglong idCliente);
    QList<ClienteCreditoDTO> listarClientes(bool incluirInativos);
    bool salvarCredito(const ClienteCreditoDTO &credito, QString *erro = nullptr);
    bool definirAtivo(qlonglong idCliente, bool ativo, QString *erro = nullptr);
    int quantidadeDeHistorico(qlonglong idCliente);     // vendas + dívidas do cliente

private:
    QSqlDatabase db;
};

#endif // CONTASRECEBER_REPOSITORY_H
