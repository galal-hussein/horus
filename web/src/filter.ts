import type { Node } from "./node";

// The whole query must appear in name, ip or url.
// Matches on the name rank first; otherwise the configured order is kept.
export function filterNodes(nodes: Node[], query: string): Node[] {
  const q = query.trim().toLowerCase();
  if (!q) return nodes;

  const scored: { node: Node; score: number }[] = [];

  for (const node of nodes) {
    const name = node.name.toLowerCase();
    let score: number;
    if (name.startsWith(q)) score = 0;
    else if (name.includes(q)) score = 1;
    else if (node.ip.includes(q) || node.url?.toLowerCase().includes(q)) score = 2;
    else continue;
    scored.push({ node, score });
  }

  return scored.sort((a, b) => a.score - b.score).map((s) => s.node);
}
