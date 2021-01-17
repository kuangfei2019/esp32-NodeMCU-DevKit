#ifndef OLED_H_
#define OLED_H_

#define		OLED_CMD  	0		// ����
#define		OLED_DATA 	1		// ����

#define 	SIZE 		16		//��ʾ�ַ��Ĵ�С
#define 	Max_Column	128		//�������
#define		Max_Row		64		//�������
#define		X_WIDTH 	128		//X��Ŀ��
#define		Y_WIDTH 	64	    //Y��Ŀ��
#define		IIC_ACK		0		//Ӧ��
#define		IIC_NO_ACK	1		//��Ӧ��
//=============================================================================

// ��������
//=============================================================================
void IIC_Init_JX(void);
uint8_t OLED_Write_Command(uint8_t OLED_Byte);

uint8_t OLED_Write_Data(uint8_t OLED_Byte);

void OLED_WR_Byte(uint8_t OLED_Byte, uint8_t OLED_Type);

void OLED_Clear(void);

void OLED_Set_Pos(uint8_t x, uint8_t y);

void OLED_Init(void);

void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t Show_char);

void OLED_ShowString(uint8_t x, uint8_t y, uint8_t * Show_char);

//=============================================================================

#endif /* OLED_H_ */
