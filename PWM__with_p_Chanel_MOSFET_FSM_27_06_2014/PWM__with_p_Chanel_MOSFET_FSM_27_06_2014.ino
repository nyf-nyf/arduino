/* Simple Arduino Solar Charger Controller
     PWM Version with P-chanel MOSFET
                       Ghost D. 2014
====================================
И окончательный алгоритм (спасибо HWman, с ресурса ARDUINO.RU) будет такой:
1- ШИМ=255 пока Uакб<14.4
2- Как только Uакб=>14.4 ставим ШИМ при котором Uакб=13.8 и держим напряжение на таком уровне
3- Если значение ШИМ уже очень мало(ток заряда уже маленький), то отключаем заряд вообще и переходим в ждущий режим 
4- как Uакб<=12.7 - снова с пункта 1
====================================
Lead-Acid Batteries

BOOST Voltage = 14.4 Volts
NORMAL Voltage = 13.9 Volts
STORAGE Voltage = 13.3 Volts

*/
#include <LiquidCrystal.h>
LiquidCrystal lcd(12,11, 5, 4, 3, 2);
int redLed=7;             // Светодиод - состояние АКБ - разряжен
int greenLed=8;           // Светодиод - состояние заряда
int pwmPin=6;             // ШИМ вывод на MOSFET
byte batPin=A0;           // Аналговый порт подключения делителя с АКБ
byte solarPin=A1;         // Аналоговый порт подключения делителя с АКБ

float solar_volt =0; // Напряжение на солнечной панели
float bat_volt=0;    // Напряжение на АКБ
float real_bat_volt=0;    // Напряжение на АКБ при ШИМ=0
float start_bat_volt=0; //Напряжение на АКБ, с которого стартовал режим REFRESH (Восстановление)
byte curPWM=0;  //Текущее значение ШИМ
int charged_percent =0; //Заряд АКБ в %
volatile boolean state = LOW; //для мигания светодиодом
//long int timeWork=0;
byte mode=0; //Режим работы "КОНЕЧНОГО АВТОМАТА"
byte tmpCount=0; //счетчик, для возможности "крутиться" в нужном режиме нужное кол-во раз
char outMess[16]; //содержимое второй строки на LCD

//коэф. делителя для 100к/47к
#define batDev 3.2
//коэф. делителя для 100к/22к
#define solDev 6.0
//коэфициент для пересчета данных с АЦП в вольты
#define adc2volt 0.00485

//Границы напряжения на АКБ
#define bat_full_volt 14.4
#define bat_balance_volt 13.8
#define bat_disch_volt 11.4
#define bat_charge_volt 13.0
#define bat_storage_volt 12.7
#define bat_refresh_volt 9 

//Значения для ШИМ
#define pwm_charge 255
#define pwm_balance 0
#define pwm_off 0
//шаг изменения значения ШИМ
#define deltaPWM 10

//режимы работы
#define mode_charge 1
#define mode_balance 2
#define mode_storage 3
#define mode_sleep 4
#define mode_refresh 5


//-----------------------------------
// Чтение значения АЦП с аналогового порта
// 250 замеров и подсчет среднего
float readVolts(byte pin){
  float tmpRead=0;
  for (int i=0;i<250;i++){
    tmpRead+=analogRead(pin);
  }
  tmpRead=tmpRead/250;
  return(tmpRead*adc2volt);
}
//-----------------------------------
// считываем значения напряжений на СП и АКБ
void read_U()
{
  solar_volt=(readVolts(solarPin))*solDev;
  bat_volt=(readVolts(batPin))*batDev;
}

// считываем значения напряжения на АКБ при ШИМ=0
void read_U_bat_real()
{
  analogWrite(pwmPin,0);  
  real_bat_volt=(readVolts(batPin))*batDev;
  analogWrite(pwmPin,curPWM);
 }

void setup()
{
  TCCR0B = TCCR0B & 0b11111000 | 0x05; // Устанавливаем ШИМ 61.03 Hz (при этом портиться функция delay)
  Serial.begin(9600);
  pinMode(pwmPin,OUTPUT);
  pinMode(redLed,OUTPUT);
  pinMode(greenLed,OUTPUT);
  
  digitalWrite(pwmPin,LOW);
  digitalWrite(redLed,LOW);
  digitalWrite(greenLed,LOW);

  lcd.begin(16,2);                     // 
  lcd.clear();                         // 
  
  read_U();
   mode=mode_sleep;

} //end setup


