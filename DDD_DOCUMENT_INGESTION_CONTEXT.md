# DDD — DOCUMENT INGESTION CONTEXT
## Ingestão Observacional de Documentos na Inbox (md/txt/pdf/tex)

**VERSÃO:** 1.0  
**STATUS:** Proposta (pronta para MVP)  
**NATUREZA:** Observacional / Declarativa / Read-only (por padrão)  
**ESCOPO:** Contexto Auxiliar (NÃO Core Domain)  
**INTEGRAÇÃO:** Inbox como portal de fontes; notas como síntese; diálogo como apoio reflexivo

---

## 1. Motivação

O IdeaWalker evoluiu de um pipeline de áudio → texto → nota para um ambiente que
também sustenta a organização de ideias dispersas em documentos de trabalho.

Documentos (md/txt/pdf/tex) representam diferentes estágios cognitivos:
- ideias emergentes (txt cru, transcrições)
- ideias em formação (md, tex rascunho)
- artefatos consolidados (pdf, versões finais)

O sistema deve **observar** esses artefatos e produzir:
- sínteses
- comentários
- conexões
sem reescrever automaticamente o conteúdo original.

---

## 2. Princípios do Contexto

1. **Inbox é um portal de fontes observacionais**, não um “lugar de texto homogêneo”.
2. **Tipagem por fonte**: extensão e/ou metadados definem “modo de observação”.
3. **Por padrão, não reescrever** documentos (md/tex/pdf) — apenas observar/sintetizar.
4. **Rastreabilidade total**: qualquer saída aponta para a fonte original.
5. **Separação de responsabilidades**:
   - Core Domain: organização de ideias (permanece puro)
   - Observational: leitura/classificação/extração de texto e síntese

---

## 3. Bounded Context

### 3.1 Nome
**Document Ingestion Context**

### 3.2 Tipo
**Support/Observational Context** (não Core)

### 3.3 Responsabilidade
- Detectar arquivos novos/alterados na inbox
- Classificar tipo de fonte e modo de ingestão
- Extrair texto quando necessário (ex: PDF)
- Gerar “Observações” e/ou “Sínteses” em notas separadas
- Persistir metadados e rastreabilidade

### 3.4 Fora do escopo (por padrão)
- Editar o arquivo original
- “Corrigir” documento automaticamente
- Alterar diretamente notas existentes (sem ação explícita do usuário)

---

## 4. Linguagem Ubíqua

- **SourceArtifact**: arquivo observado na inbox (md/txt/pdf/tex)
- **SourceType**: tipo de fonte (Markdown, PlainText, PDF, LaTeX)
- **IngestionMode**: modo de ingestão (Estruturar, Sintetizar, Comentar, Extrair)
- **TextExtraction**: texto extraído de um artefato (principalmente PDF)
- **Observation**: registro observacional gerado a partir da fonte (read-only)
- **SynthesisNote**: nota produzida como síntese (novo artefato em /notas)
- **TraceLink**: vínculo rastreável entre saída e fonte (path/hash/timestamp)

---

## 5. Modelo de Domínio (Observational)

### 5.1 Entidades

#### 5.1.1 SourceArtifact (Entity)
Representa um arquivo detectado na inbox.

**Atributos**
- `ArtifactId` (hash ou ID interno)
- `path` (relativo ao projeto)
- `sourceType` (enum)
- `lastModified`
- `contentHash` (para detectar mudanças)
- `sizeBytes`
- `status` (New | Processed | Failed | Skipped)

**Invariantes**
- Um `ArtifactId` deve ser estável para o mesmo `path + contentHash`
- Mudança em `contentHash` cria nova “versão observacional” do mesmo path

---

#### 5.1.2 IngestionJob (Entity)
Unidade de trabalho para processar um artefato com um modo específico.

**Atributos**
- `JobId`
- `artifactId`
- `mode` (IngestionMode)
- `createdAt`
- `result` (Success | Failed)
- `errorMessage` (opcional)

**Invariantes**
- Todo Job deve apontar para um SourceArtifact existente
- Um Job não altera o arquivo original

---

#### 5.1.3 ObservationRecord (Entity)
Registro do resultado observacional.

**Atributos**
- `ObservationId`
- `artifactId`
- `mode`
- `summary` (texto)
- `keywords/tags` (opcional)
- `traceLink` (value object)
- `createdAt`

**Invariantes**
- Toda observation deve ser rastreável a uma fonte (TraceLink obrigatório)

---

### 5.2 Value Objects

#### 5.2.1 SourceType (VO / Enum)
- `PlainText`
- `Markdown`
- `PDF`
- `LaTeX`

#### 5.2.2 IngestionMode (VO / Enum)
Define o que a IA (ou pipeline) deve fazer:

- `STRUCTURE`  (mais adequado para .txt cru)
- `SYNTHESIZE`  (adequado para md/tex quando objetivo é resumo)
- `COMMENT`     (adequado para md/tex: críticas, perguntas, coerência interna)
- `EXTRACT_ONLY` (adequado para pdf: extrair texto + resumo leve)

Regra base sugerida (default):
- `.txt`  -> STRUCTURE
- `.md`   -> COMMENT (ou SYNTHESIZE, conforme configuração)
- `.tex`  -> COMMENT (sem reescrever o LaTeX)
- `.pdf`  -> EXTRACT_ONLY (e depois SYNTHESIZE, opcional)

