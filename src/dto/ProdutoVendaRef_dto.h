#ifndef PRODUTOVENDAREF_DTO_H
#define PRODUTOVENDAREF_DTO_H

#include <QString>

// Uma venda em que o produto aparece (para explicar por que ele não pode ser apagado).
struct ProdutoVendaRefDTO {
    qlonglong idVenda = 0;        // 0 = item sem venda ligada
    QString dataHora;
    QString cliente;
    double quantidade = 0;
    double precoVendido = 0;      // unitário
    double valorFinalVenda = 0;
    QString formaPagamento;
    qlonglong idEmpresa = 0;
    bool estaPago = true;
};

#endif // PRODUTOVENDAREF_DTO_H
