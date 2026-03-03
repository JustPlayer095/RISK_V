#include <k1921vg015_hwlibs/k1921vg015.h>
#include <k1921vg015_hwlibs/csr.h>

#include <runtime.h>
#include <stdbool.h>
#include <string.h>


// системная частота выбрана равной 44236800 Гц
// задаём системные частоты
uint32_t g_hse_clock = HSE_CLOCK_FREQ;
uint32_t g_pll_clock_1 = PLL_CLOCK_FREQ0;
uint32_t g_pll_clock_2 = PLL_CLOCK_FREQ1;

// скорость последовательного порта 460800 бит/в секунду
#define SERIAL_PORT_SPEED 460800u

// сколько доступно флэш-памяти для прошивки
#define ONBOARD_FLASH_SIZE	    (1*1024*1024)
// стирание секторами размером 4K
#define FLASH_SECTOR_4K         4096
#define FLASH_PAGE_16           16
// размер загрузчика в секторах
#define LOADER_SIZE_BY_4K       2
// адрес, с которого загружается прошивка, т.е. доступно 1020 КиБ флэша, первая страница отведена под загрузчик
#define APP_START_ADDR          (MEM_FLASH_BASE + (LOADER_SIZE_BY_4K*FLASH_SECTOR_4K))
#define AVAILABLE_FLASH_SIZE    (ONBOARD_FLASH_SIZE - (LOADER_SIZE_BY_4K*FLASH_SECTOR_4K))

#define REPLY_WAITING                 1
#define REPLY_ACK                     2
// коды ошибок, идут не подряд, так как исходно являются подмножеством кодов ошибок
// загрузчика для MIK32/Амур. но ничего не мешает сделать подряд идущие числа :)
#define REPLY_ERR_TOO_BIG             3
#define REPLY_ERR_RECEIVE             4
#define REPLY_ERR_CRC32               6
#define REPLY_ERR_WAIT_WRITE_PAGE     7
#define REPLY_ERR_WAIT_ERASE_PAGE     11


// конфиги для всех для каналов DMA
__attribute__((aligned(0x400))) DMA_CtrlData_TypeDef g_dma_channels_data;
// два буфера для приёма данных прошивки, размером с сектор
uint8_t g_recv_buf_1[FLASH_SECTOR_4K], g_recv_buf_2[FLASH_SECTOR_4K];


