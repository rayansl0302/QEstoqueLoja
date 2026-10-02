#ifndef TEST_CONTASRECEBER_SERVICE_H
#define TEST_CONTASRECEBER_SERVICE_H

#include <QObject>
#include "dto/ContasReceber_dto.h"

// Contas a receber (fiado / caderneta).
class TestContasReceber : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void cleanupTestCase();

    void nome_do_cliente_e_unico_sem_diferenciar_espacos_e_maiusculas();
    void lancar_divida_exige_sessao();
    void lancar_divida_valida_cliente_data_e_valor();
    void pagamento_parcial_abate_o_saldo_e_quita_sozinho();
    void pagamento_maior_que_o_saldo_e_recusado();
    void pagamento_exige_caixa_aberto_e_entra_no_caixa();
    void receber_do_cliente_abate_da_mais_antiga_e_junta_o_fiado_do_pdv();
    void limite_de_credito_bloqueia_e_o_gerente_libera();
    void cancelar_lancamento_so_sem_pagamento_e_estorno_so_gerente();
    void estorno_com_caixa_fechado_e_recusado();
    void cliente_com_divida_nao_e_excluido_e_inativo_nao_compra_a_prazo();
    void extrato_junta_as_duas_fontes_com_saldo_acumulado();
    void operacoes_de_gerente_respeitam_o_autorizador();

private:
    qlonglong novoCliente(const QString &nome = QString());
    DividaDTO divida(qlonglong cliente, double valor, const QString &descricao = QStringLiteral("Compras da semana"),
                     int diasAtras = 0);
    qlonglong idOperador = 0;
    qlonglong idCaixa = 0;
    QList<qlonglong> caixasAbertos;
};

#endif // TEST_CONTASRECEBER_SERVICE_H
