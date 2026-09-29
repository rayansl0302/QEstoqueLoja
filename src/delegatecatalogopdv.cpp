#include "delegatecatalogopdv.h"
#include <QPainter>

DelegateCatalogoPDV::DelegateCatalogoPDV(bool bordaSeZero, QObject *parent)
    : QStyledItemDelegate(parent), bordaSeZero(bordaSeZero)
{}

void DelegateCatalogoPDV::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);

    if (option->state & QStyle::State_Selected)
        return;

    const QModelIndex estoque = index.sibling(index.row(), colunaEstoque);
    if (estoque.data().toDouble() <= 0)
        option->backgroundBrush = QBrush(QColor(252, 232, 230));
}

void DelegateCatalogoPDV::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    if (bordaSeZero && index.data().toInt() <= 0) {
        painter->save();
        painter->setPen(QPen(Qt::red, 2));
        painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
        painter->restore();
    }
}
