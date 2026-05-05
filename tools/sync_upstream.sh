#!/usr/bin/env bash
# sync_upstream.sh — Sync our 70D repo with reticulatedpines/magiclantern_simplified
#
# This repo was forked from reticulatedpines/magiclantern_simplified (the ML mainline).
# Both the ML codebase and the qemu-eos subtree may receive upstream changes.
# This script automates the merge process.
#
# Usage:
#   ./sync_upstream.sh                 # Check upstream status (dry run)
#   ./sync_upstream.sh --merge         # Perform the merge
#   ./sync_upstream.sh --merge-qemu    # Merge only qemu-eos subtree
#   ./sync_upstream.sh --status        # Show divergence stats

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SCRIPT_DIR"

UPSTREAM_REPO="https://github.com/reticulatedpines/magiclantern_simplified.git"
UPSTREAM_BRANCH="dev"
QEMU_REPO="https://github.com/reticulatedpines/qemu-eos.git"
QEMU_BRANCH="master"
QEMU_SUBTREE_PREFIX="qemu-eos"

# NOTE on qemu-eos upstream: The upstream repo's master branch no longer contains
# Canon EOS emulation code (hw/eos/* was removed). The last branch with EOS code
# is "qemu-eos-v4.2.1" which is what our subtree was forked from. Merging from
# master would DELETE all EOS emulation. DO NOT merge qemu-eos master unless
# upstream restores the EOS code. Our subtree is the canonical version.

# ── helpers ──────────────────────────────────────────────────────────────────
green()  { printf "\033[32m%s\033[0m\n" "$*"; }
yellow() { printf "\033[33m%s\033[0m\n" "$*"; }
red()    { printf "\033[31m%s\033[0m\n" "$*"; }

check_remote() {
    if ! git remote get-url upstream &>/dev/null; then
        echo "Adding upstream remote: $UPSTREAM_REPO"
        git remote add upstream "$UPSTREAM_REPO"
    fi
}

fetch_all() {
    echo "==> Fetching upstream ML..."
    git fetch upstream "$UPSTREAM_BRANCH" 2>&1 | tail -3
    echo "==> Fetching upstream QEMU..."
    git fetch "$QEMU_REPO" "$QEMU_BRANCH" 2>&1 | tail -3
}

# ── status ────────────────────────────────────────────────────────────────────
cmd_status() {
    check_remote
    fetch_all

    echo ""
    echo "=== Divergence Stats ==="

    # ML upstream
    LOCAL_CNT=$(git rev-list --count HEAD 2>/dev/null || echo "?")
    UPSTREAM_CNT=$(git rev-list --count upstream/"$UPSTREAM_BRANCH" 2>/dev/null || echo "?")
    BEHIND=$(git rev-list --count HEAD..upstream/"$UPSTREAM_BRANCH" 2>/dev/null || echo "?")
    AHEAD=$(git rev-list --count upstream/"$UPSTREAM_BRANCH"..HEAD 2>/dev/null || echo "?")

    echo "  Local commits:              $LOCAL_CNT"
    echo "  Upstream commits:           $UPSTREAM_CNT"
    echo "  Commits behind upstream:    $BEHIND (new in mainline, not in our fork)"
    echo "  Commits ahead of upstream:  $AHEAD (our 70D-specific changes)"

    if [ "$BEHIND" -gt 0 ] 2>/dev/null; then
        echo ""
        yellow "=== Upstream changes not yet merged ($BEHIND commits) ==="
        git log --oneline HEAD..upstream/"$UPSTREAM_BRANCH" | head -30
        if [ "$BEHIND" -gt 30 ]; then
            yellow "... and $((BEHIND - 30)) more"
        fi
    fi

    if [ "$AHEAD" -gt 0 ] 2>/dev/null; then
        echo ""
        echo "=== Our changes not upstream ($AHEAD commits) ==="
        echo "(These are our 70D-specific commits — not expected upstream)"
    fi

    # QEMU subtree
    echo ""
    echo "=== QEMU-eOS Subtree ==="
    QEMU_HEAD=$(git ls-tree HEAD "$QEMU_SUBTREE_PREFIX" 2>/dev/null | awk '{print $3}' | cut -d' ' -f1 || echo "?")
    if [ "$QEMU_HEAD" != "?" ]; then
        QEMU_BEHIND=$(git rev-list --count "$QEMU_HEAD"..FETCH_HEAD 2>/dev/null || echo "?")
        echo "  Local QEMU commit:          ${QEMU_HEAD:0:8}"
        echo "  Upstream QEMU commits behind: $QEMU_BEHIND"
    fi
}

