import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { readFileSync, writeFileSync, existsSync, realpathSync } from 'node:fs';
import { dirname, resolve, relative, isAbsolute } from 'node:path';
import { fileURLToPath } from 'node:url';

const site = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const root = resolve(process.env.DOMES_REPO_ROOT || resolve(site, '../..'));
const hash = text => createHash('sha256').update(text).digest('hex');
const json = value => JSON.stringify(value, null, 2) + '\n';
const fail = message => { throw new Error(message); };
const states = ['Complete', 'Ready', 'Active', 'Acceptance pending', 'Not due', 'Blocked'];
const esc = text => String(text).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
export function safeRead(base, path) {
  if (typeof path !== 'string' || isAbsolute(path) || path.split('/').includes('..')) fail(`Unsafe source path: ${path}`);
  const full = resolve(base, path);
  if (!existsSync(full)) fail(`Missing evidence: ${path}`);
  const canonical = realpathSync(full);
  const rel = relative(realpathSync(base), canonical);
  if (rel.startsWith('..') || isAbsolute(rel)) fail(`Source escapes repository: ${path}`);
  const text = readFileSync(full, 'utf8');
  if (!text.trim()) fail(`Empty evidence: ${path}`);
  return text;
}

export function validate(model, sources) {
  if (model.schemaVersion !== 1) fail('Unsupported milestone schema');
  if (!/^\d{4}-\d{2}-\d{2}T/.test(model.reviewedAt) || !Number.isFinite(Date.parse(model.reviewedAt))) fail('Invalid review date');
  if (!/^[a-f0-9]{40}$/.test(model.sourceCommit)) fail('Source commit must be a full SHA');
  if (!model.review || !sources[model.review]) fail('A retained review note is required');
  if (!model.sources?.length || new Set(model.sources).size !== model.sources.length) fail('Duplicate or missing sources');
  for (const path of model.sources) if (!sources[path]) fail(`Missing evidence: ${path}`);
  const ledger = sources['PROGRAM_STATUS.md'];
  const control = ledger?.match(/<!-- domes-control: (.*?) -->/)?.[1];
  if (!control) fail('Executive control marker is missing');
  const panel = JSON.parse(control);
  for (const key of ['phase', 'nextGate', 'verdict']) if (panel[key] !== model.program[key]) fail(`Executive conflict: ${key}`);
  if (!ledger.includes(`**As of:** ${model.reviewedAt.slice(0,10)}`)) fail('Executive review date conflicts');
  if (model.phases.length !== 8 || new Set(model.phases.map(p => p.id)).size !== 8) fail('Require all eight unique program phases');
  if (model.phases.filter(p => p.state === 'Active').length !== 1 || model.phases.find(p => p.state === 'Active').id !== model.program.phase) fail('Exactly one phase must match the executive panel');
  if (model.streams.map(s => s.id).join(',') !== 'app,nff,hardware') fail('Three delivery tracks are required');
  const ids = new Set();
  for (const item of [...model.nodes, ...model.gates]) {
    if (!item.id || ids.has(item.id)) fail(`Duplicate milestone: ${item.id}`);
    ids.add(item.id);
  }
  const state = id => model.nodes.find(n => n.id === id)?.state || model.gates.find(g => g.id === id)?.state;
  const satisfied = id => ['Complete', 'Go'].includes(state(id));
  for (const node of model.nodes) {
    if (!states.includes(node.state)) fail(`Invalid state: ${node.id}`);
    for (const field of ['title','summary','acceptance','gap','source','owner','evidence','invalidates']) if (typeof node[field] !== 'string' || !node[field].trim()) fail(`Incomplete milestone ${node.id}: ${field}`);
    if (!sources[node.source]) fail(`Unreviewed node source: ${node.id}`);
    if (!['app','nff','hardware','support','simulation'].includes(node.stream)) fail(`Invalid track: ${node.id}`);
    if (!Array.isArray(node.depends) || new Set(node.depends).size !== node.depends.length) fail(`Invalid dependencies: ${node.id}`);
    for (const dep of node.depends) if (!ids.has(dep)) fail(`Unknown dependency ${dep} on ${node.id}`);
    if (['Ready','Complete','Acceptance pending'].includes(node.state) && node.depends.some(id => !satisfied(id))) fail(`Unsatisfied prerequisite for ${node.state} node ${node.id}`);
    if (!ledger.includes(`| ${node.id} | ${node.title} | ${node.state} |`)) fail(`Ledger conflicts with milestone ${node.id}`);
    if (node.state === 'Complete' && !/accepted|recorded|historical|passed/i.test(node.evidence)) fail(`Missing completion basis: ${node.id}`);
  }
  for (const gate of model.gates) {
    if (!['Hold','Go','Conditional Go','Recycle','Stop','Not due'].includes(gate.state)) fail(`Invalid gate state: ${gate.id}`);
    for (const id of gate.requires) if (!ids.has(id)) fail(`Unknown gate prerequisite ${id}`);
    if (['Go','Conditional Go'].includes(gate.state)) {
      if (!gate.criteria.length || gate.criteria.some(c=>c.state!=='Pass') || gate.requires.some(id=>!satisfied(id))) fail(`Gate ${gate.id} lacks passing critical evidence`);
      if (!gate.acceptanceRecord || !sources[gate.acceptanceRecord]) fail(`Gate ${gate.id} needs a reviewed immutable acceptance record`);
    }
    if (!gate.permits || !gate.prohibits) fail(`Missing gate boundary: ${gate.id}`);
  }
  if (model.gates.find(g => g.id === model.program.nextGate)?.state !== model.program.verdict) fail('Gate verdict conflicts with executive panel');
  const visiting = new Set(), visited = new Set();
  function visit(id) {
    if (visiting.has(id)) fail(`Dependency cycle: ${id}`);
    if (visited.has(id)) return;
    visiting.add(id);
    const item = model.nodes.find(n=>n.id===id) || model.gates.find(g=>g.id===id);
    for (const dep of item.depends || item.requires || []) visit(dep);
    visiting.delete(id); visited.add(id);
  }
  ids.forEach(visit);
  for (const gap of model.gaps) {
    if (!sources[gap.source] || !gap.owner || !gap.detail) fail(`Gap lacks evidence/owner: ${gap.id}`);
    for (const id of gap.closesWith) if (!ids.has(id)) fail(`Gap points at missing milestone: ${id}`);
  }
  for (const decision of model.decisions) {
    if (!decision.owner || !decision.recommendation || !decision.alternative || !decision.consequence || !decision.when) fail(`Incomplete decision: ${decision.id}`);
    for (const id of decision.blocks) if (!ids.has(id)) fail(`Decision points at missing milestone: ${id}`);
  }
  if (panel.hardwareCount < 6 && state('FS-WP-004D') === 'Complete') fail('Six-node acceptance conflicts with hardware inventory');
  const qualification = JSON.parse(sources['tools/simulation/qualification/operational-entry-report.json']);
  if (qualification.entry_result === 'rejected' && state('VC-WP-002A') === 'Complete') fail('Predictive acceptance conflicts with rejected entry');
  function scan(value) {
    if (!value || typeof value !== 'object') return;
    for (const [key,v] of Object.entries(value)) {
      if (/percent|percentage|progressPct/i.test(key)) fail('Percentage rollups are forbidden');
      scan(v);
    }
  }
  scan(model);
  return true;
}

