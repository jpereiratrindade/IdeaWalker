#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "application/ConversationService.hpp"
#include "application/AppServices.hpp" // For ContextBundle
#include "imgui.h"

namespace ideawalker::ui {

struct CommandResult {
    int exitCode = 0;
    std::string output;
};

// IO / Strings
bool WriteTextFile(const std::filesystem::path& path, const std::string& content);
bool ReadTextFile(const std::filesystem::path& path, std::string& contentOut);
std::string TrimWhitespace(std::string text);
std::string ToSingleLineCompact(const std::string& text, std::size_t maxChars);
std::string FileExtensionLower(const std::filesystem::path& filePath);
std::string SlugifyForFilename(const std::string& title);

// Shell
std::string EscapeForDoubleQuotes(std::string value);
std::string ShellQuoteSingle(std::string value);
int ExtractExitCode(int pcloseStatus);
CommandResult RunShellCommandInWorkspace(const std::filesystem::path& workspaceRoot, const std::string& cmd);

// ImGui Helpers
int TextEditCallback(ImGuiInputTextCallbackData* data);
bool InputTextMultilineString(const char* label, std::string* str, const ImVec2& size);

// Dates
std::string CurrentDateIso();
std::string CurrentTimestampCompact();

// DocOps Core Rules
bool IsIgnoredDirectoryName(const std::string& name);
bool IsEditableDocumentPath(const std::filesystem::path& filePath);
std::filesystem::path ResolveTargetPath(const std::filesystem::path& wsRoot, const char* target);
std::filesystem::path FindFileByNameInWorkspace(const std::filesystem::path& wsRoot, const std::string& filename);
std::vector<std::filesystem::path> ListEditableDocuments(const std::filesystem::path& wsRoot);

// Commands
std::string BuildDocTypeCommand(const std::filesystem::path& workspaceRoot, const std::filesystem::path& relativeFile, bool buildMode);

// Parsing / Patches
bool LooksLikeUnifiedDiff(const std::string& text);
std::string ExtractFencedCodeBlock(const std::string& text, const char* languageHint);
std::string ExtractBetweenMarkers(const std::string& text, const std::string& beginMarker, const std::string& endMarker);
std::string ExtractUnifiedDiffCandidate(const std::string& assistantText);
bool ApplyUnifiedDiffPatch(const std::filesystem::path& workspaceRoot, const std::string& patchText, CommandResult& resultOut);
std::string ExtractAssistantPayloadForDocument(const std::string& assistantText);
std::string GetLastAssistantResponse(application::ConversationService* conversationService);

// Generators
std::string BuildTargetFileTemplate(const std::filesystem::path& targetPath, const char* reason, const char* adrRef, const char* profile);
bool EnsureDocOpsContractSkeleton(const std::filesystem::path& wsRoot, std::string& errorOut);
std::vector<std::string> ListAdrIds(const std::filesystem::path& wsRoot);
int NextAdrNumber(const std::vector<std::string>& ids);
std::string BuildAdrDraft(const std::string& adrId, const std::string& title, const std::string& decisionType);
application::ContextBundle BuildDocOpsFallbackBundle(const std::filesystem::path& workspace, const char* targetFile, const char* reason, const char* adrRef, const char* profile);
std::string BuildDirectEditPrompt(const std::string& profile, const std::string& targetFile, const char* reason, const char* adrRef, const std::string& instruction, const std::string& currentContent);
std::string BuildConformityEvaluationDraft(const std::string& dateIso, const std::string& author, const std::string& base, const std::string& scope);

} // namespace ideawalker::ui
