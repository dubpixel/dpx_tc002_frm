---
applyTo: "pio-scripts/*.py,.github/workflows/*.yml,CHANGELOG.md,VERSION,package.json"
---
# Release & Web-Flash Pipeline — How It Works, and What's Bitten Us

This documents the actual end-to-end path from "bump the version" to "a
customer can click Install on `friendster.dubpixel.tv/flash-guide` and get
working firmware." Written after shipping 0.8.0 broken and fixing it as
0.8.1 within the same session — the bugs below are real, not hypothetical.

## The pipeline, step by step

1. **Version bump** (`VERSION`, `package.json`) + **CHANGELOG.md entry**,
   as a standalone commit (`chore: bump version to X.Y.Z`). See
   `.github/AGENTS.md`'s Version Bump Workflow for the commit conventions.
   CHANGELOG.md entries **must** start with `### [X.Y.Z](...)` exactly —
   `release.yml`'s changelog extraction is a dumb `awk` match on that
   literal pattern (see "Changelog format is load-bearing" below).
2. **Tag the commit**: `git tag -a dpx-vX.Y.Z -m "..."` — note the
   `dpx-v` prefix (not bare `vX.Y.Z`; that was the old pre-0.6.0
   convention, and `release.yml`'s version extraction strips both).
3. **Push both**: `git push origin main` then `git push origin dpx-vX.Y.Z`.
   **Don't push them in the same breath without knowing what that
   triggers** — see "The two-push race" below, this is the actual bug
   that broke 0.8.0.
4. The tag push triggers two separate workflows:
   - **`WLED Release CI`** (`release.yml`) — builds firmware, extracts
     this version's CHANGELOG.md section, creates a **draft** GitHub
     Release (you still have to manually publish it).
   - **`Pages Deploy`** (`pages-deploy.yml`) — builds firmware again
     (separately, not shared with the above), writes `manifest.json` +
     copies the `.bin` files into `site/firmware/release/`, deploys to
     GitHub Pages. **This is what `site/flash/` and `flash-guide.html`
     actually serve** — the GitHub Release's attached binaries are a
     separate, redundant copy for people who want a raw download.
5. A plain push to `main` (no tag) only triggers `Pages Deploy`, and only
   rebuilds the **nightly** channel (`site/firmware/nightly/`).
   `flash-guide.html` is hardcoded to the **release** manifest — a
   nightly-only push never touches what it serves.

## Real bugs found and fixed this way (both from GH #151/#152 era)

### Bug 1 — MQTT provisioning credential missing from first-boot config (GH #151)
Unrelated to the pipeline itself, but the reason a release got cut at
all this round: `dpx_firstboot.h` wrote blank MQTT `user`/`psk`, so a
freshly-flashed device could never authenticate to the broker post-GH
#110 (dynsec). Fixed by baking a real shared credential in via
`DPX_MQTT_PROV_USER`/`DPX_MQTT_PROV_PASS` build-time defines, sourced
from a **GitHub Actions repo secret** for public builds (`gh secret set
DPX_MQTT_PROV_USER`/`DPX_MQTT_PROV_PASS`) and a gitignored
`platformio_local.ini` for local dev builds. **These secrets must be set
before cutting a release that depends on them** — the build silently
succeeds with empty-string defaults if they're not, producing firmware
that compiles fine but is just as broken as before the fix.

### Bug 2 — tag pushes silently drop the main firmware from manifest.json (GH #152)
The actual 0.8.0-breaking bug. `pio-scripts/output_bins.py`'s
`_branch_slug()` (added for GH #101's dev-branch-build naming) reads
`GITHUB_REF_NAME` — which on a **tag push** is the tag itself (e.g.
`dpx-v0.8.0`), not a branch name. Since that's not `"main"`, the
branch-suffix logic treated an official release build as a dev-branch
build and suffixed the firmware filename
(`dpx_tc002_0.8.0_TC001-dpx-v0.8.0-<sha>.bin`).

`pio-scripts/dpx_web_flash_bins.py` separately checks
`os.path.isfile(f"dpx_tc002_{version}_TC001.bin")` (no suffix
awareness) before adding the main firmware to `manifest.json` — the
check silently failed, and the manifest shipped with only the
bootloader/partitions/boot_app0 stub entries, **no actual application
image**.

**Symptom on real hardware**: ESP Web Tools "successfully" flashed in
~2 seconds (should take ~1-2 minutes), chip erased fine, wrote only the
three tiny stub files, then couldn't boot a real application —
continuous buzzer beeping, no error anywhere in the UI. The actual
proof came from the **browser's DevTools console log** during the
flash attempt (not server-side checks) — it showed exactly which flash
offsets got written (0x1000, 0x8000, 0xe000) and which didn't (0x10000,
where the real app goes). **If "flash finishes suspiciously fast and
then the device won't boot" ever recurs, get that console log first —
it tells you immediately whether this is a manifest problem or
something else.**

Fixed: `_branch_slug()` now returns `None` for `GITHUB_REF_TYPE ==
"tag"`. `dpx_web_flash_bins.py` now **raises** (fails the build) instead
of silently omitting the manifest entry if the main firmware file isn't
found where expected — this class of bug should never again ship
silently.

### Bug 3 — the two-push race (discovered fixing Bug 2, no GH issue filed — infra-level, not product-level)
`git push origin main` followed immediately by `git push origin
dpx-vX.Y.Z` (pushing a tag that points at a commit already on `main`)
triggers **two concurrent `Pages Deploy` runs** — one for `nightly`
(from the `main` push) and one for `release` (from the tag push). They
run in separate `concurrency` groups (`pages-nightly` /
`pages-release`), so neither cancels the other, but **GitHub Pages
deployment is "last upload wins," not transactional** — the one that
finishes publishing *last* determines the live site's content.

Each run's "preserve the other channel's currently-live files" step
fetches the *other* channel's manifest+bins over HTTP from the live
site, to carry them forward unchanged. If the nightly run's preserve
step fetches the release channel's files **before** the release run has
actually published its fix, it preserves the *old, broken* release
files — and if the nightly run then finishes and deploys *after* the
release run, it republishes the whole site with that stale copy,
silently undoing the release run's fix even though both runs reported
`success`.

**Confirmed empirically**: this is exactly what happened the first time
0.8.1 was tagged — both runs succeeded, but the live manifest still
showed 0.8.0 minutes later. Re-triggering *only* the release build
(`gh workflow run "Pages Deploy" --ref dpx-vX.Y.Z`, with no competing
`main` push happening at the same time) fixed it immediately — confirmed
via the build log showing the correctly-named, unsuffixed firmware file
staged and uploaded.

**Mitigation for next time**: after pushing a tag, don't also push
`main` in the same window (the version-bump commit should already be on
`main` *before* you tag it, not pushed simultaneously). If a release
ever looks stale shortly after tagging, don't just wait for cache —
check `gh run list` for a `main`-triggered `Pages Deploy` run that
started around the same time, and if one did, re-run the release build
with `gh workflow run "Pages Deploy" --ref dpx-vX.Y.Z` to get a clean,
uncontested deploy. **This workflow has a real design gap** (the
"preserve other channel" step isn't safe against concurrent deploys to
the other channel) that's worth fixing properly if this keeps
happening — not done yet, this doc is the interim mitigation.

### Bug 4 — `actions/deploy-pages` dedupes by commit SHA, so re-running a stuck tag NEVER fixes it (found fixing 0.8.1, the reason Bug 3's "just re-run it" mitigation didn't work)

**This is the important one — read this before re-running a `Pages Deploy`
build for a release that's showing stale content.**

`actions/deploy-pages@v4` sets its Pages deployment's identity
(`pages_build_version`) directly from `GITHUB_SHA` — confirmed by reading
its source (`this.sha = process.env.GITHUB_SHA` in `api-client.js`; no
action input overrides it). A tag (`dpx-v0.8.1`) always points at one
fixed commit, so **every single `Pages Deploy` run against that tag —
whether from the original push, a `workflow_dispatch` retry, or a tenth
retry — creates a deployment with the exact same ID.** If the *first*
deployment for that SHA landed with bad content (e.g. lost the Bug 3
race), GitHub Pages appears to treat every later "deployment" of the
same SHA as redundant and never actually swaps the live content — no
matter how many times the build re-runs and re-uploads a correct
artifact.

**Symptom**: `gh run view` shows a brand new, fully successful run that
built and staged the *correct* manifest.json + firmware .bin (confirmed
in the build log), `actions/deploy-pages`'s own log says `Reported
success!`, and the GitHub Deployments API shows a new deployment
record with a recent `created_at` — but `curl`'ing the live manifest
(even from a different CDN edge node with a confirmed cache `MISS`,
proving it's not an edge-cache artifact) still shows the **old**
content, with a `Last-Modified` timestamp frozen at whenever the
*first* (bad) deploy for that SHA went live. This can persist for 40+
minutes and will never resolve on its own — it is not slow propagation,
it's GitHub Pages refusing to re-publish an SHA it already has a record
for.

**How to tell this is what's happening**: grep the `Deploy to GitHub
Pages` step of the suspect runs' logs for `pages_build_version` / `ID:`.
If multiple runs against the same tag show the identical SHA there,
re-running that tag again will not help.

**The only real fix**: get a new commit SHA. Re-running
`workflow_dispatch` against the *same* tag cannot do this — `--ref
dpx-vX.Y.Z` always resolves to the same commit. Cut a new patch release
(`X.Y.Z+1`) instead, even for a documentation-only or no-op change if
nothing else needs fixing — the version bump commit itself is what
buys a fresh SHA and therefore a fresh, un-stuck Pages deployment.

**Mitigation for next time**: avoid ever landing a bad first deploy for
a given tag's SHA in the first place — i.e. take Bug 3's mitigation
(don't push `main` and the tag in the same breath) seriously, because
unlike ordinary CDN staleness, **recovering from a bad deploy under a
given SHA costs an entire extra version bump**, not just a retry or a
wait.

### GitHub Pages' CDN cache — a real but secondary factor
GH Pages has a fixed ~10-minute edge cache that doesn't respect
query-string cache-busting (`?t=<timestamp>`) from an external client
like `curl` — that trick only works for a **browser's own `fetch()`
call** with `{cache: 'no-store'}` (already used in `site/flash/`'s own
channel-switcher JS). When verifying a fresh deploy from outside a
browser, a stale response for the first several minutes is normal and
not itself evidence of a failed deploy — check `gh run list` / the
actual build log first before assuming the deploy failed.

## Quick checklist for the next release

- [ ] `DPX_MQTT_PROV_USER` / `DPX_MQTT_PROV_PASS` repo secrets already
      set? (`gh secret list --repo dubpixel/dpx_tc002_frm`) — only
      matters if the MQTT provisioning fix is still relevant; generally,
      check whether any new build-time secret a recent fix depends on is
      actually set before tagging.
- [ ] Version bump + CHANGELOG.md entry committed and **pushed to `main`
      first**, confirmed on GitHub, *before* creating the tag.
- [ ] Tag pushed on its own, not in the same breath as another `main`
      push.
- [ ] After the tag's `Pages Deploy` run completes, check `gh run list`
      for a second, `main`-triggered `Pages Deploy` run with an
      overlapping time window. If one exists, re-run the release build
      explicitly: `gh workflow run "Pages Deploy" --ref dpx-vX.Y.Z` —
      **but only once, and only if the live manifest hasn't settled
      within ~2 minutes of that first retry's deploy-pages step
      finishing.** If it's *still* stale after the retry, stop retrying
      (see Bug 4 — re-running the same tag forever will not fix it) and
      cut a new patch version instead.
- [ ] Verify the live manifest directly:
      `curl -s "https://dubpixel.github.io/dpx_tc002_frm/firmware/release/manifest.json"`
      — confirm it shows the new version **and** a 4-part build (main
      firmware at offset `65536`, not just bootloader/partitions/boot_app0).
- [ ] Confirm the main `.bin` itself is actually reachable (not 404)
      before telling anyone to flash.
- [ ] If stale after one retry: grep that run's `Deploy to GitHub
      Pages` log step for `pages_build_version` — if it matches the
      SHA from an earlier bad deploy of the same tag, this is Bug 4, not
      cache lag. Cut `X.Y.Z+1` to get a fresh commit SHA rather than
      retrying again.
- [ ] The GitHub Release itself ships as a **draft** — remember to
      publish it manually if a public release-notes page matters this
      time.
