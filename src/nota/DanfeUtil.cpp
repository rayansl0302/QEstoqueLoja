#include "DanfeUtil.h"
#include <QSqlQuery>
#include "../configuracao.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <exception>

DanfeUtil::DanfeUtil(QObject *parent)
    : QObject{parent}
{
    QStringList dataLocations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    for (const QString &basePath : dataLocations) {
        QString candidateNfe = basePath + "/reports/DANFE-NFe.xml";
        QString candidateNFCe = basePath + "/reports/DANFE-NFCe.xml";
        if (QFileInfo::exists(candidateNfe)) {
            caminhoReportNFe = candidateNfe;
            caminhoReportNFCe = candidateNFCe;
            break;
        }
    }


    Config_service *confServ = new Config_service(this);
    configDTO = confServ->carregarTudo();
    QString caminhoCompletoLogo = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                              "/imagens/" + QFileInfo(configDTO.logoPathEmpresa).fileName();
    caminhoLogo = caminhoCompletoLogo;
}
bool DanfeUtil::imprimirNotaCliente(qlonglong idVenda, QString *erro)
{
    const QString impressora = configDTO.impressoraNomeDispositivo.trimmed();
    if (impressora.isEmpty()) {
        if (erro)
            *erro = "Nenhuma impressora térmica selecionada. Escolha em Configurações.";
        return false;
    }

    const QString xmlPathRelativo = notaServ.getXmlPathFromIdVenda(idVenda);
    if (xmlPathRelativo.isEmpty()) {
        if (erro)
            *erro = "Não foi possível localizar o XML da nota dessa venda.";
        return false;
    }

    const QString xmlPath = AppPath_service::pastaArmazenamentoArquivos() + "/" + xmlPathRelativo;
    if (!QFileInfo::exists(xmlPath)) {
        if (erro)
            *erro = "XML da nota não encontrado.";
        return false;
    }

    auto *nf = AcbrManager::instance()->nfe();
    if (!nf) {
        if (erro)
            *erro = "O módulo fiscal não está disponível para imprimir a nota.";
        return false;
    }

    try {
        nf->ConfigGravarValor("DANFE", "Impressora", impressora.toStdString());
        nf->ConfigGravarValor("DANFE", "MostraPreview", "0");
        nf->ConfigGravarValor("DANFENFCe", "ImprimeItens", "1");
        nf->ConfigGravarValor("DANFENFCe", "ViaConsumidor", "1");
        nf->LimparLista();
        nf->CarregarXML(xmlPath.toStdString());
        nf->Imprimir(impressora.toStdString(), 1, "", false, false, true, std::nullopt);
    } catch (const std::exception &e) {
        if (erro)
            *erro = QString::fromLocal8Bit(e.what());
        return false;
    }
    return true;
}

bool DanfeUtil::abrirDanfe(qlonglong idVenda){
    QString xmlPathRelativo = notaServ.getXmlPathFromIdVenda(idVenda);
    if (xmlPathRelativo.isEmpty()) {
        return false;
    }
    QString xmlPath = AppPath_service::pastaArmazenamentoArquivos() + "/" + xmlPathRelativo;
    if (xmlPath.isEmpty()) {
        return false;
    }
    qDebug() << "abrindo danfe do xml em: " << xmlPath;
    auto nf = AcbrManager::instance()->nfe();
    nf->LimparLista();
    nf->CarregarXML(xmlPath.toStdString());

    imprimirDanfe(nf);
    return true;
}
void DanfeUtil::imprimirDanfe(const ACBrNFe *nf){
    nf->Imprimir("", 1, "", true, true, std::nullopt, std::nullopt);
}
void DanfeUtil::setCaminhoLogo(QString logo){
    caminhoLogo = logo;
}

bool DanfeUtil::abrirDanfePorXml(const QString& xmlPath)
{
    if (xmlPath.isEmpty()) {
        qDebug() << "XML vazio";
        return false;
    }

    if (!QFileInfo::exists(xmlPath)) {
        qDebug() << "XML nao encontrado:" << xmlPath;
        return false;
    }

    auto nf = AcbrManager::instance()->nfe();
    nf->LimparLista();

    nf->CarregarXML(xmlPath.toStdString());


    imprimirDanfe(nf);
    return true;
}

bool DanfeUtil::abrirDanfePorXmlEvento(const QString& xmlPath)
{
    if (xmlPath.isEmpty()) {
        qDebug() << "XML vazio";
        return false;
    }

    if (!QFileInfo::exists(xmlPath)) {
        qDebug() << "XML nao encontrado:" << xmlPath;
        return false;
    }

    auto nf = AcbrManager::instance()->nfe();
    nf->LimparLista();

    nf->CarregarXML(xmlPath.toStdString());


    nf->ImprimirEvento(xmlPath.toStdString(), xmlPath.toStdString());
    return true;
}



