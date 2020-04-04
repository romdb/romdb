Example import files for master system (790 files)

create a folder named "files" and put all files in it and run:

  romdb -o "master system.db" -i "examples\master system"

Use xz for compression:

  romdb -o "master system.db" -i "examples\master system" -c xz

Use a different files folder:

  romdb -o "master system.db" -i "examples\master system" -r "Z:\roms"
