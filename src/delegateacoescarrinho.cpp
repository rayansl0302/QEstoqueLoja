#include "delegateacoescarrinho.h"
#include <QPainter>
#include <QMouseEvent>

namespace {
constexpr int kLargura = 36;
constexpr int kAltura = 28;
constexpr int kEspaco = 6;
}

DelegateAcoesCarrinho::DelegateAcoesCarrinho(QObject *parent)
    : QStyledItemDelegate(parent)
{
    iconeMenos.addFile(":/QEstoqueLOja/list-remove.svg");
    iconeMais.addFile(":/QEstoqueLOja/list-add.svg");
    iconeRemover.addFile(":/QEstoqueLOja/amarok-cart-remove.svg");
}

QRect DelegateAcoesCarrinho::retanguloBotao(const QRect &celula, int botao) const
{
    const int total = 3 * kLargura + 2 * kEspaco;
    const int x0 = celula.left() + (celula.width() - total) / 2;
    const int y0 = celula.top() + (celula.height() - kAltura) / 2;
    return QRect(x0 + botao * (kLargura + kEspaco), y0, kLargura, kAltura);
}

DelegateAcoesCarrinho::Botao DelegateAcoesCarrinho::botaoEm(const QRect &celula, const QPoint &pos) const
{
    for (int b = Menos; b <= Remover; ++b) {
        if (retanguloBotao(celula, b).contains(pos))
            return static_cast<Botao>(b);
    }
    return Nenhum;
}

void DelegateAcoesCarrinho::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillRect(option.rect, (option.state & QStyle::State_Selected)
                                       ? option.palette.highlight()
                                       : option.palette.base());

    const QIcon *icones[3] = {&iconeMenos, &iconeMais, &iconeRemover};
    for (int b = Menos; b <= Remover; ++b) {
        const QRect r = retanguloBotao(option.rect, b);
        painter->setPen(QPen(QColor(175, 185, 205), 1));
        painter->setBrush(QColor(245, 247, 250));
        painter->drawRoundedRect(r, 6, 6);
        icones[b]->paint(painter, r.adjusted(6, 3, -6, -3));
    }
    painter->restore();
}

bool DelegateAcoesCarrinho::editorEvent(QEvent *event, QAbstractItemModel *,
                                        const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() != QEvent::MouseButtonRelease)
        return false;

    auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() != Qt::LeftButton)
        return false;

    switch (botaoEm(option.rect, mouse->position().toPoint())) {
    case Menos:   emit menosClicado(index.row());   return true;
    case Mais:    emit maisClicado(index.row());    return true;
    case Remover: emit removerClicado(index.row()); return true;
    default:      return false;
    }
}
