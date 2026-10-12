const fs = require('node:fs');
const { execFileSync } = require('node:child_process');

// Node cannot exchange nonempty directories. Use the OS atomic swap via Python's stdlib.
function replaceDirectory(staging, destination) {
  if (!fs.existsSync(destination)) {
    fs.renameSync(staging, destination);
    return;
  }
  const script = `
import ctypes, os, sys
libc = ctypes.CDLL(None, use_errno=True)
source, destination = map(os.fsencode, sys.argv[1:])
if hasattr(libc, 'renameat2'):
    rename = libc.renameat2
    rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
    rename.restype = ctypes.c_int
    result = rename(-100, source, -100, destination, 2)  # RENAME_EXCHANGE
elif hasattr(libc, 'renamex_np'):
    rename = libc.renamex_np
    rename.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint]
    rename.restype = ctypes.c_int
    result = rename(source, destination, 2)  # RENAME_SWAP
else:
    raise RuntimeError('Atomic Stable promotion requires Linux or macOS with Python 3')
if result != 0:
    error = ctypes.get_errno()
    raise OSError(error, os.strerror(error))
`;
  execFileSync('python3', ['-c', script, staging, destination], { stdio: ['ignore', 'pipe', 'pipe'] });
  // After the exchange, staging holds the old snapshot; its caller cleans it up.
}

module.exports = { replaceDirectory };
