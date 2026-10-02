#ifndef CONTASRECEBER_SERVICE_H
#define CONTASRECEBER_SERVICE_H

#include <QObject>
#include <QList>
#include <functional>
#include "../dto/ContasReceber_dto.h"
#include "../repository/contasreceber_repository.h"

// Contas a receber (fiado / caderneta): o que cada cliente deve, lançar dívida, receber (total ou parcial),
// estornar e cancelar. Junta o fiado do PDV (vendas "Prazo") com as dívidas lançadas à mão.
//
// Regras principais:
//  - tudo exige operador logado (a sessão vira id_operador_sessao);
//  - recebimento entra no caixa aberto de quem está logado (como o recebimento de venda a prazo);
//  - pagamento nunca passa do saldo; quando o saldo zera a dívida vira QUITADA;
//  - cancelar lançamento, estornar pagamento, inativar cliente e mexer no limite: só gerente.
class ContasReceber_service : public QObject
{
    Q_OBJECT
public:
    struct Resultado {
        bool ok = false;
        QString msg;
        qlonglong id = 0;
        double valor = 0;
        int quantidade = 0;
        bool limiteExcedido = false;      // recusado só por causa do limite (gerente pode liberar)
    };

    struct SituacaoLimite {
        bool temLimite = false;
        double limite = 0;
        double devido = 0;
        double disponivel = 0;            // limite - devido (0 se sem limite)
        double excedente = 0;             // quanto passa do limite com o valor novo
        bool excede = false;
    };

    explicit ContasReceber_service(QObject *parent = nullptr);

    // Quem registra é o main.cpp (mesmo critério do cadastro de operadores); sem autorizador tudo passa (testes).
    static void definirAutorizador(std::function<bool()> autorizador);
    static QStringList formasDePagamento();

    // ---- consultas
    double totalDevido(qlonglong idCliente, qlonglong idEmpresa = 0);
    SituacaoLimite situacaoLimite(qlonglong idCliente, double valorNovo = 0);
    QList<DevedorDTO> listarDevedores(const FiltroDevedoresDTO &filtro);
    QList<LinhaExtratoDTO> extrato(qlonglong idCliente, qlonglong idEmpresa = 0);
    QList<DividaDTO> dividasAbertas(qlonglong idCliente, qlonglong idEmpresa = 0);
    DividaDTO getDivida(qlonglong id);
    ClienteCreditoDTO getCredito(qlonglong idCliente);
    ResumoReceberDTO resumo(qlonglong idEmpresa);

    // ---- venda a prazo no PDV: cliente identificado, ativo e dentro do limite
    Resultado validarVendaPrazo(qlonglong idCliente, double valor, bool limiteLiberado = false);

    // ---- operações
    Resultado lancarDivida(const DividaDTO &divida, bool limiteLiberado = false);
    Resultado receberPagamento(qlonglong idDivida, double valor, const QString &forma, const QDate &data,
                               const QString &observacao = QString());
    // abate da dívida mais antiga para a mais nova (primeiro as da empresa em uso), nas duas fontes
    Resultado receberDoCliente(qlonglong idCliente, double valor, const QString &forma, const QDate &data,
                               const QString &observacao = QString());
    Resultado estornarPagamento(qlonglong idPagamento, const QString &motivo);
    Resultado cancelarDivida(qlonglong idDivida, const QString &motivo);

    // ---- cliente
    Resultado definirCredito(qlonglong idCliente, bool temLimite, double limite, const QString &observacao,
                             const QString &whatsapp);
    Resultado inativarCliente(qlonglong idCliente);
    Resultado reativarCliente(qlonglong idCliente);

private:
    ContasReceber_repository repo;
    static std::function<bool()> autorizador;
    static bool autorizado();
    Resultado verificarSessao() const;
    Resultado validarPagamento(double valor, const QString &forma, const QDate &data) const;
};

#endif // CONTASRECEBER_SERVICE_H
