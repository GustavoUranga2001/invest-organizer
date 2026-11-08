# Histórico de mudanças

## v0.3.14 — rate.sx para BTC e XMR

- Fonte de BTC e Monero alterada para `rate.sx` (tabela de texto, sem JSON).
- Preferência por `brl.rate.sx`, com validação explícita da moeda BRL.
- Alternativa: tabela USD do `rate.sx` multiplicada pela PTAX USD/BRL consultada na mesma atualização.
- Uma única consulta fornece ambas as criptomoedas em cada tentativa.
- Nenhum dado estimado é gravado se a moeda não puder ser identificada corretamente.
- Cache offline, backups, atualização parcial e as quatro abas preservados.


## v0.3.13 — diagnóstico do módulo Mercado

- Identifica separadamente: falta de `curl`, erros de DNS, falhas TLS,
  erros HTTP, limites de tempo, conexão e incompatibilidade de JSON.
- Exibe um resumo no rodapé da GUI em vez do aviso genérico.
- Adiciona o executável `invest-market-check` para diagnosticar cada fonte.
- Preserva as cotações salvas e aplica atualizações parciais.
- Persiste apenas o diagnóstico da última tentativa, sem dados privados.
- Acrescenta testes de integração com falhas simuladas de DNS e HTTP.
- Mantém dados, visual, quatro abas, orçamento e fórmulas da v0.3.12.

# Histórico de versões

## 0.3.12 — Indicadores de mercado

- Selic-meta, PTAX USD/BRL e EUR/BRL, BTC/BRL e XMR/BRL na lateral.
- Atualização pontual por HTTPS via `curl`, em processo separado, sem bloquear a janela.
- Cache offline em `mercado.tsv`, com timestamps por indicador e backups.
- Leitura de JSON restrita às chaves necessárias; rejeição de valores inválidos.
- Testes de parsing, arquivo inválido e atualização completa simulada.
- Interface Dark Elegant e os dados/cálculos financeiros permanecem intactos.


## 0.3.11 — Estabilidade e proteção dos dados

- Escrita atômica e sincronização de disco em cenários, orçamento e meses.
- Backup rotativo `.bak` de versões válidas; nunca substituído por dados corrompidos.
- `invest-doctor check` para validação sem modificar arquivos.
- `invest-doctor recover` para recuperação explícita, preservando `.corrupt.*`.
- Bloqueio de instância para prevenir gravações simultâneas.
- Testes de regressão de corrupção, recuperação, acentos e cálculos.
- Interface Dark Elegant, quatro abas, desempenho e formatos TSV preservados.

## 0.3.10 — Dark Elegant / otimizações para ThinkPad X200

- Novo tema escuro (grafite-azulado, verde suave) para todas as quatro abas,
  com contraste de texto corrigido também na barra lateral.
- Abas superiores e fluxo de trabalho preservados.
- Layout do simulador compactado para acomodar gráfico e resultados no painel
  1280×800 do ThinkPad X200, mantendo a taxa equivalente anual.
- Superfície antialiasada em `GOOD`, sem efeitos de blur ou animação.
- Cache de cores RGB e seleção de fontes repetidas.
- Repintura X11 otimizada: `Expose` usa cópia da área afetada do backbuffer.
- Cache de dados do gráfico anual, invalidado ao navegar ou alterar orçamento.
- Eventos de redimensionamento agrupados para reduzir a realocação de pixmaps.
- Compilação incremental via `make`.
- Formatos de dados preservados, sem migração.

## 0.3.9

- Orçamento por mês, planejado versus realizado, cópia de planejamento e
  gráfico de evolução anual.

## 0.3.8

- Tratamento de UTF-8, teclas mortas e combinação de acentos.
