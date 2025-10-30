#include <Arduino.h>

#define ASSERT(exp) ((exp) ? 1 : 0 && Serial.printf("%s:%u: assertion %s failed\n", __FILENAME__, __LINE__, #exp))

#define PWM_MAX ((1<<10)-1)/*1023*/

#define PULSO_MIN 1000
#define PULSO_MAX 2000
#define PULSO_DIF (PULSO_MAX-PULSO_MIN)
#define PULSO_MED (PULSO_MIN + PULSO_DIF/2)

#define INTERRUPTOR_BAIXO(p) (p < (PULSO_MIN+PULSO_DIF/3))
#define INTERRUPTOR_ALTO(p)  (p > (PULSO_MAX-PULSO_DIF/3))
#define INTERRUPTOR_MEIO(p)  (!INTERRUPTOR_BAIXO(p) && !INTERRUPTOR_ALTO(p))

#define TEMPO_FOGO 1000


// #define PROTO
#define DRACARYS

#if defined(PROTO)
  #warning "robô PROTOBOARD"
#elif defined(DRACARYS)
  #pragma message "robô DRACARYS"

  #undef MIXAR
  #undef FOGO_MANUAL
  #undef ESPNOW
  #define RADIO

  #define eixo_x_ch   7  /*ch1*/
  #define eixo_y_ch   8  /*ch2*/
  #define fogo_ch     9  /*ch3*/
  #define isqueiro_ch 10 /*ch4*/

  #define roda_esq_m1 1
  #define roda_esq_m2 2

  #define roda_dir_m1 3
  #define roda_dir_m2 4

  #define fogo_m1 5
  #define fogo_m2 6

  #define isqueiro_fogo 0
#else
  #error "robô NENHUM"
#endif

struct par {
    union { int16_t esq, x, a; };
    union { int16_t dir, y, b; };
};

#define ENUM_ITEM(nome) nome,
#define ENUM_STR(nome) [nome]=#nome,
#define ENUM(tipo, lista) \
    enum  tipo       { lista(ENUM_ITEM) }; \
    char* tipo##_str[] { lista(ENUM_STR)  } \

#define ITENS_PEDIDO(X) \
    X(TRAS) \
    X(FRENTE)
ENUM(pedido, ITENS_PEDIDO);

#define ITENS_FOGO(X) \
    X(PARADO_TRAS) \
    X(PARADO_FRENTE) \
    X(INDO) \
    X(VOLTANDO)
ENUM(estado_fogo, ITENS_FOGO);

enum estado_fogo fogo = PARADO_TRAS;
unsigned long fim_fogo = 0;

void setup() {
    pinMode(eixo_x_ch, INPUT);
    pinMode(eixo_y_ch, INPUT);
    pinMode(fogo_ch,   INPUT);
    pinMode(isqueiro_ch, INPUT);
    
    pinMode(roda_esq_m1, OUTPUT);
    pinMode(roda_esq_m2, OUTPUT);
    pinMode(roda_dir_m1, OUTPUT);
    pinMode(roda_dir_m2, OUTPUT);

    pinMode(fogo_m1, OUTPUT);
    pinMode(fogo_m2, OUTPUT);

    pinMode(isqueiro_fogo, OUTPUT);

    Serial.begin(115200);
}

