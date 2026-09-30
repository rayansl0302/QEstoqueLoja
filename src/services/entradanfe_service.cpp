#include "entradanfe_service.h"
#include "../util/chaveacessoutil.h"
#include "../infra/apppath_service.h"
#include "../nota/acbrmanager.h"
#include "../nota/eventocienciaop.h"
#include "acbr_service.h"
#include <QRegularExpression>
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
        resumo.nnf = nf.nnf;
        resumo.serie = nf.serie;
        resumo.cuf = nf.cuf;
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

// ─── Busca pela chave de acesso ──────────────────────────────────────────────

void EntradaNfe_service::setConsultaChave(ConsultaChaveFn fn)
{
    consultaChave = std::move(fn);
}

void EntradaNfe_service::setEnviarCiencia(EnviarCienciaFn fn)
{
    enviarCiencia = std::move(fn);
}

EntradaNfe_service::Resultado EntradaNfe_service::verificarPodeBuscarPorChave()
{
    Config_service confServ;
    const ConfigDTO c = confServ.carregarTudo();

    QStringList faltas;
    if (c.certificadoPathFiscal.trimmed().isEmpty())
        faltas << "certificado digital A1 não configurado (caminho do arquivo .pfx)";
    else if (!QFile::exists(c.certificadoPathFiscal))
        faltas << "arquivo do certificado digital não encontrado: " + c.certificadoPathFiscal;
    if (c.senhaCertificadoFiscal.isEmpty())
        faltas << "senha do certificado digital não configurada";
    if (c.estadoEmpresa.trimmed().size() != 2 || c.cUfFiscal.trimmed().isEmpty())
        faltas << "UF da empresa (e o código da UF) não configurados";
    if (ChaveAcessoUtil::somenteDigitos(c.cnpjEmpresa).size() != 14)
        faltas << "CNPJ da empresa não configurado";
    if (c.tpAmbFiscal != 1)
        faltas << "o ambiente fiscal está em Homologação (a busca pela chave só funciona em Produção)";

    // a ciência da operação é assinada e validada pela ACBrLib, que precisa dos schemas XSD
    const QString schema = c.schemaPathFiscal.isEmpty() ? AppPath_service::schemaPath() : c.schemaPathFiscal;
    if (QDir(schema).entryList(QStringList() << "*.xsd").isEmpty())
        faltas << "schemas XSD da NF-e não encontrados em: " + schema;

    Resultado r;
    if (faltas.isEmpty()) {
        r.ok = true;
        return r;
    }
    r.erro = EntradaNfeErro::SemConfiguracao;
    r.msg = "Para buscar pela chave de acesso falta:\n• " + faltas.join("\n• ")
            + "\n\nVocê ainda pode lançar a nota com \"Importar XML...\", sem certificado.";
    return r;
}

QString EntradaNfe_service::consultarSefaz(const QString &chave, QString &erro)
{
    if (consultaChave)
        return consultaChave(chave);

    Config_service confServ;
    const ConfigDTO c = confServ.carregarTudo();
    try {
        ACBrNFe *acbr = AcbrManager::instance()->nfe();
        if (!acbr) {
            erro = "A biblioteca fiscal (ACBrLib) não está disponível.";
            return QString();
        }
        const QString cnpj = ChaveAcessoUtil::somenteDigitos(c.cnpjEmpresa);
        return QString::fromStdString(
            acbr->DistribuicaoDFePorChave(c.cUfFiscal.toInt(), cnpj.toStdString(), chave.toStdString()));
    } catch (const std::exception &e) {
        erro = QString("Falha ao consultar a SEFAZ: %1").arg(e.what());
    } catch (...) {
        erro = "Falha desconhecida ao consultar a SEFAZ.";
    }
    return QString();
}

EventoFiscalDTO EntradaNfe_service::enviarCienciaPadrao(const QString &chave)
{
    EventoCienciaOP evento(nullptr, chave);
    return evento.gerarEnviarRetorno();
}

void EntradaNfe_service::registrarCiencia(EventoFiscalDTO evento, qlonglong idNota)
{
    evento.idNf = idNota;
    auto r = eveServ.inserir(evento);
    if (!r.ok)
        qDebug() << "Não gravou o evento de ciência:" << r.msg;
}

