PRAGMA foreign_keys=ON;
CREATE TABLE packages (
    id INTEGER PRIMARY KEY,

    name TEXT NOT NULL,
    version TEXT NOT NULL,

    base_name TEXT,
    description TEXT,
    url TEXT,
    arch TEXT,

    build_date INTEGER,
    install_date INTEGER,

    license TEXT,
    validation TEXT,

    UNIQUE(name, version)
);
CREATE TABLE dependencies (
    id INTEGER PRIMARY KEY,

    package_id INTEGER NOT NULL,
    dependency_name TEXT NOT NULL,
    dependency_version TEXT,
    is_optional INTEGER DEFAULT 0,

    FOREIGN KEY (package_id)
        REFERENCES packages(id)
        ON DELETE CASCADE
);
CREATE TABLE files (
    id INTEGER PRIMARY KEY,

    package_id INTEGER NOT NULL,
    path TEXT NOT NULL,

    type TEXT,
    mode INTEGER,
    mtime INTEGER,
    sha256 TEXT,

    is_backup INTEGER DEFAULT 0,
    checksum TEXT,

    FOREIGN KEY (package_id)
        REFERENCES packages(id)
        ON DELETE CASCADE,

    UNIQUE(package_id, path)
);

CREATE INDEX idx_packages_name_version ON packages(name, version);
CREATE INDEX idx_dependencies_package ON dependencies(package_id);
CREATE INDEX idx_dependencies_name ON dependencies(dependency_name);
CREATE INDEX idx_files_package ON files(package_id);
CREATE INDEX idx_files_path ON files(path);
