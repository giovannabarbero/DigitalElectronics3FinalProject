#include "LPC17xx.h"
#include <math.h> //log
#include "lpc17xx_timer.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_pinsel.h"
#include <stdlib.h>	//Para memoria
#include "lpc17xx_gpdma.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_clkpwr.h"
#include "arm_math.h" //Funciones complejas
#include "lpc17xx_ssp.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_uart.h"

//Defines Generales de math
#define BLOCK_SIZE 128
#define FS         24000.0f
#define TAU_FAST   0.125f
#define TAU_SLOW   1.0f
#define NUM_STAGES_A 3
#define NUM_STAGES_C 2
#define AHB_SRAM0 0x2007C000
//Mux
#define DEBOUNCE_TIME_MS 350
//Defines npxl
#define NUM_LEDS 8
#define BYTES_PER_LED 9
// Le devolvemos el +1 para que el último LED no se vuelva loco
#define SPI_BUFFER_SIZE ((NUM_LEDS * BYTES_PER_LED) + 1) // 72 bytes exactos
//Defines DAC
#define tabla_seno ((volatile uint32_t *) AHB_SRAM0)
const uint32_t valores_crudos[256] = {
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
#define LLI_ADDR (AHB_SRAM0 + (256 * 4))
#define listSin (*((volatile GPDMA_LLI_T *) LLI_ADDR))
//Defines UART
#define TX_SIZE 30
#define RX_SIZE 8
uint8_t uart_tx_buffer[TX_SIZE];
uint8_t uart_rx_buffer[RX_SIZE];
volatile uint8_t flag_rx_listo = 0;
volatile uint8_t flag_tx_listo = 1;
//Defines prom para lecturas
#define NUM_PROMEDIOS 4 // Promedia las 4 lecturas 4 segundos cada 1s
float32_t buffer_promedios[NUM_PROMEDIOS] = {0.0f};
uint8_t indice_promedio = 0;
// --- BUFFERS DMA ---
uint32_t buffer_ping[BLOCK_SIZE] __attribute__((aligned(4))); //Necesito que este bien alineada
uint32_t buffer_pong[BLOCK_SIZE] __attribute__((aligned(4)));
volatile GPDMA_LLI_T listPING __attribute__((aligned(4)));
volatile GPDMA_LLI_T listPONG __attribute__((aligned(4)));
volatile uint8_t flag_ping_listo = 0;
volatile uint8_t flag_pong_listo = 0;
//Variables de modos
volatile uint8_t curveMode = 0; // 0 = Curva A, 1 = Curva C
volatile uint8_t adqMode   = 0; // 0 = FAST, 1 = SLOW
volatile uint8_t running   = 0; // Empezamos apagado
volatile uint8_t calibMode = 0;
//Para displays
volatile uint32_t ticks_20ms = 0; // Para el antirrebote de los botones
volatile uint8_t c = 0;

//Variables Math
int32_t estados_A[3][4] = {0};
int32_t estados_C[2][4] = {0};
// --- VARIABLES DSP ---
float32_t coeficientes_A[15] = {
		    0.2441f,  0.4882f,  0.2441f, -0.2743f, -0.0156f,
		    1.0000f, -2.0000f,  1.0000f,  1.6502f, -0.7123f,
		    1.0000f, -2.0000f,  1.0000f,  1.8974f, -0.9021f
};
float32_t estados_filtro_A[4 * NUM_STAGES_A];
arm_biquad_casd_df1_inst_f32 filtro_A;

float32_t coeficientes_C[10] = {
    0.994628f, -1.989257f,  0.994628f,  1.989243f, -0.989272f,
    0.622000f,  1.244000f,  0.622000f, -1.154700f, -0.333300f
};
float32_t estados_filtro_C[4 * NUM_STAGES_C];
arm_biquad_casd_df1_inst_f32 filtro_C;
float32_t entrada_float[BLOCK_SIZE];
float32_t salida_filtro[BLOCK_SIZE];
float32_t senal_cuadrado[BLOCK_SIZE];
float32_t alpha;
float32_t uno_menos_alpha;
float32_t ms_anterior = 0.0f;
float32_t nivel_dc = 2048.0f; // Asumiendo centro en 1.65V
float32_t offset = 0.0f;      // Ajustar luego para calibrar con el sonómetro real ->Unused
// Variable global del reloj
volatile uint32_t msTicks = 0;
//neopxl
int ledsaencender = 0;
uint8_t spi_buffer[SPI_BUFFER_SIZE];
// Variables UART para ver en TIMER2
volatile uint32_t contador_bloques = 0;
volatile float32_t nivel_dB_mostrar = 0.0f;
volatile uint8_t dato_dB_nuevo = 0;

// Protos

void ConfigurarReloj100MHz_FuerzaBruta(void);
void configButton(void);
void configLED(void);

void configDMA_ADCpp(void);
void configADC(void);
void actualizarAlpha(void);
float32_t calibrar_lectura_dB(float32_t db_crudo);

void configDMA_DAC(void);
void configDAC(void);
void initDMA(void);
void cargarValoresSin(void);

void configTMR1_PWM(void);
void setServoAngle(uint16_t angle);
void moverServoPorDb(int dB_actual);

void carga_dB_NeoPixel(float nivel_dB);
void Set_LED_Color(int led_index, uint8_t g, uint8_t r, uint8_t b);
void enviar_NeoPixel_Rapido(void);
void Configurar_SSP0_NeoPixel(void);
int Calcular_Leds_Desde_dB(float valor_dB);

void Configurar_Timer0_Retardos(void);
void Delay_us(uint32_t us);
void Delay_ms(uint32_t ms);

void configPIN_disp(void);
uint8_t DecADisp(uint8_t);
void configTMR2(void);
volatile int PDdisplay; //Variables Disp
volatile int PEdisplay;

void turnOffButtons(void);
void turnOnButtons(void);

void Configurar_UART0(void);
void EnviarString_DMA(char* texto);
void ConfigurarRX_DMA(void);

void apagar_DAC_pin(void);
void encender_DAC_pin(void);

int main() {
	cargarValoresSin();
	LPC_SC->PCONP |= (1 << 26); // DAC
	LPC_SC->PCONP |= (1 << 29); // GPDMA
    LPC_SC->FLASHCFG = (LPC_SC->FLASHCFG & ~0x0000F000) | (4 << 12);//Proteger Flash y subir a 100 MHz (Necesario)
    ConfigurarReloj100MHz_FuerzaBruta();

    configTMR1_PWM();
    Configurar_Timer0_Retardos();
    Configurar_SSP0_NeoPixel();
    configADC();
    configDMA_ADCpp();
	//configDAC();
	//configDMA_DAC();
	//GPDMA_ChannelStart(GPDMA_CH_1); -->Pasados a calivb
    configPIN_disp();
    configTMR2();
    TIM_Enable(LPC_TIM2);
    Configurar_UART0();
    ConfigurarRX_DMA();
    configLED();     // LEDs de modo FAST/SLOW y calibración
    configButton();  // EINT0-3
    //apagar npxl
    for(int i=0; i<NUM_LEDS; i++) Set_LED_Color(i, 0, 0, 0);
    enviar_NeoPixel_Rapido();

    // Inicializar Filtro A y Filtro C siempre al inicio
    arm_biquad_cascade_df1_init_f32(&filtro_A, NUM_STAGES_A, coeficientes_A, estados_filtro_A);
    arm_biquad_cascade_df1_init_f32(&filtro_C, NUM_STAGES_C, coeficientes_C, estados_filtro_C);
    curveMode = 0;       // Aseguramos
    adqMode = 0;         // Aseguramos
    actualizarAlpha();

    uint32_t *buffer_a_procesar = NULL;
    contador_bloques = 0; // global

while(1){
    while(running){
    	if (calibMode) {
    		//Inicio DAC
    		LPC_SC->PCONP |= (1 << 26);
    		encender_DAC_pin();
    		configDAC();
    		configDMA_DAC();

    		GPIO_SetPins(1, (1U << 31)); //LED
    		turnOffButtons(); //Bloq

			// Apago npxl y dsp
			for(int i=0; i<NUM_LEDS; i++) Set_LED_Color(i, 0, 0, 0);
			enviar_NeoPixel_Rapido();
			PDdisplay = 0;
			PEdisplay = 0;

			// Servo a 0° (El mínimo ruido calibrado es maso 13 dB)
			moverServoPorDb(13);

			//Iniciar el DAC
			GPDMA_ChannelStart(GPDMA_CH_1);

			uint8_t calibrado = 0;
			uint8_t error_calib = 0; // <- Flag de error solicitada
			uint8_t intentos = 0;    // <- Contador de intentos (5)
			uint32_t *buffer_calib = NULL;

			// Variables temporales
			float32_t salida_A[BLOCK_SIZE], salida_C[BLOCK_SIZE];
			float32_t senal_cuad_A[BLOCK_SIZE], senal_cuad_C[BLOCK_SIZE];
			float32_t ms_ant_A = ms_anterior, ms_ant_C = ms_anterior;
			uint32_t contador_calib = 0;

			//Bucle de calibración: Adquirir hasta que se calibre o falle 5 veces
			while (!calibrado && !error_calib) {
				// Chequear disponibilidad de buffer DMA del ADC
				if (flag_ping_listo) {
					flag_ping_listo = 0;
					buffer_calib = buffer_ping;
				} else if (flag_pong_listo) {
					flag_pong_listo = 0;
					buffer_calib = buffer_pong;
				}
				if (buffer_calib != NULL) {
					// Remover bias DC
					for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
						uint32_t adc_raw = (buffer_calib[i] >> 4) & 0xFFF;
						entrada_float[i] = ((float32_t)adc_raw) - nivel_dc;
					}
					// Filtrar en paralelo (Curva A y Curva C)
					arm_biquad_cascade_df1_f32(&filtro_A, entrada_float, salida_A, BLOCK_SIZE);
					arm_biquad_cascade_df1_f32(&filtro_C, entrada_float, salida_C, BLOCK_SIZE);
					// Elevar al cuadrado
					arm_mult_f32(salida_A, salida_A, senal_cuad_A, BLOCK_SIZE);
					arm_mult_f32(salida_C, salida_C, senal_cuad_C, BLOCK_SIZE);
					// Integración Exponencial RMS
					float32_t ms_A = 0.0f, ms_C = 0.0f;
					for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
						ms_A = (alpha * senal_cuad_A[i]) + (uno_menos_alpha * ms_ant_A);
						ms_ant_A = ms_A;

						ms_C = (alpha * senal_cuad_C[i]) + (uno_menos_alpha * ms_ant_C);
						ms_ant_C = ms_C;
					}

					contador_calib++;
					buffer_calib = NULL;

					// Mostrar e inspeccionar resultados cada ~1 segundo (~187 bloques) si 1blq = Muestras/MPS = 5.33 mseg, cant blq = 1s / 5.33ms
					if (contador_calib >= 187) {
						contador_calib = 0;
						intentos++; // Incrementamos el intento en cada segundo transcurrido

						float32_t db_A_crudo = 0.0f, db_C_crudo = 0.0f;

						if (ms_A > 0.0001f) db_A_crudo = 10.0f * log10f(ms_A);
						if (ms_C > 0.0001f) db_C_crudo = 10.0f * log10f(ms_C);

						// Aplicar corrección final de Hardware
						float32_t db_A = calibrar_lectura_dB(db_A_crudo);
						float32_t db_C = calibrar_lectura_dB(db_C_crudo);

						printf("Intento %d/5 - Curva A: %.2f dB | Curva C: %.2f dB\n", intentos, db_A, db_C);
						fflush(stdout);

						// Chequear Condiciones (Misma onda con tolerancia de 1dB y rango entre 90 y 95)
						if (fabs(db_A - db_C) <= 1.0f && (db_A >= 90.0f && db_A <= 95.0f)) {
							printf("¡Calibracion Exitosa!\n");
							calibrado = 1;
						}
						// Max intentos entonces err
						else if (intentos >= 5) {
							printf("Error: No se pudo calibrar tras 5 intentos.\n");
							fflush(stdout);
							error_calib = 1;
						}
					}
				}
			}

			// Caso error
			if (error_calib) {
				// PRINT EE
				PDdisplay = 11;
				PEdisplay = 11;

				// Bloqueo para ver EE
				for (volatile uint32_t i = 0; i < 24000000; i++);
				error_calib = 0;
			}

			// Fin calib, vuelvo a comun
			calibMode = 0;
			GPIO_ClearPins(1, (1U << 31));
			// Apagamos el funcionamiento del DAC
			GPDMA_ChannelStop(GPDMA_CH_1);
			LPC_DAC->DACR = 0;
			apagar_DAC_pin();
			GPIO_SetDir(0, (1 << 26), 1);
			GPIO_ClearPins(0, (1 << 26));
			LPC_SC->PCONP &= ~(1 << 26);
			// Reactivamos los botones llamando a la función
			turnOnButtons();
    	}
    	else //	NORMAL
    	{
    	//Recepcion ordenes UART
		if (flag_rx_listo) {
			flag_rx_listo = 0;
			if (strncmp((char*)uart_rx_buffer, "SETMODOA", 8) == 0) curveMode = 0;
			else if (strncmp((char*)uart_rx_buffer, "SETMODOC", 8) == 0) curveMode = 1;
			else if (strncmp((char*)uart_rx_buffer, "SETVEL_F", 8) == 0) { adqMode = 0; actualizarAlpha(); }
			else if (strncmp((char*)uart_rx_buffer, "SETVEL_S", 8) == 0) { adqMode = 1; actualizarAlpha(); }
			else if (strncmp((char*)uart_rx_buffer, "FAILBTTN", 8) == 0) { turnOffButtons(); }
			else if (strncmp((char*)uart_rx_buffer, "RETUBTTN", 8) == 0) { turnOnButtons(); }

			ConfigurarRX_DMA();
			//GPDMA_ChannelStart(GPDMA_CH_4); -> empezarlo en la DMA directo
		}
    	// Check buffer
		if (flag_ping_listo) {
			flag_ping_listo = 0;
			buffer_a_procesar = buffer_ping;
		} else if (flag_pong_listo) {
			flag_pong_listo = 0;
			buffer_a_procesar = buffer_pong;
		}
		//Procesamos audio
		if (buffer_a_procesar != NULL) {
			// Caast a float y elimino bias DC
			for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
				uint32_t adc_raw = (buffer_a_procesar[i] >> 4) & 0xFFF;
				entrada_float[i] = ((float32_t)adc_raw) - nivel_dc;
			}
			if(curveMode){
			arm_biquad_cascade_df1_f32(&filtro_C, entrada_float, salida_filtro, BLOCK_SIZE);}
			else {
			arm_biquad_cascade_df1_f32(&filtro_A, entrada_float, salida_filtro, BLOCK_SIZE);
			}
			// sqrt
			arm_mult_f32(salida_filtro, salida_filtro, senal_cuadrado, BLOCK_SIZE);
			//actualizarAlpha();//pasar a el cambio directo en eint
			// D. Integración Exponencial (Fast RMS)
			float32_t valor_MS_actual = 0.0f;

			for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
				valor_MS_actual = (alpha * senal_cuadrado[i]) + (uno_menos_alpha * ms_anterior);
				ms_anterior = valor_MS_actual;
			}
			//Imprimir resultados 1 vez por segundo (~187 bloques) si 1blq = Muestras/MPS = 5.33 mseg, cant blq = 1s / 5.33ms
			contador_bloques++;
			if (contador_bloques >= 187) {
				contador_bloques = 0;

				// 1. Calculamos el nivel crudo
				float32_t nivel_dB_crudo = 0.0f;
				if (valor_MS_actual > 0.0001f) {
				    nivel_dB_crudo = 10.0f * log10f(valor_MS_actual);
				}

				// LIMITAMOS EL RANGO (Esto evita el crash por valores fuera de tabla)
				if (curveMode == 1) {
									nivel_dB_crudo = nivel_dB_crudo - 15.0f; // <--- Restamos el exceso
								}

				//llamamos a tu función de calibración
				float32_t nivel_dB_final = calibrar_lectura_dB(nivel_dB_crudo);

				// MOVING AVERAGE
				//buffer rápido para evitar arrancar en 0
				if (buffer_promedios[0] == 0.0f) {
					for(int i=0; i<NUM_PROMEDIOS; i++) buffer_promedios[i] = nivel_dB_final;
				}

				buffer_promedios[indice_promedio] = nivel_dB_final;
				indice_promedio = (indice_promedio + 1) % NUM_PROMEDIOS;

				float32_t suma_promedio = 0.0f;
				for(int i = 0; i < NUM_PROMEDIOS; i++) {
					suma_promedio += buffer_promedios[i];
				}

				nivel_dB_mostrar = suma_promedio / NUM_PROMEDIOS; //promedio de 4
				dato_dB_nuevo = 1; // Avisar a TIMER2_IRQHandler que hay un dato fresco para UART

				// Formateo para imprimir con int usando prom
				int parte_entera  = (int)nivel_dB_mostrar;
				int parte_decimal = (int)((nivel_dB_mostrar - (float32_t)parte_entera) * 100.0f);
				if (parte_decimal >= 100) {
					parte_decimal = 0;
					parte_entera++;
				}

				//char* modo_curva = (curveMode == 0) ? "A" : "C";
				//char* modo_adq   = (adqMode   == 0) ? "FAST" : "SLOW";

				// Imprimir resultados con la etiqueta dinámica
				//printf("Nivel de Ruido [%s/%s]: %d.%02d dB\n", modo_curva, modo_adq, parte_entera, parte_decimal);
				//fflush(stdout);

				int valor_display = parte_entera;
				if (valor_display > 99) {
					valor_display = 99;
				}

				PDdisplay = valor_display / 10;
				PEdisplay = valor_display % 10;

				moverServoPorDb(parte_entera);// Servo directamente se actualiza al actualizar valor
				//Delay_ms(50);
				carga_dB_NeoPixel(parte_entera);
				//Delay_ms(20);
			    if(adqMode == 0) { // FAST
			        GPIO_SetPins(2, (1U << 8));   // Encender LED FAST (P2.8)
			        GPIO_ClearPins(2, (1U << 7)); // Apagar  LED SLOW (P0.7)
			    } else { // SLOW
			        GPIO_ClearPins(2, (1U << 8)); // Apagar  LED FAST
			        GPIO_SetPins(2, (1U << 7));   // Encender LED SLOW
			    }
			}
			// Liberamos el buffer para el DMA
			buffer_a_procesar = NULL;
			}
    	}
    }
	//termina el while running
}
    return 0;
}

