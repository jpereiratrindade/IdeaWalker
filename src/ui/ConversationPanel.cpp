/**
 * @file ConversationPanel.cpp
 * @brief Implementation of ConversationPanel.
 */

#include "ui/ConversationPanel.hpp"
#include "application/ConversationService.hpp"
// #include "application/OrganizerService.hpp" // Removed
#include "imgui.h"
#include <algorithm>

namespace {

int TextEditCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    flags |= ImGuiInputTextFlags_NoHorizontalScroll; // Enable Word Wrap
    if (str->capacity() == 0) {
        str->reserve(1024);
    }
    return ImGui::InputTextMultiline(label, str->data(), str->capacity() + 1, size, flags, TextEditCallback, str);
}

} // namespace

namespace ideawalker::ui {

void ConversationPanel::DrawContent(AppState& app) {
    if (!app.services.conversationService) {
        ImGui::Text("Service not available (No project open?)");
        return;
    }

    auto& service = *app.services.conversationService;
    std::string activeNoteId = app.ui.selectedFilename;
    
    // Dialogue Selection
    if (!app.ui.dialogueFiles.empty()) {
        ImGui::SetNextItemWidth(250.0f);
        if (ImGui::BeginCombo("##dialogue_select", app.ui.selectedDialogueIndex >= 0 ? app.ui.dialogueFiles[app.ui.selectedDialogueIndex].c_str() : "Selecionar diálogo anterior...")) {
            for (int i = 0; i < (int)app.ui.dialogueFiles.size(); i++) {
                bool isSelected = (app.ui.selectedDialogueIndex == i);
                if (ImGui::Selectable(app.ui.dialogueFiles[i].c_str(), isSelected)) {
                    app.ui.selectedDialogueIndex = i;
                    service.loadSession(app.ui.dialogueFiles[i]);
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Atualizar Lista")) {
            app.RefreshDialogueList();
        }
    }

    ImGui::Separator();

    // Header Info
    if (activeNoteId.empty() && !service.isSessionActive()) {
        ImGui::TextDisabled("Selecione uma nota para iniciar o contexto ou selecione um diálogo anterior.");
    } else {
        std::string currentContext = service.getCurrentNoteId();
        bool needsStart = (currentContext != activeNoteId);
        
        if (needsStart) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
            ImGui::TextWrapped("Contexto Inativo: %s", activeNoteId.c_str());
            ImGui::PopStyleColor();
            
            if (ImGui::Button("Iniciar Sessão de Diálogo")) {
                 if (app.services.contextAssembler) {
                     auto bundle = app.services.contextAssembler->assemble(activeNoteId, app.ui.selectedNoteContent);
                     service.startSession(bundle);
                 }
            }
        } else {
             ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[Contexto Ativo: %s]", currentContext.c_str());
             ImGui::SameLine();
             
             if (ImGui::SmallButton("Reiniciar")) {
                 if (app.services.contextAssembler) {
                     auto bundle = app.services.contextAssembler->assemble(activeNoteId, app.ui.selectedNoteContent);
                     service.startSession(bundle);
                 }
             }
        }
    }

    ImGui::Separator();

    // Calculate remaining height for history so Input stays at bottom
    float inputHeight = 60.0f;
    float availY = ImGui::GetContentRegionAvail().y;
    float historyHeight = availY - inputHeight;
    if (historyHeight < 100.0f) historyHeight = 100.0f; // Minimal visual safety

    // Chat History Area
    ImGui::BeginChild("ChatHistory", ImVec2(0, historyHeight), true);
    
    // Get copy of history (Thread-safe)
    auto history = service.getHistory();
    
    // Auto-scroll logic: if history size increased, scroll to bottom
    static size_t lastHistorySize = 0;
    bool newMessages = (history.size() > lastHistorySize);
    lastHistorySize = history.size();

    for (size_t i = 0; i < history.size(); ++i) {
        const auto& msg = history[i];
        if (msg.role == domain::AIService::ChatMessage::Role::System) continue;

        bool isUser = (msg.role == domain::AIService::ChatMessage::Role::User);
        std::string label = std::string("##msg_") + std::to_string(i);

        if (isUser) {
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Você:");
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "IA:");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton((std::string("Copiar##") + std::to_string(i)).c_str())) {
            ImGui::SetClipboardText(msg.content.c_str());
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(msg.content.c_str());
        ImGui::PopTextWrapPos();
        
        ImGui::Dummy(ImVec2(0, 5)); // Spacing
    }

    if (service.isThinking()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Pensando... (aguarde)");
        // Force scroll to see thinking
        ImGui::SetScrollHereY(1.0f);
    } else if (newMessages || app.ui.activeTab == -999) {
        ImGui::SetScrollHereY(1.0f);
        if (app.ui.activeTab == -999) app.ui.activeTab = 0;
    }
    
    ImGui::EndChild();

    // Input Area
    static std::string inputBuffer;
    bool send = false;
    bool isThinking = service.isThinking();
    
    if (service.isSessionActive() && !activeNoteId.empty()) {
        bool reclaimed_focus = false;
        
        ImGui::BeginDisabled(isThinking); // Disable input while thinking

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
        if (InputTextMultilineString("##chatinput", &inputBuffer, ImVec2(-60, inputHeight - 10), flags)) {
            send = true;
            reclaimed_focus = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Enviar", ImVec2(-1, inputHeight - 10))) {
            send = true;
            reclaimed_focus = true;
        }

        ImGui::EndDisabled();

        if (reclaimed_focus && !isThinking) {
            ImGui::SetKeyboardFocusHere(-1); // Auto focus back
        }

        if (send && !inputBuffer.empty() && !isThinking) {
            service.sendMessage(inputBuffer);
            inputBuffer.clear();
            // Scroll trigger handled by history size check next frame
        }
    } else {
        ImGui::TextDisabled("(Inicie a sessão para conversar)");
    }
}

} // namespace ideawalker::ui
