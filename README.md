# invest v0.3.1 — Linux

Simulador de investimentos com interface grafica nativa X11, em C11 e Xlib.
A v0.3.1 recupera as abas no topo e a paleta escura/verde da v0.2, mantendo
os formularios simples da v0.3.
Nao usa GTK, Qt, Electron, navegadores, telemetria ou internet.

## Requisitos

- Linux com sessao X11 ou Wayland com XWayland
- Compilador C (`cc` / `gcc`), `make` e cabecalhos de desenvolvimento da Xlib
- Fontes X11 (preferencialmente Helvetica, com fallback para `fixed`)

**Arch Linux:**

```sh
sudo pacman -S --needed base-devel libx11 xorg-fonts-misc xorg-fonts-75dpi
```

**Debian / Ubuntu:**

```sh
sudo apt install build-essential libx11-dev xfonts-base xfonts-75dpi
```

## Compilar e executar

```sh
make
make test
./invest
```

Ou simplesmente execute `./run.sh`, que compila caso nao exista um binario local.

Para adicionar ao menu de aplicativos sem privilegios de administrador:

```sh
make install PREFIX="$HOME/.local"
```

Se `~/.local/bin` nao estiver no PATH, execute o binario por esse caminho.

## Como usar

### Simulador (aba superior)

1. Clique em **Novo cenario** ou selecione um cenario no painel lateral.
2. Clique no campo de nome para renomear.
3. Digite **valor inicial**, **aporte mensal**, **juros por mes** e **prazo**.
4. Pressione Enter ou clique fora do campo para atualizar o resultado e grafico.
5. Clique em **Salvar** ou pressione Ctrl+S (o programa tambem salva ao sair normalmente).

Abaixo da taxa mensal aparece sua **taxa anual efetiva equivalente**, calculada por
`((1 + taxa_mensal / 100)^12 - 1) * 100` e arredondada a duas casas decimais.
Por exemplo, 1,09% a.m. equivale a aproximadamente 13,89% a.a.
Quando ha mudancas de juros programadas, esse percentual anual se refere apenas
**a taxa mensal base**; nao representa a rentabilidade anual completa do cenario.

O simulador mostra total investido, juros acumulados e patrimonio projetado mes a mes.

### Eventos (aba superior)

Use a aba **Eventos** (ou o atalho **Eventos e aportes** na aba Simulador):

- **Mudancas de juros**: clique em **Adicionar**, informe o mes a partir do qual a nova taxa mensal deve valer e a propria taxa.
- **Aportes extras**: informe o mes e o valor do aporte unico.
- Para alterar ou retirar um evento use os botoes **Editar** e **Remover**.

Nao e necessario digitar expressoes do tipo `6:1,25;12:0,90`.

### Comparar

Clique em **COMPARAR** no topo. Os dois botoes de selecao de cenarios alternam entre os cenarios disponiveis. O grafico mostra as curvas de patrimonio e os montantes finais. Quando os prazos diferem, a interface exibe um aviso.

### Teclas

| Tecla | Acao |
| --- | --- |
| Ctrl+S | Salvar |
| Ctrl+A | Selecionar o conteudo do campo em edicao |
| Tab | Ir ao proximo campo |
| Enter | Confirmar a edicao atual |
| Esc | Cancelar a edicao / fechar um dialogo |
| F1 / F2 / F3 | Ir para Simulador / Eventos / Comparar |
| Roda do mouse | Rolar a lista de cenarios ou de eventos |

## Dados e compatibilidade

O arquivo e armazenado em:

```text
${XDG_DATA_HOME:-$HOME/.local/share}/invest/cenarios.tsv
```

O aplicativo usa exatamente o **formato INVEST 2** da v0.2 e consegue abrir os dados da v0.1 e da v0.2 sem conversao manual. Na primeira gravacao de arquivo v0.1, cria o backup `cenarios.tsv.v1.bak`. Grava as alteracoes por substituicao atomica.

**Aviso importante:** se encontrar dados invalidos ou danificados, o programa nao os sobrescreve. Ele informa o problema no terminal e nao abre ate o arquivo ser verificado ou recuperado.

## Convencoes do calculo

O saldo inicial rende juros em cada mes; no fim do mes, ocorrem o aporte mensal e os eventuais aportes extras. Uma mudanca de taxa tem efeito a partir do mes selecionado e se mantem ate a proxima mudanca. O montante e **nominal e bruto**: nao inclui impostos, taxas, inflacao ou variacao de indexadores externos.

Para o exemplo padrao, com R$ 8.440,66 iniciais, R$ 1.000 por mes, 1,09% ao mes e 24 meses, o montante projetado e **R$ 38.212,22**.

## Limites

- Ate 32 cenarios
- Prazos de 1 a 600 meses
- Ate 32 alteracoes de taxas e 32 aportes extras por cenario
- A faixa de taxas por mes e maior que -100% e ate 100%
- Valores monetarios sao simulados em ponto flutuante (`double`) e exibidos com duas casas decimais. Nao se trata de um sistema contabil ou de negociacao.

## Observacoes de projeto

A interface usa a paleta original da v0.2: barra superior e painel lateral escuros, conteudo cinza claro e botoes verdes. E implementada com primitivas Xlib e fontes X11, sem toolkit; foi projetada prioritariamente para uso com mouse e teclado em monitores desktop. Os campos de entrada sao simples: para modificar um numero, clique no campo, digite o valor e pressione Enter. Caso use Wayland puro, e necessario ter XWayland habilitado.

## Testes

```sh
make test
```

Valida formulas com e sem eventos, limites, compatibilidade e armazenamento seguro.

Codigo-fonte sob licenca MIT. Nao constitui aconselhamento financeiro.
