CREATE TABLE firms (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE
);

CREATE TABLE filings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    firm_id INTEGER,
    filing_date TEXT,
    quarter TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(firm_id) REFERENCES firms(id)
);

CREATE TABLE holdings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filing_id INTEGER,
    cusip TEXT,
    name_of_issuer TEXT,
    shares INTEGER,
    value INTEGER,
    put_call TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(filing_id, cusip),
    FOREIGN KEY(filing_id) REFERENCES filings(id)
);

CREATE VIEW quarter_firm_stats AS
SELECT
    f.quarter,
    fi.name AS firm_name,
    SUM(h.value) AS total_value,
    SUM(h.shares) AS total_shares,
    COUNT(*) AS num_holdings
FROM holdings h
JOIN filings f ON h.filing_id = f.id
JOIN firms fi ON f.firm_id = fi.id
GROUP BY f.quarter, fi.name;

-- Optional for performance
CREATE INDEX idx_filings_firm_quarter ON filings(firm_id, quarter);
CREATE INDEX idx_holdings_filing_cusip ON holdings(filing_id, cusip);
CREATE INDEX idx_holdings_cusip ON holdings(cusip); -- for fast issuer-wide queries

/* -- usful prompts to show information after ./sqlite3.exe holdings.db
SELECT
  MIN(h.name_of_issuer) AS issuer_name,
  SUM(h.shares) AS total_shares,
  SUM(h.value) AS total_value
FROM holdings h
WHERE LOWER(h.name_of_issuer) LIKE '%ast space%';


SELECT
  RANK() OVER (ORDER BY SUM(h.value) DESC) AS rank,
  h.name_of_issuer AS issuer_name,
  SUM(h.shares) AS total_shares,
  SUM(h.value) AS total_value
FROM holdings h
GROUP BY h.name_of_issuer
ORDER BY total_value DESC;


*/
/*
SELECT h.*
FROM holdings h
JOIN filings f ON h.filing_id = f.id
JOIN firms fi ON f.firm_id = fi.id
WHERE fi.name = 'TRAN CAPITAL MANAGEMENT, L.P.';
*/

/* try this when you have downloaded data from multiple quarters.
-- Compare how many shares of AAPL a firm held in Q2 vs Q3
SELECT
    h.name_of_issuer,
    SUM(CASE WHEN f.quarter = '2025Q1' THEN h.value ELSE 0 END) AS q1_holdings,
    SUM(CASE WHEN f.quarter = '2025Q2' THEN h.value ELSE 0 END) AS q2_holdings
FROM
    holdings h
JOIN
    filings f ON h.filing_id = f.id
WHERE
    f.quarter IN ('2025Q1', '2025Q2')
GROUP BY
    h.name_of_issuer
ORDER BY
    q2_holdings DESC;


*/