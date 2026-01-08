===============================================================================
DDD – CONTEXTUAL RAG & KNOWLEDGE ASSEMBLY
Projeto: IdeaWalker
Versão: 1.0
Status: Proposta Arquitetural Fundacional
Formato: TXT (Markdown compatível)
===============================================================================


1. VISÃO GERAL
-------------------------------------------------------------------------------

O IdeaWalker implementa um modelo de RAG local, determinístico e orientado a
domínio, no qual o modelo de linguagem (Qwen via Ollama) não atua como agente
autônomo, mas como um componente de interpretação semântica operando sobre
contextos explicitamente estruturados.

Este documento define o Contextual RAG do sistema sob a ótica de
Domain-Driven Design (DDD), delimitando responsabilidades, fronteiras
ontológicas e contratos semânticos.


2. PRINCÍPIOS ARQUITETURAIS
-------------------------------------------------------------------------------

1. O modelo não navega o domínio
2. O modelo não decide contexto
3. O modelo não persiste estado
4. Todo contexto enviado ao modelo é:
   - explícito
   - tipado
   - rastreável
5. Contextos observacionais não interferem no Core Domain
6. Recuperação sempre precede geração (R → G)


3. BOUNDED CONTEXTS ENVOLVIDOS
-------------------------------------------------------------------------------

3.1 Core Domain — Knowledge Organization

Responsável por:
- Organização de ideias
- Estruturação de notas
- Gestão de tarefas
- Estado ativo do usuário

Não depende de IA.


3.2 Supporting Context — Contextual RAG Assembly

Responsável por:
- Selecionar contextos
- Compor pacotes semânticos
- Rotular informações
- Entregar contexto ao modelo

Este contexto orquestra o RAG, mas não interpreta semanticamente o conteúdo.


3.3 External System — LLM (Qwen via Ollama)

Responsável por:
- Interpretação semântica
- Transformação textual
- Sugestões estruturadas

Não possui memória de domínio.

3.4 Ingest Context — Content Normalization

Responsável por:
- Receber conteúdos em múltiplos formatos (PDF, DOCX, TEX, áudio)
- Extrair texto bruto
- Normalizar para Markdown canônico

Este contexto:
- Não interpreta semanticamente
- Não organiza ideias
- Não cria estrutura de domínio

Função exclusiva:
Transformação de formato → texto estruturável

4. CONTEXTOS SEMÂNTICOS (CONTEXT PROVIDERS)
-------------------------------------------------------------------------------

4.1 Active Note Context

Fonte: notas/*.md
Tipo: Read / Write
Função: Foco cognitivo atual
Escopo: Curto prazo


4.2 Backlink Context

Fonte: Varredura estrutural do projeto
Tipo: Read-only
Função: Associatividade e memória lateral
Escopo: Médio prazo


4.3 Task Context

Fonte: Checkboxes Markdown
Tipo: Read / Write
Função: Execução e concretude
Escopo: Operacional


4.4 Narrative Observation Context

Fonte: Entrevistas, documentos, relatos
Tipo: Read-only
Função: Interpretação e sentido
Escopo: Observacional

Restrições:
- Não possui causalidade
- Não interfere em simulação
- Não interfere em decisão


4.5 Unified Knowledge Context (Experimental)

Fonte: Agregação de múltiplos contextos
Tipo: Read-only
Risco: Estouro de contexto e perda de foco
Uso: Opcional, com alerta explícito ao usuário


5. STRUCTURED RAG — CONTRATO SEMÂNTICO
-------------------------------------------------------------------------------

Todo envio de contexto ao modelo segue uma estrutura semântica explícita.

Exemplo conceitual:

=== ACTIVE_NOTE ===
(conteúdo da nota ativa)

=== BACKLINKS ===
(lista de notas relacionadas)

=== TASK_STATE ===
(itens de execução)

=== NARRATIVE_OBSERVATION (READ-ONLY) ===
(texto observacional)

Regras:
- Nenhum bloco é implícito
- Nenhum texto é enviado sem rótulo
- Ordem e papéis importam


6. APPLICATION LAYER — CONTEXT ASSEMBLER
-------------------------------------------------------------------------------

6.1 Responsabilidade

O ContextAssembler é responsável por:
- Receber a intenção do usuário
- Selecionar contextos adequados
- Montar o pacote semântico
- Entregar o contexto ao AIService


6.2 Interface Conceitual

ContextBundle assemble(
    Intent intent,
    SessionState session
);

O ContextBundle é um Value Object imutável.


7. PAPEL DO MODELO (QWEN)
-------------------------------------------------------------------------------

O modelo Qwen atua como:
- Intérprete semântico
- Transformador textual
- Assistente de organização

O modelo não:
- Seleciona contexto
- Acessa filesystem
- Decide prioridade
- Persiste estado


8. RELAÇÃO COM RAG TRADICIONAL
-------------------------------------------------------------------------------

RAG Tradicional:
- Recuperação vetorial
- Texto bruto no prompt
- Controle implícito do modelo
- Debug difícil

IdeaWalker:
- Recuperação estrutural
- Contexto tipado
- Controle explícito do domínio
- Debug direto via arquivos


9. EVOLUÇÃO PLANEJADA (NÃO OBRIGATÓRIA)
-------------------------------------------------------------------------------

1. Structured RAG explícito (estado atual)
2. Recuperação híbrida (estrutura + ranking)
3. Vetores apenas como índice auxiliar
4. Interface mostrando contexto utilizado


10. ANTI-OBJETIVOS (O QUE NÃO FAZER)
-------------------------------------------------------------------------------

- Não criar agentes autônomos
- Não permitir escrita indireta pelo modelo
- Não misturar narrativa com simulação
- Não esconder contexto do usuário


11. ENCERRAMENTO
-------------------------------------------------------------------------------

O Contextual RAG do IdeaWalker não é uma técnica de IA,
mas uma decisão arquitetural cognitiva.

O modelo serve ao domínio.
Nunca o contrário.

===============================================================================
