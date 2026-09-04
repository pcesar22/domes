'use client';

import { useEffect, useRef, useState } from 'react';
import data from '../status/program-status.json';

type Task = (typeof data.nodes)[number];
const stateLabel: Record<string, string> = { Complete: 'Recorded done', Ready: 'Ready to start', Active: 'In progress', 'Acceptance pending': 'Evidence gap', 'Not due': 'Later', Blocked: 'Needs resource' };
const tone = (state: string) => state === 'Complete' ? 'done' : state === 'Ready' || state === 'Active' ? 'ready' : state === 'Not due' ? 'later' : 'gap';
const sourceURL = (path: string) => `/evidence.html#${encodeURIComponent(path)}`;

export default function Dashboard() {
  const [selected, setSelected] = useState('FS-WP-004A');
  const [showDone, setShowDone] = useState(true);
  const [paths, setPaths] = useState<string[]>([]);
  const [ageDays, setAgeDays] = useState(0);
  const [connection, setConnection] = useState('Last published review');
  const graph = useRef<HTMLDivElement>(null);
  const task = data.nodes.find(n => n.id === selected)!;
  const dependencies = task.depends.map(id => data.nodes.find(n => n.id === id) || data.gates.find(g => g.id === id)!);
  const next = data.nodes.filter(n => n.depends.includes(selected));

  useEffect(() => {
    const update = () => setAgeDays(Math.max(0, Math.floor((Date.now() - Date.parse(data.reviewedAt)) / 86400000)));
    update();
    const timer = setInterval(update, 60000);
    return () => clearInterval(timer);
  }, []);

  useEffect(() => {
    let active = true;
    const check = async () => {
      try {
        const result = await fetch(`/status.json?t=${Date.now()}`, { cache: 'no-store' });
        if (!result.ok) throw new Error('unavailable');
        const current: unknown = await result.json();
        if (!current || typeof current !== 'object' || !('reviewedAt' in current) || typeof current.reviewedAt !== 'string') throw new Error('Invalid publication');
        if (!active) return;
        if (!('modelSha256' in current) || typeof current.modelSha256 !== 'string') throw new Error('Invalid content identity');
        if (current.modelSha256 !== data.modelSha256) { window.location.reload(); return; }
        setConnection('Publication checked just now');
      } catch { if (active) setConnection('Connection unavailable · showing saved review'); }
    };
    check();
    const timer = setInterval(check, 60000);
    return () => { active = false; clearInterval(timer); };
  }, []);

  useEffect(() => {
    const root = graph.current;
    if (!root) return;
    const update = () => {
      const bounds = root.getBoundingClientRect();
      const result: string[] = [];
      for (const n of data.nodes) for (const dependency of n.depends) {
        const from = root.querySelector<HTMLElement>(`[data-node="${dependency}"]`);
        const to = root.querySelector<HTMLElement>(`[data-node="${n.id}"]`);
        if (!from || !to) continue;
        const a = from.getBoundingClientRect(), b = to.getBoundingClientRect();
        const x1 = a.left + a.width / 2 - bounds.left, y1 = a.bottom - bounds.top;
        const x2 = b.left + b.width / 2 - bounds.left, y2 = b.top - bounds.top - 5;
        if (Math.abs(x1 - x2) < 5) result.push(`M ${x1} ${y1} L ${x2} ${y2}`);
        else if (n.id === selected || dependency === selected) result.push(`M ${x1} ${y1} C ${x1} ${y1 + 40}, ${x2} ${y2 - 40}, ${x2} ${y2}`);
      }
      setPaths(result);
    };
    update();
    const observer = new ResizeObserver(update);
    observer.observe(root);
    return () => observer.disconnect();
  }, [selected, showDone]);

  function choose(id: string) {
    if (data.nodes.some(n => n.id === id)) setSelected(id);
    else document.getElementById('gates')?.scrollIntoView({ behavior: 'smooth' });
  }
  function card(n: Task, compact = false) {
    return <button key={n.id} data-node={n.id} className={`task ${tone(n.state)} ${selected === n.id ? 'selected' : ''} ${compact ? 'compact' : ''}`} onClick={() => choose(n.id)} aria-pressed={selected === n.id} aria-controls="task-detail">
      <span className="task-top"><span className={`state ${tone(n.state)}`}>{stateLabel[n.state]}</span><span className="task-id">{n.id}</span></span>
      <strong>{n.title}</strong><span className="task-summary">{n.summary}</span>
      <span className="task-foot">{n.depends.length ? `${n.depends.length} prerequisite${n.depends.length === 1 ? '' : 's'}` : 'Independent start'} <span aria-hidden="true">↗</span></span>
    </button>;
  }

  return <div className="shell">
    <header className="topbar"><a className="brand" href="#"><span className="brand-mark" aria-hidden="true">◉</span> DOMES <span>Development</span></a><div className="private">Owner only <span className="dot" /> <span>Review · {data.reviewedAt.slice(0, 10)}</span></div></header>
    <main>
      <section className="overview" aria-label="Current program position"><div><div className="eyebrow">{data.program.phase} / {data.program.stage}</div><h1>Three tracks. One product.</h1><p>{data.program.summary}</p></div><div className="gate-now"><span className="eyebrow">NEXT COMMITMENT</span><strong>{data.program.nextGate} · {data.gates.find(g => g.id === data.program.nextGate)?.title}</strong><span><i className="dot amber" /> {data.program.verdict} · {data.program.confidence} confidence</span><small>{data.program.baseline} baseline · {data.program.forecast || 'forecast needs replan'}</small></div></section>
      <div className={`freshness ${ageDays > data.freshness.reviewDays ? 'stale' : ''}`}><span><span className="dot" /> {ageDays > data.freshness.reviewDays ? `Review stale · ${ageDays} days old` : 'Reviewed repository evidence'} · {connection}</span><a href="#gaps">{data.gaps.length} open evidence gaps ↗</a></div>
      <section className="steer" aria-labelledby="steer-title"><div><span className="eyebrow">HUMAN STEER</span><h2 id="steer-title">Where your steer<br />moves the work.</h2></div>{data.decisions.map(d => <details key={d.id}><summary><span className="decision-status">{d.state}</span><strong>{d.title}</strong><span aria-hidden="true">+</span></summary><p>{d.recommendation}</p><p><b>Alternative:</b> {d.alternative}</p><p><b>Timing:</b> {d.when}</p><p><b>Delay:</b> {d.consequence}</p><small>{d.owner} · {d.blocks.join(', ')}</small></details>)}</section>
      <section aria-labelledby="graph-title"><div className="section-heading"><div><span className="eyebrow">DELIVERY MAP</span><h2 id="graph-title">What moves next</h2></div><label className="toggle"><input type="checkbox" checked={showDone} onChange={e => setShowDone(e.target.checked)} /> Show recorded work</label></div>
        <div className="workspace"><div className="graph" ref={graph}><svg className="edges" aria-hidden="true"><defs><marker id="arrow" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6" fill="currentColor" /></marker></defs>{paths.map((d, i) => <path key={i} d={d} fill="none" stroke="currentColor" strokeWidth="1.5" markerEnd="url(#arrow)" />)}</svg>{data.streams.map((stream, i) => <section className={`lane ${stream.color}`} key={stream.id} aria-label={stream.name}><header><span className="lane-number">0{i + 1}</span><h3>{stream.name}</h3><p>{stream.resource}</p></header><div className="lane-tasks">{data.nodes.filter(n => n.stream === stream.id && (showDone || n.state !== 'Complete')).map(n => card(n))}</div></section>)}</div>
          <aside className="inspector" id="task-detail" aria-label="Selected milestone"><div className="inspector-top"><span className="eyebrow">MILESTONE DETAIL</span><span className={`state ${tone(task.state)}`}>{stateLabel[task.state]}</span></div><span className="mono">{task.id} · {task.refs.join(' / ')}</span><h2>{task.title}</h2><p>{task.summary}</p><h4>Exit evidence</h4><p>{task.acceptance}</p><h4>What is still open</h4><p className="gap-copy">{task.gap}</p><h4>Evidence level</h4><p>{task.evidence}</p><h4>Owner</h4><p>{task.owner}</p><h4>Depends on</h4><div className="chips">{dependencies.length ? dependencies.map(d => <button key={d.id} onClick={() => choose(d.id)}>{d.id} · {d.title} ↗</button>) : <span>Can start independently</span>}</div><h4>Unlocks</h4><div className="chips">{next.length ? next.map(d => <button key={d.id} onClick={() => choose(d.id)}>{d.id} · {d.title} ↗</button>) : <span>Workstream evidence release</span>}</div><h4>Reopens if</h4><p>{task.invalidates}</p><a className="source" href={task.source.startsWith('docs/program/') || task.source === 'hardware/DEVELOPMENT_SETUP.md' || task.source === 'PROGRAM_STATUS.md' ? '/evidence.html' : sourceURL(task.source)} target="_blank" rel="noreferrer">Read source evidence ↗</a></aside>
        </div>
      </section>
      <section className="support" aria-labelledby="support-title"><div className="section-heading"><div><span className="eyebrow">SHARED INPUTS</span><h2 id="support-title">Work that unlocks more than one track</h2></div></div><div className="support-grid">{data.nodes.filter(n => n.stream === 'support').map(n => card(n, true))}</div></section>
      <section className="simulation" aria-labelledby="simulation-title"><div className="section-heading"><div><span className="eyebrow">INSIDE THE SIMULATION TRACK</span><h2 id="simulation-title">From repeatable tests to trusted predictions</h2></div><span className="warning-label">{data.nodes.find(n => n.id === 'VC-WP-002A')?.state === 'Complete' ? 'Qualified only in the reviewed prediction envelope' : 'Predictive qualification not passed'}</span></div><p className="section-description">The app’s virtual lab can advance now. Production simulator parity has its own evidence ladder; code and candidate traces do not close these milestones automatically.</p><div className="sim-chain">{data.nodes.filter(n => n.stream === 'simulation').map(n => card(n, true))}</div></section>
      <section id="gaps" className="gaps"><div className="section-heading"><div><span className="eyebrow">EVIDENCE TO CLOSE</span><h2>Make the uncertainty visible</h2></div></div><div className="gap-grid">{data.gaps.map(g => <article key={g.id}><span className="gap-id">{g.id}</span><h3>{g.title}</h3><p>{g.detail}</p><small>{g.owner}</small><div className="chips">{g.closesWith.map(id => <button key={id} onClick={() => { choose(id); document.getElementById('task-detail')?.scrollIntoView({ behavior: 'smooth', block: 'center' }); }}>{id} ↗</button>)}</div></article>)}</div></section>
      <section id="gates" className="gates"><div className="section-heading"><div><span className="eyebrow">COMMITMENT BOUNDARIES</span><h2>Evidence before irreversible decisions</h2></div></div><div className="gate-grid">{data.gates.map(g => <article key={g.id}><span className="state gap">{g.state}</span><h3>{g.id} · {g.title}</h3><p>{g.permits}</p><p className="gap-copy">Still outside this gate: {g.prohibits}</p>{g.criteria.length > 0 && <ul>{g.criteria.map(c => <li key={c.id}><span>{c.title}</span><b>{c.state}</b></li>)}</ul>}</article>)}</div></section>
      <section className="history"><div className="section-heading"><div><span className="eyebrow">RETAINED PROGRESS</span><h2>What has been done</h2></div></div><ol>{data.history.map(h => <li key={h.title}><time>{h.date}</time><div><h3>{h.title}</h3><p>{h.detail}</p></div></li>)}</ol></section>
      <section className="horizon" aria-label="Product lifecycle">{data.phases.map(p => <div key={p.id} className={p.state === 'Active' ? 'current-phase' : ''}><span>{p.id} → {p.gate}</span><strong>{p.title}</strong><small>{p.state}</small></div>)}</section>
      <footer><div>DOMES · Evidence-led development<br /><small>Reviewed {data.reviewedAt.slice(0, 10)} · Source {data.sourceCommit.slice(0, 7)} · no percentage rollups</small></div><div><a href="/evidence.html">Reviewed sources</a><a href="/status.json">Machine-readable status</a><small>Refresh every 2 hours while the automation host is available.<br />GitHub activity never advances a gate.</small></div></footer>
    </main>
  </div>;
}
