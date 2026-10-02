#ifndef CONTASRECEBERJANELA_H
#define CONTASRECEBERJANELA_H

#include <QDialog>
#include <QList>
#include "contareceberdialogs.h"
#include "services/contasreceber_service.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QToolButton;

// Contas a receber: quem deve, quanto, desde quando. Abre o extrato, lança dívida, recebe e cobra.
class ContasReceberJanela : public QDialog
{
    Q_OBJECT
public:
    explicit ContasReceberJanela(GateGerente gate, qlonglong abrirClienteId = 0, QWidget *parent = nullptr);

    void novaDivida();

private:
    GateGerente gate;
    ContasReceber_service servico;
    QList<DevedorDTO> devedores;

    QComboBox *cbEmpresa;
    QComboBox *cbModo;
    QLineEdit *edBusca;
    QTableWidget *tabela;
    QToolButton *cartaoTotal;
    QToolButton *cartaoClientes;
    QToolButton *cartaoMaior;
    QPushButton *btnExtrato;
    QPushButton *btnReceber;
    QPushButton *btnCobrar;
    QPushButton *btnCredito;

    void montar();
    void recarregar();
    void atualizarBotoes();
    DevedorDTO selecionado() const;
    void abrirExtrato();
};

#endif // CONTASRECEBERJANELA_H
