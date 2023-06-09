// fila

#ifndef queue_h
#define queue_h

#import <string.h>

typedef struct {
  int size;
  void** data;
  size_t item_size;
  int head, tail;
  int has_content, is_empty;
  pthread_mutex_t *mutex;
  pthread_cond_t *condition_not_in_use, *condition_not_empty;
} queue;

// inicia a fila com um tamanho especifico, e determina quanto de tamanho cada objeto vai ter
queue *queue_init (unsigned size, size_t item_size) {
  queue *q = (queue *)malloc(sizeof (queue));
  q->data = malloc(sizeof(item_size) * size);
  q->size = size;
  q->item_size = item_size;
  q->is_empty = 1;
  q->has_content = 0;
  q->head = 0;
  q->tail = 0;

  // pthread_mutex_t - exclusao mutua - garante que apenas uma thread tera acesso a um dado na memoria compartilhada em um determinado instante de tempo
  q->mutex = (pthread_mutex_t *) malloc(sizeof (pthread_mutex_t));
  pthread_mutex_init(q->mutex, NULL);
  // pthread_cond_t - variavel de condicao - permite o acesso a uma secao critica quando uma determinada condicao for satisfeita
  q->condition_not_in_use = (pthread_cond_t *) malloc(sizeof (pthread_cond_t));
  pthread_cond_init(q->condition_not_in_use, NULL);
  q->condition_not_empty = (pthread_cond_t *) malloc(sizeof (pthread_cond_t));
  pthread_cond_init(q->condition_not_empty, NULL);

  return (q);
}

void queue_destroy(queue *q) {
  pthread_mutex_destroy(q->mutex);
  free(q->mutex);
  pthread_cond_destroy(q->condition_not_in_use);
  free(q->condition_not_in_use);
  pthread_cond_destroy(q->condition_not_empty);
  free(q->condition_not_empty);
  free(q->data);
  free(q);
}

void queue_push(queue *q, void *item) {
  q->data[q->tail] = item;
  q->tail++;
  if (q->tail == q->size) {
    q->tail = 0;
  }
  if (q->tail == q->head) {
    q->has_content = 1;
  }
  q->is_empty = 0;
}

void queue_pop(queue *q, void *item) {
  memcpy(item, q->data[q->head], q->item_size);
  q->head++;
  if (q->head == q->size) {
    q->head = 0;
  }
  if (q->head == q->tail) {
    q->is_empty = 1;
  }
  q->has_content = 0;
}
#endif
