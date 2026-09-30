#ifndef HISTORICOCAIXAS_H
#define HISTORICOCAIXAS_H

#include <QDialog>
#include <QSqlQueryModel>
#include "services/caixa_service.h"

namespace Ui {
class HistoricoCaixas;
}

class HistoricoCaixas : public QDialog
{
    Q_OBJECT

public:
    explicit HistoricoCaixas(QWidget *parent = nullptr);
    ~HistoricoCaixas();

private slots:
    void on_DateEdt_De_userDateChanged(const QDate &date);
    void on_DateEdt_Ate_userDateChanged(const QDate &date);
    void on_Btn_VerRelatorio_clicked();
    void on_Btn_Fechar_clicked();
    void on_Tview_Caixas_doubleClicked(const QModelIndex &index);

private:
    Ui::HistoricoCaixas *ui;
    Caixa_service caixaServ;
    QSqlQueryModel *model;
    void atualizarTabela();
    qlonglong idSelecionado() const;
    void abrirRelatorio(qlonglong id);
};

#endif // HISTORICOCAIXAS_H
