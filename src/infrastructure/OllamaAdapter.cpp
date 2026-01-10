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
    : m_host(host), m_port(port) {
    detectBestModel();
}

void OllamaAdapter::detectBestModel() {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(5); // Short timeout for detection
    
    auto res = cli.Get("/api/tags");
    if (res && res->status == 200) {
        try {
            auto body = json::parse(res->body);
            if (body.contains("models") && body["models"].is_array()) {
                std::vector<std::string> availableModels;
                for (const auto& item : body["models"]) {
                    if (item.contains("name")) {
                        availableModels.push_back(item["name"].get<std::string>());
                    }
                }

                if (availableModels.empty()) return;

                // Priority Hierarchy
                const std::vector<std::string> priorities = {
                    "qwen2.5:7b",
                    "qwen2.5", 
                    "llama3", 
                    "mistral",
                    "gemma",
                    "deepseek-coder"
                };

                for (const auto& priority : priorities) {
                    for (const auto& model : availableModels) {
                        if (model.find(priority) != std::string::npos) {
                            m_model = model;
                            std::cout << "[OllamaAdapter] Auto-selected model: " << m_model << std::endl;
                            return;
                        }
                    }
                }

                // Fallback: Pick the first available
                m_model = availableModels[0];
                std::cout << "[OllamaAdapter] Fallback model: " << m_model << std::endl;
            }
        } catch (...) {
            std::cerr << "[OllamaAdapter] Error parsing models. Using default: " << m_model << std::endl;
        }
    } else {
        std::cerr << "[OllamaAdapter] Failed to list models. Is Ollama running? Keeping default: " << m_model << std::endl;
    }
}

std::string OllamaAdapter::getSystemPrompt(domain::AIPersona persona) {
    switch (persona) {
    case domain::AIPersona::AnalistaCognitivo:
        return 
            "Você é um Analista Cognitivo e Estrategista de Sistemas Complexos. Sua função é mapear a estrutura real do documento.\n\n"
            "MODO DOCUMENTAL ATIVO:\n"
            "1. EPISTEMIC HUMILITY: Descreva APENAS o que está no texto. Não invente autores, títulos, instituições ou datas que não estejam explicitamente escritos.\n"
            "2. PROIBIDO: Criar 'obras fantasmas'. Se o texto fala sobre 'tempo' e 'resiliência', NÃO assuma que é uma tese chamada 'O Tempo das Coisas' a menos que isso esteja escrito na capa.\n"
            "3. Se o texto for um fragmento ou rascunho, trate-o como tal.\n\n"
            "REGRAS RÍGIDAS DE SAÍDA:\n"
            "1. NÃO use blocos de código. Retorne apenas texto cru.\n"
            "2. Se identificar similaridade com outras obras, use a seção 'Ressonância'.\n"
            "3. Mantenha os headers exatos.\n\n"
            "ESTRUTURA OBRIGATÓRIA:\n"
            "# Título: [Título Conceitual Baseado no Conteúdo]\n\n"
            "## Tensão Central\n"
            "(Qual é o conflito real ou tema abordado?)\n\n"
            "## Análise Documental\n"
            "(O que é este documento? Um rascunho? Um artigo? Uma anotação?)\n\n"
            "## Decisões & Caminhos\n"
            "- (O que o texto propõe ou descarta?)\n\n"
            "## Ressonância (Similaridade)\n"
            "⚠️ (Alerta: O texto dialoga com [Autor/Teoria], mas não criar confusão de autoria.)\n\n"
            "## Conexões Sugeridas\n"
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

    case domain::AIPersona::Tecelao:
        return
            "Você é o TECELÃO (The Weaver). Sua função é encontrar pontes e conexões emergentes entre diferentes notas.\n"
            "Você não deve resumir, mas sim mapear como uma nova ideia se ancora ou desafia o conhecimento existente.\n\n"
            "REGRAS RÍGIDAS DE SAÍDA:\n"
            "1. NÃO use blocos de código. Retorne apenas texto cru.\n"
            "2. Seja breve e provocativo.\n"
            "3. Foque em CONEXÕES não óbvias.\n\n"
            "ESTRUTURA OBRIGATÓRIA:\n"
            "🔗 Conexão Sugerida: [[Título da Nota]]\n"
            "Raciocínio: (Uma frase curta explicando a ponte epistemológica)\n"
            "Pergunta: (Uma pergunta de verificação para o usuário)";
    }
    return "";
}

std::optional<std::string> OllamaAdapter::generateRawResponse(const std::string& systemPrompt,
                                                              const std::string& userContent,
                                                              bool forceJson) {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(600); // Increased to 600s (10 min) for CPU inference

    std::cout << "[OllamaAdapter] Sending request to " << m_model << " (JSON=" << forceJson << ")" 
              << " PromptSize=" << (systemPrompt.size() + userContent.size()) << " bytes" << std::endl;

    json requestData = {
        {"model", m_model},
        {"prompt", systemPrompt + "\n\nTexto:\n" + userContent},
        {"stream", false}
    };
    if (forceJson) {
        requestData["format"] = "json";
        std::cout << "[OllamaAdapter] Enforcing JSON format." << std::endl;
    }

    auto res = cli.Post("/api/generate", requestData.dump(), "application/json");
    // ... [Rest of method remains same, but we need to include it or be careful with replace_file_content scope]
    if (res) {
        if (res->status == 200) {
            try {
                std::cout << "[OllamaAdapter] Response received (" << res->body.size() << " bytes)" << std::endl;
                auto body = json::parse(res->body);
                if (body.contains("response")) {
                    return body.value("response", "");
                } else {
                    std::cerr << "[OllamaAdapter] Response JSON missing 'response' field: " << res->body << std::endl;
                }
            } catch (const std::exception& e) {
                 std::cerr << "[OllamaAdapter] JSON Parse Error: " << e.what() << "\nBody: " << res->body << std::endl;
            }
        } else {
            std::cerr << "[OllamaAdapter] HTTP Error " << res->status << ": " << res->body << std::endl;
        }
    } else {
        auto err = res.error();
        std::cerr << "[OllamaAdapter] Connection failed. Error code: " << static_cast<int>(err) << std::endl;
    }
    return std::nullopt;
}


