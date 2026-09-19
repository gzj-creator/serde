#!/usr/bin/env node

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const args = process.argv.slice(2);
const [baselineArg, candidateArg, outputArg, cpu] = args;
if (args.length < 3 || args.length > 4) {
    throw new Error('usage: node benchmark/compare.mjs BASELINE_BIN CANDIDATE_BIN OUTPUT_DIR [CPU]');
}
const baseline = resolve(baselineArg);
const candidate = resolve(candidateArg);
const output = resolve(outputArg);
const root = dirname(dirname(fileURLToPath(import.meta.url)));
mkdirSync(output, { recursive: true });

const fixtures = [{ name: 'default', json: join(root, 'benchmark/data/config.json'),
    toml: join(root, 'benchmark/data/config.toml'), iterations: 300000 }];

for (const [name, tags, limits, servers, zones, message, iterations] of [
    ['medium', 16, 16, 8, 4, 256, 30000],
    ['large', 64, 64, 64, 8, 4096, 3000],
    ['wide-4096', 0, 4096, 0, 0, 0, 30],
]) {
    const document = {
        application: 'app', enabled: true, retries: 5, ratio: 0.875,
        tags: Array.from({ length: tags }, (_, i) => `tag-${i}`),
        limits: Object.fromEntries(Array.from({ length: limits }, (_, i) => [`limit-${i}`, i])),
        owner: { name: 'owner', team: 'team' },
        servers: Array.from({ length: servers }, (_, i) => ({
            host: `host-${i}`, port: 9000 + i,
            zones: Array.from({ length: zones }, (_, j) => `zone-${i}-${j}`),
        })),
        message: 'm'.repeat(message),
    };
    const quote = JSON.stringify;
    const lines = [
        `application = ${quote(document.application)}`, 'enabled = true', 'retries = 5', 'ratio = 0.875',
        `tags = ${quote(document.tags)}`, `message = ${quote(document.message)}`,
    ];
    if (!servers) lines.push('servers = []');
    lines.push('[limits]', ...Object.entries(document.limits).map(([key, value]) => `${key} = ${value}`),
        '[owner]', 'name = "owner"', 'team = "team"');
    for (const server of document.servers) {
        lines.push('[[servers]]', `host = ${quote(server.host)}`, `port = ${server.port}`,
            `zones = ${quote(server.zones)}`);
    }
    const fixture = { name, json: join(output, `${name}.json`), toml: join(output, `${name}.toml`), iterations };
    writeFileSync(fixture.json, `${quote(document)}\n`);
    writeFileSync(fixture.toml, `${lines.join('\n')}\n`);
    fixtures.push(fixture);
}

function run(bin, format, fixture, phase, wide = false) {
    const binary = join(bin, `benchmark_serde_${format}${wide ? '_wide' : ''}`);
    const params = [...(wide ? [] : [`--${format}`, fixture[format]]), '--phase', phase,
        '--iterations', String(fixture.iterations), '--warmup', String(Math.min(fixture.iterations, 100))];
    const command = cpu === undefined ? binary : 'taskset';
    const argv = cpu === undefined ? params : ['-c', cpu, binary, ...params];
    const result = execFileSync(command, argv, { encoding: 'utf8', cwd: root });
    const row = result.trim().split('\n').find(line => line.includes(': bytes='));
    assert(row, `missing benchmark row: ${result}`);
    const fields = Object.fromEntries(row.split(/\s+/).slice(1).map(field => field.split('=')));
    assert(Number.isFinite(Number(fields.mean_ns)), `invalid result: ${row}`);
    assert(fields.checksum && Number(fields.iterations) === fixture.iterations, `invalid result: ${row}`);
    return { ns: Number(fields.mean_ns), checksum: fields.checksum, bytes: Number(fields.bytes) };
}

const samples = [];
const summary = [];
const median = values => [...values].sort((a, b) => a - b)[Math.floor(values.length / 2)];
console.log('format,fixture,phase,baseline_ns,candidate_ns,speedup');
for (const fixture of [...fixtures, { name: 'reflected-64', iterations: 30000, wide: true }]) {
    for (const format of fixture.wide ? ['json'] : ['json', 'toml']) {
        for (const phase of ['end-to-end', 'parse-only', 'decode-only']) {
            const paired = { baseline: [], candidate: [] };
            for (let round = 0; round < 5; round++) {
                const order = round % 2 ? ['candidate', 'baseline'] : ['baseline', 'candidate'];
                const checksums = [];
                for (const version of order) {
                    const result = run(version === 'baseline' ? baseline : candidate, format, fixture, phase, fixture.wide);
                    checksums.push(result.checksum);
                    paired[version].push(result.ns);
                    samples.push({ fixture: fixture.name, format, phase, version, round, ...result });
                }
                assert.equal(checksums[0], checksums[1], `${format} ${fixture.name} ${phase} checksum differs`);
            }
            const before = median(paired.baseline);
            const after = median(paired.candidate);
            const row = { fixture: fixture.name, format, phase,
                baseline_ns: before, candidate_ns: after, speedup: before / after };
            summary.push(row);
            console.log(`${format},${fixture.name},${phase},${before.toFixed(2)},${after.toFixed(2)},${row.speedup.toFixed(3)}`);
        }
    }
}
writeFileSync(join(output, 'results.json'), JSON.stringify({ baseline, candidate, cpu, samples, summary }, null, 2) + '\n');
