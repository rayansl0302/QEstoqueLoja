#ifndef RELATORIOCAIXAJANELA_H
#define RELATORIOCAIXAJANELA_H

#include <QDialog>
#include "services/caixa_service.h"
#include "services/relatoriocaixa_service.h"

namespace Ui {
class RelatorioCaixaJanela;
}

class RelatorioCaixaJanela : public QDialog
{
    Q_OBJECT

public:
    explicit RelatorioCaixaJanela(qlonglong idCaixa, QWidget *parent = nullptr);
    ~RelatorioCaixaJanela();

private slots:
    void on_Btn_Pdf_clicked();
    void on_Btn_Imprimir_clicked();
    void on_Btn_Fechar_clicked();

private:
    Ui::RelatorioCaixaJanela *ui;
    Caixa_service caixaServ;
    RelatorioCaixa_service relServ;
    ResumoCaixaDTO resumo;
    QString html;
};

#endif // RELATORIOCAIXAJANELA_H
