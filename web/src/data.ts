import type { Node, Server } from "./node";

const API_URL = "/api/nodes";

function isObject(v: unknown): v is Record<string, unknown> {
	return typeof v === "object" && v !== null && !Array.isArray(v);
}

// Returns the reason an entry is invalid, or null if it is fine.
// Unknown fields are ignored so the server can add new ones freely.
function invalidReason(v: unknown): string | null {
	if (!isObject(v)) return "not an object";
	if (typeof v.name !== "string" || !v.name) return "name must be a non-empty string";
	if (typeof v.ip !== "string" || !v.ip) return "ip must be a non-empty string";
	if (v.url !== undefined && typeof v.url !== "string") return "url must be a string";
	return null;
}


// Returns the valid nodes in a server message, or null if the message is not
// a node list (e.g. one of the server's {"type":"error"} messages).
export function loadNodes(data: unknown): Node[] | null {
	if (!isObject(data) || !Array.isArray(data.nodes)) return null;

	const nodes: Node[] = [];
	data.nodes.forEach((entry, i) => {
		const reason = invalidReason(entry);
		if (reason) console.warn(`horus: skipping node #${i}: ${reason}`, entry);
		else nodes.push(entry as Node);
	});
	return nodes;
}

const WS_URL = "ws://localhost:8081/ws";
const RECONNECT_MS = 2000;
const NODES_API_URI = "/api/nodes";

export async function initialLoad(onNodes: (nodes: Node[]) => void): Promise<void> {
	try {
		const response = await (fetch(NODES_API_URI));
		if (!response.ok) throw new Error(`${API_URL}: HTTP ${response.status}`);

		const data: Server = await response.json();

		const nodes = loadNodes(data);
		if (nodes === null) {
			console.warn("horus: ignoring unexpected message", data);
			return;
		}
		onNodes(nodes);
	} catch (err) {
		console.error(`failed to load the nodes: ${err}`)
	}
}

// Connects to the server websocket and calls onNodes with the fresh node list
// every time the server broadcasts one. Reconnects if the connection drops.
export function getUpdates(onNodes: (nodes: Node[]) => void): void {
	const ws = new WebSocket(WS_URL);
	ws.onopen = () => {
		console.log("connected to websocket");
		ws.send(`{"type": "command", "name": "start_vpn"}`);
	}

	ws.onclose = () => {
		console.log(`disconnected from websocket, retrying in ${RECONNECT_MS}ms`);
		setTimeout(() => getUpdates(onNodes), RECONNECT_MS);
	};

	ws.onmessage = (e: MessageEvent) => {
		console.log(e.data);
		let data: unknown;
		try {
			data = JSON.parse(e.data);
		} catch {
			console.warn("horus: ignoring non-JSON message", e.data);
			return;
		}

		const nodes = loadNodes(data);
		if (nodes === null) {
			console.warn("horus: ignoring unexpected message", data);
			return;
		}
		onNodes(nodes);
	};
}