EntradaNfe_service::Resultado EntradaNfe_service::buscarPorChave(const QString &texto)
{
    Resultado r;

    const ChaveAcessoInfo ci = ChaveAcessoUtil::analisar(texto);
    if (!ci.valida) {
        r.erro = EntradaNfeErro::ChaveInvalida;
        r.msg = ci.erro;
        return r;
    }
    const QString chave = ci.chave;
    r.chave = chave;

    // já lançada: só devolve a nota para a tela selecionar (sem consultar a SEFAZ)
    const qlonglong existente = nfServ.getIdFromChave(chave);
    if (existente > 0) {
        const NotaFiscalDTO nota = nfServ.getNotaById(existente);
        const bool completa = nota.finalidade == "ENTRADA EXTERNA"
                              && (nota.cstat == "100" || nota.cstat == "150");
        if (completa || (nota.finalidade != "resNFe" && nota.finalidade != "ENTRADA EXTERNA")) {
            r.erro = EntradaNfeErro::Duplicada;
            r.idNota = existente;
            r.msg = "Esta NF-e já está lançada em Compras.";
            return r;
        }
    }

    if (!consultaChave) {
        Resultado pode = verificarPodeBuscarPorChave();
        if (!pode.ok)
            return pode;
        Acbr_service acbrServ;
        auto cfg = acbrServ.configurarParaDFE();
        if (!cfg.ok) {
            r.erro = EntradaNfeErro::SemConfiguracao;
            r.msg = cfg.msg;
            return r;
        }
    }

    QString erro;
    const QString retorno = consultarSefaz(chave, erro);
    if (!erro.isEmpty()) {
        r.erro = EntradaNfeErro::SefazRejeitou;
        r.msg = erro;
        return r;
    }

    Resultado r1 = processarRetornoDistribuicao(retorno, chave);
    if (r1.erro != EntradaNfeErro::AguardandoXml)
        return r1;

    // Só veio o resumo: registra a Ciência da Operação e consulta UMA vez de novo.
    // (Não repetimos sozinhos: a SEFAZ bloqueia o CNPJ por excesso de consultas.)
    const EventoFiscalDTO ciencia = enviarCiencia ? enviarCiencia(chave) : enviarCienciaPadrao(chave);
    const bool cienciaOk = ciencia.cstat == "128" || ciencia.cstat == "135"
                           || ciencia.cstat == "136" || ciencia.cstat == "573"; // 573 = já manifestada antes
    if (!cienciaOk) {
        r1.ok = false;
        r1.erro = EntradaNfeErro::SefazRejeitou;
        r1.msg = QString("Não foi possível registrar a Ciência da Operação na SEFAZ (cStat %1 - %2). "
                         "Ela é necessária para liberar o XML completo da nota.")
                     .arg(ciencia.cstat, ciencia.justificativa);
        return r1;
    }

    QString erro2;
    const QString retorno2 = consultarSefaz(chave, erro2);
    if (!erro2.isEmpty()) {
        r.erro = EntradaNfeErro::SefazRejeitou;
        r.msg = erro2;
        return r;
    }

    Resultado r2 = processarRetornoDistribuicao(retorno2, chave);
    if (r2.ok) {
        if (ciencia.cstat != "573")
            registrarCiencia(ciencia, r2.idNota);
        return r2;
    }
    if (r2.erro == EntradaNfeErro::AguardandoXml) {
        r2.msg = "A Ciência da Operação foi enviada, mas a SEFAZ ainda não liberou o XML completo desta nota. "
                 "Nada foi lançado: aguarde alguns minutos e busque a chave novamente.";
    }
    return r2;
}

