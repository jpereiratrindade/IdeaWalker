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
- **Serviços**:
    - `OrganizerService`: Orquestra o fluxo entre o Domínio e a Infraestrutura.

### 4. Camada de Apresentação (`src/ui`)
- **UiRenderer**: Telas ImGui (Dashboard & Inbox, Organized Knowledge, Execução).
- **AppState**: Estado da UI, seleções, raiz do projeto e flags de atualização em segundo plano.

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
