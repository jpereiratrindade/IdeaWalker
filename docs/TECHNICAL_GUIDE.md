# Guia Tecnico de Funcionamento - Idea Walker

Este documento descreve o fluxo de dados, os principais componentes e como o projeto e organizado.

## 1. Fluxo de Execucao
1. **Entrada**: arquivos `.txt` sao colocados em `<projeto>/inbox/`.
2. **Leitura**: `FileRepository` converte arquivos em `RawThought`.
3. **Processamento**: `OrganizerService` chama o `AIService` (`OllamaAdapter`) para gerar Markdown estruturado.
4. **Persistencia**: o resultado vira `Insight` e e salvo em `<projeto>/notas/`.
5. **Consolidacao**: o servico atualiza `notas/_Consolidated_Tasks.md` com tarefas unificadas e sem duplicatas.

## 2. Camada de Dominio (DDD)
- **Insight**: conteudo estruturado, metadados e actionables.
- **Actionable**: tarefas extraidas dos insights.
- **RawThought**: entrada bruta do inbox.
- **Ports**: `ThoughtRepository` (persistencia) e `AIService` (IA).

## 3. Camada de Aplicacao
- **OrganizerService**: orquestra processamento, atualizacao de tarefas consolidadas e mudanca de status das tarefas.

## 4. Infraestrutura
- **FileRepository**: leitura e escrita de arquivos, backlinks e historico de atividade.
- **OllamaAdapter**: integracao com a API do Ollama usando o modelo `qwen2.5:14b`. Veja as [Diretrizes de Prompt](LLM_PROMPT_GUIDELINES.md) para detalhes da interpretacao.

## 5. UI (ImGui)
- **AppState**: estado da UI, selecoes, logs, visao unificada e projeto ativo.
- **UiRenderer**: renderiza Dashboard & Inbox, Organized Knowledge e Execucao (Kanban).
- **Processamento assincrono**: threads de IA atualizam `pendingRefresh` para recarregar a UI.

## 6. Projetos (File Menu)
- **New/Open**: define o diretorio do projeto e cria `inbox/` e `notas/` se necessario.
- **Save/Save As**: garante pastas e copia os dados ao salvar como.
- **Close**: limpa o estado e desativa o projeto atual.
- **Exit**: encerra o loop principal da aplicacao.
- **Navegacao**: os modais trazem um browser de pastas com atalhos para raizes do SO.

---
*Versao do Documento: 0.1.0-alpha*
