#ifndef ENTRADANFE_SERVICE_H
#define ENTRADANFE_SERVICE_H

#include <QObject>
#include <QByteArray>
#include <functional>
#include "notafiscal_service.h"
#include "produtonota_service.h"
#include "cliente_service.h"
#include "config_service.h"
#include "eventofiscal_service.h"
#include "../dto/EventoFiscal_dto.h"
#include "../util/nfxmlutil.h"

enum class EntradaNfeErro {
    Nenhum,
    ArquivoInvalido,       // não abriu / não é XML
    NaoEhNfe,              // XML de outro tipo (resumo, evento, NFC-e...)
    NaoAutorizada,         // sem protocolo, cancelada, denegada...
    NotaPropria,           // nota emitida por esta empresa
    Duplicada,             // já lançada em Compras
    DestinatarioDiferente, // destinatário não é o CNPJ da empresa (pode ser ignorado)
    Salvar,                // falha ao gravar arquivo ou banco
    // busca pela chave
    ChaveInvalida,
    SemConfiguracao,       // falta certificado, senha, UF ou ambiente de produção
    SefazRejeitou,         // SEFAZ respondeu com erro (msg explica)
    AguardandoXml          // ciência enviada, XML completo ainda não disponível
};

class EntradaNfe_service : public QObject
{
    Q_OBJECT
public:
    struct Resultado {
        bool ok = false;
        EntradaNfeErro erro = EntradaNfeErro::Nenhum;
        QString msg;
        qlonglong idNota = 0;  // nota já lançada/importada (também em Duplicada)
        QString chave;
        QString cnpjEmit;      // preenchido quando só veio o resumo da nota
    };

    // Consulta à SEFAZ (DistribuicaoDFePorChave) e envio da Ciência da Operação.
    // Os padrões usam a ACBrLib; podem ser trocados (testes).
    using ConsultaChaveFn = std::function<QString(const QString &chave)>;
    using EnviarCienciaFn = std::function<EventoFiscalDTO(const QString &chave)>;

    explicit EntradaNfe_service(QObject *parent = nullptr);

    // Pasta-base dos dados (padrão: AppPath_service::pastaArmazenamentoArquivos()). Usado nos testes.
    void setPastaBase(const QString &pasta);
    // CNPJ da empresa usado na conferência do destinatário (padrão: configuração). Usado nos testes.
    void setCnpjEmpresa(const QString &cnpj);
    void setConsultaChave(ConsultaChaveFn fn);
    void setEnviarCiencia(EnviarCienciaFn fn);

    // Importa um XML de NF-e (nfeProc) de um arquivo ou já lido.
    Resultado importarXml(const QString &arquivo, bool ignorarDestinatario = false);
    Resultado importarConteudo(const QByteArray &xml, bool ignorarDestinatario = false);

    // Buscar pela chave exige só certificado A1 (caminho e senha), UF e ambiente de Produção.
    // NÃO exige CSC/IdCSC nem "Emitir Notas Fiscais". A mensagem lista exatamente o que falta.
    Resultado verificarPodeBuscarPorChave();

    // Fluxo completo: valida a chave, não duplica, consulta a SEFAZ e, se vier só o resumo,
    // envia a Ciência da Operação e consulta uma única vez de novo.
    Resultado buscarPorChave(const QString &texto);

    // Interpreta o retorno de DistribuicaoDFePorChave (INI da ACBrLib): grava a nota se vier o
    // procNFe; devolve AguardandoXml se só vier o resumo; traduz os erros da SEFAZ.
    Resultado processarRetornoDistribuicao(const QString &retorno, const QString &chave);

private:
    NotaFiscal_service nfServ;
    ProdutoNota_service prodNotaServ;
    Cliente_service cliServ;
    EventoFiscal_service eveServ;
    NfXmlUtil xmlUtil;
    QString pastaBase;
    QString cnpjEmpresa;
    ConsultaChaveFn consultaChave;
    EnviarCienciaFn enviarCiencia;

    QString pastaEntradas() const;
    QString relativoAPasta(const QString &caminhoAbsoluto) const;
    bool salvarArquivo(const QString &caminho, const QByteArray &xml) const;
    QString consultarSefaz(const QString &chave, QString &erro);
    EventoFiscalDTO enviarCienciaPadrao(const QString &chave);
    void registrarCiencia(EventoFiscalDTO evento, qlonglong idNota);
};

#endif // ENTRADANFE_SERVICE_H
