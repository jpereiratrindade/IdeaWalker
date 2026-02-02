# Registro de Alterações (Changelog)

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

## [v0.1.5-beta] - 2026-01-09
### Adicionado
- **Finalização e Limpeza**: Revisão completa do sistema para release v0.1.5-beta.
- **Sincronização de Documentação**: README atualizado com todas as funcionalidades recentes (PDF, Orquestração, Ressonância).
- **Persistência de Modelo de IA**: A seleção do modelo de IA (Configurações > Selecionar Modelo) agora é salva em `settings.json` e restaurada automaticamente ao abrir o projeto.
- **Resiliência de Ícones (Font Fallback)**: Implementado sistema robusto que reverte ícones para texto simples caso a fonte Emoji não seja encontrada, prevenindo crashes e inconsistências visuais.
- **Correção de Segfault em Fontes**: Corrigido um bug crítico de gerenciamento de memória no carregamento de ranges de glifos unicode (`IdeaWalkerApp.cpp`).

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
