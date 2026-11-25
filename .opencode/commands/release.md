---
description: Cut a Bialet release end to end — version bump, CHANGELOG, test gate, scoped security audit, merge to main, tag, and publish verification.
agent: build
subagent: false
---

Load the `release` skill (skill id: `release`) with the skill tool, then follow
its phases in order, Phase 0 through Phase 6, in this session. Each phase has a
pass condition; stop and ask instead of skipping one.

Run this in the current session, never in a subagent: every phase that commits,
tags, or pushes needs interactive confirmation from the user. Do not commit,
tag, or push until the user has confirmed the target version explicitly.

Release request: $ARGUMENTS

If the request is empty, start with Phase 0 (preflight) and then propose a
version from the diff since the last tag.
