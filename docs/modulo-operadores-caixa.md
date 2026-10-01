# Módulo de operadores e caixa

Controle de turno por operador: quem abre o caixa, o que entra e sai da gaveta e o fechamento com contagem.

O terminal é o nome da máquina (`QSysInfo::machineHostName()`, em maiúsculas). Só pode haver **um caixa aberto por terminal** e **um caixa aberto por operador**.

Dois níveis de controle convivem:

1. **Sessão do operador** (migração 16) — quem está usando o programa agora, com troca, logout, aviso de expiração e log de auditoria.
2. **Turno de caixa** — quem abre a gaveta, o que entra e sai dela e o fechamento com contagem.

---

## Sessão do operador

Sessão **em memória**: fechar e reabrir o programa sempre pede login de novo. Ela vale até logout explícito, troca de operador ou expiração por inatividade.

### Entrada

A tela `LoginOperador` abre antes da janela principal e lista os operadores ativos; a última linha é sempre **Entrar como gerente**.

- **Operador do cadastro**: PIN próprio de 4 a 6 dígitos. A coluna `operadores.gerente` é lida no login — um operador comum entra como operador.
- **Entrar como gerente**: usa o **PIN geral** (`config_caixa`), sem identidade de operador. A linha fictícia tem `id_operador = 0`; os ids do banco começam em 1, então não há colisão.

| Situação na abertura | O que acontece |
| --- | --- |
| Sem operador e sem PIN do gerente | Primeiro define o PIN do gerente, depois entra |
| Sem operador e com PIN do gerente | Entra pelo PIN do gerente, com aviso para cadastrar o primeiro operador |
| Com operadores | Lista os ativos + linha do gerente |

Cancelar o login fecha o programa: sem sessão aceita nenhuma tela de venda abre.

### Indicador

No rodapé: chip grande com o operador logado (menu *Trocar operador* / *Sair da conta*), botão vermelho **Sair da conta** sempre à vista e o estado do **seu** caixa (🟢 aberto / 🔴 fechado).

O rodapé da janela principal mostra `Nome` e `Nome (gerente)`. A tela de venda e a de lista de vendas repetem o indicador, para ficar claro quem responde pelo que está saindo.

### Matriz de acesso

| Tela | Operador | Gerente |
| --- | --- | --- |
| Venda (PDV), produtos, clientes, orçamentos | livre | livre |
| Histórico de caixas, Operadores, Configurações, Relatórios | pede o **PIN do gerente** | entra direto |

### Troca, logout e expiração

- **O caixa é do operador logado**, não do computador: vários operadores podem ter caixa aberto no mesmo terminal (um por operador) e cada um só vê/usa o seu (`Caixa_service::caixaAtual()`). Trocar de operador ou sair da conta **não exige fechar o caixa**; ele continua aberto até o dono fechar. Quem já está logado abre o próprio caixa **sem digitar o PIN de novo**; abrir o de outro operador continua pedindo o PIN dele.
- **Venda em andamento** (qualquer tela de venda registrada com itens no carrinho) bloqueia troca e logout, e a sessão **nunca expira**: só é **bloqueada**.
- **Bloqueio de sessão**: por inatividade com venda em andamento, a tela fica travada e a venda preservada. Desbloqueia com o PIN do próprio operador ou com o PIN do gerente (este fica na auditoria como `DESBLOQUEIO_POR_GERENTE`).
- **Revalidação**: a cada batida do relógio a sessão é conferida no banco; se o operador foi desativado/bloqueado/rebaixado ou o PIN geral mudou, a sessão é invalidada (`INVALIDADA`).
- **Elevação**: operador comum que digita o PIN do gerente para uma ação administrativa fica elevado por pouco tempo, com registro (`ELEVACAO_PIN_GERAL`). O serviço de operadores também recusa cadastro/desbloqueio/redefinição de PIN/marcar gerente sem gerente ou elevação (defesa em profundidade, não só na tela).
- **Uma instância por computador** (trava `QLockFile`): uma segunda janela avisa e fecha.
- O **aviso de expiração é não-modal** (rodapé, sem-trava), para não derrubar a venda.
- Uma sessão que o programa anterior deixou aberta (sem logout) é fechada na entrada seguinte, com motivo `SEM_LOGOUT` — o mesmo tratamento do caixa esquecido aberto.
- Motivos gravados: `LOGOUT`, `TROCA_OPERADOR`, `TIMEOUT`, `INVALIDADA`, `SEM_LOGOUT`, `ENCERRAMENTO`.

