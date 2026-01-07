    // --- Settings Modal ---
    if (app.showSettingsModal) {
        ImGui::OpenPopup("Preferências");
    }
    if (ImGui::BeginPopupModal("Preferências", &app.showSettingsModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Configurações Gerais");
        ImGui::Separator();
        
        // AI Persona Selection
        ImGui::Text("🧠 Personalidade da IA");
        
        const char* personas[] = { "Analista Cognitivo", "Secretário Executivo", "Brainstormer" };
        int currentItem = static_cast<int>(app.currentPersona);
        
        if (ImGui::Combo("##persona", &currentItem, personas, IM_ARRAYSIZE(personas))) {
            app.currentPersona = static_cast<domain::AIPersona>(currentItem);
            if (app.organizerService) {
                app.organizerService->setAIPersona(app.currentPersona);
            }
        }
        
        ImGui::TextDisabled((currentItem == 0) ? "Focado em tensão, conflito e estratégia." : 
                            (currentItem == 1) ? "Focado em tarefas, resumo e eficiência." : 
                            "Focado em expansão, criatividade e divergência.");

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));
        
        if (ImGui::Button("Fechar", ImVec2(120, 0))) {
            app.showSettingsModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
