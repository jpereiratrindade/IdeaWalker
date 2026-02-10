# Registro de Alterações (Changelog)

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

## [Unreleased]
### Alterado
- **UI Refactor**: Extração da interface de Ingestão Científica do `DashboardPanel` para um novo painel dedicado (`ScientificPanel`).
- **Navegação**: Nova aba "Scientific" (🧪) adicionada à barra de guias principal.
- **Layout**: Implementação de visualização dividida (Split View) para a aba Científica (Lista de Arquivos vs Detalhes/Validação).

## [v0.1.15-beta] - 2026-02-10
### Consolidado
- **Ingestão Científica & UI**: Integração final dos serviços de ingestão com o Dashboard, incluindo validação epistêmica e feedback visual de progresso.
- **Estabilização do Repositório**: Sincronização de commits e limpeza de estado para release seguro.

## [v0.1.14-beta] - 2026-02-06
### Adicionado
- **Ancoragem Difusa (Fuzzy Match)**: O sistema de validação epistêmica agora utiliza um algoritmo de "Janela Deslizante baseada em Tokens" com tolerância a Levenshtein (distância de edição).
    - Permite que snippets de evidência sejam aceitos mesmo com pequenos erros de OCR (ex: `infiltração` vs `infiltragao`) ou normalizações do modelo.
    - Resolve o problema de esvaziamento do `DiscursiveContext` quando o modelo corrigia a ortografia da citação.

## [v0.1.13-beta] - 2026-02-06
### Adicionado
- **Regra de Exclusão Estrutural (ADR 001)**: Nova camada de pré-processamento no `ContentExtractor` que identifica e remove cabeçalhos e rodapés repetitivos (>60% das páginas). Isso limpa metadados editoriais (nomes de revista, paginação) do texto antes da ingestão.
- **Mitigação de Alucinação (Discursive Anchoring)**:
    - Ingestão Científica agora exige `evidenceSnippet` (cópia literal) para todo problema, ação e efeito discursivo.
    - Itens sem âncora textual verificada são automaticamente descartados, prevenindo invenção de conceitos pelo modelo.

### Alterado
- **Ingestão Bifásica**: O processo de ingestão científica foi dividido em dois turnos cognitivos distintos:
    1. **Narrativa**: Foco em observações factuais e teorias.
    2. **Discursiva**: Foco em frames, retórica e sistemas de problemas.
    - Isso resolve falhas de atenção onde modelos menores (7B) ignoravam instruções complexas combinadas.

## [v0.1.12-beta] - 2026-02-05
### Adicionado
- **Injeção de Contexto (Memória de Curto Prazo)**: A IA agora consulta a pasta `observations` antes de gerar uma Nota Estruturada. Se uma "Observação Narrativa" (gerada via Sync Inbox) existir, ela é injetada no prompt como contexto, garantindo continuidade epistêmica.
    - Implementado `findObservationContent` no repositório.
    - `AIProcessingService` atualizado para fundir contexto prévio.
- **Progresso de OCR em Tempo Real**: Implementada leitura de fluxo (`popen`) do `ocrmypdf`, permitindo que a barra de status exiba o progresso real ("Scanning...", "Page 1/X", "Optimizing") em vez de congelar.

### Alterado
- **Otimização de OCR**: Refinado o pipeline de ingestão de PDF para desempenho máximo.
    - Adicionado `--fast` e `--jobs 4` para paralelismo.
    - Adicionado `--optimize 0` para pular recompressão de imagens (gargalo de CPU).
    - Removido `--force-ocr` para evitar re-rasterização desnecessária.
    - **Persistência (.ocr cache)**: O output do OCR agora é salvo em `inbox/.ocr/` e reutilizado, evitando reprocessamento eterno e permitindo inspeção.
- **Prioridade de Contexto**: A injeção de contexto (`[CONTEXTO PRE-EXISTENTE]`) agora é inserida no **início** do prompt para garantir que o modelo a considere antes de analisar o texto bruto.

### Corrigido
- **Persistência de Modelo**: Corrigida a reinicialização da preferência de modelo (ex: `14b` voltando para `7b`). O `IdeaWalkerApp` agora carrega explicitamente o `settings.json` antes de inicializar o adaptador Ollama.

