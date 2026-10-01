#include "config_repository.h"
#include "../infra/apppath_service.h"
#include "../infra/empresaativa.h"
#include <QSettings>
#include <QSql>
#include <quuid.h>
#include <QSqlDatabase>
#include <QSqlError>

namespace {
// Dados da empresa e fiscais (certificado, CSC, numeração de NF) valem por empresa. A empresa 1 usa as
// chaves de sempre (nada a migrar); as demais ficam em "empresa_<id>/...".
QString ke(const QString &chave)
{
    const qlonglong id = EmpresaAtiva::id();
    return id <= 1 ? chave : QStringLiteral("empresa_%1/").arg(id) + chave;
}
}

Config_repository::Config_repository(QObject *parent)
    : QObject{parent}
{}

ConfigDTO Config_repository::loadAll()
{
    ConfigDTO dto;
    QSettings s(AppPath_service::configPath(), QSettings::IniFormat);

    dto.nomeEmpresa             = s.value(ke("empresa/nome_empresa")).toString();
    dto.nomeFantasiaEmpresa     = s.value(ke("empresa/nfant_empresa")).toString();
    dto.enderecoEmpresa         = s.value(ke("empresa/endereco_empresa")).toString();
    dto.numeroEmpresa           = s.value(ke("empresa/numero_empresa")).toString();
    dto.bairroEmpresa           = s.value(ke("empresa/bairro_empresa")).toString();
    dto.cepEmpresa              = s.value(ke("empresa/cep_empresa")).toString();
    dto.cidadeEmpresa           = s.value(ke("empresa/cidade_empresa")).toString();
    dto.estadoEmpresa           = s.value(ke("empresa/estado_empresa")).toString();
    dto.emailEmpresa            = s.value(ke("empresa/email_empresa")).toString();
    dto.telefoneEmpresa         = s.value(ke("empresa/telefone_empresa")).toString();
    dto.cnpjEmpresa             = s.value(ke("empresa/cnpj_empresa")).toString();
    dto.logoPathEmpresa         = s.value(ke("empresa/caminho_logo_empresa")).toString();

    dto.regimeTribFiscal        = s.value(ke("fiscal/regime_trib")).toInt();
    dto.tpAmbFiscal             = s.value(ke("fiscal/tp_amb")).toInt();
    dto.idCscFiscal             = s.value(ke("fiscal/id_csc")).toString();
    dto.cscFiscal               = s.value(ke("fiscal/csc")).toString();
    dto.schemaPathFiscal        = s.value(ke("fiscal/caminho_schema")).toString();
    dto.certAcPathFiscal        = s.value(ke("fiscal/caminho_certac")).toString();
    dto.certificadoPathFiscal   = s.value(ke("fiscal/caminho_certificado")).toString();
    dto.senhaCertificadoFiscal  = s.value(ke("fiscal/senha_certificado")).toString();
    dto.cUfFiscal               = s.value(ke("fiscal/cuf")).toString();
    dto.cMunFiscal              = s.value(ke("fiscal/cmun")).toString();
    dto.iEstadFiscal            = s.value(ke("fiscal/iest")).toString();
    dto.cnpjRTFiscal            = s.value(ke("fiscal/cnpj_rt")).toString();
    dto.nomeRTFiscal            = s.value(ke("fiscal/nome_rt")).toString();
    dto.emailRTFiscal           = s.value(ke("fiscal/email_rt")).toString();
    dto.foneRTFiscal            = s.value(ke("fiscal/fone_rt")).toString();
    dto.idCSRTFiscal            = s.value(ke("fiscal/id_csrt")).toString();
    dto.hashCSRTFiscal          = s.value(ke("fiscal/hash_csrt")).toString();
    dto.emitNfFiscal            = s.value(ke("fiscal/emit_nf")).toString() == "1";
    dto.usarIbsFiscal           = s.value(ke("fiscal/usar_ibs")).toString() == "1";
    dto.nnfHomologFiscal        = s.value(ke("fiscal/nnf_homolog")).toInt();
    dto.nnfProdFiscal           = s.value(ke("fiscal/nnf_prod")).toInt();
    dto.nnfHomologNfeFiscal     = s.value(ke("fiscal/nnf_homolog_nfe")).toInt();
    dto.nnfProdNfeFiscal        = s.value(ke("fiscal/nnf_prod_nfe")).toInt();

    dto.ncmPadraoProduto        = s.value("produto/ncm_padrao").toString();
    dto.csosnPadraoProduto      = s.value("produto/csosn_padrao").toString();
    dto.pisPadraoProduto        = s.value("produto/pis_padrao").toString();
    dto.cestPadraoProduto       = s.value("produto/cest_padrao").toString();

    dto.porcentLucroFinanceiro  = s.value("financeiro/porcent_lucro").toDouble();
    dto.taxaDebitoFinanceiro    = s.value("financeiro/taxa_debito").toDouble();
    dto.taxaCreditoFinanceiro   = s.value("financeiro/taxa_credito").toDouble();

    dto.nomeEmail               = s.value("email/email_nome").toString();
    dto.smtpEmail               = s.value("email/email_smtp").toString();
    dto.contaEmail              = s.value("email/email_conta").toString();
    dto.usuarioEmail            = s.value("email/email_usuario").toString();
    dto.senhaEmail              = s.value("email/email_senha").toString();
    dto.portaEmail              = s.value("email/email_porta").toString();
    dto.sslEmail                = s.value("email/email_ssl").toString() == "1";
    dto.tlsEmail                = s.value("email/email_tls").toString() == "1";

    dto.nomeContador            = s.value("contador/contador_nome").toString();
    dto.emailContador           = s.value("contador/contador_email").toString();

    dto.driverDB = s.value("database/driver").toInt();
    dto.ipHostDB = s.value("database/ip").toString();
    dto.portaDB = s.value("database/porta").toString();
    dto.nomeDB = s.value("database/nome_db").toString();
    dto.userDB = s.value("database/usuario_db").toString();
    dto.senhaDB = s.value("database/senha").toString();
    dto.pathPastaSqliteDB = s.value("database/path_pasta_sqlite").toString();
    dto.pathPastaPostgreDB = s.value("database/path_pasta_postgre").toString();

    dto.impressoraNomeDispositivo = s.value("dispositivo/nome_impressora").toString();

    dto.caixaToleranciaValor   = s.value("caixa/tolerancia_valor", 2.0).toDouble();
    dto.caixaToleranciaPercent = s.value("caixa/tolerancia_percent", 0.5).toDouble();

    dto.caixaTimeoutAtivo    = s.value("caixa/timeout_ativo", true).toBool();
    dto.caixaTimeoutMinutos  = s.value("caixa/timeout_minutos", 30).toInt();
    if (dto.caixaTimeoutMinutos < 1)
        dto.caixaTimeoutMinutos = 1;

    return dto;
}

