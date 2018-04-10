#include "cbuf.h"
#include <string.h>
#include "linux/types.h"
#include <stdio.h>

// #define printf_CBUF 

#define min(x, y) ((x) < (y) ? (x) : (y))

/* 初始化环形缓冲区 */
int        cbuf_init(cbuf_t *c)
{
    int    ret = OPER_OK;

    if((ret = mutex_init(&c->mutex)) != OPER_OK)    
    {
#ifdef printf_CBUF
    printf("cbuf init fail ! mutex init fail !\n");
#endif
        return ret;
    }

    if((ret = cond_init(&c->not_full)) != OPER_OK)    
    {
#ifdef printf_CBUF
    printf("cbuf init fail ! cond not full init fail !\n");
#endif
        mutex_destroy(&c->mutex);
        return ret;
    }

    if((ret = cond_init(&c->not_empty)) != OPER_OK)
    {
#ifdef printf_CBUF
    printf("cbuf init fail ! cond not empty init fail !\n");
#endif
        cond_destroy(&c->not_full);
        mutex_destroy(&c->mutex);
        return ret;
    }

    c->size     = 0;
    c->next_in    = 0;
    c->next_out = 0;
    c->capacity    = CBUF_MAX;
    #ifdef printf_CBUF
    printf("INIT c->capacity :%u, c->size:%u\n",c->capacity ,c->size);
    #endif
    for(int i=0; i<c->capacity; i++)
    {
        c->data[i] = new char[c->CUBFEACHDATALEN];
        c->data_real_len[i] = 0;
    }

#ifdef printf_CBUF
    printf("cbuf init success !\n");
#endif

    return ret;
}


/* 销毁环形缓冲区 */
void        cbuf_destroy(cbuf_t    *c)
{

#ifdef printf_CBUF
    printf("cbuf destroy 1 \n");
#endif
    cond_destroy(&c->not_empty);

#ifdef printf_CBUF
    printf("cbuf destroy 2 \n");
#endif
    cond_destroy(&c->not_full);
#ifdef printf_CBUF
    printf("cbuf destroy 3 \n");
#endif
    mutex_destroy(&c->mutex);
#ifdef printf_CBUF
    printf("cbuf destroy 4 \n");
#endif
    for(int i=0; i<c->capacity; i++)
    {
        if(c->data[i])
        {
            delete[] c->data[i];
            c->data[i] = NULL;
            c->data_real_len[i] = 0;
        }
    }

#ifdef printf_CBUF
    printf("cbuf destroy 5 \n");
#endif
#ifdef printf_CBUF
    printf("cbuf destroy success \n");
#endif
}



/* 压入数据 */
int   cbuf_enqueue(cbuf_t *c,void *data, int len)
{
    int    ret = OPER_OK;

    if((ret = mutex_lock(&c->mutex)) != OPER_OK)    return ret;

    /*
     * Wait while the buffer is full.
     */
    while(cbuf_full(c))
    {
#ifdef printf_CBUF
    printf("cbuf is full !!!\n");
#endif
        //cond_wait(&c->not_full,&c->mutex);
        mutex_unlock(&c->mutex);
        return 0;
    }

    len = min(len, c->CUBFEACHDATALEN);
    int next_in = c->next_in;
    c->next_in++;
    memcpy(c->data[next_in], data, len);
    c->data_real_len[next_in] = len;
    c->size++;
    c->next_in %= c->capacity;

    mutex_unlock(&c->mutex);

    /*
     * Let a waiting consumer know there is data.
     */
    cond_signal(&c->not_empty);

#ifdef printf_CBUF
//    printf("cbuf enqueue success ,data : %p\n",data);
    printf("enqueue\n");
#endif

    return len;
}



/* 取出数据 */
int      cbuf_dequeue(cbuf_t *c, void* fto)
{
    
    // void     *data     = NULL;
    int    ret     = OPER_OK;

    if((ret = mutex_lock(&c->mutex)) != OPER_OK)    return NULL;

    if(fto == NULL)
     {
            
            while(!cbuf_empty(c))
            {
                c->next_out++;
                c->size--;
                c->next_out %= c->capacity;
            }

           mutex_unlock(&c->mutex);


            /*
            * Let a waiting producer know there is room.
            * 取出了一个元素，又有空间来保存接下来需要存储的元素
            */
            cond_signal(&c->not_full);
            return 0;
        }
       /*
     * Wait while there is nothing in the buffer
     */
    while(cbuf_empty(c))
    {
#ifdef printf_CBUF
    printf("cbuf is empty!!!\n");
#endif
         cond_wait(&c->not_empty,&c->mutex);
        //mutex_unlock(&c->mutex);
        //return 0;
    }

    int next_out = c->next_out;
    c->next_out++;
    int len = c->data_real_len[next_out];
    memcpy(fto, c->data[next_out], len);

    c->size--;
    c->next_out %= c->capacity;

    mutex_unlock(&c->mutex);


    /*
     * Let a waiting producer know there is room.
     * 取出了一个元素，又有空间来保存接下来需要存储的元素
     */
    cond_signal(&c->not_full);

#ifdef printf_CBUF
//    printf("cbuf dequeue success ,data : %p\n",data);
    printf("dequeue\n");
#endif

    return len;
}


/* 取出数据 */
int      cbuf_dequeue_pcm(cbuf_t *c, void* fto)
{
    
    // void     *data     = NULL;
    int    ret     = OPER_OK;

    if((ret = mutex_lock(&c->mutex)) != OPER_OK)    return NULL;


       /*
     * Wait while there is nothing in the buffer
     */
    while(cbuf_empty(c))
    {
#ifdef printf_CBUF
    printf("cbuf is empty!!!\n");
#endif
        //  cond_wait(&c->not_empty,&c->mutex);
        mutex_unlock(&c->mutex);
        return 0;
    }

    int next_out = c->next_out;
    c->next_out++;
    int len = c->data_real_len[next_out];
    memcpy(fto, c->data[next_out], len);

    c->size--;
    c->next_out %= c->capacity;

    mutex_unlock(&c->mutex);


    /*
     * Let a waiting producer know there is room.
     * 取出了一个元素，又有空间来保存接下来需要存储的元素
     */
    cond_signal(&c->not_full);

#ifdef printf_CBUF
//    printf("cbuf dequeue success ,data : %p\n",data);
    printf("dequeue\n");
#endif

    return len;
}




/* 判断缓冲区是否为满 */
int cbuf_full(cbuf_t    *c)
{
    printf("cbuf_full c->capacity :%u, c->size:%u\n",c->capacity ,c->size);
    return (c->size == c->capacity);
}

/* 判断缓冲区是否为空 */
int        cbuf_empty(cbuf_t *c)
{
    return (c->size == 0);
}

/* 获取缓冲区可存放的元素的总个数 */
int        cbuf_capacity(cbuf_t *c)
{
    return c->capacity;
}
