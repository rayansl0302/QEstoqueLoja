#include "operador_service.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>
#include <QSysInfo>

namespace {
// comparação sem atalho: o tempo não revela em que posição o hash difere
bool iguaisTempoConstante(const QString &a, const QString &b)
{
    // percorre sempre o maior dos dois (sem sair cedo por tamanho diferente)
    const int n = qMax(a.size(), b.size());
    uint diferenca = uint(a.size() ^ b.size());
    for (int i = 0; i < n; ++i) {
        const ushort ca = i < a.size() ? a.at(i).unicode() : 0;
        const ushort cb = i < b.size() ? b.at(i).unicode() : 0;
        diferenca |= uint(ca ^ cb);
    }
    return diferenca == 0;
}

// Tentativas e bloqueio do PIN do gerente valem por computador: errar (ou alguém errar de
// propósito) em um caixa não deixa todos os outros sem acesso ao PIN.
QString terminalLocal()
{
    const QString host = QSysInfo::machineHostName().trimmed();
    return host.isEmpty() ? QStringLiteral("TERMINAL") : host.toUpper();
}
QString chaveTentativas() { return "gerente_tentativas_" + terminalLocal(); }
QString chaveBloqueio() { return "gerente_bloqueio_ate_" + terminalLocal(); }

constexpr int MaxTentativasGerente = 5;
constexpr int BloqueioGerenteSegundos = 300;
}

std::function<bool()> Operador_service::autorizador;

Operador_service::Operador_service(QObject *parent)
    : QObject{parent}
{}

void Operador_service::definirAutorizador(std::function<bool()> fn)
{
    autorizador = std::move(fn);
}

bool Operador_service::autorizado()
{
    return !autorizador || autorizador();
}

Operador_service::Resultado Operador_service::negado()
{
    return {false, "Esta ação é restrita a gerentes."};
}

bool Operador_service::pinValido(const QString &pin)
{
    static const QRegularExpression re("^\\d{4,6}$");
    return re.match(pin).hasMatch();
}

QString Operador_service::gerarSalt()
{
    QByteArray salt(16, '\0');
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(salt.data()), salt.size() / 4);
    return salt.toHex();
}

QString Operador_service::hashPin(const QString &pin, const QString &salt)
{
    // SHA-256 iterado com salt por operador: PIN nunca fica em texto puro no banco
    QByteArray dado = (salt + ":" + pin).toUtf8();
    for (int i = 0; i < 20000; ++i)
        dado = QCryptographicHash::hash(dado + salt.toUtf8(), QCryptographicHash::Sha256);
    return dado.toHex();
}

Operador_service::Resultado Operador_service::cadastrar(const QString &nome, const QString &pin)
{
    if (!autorizado())
        return negado();
    const QString nomeLimpo = nome.simplified();
    if (nomeLimpo.isEmpty())
        return {false, "Informe o nome do operador."};
    if (!pinValido(pin))
        return {false, "O PIN deve ter de 4 a 6 dígitos numéricos."};
    if (repo.nomeExiste(nomeLimpo))
        return {false, "Já existe um operador com esse nome."};

    OperadorDTO op;
    op.nome = nomeLimpo;
    op.pinSalt = gerarSalt();
    op.pinHash = hashPin(pin, op.pinSalt);
    op.ativo = true;

    QString erro;
    const qlonglong id = repo.inserir(op, &erro);
    if (id <= 0)
        return {false, "Não foi possível cadastrar o operador: " + erro};
    return {true, "Operador cadastrado.", id};
}

Operador_service::Resultado Operador_service::renomear(qlonglong id, const QString &nome)
{
    if (!autorizado())
        return negado();
    OperadorDTO op = repo.getPorId(id);
    if (op.id <= 0)
        return {false, "Operador não encontrado."};
    const QString nomeLimpo = nome.simplified();
    if (nomeLimpo.isEmpty())
        return {false, "Informe o nome do operador."};
    if (repo.nomeExiste(nomeLimpo, id))
        return {false, "Já existe um operador com esse nome."};
    op.nome = nomeLimpo;
    QString erro;
    if (!repo.atualizar(op, &erro))
        return {false, "Não foi possível salvar: " + erro};
    return {true, "Operador atualizado.", id};
}

Operador_service::Resultado Operador_service::redefinirPin(qlonglong id, const QString &pin)
{
    if (!autorizado())
        return negado();
    if (repo.getPorId(id).id <= 0)
        return {false, "Operador não encontrado."};
    if (!pinValido(pin))
        return {false, "O PIN deve ter de 4 a 6 dígitos numéricos."};
    const QString salt = gerarSalt();
    QString erro;
    if (!repo.atualizarPin(id, hashPin(pin, salt), salt, &erro))
        return {false, "Não foi possível redefinir o PIN: " + erro};
    return {true, "PIN redefinido. O operador foi desbloqueado.", id};
}