export function collect(base) {
  const raw = safeRead(base, 'docs/program/milestones.json');
  const model = JSON.parse(raw);
  const sources = Object.fromEntries(model.sources.map(path => [path, safeRead(base, path)]));
  validate(model, sources);
  // Inventory relevant tracked and nonignored new files, including additions/deletions beyond
  // the cited excerpts. A new implementation file must not silently evade evidence review.
  const scopes=['ios/domes_app/lib','ios/domes_app/test','firmware/domes/main','firmware/common/proto','firmware/domes/profiles','tools/simulation','hardware','docs/plans'];
  const files=execFileSync('git',['-C',base,'ls-files','-z','--cached','--others','--exclude-standard','--',...scopes],{encoding:'utf8'}).split('\0').filter(Boolean);
  const watchHashes=Object.fromEntries([...new Set(files)].sort().filter(p=>p!=='docs/plans/product-control-reset.md').map(path=>[path,existsSync(resolve(base,path))?hash(readFileSync(resolve(base,path))):'MISSING']));
  return { model, raw, sources, receipt: { schemaVersion:1, reviewedAt:model.reviewedAt, review:model.review, modelSha256:hash(raw), sources:Object.fromEntries(Object.entries(sources).map(([path,text])=>[path,hash(text)])), watchHashes } };
}

