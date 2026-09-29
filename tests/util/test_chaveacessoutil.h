#ifndef TEST_CHAVEACESSOUTIL_H
#define TEST_CHAVEACESSOUTIL_H

#include <QObject>

class TestChaveAcessoUtil : public QObject
{
    Q_OBJECT
private slots:
    void chave_valida();
    void aceita_espacos_e_pontuacao();
    void tamanho_errado();
    void digito_verificador_errado();
    void nfce_e_recusada();
    void vazia();
    void uf_invalida();
};

#endif // TEST_CHAVEACESSOUTIL_H
