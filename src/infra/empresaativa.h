#ifndef EMPRESAATIVA_H
#define EMPRESAATIVA_H

#include <QtGlobal>
#include <atomic>

// Empresa (CNPJ) em uso neste computador. Tudo que é "por empresa" (dados da empresa, certificado,
// numeração de NF, vendas, contas a pagar) lê daqui. A troca é feita só por Empresa_service.
// Id 1 é a empresa original (a que já existia antes do multi-empresa).
namespace EmpresaAtiva {
inline std::atomic<qlonglong> &ref()
{
    static std::atomic<qlonglong> id{1};
    return id;
}
inline qlonglong id() { return ref().load(); }
inline void definir(qlonglong id) { ref().store(id > 0 ? id : 1); }
}

#endif // EMPRESAATIVA_H
