#include "sessao_service.h"
#include "caixa_service.h"
#include "operador_service.h"
#include "../infra/databaseconnection_service.h"
#include <QApplication>
#include <QTimer>
#include <QEvent>
#include <QDateTime>
#include <QDebug>

namespace {
// Intervalo do tique (timeout e revalidação) e margem do aviso antes da expiração.
constexpr int IntervaloTiqueMs = 30000;
constexpr int MargemAvisoSegundos = 120;
constexpr int IntervaloTiqueAtividadeMs = 2000;
constexpr qint64 ElevacaoMs = 10 * 60 * 1000;
}

Sessao_service::Sessao_service(QObject *parent)
    : QObject(parent)
    , repo()
{
    relogio.start();
    relogioProcesso.start();
    // fica ouvindo a aplicação inteira: mouse, teclado e roda reiniciam a contagem
    if (qApp)
        qApp->installEventFilter(this);
}

Sessao_service *Sessao_service::instancia()
{
    static Sessao_service *serv = new Sessao_service(qApp);
    return serv;
}

void Sessao_service::abrir(const SessaoDTO &nova, QString *erro, bool confiarNoDto)
{
    if (!nova.valida()) {
        if (erro) *erro = "Sessão inválida.";
        return;
    }

    SessaoDTO novaSessao = nova;

    // quem é e o que pode vem do banco, não do DTO que a tela montou
    if (!confiarNoDto) {
        Operador_service os;
        if (novaSessao.idOperador == kOperadorGerenteId) {
            if (!os.gerentePinDefinido()) {
                if (erro) *erro = "O PIN do gerente não está definido.";
                return;
            }
            novaSessao.gerente = true;
            novaSessao.pinGeral = true;
            novaSessao.marcaPinGeral = os.marcaPinGeral();
        } else {
            const OperadorDTO op = os.getPorId(novaSessao.idOperador);
            if (op.id <= 0 || !op.ativo || op.bloqueado) {
                if (erro) *erro = "Operador inválido, desativado ou bloqueado.";
                return;
            }
            novaSessao.nomeOperador = op.nome;
            novaSessao.gerente = op.gerente;
            novaSessao.pinGeral = false;
        }
    }

    // a sessão entra sempre com o terminal real desta máquina: LoginOperador devolve um
    // DTO sem terminal e é aqui que ele é resolvido
    novaSessao.terminal = Caixa_service::terminalAtual();
    if (novaSessao.entradaEm.isEmpty())
        novaSessao.entradaEm = QDateTime::currentDateTime().toString(Qt::ISODate);

    QString erroLog;
    // sessão deixada aberta pelo programa anterior (encerrado sem logout) fica com saída
    // agora. Precisa vir depois de resolver o terminal, senão o filtro não acha nada.
    repo.fecharSessoesPendentes(novaSessao.terminal, &erroLog);
    if (!erroLog.isEmpty())
        qDebug() << "Sessao: nao foi possivel fechar sessoes pendentes:" << erroLog;

    sessao_ = novaSessao;
    Caixa_service caixaServ;
    const CaixaDTO caixa = caixaServ.caixaAtual();
    idLog = repo.registrarEntrada(sessao_, caixa.aberto() ? caixa.id : 0, erro);

    bloqueada_ = false;
    elevadoAteMs = 0;
    avisoMostrado = false;
    expirando = false;
    relogio.restart();
    iniciarTique();

    emit sessaoMudou();
}

void Sessao_service::encerrar(const QString &motivo)
{
    if (!ativa())
        return;

    if (idLog > 0) {
        QString erro;
        if (!repo.registrarSaida(idLog, motivo, &erro))
            qDebug() << "Sessao: falha ao gravar saida:" << erro;
    }

    if (timerTique)
        timerTique->stop();
    avisoMostrado = false;
    bloqueada_ = false;
    elevadoAteMs = 0;
    sessao_ = SessaoDTO();
    idLog = 0;

    emit avisoTimeout(0);
    emit sessaoMudou();
}

void Sessao_service::encerrarNoFechamento()
{
    encerrar(QStringLiteral("ENCERRAMENTO"));
}

// ─── matriz de acesso ────────────────────────────────────────────────────────

bool Sessao_service::bootstrapPermitido() const
{
    // instalação nova: ainda não existe operador nem PIN do gerente
    Operador_service os;
    return !os.gerentePinDefinido() && os.listar(false).isEmpty();
}

