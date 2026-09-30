#ifndef SESSAO_DTO_H
#define SESSAO_DTO_H

#include <QtGlobal>
#include <QString>

// Linha fictícia do login: o id 0 é reservado para quem entra com o PIN geral do gerente.
// Os ids de operador do banco começam em 1 (SERIAL/AUTOINCREMENT), então 0 nunca colide.
constexpr qlonglong kOperadorGerenteId = 0;
constexpr const char *kOperadorGerenteNome = "Gerente";

// Sessão ativa do operador. Fica só em memória: não sobrevive a fechar o programa.
struct SessaoDTO {
    qlonglong idOperador = 0;
    QString nomeOperador;
    bool gerente = false;
    QString entradaEm;        // ISO
    QString terminal;

    // login feito com o PIN geral do gerente (config_caixa) em vez do PIN de um operador do cadastro
    bool pinGeral = false;
    // "impressão digital" do PIN geral no momento do login: se o PIN for trocado, a sessão
    // aberta com o PIN antigo deixa de valer (revalidação periódica)
    QString marcaPinGeral;

    bool valida() const { return idOperador >= kOperadorGerenteId && !nomeOperador.isEmpty(); }
    QString papel() const { return gerente ? QStringLiteral("Gerente") : QStringLiteral("Operador"); }
};

// Uma entrada do log de sessões (auditoria de entrada/saida).
// saidaEm vazia = sessão que o programa encerrou sem logout (ou seja, saiu no meio).
struct LogSessaoDTO {
    qlonglong id = 0;
    qlonglong idOperador = 0;
    QString nomeOperador;
    bool gerente = false;
    QString terminal;
    QString entradaEm;
    QString saidaEm;
    QString motivoSaida;      // LOGOUT | TROCA_OPERADOR | TIMEOUT | INVALIDADA | SEM_LOGOUT | ENCERRAMENTO
    qlonglong idCaixaNoMomento = 0;
};

// Ação sensível registrada na auditoria (tabela auditoria_acesso): uso do PIN do gerente,
// bloqueio/desbloqueio de sessão, sessão invalidada, mudança de permissão.
struct LogAcaoDTO {
    qlonglong id = 0;
    QString dataHora;
    qlonglong idOperadorSessao = -1;   // -1 = sem sessão; 0 = gerente do PIN geral
    QString nomeOperador;
    QString terminal;
    QString acao;
    QString detalhe;
};

#endif // SESSAO_DTO_H