void actualizarAlpha(void){
    if(adqMode){
        alpha = 1.0f - expf(-1.0f / (FS * TAU_SLOW));
        uno_menos_alpha = 1.0f - alpha;
    } else {
        alpha = 1.0f - expf(-1.0f / (FS * TAU_FAST));
        uno_menos_alpha = 1.0f - alpha;
    }
}

void configADC(void) {
    LPC_SC->PCONP |= (1 << 12);
    ADC_PowerUp();
    ADC_Init(24000);
    ADC_PinConfig(ADC_CHANNEL_0);
    ADC_ChannelEnable(ADC_CHANNEL_0);
    ADC_BurstEnable();
    ADC_StartCmd(ADC_START_CONTINUOUS);
}

void configDMA_ADCpp(void) {
    LPC_SC->PCONP |= (1 << 29);
    LPC_GPDMA->DMACConfig = 0x01;
    GPDMA_Init();

    uint32_t controlADCPP = (BLOCK_SIZE & 0xFFF)|(2 << 18)|(2 << 21)|(1 << 27)|(1U << 31);

    listPING.control = controlADCPP;
    listPING.nextLLI = (uint32_t)&listPONG;
    listPING.dstAddr = (uint32_t)buffer_ping;
    listPING.srcAddr = (uint32_t)(&LPC_ADC->ADDR0);

    listPONG.control = controlADCPP;
    listPONG.nextLLI = (uint32_t)&listPING;
    listPONG.dstAddr = (uint32_t)buffer_pong;
    listPONG.srcAddr = (uint32_t)(&LPC_ADC->ADDR0);

    GPDMA_Channel_CFG_T ConADC;
    GPDMA_Endpoint_T S1, D1;
    S1.increment = DISABLE; S1.width = GPDMA_WORD; S1.burst = GPDMA_BSIZE_1;
    D1.increment = ENABLE;  D1.width = GPDMA_WORD; D1.burst = GPDMA_BSIZE_1;

    ConADC.channelNum = 0;
    ConADC.transferSize = BLOCK_SIZE;
    ConADC.type = GPDMA_P2M;
    ConADC.srcMemAddr = 0;
    ConADC.dstMemAddr = (uint32_t)buffer_ping;
    ConADC.srcConn = GPDMA_ADC;
    ConADC.dstConn = (GPDMA_CONNECTION)0;
    ConADC.src = S1;
    ConADC.dst = D1;
    ConADC.intTC = ENABLE;
    ConADC.intErr = DISABLE;
    ConADC.linkedList = (uint32_t)&listPONG;

    GPDMA_SetupChannel(&ConADC);
    NVIC_EnableIRQ(DMA_IRQn);
    GPDMA_ChannelStart(GPDMA_CH_0);
}

