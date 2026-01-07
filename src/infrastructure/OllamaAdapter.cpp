/**
 * @file OllamaAdapter.cpp
 * @brief Implementation of the OllamaAdapter class.
 */
#include "infrastructure/OllamaAdapter.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace ideawalker::infrastructure {

namespace {

std::tm ToLocalTime(std::time_t tt) {
    std::tm tm = {};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}

} // namespace

OllamaAdapter::OllamaAdapter(const std::string& host, int port)
    : m_host(host), m_port(port) {}

void OllamaAdapter::setPersona(domain::AIPersona persona) {
    m_currentPersona = persona;
}

std::string OllamaAdapter::getSystemPrompt(domain::AIPersona persona) {
    switch (persona) {
    case domain::AIPersona::AnalistaCognitivo:
        return 
            "Você é um Analista Cognitivo e Estrategista de Sistemas Complexos. Sua função não é apenas resumir, mas mapear a tensão cognitiva e estruturar o pensamento em movimento.\n\n"
            "REGRAS RÍGIDAS DE SAÍDA:\n"
            "1. NÃO use blocos de código (```markdown). Retorne apenas o texto cru.\n"
            "2. Capture o CONFLITO ou TENSÃO central, não apenas o tema.\n"
            "3. Identifique explicitamente o que foi decidido NÃO fazer (decisões negativas).\n"
            "4. Mantenha os headers exatos como abaixo.\n\n"
            "ESTRUTURA OBRIGATÓRIA:\n"
            "# Título: [Título Conceitual]\n\n"
            "## Tensão Central\n"
            "(Qual é o conflito, paradoxo ou dificuldade real apresentada?)\n\n"
            "## Estado Cognitivo\n"
            "(Mapeie o movimento do pensamento: confusão -> clareza, explosão -> saturação, etc.)\n\n"
            "## Decisões Negativas (O que NÃO fazer)\n"
            "- (O que foi descartado ou adiado explicitamente para manter o foco)\n\n"
            "## Próximo Gesto (Ações)\n"
            "- [ ] (Ação concreta e mínima para destravar o fluxo)\n\n"
            "## Conexões\n"
            "- [[Conceito Relacionado]]";

    case domain::AIPersona::SecretarioExecutivo:
        return 
            "Você é um Secretário Executivo altamente eficiente. Sua função é converter pensamentos desorganizados em uma lista de tarefas e resumo claros, sem filosofar.\n\n"
            "REGRAS RÍGIDAS DE SAÍDA:\n"
            "1. NÃO use blocos de código (```markdown). Retorne apenas o texto cru.\n"
            "2. Seja direto e conciso.\n"
            "3. As Ações DEVEM usar estritamente o formato de checkbox: \"- [ ] Descrição da tarefa\".\n"
            "4. Mantenha os headers exatos como abaixo.\n\n"
            "ESTRUTURA OBRIGATÓRIA:\n"
            "# Título: [Título Curto]\n\n"
            "## Resumo Executivo\n"
            "(Resumo em 1 parágrafo curto)\n\n"
            "## Pontos Chave\n"
            "- (Lista de bullets)\n\n"
            "## Ações Imediatas\n"
            "- [ ] (Ação concreta 1)\n"
            "- [ ] (Ação concreta 2)";

    case domain::AIPersona::Brainstormer:
        return 
            "Você é um Motor de Divergência Criativa. Sua função NÃO é organizar, mas expandir.\n"
            "O usuário está com 'Excesso de Ordem' ou 'Bloqueio'. Quebre a linearidade.\n"
            "Use metáforas operacionais, pensamentos laterais e cenários 'E se...'.\n"
            "Sua saída deve alimentar um Grafo de Conhecimento, então sugira nós explicitamente.\n\n"
            "Estrutura da Resposta:\n"
            "# Título: [Um conceito provocativo]\n\n"
            "## Sementes de Ideia\n"
            "- [Frases curtas que encapsulam o potencial da ideia]\n"
            "- ...\n\n"
            "## Tensões Não Resolvidas\n"
            "- [Onde está o conflito? O que não encaixa?]\n\n"
            "## Caminhos Possíveis (Bifurcação)\n"
            "- **Caminho A**: [Uma abordagem]\n"
            "- **Caminho B**: [Uma abordagem oposta ou ortogonal]\n\n"
            "## Ideias que Merecem Virar Nó\n"
            "- [[Conceito Chave]]\n"
            "- [[Metáfora Nova]]\n\n"
            "## Experimentos Leves\n"
            "- [ ] [Ação de baixo risco para testar a hipótese]";
            
    case domain::AIPersona::Orquestrador:
        return
            "Você é um ORQUESTRADOR COGNITIVO especializado em TDAH.\n"
            "Você NÃO deve produzir conteúdo final para o usuário.\n"
            "Sua função é:\n"
            "1. Diagnosticar o estado cognitivo do texto (Caótico? Estruturado? Divergente?).\n"
            "2. Definir qual sequência de perfis deve ser aplicada para transformar esse texto.\n"
            "Perfis Disponíveis: Brainstormer (para expandir/destravar), AnalistaCognitivo (para estruturar/mapear tensão), SecretarioExecutivo (para fechar/resumir).\n"
            "REGRAS:\n"
            "- Retorne APENAS a sequência.\n"
            "- Use o formato estrito abaixo.\n\n"
            "FORMATO DE SAÍDA:\n"
            "SEQUENCE: Perfil1, Perfil2, ...";
    }
    return "";
}

