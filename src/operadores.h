#ifndef OPERADORES_H
#define OPERADORES_H

#include <QDialog>
#include <QSqlQueryModel>
#include "services/operador_service.h"

namespace Ui {
class Operadores;
}

class Operadores : public QDialog
{
    Q_OBJECT

public:
    explicit Operadores(QWidget *parent = nullptr);
    ~Operadores();

    // Pede o PIN do gerente (define na primeira vez). O cadastro de operadores só abre com ele,
    // para que um operador não consiga redefinir ou desbloquear o PIN de outro.
    static bool autenticarGerente(QWidget *parent);
    // Libera a ação de gerente: gerente logado passa direto; operador comum digita o PIN do gerente
    // (fica elevado por um tempo, com registro). false = negado ou cancelado.
    static bool exigirGerente(QWidget *parent, const QString &acao);
    static void alterarPinGerente(QWidget *parent);

private slots:
    void on_Btn_Cadastrar_clicked();
    void on_Btn_Renomear_clicked();
    void on_Btn_RedefinirPin_clicked();
    void on_Btn_Desbloquear_clicked();
    void on_Btn_AtivarDesativar_clicked();
    void on_Btn_Gerente_clicked();
    void on_Btn_Fechar_clicked();

private:
    Ui::Operadores *ui;
    Operador_service operadorServ;
    QSqlQueryModel *model;

    void atualizarTabela();
    qlonglong idSelecionado() const;
    bool pinsConferem(QString *pin);
    void limparCampos();
};

#endif // OPERADORES_H
