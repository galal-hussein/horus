export type Status = "up" | "down";

export interface Server {
	nodes: Array<Node>;
}

export interface Node {
	name: string;
	ip: string;
	url?: string;
	live?: boolean;
}

export function nodeUrl(d: Node): string {
	return d.url || `http://${d.ip}`;
}
