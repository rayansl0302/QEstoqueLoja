#include "logacessodialog.h"
#include "services/sessao_service.h"
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDateTime>

namespace {
QString dataHora(const QString &iso)
{
    if (iso.isEmpty())
        return QStringLiteral("—");
    const QDateTime d = QDateTime::fromString(iso, Qt::ISODate);
    return d.isValid() ? d.toString("dd/MM/yyyy HH:mm:ss") : iso;
}

QString motivoLegivel(const QString &motivo)
{
    if (motivo == "LOGOUT") return "Saiu da conta";
    if (motivo == "TROCA_OPERADOR") return "Troca de operador";
    if (motivo == "TIMEOUT") return "Inatividade";
    if (motivo == "INVALIDADA") return "Sessão invalidada";
    if (motivo == "SEM_LOGOUT") return "Programa fechado sem sair";
    if (motivo == "ENCERRAMENTO") return "Programa encerrado";
    return motivo.isEmpty() ? QStringLiteral("—") : motivo;
}

QString acaoLegivel(const QString &acao)
{
    if (acao == "ELEVACAO_PIN_GERAL") return "Usou o PIN do gerente";
    if (acao == "BLOQUEIO_SESSAO") return "Sessão bloqueada";
    if (acao == "DESBLOQUEIO_SESSAO") return "Sessão desbloqueada";
    if (acao == "DESBLOQUEIO_POR_GERENTE") return "Desbloqueada com o PIN do gerente";
    if (acao == "SESSAO_INVALIDADA") return "Sessão invalidada";
    if (acao == "PERMISSAO_ALTERADA") return "Permissão alterada";
    return acao;
}

QTableWidget *novaTabela(const QStringList &colunas)
{
    auto *t = new QTableWidget;
    t->setColumnCount(colunas.size());
    t->setHorizontalHeaderLabels(colunas);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setAlternatingRowColors(true);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);
    return t;
}

void item(QTableWidget *t, int linha, int coluna, const QString &texto)
{
    t->setItem(linha, coluna, new QTableWidgetItem(texto));
}
}

LogAcessoDialog::LogAcessoDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Log de acesso");
    resize(960, 560);

    tabelaSessoes = novaTabela({"Operador", "Papel", "Terminal", "Entrada", "Saída", "Motivo da saída", "Caixa"});
    tabelaAcoes = novaTabela({"Data e hora", "Operador", "Terminal", "Ação", "Detalhe"});

    auto *abas = new QTabWidget;
    abas->addTab(tabelaSessoes, "Sessões");
    abas->addTab(tabelaAcoes, "Ações sensíveis");

    auto *fechar = new QPushButton("Fechar");
    connect(fechar, &QPushButton::clicked, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(abas);
    layout->addWidget(fechar, 0, Qt::AlignRight);

    carregar();
}

void LogAcessoDialog::carregar()
{
    Sessao_service *serv = Sessao_service::instancia();

    const QList<LogSessaoDTO> sessoes = serv->ultimasSessoes(300);
    tabelaSessoes->setRowCount(sessoes.size());
    for (int i = 0; i < sessoes.size(); ++i) {
        const LogSessaoDTO &s = sessoes.at(i);
        item(tabelaSessoes, i, 0, s.nomeOperador);
        item(tabelaSessoes, i, 1, s.gerente ? "Gerente" : "Operador");
        item(tabelaSessoes, i, 2, s.terminal);
        item(tabelaSessoes, i, 3, dataHora(s.entradaEm));
        item(tabelaSessoes, i, 4, s.saidaEm.isEmpty() ? QStringLiteral("(ainda aberta)") : dataHora(s.saidaEm));
        item(tabelaSessoes, i, 5, s.saidaEm.isEmpty() ? QStringLiteral("—") : motivoLegivel(s.motivoSaida));
        item(tabelaSessoes, i, 6, s.idCaixaNoMomento > 0 ? QString("#%1").arg(s.idCaixaNoMomento) : QStringLiteral("—"));
    }

    const QList<LogAcaoDTO> acoes = serv->ultimasAcoes(300);
    tabelaAcoes->setRowCount(acoes.size());
    for (int i = 0; i < acoes.size(); ++i) {
        const LogAcaoDTO &a = acoes.at(i);
        item(tabelaAcoes, i, 0, dataHora(a.dataHora));
        item(tabelaAcoes, i, 1, a.nomeOperador.isEmpty() ? QStringLiteral("—") : a.nomeOperador);
        item(tabelaAcoes, i, 2, a.terminal);
        item(tabelaAcoes, i, 3, acaoLegivel(a.acao));
        item(tabelaAcoes, i, 4, a.detalhe);
    }

    tabelaSessoes->resizeColumnsToContents();
    tabelaAcoes->resizeColumnsToContents();
}
