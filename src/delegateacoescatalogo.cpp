#include "delegateacoescatalogo.h"
#include <QPainter>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QAbstractItemView>

namespace {
constexpr int kLargura = 32;
constexpr int kAltura = 26;
constexpr int kEspaco = 6;
constexpr int kColunaEstoque = 1;
}

DelegateAcoesCatalogo::DelegateAcoesCatalogo(QObject *parent)
    : QStyledItemDelegate(parent)
{
    iconeAdicionar.addFile(":/QEstoqueLOja/add-product.svg");
    iconeVer.addFile(":/QEstoqueLOja/amarok-cart-view.svg");
}

QRect DelegateAcoesCatalogo::retanguloBotao(const QRect &celula, int botao) const
{
    const int total = 2 * kLargura + kEspaco;
    const int x0 = celula.left() + (celula.width() - total) / 2;
    const int y0 = celula.top() + (celula.height() - kAltura) / 2;
    return QRect(x0 + botao * (kLargura + kEspaco), y0, kLargura, kAltura);
}

DelegateAcoesCatalogo::Botao DelegateAcoesCatalogo::botaoEm(const QRect &celula, const QPoint &pos) const
{
    for (int b = Adicionar; b <= Ver; ++b) {
        if (retanguloBotao(celula, b).contains(pos))
            return static_cast<Botao>(b);
    }
    return Nenhum;
}

void DelegateAcoesCatalogo::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QColor fundo = option.palette.base().color();
    if (option.state & QStyle::State_Selected)
        fundo = option.palette.highlight().color();
    else if (index.sibling(index.row(), kColunaEstoque).data().toDouble() <= 0)
        fundo = QColor(252, 232, 230);
    painter->fillRect(option.rect, fundo);

    const QIcon *icones[2] = {&iconeAdicionar, &iconeVer};
    for (int b = Adicionar; b <= Ver; ++b) {
        const QRect r = retanguloBotao(option.rect, b);
        painter->setPen(QPen(QColor(175, 185, 205), 1));
        painter->setBrush(QColor(245, 247, 250));
        painter->drawRoundedRect(r, 6, 6);
        icones[b]->paint(painter, r.adjusted(5, 3, -5, -3));
    }
    painter->restore();
}

bool DelegateAcoesCatalogo::editorEvent(QEvent *event, QAbstractItemModel *,
                                        const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QEvent::MouseMove) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const Botao botao = botaoEm(option.rect, mouse->position().toPoint());
        const char *dicas[] = {
            "Adicionar ao carrinho",
            "Ver produto"
        };
        if (botao == Nenhum)
            QToolTip::hideText();
        else
            QToolTip::showText(mouse->globalPosition().toPoint(), QString::fromUtf8(dicas[botao]));
        return false;
    }

    if (event->type() == QEvent::MouseButtonDblClick)
        return true;
    if (event->type() != QEvent::MouseButtonPress)
        return false;

    auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() != Qt::LeftButton)
        return false;

    switch (botaoEm(option.rect, mouse->position().toPoint())) {
    case Adicionar: emit adicionarClicado(index.row()); return true;
    case Ver:       emit verClicado(index.row());       return true;
    default:        return false;
    }
}

bool DelegateAcoesCatalogo::helpEvent(QHelpEvent *event, QAbstractItemView *view,
                                      const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() != QEvent::ToolTip)
        return QStyledItemDelegate::helpEvent(event, view, option, index);

    const Botao botao = botaoEm(option.rect, event->pos());
    const char *dicas[] = {
        "Adicionar ao carrinho",
        "Ver produto"
    };
    if (botao == Nenhum) {
        QToolTip::hideText();
        return false;
    }

    QToolTip::showText(event->globalPos(),
                       QString::fromUtf8(dicas[botao]), view->viewport(),
                       retanguloBotao(option.rect, botao));
    return true;
}
