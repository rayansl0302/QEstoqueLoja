#include "empresa_service.h"
#include "sessao_service.h"
#include "../infra/apppath_service.h"
#include "../infra/empresaativa.h"
#include <QSettings>

namespace {
constexpr auto kChaveAtiva = "geral/empresa_ativa";
}

Empresa_service *Empresa_service::instancia()
{
    static Empresa_service *unica = new Empresa_service(nullptr);
    return unica;
}

Empresa_service::Empresa_service(QObject *parent)
    : QObject(parent)
{
}

QString Empresa_service::somenteDigitos(const QString &texto)
{
    QString r;
    for (const QChar c : texto)
        if (c.isDigit())
            r.append(c);
    return r;
}

bool Empresa_service::cnpjValido(const QString &entrada)
{
    const QString c = somenteDigitos(entrada);
    if (c.size() != 14)
        return false;
    bool todosIguais = true;
    for (int i = 1; i < 14; ++i)
        if (c.at(i) != c.at(0)) { todosIguais = false; break; }
    if (todosIguais)
        return false;
    const int pesos1[12] = {5, 4, 3, 2, 9, 8, 7, 6, 5, 4, 3, 2};
    const int pesos2[13] = {6, 5, 4, 3, 2, 9, 8, 7, 6, 5, 4, 3, 2};
    auto digito = [&](int n, const int *pesos) {
        int soma = 0;
        for (int i = 0; i < n; ++i)
            soma += c.at(i).digitValue() * pesos[i];
        const int resto = soma % 11;
        return resto < 2 ? 0 : 11 - resto;
    };
    return digito(12, pesos1) == c.at(12).digitValue() && digito(13, pesos2) == c.at(13).digitValue();
}

void Empresa_service::persistirAtiva(qlonglong id)
{
    QSettings s(AppPath_service::configPath(), QSettings::IniFormat);
    s.setValue(kChaveAtiva, id);
    s.sync();
}

void Empresa_service::carregarEstadoInicial()
{
    QSettings s(AppPath_service::configPath(), QSettings::IniFormat);
    const qlonglong guardada = s.value(kChaveAtiva, 1).toLongLong();

    const EmpresaDTO e = repo.getPorId(guardada);
    if (e.valida() && e.ativa) {
        EmpresaAtiva::definir(e.id);
        return;
    }
    // a guardada sumiu ou foi desativada: usa a primeira ativa
    const QList<EmpresaDTO> ativas = repo.listar(true);
    EmpresaAtiva::definir(ativas.isEmpty() ? 1 : ativas.first().id);
}

qlonglong Empresa_service::idAtiva() const
{
    return EmpresaAtiva::id();
}

EmpresaDTO Empresa_service::ativa()
{
    return repo.getPorId(EmpresaAtiva::id());
}

QList<EmpresaDTO> Empresa_service::listar(bool somenteAtivas)
{
    return repo.listar(somenteAtivas);
}

EmpresaDTO Empresa_service::getPorId(qlonglong id)
{
    return repo.getPorId(id);
}

Empresa_service::Resultado Empresa_service::cadastrar(const QString &apelido, const QString &cnpj,
                                                      const QString &razaoSocial)
{
    const QString nome = apelido.trimmed();
    const QString digitos = somenteDigitos(cnpj);
    if (nome.size() < 2)
        return {false, "Informe o nome da empresa (mínimo 2 letras)."};
    if (!cnpjValido(digitos))
        return {false, "CNPJ inválido. Confira os 14 dígitos."};
    if (repo.getPorCnpj(digitos).valida())
        return {false, "Já existe uma empresa cadastrada com este CNPJ."};
    for (const EmpresaDTO &e : repo.listar(false))
        if (e.apelido.compare(nome, Qt::CaseInsensitive) == 0)
            return {false, "Já existe uma empresa com este nome."};

    EmpresaDTO nova;
    nova.apelido = nome;
    nova.cnpj = digitos;
    nova.razaoSocial = razaoSocial.trimmed();
    QString erro;
    const qlonglong id = repo.inserir(nova, &erro);
    if (id <= 0)
        return {false, "Não foi possível cadastrar: " + erro};
    return {true, "Empresa cadastrada.", id};
}

Empresa_service::Resultado Empresa_service::renomear(qlonglong id, const QString &apelido)
{
    EmpresaDTO e = repo.getPorId(id);
    if (!e.valida())
        return {false, "Empresa não encontrada."};
    const QString nome = apelido.trimmed();
    if (nome.size() < 2)
        return {false, "Informe o nome da empresa (mínimo 2 letras)."};
    e.apelido = nome;
    QString erro;
    if (!repo.atualizar(e, &erro))
        return {false, "Não foi possível salvar: " + erro};
    return {true, "Nome atualizado.", id};
}

Empresa_service::Resultado Empresa_service::definirAtivaNoCadastro(qlonglong id, bool ativa)
{
    if (!ativa) {
        if (id == EmpresaAtiva::id())
            return {false, "Troque para outra empresa antes de desativar esta."};
        if (repo.listar(true).size() <= 1)
            return {false, "É preciso manter ao menos uma empresa ativa."};
    }
    QString erro;
    if (!repo.definirAtiva(id, ativa, &erro))
        return {false, "Não foi possível salvar: " + erro};
    return {true, ativa ? "Empresa reativada." : "Empresa desativada.", id};
}

Empresa_service::Resultado Empresa_service::trocarPara(qlonglong id)
{
    if (id == EmpresaAtiva::id())
        return {true, "Esta já é a empresa em uso.", id};
    const EmpresaDTO e = repo.getPorId(id);
    if (!e.valida() || !e.ativa)
        return {false, "Empresa inválida ou desativada."};
    if (Sessao_service::instancia()->temVendaEmAndamento())
        return {false, "Há uma venda em andamento. Finalize ou cancele a venda antes de trocar a empresa."};

    EmpresaAtiva::definir(id);
    persistirAtiva(id);
    emit empresaMudou(id);
    return {true, QString("Agora as vendas serão da empresa %1.").arg(e.apelido), id};
}

void Empresa_service::sincronizarComConfig(const ConfigDTO &cfg)
{
    EmpresaDTO e = repo.getPorId(EmpresaAtiva::id());
    if (!e.valida())
        return;
    const QString cnpj = somenteDigitos(cfg.cnpjEmpresa);
    const QString nomeConfig = !cfg.nomeFantasiaEmpresa.trimmed().isEmpty() ? cfg.nomeFantasiaEmpresa.trimmed()
                                                                            : cfg.nomeEmpresa.trimmed();
    bool mudou = false;
    if (!cnpj.isEmpty() && e.cnpj != cnpj) {
        // não deixa dois cadastros com o mesmo CNPJ
        const EmpresaDTO outra = repo.getPorCnpj(cnpj);
        if (!outra.valida() || outra.id == e.id) {
            e.cnpj = cnpj;
            mudou = true;
        }
    }
    if (!cfg.nomeEmpresa.trimmed().isEmpty() && e.razaoSocial != cfg.nomeEmpresa.trimmed()) {
        e.razaoSocial = cfg.nomeEmpresa.trimmed();
        mudou = true;
    }
    // o apelido só é preenchido pela configuração enquanto for o nome provisório
    if (e.apelido == QStringLiteral("Empresa principal") && !nomeConfig.isEmpty()) {
        e.apelido = nomeConfig;
        mudou = true;
    }
    if (mudou)
        repo.atualizar(e);
}
