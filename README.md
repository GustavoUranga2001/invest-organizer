# invest v0.3.2 — Linux

Simulador de investimentos com interface gráfica nativa para Linux, em **C11 e Xlib**.
Sem GTK, Qt, Electron, banco de dados, conta na nuvem ou conexão obrigatória à internet.

A v0.3.2 acrescenta **aportes mensais e taxas de juros temporários ou permanentes**, com formulários acessíveis pela aba **EVENTOS**. Mantém o tema escuro/verde, as três abas no topo, a taxa anual equivalente e a comparação de cenários da v0.3.1.

## Dependências

- Linux com X11 (ou Wayland com XWayland habilitado)
- `cc`/GCC, `make` e Xlib de desenvolvimento
- Fontes X11 (recomendado: Helvetica; há fallback para `fixed`)

**Arch Linux:**

```sh
sudo pacman -S --needed base-devel libx11 xorg-fonts-misc xorg-fonts-75dpi
```

**Debian / Ubuntu:**

```sh
sudo apt install build-essential libx11-dev xfonts-base xfonts-75dpi
```

## Compilar, executar e testar

```sh
make
make test
./invest
```

Alternativa: `./run.sh`. Para instalar no menu de aplicativos:

```sh
make install PREFIX="$HOME/.local"
```

## O que há na interface

- **SIMULADOR:** nome, valor inicial, aporte mensal BASE, juros mensais BASE, prazo, taxa anual equivalente BASE, resultados e gráfico de patrimônio versus capital investido.
- **EVENTOS:** três controles internos: **Juros**, **Aportes mensais**, **Aportes extras**.
- **COMPARAR:** seleção de dois cenários com resultados e gráficos.
- Lateral: criar, selecionar, duplicar e excluir cenários (máximo: 32).

### Ajustar os aportes mensais

1. Abra **EVENTOS → Aportes mensais → + Adicionar**.
2. Informe o mês em que o novo aporte começa e o valor mensal desejado.
3. Em **Por quanto tempo?**, escolha **Até o fim** ou **Por alguns meses**.
4. Se escolheu um intervalo, informe sua duração em meses (o mês inicial conta como primeiro mês).
5. Clique em **Salvar**. Use **Editar** ou **Remover** para alterar depois.

**Exemplo:** aporte base de R$ 100 por mês; do mês 3 ao mês 4, R$ 50 por mês; a partir do mês 5, R$ 500 por mês.

- Crie uma alteração para **mês inicial 3**, **R$ 50**, **Por alguns meses → 2**.
- Crie outra para **mês inicial 5**, **R$ 500**, **Até o fim**.

Nos meses 1–2 serão aportados R$ 100, nos meses 3–4 R$ 50 e do mês 5 em diante R$ 500. O investimento adicional pontual fica em **Aportes extras**, separado dos aportes recorrentes.

### Ajustar os juros

Em **EVENTOS → Juros**, repita o processo: informe o mês de início, a taxa mensal e a duração. É possível, por exemplo, aplicar 1,20% a.m. nos meses 1–6, depois 0,85% a.m. nos meses 7–9 e 1,00% a.m. dos meses 10 ao fim.

A **taxa anual equivalente mostrada no SIMULADOR se refere à taxa mensal BASE**, não a uma rentabilidade anual consolidada quando há variações. Seu cálculo é `((1+taxa_mensal/100)^12-1)*100`.

### Como funcionam os períodos que se sobrepõem?

- Um período é **inclusivo**: início no mês 3 com duração de 2 meses vale para os meses 3 e 4.
- Quando várias regras estiverem ativas no mesmo mês, **vence a que começou mais recentemente**.
- Quando um período temporário acaba, o programa **retoma a regra anterior ainda ativa**. Se não houver uma regra anterior ativa, retorna ao valor BASE do cenário.
- Uma alteração **Até o fim** continua ativa até o último mês, exceto enquanto outra regra mais recente estiver vigente.
- Há no máximo uma regra por tipo com o mesmo mês inicial. Edite-a em vez de cadastrá-la duas vezes.
- A duração deve caber no prazo do cenário. Se reduzir o prazo e algum evento ficar de fora, a alteração será rejeitada sem apagar os eventos.

### Salvamento e teclas

- **Ctrl+S:** salvar. **Enter:** confirmar a edição de um campo. **Tab:** ir ao próximo campo. **Esc:** cancelar edição/fechar diálogo.
- **F1/F2/F3:** Simulador / Eventos / Comparar.
- O programa também salva ao fechar normalmente, caso existam alterações válidas.

## Dados e migração

Os cenários são armazenados localmente em:

```text
${XDG_DATA_HOME:-$HOME/.local/share}/invest/cenarios.tsv
```

O formato interno passou a **INVEST 3**, pois agora salva intervalos com mês inicial e final. A v0.3.2 abre cenários das versões v0.1, v0.2, v0.3 e v0.3.1 e migra os dados na gravação. A primeira migração cria um backup do arquivo anterior:

- De arquivo `INVEST 1`: `cenarios.tsv.v1.bak`.
- De arquivo `INVEST 2`: `cenarios.tsv.v2.bak`.

**Atenção:** versões anteriores do aplicativo **não leem o formato INVEST 3**. Antes de testar a v0.3.2, é prudente copiar também a pasta `~/.local/share/invest` (ou seu `XDG_DATA_HOME`) para outro local.

A gravação usa arquivo temporário e troca atômica. Se um arquivo existente for inválido, o programa recusa abri-lo, evitando sobrescrevê-lo automaticamente.

## Convenções e limitações

- Juros são calculados em cada mês usando a taxa vigente; os aportes mensais e eventuais extras entram **no fim do mês**, depois dos juros.
- São projeções **nominais e brutas**. Não há impostos, inflação, tarifas, CDI/IPCA em tempo real nem taxas futuras automaticamente previstas. Para simular um pós-fixado, informe hipóteses de taxas por período.
- 1–600 meses, até 32 alterações de juros, 32 de aporte mensal e 32 aportes extras por cenário.
- Taxas maiores que -100% e até 100% ao mês; valores entre R$ 0 e R$ 1 trilhão.
- Os cálculos usam `double` e exibem centavos arredondados; não é um sistema contábil nem de negociação.

Teste de referência: R$ 8.440,66 de capital inicial, R$ 1.000 mensais, 1,09% a.m. e 24 meses ⇒ **R$ 38.212,22**, sem eventos.

Código-fonte sob licença MIT. O programa não constitui aconselhamento financeiro.
