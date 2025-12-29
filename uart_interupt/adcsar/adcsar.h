#ifndef ADCSAR_ADCSAR_H
#define ADCSAR_ADCSAR_H

#include <stdint.h>
#include <stdbool.h>

#define ADCSAR_THR_SHORT_MAX    49
#define ADCSAR_THR_NORMAL_MIN   121
#define ADCSAR_THR_NORMAL_MAX   240
#define ADCSAR_THR_ALARM_MIN    50
#define ADCSAR_THR_ALARM_MAX    120
#define ADCSAR_THR_TAMPER_MIN   241

typedef enum {
    ADCSAR_STATE_UNDEF = 0,
    ADCSAR_STATE_SHORT,
    ADCSAR_STATE_ALARM,
    ADCSAR_STATE_NORMAL,
    ADCSAR_STATE_TAMPER
} adcsar_state_t;

typedef struct {
    uint16_t raw_code;
    adcsar_state_t state;
    char state_char;
} adcsar_sample_t;

void adcsar_init(void);
void adcsar_start(void);
bool adcsar_poll(adcsar_sample_t *out_sample);
char adcsar_classify_code(uint16_t code);

#endif /* ADCSAR_ADCSAR_H */

