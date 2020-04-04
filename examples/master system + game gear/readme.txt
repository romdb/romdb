Example import files for 2 systems (master system + game gear (790 + 532 files))

This example shows how to import 2 systems and use patches between files of different systems.

create a folder named "files" and put all files for both systems in it and run:

  romdb -o "master system + game gear.db" -i "examples\master system + game gear"

Use xz for compression:

  romdb -o "master system + game gear.db" -i "examples\master system + game gear" -c xz

Use a different files folder:

  romdb -o "master system + game gear.db" -i "examples\master system + game gear" -r "Z:\roms"
