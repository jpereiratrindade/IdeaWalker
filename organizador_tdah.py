import time

class AssistenteOrganizacao:
    def __init__(self):
        self.perfil = {}
        self.recomendacao = ""
        self.estrategia = ""

    def perguntar(self, pergunta, opcoes):
        print(f"\n---> {pergunta}")
        for chave, valor in opcoes.items():
            print(f"[{chave}] {valor}")
        
        while True:
            resposta = input("Sua escolha: ").lower().strip()
            if resposta in opcoes.keys():
                return resposta
            print("Ops, opção inválida. Tente novamente.")

    def diagnostico(self):
        print("=== INICIANDO DIAGNÓSTICO DE FLUXO TDAH ===")
        time.sleep(1)

        # Pergunta 1: Captura
        self.perfil['captura'] = self.perguntar(
            "Como a ideia surge na sua cabeça?",
            {
                "1": "Visual (imagens, diagramas, setas)",
                "2": "Verbal (eu falo comigo mesmo, preciso explicar)",
                "3": "Caótica (muitas palavras soltas, nuvem de tags)",
                "4": "Linear (listas, tópicos, sequência)"
            }
        )

        # Pergunta 2: O Bloqueio da Escrita
        self.perfil['bloqueio'] = self.perguntar(
            "Onde a 'redação' costuma travar?",
            {
                "a": "Tela em branco (não sei por onde começar)",
                "b": "Meio do caminho (perco o fio da meada/foco)",
                "c": "Estrutura (escrevo muito, mas fica confuso/desconexo)",
                "d": "Acabamento (preguiça de revisar e formatar)"
            }
        )

        # Pergunta 3: Fricção Tecnológica
        self.perfil['tech'] = self.perguntar(
            "Qual sua tolerância para aprender softwares novos?",
            {
                "low": "Baixa. Quero algo que abra e funcione (tipo Bloco de Notas).",
                "med": "Média. Posso configurar um pouco, mas sem exageros.",
                "high": "Alta. Adoro mexer em sistemas complexos (tipo Notion avançado)."
            }
        )

    def processar_resultados(self):
        # Lógica de decisão simplificada
        captura = self.perfil['captura']
        bloqueio = self.perfil['bloqueio']
        tech = self.perfil['tech']

        # 1. Definição da Ferramenta de Captura (Onde a ideia nasce)
        if captura == "1": # Visual
            ferramenta = "Miro, Scrintal ou Obsidian Canvas (Mapas Mentais Infinitos)"
            metodo_captura = "Brain Dump Visual: Jogue caixas e conecte com setas. Não escreva frases, use palavras-chave."
        elif captura == "2": # Verbal
            ferramenta = "AudioPen, Otter.ai ou Gravador do Google (Speech-to-Text)"
            metodo_captura = "Fale para pensar. Grave um áudio explicando a ideia para um amigo imaginário, depois transcreva."
        elif captura == "3": # Caótica
            ferramenta = "Google Keep ou Obsidian (com Links Bidirecionais)"
            metodo_captura = "Zettelkasten Simplificado: Cada ideia é uma nota pequena. Não tente fazer o texto todo, faça 'átomos' de texto."
        else: # Linear
            ferramenta = "Workflowy ou Notion (Toggles)"
            metodo_captura = "Outlining: Crie apenas a estrutura de tópicos. Título > Subtítulo > Bullet point."

        # 2. Definição da Estratégia de Redação (Como o texto sai)
        if bloqueio == "a": # Tela em branco
            dica_redacao = "A Técnica do 'Vômito de Palavras' (Draft Zero). Escreva com a fonte na cor branca ou monitor desligado por 5 min. Proibido apagar."
        elif bloqueio == "c": # Estrutura
            dica_redacao = "Engenharia Reversa. Escreva os parágrafos soltos em post-its (ou blocos digitais) e só depois arraste para colocar na ordem."
        else:
            dica_redacao = "Pomodoro Reverso. Escreva por 10 min, descanse 2. O hiperfoco precisa de limites curtos para não exaurir."

        # Montagem do Relatório
        self.recomendacao = f"""
        \n==============================================
        RELATÓRIO DE ESTRATÉGIA TDAH PERSONALIZADA
        ==============================================
        
        FERRAMENTA SUGERIDA: 
        -> {ferramenta}
        
        MÉTODO DE CAPTURA (O PRIMEIRO PASSO):
        -> {metodo_captura}
        
        TÉCNICA DE REDAÇÃO (PARA DESTRAVAR):
        -> {dica_redacao}
        
        CONSELHO FINAL:
        Lembre-se: O TDAH separa o 'Planejador' do 'Executor'. 
        Nunca tente editar enquanto escreve. São modos cerebrais diferentes.
        """

    def executar(self):
        self.diagnostico()
        print("\nProcessando seus dados neurais...")
        time.sleep(2)
        self.processar_resultados()
        print(self.recomendacao)

# Rodar o script
if __name__ == "__main__":
    assistente = AssistenteOrganizacao()
    assistente.executar()