// настройка оборудования, вызывается из кода ../libbaremetal/src/libc/runtime.c до вызова main()
void process_init_mcu() {
  // указатель на управляющие структуры каналов DMA
  DMA->BASEPTR = (uint32_t)&g_dma_channels_data;
  // UART0_RX - PA0, UART0_TX - PA1
  // включаем тактирование GPIOA
  RCU->CGCFGAHB_bit.GPIOAEN = 1;
  // включение GPIOB
  RCU->RSTDISAHB_bit.GPIOAEN = 1;
  // отключаем "подтяжку" у ног PA0, PA1
  GPIOA->PULLMODE &= ~(GPIO_PULLMODE_PIN0_Msk | GPIO_PULLMODE_PIN1_Msk);
  // режим выхода push-pull
  GPIOA->OUTMODE_bit.PIN1 = GPIO_OUTMODE_PIN_PP;
  // альтернативная функция выводов PA0, PA1
  GPIOA->ALTFUNCNUM = (GPIOA->ALTFUNCNUM & ~(GPIO_ALTFUNCNUM_PIN_Msk(0) | GPIO_ALTFUNCNUM_PIN_Msk(1)))
                    | GPIO_ALTFUNCNUM_PIN(0, GPIO_ALTFUNCNUM_PIN_AF1)
                    | GPIO_ALTFUNCNUM_PIN(1, GPIO_ALTFUNCNUM_PIN_AF1)
                    ;
  // включаем альтернативную функцию
  GPIOA->ALTFUNCSET = (GPIO_PIN_MASK(0) | GPIO_PIN_MASK(1));
  
  // на PC0 "кнопка"
  // включаем тактирование GPIOC
  RCU->CGCFGAHB_bit.GPIOCEN = 1;
  // включение GPIOC
  RCU->RSTDISAHB_bit.GPIOCEN = 1;
  // включаем "подтяжку" у ноги PC0
  GPIOC->PULLMODE_bit.PIN0 = 1;
  // выключение альтернативной функции
  GPIOC->ALTFUNCCLR = GPIO_PIN_MASK(0);
  // отключение управлением выходом
  GPIOC->OUTENCLR = GPIO_PIN_MASK(0);
  
  // UART0
  // включаем тактирование UART0
  RCU->CGCFGAPB_bit.UART0EN = 1;
  // включение UART0
  RCU->RSTDISAPB_bit.UART0EN = 1;
  // сброс UART0
  RCU->UARTCLKCFG[0].UARTCLKCFG = RCU_UARTCLKCFG_CLKEN_Msk | (RCU_UARTCLKCFG_CLKSEL_PLL0 << RCU_UARTCLKCFG_CLKSEL_Pos);
  UART0->RSR; // пауза
  RCU->UARTCLKCFG[0].UARTCLKCFG_bit.RSTDIS = 1; // снимаем сброс с UART0
  // настройка делителя
  uint32_t vIBRD = g_systemclock / (16u * SERIAL_PORT_SPEED);
  UART0->FBRD = ((g_systemclock - (vIBRD * 16u * SERIAL_PORT_SPEED)) << 6) / (16u * SERIAL_PORT_SPEED);
  UART0->IBRD = vIBRD;
  // включение FIFO и размер передаваемого слова
  UART0->LCRH = UART_LCRH_FEN_Msk | (UART_LCRH_WLEN_8bit << UART_LCRH_WLEN_Pos);
  //UART0->LCRH = (UART_LCRH_WLEN_8bit << UART_LCRH_WLEN_Pos);
  // запускаем UART
  UART0->CR = (UART_CR_RXE_Msk | UART_CR_TXE_Msk | UART_CR_UARTEN_Msk);
  
  // настроим CRC0 (CRC32/POSIX)
  RCU->CGCFGAHB_bit.CRC0EN = 1;
  RCU->RSTDISAHB_bit.CRC0EN = 1;
  CRC0->POL = 0x04C11DB7ul;
  CRC0->INIT = 0;
  CRC0->CR = (CRC_CR_REV_IN_Disable << CRC_CR_REV_IN_Pos)
           | (CRC_CR_REV_OUT_Disable << CRC_CR_REV_OUT_Pos)
           | (CRC_CR_XOROUT_Enable << CRC_CR_XOROUT_Pos)
           | (CRC_CR_MODE_StandartCRC << CRC_CR_MODE_Pos)
           | (CRC_CR_POLYSIZE_POL32 << CRC_CR_POLYSIZE_Pos)
           ;

  // "сброс" контроллера DMA
  DMA->CFG = 0;
  DMA->ERRCLR = 1;
  DMA->ENCLR = 0x00FFFFFFul;
  DMA->REQMASKCLR = 0x00FFFFFFul;
  DMA->IRQSTATCLR = 0x00FFFFFFul;
  DMA->PRIALTCLR = 0x00FFFFFFul;
  // включаем контроллер DMA
  DMA->CFG = DMA_CFG_MASTEREN_Msk;

  // разрешаем прерывания вообще (у нас системный таймер миллисекунды отсчитывает)
  set_csr(mstatus, MSTATUS_MIE);
}


// отправить байт через UART0
void local_putc( uint8_t a_byte ) {
  // пока писАть запрощено, ожидаем
  while ( 0 == UART0->FR_bit.TXFE ) {}
  // появилось место - записываем байт
  UART0->DR = a_byte;
}

// получить байты из UART0 с указанным таймаутом
bool local_gets( uint32_t a_timeout_ms, uint8_t * a_dst, int a_len ) {
  uint32_t v_from = g_milliseconds;
  for (;;) {
    // пока есть чего читать
    while ( a_len > 0 && 0 == UART0->FR_bit.RXFE ) {
      // читаем
      *a_dst++ = UART0->DR;
      // уменьшаем счётчик
      --a_len;
    }
    // всё прочитали?
    if ( 0 == a_len ) {
      // на выход из цикла чтения
      break;
    }
    // если время вышло - вернём признак ошибки
    if ( (g_milliseconds - v_from) >= a_timeout_ms ) {
      return false;
    }
  }
  // всё прочитано вовремя, возвращаем признак успешного выполнения
  return true;
}


