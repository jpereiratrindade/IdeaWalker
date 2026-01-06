# Idea Walker 🚀

### "Transformando o caos do TDAH em concretude técnica."

O **Idea Walker** e um suporte cognitivo projetado para transformar pensamentos nao-lineares e transcricoes de audio em estruturas de conhecimento organizadas (Markdown). Desenvolvido em C++ com uma arquitetura baseada em **Domain-Driven Design (DDD)** e **Ports & Adapters**, ele garante que a logica de organizacao permaneça pura e desacoplada das ferramentas de IA ou de interface.

---

## 🛠️ Tecnologias
- **Linguagem**: C++17
- **Interface**: Dear ImGui (OpenGL3 + SDL2)
- **Cerebro (IA)**: Ollama (**Qwen 2.5:14b** local)
- **Comunicação**: cpp-httplib & nlohmann-json
- **Arquitetura**: DDD (Domain-Driven Design)

---

## 🏗️ Estrutura do Projeto
- `src/app`: Ciclo de vida da aplicacao e carregamento de fontes.
- `src/domain`: Entidades puras e interfaces (Ports).
- `src/infrastructure`: Implementações técnicas (Ollama, FileSystem).
- `src/application`: Orquestração de serviços.
- `src/ui`: Estado e renderizacao da UI (ImGui).
- `inbox/`: Onde as ideias brutas (.txt) entram.
- `notas/`: Onde o conhecimento estruturado (.md) é salvo.
- `docs/`: Documentacao tecnica e arquitetura.

---

## ✨ Funcionalidades
- Ingestao de arquivos `.txt` do **inbox** e geracao de notas em Markdown.
- **Execucao** com Kanban (A Fazer, Em Andamento, Feito) e drag & drop.
- **Consolidacao** de tarefas em `notas/_Consolidated_Tasks.md`.
- **Organized Knowledge** com visao unificada ou por arquivo, com backlinks.
- **Dashboard** com log do sistema e heatmap de atividade.
- **Menu File** com abrir/salvar/fechar projeto, Exit e criacao das pastas necessarias.
- **Navegacao por pastas** integrada nos modais de projeto.

---

## 🚀 Como Rodar
### Pré-requisitos
- **Ollama** rodando localmente (`ollama serve`).
- Modelo **qwen2.5:14b** baixado (`ollama run qwen2.5:14b`).
- **SDL2** instalado no sistema.

### Build
```bash
mkdir build && cd build
cmake ..
make
./IdeaWalker
```

---

## 🛡️ Governança
- **Versao Atual**: v0.1.0-alpha
- **Licença**: MIT
- **Design System**: SisterSTRATA inspired.
- **Recursos**: Brainstorming, Task Extraction, Backlinks e Heatmap de Atividade.

---

*Desenvolvido com curiosidade intelectual e resiliencia cognitiva.*
