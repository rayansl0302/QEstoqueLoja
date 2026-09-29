#ifndef CHAVEACESSOUTIL_H
#define CHAVEACESSOUTIL_H

#include <QString>

struct ChaveAcessoInfo {
    bool valida = false;
    QString erro;       // motivo, em português, quando !valida
    QString chave;      // só os 44 dígitos
    QString cUf;
    QString cnpjEmit;
    int modelo = 0;
    int serie = 0;
    qlonglong numero = 0;
};

// Validação da chave de acesso de 44 dígitos da NF-e (layout e dígito verificador módulo 11).
class ChaveAcessoUtil
{
public:
    // Remove tudo que não for dígito (espaços, pontos, hífens do código de barras/DANFE).
    static QString somenteDigitos(const QString &texto);
    static int calcularDigitoVerificador(const QString &chave43);
    // Só aceita NF-e (modelo 55): a distribuição DF-e não entrega NFC-e.
    static ChaveAcessoInfo analisar(const QString &texto);
};

#endif // CHAVEACESSOUTIL_H
