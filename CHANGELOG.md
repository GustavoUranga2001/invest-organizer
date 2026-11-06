# Historico de alteracoes

## v0.3.2

- Permite alteracoes **temporarias ou permanentes** das taxas mensais e dos aportes mensais.
- Novo controle "Aportes mensais" na aba Eventos, separado dos aportes extras pontuais.
- Formularios com opcoes "Ate o fim" e "Por alguns meses" e campo de duracao.
- Periodos temporarios retomam a regra anterior ainda vigente ao terminar.
- Regras iniciadas mais recentemente prevalecem quando ha sobreposicao.
- Armazena cenarios no formato INVEST 3, compativel para leitura de INVEST 1 e 2.
- Backup automatico da versao anterior antes da primeira migracao.
- Testes de intervalos inclusivos, sobreposicoes, migracao, formatos e calculos.

## v0.3.1

- Recupera a paleta da v0.2: cabecalho e painel lateral escuros, acoes verdes.
- Restaura as abas superiores Simulador, Eventos e Comparar.
- Mostra a taxa efetiva anual equivalente imediatamente abaixo da mensal.
- Mantem formulario simples para mudancas de juros e aportes extras.
- Preserva os dados no formato INVEST 2, compativel com v0.2 e v0.3.
- Adiciona teste para equivalencia de taxas e melhora a legibilidade do grafico em janelas menores.

## v0.3.0

- Layout com cartoes, eventos editaveis por formularios e comparacao.
