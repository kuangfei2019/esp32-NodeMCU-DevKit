// ͷ�ļ�����
//==============================================================================
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "driver/i2c.h"

#include "oled.h"
#include "oledfont.h"	// �ַ���

#define I2C_EXAMPLE_MASTER_SCL_IO           22                /*!< gpio number for I2C master clock */
#define I2C_EXAMPLE_MASTER_SDA_IO           21               /*!< gpio number for I2C master data  */
#define I2C_EXAMPLE_MASTER_NUM              I2C_NUM_0        /*!< I2C port number for master dev */
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE   0                /*!< I2C master do not need buffer */
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE   0                /*!< I2C master do not need buffer */
#define WRITE_BIT                           I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT                            I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN                        0x1              /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                       0x0              /*!< I2C master will not check ack from slave */
#define ACK_VAL                             0x0              /*!< I2C ack value */
#define NACK_VAL                            0x1              /*!< I2C nack value */
#define LAST_NACK_VAL                       0x2              /*!< I2C last_nack value */
#define OLED_ADDR			    0x78
#define I2C_MASTER_FREQ_HZ    100000

static const char *TAG = "main";

/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_example_master_init()
{
    int i2c_master_port = I2C_EXAMPLE_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, I2C_EXAMPLE_MASTER_RX_BUF_DISABLE, I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);
}

//==============================================================================

// OLED��غ���
//==============================================================================
// IIC��ʼ��( SCL==IO14��SDA==IO2 )
//-----------------------------------------------------------
void IIC_Init_JX(void)
{
	i2c_example_master_init();
}
//-----------------------------------------------------------

// ��OLEDд��ָ���ֽ�
//----------------------------------------------------------------------------
uint8_t OLED_Write_Command(uint8_t OLED_Byte)
{
    int ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, OLED_ADDR, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, 0x00, ACK_CHECK_EN); // 0x00 command reg
    i2c_master_write_byte(cmd, OLED_Byte, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}
//----------------------------------------------------------------------------

// ��OLEDд�������ֽ�
//----------------------------------------------------------------------------
uint8_t OLED_Write_Data(uint8_t OLED_Byte)
{
    int ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, OLED_ADDR, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, 0x40, ACK_CHECK_EN); // 0x40 data reg
    i2c_master_write_byte(cmd, OLED_Byte, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}
//----------------------------------------------------------------------------

// ��OLEDд��һ�ֽ�����/ָ��
//---------------------------------------------------------------
void OLED_WR_Byte(uint8_t OLED_Byte, uint8_t OLED_Type)
{
	if(OLED_Type)
		OLED_Write_Data(OLED_Byte);		// д������
	else
		OLED_Write_Command(OLED_Byte);	// д��ָ��
}
//---------------------------------------------------------------

// ����д��ĳֵ
//------------------------------------------------------------------------
void OLED_Clear(void)
{
	uint8_t N_Page, N_row;

	for(N_Page=0; N_Page<8; N_Page++)
	{
		OLED_WR_Byte(0xb0+N_Page,OLED_CMD);	// ��0��7ҳ����д��
		OLED_WR_Byte(0x00,OLED_CMD);      	// �е͵�ַ
		OLED_WR_Byte(0x10,OLED_CMD);      	// �иߵ�ַ

		for(N_row=0; N_row<128; N_row++)OLED_WR_Byte(0x00,OLED_DATA);
	}
}
//------------------------------------------------------------------------

// ��������д�����ʼ�С���
//------------------------------------------------------------------------
void OLED_Set_Pos(uint8_t x, uint8_t y)
{
	OLED_WR_Byte(0xb0+y,OLED_CMD);				// д��ҳ��ַ

	OLED_WR_Byte((x&0x0f),OLED_CMD);  			// д���еĵ�ַ(�Ͱ��ֽ�)

	OLED_WR_Byte(((x&0xf0)>>4)|0x10,OLED_CMD);	// д���еĵ�ַ(�߰��ֽ�)
}
//------------------------------------------------------------------------

