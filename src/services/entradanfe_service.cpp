#include "entradanfe_service.h"
#include "../util/chaveacessoutil.h"
#include "../infra/apppath_service.h"
#include <QDomDocument>
#include <QDomElement>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QSqlQueryModel>
#include <QDebug>

namespace {
QString textoTag(const QDomElement &pai, const QString &tag)
{
    QDomNode n = pai.elementsByTagName(tag).item(0);
    return n.isNull() ? QString() : n.toElement().text().trimmed();
}
}

EntradaNfe_service::EntradaNfe_service(QObject *parent)
    : QObject{parent}
{}

void EntradaNfe_service::setPastaBase(const QString &pasta)
{
    pastaBase = pasta;
}

void EntradaNfe_service::setCnpjEmpresa(const QString &cnpj)
{
    cnpjEmpresa = cnpj;
}

QString EntradaNfe_service::pastaEntradas() const
{
    const QString base = pastaBase.isEmpty() ? AppPath_service::pastaArmazenamentoArquivos() : pastaBase;
    return base + "/xmlNf/entradas";
}

// O banco guarda o caminho relativo à pasta de dados, para funcionar em qualquer computador da rede.
QString EntradaNfe_service::relativoAPasta(const QString &caminhoAbsoluto) const
{
    const QString base = pastaBase.isEmpty() ? AppPath_service::pastaArmazenamentoArquivos() : pastaBase;
    return QDir(base).relativeFilePath(caminhoAbsoluto);
}

bool EntradaNfe_service::salvarArquivo(const QString &caminho, const QByteArray &xml) const
{
    QDir().mkpath(QFileInfo(caminho).absolutePath());
    QFile f(caminho);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const bool ok = f.write(xml) == xml.size();
    f.close();
    return ok;
}

EntradaNfe_service::Resultado EntradaNfe_service::importarXml(const QString &arquivo, bool ignorarDestinatario)
{
    QFile f(arquivo);
    if (!f.open(QIODevice::ReadOnly)) {
        Resultado r;
        r.erro = EntradaNfeErro::ArquivoInvalido;
        r.msg = "Não foi possível abrir o arquivo: " + arquivo;
        return r;
    }
    const QByteArray xml = f.readAll();
    f.close();
    return importarConteudo(xml, ignorarDestinatario);
}

