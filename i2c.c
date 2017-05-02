#include "i2c.h"
#include "msp430g2553.h"

#define I2CSEL   P1SEL
#define I2CSEL2  P1SEL2
#define I2CSDA   BIT6
#define I2CSCL   BIT7

void I2C_init(uint8_t slaveAddress){
    // Port Configuration
    I2CSEL |= I2CSDA + I2CSCL;                     // Assign I2C pins to USCI_B0
    I2CSEL2|= I2CSDA + I2CSCL;                     // Assign I2C pins to USCI_B0

//    isRx = 0;                                    // State variable - possibly useless

    //USCI Configuration
    UCB0CTL1 |= UCSWRST;                           // Enable SW reset
    UCB0CTL0 = UCMST + UCMODE_3 + UCSYNC;          // I2C Master, synchronous mode
    UCB0CTL1 = UCSSEL_2 + UCSWRST;                 // Use SMCLK, keep SW reset

    //Set USCI Clock Speed
    UCB0BR0 = 12;                                  // fSCL = SMCLK/12 = ~100kHz
    UCB0BR1 = 0;

    //Set Slave Address and Resume operation
    UCB0I2CSA = slaveAddress;                      // Slave Address passed as parameter
    UCB0CTL1 &= ~UCSWRST;                          // Clear SW reset, resume operation
}


void I2C_write(uint8_t ByteCtr, uint8_t *TxData) {
    __disable_interrupt();
//    isRx = 0;
    //Interrupt management
    IE2 &= ~UCB0RXIE;                              // Disable RX interrupt
//    while (UCB0CTL1 & UCTXSTP);                  // Ensure stop condition got sent
    IE2 |= UCB0TXIE;                               // Enable TX interrupt

    //Pointer to where data is stored to be sent
    PTxData = (uint8_t *) TxData;                  // TX array start address
    TxByteCtr = ByteCtr;                           // Load TX byte counter

    //Send start condition
    //    while (UCB0CTL1 & UCTXSTP);              // Ensure stop condition got sent
    UCB0CTL1 |= UCTR + UCTXSTT;                    // I2C TX, start condition

    __bis_SR_register(CPUOFF + GIE);               // Enter LPM0 w/ interrupts
    while (UCB0CTL1 & UCTXSTP);
}

void I2C_read(uint8_t ByteCtr, volatile uint8_t *RxData) {
    __disable_interrupt();
//    isRx = 1;

    //Interrupt management
    IE2 &= ~UCB0TXIE;                              // Disable TX interrupt
    UCB0CTL1 = UCSSEL_2 + UCSWRST;                 // Use SMCLK, keep SW reset
    UCB0CTL1 &= ~UCSWRST;                          // Clear SW reset, resume operation
    IE2 |= UCB0RXIE;                               // Enable RX interrupt

    //Pointer to where data will be stored
    PRxData = (uint8_t *) RxData;                  // Start of RX buffer
    RxByteCtr = ByteCtr;                           // Load RX byte counter

    //while (UCB0CTL1 & UCTXSTP);                  // Ensure stop condition got sent

    //If only 1 byte will be read send stop signal as soon as it starts transmission
    if(RxByteCtr == 1){
        UCB0CTL1 |= UCTXSTT;                       // I2C start condition
        while (UCB0CTL1 & UCTXSTT);                // Start condition sent?
        UCB0CTL1 |= UCTXSTP;                       // I2C stop condition
        __enable_interrupt();
    } else {
        UCB0CTL1 |= UCTXSTT;                       // I2C start condition
    }

    __bis_SR_register(CPUOFF + GIE);               // Enter LPM0 w/ interrupts
    while (UCB0CTL1 & UCTXSTP);                    // Ensure stop condition got sent
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = USCIAB0TX_VECTOR
__interrupt void USCIAB0TX_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCIAB0TX_VECTOR))) USCIAB0TX_ISR (void)
#else
#error Compiler not supported!
#endif
{
  if(IFG2 & UCB0RXIFG){                              // Receive In
  if (RxByteCtr == 1)
  {
     *PRxData = UCB0RXBUF;                           // Move final RX data to PRxData
     __bic_SR_register_on_exit(CPUOFF);              // Exit LPM0
  }
  else
  {
      *PRxData++ = UCB0RXBUF;                        // Move RX data to address PRxData
      if (RxByteCtr == 2)                            // Check whether byte is second to last to be read to send stop condition
      UCB0CTL1 |= UCTXSTP;
      __no_operation();
  }
  RxByteCtr--;                                       // Decrement RX byte counter
  }

  else{                                              // Master Transmit
      if (TxByteCtr)                                 // Check TX byte counter
  {
    UCB0TXBUF = *PTxData;                            // Load TX buffer
    PTxData++;
    TxByteCtr--;                                     // Decrement TX byte counter
  }
  else
  {
    UCB0CTL1 |= UCTXSTP;                             // I2C stop condition
    IFG2 &= ~UCB0TXIFG;                              // Clear USCI_B0 TX int flag
    __bic_SR_register_on_exit(CPUOFF);               // Exit LPM0
  }
 }
}



