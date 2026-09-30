#include "movimentacaocaixa.h"
#include "ui_movimentacaocaixa.h"
#include <QMessageBox>
#include <QDoubleValidator>

MovimentacaoCaixa::MovimentacaoCaixa(Tipo tipo, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MovimentacaoCaixa)
    , portugues(QLocale::Portuguese, QLocale::Brazil)
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);

    if (tipo == Tipo::Suprimento)
        ui->Rb_Suprimento->setChecked(true);
    else
        ui->Rb_Sangria->setChecked(true);

    const CaixaDTO caixa = caixaServ.caixaAbertoNoTerminal();
    if (caixa.aberto()) {
        ui->Lbl_Caixa->setText(QString("Caixa #%1 · %2 · terminal %3")
                                   .arg(caixa.id).arg(caixa.nomeOperador, caixa.terminal));
    } else {
        ui->Lbl_Caixa->setText("Nenhum caixa aberto neste terminal.");
        ui->Btn_Confirmar->setEnabled(false);
    }

    QDoubleValidator *validador = new QDoubleValidator(0.01, 99999999.99, 2, this);
    validador->setLocale(portugues);
    ui->Ledit_Valor->setValidator(validador);
}

MovimentacaoCaixa::~MovimentacaoCaixa()
{
    delete ui;
}

void MovimentacaoCaixa::on_Btn_Confirmar_clicked()
{
    bool ok = false;
    const double valor = portugues.toDouble(ui->Ledit_Valor->text(), &ok);
    if (!ok || valor <= 0) {
        QMessageBox::warning(this, "Caixa", "Informe um valor maior que zero.");
        ui->Ledit_Valor->setFocus();
        return;
    }

    const QString motivo = ui->PTEdit_Motivo->toPlainText().trimmed();
    const auto r = ui->Rb_Suprimento->isChecked()
                       ? caixaServ.registrarSuprimento(valor, motivo)
                       : caixaServ.registrarSangria(valor, motivo);
    if (!r.ok) {
        QMessageBox::warning(this, "Caixa", r.msg);
        return;
    }
    QMessageBox::information(this, "Caixa", r.msg);
    accept();
}

void MovimentacaoCaixa::on_Btn_Cancelar_clicked()
{
    reject();
}
