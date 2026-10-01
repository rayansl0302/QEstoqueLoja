#ifndef EMPRESA_DTO_H
#define EMPRESA_DTO_H

#include <QString>

// Empresa (CNPJ) cadastrada. Só a identidade fica no banco, compartilhada por todos os computadores;
// os dados completos (endereço, certificado, CSC, numeração de NF) ficam na configuração de cada
// empresa (Configurações), que é local do computador.
struct EmpresaDTO {
    qlonglong id = 0;
    QString apelido;       // nome curto que aparece nos botões e no rodapé
    QString cnpj;          // só dígitos
    QString razaoSocial;
    bool ativa = true;

    bool valida() const { return id > 0; }
    QString cnpjFormatado() const;
};

inline QString EmpresaDTO::cnpjFormatado() const
{
    if (cnpj.size() != 14)
        return cnpj;
    return QStringLiteral("%1.%2.%3/%4-%5").arg(cnpj.left(2), cnpj.mid(2, 3), cnpj.mid(5, 3),
                                                 cnpj.mid(8, 4), cnpj.mid(12, 2));
}

#endif // EMPRESA_DTO_H
