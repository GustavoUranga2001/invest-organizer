# invest v0.3.12 — Dark Elegant / Indicadores econômicos

Simulador de investimentos e organizador de finanças pessoais, **nativo para Linux**.
Construído em C11 + Xlib + Cairo, sem GTK, Qt, Electron, navegador, banco de dados,
telemetria ou conexão obrigatória à internet.

## Novo na v0.3.12 — Mercado na barra lateral

A barra lateral exibe **Selic-meta (% a.a.)**, **dólar e euro PTAX (venda,
BRL)** e **Bitcoin/Monero (preços de referência em BRL)**. Há um botão
**Atualizar mercado**. O app também tenta atualizar uma vez ao abrir caso os
dados estejam ausentes ou a última consulta tenha mais de 15 minutos.

- **Banco Central do Brasil (SGS 432):** Selic-meta definida pelo Copom,
  distinta da taxa Selic efetiva e do rendimento de aplicações.
- **Banco Central (PTAX/OData):** valor de venda do último boletim disponível
  para USD e EUR. É uma cotação de referência, não um preço de compra e venda
  em bancos ou corretoras, nem cotação de mercado em tempo real.
- **CoinPaprika (API de tickers):** preços indicativos agregados em BRL
  de BTC e XMR. A API tem limites, termos de uso e pode ficar indisponível.
- **Cotações são informativas:** não afetam cálculos, orçamento nem criam
  operações financeiras automaticamente. Nenhum valor é inventado quando a
  consulta falha: é exibido `--` ou o último valor local com data da consulta.
- **Atualização não bloqueante:** um processo auxiliar e temporário executa
  até cinco requisições HTTPS com `curl` (limites de 4 s para conexão e 8 s por
  requisição). O processo é encerrado ao sair; não há daemon nem polling
  permanente. A interface continua utilizável se a rede estiver lenta.
- **Armazenamento:** `mercado.tsv` é salvo separadamente em
  `~/.local/share/invest/` (ou `$XDG_DATA_HOME/invest/`). O arquivo usa
  substituição atômica e backup `.bak` da última versão válida.
- **Privacidade:** as consultas envolvem apenas endereços HTTPS fixos de
  fornecedores públicos, sem transmitir seus cenários ou orçamento.

Requer o executável `curl` **apenas para atualizar** os indicadores; o
restante do programa e a consulta dos valores salvos funcionam sem ele.
Não há nova biblioteca C obrigatória. Os valores de criptomoedas podem ser
abreviados na barra lateral para caber em 1280×800.



## Novidades v0.3.11 — segurança e estabilidade

- **Gravação segura unificada:** cenários, orçamento legado e orçamento mensal
  são gravados primeiro em arquivo temporário privado, sincronizados com disco
  (`fsync`) e só depois substituem o arquivo anterior por `rename` atômico.
  O diretório também é sincronizado no final.
- **Backup verificado:** cada novo salvamento preserva em `.bak` a versão
  anterior, *somente se ela for válida*. Dados corrompidos jamais substituem
  um backup válido por conta de uma gravação posterior.
- **Recuperação explícita:** `invest-doctor check` verifica cenários, orçamento
  legado e todos os meses existentes, sem fazer alterações. Com
  `invest-doctor recover`, cada arquivo inválido com backup válido pode ser
  restaurado. Antes disso, o conteúdo danificado é preservado como
  `arquivo.tsv.corrupt.*` para análise.
- **Bloqueio de sessão:** duas instâncias do `invest` não podem editar ao mesmo
  tempo o mesmo diretório de dados. A recuperação também exige fechar o app.
- **Testes de regressão:** incluídos casos de arquivos truncados, backups,
  recuperação e prevenção de sobrescrita de arquivos corrompidos.
- **Sem migração:** fórmulas, formatos TSV, layout e desempenho da versão
  anterior foram preservados.

### Diagnosticar e recuperar dados

Antes de qualquer recuperação, faça uma cópia da pasta `invest`, feche o
aplicativo e execute, na pasta do código compilado:

```sh
./invest-doctor check
```

Se houver um arquivo danificado e um backup válido, execute:

```sh
./invest-doctor recover
./invest-doctor check
```

A recuperação é **opt-in**. Um arquivo válido nunca é substituído por esse
comando. Se o backup também estiver inválido ou não existir, a ferramenta
**não inventa dados nem apaga o arquivo original**. Quando um arquivo tinha
sido danificado, seus bytes são salvos em `.corrupt.*` antes da restauração.
O backup `.bak` é a **versão imediatamente anterior ao último salvamento**;
não representa necessariamente o estado mais recente se houver corrupção.

## Atualizações herdadas da v0.3.10

- Tema escuro **Dark Elegant**, cartões e campos com contraste adequado, abas e
  barra lateral em grafite-azulado, gráficos em verde e azul discretos.
- Tela **Simulador** mais compacta: gráfico e resultados cabem em um monitor de
  1280×800 (ThinkPad X200), sem perder a taxa equivalente anual.
- Renderização **sob demanda**: o loop X11 usa `XNextEvent()` (bloqueia quando
  ocioso; apenas durante consultas assíncronas verifica periodicamente
  a conclusão do processo temporário), e `Expose` copia somente a região afetada do
  pixmap de backbuffer já pronto, sem repetir a construção da tela.