### Timeout de inatividade

Configurável em **Configurações → grupo *Sessão do operador*** (é local desta máquina, não vai para o banco):

- `caixaTimeoutAtivo` (padrão **ligado**) e `caixaTimeoutMinutos` (padrão **30**).
- Qualquer evento de mouse, teclado ou roda na aplicação inteira reinicia a contagem.
- Aviso no rodapé a partir de **2 minutos** para o fim.
- **Nunca expira no meio de uma venda**: com carrinho montado ou pagamento em aberto, o relógio da sessão fica congelado — e o operador ainda tem o tempo normal ao terminar a venda, em vez de ser expulso no instante seguinte.
- Alterar a configuração vale para a sessão em andamento, sem precisar de novo login.

### Auditoria

Tabela `sessoes_operador`, append-only: entrada no login, saída no logout/troca/expiração. Guarda `id_operador`, `nome_operador`, `gerente`, `terminal`, `entrada_em`, `saida_em`, `motivo_saida` e `id_caixa_no_momento`.

Ações sensíveis vão para `auditoria_acesso` (migração 17): uso do PIN do gerente, bloqueio/desbloqueio, sessão invalidada. O gerente consulta as duas tabelas em **Caixa → Log de acesso** (`LogAcessoDialog`, só leitura).

O PIN do gerente (tentativas e bloqueio) vale **por terminal**: errar em um computador não trava os outros.

### Quem responde pela venda

Antes da migração 16 o dono do caixa e o operador da ação eram o mesmo campo. Agora são separados:

| Campo | Significado |
| --- | --- |
| `vendas2.id_caixa` → `caixas.id_operador` | **dono do caixa** (quem abriu o turno) |
| `vendas2.id_operador_sessao` | **quem estava logado** quando a venda saiu |
| `movimentacoes_caixa.id_operador` | dono do caixa |
| `movimentacoes_caixa.id_operador_sessao` | quem praticou o movimento |
| `entradas_vendas.id_operador_sessao` | quem recebeu |

Na tabela, `-1` (sem sessão) vira `NULL`; `0` é o gerente do PIN geral e é uma sessão válida.

O backfill da migração 16 reproduz o histórico com a única identidade que existia: `movimentacoes_caixa` recebe o próprio `id_operador`, `vendas2` recebe o dono do caixa via `id_caixa` e `entradas_vendas` recebe o operador do movimento `RECEBIMENTO` ligado a ela.

No fechamento, o repositório confere (já com o caixa marcado como fechado) se o número de vendas e movimentações ainda é o que o operador conferiu; se alguém lançou no meio, o fechamento é desfeito e deve ser refeito.

O relatório do caixa lista, abaixo do dono, os **outros operadores da sessão** com a contagem de registros de cada um (`Caixa_service::resumo()` → `operadoresDaSessao`, no HTML e na impressão térmica).

### Entrada em modo de desenvolvimento

`QESTOQUELOJA_DEV_AUTOLOGIN=1` entra no primeiro operador ativo sem pedir PIN. Só funciona em build **Debug** (a macro `QEL_MODO_DESENVOLVIMENTO` só é definida na configuração Debug do CMake; o workflow do Windows falha se o texto da variável aparecer no `.exe`) e mostra a tarja laranja *MODO DESENVOLVIMENTO* no topo da janela. `--preview-caixa` **não** recebe esse bypass.

---

## Telas (menu Caixa)

O menu fica na barra principal, entre Exportar e Ajuda. O rodapé mostra `Caixa fechado` (vermelho) ou `Caixa aberto · Nome · #id` (verde).

