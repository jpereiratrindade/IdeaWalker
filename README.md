# Idea Walker 🚀

### "Transformando o caos do TDAH em concretude técnica."

O **Idea Walker** é um suporte cognitivo projetado para transformar pensamentos não-lineares e transcrições de áudio em estruturas de conhecimento organizadas (Markdown). Desenvolvido em C++ com uma arquitetura baseada em **Domain-Driven Design (DDD)** e **Service Layer Isolation**, ele garante que a lógica de organização permaneça pura, modular e desacoplada das ferramentas de IA ou de interface.

---

## 🛠️ Tecnologias
- **Linguagem**: C++17
- **Interface**: Dear ImGui (OpenGL3 + SDL2)
- **Cérebro (IA)**: Ollama (**Qwen 2.5:14b** local)
- **Comunicação**: cpp-httplib & nlohmann-json
- **Arquitetura**: DDD (Services: Knowledge, AIProcessing, Conversation) + Async Task Manager
- **Infraestrutura**: Ports & Adapters

---

## 🏗️ Estrutura do Projeto
- `src/app`: Ciclo de vida da aplicação e carregamento de fontes.
- `src/domain`: Entidades puras e interfaces (Ports).
- `src/infrastructure`: Implementações técnicas (OllamaClient, FileSystem, Whisper).
- `src/application`: Serviços de Domínio (KnowledgeService, AIProcessingService, ConversationService).
- `src/ui`: Estado e renderização da UI (ImGui Panels).
- `inbox/`: Onde as ideias brutas (.txt) entram.
- `inbox/scientific/`: Inbox científica para artigos e PDFs.
- `notas/`: Onde o conhecimento estruturado (.md) é salvo.
- `observations/scientific/`: Bundles científicos brutos para auditoria.
- `strata/consumables/`: Consumíveis estáveis exportados para o STRATA.
- `docs/`: Documentação técnica e arquitetura.

---

### Fluxo de Trabalho (Workflow)
1.  **Ingestão**: Jogue arquivos de texto, áudio ou PDFs na pasta `inbox/`.
2.  **Orquestração Autônoma**: O sistema utiliza o `AIProcessingService` para diagnosticar o estado cognitivo e aplicar a sequência correta de personas via `PersonaOrchestrator`.
3.  **Processamento Assíncrono**: Transcrições e indexações ocorrem em segundo plano, gerenciadas pelo `AsyncTaskManager`, sem travar a UI.
4.  **Refinamento**: O output é salvo como Markdown estruturado na pasta `notas/` via `KnowledgeService`.
5.  **Ação**: Tarefas são extraídas e consolidadas em `_Consolidated_Tasks.md`.

## ✨ Funcionalidades
- **📂 Suporte Multi-formato**: Ingestão automática de `.txt`, `.pdf` (via pdftotext), `.md` e `.tex` do **inbox**.
- **🧪 Ingestão Científica Governada**: Gera artefatos cognitivos tipados e exporta consumíveis STRATA (sem normatividade).
- **📝 DocOps (Document Workbench)**: Executa checks/releases em workspaces documentais (ex.: ontologias/relatórios) com logs integrados e rastreáveis.
- **✅ Validador Epistemológico (VE-IW)**: Gate obrigatório antes de exportar para STRATA, com relatório e selo de exportação.
- **🧠 Orquestração Cognitiva Autônoma**: O sistema diagnostica o estado do texto e aplica automaticamente a sequência ideal de personas (Brainstormer, Analista, Secretário).
- **🕸️ Neural Web & Mind Map**: Visualização interativa de conexões entre notas, tarefas e conceitos.
- **✨ Ressonância Semântica**: Motor de sugestão que identifica conexões automáticas entre notas baseado em similaridade vetorial (Embeddings).
- **Static Preview**: Visualização estável e organizada de gráficos Mermaid.
- **🚀 Gestão de Execução**: Kanban board e lista de tarefas consolidadas sincronizados via IA.
- **📤 Exportação Flexível**: Geração de diagramas e relatórios completos para Obsidian/GitHub.
- **🎙️ Captura de Áudio Offline**: Transcrição local de voz (Whisper.cpp) gerenciada por tarefas assíncronas.
- **📜 Licença GPLv3**: Software livre e de código aberto.
- **Menu File & Navegação**: Gestão completa de projetos e navegação integrada por pastas.
- **Troca de Projeto Confiável**: ao abrir um novo projeto, os serviços são reconfigurados para usar o novo `root` (inbox/notas/histórico).
- **🖊️ Trajetórias de Escrita**: Ambiente focado em intenção e revisão para escrita longa (DDD + Event Sourcing). Inclui **Editor de Segmentos**, **Modo de Defesa** com IA e verificação de coerência. [Detalhes da Implementação](docs/WRITING_TRAJECTORY_IMPLEMENTATION.md)

---

## 🧭 Governança ADR
- **Registro canônico ADR**: [adr/README.md](adr/README.md)
- **Índice normativo manual**: [adr/ADR_INDEX.md](adr/ADR_INDEX.md)
- **Índice arquitetural gerado**: `reports/architecture/ArchitectureDecisionIndex.latest.json` e `.md`
- **Comandos de governança**:
  - `bash scripts/audit_adr_index.sh`
  - `python3 scripts/build_adr_catalog.py`

---

## 🚀 Como Rodar
### Pré-requisitos
- **Ollama** rodando localmente (`ollama serve`).
- Modelo **qwen2.5:14b** baixado (`ollama run qwen2.5:14b`).
- **SDL2** instalado no sistema.
- **FFmpeg** instalado (para conversão de áudio MP3/M4A).

### Build
```bash
mkdir build && cd build
cmake ..
make
./IdeaWalker
```

### Configuração de Driver de Vídeo (Linux/Wayland)
Se você encontrar erros como `Wayland display connection closed`, você pode forçar o back-end X11 editando o arquivo `settings.json` na raiz do projeto:
```json
{
    "video_driver": "x11"
}
```
Isso evita a necessidade de alterar variáveis de ambiente a cada execução.

---

## 🎙️ Transcrição de Áudio
O IdeaWalker suporta transcrição local offline usando **Whisper.cpp**.
- **Modelos**: Baixa automaticamente o modelo `ggml-base.bin` (~140MB) na primeira execução.
- **Como usar**:
    1. **Drag & Drop**: Arraste um arquivo de áudio para a janela.
    2. **Menu**: Arquivo > Transcrever Áudio... (Executado em segundo plano via `AsyncTaskManager`).

---

## 🛡️ Governança
- **Versão Atual**: v0.1.17-beta (em hardening F1)
- **Licença**: GPLv3
- **Design System**: SisterSTRATA inspired.
- **Recursos**: Brainstorming, Task Extraction, Backlinks, Heatmap e Chat Contextual (ConversationService).

---

*Desenvolvido com curiosidade intelectual e resiliência cognitiva.*