void DMA_IRQHandler(void) {
    static uint8_t turno_ping = 1;

    // Canal 0: ADC (ping/pong)
    if (GPDMA_IntGetStatus(GPDMA_INTTC, GPDMA_CH_0) == SET) {
        GPDMA_ClearIntPending(GPDMA_CLR_INTTC, GPDMA_CH_0);
        if (turno_ping) { flag_ping_listo = 1; turno_ping = 0; }
        else            { flag_pong_listo = 1; turno_ping = 1; }
    }

    // 1 lo usa DAC

    // 2 lo iba a usar npxl

    // Canal 3: UART TX
    if (GPDMA_IntGetStatus(GPDMA_INTTC, GPDMA_CH_3) == SET) {
        GPDMA_ClearIntPending(GPDMA_CLR_INTTC, GPDMA_CH_3);
        flag_tx_listo = 1;
    }

    // Canal 4: UART RX
    if (GPDMA_IntGetStatus(GPDMA_INTTC, GPDMA_CH_4) == SET) {
        GPDMA_ClearIntPending(GPDMA_CLR_INTTC, GPDMA_CH_4);
        flag_rx_listo = 1;
    }
}

void cargarValoresSin(void){
	for (int i = 0; i < 256; i++) {
	    tabla_seno[i] = valores_crudos[i];
	}
}

