#include <string.h>
#include <stdint.h>
#include "osdp.h"
#include "ccitt_crc16.h"
#include "../../device/Include/K1921VG015.h"
#include "../../device/Include/retarget.h"
#include "../../plib/inc/plib015_gpio.h"
#include "../config/config.h"

#define OSDP_SOM 0x53
#define OSDP_HEADER_LEN 8

static void set_uart_baud(uint32_t baud); /* forward: определяется ниже */

// Текущий адрес в RAM
static uint8_t g_addr;
static uint32_t g_baud;

static void osdp_load_addr_baud(void)
{
	// Всегда задаём значения по умолчанию — иначе при неудачной загрузке
	// из EEPROM g_addr и g_baud остаются 0 и устройство не отвечает на POLL
	g_addr = 0x01;
	g_baud = 115200;

	config_storage_t cfg;
	if (config_storage_load(&cfg)) {
		if (cfg.osdp_addr > 0 && cfg.osdp_addr <= 0x7F) {
			g_addr = cfg.osdp_addr;
		}
		if (cfg.osdp_baud >= 9600 && cfg.osdp_baud <= 1000000) {
			g_baud = cfg.osdp_baud;
		}
	}
	// ВАЖНО: здесь baud только сохраняем в RAM / cfg.
	// Реальную смену скорости UART делаем ТОЛЬКО по команде osdp_COMSET,
	// чтобы не ломать консольный вывод при старте.
}

typedef enum {
	st_wait_som = 0,
	st_wait_addr,
	st_wait_len_l,
	st_wait_len_m,
	st_receive_bytes
} rx_state_t;

static rx_state_t rx_state = st_wait_som;
static uint16_t   rx_expected_len = 0;
static uint16_t   rx_pos = 0;
static uint8_t    rx_buf[64];

static void osdp_send_blocking(const uint8_t *data, uint16_t len)
{
	for (uint16_t i = 0; i < len; i++) {
		while (RETARGET_UART->FR_bit.TXFF) { }
		RETARGET_UART->DR_bit.DATA = data[i];
	}
	while (!RETARGET_UART->FR_bit.TXFE) { }
}

// Вспомогательная функция для записи заголовка ответа.
static uint16_t osdp_build_header(uint8_t *tx, uint16_t dlen, uint8_t seq)
{
	uint16_t i = 0;
	tx[i++] = OSDP_SOM;
	tx[i++] = (uint8_t)(g_addr | 0x80);
	tx[i++] = (uint8_t)(dlen & 0xFF);
	tx[i++] = (uint8_t)((dlen >> 8) & 0xFF);
	tx[i++] = (uint8_t)((seq & 0x03) | 0x04);
	return i;
}

// Универсальное завершение кадра: добавить CRC и отправить
static void osdp_build_crc_and_send(uint8_t *tx, uint16_t dlen)
{
	uint16_t crc = ccitt_crc16_calc(OSDP_INIT_CRC16, tx, dlen);
	tx[dlen++] = (uint8_t)(crc & 0xFF);
	tx[dlen++] = (uint8_t)((crc >> 8) & 0xFF);
	osdp_send_blocking(tx, dlen);
}

static void osdp_build_and_send_ack(uint8_t seq)
{
	uint8_t tx[16];
	uint16_t dlen = OSDP_HEADER_LEN; // заголовок(5) + ответ(1) + crc(2)
	uint16_t i = 0;
	// Заголовок
	i = osdp_build_header(tx, dlen, seq);
	// Ответ
	tx[i++] = osdp_ACK;
	// CRC + отправка
	osdp_build_crc_and_send(tx, i);
}

static void osdp_build_and_send_nak(uint8_t seq, uint8_t reason)
{
	uint8_t tx[16];
	uint16_t dlen = (uint16_t)(OSDP_HEADER_LEN + 1); // +1 байт причины
	uint16_t i = 0;
	i = osdp_build_header(tx, dlen, seq);
	tx[i++] = osdp_NAK;
	tx[i++] = reason; // причина NAK
	osdp_build_crc_and_send(tx, i);
}

