# invest v0.3.11 — Dark Elegant / Proteção de dados

Simulador de investimentos e organizador de finanças pessoais, **nativo para Linux**.
Construído em C11 + Xlib + Cairo, sem GTK, Qt, Electron, navegador, banco de dados,
telemetria ou conexão obrigatória à internet.

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
  ocioso, sem polling/timer), e `Expose` copia somente a região afetada do
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
sudo xbps-install -S gcc make pkg-config cairo-devel libX11-devel
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
sem renderização contínua, serviços em segundo plano ou requisições de rede.

## Testes

`make test` cobre fórmulas financeiras, regras temporárias, orçamentos,
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
