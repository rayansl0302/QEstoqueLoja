#ifndef LOGINOPERADOR_H
#define LOGINOPERADOR_H

#include <QDialog>
#include <QList>
#include <QToolButton>
#include "dto/Operador_dto.h"
#include "dto/Sessao_dto.h"
#include "services/operador_service.h"

namespace Ui {
class LoginOperador;
}

// Entrada do operador no programa. Sempre modal e antes de qualquer janela de venda:
// enquanto não houver sessão aceita o aplicativo não abre o PDV.
class LoginOperador : public QDialog
{
    Q_OBJECT

public:
    explicit LoginOperador(QWidget *parent = nullptr);
    ~LoginOperador();

    // Carrega a lista de operadores ativos.Quando somenteGerente é verdadeiro o
    // combo fica com a linha única "Entrar como gerente" (primeira instalação).
    void carregarOperadores(const QList<OperadorDTO> &operadores, bool somenteGerente);

    // Aviso exibido abaixo dos campos, em vermelho.
    void setAviso(const QString &texto);

    // Sessão aceita; idOperador == kOperadorGerenteId indica entrada pelo PIN geral.
    SessaoDTO sessao() const;

    // Executa a entrada do operador. Na primeira instalação (sem nenhum operador e
    // sem PIN geral) força a definição do PIN do gerente antes de mostrar a tela.
    // Devolve sessão vazia se o usuário desistir; nesse caso *cancelou fica true.
    static SessaoDTO executar(bool *cancelou, QWidget *parent = nullptr);

private slots:
    void on_Btn_Entrar_clicked();
    void on_Btn_Sair_clicked();

private:
    Ui::LoginOperador *ui;
    Operador_service operadorServ;
    SessaoDTO sessaoAtual;
    bool somenteGerente = false;

    void tentarEntrar();
    void selecionarCartao(int indice);
    QWidget *cartoes = nullptr;          // lista visual de operadores (no lugar do combo)
    QList<QToolButton *> botoesOperador;
    void limparPin();
};

#endif // LOGINOPERADOR_H
