#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_clkpwr.h"
#define AHB_SRAM0 0x2007C000

volatile uint8_t curveMode = 0;
volatile uint8_t adqMode = 0;
volatile uint8_t calibMode = 0;
volatile uint8_t running = 0;
volatile uint16_t * tabla_seno = (uint16_t *) AHB_SRAM0;
const uint16_t valores_crudos[256] = {
		32768, 33600, 34368, 35200, 35968, 36800, 37568, 38336, 39168, 39936, 40704, 41472,
		42240, 43008, 43776, 44544, 45312, 46016, 46720, 47488, 48192, 48896, 49600, 50240,
		50944, 51584, 52224, 52864, 53504, 54144, 54720, 55296, 55872, 56448, 57024, 57536,
		58048, 58560, 59008, 59520, 59968, 60416, 60800, 61248, 61632, 61952, 62336, 62656,
		62976, 63296, 63552, 63808, 64064, 64256, 64512, 64704, 64832, 64960, 65088, 65216,
		65344, 65408, 65408, 65472, 65472, 65472, 65408, 65408, 65344, 65216, 65088, 64960,
		64832, 64704, 64512, 64256, 64064, 63808, 63552, 63296, 62976, 62656, 62336, 61952,
		61632, 61248, 60800, 60416, 59968, 59520, 59008, 58560, 58048, 57536, 57024, 56448,
		55872, 55296, 54720, 54144, 53504, 52864, 52224, 51584, 50944, 50240, 49600, 48896,
		48192, 47488, 46720, 46016, 45312, 44544, 43776, 43008, 42240, 41472, 40704, 39936,
		39168, 38336, 37568, 36800, 35968, 35200, 34368, 33600, 32768, 31936, 31168, 30336,
		29568, 28736, 27968, 27200, 26368, 25600, 24832, 24064, 23296, 22528, 21760, 20992,
		20224, 19520, 18816, 18048, 17344, 16640, 15936, 15296, 14592, 13952, 13312, 12672,
		12032, 11392, 10816, 10240, 9664, 9088, 8512, 8000, 7488, 6976, 6528, 6016,
		5568, 5120, 4736, 4288, 3904, 3584, 3200, 2880, 2560, 2240, 1984, 1728,
		1472, 1280, 1024, 832, 704, 576, 448, 320, 192, 128, 128, 64,
		64, 64, 128, 128, 192, 320, 448, 576, 704, 832, 1024, 1280,
		1472, 1728, 1984, 2240, 2560, 2880, 3200, 3584, 3904, 4288, 4736, 5120,
		5568, 6016, 6528, 6976, 7488, 8000, 8512, 9088, 9664, 10240, 10816, 11392,
		12032, 12672, 13312, 13952, 14592, 15296, 15936, 16640, 17344, 18048, 18816, 19520,
		20224, 20992, 21760, 22528, 23296, 24064, 24832, 25600, 26368, 27200, 27968, 28736,
		29568, 30336, 31168, 31936
};
GPDMA_LLI_T listSin;

void configButton(void);
void configLED(void);//LED marca mode F/S, marca Calib
void ConfigurarReloj100MHz_FuerzaBruta(void);
void configDMA_DAC(void);
void configDAC(void);
void initDMA(void);
void cargarValoresSin(void);
void configTMR2_PWM(void);
void setServoAngle(uint16_t angle);

int main(){
	//...llamado a funciones
	configButton();
	configLED();
	configTMR2_PWM();
	ConfigurarReloj100MHz_FuerzaBruta();
	cargarValoresSin();
	configDAC();
	configDMA_DAC();
	while(running){
		if (calibMode) {
			//modo calib
		} else {
			//modo running
		}
	}
}

void EINT0_IRQHandler(void){
	EXTI_ClearFlag(0);
	//condition ? true : false;
	curveMode ? curveMode = 0 : curveMode = 1;
}

void EINT1_IRQHandler(void){
	EXTI_ClearFlag(1);
	adqMode ? adqMode = 0 : adqMode = 1;
	GPIO_TogglePins(PORT_2, (1<<8));
	GPIO_TogglePins(PORT_2, (1<<7));
}

void EINT2_IRQHandler(void){
	EXTI_ClearFlag(2);
	calibMode ? calibMode = 0 : calibMode = 1;
	GPIO_SetPins(GPIO_0, (1<<4)); // Se deberá apagar al terminar la calibración
}

void EINT3_IRQHandler(void){
	EXTI_ClearFlag(3);
	running ? running = 0 : running = 1;
}

