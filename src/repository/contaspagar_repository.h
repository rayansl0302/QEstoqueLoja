#ifndef CONTASPAGAR_REPOSITORY_H
#define CONTASPAGAR_REPOSITORY_H

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include "../dto/ContasPagar_dto.h"

// Contas a pagar (tabela contas_pagar, migração 20).
class ContasPagar_repository : public QObject
{
    Q_OBJECT
public:
    explicit ContasPagar_repository(QObject *parent = nullptr);

    // grava todas as parcelas ou nenhuma
    bool inserirVarias(const QList<ContaPagarDTO> &contas, QList<qlonglong> *ids = nullptr, QString *erro = nullptr);
    ContaPagarDTO getPorId(qlonglong id);
    QList<ContaPagarDTO> listar(const FiltroContasPagarDTO &filtro);
    QList<ContaPagarDTO> listarDoGrupo(const QString &grupo);

    // só mexem em conta no estado esperado (a condição vai no WHERE: dois computadores não se atropelam)
    bool baixar(qlonglong id, double valorPago, const QString &forma, const QString &pagoEm, QString *erro = nullptr);
    bool estornarBaixa(qlonglong id, QString *erro = nullptr);
    bool cancelar(qlonglong id, const QString &motivo, QString *erro = nullptr);
    bool atualizarAberta(const ContaPagarDTO &conta, QString *erro = nullptr);

    ResumoContasPagarDTO resumo(qlonglong idEmpresa, const QString &hoje);

private:
    QSqlDatabase db;
};

#endif // CONTASPAGAR_REPOSITORY_H
