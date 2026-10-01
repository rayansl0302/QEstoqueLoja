#ifndef EMPRESADIALOG_H
#define EMPRESADIALOG_H

#include <QDialog>
#include <QList>
#include <functional>
#include "dto/Empresa_dto.h"

class QGridLayout;
class QLabel;
class QToolButton;
class QPushButton;

// Escolha da empresa (CNPJ) em uso. Toda venda, conta a pagar e emissão fiscal feita depois da escolha
// pertence a ela. Cadastrar/desativar empresa é só do gerente (exigirGerente decide).
class EmpresaDialog : public QDialog
{
    Q_OBJECT
public:
    // titulo/instrucao permitem usar a mesma tela no começo do dia ("Em qual empresa vai trabalhar?")
    explicit EmpresaDialog(std::function<bool(const QString &)> exigirGerente,
                           const QString &titulo = QString(), QWidget *parent = nullptr);

    // Empresa escolhida e já ativada (0 se a pessoa só fechou a janela)
    qlonglong escolhida() const { return idEscolhida; }

private:
    std::function<bool(const QString &)> exigirGerente;
    QList<EmpresaDTO> empresas;
    QList<QToolButton *> cartoes;
    QWidget *areaCartoes = nullptr;
    QGridLayout *grade = nullptr;
    QLabel *lblAviso = nullptr;
    QPushButton *btnUsar = nullptr;
    QPushButton *btnDesativar = nullptr;
    qlonglong idSelecionada = 0;
    qlonglong idEscolhida = 0;

    void recarregar();
    void selecionar(qlonglong id);
    void usarSelecionada();
    void cadastrarNova();
    void desativarSelecionada();
};

#endif // EMPRESADIALOG_H