static void osdp_build_and_send_pdid(uint8_t seq)
{
	uint8_t tx[64];
	uint16_t i = 0;
	uint8_t pdid[12] = {
		'P','R','S',             // условный вендор
		1,                       // модель
		1,                       // версия
		0x01,0x00,0x00,0x00,     // серийный
		1,1,1                    
	};
	uint16_t dlen = (uint16_t)(OSDP_HEADER_LEN + sizeof(pdid));
	i = osdp_build_header(tx, dlen, seq);
	tx[i++] = osdp_PDID;
	memcpy(&tx[i], pdid, sizeof(pdid));             // копируем данные в буфер начиная с позиции i, т.е. после заголовка и ctrl
	i += (uint16_t)sizeof(pdid);
	osdp_build_crc_and_send(tx, i);
}

static void osdp_build_and_send_pdcap(uint8_t seq)
{
	uint8_t tx[64];
	uint16_t i = 0;
	// Включены коды 0x01..0x10; неподдерживаемые помечены CL=0 и Number=0.
	uint8_t caps[] = {
		0x01, 0x01, 0x04, // 1  Inputs: CL=01, Number=2
		0x02, 0x01, 0x04, // 2  Outputs: CL=01, Number=2
		0x03, 0x00, 0x00, // 3  Card data format: not supported
		0x04, 0x01, 0x01, // 4  Reader LED control: CL=01, Number=1
		0x05, 0x00, 0x00, // 5  Reader audible output: not supported
		0x06, 0x00, 0x00, // 6  Reader text output: not supported
		0x07, 0x00, 0x00, // 7  Time keeping: not supported
		0x08, 0x01, 0x00, // 8  Check character support: CRC-16
		0x09, 0x00, 0x00, // 9  Communication security: not supported
		0x0A, 0x40, 0x00, // 10 Receive buffer size: 0x0040 (64)
		0x0B, 0x40, 0x00, // 11 Largest combined message size: 0x0040 (64)
		0x0C, 0x00, 0x00, // 12 Smart card support: not supported
		0x0D, 0x00, 0x00, // 13 Readers: none
		0x0E, 0x00, 0x00, // 14 Biometrics: none
		0x0F, 0x00, 0x00, // 15 Secure PIN entry: not supported
		0x10, 0x01, 0x00  // 16 OSDP version: IEC 60839-11-5(почему-то не отображается в утилите osdp client)
	};
	uint16_t dlen = (uint16_t)(OSDP_HEADER_LEN + sizeof(caps));
	i = osdp_build_header(tx, dlen, seq);
	tx[i++] = osdp_PDCAP;
	memcpy(&tx[i], caps, sizeof(caps));
	i += (uint16_t)sizeof(caps);
	osdp_build_crc_and_send(tx, i);
}

static void osdp_build_and_send_com(uint8_t seq, uint8_t new_addr, uint32_t new_baud)
{
	uint8_t tx[16];
	uint16_t i = 0;
	uint16_t dlen = (uint16_t)(OSDP_HEADER_LEN + 5); 
	i = osdp_build_header(tx, dlen, seq);
	tx[i++] = osdp_COM;
	tx[i++] = (uint8_t)(new_addr & 0x7F);
	tx[i++] = (uint8_t)(new_baud & 0xFF);
	tx[i++] = (uint8_t)((new_baud >> 8) & 0xFF);
	tx[i++] = (uint8_t)((new_baud >> 16) & 0xFF);
	tx[i++] = (uint8_t)((new_baud >> 24) & 0xFF);
	osdp_build_crc_and_send(tx, i);

	// Сохранить новые адрес и скорость в EEPROM
	config_storage_t cfg;
	if (!config_storage_load(&cfg)) {
		config_storage_default(&cfg);
	}
	cfg.osdp_addr = (uint8_t)(new_addr & 0x7F);
	cfg.osdp_baud = new_baud;
	config_storage_save(&cfg);

	// Обновить текущие значения в RAM
	g_addr = cfg.osdp_addr;
	g_baud = cfg.osdp_baud;
}

