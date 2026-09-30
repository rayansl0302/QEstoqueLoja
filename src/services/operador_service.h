#ifndef OPERADOR_SERVICE_H
#define OPERADOR_SERVICE_H

#include <QObject>
#include <QSqlQueryModel>
#include <functional>
#include "../repository/operador_repository.h"
#include "../repository/caixa_repository.h"
#include "../dto/Sessao_dto.h"

class Operador_service : public QObject
{
    Q_OBJECT
public:
    struct Resultado {
        bool ok;
        QString msg;
        qlonglong id = 0;
    };
    static constexpr int MaxTentativas = 3;

    explicit Operador_service(QObject *parent = nullptr);

    Resultado cadastrar(const QString &nome, const QString &pin);
    Resultado renomear(qlonglong id, const QString &nome);
    Resultado redefinirPin(qlonglong id, const QString &pin);
    Resultado definirAtivo(qlonglong id, bool ativo);
    Resultado desbloquear(qlonglong id);
    // gerente com identidade no cadastro: libera as telas restritas da matriz de acesso
    Resultado marcarGerente(qlonglong id, bool gerente);
    int contarGerentesAtivos();

    // Confere o PIN; a cada erro soma uma tentativa e, na terceira, bloqueia o operador.
    Resultado validarPin(qlonglong id, const QString &pin);

    // Confere o PIN SEM contar tentativa nem bloquear (para telas que aceitam mais de um PIN,
    // como o desbloqueio de sessão). Quem recusa de verdade deve chamar validarPin depois.
    bool pinConfere(qlonglong id, const QString &pin);
    bool gerentePinConfere(const QString &pin);
    // "impressão digital" do PIN geral atual; muda quando o PIN é trocado
    QString marcaPinGeral();

    // Quem pode administrar (cadastrar, desbloquear, redefinir PIN, marcar gerente...). O programa
    // registra aqui uma função que consulta a sessão; sem ela (testes, carga inicial) tudo é permitido.
    // Assim a regra vale também se uma tela nova esquecer de conferir.
    static void definirAutorizador(std::function<bool()> autorizador);

    // PIN do gerente (compartilhado entre os terminais): protege o cadastro de operadores,
    // para que um operador não consiga redefinir/desbloquear o PIN de outro.
    bool gerentePinDefinido();
    Resultado definirGerentePin(const QString &pin);
    Resultado validarGerentePin(const QString &pin);

    OperadorDTO getPorId(qlonglong id);
    QList<OperadorDTO> listar(bool somenteAtivos);
    void listar(QSqlQueryModel *model);

    static bool pinValido(const QString &pin);

    // Login da tela de entrada. idOperador == kOperadorGerenteId entra com o PIN geral.
    // Devolve uma SessaoDTO sem nome quando o PIN é recusado; erro recebe a mensagem
    // ("tentativas restantes", "bloqueado por 5 minutos", etc.) para ser mostrada no diálogo.
    SessaoDTO validarLogin(qlonglong idOperador, const QString &pin, QString *erro = nullptr);

private:
    Operador_repository repo;
    Caixa_repository caixaRepo;
    static std::function<bool()> autorizador;
    static bool autorizado();
    static Resultado negado();
    static QString gerarSalt();
    static QString hashPin(const QString &pin, const QString &salt);
};

#endif // OPERADOR_SERVICE_H
