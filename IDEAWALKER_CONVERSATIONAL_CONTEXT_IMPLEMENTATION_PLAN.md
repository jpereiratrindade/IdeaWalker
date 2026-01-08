================================================================================
IDEAWALKER — COGNITIVE DIALOGUE CONTEXT
Plano Final de Implementação (Codex + Gemini)
================================================================================

VERSÃO: 1.0
STATUS: APROVADO PARA IMPLEMENTAÇÃO
NATUREZA: Observacional / Dialógica / Persistente
ESCOPO: Contexto Auxiliar (NÃO Core Domain)
MODELO DE IA: Qwen (via Ollama /api/chat)
================================================================================


1. OBJETIVO
--------------------------------------------------------------------------------
Implementar um Contexto de Diálogo Cognitivo que permita ao usuário conversar
com o estado atual do projeto, utilizando a IA como interlocutora contextualizada.

O sistema deve:
- Apoiar integração, síntese e continuidade cognitiva
- Manter persistência temporal do diálogo
- Observar o estado do projeto sem modificá-lo
- Evitar geração dispersiva de novas ideias


2. PRINCÍPIO FUNDAMENTAL
--------------------------------------------------------------------------------
Este módulo NÃO é um chat genérico.

Regra central:
"O diálogo sempre parte do que já existe no projeto."

A IA:
- NÃO decide
- NÃO cria estrutura de domínio
- NÃO altera notas, tarefas ou grafos

A IA:
- Observa
- Reflete
- Auxilia síntese e integração


3. POSICIONAMENTO ARQUITETURAL (DDD)
--------------------------------------------------------------------------------
Classificação:
- Contexto Observacional
- Read-only sobre o Core Domain
- Sem autoridade decisória

Nome do contexto:
Cognitive Dialogue Context

Camadas envolvidas:
- Infrastructure Layer: integração com Ollama (Qwen)
- Application Layer: gestão de sessão e persistência
- UI Layer: painel de diálogo


4. ESCOPO DO MVP (FASE 1)
--------------------------------------------------------------------------------
Incluído:
- Painel de diálogo acoplável (DockSpace)
- Contexto mínimo: nota ativa (título + conteúdo)
- Histórico de mensagens visível
- Campo de entrada de texto
- Persistência em arquivos .md
- Uma sessão por nota ativa

Explicitamente fora do escopo:
- Backlinks
- Tarefas
- Decisões automáticas
- Modificação do domínio


5. INFRASTRUCTURE LAYER
--------------------------------------------------------------------------------
Arquivo a modificar:
- OllamaAdapter.hpp / OllamaAdapter.cpp

Responsabilidades:
- Implementar suporte ao endpoint /api/chat
- Gerenciar troca de mensagens com o modelo Qwen

Estrutura de mensagem:
- ChatMessage
  - Role: enum { System, User, Assistant }
  - Content: string

Método principal:
- chat(std::vector<ChatMessage>& history, bool stream = false)

Regras:
- Formatação correta do JSON para Ollama
- Suporte a histórico completo da conversa
- Nenhuma lógica de decisão ou persistência aqui


6. APPLICATION LAYER
--------------------------------------------------------------------------------
Novos arquivos:
- ConversationService.hpp
- ConversationService.cpp

Responsabilidades:
- Gerenciar ConversationSession (histórico de mensagens)
- Injetar contexto da nota ativa como System Message
- Orquestrar chamadas ao OllamaAdapter
- Gerenciar persistência do diálogo

Regra de injeção de contexto (System Prompt):
- Identificar o diálogo como Observacional
- Limitar a IA ao conteúdo da nota ativa
- Proibir introdução de novos tópicos

Formato conceitual do System Prompt:
"You are participating in a Cognitive Dialogue Context.
You may only reason based on the active note below.
Your role is to help with synthesis and integration, not creation."

Persistência:
- Diretório: /<project>/dialogues/
- Nome do arquivo:
  YYYY-MM-DD_HH-MM-SS_{NoteId}.md


7. FORMATO DO ARQUIVO DE DIÁLOGO (.md)
--------------------------------------------------------------------------------
Estrutura mínima:

# Conversa do Projeto

Data: YYYY-MM-DD HH:MM
Nota ativa: <Título da Nota>
NoteId: <ID da Nota>

---

## Diálogo

**Usuário:** ...
**IA:** ...

Regras:
- Ordem cronológica
- Sem edição retroativa
- Legível fora do sistema


8. UI LAYER
--------------------------------------------------------------------------------
Novo componente:
- ConversationPanel

Características:
- Painel acoplável via DockSpace
- Exibido simultaneamente à nota ativa
- Área rolável de histórico
- Campo de entrada para nova mensagem
- Indicador visível da nota em contexto

Modificações:
- UiRenderer.hpp / UiRenderer.cpp
- Registro e desenho do painel


9. FLUXO DE DADOS
--------------------------------------------------------------------------------
1. Usuário abre uma nota
2. ConversationPanel detecta nota ativa
3. ConversationService cria ou retoma sessão
4. System Prompt é injetado com o conteúdo da nota
5. Usuário envia mensagem
6. OllamaAdapter executa /api/chat
7. Resposta da IA é adicionada à sessão
8. Conversa é persistida em .md


10. VERIFICAÇÃO (MVP)
--------------------------------------------------------------------------------
Testes manuais obrigatórios:
- Chat responde usando o conteúdo da nota ativa
- Mudança de nota inicia nova sessão
- Arquivo .md é criado corretamente
- Histórico persiste após reiniciar o app
- Não há contaminação entre notas diferentes


11. RISCOS E MITIGAÇÕES
--------------------------------------------------------------------------------
Risco:
- Sistema virar chat dispersivo

Mitigação:
- System Prompt restritivo
- Contexto limitado à nota ativa
- Escopo MVP reduzido

Risco:
- Crescimento descontrolado do módulo

Mitigação:
- Contexto Observacional explícito
- Evolução apenas por fases documentadas


12. CONSIDERAÇÃO FINAL
--------------------------------------------------------------------------------
Este plano implementa um companheiro de diálogo cognitivo que respeita os
princípios do IdeaWalker: clareza, continuidade, memória e responsabilidade
epistemológica.

O sistema amplia a capacidade reflexiva do usuário sem substituir seu
julgamento nem fragmentar o domínio.

================================================================================
FIM DO DOCUMENTO
================================================================================
