#ifndef TEST_EMPRESA_CONTAS_H
#define TEST_EMPRESA_CONTAS_H

#include <QObject>

// Multi-empresa (CNPJs) e contas a pagar.
class TestEmpresaContas : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void cnpj_valida_digitos_verificadores();
    void cadastrar_empresa_valida_nome_cnpj_e_duplicidade();
    void editar_empresa_corrige_nome_cnpj_e_valida_duplicidade();
    void troca_de_empresa_recusa_com_venda_em_andamento();
    void configuracao_fiscal_e_separada_por_empresa();
    void venda_grava_a_empresa_ativa();
    void relatorios_e_lista_de_vendas_filtram_pela_empresa();

    void produto_vendido_nao_e_apagado_e_lista_as_vendas();
    void dividir_valor_soma_exata();
    void lancar_parcelado_gera_vencimentos_mensais_sem_deriva();
    void lancar_valida_campos();
    void baixar_estornar_e_cancelar_seguem_as_regras();
    void listagem_e_resumo_por_empresa_e_vencimento();
};

#endif // TEST_EMPRESA_CONTAS_H
