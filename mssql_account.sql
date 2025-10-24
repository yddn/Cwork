SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO

CREATE OR ALTER FUNCTION dbo.fn_BetU_MonthlyReport
(
    @kjyear INT,
    @period INT
)
RETURNS TABLE
AS
RETURN
(
    WITH b_agg AS (
        SELECT 
            b.betucode,
            b.kjyear,
            b.period,
            c.IOTYPE,
            SUM(b.money) AS total_money
        FROM b
        INNER JOIN c ON b.IOC = c.IOC
        WHERE b.kjyear = @kjyear AND b.period = @period
        GROUP BY b.betucode, b.kjyear, b.period, c.IOTYPE
    ),
    -- 获取所有 betucode,kjyear,period 的组合，来自 a 和 b
    all_keys AS (
        SELECT betucode, kjyear, period FROM a WHERE kjyear = @kjyear AND period = @period
        UNION
        SELECT betucode, kjyear, period FROM b WHERE kjyear = @kjyear AND period = @period
    ),
    -- 汇总 b 表数据行转列
    b_pivot AS (
        SELECT
            betucode,
            kjyear,
            period,
            ISNULL([IN], 0) AS IN_Money,
            ISNULL([OUT], 0) AS OUT_Money,
            ISNULL([TRANSFER], 0) AS TRANSFER_Money
            -- 这里假设 c.IOTYPE 只有 IN, OUT, TRANSFER 三种类型，若更多需补充
        FROM (
            SELECT betucode, kjyear, period, IOTYPE, total_money FROM b_agg
        ) src
        PIVOT
        (
            SUM(total_money)
            FOR IOTYPE IN ([IN], [OUT], [TRANSFER])
        ) pvt
    )
    SELECT
        k.betucode,
        k.kjyear,
        k.period,
        ISNULL(a.sMon, 0) AS sMon,
        ISNULL(bp.IN_Money, 0) AS IN_Money,
        ISNULL(bp.OUT_Money, 0) AS OUT_Money,
        ISNULL(bp.TRANSFER_Money, 0) AS TRANSFER_Money
    FROM all_keys k
    LEFT JOIN a ON a.betucode = k.betucode AND a.kjyear = k.kjyear AND a.period = k.period
    LEFT JOIN b_pivot bp ON bp.betucode = k.betucode AND bp.kjyear = k.kjyear AND bp.period = k.period
)
GO


SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO

CREATE OR ALTER FUNCTION dbo.fn_BetU_MonthlyReport
(
    @kjyear INT,
    @period INT
)
RETURNS TABLE
AS
RETURN
(
    WITH b_agg AS (
        SELECT 
            b.betucode,
            b.kjyear,
            b.period,
            c.IOTYPE,
            SUM(b.money) AS total_money
        FROM b
        INNER JOIN c ON b.IOC = c.IOC
        WHERE b.kjyear = @kjyear AND b.period = @period
        GROUP BY b.betucode, b.kjyear, b.period, c.IOTYPE
    ),
    all_keys AS (
        SELECT betucode, kjyear, period FROM a WHERE kjyear = @kjyear AND period = @period
        UNION
        SELECT betucode, kjyear, period FROM b WHERE kjyear = @kjyear AND period = @period
    ),
    b_pivot AS (
        SELECT
            betucode,
            kjyear,
            period,
            ISNULL([IN], 0) AS IN_Money,
            ISNULL([OUT], 0) AS OUT_Money,
            ISNULL([TRANSFER], 0) AS TRANSFER_Money,
            ISNULL([ADJUST], 0) AS ADJUST_Money,
            ISNULL([FEE], 0) AS FEE_Money,
            ISNULL([OTHER], 0) AS OTHER_Money
        FROM (
            SELECT betucode, kjyear, period, IOTYPE, total_money FROM b_agg
        ) src
        PIVOT
        (
            SUM(total_money)
            FOR IOTYPE IN ([IN], [OUT], [TRANSFER], [ADJUST], [FEE], [OTHER])
        ) pvt
    )
    SELECT
        k.betucode,
        k.kjyear,
        k.period,
        ISNULL(a.sMon, 0) AS sMon,
        ISNULL(bp.IN_Money, 0) AS IN_Money,
        ISNULL(bp.OUT_Money, 0) AS OUT_Money,
        ISNULL(bp.TRANSFER_Money, 0) AS TRANSFER_Money,
        ISNULL(bp.ADJUST_Money, 0) AS ADJUST_Money,
        ISNULL(bp.FEE_Money, 0) AS FEE_Money,
        ISNULL(bp.OTHER_Money, 0) AS OTHER_Money
    FROM all_keys k
    LEFT JOIN a ON a.betucode = k.betucode AND a.kjyear = k.kjyear AND a.period = k.period
    LEFT JOIN b_pivot bp ON bp.betucode = k.betucode AND bp.kjyear = k.kjyear AND bp.period = k.period
)
GO
