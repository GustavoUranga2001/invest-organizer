# Changelog

## 0.3.6
- Corrigido o fluxo de edição de nome de cenário: nomes vazios ou inválidos
  são descartados, preservando os parâmetros e permitindo continuar a navegação.
- Corrigida a seleção de cenário para respeitar uma edição pendente inválida.
- Criado backup atômico da última versão válida de `cenarios.tsv` antes de cada
  gravação, em `cenarios.tsv.bak`.
- Impedida a substituição de arquivo anterior inválido, preservando o original.
- Acrescentados testes de regressão para nomes vazios e backups.
- Interface, gráficos, cálculos e dados da 0.3.5 permanecem compatíveis.

## 0.3.5
- Fontes e gráficos antialiasados com Cairo, com a mesma paleta e quatro abas.
