#!/bin/bash
# Download ggml-base.bin model for whisper.cpp

MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin"
OUTPUT_FILE="ggml-medium.bin"

if [ -f "$OUTPUT_FILE" ]; then
    echo "Model $OUTPUT_FILE already exists."
else
    echo "Downloading $OUTPUT_FILE from $MODEL_URL..."
    curl -L -o "$OUTPUT_FILE" "$MODEL_URL"
    echo "Download complete."
fi
