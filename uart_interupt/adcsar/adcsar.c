#include "adcsar.h"

#include "../plib/inc/plib015.h"
#include "../device/Include/plic.h"

void ADC_IRQHandler(void);

static volatile uint8_t s_adc_irq_flag = 0;

static char classify_code_char(uint16_t code)
{
    if (code <= ADCSAR_THR_SHORT_MAX) return 'T';
    if (code >= ADCSAR_THR_TAMPER_MIN) return 'S';
    if (code >= ADCSAR_THR_ALARM_MIN && code <= ADCSAR_THR_ALARM_MAX) return 'A';
    if (code >= ADCSAR_THR_NORMAL_MIN && code <= ADCSAR_THR_NORMAL_MAX) return 'N';
    return 'U';
}

char adcsar_classify_code(uint16_t code)
{
    return classify_code_char(code);
}

static adcsar_state_t state_from_char(char c)
{
    switch (c) {
    case 'T': return ADCSAR_STATE_SHORT;
    case 'A': return ADCSAR_STATE_ALARM;
    case 'N': return ADCSAR_STATE_NORMAL;
    case 'S': return ADCSAR_STATE_TAMPER;
    default:  return ADCSAR_STATE_UNDEF;
    }
}

void adcsar_init(void)
{
    PMUSYS->ADCPWRCFG_bit.LDOEN = 1;
    PMUSYS->ADCPWRCFG_bit.LVLDIS = 0;

    RCU->ADCSARCLKCFG_bit.CLKSEL = 1;
    RCU->ADCSARCLKCFG_bit.DIVN = 2;
    RCU->ADCSARCLKCFG_bit.DIVEN = 1;

    RCU->CGCFGAPB_bit.ADCSAREN = 1;
    RCU->RSTDISAPB_bit.ADCSAREN = 1;
    RCU->ADCSARCLKCFG_bit.CLKEN = 1;
    RCU->ADCSARCLKCFG_bit.RSTDIS = 1;

    ADCSAR->ACTL_bit.SELRES = ADCSAR_ACTL_SELRES_8bit;
    ADCSAR->ACTL_bit.CALEN = 1;
    ADCSAR->ACTL_bit.ADCEN = 1;

    ADCSAR->EMUX_bit.EM0 = 0;
    ADCSAR->SEQ[0].SRQCTL_bit.RQMAX = 0x0;
    ADCSAR->SEQ[0].SRQSEL_bit.RQ0 = 0x0;
    ADCSAR->SEQEN_bit.SEQEN0 = 1;
    ADCSAR_SEQ_StartEventConfig(ADCSAR_SEQ_Num_0, ADCSAR_SEQ_StartEvent_SwReq);
    ADCSAR_SEQ_SwStartEnCmd(ADCSAR_SEQ_Num_0, ENABLE);

    {
        uint32_t guard = 1000000;
        while (!ADCSAR->ACTL_bit.ADCRDY && guard--) ;
    }
    ADCSAR->SEQSYNC_bit.SYNC0 = 1;
    ADCSAR->SEQSYNC_bit.GSYNC = 1;

    ADCSAR_SEQ_ITConfig(ADCSAR_SEQ_Num_0, ADCSAR_SEQ_ReqNum_0, ENABLE);
    ADCSAR_SEQ_ITCmd(ADCSAR_SEQ_Num_0, ENABLE);
    ADCSAR_SEQ_ITStatusClear(ADCSAR_SEQ_Num_0);
    ADCSAR_SEQ_FIFOFullStatusClear(ADCSAR_SEQ_Num_0);
    ADCSAR_SEQ_FIFOEmptyStatusClear(ADCSAR_SEQ_Num_0);

    PLIC_SetIrqHandler(Plic_Mach_Target, IsrVect_IRQ_ADC, ADC_IRQHandler);
    PLIC_SetPriority(IsrVect_IRQ_ADC, 1);
    PLIC_IntEnable(Plic_Mach_Target, IsrVect_IRQ_ADC);
    PLIC_SetThreshold(Plic_Mach_Target, 0);

    while (ADCSAR_SEQ_GetFIFOLoad(ADCSAR_SEQ_Num_0)) {
        (void)ADCSAR_SEQ_GetFIFOData(ADCSAR_SEQ_Num_0);
    }
}

void adcsar_start(void)
{
    ADCSAR_SEQ_SwStartCmd();
}

bool adcsar_poll(adcsar_sample_t *out_sample)
{
    if (!s_adc_irq_flag) {
        return false;
    }

    s_adc_irq_flag = 0;

    uint32_t load = ADCSAR_SEQ_GetFIFOLoad(ADCSAR_SEQ_Num_0);
    if (!load) {
        return false;
    }

    uint32_t raw = ADCSAR_SEQ_GetFIFOData(ADCSAR_SEQ_Num_0);

    while (ADCSAR_SEQ_GetFIFOLoad(ADCSAR_SEQ_Num_0)) {
        (void)ADCSAR_SEQ_GetFIFOData(ADCSAR_SEQ_Num_0);
    }

    char state_char = classify_code_char((uint16_t)raw);
    if (out_sample) {
        out_sample->raw_code = (uint16_t)raw;
        out_sample->state_char = state_char;
        out_sample->state = state_from_char(state_char);
    }

    ADCSAR_SEQ_SwStartCmd();
    return true;
}

void ADC_IRQHandler(void)
{
    ADCSAR_SEQ_ITStatusClear(ADCSAR_SEQ_Num_0);
    s_adc_irq_flag = 1;
}

