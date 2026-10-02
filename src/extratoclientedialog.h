#ifndef EXTRATOCLIENTEDIALOG_H
#define EXTRATOCLIENTEDIALOG_H

#include <QDialog>
#include <QList>
#include "contareceberdialogs.h"
#include "services/contasreceber_service.h"

class QLabel;
class QPushButton;
class QTableWidget;

// Extrato do cliente: compras a prazo (PDV e caderneta) e pagamentos, com o saldo depois de cada linha.
class ExtratoClienteDialog : public QDialog
{
    Q_OBJECT
public:
    ExtratoClienteDialog(qlonglong idCliente, GateGerente gate, QWidget *parent = nullptr);

private:
    qlonglong idCliente;
    GateGerente gate;
    ContasReceber_service servico;
    QList<LinhaExtratoDTO> linhas;

    QLabel *lblCliente;
    QLabel *lblResumo;
    QTableWidget *tabela;
    QPushButton *btnReceber;
    QPushButton *btnReceberDivida;
    QPushButton *btnEstornar;
    QPushButton *btnCancelar;

    void recarregar();
    void atualizarBotoes();
    LinhaExtratoDTO selecionada() const;
    void estornar();
    void cancelarLancamento();
};

#endif // EXTRATOCLIENTEDIALOG_H