bool Sessao_service::elevacaoValida() const
{
    return ativa() && elevadoAteMs > relogioProcesso.elapsed();
}

bool Sessao_service::autorizadoParaAdministrar()
{
    if (!ativa())
        return bootstrapPermitido();
    if (sessao_.gerente)
        return true;
    if (elevacaoValida()) {
        elevadoAteMs = relogioProcesso.elapsed() + ElevacaoMs;   // uso renova
        return true;
    }
    return false;
}

void Sessao_service::concederElevacao(const QString &acao)
{
    if (!ativa())
        return;
    elevadoAteMs = relogioProcesso.elapsed() + ElevacaoMs;
    registrarAcao("ELEVACAO_PIN_GERAL", acao);
}

// ─── timeout / bloqueio ──────────────────────────────────────────────────────

void Sessao_service::iniciarTique()
{
    if (!timerTique) {
        timerTique = new QTimer(this);
        timerTique->setInterval(IntervaloTiqueMs);
        connect(timerTique, &QTimer::timeout, this, &Sessao_service::tique);
    }
    timerTique->start();
}

void Sessao_service::iniciarTimeout(bool ativo, int minutos)
{
    timeoutAtivo = ativo && minutos > 0;
    minutosTimeout = minutos;
    avisoMostrado = false;
    relogio.restart();
    offsetTesteMs = 0;
    if (ativa())
        iniciarTique();
}

void Sessao_service::pararTimeout()
{
    timeoutAtivo = false;
    avisoMostrado = false;
}

void Sessao_service::registrarAtividade()
{
    registrarAtividadeInterna();
}

void Sessao_service::registrarAtividadeInterna()
{
    // sessão bloqueada só volta com o PIN: mexer no mouse não conta
    if (!ativa() || bloqueada_)
        return;

    relogio.restart();
    offsetTesteMs = 0;
    if (avisoMostrado) {
        avisoMostrado = false;
        emit avisoTimeout(0);
    }
}

void Sessao_service::dispensarAvisoTimeout()
{
    if (!avisoMostrado)
        return;
    avisoMostrado = false;
    emit avisoTimeout(0);
}

qint64 Sessao_service::segundosParado() const
{
    return (relogio.elapsed() + offsetTesteMs) / 1000;
}

void Sessao_service::registrarTelaDeVenda(QObject *tela, std::function<bool()> temItens)
{
    if (!tela)
        return;
    removerTelaDeVenda(tela);
    telasVenda.append({QPointer<QObject>(tela), std::move(temItens)});
}

void Sessao_service::removerTelaDeVenda(QObject *tela)
{
    for (int i = telasVenda.size() - 1; i >= 0; --i) {
        if (telasVenda.at(i).tela.isNull() || telasVenda.at(i).tela == tela)
            telasVenda.removeAt(i);
    }
}

bool Sessao_service::temVendaEmAndamento() const
{
    if (vendaEmAndamentoAtiva)
        return true;
    for (const TelaVenda &t : telasVenda) {
        if (!t.tela.isNull() && t.temItens && t.temItens())
            return true;
    }
    return verificadorVenda && verificadorVenda();
}

void Sessao_service::tique()
{
    if (!ativa() || expirando || bloqueada_)
        return;

    // antes de qualquer outra coisa: a sessão ainda vale? (operador desativado, PIN geral trocado...)
    revalidar();
    if (!ativa() || expirando)
        return;

    if (!timeoutAtivo || minutosTimeout <= 0)
        return;

    const qint64 limiteSegundos = qint64(minutosTimeout) * 60;
    const qint64 paradoSegundos = segundosParado();

    if (paradoSegundos < limiteSegundos - MargemAvisoSegundos)
        return;

    if (paradoSegundos < limiteSegundos) {
        if (!avisoMostrado) {
            avisoMostrado = true;
            emit avisoTimeout(int(limiteSegundos - paradoSegundos));
        }
        return;
    }

    // Passou do limite. Com venda aberta a sessão não é encerrada (a venda ficaria sem dono),
    // mas também não fica liberada: é bloqueada até o PIN ser informado de novo.
    if (temVendaEmAndamento())
        bloquearSessao();
    else
        expirarSessao();
}

void Sessao_service::expirarSessao()
{
    expirando = true;
    // Encerra ANTES de avisar. A UI trata o sinal pedindo um novo login (diálogo modal dentro do
    // slot): se o encerramento viesse depois, derrubaria a sessão recém-aberta.
    encerrar(QStringLiteral("TIMEOUT"));
    emit sessaoExpirada();
    expirando = false;
}

