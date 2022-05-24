CREATE TABLE note (
    note_id VARCHAR(64) NOT NULL PRIMARY KEY,
    note_meta LONGTEXT,
    blob_content LONGBLOB
) DEFAULT CHARSET=utf8;
