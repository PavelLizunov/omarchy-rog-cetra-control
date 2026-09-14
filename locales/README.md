# Plugin Localization

`Cetra.qml` creates one I18n per view. CetraViewModel exposes
`tr(key, fallback, params)` to explicit section components. SettingToggle uses
the same formatter through `panelRoot`; no section owns another translator.
The manifest exposes a persisted `locale` string setting, defaulting to `system`.
Bundled languages: `en`, `ru`, `de`, `fr`, `es`, `it`, `pt`, `zh`, `ja`, `ko`. Unsupported languages fall back to English.
Localization adds no hardware communication, process, or network request.

## Integration

The entry point assigns its inherited i18n property:

```qml
i18n: I18n {
  language: root.preference("locale", "system")
}

Text {
  text: root.tr("noise.switching", "Switching to {mode}\u2026", {
    mode: root.modeText("ambient")
  })
  textFormat: Text.PlainText
}
```

`language` defaults to `"system"`. `"auto"`, `"system"`, and an empty string use
`Qt.locale().name`; an explicit language tag overrides it. Set the widget's
`locale` through the header picker or host settings. The host supports a string schema field (also used by
its built-in agents plugin); its manifest validator does not check field types.
The header picker lists System and the ten bundled languages. This setting is independent of the headset's
English/Chinese/beep voice prompts. A changed system locale may require a shell
reload if Qt does not notify the running process.

`text(key, fallback, values)` uses stable named keys from `en.json`. Always pass
the complete English source as `fallback`, including on tooltips, accessible
labels, status messages, and translated model labels. It is immediately usable
before any asynchronous read completes. Missing keys fall through exact locale,
base language, English catalog, then the supplied fallback. Without a string
fallback, the key itself is the last resort. Empty/whitespace-only entries and
non-string values count as missing. Add each new key to `en.json` in the same
change as its widget fallback; tests require exact key/value coverage with no
unused English entries. Use complete captions with named placeholders, not
concatenated English fragments, including percentages and channel/value labels.

`{name}` interpolates own string, number, or boolean properties from `values`.
Repeated placeholders work; missing, null, or object values leave the placeholder
intact. `{{` and `}}` escape literal braces (`{{name}}` displays `{name}`). Inserted
values are not recursively interpolated and dollar signs are literal. There is
no evaluation, plural engine, HTML escaping, or rich-text support. **Every text
consumer must use `Text.PlainText` or an equivalent plain-text component API**;
Qt's default `AutoText` is not safe for arbitrary catalog/parameter content.
Direct widget `Text` items explicitly use `Text.PlainText`. The installed Omarchy
`Button`, `PanelHero`, `PanelSectionHeader`, and bar tooltip also render plain text;
do not set an unsupported `textFormat` property on composite buttons. Manifest
names, descriptions, and schema labels remain static English: the host manifest
contract has no plugin translation-key resolution. Host-owned generic UI strings
are outside this catalog.

Translate display labels only. Keep protocol enums (`off`, `anc`, `ambient`,
`english`, `sound`, etc.), opcodes, IPC commands, settings IDs, keyboard bindings,
and telemetry fields unchanged. Pass the localized display label, not an
uppercased protocol value, to `noise.switching`. Native microphone state must
remain unknown; no translation may imply confirmed Live/Muted or proven case
placement.

## Locale Selection

`locale` exposes the normalized request. Underscores become hyphens, language
subtags are lowercase, scripts title case, and alphabetic regions uppercase.
Examples: `PT_br` becomes `pt-BR`, `zh_hant_tw` becomes `zh-Hant-TW`.
Modern BCP 47 language/script/region/variant/extension tags are supported
syntactically, without an IANA registry or deprecated-alias lookup. Private-use-only
and grandfathered tags, POSIX encoding/modifier suffixes, and malformed tags
fall back to English. The algorithm is exact tag, compatible base language, then `en`.
The bundled `zh` catalog is Simplified Chinese. Traditional requests use
`zh-Hant-TW -> zh-Hant -> en`; TW/HK/MO regions without an explicit script also
use `zh-Hant`. An explicit script wins over region (`zh-Hans-TW -> zh -> en`).
No locale becomes a
filesystem path merely because the user requested it.

