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

std::optional<domain::Insight> OllamaAdapter::processRawThought(const std::string& rawContent) {
    httplib::Client cli(m_host, m_port);
    cli.set_read_timeout(60); // LLMs take time

    std::string prompt;

    switch (m_currentPersona) {
    case domain::AIPersona::AnalistaCognitivo:
        prompt = 
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
            "- [[Conceito Relacionado]]\n\n"
            "Texto a processar:\n\n" + rawContent;
        break;

    case domain::AIPersona::SecretarioExecutivo:
        prompt = 
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
            "- [ ] (Ação concreta 2)\n\n"
            "Texto a processar:\n\n" + rawContent;
        break;

    case domain::AIPersona::Brainstormer:
        prompt = 
            "Você é um Facilitador Criativo e Brainstormer. Sua função é expandir as ideias, sugerir conexões inusitadas e explorar possibilidades.\n\n"
            "REGRAS RÍGIDAS DE SAÍDA:\n"
            "1. NÃO use blocos de código (```markdown). Retorne apenas o texto cru.\n"
            "2. Seja encorajador e expansivo.\n"
            "3. Sugira caminhos alternativos.\n"
            "4. Mantenha os headers exatos como abaixo.\n\n"
            "ESTRUTURA OBRIGATÓRIA:\n"
            "# Título: [Título Criativo]\n\n"
            "## A Ideia Central\n"
            "(Qual é a semente dessa ideia?)\n\n"
            "## Possibilidades e Expansões\n"
            "- (Onde isso pode chegar?)\n\n"
            "## Conexões Laterais\n"
            "- [[Conceito Inusitado]]\n\n"
            "## Próximos Passos (Exploração)\n"
            "- [ ] (Experimento sugerido)\n\n"
            "Texto a processar:\n\n" + rawContent;
        break;
    }

    json requestData = {
        {"model", m_model},
        {"prompt", prompt},
        {"stream", false}
    };

    auto res = cli.Post("/api/generate", requestData.dump(), "application/json");

    if (res && res->status == 200) {
        try {
            auto body = json::parse(res->body);
            std::string responseText = body.value("response", "");

            // Timestamp generation
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            std::tm localTime = ToLocalTime(in_time_t);
            ss << std::put_time(&localTime, "%Y-%m-%d %X");

            domain::Insight::Metadata meta;
            meta.id = std::to_string(in_time_t);
            meta.date = ss.str();
            meta.tags = {"#AutoGenerated"};

            // Extract Title from response
            std::string titlePrefix = "# Título: ";
            size_t titlePos = responseText.find(titlePrefix);
            if (titlePos != std::string::npos) {
                size_t startInfo = titlePos + titlePrefix.length();
                size_t endOfLine = responseText.find('\n', startInfo);
                if (endOfLine != std::string::npos) {
                    meta.title = responseText.substr(startInfo, endOfLine - startInfo);
                } else {
                     meta.title = responseText.substr(startInfo);
                }
                // Trim trailing CR if present
                if (!meta.title.empty() && meta.title.back() == '\r') {
                    meta.title.pop_back();
                }
            } else {
                 meta.title = "Structured Thought";
            }

            return domain::Insight(meta, responseText);
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
