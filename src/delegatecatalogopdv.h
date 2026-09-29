#ifndef DELEGATECATALOGOPDV_H
#define DELEGATECATALOGOPDV_H

#include <QStyledItemDelegate>

// Delegate das células do catálogo do PDV: destaca com um fundo discreto
// os produtos com estoque <= 0 (apenas visual, não bloqueia a venda).
// Com bordaSeZero = true também desenha a borda vermelha da coluna de estoque,
// como o CustomDelegate faz.
class DelegateCatalogoPDV : public QStyledItemDelegate
{
public:
    explicit DelegateCatalogoPDV(bool bordaSeZero, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;

private:
    bool bordaSeZero;
    static constexpr int colunaEstoque = 1;
};

#endif // DELEGATECATALOGOPDV_H
