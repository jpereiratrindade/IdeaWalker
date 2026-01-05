# Guia Técnico de Funcionamento - Idea Walker

Este documento descreve as entranhas técnicas e o fluxo de dados do sistema.

## 1. Fluxo de Execução
O sistema utiliza o  para mediar as interações entre as camadas.

1.  **Input**: Arquivos  são depositados na pasta .
2.  **Mapeamento**: O  (Adapter) lê esses arquivos e os converte em objetos  (Domain).
3.  **Processamento**: O  (Application) envia o conteúdo bruta para o  (Infrastructure).
4.  **Enriquecimento**: O Ollama (via Qwen 2.5) retorna uma estrutura Markdown.
5.  **Output**: O sistema gera um objeto  (Domain) e o  persiste no disco como um arquivo  na pasta .

## 2. Camada de Domínio (DDD)
- **Insight**: Representa uma ideia já refinada. Contém metadados (ID, Título, Tags) e o conteúdo final.
- **Actionable**: Valor que representa uma tarefa concreta derivada do Insight.
- **ThoughtRepository**: Porta de saída para persistência.
- **AIService**: Porta de saída para inteligência artificial.

## 3. Infraestrutura
- **OllamaAdapter**: Realiza chamadas POST HTTP para a API  do Ollama.
- **FileRepository**: Gerencia o sistema de arquivos local usando .

## 4. UI (ImGui)
A interface é baseada em estados imediatos. O botão "Run AI Orchestrator" dispara uma **worker thread** para não bloquear o loop principal de renderização.

---
*Versão do Documento: 0.1.0-alpha*
