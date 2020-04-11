Example import files for nintendo 64 as 7z archives

romdb will use each file name as the media name and read the top level files in
each archive and add them to the file table linked to its parent archive.

this should be the most eficient storage format.

create a folder named "files" and put all 7z files in it and run:

  romdb -o "master system.db" -i "examples\nintendo 64 archive"

Use a different files folder:

  romdb -o "master system.db" -i "examples\nintendo 64 archive" -r "Z:\roms"
