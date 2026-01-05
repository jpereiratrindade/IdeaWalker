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
- `src/domain`: Entidades puras e interfaces (Ports).
- `src/infrastructure`: Implementações técnicas (Ollama, FileSystem).
- `src/application`: Orquestração de serviços.
- `inbox/`: Onde as ideias brutas (.txt) entram.
- `notas/`: Onde o conhecimento estruturado (.md) é salvo.

---

## 🚀 Como Rodar
### Pré-requisitos
- **Ollama** rodando localmente (`ollama serve`).
- Modelo **qwen2.5:7b** baixado (`ollama run qwen2.5:7b`).
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
- **Versão Atual**: v0.1.0-alpha
- **Licença**: MIT
- **Design System**: SisterSTRATA inspired.

---

*Desenvolvido com curiosidade intelectual e resiliência cognitiva.*
