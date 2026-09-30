# Módulo de operadores e caixa

Controle de turno por operador: quem abre o caixa, o que entra e sai da gaveta e o fechamento com contagem.

O terminal é o nome da máquina (`QSysInfo::machineHostName()`, em maiúsculas). Só pode haver **um caixa aberto por terminal** e **um caixa aberto por operador**.

---

## Telas (menu Caixa)

O menu fica na barra principal, entre Exportar e Ajuda. O rodapé mostra `Caixa fechado` (vermelho) ou `Caixa aberto · Nome · #id` (verde).

| Item | Janela | Função |
| --- | --- | --- |
| Abrir caixa… | `AberturaCaixa` | Operador, PIN, troco inicial (sugerido e editável) |
| Fechar caixa… | `FechamentoCaixa` | Contagem das 4 formas, observação e PIN |
| Sangria… | `MovimentacaoCaixa` | Retirar dinheiro da gaveta (motivo obrigatório) |
| Suprimento… | `MovimentacaoCaixa` | Colocar dinheiro na gaveta (motivo obrigatório) |
| Histórico… | `HistoricoCaixas` | Lista por período e relatório do turno |
| Operadores… | `Operadores` | Cadastro, PIN, bloqueio e ativação (pede o **PIN do gerente**) |
| PIN do gerente… | (diálogos) | Define ou troca o PIN do gerente |

O relatório do histórico abre em `RelatorioCaixaJanela` (HTML na tela, PDF ou térmica).

---

## Operador

Cadastro em **Caixa → Operadores**.

| Campo | Regra |
| --- | --- |
| Nome | Obrigatório, único (sem diferenciar só por espaços) |
| PIN | 4 a 6 dígitos numéricos |
| Ativo | Desativado não abre nem fecha caixa |
| Bloqueado | Após 3 PINs errados; só desbloqueia nesta tela (ou ao redefinir o PIN) |

O PIN **não** é gravado em texto. Vai para o banco como SHA-256 iterado 20.000 vezes com um salt aleatório por operador.

O PIN é pedido na **abertura** e no **fechamento** (sempre o PIN de quem abriu o turno). A conferência do hash é feita em tempo constante.

**Desativar** um operador que tem caixa aberto é recusado: ele não conseguiria informar o PIN no fechamento e o caixa ficaria preso.

### PIN do gerente

A tela **Operadores** (e o botão *Operadores...* da abertura) só abre com o **PIN do gerente**. Sem isso, qualquer operador poderia redefinir ou desbloquear o PIN de outro.

- Na primeira vez o sistema pede para **definir** o PIN do gerente (4 a 6 dígitos, digitado duas vezes).
- Trocar: **Caixa → PIN do gerente…** (pede o atual).
- Fica no banco (`config_caixa`), então vale para **todos os terminais**. Mesmo formato de hash do PIN dos operadores.
- 5 erros seguidos bloqueiam por 5 minutos (inclusive para o PIN certo).
- Se o PIN do gerente for esquecido não há recuperação pela interface.

---

## Turno de caixa

### Abertura

1. Escolher operador ativo e informar o PIN.
2. Informar o troco inicial. O sistema sugere o **dinheiro contado no último fechamento deste terminal**; os dois valores (sugerido e o que foi digitado) ficam gravados.
3. Se já houver caixa aberto no terminal, ou se aquele operador já tiver caixa aberto em outro terminal, a abertura é recusada. O banco também garante isso com índices únicos parciais (migração 15), para o caso de dois computadores abrirem ao mesmo tempo.

### Durante o turno

- **Venda (PDV, nova venda na lista de Vendas e demais fluxos)** só segue com caixa aberto neste terminal; se não houver, o sistema oferece abrir na hora. A venda grava `id_caixa`. Vendas antigas (antes do módulo) ficam com `id_caixa` nulo.
- **Recebimento de venda a prazo** entra no caixa **do momento do recebimento**, como movimentação `RECEBIMENTO` (não no caixa da venda original).
- **Sangria / suprimento** só em dinheiro, com valor > 0 e motivo.

### Fechamento

Conta-se **Dinheiro, Crédito, Débito e Pix**. Os campos de contagem começam **vazios**: o operador precisa contar e digitar (antes vinham preenchidos com o esperado, o que permitia fechar sem contar). Antes de fechar há uma confirmação.

Vendas ou recebimentos em formas fora dessas quatro (ex.: *Não Sei*) não entram na conferência; o fechamento e o relatório mostram um **aviso** com o valor.

Valor esperado:

- **Dinheiro** = troco inicial + vendas em dinheiro + recebimentos em dinheiro + suprimentos − sangrias
- **Cartão / Pix** = vendas daquela forma + recebimentos daquela forma

