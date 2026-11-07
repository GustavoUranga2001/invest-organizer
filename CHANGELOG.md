# Histórico de versões

## v0.3.9

- Aba Finanças com seleção de mês e ano e valores independentes de **Planejado** e **Realizado**.
- Receitas, despesas e investimentos mensais organizados por período, sem duplicação automática de lançamentos reais.
- Botão para copiar o planejamento do mês anterior para um planejamento vazio.
- Saldo previsto e realizado do mês em destaque, sem misturar o simulador com o orçamento.
- Visão anual com gráficos de saldo, receitas, despesas e investimentos, distinguindo o planejado do realizado.
- Armazenamento em `meses/AAAA-MM.tsv` com gravação atômica e backup `.bak`.
- Importação não destrutiva de `orcamento.tsv` da v0.3.8 para o planejamento do primeiro mês.
- Testes automatizados de períodos, persistência, backups, corrupção e valores financeiros.
- Gráficos antialiasados, acentos, abas e simulações anteriores preservados.

## v0.3.8

- Composição de acentos em campos de texto, incluindo `~` + `a` -> `ã`.
- Suporte a agudo, grave, circunflexo, til, trema, cedilha e caracteres Unicode.
- Composição alternativa quando XIM não estiver disponível, sem dependências adicionais.
- Normalização de sequências combinantes comuns para caracteres precompostos.
- Rótulos da interface com acentos, incluindo Cenários, Evolução e Orçamento.
- Testes de letras minúsculas/maiúsculas, Backspace, XIM, fallback e persistência.
- Formatos de arquivo e cálculos preservados.

## v0.3.7

- Correção de Backspace e Delete com caracteres UTF-8 multibyte.
- Proteção contra textos UTF-8 inválidos durante a renderização com Cairo.
- Recuperação de nomes antigos danificados, preservando os dados financeiros.
- Entrada inicial via XIM / `Xutf8LookupString`.

## v0.3.6

- Validação de nomes vazios sem bloquear a navegação.
- Backup de gravações de cenários em `cenarios.tsv.bak`.
