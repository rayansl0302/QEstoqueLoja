#ifndef EMPRESA_REPOSITORY_H
#define EMPRESA_REPOSITORY_H

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include "../dto/Empresa_dto.h"

// Cadastro de empresas (tabela empresas, migração 19).
class Empresa_repository : public QObject
{
    Q_OBJECT
public:
    explicit Empresa_repository(QObject *parent = nullptr);

    QList<EmpresaDTO> listar(bool somenteAtivas);
    EmpresaDTO getPorId(qlonglong id);
    EmpresaDTO getPorCnpj(const QString &cnpj);
    qlonglong inserir(const EmpresaDTO &empresa, QString *erro = nullptr);
    bool atualizar(const EmpresaDTO &empresa, QString *erro = nullptr);
    bool definirAtiva(qlonglong id, bool ativa, QString *erro = nullptr);
    int quantidadeDeVendas(qlonglong id);

private:
    QSqlDatabase db;
};

#endif // EMPRESA_REPOSITORY_H