void configButton(void){
	EXTI_Init();
	EXTI_CFG_T pC,sC,tC,cC;
	pC.line = EXTI_EINT0; 			sC.line = EXTI_EINT1;			tC.line = EXTI_EINT2;			cC.line = EXTI_EINT2;
	pC.mode = EXTI_EDGE_SENSITIVE; 	sC.mode = EXTI_EDGE_SENSITIVE;	tC.mode = EXTI_EDGE_SENSITIVE;	cC.line = EXTI_EINT2;
	pC.polarity = EXTI_HIGH_ACTIVE;	sC.polarity = EXTI_HIGH_ACTIVE;	tC.polarity = EXTI_HIGH_ACTIVE; cC.polarity = EXTI_HIGH_ACTIVE;
	EXTI_PinConfig(EXTI_EINT0, EXTI_PULLDOWN); EXTI_PinConfig(EXTI_EINT1, EXTI_PULLDOWN); EXTI_PinConfig(EXTI_EINT2, EXTI_PULLDOWN); EXTI_PinConfig(EXTI_EINT3, EXTI_PULLDOWN);
	EXTI_ClearFlag(0); EXTI_ClearFlag(1); EXTI_ClearFlag(2); EXTI_ClearFlag(3);
	EXTI_ConfigEnable(&pC); EXTI_ConfigEnable(&sC); EXTI_ConfigEnable(&tC); EXTI_ConfigEnable(&cC);
	EXTI_ClearFlag(0); EXTI_ClearFlag(1); EXTI_ClearFlag(2); EXTI_ClearFlag(3);
	EXTI_EnableIRQ(0); EXTI_EnableIRQ(1); EXTI_EnableIRQ(2); EXTI_EnableIRQ(3);
}

void configLED(void){
	PINSEL_CFG_T C,D;
	C.port = 2; D.port = 0;
	C.func = 0; D.func = 0;
	C.mode = 3; D.mode = 3;
	C.openDrain = Disable; D.openDrain = Disable;
	D.pin = 4;
	PINSEL_ConfigMultiplePins(&C, (3<<7));
	PINSEL_ConfigPin(&D);
	GPIO_SetDir(2,(3<<7),1);
	GPIO_SetDir(0,(1<<4),1);
	GPIO_ClearPins(2,(3<<7),1);
	GPIO_ClearPins(0,(1<<4),1);
}

void configTMR2_PWM(void){
    TIM_TIMERCFG_T tC;
    TIM_MATCHCFG_T mP;
    TIM_MATCHCFG_T mPu;

    // Configurar el PINSEL para que P0.6 sea GPIO
    PINSEL_CFG_T pinCfg;
    pinCfg.port = PORT_0;
    pinCfg.pin = PIN_6;
    pinCfg.func = PINSEL_FUNC_00;
    pinCfg.mode = PINSEL_TRISTATE;
    pinCfg.openDrain = DISABLE;
    PINSEL_ConfigPin(&pinCfg);

    // Configurar GPIO P0.6 como salida e iniciar en ALTO
    GPIO_SetDir(PORT_0, (1 << 6), GPIO_OUTPUT);
    GPIO_SetPinState(PORT_0, PIN_6, SET);

    // Configurar Timer en microsegundos
    tC.prescaleOpt = TIM_US;
    tC.prescaleValue = 0;
    TIM_InitTimer(LPC_TIM2, &tC);

    // Match 3: Periodo total 50Hz (20000 us)
    mP.channel = TIM_MATCH_3;
    mP.intEn = ENABLE;
    mP.resetEn = ENABLE;
    mP.stopEn = DISABLE;
    mP.extOpt = TIM_NOTHING;
    mP.matchValue = 20000;
    TIM_ConfigMatch(LPC_TIM2, &mP);

    // Match 0: Ancho de pulso inicial (1500 us)
    mPu.channel = TIM_MATCH_0;
    mPu.intEn = ENABLE;
    mPu.resetEn = DISABLE;
    mPu.stopEn = DISABLE;
    mPu.extOpt = TIM_NOTHING;
    mPu.matchValue = 1500;
    TIM_ConfigMatch(LPC_TIM2, &mPu);

    // Habilitar interrupción en el NVIC y arrancar
    NVIC_EnableIRQ(TIMER2_IRQn);
    TIM_Enable(LPC_TIM2);
}

// 2. Función para cambiar el ángulo
void setServoAngle(uint16_t angle){
    if(angle == 0){
        TIM_UpdateMatchValue(LPC_TIM2, TIM_MATCH_0, 1000);
    } else if(angle == 90){
        TIM_UpdateMatchValue(LPC_TIM2, TIM_MATCH_0, 1500);
    } else if(angle == 180){
        TIM_UpdateMatchValue(LPC_TIM2, TIM_MATCH_0, 2000);
    }
}

