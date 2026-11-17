# Source attribution

- New Melodee code is credited to `Kerem Kilic (Ellic Studio)`; do not copy Leo Kuroshita's copyright header into independently written files.
- Preserve the applicable SPDX licence identifier and all actual third-party notices.
- For mixed files, credit Melodee's work and identify retained Felucca code with `Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments`.
- Check source history and provenance when moving, extracting, porting or replacing code. A shared licence, include, protocol, or generic boilerplate does not establish authorship.
- Keep original notices on unchanged upstream code. Do not remove attribution merely because a derived file has extensive modifications.

# UI status placement

- Follow `docs/design/README.md` → “Status indicators” for new or changed indicators.
- Ongoing state belongs in the shared header and follows the state lifetime; reserve toasts for brief explanations and dialogs for decisions.

# Hookology

- Hookology is a fork of Melodee 1.6.1. Mark Hookology changes with a `(Hookology)` comment and record them in `HOOKOLOGY.md`.
- Its owner does not program: explain changes in plain language, and get designs approved before coding.
- Never install firmware on the FM-1, commit, push, or make anything public without the owner's explicit go-ahead.

# Releases

- Follow [BUILDING.md → Releasing Hookology](BUILDING.md#releasing-hookology). Melodee's release workflows are in `.github/melodee-workflows/` and are not used.
