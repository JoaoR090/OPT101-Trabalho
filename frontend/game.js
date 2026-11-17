const elements = {
  setup: document.querySelector("#gameSetup"),
  playerCount: document.querySelector("#playerCount"),
  playerNames: document.querySelector("#playerNames"),
  setupMessage: document.querySelector("#setupMessage"),
  startGame: document.querySelector("#startGame"),
  panel: document.querySelector("#gamePanel"),
  roundLabel: document.querySelector("#roundLabel"),
  playersRemaining: document.querySelector("#playersRemaining"),
  prompt: document.querySelector("#gamePrompt"),
  instruction: document.querySelector("#gameInstruction"),
  masterControls: document.querySelector("#masterControls"),
  responseControls: document.querySelector("#responseControls"),
  activePlayers: document.querySelector("#activePlayers"),
  eliminatedSection: document.querySelector("#eliminatedSection"),
  eliminatedPlayers: document.querySelector("#eliminatedPlayers"),
  message: document.querySelector("#gameMessage"),
  nextRound: document.querySelector("#nextRound"),
  newGame: document.querySelector("#newGame"),
};

const flagNames = {
  red: "bandeira vermelha",
  white: "bandeira branca",
  both: "as duas bandeiras",
};

let players = [];
let round = 0;
let command = null;
let currentPlayerIndex = 0;
let stage = "setup";

function renderNameFields() {
  const count = Number(elements.playerCount.value);
  const currentNames = [...elements.playerNames.querySelectorAll("input")].map((input) => input.value);
  elements.playerNames.replaceChildren();

  for (let index = 0; index < count; index += 1) {
    const label = document.createElement("label");
    label.className = "player-name-field";
    label.textContent = `Jogador ${index + 1}`;

    const input = document.createElement("input");
    input.type = "text";
    input.maxLength = 24;
    input.autocomplete = "off";
    input.placeholder = `Nome do jogador ${index + 1}`;
    input.value = currentNames[index] || "";
    input.dataset.playerName = String(index);
    label.append(input);
    elements.playerNames.append(label);
  }
}

function showGameMessage(message, kind = "info") {
  elements.message.textContent = message;
  elements.message.dataset.kind = kind;
}

function renderPlayers() {
  elements.activePlayers.replaceChildren();
  const activePlayers = players.filter((player) => player.active);
  elements.playersRemaining.textContent = `${activePlayers.length} ${activePlayers.length === 1 ? "jogador restante" : "jogadores restantes"}`;

  for (const [index, player] of players.entries()) {
    if (!player.active) continue;

    const row = document.createElement("li");
    row.className = "player-row";
    row.dataset.current = String(stage === "response" && index === currentPlayerIndex);

    const name = document.createElement("strong");
    name.textContent = player.name;
    const status = document.createElement("span");
    status.className = "player-status";
    status.textContent = stage === "response" && index === currentPlayerIndex
      ? "Sua vez"
      : "Na partida";
    row.append(name, status);
    elements.activePlayers.append(row);
  }

  const eliminated = players.filter((player) => !player.active);
  elements.eliminatedSection.hidden = eliminated.length === 0;
  elements.eliminatedPlayers.replaceChildren();
  for (const player of eliminated) {
    const item = document.createElement("li");
    item.textContent = player.name;
    elements.eliminatedPlayers.append(item);
  }
}

function setStage(nextStage) {
  stage = nextStage;
  elements.masterControls.hidden = stage !== "command";
  elements.responseControls.hidden = stage !== "response";
  elements.nextRound.hidden = stage !== "next";
  renderPlayers();
}

function beginRound() {
  round += 1;
  command = null;
  elements.roundLabel.textContent = `Rodada ${round}`;
  elements.prompt.textContent = "Mestre, levante uma ou ambas as bandeiras";
  elements.instruction.textContent = "Escolha o comando. Depois, cada jogador ativo deverá reproduzi-lo corretamente.";
  showGameMessage("O mestre escolhe o próximo comando.");
  setStage("command");
}

function startGame() {
  const names = [...elements.playerNames.querySelectorAll("input")]
    .map((input, index) => input.value.trim() || `Jogador ${index + 1}`);
  const normalizedNames = names.map((name) => name.toLocaleLowerCase("pt-BR"));
  if (new Set(normalizedNames).size !== normalizedNames.length) {
    elements.setupMessage.textContent = "Use nomes diferentes para identificar cada jogador.";
    elements.setupMessage.dataset.kind = "error";
    return;
  }

  players = names.map((name) => ({ name, active: true }));
  round = 0;
  elements.setup.hidden = true;
  elements.panel.hidden = false;
  beginRound();
}

function announceCommand(nextCommand) {
  command = nextCommand;
  currentPlayerIndex = players.findIndex((player) => player.active);
  elements.prompt.textContent = `O mestre levantou ${flagNames[command]}`;
  elements.instruction.textContent = `Passe o dispositivo para ${players[currentPlayerIndex].name}. Escolha a mesma combinação de bandeiras.`;
  showGameMessage("Agora é a vez dos jogadores responderem, um por vez.");
  setStage("response");
}

function recordResponse(response) {
  const player = players[currentPlayerIndex];
  if (!player || !player.active || stage !== "response") return;

  if (response === command) {
    showGameMessage(`${player.name} acertou e continua na partida.`, "success");
  } else {
    player.active = false;
    showGameMessage(`${player.name} errou e foi desclassificado.`, "error");
  }

  const remaining = players.filter((entry) => entry.active);
  if (remaining.length <= 1) {
    const winner = remaining[0];
    elements.prompt.textContent = winner ? `${winner.name} venceu!` : "A partida terminou sem vencedor.";
    elements.instruction.textContent = "Quem sobrou por último vence a partida.";
    setStage("finished");
    return;
  }

  currentPlayerIndex = players.findIndex(
    (entry, index) => index > currentPlayerIndex && entry.active,
  );
  if (currentPlayerIndex === -1) {
    elements.prompt.textContent = "Todos responderam!";
    elements.instruction.textContent = "A próxima rodada começa com os jogadores que continuam na partida.";
    setStage("next");
    return;
  }

  elements.instruction.textContent = `Passe o dispositivo para ${players[currentPlayerIndex].name}.`;
  renderPlayers();
}

elements.playerCount.addEventListener("change", renderNameFields);
elements.startGame.addEventListener("click", startGame);
elements.masterControls.addEventListener("click", (event) => {
  const button = event.target.closest("[data-flag]");
  if (button && stage === "command") announceCommand(button.dataset.flag);
});
elements.responseControls.addEventListener("click", (event) => {
  const button = event.target.closest("[data-response]");
  if (button) recordResponse(button.dataset.response);
});
elements.nextRound.addEventListener("click", beginRound);
elements.newGame.addEventListener("click", () => {
  players = [];
  round = 0;
  command = null;
  elements.panel.hidden = true;
  elements.setup.hidden = false;
  elements.setupMessage.textContent = "";
  elements.setupMessage.dataset.kind = "";
});

renderNameFields();