Operador_service::Resultado Operador_service::definirAtivo(qlonglong id, bool ativo)
{
    if (!autorizado())
        return negado();
    const OperadorDTO alvo = repo.getPorId(id);
    if (alvo.id <= 0)
        return {false, "Operador não encontrado."};
    if (!ativo) {
        // desativado não consegue informar o PIN no fechamento: o caixa ficaria preso
        const CaixaDTO aberto = caixaRepo.getAbertoDoOperador(id);
        if (aberto.aberto())
            return {false, QString("Este operador tem o caixa #%1 aberto no terminal %2. "
                                   "Feche o caixa antes de desativá-lo.").arg(aberto.id).arg(aberto.terminal)};
        // desativar o único gerente com identidade tiraria das telas restritas o caminho com login
        if (alvo.gerente && contarGerentesAtivos() <= 1)
            return {false, "Este é o único gerente ativo. Cadastre outro antes de desativá-lo."};
    }
    QString erro;
    if (!repo.atualizarAtivo(id, ativo, &erro))
        return {false, "Não foi possível salvar: " + erro};
    return {true, ativo ? "Operador ativado." : "Operador desativado.", id};
}

Operador_service::Resultado Operador_service::desbloquear(qlonglong id)
{
    if (!autorizado())
        return negado();
    if (repo.getPorId(id).id <= 0)
        return {false, "Operador não encontrado."};
    QString erro;
    if (!repo.registrarTentativa(id, 0, false, &erro))
        return {false, "Não foi possível desbloquear: " + erro};
    return {true, "Operador desbloqueado.", id};
}

Operador_service::Resultado Operador_service::marcarGerente(qlonglong id, bool gerente)
{
    if (!autorizado())
        return negado();
    OperadorDTO op = repo.getPorId(id);
    if (op.id <= 0)
        return {false, "Operador não encontrado."};
    if (gerente && op.bloqueado)
        return {false, "Desbloqueie o operador antes de marcá-lo como gerente."};
    if (gerente && !op.ativo)
        return {false, "Ative o operador antes de marcá-lo como gerente."};

    if (!gerente && op.gerente) {
        // ficar sem gerente com identidade tiraria das telas restritas o único caminho com login
        if (contarGerentesAtivos() <= 1)
            return {false, "Este é o único gerente ativo. Cadastre outro antes de remover o acesso."};
    }

    QString erro;
    if (!repo.atualizarGerente(id, gerente, &erro))
        return {false, "Não foi possível salvar: " + erro};
    return {true, gerente ? "Operador marcado como gerente." : "Acesso de gerente removido.", id};
}

int Operador_service::contarGerentesAtivos()
{
    return repo.contarGerentesAtivos();
}

Operador_service::Resultado Operador_service::validarPin(qlonglong id, const QString &pin)
{
    OperadorDTO op = repo.getPorId(id);
    if (op.id <= 0)
        return {false, "Operador não encontrado."};
    if (!op.ativo)
        return {false, "Este operador está desativado."};
    if (op.bloqueado)
        return {false, "Operador bloqueado após " + QString::number(MaxTentativas) +
                           " tentativas de PIN incorretas. Peça o desbloqueio em Caixa > Operadores."};

    if (iguaisTempoConstante(hashPin(pin, op.pinSalt), op.pinHash)) {
        if (op.tentativasFalhas > 0)
            repo.registrarTentativa(id, 0, false);
        return {true, "PIN correto.", id};
    }

    const int tentativas = op.tentativasFalhas + 1;
    const bool bloquear = tentativas >= MaxTentativas;
    repo.registrarTentativa(id, tentativas, bloquear);
    if (bloquear)
        return {false, "PIN incorreto. Operador bloqueado após " + QString::number(MaxTentativas) +
                           " tentativas. Peça o desbloqueio em Caixa > Operadores."};
    return {false, QString("PIN incorreto. Restam %1 tentativa(s).").arg(MaxTentativas - tentativas)};
}

OperadorDTO Operador_service::getPorId(qlonglong id)
{
    return repo.getPorId(id);
}

QList<OperadorDTO> Operador_service::listar(bool somenteAtivos)
{
    return repo.listar(somenteAtivos);
}

void Operador_service::listar(QSqlQueryModel *model)
{
    repo.listar(model);
}

bool Operador_service::gerentePinDefinido()
{
    return !repo.getConfigCaixa("gerente_pin_hash").isEmpty();
}

