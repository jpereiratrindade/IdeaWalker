# Diretrizes de Funcionamento LLM - IdeaWalker

Este documento formaliza as "REGRAS RÍGIDAS" e a lógica de processamento utilizada pelo motor de IA (LLM via Ollama) para interpretar e estruturar pensamentos brutos e notas transcritas.

## 1. Perfil de Processamento: Parser de Texto Estrito
O LLM opera em modo **Parser**, o que significa que ele não deve ser criativo ou conversacional. Sua função é puramente de estruturação semântica.

### Regras de Ouro (Prompt System)
- **Nível de Ruído**: Deve ignorar hesitações típicas de transcrição de áudio (ex: "hã", "tipo", repetições causadas por erro de Whisper).
- **Sem Blocos de Código**: A saída deve ser Markdown puro, sem cercas de código (fenced code blocks), para evitar quebras de renderização na UI do IdeaWalker.
- **Concisão**: Introduções como "Aqui está sua nota processada..." são proibidas.

## 2. Estrutura Obrigatória de Saída
Todo pensamento processado deve seguir estritamente o template abaixo:

```markdown
# Título: [Título Curto e Representativo]

## Insight Central
(Um resumo denso de no máximo um parágrafo capturando a essência do pensamento)

## Pontos Principais
- (Item chave 1)
- (Item chave 2)

## Ações
- [ ] (Ação concreta extraída do contexto)
- [ ] (Tarefa pendente identificada)

## Conexões
- [[Conceito A]]
- [[Conceito B]]
```

## 3. Lógica de Extração de Ações (Actionables)
- O LLM deve identificar verbos de ação no imperativo ou infinitivo.
- Sentenças vagas (ex: "Eu deveria pensar nisso") devem ser ignoradas.
- Tarefas explícitas (ex: "Ligar para o fornecedor X amanhã") devem ser extraídas para a seção de `## Ações`.

## 4. Consolidação de Tarefas
Ao processar o arquivo `_Consolidated_Tasks.md`, o LLM segue estas diretrizes:
- **De-duplicação Semântica**: Identificar tarefas que dizem a mesma coisa com palavras diferentes e unificá-las na versão mais clara.
- **Preservação de Status**: Não alterar o status de tarefas que já foram marcadas como concluídas ou em progresso em rodadas anteriores.
- **Agrupamento**: Manter tarefas relacionadas próximas umas das outras.

## 5. Mapeamento de Conexões (Knowledge Graph)
Para alimentar o **Neural Web**, o LLM deve sugerir conexões usando double brackets `[[...]]`.
- Deve sugerir termos que já existem na base (se fornecidos no contexto) ou termos de alto nível que possam virar novas notas futuras.

---
*Versão do Documento: 1.0.0*

## Licença

Este projeto é licenciado sob a **GNU General Public License v3.0 (GPLv3)**.
Consulte o arquivo `LICENSE` para mais detalhes.
