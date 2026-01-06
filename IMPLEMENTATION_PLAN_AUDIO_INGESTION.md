# ESPECIFICAÇÃO DE INCREMENTO TÉCNICO: INGESTÃO DE ÁUDIO VIA DRAG & DROP
# Projeto: Idea Walker (C++ / ImGui / DDD)
# Objetivo: Integrar script Python existente (WhisperX) ao fluxo C++ via Adapter

================================================================================
1. VISÃO GERAL DA MUDANÇA
================================================================================
Atualmente, a transcrição é manual (terminal). O objetivo é permitir que o usuário
arraste um arquivo .WAV para a janela do Idea Walker. O sistema deve:
1. Chamar o script Python em background.
2. Aguardar a conclusão.
3. Mover o arquivo .txt gerado para a pasta /inbox automaticamente.
4. Atualizar a UI.

================================================================================
2. DOMAIN LAYER (A Interface)
================================================================================
Arquivo: src/Domain/Ports/ITranscriptionService.hpp

Definir o contrato para serviços de transcrição, desacoplando a UI da implementação
específica (Python/Shell).

[Requisitos da Interface]
- Método: TranscribeAsync
- Parâmetros: 
  - audioPath (string)
  - onSuccess (callback com o caminho do arquivo de texto gerado)
  - onError (callback com mensagem de erro)
- Deve ser não-bloqueante (Assíncrono).

================================================================================
3. INFRASTRUCTURE LAYER (O Adaptador)
================================================================================
Arquivo: src/Infrastructure/Adapters/WhisperScriptAdapter.hpp

Implementar a chamada de sistema (`std::system`) para rodar o script Python existente.

[Lógica do Adaptador]
1. Construtor: Recebe caminhos absolutos para:
   - Python Executable (venv/bin/python3)
   - Script Transcritor (transcritor_simples.py)
   - Pasta Inbox de Destino (para onde o txt deve ir)

2. Método TranscribeAsync:
   - Dispara uma `std::thread` (detach).
   - Monta o comando Shell blindado contra Proxy:
     `export NO_PROXY=localhost && /path/to/python /path/to/script "audio.wav"`
   - Executa via `std::system()`.

3. Lógica de Pós-Processamento (File Move):
   - O script Python salva `nome_audio_transcricao.txt` na mesma pasta do áudio.
   - O Adaptador deve:
     a) Verificar se o arquivo .txt foi criado na origem.
     b) Mover este arquivo para `AppConfig.InboxPath`.
     c) Invocar `onSuccess` com o novo caminho já dentro do inbox.

================================================================================
4. UI LAYER (ImGui Implementation)
================================================================================
Arquivo: src/UI/Dashboard.cpp (ou main.cpp)

[Funcionalidade Drag & Drop]
- Usar `ImGui::BeginDragDropTarget()` na área do Dashboard.
- Aceitar payload tipo "FILES" (dependendo do backend SDL2, pode exigir lógica extra
  de evento SDL_DROPFILE no loop principal para capturar o caminho).

[Feedback Visual]
- Criar estado `bool isTranscribing`.
- Se `true`:
  - Exibir Spinner/Barra de Progresso (Indeterminado).
  - Bloquear botão de nova transcrição.
  - Exibir texto: "Processando áudio com WhisperX... (Isso pode demorar)".

================================================================================
5. EXEMPLO DE CÓDIGO (Snippet para o Adapter)
================================================================================

```cpp
// Lógica de Movimentação de Arquivo (std::filesystem)
namespace fs = std::filesystem;

if (result == 0) {
    // 1. Identificar arquivo gerado
    fs::path audioP(audioPath);
    std::string txtName = audioP.stem().string() + "_transcricao.txt";
    fs::path sourceTxt = audioP.parent_path() / txtName;
    
    // 2. Definir destino
    fs::path destTxt = fs::path(this->inboxPath) / txtName;

    // 3. Mover e Notificar
    if (fs::exists(sourceTxt)) {
        try {
            fs::rename(sourceTxt, destTxt); // Move para o Inbox
            onSuccess(destTxt.string());
        } catch (const fs::filesystem_error& e) {
            onError("Erro ao mover arquivo para Inbox: " + std::string(e.what()));
        }
    } else {
        onError("Script rodou, mas arquivo .txt não encontrado na origem.");
    }
}