CREATE TABLE firms (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE
);

CREATE TABLE filings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    firm_id INTEGER,
    filing_date TEXT,
    quarter TEXT,
    FOREIGN KEY(firm_id) REFERENCES firms(id)
);

CREATE TABLE holdings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filing_id INTEGER,
    cusip TEXT,
    name_of_issuer TEXT,
    shares INTEGER,
    value INTEGER,
    UNIQUE(filing_id, cusip),
    FOREIGN KEY(filing_id) REFERENCES filings(id)
);