// ��ʼ��OLED
//-----------------------------------------------------------------------------
void OLED_Init(void)
{
	IIC_Init_JX();					// ��ʼ��IIC

	vTaskDelay(100 / portTICK_RATE_MS);

	OLED_WR_Byte(0xAE,OLED_CMD);	// �ر���ʾ

	OLED_WR_Byte(0x00,OLED_CMD);	// ���õ��е�ַ
	OLED_WR_Byte(0x10,OLED_CMD);	// ���ø��е�ַ
	OLED_WR_Byte(0x40,OLED_CMD);	// ������ʼ�е�ַ
	OLED_WR_Byte(0xB0,OLED_CMD);	// ����ҳ��ַ

	OLED_WR_Byte(0x81,OLED_CMD); 	// �Աȶ����ã�����������
	OLED_WR_Byte(0xFF,OLED_CMD);	// 265

	OLED_WR_Byte(0xA1,OLED_CMD);	// ���ö�(SEG)����ʼӳ���ַ
	OLED_WR_Byte(0xA6,OLED_CMD);	// ������ʾ��0xa7����ʾ

	OLED_WR_Byte(0xA8,OLED_CMD);	// ��������·����16~64��
	OLED_WR_Byte(0x3F,OLED_CMD);	// 64duty

	OLED_WR_Byte(0xC8,OLED_CMD);	// ��ӳ��ģʽ��COM[N-1]~COM0ɨ��

	OLED_WR_Byte(0xD3,OLED_CMD);	// ������ʾƫ��
	OLED_WR_Byte(0x00,OLED_CMD);	// ��ƫ��

	OLED_WR_Byte(0xD5,OLED_CMD);	// ����������Ƶ
	OLED_WR_Byte(0x80,OLED_CMD);	// ʹ��Ĭ��ֵ

	OLED_WR_Byte(0xD9,OLED_CMD);	// ���� Pre-Charge Period
	OLED_WR_Byte(0xF1,OLED_CMD);	// ʹ�ùٷ��Ƽ�ֵ

	OLED_WR_Byte(0xDA,OLED_CMD);	// ���� com pin configuartion
	OLED_WR_Byte(0x12,OLED_CMD);	// ʹ��Ĭ��ֵ

	OLED_WR_Byte(0xDB,OLED_CMD);	// ���� Vcomh���ɵ������ȣ�Ĭ�ϣ�
	OLED_WR_Byte(0x40,OLED_CMD);	// ʹ�ùٷ��Ƽ�ֵ

	OLED_WR_Byte(0x8D,OLED_CMD);	// ����OLED��ɱ�
	OLED_WR_Byte(0x14,OLED_CMD);	// ����ʾ

	OLED_WR_Byte(0xAF,OLED_CMD);	// ����OLED�����ʾ

	OLED_Clear();        			// ����

	OLED_Set_Pos(0,0); 				// ��������д�����ʼ�С���
}

// ��ָ�����괦��ʾһ���ַ�
//-----------------------------------------------------------------------------
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t Show_char)
{
	uint8_t c=0,i=0;

	c=Show_char-' '; 				// ��ȡ�ַ���ƫ����

	if(x>Max_Column-1){x=0;y=y+2;}	// ������������Χ��������2ҳ

	if(SIZE ==16) 					// �ַ���СΪ[8*16]��һ���ַ�����ҳ
	{
		// ����һҳ
		//-------------------------------------------------------
		OLED_Set_Pos(x,y);						// ���û�����ʼ��
		for(i=0;i<8;i++)  						// ѭ��8��(8��)
		OLED_WR_Byte(F8X16[c*16+i],OLED_DATA); 	// �ҵ���ģ

		// ���ڶ�ҳ
		//-------------------------------------------------------
		OLED_Set_Pos(x,y+1); 					// ҳ����1
		for(i=0;i<8;i++)  						// ѭ��8��
		OLED_WR_Byte(F8X16[c*16+i+8],OLED_DATA);// �ѵڶ�ҳ����
	}
}
//-----------------------------------------------------------------------------

// ��ָ��������ʼ����ʾ�ַ���
//-------------------------------------------------------------------
void OLED_ShowString(uint8_t x, uint8_t y, uint8_t * Show_char)
{
	uint8_t N_Char = 0;		// �ַ����

	while (Show_char[N_Char]!='\0') 	// ����������һ���ַ�
	{
		OLED_ShowChar(x,y,Show_char[N_Char]); 	// ��ʾһ���ַ�

		x += 8;					// ������8��һ���ַ�ռ8��

		if(x>=128){x=0;y+=2;} 	// ��x>=128������һҳ

		N_Char++; 				// ָ����һ���ַ�
	}
}
//-------------------------------------------------------------------


// ��ָ��λ����ʾIP��ַ(���ʮ����)
//-----------------------------------------------------------------------------
void OLED_ShowIP(uint8_t x, uint8_t y, uint8_t *Array_IP)
{
	uint8_t N_IP_Byte = 0;		// IP�ֽ����

	// ѭ����ʾ4��IP�ֽ�(�ɸߵ����ֽ���ʾ)
	//----------------------------------------------------------
	for(; N_IP_Byte<4; N_IP_Byte++)
	{
		// ��ʾ��λ/ʮλ
		//------------------------------------------------------
		if(Array_IP[N_IP_Byte]/100)		// �жϰ�λ?=0
		{
			OLED_ShowChar(x,y,48+Array_IP[N_IP_Byte]/100); x+=8;

			// ��ʾʮλ����λ!=0��
			//---------------------------------------------------------
			//if(Array_IP[N_IP_Byte]%100/10)
			{ OLED_ShowChar(x,y,48+Array_IP[N_IP_Byte]%100/10); x+=8; }

		}

		// ��ʾʮλ����λ==0��
		//---------------------------------------------------------
		else if(Array_IP[N_IP_Byte]%100/10)		// �ж�ʮλ?=0
		{ OLED_ShowChar(x,y,48+Array_IP[N_IP_Byte]%100/10); x+=8; }


		// ��ʾ��λ
		//---------------------------------------------------------
		//if(Array_IP[C_IP_Byte]%100%10)
		{ OLED_ShowChar(x,y,48+Array_IP[N_IP_Byte]%100%10); x+=8; }

		// ��ʾ��.��
		if(N_IP_Byte<3)
		{ OLED_ShowChar(x,y,'.'); x+=8; }
	}
}
//-----------------------------------------------------------------------------
//==============================================================================
