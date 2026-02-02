#include "ui/UiFileBrowser.hpp"
#include "imgui.h"
#include <filesystem>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdio>

namespace ideawalker::ui {


std::filesystem::path ResolveBrowsePath(const char* buffer, const std::string& fallbackRoot) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path current;
    if (buffer && buffer[0] != '\0') {
        current = fs::path(buffer);
    } else if (!fallbackRoot.empty()) {
        current = fs::path(fallbackRoot);
    } else {
        current = fs::current_path(ec);
    }

    if (current.empty()) {
        current = fs::current_path(ec);
    }

    if (!fs::exists(current, ec) || !fs::is_directory(current, ec)) {
        fs::path parent = current.parent_path();
        if (!parent.empty() && fs::exists(parent, ec) && fs::is_directory(parent, ec)) {
            current = parent;
        }
    }

    if (current.empty()) {
        current = fs::current_path(ec);
    }

    return current;
}

void SetPathBuffer(char* buffer, size_t bufferSize, const std::filesystem::path& path) {
    if (!buffer || bufferSize == 0) {
        return;
    }
    std::string value = path.string();
    std::snprintf(buffer, bufferSize, "%s", value.c_str());
}

std::vector<std::filesystem::path> GetRootShortcuts() {
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    std::error_code ec;

#if defined(_WIN32)
    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        std::string root = std::string(1, drive) + ":\\";
        if (fs::exists(root, ec)) {
            roots.emplace_back(root);
        }
    }
#elif defined(__APPLE__)
    const char* candidates[] = { "/", "/Volumes", "/Users" };
    for (const char* path : candidates) {
        if (fs::exists(path, ec)) {
            roots.emplace_back(path);
        }
    }
#else
    const char* candidates[] = { "/", "/mnt", "/media", "/run/media", "/home" };
    for (const char* path : candidates) {
        if (fs::exists(path, ec)) {
            roots.emplace_back(path);
        }
    }
#endif

    if (roots.empty()) {
        roots.emplace_back("/");
    }
    return roots;
}

bool DrawFolderBrowser(const char* id, char* pathBuffer, size_t bufferSize, const std::string& fallbackRoot) {
    namespace fs = std::filesystem;
    ImGui::PushID(id);

    fs::path current = ResolveBrowsePath(pathBuffer, fallbackRoot);
    bool updated = false;

    ImGui::Text("Localizar:");
    ImGui::SameLine();
    ImGui::TextUnformatted(current.string().c_str());

    if (ImGui::Button("Subir")) {
        if (current.has_parent_path()) {
            current = current.parent_path();
            updated = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Usar Atual")) {
        updated = true;
    }

    ImGui::Separator();
    ImGui::Text("Raízes:");
    if (ImGui::BeginChild("Roots", ImVec2(0, 70), true)) {
        auto roots = GetRootShortcuts();
        for (const auto& root : roots) {
            std::string label = root.string();
            if (ImGui::Selectable(label.c_str())) {
                current = root;
                updated = true;
            }
        }
    }
    ImGui::EndChild();

    ImGui::Text("Pastas:");
    if (ImGui::BeginChild("FolderList", ImVec2(0, 200), true)) {
        std::error_code ec;
        if (!fs::exists(current, ec) || !fs::is_directory(current, ec)) {
            ImGui::TextDisabled("Pasta não disponível.");
        } else {
            std::vector<fs::directory_entry> entries;
            for (const auto& entry : fs::directory_iterator(current, ec)) {
                if (entry.is_directory(ec)) {
                    entries.push_back(entry);
                }
            }
            if (ec) {
                ImGui::TextDisabled("Não foi possível ler a pasta.");
            } else {
                std::sort(entries.begin(), entries.end(),
                          [](const fs::directory_entry& a, const fs::directory_entry& b) {
                              return a.path().filename().string() < b.path().filename().string();
                          });
                for (const auto& entry : entries) {
                    std::string name = entry.path().filename().string();
                    if (name.empty()) {
                        name = entry.path().string();
                    }
                    if (ImGui::Selectable(name.c_str())) {
                        current = entry.path();
                        updated = true;
                    }
                }
            }
        }
    }
    ImGui::EndChild();

    if (updated) {
        SetPathBuffer(pathBuffer, bufferSize, current);
    }

    ImGui::PopID();
    return updated; 
}

bool DrawFileBrowser(const char* id, char* pathBuffer, size_t bufferSize, const std::string& fallbackRoot) {
    namespace fs = std::filesystem;
    ImGui::PushID(id);

    fs::path current = ResolveBrowsePath(pathBuffer, fallbackRoot);
    if (!fs::is_directory(current)) {
        if (current.has_parent_path()) current = current.parent_path();
        else current = "/";
    }

    bool updated = false;
    bool confirmed = false;

    ImGui::Text("Localizar:");
    ImGui::SameLine();
    ImGui::TextUnformatted(current.string().c_str());

    if (ImGui::Button("Subir")) {
        if (current.has_parent_path()) {
            current = current.parent_path();
            updated = true;
        }
    }
    
    ImGui::Separator();
    
    ImGui::Text("Arquivos:");
    if (ImGui::BeginChild("FileList", ImVec2(0, 300), true)) {
        std::error_code ec;
        if (!fs::exists(current, ec) || !fs::is_directory(current, ec)) {
            ImGui::TextDisabled("Folder not available.");
        } else {
            std::vector<fs::directory_entry> entries;
            for (const auto& entry : fs::directory_iterator(current, ec)) {
                entries.push_back(entry);
            }
            std::sort(entries.begin(), entries.end(),
                      [](const fs::directory_entry& a, const fs::directory_entry& b) {
                          if (a.is_directory() != b.is_directory()) 
                              return a.is_directory() > b.is_directory();
                          return a.path().filename().string() < b.path().filename().string();
                      });

            std::string currentSelection = pathBuffer;

            for (const auto& entry : entries) {
                std::string name = entry.path().filename().string();
                if (entry.is_directory()) name = "[DIR] " + name;
                else name = "📄 " + name;

                bool isSelected = false;
                if (!entry.is_directory()) {
                     if (currentSelection == entry.path().string()) {
                         isSelected = true;
                     }
                }

                if (ImGui::Selectable(name.c_str(), isSelected)) {
                    if (entry.is_directory()) {
                        current = entry.path();
                        updated = true;
                    } else {
                        SetPathBuffer(pathBuffer, bufferSize, entry.path());
                        confirmed = true;
                    }
                }
            }
        }
    }
    ImGui::EndChild();

    if (updated) {
        SetPathBuffer(pathBuffer, bufferSize, current);
    }

    ImGui::PopID();
    return confirmed;
}

} // namespace ideawalker::ui
