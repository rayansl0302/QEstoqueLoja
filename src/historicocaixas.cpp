#include "historicocaixas.h"
#include "ui_historicocaixas.h"
#include "relatoriocaixajanela.h"
#include "delegatehora.h"
#include "fechamentocaixa.h"
#include <QMenu>
#include <QHeaderView>
#include <QMessageBox>
#include <QDate>
#include <QLocale>
#include <QStyledItemDelegate>

namespace {
QString moeda(double v)
{
    return QLocale(QLocale::Portuguese, QLocale::Brazil).toCurrencyString(v, "R$ ");
}

// valores em reais, alinhados à direita
class DelegateMoeda : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QString displayText(const QVariant &value, const QLocale &) const override { return moeda(value.toDouble()); }
    void initStyleOption(QStyleOptionViewItem *o, const QModelIndex &i) const override
    {
        QStyledItemDelegate::initStyleOption(o, i);
        o->displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
    }
};

// só vale para caixa fechado (aberto ainda não foi contado): "—"
class DelegateContado : public DelegateMoeda
{
public:
    using DelegateMoeda::DelegateMoeda;
    void initStyleOption(QStyleOptionViewItem *o, const QModelIndex &i) const override
    {
        DelegateMoeda::initStyleOption(o, i);
        if (i.sibling(i.row(), 3).data().toString() == "ABERTO")
            o->text = QStringLiteral("—");
    }
};

// diferença do fechamento: + sobrou (verde), − faltou (vermelho), confere (cinza)
class DelegateDiferenca : public DelegateMoeda
{
public:
    using DelegateMoeda::DelegateMoeda;
    void initStyleOption(QStyleOptionViewItem *o, const QModelIndex &i) const override
    {
        DelegateMoeda::initStyleOption(o, i);
        if (i.sibling(i.row(), 3).data().toString() == "ABERTO") {
            o->text = QStringLiteral("—");
            return;
        }
        const double v = i.data().toDouble();
        QFont f = o->font;
        f.setBold(true);
        o->font = f;
        if (v > 0.004) {
            o->text = "+" + moeda(v) + "  sobra";
            o->palette.setColor(QPalette::Text, QColor("#15803D"));
            o->palette.setColor(QPalette::HighlightedText, QColor("#15803D"));
        } else if (v < -0.004) {
            o->text = "−" + moeda(-v) + "  falta";
            o->palette.setColor(QPalette::Text, QColor("#B91C1C"));
            o->palette.setColor(QPalette::HighlightedText, QColor("#B91C1C"));
        } else {
            o->text = QStringLiteral("confere");
            o->palette.setColor(QPalette::Text, QColor("#64748B"));
            o->palette.setColor(QPalette::HighlightedText, QColor("#64748B"));
        }
    }
};
}

HistoricoCaixas::HistoricoCaixas(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HistoricoCaixas)
    , model(new QSqlQueryModel(this))
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);
    resize(qMax(width(), 1120), qMax(height(), 520));

    ui->Tview_Caixas->setModel(model);
    DelegateHora *delegateData = new DelegateHora(this);
    ui->Tview_Caixas->setItemDelegateForColumn(4, delegateData);
    ui->Tview_Caixas->setItemDelegateForColumn(5, delegateData);
    ui->Tview_Caixas->setItemDelegateForColumn(6, new DelegateMoeda(this));
    ui->Tview_Caixas->setItemDelegateForColumn(7, new DelegateMoeda(this));
    ui->Tview_Caixas->setItemDelegateForColumn(8, new DelegateContado(this));
    ui->Tview_Caixas->setItemDelegateForColumn(9, new DelegateDiferenca(this));

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
    model->setHeaderData(6, Qt::Horizontal, "Começou com");
    model->setHeaderData(7, Qt::Horizontal, "Vendas");
    model->setHeaderData(8, Qt::Horizontal, "Fechou com");
    model->setHeaderData(9, Qt::Horizontal, "Diferença");
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