std::optional<domain::Insight> OllamaAdapter::processRawThought(const std::string& rawContent, bool fastMode, std::function<void(std::string)> statusCallback) {
    if (statusCallback) statusCallback(fastMode ? "Iniciando modo rápido (CPU Optimization)..." : "Iniciando processamento...");
    
    std::string finalContent;
    std::vector<std::string> tags = {"#AutoGenerated"};
    std::vector<domain::CognitiveSnapshot> snapshots;

    if (fastMode) {
        // Fast Mode: Single Pass (Direct Analysis)
        if (statusCallback) statusCallback("Modo Rápido: Analisando diretamente...");
        tags.push_back("#FastMode");
        
        std::string prompt = getSystemPrompt(domain::AIPersona::AnalistaCognitivo);
        auto res = generateRawResponse(prompt, rawContent, false);
        
        if (res) {
            finalContent = *res;
            domain::CognitiveSnapshot snap;
            snap.persona = domain::AIPersona::AnalistaCognitivo;
            snap.textInput = rawContent;
            snap.textOutput = finalContent;
            snap.state = domain::CognitiveState::Convergent;
            
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ttss;
            std::tm localTimeSnap = ToLocalTime(in_time_t);
            ttss << std::put_time(&localTimeSnap, "%Y-%m-%d %X");
            snap.timestamp = ttss.str();
            
            snapshots.push_back(snap);
        } else {
            return std::nullopt;
        }

    } else {
        // Standard Mode: Full Orchestration
        if (statusCallback) statusCallback("Orquestrador: Diagnosticando...");
        tags.push_back("#Orchestrated");
        std::string orquestradorPrompt = getSystemPrompt(domain::AIPersona::Orquestrador);
        auto planOpt = generateRawResponse(orquestradorPrompt, rawContent, true);
        
        if (!planOpt) return std::nullopt;
        
        std::string plan = *planOpt;
        std::vector<domain::AIPersona> sequence;
        
        try {
            auto data = json::parse(plan);
            if (data.contains("primary_tag") && data["primary_tag"].is_string()) {
                std::string tagLine = data["primary_tag"].get<std::string>();
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
        } catch (...) {
            // Fallback parsing or default sequence
        }

        if (sequence.empty()) {
            sequence.push_back(domain::AIPersona::AnalistaCognitivo);
        }

        std::string currentText = rawContent;
        for (auto persona : sequence) {
            std::string pName = (persona == domain::AIPersona::Brainstormer) ? "Brainstormer" :
                                (persona == domain::AIPersona::AnalistaCognitivo) ? "Analista Cognitivo" : "Secretário";
            if (statusCallback) statusCallback("Executando: " + pName + "...");
            
            std::string pPrompt = getSystemPrompt(persona);
            auto res = generateRawResponse(pPrompt, currentText, false);
            
            domain::CognitiveSnapshot snap;
            snap.persona = persona;
            snap.textInput = currentText;
            if (persona == domain::AIPersona::Brainstormer) snap.state = domain::CognitiveState::Divergent;
            else if (persona == domain::AIPersona::AnalistaCognitivo) snap.state = domain::CognitiveState::Convergent;
            else if (persona == domain::AIPersona::SecretarioExecutivo) snap.state = domain::CognitiveState::Closing;
            else snap.state = domain::CognitiveState::Unknown;

            if (res) {
                currentText = *res;
                snap.textOutput = currentText;
            } else {
                snap.textOutput = "[ERROR: Failed to generate]";
            }
            
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ttss;
            std::tm localTimeSnap = ToLocalTime(in_time_t);
            ttss << std::put_time(&localTimeSnap, "%Y-%m-%d %X");
            snap.timestamp = ttss.str();
            
            snapshots.push_back(snap);
        }
        finalContent = currentText;
    }

    if (statusCallback) statusCallback("Finalizando...");

    try {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        std::tm localTime = ToLocalTime(in_time_t);
        ss << std::put_time(&localTime, "%Y-%m-%d %X");

        domain::Insight::Metadata meta;
        meta.id = std::to_string(in_time_t);
        meta.date = ss.str();
        meta.tags = tags;

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
            if (!meta.title.empty() && meta.title.back() == '\r') meta.title.pop_back();
        } else {
             meta.title = "Thought " + ss.str();
        }

        domain::Insight insight(meta, finalContent);
        for(const auto& snap : snapshots) insight.addSnapshot(snap);
        return insight;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> OllamaAdapter::chat(const std::vector<domain::AIService::ChatMessage>& history, bool stream) {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(600); // 10 min

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
    cli.set_read_timeout(600); // 10 min

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

std::vector<float> OllamaAdapter::getEmbedding(const std::string& text) {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(180); // 3 min

    json requestData = {
        {"model", m_model},
        {"prompt", text}
    };

    auto res = cli.Post("/api/embeddings", requestData.dump(), "application/json");
    if (res && res->status == 200) {
        try {
            auto body = json::parse(res->body);
            if (body.contains("embedding") && body["embedding"].is_array()) {
                return body["embedding"].get<std::vector<float>>();
            }
        } catch (...) {}
    }
    return {};
}

} // namespace ideawalker::infrastructure
