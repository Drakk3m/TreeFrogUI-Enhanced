# Enhanced Game Details

The existing three-column platform filelist keeps working. The following explicit UTF-8 header opts in to optional details. Keep the three CubeGM columns first and leave unknown values empty. Rows may omit trailing optional columns. Use the real ROM filename, and quote fields containing commas or double quotes.

~~~csv
Rom.ext,Display Title,ChineseTitle,Original Title,Release Date,Developer,Publisher,Genre,Players,Description
Example.nes,Example,,Example Original,1994,Example Studio,,Platformer,2,"Explore the cave, find its secrets."
Second.nes,Second,
~~~

FrogUI reads the current platform's filelist when it scans the platform. Entries remain tied to the exact physical ROM filename. The first nonempty title and the first nonempty detail record win independently. Existing metadata-free files retain filename fallback. Legacy ROM Studio 87.9.2 writes three columns and its repair path can discard extra columns, so keep a copy of an enriched CSV when using that version to rewrite it. The extra header is a FrogUI extension. Compatibility of enriched files with CubeGM itself needs a separate test.

Each field is limited to 255 UTF-8 bytes and the CSV to 1 MiB. The listing retains at most 256 KiB of detail text plus a four-byte offset per entry up to the existing 100,000-entry scan limit. The launch path, artwork key and recent-game identity use the actual ROM. The most recent exact path is labeled Last played. Other exact matches are labeled In Recents. The native history does not contain a timestamp, and legacy entries without paths are left unmarked.

## Offline RetroAchievements

When a game has a valid ROM Studio export, Details shows L2 Achievements. Up and Down browse achievements. B or L2 returns to the same game. The gallery displays the title, description, point value and optional PNG badge. It reads the existing SD root-relative path mapping in cubegm/enhanced/ra/rom_identity.json and the matching metadata in cache/<console>/<hash>/gamepack.json. The exported ROM size must match the scanned ROM. Hash and game identifiers must agree across the two JSON files. FAT copy timestamps are not used. Each JSON file is capped at 2 MiB, with one game pack in memory and at most 256 achievements. Missing, incompatible or corrupt exports hide the gallery shortcut. The UI displays the available achievements from the exported metadata. Offline game packs do not establish unlocks, and the Home counter remains the previously approved placeholder.

The supplied transparent 28×28 trophy is built into the frontend. A theme may replace it with a valid [resource.ra.trophy] entry in its manifest. The frontend scales a theme image to fit the existing space and keeps the bundled icon available as a fallback. Missing per-achievement badge artwork uses the trophy as well.
