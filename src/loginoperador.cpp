#include <QLineEdit>
#include "loginoperador.h"
#include "ui_loginoperador.h"
#include "operadores.h"

SessaoDTO LoginOperador::executar(bool *cancelou, QWidget *parent)
{
    if (cancelou)
        *cancelou = false;

    Operador_service serv;
    const QList<OperadorDTO> ativos = serv.listar(true);
    const bool cadastroVazio = serv.listar(false).isEmpty();

    // primeira instalação: sem nenhum operador no cadastro o PIN do gerente é o que
    // destrava o cadastro. Se existe cadastro e o PIN geral não foi definido, isso é
    // inconsistência de instalação: não se cria o PIN aqui, senão qualquer pessoa
    // assumiria a gerência só porque todos os operadores estão desativados.
    if (cadastroVazio && !serv.gerentePinDefinido()) {
        if (!Operadores::autenticarGerente(parent)) {
            if (cancelou)
                *cancelou = true;
            return SessaoDTO();
        }
    }

    LoginOperador dialog(parent);
    dialog.carregarOperadores(ativos, ativos.isEmpty());
    if (ativos.isEmpty()) {
        dialog.setAviso(cadastroVazio
                           ? QStringLiteral("Nenhum operador cadastrado. Entre com o PIN do gerente "
                                            "e cadastre o primeiro operador.")
                           : QStringLiteral("Nenhum operador ativo. Fale com o gerente para reativar "
                                            "o cadastro ou entre com o PIN do gerente."));
    }
    if (dialog.exec() != QDialog::Accepted) {
        if (cancelou)
            *cancelou = true;
        return SessaoDTO();
    }
    return dialog.sessao();
}

LoginOperador::LoginOperador(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginOperador)
{
    ui->setupUi(this);
    connect(ui->Ledit_Pin, &QLineEdit::returnPressed, this, &LoginOperador::on_Btn_Entrar_clicked);
    ui->Ledit_Pin->setFocus();
}

LoginOperador::~LoginOperador()
{
    delete ui;
}

void LoginOperador::carregarOperadores(const QList<OperadorDTO> &operadores, bool somenteGerente)
{
    this->somenteGerente = somenteGerente;
    ui->Cmb_Operador->clear();

    // operadores do cadastro primeiro
    for (const OperadorDTO &op : operadores) {
        const QString rotulo = op.gerente ? QStringLiteral("%1  (gerente)").arg(op.nome) : op.nome;
        ui->Cmb_Operador->addItem(rotulo, op.id);
    }

    // a linha do PIN geral do gerente é sempre a última: sem identidade de operador,
    // serve tanto para o primeiro acesso quanto para quem não tem cadastro
    ui->Cmb_Operador->addItem(QStringLiteral("Entrar como gerente"), kOperadorGerenteId);
    ui->Cmb_Operador->setItemData(ui->Cmb_Operador->count() - 1,
                                  QStringLiteral("Usa o PIN geral do gerente"),
                                  Qt::ToolTipRole);

    // sem nenhum operador ativo não há como escolher outra coisa
    if (somenteGerente)
        ui->Cmb_Operador->setCurrentIndex(ui->Cmb_Operador->count() - 1);

    ui->Cmb_Operador->setFocus();
    ui->Ledit_Pin->clear();
    ui->Lbl_Aviso->clear();
}

void LoginOperador::setAviso(const QString &texto)
{
    ui->Lbl_Aviso->setText(texto);
}

SessaoDTO LoginOperador::sessao() const
{
    return sessaoAtual;
}

void LoginOperador::limparPin()
{
    ui->Ledit_Pin->clear();
    ui->Ledit_Pin->setFocus();
}

void LoginOperador::tentarEntrar()
{
    const QVariant dados = ui->Cmb_Operador->currentData();
    if (!dados.isValid()) {
        setAviso("Selecione o operador.");
        return;
    }

    const QString pin = ui->Ledit_Pin->text();
    if (!Operador_service::pinValido(pin)) {
        setAviso("O PIN deve ter de 4 a 6 dígitos numéricos.");
        limparPin();
        return;
    }

    const qlonglong id = dados.toLongLong();
    QString erro;
    const SessaoDTO sessao = operadorServ.validarLogin(id, pin, &erro);
    if (sessao.nomeOperador.isEmpty()) {
        setAviso(erro.isEmpty() ? QStringLiteral("PIN incorreto.") : erro);
        limparPin();
        return;
    }

    sessaoAtual = sessao;
    ui->Lbl_Aviso->clear();
    accept();
}

void LoginOperador::on_Btn_Entrar_clicked()
{
    tentarEntrar();
}

void LoginOperador::on_Btn_Sair_clicked()
{
    reject();
}
