#ifndef ESCPOSPRINTER_SERVICE_H
#define ESCPOSPRINTER_SERVICE_H
#include <QObject>
#include <QByteArray>

class EscPosPrinter_service : public QObject
{
    Q_OBJECT
public:
    explicit EscPosPrinter_service(QObject *parent = nullptr);
    bool imprimirTeste(const QString &printerName, QString *erro = nullptr);

    bool imprimirRaw(const QString &printerName,
                     const QByteArray &dados,
                     QString *erro = nullptr);
private:
    QByteArray inicializar() const;
    QByteArray cortar() const;
    QByteArray alinharCentro() const;
    QByteArray alinharEsquerda() const;
signals:
};

#endif // ESCPOSPRINTER_SERVICE_H
