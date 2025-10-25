WITH 
b_agg_inout AS (
    SELECT 
        b.betucode, b.kjyear, b.period, c.sMod,
        SUM(b.money) AS total_money
    FROM Cw_InOut b
    INNER JOIN Gy_IOClass c ON b.IOClassCode = c.IOClassCode
    WHERE b.kjyear = 2025 AND b.period = 10
    GROUP BY b.betucode, b.kjyear, b.period, c.sMod
),
b_agg_iof AS (
    SELECT 
        c.betucode, c.kjyear, c.period, d.sMod,
        SUM(c.money) AS total_money
    FROM Cw_IOF c
    INNER JOIN Gy_IOClass d ON c.IOClassCode = d.IOClassCode
    WHERE c.kjyear = 2025 AND c.period = 10
    GROUP BY c.betucode, c.kjyear, c.period, d.sMod
),
all_keys AS (
    SELECT a.betucode, a.kjyear, a.period  FROM Cw_Account a 
    INNER JOIN Gy_Betunit g ON a.betucode = g.betucode AND g.BetuSort = '04'
    WHERE a.kjyear = 2025 AND a.period = 10
    UNION
    SELECT b.betucode, b.kjyear, b.period  FROM Cw_InOut b 
    INNER JOIN Gy_Betunit g ON b.betucode = g.betucode AND g.BetuSort = '04'
    WHERE b.kjyear = 2025 AND b.period = 10
    UNION
    SELECT c.betucode, c.kjyear, c.period  FROM Cw_IOF c 
    INNER JOIN Gy_Betunit g ON c.betucode = g.betucode AND g.BetuSort = '04'
    WHERE c.kjyear = 2025 AND c.period = 10
),
b_pivot_inout AS (
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
        SELECT betucode, kjyear, period, sMod, total_money FROM b_agg_inout
    ) src
    PIVOT
    (
        SUM(total_money)
        FOR sMod IN (YF, YS, FK, SK, FY, OM)
    ) pvt
),
b_pivot_iof AS (
    SELECT
        betucode,
        kjyear,
        period,
        ISNULL(YF, 0) AS YF_IOF,
        ISNULL(YS, 0) AS YS_IOF,
        ISNULL(FK, 0) AS FK_IOF,
        ISNULL(SK, 0) AS SK_IOF,
        ISNULL(FY, 0) AS FY_IOF,
        ISNULL(OM, 0) AS OM_IOF
    FROM (
        SELECT betucode, kjyear, period, sMod, total_money FROM b_agg_iof
    ) src
    PIVOT
    (
        SUM(total_money)
        FOR sMod IN (YF, YS, FK, SK, FY, OM)
    ) pvt
)
SELECT
    k.betucode,
    k.kjyear,
    k.period,
    ISNULL(a.StartMoney, 0) AS sMon,
    -- Cw_InOut 的 sMod 列
    ISNULL(bp_inout.YF, 0) AS YF_InOut,
    ISNULL(bp_inout.YS, 0) AS YS_InOut,
    ISNULL(bp_inout.FK, 0) AS FK_InOut,
    ISNULL(bp_inout.SK, 0) AS SK_InOut,
    ISNULL(bp_inout.FY, 0) AS FY_InOut,
    ISNULL(bp_inout.OM, 0) AS OM_InOut,
    -- Cw_IOF 的 sMod 列
    ISNULL(bp_iof.YF_IOF, 0) AS YF_IOF,
    ISNULL(bp_iof.YS_IOF, 0) AS YS_IOF,
    ISNULL(bp_iof.FK_IOF, 0) AS FK_IOF,
    ISNULL(bp_iof.SK_IOF, 0) AS SK_IOF,
    ISNULL(bp_iof.FY_IOF, 0) AS FY_IOF,
    ISNULL(bp_iof.OM_IOF, 0) AS OM_IOF
FROM all_keys k
LEFT JOIN Cw_Account a ON a.betucode = k.betucode AND a.kjyear = k.kjyear AND a.period = k.period
LEFT JOIN b_pivot_inout bp_inout ON bp_inout.betucode = k.betucode AND bp_inout.kjyear = k.kjyear AND bp_inout.period = k.period
LEFT JOIN b_pivot_iof bp_iof ON bp_iof.betucode = k.betucode AND bp_iof.kjyear = k.kjyear AND bp_iof.period = k.period
ORDER BY k.betucode, k.kjyear, k.period;
