# invest v0.3.3 — Linux

Aplicativo financeiro minimalista em **C11 + Xlib**, para Linux com X11 ou XWayland.
Sem GTK, Qt, Electron, banco de dados, contas online nem conexão obrigatória à internet.

A v0.3.3 preserva **Simulador**, **Eventos** e **Comparar** da v0.3.2 e acrescenta a aba **Finanças** (organização financeira pessoal).

## Instalar e executar

No **Arch Linux**:

```sh
sudo pacman -S --needed base-devel libx11 xorg-fonts-misc xorg-fonts-75dpi
tar -xzf invest-v0.3.3-source.tar.gz
cd invest-v0.3.3
make
make test
./invest
```

No **Debian/Ubuntu**, instale as dependências antes:

```sh
sudo apt install build-essential libx11-dev xfonts-base xfonts-75dpi
```

Para instalar o aplicativo no menu (após `make`):

```sh
make install PREFIX="$HOME/.local"
```

Alternativamente, execute `./run.sh` na pasta do projeto.

## Aba Finanças

A aba **FINANÇAS** representa um **mês típico**, com receitas e despesas recorrentes. O objetivo é calcular quanto sobra após pagar os gastos mensais planejados.

1. Abra **FINANÇAS** (ou pressione **F4**).
2. Selecione **Receitas** e clique em **+ Adicionar**. Informe uma descrição (por exemplo, `Salário`) e o valor mensal.
3. Selecione **Despesas** e clique em **+ Adicionar**. Informe uma descrição, o valor mensal, a categoria e se é um gasto **fixo** ou **variável**.
4. O aplicativo atualiza automaticamente as receitas, despesas e o saldo previsto, além do percentual da renda comprometida.
5. Para corrigir os lançamentos, use **Editar** ou **Excluir**. Uma exclusão solicita confirmação.
6. Salve com **Ctrl+S** ou o botão **Salvar**. Alterações válidas também são salvas ao fechar normalmente.

Categorias de despesas: Moradia, Alimentação, Transporte, Saúde, Educação, Lazer, Contas e Outros. Ao clicar no botão **Categoria**, o programa percorre essas opções.

### Exemplo

| Item | Valor mensal |
|---|---:|
| Salário | R$ 4.500,00 |
| Aluguel (fixa, Moradia) | R$ 1.800,00 |
| Mercado (variável, Alimentação) | R$ 600,00 |
| **Saldo previsto** | **R$ 2.100,00** |

Percentual comprometido: **53,3%** (R$ 2.400,00 / R$ 4.500,00).

**Importante:** os lançamentos são recorrentes, não um extrato bancário real, calendário de vencimentos ou histórico de transações. O orçamento não transfere automaticamente o saldo para aportes do simulador e não acessa bancos.

## Compatibilidade com a v0.3.2

Os cenários de investimento continuam no arquivo e no formato **INVEST 3** da versão anterior:

```text
${XDG_DATA_HOME:-$HOME/.local/share}/invest/cenarios.tsv
```

O orçamento é salvo **separadamente** em:

```text
${XDG_DATA_HOME:-$HOME/.local/share}/invest/orcamento.tsv
```

O novo arquivo usa o formato texto `INVEST_BUDGET 1` (tabulação/TSV), compatível com inspeção manual. Ele é criado apenas quando um orçamento é salvo. Os arquivos são gravados usando um temporário seguido de substituição atômica. Caso o orçamento existente seja inválido, o programa se recusa a carregá-lo para evitar sobrescrevê-lo.

**Dica:** faça uma cópia de segurança da pasta `~/.local/share/invest` antes de instalar uma nova versão.

## Simulador e eventos (mantidos)

- **SIMULADOR (F1):** capital inicial, aporte mensal base, juros mensais base, taxa anual equivalente, prazo, resultado e gráfico.
- **EVENTOS (F2):** taxas mensais e aportes recorrentes temporários/permanentes, além de aportes pontuais.
- **COMPARAR (F3):** comparação de dois cenários de investimento.
- **FINANÇAS (F4):** orçamento mensal (independente das projeções).
- **Ctrl+S:** salvar; **Tab:** próximo campo; **Enter:** confirmar um campo; **Esc:** cancelar edição ou fechar o diálogo.

Os cálculos do simulador são nominais e brutos; não incluem impostos, inflação nem projeções automáticas de CDI/IPCA. Juros são aplicados antes dos aportes ao final de cada mês.

## Projeto

```text
src/
  main.c       Interface gráfica nativa X11
  finance.c    Projeções e regras de investimento
  storage.c    Cenários e migração do formato INVEST
  budget.c     Orçamento mensal e persistência independente
tests/
  test_finance.c
  test_storage.c
  test_budget.c
```

Os testes são executados com `make test`. Os valores monetários de orçamento são arredondados a centavos no cadastro e somados internamente em centavos inteiros no resumo. Não se trata de software de contabilidade ou negociação.

Código-fonte sob licença MIT.
