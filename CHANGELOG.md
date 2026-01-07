# Registro de Alterações (Changelog)

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

## [v0.1.1-beta] - 2026-01-07

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

### Corrigido
- **Desaparecimento da Neural Web**: Resolvido um conflito onde o gráfico neural principal e o gráfico de preview compartilhavam o mesmo contexto `ImNodes`. Implementada isolação estrita de contextos.
- **Falha de Segmentação (Segfault) no Início**: Corrigido um problema na ordem de inicialização onde os contextos de gráfico eram acessados antes da criação.
- **Sobreposição de Nós**: Nós com textos longos não se sobrepõem mais no preview.
- **Pontos de Lista**: Corrigida renderização de bullet points em Markdown com fallback ASCII.

### Alterado
- Refatorado `UiRenderer` para incluir melhorias no `ParseMermaidToGraph` e na função `DrawStaticMermaidPreview`.
- Atualizado `AppState` para gerenciar ponteiros independentes de `ImNodesEditorContext`.
- Documentação migrada para Português (README, Guia Técnico, Arquitetura).
