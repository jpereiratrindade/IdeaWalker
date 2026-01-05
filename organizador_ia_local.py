import os
import glob
import datetime
import ollama

# === 🛡️ BLOQUEIO DE INTERNET / FORÇA LOCAL ===
# Isso diz ao Python: "Não use o Proxy para falar com o localhost"
# Assim, ele conecta direto no Ollama rodando na sua máquina.
os.environ['no_proxy'] = '127.0.0.1,localhost'
os.environ['NO_PROXY'] = '127.0.0.1,localhost'

# === CONFIGURAÇÕES ===
PASTA_INBOX = "./inbox"       
PASTA_ORGANIZADA = "./notas"  
MODELO_LLM = "qwen2.5:7b"     

# Prompt do Sistema (A "Persona" Organizadora)
PROMPT_SISTEMA = """
Você é um Assistente de Gestão de Conhecimento pessoal para um desenvolvedor com TDAH.
Sua função NÃO é apenas resumir, mas ESTRUTURAR o pensamento caótico.

Receba o texto transcrito (que pode estar confuso, falado) e gere uma saída Markdown estrita com esta estrutura:

# [Um Título Curto e Impactante]

**Data:** {data_hoje}
**Tags:** #tag1 #tag2 (Ex: #SisterEpic, #Dev, #Arquitetura)

## 💎 O Insight Central
(Em 1 parágrafo: Qual é a ideia de ouro aqui?)

## 🧠 Pontos Principais
- (Bullet points curtos e diretos)
- (Conecte conceitos abstratos)

## 🚧 Ações / Próximos Passos
- [ ] (Coisas concretas a fazer)

## 🔗 Conexões Potenciais
(Conceitos relacionados, ex: "Vegetação", "Vetores", "Resiliência")
---
"""

def garantir_pastas():
    if not os.path.exists(PASTA_INBOX):
        os.makedirs(PASTA_INBOX)
    if not os.path.exists(PASTA_ORGANIZADA):
        os.makedirs(PASTA_ORGANIZADA)

def processar_texto(caminho_arquivo):
    print(f"🔄 Lendo localmente: {caminho_arquivo}...")
    
    with open(caminho_arquivo, "r", encoding="utf-8") as f:
        conteudo_bruto = f.read()

    data_hoje = datetime.datetime.now().strftime("%Y-%m-%d")
    
    print(f"🧠 Processando no Ollama (Offline/Local)...")
    try:
        resposta = ollama.chat(model=MODELO_LLM, messages=[
            {
                'role': 'system',
                'content': PROMPT_SISTEMA.replace("{data_hoje}", data_hoje)
            },
            {
                'role': 'user',
                'content': f"Organize este pensamento:\n\n{conteudo_bruto}"
            },
        ])
        
        texto_estruturado = resposta['message']['content']
        
        nome_base = os.path.splitext(os.path.basename(caminho_arquivo))[0]
        caminho_saida = os.path.join(PASTA_ORGANIZADA, f"Nota_{nome_base}.md")
        
        with open(caminho_saida, "w", encoding="utf-8") as f_out:
            f_out.write(texto_estruturado)
            
        print(f"✅ Nota salva: {caminho_saida}")
        
    except Exception as e:
        print(f"❌ Erro: {e}")
        print("Dica: Verifique se rodou 'ollama serve' em outro terminal.")

def main():
    print("=== ORGANIZADOR LOCAL TDAH ===")
    garantir_pastas()
    arquivos = glob.glob(os.path.join(PASTA_INBOX, "*.txt"))
    
    if not arquivos:
        print("📭 Pasta 'inbox' vazia.")
        return

    for arquivo in arquivos:
        processar_texto(arquivo)

if __name__ == "__main__":
    main()