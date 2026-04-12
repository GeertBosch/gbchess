#!/usr/bin/env python3
"""
dead_code.py - Find declarations in a C++ file ranked by external reference count.

For each declaration in the target file, reports the set of files outside that file
which transitively reference it: directly, or through another declaration in the same
file that is itself externally referenced.

Declarations with zero external references appear first.

Usage:
    python3 dead_code.py <source_file> [--build-dir=<path>] [--unused-only] [--max-use=N] [--quiet]

Options:
    --unused-only   Print only declarations with zero external uses in gcc diagnostic
                    format (file:line:col: warning: name (kind)) and exit with code 1
                    if any are found. Suitable for VS Code problemMatcher.
    --max-use=N     Only output declarations with at most N external uses.
                    N=0 is equivalent to --unused-only (but keeps the table format).
    --quiet         Suppress progress messages on stderr.

Requirements:
    - clangd on PATH
    - A populated clangd background index (.cache/clangd/index/) — running VS Code
      with clangd enabled is sufficient to build it.
    - A compile_commands.json reachable from the source file (searched upward).
"""

import json, os, pathlib, queue, re, subprocess, sys, threading
from urllib.parse import urlparse, unquote


# LSP SymbolKind values to skip as report entries (but still recurse into their children)
_SKIP_KINDS = {1, 2, 3}  # File, Module, Namespace

# Method/class names that are implicitly invoked by range-based for loops and the iterator
# protocol, or as default constructors. clangd does not emit explicit reference sites for
# these, so we propagate the containing class's external references to them rather than
# reporting them as unused.
_IMPLICIT_USE_NAMES = {
    'begin', 'end',
    'operator++', 'operator--', 'operator*', 'operator->',
    'operator==', 'operator!=',
    'iterator', 'const_iterator',
    'value_type', 'difference_type', 'iterator_category', 'pointer', 'reference',
}

_KIND_NAMES = {
    4: 'package',    5: 'class',      6: 'method',     7: 'property',
    8: 'field',      9: 'constructor', 10: 'enum',     11: 'interface',
    12: 'function', 13: 'variable',   14: 'constant',  22: 'enum_member',
    23: 'struct',   24: 'event',      25: 'operator',  26: 'type_param',
}


# ── LSP/clangd client ──────────────────────────────────────────────────────────

