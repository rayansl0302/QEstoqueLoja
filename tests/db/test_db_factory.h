#ifndef TEST_DB_FACTORY_H
#define TEST_DB_FACTORY_H

#include <QSqlDatabase>

class TestDbFactory
{
public:
    static QSqlDatabase create();
    // static QSqlDatabase createPostgres();
    static QSqlDatabase createPostgres();
    static void removerBDAtual();
    static void garantirCaixaAberto();
private:
    static QString currentConnectionName;
    static QString currentDatabaseName;
private slots:

};

#endif
