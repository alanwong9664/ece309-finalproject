---
name: csv-report
description: Turn a CSV of sales rows into a formatted summary report.
requires: [read-csv, table-format, number-format]
---
# CSV report
1. Load the rows (read-csv).
2. Total the amount column per region.
3. Show the totals as a table (table-format), largest first.
4. End with one sentence naming the best region.