bool Config_repository::saveAll(const ConfigDTO &dto)
{
    QSettings s(AppPath_service::configPath(), QSettings::IniFormat);

    s.setValue(ke("empresa/nome_empresa"),          dto.nomeEmpresa);
    s.setValue(ke("empresa/nfant_empresa"),         dto.nomeFantasiaEmpresa);
    s.setValue(ke("empresa/endereco_empresa"),      dto.enderecoEmpresa);
    s.setValue(ke("empresa/numero_empresa"),        dto.numeroEmpresa);
    s.setValue(ke("empresa/bairro_empresa"),        dto.bairroEmpresa);
    s.setValue(ke("empresa/cep_empresa"),           dto.cepEmpresa);
    s.setValue(ke("empresa/cidade_empresa"),        dto.cidadeEmpresa);
    s.setValue(ke("empresa/estado_empresa"),        dto.estadoEmpresa);
    s.setValue(ke("empresa/email_empresa"),         dto.emailEmpresa);
    s.setValue(ke("empresa/telefone_empresa"),      dto.telefoneEmpresa);
    s.setValue(ke("empresa/cnpj_empresa"),          dto.cnpjEmpresa);
    s.setValue(ke("empresa/caminho_logo_empresa"),  dto.logoPathEmpresa);

    s.setValue(ke("fiscal/regime_trib"),            dto.regimeTribFiscal);
    s.setValue(ke("fiscal/tp_amb"),                 dto.tpAmbFiscal);
    s.setValue(ke("fiscal/id_csc"),                 dto.idCscFiscal);
    s.setValue(ke("fiscal/csc"),                    dto.cscFiscal);
    s.setValue(ke("fiscal/caminho_schema"),         dto.schemaPathFiscal);
    s.setValue(ke("fiscal/caminho_certac"),         dto.certAcPathFiscal);
    s.setValue(ke("fiscal/caminho_certificado"),    dto.certificadoPathFiscal);
    s.setValue(ke("fiscal/senha_certificado"),      dto.senhaCertificadoFiscal);
    s.setValue(ke("fiscal/cuf"),                    dto.cUfFiscal);
    s.setValue(ke("fiscal/cmun"),                   dto.cMunFiscal);
    s.setValue(ke("fiscal/iest"),                   dto.iEstadFiscal);
    s.setValue(ke("fiscal/cnpj_rt"),                dto.cnpjRTFiscal);
    s.setValue(ke("fiscal/nome_rt"),                dto.nomeRTFiscal);
    s.setValue(ke("fiscal/email_rt"),               dto.emailRTFiscal);
    s.setValue(ke("fiscal/fone_rt"),                dto.foneRTFiscal);
    s.setValue(ke("fiscal/id_csrt"),                dto.idCSRTFiscal);
    s.setValue(ke("fiscal/hash_csrt"),              dto.hashCSRTFiscal);
    s.setValue(ke("fiscal/emit_nf"),                dto.emitNfFiscal  ? "1" : "0");
    s.setValue(ke("fiscal/usar_ibs"),               dto.usarIbsFiscal ? "1" : "0");
    s.setValue(ke("fiscal/nnf_homolog"),            dto.nnfHomologFiscal);
    s.setValue(ke("fiscal/nnf_prod"),               dto.nnfProdFiscal);
    s.setValue(ke("fiscal/nnf_homolog_nfe"),        dto.nnfHomologNfeFiscal);
    s.setValue(ke("fiscal/nnf_prod_nfe"),           dto.nnfProdNfeFiscal);

    s.setValue("produto/ncm_padrao",            dto.ncmPadraoProduto);
    s.setValue("produto/csosn_padrao",          dto.csosnPadraoProduto);
    s.setValue("produto/pis_padrao",            dto.pisPadraoProduto);
    s.setValue("produto/cest_padrao",           dto.cestPadraoProduto);

    s.setValue("financeiro/porcent_lucro",      dto.porcentLucroFinanceiro);
    s.setValue("financeiro/taxa_debito",        dto.taxaDebitoFinanceiro);
    s.setValue("financeiro/taxa_credito",       dto.taxaCreditoFinanceiro);

    s.setValue("email/email_nome",              dto.nomeEmail);
    s.setValue("email/email_smtp",              dto.smtpEmail);
    s.setValue("email/email_conta",             dto.contaEmail);
    s.setValue("email/email_usuario",           dto.usuarioEmail);
    s.setValue("email/email_senha",             dto.senhaEmail);
    s.setValue("email/email_porta",             dto.portaEmail);
    s.setValue("email/email_ssl",               dto.sslEmail ? "1" : "0");
    s.setValue("email/email_tls",               dto.tlsEmail ? "1" : "0");

    s.setValue("contador/contador_nome",        dto.nomeContador);
    s.setValue("contador/contador_email",       dto.emailContador);

    s.setValue("database/driver", dto.driverDB);
    s.setValue("database/ip", dto.ipHostDB);
    s.setValue("database/porta", dto.portaDB);
    s.setValue("database/nome_db", dto.nomeDB);
    s.setValue("database/usuario_db", dto.userDB);
    s.setValue("database/senha", dto.senhaDB);
    s.setValue("database/path_pasta_sqlite", dto.pathPastaSqliteDB);
    s.setValue("database/path_pasta_postgre", dto.pathPastaPostgreDB);

    s.setValue("dispositivo/nome_impressora", dto.impressoraNomeDispositivo);

    s.setValue("caixa/tolerancia_valor",   dto.caixaToleranciaValor);
    s.setValue("caixa/tolerancia_percent", dto.caixaToleranciaPercent);

    s.setValue("caixa/timeout_ativo",   dto.caixaTimeoutAtivo);
    s.setValue("caixa/timeout_minutos", dto.caixaTimeoutMinutos);



    s.sync();
    return s.status() == QSettings::NoError;
}

