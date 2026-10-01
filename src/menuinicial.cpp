#include "menuinicial.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
constexpr int kColunas = 5;
constexpr int kLargura = 158;
constexpr int kAltura = 112;

// o tamanho vem do sizeHint: com botão sem texto próprio o layout usaria o mínimo do estilo
class Tile : public QPushButton
{
public:
    QSize sizeHint() const override { return QSize(kLargura, kAltura); }
    QSize minimumSizeHint() const override { return QSize(kLargura, kAltura); }
};

const char *kEstiloNormal =
    "QPushButton { background: white; border: 1px solid #D5DEE9; border-radius: 12px; padding: 0; }"
    "QPushButton:hover { background: #EAF3FB; border: 2px solid #2B84BF; }"
    "QPushButton:pressed { background: #D3E8F8; }"
    "QPushButton:focus { border: 2px solid #2B84BF; }";

const char *kEstiloDestaque =
    "QPushButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2F8FCC, stop:1 #1F6A9C);"
    " border: 1px solid #174F75; border-radius: 12px; padding: 0; }"
    "QPushButton:hover { background: #1F6A9C; }"
    "QPushButton:pressed { background: #174F75; }"
    "QPushButton:focus { border: 2px solid #8CC4EC; }";
}

MenuInicial::MenuInicial(QWidget *parent)
    : QWidget(parent)
{
    auto *externo = new QVBoxLayout(this);
    externo->setContentsMargins(0, 0, 0, 0);

    auto *rolagem = new QScrollArea(this);
    rolagem->setWidgetResizable(true);
    rolagem->setFrameShape(QFrame::NoFrame);
    externo->addWidget(rolagem);

    auto *pagina = new QWidget;
    pagina->setObjectName(QStringLiteral("paginaMenuInicial"));
    pagina->setStyleSheet(QStringLiteral("#paginaMenuInicial { background: transparent; }"));
    rolagem->setWidget(pagina);
    rolagem->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));

    conteudo = new QVBoxLayout(pagina);
    conteudo->setContentsMargins(28, 20, 28, 24);
    conteudo->setSpacing(10);

    lblTitulo = new QLabel(pagina);
    lblTitulo->setStyleSheet(QStringLiteral("font-size: 22pt; font-weight: 700; color: #16324F; background: transparent;"));
    lblSubtitulo = new QLabel(pagina);
    lblSubtitulo->setStyleSheet(QStringLiteral("font-size: 11pt; color: #5B6B7F; background: transparent;"));
    conteudo->addWidget(lblTitulo);
    conteudo->addWidget(lblSubtitulo);
    conteudo->addSpacing(6);
    conteudo->addStretch(1);
}

void MenuInicial::definirSaudacao(const QString &titulo, const QString &subtitulo)
{
    lblTitulo->setText(titulo);
    lblSubtitulo->setText(subtitulo);
}

void MenuInicial::adicionarGrupo(const QString &titulo)
{
    auto *rotulo = new QLabel(titulo.toUpper());
    rotulo->setStyleSheet(QStringLiteral(
        "font-size: 10pt; font-weight: 700; color: #1E3A5F; letter-spacing: 1px;"
        " background: transparent; border-bottom: 2px solid #2B84BF; padding: 10px 0 4px 0;"));

    auto *grade = new QGridLayout;
    grade->setHorizontalSpacing(12);
    grade->setVerticalSpacing(12);
    grade->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    gradeAtual = grade;
    posicao = 0;

    // antes do stretch final
    const int indiceStretch = conteudo->count() - 1;
    conteudo->insertWidget(indiceStretch, rotulo);
    conteudo->insertLayout(indiceStretch + 1, grade);
}

void MenuInicial::adicionarBotao(const QString &emoji, const QString &titulo, const QString &dica,
                                 std::function<void()> acao, bool destaque)
{
    if (!gradeAtual)
        adicionarGrupo(QString());

    auto *botao = new Tile;
    botao->setFixedSize(kLargura, kAltura);
    botao->setCursor(Qt::PointingHandCursor);
    botao->setToolTip(dica);
    botao->setStyleSheet(QString::fromLatin1(destaque ? kEstiloDestaque : kEstiloNormal));
    botao->setAccessibleName(titulo);

    auto *layout = new QVBoxLayout(botao);
    layout->setContentsMargins(6, 10, 6, 8);
    layout->setSpacing(4);

    auto *icone = new QLabel(emoji);
    icone->setAlignment(Qt::AlignCenter);
    icone->setStyleSheet(QStringLiteral("font-size: 28pt; background: transparent; border: none;"));
    icone->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *texto = new QLabel(titulo);
    texto->setAlignment(Qt::AlignCenter);
    texto->setWordWrap(true);
    texto->setStyleSheet(QStringLiteral("font-size: 10.5pt; font-weight: 700; background: transparent; border: none; color: %1;")
                             .arg(destaque ? QStringLiteral("white") : QStringLiteral("#1E3A5F")));
    texto->setAttribute(Qt::WA_TransparentForMouseEvents);

    layout->addWidget(icone, 1);
    layout->addWidget(texto);

    connect(botao, &QPushButton::clicked, this, [acao]() { acao(); });
    gradeAtual->addWidget(botao, posicao / kColunas, posicao % kColunas);
    ++posicao;
}
