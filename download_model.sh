#!/bin/bash
# Download ggml-base.bin model for whisper.cpp

MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin"

# XDG Base Directory Standard for data
XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
MODELS_DIR="$XDG_DATA_HOME/IdeaWalker/models"
OUTPUT_FILE="$MODELS_DIR/ggml-medium.bin"

mkdir -p "$MODELS_DIR"

if [ -f "$OUTPUT_FILE" ]; then
    echo "Model already exists at: $OUTPUT_FILE"
else
    echo "Downloading to $OUTPUT_FILE..."
    curl -L -o "$OUTPUT_FILE" "$MODEL_URL"
    echo "Download complete."
fi