void loop()
{
  
switch (mode){
  
  case (mode_charge):
  // Режим ЗАРЯДА. ШИМ=100% 
  // В этом режиме горит красный светодиод, зеленый мигает
  // Пока напряжение на АКБ напряжение не станет больше, чем 14.4Вольта
  // после этого переход в режим ВЫРАВНИВАЮЩЕГО заряда
  Serial.println("==============");
  Serial.println("MODE: Charge");
  Serial.println("==============");
  
    curPWM=pwm_charge;
    digitalWrite(redLed, HIGH);
    digitalWrite(greenLed,state);
    state = !state;
    if (bat_volt >= bat_full_volt ) 
      {
        curPWM=pwm_balance;
        mode=mode_balance;
        tmpCount=3;
      }
   sprintf(outMess, "%2d%% CHRG PWM=%3d",charged_percent, (map(curPWM,pwm_off,pwm_charge,0,100)));
  break;
  
  case (mode_balance):
  // Режим ВЫРАВНЕВАЮЩЕГО ЗАРЯДА. ШИМ= динамический, для удержания напряжения на АКБ =13,8В 
  // В этом режиме красный и зеленый светодиоды перемигиваются
  // При значении ШИМ меньше, чем 10
  // переход в режим ПОДДЕРЖИВАЮЩЕГО заряда
  // после изменения ШИМ, "для устаканивания" значения делаем 3 прохода
  Serial.println("==============");
  Serial.print("MODE: Balance/Count: ");
  Serial.println(tmpCount);
  Serial.println("==============");
  
      if (tmpCount==0)
    {
      if (bat_volt >= bat_balance_volt)
        {
          if (curPWM < deltaPWM) curPWM=pwm_off; 
          if (curPWM != pwm_off) curPWM=curPWM-deltaPWM;
           
           Serial.print("- Decrise. ");
           Serial.println(curPWM);
        }
      
      if (bat_volt < bat_balance_volt)
        {
          if(curPWM > (pwm_charge-deltaPWM)) curPWM=pwm_charge;
          if(curPWM != pwm_charge) curPWM=curPWM+deltaPWM;
          
           Serial.print("+ Incrise. ");
           Serial.println(curPWM);
        }
        tmpCount=3;
    }

    digitalWrite(redLed, state);
    digitalWrite(greenLed,!state);
    state = !state;
    tmpCount=tmpCount-1;
//    if (curPWM < 10) mode=mode_storage; 
    sprintf(outMess, "Balance. PWM=%3d", (map(curPWM,pwm_off,pwm_charge,0,100)));   
   break;
  
//  case (mode_storage):
//  // Режим ПОДДЕРЖИВАЮЩЕГО ЗАРЯДА. ШИМ=0 
//  // В этом режиме горит зеленый светодиод, красный мигает
//  // Пока напряжение на АКБ напряжение станет меньше, чем 12.7 Вольта
//  // переход в режим ЗАРЯДА
//  Serial.println("==============");
//  Serial.println("MODE: Storage");
//  Serial.println("==============");
//  
//    curPWM=pwm_off;
//    if (bat_volt < bat_storage_volt) mode=mode_charge;
//    digitalWrite(greenLed, HIGH);
//    digitalWrite(redLed,state);
//    state = !state;
//    sprintf(outMess, "Storage Mode     ");
//  break;
  
  case (mode_sleep):
  // Режим СНА. ШИМ=0 
  // В этом режиме светодиоды не горят
  // Когда напряжение на СП станет больше, чем 14.4 Вольта
  // переход в режим ЗАРЯДА
  sprintf(outMess, "Sleep Mode      ");
  Serial.println("==============");
  Serial.println("MODE: Sleep");
  Serial.println("==============");
  
    curPWM=pwm_off;
    digitalWrite(redLed, LOW);
    digitalWrite(greenLed,LOW);
    if ((solar_volt > bat_charge_volt ) && (bat_volt >= bat_disch_volt)) mode=mode_charge;
    if ((solar_volt > bat_charge_volt ) && (bat_volt < bat_disch_volt) && (bat_volt >= bat_refresh_volt))
     { 
       mode=mode_refresh; 
       start_bat_volt=bat_volt;
       tmpCount=5;
     }
  break;
  
  case (mode_refresh):
  // Режим ВОССТАНОВЛЕНИЯ АКБ. ШИМ= ИМПУЛЬСЫ, соотношение времени 4/1 
  // В этом режиме красный и зеленый светодиоды вспыхивают
  // Выход из этого режима при достижении значения на АКБ bat_disch_volt
  // или если напр. на АКБ станет (вдруг, откл. АКБ) < bat_refresh_volt
  Serial.println("==============");
  Serial.println("MODE: Refresh");
  Serial.println("==============");
  lcd.setCursor(0,0);
  lcd.print("- Refresh Mode -");
  curPWM=pwm_charge;
    if (tmpCount==0)
  {
    tmpCount=5;
    curPWM=pwm_off;
  }
  tmpCount=tmpCount-1;
  lcd.setCursor(0,1);
  lcd.print("Ust:");
  lcd.print(start_bat_volt,1);
  lcd.print("  Uc:");
  lcd.print(real_bat_volt,1);
    
  if (real_bat_volt >= bat_disch_volt) mode=mode_charge;
 
    digitalWrite(redLed, state);
    digitalWrite(greenLed,state);
    state = !state;
  break;
} //end switch



//Общая процедура вывода информации (пока сумбурно)
  read_U();
  if (solar_volt < bat_charge_volt) mode=mode_sleep;
  read_U_bat_real();
  if (real_bat_volt<bat_refresh_volt) 
  {
    sprintf(outMess, "!Low Battery! :(");   
    mode=mode_sleep;
  }
 //analogWrite(pwmPin,curPWM);
  
//  if (curPWM==pwm_off) read_U_bat_real();
  Serial.print("SP  voltage :");
  Serial.println(solar_volt);
  Serial.print("AKB voltage :");
  Serial.println(bat_volt);
  Serial.print("Real AKB voltage :");
  Serial.println(real_bat_volt);
  Serial.print("PWM=");
  Serial.println(curPWM);
  Serial.print("pwm duty cycle is (%) :");
  Serial.println(map(curPWM,pwm_off,pwm_charge,0,100));
  charged_percent=map(bat_volt*10, (bat_disch_volt)*10, (bat_full_volt)*10, 0, 99);
  if (mode != mode_refresh) lcdShow();
  
  delay(100);
  
  Serial.println("----------------------------------------");

}// конец основного цикла

void lcdShow()
{
  lcd.setCursor(16,1);
  lcd.print(" ");
  lcd.setCursor(0,0);
  lcd.print("SP:");
  lcd.print(solar_volt,1);
  lcd.print(" BAT:");
  lcd.print(bat_volt,1);
  
  lcd.setCursor(0,1); 
  lcd.print(outMess);
}
