import whisperx
import argparse
import os

# Configuração de argumentos
parser = argparse.ArgumentParser(description="Transcrição Otimizada WhisperX")
parser.add_argument("audio_path", type=str, help="Caminho do áudio")
args = parser.parse_args()

device = "cuda" # Mude para "cpu" se não tiver placa de vídeo NVIDIA
batch_size = 16 # Reduza se der erro de memória na GPU
compute_type = "float16" if device == "cuda" else "int8"

print(f"--- Processando: {args.audio_path} ---")

# 1. Transcrever (WhisperX já faz VAD - detecção de voz - automaticamente)
print("1. Carregando modelo e transcrevendo...")
model = whisperx.load_model("medium", device=device, compute_type=compute_type)
audio = whisperx.load_audio(args.audio_path)
result = model.transcribe(audio, batch_size=batch_size)

# 2. Alinhar (Melhora a precisão do tempo)
print("2. Alinhando timestamps...")
model_a, metadata = whisperx.load_align_model(language_code=result["language"], device=device)
result = whisperx.align(result["segments"], model_a, metadata, audio, device, return_char_alignments=False)

# 3. Salvar
base_name = os.path.splitext(args.audio_path)[0]
output_file = f"{base_name}_transcricao.txt"

print(f"3. Salvando em {output_file}...")
with open(output_file, "w", encoding="utf-8") as f:
    for seg in result["segments"]:
        start = round(seg['start'], 2)
        end = round(seg['end'], 2)
        text = seg['text'].strip()
        f.write(f"[{start}s -> {end}s] {text}\n")

print("✅ Concluído!")