void configDAC(void){
	CLKPWR_SetPCLKDiv(CLKPWR_PCLKSEL_DAC, CLKPWR_PCLKSEL_CCLK_DIV_4);

	DAC_CONVERTER_CFG_T DAC;
	DAC.dmaCounter   = ENABLE;
	DAC.dmaRequest   = ENABLE;
	DAC.doubleBuffer = DISABLE;

	DAC_Init();
	DAC_ConfigDAConverterControl(&DAC);
	DAC_SetBias(DAC_700uA);
	DAC_SetDMATimeOut(96); // 3.9uS a CClock 100MHz -> 1.00673 kHz
}

void configDMA_DAC(void){
	listSin.srcAddr = (uint32_t) AHB_SRAM0;
	listSin.dstAddr = (uint32_t) &(LPC_DAC->DACR);
	listSin.nextLLI = (uint32_t) &listSin;
	listSin.control = GPDMA_DMACCxControl_TransferSize(256)
	                 | GPDMA_DMACCxControl_SWidth(GPDMA_WORD)
	                 | GPDMA_DMACCxControl_DWidth(GPDMA_WORD)
	                 | GPDMA_DMACCxControl_SI;

	GPDMA_Channel_CFG_T Con;
	GPDMA_Endpoint_T S, D;

	S.increment = ENABLE;
	S.width     = GPDMA_WORD;
	S.burst     = GPDMA_BSIZE_1;

	D.increment = DISABLE;
	D.width     = GPDMA_WORD;
	D.burst     = GPDMA_BSIZE_1;

	Con.channelNum  = GPDMA_CH_1;
	Con.transferSize = 256;
	Con.type        = GPDMA_M2P;
	Con.srcMemAddr  = AHB_SRAM0;
	Con.dstMemAddr  = (uint32_t) &(LPC_DAC->DACR);
	Con.srcConn     = (GPDMA_CONNECTION) 0;
	Con.dstConn     = GPDMA_DAC;
	Con.src = S;
	Con.dst = D;
	Con.intTC  = DISABLE;
	Con.intErr = DISABLE;
	Con.linkedList = (uint32_t) &listSin;

	GPDMA_SetupChannel(&Con);
}

void ConfigurarReloj100MHz_FuerzaBruta(void) {
    LPC_SC->CLKSRCSEL = 0x00;
    LPC_SC->PLL0CON   = 0x00;
    LPC_SC->PLL0FEED  = 0xAA;
    LPC_SC->PLL0FEED  = 0x55;

    LPC_SC->FLASHCFG = (LPC_SC->FLASHCFG & ~0x0000F000) | (4 << 12);

    LPC_SC->SCS       = 0x20;
    while ((LPC_SC->SCS & 0x40) == 0);

    LPC_SC->CLKSRCSEL = 0x01;
    LPC_SC->PLL0CFG   = 0x00010018;
    LPC_SC->PLL0FEED  = 0xAA;
    LPC_SC->PLL0FEED  = 0x55;
    LPC_SC->PLL0CON   = 0x01;
    LPC_SC->PLL0FEED  = 0xAA;
    LPC_SC->PLL0FEED  = 0x55;
    while ((LPC_SC->PLL0STAT & (1 << 26)) == 0);
    LPC_SC->CCLKCFG   = 2;
    LPC_SC->PLL0CON   = 0x03;
    LPC_SC->PLL0FEED  = 0xAA;
    LPC_SC->PLL0FEED  = 0x55;
    SystemCoreClock = 100000000;
}

