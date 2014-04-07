#ifndef LATENCY_H
#define LATENCY_H

#ifdef CPU_LPC2387

/* Using P0.1 as second debug PIN */
#define TRIGGER_PIN     (BIT1)

#define TRIGGER_PIN_INIT        (FIO0DIR |= BIT1)

#define TRIGGER_PIN_OFF         (FIO0CLR = BIT1)
#define TRIGGER_PIN_ON          (FIO0SET = BIT1)
#define TRIGGER_PIN_TOGGLE      (FIO0PIN ^= BIT1)
#endif

#endif /* LATENCY_H */
