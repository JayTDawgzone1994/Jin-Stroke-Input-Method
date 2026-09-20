"""Read-only audit of the pinned Conway checkout; writes audit-results.json."""
from pathlib import Path
from collections import Counter, defaultdict
import re, json, sys, importlib.util

root = Path(__file__).parent
repo = root / 'stroke-input-data'
sys.path.insert(0, str(repo))
from generate import to_sequence_set

entries = {}
errors = []
generated = defaultdict(set)
types = Counter()
for n, line in enumerate((repo/'codepoint-character-sequence.txt').read_text(encoding='utf-8').splitlines(), 1):
    if not line.startswith('U+'):
        continue
    m = re.fullmatch(r'U\+([0-9A-F]{4,5})(!?)\t(\S)([\^*]?)\t([1-5|()\\]+)', line)
    if not m:
        errors.append(['invalid_source_line', n, line]); continue
    cp, font, ch, kind, regex = m.groups()
    if int(cp,16) != ord(ch) or ch in entries:
        errors.append(['codepoint_or_duplicate', n])
    seqs = to_sequence_set(regex)
    # Upstream uses single-digit backreferences even when followed by digits;
    # Python would interpret \\21 as group 21. Translate to named references.
    group_index = [0]
    def name_group(match):
        group_index[0] += 1
        return '(?P<g%d>' % group_index[0]
    python_regex = re.sub(r'\(', name_group, regex)
    python_regex = re.sub(r'\\([1-9])', lambda m: '(?P=g%s)' % m[1], python_regex)
    compiled = re.compile(python_regex)
    for seq in seqs:
        if not re.fullmatch('[1-5]+', seq) or not compiled.fullmatch(seq):
            errors.append(['invalid_expansion', ch, seq])
        generated[seq].add(ch)
    entries[ch] = {'kind':kind, 'regex':regex, 'sequences':sorted(seqs), 'font':font}
    types[kind or 'unmarked'] += 1

provided = {}
for line in (repo/'sequence-characters.txt').read_text(encoding='utf-8').splitlines():
    if not line or line.startswith('#'): continue
    seq, chars = line.split('\t')
    if seq in provided or len(chars) != len(set(chars)):
        errors.append(['duplicate_generated', seq])
    provided[seq] = set(chars)
if provided != dict(generated): errors.append(['generated_file_mismatch'])

def data_lines(name):
    return [x.strip() for x in (repo/name).read_text(encoding='utf-8').splitlines() if x.strip() and not x.lstrip().startswith(('#','<'))]

rank = ''.join(data_lines('ranking-traditional.txt'))
phrases = data_lines('phrases-traditional.txt')
# Standard Big5 level-1 byte range A440..C67E, decoded with Python's big5 codec.
# This is a legacy repertoire check, NOT the MOE 4808 list or a stroke-order check.
big5 = set()
for lead in range(0xA4,0xC7):
    for trail in list(range(0x40,0x7F))+list(range(0xA1,0xFF)):
        if lead == 0xC6 and trail > 0x7E: continue
        try: big5.add(bytes([lead,trail]).decode('big5'))
        except UnicodeDecodeError: pass
sample = '臺灣繁體輸入筆劃橫豎撇點折提鈎鉤學國語龍龜鬱裏裡著着示水永心必巨臣凹凸馬鳥𠮷𠮟𪚥'
prefix = {}
for s in ['1','12','123','1234','251','2511']:
    prefix[s] = len(set().union(*(cs for seq,cs in generated.items() if seq.startswith(s))))
result = {
 'source_commit':'d66ba5f5aa4cb6583883dfe8c14de553bb43616a',
 'characters':len(entries), 'types':dict(types),
 'non_bmp':sum(ord(c)>65535 for c in entries),
 'bmp_main_missing':sum(chr(i) not in entries for i in range(0x4e00,0xa000)),
 'extension_a_missing':sum(chr(i) not in entries for i in range(0x3400,0x4dc0)),
 'unique_sequences':len(generated), 'character_sequence_pairs':sum(map(len,generated.values())),
 'characters_with_multiple_sequences':sum(len(x['sequences'])>1 for x in entries.values()),
 'max_variants':sorted([(len(x['sequences']),ch) for ch,x in entries.items()],reverse=True)[:10],
 'sequence_length_range':[min(map(len,generated)),max(map(len,generated))],
 'colliding_full_sequences':sum(len(cs)>1 for cs in generated.values()),
 'max_full_sequence_candidates':max(map(len,generated.values())),
 'prefix_candidates_all_scripts':prefix,
 'big5_level1_characters':len(big5), 'big5_level1_missing':sorted(big5-entries.keys()),
 'big5_level1_marked_simplified':sorted(c for c in big5 if c in entries and entries[c]['kind']=='*'),
 'ranking_unique_characters':len(set(rank)), 'ranking_missing':sorted(set(rank)-entries.keys()),
 'phrase_lines':len(phrases), 'phrase_unique':len(set(phrases)),
 'phrase_characters_missing':sorted(set(''.join(phrases))-entries.keys()),
 'sample':{c:entries.get(c) for c in sample}, 'errors':errors,
 'data_file_bytes':{p.name:p.stat().st_size for p in repo.glob('*.txt')},
}
(root/'audit-results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k!='sample'},ensure_ascii=False,indent=2))
print('SAMPLES',json.dumps({c:entries.get(c,{}).get('sequences') for c in sample},ensure_ascii=False))
