"""Generate private adapter names and callback selection in our own loader."""
from pathlib import Path
import re
import sys

source, output = map(Path, sys.argv[1:])
adapter = source.parent / 'flight-feel/src/efm/performance/dcs_adapter/performance_adapter.cpp'
names = re.findall(r'F23B_PERF_EXPORT \w+ (ed_fm_\w+)\(', adapter.read_text())
(output / 'own_adapter.hpp').write_text(
    '\n'.join(f'#define {n} own_{n}' for n in names) + '\n' +
    f'#include "{adapter.resolve().as_posix()}"\n' +
    '\n'.join(f'#undef {n}' for n in names) + '\n')
loader = (source / 'native_loader.cpp').read_text()
loader = loader.replace('#include "names.hpp"', '#include "names.hpp"\nFARPROC f23b_select_callback(const char*, FARPROC);')
anchor = '    note("resolved all "'
assert anchor in loader
loader = loader.replace(anchor, '    for (std::size_t i = 0; i < count; ++i) callbacks[i] = f23b_select_callback(names[i], callbacks[i]);\n' + anchor)
loader = loader.replace('zero force modification', 'experimental independent force/moment outputs with native system callbacks')
loader = loader.replace('F23B_NATIVE_FORWARD_TRACE', 'F23B_FORCE_BRIDGE_TRACE')
(output / 'loader.cpp').write_text(loader)
definition = output / 'exports.def'
definition.write_text(definition.read_text().replace('LIBRARY F23B_NativeForward', 'LIBRARY F23B_ForceBridge'))
