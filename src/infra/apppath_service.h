#ifndef APPPATH_SERVICE_H
#define APPPATH_SERVICE_H

#include <QObject>
#include "../services/config_service.h"

class AppPath_service : public QObject
{
    Q_OBJECT
public:
    explicit AppPath_service(QObject *parent = nullptr);

    static QString appDataPath();
    static QString databasePath();
    static QString configPath();
    static QString xmlPath();
    static QString schemaPath();
    static QString imagesPath();
    static QString generalConfigPath();
    static QString nfeConfigPath();
    static QString consultaCnpjConfigPath();
    static QString mailConfigPath();
    static QString contingenciaPath();
    static QString pastaArmazenamentoArquivos();
    // Caminho de XML gravado no banco -> caminho utilizável. Aceita relativo (à pasta de dados)
    // e absoluto (notas baixadas antes por outro caminho).
    static QString resolverXmlPath(const QString &caminhoGravado);
private:
    Config_service confServ;
signals:
};

#endif // APPPATH_SERVICE_H
