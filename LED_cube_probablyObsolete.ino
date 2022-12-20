/*
Author:		badBadger
Date:		2022-10-06
Version:	1.0
*/

// Pattern 0: Cycles through all LEDs consecutively, layer by layer
// Pattern 1: Every other LED is on (all layers are the same)
// Pattern 2: Every other LED is on (different for even/odd layers)
// Pattern 3: Cycles through all rows in each layer
// Pattern 4: Goes up one column of LEDs, then goes to the next one

//define attiny pins
#define BTN PB0
#define DATA PB2
#define CLK PB3
#define LATCH PB4

//changing variables
short data; //16-bits for 16 LEDs per layer
byte layer; //lower 4 bits for layers, upper 4 bits for buffer
uint16_t delayLED; //wait time between LEDs [ms]
uint32_t lastTime; //last timestamp for delay
uint8_t pattern;
uint8_t LED_counter;
uint8_t layer_counter;
volatile bool buttonPressed;
volatile bool previousState;
volatile uint8_t switchCheck;

void setup()
{
  cli(); // Disable interrupts during setup
  
  pinMode(BTN, INPUT); // Set interrupt pin as input w/ internal pullup
  pinMode(DATA, OUTPUT); // Set serial data as output
  pinMode(CLK, OUTPUT); // Set shift register clock as output
  pinMode(LATCH, OUTPUT); // Set output register (latch) clock as output

  // set up Timer 1
  GTCCR |= (1 << PSR1); // Reset prescaler
  TCCR1 = 0;
  TCCR1 |= (1 << CTC1) | (1 << CS12) | (1 << CS11) | (1 << CS10); // Enable counter on compare match and set prescaler to 64
  TCNT1 = 0; // Zero the timer
  OCR1C = 49; // set compare time to ~200us -> 1/(1e6 / 64 / (49 + 1)) = 200us
  TIMSK |= (1 << OCIE1A); // compare interrupt enable of matchA

  //default conditions
  data = 0b0000000000000001;
  layer = 0b00000001;
  delayLED = 500;
  lastTime = 0;
  pattern = 0;
  LED_counter = 0;
  layer_counter = 0;
  buttonPressed = false;
  sei(); //enable interrupts after setup
}

void loop()
{
  if(buttonPressed){
    changePattern();
  }

  if((millis()-lastTime)>=delayLED){ //if delay time is over...
    //...do the next iteration of things
    lastTime = millis(); //update timestamp
    digitalWrite(LATCH, 0); //Set latch pin LOW so nothing gets shifted out
    shiftOutLayer(DATA, CLK, layer); //Shift out information which layer is on
    data = getPattern(pattern); //Choose  pattern for layer
    shiftOutShort(DATA, CLK, data); //Shift out LED states in that layer
    digitalWrite(LATCH, 1); //sent everything out in parallel
  }
} 

ISR(TIMER1_COMPA_vect){  
  static uint8_t count = 0; // Counter for number of equal states
  static uint8_t button_state = 0; // Keeps track of current (debounced) state
  uint8_t current_state = PINB & (1 << PINB0) != 0; // Check if button is high or low for the moment
  
  if (current_state != button_state) {
    count++; // Button state is about to be changed, increase counter
    if (count >= 4) {
      button_state = current_state; // The button have not bounced for four checks, change state
        if (current_state != 0) {
          buttonPressed = true; // If the button was pressed (not released), tell main so
        }
      count = 0;
    }
  }
  else{
    count = 0; // Reset counter
  }
}

short getPattern(int pattern){
  short output = 0b0000000000000000;
  
  switch(pattern){
    //just cycles through all LEDs in each layer
    case 0:
      //Set delays for pattern
      //delayLED = 500;
    
      //Set LED states
      if(LED_counter == 17){
        output = 0000000000000001;
        LED_counter = 0;
      }
      else{
        output = 0b0000000000000001 << LED_counter;
      }
    
      if(LED_counter == 16){
        changeLayer();
      }
    
      LED_counter++;
      break;
    
    //every other LED is on (same for all layers)
    case 1:
      //Set delays for pattern
      delayLED = 500;
    
      //Set LED states
      output = 0b1010101010101010;
    
      changeLayer();
      break;
    
    //every other LED is on (mirrored in even layers)
    case 2:
      //Set delays for pattern
      //delayLED = 500;
    
      //Set LED states
      if(layer%2 == 0){
        output = 0b0101010101010101;
      }
      else{
        output = 0b1010101010101010;
      }
    
      changeLayer();
      break;
    
    //cycles through all rows in each layer
    case 3:
      //Set delays for pattern
      //delayLED = 500;
    
      //Set LED states
      if(LED_counter == 5){
      	output = 0b0000000000001111;
        LED_counter = 0;
      }
      else{
		output = 0b0000000000001111 << LED_counter*4;
      }
    
      if(LED_counter == 4){
        changeLayer();
      }
    
      LED_counter++;
      break;
    
    case 4:
      //Set delays for pattern
      //delayLED = 500;
    
      //Set LED states
      if(layer_counter == 4){
        layer_counter = 0;
        LED_counter++;
      }
         
      if(LED_counter == 16){
        LED_counter = 0;
      }
      
      output = 0b0000000000000001 << LED_counter;
      changeLayer();
      layer_counter++;
      break;
      
    
    //just defaults to all LEDs off
    default:
      output = 0b0000000000000000;
      break;
   }

  return output;
}

// Switch to the next layer, or circle back to the first one
void changeLayer(){
  if(layer & 0b00001000){
    layer = 0b0001;
  }
  else{
    layer = layer << 1;
  }
}

// Changes to the next pattern after interrupt
void changePattern(){
  //reset pattern change flag
  buttonPressed = false;
  
  //check if last pattern is reached
  if(pattern < 4){
    pattern +=1;
  }
  else{
    pattern = 0;
  }
    
  layer = 1;
  LED_counter = 0;
}

// This shifts 4 bits out MSB first, on the rising edge of the clock.
void shiftOutLayer(int dataPin, int clockPin, byte layer){
  int i = 0;
  int pinState = 0;
  
  //clear everything just in case
  digitalWrite(dataPin, 0);
  digitalWrite(clockPin, 0);

  for(i=3; i>=0; i--){
    digitalWrite(clockPin,0);
    
    if(layer & (1<<i)){
      pinState = 1;
    }
    else{
      pinState = 0;
    }
    
    digitalWrite(dataPin, pinState);
    digitalWrite(clockPin,1);
    digitalWrite(dataPin, 0);
  }
  
  digitalWrite(clockPin,0);
}

// This shifts 16 bits out MSB first, on the rising edge of the clock.
void shiftOutShort(int dataPin, int clockPin, short toBeSent){
  int i=0;
  int pinState = 0;
  
  //Clear everything out just in case
  digitalWrite(dataPin, 0);
  digitalWrite(clockPin, 0);

  //Loop through bits in the data bytes
  //COUNTING DOWN in the for loop so that %00000001 or "1"
  //will go through such that it will be pin Q0 that lights.
  for(i=15; i>=0; i--){
    digitalWrite(clockPin, 0);
    //if the value passed to myDataOut AND a bitmask result
    //in true then set pinState to 1
    if(toBeSent & (1<<i)){
      pinState = 1;
    }
    else{
      pinState = 0;
    }
    
    digitalWrite(dataPin, pinState); //Sets the pin to HIGH or LOW depending on pinState
    digitalWrite(clockPin, 1); //Shifts bits on upstroke of clock pin
    digitalWrite(dataPin, 0); //Zero the data pin after shift to prevent bleed through
  }
  digitalWrite(clockPin, 0); //Stop shifting
}