#ifndef DELEGATEACOESCARRINHO_H
#define DELEGATEACOESCARRINHO_H

#include <QStyledItemDelegate>
#include <QIcon>

// Desenha os botões [−] [+] [lixeira] na última coluna do carrinho do PDV
// e avisa qual foi clicado. Não usa widgets de índice, então continua
// correto quando linhas são inseridas ou removidas.
class DelegateAcoesCarrinho : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit DelegateAcoesCarrinho(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *, const QStyleOptionViewItem &,
                          const QModelIndex &) const override { return nullptr; }
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;

signals:
    void menosClicado(int linha);
    void maisClicado(int linha);
    void removerClicado(int linha);

private:
    enum Botao { Nenhum = -1, Menos = 0, Mais = 1, Remover = 2 };
    QRect retanguloBotao(const QRect &celula, int botao) const;
    Botao botaoEm(const QRect &celula, const QPoint &pos) const;

    QIcon iconeMenos;
    QIcon iconeMais;
    QIcon iconeRemover;
};

#endif // DELEGATEACOESCARRINHO_H
