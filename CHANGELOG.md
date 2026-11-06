# Histórico de versões

## v0.3.3 — Aba Finanças

- Nova aba **FINANÇAS** na barra superior, atalho F4.
- Cadastro de múltiplas receitas mensais e despesas mensais, com edição e exclusão confirmada.
- Classificação de despesas por categoria e por tipo (fixa ou variável).
- Resumo mensal: receitas, despesas, saldo previsto, percentual de renda comprometida e totais fixos/variáveis.
- Arquivo independente `orcamento.tsv` no mesmo diretório XDG de dados, com gravação atômica e validação.
- Preservação integral da aba de Eventos, das regras temporárias/permanentes, da comparação de cenários, da taxa anual equivalente e da paleta anterior.
- Novos testes automatizados para o orçamento: soma, categorias, persistência e rejeição de dados corrompidos.

## v0.3.2

- Aportes mensais e taxas de juros temporários e permanentes.
- Regras sobrepostas e retomada de valores anteriores ainda vigentes.
- Compatibilidade com os cenários das versões anteriores.
