WITH b_agg AS (
        SELECT 
            b.betucode,  b.kjyear,  b.period,  c.sMod,
            SUM(b.money) AS total_money
        FROM Cw_InOut b
        INNER JOIN Gy_IOClass c ON b.IOClassCode = c.IOClassCode
        WHERE b.kjyear = 2025 AND b.period = 10
        GROUP BY b.betucode, b.kjyear, b.period, c.sMod
    ),
    all_keys AS (
        SELECT a.betucode, kjyear, [period] FROM Cw_Account a INNER JOIN Gy_Betunit g ON a.betucode = g.betucode AND g.BetuSort = '04'
		WHERE kjyear = 2025 AND [period] = 10
        UNION
        SELECT b.betucode, kjyear, [period] FROM Cw_InOut b INNER JOIN Gy_Betunit g ON b.betucode = g.betucode AND g.BetuSort = '04'
		WHERE kjyear = 2025 AND [period] = 10
        UNION
        SELECT c.betucode, kjyear, [period] FROM Cw_IOF c INNER JOIN Gy_Betunit g ON c.betucode = g.betucode AND g.BetuSort = '04'
		WHERE kjyear = 2025 AND [period] = 10

    ),
    b_pivot AS (
        SELECT
            betucode,
            kjyear,
            period,
            ISNULL(YF, 0) AS YF,
            ISNULL(YS, 0) AS YS,
            ISNULL(FK, 0) AS FK,
            ISNULL(SK, 0) AS SK,
            ISNULL(FY, 0) AS FY,
            ISNULL(OM, 0) AS OM
        FROM (
            SELECT betucode, kjyear, period, SMod, total_money FROM b_agg
        ) src
        PIVOT
        (
            SUM(total_money)
            FOR sMod IN (YF,YS,FK,SK,FY,OM)
        ) pvt
    )
    SELECT
        k.betucode,
        k.kjyear,
        k.period,
        ISNULL(a.StartMoney, 0) AS sMon,
        ISNULL(bp.YF, 0) AS YF,
        ISNULL(bp.YS, 0) AS YS,
        ISNULL(bp.FK, 0) AS FK,
        ISNULL(bp.SK, 0) AS SK,
        ISNULL(bp.FY, 0) AS FY,
        ISNULL(bp.OM, 0) AS OM
    FROM all_keys k
    LEFT JOIN Cw_Account a ON a.betucode = k.betucode AND a.kjyear = k.kjyear AND a.period = k.period
    LEFT JOIN b_pivot bp ON bp.betucode = k.betucode AND bp.kjyear = k.kjyear AND bp.period = k.period