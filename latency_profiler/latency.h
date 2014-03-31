#ifndef LATENCY_H
#define LATENCY_H

#ifdef CPU_LPC2387

#define INIT_TRIGGER_PIN    (FIO0DIR |= BIT1)

#define TRIGGER_PIN_OFF      (FIO0CLR = BIT1)
#define TOGGLE_TRIGGER_PIN      (FIO0PIN ^= BIT1)
#endif

#endif /* LATENCY_H */
