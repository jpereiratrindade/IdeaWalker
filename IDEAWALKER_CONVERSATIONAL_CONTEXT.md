================================================================================
IDEAWALKER — CONTEXTUAL COGNITIVE DIALOGUE
Proposta de Integração de Chat Persistente com Qwen
================================================================================

VERSÃO: 0.1 (Proposta conceitual)
STATUS: Em avaliação
NATUREZA: Observacional / Dialógica / Persistente
ESCOPO: Contexto Auxiliar (NÃO Core Domain)
MODELO DE IA: Qwen (via Ollama)
================================================================================


1. MOTIVAÇÃO
--------------------------------------------------------------------------------
Durante o desenvolvimento do IdeaWalker, emerge de forma recorrente a dificuldade
de integrar múltiplas ideias que, embora façam sentido de maneira intuitiva,
perdem coerência no momento de sua externalização e organização formal.

A proposta deste contexto é criar um espaço dialógico persistente que permita
ao usuário conversar com o próprio estado do projeto, utilizando a IA como
interlocutora contextualizada — não como agente decisório.

O objetivo não é gerar mais ideias, mas favorecer síntese, integração e
continuidade cognitiva ao longo do tempo.


2. PRINCÍPIO FUNDAMENTAL
--------------------------------------------------------------------------------
Não se trata de "adicionar um chat com IA".

Trata-se de introduzir um CONTEXTO DE DIÁLOGO COGNITIVO que:
- Observa o estado atual do projeto
- Mantém memória ao longo do tempo
- Dialoga a partir do que já existe
- Apoia processos de integração e fechamento

Regra central:
"O diálogo sempre aponta para o que já existe, nunca para o vazio."


3. POSICIONAMENTO ARQUITETURAL (DDD)
--------------------------------------------------------------------------------
Este contexto NÃO pertence ao Core Domain.

Classificação:
- Contexto Observacional
- Natureza Read-only sobre o estado do projeto
- Sem autoridade decisória ou modificadora do domínio

Nome sugerido:
- Cognitive Dialogue Context
ou
- Contextual Cognitive Dialogue

Integração arquitetural:
- Application Layer: orquestração
- Infrastructure Layer: adapter para Qwen/Ollama
- UI Layer: painel de diálogo dedicado


4. PAPEL DA IA (Qwen)
--------------------------------------------------------------------------------
Qwen atua exclusivamente como motor linguístico.

A IA:
- NÃO toma decisões
- NÃO cria estrutura de domínio
- NÃO altera diretamente notas, tarefas ou grafos

A IA:
- Reflete sobre o estado atual
- Ajuda a identificar tensões, padrões e recorrências
- Apoia processos de síntese e articulação conceitual

Modelo de interação:
IA como interlocutora cognitiva, não como arquiteta do sistema.


5. INTERFACE (UI)
--------------------------------------------------------------------------------
Criação de um painel dedicado de diálogo no ImGui:

Exemplo conceitual:

┌────────────────────────────────────────┐
│ 🧠 Conversa do Projeto                  │
├────────────────────────────────────────┤
│ Histórico contextual do diálogo         │
│                                        │
│ Usuário: dificuldade de integração     │
│ IA: padrões recorrentes observados     │
│                                        │
├────────────────────────────────────────┤
│ > Entrada de texto                      │
└────────────────────────────────────────┘

Características:
- Painel acoplável (DockSpace)
- Contextualizado à nota ativa
- Histórico visível e navegável


6. PERSISTÊNCIA TEMPORAL (ELEMENTO-CHAVE)
--------------------------------------------------------------------------------
Cada sessão de diálogo deve ser persistida no projeto.

Sugestão de armazenamento:
- Diretório: /dialogues/
- Formato: .md ou .json

Indexação mínima:
- Data e hora
- Nota ativa no momento do diálogo
- Tags semânticas (ex: integração, travamento, síntese)

Objetivo:
- Combater cegueira temporal
- Evitar perda de reflexões já realizadas
- Permitir retomada cognitiva ao longo do tempo


7. INTEGRAÇÃO COM O ESTADO DO PROJETO
--------------------------------------------------------------------------------
O diálogo pode observar:
- Nota ativa
- Backlinks
- Tarefas abertas
- Histórico recente de atividades
- Trajetória temporal (quando disponível)

O diálogo NÃO modifica diretamente esses elementos.


8. RISCOS IDENTIFICADOS
--------------------------------------------------------------------------------
Se mal implementado, o sistema pode:
- Virar um chat genérico
- Estimular dispersão
- Criar mais ideias soltas sem fechamento

Mitigação:
- Prompts sempre ancorados no estado atual
- Ênfase em síntese, integração e continuidade
- Evitar geração excessiva de novos tópicos


9. ROADMAP SUGERIDO
--------------------------------------------------------------------------------
Fase 1 — MVP:
- Painel de diálogo
- Persistência simples
- Contexto limitado à nota ativa

Fase 2 — Integração:
- Leitura de backlinks
- Referência a tarefas abertas
- Resumos automáticos do diálogo

Fase 3 — Expansão:
- Diálogos como elementos conectáveis no grafo
- Conversas integradas à Neural Web


10. CONSIDERAÇÃO FINAL
--------------------------------------------------------------------------------
Este contexto amplia o IdeaWalker de um organizador de ideias para um
companheiro de travessia cognitiva, mantendo fidelidade aos princípios de
DDD, clareza arquitetural e responsabilidade epistemológica.

================================================================================
FIM DO DOCUMENTO
================================================================================
