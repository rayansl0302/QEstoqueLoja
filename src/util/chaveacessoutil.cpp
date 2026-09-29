#include "chaveacessoutil.h"
#include <QStringList>

QString ChaveAcessoUtil::somenteDigitos(const QString &texto)
{
    QString r;
    for (const QChar &c : texto) {
        if (c.isDigit())
            r.append(c);
    }
    return r;
}

int ChaveAcessoUtil::calcularDigitoVerificador(const QString &chave43)
{
    int soma = 0;
    int peso = 2;
    for (int i = chave43.size() - 1; i >= 0; --i) {
        soma += chave43.at(i).digitValue() * peso;
        peso = (peso == 9) ? 2 : peso + 1;
    }
    const int resto = soma % 11;
    return resto < 2 ? 0 : 11 - resto;
}

ChaveAcessoInfo ChaveAcessoUtil::analisar(const QString &texto)
{
    ChaveAcessoInfo info;
    info.chave = somenteDigitos(texto);

    if (info.chave.isEmpty()) {
        info.erro = "Informe a chave de acesso (44 dígitos) ou bipe o código de barras do DANFE.";
        return info;
    }
    if (info.chave.size() != 44) {
        info.erro = QString("A chave de acesso deve ter 44 dígitos (foram informados %1).")
                        .arg(info.chave.size());
        return info;
    }

    const int dvInformado = info.chave.at(43).digitValue();
    if (calcularDigitoVerificador(info.chave.left(43)) != dvInformado) {
        info.erro = "Dígito verificador da chave inválido. Confira a digitação ou refaça a leitura do código de barras.";
        return info;
    }

    static const QStringList ufsValidas = {"11", "12", "13", "14", "15", "16", "17", "21", "22", "23",
                                           "24", "25", "26", "27", "28", "29", "31", "32", "33", "35",
                                           "41", "42", "43", "50", "51", "52", "53"};
    info.cUf = info.chave.mid(0, 2);
    if (!ufsValidas.contains(info.cUf)) {
        info.erro = QString("Código de UF %1 inválido na chave de acesso.").arg(info.cUf);
        return info;
    }

    info.cnpjEmit = info.chave.mid(6, 14);
    info.modelo = info.chave.mid(20, 2).toInt();
    info.serie = info.chave.mid(22, 3).toInt();
    info.numero = info.chave.mid(25, 9).toLongLong();

    if (info.modelo == 65) {
        info.erro = "Esta chave é de uma NFC-e (modelo 65), que não pode ser buscada na SEFAZ por aqui. "
                    "Use a chave de uma NF-e (modelo 55).";
        return info;
    }
    if (info.modelo != 55) {
        info.erro = QString("Modelo %1 não suportado: apenas NF-e (modelo 55).").arg(info.modelo);
        return info;
    }

    info.valida = true;
    return info;
}