## [v0.1.11-beta] - 2026-02-04
### Corrigido
- **Persistência de Modelo de IA**: Corrigido um bug na seleção de modelos onde o `ModelSelector` ignorava a preferência do usuário e a inicialização sobrescrevia a escolha salva.
    - Centralizada a lógica em `ConfigLoader::GetAIModelPreference`.
    - Garantido que `OllamaAdapter` respeita o modelo injetado antes da inicialização.
- **Ingestão Científica (Qwen 2.5)**: Atualizado o método `generateJson` para utilizar a API de Chat (`/api/chat`) em vez de Completions. Isso resolve falhas de ingestão com modelos Instruct (como o 14b) que exigem templates de prompt rigorosos.

## [v0.1.10-beta] - 2026-02-04
### Adicionado
- **Ponte de Ingestão Científica Limpa (Scientific Bridge)**: Refatoração completa da integração IdeaWalker -> SisterSTRATA.
    - `ScientificIngestionService` agora expõe método público `ingestScientificBundle` para ingestão direta.
    - Roteamento baseado em intenção (`Intent-Based Routing`) no `AIProcessingService`, eliminando acoplamento no repositório.
    - Remoção de código legado ("spaghetti code") do `FileRepository.cpp`.
- **Rigor Epistêmico (Prompt Engineering)**:
    - O prompt do `ScientificObserver` foi blindado para gerar apenas **CANDIDATE BUNDLES**.
    - Suporte explícito a `DiscursiveContext` com frames e `NarrativeObservation` com Temas dinâmicos mapeados para Eixos do STRATA.
    - Output 100% compatível com o schema de consumo do SisterSTRATA (validado via exemplos `example_*.json`).

## [v0.1.9-beta] - 2026-02-03
### Adicionado
- **Extração de Contexto Discursivo**: Nova camada na ingestão científica que captura *frames* discursivos, valência e retórica (`DiscursiveContext.json`), separada da narrativa factual.
- **DTOs Candidatos (Scientific Ingestion)**: A saída narrativa (`NarrativeObservation.json`) agora é explicitamente tratada como "Candidatos" (strings), evitando acoplamento prematuro com Enums do STRATA.
- **Exportação de Artefatos Estendida**: O `ScientificIngestionService` agora gera e exporta o arquivo `DiscursiveContext.json` quando frames são detectados.

### Alterado
- **Validação Epistêmica Relaxada**: A ausência de `temporalWindowReferences` agora é tratada como um **Aviso** (pass-with-warnings) em vez de Erro bloqueante, aumentando a resiliência do processo de ingestão.
- **Prompts de IA**: Instruções atualizadas para solicitar explicitamente a separação entre observação narrativa e enquadramento discursivo.
- **Compatibilidade STRATA (Metadados)**: `metadata` e `interpretationMetadata` agora são forçados a `map<string,string>` e arrays discursivos são normalizados para `{statement}`.
- **Exportação Limpa**: Consumíveis vazios não são mais gerados; `NarrativeObservation.json` só é escrito quando há conteúdo.

## [v0.1.8-beta] - 2026-02-03
### Adicionado
- **Isolamento da Camada de Serviço (Phase 5.3)**: Completa decomposição do `OrganizerService` monolítico em serviços de domínio focados:
    - `KnowledgeService`: Gerenciamento puro de conhecimento (Notas, Insights, Histórico).
    - `AIProcessingService`: Orquestração de pipelines de IA e consolidação.
- **Gerenciamento Assíncrono Centralizado**: Introdução do `AsyncTaskManager` para execução robusta e não-bloqueante de tarefas em background (Transcrição, Indexação, IA), com status unificado na UI.
- **Refatoração dos Painéis de Escrita**: `WritingPanels` e `ModalPanels` atualizados para consumir a nova arquitetura de serviços.
- **Chat Conversacional Desacoplado**: O painel de chat agora depende estritamente do `ConversationService`, removendo dependências legadas.
### Corrigido
- **Troca de Projeto com Inbox Antiga**: ao abrir um novo projeto, os serviços são reconstruídos com o novo `root`, evitando leitura da inbox e notas de pastas anteriores.

## [v0.1.7-beta] - 2026-02-03
### Adicionado
- **Arquitetura de Serviços Desacoplada (Phase 4)**: Extração da lógica de negócio do `AppState` para `GraphService`, `ProjectService` e `KnowledgeExportService`.
- **Decomposição Completa da UI (Phase 2)**: Modularização do `UiRenderer.cpp` em painéis especializados (`DashboardPanel`, `KnowledgePanel`, `ExecutionPanel`, etc.).
- **Injeção de Dependência**: Inicialização de serviços via `AppServices` no `IdeaWalkerApp`.

