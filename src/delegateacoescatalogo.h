#ifndef DELEGATEACOESCATALOGO_H
#define DELEGATEACOESCATALOGO_H

#include <QStyledItemDelegate>
#include <QIcon>

// Botões [carrinho] [ver] na coluna Ações do catálogo do PDV.
class DelegateAcoesCatalogo : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit DelegateAcoesCatalogo(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *, const QStyleOptionViewItem &,
                          const QModelIndex &) const override { return nullptr; }
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
                   const QStyleOptionViewItem &option, const QModelIndex &index) override;

signals:
    void adicionarClicado(int linha);
    void verClicado(int linha);

private:
    enum Botao { Nenhum = -1, Adicionar = 0, Ver = 1 };
    QRect retanguloBotao(const QRect &celula, int botao) const;
    Botao botaoEm(const QRect &celula, const QPoint &pos) const;

    QIcon iconeAdicionar;
    QIcon iconeVer;
};

#endif // DELEGATEACOESCATALOGO_H
