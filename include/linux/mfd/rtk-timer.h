
#define REG_TIM_EN		0x00
#define REG_TIM_CR		0x04
#define REG_TIM_DIER		0x08
#define REG_TIM_SR		0x0C
#define REG_TIM_EGR		0x10
#define REG_TIM_CNT		0x14
#define REG_TIM_PSC		0x18
#define REG_TIM_ARR		0x1C
#define REG_TIM_SEC		0x20
#define REG_TIM_CCR0		0x24
#define REG_TIM_CCR1		0x28
#define REG_TIM_CCR2		0x2c
#define REG_TIM_CCR3		0x30
#define REG_TIM_CCR4		0x34
#define REG_TIM_CCR5		0x38
#define REG_TIM_PSYNC0		0x3C
#define REG_TIM_PSYNC1		0x40
#define REG_TIM_PSYNC2		0x44
#define REG_TIM_PSYNC3		0x48
#define REG_TIM_PSYNC4		0x4c
#define REG_TIM_PSYNC5		0x50
#define REG_TIM_PHASECNT0		0x54
#define REG_TIM_PHASECNT1		0x58
#define REG_TIM_PHASECNT2		0x5c
#define REG_TIM_PHASECNT3		0x60
#define REG_TIM_PHASECNT4		0x64
#define REG_TIM_PHASECNT5		0x68


#define TIM_BIT_CNT_EN       ((u32)0x00000001 << 16)
#define TIM_BIT_CEN          ((u32)0x00000001 << 8)
#define TIM_BIT_CNT_STOP     ((u32)0x00000001 << 1)
#define TIM_BIT_CNT_START    ((u32)0x00000001 << 0)


#define TIM_MASK_ETP         ((u32)0x00000003 << 8)
#define TIM_ETP(x)           ((u32)(((x) & 0x00000003) << 8))
#define TIM_GET_ETP(x)       ((u32)(((x >> 8) & 0x00000003)))
#define TIM_BIT_ARPE         ((u32)0x00000001 << 4)
#define TIM_BIT_OPM          ((u32)0x00000001 << 3)
#define TIM_BIT_URS          ((u32)0x00000001 << 2)
#define TIM_BIT_UDIS         ((u32)0x00000001 << 1)


#define TIM_BIT_UIE5         ((u32)0x00000001 << 21)
#define TIM_BIT_UIE4         ((u32)0x00000001 << 20)
#define TIM_BIT_UIE3         ((u32)0x00000001 << 19)
#define TIM_BIT_UIE2         ((u32)0x00000001 << 18)
#define TIM_BIT_UIE1         ((u32)0x00000001 << 17)
#define TIM_BIT_UIE0         ((u32)0x00000001 << 16)
#define TIM_BIT_CC5IE        ((u32)0x00000001 << 6)
#define TIM_BIT_CC4IE        ((u32)0x00000001 << 5)
#define TIM_BIT_CC3IE        ((u32)0x00000001 << 4)
#define TIM_BIT_CC2IE        ((u32)0x00000001 << 3)
#define TIM_BIT_CC1IE        ((u32)0x00000001 << 2)
#define TIM_BIT_CC0IE        ((u32)0x00000001 << 1)
#define TIM_BIT_UIE          ((u32)0x00000001 << 0)


#define TIM_BIT_UG_DONE      ((u32)0x00000001 << 31)
#define TIM_BIT_UIF5         ((u32)0x00000001 << 21)
#define TIM_BIT_UIF4         ((u32)0x00000001 << 20)
#define TIM_BIT_UIF3         ((u32)0x00000001 << 19)
#define TIM_BIT_UIF2         ((u32)0x00000001 << 18)
#define TIM_BIT_UIF1         ((u32)0x00000001 << 17)
#define TIM_BIT_UIF0         ((u32)0x00000001 << 16)
#define TIM_BIT_CC5IF        ((u32)0x00000001 << 6)
#define TIM_BIT_CC4IF        ((u32)0x00000001 << 5)
#define TIM_BIT_CC3IF        ((u32)0x00000001 << 4)
#define TIM_BIT_CC2IF        ((u32)0x00000001 << 3)
#define TIM_BIT_CC1IF        ((u32)0x00000001 << 2)
#define TIM_BIT_CC0IF        ((u32)0x00000001 << 1)
#define TIM_BIT_UIF          ((u32)0x00000001 << 0)


#define TIM_BIT_CC5G         ((u32)0x00000001 << 6)
#define TIM_BIT_CC4G         ((u32)0x00000001 << 5)
#define TIM_BIT_CC3G         ((u32)0x00000001 << 4)
#define TIM_BIT_CC2G         ((u32)0x00000001 << 3)
#define TIM_BIT_CC1G         ((u32)0x00000001 << 2)
#define TIM_BIT_CC0G         ((u32)0x00000001 << 1)
#define TIM_BIT_UG           ((u32)0x00000001 << 0)


