#include <SoftwareSerial.h>

const byte MACHINETYPE = 1; // 1 = handing out, 0 = receiving cards

const byte rxPin = 8;
const byte txPin = 9;
const byte buttonPin = 2;
const byte invertedSignal = 1;

// Set up a new SoftwareSerial object
SoftwareSerial mySerial (rxPin, txPin, invertedSignal);

// Reference lib: https://github.com/catzhou2002/K750Demo
int counter = 0;
int LEDPIN = 13;
int waitCounter = 0;
int setReceiveCounter = 0;

enum class States {READY, SENDING, WAITING};
States state;

enum class Commands {STATUS_AP, RECEIVE_CARD_FC8, RECYCLE_CARD};
Commands nextCommand;

bool cardOutputPreviouslyHadCard = true;

void setup()
{
  state = States::SENDING;
  nextCommand = Commands::STATUS_AP;
  delay(500);
  Serial.begin(9600);
  mySerial.begin(9600);
  pinMode(LEDPIN, OUTPUT);
  pinMode(txPin, OUTPUT);
  pinMode(rxPin, INPUT);
  pinMode(buttonPin, OUTPUT);
  digitalWrite(buttonPin, LOW);
  delay(1000);
}

void sendData(char cmdLetter1, char cmdLetter2)
{
  int data[20];
  int len = 9;
  data[0] = 0x02;//stx
  data[1] = 0x30; //addr H
  data[2] = 0x30; //addr L
  data[3] = 0x00; //len H
  data[4] = 2; //len L
  data[5] = (byte)cmdLetter1;
  data[6] = (byte)cmdLetter2;
  data[7] = 0x03; //etx

  data [8] =  data[0]; // check code
  for (int i = 1; i < len - 1; i++)
  {
    data[8] ^= byte(data[i]);
  }
  Serial.print("Sending:");
  for (int i = 0; i < len; i++)
  {
    Serial.print(" 0x");
    Serial.print(data[i], HEX);
    mySerial.write(data[i]);
  }
  Serial.println("");
  // Sending: 0x02 0x30 0x30 0x00 0x02 0x41 0x50 0x03 0x12
}

void sendData2(char cmdLetter1, char cmdLetter2, char cmdLetter3)
{
  int data[20];
  int len = 10;
  data[0] = 0x02;//stx
  data[1] = 0x30; //addr H
  data[2] = 0x30; //addr L
  data[3] = 0x00; //len H
  data[4] = 3; //len L
  data[5] = (byte)cmdLetter1;
  data[6] = (byte)cmdLetter2;
  data[7] = (byte)cmdLetter3;
  data[8] = 0x03; //etx

  data [9] =  data[0]; // check code
  for (int i = 1; i < len - 1; i++)
  {
    data[9] ^= byte(data[i]);
  }
  Serial.print("Sending:");
  for (int i = 0; i < len; i++)
  {
    Serial.print(" 0x");
    Serial.print(data[i], HEX);
    mySerial.write(data[i]);
  }
  Serial.println("");
  Serial.println("******************************************");
  Serial.println("");
  // Sending: 0x02 0x30 0x30 0x00 0x02 0x41 0x50 0x03 0x12
}

void soundTheAlarm()
{
  Serial.println("Card feeder error!");
  delay(500);
}

void sendACK()
{
  int data[20];
  int len = 3;
  data[0] = 0x05;
  data[1] = 0x30;
  data[2] = 0x30;

  Serial.print("ACK Received! Confirming: ");
  for (int i = 0; i < len; i++)
  {
    Serial.print(" 0x");
    Serial.print(data[i], HEX);
    mySerial.write(data[i]);
  }
  Serial.println("");
}

void loop()
{
  // Send requests every now and then
  if ( counter % 15000 == 0 )
  {
    if ( state == States::SENDING )
    {
      if ( nextCommand == Commands::RECEIVE_CARD_FC8 )
      {
        sendData2('F', 'C', '8'); // 0x2 0x30 0x30 0x0 0x3 0x46 0x43 0x38 0x3 0x3f
        nextCommand = Commands::STATUS_AP;
      }
      else if ( nextCommand == Commands::RECYCLE_CARD )
      {
        sendData('C', 'P'); // 0x2 0x30 0x30 0x0 0x2 0x43 0x50 0x3 0x12
        nextCommand = Commands::STATUS_AP;
        digitalWrite(buttonPin, HIGH);
        delay(1000);
        digitalWrite(buttonPin, LOW);
      }
      else // Commands::STATUS_AP
      {
        sendData('A', 'P'); // 0x2 0x30 0x30 0x0 0x2 0x41 0x50 0x3 0x12
        setReceiveCounter++;
        if(setReceiveCounter >= 10)
        {
          setReceiveCounter = 0;
          nextCommand = Commands::RECEIVE_CARD_FC8;
        }
      }


      state = States::WAITING;
      waitCounter = 0;
      Serial.print("Waiting ");
    }
    else if ( state == States::WAITING )
    {
      waitCounter++;
      Serial.print(" ");
      if (waitCounter > 2 )
      {
        Serial.println("Timeout!");
        state = States::SENDING;
        nextCommand = Commands::STATUS_AP;
      }
      else
      {
        Serial.print(waitCounter);
      }
    }
  }

  // Always print received data
  if (mySerial.available() > 0) {
    digitalWrite(LEDPIN, HIGH); // turn on LED to show we got data
    Serial.print("Received: ");
    int data[20];
    int len = 0;
    while ( mySerial.available() > 0 )
    {
      char ch = mySerial.read();
      Serial.print(" 0x");
      Serial.print(ch, HEX);
      data[len] = ch;
      len++;
    }
    Serial.println("");

    if ( len == 3 && data[0] == 0x06 && data[1] == 0x30 && data[2] == 0x30 ) // ACK
    {
      sendACK();
    }
    else if ( len == 3 && data[0] == 0x15 && data[1] == 0x30 && data[2] == 0x30 ) // NAK
    {
      state = States::SENDING;
      Serial.println("NAK???");
    }
    else if ( len > 3 ) // Status bytes returned from the machine
    {
      // 0  1  2  3  4  5  6  7  8  9  0  1  2
      // 02 30 30 00 06 53 46 30 30 31 31 03 12
      // 02 30 30 00 06 53 46 30 30 30 31 3 13
      // 02 30 30 06 53 46 30 30 30 31 03 13

      // #7  For mange kort?  30 = ok, 31 = for mange
      // #8
      // #9  Kort i maskinen: 30 = nok kort, 31 = tom for kort, 38 = helt full
      // #10 Utmating:        30 = ingen kort, 31 = kort i utmater, 33 kort i Hopper
      bool isThereCardInHopper = (data[10] == 0x33);
      bool weHaveProblem = false;

      Serial.println("");
      Serial.print("isThereCardInHopper: ");
      Serial.print(isThereCardInHopper);
      Serial.print(" setReceiveCounter: ");
      Serial.println(setReceiveCounter);
      Serial.println("");
      if ( isThereCardInHopper )
      {
        nextCommand = Commands::RECYCLE_CARD;
        Serial.println("RECYCLE_CARD!");
      }
      else if ( weHaveProblem )
      {
        soundTheAlarm();
      }
      else
      {
        
      }
      state = States::SENDING;
    }
    else
    {
      nextCommand = Commands::STATUS_AP;
      Serial.println("");
      state = States::SENDING;
    }
  } else {
    digitalWrite(LEDPIN, LOW);
  }
  counter++;
}