| Item | Janela | Função |
| --- | --- | --- |
| Trocar operador… | `LoginOperador` | Encerra a sessão e pede login de novo |
| Sair do operador | (confirmação) | Encerra a sessão; se não entrar com outra, o programa fecha |
| Abrir caixa… | `AberturaCaixa` | Operador, PIN, troco inicial (sugerido e editável) |
| Fechar caixa… | `FechamentoCaixa` | Contagem das 4 formas, observação e PIN |
| Sangria… | `MovimentacaoCaixa` | Retirar dinheiro da gaveta (motivo obrigatório) |
| Suprimento… | `MovimentacaoCaixa` | Colocar dinheiro na gaveta (motivo obrigatório) |
| Histórico… | `HistoricoCaixas` | Lista por período e relatório do turno (pede gerente) |
| Operadores… | `Operadores` | Cadastro, PIN, gerente, bloqueio e ativação (pede gerente) |
| PIN do gerente… | (diálogos) | Define ou troca o PIN do gerente |

O relatório do histórico abre em `RelatorioCaixaJanela` (HTML na tela, PDF ou térmica).

---

## Operador

Cadastro em **Caixa → Operadores**.

| Campo | Regra |
| --- | --- |
| Nome | Obrigatório, único (sem diferenciar só por espaços) |
| PIN | 4 a 6 dígitos numéricos |
| Gerente | (migração 16) Acesso direto às telas restritas, sem digitar o PIN do gerente |
| Ativo | Desativado não abre nem fecha caixa |
| Bloqueado | Após 3 PINs errados; só desbloqueia nesta tela (ou ao redefinir o PIN) |

O PIN **não** é gravado em texto. Vai para o banco como SHA-256 iterado 20.000 vezes com um salt aleatório por operador.

O PIN é pedido na **abertura** e no **fechamento** (sempre o PIN de quem abriu o turno). A conferência do hash é feita em tempo constante.

**Desativar** um operador que tem caixa aberto é recusado: ele não conseguiria informar o PIN no fechamento e o caixa ficaria preso.

**Gerente** é o que libera Histórico, Operadores, Configurações e Relatórios sem PIN. A loja nunca pode ficar sem gerente com identidade: remover a marca ou desativar o **único** gerente ativo é recusado, com a orientação de cadastrar outro antes.

### PIN do gerente

A tela **Operadores** (e o botão *Operadores...* da abertura) só abre com o **PIN do gerente** ou com sessão de gerente. Sem isso, qualquer operador poderia redefinir ou desbloquear o PIN de outro.

- Na primeira vez o sistema pede para **definir** o PIN do gerente (4 a 6 dígitos, digitado duas vezes). Com operadores já cadastrados, a tela de configurações **não** deixa definir o PIN ali: ele só nasce junto com o primeiro cadastro, senão qualquer operador assumiria o controle da loja.
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

- **PDV / venda**: `MainWindow::garantirCaixaAberto()` antes de abrir a venda; `Vendas_service` recusa insert sem caixa e grava `id_caixa` e `id_operador_sessao` (quem está logado).
- **Pagamento a prazo**: exige caixa, grava `id_caixa` e `id_operador_sessao` na entrada e chama `registrarRecebimento`. Se o lançamento no caixa falhar, a entrada é desfeita.
- **Excluir recebimento a prazo**: `podeRemoverRecebimento` confere antes (recusa se o caixa daquele lançamento já fechou); o lançamento só sai do caixa depois da entrada ser apagada.
- **Cancelar venda**: `validarCancelamento` + motivo na tela de vendas; rollback de NF usa o motivo *Falha ao emitir a nota fiscal.*
- **Schema**: migração **16** (`ultimaVersaoSchema = 16`).
- **Rodapé da janela principal**: atualiza sozinho a cada 15 s (o caixa pode ser aberto/fechado em outra tela ou computador).

---

## Banco (migrações 14 a 18)