static void set_led_state(uint8_t on)
{
	if (on) {
		GPIO_SetBits(GPIOA, GPIO_Pin_7);
		GPIO_SetBits(GPIOA, GPIO_Pin_6);
		GPIO_SetBits(GPIOA, GPIO_Pin_5);
		GPIO_SetBits(GPIOA, GPIO_Pin_4);
	} else {
		GPIO_ClearBits(GPIOA, GPIO_Pin_7);
		GPIO_ClearBits(GPIOA, GPIO_Pin_6);
		GPIO_ClearBits(GPIOA, GPIO_Pin_5);
		GPIO_ClearBits(GPIOA, GPIO_Pin_4);
	}
}

// Структура для управления состоянием выходов с таймерами
typedef struct {
	uint8_t  permanent_state;  // Постоянное состояние: 0=OFF, 1=ON
	uint8_t  temp_active;      // Активна ли временная операция (0x05 или 0x06)
	uint8_t  temp_state;       // Временное состояние: 0=OFF, 1=ON
	uint32_t timer_ms_left;    // Оставшееся время таймера в миллисекундах (0 = таймер не активен)
	uint8_t  allow_completion; // Разрешить завершение временной операции (для 0x03, 0x04)
} output_ctrl_t;

static output_ctrl_t output_ctrl[4]; // Состояние для 4 выходов (PA12-PA15)

static void osdp_build_and_send_istat(uint8_t seq)
{
	uint8_t tx[16];
	uint16_t i = 0;
	// Дополнительных данных 1 байт:
	//  - бит0 = состояние входа 0 (PA0)
	//  - бит1 = состояние входа 1 (PA1)
	//  - бит2 = состояние входа 2 (PA2)
	//  - бит3 = состояние входа 3 (PA3)
	uint16_t dlen = (uint16_t)(OSDP_HEADER_LEN + 1);
	i = osdp_build_header(tx, dlen, seq);
	tx[i++] = osdp_ISTATR;
	uint8_t inputs = 0;
	// Кнопки с PU активны уровнем 0 → инвертируем, чтобы 1 означало "замкнуто/нажато"
	inputs |= (GPIO_ReadBit(GPIOA, GPIO_Pin_0) ? 0u : 1u) << 0; 
	inputs |= (GPIO_ReadBit(GPIOA, GPIO_Pin_1) ? 0u : 1u) << 1; 
	inputs |= (GPIO_ReadBit(GPIOA, GPIO_Pin_2) ? 0u : 1u) << 2; 
	inputs |= (GPIO_ReadBit(GPIOA, GPIO_Pin_3) ? 0u : 1u) << 3; 
	tx[i++] = inputs;
	osdp_build_crc_and_send(tx, i);
}

static void osdp_build_and_send_ostat(uint8_t seq)
{
	uint8_t tx[16];
	uint16_t i = 0;
	// Дополнительных данных 1 байт:

	uint16_t dlen = (uint16_t)(OSDP_HEADER_LEN + 1);
	i = osdp_build_header(tx, dlen, seq);
	tx[i++] = osdp_OSTATR;
	uint8_t outputs = 0;
	outputs |= (GPIO_ReadBit(GPIOA, GPIO_Pin_4) ? 1u : 0u) << 0; 
	outputs |= (GPIO_ReadBit(GPIOA, GPIO_Pin_5) ? 1u : 0u) << 1; 
	outputs |= (GPIO_ReadBit(GPIOA, GPIO_Pin_6) ? 1u : 0u) << 2; 
	outputs |= (GPIO_ReadBit(GPIOA, GPIO_Pin_7) ? 1u : 0u) << 3; 
	tx[i++] = outputs;
	osdp_build_crc_and_send(tx, i);
}

