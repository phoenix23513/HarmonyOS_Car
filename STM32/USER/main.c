#include "stm32f10x.h"
#include "sys.h"

int main(void)
{
    u8 i;

    Stm32_Clock_Init(9);
    MY_NVIC_PriorityGroupConfig(2);
    uart_init(115200);

    JTAG_Set(JTAG_SWD_DISABLE);
    JTAG_Set(SWD_ENABLE);

    colorful_led_Init();

    printf("LED CONTROL READY\r\n");
    printf("HELLO = start, STOP! = stop\r\n");

    while(1)
    {
        if(USART_RX_STA == 1)
        {
            printf("LED START\r\n");

            R_led_mode();       /* 点亮后灯 */
            L_led_mode();       /* 前灯彩色循环，收到 STOP! 后返回 */
        }

        if(USART_RX_STA == 2)
        {
            /* 关闭六颗前灯 */
            for(i = 1; i <= led_num; i++)
            {
                L_ws2812_rgb(i, WS_DARK);
            }
            L_ws2812_refresh(led_num);

            /* 关闭六颗后灯 */
            R_led_CLC();

            USART_RX_STA = 0;
            printf("LED OFF\r\n");
        }

        delay_ms(100);
    }
}
