# IdeaWalker (IW)
## Diretrizes de Implementação – Alinhamento Metodológico

Este documento sintetiza ajustes e cuidados de implementação derivados
da consolidação metodológica do projeto IdeaWalker (IW).

Ele NÃO introduz uma nova arquitetura.
Seu objetivo é alinhar, explicitar e proteger decisões conceituais
já assumidas no projeto.

---

## 1. Princípio Geral

O IdeaWalker é:

- **Document-Driven por natureza**
- **Governado por Domain-Driven Design**
- **Orientado por princípios epistemológicos explícitos**

O código não redefine teoria.
O código operacionaliza e protege decisões epistemológicas.

---

## 2. Separação de Contextos (Regra Estrutural)

### 2.1 Contexto Epistemológico (fora do código)

Responsável por:
- fundamentos teóricos
- limites interpretativos
- distinção entre observação e inferência
- princípios metodológicos

Forma:
- documentos (.md, .tex)
- notas metodológicas
- decisões arquiteturais (ADR)

⚠️ Este contexto **não executa código**.

---

### 2.2 Contexto Computacional (no código)

Responsável por:
- garantir invariantes
- estruturar entidades e value objects
- aplicar regras de interpretação autorizadas
- orquestrar fluxos de análise

⚠️ Este contexto **não cria nem altera teoria**.

---

## 3. Narrativa como Contexto de Aplicação

A Narrativa NÃO é apenas conteúdo textual.

Ela funciona como:
- delimitador pragmático
- configurador de análise discursiva
- condicionante de validade interpretativa

### Diretriz prática:
- A análise discursiva deve **receber explicitamente**
  um contexto narrativo.
- Nunca deduzir narrativa por ausência de informação.
- Narrativa condiciona regras válidas,
  mas NÃO decide resultados.

---

## 4. Inferência: Limites Claros

O IW distingue explicitamente entre:

- inferência **declarada no discurso**
- inferência **computacionalmente produzida**

### Diretriz central:
❌ O IW não infere causalidade, impacto ou intenção.
✅ O IW observa, extrai e estrutura
   como causalidade, impacto ou intenção
   são ENUNCIADOS no discurso.

---

## 5. Nomenclatura e Semântica no Código

Evitar termos que impliquem decisão ou inferência automática.

### Preferir:
- extractDeclaredX
- observeStatedY
- registerNarratedZ

### Evitar:
- inferX
- deriveCausalY
- computeImpactZ

⚠️ Ajustes aqui são semânticos,
não exigem refatoração estrutural.

---

## 6. Domínio vs Orquestração

### Regra prática:
- O domínio decide **SE algo é interpretável**
- A aplicação decide **QUANDO e COMO interpretar**

Sinais de alerta:
- entidades de domínio orquestrando fluxo
- domínio dependendo de tempo, IA ou I/O
- métodos “faz-tudo” em Aggregates

---

## 7. O que NÃO fazer (importante)

❌ Não tentar “purificar” DDD  
❌ Não mover parsing documental para fora do domínio  
❌ Não introduzir banco de dados como nova fonte da verdade  
❌ Não automatizar inferência com base em narrativa  
❌ Não tratar o documento metodológico como spec rígida  

---

## 8. Regra de Ouro para Novas Funcionalidades

Antes de implementar algo novo, responder:

1. Isso altera o que pode ser interpretado?
   → atualizar documentação epistemológica

2. Isso altera como garantimos essa interpretação?
   → ajustar código (DDD)

Se a resposta for “nenhum dos dois”,
avaliar se a funcionalidade é realmente necessária.

---

## 9. Encerramento

Estas diretrizes existem para:
- evitar deriva conceitual
- proteger o caráter científico do IW
- manter coerência entre documentos, IA e código

Em caso de dúvida:
👉 priorizar clareza epistemológica
   sobre conveniência técnica.