// прототип функции запуска загруженной прошивки
void go_to_application();
// обновление "прошивки" во флэш-памяти
bool firmware_update();


// точка входа
void main() {
  //
  for (;;) {
    // если "прошивка" загружена
    if ( firmware_update() ) {
      // запускаем её
      go_to_application();
    }
  }
}


// naked убирает код пролога/эпилога функции, что экономит несколько байтов
__attribute__((naked))
void go_to_application() {
  // отключение UART0
  RCU->UARTCLKCFG[0].UARTCLKCFG = 0;
  // сброс и отключение блоков
  RCU->RSTDISAHB = 0;
  RCU->CGCFGAHB = 0;
  // отключение прерываний от системного таймера
  clear_csr(mie, MIE_MTIE);
  // отключение прерываний вообще
  clear_csr(mstatus, MSTATUS_MIE);
  // переход на загруженную прошивку
  asm volatile(	"la ra, " _TOSTR(APP_START_ADDR) "\n" \
                "jalr ra"             \
              );
}


//
typedef struct header_s {
  uint32_t m_size;
  uint32_t m_crc32;
} firmware_header_t;


// конфиги для чтения не более чем FLASH_SECTOR_4K байтов, один конфиг маскимум на 1024 байта
DMA_Channel_TypeDef g_dma_cfg[FLASH_SECTOR_4K/1024];