// Обработка команды OUT (Output Control Command) согласно стандарту OSDP
// Формат данных: 4 байта на выход, повторяющиеся 1 или более раз:
//   - Output number (1 byte): номер выхода (0x00 = первый выход, 0x01 = второй выход и т.д.)
//   - Control code (1 byte): код управления согласно Table 14 стандарта OSDP
//   - Timer LSB (1 byte): младший байт таймера в единицах 100мс
//   - Timer MSB (1 byte): старший байт таймера (0 = навсегда)
// 
// Control code values (Table 14):
//   0x00: NOP – do not alter this output
//   0x01: set the permanent state to OFF, abort timed operation (if any)
//   0x02: set the permanent state to ON, abort timed operation (if any)
//   0x03: set the permanent state to OFF, allow timed operation to complete
//   0x04: set the permanent state to ON, allow timed operation to complete
//   0x05: set the temporary state to ON, resume permanent state on timeout
//   0x06: set the temporary state to OFF, resume permanent state on timeout
static void handle_osdp_out(uint8_t *data, uint16_t data_len)
{
	// Массив пинов для выходов: PA4, PA5, PA6, PA7
	const uint32_t output_pins[4] = {GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_6, GPIO_Pin_7};
	
	// Проверяем, что длина данных кратна 4 (стандартный формат OSDP)
	if ((data_len % 4) != 0 || data_len < 4) {
		// Неправильный формат - игнорируем
		return;
	}
	
	// Обрабатываем данные по 4 байта на каждый выход
	uint16_t count = (uint16_t)(data_len / 4u);
	for (uint16_t i = 0; i < count; i++) {
		uint8_t *p = &data[i * 4u];
		uint8_t output_num = p[0];
		uint8_t control_code = p[1];
		uint16_t timer_100ms = (uint16_t)p[2] | ((uint16_t)p[3] << 8);
		
		// Проверяем, что номер выхода в допустимом диапазоне (0-3)
		if (output_num >= 4) continue;
		
		uint32_t pin = output_pins[output_num];
		
		// Обработка команд управления выходом согласно стандарту OSDP Table 14
		switch (control_code) {
		case 0x00: // NOP – do not alter this output
			break;
			
		case 0x01: // set the permanent state to OFF, abort timed operation (if any)
			output_ctrl[output_num].permanent_state = 0;
			output_ctrl[output_num].temp_active = 0; // Прервать временную операцию
			output_ctrl[output_num].timer_ms_left = 0;
			GPIO_ClearBits(GPIOA, pin);
			break;
			
		case 0x02: // set the permanent state to ON, abort timed operation (if any)
			output_ctrl[output_num].permanent_state = 1;
			output_ctrl[output_num].temp_active = 0; // Прервать временную операцию
			output_ctrl[output_num].timer_ms_left = 0;
			GPIO_SetBits(GPIOA, pin);
			break;
			
		case 0x03: // set the permanent state to OFF, allow timed operation to complete
			output_ctrl[output_num].permanent_state = 0;
			output_ctrl[output_num].allow_completion = 1; // Разрешить завершение временной операции
			// Если временная операция не активна, сразу установить постоянное состояние
			if (!output_ctrl[output_num].temp_active) {
				GPIO_ClearBits(GPIOA, pin);
			}
			break;
			
		case 0x04: // set the permanent state to ON, allow timed operation to complete
			output_ctrl[output_num].permanent_state = 1;
			output_ctrl[output_num].allow_completion = 1; // Разрешить завершение временной операции
			// Если временная операция не активна, сразу установить постоянное состояние
			if (!output_ctrl[output_num].temp_active) {
				GPIO_SetBits(GPIOA, pin);
			}
			break;
			
		case 0x05: // set the temporary state to ON, resume permanent state on timeout
			output_ctrl[output_num].temp_active = 1;
			output_ctrl[output_num].temp_state = 1;
			output_ctrl[output_num].allow_completion = 0;
			if (timer_100ms == 0) {
				// Таймер = 0 означает "навсегда" - устанавливаем как постоянное состояние
				output_ctrl[output_num].permanent_state = 1;
				output_ctrl[output_num].temp_active = 0;
				output_ctrl[output_num].timer_ms_left = 0;
			} else {
				output_ctrl[output_num].timer_ms_left = (uint32_t)timer_100ms * 100u; // Конвертируем в миллисекунды
			}
			GPIO_SetBits(GPIOA, pin);
			break;
			
		case 0x06: // set the temporary state to OFF, resume permanent state on timeout
			output_ctrl[output_num].temp_active = 1;
			output_ctrl[output_num].temp_state = 0;
			output_ctrl[output_num].allow_completion = 0;
			if (timer_100ms == 0) {
				// Таймер = 0 означает "навсегда" - устанавливаем как постоянное состояние
				output_ctrl[output_num].permanent_state = 0;
				output_ctrl[output_num].temp_active = 0;
				output_ctrl[output_num].timer_ms_left = 0;
			} else {
				output_ctrl[output_num].timer_ms_left = (uint32_t)timer_100ms * 100u; // Конвертируем в миллисекунды
			}
			GPIO_ClearBits(GPIOA, pin);
			break;
			
		default:
			// Неизвестный код управления - игнорируем согласно стандарту
			break;
		}
	}
}