//! se o interruptor tiver no meio, devia usar o eixo x pra mexer os motores pra frente e pra trás
void loop() {
  #ifdef RADIO
    // lê o que o rádio manda como pulsos e vê se tão chegando mesmo
    unsigned long pulso_fogo = pulseIn(fogo_ch,     HIGH, 20000);
    unsigned long pulso_x    = pulseIn(eixo_x_ch,   HIGH, 20000);
    unsigned long pulso_y    = pulseIn(eixo_y_ch,   HIGH, 20000);
    unsigned long pulso_isq  = pulseIn(isqueiro_ch, HIGH, 20000);
  #elif defined(ESPNOW)
    unsigned long pulso_fogo = 0; //!
    unsigned long pulso_x    = 0; //!
    unsigned long pulso_y    = 0; //!
    unsigned long pulso_isq  = 0; //!
  #else
    static_assert(!"comunicação!!!!!!") //!
  #endif

    // failsafe // checa se teve timeout
    if (pulso_fogo + pulso_x + pulso_y + pulso_isq == 0) {
        // desliga os motores e volta o motor de fogo
        digitalWrite(isqueiro_fogo, 0);
        mover(0, 0);
        esperar_fogo_desligar();

        //! debug
        Serial.printf("tentando failsafe: fogo=%s\n", estado_fogo_str[fogo]);

        // tenta de novo
        return;
    }

    // arma
    enum pedido pedido_fogo = pulso_fogo > PULSO_MED ? FRENTE : TRAS;
  #if defined(FOGO_MANUAL)
    if      (INTERRUPTOR_ALTO (pulso_fogo)) motor_fogo( PWM_MAX/3);
    else if (INTERRUPTOR_BAIXO(pulso_fogo)) motor_fogo(-PWM_MAX/3);
    else                                    motor_fogo(0);
  #else
    fogo = pedir_fogo(pedido_fogo, fogo);
  #endif

    // isqueiro
    digitalWrite(isqueiro_fogo, pulso_isq > PULSO_MED);

    // movimento
    struct par vels = mixar(pulsoPWM(pulso_x),
                            pulsoPWM(pulso_y));
    mover(vels.esq, vels.dir);

    //! debug
    Serial.printf("%4lu, %4lu:\t"  "pwm %5d, %5d | "  "%d, "       "fogo=%s\n",
                  pulso_x,pulso_y, vels.esq,vels.dir, pedido_fogo, estado_fogo_str[fogo]);
}

//! devia voltar direto o tanto que precisa quando tá indo mas tem que voltar etc
enum estado_fogo pedir_fogo(enum pedido pedido_atual, enum estado_fogo fogo_atual) {
    if (fogo_atual == PARADO_FRENTE) {
        if (pedido_atual == FRENTE) return fogo_atual;

        fim_fogo = millis() + TEMPO_FOGO;
        fogo_tras(); return VOLTANDO;
    }
    if (fogo_atual == PARADO_TRAS) {
        if (pedido_atual == TRAS) return fogo_atual;

        fim_fogo = millis() + TEMPO_FOGO;
        fogo_frente(); return INDO;
    }

    if (acabou_fogo()) {
        motor_fogo(0);
        if (fogo_atual == INDO)     return PARADO_FRENTE;
        if (fogo_atual == VOLTANDO) return PARADO_TRAS;
    }

    Serial.printf(
        "pedido: %s, atual: %s\n",
        pedido_str[pedido_atual],
        estado_fogo_str[fogo_atual]
    );
    return fogo_atual;
}

bool acabou_fogo() { return millis() > fim_fogo; }
void esperar_fogo_desligar() { 
    while (fogo != PARADO_TRAS) fogo = pedir_fogo(TRAS, fogo);
}

void motor_fogo(int16_t vel) {
    motor(fogo_m1,fogo_m2, vel);
  #if   defined(fogo_m3) && defined(fogo_m4)
    motor(fogo_m3,fogo_m4, vel);
  #elif defined(fogo_m3) || defined(fogo_m4)
    #error "é preciso definir tanto fogo_m3 quanto fogo_m4 se o fogo tiver em 4 pinos"
  #endif
}
void fogo_frente() { motor_fogo( PWM_MAX); }
void fogo_tras()   { motor_fogo(-PWM_MAX); }

void mover(int16_t esq, int16_t dir) {
    motor(roda_esq_m1, roda_esq_m2, esq);
    motor(roda_dir_m1, roda_dir_m2, dir);
}

void motor(uint8_t m1, uint8_t m2, int16_t vel) {
    if (vel < 0) {
        analogWrite(m1, abs(vel));
        analogWrite(m2, 0);
    } else {
        analogWrite(m1, 0);
        analogWrite(m2, vel);
    }
}

struct par mixar(int16_t x, int16_t y) {
  #ifdef MIXAR
    //! avaliar a ideia de normalizar esse "vetor" nos cantos
    return {
        constrain(y + x, -PWM_MAX,PWM_MAX),
        constrain(y - x, -PWM_MAX,PWM_MAX),
    };
  #else
    return { x, y };
  #endif
}

int16_t pulsoPWM(unsigned long pulso) {
    int16_t pwm = map(pulso, PULSO_MIN,PULSO_MAX,
                            -PWM_MAX,  PWM_MAX);
    return constrain(pwm, -PWM_MAX,PWM_MAX);
}