## [v0.1.6-beta] - 2026-02-03
### Adicionado
- **Arquitetura Modular de UI**: Refatoração profunda do `UiRenderer.cpp`, extraindo componentes especializados para reduzir o acoplamento e facilitar a manutenção.
    - `UiMarkdownRenderer`: Módulo dedicado para renderização de Markdown e previews de Mermaid.
    - `UiFileBrowser`: Componente isolado para navegação no sistema de arquivos e utilitários de caminho.
- **Menu de Ajuda e Documentação**: Implementado sistema de visualização de manuais técnicos e guias diretamente na interface do app.
- **Verificador de Atualizações**: Novo recurso para checagem assíncrona de novas versões no repositório GitHub.
- **Domínio Mermaid**: O parser de Mermaid foi movido para o diretório de domínio (`src/domain/writing`), alinhando o projeto com princípios de DDD (Domain-Driven Design).
- **Utilitários de Áudio**: Lógica de processamento de áudio extraída de adaptadores para utilitários reaproveitáveis.
- **Limpeza de Build**: Atualização do `CMakeLists.txt` (v0.1.7-beta) e correção de dependências de headers no `UiRenderer.cpp`.

## [v0.1.5-beta] - 2026-01-09
### Adicionado
- **Finalização e Limpeza**: Revisão completa do sistema para release v0.1.5-beta.
- **Sincronização de Documentação**: README atualizado com todas as funcionalidades recentes (PDF, Orquestração, Ressonância).
- **Persistência de Modelo de IA**: A seleção do modelo de IA (Configurações > Selecionar Modelo) agora é salva em `settings.json` e restaurada automaticamente ao abrir o projeto.
- **Resiliência de Ícones (Font Fallback)**: Implementado sistema robusto que reverte ícones para texto simples caso a fonte Emoji não seja encontrada, prevenindo crashes e inconsistências visuais.
- **Correção de Segfault em Fontes**: Corrigido um bug crítico de gerenciamento de memória no carregamento de ranges de glifos unicode (`IdeaWalkerApp.cpp`).
- **Auto-Download de Modelo Whisper**: O sistema verifica e baixa automaticamente o modelo `ggml-base.bin` se não encontrado, garantindo transcrição out-of-the-box.
- **Seletor de Driver de Vídeo (Wayland/X11)**: Adicionada opção `video_driver` em `settings.json` para forçar compatibilidade X11 em ambientes Wayland instáveis, sem necessidade de recompilação.
- **Interface de Transcrição Explícita**: Nova opção de menu `File > Transcrever Áudio...` permite carregar arquivos via caminho absoluto, contornando limitações de Drag & Drop em alguns ambientes Linux/Wayland.
- **Estabilidade Wayland**: Revertida bandeira experimental `LIBDECOR` que causava instabilidade; solução via configuraçao é recomendada.

## [v0.1.4-beta] - 2026-01-08
### Adicionado
- **Extração de Texto de PDF**: Implementado fallback automático via `pdftotext` para processar documentos PDF na inbox.
- **Log de Atividade Persistente**: Introduzido arquivo `.activity_log.json` para rastrear histórico de criação de notas, garantindo que o Heatmap de atividade seja preservado mesmo após reprocessamento total.
- **Botão de Copiar no Chat**: Adicionado botão de conveniência ao lado de cada mensagem no painel de conversa para copiar o conteúdo para a área de transferência.
- **Ressonância Semântica (Suggestion Engine)**: O sistema agora sugere conexões entre notas baseado em similaridade semântica (Embeddings), permitindo linkagem assistida.
- **Seleção de Diálogos**: Agora é possível listar e carregar sessões de conversa anteriores na aba de chat.
- **Quebra de Linha Automática (Word Wrap)**: O editor de notas e o chat agora ajustam o texto automaticamente ao tamanho da tela.
- **Expanded Format Support**: O orquestrador agora suporta arquivos `.pdf`, `.md` e `.tex` na inbox via extração automática de texto.

### Alterado (Arquitetura)
- **Pipeline Cognitivo Autônomo**: A seleção manual de persona foi removida. O sistema agora opera de forma autônoma, diagnosticando o estado cognitivo do texto e aplicando a sequência ideal de operadores (Brainstormer, Analista, Secretário).
- **Rastreabilidade Cognitiva (Snapshots)**: Implementada estrutura interna de `CognitiveSnapshot` para registrar cada transformação aplicada ao pensamento, preparando o terreno para futuros grafos de decisão.