class ClangdClient:
    def __init__(self, build_dir: str, root_uri: str, quiet: bool = False):
        self._proc = subprocess.Popen(
            ['clangd', f'--compile-commands-dir={build_dir}', '--log=error'],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL if quiet else sys.stderr,
        )
        self._next_id = 1
        self._pending: dict[int, queue.Queue] = {}
        self._lock = threading.Lock()
        self._index_tokens: set = set()          # tokens whose begin said 'index'
        self._index_begin = threading.Event()   # set when indexing starts
        self._index_ready = threading.Event()   # set when indexing finishes
        self._opened: set = set()               # URIs already sent didOpen
        threading.Thread(target=self._reader, daemon=True).start()

        self.request('initialize', {
            'processId': os.getpid(),
            'rootUri': root_uri,
            'capabilities': {
                'textDocument': {
                    'documentSymbol': {'hierarchicalDocumentSymbolSupport': True},
                },
                'window': {
                    'workDoneProgress': True,
                },
            },
        })
        self.notify('initialized', {})

    def _send(self, msg: dict):
        body = json.dumps(msg).encode()
        self._proc.stdin.write(f'Content-Length: {len(body)}\r\n\r\n'.encode() + body)
        self._proc.stdin.flush()

    def _reader(self):
        while True:
            line = self._proc.stdout.readline()
            if not line:
                break
            m = re.match(rb'Content-Length: (\d+)', line)
            if not m:
                continue
            self._proc.stdout.readline()  # blank separator line
            body = self._proc.stdout.read(int(m.group(1)))
            msg = json.loads(body)
            if (msg_id := msg.get('id')) is not None:
                with self._lock:
                    q = self._pending.get(msg_id)
                if q:
                    q.put(msg)
                else:
                    # Server-initiated request (e.g. window/workDoneProgress/create)
                    self._send({'jsonrpc': '2.0', 'id': msg_id, 'result': None})
            else:
                self._handle_notification(msg)

    def _handle_notification(self, msg: dict):
        # Watch for background-index progress via $/progress.
        # WorkDoneProgressEnd carries no title/message, so we track the token
        # from the begin notification and match on that for the end.
        if msg.get('method') == '$/progress':
            params = msg.get('params', {})
            token  = params.get('token')
            value  = params.get('value', {})
            kind   = value.get('kind')
            text   = (value.get('message', '') + value.get('title', '')).lower()
            if kind == 'begin':
                if 'index' in text:
                    self._index_tokens.add(token)
                    self._index_begin.set()
            elif kind == 'end':
                if token in self._index_tokens:
                    self._index_ready.set()

    def wait_for_index(self, timeout: float = 60.0):
        """Block until the background index is ready.

        On a cold start clangd sends begin→end progress notifications; we wait
        for the end. On a warm start (index loaded from disk in <2 s) clangd
        sends nothing or finishes before we can observe the begin, so we return
        after the short begin-poll instead of waiting the full timeout.
        """
        if not self._index_begin.wait(timeout=10.0):
            return  # no indexing started — index was already warm
        self._index_ready.wait(timeout=timeout)

    def request(self, method: str, params) -> object:
        with self._lock:
            id_ = self._next_id
            self._next_id += 1
            q: queue.Queue = queue.Queue()
            self._pending[id_] = q
        self._send({'jsonrpc': '2.0', 'id': id_, 'method': method, 'params': params})
        result = q.get(timeout=60)
        with self._lock:
            del self._pending[id_]
        if 'error' in result:
            raise RuntimeError(f'{method}: {result["error"]}')
        return result.get('result')

    def notify(self, method: str, params):
        self._send({'jsonrpc': '2.0', 'method': method, 'params': params})

    def open(self, path: str) -> str:
        uri = pathlib.Path(path).resolve().as_uri()
        if uri not in self._opened:
            self._opened.add(uri)
            self.notify('textDocument/didOpen', {
                'textDocument': {
                    'uri': uri,
                    'languageId': 'cpp',
                    'version': 1,
                    'text': pathlib.Path(path).read_text(errors='replace'),
                },
            })
        return uri

    def shutdown(self):
        try:
            self.request('shutdown', None)
        except Exception:
            pass
        self.notify('exit', None)
        try:
            self._proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self._proc.terminate()


# ── symbol utilities ───────────────────────────────────────────────────────────

def flatten(syms: list, _parent: dict | None = None) -> list:
    """Recursively flatten a DocumentSymbol tree, skipping containers."""
    result = []
    for sym in syms:
        sym['_parent'] = _parent
        if sym['kind'] not in _SKIP_KINDS and not sym['name'].startswith('(anonymous'):
            result.append(sym)
        result.extend(flatten(sym.get('children', []), _parent=sym))
    return result


def sym_id(sym: dict) -> tuple:
    """Unique key for a symbol: (name, line, col) of its selectionRange start."""
    s = sym['selectionRange']['start']
    return (sym['name'], s['line'], s['character'])


def enclosing(flat: list, line: int, col: int) -> dict | None:
    """Find the smallest symbol in flat whose range contains (line, col)."""
    pos = (line, col)
    best = best_size = None
    for sym in flat:
        r = sym['range']
        start = (r['start']['line'], r['start']['character'])
        end   = (r['end']['line'],   r['end']['character'])
        if start <= pos <= end:
            size = r['end']['line'] - r['start']['line']
            if best_size is None or size < best_size:
                best, best_size = sym, size
    return best


def uri_to_rel(uri: str, root: pathlib.Path) -> str:
    path = unquote(urlparse(uri).path)
    try:
        return str(pathlib.Path(path).relative_to(root))
    except ValueError:
        return path


# ── project root / build dir discovery ────────────────────────────────────────

def find_build_dir(source: pathlib.Path) -> str:
    d = source.parent
    while d != d.parent:
        for candidate in [d / 'build', d]:
            if (candidate / 'compile_commands.json').exists():
                return str(candidate)
        d = d.parent
    return str(source.parent)


def find_root(source: pathlib.Path) -> pathlib.Path:
    d = source.parent
    while d != d.parent:
        if (d / '.git').exists():
            return d
        d = d.parent
    return source.parent


# ── per-file analysis ─────────────────────────────────────────────────────────

