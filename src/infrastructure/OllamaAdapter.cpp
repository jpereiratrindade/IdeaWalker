/**
 * @file OllamaAdapter.cpp
 * @brief Implementation of the OllamaAdapter class.
 */
#include "infrastructure/OllamaAdapter.hpp"
#include "domain/CognitiveModel.hpp"
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

std::string NormalizeToken(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}

bool PersonaFromToken(const std::string& value, domain::AIPersona& outPersona) {
    std::string token = NormalizeToken(value);
    if (token == "brainstormer") {
        outPersona = domain::AIPersona::Brainstormer;
        return true;
    }
    if (token == "analistacognitivo") {
        outPersona = domain::AIPersona::AnalistaCognitivo;
        return true;
    }
    if (token == "secretarioexecutivo") {
        outPersona = domain::AIPersona::SecretarioExecutivo;
        return true;
    }
    return false;
}

} // namespace

OllamaAdapter::OllamaAdapter(const std::string& host, int port)
    : m_host(host), m_port(port) {}

OllamaAdapter::OllamaAdapter(const std::string& host, int port)
    : m_host(host), m_port(port) {}

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
            "2. Seja direto, conciso e operacional. Evite abstrações e interpretações conceituais.\n"
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
            "3. Definir uma TAG cognitiva dominante.\n\n"
            "HEURÍSTICAS DE DECISÃO:\n"
            "- Se o texto contiver REPETIÇÕES, FRASES METACOGNITIVAS ('isso me trava', 'não consigo') ou CAOS -> Comece com BRAINSTORMER.\n"
            "- Se o texto tiver OBJETOS CONCEITUAIS CLAROS, Modelos ou Matrizes -> Pule Brainstormer, comece com ANALISTA.\n"
            "- Se for apenas uma lista de pendências -> Apenas SECRETÁRIO.\n\n"
            "Perfis Disponíveis: Brainstormer (expandir/destravar), AnalistaCognitivo (estruturar/mapear tensão), SecretarioExecutivo (fechar/resumir).\n"
            "Tags Sugeridas: #Divergent, #Integrative, #Closing, #Chaotic, #Structured.\n\n"
            "REGRAS DE SAÍDA:\n"
            "- Retorne APENAS um JSON válido, sem texto extra.\n\n"
            "FORMATO DE SAÍDA (JSON):\n"
            "{ \"sequence\": [\"Brainstormer\", \"AnalistaCognitivo\"], \"primary_tag\": \"#Divergent\" }";
    }
    return "";
}