### Corrigido
- **Ajuste de Texto (Word Wrap)**: Refatorada a exibição de mensagens no chat para usar quebra de linha automática (TextWrapped), garantindo legibilidade em qualquer largura de painel.
- **Persistência do Heatmap**: Resolvido o problema onde o heatmap resetava para o dia atual ao usar a função "Reprocessar Tudo".

## [v0.1.3-beta] - 2026-01-07
### Adicionado
- **Orquestrador Cognitivo (Perfil TDAH)**: Uma nova meta-persona que diagnostica a transcrição e decide dinamicamente a sequência de perfis (Brainstormer, Analista, Secretário) a serem executados.
- **Feedback de Status em Tempo Real**: O systema agora exibe explicitamente qual persona está operando ("Diagnosing...", "Running Brainstormer...") ao lado do indicador de progresso.
- **Brainstormer Refinado**: Prompt atualizado para gerar bifurcações explícitas, sementes de ideia e nós de grafo (`[[WikiLink]]`).

## [v0.1.2-beta] - 2026-01-07

### Adicionado
- **Localização Completa (pt-BR)**: Toda a interface do usuário (menus, logs, diálogos) e documentação foram traduzidas para o Português do Brasil.
- **Suporte a Fontes Latinas**: Adicionado o intervalo `Latin-1 Supplement` ao atlas de fontes para renderização correta de acentos (ã, é, ç, etc.).
- **Licença GPLv3**: O projeto agora é explicitamente licenciado sob a GNU GPLv3.
- **Preview Estático para Arquivos Externos**: Substituição da visualização contínua por física por uma renderização estática e pré-calculada para previews de Markdown/Mermaid. Isso garante que os gráficos fiquem estritamente organizados e instantaneamente estáveis ao carregar.
- **Parser Mermaid Aprimorado**:
    - Adicionado suporte para sintaxe `mindmap` com hierarquia baseada em indentação.
    - Adicionado suporte para definições de nós isolados (ex: `A[Label]`) sem exigir links explícitos.
    - Suporte aprimorado para sintaxe de formas de nós como `((círculo))`.
- **Layout Ciente de Texto**: A física do grafo agora leva em conta o comprimento do texto do nó para prevenir sobreposições de forma eficaz.
- **Coloração Radial**: Nós no preview estático são coloridos com base em sua posição angular em relação ao centro do gráfico.
- **Suporte a Áudio MP3/M4A**: Conversão automática de formatos de áudio (via `ffmpeg`) para WAV compatível (16kHz Mono) antes da transcrição.
- **Padrão XDG para Modelos**: Modelos de IA agora são buscados no diretório padrão de dados do usuário (`~/.local/share/IdeaWalker/models/`) em sistemas Linux.
- **Persona "Analista Cognitivo"**: Atualização do prompt do sistema para uma análise mais profunda, focada em mapear tensões cognitivas e decisões estratégicas (o que não fazer), em vez de apenas resumir.
- **Configuração de Persona de IA**: Nova interface em "Configurações > Preferências" para alternar entre personas ("Analista Cognitivo", "Secretário Executivo", "Brainstormer").
- **Parsing de Metadados**: Implementada extração automática de títulos gerados pela IA (# Título: ...) para melhor organização de notas.


### Corrigido
- **Desaparecimento da Neural Web**: Resolvido um conflito onde o gráfico neural principal e o gráfico de preview compartilhavam o mesmo contexto `ImNodes`. Implementada isolação estrita de contextos.
- **Falha de Segmentação (Segfault) no Início**: Corrigido um problema na ordem de inicialização onde os contextos de gráfico eram acessados antes da criação.
- **Sobreposição de Nós**: Nós com textos longos não se sobrepõem mais no preview.
- **Pontos de Lista**: Corrigida renderização de bullet points em Markdown com fallback ASCII.

### Alterado
- Refatorado `UiRenderer` para incluir melhorias no `ParseMermaidToGraph` e na função `DrawStaticMermaidPreview`.
- Atualizado `AppState` para gerenciar ponteiros independentes de `ImNodesEditorContext`.
- Documentação migrada para Português (README, Guia Técnico, Arquitetura).