EntradaNfe_service::Resultado EntradaNfe_service::importarConteudo(const QByteArray &xml, bool ignorarDestinatario)
{
    Resultado r;

    QDomDocument doc;
    QString erroXml;
    if (!doc.setContent(xml, &erroXml)) {
        r.erro = EntradaNfeErro::ArquivoInvalido;
        r.msg = "O arquivo não é um XML válido.";
        return r;
    }

    QDomNodeList infList = doc.elementsByTagName("infNFe");
    if (infList.isEmpty()) {
        r.erro = EntradaNfeErro::NaoEhNfe;
        r.msg = "O XML não é uma NF-e. Use o XML completo da nota, com o protocolo de autorização (nfeProc).";
        return r;
    }
    const QDomElement inf = infList.at(0).toElement();

    QString id = inf.attribute("Id");
    const QString chave = id.startsWith("NFe") ? id.mid(3) : id;
    r.chave = chave;

    const ChaveAcessoInfo ci = ChaveAcessoUtil::analisar(chave);
    if (!ci.valida) {
        r.erro = EntradaNfeErro::NaoEhNfe;
        r.msg = (ci.modelo == 65) ? ci.erro : "Chave de acesso do XML inválida: " + ci.erro;
        return r;
    }
    if (textoTag(inf, "mod") != "55") {
        r.erro = EntradaNfeErro::NaoEhNfe;
        r.msg = "O XML não é de uma NF-e (modelo 55).";
        return r;
    }

    // precisa ter o protocolo de autorização
    QDomNodeList protList = doc.elementsByTagName("infProt");
    if (protList.isEmpty()) {
        r.erro = EntradaNfeErro::NaoAutorizada;
        r.msg = "O XML não tem o protocolo de autorização da SEFAZ. "
                "Use o XML completo da nota (nfeProc), não apenas a NF-e sem protocolo.";
        return r;
    }
    const QDomElement prot = protList.at(0).toElement();
    const QString cStat = textoTag(prot, "cStat");
    if (cStat != "100" && cStat != "150") {
        r.erro = EntradaNfeErro::NaoAutorizada;
        r.msg = QString("A NF-e não está autorizada (cStat %1 - %2).").arg(cStat, textoTag(prot, "xMotivo"));
        return r;
    }

    // emitente / destinatário
    const QDomElement emitEl = inf.elementsByTagName("emit").item(0).toElement();
    QString docEmit = textoTag(emitEl, "CNPJ");
    if (docEmit.isEmpty())
        docEmit = textoTag(emitEl, "CPF");

    QString cnpjEmp = cnpjEmpresa;
    if (cnpjEmp.isEmpty()) {
        Config_service confServ;
        cnpjEmp = confServ.carregarTudo().cnpjEmpresa;
    }
    cnpjEmp = ChaveAcessoUtil::somenteDigitos(cnpjEmp);

    if (!cnpjEmp.isEmpty() && docEmit == cnpjEmp) {
        r.erro = EntradaNfeErro::NotaPropria;
        r.msg = "Esta NF-e foi emitida por esta própria empresa (é uma nota de saída), não uma compra.";
        return r;
    }

    const QDomElement dest = inf.elementsByTagName("dest").item(0).toElement();
    QString docDest = textoTag(dest, "CNPJ");
    if (docDest.isEmpty())
        docDest = textoTag(dest, "CPF");
    if (!ignorarDestinatario && !cnpjEmp.isEmpty() && docDest != cnpjEmp) {
        r.erro = EntradaNfeErro::DestinatarioDiferente;
        r.msg = QString("O destinatário desta NF-e (%1) não é o CNPJ da empresa (%2).")
                    .arg(docDest.isEmpty() ? "não informado" : docDest, cnpjEmp);
        return r;
    }

    // já existe? (uma nota que só tem o resumo é completada; uma completa não é duplicada)
    qlonglong idNota = nfServ.getIdFromChave(chave);
    bool existeLinha = idNota > 0;
    if (existeLinha) {
        const NotaFiscalDTO existente = nfServ.getNotaById(idNota);
        const bool completa = existente.finalidade == "ENTRADA EXTERNA"
                              && (existente.cstat == "100" || existente.cstat == "150");
        if (completa || (existente.finalidade != "resNFe" && existente.finalidade != "ENTRADA EXTERNA")) {
            r.erro = EntradaNfeErro::Duplicada;
            r.idNota = idNota;
            r.msg = "Esta NF-e já está lançada em Compras.";
            return r;
        }
    }

    // grava o XML no local padrão (o banco guarda o caminho relativo)
    const QString caminho = pastaEntradas() + "/" + chave + "-nfe.xml";
    if (!salvarArquivo(caminho, xml)) {
        r.erro = EntradaNfeErro::Salvar;
        r.msg = "Não foi possível gravar o XML em: " + caminho;
        return r;
    }
    const QString caminhoRelativo = relativoAPasta(caminho);

    NotaFiscalDTO nf = xmlUtil.lerNotaFiscalDoXML(caminho);

    if (!existeLinha) {
        NotaFiscalDTO resumo;
        resumo.chNfe = chave;
        resumo.cnpjEmit = docEmit;
        resumo.finalidade = "resNFe";
        resumo.cstat = cStat;
        resumo.nProt = textoTag(prot, "nProt");
        resumo.tpAmb = nf.tpAmb;
        resumo.saida = false;
        resumo.valorTotal = nf.valorTotal;
        resumo.xmlPath = caminhoRelativo;
        const QDateTime dt = QDateTime::fromString(nf.dhEmi, Qt::ISODate);
        resumo.dhEmi = dt.toString("dd/MM/yyyy HH:mm:ss");

        auto rs = nfServ.salvarResNfe(resumo);
        if (!rs.ok) {
            r.erro = EntradaNfeErro::Salvar;
            r.msg = rs.msg;
            return r;
        }
        idNota = nfServ.getIdFromChave(chave);
    }

    // fornecedor
    const ClienteDTO emitente = xmlUtil.getEmitenteFromXML(caminho);
    if (cliServ.contarQuantosRegistrosPorCPFCNPJ(emitente.cpf) <= 0) {
        auto rc = cliServ.inserirClienteEmitente(emitente);
        if (!rc.ok) {
            r.erro = EntradaNfeErro::Salvar;
            r.msg = "Não foi possível cadastrar o fornecedor: " + rc.msg;
            return r;
        }
    }

    // itens (só se a nota ainda não tiver)
    QSqlQueryModel itens;
    prodNotaServ.listarPorNota(&itens, idNota);
    if (itens.rowCount() == 0) {
        const QList<ProdutoNotaDTO> produtos = xmlUtil.carregarProdutosDaNFe(caminho, idNota);
        auto rp = prodNotaServ.inserirListaProdutos(produtos);
        if (!rp.ok) {
            r.erro = EntradaNfeErro::Salvar;
            r.msg = "Não foi possível gravar os itens da nota: " + rp.msg;
            return r;
        }
    }

    // por último vira "ENTRADA EXTERNA": só aparece em Compras quando está completa
    nf.xmlPath = caminhoRelativo;
    nf.idEmissorCliente = cliServ.getIdFromCpfCnpj(emitente.cpf);
    auto ru = nfServ.updateWhereChave(nf, chave);
    if (!ru.ok) {
        r.erro = EntradaNfeErro::Salvar;
        r.msg = ru.msg;
        return r;
    }

    r.ok = true;
    r.idNota = idNota;
    r.msg = "NF-e importada com sucesso.";
    return r;
}