`effectiveLocale` is the first loaded, nonempty catalog in that chain, or `en`.
`rightToLeft` follows its index direction, not an unavailable requested language:
an Arabic request with only English installed stays LTR. Partial catalogs can
mix translated and English strings. The widget opts in on `PanelKeyCatcher`, a
content item rather than the panel window, and propagates mirroring to children:

```qml
LayoutMirroring.enabled: i18n.rightToLeft
LayoutMirroring.childrenInherit: true
```

Review left/right hardware diagrams and keyboard navigation when adding RTL.
Labels and data stay paired with their physical earbud; protocol sides never swap.
The helper itself never changes any item's direction or geometry.

## Adding a Catalog

1. Add an ordinary UTF-8 JSON file containing a flat object with the stable keys
   from `en.json` and reviewed string values. Preserve interpolation names. Partial
   catalogs are allowed; empty strings cannot intentionally hide UI text.
2. Add a canonical locale key to `index.json` with `file`, a nonempty native
   display `name`, and `direction` (`ltr` or `rtl`). Filenames must match
   `^[A-Za-z0-9-]+\.json$`; directories, URLs, encoded paths, and symlinks are not
   allowed. For example, a future reviewed Brazilian Portuguese catalog could
   use `"pt-BR": {"file":"pt-BR.json","name":"Portuguese (Brazil)","direction":"ltr"}`.
3. Run `node tests/i18n/run.js` from the repository root and the normal plugin
   checks. Reload only through the authorized existing shell lifecycle, with the
   screen unlocked. Catalog files are read once per instance/selection, not watched.

Additional catalog IDs could include `pt-BR`, `uk`, `zh-Hant`, `ar`, and `hi`.
Base and regional catalogs are independent additions. English is pinned to `en.json`,
`English`, and `ltr` even if the index is missing, malformed, or tries to override
it. Invalid index entries are ignored independently.

## Loading and Safety

The index and English fallback each have a fixed local `FileView`. Only validated
index entries can produce optional catalog readers. URLs must resolve under the
bundled `locales/` directory with the `file:///` scheme; `FileView` receives the
decoded absolute path, because Quickshell 0.3.1 strips the URL prefix but does
not decode escaped path characters itself.
FileView follows filesystem symlinks: packaging validation must prohibit them;
this helper is not a sandbox against someone who can rewrite the plugin itself.

Optional readers have immutable request metadata and a new instance per selection.
Changing language clears translated dictionaries immediately. Generation and locale
checks reject late callbacks, including `A -> B -> A` changes; fixed English reads
are independent of that generation. This avoids FileView's old-data retention
when changing the path of an already-loaded view. Missing or malformed files fall
back without throwing; failed index/English reads cannot remove the source fallback.
Async disk loading does not block the UI, and there is no HID access.

## Tests and Limits

`node tests/i18n/run.js` uses only Node built-ins. It extracts the actual production
functions from I18n and modules listed in `tests/qml-source.js` into a VM, models asynchronous reader
callbacks, validates shipped JSON and paths, and tests locale fallback, malformed
input, interpolation, stale-request rejection, UI key coverage, plain-text and RTL
wiring, and unchanged command enums. It neither launches QML/Quickshell nor installs
anything or writes fixtures inside the active plugin. `tests/run.sh` includes
the i18n suite. Real QML signal ordering, binding updates,
translated layout, and RTL navigation still need
integration verification in the existing shell, not a second Quickshell process.
If the shell cached the QML directory before `I18n.qml` was added, the owning
integration session may need one authorized restart with the screen unlocked.