void Sessao_service::bloquearSessao()
{
    if (bloqueada_)
        return;
    expirando = true;
    bloqueada_ = true;
    avisoMostrado = false;
    emit avisoTimeout(0);
    registrarAcao("BLOQUEIO_SESSAO", "Inatividade com venda em andamento");
    emit sessaoMudou();
    // a UI pede o PIN (modal) dentro deste sinal e chama desbloquear()
    emit sessaoBloqueada();
    expirando = false;
}

Sessao_service::Resultado Sessao_service::desbloquear(const QString &pin)
{
    if (!ativa())
        return {false, "Não há sessão ativa."};
    if (!bloqueada_)
        return {true, QString()};

    Operador_service os;
    bool porGerente = false;
    bool ok = false;

    if (sessao_.pinGeral) {
        ok = os.gerentePinConfere(pin);
    } else if (os.pinConfere(sessao_.idOperador, pin)) {
        ok = true;
    } else if (os.gerentePinConfere(pin)) {
        ok = true;
        porGerente = true;
    }

    if (!ok) {
        // conta a tentativa contra quem está na sessão (3 erros bloqueiam o operador)
        if (sessao_.pinGeral)
            return os.validarGerentePin(pin).ok ? Resultado{true, QString()} : Resultado{false, "PIN incorreto."};
        const auto r = os.validarPin(sessao_.idOperador, pin);
        return {false, r.msg};
    }

    bloqueada_ = false;
    relogio.restart();
    offsetTesteMs = 0;
    registrarAcao(porGerente ? "DESBLOQUEIO_POR_GERENTE" : "DESBLOQUEIO_SESSAO", QString());
    emit sessaoMudou();
    return {true, "Sessão desbloqueada."};
}

// ─── revalidação ─────────────────────────────────────────────────────────────

void Sessao_service::invalidar(const QString &motivo)
{
    expirando = true;
    registrarAcao("SESSAO_INVALIDADA", motivo);
    encerrar(QStringLiteral("INVALIDADA"));
    emit sessaoInvalidada(motivo);
    expirando = false;
}

void Sessao_service::revalidar()
{
    if (!ativa())
        return;
    // banco fora do ar não é motivo para derrubar ninguém
    if (!DatabaseConnection_service::open())
        return;

    Operador_service os;
    if (sessao_.pinGeral) {
        if (!os.gerentePinDefinido() || os.marcaPinGeral() != sessao_.marcaPinGeral)
            invalidar("O PIN do gerente foi alterado. Entre novamente.");
        return;
    }

    const OperadorDTO op = os.getPorId(sessao_.idOperador);
    if (op.id <= 0 || !op.ativo) {
        invalidar("Este operador foi desativado.");
        return;
    }
    if (op.bloqueado) {
        invalidar("Este operador foi bloqueado.");
        return;
    }
    if (op.gerente != sessao_.gerente) {
        registrarAcao("PERMISSAO_ALTERADA",
                      op.gerente ? "Passou a gerente" : "Deixou de ser gerente");
        sessao_.gerente = op.gerente;
        if (!op.gerente)
            elevadoAteMs = 0;
        emit sessaoMudou();
    }
}

// ─── auditoria ───────────────────────────────────────────────────────────────

void Sessao_service::registrarAcao(const QString &acao, const QString &detalhe)
{
    LogAcaoDTO a;
    a.idOperadorSessao = idOperador();
    a.nomeOperador = sessao_.nomeOperador;
    a.terminal = Caixa_service::terminalAtual();
    a.acao = acao;
    a.detalhe = detalhe;
    QString erro;
    if (!repo.registrarAcao(a, &erro))
        qDebug() << "Auditoria: acao nao registrada:" << acao << erro;
}

bool Sessao_service::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::KeyPress:
    case QEvent::Wheel:
    case QEvent::TabletPress:
        registrarAtividadeInterna();
        break;
    case QEvent::MouseMove:
        // evento muito frequente: só conta de tempos em tempos
        if (relogio.elapsed() + offsetTesteMs > IntervaloTiqueAtividadeMs)
            registrarAtividadeInterna();
        break;
    default:
        break;
    }

    return QObject::eventFilter(obj, event);
}

QList<LogSessaoDTO> Sessao_service::ultimasSessoes(int limite)
{
    return repo.listar(limite);
}

QList<LogAcaoDTO> Sessao_service::ultimasAcoes(int limite)
{
    return repo.listarAcoes(limite);
}