# ── merge ML upstream ─────────────────────────────────────────────────────────
cmd_merge() {
    check_remote
    echo "==> Fetching upstream $UPSTREAM_BRANCH..."
    git fetch upstream "$UPSTREAM_BRANCH"
    echo ""
    echo "=== Upstream changes ==="
    git log --oneline HEAD..upstream/"$UPSTREAM_BRANCH"
    echo ""
    yellow "WARNING: Merging upstream changes will create a merge commit."
    yellow "This may cause conflicts with our 70D-specific changes."
    echo ""
    read -r -p "Proceed with merge? [y/N] " reply
    if [ "$reply" != "y" ] && [ "$reply" != "Y" ]; then
        echo "Aborted."
        exit 0
    fi
    echo ""
    echo "==> Merging upstream/$UPSTREAM_BRANCH..."
    if git merge upstream/"$UPSTREAM_BRANCH" --no-edit; then
        green "✓ Merge successful!"
        echo ""
        echo "Next steps:"
        echo "  1. Run ./validate_build.sh to verify build"
        echo "  2. Run tests/run_all.sh for pre-deployment checks"
        echo "  3. Test in QEMU: ./test_70d_qemu.sh --boot --no-build --timeout 60"
        echo "  4. Commit & push: git push origin main"
    else
        red "✗ Merge conflicts detected!"
        echo ""
        echo "Resolve conflicts, then:"
        echo "  git add <resolved-files>"
        echo "  git commit"
        echo "  ./validate_build.sh"
        echo "  git push origin main"
        exit 1
    fi
}

# ── merge QEMU subtree ────────────────────────────────────────────────────────
cmd_merge_qemu() {
    echo "==> Fetching upstream QEMU..."
    git fetch "$QEMU_REPO" "$QEMU_BRANCH"
    echo ""
    echo "=== Upstream QEMU changes ==="
    QEMU_HEAD=$(git ls-tree HEAD "$QEMU_SUBTREE_PREFIX" 2>/dev/null | awk '{print $3}')
    git log --oneline "$QEMU_HEAD"..FETCH_HEAD
    echo ""
    yellow "WARNING: QEMU subtree merges can be complex."
    yellow "Always verify the build after merging."
    echo ""
    read -r -p "Proceed with QEMU subtree merge? [y/N] " reply
    if [ "$reply" != "y" ] && [ "$reply" != "Y" ]; then
        echo "Aborted."
        exit 0
    fi
    echo ""
    echo "==> Merging QEMU subtree..."
    QEMU_FETCH=$(git rev-parse FETCH_HEAD)
    if git merge -s subtree "$QEMU_FETCH" --no-edit; then
        green "✓ QEMU subtree merge successful!"
        echo ""
        echo "Next steps:"
        echo "  1. Rebuild QEMU: cd qemu-eos/build && make -j\$(nproc)"
        echo "  2. Test 70D boot: ./test_70d_qemu.sh --boot --no-build --timeout 60"
        echo "  3. Commit & push"
    else
        red "✗ Merge conflicts in QEMU subtree!"
        echo "Resolve conflicts, then commit and rebuild QEMU."
        exit 1
    fi
}

# ── main ──────────────────────────────────────────────────────────────────────
case "${1:-}" in
    --status)     cmd_status ;;
    --merge)      cmd_merge ;;
    --merge-qemu) cmd_merge_qemu ;;
    *)
        echo "Usage: $0 [--status|--merge|--merge-qemu]"
        echo ""
        cmd_status
        echo ""
        echo "Run with --merge to pull in upstream ML changes."
        echo "Run with --merge-qemu to update the qemu-eos subtree."
        echo "Run with --status (or no args) to check divergence."
        ;;
esac
