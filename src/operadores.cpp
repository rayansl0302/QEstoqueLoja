#include "operadores.h"
#include "ui_operadores.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QLineEdit>
#include "services/sessao_service.h"

namespace {
bool definirNovoPinGerente(QWidget *parent, Operador_service &serv)
{
    while (true) {
        bool ok = false;
        const QString pin = QInputDialog::getText(parent, "PIN do gerente",
            "Defina o PIN do gerente (4 a 6 dígitos):", QLineEdit::Password, QString(), &ok);
        if (!ok)
            return false;
        const QString conf = QInputDialog::getText(parent, "PIN do gerente",
            "Repita o PIN do gerente:", QLineEdit::Password, QString(), &ok);
        if (!ok)
            return false;
        if (pin != conf) {
            QMessageBox::warning(parent, "PIN do gerente", "Os PINs não conferem.");
            continue;
        }
        const auto r = serv.definirGerentePin(pin);
        if (r.ok)
            return true;
        QMessageBox::warning(parent, "PIN do gerente", r.msg);
    }
}
}

Operadores::Operadores(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Operadores)
    , model(new QSqlQueryModel(this))
{
    ui->setupUi(this);
    ui->Tview_Operadores->setModel(model);
    atualizarTabela();
}

Operadores::~Operadores()
{
    delete ui;
}

void Operadores::atualizarTabela()
{
    operadorServ.listar(model);
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Nome");
    model->setHeaderData(2, Qt::Horizontal, "Ativo");
    model->setHeaderData(3, Qt::Horizontal, "Gerente");
    model->setHeaderData(4, Qt::Horizontal, "Bloqueado");
    model->setHeaderData(5, Qt::Horizontal, "Erros de PIN");
    ui->Tview_Operadores->setColumnHidden(0, true);
    ui->Tview_Operadores->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int col = 2; col <= 5; ++col)
        ui->Tview_Operadores->resizeColumnToContents(col);
}

qlonglong Operadores::idSelecionado() const
{
    const QModelIndex idx = ui->Tview_Operadores->currentIndex();
    if (!idx.isValid())
        return 0;
    return model->data(model->index(idx.row(), 0)).toLongLong();
}

bool Operadores::pinsConferem(QString *pin)
{
    if (!Operador_service::pinValido(ui->Ledit_Pin->text())) {
        QMessageBox::warning(this, "Operadores", "O PIN deve ter de 4 a 6 dígitos numéricos.");
        ui->Ledit_Pin->setFocus();
        return false;
    }
    if (ui->Ledit_Pin->text() != ui->Ledit_PinConf->text()) {
        QMessageBox::warning(this, "Operadores", "Os PINs não conferem.");
        ui->Ledit_PinConf->setFocus();
        return false;
    }
    *pin = ui->Ledit_Pin->text();
    return true;
}

void Operadores::limparCampos()
{
    ui->Ledit_Nome->clear();
    ui->Ledit_Pin->clear();
    ui->Ledit_PinConf->clear();
    ui->Chk_Gerente->setChecked(false);
}

void Operadores::on_Btn_Cadastrar_clicked()
{
    QString pin;
    if (!pinsConferem(&pin))
        return;
    auto r = operadorServ.cadastrar(ui->Ledit_Nome->text(), pin);
    if (!r.ok) {
        QMessageBox::warning(this, "Operadores", r.msg);
        return;
    }
    if (ui->Chk_Gerente->isChecked()) {
        r = operadorServ.marcarGerente(r.id, true);
        if (!r.ok) {
            QMessageBox::warning(this, "Operadores",
                "O operador foi cadastrado, mas não pode ser gerente agora:\n" + r.msg);
        }
    }
    limparCampos();
    atualizarTabela();
}

void Operadores::on_Btn_Renomear_clicked()
{
    const qlonglong id = idSelecionado();
    if (id <= 0) {
        QMessageBox::information(this, "Operadores", "Selecione um operador na lista.");
        return;
    }
    bool ok = false;
    const QString nome = QInputDialog::getText(this, "Renomear operador", "Novo nome:", QLineEdit::Normal,
                                               operadorServ.getPorId(id).nome, &ok);
    if (!ok)
        return;
    const auto r = operadorServ.renomear(id, nome);
    if (!r.ok)
        QMessageBox::warning(this, "Operadores", r.msg);
    atualizarTabela();
}

void Operadores::on_Btn_RedefinirPin_clicked()
{
    const qlonglong id = idSelecionado();
    if (id <= 0) {
        QMessageBox::information(this, "Operadores", "Selecione um operador na lista.");
        return;
    }
    QString pin;
    if (!pinsConferem(&pin))
        return;
    const auto r = operadorServ.redefinirPin(id, pin);
    QMessageBox::information(this, "Operadores", r.msg);
    if (r.ok)
        limparCampos();
    atualizarTabela();
}

void Operadores::on_Btn_Desbloquear_clicked()
{
    const qlonglong id = idSelecionado();
    if (id <= 0) {
        QMessageBox::information(this, "Operadores", "Selecione um operador na lista.");
        return;
    }
    const auto r = operadorServ.desbloquear(id);
    if (!r.ok)
        QMessageBox::warning(this, "Operadores", r.msg);
    atualizarTabela();
}

