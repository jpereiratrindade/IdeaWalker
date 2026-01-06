# Idea Walker Architecture (DDD + Ports & Adapters)

This project follows a Domain-Driven Design approach to ensure the core logic (organizing thoughts for ADHD) is decoupled from external technologies (LLMs, File System, UI).

## Layers

### 1. Domain Layer (`src/domain`)
- **Entities**: `Insight` (structured thought), `RawThought` (unprocessed input).
- **Value Objects**: `Actionable` (task items).
- **Ports (Interfaces)**: 
    - `ThoughtRepository`: For persistence.
    - `AIService`: For LLM processing.

### 2. Infrastructure Layer (`src/infrastructure`)
- **Adapters**:
    - `FileRepository`: Implements `ThoughtRepository` using the local filesystem (`/inbox` and `/notas`).
    - `OllamaAdapter`: Implements `AIService` using the local Ollama API (Qwen 2.5).

### 3. Application Layer (`src/application`)
- **Services**:
    - `OrganizerService`: Orchestrates the flow between Domain and Infrastructure.

### 4. Presentation Layer (`src/ui`)
- **UiRenderer**: ImGui screens (Dashboard & Inbox, Organized Knowledge, Execucao).
- **AppState**: UI state, selections, project root and background refresh flags.

### 5. App Layer (`src/app`)
- **IdeaWalkerApp**: App lifecycle, font loading and main loop.

## Data Flow
1. User places a `.txt` in `<project>/inbox`.
2. `OrganizerService` fetches `RawThought` via `FileRepository`.
3. `OrganizerService` sends content to `OllamaAdapter`.
4. `OllamaAdapter` receives structured Markdown from LLM.
5. `OrganizerService` saves the resulting `Insight` into `<project>/notas`.
6. `OrganizerService` updates `_Consolidated_Tasks.md` with unified tasks.