bool Config_repository::testarConexaoBanco(const ConfigDTO &dto, QString &erro)
{
    QString connName = QUuid::createUuid().toString();

    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", connName);

    db.setHostName(dto.ipHostDB);
    db.setPort(dto.portaDB.toInt());
    db.setDatabaseName(dto.nomeDB);
    db.setUserName(dto.userDB);
    db.setPassword(dto.senhaDB);

    if (!db.open()) {
        erro = db.lastError().text();

        QSqlDatabase::removeDatabase(connName);
        return false;
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);

    return true;
}

ConfigDbDTO Config_repository::getConfigsDb(){

    ConfigDbDTO dto;
    QSettings s(AppPath_service::configPath(), QSettings::IniFormat);
    dto.driverDB = s.value("database/driver").toInt();
    dto.ipHostDB = s.value("database/ip").toString();
    dto.portaDB = s.value("database/porta").toString();
    dto.nomeDB = s.value("database/nome_db").toString();
    dto.userDB = s.value("database/usuario_db").toString();
    dto.senhaDB = s.value("database/senha").toString();
    dto.pathPastaSqliteDB = s.value("database/path_pasta_sqlite").toString();
    dto.pathPastaPostgreDB = s.value("database/path_pasta_postgre").toString();

    return dto;

}

bool Config_repository::saveMigration13Changes()
{
    QSettings s(AppPath_service::configPath(), QSettings::IniFormat);

    s.setValue("database/river",0);
    s.setValue("database/path_pasta_sqlite", AppPath_service::appDataPath());
    s.sync();
    return s.status() == QSettings::NoError;
}