void Operadores::on_Btn_AtivarDesativar_clicked()
{
    const qlonglong id = idSelecionado();
    if (id <= 0) {
        QMessageBox::information(this, "Operadores", "Selecione um operador na lista.");
        return;
    }
    const OperadorDTO op = operadorServ.getPorId(id);
    const auto r = operadorServ.definirAtivo(id, !op.ativo);
    if (!r.ok)
        QMessageBox::warning(this, "Operadores", r.msg);
    atualizarTabela();
}

void Operadores::on_Btn_Gerente_clicked()
{
    const qlonglong id = idSelecionado();
    if (id <= 0) {
        QMessageBox::information(this, "Operadores", "Selecione um operador na lista.");
        return;
    }
    const OperadorDTO op = operadorServ.getPorId(id);
    const bool novo = !op.gerente;
    if (novo) {
        const auto resp = QMessageBox::question(this, "Operadores",
            QString("Marcar %1 como gerente?\n\n"
                    "Gerentes podem abrir o histórico de caixas, o cadastro de operadores, "
                    "as configurações e os relatórios gerenciais.").arg(op.nome),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (resp != QMessageBox::Yes)
            return;
    }
    const auto r = operadorServ.marcarGerente(id, novo);
    if (!r.ok)
        QMessageBox::warning(this, "Operadores", r.msg);
    atualizarTabela();
}

void Operadores::on_Btn_Fechar_clicked()
{
    accept();
}

bool Operadores::exigirGerente(QWidget *parent, const QString &acao)
{
    Sessao_service *sessao = Sessao_service::instancia();
    sessao->revalidar();           // a permissão pode ter mudado desde o login
    if (!sessao->ativa())
        return false;
    if (sessao->sessao().gerente)
        return true;

    Operador_service serv;
    if (!serv.gerentePinDefinido()) {
        QMessageBox::warning(parent, "Acesso restrito",
            acao + QStringLiteral(" é restrito a gerentes.\n"
                                  "O PIN do gerente ainda não foi definido nesta instalação.\n"
                                  "Peça a um gerente para definí-lo."));
        return false;
    }
    bool ok = false;
    const QString pin = QInputDialog::getText(parent, "PIN do gerente",
        acao + QStringLiteral(" é restrito a gerentes.\nInforme o PIN do gerente:"),
        QLineEdit::Password, QString(), &ok);
    if (!ok)
        return false;
    const auto r = serv.validarGerentePin(pin);
    if (!r.ok) {
        QMessageBox::warning(parent, "Acesso restrito", r.msg);
        return false;
    }
    sessao->concederElevacao(acao);
    return true;
}

bool Operadores::autenticarGerente(QWidget *parent)
{
    Operador_service serv;
    if (!serv.gerentePinDefinido()) {
        // O PIN do gerente nasce junto com o primeiro operador. Se já existe cadastro e
        // ninguém logou como gerente, definir o PIN aqui daria a qualquer operador o
        // controle da loja — neste caso o caminho está fechado de propósito.
        if (!serv.listar(false).isEmpty() && !Sessao_service::instancia()->logadoComPrivilegio()) {
            QMessageBox::warning(parent, "PIN do gerente",
                "O PIN do gerente ainda não foi definido nesta instalação.\n"
                "Peça a um gerente para definí-lo.");
            return false;
        }

        QMessageBox::information(parent, "PIN do gerente",
            "Antes de gerenciar operadores é preciso definir o PIN do gerente.\n"
            "Ele protege o cadastro: só quem souber o PIN redefine ou desbloqueia o PIN dos operadores.\n"
            "Guarde-o em local seguro.");
        return definirNovoPinGerente(parent, serv);
    }
    for (int tentativa = 0; tentativa < 3; ++tentativa) {
        bool ok = false;
        const QString pin = QInputDialog::getText(parent, "PIN do gerente", "Informe o PIN do gerente:",
                                                  QLineEdit::Password, QString(), &ok);
        if (!ok)
            return false;
        const auto r = serv.validarGerentePin(pin);
        if (r.ok) {
            // sem isso o serviço recusaria o cadastro logo em seguida: operador comum que digitou
            // o PIN do gerente fica elevado por um tempo, com registro na auditoria
            Sessao_service::instancia()->concederElevacao(QStringLiteral("Cadastro de operadores / PIN do gerente"));
            return true;
        }
        QMessageBox::warning(parent, "PIN do gerente", r.msg);
        if (r.msg.contains("bloqueado", Qt::CaseInsensitive))
            return false;
    }
    return false;
}

void Operadores::alterarPinGerente(QWidget *parent)
{
    Operador_service serv;
    if (serv.gerentePinDefinido()) {
        // confirma com o PIN atual antes de trocar
        if (!autenticarGerente(parent))
            return;
        if (definirNovoPinGerente(parent, serv))
            QMessageBox::information(parent, "PIN do gerente", "PIN do gerente alterado.");
        return;
    }
    if (autenticarGerente(parent))
        QMessageBox::information(parent, "PIN do gerente", "PIN do gerente definido.");
}
