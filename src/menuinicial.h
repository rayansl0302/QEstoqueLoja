#ifndef MENUINICIAL_H
#define MENUINICIAL_H

#include <QWidget>
#include <functional>

class QLabel;
class QVBoxLayout;
class QGridLayout;

// Tela inicial do sistema: botões grandes agrupados por assunto (Atendimento, Cadastros, Caixa...),
// no lugar de abrir já listando produtos. Só monta a tela; quem usa define o que cada botão faz.
class MenuInicial : public QWidget
{
    Q_OBJECT
public:
    explicit MenuInicial(QWidget *parent = nullptr);

    void definirSaudacao(const QString &titulo, const QString &subtitulo);
    void adicionarGrupo(const QString &titulo);
    // emoji: ícone do botão; destaque: botão azul cheio (ação principal)
    void adicionarBotao(const QString &emoji, const QString &titulo, const QString &dica,
                        std::function<void()> acao, bool destaque = false);

private:
    QLabel *lblTitulo;
    QLabel *lblSubtitulo;
    QVBoxLayout *conteudo;
    QGridLayout *gradeAtual = nullptr;
    int posicao = 0;
};

#endif // MENUINICIAL_H
