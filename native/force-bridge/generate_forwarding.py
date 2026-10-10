"""Generate original x64 ABI-preserving entry points for named EFM exports."""
from pathlib import Path
import sys

names = Path(sys.argv[1]).read_text().splitlines()
out = Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
assert names and all(n.startswith('ed_fm_') and n.isidentifier() for n in names)
asm = ['; SPDX-License-Identifier: MIT', 'EXTERN f23b_resolve:PROC', '.code']
for index, name in enumerate(names):
    asm += [f'{name} PROC FRAME', '    sub rsp, 0A8h', '    .allocstack 0A8h',
            '    .endprolog',
            '    mov [rsp+20h], rcx', '    mov [rsp+28h], rdx',
            '    mov [rsp+30h], r8', '    mov [rsp+38h], r9',
            '    movdqu [rsp+40h], xmm0', '    movdqu [rsp+50h], xmm1',
            '    movdqu [rsp+60h], xmm2', '    movdqu [rsp+70h], xmm3',
            f'    mov ecx, {index}', '    call f23b_resolve', '    mov r11, rax',
            '    mov rcx, [rsp+20h]', '    mov rdx, [rsp+28h]',
            '    mov r8, [rsp+30h]', '    mov r9, [rsp+38h]',
            '    movdqu xmm0, [rsp+40h]', '    movdqu xmm1, [rsp+50h]',
            '    movdqu xmm2, [rsp+60h]', '    movdqu xmm3, [rsp+70h]',
            '    add rsp, 0A8h', '    jmp QWORD PTR [r11]', f'{name} ENDP']
asm += ['END']
(out / 'forward.asm').write_text('\n'.join(asm) + '\n')
(out / 'exports.def').write_text('LIBRARY F23B_NativeForward\nEXPORTS\n' + '\n'.join(names) + '\n')
(out / 'names.hpp').write_text('#pragma once\ninline constexpr const char* names[] = {\n' +
                               ',\n'.join('"' + n + '"' for n in names) + '\n};\n')
