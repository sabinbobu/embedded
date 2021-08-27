#include <avr/wdt.h>
//#include <MAX30100 .h>
#include <SPI.h>


// ----------- SPI variables --------
const bool is_master = true;
volatile bool master_send_ok = true;
const unsigned LEN = 10;
char buf1[LEN + 1], buf2[LEN + 1];
volatile unsigned index = 0;
volatile unsigned to_be_displayed = 1u;
volatile bool ready_to_display = false;

void setup_master()
{
  cli();
  SPI.begin();

  pinMode(MOSI, OUTPUT);
  pinMode(MISO, INPUT);
  pinMode(SCK, OUTPUT);
  pinMode(SS, OUTPUT);

  SPCR |= 1 << SPE;   // SPI enable
  SPCR |= 1 << SPIE;  // enable interrupt
  SPCR |= 1 << MSTR;  // this is master => MSTR = 1

  // setez frecventa SPI-ului = 16MHz / 4 = 4MHz
  SPCR &= ~(1 << SPR1);   // SPR1 = 0
  SPCR &= ~(1 << SPR0);   // SPR0 = 0
  SPSR &= ~(1 << SPI2X);  // SPI2X = 0

  SPCR &= ~(1 << DORD);  // DORD = 0 => MSB first

  SPCR &= ~(1 << CPOL);
  SPCR &= ~(1 << CPHA);

  sei();
}


ISR(SPI_STC_vect) // ISR SPI ----
{
  if (!is_master) {
    char *buf = (to_be_displayed == 1) ? buf2 : buf1;
    buf[index++] = SPDR;  // data register
    buf[index] = 0;

    if (index == LEN) {
      to_be_displayed = (to_be_displayed == 1) ? 2 : 1;
      index = 0;
      ready_to_display = true;
    }
  } else {
    master_send_ok = true;
  }
}
// ----------------- end SPI -------------------

const unsigned BAUD_RATE = 9600;
const unsigned FREQ = 10; // HERTZ

// ================Sensor==========================

//MAX30100  heart_rate(2);
// MAX30100* pulseOxymeter;

float heartBPM = 60;
float heartBPM_avg;
unsigned int seconds;
unsigned int oneminute50;
unsigned int oneminute140;

const int buzzer = 9; //buzzer to arduino pin 9

void setup_buzzer() {
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output
}

// =====================================
const unsigned NTASKS = 10;

typedef enum TASK_STATE { WAIT, READY, RUN } task_state;
typedef struct task
{
  unsigned id{0};
  unsigned vtime{};
  task_state state{WAIT};
  void (*f)(void *) {};
  void *param{};
  unsigned prio{};
  unsigned T{};  // period
  unsigned D{};  // delay
  unsigned timeout{};
  bool periodic{true};
} task;

task tasks[NTASKS];

// =====================================
// pentru 1 min = 60 sec = > 600 ticks
// pentru 1 sec = 600 ticks / 60 = 10 ticks = 10 Hz
// +++++++++++++++++++++++++++++++++++++++++++++++++++
void setup_timer1()
{
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0; //SET INIT VALUE

  //set prescaler to 1024
  TCCR1B |= 1 << CS02 | 1 << CS00;

  OCR1A = 16 * pow(10, 6) / (1024 * FREQ) - 1;

  TIMSK1 |= 1 << OCIE1A; // ENABLE INTERRUPT FOR OCR1A
  TCCR1B |= 1 << WGM12;  // CTC (clear timer on compare) mode

  sei();  // begin to count
}
// +++++++++++++++++++++++++++++++++++++++++++++++++++

unsigned add_task(void (*f)(void*), void *param, unsigned T, unsigned D, unsigned timeout, bool periodic, unsigned prio)
{
  if (!f || T == 0) return NTASKS;
  for (unsigned i = 0; i < NTASKS; i++) {
    if (tasks[i].f == NULL) {
      tasks[i].id = i;
      tasks[i].f = f;
      tasks[i].param = param;
      tasks[i].T = T;
      tasks[i].D = D;
      tasks[i].timeout = timeout;
      tasks[i].periodic = periodic;
      tasks[i].prio = prio;
      tasks[i].vtime = 0;
      tasks[i].state = WAIT;

      return i;
    }
  }
  return NTASKS;  // lista e plina
}

unsigned delete_task(unsigned id)
{
  if (id < NTASKS) {
    tasks[id].f = NULL;
    tasks[id].state = WAIT;
    tasks[id].T = 0;
  }
}

