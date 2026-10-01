#include "empresadialog.h"
#include "services/empresa_service.h"
#include "util/icones.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kColunas = 2;

// Janela pequena para cadastrar a empresa: nome curto e CNPJ (o resto é preenchido em Configurações)
class NovaEmpresaDialog : public QDialog
{
public:
    explicit NovaEmpresaDialog(QWidget *parent)
        : QDialog(parent)
    {
        setWindowTitle("Cadastrar empresa");
        setModal(true);
        auto *form = new QFormLayout;
        nome = new QLineEdit;
        nome->setPlaceholderText("Ex.: Salão da Ana");
        cnpj = new QLineEdit;
        cnpj->setPlaceholderText("00.000.000/0000-00");
        cnpj->setInputMask(QStringLiteral("99.999.999/9999-99;_"));
        form->addRow("Nome da empresa:", nome);
        form->addRow("CNPJ:", cnpj);

        auto *dica = new QLabel("Depois de cadastrar, escolha a empresa e preencha os dados completos e o "
                                "certificado em Configurações.");
        dica->setWordWrap(true);
        dica->setStyleSheet("color: #5B6B7F;");

        auto *botoes = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        botoes->button(QDialogButtonBox::Ok)->setText("Cadastrar");
        botoes->button(QDialogButtonBox::Cancel)->setText("Cancelar");
        connect(botoes, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(botoes, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto *layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(dica);
        layout->addWidget(botoes);
        setMinimumWidth(420);
    }
    QString nomeDigitado() const { return nome->text(); }
    QString cnpjDigitado() const { return cnpj->text(); }

private:
    QLineEdit *nome;
    QLineEdit *cnpj;
};
}

EmpresaDialog::EmpresaDialog(std::function<bool(const QString &)> gate, const QString &titulo, QWidget *parent)
    : QDialog(parent)
    , exigirGerente(std::move(gate))
{
    setWindowTitle("Empresa");
    setModal(true);
    resize(620, 460);

    auto *lblTitulo = new QLabel(titulo.isEmpty() ? QStringLiteral("Escolher empresa") : titulo);
    lblTitulo->setStyleSheet("font-size: 17pt; font-weight: 700; color: #16324F;");
    auto *lblInfo = new QLabel("As vendas, contas a pagar e notas feitas a partir de agora ficam no CNPJ escolhido, "
                               "até você trocar de empresa.");
    lblInfo->setWordWrap(true);
    lblInfo->setStyleSheet("color: #5B6B7F;");

    auto *rolagem = new QScrollArea;
    rolagem->setWidgetResizable(true);
    rolagem->setFrameShape(QFrame::NoFrame);
    rolagem->setStyleSheet("QScrollArea { background: transparent; }");
    areaCartoes = new QWidget;
    areaCartoes->setStyleSheet("background: transparent;");
    grade = new QGridLayout(areaCartoes);
    grade->setContentsMargins(2, 2, 2, 2);
    grade->setSpacing(10);
    rolagem->setWidget(areaCartoes);

    lblAviso = new QLabel;
    lblAviso->setWordWrap(true);
    lblAviso->setStyleSheet("color: #B91C1C;");

    auto *btnNova = new QPushButton("Cadastrar empresa...");
    btnNova->setIcon(Icones::icone("plus", QColor(Qt::white), 18));
    btnDesativar = new QPushButton("Desativar");
    btnDesativar->setStyleSheet("QPushButton { background: white; color: #B91C1C; border: 1px solid #FCA5A5; }"
                                "QPushButton:hover { background: #FEF2F2; }"
                                "QPushButton:disabled { background: #F1F5F9; color: #94A3B8; border-color: #E2E8F0; }");
    auto *btnFechar = new QPushButton("Fechar");
    btnFechar->setStyleSheet("QPushButton { background: white; color: #1E3A5F; border: 1px solid #C9D3DF; }"
                             "QPushButton:hover { background: #EAF3FB; }");
    btnUsar = new QPushButton("Usar esta empresa");
    btnUsar->setDefault(true);
    btnUsar->setIcon(Icones::icone("check", QColor(Qt::white), 18));

    connect(btnNova, &QPushButton::clicked, this, &EmpresaDialog::cadastrarNova);
    connect(btnDesativar, &QPushButton::clicked, this, &EmpresaDialog::desativarSelecionada);
    connect(btnFechar, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnUsar, &QPushButton::clicked, this, &EmpresaDialog::usarSelecionada);

    auto *rodape = new QHBoxLayout;
    rodape->addWidget(btnNova);
    rodape->addWidget(btnDesativar);
    rodape->addStretch(1);
    rodape->addWidget(btnFechar);
    rodape->addWidget(btnUsar);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->addWidget(lblTitulo);
    layout->addWidget(lblInfo);
    layout->addWidget(rolagem, 1);
    layout->addWidget(lblAviso);
    layout->addLayout(rodape);

    idSelecionada = Empresa_service::instancia()->idAtiva();
    recarregar();
}

void EmpresaDialog::recarregar()
{
    for (QToolButton *c : std::as_const(cartoes))
        c->deleteLater();
    cartoes.clear();

    auto *es = Empresa_service::instancia();
    empresas = es->listar(true);
    const qlonglong emUso = es->idAtiva();

    int i = 0;
    for (const EmpresaDTO &e : std::as_const(empresas)) {
        auto *botao = new QToolButton;
        botao->setCheckable(true);
        botao->setAutoExclusive(true);
        botao->setCursor(Qt::PointingHandCursor);
        botao->setMinimumSize(250, 96);
        botao->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        botao->setIconSize(QSize(34, 34));
        botao->setIcon(Icones::icone("building-2", QColor("#2B84BF"), 34));
        const QString cnpj = e.cnpj.isEmpty() ? QStringLiteral("CNPJ não informado") : e.cnpjFormatado();
        botao->setText(QStringLiteral("%1\n%2%3").arg(e.apelido, cnpj,
                                                       e.id == emUso ? QStringLiteral("\nEm uso agora") : QString()));
        botao->setStyleSheet(
            "QToolButton { background: white; color: #1E3A5F; border: 1px solid #D5DEE9; border-radius: 12px;"
            " font-weight: 700; padding: 8px 12px; text-align: left; }"
            "QToolButton:hover { background: #EAF3FB; border: 2px solid #2B84BF; }"
            "QToolButton:checked { background: #D3E8F8; border: 2px solid #2B84BF; color: #0B2540; }");
        const qlonglong id = e.id;
        connect(botao, &QToolButton::clicked, this, [this, id]() { selecionar(id); });
        cartoes.append(botao);
        grade->addWidget(botao, i / kColunas, i % kColunas);
        ++i;
    }
    grade->setRowStretch(grade->rowCount(), 1);

    if (idSelecionada <= 0 || !es->getPorId(idSelecionada).ativa)
        idSelecionada = emUso;
    selecionar(idSelecionada);
}

void EmpresaDialog::selecionar(qlonglong id)
{
    idSelecionada = id;
    for (int i = 0; i < empresas.size() && i < cartoes.size(); ++i)
        cartoes.at(i)->setChecked(empresas.at(i).id == id);
    const bool emUso = id == Empresa_service::instancia()->idAtiva();
    btnUsar->setText(emUso ? QStringLiteral("Continuar nesta empresa") : QStringLiteral("Usar esta empresa"));
    btnDesativar->setEnabled(!emUso && empresas.size() > 1);
    lblAviso->clear();
}

void EmpresaDialog::usarSelecionada()
{
    if (idSelecionada <= 0) {
        lblAviso->setText("Selecione uma empresa.");
        return;
    }
    const auto r = Empresa_service::instancia()->trocarPara(idSelecionada);
    if (!r.ok) {
        lblAviso->setText(r.msg);
        return;
    }
    idEscolhida = idSelecionada;
    accept();
}

void EmpresaDialog::cadastrarNova()
{
    if (exigirGerente && !exigirGerente(QStringLiteral("O cadastro de empresas")))
        return;
    NovaEmpresaDialog dlg(this);
    while (dlg.exec() == QDialog::Accepted) {
        const auto r = Empresa_service::instancia()->cadastrar(dlg.nomeDigitado(), dlg.cnpjDigitado());
        if (r.ok) {
            idSelecionada = r.id;
            recarregar();
            QMessageBox::information(this, "Empresa cadastrada",
                "Empresa cadastrada.\n\nEscolha-a e use \"Usar esta empresa\"; depois preencha os dados completos "
                "e o certificado em Configurações.");
            return;
        }
        QMessageBox::warning(this, "Cadastrar empresa", r.msg);
    }
}

void EmpresaDialog::desativarSelecionada()
{
    if (exigirGerente && !exigirGerente(QStringLiteral("Desativar empresa")))
        return;
    const EmpresaDTO e = Empresa_service::instancia()->getPorId(idSelecionada);
    if (!e.valida())
        return;
    const auto resp = QMessageBox::question(this, "Desativar empresa",
        QStringLiteral("Desativar \"%1\"?\nAs vendas e contas antigas continuam no histórico; ela só deixa de "
                       "aparecer para escolha.").arg(e.apelido),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (resp != QMessageBox::Yes)
        return;
    const auto r = Empresa_service::instancia()->definirAtivaNoCadastro(e.id, false);
    if (!r.ok) {
        lblAviso->setText(r.msg);
        return;
    }
    idSelecionada = Empresa_service::instancia()->idAtiva();
    recarregar();
}