#### 5.2.3 TraceLink (VO)
- `sourcePath`
- `contentHash`
- `observedAt`
- `excerptOffsets` (opcional; para futuro)
Objetivo: rastreabilidade e auditoria.

---

## 6. Aggregates

### 6.1 DocumentIngestionAggregate (Aggregate Root)
**Raiz:** `SourceArtifact`

Coordena:
- criação de IngestionJobs
- geração de ObservationRecords
- persistência de status

**Regras**
- Nunca modifica o arquivo fonte
- Produz outputs apenas como:
  - ObservationRecord (registro)
  - SynthesisNote (novo arquivo em /notas, opcional)

---

## 7. Domain Services

### 7.1 ArtifactClassifier (Domain Service)
Determina `SourceType` e `IngestionMode` iniciais.
- Input: path + extensão + heurísticas mínimas
- Output: classificação + modo sugerido

### 7.2 PromptPolicy (Domain Service)
Gera o “System Prompt” de acordo com `IngestionMode` e `SourceType`.
Regras:
- Proibir reescrita por padrão (md/tex/pdf)
- Enfatizar observação, síntese, coerência, perguntas

---

## 8. Application Layer (Orquestração)

### 8.1 DocumentIngestionService (Application Service)
Fluxo:
1. Scanner detecta novos/alterados em `/inbox`
2. Cria/atualiza SourceArtifact
3. Classifica (ArtifactClassifier)
4. Se PDF: extrai texto (TextExtractor Port)
5. Monta prompt (PromptPolicy)
6. Chama IA (AIService Port existente via Ollama/Qwen)
7. Salva resultado:
   - ObservationRecord + arquivo `.md` em `/observations` ou `/dialogues` (a decidir)
   - opcional: gera SynthesisNote em `/notas`

---

## 9. Ports & Adapters

### 9.1 Ports (interfaces)

- `IArtifactRepository`
  - save/load SourceArtifact metadata
- `IIngestionJobRepository`
  - persist jobs, status
- `IObservationRepository`
  - persist ObservationRecords
- `ITextExtractor`
  - extrair texto de PDF (porta para adapter)
- `IAIService`
  - já existe (OllamaAdapter) — reuso

### 9.2 Adapters (infra)

- `FileSystemArtifactScannerAdapter`
  - varre `/inbox`, detecta mudanças por mtime/hash
- `PdfTextExtractorAdapter`
  - extrai texto do PDF (implementação a definir)
- `OllamaAdapter`
  - já existente: gerar saída via modelo Qwen

---

## 10. Eventos (opcional no MVP, mas previstos)

- `ArtifactDetected`
- `ArtifactClassified`
- `IngestionJobCompleted`
- `ObservationCreated`

No MVP, podem ser logs/telemetria simples.

---

## 11. Persistência e Pastas

- `/inbox/`            (fontes observadas)
- `/dialogues/`        (conversas do projeto — já planejado)
- `/notas/`            (notas do projeto)
- `/observations/`     (NOVO — recomendado para outputs observacionais)
  - `YYYY-MM-DD_HH-MM-SS_{ArtifactId}.md`

Recomendação:
- **Observations** ≠ **Dialogues**
  - Observations: resultado de ingestão de documento
  - Dialogues: conversas contextuais sobre nota ativa

---

## 12. UI (MVP mínimo)

Adicionar uma seção “Ingestão de Documentos” (pode ser simples):
- Lista de arquivos detectados na inbox
- Tipo e modo sugerido
- Botão: “Processar”
- Indicador: Processed/Failed
- Link para abrir a Observation gerada

Obs.: Pode ser depois do MVP do chat. Mas o DDD já suporta.

---

## 13. Regras de Segurança Cognitiva (importantes)

- Para `.tex` e `.md`: por padrão a IA **não reescreve**, apenas comenta/sintetiza.
- Para `.pdf`: extrair → sintetizar (sem inventar referências).
- Para todos: saída deve conter `TraceLink` (path + hash + data).

---

## 14. MVP recomendado (Fase 1)

- Scanner da inbox detecta `.txt` e `.md`
- Classificador por extensão
- Um modo por padrão:
  - `.txt` -> STRUCTURE
  - `.md`  -> COMMENT
- IA gera Observation `.md` em `/observations`
- Sem PDF/LaTeX ainda (preparar portas, mas implementar depois)

Fase 2:
- `.tex` com COMMENT
Fase 3:
- `.pdf` com TextExtractorAdapter

---

## 15. Conclusão

Este contexto amplia o IdeaWalker para organizar não apenas ideias emergentes
(áudio/texto cru), mas também artefatos intelectuais em andamento, preservando:
- arquitetura DDD
- separação ontológica
- rastreabilidade
- e o princípio observacional

O sistema sustenta o pensamento no tempo — não o substitui.

---

## 16. Extensão Científica (STRATA)

Quando a fonte é científica, a ingestão passa por um pipeline dedicado e governado,
produzindo artefatos cognitivos tipados e consumíveis estáveis para o STRATA.

Detalhes completos em `docs/SCIENTIFIC_INGESTION_CONTEXT.md`, incluindo o Validador Epistemológico (VE-IW).
