ARQUIVO: iw_epistemic_contract_fixes.md

CONTEXTO
--------
Foram identificadas falhas recorrentes na validação de bundles científicos no
IdeaWalker, associadas a valores inválidos em enums (uso de "|" com múltiplas
alternativas) e ausência de "contextuality" em narrativeObservations e
allegedMechanisms.

Essas falhas não são bugs do schema nem do Validador Epistemológico, mas sim
resultado de um contrato cognitivo frouxo nos prompts fornecidos à IA.

OBJETIVO
--------
Endurecer o contrato epistemológico entre o IW e a IA, garantindo:
- decisões classificatórias únicas (enums)
- rastreabilidade epistemológica
- compatibilidade com exportação para o STRATA
- preservação da responsabilidade semântica da IA

--------------------------------------------------
1. PROBLEMA CENTRAL IDENTIFICADO
--------------------------------------------------

O prompt atual permite (implicitamente) que a IA:
- enumere possibilidades
- expresse ambiguidade classificatória
- trate enums como espaço analítico

Isso resulta em valores inválidos como:
"terrestrial|aquatic|urban|mixed"
"short|medium|long"

O schema exige escolha única. O STRATA exige compromisso ontológico mínimo.

--------------------------------------------------
2. PRINCÍPIO FUNDAMENTAL A SER IMPOSTO
--------------------------------------------------

Enums NÃO são espaço de análise.
Enums são decisões classificatórias obrigatórias.

Ambiguidade deve ser resolvida como "unknown", nunca como lista.

--------------------------------------------------
3. AJUSTES OBRIGATÓRIOS NO PROMPT (SOURCE PROFILE)
--------------------------------------------------

Adicionar instruções explícitas e duras ao prompt responsável por sourceProfile:

- Escolha EXATAMENTE UM valor para cada campo enum.
- NUNCA combine valores.
- NUNCA liste alternativas.
- Se houver dúvida ou múltiplas interpretações possíveis, use "unknown".
- Não explique a escolha.
- Não justifique alternativas descartadas.

Exemplo de instrução conceitual:

"You must select exactly ONE value from the allowed set.
Do not combine values.
Do not list alternatives.
If uncertain, return 'unknown'."

--------------------------------------------------
4. CONTEXTUALITY É CAMPO OBRIGATÓRIO CONCEITUALMENTE
--------------------------------------------------

Embora o schema permita ausência de "contextuality",
o Validador Epistemológico corretamente bloqueia exportação.

A IA deve ser instruída que:

- narrativeObservations e allegedMechanisms
  DEVEM incluir contextuality.
- contextuality define ONDE e SOB QUAIS CONDIÇÕES
  a observação é válida.

Sem contextuality, o item NÃO deve ser incluído.

--------------------------------------------------
5. AJUSTES NO PROMPT DISCURSIVO
--------------------------------------------------

O prompt discursivo atual foca corretamente em argumentação,
mas NÃO comunica à IA que:

- decisões classificatórias são obrigatórias
- ausência de contextualidade invalida o item
- enums não são interpretativos

Sugere-se incluir no prompt discursivo:

- Regra de decisão única para campos categóricos
- Obrigatoriedade de contextuality
- Proibição explícita de respostas ambíguas

--------------------------------------------------
6. PÓS-PROCESSAMENTO (FALLBACK CONTROLADO)
--------------------------------------------------

Opcional, mas recomendado como rede de segurança:

- Detectar valores contendo "|"
- Substituir automaticamente por "unknown"
- Registrar warning explícito
- Marcar bundle como "coerced"

NUNCA selecionar silenciosamente o primeiro valor.

--------------------------------------------------
7. O QUE NÃO FAZER
--------------------------------------------------

- NÃO relaxar o schema
- NÃO relaxar o Validador Epistemológico
- NÃO mover essa correção para o STRATA
- NÃO permitir decisões implícitas não rastreáveis

--------------------------------------------------
8. CONCLUSÃO
--------------------------------------------------

O IW está epistemicamente correto, mas cognitivamente permissivo.
O problema é de contrato semântico com a IA, não de arquitetura.

A correção exige endurecimento explícito dos prompts,
não refatoração estrutural do sistema.
