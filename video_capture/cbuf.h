#ifndef __CBUF_H__
#define __CBUF_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Define to prevent recursive inclusion 
-------------------------------------*/
#include "thread.h"

#ifndef CBUF_MAX
#define CBUF_MAX 1000
#endif

typedef    struct _cbuf
{   
    int        CUBFEACHDATALEN;
    int        size;            /* 当前缓冲区中存放的数据的个数 */
    int        next_in;        /* 缓冲区中下一个保存数据的位置 */
    int        next_out;        /* 从缓冲区中取出下一个数据的位置 */
    int        capacity;        /* 这个缓冲区的可保存的数据的总个数 */
    mutex_t        mutex;            /* Lock the structure */
    cond_t        not_full;        /* Full -> not full condition */
    cond_t        not_empty;        /* Empty -> not empty condition */
    void        *data[CBUF_MAX];/* 缓冲区中保存的数据指针 */
    int      data_real_len[CBUF_MAX];/*保存缓冲区中对应数据的真实长度*/
}cbuf_t;


/* 初始化环形缓冲区 */
extern    int        cbuf_init(cbuf_t *c);

/* 销毁环形缓冲区 */
extern    void        cbuf_destroy(cbuf_t    *c);

/* 压入数据 */
extern    int        cbuf_enqueue(cbuf_t *c,void *data, int len);

/* 取出数据 */
//fto : NULL 清空buffer
extern    int      cbuf_dequeue(cbuf_t *c, void* fto);


/* 判断缓冲区是否为满 */
extern    int cbuf_full(cbuf_t    *c);

/* 判断缓冲区是否为空 */
extern    int cbuf_empty(cbuf_t *c);

/* 获取缓冲区可存放的元素的总个数 */
extern    int        cbuf_capacity(cbuf_t *c);

extern   int      cbuf_dequeue_pcm(cbuf_t *c, void* fto);

#ifdef __cplusplus
}
#endif

#endif
/* END OF FILE 
---------------------------------------------------------------*/
