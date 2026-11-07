# invest v0.3.9

Aplicativo nativo de simulação de investimentos e orçamento pessoal para Linux.
Escrito em **C11 + Xlib + Cairo**, sem GTK, Qt, serviços externos ou banco de dados.

## Novidades: Finanças mês a mês

Na aba **FINANÇAS**:

1. Use as setas `<` e `>` para selecionar o mês e o ano.
2. Alterne entre **Planejado** e **Realizado**. Cada visão tem lançamentos e investimentos independentes.
3. Clique em **+ Adicionar** para registrar uma receita ou despesa *do mês escolhido*; edite ou exclua conforme necessário.
4. Em **Definir valor**, informe a quantia que pretende investir ou realmente investiu naquele mês. O valor reduz o saldo disponível, mas não é uma despesa.
5. Use **Copiar anterior** se quiser aproveitar o planejamento do mês anterior. A cópia **só funciona na visão Planejado e com o planejamento atual vazio**; não modifica o mês de origem.
6. Selecione **Visão anual** para acompanhar a evolução de **saldo disponível, receitas, despesas e investimentos**. O botão **Trocar indicador** alterna o tipo de gráfico. Duas cores distinguem planejado e realizado.

**Importante:** `Realizado` não se preenche automaticamente: registre os valores efetivos para compará-los ao planejamento. Meses sem registros não são tratados como observações de zero nos gráficos. Os saldos são os valores informados, não dados bancários sincronizados.

As abas **SIMULADOR**, **EVENTOS** e **COMPARAR** permanecem, incluindo taxas e aportes variáveis, eventos temporários, gráfico, taxa anual equivalente e múltiplos cenários.

## Instalação no Void Linux

```sh
sudo xbps-install -S gcc make pkg-config cairo-devel libX11-devel
make
make test
./invest
```

Para instalar no diretório pessoal:

```sh
make install PREFIX="$HOME/.local"
```

O programa precisa de X11 ou XWayland. Para renderizar textos com acentos, use uma fonte Unicode (DejaVu Sans ou Inter) e uma localidade UTF-8. O método de entrada de acentos e o Backspace da v0.3.8 foram preservados.

## Dados e migração

Por padrão, o diretório de dados é `~/.local/share/invest/`, ou `$XDG_DATA_HOME/invest/` se a variável XDG estiver definida.

- `cenarios.tsv` — simulações existentes, **sem alterações de formato**.
- `orcamento.tsv` — orçamento mensal típico **legado** da v0.3.8, preservado e não sobrescrito pela v0.3.9.
- `meses/2026-11.tsv`, `meses/2026-12.tsv` etc. — **novos arquivos de planejamento/realizado por mês**.
- `meses/2026-11.tsv.bak` — cópia da gravação anterior do mês quando já existia.

Na primeira abertura, se o mês atual ainda não possui arquivo mensal, o orçamento antigo de `orcamento.tsv` é importado **somente para o planejamento do mês atual**. O arquivo original permanece intacto. Para outros meses, comece com registros vazios ou clique em **Copiar anterior**. O programa não replica o orçamento automaticamente em todos os meses, para evitar lançamentos inventados.

A nova versão salva arquivos mensais com gravação temporária seguida de substituição atômica. Se um arquivo mensal estiver corrompido, a leitura falha sem apagá-lo ou sobrescrevê-lo silenciosamente. Faça sempre um backup antes de atualizar:

```sh
cp -a ~/.local/share/invest "$HOME/invest-backup-$(date +%Y%m%d-%H%M%S)"
```

A v0.3.8 continua podendo ler o **antigo** `orcamento.tsv`, mas **não exibe** os novos registros mensais. Evite usar duas versões simultaneamente sobre a mesma pasta de dados.

## Testes

```sh
make test
```

Valida o motor de simulações, persistência dos cenários, orçamento, UTF-8, composição de acentos e o novo armazenamento mensal. A interface também foi testada com X11 virtual: navegação de novembro a janeiro, cópia de planejamento, lançamento de receita realizada e persistência de dados.

Licença MIT. Veja `LICENSE`.
