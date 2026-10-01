#include "icones.h"

#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QScreen>
#include <QSvgRenderer>

QPixmap Icones::pixmap(const QString &nome, const QColor &cor, int tamanho)
{
    static QHash<QString, QPixmap> cache;
    const qreal escala = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
    const QString chave = QStringLiteral("%1|%2|%3|%4").arg(nome, cor.name(), QString::number(tamanho), QString::number(escala));
    const auto it = cache.constFind(chave);
    if (it != cache.constEnd())
        return it.value();

    QFile arquivo(QStringLiteral(":/icones/%1.svg").arg(nome));
    QPixmap resultado;
    if (arquivo.open(QIODevice::ReadOnly)) {
        QByteArray svg = arquivo.readAll();
        svg.replace("currentColor", cor.name().toUtf8());
        QSvgRenderer renderer(svg);
        const int lado = qRound(tamanho * escala);
        resultado = QPixmap(lado, lado);
        resultado.fill(Qt::transparent);
        QPainter p(&resultado);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p);
        p.end();
        resultado.setDevicePixelRatio(escala);
    }
    cache.insert(chave, resultado);
    return resultado;
}

QIcon Icones::icone(const QString &nome, const QColor &cor, int tamanho)
{
    return QIcon(pixmap(nome, cor, tamanho));
}
