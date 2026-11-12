// Wobbling boat: simple sketch
// Wiring: servo orange -> D9, red -> 5V, brown -> GND.
//         2 blue LEDs: D5 -> 220 ohm -> LED long leg (+), LED short leg (-) -> GND
//                      D6 -> 220 ohm -> LED long leg (+), LED short leg (-) -> GND
//
// 1) FIND YOUR CENTER (Serial Monitor, 9600 baud):
//    type an angle like 90 and press Enter -> servo goes there and holds.
//    Try 80, 85, 95, 100 ... until both crank pins point straight at each other
//    (boat level). Write that number into CENTER below and upload again.
//
// 2) RUN: close the Serial Monitor, open the wave page, Connect Arduino.
//    The page sends "amp,freq,rain" lines: amp 0..35 deg, freq 0.1..1.5 Hz,
//    rain 0..1 (0 = dry, 1 = downpour). The blue LEDs glow brighter as it rains harder.
//    No message for 3 s -> boat eases back to CENTER, LEDs fade off.
//    In the thunderstorm stage the page also sends "L" each time a bolt
//    strikes: the blue LEDs flicker like lightning, then fall back to the rain glow.
//
// The onboard LED lights while messages from the page are arriving.

#include <Servo.h>

int   CENTER = 100;        // <-- put your calibrated angle here
const float MAX_AMP = 35; // never swing more than CENTER +/- 35

const int RAIN_LEDS[] = {5, 6};    // PWM pins on Timer0 (the Servo library uses Timer1: pins 9/10)

Servo servo;
float rain = 0, targetRain = 0;        // 0..1, LED brightness follows this
float amp = 0, freq = 0.35;            // what the boat is doing now
float targetAmp = 0, targetFreq = 0.35;   // still until the camera says otherwise
float phase = 0;
unsigned long lastMsg = 0, lastTime = 0;
bool holding = false;                  // true after you type a single angle

// Lightning flicker: {brightness, ms} steps played once per "L" from the page.
// Bright-dark-bright stutter like a real strike, then a fading afterglow.
const int FLICKER[][2] = {
  {255, 50}, {0, 40}, {255, 30}, {20, 60}, {220, 70}, {90, 60}, {160, 40}, {40, 60}
};
const int FLICKER_STEPS = sizeof(FLICKER) / sizeof(FLICKER[0]);
int flickerStep = -1;                  // -1 = not flickering
unsigned long flickerAt = 0;           // when the current step started

void startLightning() {
  flickerStep = 0;
  flickerAt = millis();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  for (int p : RAIN_LEDS) { pinMode(p, OUTPUT); analogWrite(p, 0); }
  Serial.begin(9600);
  Serial.setTimeout(50);               // works with or without a line ending
  servo.attach(9);
  servo.write(CENTER);
  delay(500);
  Serial.println("ready. type an angle (e.g. 90) to hold it, or send amp,freq");
  lastTime = millis();
}

void readSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line[0] == 'L' || line[0] == 'l') {   // "L" = lightning strike from the page
    startLightning();
    lastMsg = millis();
    return;
  }

  int comma = line.indexOf(',');
  if (comma >= 0) {                    // "amp,freq,rain" from the wave page
    int comma2 = line.indexOf(',', comma + 1);
    targetAmp  = constrain(line.substring(0, comma).toFloat(), 0, MAX_AMP);
    targetFreq = constrain(line.substring(comma + 1, comma2 < 0 ? line.length() : comma2).toFloat(), 0.1, 1.5);
    targetRain = comma2 < 0 ? 0 : constrain(line.substring(comma2 + 1).toFloat(), 0, 1);
    lastMsg = millis();
    holding = false;
  } else {                             // a single number: hold that angle
    int a = constrain(line.toInt(), 0, 180);
    CENTER = a;
    holding = true;
    servo.write(a);
    Serial.print("holding at ");
    Serial.println(a);
  }
}

void loop() {
  readSerial();

  if (holding) { delay(20); return; }  // calibration: stay put

  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0;
  lastTime = now;

  bool pageActive = (now - lastMsg < 3000) && lastMsg != 0;
  digitalWrite(LED_BUILTIN, pageActive ? HIGH : LOW);
  if (!pageActive) { targetAmp = 0; targetRain = 0; }   // no camera input -> settle, lights off

  // ease toward the targets over about a second
  float k = dt * 1.5; if (k > 1) k = 1;
  amp  += (targetAmp  - amp)  * k;
  freq += (targetFreq - freq) * k;
  rain += (targetRain - rain) * min(1.0, dt * 3.0);

  // blue "rain" lights: visible glow as soon as the first drops fall, full bright in a downpour
  int level = rain < 0.02 ? 0 : (int)(50 + rain * 205);

  // lightning: play the flicker steps on top of the rain glow, then hand back to it
  if (flickerStep >= 0) {
    while (flickerStep < FLICKER_STEPS && now - flickerAt >= (unsigned long)FLICKER[flickerStep][1]) {
      flickerAt += FLICKER[flickerStep][1];
      flickerStep++;
    }
    if (flickerStep >= FLICKER_STEPS) flickerStep = -1;     // done
    else level = FLICKER[flickerStep][0];
  }
  for (int p : RAIN_LEDS) analogWrite(p, level);

  phase += 2 * PI * freq * dt;
  if (phase > 2 * PI) phase -= 2 * PI;

  float angle = CENTER + amp * sin(phase);
  servo.write((int)angle);
  delay(20);
}
