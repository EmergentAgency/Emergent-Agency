/* RGB Analog Example, Teensyduino Tutorial #2
   http://www.pjrc.com/teensy/tutorial2.html

   This example code is in the public domain.
*/

#include <Tone.h>
Tone tone1;
HardwareSerial Uart = HardwareSerial();

const int redPin =  25;
const int greenPin =  24;
const int bluePin =  14;
const int pinfour = 15;
const int pinfive = 16;
char node = 'b';
char partner = 'c';
char incomingbyte;
int motionvalue = 0;

const int motionpin = 38;

void setup()   {
  Uart.begin(9600);
  Serial.begin(9600);
//  tone1.begin(18);  
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(pinfour, OUTPUT);
  pinMode(pinfive, OUTPUT);
}

void loop()                     
{
//  if (node == 'a') {
//      motionvalue = analogRead(motionpin);
//     if (motionvalue > 60 ) {
//      Serial.println(motionvalue);
//    
// //   }
//      analogWrite(redPin, 20);
//      delay(50);
//      analogWrite(greenPin, 50);
//      analogWrite(redPin, 0);
//      delay(50);
//      analogWrite(bluePin, 100);
//      analogWrite(greenPin, 0);
//      delay(50);
//      analogWrite(pinfour, 150);
//      analogWrite(bluePin, 0);
//      delay(50);
//      analogWrite(pinfive, 200);
//      analogWrite(pinfour, 0);
//      delay(50);
//      analogWrite(pinfive, 0);
//      Uart.print('b');
//      delay(50);
//      //Uart.println(partner);
// }
//}
  if (Uart.available() > 0 && node != 'a') {
    incomingbyte = Uart.read();
    Serial.println(incomingbyte);
    if (incomingbyte == node) {
      Serial.println("match");
      analogWrite(redPin, 255);
  //    delay(50);
      analogWrite(greenPin, 255);
      analogWrite(redPin, 0);
 //     delay(50);
      analogWrite(bluePin, 255);
      analogWrite(greenPin, 0);
 //     delay(50);
      analogWrite(pinfour, 255);
      analogWrite(bluePin, 0);
  //    delay(50);
      analogWrite(pinfive, 255);
      analogWrite(pinfour, 0);
 //     delay(50);
      analogWrite(pinfive, 0);
 //     delay(50);
      Uart.print(partner);
      Uart.flush();
    }
  }
}

