// Endereço da API local iniciada pelo backend Python.
const API_BASE = "http://127.0.0.1:8000/api";

// Referências aos elementos que o JavaScript atualiza na página.
const elements = {
  badge: document.querySelector("#connectionBadge"),
  connectionLabel: document.querySelector("#connectionLabel"),
  portSelect: document.querySelector("#portSelect"),
  refreshPorts: document.querySelector("#refreshPorts"),
  connectButton: document.querySelector("#connectButton"),
  disconnectButton: document.querySelector("#disconnectButton"),
  connectionMessage: document.querySelector("#connectionMessage"),
  packetCount: document.querySelector("#packetCount"),
  connectedPort: document.querySelector("#connectedPort"),
  lastActivity: document.querySelector("#lastActivity"),
  eventLog: document.querySelector("#eventLog"),
  emptyState: document.querySelector("#emptyState"),
  clearLog: document.querySelector("#clearLog"),
};

// Guarda o estado da tela e evita sobrepor consultas periódicas.
let connected = false;
let lastEventId = 0;
let refreshInProgress = false;

// Faz uma requisição à API, converte a resposta JSON e propaga erros HTTP.
async function request(path, options = {}) {
  const response = await fetch(`${API_BASE}${path}`, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...options.headers,
    },
  });
  const payload = await response.json();
  if (!response.ok) {
    throw new Error(payload.error || `Erro HTTP ${response.status}`);
  }
  return payload;
}

// Atualiza a mensagem apresentada abaixo dos controles de conexão.
function showMessage(message, kind = "info") {
  elements.connectionMessage.textContent = message;
  elements.connectionMessage.dataset.kind = kind;
}

// Reflete na interface o estado retornado pelo backend.
function renderStatus(status) {
  connected = status.connected;
  elements.badge.dataset.connected = String(connected);
  elements.connectionLabel.textContent = connected ? "Conectado" : "Desconectado";
  elements.connectButton.disabled = connected || !elements.portSelect.value;
  elements.disconnectButton.disabled = !connected;
  elements.portSelect.disabled = connected;
  elements.refreshPorts.disabled = connected;
  elements.packetCount.textContent = status.packetCount;
  elements.connectedPort.textContent = status.port || "—";
  elements.lastActivity.textContent = status.lastEventAt
    ? new Date(status.lastEventAt).toLocaleTimeString("pt-BR")
    : "—";
}

// Consulta as portas disponíveis e preserva a seleção quando possível.
async function loadPorts() {
  try {
    const { ports } = await request("/ports");
    const selectedPort = elements.portSelect.value;
    elements.portSelect.replaceChildren();

    if (ports.length === 0) {
      const option = document.createElement("option");
      option.value = "";
      option.textContent = "Nenhuma porta encontrada";
      elements.portSelect.append(option);
      showMessage("Conecte o Arduino e atualize a lista de portas.");
    } else {
      for (const port of ports) {
        const option = document.createElement("option");
        option.value = port.device;
        option.textContent = port.description
          ? `${port.device} — ${port.description}`
          : port.device;
        elements.portSelect.append(option);
      }
      if (ports.some((port) => port.device === selectedPort)) {
        elements.portSelect.value = selectedPort;
      }
      showMessage(`${ports.length} porta(s) serial(is) disponível(is).`);
    }
  } catch (error) {
    showMessage(`Não foi possível carregar as portas: ${error.message}`, "error");
  }
  // Atualiza também os indicadores, mesmo que a consulta de portas falhe.
  renderStatus(await request("/status").catch(() => ({
    connected: false,
    packetCount: 0,
    port: null,
    lastEventAt: null,
  })));
}

// Cria uma entrada de log sem interpretar a mensagem como HTML.
function renderEvent(event) {
  const entry = document.createElement("li");
  entry.className = "event-entry";
  entry.dataset.type = event.type;

  const time = document.createElement("time");
  time.className = "event-time";
  time.dateTime = event.timestamp;
  time.textContent = new Date(event.timestamp).toLocaleTimeString("pt-BR");

  const message = document.createElement("span");
  message.className = "event-message";
  message.textContent = event.message;

  entry.append(time, message);
  elements.eventLog.append(entry);
  // Limita a quantidade de linhas renderizadas para manter a tela responsiva.
  while (elements.eventLog.children.length > 250) {
    elements.eventLog.firstElementChild.remove();
  }
  elements.emptyState.hidden = elements.eventLog.children.length > 0;
  elements.eventLog.scrollTop = elements.eventLog.scrollHeight;
}

// Busca o estado e os eventos novos; é chamado ao carregar e a cada segundo.
async function poll() {
  if (refreshInProgress) return;
  refreshInProgress = true;
  try {
    const [status, result] = await Promise.all([
      request("/status"),
      request(`/events?after=${lastEventId}`),
    ]);
    renderStatus(status);
    for (const event of result.events) {
      renderEvent(event);
      lastEventId = Math.max(lastEventId, event.id);
    }
  } catch (error) {
    if (!connected) {
      showMessage(`Backend indisponível: ${error.message}`, "error");
    }
  } finally {
    refreshInProgress = false;
  }
}

// Solicita ao backend a abertura da porta selecionada.
elements.connectButton.addEventListener("click", async () => {
  const port = elements.portSelect.value;
  if (!port) {
    showMessage("Selecione uma porta serial antes de conectar.", "error");
    return;
  }
  elements.connectButton.disabled = true;
  try {
    await request("/connect", {
      method: "POST",
      body: JSON.stringify({ port }),
    });
    showMessage(`Conectado a ${port}.`);
    await poll();
  } catch (error) {
    showMessage(error.message, "error");
  }
});

// Solicita o encerramento da conexão serial atual.
elements.disconnectButton.addEventListener("click", async () => {
  elements.disconnectButton.disabled = true;
  try {
    await request("/disconnect", { method: "POST", body: "{}" });
    showMessage("Desconectado da porta serial.");
    await poll();
  } catch (error) {
    showMessage(error.message, "error");
  }
});

// Liga os controles de atualização, seleção e limpeza do histórico visual.
elements.refreshPorts.addEventListener("click", loadPorts);
elements.portSelect.addEventListener("change", () => {
  elements.connectButton.disabled = connected || !elements.portSelect.value;
});
elements.clearLog.addEventListener("click", () => {
  elements.eventLog.replaceChildren();
  elements.emptyState.hidden = false;
});

// Inicializa a tela e mantém os dados atualizados periodicamente.
loadPorts();
poll();
window.setInterval(poll, 1000);