#define TIM_MASK_CNT         ((u32)0x0000FFFF << 0)
#define TIM_CNT(x)           ((u32)(((x) & 0x0000FFFF) << 0))
#define TIM_GET_CNT(x)       ((u32)(((x >> 0) & 0x0000FFFF)))


#define TIM_MASK_PSC         ((u32)0x0000FFFF << 0)
#define TIM_PSC(x)           ((u32)(((x) & 0x0000FFFF) << 0))
#define TIM_GET_PSC(x)       ((u32)(((x >> 0) & 0x0000FFFF)))


#define TIM_MASK_ARR         ((u32)0x0000FFFF << 0)
#define TIM_ARR(x)           ((u32)(((x) & 0x0000FFFF) << 0))
#define TIM_GET_ARR(x)       ((u32)(((x >> 0) & 0x0000FFFF)))


#define TIM_BIT_OPM_DLx      ((u32)0x00000001 << 29)
#define TIM_BIT_CCxM         ((u32)0x00000001 << 27)
#define TIM_BIT_CCxP         ((u32)0x00000001 << 26)
#define TIM_BIT_OCxPE        ((u32)0x00000001 << 25)
#define TIM_BIT_CCxE         ((u32)0x00000001 << 24)
#define TIM_MASK_CCRx        ((u32)0x0000FFFF << 0)
#define TIM_CCRx(x)          ((u32)(((x) & 0x0000FFFF) << 0))
#define TIM_GET_CCRx(x)      ((u32)(((x >> 0) & 0x0000FFFF)))


#define TIM_BIT_SYNCENx      ((u32)0x00000001 << 27)
#define TIM_BIT_SYNCDIRx     ((u32)0x00000001 << 26)
#define TIM_BIT_SYNCPEx      ((u32)0x00000001 << 25)
#define TIM_MASK_SYNCPHASEx  ((u32)0x0000FFFF << 0)
#define TIM_SYNCPHASEx(x)    ((u32)(((x) & 0x0000FFFF) << 0))
#define TIM_GET_SYNCPHASEx(x) ((u32)(((x >> 0) & 0x0000FFFF)))

#define TIM_MASK_CNTx        ((u32)0x0000FFFF << 0)
#define TIM_CNTx(x)          ((u32)(((x) & 0x0000FFFF) << 0))
#define TIM_GET_CNTx(x)      ((u32)(((x >> 0) & 0x0000FFFF)))


#define TIM_BIT_SEC        ((u32)0x00000001 << 0)


#define TIM_Channel_0                      ((u16)0x0000)
#define TIM_Channel_1                      ((u16)0x0001)
#define TIM_Channel_2                      ((u16)0x0002)
#define TIM_Channel_3                      ((u16)0x0003)
#define TIM_Channel_4                      ((u16)0x0004)
#define TIM_Channel_5                      ((u16)0x0005)

#define IS_TIM_CHANNEL(CHANNEL) (((CHANNEL) == TIM_Channel_0) || \
                                    ((CHANNEL) == TIM_Channel_1) || \
                                    ((CHANNEL) == TIM_Channel_2) || \
                                    ((CHANNEL) == TIM_Channel_3) || \
                                    ((CHANNEL) == TIM_Channel_4) || \
                                    ((CHANNEL) == TIM_Channel_5))


#define IS_TIM_PSC(VAL) (VAL <= 0xFFFF)


#define TIM_OPMode_ETP_positive		((u32)0x00000000)
#define TIM_OPMode_ETP_negative		((u32)0x00000100)
#define TIM_OPMode_ETP_bothedge		((u32)0x00000200)
#define IS_TIM_OPM_ETP_MODE(MODE) (((MODE) == TIM_OPMode_ETP_positive) || \
                               ((MODE) == TIM_OPMode_ETP_negative) || \
                               ((MODE) == TIM_OPMode_ETP_bothedge))

#define TIM_OPMode_Single			((u32)0x00000008)
#define TIM_OPMode_Repetitive		((u32)0x00000000)
#define IS_TIM_OPM_MODE(MODE) (((MODE) == TIM_OPMode_Single) || \
                               ((MODE) == TIM_OPMode_Repetitive))

#define TIM_UpdateSource_Global		((u32)0x00000000)
#define TIM_UpdateSource_Overflow	((u32)0x00000004)
#define IS_TIM_UPDATE_SOURCE(SOURCE) (((SOURCE) == TIM_UpdateSource_Global) || \
                                      ((SOURCE) == TIM_UpdateSource_Overflow))



