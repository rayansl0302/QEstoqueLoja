    #include "mainwindow.h"

    #include <QApplication>
    #include <QLocale>
    #include <QTranslator>
    #include <qicon.h>
    #include <QStyleFactory>
    #include <QGuiApplication>
    #include <QTimer>
    #include <QMetaObject>

    int main(int argc, char *argv[])
    {
        QGuiApplication::setDesktopFileName("qestoqueloja");


        QApplication a(argc, argv);

        // Força estilo independente do sistema
        QApplication::setStyle(QStyleFactory::create("Fusion"));

        // Configura paleta clara manualmente
        QPalette lightPalette;

        lightPalette.setColor(QPalette::Window, QColor(255, 255, 255));
        lightPalette.setColor(QPalette::WindowText, Qt::black);
        lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
        lightPalette.setColor(QPalette::AlternateBase, QColor(220, 234, 245));
        lightPalette.setColor(QPalette::ToolTipBase, Qt::black);
        lightPalette.setColor(QPalette::ToolTipText, Qt::white);
        lightPalette.setColor(QPalette::Text, Qt::black);
        lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
        lightPalette.setColor(QPalette::ButtonText, Qt::black);
        lightPalette.setColor(QPalette::BrightText, Qt::red);
        lightPalette.setColor(QPalette::Link, QColor(0, 0, 255));

        lightPalette.setColor(QPalette::Highlight, QColor(0, 120, 215));
        lightPalette.setColor(QPalette::HighlightedText, Qt::white);

        a.setPalette(lightPalette);


        QTranslator translator;
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = "QEstoqueLoja_" + QLocale(locale).name();
            if (translator.load(":/i18n/" + baseName)) {
                a.installTranslator(&translator);
                break;
            }
        }
        MainWindow w;
        w.show();
        // atalho da Área de Trabalho: QEstoqueLoja --pdv abre direto a tela de venda
        const QStringList args = QCoreApplication::arguments();
        if (args.contains("--pdv"))
            w.abrirPdv();

        const int prev = args.indexOf("--preview-caixa");
        if (prev >= 0 && prev + 1 < args.size()) {
            const QString tela = args.at(prev + 1);
            QTimer::singleShot(800, &w, [tela, &w]() {
                const char *slot = nullptr;
                if (tela == "operadores") slot = "operadoresClicked";
                else if (tela == "abrir") slot = "abrirCaixaClicked";
                else if (tela == "fechar") slot = "fecharCaixaClicked";
                else if (tela == "historico") slot = "historicoCaixaClicked";
                else if (tela == "sangria") slot = "sangriaClicked";
                if (slot)
                    QMetaObject::invokeMethod(&w, slot);
            });
        }
        return a.exec();
    }
