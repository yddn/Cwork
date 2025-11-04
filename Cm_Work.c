#define uint8_t unsigned char
#define SYS_HXT 2
#define Cm_OFF 15
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define SYS_FJXT 1

void w_CmWork(void)								//称门工作 ==100ms调用一次
{
	uint8_t i;
	static uint8_t irp[2]={0,0};				//定义重复关闭标志 抖料延时

	for(i = 0;i < SYS_HXT; i++)
	{
		if(CmV[i].djs > 0 && CmV[i].djs != 255)	//=255手动开门,不倒计时关门
		{
      //printf("CmV Nowv:%d-%d-%d, iok:%d\r\n",CmV[i].djs,CmV[i].nv[0],CmV[i].nv[1],CmV[i].iok);
			CmV[i].djs --;
      //如果料已落完<0.2g 并且 开关小于允许关时间,可以提前关 否则必须开门
			if(CmV[i].djs	 < Cm_OFF )//|| (abs(CmV[i].nv[0]) <= 100 && abs(CmV[i].nv[1]) <= 100) )  //||           (CmV[i].djs < (Cm_OFF - 2) && (abs(CmV[i].nv[0]) + abs(CmV[i].nv[1])) / 2  < 50)) 
        Cm_OC(i,GPIO_PIN_RESET);    //关闭
			else
        Cm_OC(i,GPIO_PIN_SET);  //否则开启
//			if(CmV[i].djs < 2)	//600ms关闭,延时300 开始检测 2 1 0 检测三次
//		在设置时间内完成落料即关门,关闭计时置0
      if(CmV[i].djs < 3 && abs(CmV[i].nv[0]) <= 50 && abs(CmV[i].nv[1]) <= 50)	//100 0ms 检测重量是否<0.3g
      {
        irp[i] = 0;
        CmV[i].djs = 0;
        CmV[i].iok = Cm_iok_del;				//无去皮重,可以直接开始称重->修改为延时
        if(wrc.work == w_run || wrc.work == w_one || wrc.work == w_puse)  // && wrc.tp_kdll == TP_kd_ling)
        {           
#ifdef SYS_FJXT
          //wrc.tp_kdll = TP_kd_llfj;     //落料完成;	 手动只有free 状态
          if(fjrv.zt == FJ_ZT_RCV) s_LMZT(1);       // fjrv.zt = FJ_ZT_CUN;   //修改料门状态为存料状态
          else if(fjrv.zt == FJ_ZT_CLR) s_LMZT(2);  // fjrv.zt = FJ_ZT_FREE;
          //printf("Fj2 zt %d \r\n",fjrv.zt);
#else
          wrc.tp_kdll = TP_kd_llok;     //落料完成;	无分检,落料完成
          wrc.rys = prsv->rpfv[prsv->wp.wid].msv.ylys;
#endif
        }   
      }
      else if(CmV[i].djs == 0)		//倒计时完成检测2次>0.2g
      {
        //printf("CmV Nowv:%d-%d-%d\r\n",i,CmV[i].nv[0],CmV[i].nv[1]);
        //pvf[1] = 0; 20241021 抖料前延时100ms再判断下，
        if(irp[i] < HX_CM_DLCS) 		//小于预设抖料次数
        {
          irp[i] ++;
          CmV[i].djs = (Cm_OFF + 2);  //+2 称门才有机会打开
          Cm_OC(i,GPIO_PIN_SET);
        }         
        else			//到达预设最大抖料次数还有余量,去皮
        {					//去皮之前要判断当前余量,判断是否堵料了	
          //if(CmV[i].nv[0] > 100 && CmV[i].nv[1] > 100)  //如果>1g 判断为堵料
          if(wrc.work == w_run || wrc.work == w_puse) //&& wrc.tp_kdll == TP_kd_ling)
          { 
      #ifdef SYS_FJXT
            if(fjrv.zt == FJ_ZT_RCV)  s_LMZT(1);        // fjrv.zt = FJ_ZT_CUN;   //修改料门状态
            else if(fjrv.zt == FJ_ZT_CLR) s_LMZT(2);    // fjrv.zt = FJ_ZT_FREE;
      #else
            wrc.rys = prsv->rpfv[prsv->wp.wid].msv.ylys ;
            wrc.tp_kdll = TP_kd_llok;     //落料完成;	
      #endif
//            if(CmV[i].nv[0] > 100 && CmV[i].nv[1] > 100)    //称后余重超1g 判断为堵料,停止
//            {
//              //printf("hx dl > %d:%d-%d\r\n",i,CmV[i].nv[0],CmV[i].nv[1]);
//              CmV[i].iok = Cm_iok_stop;
//              Z_SET_BIT(rlv.oSet,oSet_lstop + i);   //标注停止
//              s_ErrShow(b_err_hxerr,3);							//称有堵料
//              return ;
//            }
          }   //落料完成 延时准备压料
          //irp[i] = 0;					//重置抖料次数
          CmV[i].iok = Cm_iok_del;			//去皮时需要延时后转入可称料状
        }
      }
		}
    else if(CmV[i].djs == 255)  
    {
      if(CmV[i].iok != Cm_iok_open)
      {
        if((fjrv.zt == FJ_ZT_FREE && fjrv.wz == FJ_WZ_NXL) || fjrv.zt == FJ_ZT_CLR) {
          CmV[i].iok = Cm_iok_open;                 //门状态为开启
          Cm_OC(i,GPIO_PIN_SET);
          fjrv.zt = FJ_ZT_CLR;
        }
      }
      //printf("opencm zt=CLR\r\n");
      if(wrc.work == w_clrt && zdpv[i].rmoc == 0) w_zdpSetOC(i,0,4); //清茶模式下，开始振动,单称在关门时关清茶模式
    }
    //在关闭和下料之间开始判断
		if(CmV[i].iok > 0 && CmV[i].iok < (Cm_iok_del + 1))	
    {
      //pvf[3] = 1;
      CmV[i].iok--;
      if(abs(CmV[i].tnv) <= CM_QP_JZ && CmV[i].iok < 4)   //不用去皮重
      {
        
        //printf("ok:%d-%d-%d-%d\r\n",i,CmV[i].tnv,CM_QP_JZ,CmV[i].iok);
        CmV[i].iok = 0;
      }
      else if(CmV[i].iok == 2)		///称后延时进入下一次称重
      {
//        if(abs(CmV[i].nv[0]) > CM_QP_JZ)    //< 0.5G
//        {
        CmV[i].iok = 1;     //再延时一次
        //printf("now qp1:%d,%d - %d - %d\r\n",i,CmV[i].tnv,CmV[i].nv[0],CmV[i].nv[1]);
        Get_Maopi(i);				// 称去皮 --必须要延时
      }
    }
//    else if(CmV[i].iok == 0 && CmV[i].rsjs > 200 && 
//      abs(CmV[i].tnv) > CM_QP_JZ && abs(CmV[i].nv[1]) > CM_QP_JZ) 
//    {
//      printf("now qp2:%d - %d\r\n",i,CmV[i].nv[0]);
//      Get_Maopi(i);				// 称去皮 --必须要延时
//      CmV[i].iok = 1;     //再延时一次
//    }
    if(wrc.work != w_clrt && wrc.work == w_run && Cm_Chk(i) == 1) w_zdpSetOC(i,0,0);   //门开关振
	}
//  if(fjrv.zt == FJ_ZT_CUN) 
//  {
//    Z_SET_BIT(rlv.oSet,oSet_fjyl);   //切换存料状态
//    Z_SET_BIT(prsv->wtv.pbset,PBSET_LMYL);
//  }
//  else if(wrc.work != w_init && IS_BIT_SET(rlv.oSet,oSet_fjyl))
//  {
//    Z_CLR_BIT(rlv.oSet,oSet_fjyl);
//    Z_CLR_BIT(prsv->wtv.pbset,PBSET_LMYL);
//    printf("CmWork>>clr PBSET_LMYL\r\n");
//  }
}
