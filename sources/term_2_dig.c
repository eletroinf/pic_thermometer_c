/*****************************************************************************
Termômetro simples utilizando o Sensor TMP36, com um PIC16F676.
São usados 2 dígitos (display a LED) para mostrar a temperatura,
de 0 °C a 99 °C.
-Sensor TMP36: 500mV @ 0 °C + 10mV/°C.
-Display: Anodo Comum. Não é usado o DP (Ponto).
-Conversor AD 10bit, Vref+ = VDD (5V).

********ATENÇÃO**********
O led ligado no pino RA1 é via capacitor, assim controlo o brilho variando a
frequencia, através do TIMER1.
Com um capacitor de 100nF, temos:
XC ~= 120R @ 13.269 KHz.
XC ~= 1700R @ 1 KHz.
******************************************************************************/

/************* Ricardo B. Morim **********************************************
Data: 05/2007 */

#include "term_2_dig.h"


#use fast_io(A)   //Configuração de Entrada/Saída manual.
#use fast_io(C)


#byte porta = 0x05
#byte portc = 0x07
#byte adcon0= 0x1F
#byte adcon1= 0x9F
#byte ansel = 0x91


#define  traco    10    //Para acessar o '-' na tabela.
#define  graus    11    //'°' na tabela.
#define  letra_c  12    //'C' na tabela.
#define  letra_h  13    //'H' na tabela.
#define  apagado  14    //Display apagado.

#bit  d1 = porta.4      //Controla Anodo Comum das unidades.
#bit  d0 = porta.5      //Controla Anodo Comum das dezenas.
#bit  seg_a = porta.2   //O segmento A do display fica em RA2.
#bit  botao = porta.3   //Botão com pull-up.
#bit  led   = porta.1   //Saída onde está conectado o led.
//*****************************************************************************
//Variáveis globais
unsigned char dados_display[2] = {0, 0};  //Dígitos a serem mostrados no display.
unsigned char pwm = 0;     //Valor para o pwm via software.

unsigned char flags = 0;   //Variável para flags.
#bit  flg_mux  = flags.0   //Para alternar entre os 2 dígitos na multiplexação.
#bit  flg_int  = flags.1   //Para mexer na variável dos díg. imediatamente após uma int.

//*****************************************************************************
//Tabela para traduzir o valor para os dígitos do display.
//Display ANODO COMUM.
//Bit:   7 6 5 4 3 2 1 0
//Seg.     A D E F G C B  ->O bit 7 Não é usado.
const unsigned char tab_display[] =
{
   0b00000100,    //0
   0b01111100,    //1
   0b00001010,    //2
   0b00011000,    //3
   0b01110000,    //4
   0b00010001,    //5
   0b00000001,    //6
   0b00111100,    //7
   0b00000000,    //8
   0b00010000,    //9
   0b01111011,    //-
   0b00110010,    //°
   0b00000111,    //C
   0b01100000,    //H
   0b01111111     //apagado
};

//*****************************************************************************
void byte_asc(unsigned char n)
{
   dados_display[1] = n % 10;       //Unidades.
   dados_display[0] = (n/10) % 10;  //Dezenas.
}

//*****************************************************************************
//Interrupção do Timer 0: A cada 2ms, para multiplexar o display.
#int_RTCC
RTCC_isr()
{
   static unsigned char pwm_ctrl = 0;

   porta &= 0b11001111;    //Desliga os dois Anodos comuns dos dígitos (A5 e A4).

   if(flg_mux)
   {
      flg_mux  = 0;
      portc = dados_display[0];
      if(dados_display[0] & 64)  seg_a = 1;
      else                       seg_a = 0;
      d1 = 1;     //Mostra Dezenas.
   } else
      {
         flg_mux = 1;
         portc = dados_display[1];
         if(dados_display[1] & 64)  seg_a = 1;
         else                       seg_a = 0;
         d0 = 1;  //Mostra unidades.
      }

   //Para gerar o pwm de controle de brilho do LED.
   if(pwm_ctrl)   pwm_ctrl--;
   else           pwm_ctrl = 7;
   if(pwm_ctrl < pwm)   led = 1;
   else                 led = 0;

   flg_int = 0;   //Limpa flag.
}