static void set_uart_baud(uint32_t baud)
{
	// Настроить делители UART4 по формуле из UART4_init()
	// Предполагаем источник тактирования HSE, как в init
	uint32_t baud_icoef = HSECLK_VAL / (16u * baud);
	float    f = (float)HSECLK_VAL / (16.0f * (float)baud) - (float)baud_icoef;
	uint32_t baud_fcoef = (uint32_t)(f * 64.0f + 0.5f);
	uint32_t cr_saved   = UART4->CR;
	uint32_t lcrh_saved = UART4->LCRH;
	uint32_t imsc_saved = UART4->IMSC;
	// Дождаться окончания текущей передачи/приёма, чтобы не обрывать ответ
	while (UART4->FR_bit.BUSY) { }
	// Остановить UART
	UART4->CR = 0;
	UART4->IBRD = baud_icoef;
	UART4->FBRD = baud_fcoef;
	// Перезаписать формат кадра и включить FIFO как в init (8N1, FIFO EN)
	UART4->LCRH = UART_LCRH_FEN_Msk | (3u << UART_LCRH_WLEN_Pos);
	// Очистить все флаги прерываний
	UART4->ICR = 0x7FF;
	while (!UART4->FR_bit.RXFE) { (void)UART4->DR_bit.DATA; }
	// Восстановить маску прерываний (на случай, если аппаратно сбросилась)
	UART4->IMSC = imsc_saved;
	// Включить обратно (TX, RX, UARTEN)
	UART4->CR = cr_saved | UART_CR_TXE_Msk | UART_CR_RXE_Msk | UART_CR_UARTEN_Msk;
}

typedef struct {
	uint8_t  temp_active;
	uint8_t  perm_state;      // 0=off, 1=on
	uint8_t  current_state;   // 0=off, 1=on
	uint32_t on_ms;
	uint32_t off_ms;
	uint16_t cycles_left;     // 0 = постоянно
	uint32_t phase_ms_left;
	// Permanent blink profile (по стандарту – постоянные времена и цвета; тут цвета игнорируем, у нас всего 1 доступный)
	uint32_t perm_on_ms;
	uint32_t perm_off_ms;
	// Отображение "цвета" в реальное состояние GPIO: 0=black(off), !=0 = on
	uint8_t  temp_on_color_is_on;
	uint8_t  temp_off_color_is_on;
	uint8_t  perm_on_color_is_on;
	uint8_t  perm_off_color_is_on;
} led_ctrl_t;

static led_ctrl_t led_ctrl;

