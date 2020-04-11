# romdb

[![](https://github.com/romdb/romdb/workflows/CI/badge.svg)](https://github.com/romdb/romdb/actions?query=workflow%3ACI)

romdb is a [SQLite](https://sqlite.org) based archive format used to store files and metadata. Its main purpose is to store collections of ROM files for 8-bit/16-bit video game consoles. This repository is for the specification and reference implementation.

# Why create romdb?

Having a folder with the full collection of ROM files for a specific console is not ideal. It's hard to search for a specific file when you have many files. It can take a lot of space if you're collecting all the hacked ROMs and different region versions. It's hard to tell the different versions apart by their names.

You can save space by storing different versions of the same archive file to compress redundancy, but then you can't load the files without extracting them. Most emulators only support single file zip archives, so using a better compressor (7zip) is only useful to archive the collection.

romdb can:

1. Store multiple ROM collections in one file
2. Add System and Media information to each file
3. Optionally compress files using deflate or xz
4. Store [VCDIFF](https://en.wikipedia.org/wiki/VCDIFF) patches for similar files to save space
5. Associate metadata to files and media

# How to use

```
SYNOPSIS
        romdb [-o <romdb file>] [-s <romdb schema file>] [-r <roms path/dump path>] [-i <import
              system(s) files path>] [-p <create patch.txt from import path>] [-c <import
              configuration name>] [-d] [-f] [-v] [-h]

OPTIONS
        -d, --dump  dump roms
        -f, --full-dump
                    dump roms and metadata

        -v, --verify
                    verify romdb integrity

        -h, --help  help
```

### import a system
`romdb -o test.db -i "Z:\roms\master system"`

### import a system using a different configuration
`romdb -o test.db -i "Z:\roms\master system" -c onlygood`

<details><summary>Configuration option</summary>

When you want to test different import options without modifying the import files, you can copy the file you want to modify and name it like `<filename>.<configuration name>.txt`:

```
├── files
│   └── ...
├── filetag
│   └── ...
├── mediatag
│   └── ...
├── file.txt
├── media.txt
├── media.xz.txt
├── patch.txt
├── system.txt
└── system.xz.txt
```
When running the import like this: `romdb -o test.db -i "Z:\roms\master system" -c xz`
The importer will try to load each of the 4 root `.txt` files with `xz` in the name and fallback to the default file if not found.
</details>

### import a system and specify a different files folder
`romdb -o test.db -r "Z:\roms\master system\files" -i "Z:\roms\master system"`

### verify a romdb
`romdb -o test.db -v`

### dump files
`romdb -o test.db -d -r "Z:\dump"`

### dump files and metadata
`romdb -o test.db -d -f -r "Z:\dump"`

### create patch.txt from filenames
`romdb -i "Z:\roms\master system" -p Z:\patch.txt`

# Building from source

<details><summary>Linux</summary>

### Installing dependencies on Debian and Ubuntu
```
sudo apt-get install cmake g++ libsqlite3-dev zlib1g-dev liblzma-dev
```
### Compiling
```
cmake CMakeLists.txt
make
```
</details>

<details><summary>Windows</summary>

### Installing dependencies with vcpkg

1. Install vcpkg following the instructions from https://github.com/microsoft/vcpkg#quick-start.

   Don't forget to perform _user-wide integration_ step for additional convenience.
2. Install required dependencies by executing the following command (via cmd or powershell):

   For the 32-bit version of the dependencies please run this command:

   ```
   vcpkg install sqlite3:x86-windows zlib:x86-windows liblzma:x86-windows
   ```

   For the 64-bit version of the dependencies please run this command:

   ```
   vcpkg install sqlite3:x64-windows zlib:x64-windows liblzma:x64-windows
   ```

### Compiling

* **Visual Studio 32-bit**
```
cmake -G "Visual Studio 16 2019" -A Win32 CMakeLists.txt -DCMAKE_TOOLCHAIN_FILE=<path to vcpkg.cmake>
```
Open the generated solution

* **Visual Studio 64-bit**
```
cmake -G "Visual Studio 16 2019" -A x64 CMakeLists.txt -DCMAKE_TOOLCHAIN_FILE=<path to vcpkg.cmake>
```
Open the generated solution

* **MinGW makefile**
```
cmake -G "MinGW Makefiles" CMakeLists.txt
make
```
</details>

# romdb format

Table    | Description                   | Example
---------|-------------------------------|--------------------------
system   | store system information      | `Master System`
media    | store media information       | `Sonic the Hedgehog`
file     | store a file                  | `Sonic the Hedgehog (UE) [!].sms`
checksum | store a checksum for a file   | `crc32:dabdabda`
tag      | store a tag or a tag -> value | `UE`, `genre:action`, `genre:platformer`, `year:1991`
mediatag | associate a tag to media      | `media 1 : tag 1`
filetag  | associate a tag to a file     | `file 1 : tag 2`

### Table hierarchy
```
├── system
│   └── media
│       ├── mediatag
│       └── file
│           ├── checksum
│           └── filetag
└── tag
```

<details><summary>Default table schema</summary>

```sql
CREATE TABLE system(
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,                            -- system name
  code TEXT COLLATE NOCASE UNIQUE NOT NULL       -- system code in lowercase
);

CREATE TABLE media(
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,                            -- media name
  system_id INTEGER NOT NULL,                    -- system the media belongs to
  FOREIGN KEY(system_id) REFERENCES system(id),
  UNIQUE(name, system_id)
);

CREATE TABLE file(
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,                            -- filename
  data BLOB,                                     -- file data
  size INTEGER NOT NULL,                         -- original file size before compression or patch
  compression TEXT,                              -- compression algorithm of the file
  media_id INTEGER NOT NULL,                     -- media
  parent_id INTEGER,                             -- file to use as input for VCDIFF patch files
  FOREIGN KEY(media_id) REFERENCES media(id),
  FOREIGN KEY(parent_id) REFERENCES file(id)
);

CREATE TABLE checksum(
  file_id INTEGER,
  name TEXT NOT NULL,                            -- checksum algorithm name
  data TEXT NOT NULL,                            -- checksum of the data blob in lowercast
  FOREIGN KEY(file_id) REFERENCES file(id),
  UNIQUE(file_id, name)
);

CREATE TABLE tag(
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,                            -- tag name
  value TEXT,                                    -- optional tag value
  UNIQUE(name, value)
);

CREATE TABLE mediatag(
  tag_id INTEGER NOT NULL,
  media_id INTEGER NOT NULL,
  FOREIGN KEY(tag_id) REFERENCES tag(id),
  FOREIGN KEY(media_id) REFERENCES media(id),
  UNIQUE(tag_id, media_id)
);

CREATE TABLE filetag(
  tag_id INTEGER NOT NULL,
  file_id INTEGER NOT NULL,
  FOREIGN KEY(tag_id) REFERENCES tag(id),
  FOREIGN KEY(file_id) REFERENCES file(id),
  UNIQUE(tag_id, file_id)
);
```
</details>

<details><summary>The default schema also create an index on the following columns</summary>

```sql
CREATE INDEX media_name_idx ON media(name);
CREATE INDEX media_system_id_idx ON media(system_id);
CREATE INDEX file_name_idx ON file(name);
CREATE INDEX file_media_id_idx ON file(media_id);
CREATE INDEX file_parent_id_idx ON file(parent_id);
CREATE INDEX checksum_file_id_idx ON checksum(file_id);
CREATE INDEX mediatag_tag_id_idx ON mediatag(tag_id);
CREATE INDEX mediatag_media_id_idx ON mediatag(media_id);
CREATE INDEX filetag_tag_id_idx ON filetag(tag_id);
CREATE INDEX filetag_file_id_idx ON filetag(file_id);
```
</details>

# Creating a romdb file

To make it easier to import a collection into a romdb file, the reference implementation implements a simple import feature that reads all information from a number of files:
```
├── files
│   ├── Alex Kidd in Miracle World (UE) [!].sms
│   └── ...
├── filetag
│   ├── !.txt
│   ├── UE.txt
│   └── ...
├── mediatag
│   ├── genre.action.txt
│   ├── year.1986.txt
│   └── ...
├── file.txt
├── media.txt
├── patch.txt
└── system.txt
```

You can import a collection like this:

`romdb -o <romdb filename> -i <path of collection>`

`romdb -o "master system.db" -i "Z:\roms\master system"`

### system.txt
The first file read by the import is the `system.txt` file, which contains 4 lines:
```
master system              <- system code
Sega Master System         <- system name
deflate                    <- compression algorithm
crc32                      <- checksum algorithm
```
This file will import a collection for `master system`, it will compress files using the `deflate` algorithm and it will calculate a `crc32` checksum for all files imported.

Here are the possible compression algorithms:
Compression algorithm    | Description
-------------------------|-------------------------
deflate                  | defalte algorithm used in the original zip archive format
xz                       | xz algorithm used by 7zip
none                     | no compression

Here are the possible checksum algorithms:
Checksum algorithm       | Description
-------------------------|-------------------------
crc32                    | CRC32 algorithm
sha1                     | SHA1 algorithm
sha256                   | SHA2-256 algorithm
sha512                   | SHA2-512 algorithm
none                     | no checksum

### media.txt
The `media.txt` file contains the list of media titles to import:
```
Alex Kidd - The Lost Stars
Alex Kidd BMX Trial
Alex Kidd in High Tech World
Alex Kidd in Miracle World
Alex Kidd in Shinobi World
```

### file.txt
The `file.txt` file contains the list of files to import:
```
Alex Kidd - The Lost Stars (UE) [!].sms
Alex Kidd - The Lost Stars (UE) [b1].sms
Alex Kidd BMX Trial (UE) [!].sms
Alex Kidd in High Tech World (UE) [!].sms
Alex Kidd in High Tech World (UE) [T+Ger1.00_Star-trans].sms
Alex Kidd in Miracle World (Brazil) [p1][!].sms
Alex Kidd in Miracle World (J) [!].sms
Alex Kidd in Miracle World (UE) [!].sms
Alex Kidd in Miracle World (UE) [T+Fre].sms
Alex Kidd in Miracle World (UE) [T+Ita1.0].sms
Alex Kidd in Miracle World (UE) [T+Por_CBT].sms
Alex Kidd in Miracle World (UE) [T-Por].sms
Alex Kidd in Shinobi World (UE) [!].sms
```
Files are associated to media by matching the start of its name to existing media. If no media is found that matches the filename, the file won't be imported. The actual files to import should be inside the `files` folder.

### patch.txt (optional)
The `patch.txt` file contains the list of files to import as a VCDIFF patch of an already imported file:
```
Alex Kidd - The Lost Stars (UE) [!].sms
Alex Kidd - The Lost Stars (UE) [b1].sms

Alex Kidd in High Tech World (UE) [!].sms
Alex Kidd in High Tech World (UE) [T+Ger1.00_Star-trans].sms

Alex Kidd in Miracle World (UE) [!].sms
Alex Kidd in Miracle World (Brazil) [p1][!].sms
Alex Kidd in Miracle World (J) [!].sms
Alex Kidd in Miracle World (UE) [T+Fre].sms
Alex Kidd in Miracle World (UE) [T+Ita1.0].sms
Alex Kidd in Miracle World (UE) [T+Por_CBT].sms
Alex Kidd in Miracle World (UE) [T-Por].sms
```
The first file is the input file to use to generate the output file using the patch. In this example, `Alex Kidd in Miracle World (UE) [!].sms` will be used as the base file for all files below it and the romdb will store a VCDIFF patch to reconstruct the file from it. Blank lines separate different patch groups. chained patching is possible (file 1 -> patch 2 -> patch 3 = file 3).

If no patch file is provided, all files will be imported as they are.

### mediatag (optional)
The `mediatag` folder contains a list of files. Each file is the name of the tag and contains a list of media to apply the tag to. tags can be set as `<tag>.txt` or as `<tag>.<value>.txt`.
```
genre.action.txt
year.1986.txt
```

Contents of `year.1986.txt`
```
Alex Kidd in Miracle World
```

### filetag (optional)
The `filetag` folder contains a list of files. Each file is the name of the tag and contains a list of files to apply the tag to. tags can be set as `<tag>.txt` or as `<tag>.<value>.txt`.
```
!.txt
UE.txt
```

Contents of `!.txt`
```
Alex Kidd in High Tech World (UE) [!].sms
Alex Kidd in Miracle World (UE) [!].sms
```

## Creating a romdb 7zip archive
Another option to create a romdb is to import a collection of 7zip archives.

To import arvhives you only need a `system.txt` file, which contains 4 lines:
```
master system              <- system code
Sega Master System         <- system name
archive                    <- import as archives
none                       <- checksum algorithm
```
A media entry will be created for each archive and the top level files in each archive will be added to the `file` table linked to its parent archive.

# Size of romdb

The main goal of romdb is not to minimize the size of the collection, but to have a whole system in one file that's fast and easy to get information from.

### Master System (790 files)
format                            | Size
----------------------------------|-------------------------
folder with uncompressed files    | 172 MB
folder with individual zip files  | 85 MB
7zip compressed archive           | 40 MB
romdb (defalte + patch)           | 52 MB
romdb (xz + patch)                | 46 MB
romdb (7zip archives)             | 40 MB

### Master System (790 files) + Game Gear (532 files)
format                              | Size
------------------------------------|-------------------------
folder with uncompressed files      | 172 MB + 166 MB = 338 MB
folder with individual zip files    | 85 MB + 87 MB = 172 MB
7zip compressed archives (2)        | 40 MB + 52 MB = 92 MB
romdb (2 systems - defalte + patch) | 105 MB
romdb (2 systems - xz + patch)      | 94 MB

# Format conventions

although not a part of the romdb format, romdb files and implementations can and should assume the following conventions:

* system codes should be in lowercase and use the name used in the USA, in case of different names in different regions. ex: `genesis`
* implementations should query systems by `code` and should assume the above naming convention was followed.
* romdb files with multiple systems should have different names between all files in all systems.
* the `name` column of the `file` table should be a valid system name in all major OSes and have an extension related to the file type.
* implementations should query column names and not use `SELECT * FROM table` queries to ensure compatibility with romdb files with extra columns that may be used by some implementations.
* implementations should follow the following conventions to find common metadata:
  * implementations should display file metadata and fallback to media metadata if no file metadata exists
  * images related to media or file should be stored in the `png` image format
  * the media screenshot should be a file named `<media name>.png`
  * the media cover should be a file named `<media name>.cover.png`
  * the file screenshot should be a file named `<file name>.png`
  * the file cover should be a file named `<file name>.cover.png`
  * the `description` tag should contain the description of the media or file

There are a few extra conventions that can be assumed. for systems that support cheat codes (Genesis Game Genie codes, for example), they can assume that if the file has a cheat code table, it's named `<file name>.<cheat table extension>`. ex: `Sonic the Hedgehog (UE) [!].pat`.
