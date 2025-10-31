ALTER PROCEDURE [dbo].[KF_SP_MIOS_CHK] 
    @MID    INT,
    @bAdd   BIT,
    @CzyID  SMALLINT,
    @RMsg   CHAR(100) OUT
AS
BEGIN
    SET NOCOUNT ON;

    BEGIN TRY
        -- 1. 将数据先放入临时表，替代游标遍历
        IF OBJECT_ID('tempdb..#MIOList') IS NOT NULL DROP TABLE #MIOList;

        CREATE TABLE #MIOList (
            SID SMALLINT,    
            BillCode CHAR(10),   
            StID SMALLINT,
            Mnum CHAR(26),   
            TQuan DECIMAL(18,6), 
            CQuan DECIMAL(18,6),
            sWHC CHAR(10),
            dWHC CHAR(10),
            OrdID INT,
            MKgM DECIMAL(18,2),
            Made CHAR(16),
            MColor CHAR(16),
            MPrj CHAR(16),
            Kjyear INT,
            Period SMALLINT,
            MLen DECIMAL(18,2),
            HH CHAR(30),
            BillDate DATE,
            BillNum CHAR(20),
            InvSortCode CHAR(12),
            MType SMALLINT,
            IONum CHAR(20),
            IOClassCode CHAR(10),
            IPrice DECIMAL(18,6)
        );

        INSERT INTO #MIOList
        SELECT SID,BillCode,StID,Mnum,TQuan,CQuan,sWhcode,dWhcode,OrdID,MkgM,Made,MColor,MPrj,Kjyear,[Period],
               MLen,HH,BillDate,BillNum,InvSortCode,MType,IONum,IOClassCode,ISNULL(IPrice,0)
        FROM Gy_V_MIOList 
        WHERE InvSortCode NOT LIKE '009%' AND MID = @MID
        ORDER BY SID;

        -- 2. 删除相关 Gy_BillJD 记录
        DELETE BJ
        FROM Gy_BillJD BJ
        INNER JOIN Gy_MIOS M ON BJ.OrdID = M.OrdID AND BJ.StID = M.StID
        INNER JOIN #MIOList ML ON M.MID = @MID AND BJ.BillCode = ML.BillCode;

        -- 3. 批量处理 #MIOList 中数据
        -- 先调整数量方向
        UPDATE #MIOList
        SET TQuan = CASE WHEN @bAdd = 0 THEN 0 - TQuan ELSE TQuan END,
            CQuan = CASE WHEN @bAdd = 0 THEN 0 - CQuan ELSE CQuan END,
            MKgM = CASE WHEN @bAdd = 0 THEN 0 - MKgM ELSE MKgM END;

        -- 4. 计算汇总数量，插入 Gy_BillJD
        ;WITH QtySummary AS (
            SELECT OrdID, BillCode, StID,
                   SUM(TQuan) AS SumTQuan,
                   SUM(CQuan) AS SumCQuan,
                   SUM(MKgM) AS SumMKgM
            FROM Gy_MIOS A
            INNER JOIN Gy_MIOM B ON A.MID = B.MID
            WHERE A.OrdID IN (SELECT OrdID FROM #MIOList)
              AND A.StID IN (SELECT StID FROM #MIOList)
              AND B.BillCode IN (SELECT BillCode FROM #MIOList)
              AND A.MID <> @MID * (1 - @bAdd)
            GROUP BY OrdID, BillCode, StID
        )
        MERGE Gy_BillJD AS target
        USING QtySummary AS source
        ON target.OrdID = source.OrdID AND target.BillCode = source.BillCode AND target.StID = source.StID
        WHEN MATCHED THEN
            UPDATE SET TQuan = source.SumTQuan, CQuan = source.SumCQuan, MkgM = source.SumMKgM, iJd = 1
        WHEN NOT MATCHED THEN
            INSERT (OrdID, BillCode, StID, TQuan, CQuan, MkgM, iJd)
            VALUES (source.OrdID, source.BillCode, source.StID, source.SumTQuan, source.SumCQuan, source.SumMKgM, 1);

        -- 5. 遍历 #MIOList 逐条处理复杂业务逻辑（仍需循环，但无游标）
        DECLARE @RowCount INT = (SELECT COUNT(*) FROM #MIOList);
        DECLARE @Index INT = 1;

        WHILE @Index <= @RowCount
        BEGIN
            DECLARE @SID SMALLINT, @BCode CHAR(10), @StID SMALLINT, @Mnum CHAR(26), @TQuan DECIMAL(18,6), @CQuan DECIMAL(18,6),
                    @sWHC CHAR(10), @dWHC CHAR(10), @OrdID INT, @MKgM DECIMAL(18,2), @Made CHAR(16), @MCol CHAR(16), @MPrj CHAR(16),
                    @Kjyear INT, @MM SMALLINT, @MLen DECIMAL(18,2), @HH CHAR(30), @BDate DATE, @BNum CHAR(20), @Inv CHAR(12),
                    @MType SMALLINT, @ION CHAR(20), @IOC CHAR(10), @iPric DECIMAL(18,6);

            SELECT @SID=SID, @BCode=BillCode, @StID=StID, @Mnum=Mnum, @TQuan=TQuan, @CQuan=CQuan, @sWHC=sWHC, @dWHC=dWHC,
                   @OrdID=OrdID, @MKgM=MKgM, @Made=Made, @MCol=MColor, @MPrj=MPrj, @Kjyear=Kjyear, @MM=Period, @MLen=MLen,
                   @HH=HH, @BDate=BillDate, @BNum=BillNum, @Inv=InvSortCode, @MType=MType, @ION=IONum, @IOC=IOClassCode, @iPric=IPrice
            FROM (
                SELECT ROW_NUMBER() OVER (ORDER BY SID) AS rn, * FROM #MIOList
            ) AS T WHERE rn = @Index;

            -- 这里继续写原游标内的业务逻辑，注意用集合操作替代多处 EXISTS+UPDATE/INSERT，示例：

            -- 示例：MERGE KF_MMKJQuan 更新或插入
            MERGE KF_MMKJQuan AS target
            USING (SELECT @Mnum AS Mnum, @Made AS Made, @MCol AS MColor, @MPrj AS MPrj, @MLen AS MLen, @Kjyear AS Kjyear, @MM AS Period, @dWHC AS WhCode) AS source
            ON target.Mnum = source.Mnum AND target.Made = source.Made AND target.MColor = source.MColor AND target.MPrj = source.MPrj
               AND target.MLen = source.MLen AND target.Kjyear = source.Kjyear AND target.Period = source.Period AND target.WhCode = source.WhCode
            WHEN MATCHED THEN
                UPDATE SET NIQuanT = NIQuanT + @TQuan, NIQuanC = NIQuanC + @CQuan, MkgM = MkgM + @MKgM
            WHEN NOT MATCHED THEN
                INSERT (Mnum, WhCode, Made, MColor, MPrj, MLen, MkgM, Kjyear, Period, NIQuanT, NIQuanC, IPrice)
                VALUES (@Mnum, @dWHC, @Made, @MCol, @MPrj, @MLen, @MKgM, @Kjyear, @MM, @TQuan, @CQuan, @iPric);

            -- 其他业务逻辑依此类推，建议拆分成独立存储过程或函数调用

            SET @Index = @Index + 1;
        END

        SET @RMsg = 'OK';
    END TRY
    BEGIN CATCH
        SET @RMsg = ERROR_MESSAGE();
    END CATCH
END