std::optional<std::string> OllamaAdapter::generateRawResponse(const std::string& systemPrompt, const std::string& userContent) {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(120); // Longer timeout because orchestration chains might timeout otherwise, though here it's per call.

    json requestData = {
        {"model", m_model},
        {"prompt", systemPrompt + "\n\nTexto:\n" + userContent},
        {"stream", false}
    };

    auto res = cli.Post("/api/generate", requestData.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto body = json::parse(res->body);
            return body.value("response", "");
        } catch (...) {}
    }
    return std::nullopt;
}

std::optional<domain::Insight> OllamaAdapter::processRawThought(const std::string& rawContent, std::function<void(std::string)> statusCallback) {
    if (statusCallback) statusCallback("Iniciando processamento...");
    
    std::string finalContent;
    std::vector<std::string> tags = {"#AutoGenerated"};

    if (m_currentPersona == domain::AIPersona::Orquestrador) {
        if (statusCallback) statusCallback("Orquestrador: Diagnosticando...");
        tags.push_back("#Orchestrated");
        // 1. Orchestration Step
        std::string orquestradorPrompt = getSystemPrompt(domain::AIPersona::Orquestrador);
        auto planOpt = generateRawResponse(orquestradorPrompt, rawContent);
        
        if (!planOpt) return std::nullopt; // Failed to plan
        
        std::string plan = *planOpt;
        std::vector<domain::AIPersona> sequence;
        
        // Parse "SEQUENCE: Brainstormer, AnalistaCognitivo"
        if (plan.find("SEQUENCE:") != std::string::npos) {
            std::string seq = plan.substr(plan.find("SEQUENCE:") + 9);
            std::stringstream ss(seq);
            std::string segment;
            while (std::getline(ss, segment, ',')) {
                 // Trim
                 segment.erase(0, segment.find_first_not_of(" \t\n\r"));
                 size_t end = segment.find_last_not_of(" \t\n\r");
                 if (end != std::string::npos) segment.erase(end + 1);
                 
                 if (segment == "Brainstormer") sequence.push_back(domain::AIPersona::Brainstormer);
                 else if (segment == "AnalistaCognitivo") sequence.push_back(domain::AIPersona::AnalistaCognitivo);
                 else if (segment == "SecretarioExecutivo") sequence.push_back(domain::AIPersona::SecretarioExecutivo);
            }
        }
        
        // Fallback if empty or parsing failed
        if (sequence.empty()) {
            sequence.push_back(domain::AIPersona::AnalistaCognitivo);
        }
        
        // 2. Execution Pipeline
        std::string currentText = rawContent;
        for (auto persona : sequence) {
            std::string pName = (persona == domain::AIPersona::Brainstormer) ? "Brainstormer" :
                                (persona == domain::AIPersona::AnalistaCognitivo) ? "Analista Cognitivo" : "Secretário";
            if (statusCallback) statusCallback("Executando: " + pName + "...");
            
            std::string pPrompt = getSystemPrompt(persona);
            auto res = generateRawResponse(pPrompt, currentText);
            if (res) {
                currentText = *res;
            }
        }
        finalContent = currentText;

    } else {
        std::string pName = (m_currentPersona == domain::AIPersona::Brainstormer) ? "Brainstormer" :
                            (m_currentPersona == domain::AIPersona::AnalistaCognitivo) ? "Analista Cognitivo" : 
                            (m_currentPersona == domain::AIPersona::SecretarioExecutivo) ? "Secretário Executivo" : "IA";
        if (statusCallback) statusCallback("Rodando: " + pName + "...");

        // Direct Execution
        std::string prompt = getSystemPrompt(m_currentPersona);
        auto res = generateRawResponse(prompt, rawContent);
        if (!res) return std::nullopt;
        finalContent = *res;
    }

    if (statusCallback) statusCallback("Finalizando...");

    // Metadata & Return
    try {
        // Timestamp generation
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        std::tm localTime = ToLocalTime(in_time_t);
        ss << std::put_time(&localTime, "%Y-%m-%d %X");

        domain::Insight::Metadata meta;
        meta.id = std::to_string(in_time_t);
        meta.date = ss.str();
        meta.tags = tags;

        // Extract Title from response
        std::string titlePrefix = "# Título: ";
        size_t titlePos = finalContent.find(titlePrefix);
        if (titlePos != std::string::npos) {
            size_t startInfo = titlePos + titlePrefix.length();
            size_t endOfLine = finalContent.find('\n', startInfo);
            if (endOfLine != std::string::npos) {
                meta.title = finalContent.substr(startInfo, endOfLine - startInfo);
            } else {
                 meta.title = finalContent.substr(startInfo);
            }
            // Trim trailing CR if present
            if (!meta.title.empty() && meta.title.back() == '\r') {
                meta.title.pop_back();
            }
        } else {
             meta.title = "Structured Thought";
             if (m_currentPersona == domain::AIPersona::Orquestrador) meta.title += " (Orch)";
        }

        return domain::Insight(meta, finalContent);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> OllamaAdapter::consolidateTasks(const std::string& tasksMarkdown) {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(60);

    json requestData = {
        {"model", m_model},
        {"prompt",
            "Você é um consolidadores de tarefas. Receberá uma lista de tarefas com checkboxes, "
            "possivelmente duplicadas ou com redações semelhantes.\n\n"
            "REGRAS RÍGIDAS DE SAÍDA:\n"
            "1. Retorne APENAS linhas no formato de checkbox: \"- [ ] Descrição da tarefa\".\n"
            "2. Não inclua cabeçalhos, explicações, nem blocos de código.\n"
            "3. Remova duplicatas e unifique tarefas semanticamente equivalentes.\n"
            "4. Reescreva para uma redação clara, curta e acionável.\n"
            "5. Se houver estados diferentes para tarefas equivalentes, use o estado mais avançado: "
            "feito (- [x]) > em andamento (- [/]) > a fazer (- [ ]).\n"
            "6. Não invente novas tarefas.\n\n"
            "Tarefas a consolidar:\n\n" + tasksMarkdown
        },
        {"stream", false}
    };

    auto res = cli.Post("/api/generate", requestData.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto body = json::parse(res->body);
            return body.value("response", "");
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

} // namespace ideawalker::infrastructure
