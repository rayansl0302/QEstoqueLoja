#ifndef CONTASPAGARJANELA_H
#define CONTASPAGARJANELA_H

#include <QDialog>
#include <functional>
#include "dto/ContasPagar_dto.h"
#include "services/contaspagar_service.h"

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QToolButton;

// Contas a pagar: resumo (vencidas, hoje, próximos 7 dias), filtros e a lista, com lançar, editar,
// baixar, estornar e cancelar. Estornar e cancelar pedem gerente (exigirGerente).
class ContasPagarJanela : public QDialog
{
    Q_OBJECT
public:
    explicit ContasPagarJanela(std::function<bool(const QString &)> exigirGerente,
                               const QString &statusInicial = QString(), QWidget *parent = nullptr);

    void novaConta();

private:
    std::function<bool(const QString &)> exigirGerente;
    ContasPagar_service servico;
    QList<ContaPagarDTO> contas;

    QComboBox *cbEmpresa;
    QComboBox *cbStatus;
    QLineEdit *edBusca;
    QCheckBox *chPeriodo;
    QDateEdit *dtDe;
    QDateEdit *dtAte;
    QTableWidget *tabela;
    QLabel *lblTotais;
    QToolButton *cartaoVencidas;
    QToolButton *cartaoHoje;
    QToolButton *cartaoSemana;
    QToolButton *cartaoAberto;
    QPushButton *btnBaixar;
    QPushButton *btnEditar;
    QPushButton *btnEstornar;
    QPushButton *btnCancelar;

    void montar();
    void recarregar();
    void atualizarResumo();
    void atualizarBotoes();
    ContaPagarDTO selecionada() const;
    void definirStatus(const QString &status);

    void editar();
    void baixar();
    void estornar();
    void cancelar();
};

#endif // CONTASPAGARJANELA_H
