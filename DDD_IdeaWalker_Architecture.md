# Arquitetura do Idea Walker (DDD + Ports & Adapters)

Este projeto segue uma abordagem de Design Orientado a Domínio (DDD) para garantir que a lógica central (organização de pensamentos para TDAH) esteja desacoplada de tecnologias externas (LLMs, Sistema de Arquivos, UI).

## Camadas

### 1. Camada de Domínio (`src/domain`)
- **Entidades**: `Insight` (pensamento estruturado), `RawThought` (entrada não processada).
- **Objetos de Valor**: `Actionable` (itens de tarefa).
- **Ports (Interfaces)**: 
    - `ThoughtRepository`: Para persistência.
    - `AIService`: Para processamento por IA.

### 2. Camada de Infraestrutura (`src/infrastructure`)
- **Adapters**:
    - `FileRepository`: Implementa `ThoughtRepository` usando o sistema de arquivos local (`/inbox` e `/notas`).
    - `OllamaAdapter`: Implementa `AIService` usando a API local do Ollama (Qwen 2.5).

### 3. Camada de Aplicação (`src/application`)
- **Serviços**: Lógicas de orquestração específicas (ex: KnowledgeService, IngestionService).
- **AppController**: Controlador central (MVC/MVVM) que delega ações da UI para os serviços e gerencia o ciclo de vida do projeto.

### 4. Camada de Apresentação (`src/ui`)
- **UiRenderer**: Telas ImGui divididas em múltiplos painéis modulares (Dashboard, Knowledge, DocOps, etc).
- **ViewModel**: Estrutura de dados com *binding* puramente visual, contendo estado da interface, seleções, buffers e variáveis *thread-safe* para atualização assíncrona.

### 5. Camada de App (`src/app`)
- **IdeaWalkerApp**: Ciclo de vida do app, carregamento de fontes e loop principal.

## Fluxo de Dados
1. O usuário coloca um arquivo `.txt` em `<projeto>/inbox`.
2. O `OrganizerService` busca o `RawThought` via `FileRepository`.
3. O `OrganizerService` envia o conteúdo para o `OllamaAdapter`.
4. O `OllamaAdapter` recebe o Markdown estruturado da IA.
5. O `OrganizerService` salva o `Insight` resultante em `<projeto>/notas`.
6. O `OrganizerService` atualiza o `_Consolidated_Tasks.md` com as tarefas unificadas.

## Licença

Este projeto é licenciado sob a **GNU General Public License v3.0 (GPLv3)**.
Consulte o arquivo `LICENSE` para mais detalhes.