// запустить чтение не более FLASH_SECTOR_4K байтов из UART0 через DMA
uint32_t start_read_DMA_UART( uint8_t * a_dst, uint32_t * a_size_remains ) {
  // отключение канала по приёму от UART0
  DMA->ENCLR = DMA_CH_MSK_UART0RX;
  // сброс флага завершения работы канала по приёму от UART0
  DMA->IRQSTATCLR = DMA_CH_MSK_UART0RX;
  // переключения на использование "основного" конфига канала по приёму от UART0
  DMA->PRIALTCLR = DMA_CH_MSK_UART0RX;
  // сколько байтов читать
  uint32_t v_bytes_to_read = (*a_size_remains >= FLASH_SECTOR_4K ? FLASH_SECTOR_4K : *a_size_remains);
  uint32_t v_size = v_bytes_to_read;
  // в случае, когда для чтения достаточно одного конфига
  if ( v_size <= 1024 ) {
    // тогда используем Basic режим работы канала
    g_dma_channels_data.PRM_DATA.CH[DMA_CH_UART0RX].SRC_DATA_END_PTR = (uint32_t)&UART0->DR; // читаем из регистра данных UART0
    g_dma_channels_data.PRM_DATA.CH[DMA_CH_UART0RX].DST_DATA_END_PTR = (uint32_t)(a_dst + (v_size - 1)); // адрес последнего байта в приёмном буфере
    g_dma_channels_data.PRM_DATA.CH[DMA_CH_UART0RX].CHANNEL_CFG =
              (DMA_CHANNEL_CFG_CYCLE_CTRL_Basic << DMA_CHANNEL_CFG_CYCLE_CTRL_Pos) // Basic режим работы канала
            | (DMA_CHANNEL_CFG_DST_SIZE_Byte << DMA_CHANNEL_CFG_DST_SIZE_Pos) // записываем байты
            | (DMA_CHANNEL_CFG_DST_INC_Byte << DMA_CHANNEL_CFG_DST_INC_Pos) // адрес для записи изменяется на 1 байт каждую передачу
            | (DMA_CHANNEL_CFG_SRC_SIZE_Byte << DMA_CHANNEL_CFG_SRC_SIZE_Pos) // читаем байты
            | (DMA_CHANNEL_CFG_SRC_INC_None << DMA_CHANNEL_CFG_SRC_INC_Pos) // адрес чтения не изменяется
            | ((v_size - 1u) << DMA_CHANNEL_CFG_N_MINUS_1_Pos) // количество передаваемых байтов минус 1
            | (0 << DMA_CHANNEL_CFG_R_POWER_Pos) // после передачи каждого байта могут работать другие каналы, но тут другие каналы не используются :)
            ;
  } else {
    // нужно передать более 1024 элементов - используем scatter-gather режим работы канала
    // заполняем конфиги для scatter-gather
    int i = 0; // индекс конфига
    for ( ;0 != v_size; ++i ) {
      // сколько байтов передать в текущем конфиге (с индексом i)
      uint32_t v_to_write = (v_size <= 1024u) ? v_size : 1024u;
      // блок для передачи
      g_dma_cfg[i].SRC_DATA_END_PTR = (uint32_t)&UART0->DR; // читаем из регистра данных UART0
      g_dma_cfg[i].DST_DATA_END_PTR = (uint32_t)(a_dst + (v_to_write - 1u)); // адрес последнего байта в приёмном буфере
      // настройки конфига
      g_dma_cfg[i].CHANNEL_CFG =
                 // режим "scatter-gather для периферии c использованием альтернативной структуры" здесь означает,
                 // что данный конфиг был скопирован в альтернативный конфиг канала и после его
                 // отработки нужно вернуться к основному конфигу
                 (DMA_CHANNEL_CFG_CYCLE_CTRL_PeriphScatGathAlt << DMA_CHANNEL_CFG_CYCLE_CTRL_Pos)
               | (DMA_CHANNEL_CFG_DST_SIZE_Byte << DMA_CHANNEL_CFG_DST_SIZE_Pos) // записываем байты
               | (DMA_CHANNEL_CFG_DST_INC_Byte << DMA_CHANNEL_CFG_DST_INC_Pos) // адрес для записи изменяется на 1 байт каждую передачу
               | (DMA_CHANNEL_CFG_SRC_SIZE_Byte << DMA_CHANNEL_CFG_SRC_SIZE_Pos) // читаем байты
               | (DMA_CHANNEL_CFG_SRC_INC_None << DMA_CHANNEL_CFG_SRC_INC_Pos) // адрес чтения не изменяется
               | ((v_to_write - 1u) << DMA_CHANNEL_CFG_N_MINUS_1_Pos)// количество передаваемых байтов минус 1
               | (0 << DMA_CHANNEL_CFG_R_POWER_Pos) // после передачи каждого байта могут работать другие каналы, но тут другие каналы не используются :)
               ;
      // смещаем адрес в приёмном буфере
      a_dst += v_to_write;
      // уменьшаем количество принимаемых байтов
      v_size -= v_to_write;
      // если все байты распределены по конфигам
      if ( 0 == v_size ) {
        // то для последнего конфига проставим Basic режим
        // если оставить PeriphScatGathAlt, то с UART_RX почему-то не взводится флаг завершения работы канала
        // в DMA->IRQSTAT после обработки всех конфигов. с SPI_TX не так. короче говоря, здесь
        // следуем рекомендациям из "Руководства пользователя" :)
        g_dma_cfg[i].CHANNEL_CFG_bit.CYCLE_CTRL = DMA_CHANNEL_CFG_CYCLE_CTRL_Basic;
        // выходим из цикла формирования конфигов
        break;
      }
    }
    // теперь настраиваем канал DMA в режиме scatter-gather для работы с периферией
    // приём от UART0.
    // здесь адрес поля RESERVED последнего из астроенных конфигов
    g_dma_channels_data.PRM_DATA.CH[DMA_CH_UART0RX].SRC_DATA_END_PTR = (uint32_t)(&g_dma_cfg[i].RESERVED);
    // здесь адрес поля RESERVED альтернативного конфига канала UART0_RX (сюда контроллер DMA будет по очереди копировать подготовленные конфиги)
    g_dma_channels_data.PRM_DATA.CH[DMA_CH_UART0RX].DST_DATA_END_PTR = (uint32_t)&g_dma_channels_data.ALT_DATA.CH[DMA_CH_UART0RX].RESERVED;
    // настройки конфига
    g_dma_channels_data.PRM_DATA.CH[DMA_CH_UART0RX].CHANNEL_CFG =
              // режим "scatter-gather для периферии с использованием основого конфига"
              (DMA_CHANNEL_CFG_CYCLE_CTRL_PeriphScatGathPrim << DMA_CHANNEL_CFG_CYCLE_CTRL_Pos)
            | (DMA_CHANNEL_CFG_DST_SIZE_Word << DMA_CHANNEL_CFG_DST_SIZE_Pos) // запись конфига словами
            | (DMA_CHANNEL_CFG_DST_INC_Word << DMA_CHANNEL_CFG_DST_INC_Pos) // адрес записи увеличивается на размер слова
            | (DMA_CHANNEL_CFG_SRC_SIZE_Word << DMA_CHANNEL_CFG_SRC_SIZE_Pos) // чтение конфига словами
            | (DMA_CHANNEL_CFG_SRC_INC_Word << DMA_CHANNEL_CFG_SRC_INC_Pos) // адрес чтения увеличивается на размер слова
            | ((((i + 1) * 4) - 1) << DMA_CHANNEL_CFG_N_MINUS_1_Pos) // конличество слов в подготовленных конфигах минус 1
            | (2 << DMA_CHANNEL_CFG_R_POWER_Pos) // передача всех четырёх слов одного конфига будет произведена без "помех" со стороны других каналов
            ;
  }
  // включаем канал UART0_RX
  DMA->ENSET = DMA_CH_MSK_UART0RX;
  // корректируем оставшееся количество байтов для чтения
  *a_size_remains -= v_bytes_to_read;
  // возвращаем, сколько байтов будет прочитано
  return v_bytes_to_read;
}


