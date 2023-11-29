// HCS300 decoder //

#define HCS_RECIEVER_PIN  2    //connected to 433Mhz receiver
const int HCS_TRANSMITION_PIN = 8; // Pino de transmissão
const int button = 4;
// unsigned long decimal = 1957523005; // Número decimal para converter

class HCS300 {
  public:
    unsigned BatteryLow : 1; 
    unsigned Repeat : 1;
    unsigned Btn1 : 1;
    unsigned Btn2 : 1; 
    unsigned Btn3 : 1; 
    unsigned Btn4 : 1;
    unsigned long SerialNum;
    unsigned long Encript;
    void print();
};

volatile boolean  HCS_Listening = true;   
byte              HCS_preamble_count = 0;
uint32_t          HCS_last_change = 0;
uint32_t          HCS_start_preamble = 0;
uint8_t           HCS_bit_counter;        
uint8_t           HCS_bit_array[66];     
uint32_t          HCS_Te = 400;                //Typical Te duration
uint32_t          HCS_Te2_3 = 600;             //HCS_TE * 3 / 2
unsigned long     millisStart = 0;

HCS300 hcs300;

void setup()
{
  Serial.begin(115200);
  pinMode(button, INPUT_PULLUP); 
  pinMode(HCS_TRANSMITION_PIN, OUTPUT);
  pinMode(HCS_RECIEVER_PIN, INPUT);
  //aciona a interrupção (HCS_interrupt) sempre que o estado do pino mudar
  attachInterrupt(digitalPinToInterrupt(HCS_RECIEVER_PIN), HCS_interrupt, CHANGE);
  
  Serial.println("Setup OK");
}

void loop()
{
  long CurTime = millis();
  HCS300 msg;
  
  if(HCS_Listening == false) {
   
    memcpy(&msg,&hcs300,sizeof(HCS300));

    //do something
    msg.print();
    if(msg.SerialNum == 4876246) {       //if remote serial matches
      millisStart = CurTime;
    }
    
    //listen for another command
    HCS_Listening = true;
  }
  if (digitalRead(button) == LOW) { // Verifica se o botão foi pressionado
      String binary = decimalToBinary(msg.Encript);
      
      Serial.println(binary); // Imprime o número binário no Serial Monitor

      transmitBinary(binary); // Transmite o número binário

      delay(500); // Debounce - pequena pausa para evitar leituras múltiplas do botão
  }
}
 
//print data from HCS300
//armazena informações sobre quais botões foram pressionados
void HCS300::print(){
  
  String btn;
  if (Btn1 == 1) btn += "B1 ";
  if (Btn2 == 1) btn += "B2 ";
  if (Btn3 == 1) btn += "B3 ";
  if (Btn4 == 1) btn += "B4 ";

  String it2;
  it2 += "Encript=";
  it2 += Encript;
  it2 += " Serial=";
  it2 += SerialNum;
  it2 += " Button=";
  it2 += btn;
  it2 += " BatteryLow=";
  it2 += BatteryLow;
  it2 += " Rep=";
  it2 += Repeat;

  Serial.println(it2);
  
}

