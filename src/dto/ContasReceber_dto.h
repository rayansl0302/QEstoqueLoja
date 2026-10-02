#ifndef CONTASRECEBER_DTO_H
#define CONTASRECEBER_DTO_H

#include <QDate>
#include <QString>

constexpr const char *kDividaAberta = "ABERTA";
constexpr const char *kDividaQuitada = "QUITADA";
constexpr const char *kDividaCancelada = "CANCELADA";

// Dívida lançada à mão (caderneta). O fiado que nasce no PDV continua em vendas2 (forma "Prazo") +
// entradas_vendas; as duas fontes aparecem juntas no extrato e no total devido do cliente.
struct DividaDTO {
    qlonglong id = 0;
    qlonglong idCliente = 0;
    qlonglong idEmpresa = 0;       // 0 = empresa ativa na hora de gravar
    QString nomeCliente;           // só leitura (listagens)
    QString dataCompra;            // yyyy-MM-dd
    QString descricao;
    double valorTotal = 0;
    double saldo = 0;              // só leitura: valor_total - pagamentos não estornados
    QString observacao;
    QString status = kDividaAberta;
    QString motivoCancelamento;
    qlonglong idOperadorSessao = -1;
    QString criadoEm;

    bool aberta() const { return status == QLatin1String(kDividaAberta); }
    QDate data() const { return QDate::fromString(dataCompra, Qt::ISODate); }
};

struct PagamentoDividaDTO {
    qlonglong id = 0;
    qlonglong idDivida = 0;
    QString dataPagamento;         // yyyy-MM-dd HH:mm:ss
    double valor = 0;
    QString formaPagamento;
    QString observacao;
    qlonglong idOperadorSessao = -1;
    qlonglong idCaixa = 0;
    bool cancelado = false;
    QString motivoCancelamento;
    QString criadoEm;
};

// Dados de crédito do cliente (os cadastrais ficam em Cliente_dto).
struct ClienteCreditoDTO {
    qlonglong id = 0;
    QString nome;
    QString telefone;
    QString whatsapp;
    QString observacao;
    bool ativo = true;
    bool temLimite = false;        // limite vazio = sem limite
    double limite = 0;
};

// Venda a prazo (PDV) ainda com saldo.
struct VendaPrazoAbertaDTO {
    qlonglong idVenda = 0;
    qlonglong idEmpresa = 0;
    QString dataHora;
    double valorFinal = 0;
    double saldo = 0;
};

struct DevedorDTO {
    qlonglong idCliente = 0;
    QString nome;
    QString telefone;
    QString whatsapp;
    bool ativo = true;
    double totalDevido = 0;
    QString ultimaCompra;          // yyyy-MM-dd (vazio se nunca comprou a prazo)
    bool temLimite = false;
    double limite = 0;
    bool acimaDoLimite() const { return temLimite && totalDevido > limite + 0.004; }
};

enum class ModoDevedores { ComDivida, Todos, Inativos };

struct FiltroDevedoresDTO {
    qlonglong idEmpresa = 0;       // 0 = todas as empresas
    ModoDevedores modo = ModoDevedores::ComDivida;
    QString texto;                 // nome ou telefone
};

// Uma linha do extrato do cliente (compras e pagamentos juntos).
struct LinhaExtratoDTO {
    enum Tipo { Compra, VendaPrazo, Pagamento, PagamentoVenda, PagamentoEstornado, DividaCancelada };
    Tipo tipo = Compra;
    QString data;                  // yyyy-MM-dd HH:mm:ss (ordenação)
    QString descricao;
    double valor = 0;              // valor da linha
    double efeito = 0;             // + aumenta a dívida, - abate; 0 para linhas canceladas
    double saldoApos = 0;          // saldo acumulado depois desta linha
    qlonglong idEmpresa = 0;
    qlonglong idDivida = 0;        // dívida manual (compra/pagamento/cancelada)
    qlonglong idPagamento = 0;     // pagamento manual
    qlonglong idVenda = 0;         // venda a prazo / pagamento de venda
    qlonglong idEntrada = 0;       // entradas_vendas
    QString operador;
    QString observacao;
    bool estornavel = false;       // pagamento manual ainda válido
    bool cancelavel = false;       // dívida manual aberta sem pagamentos
};

struct ResumoReceberDTO {
    double totalReceber = 0;
    int clientesDevendo = 0;
    double maiorDivida = 0;
    QString maiorDevedor;
};

#endif // CONTASRECEBER_DTO_H
