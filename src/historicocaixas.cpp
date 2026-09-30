#include "historicocaixas.h"
#include "ui_historicocaixas.h"
#include "relatoriocaixajanela.h"
#include "delegatehora.h"
#include "fechamentocaixa.h"
#include <QMenu>
#include <QHeaderView>
#include <QMessageBox>
#include <QDate>

HistoricoCaixas::HistoricoCaixas(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HistoricoCaixas)
    , model(new QSqlQueryModel(this))
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);

    ui->Tview_Caixas->setModel(model);
    DelegateHora *delegateData = new DelegateHora(this);
    ui->Tview_Caixas->setItemDelegateForColumn(4, delegateData);
    ui->Tview_Caixas->setItemDelegateForColumn(5, delegateData);

    const QDate hoje = QDate::currentDate();
    ui->DateEdt_De->blockSignals(true);
    ui->DateEdt_Ate->blockSignals(true);
    ui->DateEdt_De->setDate(QDate(hoje.year(), hoje.month(), 1).addMonths(-1));
    ui->DateEdt_Ate->setDate(hoje);
    ui->DateEdt_De->blockSignals(false);
    ui->DateEdt_Ate->blockSignals(false);
    atualizarTabela();

    // botão direito: ver relatório ou fechar um caixa que ficou aberto (ex.: em outro computador)
    ui->Tview_Caixas->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->Tview_Caixas, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const QModelIndex idx = ui->Tview_Caixas->indexAt(pos);
        if (!idx.isValid())
            return;
        ui->Tview_Caixas->selectRow(idx.row());
        const qlonglong id = idSelecionado();
        const bool aberto = model->data(model->index(idx.row(), 3)).toString() == "ABERTO";

        QMenu menu(this);
        QAction *acaoRelatorio = menu.addAction("Ver relatório");
        QAction *acaoFechar = aberto ? menu.addAction("Fechar este caixa...") : nullptr;
        QAction *escolhida = menu.exec(ui->Tview_Caixas->viewport()->mapToGlobal(pos));
        if (escolhida == acaoRelatorio) {
            abrirRelatorio(id);
        } else if (acaoFechar && escolhida == acaoFechar) {
            FechamentoCaixa dlg(this, id);
            dlg.exec();
            atualizarTabela();
        }
    });
}

HistoricoCaixas::~HistoricoCaixas()
{
    delete ui;
}

void HistoricoCaixas::atualizarTabela()
{
    const QString de = ui->DateEdt_De->date().toString("yyyy-MM-dd");
    const QString ate = ui->DateEdt_Ate->date().addDays(1).toString("yyyy-MM-dd");
    caixaServ.listarHistorico(model, de, ate);
    if (model->columnCount() < 2)
        return;
    model->setHeaderData(1, Qt::Horizontal, "Operador");
    model->setHeaderData(2, Qt::Horizontal, "Terminal");
    model->setHeaderData(3, Qt::Horizontal, "Status");
    model->setHeaderData(4, Qt::Horizontal, "Abertura");
    model->setHeaderData(5, Qt::Horizontal, "Fechamento");
    model->setHeaderData(6, Qt::Horizontal, "Troco inicial");
    model->setHeaderData(7, Qt::Horizontal, "Vendas");
    model->setHeaderData(8, Qt::Horizontal, "Diferença");
    ui->Tview_Caixas->setColumnHidden(0, true);
    ui->Tview_Caixas->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->Tview_Caixas->resizeColumnsToContents();
}

qlonglong HistoricoCaixas::idSelecionado() const
{
    const QModelIndex idx = ui->Tview_Caixas->currentIndex();
    if (!idx.isValid())
        return 0;
    return model->data(model->index(idx.row(), 0)).toLongLong();
}

void HistoricoCaixas::abrirRelatorio(qlonglong id)
{
    if (id <= 0) {
        QMessageBox::information(this, "Histórico de caixas", "Selecione um caixa na lista.");
        return;
    }
    RelatorioCaixaJanela rel(id, this);
    rel.exec();
}

void HistoricoCaixas::on_DateEdt_De_userDateChanged(const QDate &)
{
    atualizarTabela();
}

void HistoricoCaixas::on_DateEdt_Ate_userDateChanged(const QDate &)
{
    atualizarTabela();
}

void HistoricoCaixas::on_Btn_VerRelatorio_clicked()
{
    abrirRelatorio(idSelecionado());
}

void HistoricoCaixas::on_Tview_Caixas_doubleClicked(const QModelIndex &)
{
    abrirRelatorio(idSelecionado());
}

void HistoricoCaixas::on_Btn_Fechar_clicked()
{
    accept();
}
