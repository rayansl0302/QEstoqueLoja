#include "relatoriocaixajanela.h"
#include "ui_relatoriocaixajanela.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>

RelatorioCaixaJanela::RelatorioCaixaJanela(qlonglong idCaixa, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RelatorioCaixaJanela)
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);
    resumo = caixaServ.resumo(idCaixa);
    html = relServ.gerarHtml(resumo, caixaServ.descricaoTolerancia());
    ui->Txt_Relatorio->setHtml(html);
    setWindowTitle(QString("Relatório do caixa #%1").arg(idCaixa));
}

RelatorioCaixaJanela::~RelatorioCaixaJanela()
{
    delete ui;
}

void RelatorioCaixaJanela::on_Btn_Pdf_clicked()
{
    const QString caminho = QFileDialog::getSaveFileName(
        this, "Salvar relatório de caixa",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
            QString("/fechamento-caixa-%1.pdf").arg(resumo.caixa.id),
        "PDF (*.pdf)");
    if (caminho.isEmpty())
        return;
    QString erro;
    if (!relServ.salvarPdf(html, caminho, &erro))
        QMessageBox::warning(this, "Relatório", erro);
    else
        QMessageBox::information(this, "Relatório", "PDF gravado em:\n" + caminho);
}

void RelatorioCaixaJanela::on_Btn_Imprimir_clicked()
{
    QString erro;
    if (!relServ.imprimirTermica(resumo, &erro))
        QMessageBox::warning(this, "Impressora", erro.isEmpty() ? "Não foi possível imprimir." : erro);
}

void RelatorioCaixaJanela::on_Btn_Fechar_clicked()
{
    accept();
}
