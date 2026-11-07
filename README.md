# invest v0.3.5 — tipografia e gráficos suavizados

Aplicativo minimalista de simulação de investimentos e orçamento pessoal para Linux.

**Novidades:** substitui as fontes bitmap do X11 por fontes vetoriais antialiasadas via Cairo, desenha gráficos com traços suaves em vez de segmentos de pixels, suaviza cantos e preserva a paleta escura/verde, as quatro abas e os dados da v0.3.4. Textos em português agora usam UTF-8 diretamente.

## Compilar (Arch Linux / X11 ou XWayland)

```bash
sudo pacman -S --needed base-devel libx11 cairo fontconfig ttf-dejavu
make
make test
./invest
```

Opcional: instale uma fonte `Inter` pelo gerenciador da sua distribuição. O programa tenta usá-la e recorre à substituição de fontes do sistema quando ela não estiver presente.

No Debian/Ubuntu: `sudo apt install build-essential libx11-dev libcairo2-dev fonts-dejavu-core`

Para instalar no menu: `make install PREFIX="$HOME/.local"`.

## Dados e compatibilidade

A versão 0.3.5 mantém os formatos do simulador e orçamento anteriores, em `~/.local/share/invest/`. Não é necessário migrar nem apagar nada. Ela não modifica a fórmula financeira.

## Dependências

C11, Xlib, Cairo, libm. Sem GTK, Qt, Electron ou acesso à rede.

## Visualização

A fonte e o antialiasing podem variar segundo fontes instaladas, configurações do Fontconfig e escala do desktop.