//compensar la saturación del hardware
float32_t calibrar_lectura_dB(float32_t db_crudo) {
    // 0: Ruido de fondo. fluctuación real
    // 1: rango audible medio.
    // 2: SIRENA. daba ~56 crudos, y ~93 reales.
    // 3: Saturación máxima (gritos fuertes).
	const int NUM_PUNTOS = 6;
	    // (25 a 40): 1 a 1.
	    // (40 a 45): compensar levemente.
	    // (45 a 48): Rampa media.
	    // (48 a 51): Rampa final sirena.
	    // (51 a 55): Techo máximo de seguridad.
	float32_t crudo_x[] = { 25.0f, 40.0f, 45.0f, 48.0f, 51.0f, 55.0f };
	float32_t real_y[]  = { 20.0f, 30.0f, 50.0f, 70.0f, 93.0f, 105.0f };
    // Menor al primer punto, valor crudo
    if (db_crudo <= crudo_x[0]) {
        return db_crudo;
    }
    // Techo
    if (db_crudo >= crudo_x[NUM_PUNTOS - 1]) {
        return real_y[NUM_PUNTOS - 1];
    }
    // piecewise linear interpolation
    for (int i = 0; i < NUM_PUNTOS - 1; i++) {
        if (db_crudo >= crudo_x[i] && db_crudo <= crudo_x[i+1]) {
            float32_t pendiente = (real_y[i+1] - real_y[i]) / (crudo_x[i+1] - crudo_x[i]);
            return real_y[i] + pendiente * (db_crudo - crudo_x[i]);
        }
    }
    return db_crudo; // Por las duds
}

void configTMR1_PWM(void){ //Para Servo
    TIM_TIMERCFG_T tC;
    TIM_MATCHCFG_T mP;
    TIM_MATCHCFG_T mPu;

    PINSEL_CFG_T pinCfg;
    pinCfg.port = PORT_0;
    pinCfg.pin = PIN_6;
    pinCfg.func = PINSEL_FUNC_00;
    pinCfg.mode = PINSEL_TRISTATE;
    pinCfg.openDrain = DISABLE;
    PINSEL_ConfigPin(&pinCfg);

    GPIO_SetDir(PORT_0, (1 << 6), GPIO_OUTPUT);
    GPIO_SetPinState(PORT_0, PIN_6, SET);

    tC.prescaleOpt = TIM_TICK;
    tC.prescaleValue = 24;       //1 MHz
    TIM_InitTimer(LPC_TIM1, &tC);

    mP.channel = TIM_MATCH_3;
    mP.intEn = ENABLE;
    mP.resetEn = ENABLE;
    mP.stopEn = DISABLE;
    mP.extOpt = TIM_NOTHING;
    mP.matchValue = 20000; //20ms para este servo de T
    TIM_ConfigMatch(LPC_TIM1, &mP);

    mPu.channel = TIM_MATCH_0;
    mPu.intEn = ENABLE;
    mPu.resetEn = DISABLE;
    mPu.stopEn = DISABLE;
    mPu.extOpt = TIM_NOTHING;
    mPu.matchValue = 1500;
    TIM_ConfigMatch(LPC_TIM1, &mPu); //definir ancho pulso

    NVIC_SetPriority(TIMER1_IRQn, 0);
    NVIC_EnableIRQ(TIMER1_IRQn);
    TIM_Enable(LPC_TIM1);
}

void setServoAngle(uint16_t angle){ //Para calculo de angl
    if(angle == 0){ TIM_UpdateMatchValue(LPC_TIM1, TIM_MATCH_0, 1000); } //1ms act
    else if(angle == 1){ TIM_UpdateMatchValue(LPC_TIM1, TIM_MATCH_0, 1125); }
    else if(angle == 2){ TIM_UpdateMatchValue(LPC_TIM1, TIM_MATCH_0, 1250); }
    else if(angle == 3){ TIM_UpdateMatchValue(LPC_TIM1, TIM_MATCH_0, 1375); }
    else if(angle == 4){ TIM_UpdateMatchValue(LPC_TIM1, TIM_MATCH_0, 1500); }
    else if(angle == 5){ TIM_UpdateMatchValue(LPC_TIM1, TIM_MATCH_0, 1625); }
    else if(angle == 6){ TIM_UpdateMatchValue(LPC_TIM1, TIM_MATCH_0, 1750); }
    else if(angle == 7){ TIM_UpdateMatchValue(LPC_TIM1, TIM_MATCH_0, 1875); }
    else if(angle == 8){ TIM_UpdateMatchValue(LPC_TIM1, TIM_MATCH_0, 2000); } //2ms act
}

// directamente los decibelios
void moverServoPorDb(int dB_actual) {
    // Limites
    if (dB_actual < 13) dB_actual = 13;
    if (dB_actual > 98) dB_actual = 98;
    // Calibrasmos porque esta al reves
    int PWM_MIN_RUIDO = 2400; // PWM de 13 dB 0°
    int PWM_MAX_RUIDO = 600;  // PWM de 98 dB 180°
    // Restar y moverse para el lado contrario sola
    int match_value = (dB_actual - 13) * (PWM_MAX_RUIDO - PWM_MIN_RUIDO) / (98 - 13) + PWM_MIN_RUIDO;
    // ACTUALIZO
    TIM_UpdateMatchValue(LPC_TIM1, TIM_MATCH_0, match_value);
}

void TIMER1_IRQHandler(void){
    // INT MAT 3? t=20ms
    if(TIM_GetIntStatus(LPC_TIM1, TIM_MR3_INT) == SET){
        TIM_ClearIntPending(LPC_TIM1, TIM_MR3_INT);
        GPIO_SetPinState(PORT_0, PIN_6, SET);       //SET
        ticks_20ms++; // <--- AGREGAR ESTA LÍNEA (Suma 1 cada 20ms)
    }

    // INT MAT 0? PulWdth
    if(TIM_GetIntStatus(LPC_TIM1, TIM_MR0_INT) == SET){
        TIM_ClearIntPending(LPC_TIM1, TIM_MR0_INT);
        GPIO_SetPinState(PORT_0, PIN_6, RESET);     //CLR
    }
}

void carga_dB_NeoPixel(float nivel_dB) {
    uint8_t brillo = 25; // Brillo lindo y estable
    ledsaencender = Calcular_Leds_Desde_dB(nivel_dB);

    for (int i = 0; i < NUM_LEDS; i++) {
        if (i < ledsaencender) {
            // Le pasamos los parámetros en orden: (Verde, Azul, Rojo)
            if (i < 4) {
                Set_LED_Color(i, brillo, 0, 0);       // VERDE para los 4 primeros
            } else if (i < 6) {
                Set_LED_Color(i, brillo, 0, brillo);  // AMARILLO (Verde + Rojo) para los 2 del medio
            } else {
                Set_LED_Color(i, 0, 0, brillo);       // ROJO para los 2 últimos
            }
        } else {
            Set_LED_Color(i, 0, 0, 0); // Apagados si no llega el nivel de dB
        }
    }
    enviar_NeoPixel_Rapido();
}

