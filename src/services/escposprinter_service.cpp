#include "escposprinter_service.h"
#include <QProcess>

EscPosPrinter_service::EscPosPrinter_service(QObject *parent)
: QObject(parent)
{

}

bool EscPosPrinter_service::imprimirTeste(const QString &printerName, QString *erro)
{
    QByteArray dados;

    dados += inicializar();

    dados += alinharCentro();
    dados += "TESTE ESC/POS\n";
    dados += "ELGIN I9\n\n";

    dados += alinharEsquerda();
    dados += "Produto Teste\n";
    dados += "Preco: R$ 10,00\n\n";

    dados += "\n\n\n\n";
    dados += cortar();

    return imprimirRaw(printerName, dados, erro);
}

bool EscPosPrinter_service::imprimirRaw(const QString &printerName,
                                       const QByteArray &dados,
                                       QString *erro)
{
#ifdef Q_OS_LINUX

    QProcess process;

    process.start("lp", { "-d", printerName, "-o","raw"});

    if (!process.waitForStarted())
    {
        if (erro)
            *erro = "Não foi possível iniciar o comando lp.";

        return false;
    }

    process.write(dados);
    process.closeWriteChannel();

    if (!process.waitForFinished())
    {
        if (erro)
            *erro = "Falha ao enviar dados para a impressora.";

        return false;
    }

    return process.exitCode() == 0;

#else
    Q_UNUSED(printerName)
    Q_UNUSED(dados)

    if (erro)
        *erro = "Sistema operacional ainda não suportado.";

    return false;
#endif
}

QByteArray EscPosPrinter_service::inicializar() const
{
    return QByteArray("\x1B\x40", 2);
}

QByteArray EscPosPrinter_service::cortar() const
{
    return QByteArray("\x1D\x56\x00", 3);
}

QByteArray EscPosPrinter_service::alinharCentro() const
{
    return QByteArray("\x1B\x61\x01", 3);
}

QByteArray EscPosPrinter_service::alinharEsquerda() const
{
    return QByteArray("\x1B\x61\x00", 3);
}
