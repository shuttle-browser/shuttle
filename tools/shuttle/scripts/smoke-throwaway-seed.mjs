// Seeds the throwaway probe's profile before its launch: node scripts/smoke-throwaway-seed.mjs <userDataDir>
// (run by scripts/smoke-all.sh; smoke-throwaway.mjs section 0 checks what the browser's startup cleanup did).
//   - a persistent agent "tmp-legacy" in prefs (created before "tmp-" was reserved) with a save-text file and an
//     on-disk partition file: all kept, it stays a persistent agent;
//   - a crashed run's throwaway "tmp-0000000000aa": secrets and partition dirs removed;
//   - links: Storage/ext/shuttle-tmp-0000000000bb -> a directory outside the profile, and a link inside the stale
//     partition dir: removed as links, their targets kept.
import fs from 'node:fs';
import path from 'node:path';

const udd = process.argv[2];
if (!udd) { console.error('usage: smoke-throwaway-seed.mjs <userDataDir>'); process.exit(2); }
const prof = path.join(udd, 'Default');
const put = (file, text) => { fs.mkdirSync(path.dirname(file), { recursive: true }); fs.writeFileSync(file, text); };
put(path.join(prof, 'Preferences'), JSON.stringify({ shuttle: { profiles: { 'tmp-legacy': { name: 'Legacy tmp' } } } }));
put(path.join(prof, 'shuttle', 'secrets', 'tmp-legacy', 'old.txt'), 'legacy');
put(path.join(prof, 'Storage', 'ext', 'shuttle-tmp-legacy', 'def', 'marker'), 'legacy');
put(path.join(prof, 'shuttle', 'secrets', 'tmp-0000000000aa', 'old.txt'), 'stale');
put(path.join(prof, 'Storage', 'ext', 'shuttle-tmp-0000000000aa', 'def', 'GPUCache', 'data_0'), 'stale');
put(path.join(udd, 'outside', 'keep.txt'), 'outside');
put(path.join(udd, 'outside2', 'keep.txt'), 'outside');
fs.symlinkSync(path.join(udd, 'outside'), path.join(prof, 'Storage', 'ext', 'shuttle-tmp-0000000000bb'));
fs.symlinkSync(path.join(udd, 'outside2'), path.join(prof, 'Storage', 'ext', 'shuttle-tmp-0000000000aa', 'def', 'escape'));
