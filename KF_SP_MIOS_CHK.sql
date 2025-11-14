USE [AnyData_LH_2015]
GO
/****** Object:  StoredProcedure [dbo].[KF_SP_MIOS_CHK]    Script Date: 2025/11/14 23:43:24 ******/
SET ANSI_NULLS ON
GO
SET QUOTED_IDENTIFIER ON
GO
/* =============================================
* Author:		zzi
* Create date: 2020-09-29
* Description:	库存处理

所有凭证根据    BillCode进行出入库及占料等操作
6010_Chking:	pGYIO_ChkKC(iMid, 0, KF_MM_N_IN, iOper) 供应商仓入库，无出库 只影响入库数？
6030_Saveing:	pGYIO_ChkKC(iMid, KF_MM_U_ADD, KF_MM_W_ADD, bAdd) 源出 目标占料 在途
6060_Saveing:	pGYIO_ChkKC(ioMID, KF_MM_U_ADD, KF_MM_W_ADD, Not bAdd)       '源 占料 现存 - 目标在途- 现存+
				pGYIO_ChkKC(iMid, KF_MM_N_OUT, KF_MM_N_IN, bAdd)	货号行不为空写货号
7500_Saveing:	pGYIO_FTKC(iMid, bAdd)					'开平 首出料 它行入库 源目标仓相同	入库行写货号库 实际货号扣减
7510_Saveing:	pGYIO_FTKC(iMid, bAdd)					'分条 首出料 它行入库	入库行写货号库  实际货号扣减
7530_Saveing:	pKCChking
7550_Saveing:	pGYIO_FTKC_T(iMid, bAdd)				'生产 StID=1 入库  =2 出库	入库行写货号库  实际货号扣减
7580_Saveing:	pGYIO_ChkKC(iMid, KF_MM_N_OUT, KF_MM_N_IN, bAdd)		'型材销售出库 去占料	实际货号扣减
7570_Saveing:	pGYIO_ChkKC(iMid, KF_MM_N_OUT, KF_MM_N_IN, bAdd)		'板材销售出库 去占料 	实际货号扣减
8010_Chking:	pGYIO_ChkKC(iMid, KF_MM_U_ADD, 0, iOper)				'销售订单在审核时形成占料
6040_Saveing:   采购收货通知单，整合调拨单+运费单，实现订单至核销之间的整个环节,在保存时，对关联凭证 对分录行同步修改进度
StiD:在8010中为销售类别 0-库存直卖 1-库存待产 2-待购待产 3-待购直卖
mtype:  在8010中 <2 按支  >1 按重 ，在采购系统 中=0计重 =1 计支
1501_Saveing:	其它入库单,如果是委外加工入库,需修改仓库的出入量和占用在途量
-- =============================================*/
ALTER PROCEDURE [dbo].[KF_SP_MIOS_CHK] 
	-- Add the parameters for the stored procedure here
	@MID 	INT,
	@bAdd 	BIT,			--=1 新增 =0 删除 
	@CzyID 	SMALLINT,
	@RMsg	CHAR(100) OUT		--返回参数
