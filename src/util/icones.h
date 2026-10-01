#ifndef ICONES_H
#define ICONES_H

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

// Ícones SVG (biblioteca Lucide, licença ISC: Imagens/icones/LICENSE-lucide.txt) embutidos no
// programa. São desenhados só com traço, então a cor é aplicada na hora: o mesmo arquivo serve
// para botão azul, branco, vermelho etc. Nomes: Imagens/icones/<nome>.svg.
namespace Icones {
QPixmap pixmap(const QString &nome, const QColor &cor, int tamanho = 24);
QIcon icone(const QString &nome, const QColor &cor, int tamanho = 24);
}

#endif // ICONES_H
