#include "recibo_service.h"
#include "../infra/databaseconnection_service.h"
#include "config_service.h"
#include "escposprinter_service.h"
#include "../util/escposcomandos.h"

namespace {
constexpr int LarguraCupom = 42;

QByteArray linha(const QString &texto)
{
    return texto.toLocal8Bit() + '\n';
}

QString esquerdaDireita(const QString &esq, const QString &dir)
{
    const int espaco = LarguraCupom - esq.size() - dir.size();
    if (espaco >= 1)
        return esq + QString(espaco, QLatin1Char(' ')) + dir;
    return esq + QLatin1Char(' ') + dir;
}

QString centralizar(const QString &texto)
{
    if (texto.size() >= LarguraCupom)
        return texto.left(LarguraCupom);
    const int margem = (LarguraCupom - texto.size()) / 2;
    return QString(margem, QLatin1Char(' ')) + texto;
}

QString dinheiro(const QLocale &locale, double valor)
{
    return locale.toString(valor, 'f', 2);
}

QString formatarData(const QString &dataHora)
{
    QDateTime data;
    if (dataHora.contains('T'))
        data = QDateTime::fromString(dataHora, Qt::ISODateWithMs);
    else
        data = QDateTime::fromString(dataHora, "yyyy-MM-dd HH:mm:ss");
    if (!data.isValid())
        data = QDateTime::fromString(dataHora, "yyyy-MM-dd hh:mm:ss");
    return data.isValid() ? data.toString("dd/MM/yyyy HH:mm") : dataHora;
}
}

Recibo_service::Recibo_service(QObject *parent)
    : QObject{parent}
{
    db = DatabaseConnection_service::db();
    portugues = QLocale(QLocale::Portuguese, QLocale::Brazil);
}