AS
BEGIN
	
	SET NOCOUNT ON;
	DECLARE @SID    SMALLINT
	DECLARE @OrdID  INT
	DECLARE @StID   SMALLINT
	DECLARE @Kjyear INT
	DECLARE @MM     SMALLINT
	DECLARE @BCode  CHAR(10)
    DECLARE @Inv    CHAR(12)
	DECLARE @Mnum   CHAR(26)
	DECLARE @Made   CHAR(16)
	DECLARE @MCol   CHAR(16)
	DECLARE @MPrj   CHAR(16)
	DECLARE @sWHC   CHAR(10)
	DECLARE @dWHC   CHAR(10)
	DECLARE @IOC	CHAR(10)		--新增出入类别
	DECLARE @TQuan  DECIMAL(18,6)
	DECLARE @CQuan  DECIMAL(18,6)
	DECLARE @MKgM   DECIMAL(18,2)
	DECLARE @MLen   DECIMAL(18,2)
	DECLARE @HH	    CHAR(30)            --货号，在采购委收时兼做上流程BillCode
	DECLARE @tMz    DECIMAL(18,6)
	DECLARE @iMoP   SMALLINT		--原料或成品标志 新增实际货号中，6060=0 其它=1
	DECLARE @BDate  DATE
	DECLARE @BNum   CHAR(20)
    DECLARE @ION    CHAR(20)       --关联凭证号
    DECLARE @MType  SMALLINT        --<2 计长/支 >=2 计重 在出库中<2判断 判断库存QC=0 THEN QT=0
    DECLARE @dTQ    DECIMAL(18,6)   --库存重量偏差值 
    DECLARE @dCQ    DECIMAL(18,6)
    DECLARE @UMID   INT             --针对收货通知单，分录行关联的进度
    DECLARE @USID   SMALLINT
    DECLARE @CgdR   DECIMAL(18,3)   --采购完成偏差率
    DECLARE @TQ     DECIMAL(18,3)
    DECLARE @CQ     DECIMAL(18,3)
    DECLARE @FQ     DECIMAL(18,3)
    DECLARE @TMK    DECIMAL(18,3)
	DECLARE @SMM	CHAR(26)		--定义委外加工成品对应的原料编码 
	DECLARE @iPric  DECIMAL(18,6)		--2023-05-12 新增开平成本的成本价计算 06-25 电镀成本
	DECLARE @KTQuan	DECIMAL(18,6)			--库存数量(含当前核销数量)
	DECLARE @HPrice DECIMAL(18,3)			--核销单价
	DECLARE @AvgP   DECIMAL(18,6)           --平均价
    --DECLARE @FBCode CHAR(10)        --上流程单据类型
	BEGIN try
		DECLARE RS SCROLL CURSOR FOR 
		SELECT SID,BillCode,StID,Mnum,TQuan,CQuan,sWhcode,dWhcode,OrdID,MkgM,Made,MColor,MPrj,Kjyear,[Period],
            MLen,HH,BillDate,BillNum,InvSortCode,MType,IONum,IOClassCode,ISNULL(IPrice,0)
			FROM Gy_V_MIOList WHERE InvSortCode not like '009%' and MID=@MID ORDER by SID
		
		OPEN RS
		FETCH FIRST FROM RS
		INTO @SID,@BCode,@StID,@Mnum,@TQuan,@CQuan,@sWHC,@dWHC,@OrdID,@MKgM,@Made,@MCol,@MPrj,@Kjyear,
                    @MM,@MLen,@HH,@BDate,@BNum,@Inv,@MType,@ION,@IOC,@iPric
        
        --删除进度表中此凭证包含的物料进度信息  --整表执行
        DELETE Gy_BillJD WHERE OrdID IN (SELECT OrdID FROM Gy_MIOS WHERE MID=@MID) 
                            AND stID IN (SELECT StID From Gy_MIOS WHERE MID=@MID) AND BillCode=@BCode

        WHILE @@FETCH_STATUS=0      --开始按分录行进行处理
			BEGIN
			    IF @bAdd=0
					BEGIN
						set @TQuan=0 - @TQuan
						set @CQuan=0 - @CQuan
                        set @MKgM =0 - @MKgM               
					END
                --更新分类进度汇总表，1-@bAdd确保在扣减数量时不累加当前凭证量
                SELECT @TQ=ISNULL(SUM(TQuan),0) ,@CQ=ISNULL(SUM(CQUAN),0),@TMK=ISNULL(SUM(MkgM),0) 
                    FROM Gy_MIOS a INNER JOIN Gy_MIOM b ON a.MID = b.MID 
                    WHERE a.OrdID=@OrdID AND a.StID=@stID AND b.BillCode=@BCode AND a.MID <> @MID*(1-@bAdd)

                INSERT INTO Gy_BillJD (OrdID,BillCode,stID,TQuan,CQuan,MkgM,iJd) 
                        VALUES(@OrdID,@BCode,@StID,@TQ,@CQ,@TMK,1)
			
				--对目标仓进行入库操作 根据bAdd,有可能是增减库存 2020-10-21 物料入库时需检测规格长度，因为型材的长度不一样需区分
                --对于板材，整颗卷规格长度要求=1 ，则所有数量可以合并，生成后的原料成品，规格长度为实际加工后的长度，不进行合并
			    IF @BCode='6010' or @BCode='6060' or ((@BCode='7500' OR @BCode='7510') and @SID>1) 
					or ((@BCode='7550' OR @BCode='7530') and @StID=1 ) or @BCode='1501' or @BCode='1502'
				    IF EXISTS (Select * From KF_MMKJQuan Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj
						and MLen=@MLen and Kjyear=@Kjyear and Period=@MM and WhCode=@dWHC)
						--更新期间入库数--2021-06-11 更新入库数的核算方法为
						Update KF_MMKJQuan Set NIQuanT =NIQuanT +@TQuan,NIQuanC=NIQuanC+@CQuan,MkgM=MkgM+@MkgM 
							Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj and MLen=@MLen
							and Kjyear=@Kjyear and Period=@MM and WhCode=@dWHC
					else
						Insert Into KF_MMKJQuan(Mnum,WhCode,Made,MColor,MPrj,MLen,MkgM,Kjyear,Period,NIQuanT,NIQuanC,IPrice) 
							Values (@Mnum,@dWHC,@Made,@MCol,@Mprj,@MLen,@MKgM,@Kjyear,@MM,@TQuan,@CQuan,@iPric)
				--针对源仓进行出库操作--开平或分条的第一行，源仓出库--生产入库  7550销售出库要精确到产地 长度 
				--如果是委外入库 BCode=1501 ioclasscode='wyrk'
			    IF @BCode='6060' or ((@BCode='7500' OR @BCode='7510') and @SID=1) or ((@BCode='7550' OR @BCode='7530') and @StID=2 ) 
					or @BCode='7570'  or ( @BCode='7580' AND LEFT(@Inv,3)<>'002') or @BCode='1502' or (@BCode='1501' AND @IOC = 'wyrk')
				    IF EXISTS (Select * From KF_MMKJQuan Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj
						and MLen=@MLen and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC)
						Update KF_MMKJQuan Set NOQuanT = NOQuanT+@TQuan,NOQuanC=NOQuanC+@CQuan,MkgM=MkgM+@MkgM 
							Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj and MLen=@MLen
								and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC
					else
						Insert Into KF_MMKJQuan(Mnum,Whcode,Made,MColor,MPrj,MLen,MkgM,Kjyear,Period,NOQuanT,NOQuanC)
							Values(@Mnum,@sWHC,@Made,@MCol,@Mprj,@MLen,@MKgM,@Kjyear,@MM,@TQuan,@CQuan)
				--2023-06-25 电镀加工成品入库需要做成本核算 
				if (@BCode = '7530' AND @StID = 1)
				BEGIN
					SELECT @HPrice = IPrice,@KTQuan=(FNQuanT+NIQuanT-NOQuanT) From KF_MMKJQuan 	--读取当前的库存现存量和成本单价
                        WHERE Mnum=@Mnum AND Made=@Made AND Kjyear=@Kjyear AND [Period]=@MM AND mLen=@MLen AND @MPrj=MPrj AND @MCol=MColor
                    --在核算前，已经将当前需核算的数量也写入库存了，所以必须要对当前准备核算 的数量进行扣减，如果扣减后=0，
					IF @HPrice>0 AND (@KTQuan - @TQuan)>0 AND @KTQuan > 0	--当前已有库存单价 进行移动均价计算,当前的库存量-此次核算量=实际已核算的库存量*成本单价=库存金额
                        --已核算的库存量,因为采购核算不同步，此计算并不准确，必须采用其它辅助条件判明哪些重量已核算成本 
                        SET @AvgP=((@KTQuan-@TQuan)*@HPrice+@TQuan*@iPric) /@KTQuan	--当前(库存量-当前核算量)*成本价+当前核算金额/库存量
                    ELSE
                        SET @AvgP=@iPric           --如果当前物料未有核算成本单价，直接将此次的单价写入
                        --如果未设置成本单价，直接用当前核算单价写入
                    UPDATE KF_MMKJQuan SET IPrice=@AvgP 
                            WHERE Mnum=@Mnum AND Made=@Made AND Kjyear=@Kjyear AND [Period]=@MM AND mLen=@MLen AND @MPrj=MPrj AND @MCol=MColor
				END
				--2022-10-07 如果是委外入库 BCode=1501 ioclasscode='wyrk'
				/*ELSE IF @BCode = '1501' AND @IOC = 'wyrk' 
					BEGIN
						SELECT @SMM = sMnum From Ba_SMToM WHERE dMnum = @Mnum
						IF @SMM <>'' 
						BEGIN
							IF EXISTS (Select * From KF_MMKJQuan Where Mnum=@SMM and Made=@Made and MColor=@MCol and MPrj=@Mprj
								and MLen=@MLen and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC)
								Update KF_MMKJQuan Set NOQuanT = NOQuanT+@TQuan,NOQuanC=NOQuanC+@CQuan,MkgM=MkgM+@MkgM 
								Where Mnum=@SMM and Made=@Made and MColor=@MCol and MPrj=@Mprj and MLen=@MLen
									and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC
							else
								Insert Into KF_MMKJQuan(Mnum,Whcode,Made,MColor,MPrj,MLen,MkgM,Kjyear,Period,NOQuanT,NOQuanC)
									Values(@SMM,@sWHC,@Made,@MCol,@Mprj,@MLen,@MKgM,@Kjyear,@MM,@TQuan,@CQuan)
						END
					END*/
					
                --2021-06-18 修改型材销售出库方式，不再以出库单上的重量为准，而是按实际支重进行换算重量出库
                IF ( @BCode='7580' AND LEFT(@Inv,3)='002')
                    BEGIN
                        --获取实际米重
                        SELECT @tMz= tMz From Gy_MMTMZ WHERE Mnumber=@Mnum AND Made=@Made
                        IF @tMz>0
                            BEGIN
                                SET @dTQ = @CQuan * @tMZ * @MLen / 1000
                                --IF @bAdd = 0
                                --    SET @dTQ = 0 - @dTQ
                            END
                        ELSE
                            SET @dTQ = @TQuan
                        SET @dTQ = ROUND(@dTQ,2)            --重量保留2位小数
                        IF EXISTS (Select * From KF_MMKJQuan Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj
                            and MLen=@MLen and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC)
                            Update KF_MMKJQuan Set NOQuanT = NOQuanT+@dTQ,NOQuanC=NOQuanC+@CQuan,MkgM=MkgM+@MkgM 
                                Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj and MLen=@MLen
                                    and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC
                        else
                            Insert Into KF_MMKJQuan(Mnum,Whcode,Made,MColor,MPrj,MLen,MkgM,Kjyear,Period,NOQuanT,NOQuanC)
                                Values(@Mnum,@sWHC,@Made,@MCol,@Mprj,@MLen,@MKgM,@Kjyear,@MM,@dTQ,@CQuan)
                        --2021-06-25 检查库存支数=0 时重量不为0，则检测重量可否归0
                        SELECT @dTQ =FNQuanT+NIQuanT-NOQuanT,@dCQ=FNQuanC+ NIQuanC-NOQuanC From KF_MMKJQuan 
                            Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj
                                and MLen=@MLen and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC
                        --如果支数=0 重量小于支重 10% 
                        IF @dCQ=0 AND @dTQ<>0 AND abs(@dTQ) < @tMz*@MLen /10000
                            Update KF_MMKJQuan Set NOQuanT = NOQuanT+@dTQ  
                                Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj and MLen=@MLen
                                    and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC

                    END
				--2021-01-19 针对销售出仓，如果是型材类 ，判断库存值，如果片/支=0 而重量<>0 ，将重量置为0
                IF (@BCode='7570' or @BCode='7580') AND LEFT(@Inv,3)='002' and @MType<2
                    BEGIN
                        Select @dTQ = (FNQuanT+NIQuanT-NOQuanT),@dCQ= (FNQuanC+NIQuanC-NOQuanC) From KF_MMKJQuan 
                            WHERE Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj and MLen=@MLen
								and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC 
                        IF @dCQ=0 and @dTQ<>0
                            UPDATE KF_MMKJQuan SET NOQuanT=NOQuanT + @dTQ 
                            WHERE Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj and MLen=@MLen
								and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC 
                    END
				--对源仓进行占料(采购调拨-6030)   (销售订单-8010)
			    IF @BCode='6030' 
				    IF EXISTS (Select * From KF_MMKJQuan Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj
						and MLen=@MLen and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC)
						Update KF_MMKJQuan Set NUQuanT=NUQuanT + @TQuan ,NUQuanC=NUQuanC + @CQuan
							Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj
								and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC
					else
						Insert INTO KF_MMKJQuan(Mnum,Whcode,Made,MColor,MPrj,MLen,Kjyear,Period,NUQuanT,NUQuanC) 
							Values(@Mnum,@sWHC,@Made,@MCol,@Mprj,@MLen,@Kjyear,@MM,@TQuan,@CQuan)
                
			    IF @BCode='8010'			--销售订单在审核时形成占料，不考虑属性
				    IF EXISTS (Select * From KF_MMKJQuan Where Mnum=@Mnum and Made=@Made and MColor=@MCol 
						and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC)
						Update KF_MMKJQuan Set NUQuanT=NUQuanT + @TQuan ,NUQuanC=NUQuanC + @CQuan
							Where Mnum=@Mnum and Made=@Made and MColor=@MCol 
								and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC
					ELSE
						Insert INTO KF_MMKJQuan(Mnum,Whcode,Made,MColor,MPrj,MLen,Kjyear,Period,NUQuanT,NUQuanC) 
							Values(@Mnum,@sWHC,@Made,@MCol,@Mprj,@MLen,@Kjyear,@MM,@TQuan,@CQuan)
				--对源仓进行去占料(6060) 销售出库
			    IF @BCode='6060' or @BCode='7570' or @BCode='7580' or (@BCode = '1501' and @IOC = 'wyrk')
				    IF EXISTS (Select * From KF_MMKJQuan Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj
						and MLen=@MLen and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC)
						Update KF_MMKJQuan Set NUQuanT= NUQuanT - @TQuan,NUQuanC=NUQuanC - @CQuan
							Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj and MLen=@MLen
							and Kjyear=@Kjyear and Period=@MM and WhCode=@sWHC
					ELSE
						Insert INTO KF_MMKJQuan(Mnum,Whcode,Made,MColor,MPrj,MLen,Kjyear,Period,NUQuanT,NUQuanC) 
							Values(@Mnum,@sWHC,@Made,@MCol,@Mprj,@MLen,@Kjyear,@MM,0-@TQuan,0-@CQuan)
				--对目标仓进行在途量
			    IF @BCode='6030'
				    IF EXISTS (Select * From KF_MMKJQuan Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj
						and MLen=@MLen and Kjyear=@Kjyear and Period=@MM and WhCode=@dWHC)
						Update KF_MMKJQuan Set NWQuanT=NWQuanT + @TQuan,NWQuanC=NWQuanC + @CQuan
							Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj and MLen=@MLen
								and Kjyear=@Kjyear and Period=@MM and WhCode=@dWHC
					else
						Insert INTO KF_MMKJQuan(Mnum,Whcode,Made,MColor,MPrj,MLen,Kjyear,Period,NWQuanT,NWQuanC) 
							Values(@Mnum,@dWHC,@Made,@MCol,@Mprj,@MLen,@Kjyear,@MM,@TQuan,@CQuan)
				--对目标仓进行去在途量
			    IF @BCode='6060' or (@BCode = '1501' and @IOC = 'wyrk')
				    IF EXISTS (Select * From KF_MMKJQuan Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj
						and MLen=@MLen and Kjyear=@Kjyear and Period=@MM and WhCode=@dWHC)
						Update KF_MMKJQuan Set NWQuanT=NWQuanT - @TQuan,NWQuanC=NWQuanC - @CQuan
							Where Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj and MLen=@MLen
							and Kjyear=@Kjyear and Period=@MM and WhCode=@dWHC
					else
						Insert INTO KF_MMKJQuan(Mnum,Whcode,Made,MColor,MPrj,MLen,Kjyear,Period,NUQuanT,NUQuanC) 
							Values(@Mnum,@dWHC,@Made,@MCol,@Mprj,@MLen,@Kjyear,@MM,0-@TQuan,0-@CQuan)
                --采购入库检查米重
                --2021-06-08》》针对型材必须要检查库存中的相同产地的实际重量和支数，进行移动米重计算
				--分条或开平的板材米重算法未做进来
                IF @BCode='6060' AND @bAdd =1 AND @CQuan>0 AND @TQuan>0 AND ( LEFT(@Inv,3)='001' OR LEFT(@Inv,3)='002')
                    BEGIN
					/*	针对型材，先读取库存中的实际重量和支数 和当前入库数进行相加，然后重新计算米重*/
						IF LEFT(@Inv,3)='002'
							BEGIN
								Select @dTQ = (FNQuanT+NIQuanT-NOQuanT),@dCQ= (FNQuanC+NIQuanC-NOQuanC) From KF_MMKJQuan 
									WHERE Mnum=@Mnum and Made=@Made and MColor=@MCol and MPrj=@Mprj and MLen=@MLen
									and Kjyear=@Kjyear and Period=@MM and WhCode=@dWHC 
								IF @dCQ>0 
									BEGIN
										SET @tMz= @dTQ / @dCQ 
										IF @MLen<20 AND @MLen>0      --针对比型材支长按米录 换算成米重
											SET @tMz =@TMZ /@MLEN
										IF  @MLen >=20                --针对型材支长按mm录 ，需要补回
											SET @tMz=@tMz / @MLen *1000
									END
							END
						ELSE
							SET @tMz= @TQuan / @CQuan
						IF @tMz >0 
							IF EXISTS (SELECT * FROM Gy_MMTMZ WHERE Mnumber=@Mnum AND Made=@Made)
								UPDATE Gy_MMTMZ SET tMZ=@tMz WHERE Mnumber=@Mnum AND Made=@Made
							ELSE
								INSERT INTO Gy_MMTMZ (Mnumber,Made,tMZ) VALUES(@Mnum,@Made,@tMz)
					END
                ELSE
				    SELECT @TMz = dbo.KF_Fn_GetTMz(@Mnum,@Made)		--获取米重
				--货号不能为空，并且指定凭证才需要录入 --如果货号已存在，不写入 只有入库才需要写入
                --2022-02-18 修改HH录入模式,针对6060 此处货号为空不执行
				--2023-06-29 其它入库单如果有货号,同样也需要录入
				SELECT @iMoP = CASE WHEN @BCode='6060' then 0 ELSE 1 END
			    IF @HH<>'' AND (@BCode='6060' OR ((@BCode='7500' OR @BCode='7510') and @SID>1) 
						or (@BCode='7550' and @StID=1 ) or @BCode = '1501')
					BEGIN
					    IF exists(Select HH From KF_MHHQuan Where HH=@HH)
							UPDATE KF_MHHQuan Set IQuanT=IQuanT+@TQuan,NQuanT=NQuanT+@TQuan,IQuanC=IQuanC+@CQuan,
								NQuanC=NQuanC+@CQuan Where HH=@HH
						else
							Insert Into KF_MHHQuan(MID,SID,WHCode,Mnum,HH,Made,MColor,MPrj,MLen,IDate,iJd,IQuanT,IQuanC,
								NQuanT,NQuanC,MOP,OrdID,tMz) 
								Values(@MID,@SID,@dWHC,@Mnum,@HH,@Made,@MCol,@Mprj,@MLen,@BDate,1,@TQuan,@CQuan,@TQuan,@CQuan,
									@iMoP,@OrdID,@TMz)
					END
				--实际货号出库
			    IF @HH<>'' and (((@BCode='7500' OR @BCode='7510') and @SID=1) or (@BCode='7550' and @StID=2 ))
					Update KF_MHHQuan Set NQuanT=NQuanT - @TQuan,NQuanC=NQuanC-@CQuan Where HH=@HH
				--更改货号进度
			    IF @HH<>''
					BEGIN
						Update KF_MHHQuan Set iJd = CASE WHEN NQuanT<=0 THEN 8 ELSE 1 END Where HH=@HH
						Insert Into KF_HHIO (MID,SID,HH,BillDate,Mnum,QuanT,QuanC,IOType,CzyID) 
							Values(@MID,@SID,@HH,@BDate,@Mnum,@TQuan,@CQuan,@BCode,@CzyID)
					END
				FETCH NEXT FROM RS
				INTO @SID,@BCode,@StID,@Mnum,@TQuan,@CQuan,@sWHC,@dWHC,@OrdID,@MKgM,@Made,@MCol,@MPrj,@Kjyear,
                                @MM,@MLen,@HH,@BDate,@BNum,@Inv,@MType,@ION,@IOC,@iPric
			END
			set @RMsg='OK'
		CLOSE RS
		DEALLOCATE RS
	END try
	BEGIN catch
		CLOSE RS
		DEALLOCATE RS
		SET @RMsg=ERROR_MESSAGE()
	END catch
END
