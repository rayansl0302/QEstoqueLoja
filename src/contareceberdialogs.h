#ifndef CONTARECEBERDIALOGS_H
#define CONTARECEBERDIALOGS_H

#include <QDialog>
#include <functional>
#include "dto/ContasReceber_dto.h"

class QCheckBox;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

using GateGerente = std::function<bool(const QString &)>;   // pede o PIN do gerente quando preciso

// ---- fluxos prontos (abrem a janela, chamam o serviço e tratam limite/erros). true = aconteceu algo.
bool lancarDividaComTela(QWidget *parent, const GateGerente &gate, qlonglong idClienteFixo = 0);
bool receberComTela(QWidget *parent, qlonglong idCliente, qlonglong idDivida = 0);
bool editarCreditoComTela(QWidget *parent, const GateGerente &gate, qlonglong idCliente);
void cobrarPorWhatsApp(QWidget *parent, qlonglong idCliente);

// Lançar dívida (caderneta): cliente, data, descrição, valor e observação.
class LancarDividaDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LancarDividaDialog(qlonglong idClienteFixo, QWidget *parent = nullptr);
    DividaDTO divida() const;

private:
    QComboBox *cbCliente;
    QDateEdit *dtData;
    QLineEdit *edDescricao;
    QDoubleSpinBox *spValor;
    QPlainTextEdit *edObs;
    QLabel *lblSituacao;
    QLabel *lblAviso;
    void atualizarSituacao();
    void confirmar();
};

// Receber pagamento do cliente (da mais antiga para a mais nova) ou de uma dívida específica.
class ReceberPagamentoDialog : public QDialog
{
    Q_OBJECT
public:
    ReceberPagamentoDialog(qlonglong idCliente, qlonglong idDivida, QWidget *parent = nullptr);
    double valor() const;
    QString forma() const;
    QDate data() const;
    QString observacao() const;

private:
    QDoubleSpinBox *spValor;
    QComboBox *cbForma;
    QDateEdit *dtData;
    QLineEdit *edObs;
};

// Limite de crédito, WhatsApp, observação e ativo/inativo.
class CreditoClienteDialog : public QDialog
{
    Q_OBJECT
public:
    CreditoClienteDialog(qlonglong idCliente, const GateGerente &gate, QWidget *parent = nullptr);
    bool temLimite() const;
    double limite() const;
    QString whatsapp() const;
    QString observacao() const;
    bool mudouAtivo() const { return ativoAlterado; }

private:
    qlonglong idCliente;
    GateGerente gate;
    bool ativoAlterado = false;
    QCheckBox *chLimite;
    QDoubleSpinBox *spLimite;
    QLineEdit *edWhats;
    QPlainTextEdit *edObs;
    QPushButton *btnAtivo;
    void alternarAtivo();
};

#endif // CONTARECEBERDIALOGS_H