def analyse(client: 'ClangdClient', source: pathlib.Path, root: pathlib.Path,
            unused_only: bool, max_use: int | None = None,
            quiet: bool = False) -> int:
    """Analyse one source file using an already-connected clangd client.
    Returns the number of unused declarations."""

    def log(*args, **kwargs):
        if not quiet:
            print(*args, file=sys.stderr, **kwargs)

    log(f'source: {source}')
    uri = client.open(str(source))

    log('Fetching symbols...')
    raw  = client.request('textDocument/documentSymbol',
                           {'textDocument': {'uri': uri}}) or []
    flat = flatten(raw)

    if not flat:
        log(f'Warning: no symbols returned for {source} — skipping')
        return 0
    log(f'Found {len(flat)} symbols.')

    # direct_ext[id] = external URIs that directly reference this symbol
    # used_by[id]    = ids of same-file symbols whose body references this symbol
    direct_ext: dict[tuple, set] = {sym_id(s): set() for s in flat}
    used_by:    dict[tuple, set] = {sym_id(s): set() for s in flat}

    for i, sym in enumerate(flat):
        sid = sym_id(sym)
        s   = sym['selectionRange']['start']
        log(f'  [{i+1}/{len(flat)}] {sym["name"]}...', end='\r')

        refs = client.request('textDocument/references', {
            'textDocument': {'uri': uri},
            'position':     {'line': s['line'], 'character': s['character']},
            'context':      {'includeDeclaration': False},
        }) or []

        for ref in refs:
            if ref['uri'] != uri:
                # External reference — record the file
                direct_ext[sid].add(ref['uri'])
            else:
                # Internal reference — find which same-file symbol contains it
                rl, rc = ref['range']['start']['line'], ref['range']['start']['character']
                owner = enclosing(flat, rl, rc)
                if owner and (oid := sym_id(owner)) != sid:
                    used_by[sid].add(oid)

    log()

    # Read source lines once for default-constructor detection below.
    source_lines = source.read_text(errors='replace').splitlines()

    def is_default_ctor(sym: dict) -> bool:
        """True if sym is a defaulted constructor (i.e. its text contains '() = default')."""
        if sym['kind'] != 9:  # 9 = Constructor
            return False
        line = sym['selectionRange']['start']['line']
        # Check the line itself and the next line in case of formatting across lines.
        text = ' '.join(source_lines[line:line + 2])
        return '() = default' in text

    def is_maybe_unused(sym: dict) -> bool:
        """True if the symbol's declaration line contains [[maybe_unused]] or NOLINT(dead_code)."""
        line = sym['selectionRange']['start']['line']
        text = source_lines[line]
        return '[[maybe_unused]]' in text or 'NOLINT(dead_code)' in text

    # Propagate external references from parent class to iterator-protocol members.
    # Range-based for loops call begin()/end()/operator++ etc. implicitly and clangd
    # does not emit explicit reference sites for them. Similarly, defaulted constructors
    # (`= default`) are called implicitly on value-initialization with no visible call site.
    for sym in flat:
        sid = sym_id(sym)
        if direct_ext[sid]:
            continue
        if sym['name'] not in _IMPLICIT_USE_NAMES and not is_default_ctor(sym):
            continue
        parent = sym.get('_parent')
        if parent is None:
            continue
        pid = sym_id(parent)
        if pid in direct_ext:
            direct_ext[sid] |= direct_ext[pid]

    # [[maybe_unused]] symbols are explicitly acknowledged as used despite clangd not seeing
    # call sites (e.g. fields accessed via template instantiation). Treat them and any
    # symbols they mention in their declaration text (types, constants etc.) as externally
    # referenced so related symbols don't appear as unused. Propagate transitively so that
    # e.g. `[[maybe_unused]] Output bias` → Output → Bias, kOutputDimensions all stay quiet.
    _SYNTHETIC_REF = frozenset({'[[maybe_unused]]'})

    name_to_syms: dict[str, list] = {}
    for sym in flat:
        name_to_syms.setdefault(sym['name'], []).append(sym)

    def mentioned_syms(sym: dict) -> list[dict]:
        """Symbols from flat whose names appear in sym's declaration source text."""
        r = sym['range']
        text = ' '.join(source_lines[r['start']['line']:r['end']['line'] + 1])
        result = []
        for name, syms in name_to_syms.items():
            if re.search(r'\b' + re.escape(name) + r'\b', text):
                result.extend(s for s in syms if sym_id(s) != sym_id(sym))
        return result

    synth_worklist = [sym for sym in flat if is_maybe_unused(sym)]
    synth_visited: set = set()
    while synth_worklist:
        sym = synth_worklist.pop()
        sid = sym_id(sym)
        if sid in synth_visited:
            continue
        synth_visited.add(sid)
        direct_ext[sid] |= _SYNTHETIC_REF
        synth_worklist.extend(mentioned_syms(sym))

    # Transitive closure: for each symbol, collect all external files that can reach
    # it through any chain of same-file "A uses B" relationships.
    def transitive(sid: tuple) -> frozenset:
        files    = set(direct_ext[sid])
        visited  = set()
        worklist = [sid]
        while worklist:
            node = worklist.pop()
            if node in visited:
                continue
            visited.add(node)
            for referrer in used_by.get(node, ()):
                files |= direct_ext.get(referrer, set())
                worklist.append(referrer)
        return frozenset(files)

    # Build result rows: (n_external_files, line, col, loc_str, name, kind, files)
    rows = []
    for sym in flat:
        sid   = sym_id(sym)
        files = transitive(sid)
        s     = sym['selectionRange']['start']
        rel   = uri_to_rel(uri, root)
        loc   = f'{rel}:{s["line"]+1}:{s["character"]+1}'
        kind  = _KIND_NAMES.get(sym['kind'], str(sym['kind']))
        rows.append((len(files), s['line'], s['character'], loc, sym['name'], kind, files, sym))

    rows.sort(key=lambda r: (r[0], r[1], r[2]))

    unused = sum(1 for n, _l, _c, _loc, _name, _kind, files, sym in rows
                 if not (files - _SYNTHETIC_REF) and '[[maybe_unused]]' not in files)

    if unused_only:
        threshold = 0 if max_use is None else max_use
        for n, _line, _col, loc, name, kind, files, sym in rows:
            real_files = files - _SYNTHETIC_REF
            if len(real_files) <= threshold and '[[maybe_unused]]' not in files:
                print(f'{loc}: warning: {name} ({kind}) [UNUSED]')
    else:
        for n, _line, _col, loc, name, kind, files, sym in rows:
            real_files = files - _SYNTHETIC_REF
            if max_use is not None and len(real_files) > max_use:
                continue
            if not real_files:
                if '[[maybe_unused]]' in files:
                    continue
                refs = '[UNUSED]'
            else:
                sorted_files = sorted(uri_to_rel(f, root) for f in real_files)
                if len(sorted_files) > 2:
                    refs = f'{sorted_files[0]}, {sorted_files[1]}, and {len(sorted_files)-2} more'
                else:
                    refs = ', '.join(sorted_files)
            print(f'{loc}  {name}  ({kind})  {refs}')

        print()
        print(f'{len(rows)} declarations, {unused} unused')

    return unused


