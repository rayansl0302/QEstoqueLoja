#include "operador_service.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QDateTime>

namespace {
// comparação sem atalho: o tempo não revela em que posição o hash difere
bool iguaisTempoConstante(const QString &a, const QString &b)
{
    if (a.size() != b.size())
        return false;
    ushort diferenca = 0;
    for (int i = 0; i < a.size(); ++i)
        diferenca |= a.at(i).unicode() ^ b.at(i).unicode();
    return diferenca == 0;
}

constexpr int MaxTentativasGerente = 5;
constexpr int BloqueioGerenteSegundos = 300;
}

Operador_service::Operador_service(QObject *parent)
    : QObject{parent}
{}

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
    if (repo.getPorId(id).id <= 0)
        return {false, "Operador não encontrado."};
    if (!ativo) {
        // desativado não consegue informar o PIN no fechamento: o caixa ficaria preso
        const CaixaDTO aberto = caixaRepo.getAbertoDoOperador(id);
        if (aberto.aberto())
            return {false, QString("Este operador tem o caixa #%1 aberto no terminal %2. "
                                   "Feche o caixa antes de desativá-lo.").arg(aberto.id).arg(aberto.terminal)};
    }
    QString erro;
    if (!repo.atualizarAtivo(id, ativo, &erro))
        return {false, "Não foi possível salvar: " + erro};
    return {true, ativo ? "Operador ativado." : "Operador desativado.", id};
}

Operador_service::Resultado Operador_service::desbloquear(qlonglong id)
{
    if (repo.getPorId(id).id <= 0)
        return {false, "Operador não encontrado."};
    QString erro;
    if (!repo.registrarTentativa(id, 0, false, &erro))
        return {false, "Não foi possível desbloquear: " + erro};
    return {true, "Operador desbloqueado.", id};
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
    if (!pinValido(pin))
        return {false, "O PIN do gerente deve ter de 4 a 6 dígitos numéricos."};
    const QString salt = gerarSalt();
    QString erro;
    if (!repo.setConfigCaixa("gerente_pin_salt", salt, &erro) ||
        !repo.setConfigCaixa("gerente_pin_hash", hashPin(pin, salt), &erro) ||
        !repo.setConfigCaixa("gerente_tentativas", "0", &erro) ||
        !repo.setConfigCaixa("gerente_bloqueio_ate", "0", &erro))
        return {false, "Não foi possível salvar o PIN do gerente: " + erro};
    return {true, "PIN do gerente definido."};
}

Operador_service::Resultado Operador_service::validarGerentePin(const QString &pin)
{
    if (!gerentePinDefinido())
        return {false, "O PIN do gerente ainda não foi definido."};

    const qint64 agora = QDateTime::currentSecsSinceEpoch();
    const qint64 bloqueadoAte = repo.getConfigCaixa("gerente_bloqueio_ate").toLongLong();
    if (agora < bloqueadoAte)
        return {false, QString("PIN do gerente bloqueado por excesso de tentativas. Tente de novo em %1 minuto(s).")
                           .arg((bloqueadoAte - agora + 59) / 60)};

    const QString salt = repo.getConfigCaixa("gerente_pin_salt");
    if (iguaisTempoConstante(hashPin(pin, salt), repo.getConfigCaixa("gerente_pin_hash"))) {
        repo.setConfigCaixa("gerente_tentativas", "0");
        return {true, "PIN correto."};
    }

    const int tentativas = repo.getConfigCaixa("gerente_tentativas").toInt() + 1;
    if (tentativas >= MaxTentativasGerente) {
        repo.setConfigCaixa("gerente_tentativas", "0");
        repo.setConfigCaixa("gerente_bloqueio_ate", QString::number(agora + BloqueioGerenteSegundos));
        return {false, "PIN do gerente incorreto. Bloqueado por 5 minutos."};
    }
    repo.setConfigCaixa("gerente_tentativas", QString::number(tentativas));
    return {false, QString("PIN do gerente incorreto. Restam %1 tentativa(s).").arg(MaxTentativasGerente - tentativas)};
}
