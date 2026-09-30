#ifndef CAIXA_REPOSITORY_H
#define CAIXA_REPOSITORY_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQueryModel>
#include "../dto/Caixa_dto.h"

class Caixa_repository : public QObject
{
    Q_OBJECT
public:
    explicit Caixa_repository(QObject *parent = nullptr);

    // caixas
    qlonglong abrir(const CaixaDTO &caixa, QString *erro = nullptr);
    CaixaDTO getPorId(qlonglong id);
    CaixaDTO getAbertoNoTerminal(const QString &terminal);
    CaixaDTO getAbertoDoOperador(qlonglong idOperador);
    double getDinheiroInformadoUltimoFechamento(const QString &terminal);
    bool fechar(qlonglong idCaixa, const QString &observacao, const QList<FechamentoFormaDTO> &formas,
                QString *erro = nullptr);
    void listarHistorico(QSqlQueryModel *model, const QString &de, const QString &ate);

    // movimentacoes
    qlonglong inserirMovimentacao(const MovimentacaoCaixaDTO &mov, QString *erro = nullptr);
    MovimentacaoCaixaDTO getMovimentacaoPorEntradaVenda(qlonglong idEntradaVenda);
    bool deletarMovimentacao(qlonglong id, QString *erro = nullptr);
    bool deletarRecebimentosPorVenda(qlonglong idVenda, QString *erro = nullptr);
    // recebimentos desta venda lançados em caixas que não estão mais abertos
    int contarRecebimentosEmCaixaFechado(qlonglong idVenda);
    QList<MovimentacaoCaixaDTO> listarMovimentacoes(qlonglong idCaixa);

    // totais
    QList<VendasFormaDTO> vendasPorForma(qlonglong idCaixa);
    QList<FechamentoFormaDTO> getFechamento(qlonglong idCaixa);

private:
    QSqlDatabase db;
    CaixaDTO lerCaixa(class QSqlQuery &query);
    MovimentacaoCaixaDTO lerMovimentacao(class QSqlQuery &query);
};

#endif // CAIXA_REPOSITORY_H