// отправить данные в CRC0
void send_to_CRC( const uint8_t * a_buf, uint32_t a_len ) {
  // пока есть что отправлять
  while ( 0 != a_len ) {
    // если есть как минимум 4 байта
    if ( a_len >= 4 ) {
      // загрузим 4 байта в виде 32-битного слова, собранного в виде big-endian
      CRC0->DR = (a_buf[0] << 24)
               | (a_buf[1] << 16)
               | (a_buf[2] << 8)
               | a_buf[3]
               ;
      // двигаем указатель данных
      a_buf += 4;
      // уменьшаем количество данных
      a_len -= 4;
    } else {
      // если есть как минимум два байта
      if ( a_len >= 2 ) {
        // загрузим 2 байта в виде 16-битного слова, собранного в виде big-endian
        CRC0->DR16 = (a_buf[0] << 8)
                  | a_buf[1]
                  ;
        // двигаем указатель данных
        a_buf += 2;
        // уменьшаем количество данных
        a_len -= 2;
      } else {
        // если остался один байт, загрузим его в виде байта :)
        CRC0->DR8 = *a_buf;
        // один байт всегда последний
        break;
      }
    }
  }
}


// ожидать выполнения команды флэш-контроллером с указанным таймаутом
bool wait_flash_cmd( uint32_t a_timeout_ms ) {
  uint32_t v_from = g_milliseconds;
  //
  do {
    // проверяем бит BUSY, который сбрасывается контроллером по завершении команды
    if ( 0 == (FLASH->STAT & FLASH_STAT_BUSY_Msk) ) {
      return true;
    }
  } while ( (g_milliseconds - v_from) < a_timeout_ms );
  // врем вышло
  return false;
}