| Tabela | Conteúdo |
| --- | --- |
| `operadores` | Nome, hash/salt do PIN, ativo, tentativas, bloqueado, gerente (16) |
| `caixas` | Operador, terminal, status `ABERTO`/`FECHADO`, troco inicial/sugerido, observação, campos de reabertura |
| `movimentacoes_caixa` | `SANGRIA`, `SUPRIMENTO`, `RECEBIMENTO`, `CANCELAMENTO`, `id_operador_sessao` (16) |
| `fechamentos_caixa` | Uma linha por forma: esperado, informado, diferença, ocorrência técnica |
| `sessoes_operador` | (16) Log de entrada/saída do operador, com terminal e motivo |
| `auditoria_acesso` | (17) Ações sensíveis: data/hora, operador da sessão, terminal, ação, detalhe |
| `config_caixa` | (migração 15) Chave/valor compartilhado: PIN do gerente e controle de bloqueio |

Migração 15 também cria os índices únicos `ux_caixas_aberto_terminal` (removido na 18) e `ux_caixas_aberto_operador` (`WHERE status = 'ABERTO'`). Se já existir duplicidade de dados antigos, o índice não é criado (o sistema abre normalmente e o serviço continua conferindo a regra).

Migração 16 adiciona `operadores.gerente`, a tabela `sessoes_operador` (com o índice `idx_sessoes_abertas`) e a coluna `id_operador_sessao` em `vendas2`, `movimentacoes_caixa` e `entradas_vendas`, com o backfill descrito em *Quem responde pela venda*.

Migração 17 cria `auditoria_acesso` e índices (`idx_auditoria_data`, `idx_vendas2_operador_sessao`, `idx_mov_operador_sessao`, `idx_entradas_operador_sessao`, `idx_entradas_vendas_caixa`, `idx_sessoes_entrada`). O backfill da 16 agora **aborta** a migração se falhar (antes o erro era engolido).

**Atenção nas atualizações:** as migrações 16 e 17 não têm volta. Atualize **todos os terminais juntos**; um programa antigo abrindo um banco já migrado não entende as colunas novas.

Migração 18 remove o índice `ux_caixas_aberto_terminal`: o caixa passa a ser por operador (`ux_caixas_aberto_operador` continua garantindo um aberto por operador).

`vendas2.id_caixa` e `entradas_vendas.id_caixa` são anuláveis (legado).

---

## Arquivos

Padrão do projeto: DTO → repositório → serviço → janela `.ui`.

| Camada | Arquivos |
| --- | --- |
| DTO | `src/dto/Operador_dto.h`, `src/dto/Caixa_dto.h`, `src/dto/Sessao_dto.h`, `src/dto/Vendas_dto.h` |
| Repositório | `src/repository/operador_repository.*`, `src/repository/caixa_repository.*`, `src/repository/sessao_repository.*`, `src/repository/vendas_repository.*` |
| Serviço | `src/services/operador_service.*`, `src/services/caixa_service.*`, `src/services/sessao_service.*`, `src/services/relatoriocaixa_service.*` |
| Telas | `src/loginoperador.*`, `src/operadores.*`, `src/aberturacaixa.*`, `src/fechamentocaixa.*`, `src/movimentacaocaixa.*`, `src/historicocaixas.*`, `src/relatoriocaixajanela.*` |
| Config | `caixa/tolerancia_valor`, `caixa/tolerancia_percent` (banco) e `caixaTimeoutAtivo` / `caixaTimeoutMinutos` (local da máquina) em `Config_dto` / Configurações |
| Testes | `tests/services/test_caixa_service.*` (abertura, PIN, sangria/suprimento, esperado por forma, tolerância, ocorrência técnica, cancelamento, recebimentos, PIN do gerente, índices únicos, sessão, gerente, operador da venda) |

---

## Atalho de prévia (desenvolvimento)

`QEstoqueLoja --preview-caixa <tela>` abre a janela correspondente (`operadores`, `abrir`, `fechar`, `historico`, `sangria`) para captura de tela. Não é fluxo de loja.
