# UTF-8 Source Encoding Cleanup Design

## Context

PR #11 exposed existing mojibake in `src/Billiards/billiards.cpp`. A repository-wide audit of the merged `master` branch found six tracked text files affected by mixed GBK and UTF-8 handling:

- `src/Billiards/particle.h` is encoded as GBK rather than UTF-8.
- `src/Billiards/billiards.cpp` contains the mojibake token `锟斤拷` in a comment.
- `src/Billiards/obj/bench.mtl`, `bench.obj`, `wardrobe.mtl`, and `wardrobe.obj` contain Unicode replacement characters in generated header comments.

The project currently has no repository-level editor configuration, Git encoding declaration, or automated encoding check. That allows invalid UTF-8 and already-decoded mojibake to be committed without CI noticing.

## Goals

- Store every tracked source, configuration, documentation, script, and text resource as valid UTF-8.
- Repair all six known corrupted files without changing runtime behavior.
- Preserve recoverable Chinese comments as Chinese UTF-8 text.
- Replace irrecoverable generated-file header comments with clear ASCII/English text.
- Declare the UTF-8 policy in editor, Git, and contributor-facing project configuration.
- Make the policy executable so future violations fail local checks and CI.

## Non-goals

- Rewriting or translating valid comments throughout the project.
- Converting binary assets or treating arbitrary binary bytes as text.
- Changing game logic, rendering, physics, or automation behavior.
- Reconstructing Chinese text that has already been irreversibly replaced by U+FFFD.

## Repair Strategy

### Recoverable content

`src/Billiards/particle.h` decodes cleanly as GBK. Convert the complete file to UTF-8 while retaining its original Chinese comments and code exactly.

The mojibake comment immediately before `b_music()` in `src/Billiards/billiards.cpp` originated as `载入纹理`, but that wording does not describe the function. Replace it with the accurate comment `// Background music` rather than preserving an inaccurate recovered phrase.

### Irrecoverable content

The OBJ/MTL files already contained U+FFFD replacement characters in the repository's initial history, so their original Chinese header text cannot be recovered reliably. Replace only the damaged first-line header in each file with an English marker that preserves the existing timestamp:

- Bench files: `# File created: 14.09.2015 17:06:28`
- Wardrobe files: `# File created: 30.05.2016 20:43:35`

No geometry, material definitions, or other resource data will change.

## Encoding Policy and Enforcement

Add `.editorconfig` with UTF-8 as the repository-wide character set and LF line endings for text files. Add `.gitattributes` rules that identify source and text-resource extensions as text, normalize them to LF, and declare their working-tree encoding as UTF-8.

Document in `README.md` that all source code, configuration, documentation, scripts, and text resources must be saved as UTF-8 rather than GBK or another legacy encoding.

Add a Python scanner under `scripts/` and invoke it at the beginning of `scripts/check.sh`. The scanner will:

1. Enumerate files with `git ls-files`, so generated and ignored build output is excluded.
2. Read each tracked file as bytes and skip files containing NUL bytes as binary.
3. Decode every remaining file strictly as UTF-8.
4. Reject decoded text containing U+FFFD or the known mojibake sequence `锟斤拷`.
5. Report the affected path plus the invalid byte offset or suspicious token, then return a non-zero exit status.

Scanning all tracked text includes vendored source headers. This is intentional: the repository promises that every code file is UTF-8 regardless of ownership, and the current vendored text already satisfies the policy.

## Error Handling

For invalid UTF-8, diagnostics identify the file and byte offset reported by the decoder. For suspicious decoded text, diagnostics identify the file and token. The scanner reports all detected violations in one run instead of stopping at the first file, making a failed CI job directly actionable.

Git command failures and unreadable tracked files are treated as check failures with a concise diagnostic. Binary files are skipped only when a NUL byte is present; this avoids maintaining a fragile extension allowlist while preventing images and compiled assets from being decoded as text.

## Validation

Implementation follows a red-green sequence:

1. Add and run the scanner against the current branch before repairs. It must fail and identify exactly the six audited files.
2. Apply the encoding conversions and comment repairs.
3. Run the scanner again; it must pass across the complete tracked tree.
4. Run `scripts/check.sh`, including configuration, compilation, unit/integration tests, headless automation E2E, and rendered screenshot tests.
5. Inspect the final diff and independently audit all tracked text for strict UTF-8, U+FFFD, and `锟斤拷` before publishing the PR.

The final changes must be limited to encoding policy/enforcement, documentation, and the six repaired files. Runtime output and game behavior must remain unchanged.
