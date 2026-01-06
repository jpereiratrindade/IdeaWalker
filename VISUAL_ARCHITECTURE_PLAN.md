# ARQUITETURA DE INTERFACE E VISUALIZAÇÃO: IDEA WALKER
# Contexto: Ferramentas Cognitivas Visuais para TDAH em C++ (ImGui)
# Objetivo: Combater Cegueira Temporal e Falta de Permanência de Objeto

================================================================================
1. ESTRATÉGIA COGNITIVA
================================================================================
O TDAH sofre com "o que não é visto, não existe". 
O Idea Walker não deve ser apenas um repositório de texto, mas um "Painel de Controle" 
que torna tangível o volume, a conexão e a pendência das ideias.

Tecnologia Base: Dear ImGui (Immediate Mode GUI) + ImPlot / ImNodes.

================================================================================
2. OS 4 MOTORES VISUAIS (ESPECIFICAÇÃO TÉCNICA)
================================================================================

[2.1] O GRAFO DE CONHECIMENTO (THE NEURAL WEB)
   * Função Cognitiva: Associatividade. Ver que "A" liga em "B" sem linearidade.
   * Visualização: Nódulos (Círculos) conectados por arestas (Linhas).
   * Implementação C++:
     - Lib Sugerida: 'ImNodes' (extensão do ImGui) ou desenho direto via 'ImDrawList'.
     - Algoritmo: Force-Directed Graph (Nós se repelem, Links atraem).
     - Cores: 
       - Roxo (#Epic) para projetos grandes.
       - Verde (#Insight) para ideias soltas.
       - Cinza (#Spark) para rascunhos não processados.

[2.2] QUADRO KANBAN DE AÇÃO (THE FACTORY)
   * Função Cognitiva: Concretude e Função Executiva.
   * Visualização: Colunas verticais onde tarefas são "cards" arrastáveis.
   * Estrutura:
     - [A FAZER] -> Extraído automaticamente dos checkboxes "- [ ]".
     - [EM ANDAMENTO] -> Onde o foco atual deve estar.
     - [FEITO] -> Dopamina visual.
   * Implementação C++:
     - Uso de `ImGui::BeginDragDropSource()` e `ImGui::AcceptDragDropPayload()`.
     - Ao soltar na coluna "Feito", o código edita o Markdown: "- [ ]" vira "- [x]".

[2.3] PAINEL DE BACKLINKS (CONTEXTO REVERSO)
   * Função Cognitiva: Redescoberta (evitar reinventar a roda).
   * Visualização: Painel lateral ou rodapé da nota.
   * Lógica: 
     - "Quem cita este arquivo?"
     - Ao abrir `Nota_A.md`, o sistema varre o diretório buscando strings `[[Nota_A]]`.
   * Valor: Transforma arquivos mortos em um sistema vivo de referências.

[2.4] MAPA DE CALOR TEMPORAL (GITHUB STYLE)
   * Função Cognitiva: Combate à Cegueira Temporal e Gamificação.
   * Visualização: Grade de quadrados coloridos (Heatmap) no rodapé do Dashboard.
     - Eixo X: Dias do Ano.
     - Cor: Intensidade de criação de notas no dia.
   * Implementação C++:
     - Loop simples desenhando `ImDrawList->AddRectFilled()`.
     - Dados extraídos dos metadados de data do arquivo (`std::filesystem::last_write_time`).

================================================================================
2.5 STATUS ATUAL (IMPLEMENTAÇÃO)
================================================================================
- Kanban com drag & drop: IMPLEMENTADO.
- Backlinks: IMPLEMENTADO.
- Heatmap de atividade: IMPLEMENTADO.
- Visão unificada de conhecimento: IMPLEMENTADO.
- Grafo visual (ImNodes/ImDrawList): NÃO IMPLEMENTADO.

================================================================================
3. ROADMAP DE IMPLEMENTAÇÃO (PRIORIDADE MVP)
================================================================================

Fase 1: CONCRETUDE (Essencial)
   - Implementar a ABA "EXECUÇÃO" (Lista simples de tarefas).
   - Extrair todos os `- [ ]` do Insight atual e mostrar com `ImGui::Checkbox`.

Fase 2: FLUXO (Intermediário)
   - Transformar a lista simples em um KANBAN visual.
   - Implementar Drag & Drop básico.

Fase 3: CONTEXTO (Avançado)
   - Implementar lista de texto de BACKLINKS.

Fase 4: VISÃO SISTÊMICA (Futuro)
   - Implementar o GRAFO visual com ImNodes.

================================================================================
4. NOTAS TÉCNICAS PARA O DESENVOLVEDOR (VOCÊ)
================================================================================
- Não use plugins web (Electron/Obsidian) para isso. O custo de RAM é alto.
- No C++, mantenha o "Estado da UI" separado do "Estado do Domínio".
- Use `ImGui::DockSpace` para permitir que o usuário organize onde quer ver o Kanban 
  e onde quer ver o Texto ao mesmo tempo.
