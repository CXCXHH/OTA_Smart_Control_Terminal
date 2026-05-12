#include "bsp_adc.h"

/**
  * @brief  ADC1 驱动 (PA1 电压 + 内部温度传感器)
  * @note   ADC1_IN1 采集外部电压, 通道 TempSensor 采集 MCU 内部温度
  *         10次采样平均, ADC 时钟 = 72MHz/6 = 12MHz
  */
void ADC1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);  /* 72MHz / 6 = 12MHz ADC 时钟 */

    /* PA1 = ADC1_IN1 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);

    /* 校准 */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

static uint16_t ADC_ReadChannelAvg(uint8_t channel, uint8_t samples)
{
    uint32_t sum = 0;
    uint8_t i;

    for (i = 0; i < samples; i++)
    {
        ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_239Cycles5);
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
        sum += ADC_GetConversionValue(ADC1);
    }
    return (uint16_t)(sum / samples);
}

uint16_t ADC_GetVoltage(void)
{
    uint16_t adc_val = ADC_ReadChannelAvg(ADC_Channel_1, 10);
    /* adc_val / 4095 * 3.3 * 100 */
    return (uint16_t)((uint32_t)adc_val * 330 / 4095);
}

uint16_t ADC_GetTemperature(void)
{
    uint16_t adc_val = ADC_ReadChannelAvg(ADC_Channel_TempSensor, 10);
    /* TM30: V25=1.43, Avg_Slope=4.3mV/C
       Temp = (1.43 - adc_val*3.3/4095) / 0.0043 + 25 */
    int32_t t = ((int32_t)adc_val * 33000 / 4095);
    t = 14300 - t;
    t = t * 100 / 430 + 2500;
    return (uint16_t)(t < 0 ? 0 : t);
}
