# Contas a receber (fiado / caderneta)

Controla o que cada cliente deve, o que comprou a prazo, quando comprou e o que já pagou. Une as duas origens de dívida
que existem no sistema numa só visão.

## De onde vem a dívida

| Origem | Onde fica | Como nasce |
| --- | --- | --- |
| **Fiado do PDV** | `vendas2` com forma de pagamento `Prazo` + pagamentos em `entradas_vendas` | venda finalizada como "Prazo" (baixa estoque e emite nota como qualquer venda) |
| **Caderneta** (lançamento manual) | `dividas` + `pagamentos_divida` (migração 21) | **Financeiro → Contas a receber → Lançar dívida**: texto livre e valor, sem estoque e sem nota |

Saldo de uma venda a prazo: `valor_final − Σ entradas_vendas.total` (a mesma conta da tela de pagamentos à prazo).
Saldo de uma dívida manual: `valor_total − Σ pagamentos não estornados`.
**Total devido do cliente** = soma dos saldos das duas origens (só saldos positivos).

## Telas

- **Financeiro → Contas a receber** (menu, botão na tela inicial, botão "Contas a receber" na tela Clientes):
  cartões (total a receber, clientes devendo, maior dívida), filtros (empresa, com dívida / todos / inativos, busca por
  nome ou telefone), lista com total devido, última compra, limite e situação (*Deve*, *Acima do limite*, *Em dia*, *Inativo*).
- **Extrato do cliente** (duplo clique): compras e pagamentos juntos, em ordem de data, com o **saldo depois de cada
  linha**, operador e empresa; dívida cancelada e pagamento estornado ficam visíveis, riscados.
- **Lançar dívida**: cliente, data (não aceita futuro), descrição, valor e observação; mostra na hora quanto o cliente já
  deve, o limite e se a compra passa do limite.
- **Receber pagamento**: valor (total ou parcial), forma (Dinheiro, Crédito, Débito, Pix), data (não aceita futuro) e
  observação. Cobrar no WhatsApp abre o aplicativo com a mensagem pronta; quem envia é a pessoa.
- **Dados de crédito**: limite (vazio = sem limite), WhatsApp, observação e inativar/reativar.

## Regras

- **Tudo exige operador logado**: o `id_operador_sessao` vem da sessão. Sem sessão o serviço recusa.
- **Pagamento nunca passa do saldo.** Pagamento parcial abate e a dívida continua aberta; ao zerar vira `QUITADA`.
- **Receber do cliente** abate da dívida **mais antiga para a mais nova**, nas duas origens (primeiro as da empresa em uso).
  Receber de uma dívida específica também é possível pelo extrato.
- **Caixa**: todo recebimento entra no **caixa aberto de quem está logado** como `RECEBIMENTO` (Pix e cartão também, porque o
  fechamento confere por forma). Sem caixa aberto, o pagamento é recusado e nada fica gravado. Tudo acontece numa transação.
- **Limite de crédito** (do cliente, somando todas as empresas): vale no lançamento manual **e** na venda a prazo do PDV. Passar do
  limite exige a **liberação do gerente** (PIN), que fica no log de acesso (`LIMITE_LIBERADO`).
- **Venda a prazo** só para **cliente identificado e ativo** (o Consumidor não tem fiado).
- **Cancelar lançamento**: só se não houver pagamento válido; com pagamento, é preciso estornar antes (fica registrado).
- **Estornar pagamento**: só enquanto o caixa do recebimento ainda estiver aberto (mesma regra da exclusão de recebimento de
  venda a prazo). O estorno mantém o pagamento no histórico, marcado como estornado, com motivo e quem fez.
- **Cliente** com dívida aberta não é excluído; com histórico de compras também não (só inativa). Cliente inativo some das
  vendas e não compra a prazo, mas a dívida continua valendo e aparece em Contas a receber.
- **Nome do cliente** é único, sem diferenciar maiúsculas nem espaços a mais (validado ao cadastrar e ao editar).
- Multi-empresa: cada dívida guarda a empresa em que foi feita; o filtro da tela escolhe qual olhar.

## Quem pode o quê

| | Operador | Gerente |
| --- | --- | --- |
| Lançar dívida, receber pagamento, consultar extrato, editar WhatsApp/observação | ✔ | ✔ |
| Cancelar lançamento, estornar pagamento | — | ✔ (ou PIN do gerente) |
| Definir/alterar limite, inativar/reativar cliente, liberar compra acima do limite | — | ✔ (ou PIN do gerente) |

O serviço confere a regra (não só a tela): `ContasReceber_service::definirAutorizador` é registrado no `main.cpp` com o mesmo
critério do cadastro de operadores. Ações sensíveis vão para o **log de acesso**: `PAGAMENTO_ESTORNADO`, `DIVIDA_CANCELADA`,
`LIMITE_ALTERADO`, `LIMITE_LIBERADO`, `CLIENTE_INATIVADO`, `CLIENTE_REATIVADO`.

## Banco (migração 21)
- `clientes`: `ativo`, `limite_credito`, `observacao`, `whatsapp`.
- `dividas`, `pagamentos_divida` (com `cancelado`, motivo, quem e quando), `movimentacoes_caixa.id_pagamento_divida`.
- Como as anteriores, não tem volta: atualize **todos os terminais juntos**.

## Arquivos
`src/dto/ContasReceber_dto.h`, `src/repository/contasreceber_repository.*`, `src/services/contasreceber_service.*`,
`src/contasreceberjanela.*`, `src/extratoclientedialog.*`, `src/contareceberdialogs.*`,
`tests/services/test_contasreceber_service.*`.