void osdp_tick_1ms(void) // вызывается каждую мс из main.c
{
	// Обработка таймеров выходов OSDP
	const uint32_t output_pins[4] = {GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_6, GPIO_Pin_7};
	for (uint8_t i = 0; i < 4; i++) {
		if (output_ctrl[i].temp_active && output_ctrl[i].timer_ms_left > 0) {
			--output_ctrl[i].timer_ms_left;
			if (output_ctrl[i].timer_ms_left == 0) {
				// Таймер истек - возвращаем постоянное состояние
				output_ctrl[i].temp_active = 0;
				if (output_ctrl[i].permanent_state) {
					GPIO_SetBits(GPIOA, output_pins[i]);
				} else {
					GPIO_ClearBits(GPIOA, output_pins[i]);
				}
			}
		}
	}
	
	// Обработка таймеров LED
	if (led_ctrl.phase_ms_left > 0) {
		--led_ctrl.phase_ms_left;
		return;
	}

	if (led_ctrl.temp_active) {  // проверка на профиль temp/perm
		// Управление temp профилем
		if (led_ctrl.current_state) {
			// ON -> OFF
			led_ctrl.current_state = 0;
			set_led_state(led_ctrl.temp_off_color_is_on ? 1 : 0);
			led_ctrl.phase_ms_left = (led_ctrl.off_ms > 0) ? led_ctrl.off_ms : 0;
		} else {
			// OFF -> ON
			led_ctrl.current_state = 1;
			set_led_state(led_ctrl.temp_on_color_is_on ? 1 : 0);
			led_ctrl.phase_ms_left = (led_ctrl.on_ms > 0) ? led_ctrl.on_ms : 0;
			// Считаем циклы по фронту включения
			if (led_ctrl.cycles_left > 0) {
				--led_ctrl.cycles_left;
				if (led_ctrl.cycles_left == 0) {
					// Конец временного режима – восстановить постоянный
					led_ctrl.temp_active = 0;
					// Запустить постоянный профиль
					if (led_ctrl.perm_on_ms > 0 && led_ctrl.perm_off_ms > 0) {
						led_ctrl.current_state = 1;
						set_led_state(led_ctrl.perm_on_color_is_on ? 1 : 0);
						led_ctrl.phase_ms_left = led_ctrl.perm_on_ms;
					} else {
						set_led_state(led_ctrl.perm_state);
						led_ctrl.phase_ms_left = 0;
					}
				}
			}
		}
	} else {
		// Постоянный профиль: мигание, если задано perm_on/off
		if (led_ctrl.perm_on_ms > 0 && led_ctrl.perm_off_ms > 0) {
			if (led_ctrl.current_state) {
				led_ctrl.current_state = 0;
				set_led_state(led_ctrl.perm_off_color_is_on ? 1 : 0);
				led_ctrl.phase_ms_left = led_ctrl.perm_off_ms;
			} else {
				led_ctrl.current_state = 1;
				set_led_state(led_ctrl.perm_on_color_is_on ? 1 : 0);
				led_ctrl.phase_ms_left = led_ctrl.perm_on_ms;
			}
		}
	}
}