void Set_LED_Color(int led_index, uint8_t verde, uint8_t rojo, uint8_t azul) {
    if (led_index >= NUM_LEDS) return;

    // El mapa definitivo de tu tira (Comprobado: 1° Verde, 2° Azul, 3° Rojo)
    uint32_t color = (verde << 16) | (azul << 8) | rojo;

    int buffer_i = led_index * BYTES_PER_LED;
    uint8_t current_b = 0;
    int bit_pos = 7;

    for (int i = 23; i >= 0; i--) {
        uint8_t bit_neo = (color >> i) & 0x01;
        uint8_t pattern = bit_neo ? 0x06 : 0x04;

        for (int p = 2; p >= 0; p--) {
            if ((pattern >> p) & 0x01) {
                current_b |= (1 << bit_pos);
            }
            bit_pos--;

            if (bit_pos < 0) {
                spi_buffer[buffer_i++] = current_b;
                current_b = 0;
                bit_pos = 7;
            }
        }
    }
}

// Envio por POLLING DIRECTO a hardware

void enviar_NeoPixel_Rapido(void){
    while (LPC_SSP0->SR & (1 << 2)) {
        volatile uint32_t dummy = LPC_SSP0->DR;
        (void)dummy;
    }

    // Apagamos las interrupciones para que la CPU no parpadee
    //__disable_irq();

    for (int i = 0; i < SPI_BUFFER_SIZE; i++) {
        while ((LPC_SSP0->SR & (1 << 1)) == 0);
        LPC_SSP0->DR = spi_buffer[i];
    }

    while (LPC_SSP0->SR & (1 << 4));

    // Volvemos a encender las interrupciones
    //__enable_irq();

    Delay_ms(1);
}

void Configurar_SSP0_NeoPixel(void) {
    PINSEL_CFG_T PinCfg;
    PinCfg.port = 0;
    PinCfg.pin = 18;
    PinCfg.func = PINSEL_FUNC_10;
    PinCfg.mode = PINSEL_TRISTATE;
    PinCfg.openDrain = DISABLE;
    PINSEL_ConfigPin(&PinCfg);

    SSP_CFG_Type SSP_Ct;
    SSP_ConfigStructInit(&SSP_Ct);
    SSP_Ct.Databit = SSP_DATABIT_8;
    SSP_Ct.CPHA = SSP_CPHA_FIRST;
    SSP_Ct.CPOL = SSP_CPOL_LO;
    SSP_Ct.Mode = SSP_MASTER_MODE;
    SSP_Ct.FrameFormat = SSP_FRAME_SPI;
    SSP_Ct.ClockRate = 2500000; // 2.5 MHz -> 400ns

    SSP_Init(LPC_SSP0, &SSP_Ct);
    SSP_Cmd(LPC_SSP0, ENABLE);
}

int Calcular_Leds_Desde_dB(float valor_dB) {
    const float MIN_DB = 01.0f;
    const float MAX_DB = 99.0f;
    const int MAX_LEDS = 8;

    if (valor_dB <= MIN_DB) return 0;
    if (valor_dB >= MAX_DB) return MAX_LEDS;

    float rango_db = MAX_DB - MIN_DB;
    float db_activos = valor_dB - MIN_DB;
    float proporcion = db_activos / rango_db;

    return (int)(proporcion * MAX_LEDS);
}

void Configurar_Timer0_Retardos(void) {
    TIM_TIMERCFG_T timCfg;
    timCfg.prescaleOpt = TIM_US;
    timCfg.prescaleValue = 1;
    TIM_InitTimer(LPC_TIM0, &timCfg);
    TIM_Enable(LPC_TIM0);
}

void Delay_us(uint32_t us) {
    TIM_ResetCounter(LPC_TIM0);
    while(TIM_ReadTimer(LPC_TIM0) < us);
}

void Delay_ms(uint32_t ms) {
    Delay_us(ms * 1000);
}

void configButton(void){
    EXTI_CFG_T extiCfg;

    // 1. Reset e Inicialización general
    EXTI_Init();

    // 2. Configurar pines con PULL-DOWN interno.
    // Botones en reposo = 0V (PULL-DOWN). Al presionar = 3.3V.
    EXTI_PinConfig(EXTI_EINT0, EXTI_PULLDOWN); // Revisa en tu librería si la macro es exactamente así
    EXTI_PinConfig(EXTI_EINT1, EXTI_PULLDOWN);
    EXTI_PinConfig(EXTI_EINT2, EXTI_PULLDOWN);
    EXTI_PinConfig(EXTI_EINT3, EXTI_PULLDOWN);

    // 3. Flanco de SUBIDA: detecta el momento en que el pin salta a 3.3V al presionar.
    extiCfg.mode = EXTI_EDGE_SENSITIVE;
    extiCfg.polarity = EXTI_RISING_EDGE;       // ¡Cambio clave!

    // 4. Aplicar configuración y habilitar (Limpia banderas y activa NVIC)
    extiCfg.line = EXTI_EINT0; EXTI_ConfigEnable(&extiCfg);
    extiCfg.line = EXTI_EINT1; EXTI_ConfigEnable(&extiCfg);
    extiCfg.line = EXTI_EINT2; EXTI_ConfigEnable(&extiCfg);
    extiCfg.line = EXTI_EINT3; EXTI_ConfigEnable(&extiCfg);

    // CRÍTICO: Habilitar en el NVIC
    NVIC_EnableIRQ(EINT0_IRQn);
    NVIC_EnableIRQ(EINT1_IRQn);
    NVIC_EnableIRQ(EINT2_IRQn);
    NVIC_EnableIRQ(EINT3_IRQn);
}

void configLED(void){
    // LED FAST  -> P2.8  (libre, fuera del rango 0-7 del display)
    // LED SLOW  -> P2.7  (libre)
    // LED CALIB -> P0.28 (SCL0: forzar a GPIO FUNC_00, SIN open-drain)
    PINSEL_CFG_T C, D;

    // Configuración general para Puerto 2 y Puerto 0
    C.port = 2; C.func = PINSEL_FUNC_00; C.mode = PINSEL_PULLDOWN; C.openDrain = DISABLE;
    D.port = 1; D.func = PINSEL_FUNC_00; D.mode = PINSEL_PULLDOWN; D.openDrain = DISABLE;

    // Configurar pines de los LEDs FAST y SLOW en el Puerto 2
    C.pin = 8;  PINSEL_ConfigPin(&C); // LED FAST en P2.8
    C.pin = 7;  PINSEL_ConfigPin(&C); // LED SLOW en P2.7

    D.pin = 31; PINSEL_ConfigPin(&D); // LED CALIB en P1.31 como GPIO

    // Establecer dirección como SALIDA (1)
    GPIO_SetDir(2, (1U << 7) | (1U << 8), 1); // Puerto 2: Pines 7 y 8
    GPIO_SetDir(1, (1U << 31), 1);            // Puerto 0: Pin 28

    // Estado inicial: APAGADOS
    GPIO_ClearPins(2, (1U << 7) | (1U << 8));
    GPIO_ClearPins(1, (1U << 31));
}

