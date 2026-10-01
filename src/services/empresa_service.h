#ifndef EMPRESA_SERVICE_H
#define EMPRESA_SERVICE_H

#include <QObject>
#include <QList>
#include "../dto/Config_dto.h"
#include "../dto/Empresa_dto.h"
#include "../repository/empresa_repository.h"

// Multi-empresa: vários CNPJs no mesmo sistema (sócios dividindo o mesmo espaço). Há UMA empresa
// ativa por computador; toda venda, conta a pagar e emissão fiscal feita enquanto ela estiver
// ativa pertence a ela. O operador troca pelo botão "Empresa" do menu.
class Empresa_service : public QObject
{
    Q_OBJECT
public:
    struct Resultado {
        bool ok = false;
        QString msg;
        qlonglong id = 0;
    };

    static Empresa_service *instancia();
    explicit Empresa_service(QObject *parent = nullptr);

    // Lê do banco/config a empresa que estava ativa no último uso (1 se não houver). Chamar depois
    // de conectar ao banco, antes de carregar a configuração.
    void carregarEstadoInicial();

    qlonglong idAtiva() const;
    EmpresaDTO ativa();
    QList<EmpresaDTO> listar(bool somenteAtivas = true);
    EmpresaDTO getPorId(qlonglong id);

    // Cadastra uma empresa nova. A configuração fiscal dela é preenchida depois, em Configurações,
    // com a empresa ativa. Começa SEM emissão de nota ligada.
    Resultado cadastrar(const QString &apelido, const QString &cnpj, const QString &razaoSocial = QString());
    Resultado renomear(qlonglong id, const QString &apelido);
    Resultado definirAtivaNoCadastro(qlonglong id, bool ativa);

    // Troca a empresa em uso. Recusa se houver venda em andamento ou empresa inválida/inativa.
    Resultado trocarPara(qlonglong id);

    // Guarda no cadastro o nome/CNPJ que a configuração da empresa ativa acabou de salvar.
    void sincronizarComConfig(const ConfigDTO &cfg);

    static QString somenteDigitos(const QString &texto);
    static bool cnpjValido(const QString &cnpj);

signals:
    void empresaMudou(qlonglong idEmpresa);

private:
    Empresa_repository repo;
    void persistirAtiva(qlonglong id);
};

#endif // EMPRESA_SERVICE_H