static void handle_osdp_led(uint8_t *data, uint16_t data_len)
{
	// Стандартный формат: 14 байт на одну запись (Temporary+Permanent)
	// Temporary: code, on, off, on_color, off_color, timerLSB, timerMSB
	// Permanent: code, on, off, on_color, off_color
	if (data_len < 14) return;
	uint16_t count = (uint16_t)(data_len / 14u);
	for (uint16_t rec = 0; rec < count; rec++) {     // rec - запись
		uint8_t *p = &data[rec * 14u];
		uint8_t reader = p[0];
		uint8_t lednum  = p[1];
		if (!(reader == 0 && lednum == 0)) continue; // у нас пока один LED: reader0, led0

		uint8_t tcode  = p[2];  // 0 - NOP, 1 - cancel, 2 - start now
		uint8_t tOn100ms = p[3];
		uint8_t tOff100ms = p[4];
		uint8_t tOnColor  = p[5];
		uint8_t tOffColor = p[6];
		uint16_t timer100ms = (uint16_t)p[7] | ((uint16_t)p[8] << 8);

		uint8_t pcode  = p[9];  // 0 - NOP, 1 - start now
		uint8_t pOn100ms = p[10];
		uint8_t pOff100ms= p[11];
		uint8_t pOnColor  = p[12];
		uint8_t pOffColor = p[13];

		// Обновим постоянный профиль, если требуется
		if (pcode == 0x01) {
			uint32_t pOn_ms  = (uint32_t)pOn100ms  * 100u;
			uint32_t pOff_ms = (uint32_t)pOff100ms * 100u;
			led_ctrl.perm_on_ms  = pOn_ms;
			led_ctrl.perm_off_ms = pOff_ms;
			led_ctrl.perm_on_color_is_on  = (pOnColor  != 0);
			led_ctrl.perm_off_color_is_on = (pOffColor != 0);
			if (pOn_ms > 0 && pOff_ms == 0) {
				led_ctrl.perm_state = (pOnColor != 0) ? 1 : 0; // цветом управляем "включённостью"
			} else if (pOn_ms == 0 && pOff_ms > 0) {
				led_ctrl.perm_state = (pOffColor != 0) ? 1 : 0;
			}
			// Если temp режима нет – применим сразу
			if (!led_ctrl.temp_active) {
				if (pOn_ms > 0 && pOff_ms > 0) {
					led_ctrl.current_state = 1;
					set_led_state(led_ctrl.perm_on_color_is_on ? 1 : 0);
					led_ctrl.phase_ms_left = pOn_ms;
				} else {
					set_led_state(led_ctrl.perm_state);
				}
			}
		} else if (pcode == 0x00) {
			// Not use: NOP по стандарту — не изменяем постоянные настройки
		}

		// Temporary в соответствии со стандартом
		if (tcode == 0x00) {
			// NOP – не изменяем текущий временный режим
		} else if (tcode == 0x01) {
			// выключить temp и показать permanent немедленно
			led_ctrl.temp_active = 0;
			if (led_ctrl.perm_on_ms > 0 && led_ctrl.perm_off_ms > 0) {
				led_ctrl.current_state = 1;
				set_led_state(led_ctrl.perm_on_color_is_on ? 1 : 0);
				led_ctrl.phase_ms_left = led_ctrl.perm_on_ms;
			} else {
				led_ctrl.current_state = led_ctrl.perm_state;
				set_led_state(led_ctrl.perm_state);
				led_ctrl.phase_ms_left = 0;
			}
		} else if (tcode == 0x02) {
			// Установить временный режим немедленно и запустить таймер
			uint32_t on_ms  = (uint32_t)tOn100ms  * 100u;
			uint32_t off_ms = (uint32_t)tOff100ms  * 100u;
			led_ctrl.on_ms  = (on_ms  > 0) ? on_ms  : 0; //защита от отрицательных значений
			led_ctrl.off_ms = (off_ms > 0) ? off_ms : 0; 
			led_ctrl.temp_on_color_is_on  = (tOnColor  != 0);
			led_ctrl.temp_off_color_is_on = (tOffColor != 0);
			// Рассчитать количество циклов из таймера, если задан
			if (timer100ms == 0) {
				led_ctrl.cycles_left = 0; // бесконечно
			} else {
				uint32_t period = (led_ctrl.on_ms + led_ctrl.off_ms);
				if (period == 0) period = 100; // защита от деления на 0
				uint32_t total_ms = (uint32_t)timer100ms * 100u;
				uint32_t cycles = total_ms / period;
				if (cycles == 0) cycles = 1;
				led_ctrl.cycles_left = (uint16_t)((cycles > 0xFFFFu) ? 0xFFFFu : cycles);
			}
			// Стартовая фаза – включение, если on_ms > 0, иначе – выключение
			if (led_ctrl.on_ms > 0) {
				led_ctrl.current_state = 1;
				set_led_state(led_ctrl.temp_on_color_is_on ? 1 : 0);
				led_ctrl.phase_ms_left = led_ctrl.on_ms;
			} else {
				led_ctrl.current_state = 0;
				set_led_state(led_ctrl.temp_off_color_is_on ? 1 : 0);
				led_ctrl.phase_ms_left = led_ctrl.off_ms;
			}
			led_ctrl.temp_active = 1;
		}
	}
}


void osdp_init(void)
{
	osdp_load_addr_baud();
	rx_state = st_wait_som;
	rx_expected_len = 0;
	rx_pos = 0;
	
	// Инициализация состояния выходов
	for (uint8_t i = 0; i < 4; i++) {
		output_ctrl[i].permanent_state = 0; // По умолчанию все выходы выключены
		output_ctrl[i].temp_active = 0;
		output_ctrl[i].temp_state = 0;
		output_ctrl[i].timer_ms_left = 0;
		output_ctrl[i].allow_completion = 0;
	}
}