#define TIM_IT_Update			((u32)0x00000001)
#define TIM_IT_CC0				((u32)0x00000002)
#define TIM_IT_CC1				((u32)0x00000004)
#define TIM_IT_CC2				((u32)0x00000008)
#define TIM_IT_CC3				((u32)0x00000010)
#define TIM_IT_CC4				((u32)0x00000020)
#define TIM_IT_CC5				((u32)0x00000040)
#define TIM_IT_UIE0				((u32)0x00010000)
#define TIM_IT_UIE1				((u32)0x00020000)
#define TIM_IT_UIE2				((u32)0x00040000)
#define TIM_IT_UIE3				((u32)0x00080000)
#define TIM_IT_UIE4				((u32)0x00100000)
#define TIM_IT_UIE5				((u32)0x00200000)
#define IS_TIM_IT(IT) ((((IT) & (u32)0xFFC0FF80) == 0x0000) && (((IT) & (u32)0x3F007F) != 0x0000))

#define IS_TIM_GET_IT(IT) (((IT) == TIM_IT_Update) || \
                              ((IT) == TIM_IT_CC0) || \
                              ((IT) == TIM_IT_CC1) || \
                              ((IT) == TIM_IT_CC2) || \
                              ((IT) == TIM_IT_CC3) || \
                              ((IT) == TIM_IT_CC4) || \
                              ((IT) == TIM_IT_CC5) || \
                              ((IT) == TIM_IT_UIE0) || \
                              ((IT) == TIM_IT_UIE1) || \
                              ((IT) == TIM_IT_UIE2) || \
                              ((IT) == TIM_IT_UIE3) || \
                              ((IT) == TIM_IT_UIE4) || \
                              ((IT) == TIM_IT_UIE5))


#define TIM_PSCReloadMode_Update		((u32)0x00000000)
#define TIM_PSCReloadMode_Immediate	((u32)0x00000001)
#define IS_TIM_PRESCALER_RELOAD(RELOAD) (((RELOAD) == TIM_PSCReloadMode_Update) || \
                                         ((RELOAD) == TIM_PSCReloadMode_Immediate))


#define TIM_EventSource_Update		((u32)0x00000001)
#define TIM_EventSource_CC0			((u32)0x00000002)
#define TIM_EventSource_CC1			((u32)0x00000004)
#define TIM_EventSource_CC2			((u32)0x00000008)
#define TIM_EventSource_CC3			((u32)0x00000010)
#define TIM_EventSource_CC4			((u32)0x00000020)
#define TIM_EventSource_CC5			((u32)0x00000040)
#define IS_LP_TIM_EVENT_SOURCE(SOURCE) ((((SOURCE) & 0xFFFFFFFE) == 0x0000) && \
						(((SOURCE) & 0x1) != 0x0000))
#define IS_HP_TIM_EVENT_SOURCE(SOURCE) ((((SOURCE) & 0xFFFFFF80) == 0x0000) && \
						(((SOURCE) & 0x7F) != 0x0000))


#define TIM_CCx_Enable				((u32)0x01000000)
#define TIM_CCx_Disable				((u32)0x00000000)
#define IS_TIM_CCX(CCX) (((CCX) == TIM_CCx_Enable) || ((CCX) == TIM_CCx_Disable))

#define TIM_OCPreload_Enable		((u32)0x02000000)
#define TIM_OCPreload_Disable		((u32)0x00000000)
#define IS_TIM_OCPRELOAD_STATE(STATE) (((STATE) == TIM_OCPreload_Enable) || \
                                       ((STATE) == TIM_OCPreload_Disable))

#define TIM_CCPolarity_High			((u32)0x00000000)
#define TIM_CCPolarity_Low			((u32)0x04000000)
#define IS_TIM_CC_POLARITY(POLARITY) (((POLARITY) == TIM_CCPolarity_High) || \
                                      ((POLARITY) == TIM_CCPolarity_Low))


#define TIM_CCMode_PWM			((u32)0x00000000)
#define TIM_CCMode_Inputcapture		((u32)0x08000000)
#define IS_TIM_CC_MODE(MODE) (((MODE) == TIM_CCMode_PWM) || \
                              ((MODE) == TIM_CCMode_Inputcapture))

#define TIM_CCMode_PulseWidth		((u32)0x00000000)
#define TIM_CCMode_PulseNumber	((u32)0x10000000)
#define IS_TIM_CC_PULSEMODE(MODE) (((MODE) == TIM_CCMode_PulseWidth) || \
                              ((MODE) == TIM_CCMode_PulseNumber))

#define TIM_CCMode_CCR				((u32)0x0000FFFF)
#define IS_TIM_CC_PULSEWIDTH(Compare) ((Compare) <= TIM_CCMode_CCR)


#define TIM9_GET_CC0PM(x)                             ((u32)(((x) & 0x10000000) >> 28))
#define TIM9_CC0PM(x)                                    ((u32)(((x) & 0x00000001) << 28))


