# Multi-empresa (CNPJs) e contas a pagar

## Multi-empresa

Vários CNPJs no mesmo sistema (sócios dividindo o espaço). Há **uma empresa ativa por computador**; toda
venda, conta a pagar e emissão fiscal feita enquanto ela estiver ativa pertence a ela.

### Escolha da empresa
- Botão **Trocar empresa** na tela inicial, menu **Financeiro → Trocar empresa** e o chip da empresa no rodapé.
- Se houver mais de uma empresa ativa, o programa pergunta **com qual vai trabalhar** logo depois do login (uma vez
  por abertura). A última escolha fica guardada em `geral/empresa_ativa` do `config.ini`.
- A troca é recusada com **venda em andamento**. Ao trocar, recarrega configuração, certificado (ACBr), logo e alertas.
- Cadastrar e desativar empresa: só gerente. O cadastro pede nome e CNPJ (com dígitos verificadores); o resto
  (endereço, certificado, CSC, numeração de NF) é preenchido em **Configurações** com a empresa ativa
  (a janela mostra o nome da empresa no título). Empresa nova começa **sem emissão de nota ligada**.

### O que é por empresa
| Dado | Onde fica |
| --- | --- |
| Identidade (nome, CNPJ, ativa) | tabela `empresas` (banco compartilhado, migração 19) |
| Dados da empresa, fiscal, certificado, CSC, numeração de NF, logo | `config.ini` do computador: empresa 1 usa as chaves de sempre (`empresa/…`, `fiscal/…`); as demais `empresa_<id>/…` |
| Venda | `vendas2.id_empresa` (padrão 1 nas vendas antigas) |
| Conta a pagar | `contas_pagar.id_empresa` |

Continuam **compartilhados**: produtos, estoque, clientes, operadores, caixa, e-mail, contador, banco.

### Regras
- A lista de vendas, o resumo e os **relatórios** (quantidade, valores, formas de pagamento, produtos mais
  vendidos/lucrativos, lucro, notas por período, inadimplentes) mostram só a empresa em uso.
- **Cancelar a NF de uma venda de outra empresa** é recusado: é preciso trocar para a empresa dela (o evento é
  assinado com o certificado da empresa ativa).
- A **contingência** só reenvia notas do CNPJ da empresa ativa; as de outra empresa esperam.
- O **caixa é do operador** e não separa por empresa: o fechamento soma as vendas de todas as empresas feitas naquele caixa.
- Cada computador precisa ter o **certificado de cada empresa** configurado (os caminhos ficam no `config.ini` local).

## Contas a pagar

Tela: **Financeiro → Contas a pagar** (ou o botão na tela inicial). A tela inicial mostra uma faixa quando há
contas **vencidas** ou **vencendo hoje** da empresa em uso.

- **Lançar** com fornecedor, categoria, documento, valor, vencimento e observação. Com **parcelas** (1 a 120;
  mensal, quinzenal ou semanal) o valor é dividido com os centavos que sobram na última parcela, e os vencimentos
  partem sempre do primeiro (31/01 → 28/02 → 31/03, sem deriva).
- **Baixar (pagar)** com valor realmente pago (juros/desconto), forma e data. Não baixa duas vezes.
- **Editar** só conta em aberto. **Estornar pagamento** e **cancelar conta** (com motivo): só gerente, também no
  serviço, e ficam no **log de acesso** (`CONTA_PAGAR_BAIXA`, `CONTA_PAGAR_ESTORNO`, `CONTA_PAGAR_CANCELAMENTO`).
- Resumo em cartões (vencidas, hoje, próximos 7 dias, total em aberto) que também funcionam como filtro; filtros por
  empresa, situação, busca e período de vencimento.
- Baixa em dinheiro **não** gera saída no caixa (decisão: contas a pagar é controle financeiro à parte).

## Banco
- Migração 19: `empresas` (+ empresa 1 "Empresa principal"), `vendas2.id_empresa`.
- Migração 20: `contas_pagar` (índices por status/vencimento, empresa e grupo de parcelas).
- Como as anteriores, não têm volta: **atualize todos os terminais juntos**.

## Arquivos
`src/services/empresa_service.*`, `src/repository/empresa_repository.*`, `src/infra/empresaativa.h`,
`src/empresadialog.*`, `src/services/contaspagar_service.*`, `src/repository/contaspagar_repository.*`,
`src/contaspagarjanela.*`, `src/contapagardialogs.*`, `tests/services/test_empresa_contas.*`.