void osdp_on_rx_byte(uint8_t byte) // парсер входящих байтов
{
	switch (rx_state) {
	case st_wait_som:
		if (byte == OSDP_SOM) {
			rx_pos = 0;
			rx_buf[rx_pos++] = byte;
			rx_state = st_wait_addr;
		}
		break;
	case st_wait_addr:
		rx_buf[rx_pos++] = byte;
		rx_state = st_wait_len_l;
		break;
	case st_wait_len_l:
		rx_buf[rx_pos++] = byte;
		rx_expected_len = byte;
		rx_state = st_wait_len_m;
		break;
	case st_wait_len_m:
		rx_buf[rx_pos++] = byte;
		rx_expected_len |= ((uint16_t)byte << 8);
		if (rx_expected_len < 8 || rx_expected_len > sizeof(rx_buf)) {
			// некорректная длина — сброс
			rx_state = st_wait_som;
			break;
		}
		rx_state = st_receive_bytes;
		break;
	case st_receive_bytes:
		rx_buf[rx_pos++] = byte;
		if (rx_pos >= rx_expected_len) {
			// Проверка CRC
			if (osdp_crc_is_ok(rx_buf, rx_expected_len)) {
				uint8_t addr = (uint8_t)(rx_buf[1] & 0x7F);
				if (addr == g_addr || addr == 0x7F || addr == 0x00) {
					uint8_t ctrl = rx_buf[4];
					uint8_t seq = (uint8_t)(ctrl & 0x03);
					uint8_t cmd = rx_buf[5];
					// На широковещательный (0x7F) отвечать нельзя
					char should_reply = (addr != 0x7F);
					switch (cmd) {
					case osdp_POLL:
						if (should_reply) osdp_build_and_send_ack(seq);
						break;
					case osdp_ID:
						if (should_reply) osdp_build_and_send_pdid(seq);
						break;
					case osdp_CAP:
						if (should_reply) osdp_build_and_send_pdcap(seq);
						break;
					case osdp_ISTAT:
						if (should_reply) osdp_build_and_send_istat(seq);
						break;
					case osdp_OSTAT:
						if (should_reply) osdp_build_and_send_ostat(seq);
						break;
					case osdp_COMSET: {
						// Ожидаем 5 байт данных: [addr][baud L][baud H][baud HH][baud HHH]
						uint16_t data_len = (uint16_t)(rx_expected_len - 8);
						if (data_len == 5) {
							uint8_t *data = &rx_buf[6];
							uint8_t new_addr = (uint8_t)(data[0] & 0x7F);
							uint32_t new_baud = (uint32_t)data[1] |
							                    ((uint32_t)data[2] << 8) |
							                    ((uint32_t)data[3] << 16) |
							                    ((uint32_t)data[4] << 24);
							// Отправим ответ osdp_COM с теми же параметрами
							if (should_reply) osdp_build_and_send_com(seq, new_addr, new_baud);
							// Применим локально: адрес и скорость UART
							g_addr = new_addr;
							if (new_baud >= 1200 && new_baud <= 921600) {
								set_uart_baud(new_baud);
							}
						} else {
							// Неправильная длина — NAK (reason 0x02: invalid length)
							if (should_reply) osdp_build_and_send_nak(seq, 0x02);
						}
						break;
					}
					case osdp_LED: {
						uint16_t data_len = (uint16_t)(rx_expected_len - 8);
						uint8_t *data = &rx_buf[6];
						handle_osdp_led(data, data_len);
						if (should_reply) osdp_build_and_send_ack(seq);
						break;
					}
					case osdp_OUT: {
						uint16_t data_len = (uint16_t)(rx_expected_len - 8);
						uint8_t *data = &rx_buf[6];
						handle_osdp_out(data, data_len);
						if (should_reply) osdp_build_and_send_ack(seq);
						break;
					}
					case osdp_MFG: {
						// Команда производителя (0x80).
						// На данном этапе просто подтверждаем любую такую команду ACK,
						// не проверяя вендора и не разбирая данные.
						if (should_reply) {
							osdp_build_and_send_ack(seq);
						}
						break;
					}
					default:
						// Неизвестная команда — NAK (reason 0x03: unknown command)
						if (should_reply) osdp_build_and_send_nak(seq, 0x03);
						break;
					}
				}
			} else {
				// Плохая CRC — NAK (reason 0x01), если это не широковещательный адрес
				uint8_t addr = (uint8_t)(rx_buf[1] & 0x7F);
				if (addr != 0x7F) {
					uint8_t ctrl = rx_buf[4];
					uint8_t seq = (uint8_t)(ctrl & 0x03);
					osdp_build_and_send_nak(seq, 0x01);
				}
			}
			// Готовы к следующему пакету
			rx_state = st_wait_som;
		}
		break;
	default:
		rx_state = st_wait_som;
		break;
	}
}