export function receiptCheck(receipt, actual) {
  if (receipt.schemaVersion !== 1 || receipt.modelSha256 !== actual.modelSha256 || receipt.reviewedAt !== actual.reviewedAt || receipt.review !== actual.review) fail('Milestone model changed without a matching evidence review');
  const paths = new Set([...Object.keys(receipt.sources), ...Object.keys(actual.sources)]);
  const changed = [...paths].filter(path => receipt.sources[path] !== actual.sources[path]);
  if (changed.length) fail(`Reviewed evidence drift: ${changed.join(', ')}`);
  const watched=new Set([...Object.keys(receipt.watchHashes||{}),...Object.keys(actual.watchHashes||{})]);
  const drift=[...watched].filter(path=>receipt.watchHashes?.[path]!==actual.watchHashes?.[path]);
  if(drift.length) fail(`Implementation scope changed; evidence review required: ${drift.join(', ')}`);
}

function emit(path, contents, check) {
  const target = resolve(site,path);
  const before = existsSync(target) ? readFileSync(target,'utf8') : '';
  if (before === contents) return false;
  if (check) fail(`Generated status differs: ${path}`);
  writeFileSync(target, contents); return true;
}

export function run(args=process.argv.slice(2)) {
  const { model, sources, receipt } = collect(root);
  const receiptPath = resolve(site,'status/reviewed-sources.json');
  if (args.includes('--record-review')) {
    // Explicit audit action, never called by refresh/build or accepted merely to clear hash drift.
    const note = args[args.indexOf('--record-review')+1];
    if (note !== model.review) fail('Record-review must name the retained review note');
    if (existsSync(receiptPath)) {
      const prior = JSON.parse(readFileSync(receiptPath,'utf8'));
      if (json(prior) !== json(receipt) && sources[model.review] && prior.sources[model.review] === hash(sources[model.review])) fail('Changed evidence requires an updated substantive review note');
    }
    emit('status/reviewed-sources.json',json(receipt),false);
    console.log('Evidence review receipt recorded; this is not a test or gate pass.');
    return;
  }
  if (!existsSync(receiptPath)) fail('Missing reviewed-source receipt; complete source audit before recording it');
  receiptCheck(JSON.parse(readFileSync(receiptPath,'utf8')),receipt);
  const check=args.includes('--check');
  const snapshot={...model, sourceHashes:receipt.sources, modelSha256:receipt.modelSha256};
  const html='<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>DOMES reviewed evidence</title><style>body{font:16px/1.6 system-ui;max-width:1100px;margin:40px auto;padding:0 24px;background:#f5f7fa;color:#182334}a{color:#185ee3}summary{cursor:pointer;padding:12px;background:white;border:1px solid #dce3eb}pre{white-space:pre-wrap;overflow-wrap:anywhere;font:14px/1.6 monospace}small{color:#526174}</style><a href="/">← Development dashboard</a><h1>Reviewed source snapshot</h1><p>Review '+esc(model.reviewedAt)+' · local baseline '+esc(model.sourceCommit)+'. Source presence is not acceptance. Each status claim remains bounded by its review and configuration.</p>'+Object.entries(sources).map(([path,text])=>'<details id="'+esc(path)+'"><summary>'+esc(path)+'</summary><small>SHA-256 '+receipt.sources[path]+'</small><pre>'+esc(text)+'</pre></details>').join('')+'</html>\n';
  const changed=[emit('status/program-status.json',json(snapshot),check),emit('public/status.json',json(snapshot),check),emit('public/evidence.html',html,check)].some(Boolean);
  const age=Math.floor((Date.now()-Date.parse(model.reviewedAt))/86400000);
  console.log(`${check?'Validated':changed?'Refreshed':'Unchanged'}: ${model.nodes.length} milestones, ${model.gaps.length} gaps; ${model.program.phase}/${model.program.nextGate} ${model.program.verdict}. Review age ${age} days.`);
  if(age>model.freshness.reviewDays) console.warn('STALE: substantive evidence review is older than its freshness window; no status was inferred.');
}

if(process.argv[1] && resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  try {run();} catch(error) {console.error(`STATUS REFUSED: ${error.message}`);process.exitCode=1;}
}
