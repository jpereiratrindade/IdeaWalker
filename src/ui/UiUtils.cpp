#include "ui/UiUtils.hpp"
#include <iostream>

namespace ideawalker::ui {

std::tm ToLocalTime(std::time_t tt) {
    std::tm tm = {};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}

static int TextEditCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size, ImGuiInputTextFlags flags) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    flags |= ImGuiInputTextFlags_NoHorizontalScroll; // Enable Word Wrap
    if (str->capacity() == 0) {
        str->reserve(1024);
    }
    return ImGui::InputTextMultiline(label, str->data(), str->capacity() + 1, size, flags, TextEditCallback, str);
}

bool TaskCard(const char* id, const std::string& text, float width) {
    ImGuiStyle& style = ImGui::GetStyle();
    float paddingX = style.FramePadding.x;
    float paddingY = style.FramePadding.y;
    float wrapWidth = width - paddingX * 2.0f;
    if (wrapWidth < 1.0f) {
        wrapWidth = 1.0f;
    }

    ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrapWidth);
    float height = textSize.y + paddingY * 2.0f;

    ImGui::PushID(id);
    ImGui::InvisibleButton("##task", ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();

    ImU32 bg = ImGui::GetColorU32(active ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, max, bg, 4.0f);
    drawList->AddRect(min, max, border, 4.0f);

    ImVec2 textPos(min.x + paddingX, min.y + paddingY);
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos,
                      ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), nullptr, wrapWidth);

    ImGui::PopID();
    return clicked;
}

} // namespace ideawalker::ui