// 3. Rutina de interrupción (ISR) del Timer 2
void TIMER2_IRQHandler(void){
    // Verificar si la interrupción fue por el Match 3 (Llegó a 20000)
    if(TIM_GetIntStatus(LPC_TIM2, TIM_MR3_INT) == SET){
        TIM_ClearIntPending(LPC_TIM2, TIM_MR3_INT); // Limpiar bandera
        GPIO_SetPinState(PORT_0, PIN_6, SET);       // Pin ALTO
    }

    // Verificar si la interrupción fue por el Match 0 (Llegó al pulso)
    if(TIM_GetIntStatus(LPC_TIM2, TIM_MR0_INT) == SET){
        TIM_ClearIntPending(LPC_TIM2, TIM_MR0_INT); // Limpiar bandera
        GPIO_SetPinState(PORT_0, PIN_6, RESET);     // Pin BAJO
    }
}

void cargarValoresSin(void){
	for (int i = 0; i < 256; i++) {
	        tabla_seno[i] = valores_crudos[i];
	    }
}

void initDMA(void){
	GPDMA_Init();
}

void configDAC(void){
	CLKPWR_SetPCLKDiv(CLKPWR_PCLKSEL_DAC, CLKPWR_PCLKSEL_CCLK_DIV_4);
	DAC_CONVERTER_CFG_T DAC;
	DAC.dmaCounter = ENABLE;
	DAC.dmaRequest = ENABLE;
	DAC.doubleBuffer = DISABLE;
	DAC_Init();
	DAC_ConfigDAConverterControl(&DAC);
	DAC_SetBias(DAC_700uA);
	DAC_SetDMATimeOut(98); //3.9uS
}

void configDMA_DAC(void){
	listSin.control = 256 | (1<<18) | (2<<21) | (1<<26);
	listSin.nextLLI = (uint32_t) &listSin;
	listSin.dstAddr = (uint32_t) &(LPC_DAC->DACR);
	listSin.srcAddr = (uint32_t) AHB_SRAM0;
	GPDMA_Channel_CFG_T Con;
	GPDMA_Endpoint_T S,D;
	S.increment = ENABLE;
	S.width = GPDMA_HALFWORD;
	S.burst = GPDMA_BSIZE_1;
	D.increment = DISABLE;
	D.width = GPDMA_WORD;
	D.burst = GPDMA_BSIZE_1;
	Con.channelNum = 0;
	Con.transferSize = 256;
	Con.type = GPDMA_M2P;
	Con.srcMemAddr = AHB_SRAM0;
	Con.dstMemAddr = (uint32_t) &(LPC_DAC->DACR);
	Con.srcConn =(GPDMA_CONNECTION)0;
	Con.dstConn = GPDMA_DAC;
	Con.src = S;
	Con.dst = D;
	Con.intTC = DISABLE;
	Con.intErr = DISABLE;
	Con.linkedList = (uint32_t) &listSin;
	GPDMA_SetupChannel(&Con);
}

void ConfigurarReloj100MHz_FuerzaBruta(void) {

    // 1. Desconectar y apagar el PLL0 (por si alguna librería lo encendió antes)
    LPC_SC->PLL0CON   = 0x00;
    LPC_SC->PLL0FEED  = 0xAA;  // Llave de seguridad 1
    LPC_SC->PLL0FEED  = 0x55;  // Llave de seguridad 2

    // 2. Encender el Oscilador Principal (Cristal de 12 MHz externo)
    LPC_SC->SCS       = 0x20;
    // Quedarse atrapado aquí hasta que el cristal esté estable y listo
    while ((LPC_SC->SCS & 0x40) == 0);

    // 3. Elegir el Cristal de 12 MHz como fuente de donde nace el reloj
    LPC_SC->CLKSRCSEL = 0x01;

    // 4. Configurar el hardware para 300 MHz internos
    // Fórmula: Fcco = (2 * M * F_cristal) / N
    // Queremos M=25 y N=2 -> (2 * 25 * 12) / 2 = 300 MHz
    // El hardware pide que guardemos (M-1) y (N-1): 24 es 0x18, y 1 es 0x01
    LPC_SC->PLL0CFG   = 0x00010018;
    LPC_SC->PLL0FEED  = 0xAA;
    LPC_SC->PLL0FEED  = 0x55;

    // 5. Encender el multiplicador (PLL0)
    LPC_SC->PLL0CON   = 0x01;
    LPC_SC->PLL0FEED  = 0xAA;
    LPC_SC->PLL0FEED  = 0x55;

    // 6. Esperar a que el multiplicador "enganche" la frecuencia (PLOCK)
    while ((LPC_SC->PLL0STAT & (1 << 26)) == 0);

    // 7. Dividir esos 300 MHz por 3 para dárselos al procesador
    // 300 MHz / 3 = 100 MHz exactos
    LPC_SC->CCLKCFG   = 3;

    // 8. Conectar el sistema finalmente a este nuevo reloj de 100 MHz
    LPC_SC->PLL0CON   = 0x03; // Bit 0 (Enable) y Bit 1 (Connect) en 1
    LPC_SC->PLL0FEED  = 0xAA;
    LPC_SC->PLL0FEED  = 0x55;

    // 9. Actualizar la variable global de la librería CMSIS por si la usás después
    SystemCoreClock = 100000000;
}