ISR(TIMER1_COMPA_vect)  // "scheduler tick"
{

  // check_heart_rate
  ++seconds;
  if (seconds > 2 * FREQ)// 20 ticks = 2 sec
  {
    if (heartBPM < 50) {
      ++oneminute50;
    }
    else
      oneminute50 = 0;

    if (heartBPM > 140 )
    {
      ++oneminute140;
    }
    else
      oneminute140 = 0;
  }

  if (oneminute50 >= 60 * FREQ || oneminute140 >= 60 * FREQ) { // if heartBPM < 50 || heartBPM > 140 more than 1 min
    activate_buzzer();
    send_ALARM_message();
    Serial.println("ALARM");
  }
  else
  {
    deactivate_buzzer();
  }


  for (unsigned i = 0; i < NTASKS; i++) {
    if (tasks[i].f != NULL) {
      if (tasks[i].state == RUN) {
        if (++tasks[i].vtime > tasks[i].timeout) {
          delete_task(i);  // TODO
        }
      } else {
        if (tasks[i].D == 0) {
          tasks[i].state = READY;
          tasks[i].vtime = 0;
          if (tasks[i].periodic)
            tasks[i].D = tasks[i].T;
        } else {
          tasks[i].D--;
        }
      }
    }
  }
}

void init_scheduler()
{
  setup_timer1();
}

void process_commands(void *);

void setup_watchdog()
{
  cli();
  sei();
}


void setup_heart_rate_sensor()
{
  // setup
  //  pulseOxymeter = new MAX30100();
  //  pinMode(2, OUTPUT);
  //
  //  pulseoxymeter_t result = pulseOxymeter->update();
  //
  //  if( result.pulseDetected == true )
  //  {
  //    Serial.print( result.heartBPM );
  //  }
}

void activate_buzzer() {
  tone(buzzer, 1000);
  delay(1000);
  noTone(buzzer);
  delay(1000);
  Serial.println("Buzzer activat.");
}
void deactivate_buzzer() {
  noTone(buzzer);
  Serial.println("Buzzer dezactivat.");
}

void send_ALARM_message() {
  // send Alarm message via SPI

  if (master_send_ok) {
    digitalWrite(SS, LOW);  // slave select
    master_send_ok = false;
    SPDR = 'A';
    digitalWrite(SS, HIGH);
  } else {
    if (ready_to_display) {
      char *buf = (to_be_displayed == 1) ? buf1 : buf2;
      buf[LEN] = 0;
      Serial.println(buf);
      ready_to_display = false;
    }
  }// end else if
}

unsigned rand_heartBPM = random(50, 140); // random values for heart rate
unsigned  sum = 0.0;
unsigned nrRead = 0;

// collect every 2 sec = 60 / 2 = 30 citiri
void create_heart_rate_avg(void *unused) {
  // create the average
  unsigned rand_heartBPM = random(50, 140); // random values for heart rate
  sum += rand_heartBPM;
  nrRead++;
  if (nrRead == 30) {
    heartBPM_avg = sum / 30;
  }
}

void spi_send_heart_rate_avg(void *unused) {

  if (master_send_ok) {
    digitalWrite(SS, LOW);  // slave select
    master_send_ok = false;
    SPDR = heartBPM_avg ; // AVG = heartBPM_avg
    digitalWrite(SS, HIGH);
  } else {
    if (ready_to_display) {
      char *buf = (to_be_displayed == 1) ? buf1 : buf2;
      buf[LEN] = 0;
      Serial.println(buf);
      ready_to_display = false;
    }
  }// end else if
}



void setup()
{
  Serial.begin(BAUD_RATE);
  setup_heart_rate_sensor();
  setup_master(); // SPI
  setup_buzzer();

  // add tasks
  add_task(create_heart_rate_avg, NULL,/*T = */ 2 * FREQ, 0, 2 * FREQ, true, 1); // collect the heart rate every 2 sec

  add_task(spi_send_heart_rate_avg, NULL, 60 * FREQ, /*Delay = */ 5 * FREQ, 2 * FREQ, true, 1); // sent every 1 min AVG = value

  init_scheduler();
  wdt_enable(WDTO_30MS);
}


void loop()
{
  for (unsigned i = 0; i < NTASKS; i++) {
    if (tasks[i].f != NULL && tasks[i].state == READY) {
      tasks[i].state = RUN;
      tasks[i].f(tasks[i].param);  // execute task (might take longer than expected)
      tasks[i].state = WAIT;
      if (!tasks[i].periodic)
        delete_task(i);
    }
    wdt_reset();
  }
}