// запустить запись сектора флэш-памяти. a_size может быть меньше FLASH_SECTOR_4K, значит остальное нужно заполнить байтами 0xFF
// подразумевается, что a_src указывает на буфер размером как минимум FLASH_SECTOR_4K байтов
int write_flash_sector4K( uint8_t * a_src, uint32_t a_flash_addr, uint32_t a_size ) {
  // добиваем буфер байтами 0xFF до размера FLASH_SECTOR_4K байтов
  for ( uint32_t i = a_size; i < FLASH_SECTOR_4K; ++i ) {
    a_src[i] = 0xFF;
  }
  // сектор надо стереть
  // адрес
  FLASH->ADDR = a_flash_addr;
  // команда
  FLASH->CMD = (FLASH_CMD_KEY_Access << FLASH_CMD_KEY_Pos) | FLASH_CMD_ERSEC_Msk;
  // ждём
  if ( !wait_flash_cmd( 400u ) ) {
    return REPLY_ERR_WAIT_ERASE_PAGE;
  }
  // далее записываем (a_size + FLASH_PAGE_16 - 1)/FLASH_PAGE_16 страниц размером FLASH_PAGE_16 байтов
  for ( uint32_t v_pages_count = (a_size + (FLASH_PAGE_16 - 1))/FLASH_PAGE_16; v_pages_count != 0; --v_pages_count ) {
    // данные страницы
    uint32_t * v_page_ptr = (uint32_t *)a_src;
    FLASH->DATA[0].DATA = v_page_ptr[0];
    FLASH->DATA[1].DATA = v_page_ptr[1];
    FLASH->DATA[2].DATA = v_page_ptr[2];
    FLASH->DATA[3].DATA = v_page_ptr[3];
    // адрес
    FLASH->ADDR = a_flash_addr;
    // команда
    FLASH->CMD = (FLASH_CMD_KEY_Access << FLASH_CMD_KEY_Pos) | FLASH_CMD_WR_Msk;
    // закидываем данные со страницы в CRC32
    send_to_CRC( a_src, a_size > FLASH_PAGE_16 ? FLASH_PAGE_16 : a_size );
    // уменьшаем размер a_size для корректного подсчёта
    if ( a_size > FLASH_PAGE_16 ) {
      a_size -= FLASH_PAGE_16;
    } else {
      a_size = 0;
    }
    // двигаем на следующую страницу в буфере
    a_src += FLASH_PAGE_16;
    // адрес следующей страницы
    a_flash_addr += FLASH_PAGE_16;
    // ждём завершения записи страницы
    if ( !wait_flash_cmd( 10u ) ) {
      return REPLY_ERR_WAIT_WRITE_PAGE;
    }
  }
  //
  return 0;
}


// ожидание завершения приёма данных от UART0 через DMA
// передача 4096 байтов на скорости 460800 займёт не более 90 мс
bool wait_DMA_UART() {
  uint32_t v_from = g_milliseconds;
  uint32_t v_status = 0;
  while ( (g_milliseconds - v_from) < 100u ) {
    v_status = DMA->IRQSTAT;
    // если взведён бит заверщения работы канала
    if ( 0 != (v_status & DMA_CH_MSK_UART0RX) ) {
      // выпадаем из цикла ожидания
      break;
    }
  }
  // отключаем использованный канал
  DMA->ENCLR = DMA_CH_MSK_UART0RX;
  // сбрасываем бит завершения работы канала
  DMA->IRQSTATCLR = DMA_CH_MSK_UART0RX;
  // переключаем канал UART_RX на "основной" конфиг
  DMA->PRIALTCLR = DMA_CH_MSK_UART0RX;
  // возвращаем результат.
  return 0 != (v_status & DMA_CH_MSK_UART0RX);
}


