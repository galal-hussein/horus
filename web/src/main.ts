import { getUpdates, initialLoad } from "./data";
import { nodeUrl, type Node } from "./node";
import { filterNodes } from "./filter";

function $<T extends HTMLElement>(id: string): T {
	const el = document.getElementById(id);
	if (!el) throw new Error(`missing #${id}`);
	return el as T;
}

const input = $<HTMLInputElement>("prompt-input");
const promptText = $("prompt-text");
const list = $("list");
const count = $("count");


let nodes: Node[] = [];
let matches: Node[] = [];
let selected = 0;

function cell(text: string, className: string): HTMLSpanElement {
	const span = document.createElement("span");
	span.className = className;
	span.textContent = text;
	return span;
}

function row(d: Node, i: number): HTMLLIElement {
	const li = document.createElement("li");
	li.className = i === selected ? "row selected" : "row";
	li.setAttribute("role", "option");
	li.setAttribute("aria-selected", String(i === selected));
	li.title = nodeUrl(d);
	li.append(
		cell(i === selected ? "▶" : "", "col-marker"),
		cell(d.name, "col-name"),
		cell(d.ip, "col-ip"),
		cell(d.url ?? "", "col-url"),
		cell(d.live ? "ONLINE" : "OFFLINE", `col-status ${d.live ? "status-up" : "status-down"}`),
	);
	li.addEventListener("click", (e) => open(d, e.ctrlKey || e.metaKey || e.shiftKey));
	return li;
}

function render(): void {
	promptText.textContent = input.value;

	if (matches.length === 0) {
		list.replaceChildren(cell("NO MATCHING ENTRIES", "empty"));
	} else {
		list.replaceChildren(...matches.map(row));
		list.children[selected]?.scrollIntoView({ block: "nearest" });
	}

	count.textContent = `${matches.length}/${nodes.length} ENTRIES`;
}

function update(): void {
	matches = filterNodes(nodes, input.value);
	selected = 0;
	render();
}

// Called on every server broadcast. Unlike update(), it keeps the current
// selection so the cursor doesn't jump back to the top on each refresh.
function refresh(fresh: Node[]): void {
	nodes = fresh;
	matches = filterNodes(nodes, input.value);
	selected = Math.min(selected, Math.max(matches.length - 1, 0));
	render();
}

function move(delta: number): void {
	if (matches.length === 0) return;
	selected = (selected + delta + matches.length) % matches.length;
	render();
}

function open(d: Node | undefined, newTab: boolean): void {
	if (!d) return;
	const url = nodeUrl(d);
	if (newTab) window.open(url, "_blank", "noopener");
	else window.location.href = url;
}

function onKey(e: KeyboardEvent): void {
	const mod = e.ctrlKey || e.metaKey;

	if (e.key === "ArrowDown" || (mod && e.key === "j") || (e.key === "Tab" && !e.shiftKey)) {
		move(1);
	} else if (e.key === "ArrowUp" || (mod && e.key === "k") || (e.key === "Tab" && e.shiftKey)) {
		move(-1);
	} else if (e.key === "Enter") {
		open(matches[selected], mod || e.shiftKey);
	} else if (e.key === "Escape") {
		input.value = "";
		update();
	} else {
		// Anything else is typing: make sure it lands in the prompt.
		input.focus();
		return;
	}
	e.preventDefault();
}

document.addEventListener("keydown", onKey);
input.addEventListener("input", update);

// initial load of pages
initialLoad(refresh);
getUpdates(refresh);
