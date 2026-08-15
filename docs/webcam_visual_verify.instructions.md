# Webcam-Assisted Visual Verification — Setup Primer

Used to visually confirm firmware effects (overlays, fonts, pairing PIN, etc.) on the
physical TC002 matrix by photographing it with the Mac's built-in webcam and reading
the result via vision. This is finicky on macOS — this doc exists because a prior
session burned a lot of time on it. Read it before troubleshooting from scratch.

## 1. Install imagesnap

```
brew install imagesnap
imagesnap -l          # should list "FaceTime HD Camera" (or similar)
```

## 2. Camera permission is the #1 failure mode

macOS gates camera access per-app via TCC (Transparency, Consent & Control), keyed to
the **bundle ID of whatever process spawned the shell** — usually Terminal.app,
iTerm2, or VS Code — not to `imagesnap` itself.

- Check: System Settings → Privacy & Security → Camera → confirm Terminal/iTerm/VS
  Code (whichever hosts the agent's shell) is toggled ON.
- If it's not listed at all, TCC has never prompted for that app. A prompt only
  fires from an **interactive GUI session** — a background/headless job (no TTY,
  no window focus) cannot trigger or answer the permission dialog. This is very
  likely what's blocking the other session if it's running as a background job.
- Fix: run one `imagesnap` capture manually from a normal, foregrounded terminal
  window on the Mac (not through a headless agent) so the OS prompt actually
  appears, click Allow, then background/agent sessions using that same terminal
  app's bundle ID will inherit the grant.
- If permission was previously denied, it won't re-prompt — you have to flip it on
  manually in Privacy & Security, or `tccutil reset Camera <bundle-id>` to force a
  fresh prompt.

## 3. Only one process can hold the camera at a time

macOS built-in cameras are exclusive-access. If Zoom, FaceTime, Photo Booth,
another `imagesnap` call, or another agent session already has it open, your
capture will hang or silently fail. Symptoms seen this session: 20-45s hangs,
near-black frames, or a frame that's clearly stale/cached from a prior open.

Check for holders before capturing:
```
ps aux | grep -i imagesnap | grep -v grep
```
Kill any stray ones (`kill <pid>`), and close Zoom/FaceTime/Photo Booth if open.

## 4. Warm-up matters

The first frame after opening the camera is often black or motion-blurred (auto
exposure hasn't settled). Always give it a warm-up delay:
```
imagesnap -w 1 /tmp/shot.jpg     # ~1s warmup before capture
```
If frames still look wrong, bump `-w` to 2-3s.

## 5. Always sanity-check the frame before trusting it

This session repeatedly got "successful" captures that were pointed at the
ceiling, the user, or a dark room — `imagesnap` returning 0 and writing a file
does **not** mean the shot is useful. Before treating a capture as verification:
- Check file size isn't suspiciously tiny (near-black/empty frame).
- Actually look at it (read as image) before concluding pass/fail on the effect
  under test.
- If it's clearly mis-aimed, that's an environment problem (camera physically
  pointed wrong), not a software one — reposition the Mac/camera relative to the
  device under test, this isn't fixable from the agent side.

## 6. Minimal working capture command

```
imagesnap -w 1 -d "FaceTime HD Camera" /tmp/tc002_check.jpg
```
Drop `-d` if there's only one camera (`imagesnap -l` to confirm the exact name).

## 7. If it's still not working

Most likely it's #2 (permission never granted because the session is headless).
The only real fix is a human running one interactive capture first, from a
foregrounded GUI terminal, to satisfy the TCC prompt once. There is no way for a
background/non-interactive agent session to grant itself camera permission.