- **Cache do gráfico anual**: os arquivos de 12 meses são lidos uma vez quando
  necessário, em vez de relidos a cada repintura. O cache é invalidado ao
  editar o orçamento ou navegar entre meses.
- Converte a paleta de cores uma única vez no início e evita selecionar a
  mesma fonte repetidamente durante um desenho.
- Cairo com antialiasing `GOOD` no lugar de `BEST`, sem blur, sombras complexas
  ou animações.
- Redimensionamento X11 coalescido para evitar recriação repetida de buffers
  enquanto se arrasta a borda da janela. Um único pixmap com dimensões da
  janela serve como backbuffer.
- Makefile com compilação incremental por módulo para acelerar modificações no
  próprio computador.

## O que foi preservado

**Simulador:** vários cenários, capital inicial, aportes, juros compostos,
faixas temporárias e permanentes de juros/aportes, aportes extraordinários,
taxa equivalente anual, gráfico e armazenamento TSV.

**Eventos:** formulários individuais para taxas, aportes mensais e extras.
**Comparar:** comparação de cenários e suas curvas.
**Finanças:** meses independentes, planejamento versus realizado, receitas,
despesas, valor reservado para investimentos e gráfico anual.

Os arquivos existentes continuam no mesmo formato da v0.3.9; **não é necessário
apagar ou converter seus dados**.

## Instalação no Void Linux

O Void separa a biblioteca Cairo dos cabeçalhos de desenvolvimento:

```sh
sudo xbps-install -S gcc make pkg-config cairo-devel libX11-devel curl
```

Dentro da pasta do projeto:

```sh
make
make test
./invest
```

Para instalar sem precisar de permissões de administrador:

```sh
make install PREFIX="$HOME/.local"
```

O comando `make` gera dois programas: `invest` (interface gráfica) e
`invest-doctor` (diagnóstico em terminal, sem Cairo/X11 no executável).
O utilitário não adiciona nenhuma dependência nova ao projeto.

O programa funciona com servidor X11 ou XWayland. Necessita de uma fonte
Unicode instalada; tenta usar Inter e aceita substitutas disponíveis no
sistema. Para executar em Void musl, **compile o código-fonte no próprio Void**:
o binário de exemplo foi construído em Debian/glibc.

## Arquivos de dados

Por padrão, são mantidos em `~/.local/share/invest/` (ou
`$XDG_DATA_HOME/invest/`):

- `cenarios.tsv`: investimentos simulados e eventos.
- `mercado.tsv`: último conjunto conhecido de indicadores, hora da consulta e data da fonte; não contém seus investimentos.
- `orcamento.tsv`: dados legados de orçamento, mantidos.
- `meses/AAAA-MM.tsv`: planejado e realizado de cada mês.
- `cenarios.tsv.bak`, `orcamento.tsv.bak` e `meses/AAAA-MM.tsv.bak`:
  cópias verificadas da versão anterior.
- Arquivos `.corrupt.*`: dados danificados preservados antes da restauração.
- `.invest.lock`: bloqueio de gravações simultâneas (arquivo pequeno).

**Antes de atualizar**, recomendamos:

```sh
cp -a ~/.local/share/invest "$HOME/invest-backup-$(date +%Y%m%d-%H%M%S)"
```

## Observações de desempenho

O teste de integração foi realizado com um servidor X11 virtual (1280×800) no
ambiente de desenvolvimento, **não em um ThinkPad X200 físico**. A execução
em repouso não mostrou uso perceptível de CPU e registrou aproximadamente
10–11 MB de memória residente do processo nesse ambiente; valores variarão
conforme distribuição, fontes, resolução, bibliotecas e drivers.

A versão prioriza uso reduzido de recursos, mas não afirma um mínimo absoluto.
No X200 real, o desempenho deve ser validado pela pessoa usuária. Para medir:

```sh
ps -C invest -o pid,rss,%cpu,etime,comm
```

O desenho final é feito em uma superfície Cairo vinculada a um pixmap X11,
sem renderização contínua ou serviços residentes. Consultas de mercado são
feitas somente durante a atualização pontual, em um processo separado.

## Testes

`make test` cobre parsing e cache dos indicadores, fórmulas financeiras, regras temporárias, orçamentos,
migração de arquivos, UTF-8, acentos, Backspace, armazenamento por mês e
recuperação de arquivos danificados em três formatos.
Foi realizado também um teste gráfico com Expose, as quatro abas e
redimensionamento em X11 virtual.

Licença MIT. Consulte `LICENSE`.

## Limites da proteção

- A recuperação precisa de **um backup válido**; não reconstrói informações
  que nunca chegaram a ser salvas.
- `.bak` contém apenas a versão anterior, e não um histórico ilimitado.
- A segurança depende da integridade do sistema de arquivos e do disco.
  Para proteção contra falhas de hardware, mantenha cópias externas.
- A função de recuperação é um utilitário de terminal nesta versão. Uma tela
  gráfica de recuperação poderá ser considerada numa atualização posterior.

## Validação de API e limites

Os testes automatizados utilizam respostas JSON de exemplo para verificar
campos, limites e erros sem depender da internet. O fluxo completo de
atualização, gravação e reabertura foi testado com respostas simuladas
(`curl` de teste). A disponibilidade real das APIs pode mudar.
A compilação de integração no ambiente de desenvolvimento utilizou os
headers de compatibilidade do Cairo disponíveis ali; recomenda-se compilar
no Void com `cairo-devel` oficial e executar `make test`.
