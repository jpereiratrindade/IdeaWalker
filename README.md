# Idea Walker 🚀

### "Transformando o caos do TDAH em concretude técnica."

O **Idea Walker** é um suporte cognitivo projetado para transformar pensamentos não-lineares e transcrições de áudio em estruturas de conhecimento organizadas (Markdown). Desenvolvido em C++ com uma arquitetura baseada em **Domain-Driven Design (DDD)** e **Ports & Adapters**, ele garante que a lógica de organização permaneça pura e desacoplada das ferramentas de IA ou de interface.

---

## 🛠️ Tecnologias
- **Linguagem**: C++17
- **Interface**: Dear ImGui (OpenGL3 + SDL2)
- **Cérebro (IA)**: Ollama (**Qwen 2.5:14b** local)
- **Comunicação**: cpp-httplib & nlohmann-json
- **Arquitetura**: DDD (Domain-Driven Design)

---

## 🏗️ Estrutura do Projeto
- `src/app`: Ciclo de vida da aplicação e carregamento de fontes.
- `src/domain`: Entidades puras e interfaces (Ports).
- `src/infrastructure`: Implementações técnicas (Ollama, FileSystem).
- `src/application`: Orquestração de serviços.
- `src/ui`: Estado e renderização da UI (ImGui).
- `inbox/`: Onde as ideias brutas (.txt) entram.
- `notas/`: Onde o conhecimento estruturado (.md) é salvo.
- `docs/`: Documentação técnica e arquitetura.

---

### Fluxo de Trabalho (Workflow)
1.  **Ingestão**: Jogue arquivos de texto, áudio ou PDFs na pasta `inbox/`.
2.  **Orquestração Autônoma**: O sistema detecta novos arquivos, diagnostica o estado cognitivo (Caótico, Divergente, etc.) e aplica automaticamente a sequência correta de personas (Brainstormer, Analista, Secretário).
3.  **Refinamento**: O output é salvo como Markdown estruturado na pasta `knowledge/`.
4.  **Ação**: Tarefas são extraídas e consolidadas em `_Consolidated_Tasks.md`.

## ✨ Funcionalidades
- **📂 Suporte Multi-formato**: Ingestão automática de `.txt`, `.pdf` (via pdftotext), `.md` e `.tex` do **inbox**.
- **🧠 Orquestração Cognitiva Autônoma**: O sistema diagnostica o estado do texto e aplica automaticamente a sequência ideal de personas (Brainstormer, Analista, Secretário).
- **🕸️ Neural Web & Mind Map**: Visualização interativa de conexões entre notas, tarefas e conceitos.
- **✨ Ressonância Semântica**: Motor de sugestão que identifica conexões automáticas entre notas baseado em similaridade vetorial (Embeddings).
- **Static Preview**: Visualização estável e organizada de gráficos Mermaid.
- **🚀 Gestão de Execução**: Kanban board e lista de tarefas consolidadas sincronizados via IA.
- **📤 Exportação Flexível**: Geração de diagramas e relatórios completos para Obsidian/GitHub.
- **🎙️ Captura de Áudio**: Transcrição local de voz para insights estruturados.
- **📜 Licença GPLv3**: Software livre e de código aberto.
- **Menu File & Navegação**: Gestão completa de projetos e navegação integrada por pastas.
- **🖊️ Trajetórias de Escrita**: Ambiente focado em intenção e revisão para escrita longa (DDD + Event Sourcing). Inclui **Editor de Segmentos** com rastreabilidade, **Modo de Defesa** com IA e verificação de coerência. [Detalhes da Implementação](docs/WRITING_TRAJECTORY_IMPLEMENTATION.md)

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

---

## 🛡️ Governança
- **Versão Atual**: v0.1.5-beta
- **Licença**: GPLv3
- **Design System**: SisterSTRATA inspired.
- **Recursos**: Brainstorming, Task Extraction, Backlinks e Heatmap de Atividade.

---

*Desenvolvido com curiosidade intelectual e resiliência cognitiva.*
