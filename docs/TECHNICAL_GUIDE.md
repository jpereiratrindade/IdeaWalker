# Guia Técnico de Funcionamento - Idea Walker

Este documento descreve o fluxo de dados, os principais componentes e como o projeto é organizado.

## 1. Fluxo de Execução
1. **Entrada**: arquivos `.txt` são colocados em `<projeto>/inbox/` ou arrastados para a janela.
2. **Leitura**: `FileRepository` converte arquivos em `RawThought`.
3. **Processamento**: `AIProcessingService` orquestra o diagnóstico e transformação via `OllamaClient` e `PersonaOrchestrator`.
4. **Persistência**: o resultado vira `Insight` e é salvo em `<projeto>/notas/` através do `KnowledgeService`.
5. **Consolidação**: o `AIProcessingService` interage com o `KnowledgeService` para atualizar `notas/_Consolidated_Tasks.md`.

## 2. Camada de Domínio (DDD)
- **Insight**: conteúdo estruturado, metadados e actionables.
- **Actionable**: tarefas extraídas dos insights.
- **RawThought**: entrada bruta do inbox.
- **Ports**: `ThoughtRepository` (persistência) e `AIService` (abstração de IA).

## 3. Camada de Aplicação (Service Layer)
- **KnowledgeService**: Gerenciamento puro de conhecimento (CRUD de Notas, Histórico, Backlinks). Isolado de lógica de IA.
- **AIProcessingService**: Orquestrador de pipelines cognitivos. Gerencia o ciclo de vida de tarefas de IA (Inbox, Transcrição, Consolidação).
- **ConversationService**: Gerencia o contexto e histórico do chat com a IA (RAG leve).
- **AsyncTaskManager**: Centralizador de threads. Gerencia execução assíncrona, pools de threads e reporta progresso unificado para a UI.

## 4. Infraestrutura
- **FileRepository**: Implementação concreta de `ThoughtRepository`.
- **OllamaClient**: Cliente HTTP de baixo nível para API do Ollama.
- **PersonaOrchestrator**: Lógica de decisão de qual persona ativar (Brainstormer, Analista, Secretário).
- **WhisperCppAdapter**: Transcrição local de áudio.

## 5. UI (ImGui)
- **AppState**: Decomposto em sub-estados (`project`, `ui`, `neuralWeb`, `external`) para facilitar manutenção.
- **Painéis**: A UI é renderizada por componentes especializados em `src/ui/panels/` (`DashboardPanel`, `KnowledgePanel`, etc.), consumindo serviços injetados.
- **Reatividade**: A UI observa flags atômicas e o `AsyncTaskManager` para atualizações em tempo real sem bloquear a renderização.

## 6. Projetos (Menu File)
- **ProjectService**: Centraliza a lógica de ciclo de vida (Novo/Abrir/Salvar).
- **Navegação**: Modais padronizados via `UiFileBrowser`.

---
*Versão do Documento: v0.1.8-beta*

## Licença

Este projeto é licenciado sob a **GNU General Public License v3.0 (GPLv3)**.
Consulte o arquivo `LICENSE` para mais detalhes.
