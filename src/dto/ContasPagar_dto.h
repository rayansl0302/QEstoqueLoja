#ifndef CONTASPAGAR_DTO_H
#define CONTASPAGAR_DTO_H

#include <QDate>
#include <QString>

constexpr const char *kContaAberta = "ABERTA";
constexpr const char *kContaPaga = "PAGA";
constexpr const char *kContaCancelada = "CANCELADA";

// Uma conta (ou parcela) a pagar. Compra parcelada vira várias linhas com o mesmo "grupo".
struct ContaPagarDTO {
    qlonglong id = 0;
    qlonglong idEmpresa = 0;       // 0 = empresa ativa na hora de gravar
    QString descricao;
    QString fornecedor;
    QString categoria;
    QString documento;             // nº da nota/boleto
    double valor = 0;
    QString vencimento;            // yyyy-MM-dd
    QString status = kContaAberta;
    double valorPago = 0;
    QString formaPagamento;
    QString pagoEm;                // yyyy-MM-dd HH:mm:ss
    int parcela = 1;
    int totalParcelas = 1;
    QString grupo;
    QString observacao;
    QString motivoCancelamento;
    qlonglong idOperadorSessao = -1;
    QString criadoEm;

    bool aberta() const { return status == QLatin1String(kContaAberta); }
    QDate dataVencimento() const { return QDate::fromString(vencimento, Qt::ISODate); }
    bool vencida(const QDate &hoje = QDate::currentDate()) const
    {
        return aberta() && dataVencimento().isValid() && dataVencimento() < hoje;
    }
};

// Filtro da listagem. idEmpresa 0 = todas as empresas; status vazio = todos.
// "VENCIDA" é um filtro à parte: contas abertas com vencimento antes de hoje.
struct FiltroContasPagarDTO {
    qlonglong idEmpresa = 0;
    QString status;
    QString vencimentoDe;          // yyyy-MM-dd, inclusive
    QString vencimentoAte;         // yyyy-MM-dd, inclusive
    QString texto;                 // descrição, fornecedor, documento
};

struct ResumoContasPagarDTO {
    int qtdVencidas = 0;
    double valorVencidas = 0;
    int qtdHoje = 0;
    double valorHoje = 0;
    int qtdProximos7Dias = 0;      // de amanhã a 7 dias
    double valorProximos7Dias = 0;
    int qtdAbertas = 0;
    double valorAbertas = 0;
};

#endif // CONTASPAGAR_DTO_H