//new data
void HCS_interrupt() {

  if(HCS_Listening == false) {
    return;
  }

  uint32_t cur_timestamp = micros();
  uint8_t  cur_status = digitalRead(HCS_RECIEVER_PIN);
  uint32_t pulse_duration = cur_timestamp - HCS_last_change;
  HCS_last_change = cur_timestamp;


  // gets preamble
  // O preâmbulo sinaliza o início de uma nova transmissão de dados. No caso de um controle remoto de RF, por exemplo, o preâmbulo indica que o controle começou a enviar um comando.
  // O preâmbulo ajuda o receptor (neste caso, o dispositivo com o Arduino ou sistema similar que está lendo o sinal do controle remoto) a sincronizar-se com o transmissor. Isso é crucial para garantir que o receptor interprete corretamente os bits de dados que seguem o preâmbulo.
  //  Após detectar e processar o preâmbulo, o receptor está pronto para começar a decodificar os dados reais que seguem
  //  após a detecção do preâmbulo completo, o sistema calcula valores como HCS_Te e HCS_Te2_3, que são provavelmente usados para interpretar a duração e o espaçamento dos bits na transmissão real. Isso é essencial para a correta interpretação dos dado
  
if(HCS_preamble_count < 12) {
    if(cur_status == HIGH){
      //Se a duração do pulso estiver entre 150 e 500 microssegundos ou é o primeiro pulso do preâmbulo
      if( ((pulse_duration > 150) && (pulse_duration < 500)) || HCS_preamble_count == 0) {
        if(HCS_preamble_count == 0){
          HCS_start_preamble = cur_timestamp;
        }
      } else {
        //reinicio da contagem do preâmbulo
        HCS_preamble_count = 0;
        goto exit; 
      }
    } else {
      // verifica se a duração do pulso está entre 300 e 600 microssegundos.
      if((pulse_duration > 300) && (pulse_duration < 600)) {
         // Se estiver, incrementa HCS_preamble_count. 
        HCS_preamble_count ++;
        // Se HCS_preamble_count atingir 12, isso indica que o preâmbulo foi completamente recebido
        if(HCS_preamble_count == 12) {
         // calcula a duração total do preâmbulo
          HCS_Te = (cur_timestamp - HCS_start_preamble) / 23;
          HCS_Te2_3 = HCS_Te * 3 / 2;
          HCS_bit_counter = 0;
          goto exit; 
        }
      } else {
        HCS_preamble_count = 0;
        goto exit; 
      }
    }
  }
  
  // gets data
  if(HCS_preamble_count == 12) {
    if(cur_status == HIGH){
      if(((pulse_duration > 250) && (pulse_duration < 900)) || HCS_bit_counter == 0){
        // beginning of data pulse
      } else {
        // incorrect pause between pulses
        HCS_preamble_count = 0;
        goto exit; 
      }
    } else {
      // end of data pulse
      if((pulse_duration > 250) && (pulse_duration < 900)) {
        HCS_bit_array[65 - HCS_bit_counter] = (pulse_duration > HCS_Te2_3) ? 0 : 1;
        HCS_bit_counter++;  
        if(HCS_bit_counter == 66){
          // all bits captured
          HCS_Listening = false; 
          HCS_preamble_count = 0;

          hcs300.Repeat = HCS_bit_array[0];
          hcs300.BatteryLow = HCS_bit_array[1];
          hcs300.Btn1 = HCS_bit_array[2];
          hcs300.Btn2 = HCS_bit_array[3];
          hcs300.Btn3 = HCS_bit_array[4];
          hcs300.Btn4 = HCS_bit_array[5];

          hcs300.SerialNum = 0;
          for(int i = 6; i < 34;i++){
            hcs300.SerialNum = (hcs300.SerialNum << 1) + HCS_bit_array[i];
          };

          uint32_t Encript = 0;
          for(int i = 34; i < 66;i++){
             Encript = (Encript << 1) + HCS_bit_array[i];
          };
          hcs300.Encript = Encript;
        }
      } else {
        HCS_preamble_count = 0;
        goto exit; 
      }
    }
  }
  exit:;
}

String decimalToBinary(unsigned long num) {
  String binaryStr = "";
  
  for (int i = 31; i >= 0; i--) {
    unsigned long mask = 1ul << i;
    binaryStr += (num & mask) ? "1" : "0";
  }

  return binaryStr;
}

void transmitBinary(String binaryStr) {
  for (int i = 0; i < binaryStr.length(); i++) {
    if (binaryStr.charAt(i) == '1') {
      digitalWrite(HCS_TRANSMITION_PIN, HIGH);
    } else {
      digitalWrite(HCS_TRANSMITION_PIN, LOW);
    }
    delayMicroseconds(1000); 
  }
}

