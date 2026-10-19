import pathlib
import sys
raw=pathlib.Path(sys.argv[1]).read_bytes()
with pathlib.Path(sys.argv[2]).open('w',encoding='utf-8') as out:
    out.write('#include <stddef.h>\n#include <stdint.h>\nconst uint8_t q_winapp_data[] = {\n')
    for at in range(0,len(raw),16):
        out.write('    '+','.join(str(c) for c in raw[at:at+16])+',\n')
    out.write('};\nconst size_t q_winapp_size = sizeof(q_winapp_data);\n')
