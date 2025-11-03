use lhtest
go

update Cw_Account set fv10=0  where KjYear=2025 and Period=10 and BetUCode  IN (SELECT BetUCode FROM Gy_BetUnit WHERE BetUSort = '04');
update A
	set A.fv10 = P.tMon		--fv10 其它费用  -fv9 付款
FROM  Cw_Account A
INNER JOIN (
SELECT 
            a.BetUCode,
            sum(a.money) as tMon
        FROM Cw_InOut a 
        INNER JOIN Gy_IOClass b ON a.IOClassCode = b.IOClassCode
        WHERE a.BetUCode IN (SELECT BetUCode FROM Gy_BetUnit WHERE BetUSort = '04')
          AND a.kjyear = 2025
          AND a.period = 10
          AND b.sMod = 'FY'  -- 只汇总 FY
		Group by a.BetUCode
		) P ON A.betuCode = P.BetUCode
where A.KjYear=2025 and a.Period=10 and a.BetUCode  IN (SELECT BetUCode FROM Gy_BetUnit WHERE BetUSort = '04')

update Cw_Account set EndMoney = StartMoney + FV8 + FV9 + FV10 where KjYear=2025 and Period=10 and BetUCode  IN (SELECT BetUCode FROM Gy_BetUnit WHERE BetUSort = '04');
--UPDATE Cw_Account SET StartMoney = 0  where KjYear=2025 and Period=11 and BetUCode  IN (SELECT BetUCode FROM Gy_BetUnit WHERE BetUSort = '04');

/*UPDATE A  SET A.StartMoney = (B.StartMoney + B.FV8 + B.FV9 + B.FV10) FROM Cw_Account A 
INNER JOIN Cw_Account B ON A.BetuCode = B.BetuCode AND A.KjYear = B.Kjyear AND B.Period = 10
Where A.Period = 11  and A.BetUCode  IN (SELECT BetUCode FROM Gy_BetUnit WHERE BetUSort = '04');;*/

MERGE INTO Cw_Account AS Target
USING
(
    SELECT 
        B.BetUCode,
        B.KjYear,
        11 AS Period,
        (B.StartMoney + B.FV8 + B.FV9 + B.FV10) AS NewStartMoney
    FROM Cw_Account B
    WHERE B.Period = 10
      AND B.BetUCode IN (SELECT BetUCode FROM Gy_BetUnit WHERE BetUSort = '04')
) AS Source
ON Target.BetUCode = Source.BetUCode
   AND Target.KjYear = Source.KjYear
   AND Target.Period = Source.Period
WHEN MATCHED THEN
    UPDATE SET Target.StartMoney = Source.NewStartMoney
WHEN NOT MATCHED THEN
    INSERT (BetUCode, KjYear, Period, StartMoney /*, 其他必填字段*/)
    VALUES (Source.BetUCode, Source.KjYear, Source.Period, Source.NewStartMoney);
