#ifndef RELATORIOCAIXA_SERVICE_H
#define RELATORIOCAIXA_SERVICE_H

#include <QObject>
#include <QLocale>
#include "../dto/Caixa_dto.h"
#include "config_service.h"

// Monta o relatório de fechamento de caixa em HTML (tela/PDF) e em ESC/POS (impressora térmica).
class RelatorioCaixa_service : public QObject
{
    Q_OBJECT
public:
    explicit RelatorioCaixa_service(QObject *parent = nullptr);

    QString gerarHtml(const ResumoCaixaDTO &r, const QString &descricaoTolerancia);
    bool salvarPdf(const QString &html, const QString &caminho, QString *erro = nullptr);
    bool imprimirTermica(const ResumoCaixaDTO &r, QString *erro = nullptr);

    static QString formatarDataHora(const QString &iso);

private:
    QLocale pt;
    Config_service confServ;

    QString dinheiro(double v) const;
    QString situacao(const FechamentoFormaDTO &f) const;
};

#endif // RELATORIOCAIXA_SERVICE_H