# ── main ───────────────────────────────────────────────────────────────────────

def main():
    flags   = [a for a in sys.argv[1:] if a.startswith('--')]
    sources = [a for a in sys.argv[1:] if not a.startswith('--')]

    if not sources:
        sys.exit(f'Usage: {sys.argv[0]} <source_file>... [--build-dir=<path>] [--unused-only] [--max-use=N] [--quiet]')

    unused_only = '--unused-only' in flags
    quiet       = '--quiet' in flags
    max_use_flag = next((f for f in flags if f.startswith('--max-use=')), None)
    max_use = int(max_use_flag.split('=', 1)[1]) if max_use_flag else None
    # Use the first source file to locate build dir and repo root.
    first  = pathlib.Path(sources[0]).resolve()
    build_dir = next(
        (f.split('=', 1)[1] for f in flags if f.startswith('--build-dir=')),
        find_build_dir(first),
    )
    root = find_root(first)

    def log(*args, **kwargs):
        if not quiet:
            print(*args, file=sys.stderr, **kwargs)

    log(f'build-dir: {build_dir}')
    log(f'root:      {root}')

    client = ClangdClient(build_dir, root.as_uri(), quiet=quiet)
    try:
        # Open the first file to trigger clangd background indexing before waiting.
        client.open(str(first))
        log('Waiting for background index...')
        client.wait_for_index(timeout=120)

        total_unused = 0
        for path in sources:
            source = pathlib.Path(path).resolve()
            total_unused += analyse(client, source, root, unused_only, max_use, quiet=quiet)

    finally:
        client.shutdown()

    if unused_only and total_unused:
        sys.exit(1)


if __name__ == '__main__':
    main()
