# Release Legal Checklist (VintageDrummer)

Use before publishing each release.

1. Versioning
- [ ] `PLUG_VERSION_STR` and release tag match (for example `1.0.1` and `v1.0.1`).

2. License files in repository root
- [ ] `LICENSE` (MIT for code)
- [ ] `LICENSES/ASSETS_LICENSE.md`
- [ ] `THIRD_PARTY_NOTICES.md`
- [ ] README has a License section with links and contact email

3. Third-party compliance
- [ ] iPlug2 notice preserved
- [ ] VST3 SDK notice preserved
- [ ] Roboto/any other bundled font notices preserved
- [ ] Any added third-party dependency/asset is listed in `THIRD_PARTY_NOTICES.md`

4. Asset provenance
- [ ] Confirm all bundled samples/MIDI/images are owned by Zalex or properly licensed
- [ ] Store proofs/links for purchased or licensed content

5. VST2 entitlement check
- [ ] Confirm legal right to distribute VST2 binaries
- [ ] If not confirmed, remove VST2 artifact from release and notes

6. Release notes
- [ ] Mention included artifacts (`.vst3`, optional `.dll`)
- [ ] Mention platform support and known limitations
- [ ] Mention checksum hashes for binaries

7. Binary integrity
- [ ] Publish SHA256 sums for each artifact
- [ ] Verify uploaded release assets match local checksums
