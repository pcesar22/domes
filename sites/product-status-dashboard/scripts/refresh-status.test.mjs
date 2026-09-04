import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { collect, validate, receiptCheck, safeRead } from './refresh-status.mjs';
import { resolve } from 'node:path';

const base=resolve(process.env.DOMES_REPO_ROOT || '../..');
const current=collect(base);
const clone=()=>structuredClone(current.model);
function align(model,sources={...current.sources}) {
  const original=current.model;
  for(const node of model.nodes) {
    const old=original.nodes.find(n=>n.id===node.id);
    if(old) sources['PROGRAM_STATUS.md']=sources['PROGRAM_STATUS.md'].replace(`| ${old.id} | ${old.title} | ${old.state} |`,`| ${node.id} | ${node.title} | ${node.state} |`);
  }
  return sources;
}
test('reviewed graph and all real source files validate',()=>assert.equal(validate(current.model,current.sources),true));
test('missing evidence fails closed',()=>{const s={...current.sources};delete s['docs/TESTING.md'];assert.throws(()=>validate(clone(),s),/Missing evidence/);});
test('source paths cannot traverse outside repository',()=>assert.throws(()=>safeRead(base,'../elsewhere'),/Unsafe source/));
test('tampered source receipt fails without rewriting files',()=>{const receipt=structuredClone(current.receipt);receipt.sources['docs/TESTING.md']='bad';assert.throws(()=>receiptCheck(receipt,current.receipt),/evidence drift/);});
test('unreviewed milestone changes fail receipt validation',()=>{const receipt=structuredClone(current.receipt);receipt.modelSha256='bad';assert.throws(()=>receiptCheck(receipt,current.receipt),/model changed/);});
test('uncited implementation changes require review',()=>{const receipt=structuredClone(current.receipt);receipt.watchHashes['ios/domes_app/lib/new_transport.dart']='unreviewed';assert.throws(()=>receiptCheck(receipt,current.receipt),/Implementation scope changed/);});
test('cycles fail closed',()=>{const m=clone();m.nodes.find(n=>n.id==='HW-WP-002').state='Not due';m.nodes.find(n=>n.id==='HW-WP-002').depends=['HR1'];m.nodes.find(n=>n.id==='HW-WP-001A').state='Not due';m.nodes.find(n=>n.id==='HW-WP-001A').depends=['HW-WP-002'];assert.throws(()=>validate(m,align(m)),/Dependency cycle/);});
test('unknown dependencies fail',()=>{const m=clone();m.nodes[0].depends=['missing'];assert.throws(()=>validate(m,align(m)),/Unknown dependency/);});
test('premature completion cannot cross an unmet dependency',()=>{const m=clone();m.nodes.find(n=>n.id==='FS-WP-004B').state='Complete';assert.throws(()=>validate(m,align(m)),/Unsatisfied prerequisite/);});
test('executive and graph status conflicts fail',()=>{const m=clone();m.program.verdict='Go';assert.throws(()=>validate(m,current.sources),/Executive conflict/);});
test('gate go needs binary evidence',()=>{const m=clone();m.gates[0].state='Go';assert.throws(()=>validate(m,current.sources),/lacks passing/);});
test('percentage fields are rejected',()=>{const m=clone();m.program.percentComplete=90;assert.throws(()=>validate(m,current.sources),/Percentage/);});
test('refresh has no time-varying output fields',()=>{const again=collect(base);assert.deepEqual(again.receipt,current.receipt);assert.deepEqual(again.model,current.model);});
test('actual source receipt matches audited model',()=>receiptCheck(JSON.parse(readFileSync('status/reviewed-sources.json','utf8')),current.receipt));
