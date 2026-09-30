/*
 * circular_queue.h
 *
 *  Created on: 2026年9月12日
 *      Author: 87245
 */

#ifndef DEBUG_UART_CIRCULAR_QUEUE_H_
#define DEBUG_UART_CIRCULAR_QUEUE_H_
#include "hal_data.h"
#include <debug_uart/circular_queue.h>
#define DATA_LEN    300 //队列缓存大小

typedef struct
{
    uint16_t head;   //头指针
    uint16_t tail;   //尾指针
    uint8_t data[DATA_LEN];  //队列数据
} Circular_queue_t;

extern Circular_queue_t Circular_queue; //环形队列全局变量


bool Queue_Init(Circular_queue_t *circular_queue);  //初始化队列
bool Queue_isEmpty(Circular_queue_t *circular_queue);  //判断队列是否为空
bool Queue_isFull(Circular_queue_t *circular_queue);   //判断队列是否已满
bool Queue_Wirte(Circular_queue_t *circular_queue, uint8_t *string, uint16_t len); //写数据
bool Queue_Read(Circular_queue_t *circular_queue, uint8_t *string, uint16_t len);  //读数据
uint16_t Queue_HadUse(Circular_queue_t *circular_queue);   //返回队列中数据的长度
uint16_t Queue_NoUse(Circular_queue_t *circular_queue);    //返回未使用数据的长度




#endif /* DEBUG_UART_CIRCULAR_QUEUE_H_ */