void EINT0_IRQHandler(void){
    static uint32_t last_0 = 0;
    if ((ticks_20ms - last_0) > 17) { // 340ms de antirrebote
        last_0 = ticks_20ms;

        curveMode = !curveMode; // Alternar curva A/C

        // Reset de estados para evitar "clics" matemáticos en los filtros
        for(int i=0; i<3; i++) for(int j=0; j<4; j++) estados_A[i][j] = 0;
        for(int i=0; i<2; i++) for(int j=0; j<4; j++) estados_C[i][j] = 0;
    }
    EXTI_ClearFlag(0);
}

void EINT1_IRQHandler(void){
    static uint32_t last_1 = 0;
    if ((ticks_20ms - last_1) > 17) {
        last_1 = ticks_20ms;
        adqMode = !adqMode;
        actualizarAlpha();

        // LED FAST en P2.8, LED SLOW en P0.7
        if(adqMode == 0) { // FAST
            GPIO_SetPins(2, (1U << 8));   // Encender LED FAST
            GPIO_ClearPins(2, (1U << 7)); // Apagar  LED SLOW
        } else { // SLOW
            GPIO_ClearPins(2, (1U << 8)); // Apagar  LED FAST
            GPIO_SetPins(2, (1U << 7));   // Encender LED SLOW
        }
    }
    EXTI_ClearFlag(1);
}

void EINT2_IRQHandler(void){
	static uint32_t last_2 = 0;
	    if ((ticks_20ms - last_2) > 17) {
	        last_2 = ticks_20ms;

	        // Solo iniciamos si NO estamos calibrando actualmente
	        if (calibMode == 0) {
	            calibMode = 1;
	            GPIO_SetPins(1, (1U << 31));
	        }
	    }
	    EXTI_ClearFlag(2);
    }

void EINT3_IRQHandler(void){
    static uint32_t last_3 = 0;
    if ((ticks_20ms - last_3) > 17) {
        last_3 = ticks_20ms;

        running = !running; // Iniciar/Frenar lecturas

        // Si acabamos de APAGAR el sistema, seteamos los defaults:
        if (!running) {
            curveMode = 0; // Curva A
            adqMode = 0;   // FAST
            actualizarAlpha();

            // === APAGAR CALIBRACIÓN Y SU LED ===
			calibMode = 0;                   // Forzar salida del modo calibración
			GPIO_ClearPins(PORT_0, (1<<31)); // Apagar el pin físicamente
			GPIO_ClearPins(2, (1U << 8)); // Apaga FAST (P2.8)
			GPIO_ClearPins(2, (1U << 7)); // Apaga SLOW (P0.4)

            // Apagar la tira NeoPixel
            for(int i=0; i<NUM_LEDS; i++) Set_LED_Color(i, 0, 0, 0);
            enviar_NeoPixel_Rapido();
			PDdisplay = 0;
			PEdisplay = 0;
            GPIO_SetPins(0, (1<<5) | (1<<10) | (1<<11));
            GPIO_SetPins(2, 127);

            // Servo a 0° (El mínimo ruido en tu calibración es 13 dB)
            moverServoPorDb(13);
        }
    }
    EXTI_ClearFlag(3);
}

void TIMER2_IRQHandler(void){ // Displays y UART
    // 1. Limpiar interrupción en el registro nativo (Bit 1 = MR1)
    LPC_TIM2->IR = (1 << 1);

    //Apagar todo (Displays y segmentos)
    GPIO_SetPins(0, (1<<5) | (1<<10) | (1<<11));

    //P2.7 y P2.8.
    GPIO_SetPins(2, 127);

    // Detiene el micro unos poquitos microsegundos para que los transistores se apaguen 100%
    for(volatile int delay = 0; delay < 200; delay++);

    //Decidir qué dibujar
    uint8_t dato_a_mostrar = 0;
    if(c == 0){
        dato_a_mostrar = DecADisp(PDdisplay); // Decenas
    } else if(c == 1){
        dato_a_mostrar = DecADisp(PEdisplay); // Unidades
    } else if(c == 2){
        dato_a_mostrar = curveMode ? 198 : 136; // Curva
    }

    // 5. Encender los segmentos
    //
    GPIO_ClearPins(2, (~dato_a_mostrar) & 127);

    // 6. Encender SOLO el transistor actual
    if(c == 0){
        GPIO_ClearPins(0, (1<<5));
    } else if(c == 1){
        GPIO_ClearPins(0, (1<<10));
    } else if(c == 2){
        GPIO_ClearPins(0, (1<<11));
    }

    // 7. Avanzar turno
    c++;
    if(c > 2){
        c = 0;
    }

    // --------------------------------------------------------------- Parte UART

    if (flag_tx_listo && dato_dB_nuevo) {
        char mensaje_tx[TX_SIZE];
        char modo_curva = (curveMode == 0) ? 'A' : 'C';
        char modo_adq   = (adqMode == 0) ? 'F' : 'S';

        sprintf(mensaje_tx, "%.1fdB%c%c\r\n", nivel_dB_mostrar, modo_curva, modo_adq);
        flag_tx_listo = 0;
        dato_dB_nuevo = 0;
        EnviarString_DMA(mensaje_tx);
    }
}

void configPIN_disp(void){
    PINSEL_CFG_T C, D;

    C.port = 2; C.func = 0; C.mode = 0; C.openDrain = 0;
    D.port = 0; D.func = 0; D.mode = 0; D.openDrain = 0;

    PINSEL_ConfigMultiplePins(&C, 255);

    //(Transistores)
    D.pin = 5;  PINSEL_ConfigPin(&D);
    D.pin = 10; PINSEL_ConfigPin(&D);
    D.pin = 11; PINSEL_ConfigPin(&D);

    // Salida
    GPIO_SetDir(2, 255, 1);
    GPIO_SetDir(0, (1<<5) | (1<<10) | (1<<11), 1);

    // APAGADO inicio
    GPIO_SetPins(0, (1<<5) | (1<<10) | (1<<11));
    GPIO_SetPins(2, 255);
}

uint8_t DecADisp(uint8_t a){ //tabla
    switch(a){
        case 0: return 192;
        case 1: return 249;
        case 2: return 164;
        case 3: return 176;
        case 4: return 153;
        case 5: return 146;
        case 6: return 130;
        case 7: return 248;
        case 8: return 128;
        case 9: return 144;
        default: return 134;// E de Error
    }
}