bool Recibo_service::imprimirReciboVenda(qlonglong idvenda, QString *erro)
{
    const ConfigDTO configs = confServ.carregarTudo();
    const QString impressora = configs.impressoraNomeDispositivo.trimmed();
    if (impressora.isEmpty()) {
        if (erro)
            *erro = "Nenhuma impressora térmica selecionada. Escolha em Configurações.";
        return false;
    }

    const VendasDTO venda = vendaServ.getVenda(idvenda);
    if (venda.dataHora.isEmpty() && venda.clienteNome.isEmpty()) {
        if (erro)
            *erro = "Não foi possível carregar a venda para impressão.";
        return false;
    }

    QByteArray dados;
    dados += EscPosComandos::inicializar();
    dados += QByteArray("\x1B\x74\x10", 3);
    dados += EscPosComandos::alinharCentro();
    dados += EscPosComandos::bold(true);
    dados += linha(centralizar("CUPOM DE COMPRA"));
    dados += EscPosComandos::bold(false);
    dados += EscPosComandos::alinharEsquerda();

    if (!configs.nomeEmpresa.trimmed().isEmpty())
        dados += linha(configs.nomeEmpresa.trimmed());
    if (!configs.enderecoEmpresa.trimmed().isEmpty())
        dados += linha(configs.enderecoEmpresa.trimmed());
    if (!configs.cnpjEmpresa.trimmed().isEmpty())
        dados += linha(configs.cnpjEmpresa.trimmed());
    if (!configs.telefoneEmpresa.trimmed().isEmpty())
        dados += linha(configs.telefoneEmpresa.trimmed());

    dados += linha("Data/Hora: " + formatarData(venda.dataHora));
    dados += linha("Cliente: " + venda.clienteNome);
    dados += linha(QString(LarguraCupom, QLatin1Char('=')));
    dados += EscPosComandos::bold(true);
    dados += linha(esquerdaDireita("Qtd  Produto", "Valor"));
    dados += EscPosComandos::bold(false);

    const auto produtos = prodVendaServ.getProdutosVendidos(idvenda);
    for (const auto &produto : produtos) {
        const QString quantidade = portugues.toString(produto.quantidade, 'f', 2);
        QString descricao = produto.descricao.simplified();
        const int maxDesc = LarguraCupom - quantidade.size() - 3;
        if (descricao.size() > maxDesc && maxDesc > 3)
            descricao = descricao.left(maxDesc - 3) + "...";
        const QString valor = dinheiro(portugues, produto.precoVendido * produto.quantidade);
        dados += linha(quantidade + " x " + descricao);
        dados += linha(esquerdaDireita("", valor));
    }

    dados += linha(QString(LarguraCupom, QLatin1Char('=')));
    dados += linha(esquerdaDireita("Desconto (R$)", dinheiro(portugues, venda.desconto)));
    dados += linha("Forma: " + venda.formaPagamento);
    dados += linha(esquerdaDireita("Total produtos (R$)", dinheiro(portugues, venda.total)));

    if (venda.formaPagamento == "Dinheiro") {
        dados += linha(esquerdaDireita("Valor final (R$)", dinheiro(portugues, venda.valorFinal)));
        dados += linha(esquerdaDireita("Recebido (R$)", dinheiro(portugues, venda.valorRecebido)));
        dados += linha(esquerdaDireita("Troco (R$)", dinheiro(portugues, venda.troco)));
    } else if (venda.formaPagamento == "Crédito" || venda.formaPagamento == "Débito") {
        dados += linha(esquerdaDireita("Taxa (%)", dinheiro(portugues, venda.taxa)));
        dados += linha(esquerdaDireita("Valor final (R$)", dinheiro(portugues, venda.valorFinal)));
    } else if (venda.formaPagamento == "Prazo") {
        dados += linha(esquerdaDireita("Valor final (R$)", dinheiro(portugues, venda.valorFinal)));
        const QList<EntradaVendaDTO> entradas = entradaServ.getEntradasFromVenda(idvenda);
        double devendo = venda.valorFinal;
        int parcela = 1;
        for (const auto &entrada : entradas) {
            dados += linha(QString(LarguraCupom, QLatin1Char('-')));
            dados += EscPosComandos::bold(true);
            dados += linha("Parcela " + QString::number(parcela));
            dados += EscPosComandos::bold(false);
            dados += linha("Data: " + formatarData(entrada.dataHora));
            dados += linha("Forma: " + entrada.formaPagamento);
            dados += linha(esquerdaDireita("Descontado (R$)", dinheiro(portugues, entrada.total)));
            if (entrada.formaPagamento == "Dinheiro") {
                dados += linha(esquerdaDireita("Recebido (R$)", dinheiro(portugues, entrada.valorRecebido)));
                dados += linha(esquerdaDireita("Troco (R$)", dinheiro(portugues, entrada.troco)));
            } else if (entrada.formaPagamento == "Crédito" || entrada.formaPagamento == "Débito") {
                dados += linha(esquerdaDireita("Taxa (%)", dinheiro(portugues, entrada.taxa)));
                dados += linha(esquerdaDireita("Valor final (R$)", dinheiro(portugues, entrada.valorFinal)));
            }
            devendo -= entrada.total;
            ++parcela;
        }
        dados += EscPosComandos::bold(true);
        dados += linha(esquerdaDireita("Devendo (R$)", dinheiro(portugues, devendo)));
        dados += EscPosComandos::bold(false);
    } else {
        dados += linha(esquerdaDireita("Valor final (R$)", dinheiro(portugues, venda.valorFinal)));
    }

    dados += linha("");
    dados += linha("Assinatura:");
    dados += linha("");
    dados += EscPosComandos::alinharCentro();
    dados += linha("Obrigado pela compra");
    dados += linha("Volte sempre!");
    dados += EscPosComandos::feed(4);
    dados += EscPosComandos::cortar();

    EscPosPrinter_service impressoraServ;
    const auto resultado = impressoraServ.imprimirRaw(impressora, dados);
    if (!resultado.ok) {
        if (erro)
            *erro = resultado.msg.isEmpty() ? "Não foi possível imprimir o cupom." : resultado.msg;
        return false;
    }
    return true;
}