//*****************************************************************************
//Função para ler a temperatura através de um sensor TMP36.
//500 mV @ 0°C.,
//Com Vref = 5.1V -> 4.98 mV/bit -> 500mV ~= 100 no ADC.
//Temperatura de 0°C 99°C -> 0V a 990 mV, ADC: 0 a 202.
unsigned char ler_temperatura(void)
{
   static unsigned char sensor[16];
   static unsigned char pt_sensor = 15;
   unsigned int16 soma_sensor;
   unsigned char temp;

   sensor[pt_sensor] = (read_adc() - 100);
   if(pt_sensor)  pt_sensor--;
   else           pt_sensor = 15;

   soma_sensor =0;
   for(temp=0; temp <16; temp++) soma_sensor += sensor[temp];    //Soma as leituras.
   soma_sensor >>= 4;   //Divide pelo número de leituras (16).
   soma_sensor *= 50;   //Multiplica * 50 (~4.887 mV/bit).
   //Atenção: Soma 50 para que, se a temperatura estiver em 17.6, por ex. mostra 18 °C.
   //Se estiver em 17.4, mostra 17 °C.
   soma_sensor += 50;

   return (soma_sensor/100);
}

//*****************************************************************************
//Função que trata o botão.
void trata_botao(void)
{
   if(pwm < 7)
   {
      pwm += 2;         //Pula de 2 em 2.
   } else
      {
         pwm = 0;       //Desliga o LED.
      }
   while(!botao);
   delay_ms(100);
}

//*****************************************************************************
void main()
{
   unsigned char temperatura;
   unsigned char conta;

   set_tris_a(0b00001001);    //I/Os portA.
   set_tris_c(0);             //PortC todo Saída.
   ansel = 1;  //Somente AN0 analógico.
   adcon1= 0b01010000;  //Clock AD /16.
   adcon0= 0b10000001;  //AD ligado, seleciona canal AN0, Vref=VDD.
   setup_timer_0(RTCC_INTERNAL|RTCC_DIV_8);
   setup_timer_1(T1_DISABLED);
   setup_comparator(NC_NC_NC_NC);
   setup_vref(FALSE);
   enable_interrupts(INT_RTCC);
   enable_interrupts(GLOBAL);

   delay_ms(3000);   //3 segundos na inicialização.

   //Inicializa a função -tem que ser chamada pelo menos 16x para ler certo.
   for(conta = 35; conta; conta--)  temperatura = ler_temperatura();

   dados_display[0] = tab_display[apagado];
   dados_display[1] = tab_display[apagado];
   delay_ms(1000);   //Apaga dígitos por 1 segundo.

   conta = 0;
   while(1)
   {
      temperatura = ler_temperatura();    //Lê a temperatura.
      flg_int = 1;
      while(flg_int);      //Aguarda passar por uma interrupção de mux dos digitos.
      if(conta < 20)
      {
         if(temperatura > 99)
         {
            dados_display[0] = tab_display[letra_h];  //Acima de 99 °C mostra "HH".
            dados_display[1] = tab_display[letra_h];
         } else
            {
               byte_asc(temperatura);
               dados_display[0] = tab_display[dados_display[0]];  //Ajusta pela tabela para
               dados_display[1] = tab_display[dados_display[1]];  //poder mostrar no display.
               conta++;
            }
      }else
         {
            dados_display[0] = tab_display[graus];
            dados_display[1] = tab_display[letra_c];
            if(conta < 25) conta++;
            else           conta = 0;
         }

      if(!botao)  trata_botao();
      delay_ms(100);
      if(!botao)  trata_botao();     //Se o botão foi pressionado...
      delay_ms(100);
      if(!botao)  trata_botao();
      delay_ms(50);

   }//while(1)
}//main()

/*****************************************************************************/


