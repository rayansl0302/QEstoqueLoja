#ifndef SESSAO_SERVICE_H
#define SESSAO_SERVICE_H

#include <QObject>
#include <QElapsedTimer>
#include <QPointer>
#include <QString>
#include <functional>
#include <utility>
#include "../dto/Sessao_dto.h"
#include "../repository/sessao_repository.h"

class QTimer;
class QEvent;

// Sessão ativa do operador no sistema inteiro.
//
// A sessão existe apenas em memória: fechar e reabrir o programa sempre pede login de novo.
// Ela vale até logout explícito, troca de operador, timeout de inatividade ou invalidação
// (operador desativado/bloqueado, PIN do gerente trocado).
//
// Timeout: usa relógio monotônico (mudar a hora do Windows não antecipa nem atrasa).
//  - sem venda em andamento: a sessão é encerrada (TIMEOUT) e o login é pedido de novo;
//  - com venda em andamento: a sessão é BLOQUEADA (a venda continua intacta por trás) e só
//    volta com o PIN do operador (ou do gerente). Antes o relógio recomeçava a cada tique, e
//    um carrinho abandonado mantinha a sessão aberta para sempre.
class Sessao_service : public QObject
{
    Q_OBJECT
public:
    struct Resultado {
        bool ok = false;
        QString msg;
    };

    static Sessao_service *instancia();

    explicit Sessao_service(QObject *parent = nullptr);

    // --- sessão ---
    bool ativa() const { return sessao_.valida(); }
    SessaoDTO sessao() const { return sessao_; }
    // -1 quando não há sessão ativa: 0 está reservado para o gerente do PIN geral e não
    // pode ser gravado como se fosse "ninguém logado" (idOperadorSessao das tabelas).
    qlonglong idOperador() const { return ativa() ? sessao_.idOperador : -1; }
    QString nomeOperador() const { return sessao_.nomeOperador; }
    bool gerente() const { return sessao_.gerente; }

    // Abre a sessão. O privilégio (gerente) e a situação do operador são relidos do banco:
    // o DTO vindo da tela não é confiável. confiarNoDto só existe para o login de
    // desenvolvimento (main.cpp, build Debug).
    void abrir(const SessaoDTO &nova, QString *erro = nullptr, bool confiarNoDto = false);
    void encerrar(const QString &motivo);
    // fecha a sessão de saída já existindo quando o programa encerra
    void encerrarNoFechamento();

    // --- matriz de acesso ---
    // sessão de gerente com identidade no cadastro
    bool logadoComPrivilegio() const { return ativa() && sessao_.gerente; }
    // entrou pelo PIN geral de config_caixa (id 0), sem identidade de operador
    bool pinGeral() const { return ativa() && sessao_.pinGeral; }

    // Administração (cadastro de operadores, desbloqueio, redefinir PIN...): gerente logado ou
    // elevação por PIN geral ainda válida (10 min, renovada a cada uso). Sem nenhum operador
    // e sem PIN definido (instalação nova) é permitido. Registrada em Operador_service como
    // autorizador, vale também para telas que esqueçam de conferir.
    bool autorizadoParaAdministrar();
    // operador comum informou o PIN do gerente para uma ação: fica elevado por um tempo e a
    // elevação é gravada na auditoria
    void concederElevacao(const QString &acao);
    bool elevacaoValida() const;

    // --- timeout de inatividade ---
    void iniciarTimeout(bool ativo, int minutos);
    void pararTimeout();
    void registrarAtividade();
    // esconde o aviso de expiração; a sessão continua válida até o tempo normal
    void dispensarAvisoTimeout();

    // --- venda em andamento ---
    // durante o pagamento (venda já sendo gravada)
    void definirVendaEmAndamento(bool emAndamento) { vendaEmAndamentoAtiva = emAndamento; }
    // Cada tela de venda se registra com uma função que diz se há itens no carrinho. Vale para
    // o PDV, para "nova venda" na lista de Vendas e para qualquer outra tela de venda.
    void registrarTelaDeVenda(QObject *tela, std::function<bool()> temItens);
    void removerTelaDeVenda(QObject *tela);
    bool temVendaEmAndamento() const;
    // mantido por compatibilidade: verificador extra consultado junto com as telas registradas
    void definirVerificadorVenda(std::function<bool()> verificador) { verificadorVenda = std::move(verificador); }

    // --- bloqueio por inatividade com venda aberta ---
    bool bloqueada() const { return bloqueada_; }
    // aceita o PIN do próprio operador ou o PIN do gerente (com auditoria)
    Resultado desbloquear(const QString &pin);

    // --- revalidação ---
    // Relê o operador no banco: desativado, bloqueado ou com permissão alterada. Roda a cada tique
    // e antes de ações restritas. Invalida a sessão (sinal sessaoInvalidada) quando não vale mais.
    void revalidar();

    // --- auditoria ---
    void registrarAcao(const QString &acao, const QString &detalhe = QString());
    QList<LogSessaoDTO> ultimasSessoes(int limite = 100);
    QList<LogAcaoDTO> ultimasAcoes(int limite = 200);

    // Só para testes: adianta o relógio de inatividade sem esperar e dispara a verificação.
    void simularInatividadeParaTeste(int segundos) { offsetTesteMs += qint64(segundos) * 1000; }
    void tique();

signals:
    void sessaoMudou();
    // A sessão JÁ foi encerrada (motivo TIMEOUT) quando o sinal sai: quem trata pode abrir
    // um novo login sem que o serviço derrube a sessão nova depois.
    void sessaoExpirada();
    // sessão bloqueada com venda em andamento: a UI pede o PIN e chama desbloquear()
    void sessaoBloqueada();
    // a sessão foi encerrada porque deixou de valer (texto para mostrar ao operador)
    void sessaoInvalidada(const QString &motivo);
    // segundos até a expiração, para o aviso não-modal do rodapé
    void avisoTimeout(int segundosRestantes);

protected:
    // activity filter global: qualquer evento de usuário reinicia a contagem
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    struct TelaVenda {
        QPointer<QObject> tela;
        std::function<bool()> temItens;
    };

    void registrarAtividadeInterna();
    void iniciarTique();
    void expirarSessao();
    void bloquearSessao();
    void invalidar(const QString &motivo);
    qint64 segundosParado() const;
    bool bootstrapPermitido() const;

    SessaoDTO sessao_;
    Sessao_repository repo;
    qlonglong idLog = 0;

    QTimer *timerTique = nullptr;
    QElapsedTimer relogio;            // desde a última atividade
    qint64 offsetTesteMs = 0;
    QElapsedTimer relogioProcesso;    // para a elevação
    qint64 elevadoAteMs = 0;
    bool timeoutAtivo = false;
    int minutosTimeout = 0;
    bool vendaEmAndamentoAtiva = false;
    QList<TelaVenda> telasVenda;
    std::function<bool()> verificadorVenda;
    bool avisoMostrado = false;
    bool bloqueada_ = false;
    bool expirando = false;          // trava a reentrada durante o encerramento/bloqueio
};

#endif // SESSAO_SERVICE_H
