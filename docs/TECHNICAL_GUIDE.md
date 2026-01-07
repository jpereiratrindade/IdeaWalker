# Guia Técnico de Funcionamento - Idea Walker

Este documento descreve o fluxo de dados, os principais componentes e como o projeto é organizado.

## 1. Fluxo de Execução
1. **Entrada**: arquivos `.txt` são colocados em `<projeto>/inbox/`.
2. **Leitura**: `FileRepository` converte arquivos em `RawThought`.
3. **Processamento**: `OrganizerService` chama o `AIService` (`OllamaAdapter`) para gerar Markdown estruturado.
4. **Persistência**: o resultado vira `Insight` e é salvo em `<projeto>/notas/`.
5. **Consolidação**: o serviço atualiza `notas/_Consolidated_Tasks.md` com tarefas unificadas e sem duplicatas.

## 2. Camada de Domínio (DDD)
- **Insight**: conteúdo estruturado, metadados e actionables.
- **Actionable**: tarefas extraídas dos insights.
- **RawThought**: entrada bruta do inbox.
- **Ports**: `ThoughtRepository` (persistência) e `AIService` (IA).

## 3. Camada de Aplicação
- **OrganizerService**: orquestra processamento, atualização de tarefas consolidadas e mudança de status das tarefas.

## 4. Infraestrutura
- **FileRepository**: leitura e escrita de arquivos, backlinks e histórico de atividade.
- **OllamaAdapter**: integração com a API do Ollama usando o modelo `qwen2.5:14b`. Veja as [Diretrizes de Prompt](LLM_PROMPT_GUIDELINES.md) para detalhes da interpretação.

## 5. UI (ImGui)
- **AppState**: estado da UI, seleções, logs, visão unificada e projeto ativo.
- **UiRenderer**: renderiza Dashboard & Inbox, Organized Knowledge e Execução (Kanban).
- **Processamento assíncrono**: threads de IA atualizam `pendingRefresh` para recarregar a UI.

## 6. Projetos (Menu File)
- **New/Open**: define o diretório do projeto e cria `inbox/` e `notas/` se necessário.
- **Save/Save As**: garante pastas e copia os dados ao salvar como.
- **Close**: limpa o estado e desativa o projeto atual.
- **Exit**: encerra o loop principal da aplicação.
- **Navegação**: os modais trazem um browser de pastas com atalhos para raízes do SO.

---
*Versão do Documento: 0.1.0-alpha*

## Licença

Este projeto é licenciado sob a **GNU General Public License v3.0 (GPLv3)**.
Consulte o arquivo `LICENSE` para mais detalhes.