// ожидаем прошивку в формате <размер:uint32_t><crc32:uint32_t><данные прошивки>
bool firmware_update() {
  // структура заголовка
  firmware_header_t v_header;
  // отключаем запрос к контроллеру DMA от UART0
  UART0->CR_bit.UARTEN = 0;
  UART0->DMACR_bit.RXDMAE = 0;
  UART0->CR_bit.UARTEN = 1;
  // адрес в FLASH, с которого начинать запись
  // "адрес" в понятиях контроллера FLASH-памяти, а для него она начинается с числа 0 (ноль)
  uint32_t v_current_flash_addr = LOADER_SIZE_BY_4K*FLASH_SECTOR_4K;
  // начальные установки блока CRC0
  CRC0->CR_bit.RESET = 1;
  // отправим "приглашение" загрузчику
  local_putc( REPLY_WAITING );
  // крутимся тут
  int v_rc;
  for (;;) {
    // был ли принят заголовок за 500 миллисекунд?
    if ( !local_gets( 501u, (uint8_t *)&v_header, sizeof(v_header) ) ) {
      // нет, тогда проверяем состояние кнопки на PC0 - если нажата (низкий уровень) - то продолжаем ожидать данные
      if ( 0 == (GPIOC->DATA & GPIO_PIN_MASK(0)) ) {
        // ещё раз отправим "приглашение"
        local_putc( REPLY_WAITING );
      } else {
        // иначе считаем, что с той стороны никого не ожидается, и можно передавать управление "прошивке" в FLASH
        return true;
      }
    } else {
      // ага, проверим размер флэша
      if ( v_header.m_size > AVAILABLE_FLASH_SIZE ) {
        // слишком много
        local_putc( REPLY_ERR_TOO_BIG );
        return false;
      } else {
        // отправим подтверждения приёма заголовка
        local_putc( REPLY_ACK );
        // включаем запрос к контроллеру DMA
        UART0->CR_bit.UARTEN = 0;
        UART0->DMACR_bit.RXDMAE = 1;
        UART0->CR_bit.UARTEN = 1;
      }
      // запускаем канал DMA на чтение из UART_0 в буфер g_recv_buf_1
      uint32_t v_page_size_1 = start_read_DMA_UART( g_recv_buf_1, &v_header.m_size );
      uint32_t v_page_size_2 = 0;
      //
      for (;;) {
        // теперь ждём завершения приёма в буфер g_recv_buf_1
        if ( !wait_DMA_UART() ) {
          // не дождались - отправим код ошибки
          local_putc( REPLY_ERR_RECEIVE );
          return false;
        } else {
          // юлок данных принят - отправим подтверждение
          local_putc( REPLY_ACK );
        }
        // теперь g_recv_buf_1 содержит прочитанные данные
        // запускаем чтение g_recv_buf_2
        if ( 0 != v_header.m_size ) {
          // ещё есть, что читать, запускаем канал DMA на чтение из UART_0 в буфер g_recv_buf_1
          v_page_size_2 = start_read_DMA_UART( g_recv_buf_2, &v_header.m_size );
        } else {
          v_page_size_2 = 0;
        }
        // пока оно там читается, запускаем запись сектора флэша из g_recv_buf_1
        v_rc = write_flash_sector4K( g_recv_buf_1, v_current_flash_addr, v_page_size_1 );
        if ( 0 != v_rc ) {
          // если НЕ записалось, отправим код ошибки
          local_putc( v_rc );
          return false;
        }
        // если больше ничего не чтитали, то на выход
        if ( 0 == v_page_size_2 ) {
          break;
        }
        // сектор записан, прибавим FLASH_SECTOR_4K к адресу записи
        v_current_flash_addr += FLASH_SECTOR_4K;
        // ожидаем завершения приёма из UART в буфер g_recv_buf_2
        if ( !wait_DMA_UART() ) {
          // не дождались, отправим код ошибки
          local_putc( REPLY_ERR_RECEIVE );
          return false;
        } else {
          // отправим подтверждение
          local_putc( REPLY_ACK );
        }
        // теперь g_recv_buf_2 содержит прочитанные данные
        // запускаем чтение g_recv_buf_1
        if ( 0 != v_header.m_size ) {
          v_page_size_1 = start_read_DMA_UART( g_recv_buf_1, &v_header.m_size );
        } else {
          v_page_size_1 = 0;
        }
        // запускаем запись сектора флэша из g_recv_buf_2
        v_rc = write_flash_sector4K( g_recv_buf_2, v_current_flash_addr, v_page_size_2 ); 
        if ( 0 != v_rc ) {
          local_putc( v_rc );
          return false;
        }
        // если больше ничего не читали, то на выход
        if ( 0 == v_page_size_1 ) {
          break;
        }
        // сектор записан, прибавим FLASH_SECTOR_4K к адресу записи
        v_current_flash_addr += FLASH_SECTOR_4K;
      }
      // здесь всё записалось.
      // сверим CRC32
      if ( v_header.m_crc32 != CRC0->POST ) {
        // несходняк, отправим код ошибки
        local_putc( REPLY_ERR_CRC32 );
        return false;
      } else {
        // ура, сошлось! отправим подтверждение
        local_putc( REPLY_ACK );
        // это чтобы ACK успел долететь
        delay_ms( 2u );
      }
      // вернём признак успешной загрузки прошивки
      return true;
    }
  }
}