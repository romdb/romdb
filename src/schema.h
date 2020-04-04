#pragma once

#include <string>

const std::string defaultSchema{ R"(
CREATE TABLE system(
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  code TEXT COLLATE NOCASE UNIQUE NOT NULL
);

CREATE TABLE media(
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  system_id INTEGER NOT NULL,
  FOREIGN KEY(system_id) REFERENCES system(id),
  UNIQUE(name, system_id)
);

CREATE INDEX media_name_idx ON media(name);
CREATE INDEX media_system_id_idx ON media(system_id);

CREATE TABLE file(
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  data BLOB,
  size INTEGER NOT NULL,
  compression TEXT,
  media_id INTEGER NOT NULL,
  parent_id INTEGER,
  FOREIGN KEY(media_id) REFERENCES media(id),
  FOREIGN KEY(parent_id) REFERENCES file(id)
);

CREATE INDEX file_name_idx ON file(name);
CREATE INDEX file_media_id_idx ON file(media_id);
CREATE INDEX file_parent_id_idx ON file(parent_id);

CREATE TABLE checksum(
  file_id INTEGER,
  name TEXT NOT NULL,
  data TEXT NOT NULL,
  FOREIGN KEY(file_id) REFERENCES file(id),
  UNIQUE(file_id, name)
);

CREATE INDEX checksum_file_id_idx ON checksum(file_id);

CREATE TABLE tag(
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  value TEXT,
  UNIQUE(name, value)
);

CREATE TABLE mediatag(
  tag_id INTEGER NOT NULL,
  media_id INTEGER NOT NULL,
  FOREIGN KEY(tag_id) REFERENCES tag(id),
  FOREIGN KEY(media_id) REFERENCES media(id),
  UNIQUE(tag_id, media_id)
);

CREATE INDEX mediatag_tag_id_idx ON mediatag(tag_id);
CREATE INDEX mediatag_media_id_idx ON mediatag(media_id);

CREATE TABLE filetag(
  tag_id INTEGER NOT NULL,
  file_id INTEGER NOT NULL,
  FOREIGN KEY(tag_id) REFERENCES tag(id),
  FOREIGN KEY(file_id) REFERENCES file(id),
  UNIQUE(tag_id, file_id)
);

CREATE INDEX filetag_tag_id_idx ON filetag(tag_id);
CREATE INDEX filetag_file_id_idx ON filetag(file_id);
)" };
