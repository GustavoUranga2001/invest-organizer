# invest v0.3.6 — correção do editor de cenários

Aplicativo financeiro minimalista para Linux, escrito em **C11 + Xlib + Cairo**.
Esta atualização corrige a edição de nomes de cenário e reforça a segurança dos
arquivos de dados. Não altera os cálculos, as quatro abas nem os formatos TSV.

## O que foi corrigido

- Ao apagar todo o nome de um cenário com Backspace e sair do campo, o programa
  **cancela apenas a edição do nome e restaura o nome anterior**, sem afetar
  parâmetros financeiros, eventos ou gráficos.
- Nomes inválidos não travam o fluxo de navegação entre campos ou cenários.
- Antes de cada atualização de `cenarios.tsv`, a última versão válida é copiada
  atomicamente para `cenarios.tsv.bak` (desde que já exista arquivo anterior).
- Se o arquivo anterior estiver inválido, a aplicação se recusa a sobrescrevê-lo
  e mantém o backup existente.

## Instalar e rodar no Void Linux

Instale os pacotes de desenvolvimento do X11 e do Cairo, além do compilador,
make e pkg-config (`cairo-devel` e `libX11-devel` são essenciais no Void).

```sh
sudo xbps-install -S gcc make pkg-config cairo-devel libX11-devel
make
make test
./invest
```

Se o compilador ainda reclamar de `cairo.h`, verifique:

```sh
pkg-config --cflags cairo
```

O aplicativo funciona em X11 ou XWayland. As fontes usam substituição via Cairo
caso a fonte Inter não esteja instalada.

Para instalar no menu do ambiente gráfico:

```sh
make install PREFIX="$HOME/.local"
```

## Arquivos de dados

Padrão: `~/.local/share/invest/` (ou `$XDG_DATA_HOME/invest/`).

- `cenarios.tsv`: simulações, juros, aportes e eventos.
- `cenarios.tsv.bak`: cópia da última versão válida antes da gravação mais recente.
- `cenarios.tsv.v1.bak` / `cenarios.tsv.v2.bak`: backups de migrações antigas, se existirem.
- `orcamento.tsv`: receitas, despesas e valor reservado para investimento; não
  foi alterado pela v0.3.6.

**Antes de recuperar ou editar dados, feche o programa e faça backup da pasta:**

```sh
cp -a ~/.local/share/invest "$HOME/invest-backup-$(date +%Y%m%d-%H%M%S)"
```

Para restaurar a cópia da última gravação, somente se o arquivo `.bak` existir:

```sh
cp ~/.local/share/invest/cenarios.tsv.bak ~/.local/share/invest/cenarios.tsv
```

A restauração substitui o arquivo atual pelos dados do backup (que podem não
conter a última alteração). Não apague o diretório inteiro para corrigir problemas
em um cenário.

## Limites e validação

Testes automatizados cobrem juros compostos, eventos, orçamento, nomes vazios,
backup por gravação e proteção contra sobrescrever arquivos inválidos. A
interface também foi verificada numa sessão X11 de teste, incluindo um nome
apagado por Backspace, edição de outro campo e salvamento/reabertura.