std::optional<std::string> OllamaAdapter::generateRawResponse(const std::string& systemPrompt,
                                                              const std::string& userContent,
                                                              bool forceJson) {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(120); // Longer timeout because orchestration chains might timeout otherwise, though here it's per call.

    json requestData = {
        {"model", m_model},
        {"prompt", systemPrompt + "\n\nTexto:\n" + userContent},
        {"stream", false}
    };
    if (forceJson) {
        requestData["format"] = "json";
    }

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
    std::vector<domain::CognitiveSnapshot> snapshots;

    // Always start with Orchestrator logic (Autonomous)
    if (true) {
        if (statusCallback) statusCallback("Orquestrador: Diagnosticando...");
        tags.push_back("#Orchestrated");
        // 1. Orchestration Step
        std::string orquestradorPrompt = getSystemPrompt(domain::AIPersona::Orquestrador);
        auto planOpt = generateRawResponse(orquestradorPrompt, rawContent, true);
        
        if (!planOpt) return std::nullopt; // Failed to plan
        
        std::string plan = *planOpt;
        std::vector<domain::AIPersona> sequence;
        
        bool parsedJson = false;
        try {
            auto data = json::parse(plan);
            if (data.contains("primary_tag") && data["primary_tag"].is_string()) {
                std::string tagLine = data["primary_tag"].get<std::string>();
                if (!tagLine.empty()) tags.push_back(tagLine);
            } else if (data.contains("tag") && data["tag"].is_string()) {
                std::string tagLine = data["tag"].get<std::string>();
                if (!tagLine.empty()) tags.push_back(tagLine);
            }

            if (data.contains("sequence") && data["sequence"].is_array()) {
                for (const auto& item : data["sequence"]) {
                    if (!item.is_string()) continue;
                    domain::AIPersona persona;
                    if (PersonaFromToken(item.get<std::string>(), persona)) {
                        sequence.push_back(persona);
                    }
                }
            }
            parsedJson = true;
        } catch (...) {
            parsedJson = false;
        }

        if (!parsedJson) {
            // Fallback: Parse "SEQUENCE: ..." / "TAG: ..." text if model ignored JSON output.
            if (plan.find("TAG:") != std::string::npos) {
                std::string tagLine = plan.substr(plan.find("TAG:") + 4);
                size_t end = tagLine.find('\n');
                if (end != std::string::npos) tagLine = tagLine.substr(0, end);
                tagLine.erase(0, tagLine.find_first_not_of(" \t\n\r"));
                tagLine.erase(tagLine.find_last_not_of(" \t\n\r") + 1);
                if (!tagLine.empty()) tags.push_back(tagLine);
            }

            if (plan.find("SEQUENCE:") != std::string::npos) {
                std::string seq = plan.substr(plan.find("SEQUENCE:") + 9);
                size_t endOfSeq = seq.find('\n');
                if (endOfSeq != std::string::npos) seq = seq.substr(0, endOfSeq);
                
                std::stringstream ss(seq);
                std::string segment;
                while (std::getline(ss, segment, ',')) {
                    segment.erase(0, segment.find_first_not_of(" \t\n\r"));
                    size_t end = segment.find_last_not_of(" \t\n\r");
                    if (end != std::string::npos) segment.erase(end + 1);
                    domain::AIPersona persona;
                    if (PersonaFromToken(segment, persona)) {
                        sequence.push_back(persona);
                    }
                }
            }
        }
        
        // Fallback if empty or parsing failed
        if (sequence.empty()) {
            if (statusCallback) statusCallback("Orquestrador: Falha ao planejar. Usando fallback (Analista).");
            sequence.push_back(domain::AIPersona::AnalistaCognitivo);
        } else {
            if (statusCallback) {
                std::string seqStr;
                for (size_t i = 0; i < sequence.size(); ++i) {
                    seqStr += (sequence[i] == domain::AIPersona::Brainstormer) ? "Brainstormer" :
                              (sequence[i] == domain::AIPersona::AnalistaCognitivo) ? "Analista" : "Secretário";
                    if (i < sequence.size() - 1) seqStr += " -> ";
                }
                statusCallback("Orquestrador: Sequência definida [" + seqStr + "]");
            }
        }
        
        // 2. Execution Pipeline
        std::string currentText = rawContent;
        for (auto persona : sequence) {
            std::string pName = (persona == domain::AIPersona::Brainstormer) ? "Brainstormer" :
                                (persona == domain::AIPersona::AnalistaCognitivo) ? "Analista Cognitivo" : "Secretário";
            if (statusCallback) statusCallback("Executando: " + pName + "...");
            
            std::string pPrompt = getSystemPrompt(persona);
            auto res = generateRawResponse(pPrompt, currentText, false);
            
            // Snapshot recording
            domain::CognitiveSnapshot snap;
            snap.persona = persona;
            snap.textInput = currentText;
            snap.state = domain::CognitiveState::Unknown; // Refine later if state is inferred
            
            // Infer state from tags/Persona
            if (persona == domain::AIPersona::Brainstormer) snap.state = domain::CognitiveState::Divergent;
            else if (persona == domain::AIPersona::AnalistaCognitivo) snap.state = domain::CognitiveState::Convergent;
            else if (persona == domain::AIPersona::SecretarioExecutivo) snap.state = domain::CognitiveState::Closing;

            if (res) {
                currentText = *res;
                snap.textOutput = currentText;
            } else {
                snap.textOutput = "[ERROR: Failed to generate]";
            }
            
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
            snap.timestamp = ss.str();
            
            snapshots.push_back(snap);
        }
        finalContent = currentText;

        }
        finalContent = currentText;

    } else {
        // Fallback or explicit default (Orchestrator is now the default/only entry point for raw thoughts)
        // But if m_currentPersona was set... wait, we removed setPersona.
        // So we default to Orquestrador logic always for processRawThought.
        // The original code had an "else" block for direct execution.
        // Since we want autonomous pipeline, we should ALWAYS orchestrate or default to a safe fallback.
        // Let's assume Orquestrador IS the way.
        
        // HOWEVER, to be safe and cleaner, we just force the Orchestrator logic block above.
        // If we really need a single-shot execution, we can use chat() or internal helpers.
        // For now, I will remove the specific "AIPersona != Orquestrador" block because
        // the concept of "Current Persona" is gone. The system IS the Orchestrator at entry.
    }
        if (statusCallback) statusCallback("Executando: " + pName + "...");

        // Direct Execution
        std::string prompt = getSystemPrompt(m_currentPersona);
        auto res = generateRawResponse(prompt, rawContent, false);
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

        domain::Insight insight(meta, finalContent);
        for(const auto& snap : snapshots) {
            insight.addSnapshot(snap);
        }
        return insight;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> OllamaAdapter::chat(const std::vector<domain::AIService::ChatMessage>& history, bool stream) {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(120);

    json messagesJson = json::array();
    for (const auto& msg : history) {
        messagesJson.push_back({
            {"role", domain::AIService::ChatMessage::RoleToString(msg.role)},
            {"content", msg.content}
        });
    }

    json requestData = {
        {"model", m_model},
        {"messages", messagesJson},
        {"stream", stream}
    };

    auto res = cli.Post("/api/chat", requestData.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto body = json::parse(res->body);
            // Ollama /api/chat response: { "message": { "role": "assistant", "content": "..." }, ... }
            if (body.contains("message") && body["message"].contains("content")) {
                return body["message"]["content"].get<std::string>();
            }
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
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
