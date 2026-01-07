# ESPECIFICAÇÃO DE INCREMENTO TÉCNICO: INGESTÃO DE ÁUDIO (PURE C++ / WHISPER.CPP)
# Projeto: Idea Walker (C++ / ImGui / DDD)
# Objetivo: Integrar `whisper.cpp` para transcrição local de alta performance sem dependência de Python.

================================================================================
1. VISÃO GERAL DA MUDANÇA
================================================================================
Foi decidido substituir a integração Python instável por uma solução nativa em C++.
Usaremos a biblioteca `whisper.cpp` (ggerganov) para rodar a inferência diretamente
no processo da aplicação.

Fluxo:
1. Usuário arrasta arquivo .WAV (suporte inicial a WAV 16kHz via SDL2).
2. Sistema carrega o modelo GGML (ex: `ggml-base.bin`).
3. Áudio é reamostrado para 16kHz float32.
4. `whisper_cpp` processa o áudio.
5. Texto é salvo na /inbox.

================================================================================
2. DEPENDÊNCIAS
================================================================================
- **whisper.cpp**: Adicionar via FetchContent no CMake.
- **SDL2**: Já existente (usar para carregar e converter áudio).
- **Modelos**: O sistema deve alertar se o modelo não existir ou baixá-lo automaticamente.

================================================================================
3. CAMADA DE INFRAESTRUTURA (O Adaptador)
================================================================================
Arquivo: `src/infrastructure/WhisperCppAdapter.hpp` (Substitui `WhisperScriptAdapter`)

[Responsabilidades]
- Gerenciar `whisper_context`.
- Carregar áudio WAV usando `SDL_LoadWAV` e `SDL_AudioCVT`.
- Executar `whisper_full`.

[Gerenciamento de Modelo]
- Caminho padrão: `models/ggml-base.bin`
- Se não existir, `transcribeAsync` deve falhar com mensagem clara ou iniciar download.

================================================================================
4. CAMADA DE UI
================================================================================
- Manter a lógica de Drag & Drop existente.
- Apenas substituir a injeção de dependência no `AppState`.

================================================================================
5. ESTRATÉGIA DE IMPLEMENTAÇÃO
================================================================================
1. Atualizar `CMakeLists.txt` para incluir `whisper.cpp`.
2. Criar `WhisperCppAdapter` implementando `TranscriptionService`.
3. Implementar lógica de carregamento de áudio (SDL2).
4. Atualizar `AppState` para usar o novo adaptador.

## Licença

Este projeto é licenciado sob a **GNU General Public License v3.0 (GPLv3)**.
Consulte o arquivo `LICENSE` para mais detalhes.