Tolerância (em Configurações → grupo *Fechamento de caixa*, padrão **R$ 2,00** ou **0,5%** do esperado, o que for maior):

- Dentro da tolerância: fecha sem justificativa.
- Acima: observação com no mínimo 5 caracteres.
- Qualquer diferença em crédito, débito ou Pix (a partir de R$ 0,01) é marcada como **ocorrência técnica**.

**Caixa esquecido aberto** (outro computador desligado, nome da máquina alterado): no **Histórico**, botão direito no caixa com status `ABERTO` → *Fechar este caixa…*. O fechamento continua exigindo a contagem e o PIN de quem abriu o turno.

Não há reabertura de caixa na interface. O banco já tem colunas (`reaberto_por`, `motivo_reabertura`, `reaberto_em`) para um futuro, mas o fluxo não foi implementado.

### Cancelamento de venda

- Venda **sem** `id_caixa` (legado): cancelamento segue como antes.
- Venda **com** caixa ainda aberto: exige motivo; lança `CANCELAMENTO` no caixa; se a venda já estava paga, o movimento vai como estorno (`estornado`).
- Venda de caixa **já fechado**: não cancela.
- Venda **a prazo** que já teve dinheiro recebido em um caixa **já fechado**: não cancela (apagaria o recebimento de um caixa conferido).

---

## Onde entra no restante do sistema

- **PDV / venda**: `MainWindow::garantirCaixaAberto()` antes de abrir a venda; `Vendas_service` recusa insert sem caixa e grava `id_caixa`.
- **Pagamento a prazo**: exige caixa, grava `id_caixa` na entrada e chama `registrarRecebimento`. Se o lançamento no caixa falhar, a entrada é desfeita.
- **Excluir recebimento a prazo**: `podeRemoverRecebimento` confere antes (recusa se o caixa daquele lançamento já fechou); o lançamento só sai do caixa depois da entrada ser apagada.
- **Cancelar venda**: `validarCancelamento` + motivo na tela de vendas; rollback de NF usa o motivo *Falha ao emitir a nota fiscal.*
- **Schema**: migração **15** (`ultimaVersaoSchema = 15`).
- **Rodapé da janela principal**: atualiza sozinho a cada 15 s (o caixa pode ser aberto/fechado em outra tela ou computador).

---

## Banco (migrações 14 e 15)

| Tabela | Conteúdo |
| --- | --- |
| `operadores` | Nome, hash/salt do PIN, ativo, tentativas, bloqueado |
| `caixas` | Operador, terminal, status `ABERTO`/`FECHADO`, troco inicial/sugerido, observação, campos de reabertura |
| `movimentacoes_caixa` | `SANGRIA`, `SUPRIMENTO`, `RECEBIMENTO`, `CANCELAMENTO` |
| `fechamentos_caixa` | Uma linha por forma: esperado, informado, diferença, ocorrência técnica |
| `config_caixa` | (migração 15) Chave/valor compartilhado: PIN do gerente e controle de bloqueio |

Migração 15 também cria os índices únicos `ux_caixas_aberto_terminal` e `ux_caixas_aberto_operador` (`WHERE status = 'ABERTO'`). Se já existir duplicidade de dados antigos, o índice não é criado (o sistema abre normalmente e o serviço continua conferindo a regra).

`vendas2.id_caixa` e `entradas_vendas.id_caixa` são anuláveis (legado).

---

## Arquivos

Padrão do projeto: DTO → repositório → serviço → janela `.ui`.

| Camada | Arquivos |
| --- | --- |
| DTO | `src/dto/Operador_dto.h`, `src/dto/Caixa_dto.h` |
| Repositório | `src/repository/operador_repository.*`, `src/repository/caixa_repository.*` |
| Serviço | `src/services/operador_service.*`, `src/services/caixa_service.*`, `src/services/relatoriocaixa_service.*` |
| Telas | `src/operadores.*`, `src/aberturacaixa.*`, `src/fechamentocaixa.*`, `src/movimentacaocaixa.*`, `src/historicocaixas.*`, `src/relatoriocaixajanela.*` |
| Config | `caixa/tolerancia_valor` e `caixa/tolerancia_percent` em `Config_dto` / Configurações |
| Testes | `tests/services/test_caixa_service.*` (abertura, PIN, sangria/suprimento, esperado por forma, tolerância, ocorrência técnica, cancelamento, recebimentos, PIN do gerente, índices únicos) |

---

## Atalho de prévia (desenvolvimento)

`QEstoqueLoja --preview-caixa <tela>` abre a janela correspondente (`operadores`, `abrir`, `fechar`, `historico`, `sangria`) para captura de tela. Não é fluxo de loja.
