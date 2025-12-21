#!/usr/bin/env bash
# Abort pushes if secret-like files are tracked or private keys slip into the repo.
set -euo pipefail

ROOT_DIR="$(git rev-parse --show-toplevel)"
cd "$ROOT_DIR"

failures=0

# 1) Block tracked secret-like files (should all be gitignored)
PATTERNS=("*secrets.h" "*.pem" "*.key" "*.crt" ".env" ".env.local")
TRACKED=$(git ls-files -- "${PATTERNS[@]}" || true)
if [[ -n "$TRACKED" ]]; then
  echo "❌ Tracked secret-like files detected:" >&2
  echo "$TRACKED" | sed 's/^/   - /' >&2
  failures=1
fi

# 2) Block staged secret-like files (even if untracked overall)
STAGED=$(git diff --cached --name-only -- "${PATTERNS[@]}" || true)
if [[ -n "$STAGED" ]]; then
  echo "❌ Secret-like files are staged for commit:" >&2
  echo "$STAGED" | sed 's/^/   - /' >&2
  failures=1
fi

# 3) Look for private key blobs inside tracked files (excluding examples/docs)
if ! command -v rg >/dev/null 2>&1; then
  echo "❌ ripgrep (rg) is required for secret scanning. Install it and re-run." >&2
  exit 1
fi

if rg --hidden --no-ignore --iglob '!*.example' --iglob '!docs/**' \
      --iglob '!.git/**' --iglob '!**/README.*' \
      '-----BEGIN (RSA |EC |DSA )?PRIVATE KEY-----' > /tmp/secret-blobs.$$ 2>/dev/null; then
  echo "❌ Potential private key material found in tracked files:" >&2
  cat /tmp/secret-blobs.$$ >&2
  rm -f /tmp/secret-blobs.$$
  failures=1
else
  rm -f /tmp/secret-blobs.$$
fi

if [[ "$failures" -ne 0 ]]; then
  echo "
Push blocked. Remove secrets or unstaged sensitive files, then retry." >&2
  exit 1
fi

echo "✅ Secret scan passed (no tracked secret files or private keys)."