void configTMR2(void){ //Intentamos mejorar mux
    // Configuración directa a registros
    LPC_SC->PCONP |= (1 << 22); // Dar energía al Timer 2
    LPC_TIM2->TCR = 0x02;       // Resetear el Timer

    // Prescaler: 1 tick = 1 us (asumiendo reloj de periféricos PCLK de 25MHz)
    LPC_TIM2->PR  = 24;

    // Match: interrumpir cada 2000 us (2 milisegundos) -> Refresco de 166 Hz
    LPC_TIM2->MR1 = 2000;

    // MCR: Activa Interrupción (bit 3) y Reseteo (bit 4) automático al llegar a MR1
    LPC_TIM2->MCR = (3 << 3);
    NVIC_EnableIRQ(TIMER2_IRQn);
    LPC_TIM2->TCR = 0x01;       // Arrancar el Timer
}

void turnOffButtons(void){
	NVIC_DisableIRQ(EINT0_IRQn);
	NVIC_DisableIRQ(EINT1_IRQn);
	NVIC_DisableIRQ(EINT2_IRQn);
	NVIC_DisableIRQ(EINT3_IRQn);
}

void turnOnButtons(void){
    EXTI_ClearFlag(0);
    EXTI_ClearFlag(1);
    EXTI_ClearFlag(2);
    EXTI_ClearFlag(3);

    NVIC_EnableIRQ(EINT0_IRQn);
    NVIC_EnableIRQ(EINT1_IRQn);
    NVIC_EnableIRQ(EINT2_IRQn);
    NVIC_EnableIRQ(EINT3_IRQn);
}

void Configurar_UART0(void) {
    UART_PinConfig(UART_TX0_P0_2);
    UART_PinConfig(UART_RX0_P0_3);

    UART_CFG_T uartCfg;
    uartCfg.baudRate = 115200;
    uartCfg.dataBits = UART_DBITS_8;
    uartCfg.parity   = UART_PARITY_NONE;
    uartCfg.stopBits = UART_STOPBIT_1;
    UART_Init(UART0, &uartCfg);

    UART_FIFO_CFG_T fifoCfg;
    fifoCfg.resetTxBuf = ENABLE;
    fifoCfg.resetRxBuf = ENABLE;
    fifoCfg.level      = UART_FIFO_TRGLEV0;
    fifoCfg.dmaMode    = ENABLE;
    UART_FIFOConfig(UART0, &fifoCfg);

    UART_TxEnable(UART0);
}

// Transmicion dinamica por DMA uart
// GPDMA_SetupChannel() devuelve ERROR si el canal sigue
// habilitado (DMACCConfig_E=1). Se llama primero a
// GPDMA_ChannelStop()
void EnviarString_DMA(char* texto) {
    strncpy((char*)uart_tx_buffer, texto, TX_SIZE - 1);
    uart_tx_buffer[TX_SIZE - 1] = '\0';

    uint32_t longitud_mensaje = strlen((char*)uart_tx_buffer);

    GPDMA_Channel_CFG_T dmaTxCfg;
    dmaTxCfg.channelNum   = GPDMA_CH_3;
    dmaTxCfg.transferSize = longitud_mensaje;
    dmaTxCfg.type         = GPDMA_M2P;
    dmaTxCfg.srcMemAddr   = (uint32_t)uart_tx_buffer;
    dmaTxCfg.dstConn      = GPDMA_UART0_Tx;
    dmaTxCfg.srcConn      = GPDMA_UART0_Tx; // No usado en M2P

    dmaTxCfg.src.width     = GPDMA_BYTE;
    dmaTxCfg.src.burst     = GPDMA_BSIZE_1;
    dmaTxCfg.src.increment = ENABLE;

    dmaTxCfg.dst.width     = GPDMA_BYTE;
    dmaTxCfg.dst.burst     = GPDMA_BSIZE_1;
    dmaTxCfg.dst.increment = DISABLE;

    dmaTxCfg.intTC         = ENABLE;
    dmaTxCfg.intErr        = ENABLE;
    dmaTxCfg.linkedList    = 0;

    // Asegurar que el canal esté libre antes de reconfigurar
    GPDMA_ChannelStop(GPDMA_CH_3);

    GPDMA_SetupChannel(&dmaTxCfg);
    GPDMA_ChannelStart(GPDMA_CH_3);
}

// Config canal de recepción
void ConfigurarRX_DMA(void) {
    GPDMA_Channel_CFG_T dmaRxCfg;

    dmaRxCfg.channelNum   = GPDMA_CH_4;
    dmaRxCfg.transferSize = RX_SIZE;
    dmaRxCfg.type         = GPDMA_P2M;
    dmaRxCfg.srcConn      = GPDMA_UART0_Rx;
    dmaRxCfg.dstConn      = GPDMA_UART0_Rx; // Por seguridad
    dmaRxCfg.dstMemAddr   = (uint32_t)uart_rx_buffer;

    dmaRxCfg.src.width     = GPDMA_BYTE;
    dmaRxCfg.src.burst     = GPDMA_BSIZE_1;
    dmaRxCfg.src.increment = DISABLE;

    dmaRxCfg.dst.width     = GPDMA_BYTE;
    dmaRxCfg.dst.burst     = GPDMA_BSIZE_1;
    dmaRxCfg.dst.increment = ENABLE;

    dmaRxCfg.intTC      = ENABLE;
    dmaRxCfg.intErr     = ENABLE;
    dmaRxCfg.linkedList = 0;

    GPDMA_ChannelStop(GPDMA_CH_4);

    GPDMA_SetupChannel(&dmaRxCfg);

    GPDMA_ChannelStart(GPDMA_CH_4);
}

void apagar_DAC_pin(void){
    PINSEL_CFG_T pinCfg;

    pinCfg.func = PINSEL_FUNC_00;
    pinCfg.openDrain = DISABLE;
    pinCfg.mode = PINSEL_TRISTATE;
    pinCfg.pin = 26; // El pin del DAC es el 26
    pinCfg.port = 0; // Del puerto 0
    PINSEL_ConfigPin(&pinCfg);

    GPIO_SetDir(0, (1<<26), 1);

    GPIO_ClearPins(0, (1<<26));
}

void encender_DAC_pin(void){
    PINSEL_CFG_T pinCfg;

    pinCfg.func = PINSEL_FUNC_10;
    pinCfg.openDrain = DISABLE;
    pinCfg.mode = PINSEL_TRISTATE;
    pinCfg.pin = 26;
    pinCfg.port = 0;
    PINSEL_ConfigPin(&pinCfg);

}
