#ifndef ESCPOSPRINTER_SERVICE_H
#define ESCPOSPRINTER_SERVICE_H
#include <QObject>
#include <QByteArray>
#include <QImage>

class EscPosPrinter_service : public QObject
{
    Q_OBJECT
public:
    explicit EscPosPrinter_service(QObject *parent = nullptr);
    bool imprimirTeste(const QString &printerName, QString *erro = nullptr);

    bool imprimirRaw(const QString &printerName,
                     const QByteArray &dados,
                     QString *erro = nullptr);
    bool imprimirEtiquetas(const QString &printerName, int quantidade, const QImage &barcodeImage, const QString &descricao, double preco, QString *erro);
private:
signals:
};

#endif // ESCPOSPRINTER_SERVICE_H
