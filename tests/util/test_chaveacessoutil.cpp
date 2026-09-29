#include <QtTest>
#include "test_chaveacessoutil.h"
#include "util/chaveacessoutil.h"

namespace {
const QString kChaveNFe  = "29260211222333000181550010000001231123456780";
const QString kChaveNFCe = "29260211222333000181650010000001231123456783";
}

void TestChaveAcessoUtil::chave_valida()
{
    ChaveAcessoInfo i = ChaveAcessoUtil::analisar(kChaveNFe);
    QVERIFY2(i.valida, qPrintable(i.erro));
    QCOMPARE(i.cUf, QString("29"));
    QCOMPARE(i.cnpjEmit, QString("11222333000181"));
    QCOMPARE(i.modelo, 55);
    QCOMPARE(i.serie, 1);
    QCOMPARE(i.numero, 123LL);
}

void TestChaveAcessoUtil::aceita_espacos_e_pontuacao()
{
    QString formatada = "2926 0211 2223 3300 0181 5500 1000 0001 2311 2345 6780";
    ChaveAcessoInfo i = ChaveAcessoUtil::analisar(formatada + "\n");
    QVERIFY2(i.valida, qPrintable(i.erro));
    QCOMPARE(i.chave, kChaveNFe);
}

void TestChaveAcessoUtil::tamanho_errado()
{
    ChaveAcessoInfo i = ChaveAcessoUtil::analisar(kChaveNFe.left(43));
    QVERIFY(!i.valida);
    QVERIFY(i.erro.contains("44 dígitos"));
    QVERIFY(i.erro.contains("43"));
}

void TestChaveAcessoUtil::digito_verificador_errado()
{
    ChaveAcessoInfo i = ChaveAcessoUtil::analisar(kChaveNFe.left(43) + "1");
    QVERIFY(!i.valida);
    QVERIFY(i.erro.contains("verificador"));
}

void TestChaveAcessoUtil::nfce_e_recusada()
{
    ChaveAcessoInfo i = ChaveAcessoUtil::analisar(kChaveNFCe);
    QVERIFY(!i.valida);
    QVERIFY(i.erro.contains("NFC-e"));
    QCOMPARE(i.modelo, 65);
}

void TestChaveAcessoUtil::vazia()
{
    QVERIFY(!ChaveAcessoUtil::analisar("  ").valida);
}

void TestChaveAcessoUtil::uf_invalida()
{
    QString base = "99" + kChaveNFe.mid(2, 41);
    QString chave = base + QString::number(ChaveAcessoUtil::calcularDigitoVerificador(base));
    ChaveAcessoInfo i = ChaveAcessoUtil::analisar(chave);
    QVERIFY(!i.valida);
    QVERIFY(i.erro.contains("UF"));
}
