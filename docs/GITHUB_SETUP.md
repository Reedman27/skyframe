# Publishing Skyframe on GitHub

This package is a complete local Git repository, not a source-only ZIP. It contains the `main` branch and the locally reconstructed project history. No GitHub remote is configured and nothing has been pushed on the owner's behalf.

## Repository recovery note

The uploaded handoff ZIP did not contain a valid branch reference or recoverable original commit/tree history. The surviving staged index was preserved as a reconstructed baseline commit, then the current implementation and documentation were committed on top of it. The history is now usable and verifiable, but the reconstructed baseline must not be described as the original historical commit history.

## Suggested repository details

- **Name:** `skyframe`
- **Description:** Lightweight native 3D aircraft sandbox in C++20 and raylib.
- **Suggested topics:** `cpp`, `raylib`, `cmake`, `aircraft`, `simulation`, `game`, `linux`
- **Visibility:** Choose public or private when creating the repository.
- **License:** MIT (`LICENSE`).

## Verify the local repository

After extracting the ZIP, open a terminal in the `Skyframe/` directory:

```sh
git status --short --branch
git fsck --full
git log --oneline --decorate -5
```

The working tree should be clean on `main`. Build output under `out/`, local settings and the private `/SKYFRAME-QUICK-REVIEW.md` handoff note are excluded by `.gitignore`.

## Create the GitHub repository

Create an empty repository on GitHub named `skyframe`. Do not initialize it with a README, license, or `.gitignore`; those files and the Git history already exist in this package.

Add the remote and push the existing branch:

```sh
git remote add origin https://github.com/YOUR-ACCOUNT/skyframe.git
git push -u origin main
```

Replace `YOUR-ACCOUNT` with the GitHub account or organization that owns the repository. GitHub authentication is handled by your configured Git credential manager or SSH setup. No token or credential belongs in the project files.

## Optional portable Git bundle

A matching `Skyframe.bundle` is provided as a second backup/transfer format. To recreate a clone from it:

```sh
git clone /path/to/Skyframe.bundle Skyframe-from-bundle
cd Skyframe-from-bundle
git status --short --branch
git fsck --full
```

## After publishing

- Confirm the Actions workflow runs on the first push.
- Keep Issues enabled if you want bug reports and feature requests.
- Keep `docs/ROADMAP.md` and `docs/DEVELOPMENT_LOG.md` current.
- Claude Code should load `CLAUDE.md` automatically when opened at the repository root. Keep that file and the other permanent guidance documents in every branch and handoff.
- Keep actual application screenshots under `docs/screenshots/` and embed them in README only after verifying the exact build shown.
- Do not add credentials, personal machine settings, generated build output or `SKYFRAME-QUICK-REVIEW.md` to the repository.

The local Git repository and bundle do not create or publish the remote GitHub repository automatically.