#define TIMBasic_GET_ARR(x)                            ((u32)(((x) & 0xFFFFFFFF) >> 0))
#define TIMBasic_ARR(x)                                   ((u32)(((x) & 0xFFFFFFFF) << 0))


#define TIMBasic_GET_CNT(x)                            ((u32)(((x) & 0xFFFFFFFF) >> 0))
#define TIMBasic_CNT(x)                                   ((u32)(((x) & 0xFFFFFFFF) << 0))


#define TIMPWM_PSync_Delay				((u32)0x00000000)
#define TIMPWM_PSync_Ahead				((u32)0x00000001)
#define IS_TIMPWM_PSync_Dir(DIR) (((DIR) == TIMPWM_PSync_Delay) || ((DIR) == TIMPWM_PSync_Ahead))

#define TIMPWM_PSyncPreload_Enable		((u32)0x00000000)
#define TIMPWM_PSyncPreload_Disable		((u32)0x00000001)
#define IS_TIMPWM_PPRELOAD_STATE(STATE) (((STATE) == TIMPWM_PSyncPreload_Enable) || \
                                       ((STATE) == TIMPWM_PSyncPreload_Disable))

#define TIMPWM_DefaultLevel_High				((u32)0x00000000)
#define TIMPWM_DefaultLevel_Low				((u32)0x00000001)
#define IS_TIMPWM_DefaultLevel(LEVEL) (((LEVEL) == TIMPWM_DefaultLevel_High) || ((LEVEL) == TIMPWM_DefaultLevel_Low))

#define TimerNum	14

#define TIMER_TICK_US		 			31
#define TIMER_TICK_US_X4				(4*1000000/32000) //32k clock, 31.25us every timer_tick


struct rtk_tim{
	struct clk * tim_clk;
	void __iomem * base;
	u32 clk_rate;
	int irq;
	int valid;
	int index;

	int period;
	u32 mode;
	int enable;

	void (*intr_handler)(void * cbdata);
	void *cbdata;
};



#if IS_REACHABLE(CONFIG_MFD_RTK_AMEBA_TIMERS)

/**
  * @brief  Change timer period.
  * @param  index: timer index, can be 0~13.
  * @param  period_ns: timer preiod and its unit is nanosecond.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_change_period(u32 index, u64 period_ns);

/**
  * @brief  Start or stop timer(counter will not reset to 0).
  * @param  index: timer index, can be 0~13.
  * @param  NewState: can be 0 or 1, indicate start and stop counter.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_start(u32 index, u32 NewState);

/**
  * @brief  Enable or disable timer update interrupt.
  * @param  index: timer index, can be 0~13.
  * @param  NewState: can be 0 or 1, indicate enable and disable interrupt.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_int_config(u32 index, u32 NewState);

/**
  * @brief  Initilize timer.
  * @param  index: timer index, can be 0~13.
  * @param  period_ns: timer preiod and its unit is nanosecond.
  * @param  cbhandler: callback handler registered.
  * @param  cbdata: callback data of handler.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_init(u32 index, u64 period_ns, void * cbhandler, void * cbdata);

/**
  * @brief  Allocate timer 2~7, and initilize timer.
  * @param  period_ns: timer preiod and its unit is nanosecond.
  * @param  cbhandler: callback handler registered.
  * @param  cbdata: callback data of handler.
  * @retval timer index: if success; num < 0 if fail.
  * 		 non-zero: failed.
  */
int rtk_gtimer_dynamic_init(u64 period_ns, void * cbhandler, void * cbdata);

/**
  * @brief  Clear interrupt flag.
  * @param  index: timer index, can be 0~13.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_int_clear(u32 index);

/**
  * @brief  Deinitilize timer.
  * @param  index: timer index, can be 0~13.
  * @retval  0: success;
  * 		 non-zero: failed.
  */
int rtk_gtimer_deinit(u32 index);


#else

static inline int rtk_gtimer_change_period(u32 index, u64 period_ns)
{
	return -ENODEV;
}

static inline int rtk_gtimer_start(u32 index, u32 NewState)
{
	return -ENODEV;
}

static inline int rtk_gtimer_int_config(u32 index, u32 NewState)
{
	return -ENODEV;
}

static inline int rtk_gtimer_init(u32 index, u64 period_ns, void * cbhandler, void * cbdata)
{
	return -ENODEV;
}

int rtk_gtimer_dynamic_init(u64 period_ns, void * cbhandler, void * cbdata)
{
  return -ENODEV;
}

static inline int rtk_gtimer_int_clear(u32 index)
{
	return -ENODEV;
}

static inline int rtk_gtimer_deinit(u32 index)
{
	return -ENODEV;
}


#endif
