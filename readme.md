Configurar o Firebase (liberar acesso)
Se o Realtime Database estiver com regras fechadas, coloque temporariamente:
{
  "rules": {
    ".read": true,
    ".write": true
  }
}

Aqui está o JS pronto para integrar com o Firebase:

Cole no final do seu arquivo HTML, substituindo TODO o script antigo
<script>
const FIREBASE_URL = "https://testeproject-9a60a-default-rtdb.firebaseio.com/irrigacao";

// ============================================
// LER SENSOR DO FIREBASE
// ============================================
async function atualizarSensores() {
    try {
        const resp = await fetch(`${FIREBASE_URL}/sensor.json`);
        const data = await resp.json();

        if (data) {
            document.getElementById('humidity').innerHTML =
                data.umidade_percentual + '<span class="sensor-unit">%</span>';
        }
    } catch (err) {
        console.error("Erro lendo sensor:", err);
    }
}

// ============================================
// LER ESTADO DO RELÉ (atuador) DO FIREBASE
// ============================================
async function atualizarEstadoRele() {
    try {
        const resp = await fetch(`${FIREBASE_URL}/atuador/estado.json`);
        const estado = await resp.json();

        const status = document.getElementById('relayStatus');
        const text = document.getElementById('relayText');
        const btn = document.getElementById('irrigateBtn');

        if (estado === true) {
            status.classList.remove("inactive");
            status.classList.add("active");
            text.textContent = "Ativo";

            btn.textContent = "⏸ Parar Irrigação";
            btn.classList.add("irrigating");
        } else {
            status.classList.remove("active");
            status.classList.add("inactive");
            text.textContent = "Inativo";

            btn.textContent = "💧 Iniciar Irrigação";
            btn.classList.remove("irrigating");
        }

    } catch (err) {
        console.error("Erro lendo atuador:", err);
    }
}

// ============================================
// ENVIAR ESTADO DO RELÉ PARA O FIREBASE
// ============================================
async function toggleIrrigation() {
    try {
        // Primeiro pegamos o estado atual:
        const resp = await fetch(`${FIREBASE_URL}/atuador/estado.json`);
        const estadoAtual = await resp.json();

        const novoEstado = !estadoAtual;

        await fetch(`${FIREBASE_URL}/atuador/estado.json`, {
            method: "PUT",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(novoEstado)
        });

        // Atualiza interface
        atualizarEstadoRele();

    } catch (err) {
        console.error("Erro ao alterar estado:", err);
    }
}

// Atualiza interface automaticamente a cada 2s
setInterval(() => {
    atualizarSensores();
    atualizarEstadoRele();
}, 2000);

// Carrega na inicialização
atualizarSensores();
atualizarEstadoRele();
</script>

O que acontece agora?

Assim que abrir a página:

🔄 Leitura automática

A página chama GET /irrigacao/sensor.json

Atualiza a umidade real vinda da ESP32

💧 Controle do relé pelo navegador

O usuário clica em "Iniciar Irrigação"

O front faz PUT true em /atuador/estado

A ESP32 lê esse mesmo valor e liga o relé

🔌 Feedback instantâneo

O front lê o estado real e troca:

cor

texto

botão

🚀 5. Resultado final

Você terá:

Front estiloso e funcional

ESP32 conversando com Firebase

Firebase como hub entre hardware e interface

Controle remoto da irrigação em tempo real
