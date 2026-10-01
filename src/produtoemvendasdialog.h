#ifndef PRODUTOEMVENDASDIALOG_H
#define PRODUTOEMVENDASDIALOG_H

#include <QDialog>
#include <QList>
#include <functional>
#include "dto/ProdutoVendaRef_dto.h"

// Mostra em quais vendas o produto aparece quando se tenta apagá-lo.
class ProdutoEmVendasDialog : public QDialog
{
    Q_OBJECT
public:
    // abrirVendas (opcional): leva à tela de Vendas
    ProdutoEmVendasDialog(const QString &descricaoProduto, const QList<ProdutoVendaRefDTO> &vendas,
                          std::function<void()> abrirVendas = nullptr, QWidget *parent = nullptr);
};

#endif // PRODUTOEMVENDASDIALOG_H