Operador_service::Resultado Operador_service::definirGerentePin(const QString &pin)
{
    // a primeira definição é livre (instalação nova); trocar um PIN existente é ato de gerente
    if (gerentePinDefinido() && !autorizado())
        return negado();
    if (!pinValido(pin))
        return {false, "O PIN do gerente deve ter de 4 a 6 dígitos numéricos."};
    const QString salt = gerarSalt();
    QString erro;
    if (!repo.setConfigCaixa("gerente_pin_salt", salt, &erro) ||
        !repo.setConfigCaixa("gerente_pin_hash", hashPin(pin, salt), &erro) ||
        !repo.setConfigCaixa(chaveTentativas(), "0", &erro) ||
        !repo.setConfigCaixa(chaveBloqueio(), "0", &erro))
        return {false, "Não foi possível salvar o PIN do gerente: " + erro};
    return {true, "PIN do gerente definido."};
}

Operador_service::Resultado Operador_service::validarGerentePin(const QString &pin)
{
    if (!gerentePinDefinido())
        return {false, "O PIN do gerente ainda não foi definido."};

    const qint64 agora = QDateTime::currentSecsSinceEpoch();
    const qint64 bloqueadoAte = repo.getConfigCaixa(chaveBloqueio()).toLongLong();
    if (agora < bloqueadoAte)
        return {false, QString("PIN do gerente bloqueado neste computador por excesso de tentativas. Tente de novo em %1 minuto(s).")
                           .arg((bloqueadoAte - agora + 59) / 60)};

    const QString salt = repo.getConfigCaixa("gerente_pin_salt");
    if (iguaisTempoConstante(hashPin(pin, salt), repo.getConfigCaixa("gerente_pin_hash"))) {
        repo.setConfigCaixa(chaveTentativas(), "0");
        return {true, "PIN correto."};
    }

    const int tentativas = repo.getConfigCaixa(chaveTentativas()).toInt() + 1;
    if (tentativas >= MaxTentativasGerente) {
        repo.setConfigCaixa(chaveTentativas(), "0");
        repo.setConfigCaixa(chaveBloqueio(), QString::number(agora + BloqueioGerenteSegundos));
        return {false, "PIN do gerente incorreto. Bloqueado por 5 minutos neste computador."};
    }
    repo.setConfigCaixa(chaveTentativas(), QString::number(tentativas));
    return {false, QString("PIN do gerente incorreto. Restam %1 tentativa(s).").arg(MaxTentativasGerente - tentativas)};
}

SessaoDTO Operador_service::validarLogin(qlonglong idOperador, const QString &pin, QString *erro)
{
    SessaoDTO sessao;
    if (erro)
        erro->clear();

    // linha fictícia do login: entra com o PIN geral do gerente, sem identidade de operador
    if (idOperador == kOperadorGerenteId) {
        const auto r = validarGerentePin(pin);
        if (!r.ok) {
            qWarning() << "login: PIN do gerente recusado:" << r.msg;
            if (erro)
                *erro = r.msg;
            return sessao;
        }
        sessao.idOperador = kOperadorGerenteId;
        sessao.nomeOperador = QString::fromLatin1(kOperadorGerenteNome);
        sessao.gerente = true;
        sessao.pinGeral = true;
        return sessao;
    }

    const auto r = validarPin(idOperador, pin);
    if (!r.ok) {
        qWarning() << "login: PIN do operador recusado:" << r.msg;
        if (erro)
            *erro = r.msg;
        return sessao;
    }

    const OperadorDTO op = repo.getPorId(idOperador);
    sessao.idOperador = op.id;
    sessao.nomeOperador = op.nome;
    sessao.gerente = op.gerente;
    return sessao;
}

bool Operador_service::pinConfere(qlonglong id, const QString &pin)
{
    const OperadorDTO op = repo.getPorId(id);
    if (op.id <= 0 || !op.ativo || op.bloqueado)
        return false;
    return iguaisTempoConstante(hashPin(pin, op.pinSalt), op.pinHash);
}

bool Operador_service::gerentePinConfere(const QString &pin)
{
    if (!gerentePinDefinido())
        return false;
    if (QDateTime::currentSecsSinceEpoch() < repo.getConfigCaixa(chaveBloqueio()).toLongLong())
        return false;
    return iguaisTempoConstante(hashPin(pin, repo.getConfigCaixa("gerente_pin_salt")),
                                repo.getConfigCaixa("gerente_pin_hash"));
}

QString Operador_service::marcaPinGeral()
{
    return repo.getConfigCaixa("gerente_pin_salt");
}
