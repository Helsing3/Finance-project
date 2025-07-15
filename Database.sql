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

/* -- usful prompts to show information after ./sqlite3.exe holdings.db
SELECT
  RANK() OVER (ORDER BY SUM(h.value) DESC) AS rank,
  h.cusip,
  MIN(h.name_of_issuer) AS issuer_name,
  SUM(h.shares) AS total_shares,
  SUM(h.value) AS total_value
FROM holdings h
GROUP BY h.cusip
ORDER BY total_value DESC;
WHERE LOWER(h.name_of_issuer) LIKE '%ast space%' // optional
*/
/*
SELECT h.*
FROM holdings h
JOIN filings f ON h.filing_id = f.id
JOIN firms fi ON f.firm_id = fi.id
WHERE fi.name = 'TRAN CAPITAL MANAGEMENT, L.P.';
*/

-- Optional for performance
CREATE INDEX idx_filings_firm_quarter ON filings(firm_id, quarter);
CREATE INDEX idx_holdings_filing_cusip ON holdings(filing_id, cusip);