EntradaNfe_service::Resultado EntradaNfe_service::processarRetornoDistribuicao(const QString &retorno,
                                                                               const QString &chave)
{
    Resultado r;
    r.chave = chave;

    if (retorno.trimmed().isEmpty()) {
        r.erro = EntradaNfeErro::SefazRejeitou;
        r.msg = "A SEFAZ não devolveu resposta. Verifique a conexão com a internet e tente novamente.";
        return r;
    }

    auto campo = [](const QString &bloco, const QString &nome) {
        QRegularExpression re("^" + nome + R"(=([^\r\n]*))", QRegularExpression::MultilineOption);
        const QRegularExpressionMatch m = re.match(bloco);
        return m.hasMatch() ? m.captured(1).trimmed() : QString();
    };

    // início dos blocos de documento ([ResDFe001], [ResNFe001], [ProcNFe001]...)
    static const QRegularExpression inicioBloco(R"(^\[(ResDFe|ResNFe|ProcNFe|ResEvento|ProcEvento)\w*\]\s*$)",
                                                QRegularExpression::MultilineOption);
    QList<int> inicios;
    auto it = inicioBloco.globalMatch(retorno);
    while (it.hasNext())
        inicios << it.next().capturedStart();

    const QString cabecalho = inicios.isEmpty() ? retorno : retorno.left(inicios.first());
    const QString cStat = campo(cabecalho, "CStat");
    QString motivo = campo(cabecalho, "XMotivo");
    if (motivo.isEmpty())
        motivo = campo(cabecalho, "Msg");

    if (cStat == "137") {
        r.erro = EntradaNfeErro::SefazRejeitou;
        r.msg = "A SEFAZ não encontrou esta NF-e para o CNPJ da empresa. Confira se a chave está correta e se a "
                "nota foi emitida contra o CNPJ desta empresa (destinatário). Notas muito recentes podem levar "
                "alguns minutos para aparecer.";
        return r;
    }
    if (cStat == "656") {
        r.erro = EntradaNfeErro::SefazRejeitou;
        r.msg = "A SEFAZ bloqueou temporariamente as consultas deste CNPJ por excesso de consultas "
                "(consumo indevido, cStat 656). Aguarde cerca de 1 hora antes de tentar de novo. "
                "Enquanto isso, use \"Importar XML...\".";
        return r;
    }
    if (cStat == "593") {
        r.erro = EntradaNfeErro::SefazRejeitou;
        r.msg = "O CNPJ-base do certificado digital difere do CNPJ da empresa (cStat 593). "
                "Confira se o certificado A1 configurado é da empresa.";
        return r;
    }
    if (cStat != "138") {
        r.erro = EntradaNfeErro::SefazRejeitou;
        r.msg = QString("A SEFAZ rejeitou a consulta (cStat %1 - %2).").arg(cStat.isEmpty() ? "?" : cStat, motivo);
        const QString m = motivo.toLower();
        if (m.contains("interessad") || m.contains("destinat"))
            r.msg += " Confira se esta empresa é a destinatária da NF-e.";
        return r;
    }

    // percorre os documentos devolvidos: prefere o procNFe; guarda o resumo como alternativa
    QString procXml;
    bool temResumo = false;
    for (int i = 0; i < inicios.size(); ++i) {
        const int fim = (i + 1 < inicios.size()) ? inicios.at(i + 1) : retorno.size();
        const QString bloco = retorno.mid(inicios.at(i), fim - inicios.at(i));

        QString chDoc = campo(bloco, "chDFe");
        if (chDoc.isEmpty())
            chDoc = campo(bloco, "chNFe");
        if (chDoc != chave && !bloco.contains(chave))
            continue;

        const QString schema = campo(bloco, "schema");
        if (schema.contains("procNFe")) {
            const QString situacao = campo(bloco, "cSitNFe");
            if (situacao == "2" || situacao == "3") {
                r.erro = EntradaNfeErro::NaoAutorizada;
                r.msg = situacao == "3" ? "Esta NF-e foi cancelada pelo emitente e não pode ser lançada."
                                        : "Esta NF-e foi denegada pela SEFAZ e não pode ser lançada.";
                return r;
            }
            const int a = bloco.indexOf("<?xml");
            const int b = bloco.lastIndexOf("</nfeProc>");
            if (a >= 0 && b > a) {
                procXml = bloco.mid(a, b + int(QString("</nfeProc>").size()) - a);
            } else {
                // plano B: o arquivo que a própria ACBrLib gravou
                QString caminho = campo(bloco, "arquivo");
                caminho.replace('\\', '/');
                QFile f(caminho);
                if (!caminho.isEmpty() && f.open(QIODevice::ReadOnly))
                    procXml = QString::fromUtf8(f.readAll());
            }
        } else if (schema.contains("resNFe")) {
            temResumo = true;
            QString cnpj = campo(bloco, "CNPJCPF");
            if (cnpj.isEmpty())
                cnpj = campo(bloco, "CNPJ");
            r.cnpjEmit = ChaveAcessoUtil::somenteDigitos(cnpj);
        }
    }

    if (!procXml.isEmpty()) {
        Resultado imp = importarConteudo(procXml.toUtf8(), /*ignorarDestinatario=*/false);
        if (imp.erro == EntradaNfeErro::DestinatarioDiferente)
            imp.msg += " A nota não foi lançada. Se realmente precisa dela, use \"Importar XML...\" e confirme.";
        return imp;
    }
    if (temResumo) {
        r.erro = EntradaNfeErro::AguardandoXml;
        r.msg = "A SEFAZ devolveu apenas o resumo da nota. É preciso registrar a Ciência da Operação para "
                "liberar o XML completo.";
        return r;
    }

    r.erro = EntradaNfeErro::SefazRejeitou;
    r.msg = "A SEFAZ respondeu, mas sem a NF-e pedida. Tente novamente em alguns minutos.";
    return r;
}
