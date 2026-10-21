# Translating VOX4U

The interface of the plugin (import dialog, options, tooltips, messages, project settings) follows the language of the
Unreal Editor (*Editor Preferences → Region & Language*). Every translation is one `.po` file in this folder:

```
Plugins/VOX4U/Localization/
    VOX4U.pot   template: every English text of the plugin
    pl.po       Polish
    ...         one file per language
```

The plugin loads the file of the editor language when the editor starts and again when the language is changed.
Nothing has to be compiled and the Localization Dashboard is not needed. Texts without a translation stay in English.

## File names

The file is named after Unreal's culture code, with a **hyphen** (`pt-BR.po`, not `pt_BR.po`):

| language | file |
|---|---|
| Polish | `pl.po` |
| Spanish | `es.po` |
| German | `de.po` |
| French | `fr.po` |
| Italian | `it.po` |
| Portuguese (Brazil) | `pt-BR.po` |
| Russian | `ru.po` |
| Japanese | `ja.po` |
| Korean | `ko.po` |
| Chinese (Simplified) | `zh-Hans.po` |

A regional language falls back to its parent: with the editor in `pt-BR`, texts missing from `pt-BR.po` are taken
from `pt.po`.

## Translating with Poedit (recommended)

[Poedit](https://poedit.net) is a free editor for `.po` files (Windows, macOS, Linux).

### A new language

1. *File → New from POT/PO file…* and open `Localization/VOX4U.pot`.
2. Choose the language.
3. Translate: the English text is shown in the list, the translation is typed below it.
4. Save the file in this folder under the name from the table above, e.g. `es.po`.
   Poedit may suggest a name with an underscore (`pt_BR.po`); rename it to use a hyphen (`pt-BR.po`).
5. Start the editor in that language, or switch the language in *Editor Preferences → Region & Language*.

### An existing language

Open the `.po` file in Poedit, change the translations and save.

### Some texts appear more than once

Every text has its own key (the *context* in Poedit, `msgctxt` in the file). The same English word can appear several
times, e.g. *Scale* in the import dialog and in the reimport settings, because Unreal translates every property
separately. Translate each of them; Poedit suggests the translation you already gave.

## Translating in a text editor

A `.po` file is plain text, so any editor works:

1. Copy `VOX4U.pot` to the new file name, e.g. `es.po`.
2. In the header at the top, add the language: `"Language: es\n"`.
3. Fill in the `msgstr` lines:

   ```
   #. Key: VoxImportOption:Scale
   msgctxt "UObjectDisplayNames,VoxImportOption:Scale"
   msgid "Scale"
   msgstr "Escala"
   ```

4. Save the file as **UTF-8**.

Change only the `msgstr` lines. `msgctxt` and `msgid` must stay exactly as they are: the plugin uses them to find the
place of the text in the interface, and a translation whose `msgid` does not match the English text in the code is
ignored. Keep placeholders such as `{0}`, `{1}`, `{BaseName}` or `{Color}` in the translation.

## When the plugin's texts change

After texts are added or changed in the code, update the template and every translation:

```sh
python Tools/UpdateLocalization.py
```

The script:

- rebuilds `VOX4U.pot` from the source code,
- adds the new texts to every `.po` file (untranslated),
- marks translations whose English text changed as **fuzzy** (*Needs work* in Poedit). Fuzzy translations are not
  used until they are reviewed,
- removes texts that no longer exist,
- prints how many texts of each language are translated.

Then open the `.po` files, translate the new texts, review the fuzzy ones and, in Poedit, clear *Needs work*.
Poedit's *Translation → Update from POT file* does the same as the script for a single file.

`python Tools/UpdateLocalization.py --check` only reports and returns an error code when something is out of date,
for use in CI.

For developers: every text shown to users must be localizable (`LOCTEXT` / `NSLOCTEXT`, and an explicit
`DisplayName` and `ToolTip` on every visible `UPROPERTY` and `UENUM` value). The script warns about texts it
cannot collect.

## Other tools

- [Lokalize](https://apps.kde.org/lokalize/) (KDE) and [Gtranslator](https://wiki.gnome.org/Apps/Gtranslator)
  (GNOME): free desktop editors, mainly for Linux.
- [Weblate](https://weblate.org) and [Crowdin](https://crowdin.com): web services that connect to the GitHub
  repository, so that people can contribute translations without Git. Usually free for open source projects.

## Not translated

- Names of Blueprint functions and properties in Blueprint graphs (`AddVoxel`, `SetFrame`, ...).
- Asset names created by the importer (they follow the patterns in *Project Settings → Plugins → VOX4U*).
- Log messages.
