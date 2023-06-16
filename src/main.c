#import <math.h>
#import <stdlib.h>
#import <stdio.h>
#import <unistd.h>
#import <pthread.h>
#import <err.h>

#import "colors.h"
#import "queue.h"
#import "x11-helpers.h"

// determinando nomes de variaveis utilizadas como padrao
static const int THREADS_QUANTITY = 32;
static const int COLORS_COMPLEXITY = 64;
static const int NUMBER_OF_COLORS = 3;
const int IMAGE_SIZE = 800;
const int GRAIN_SIZE = 200;
// o tamanho maximo das filas é a quantidade de graos em que a tela foi dividida, é a quantidade de jobs que tera para ser processado pela threads
static const int QUEUE_SIZE = IMAGE_SIZE/GRAIN_SIZE * IMAGE_SIZE/GRAIN_SIZE;
static int colors[COLORS_COMPLEXITY + 1] = {0};

// fila de jobs
static queue *jobs_queue;
// fila de resultados
static queue *results_queue;

int threadsQuantity = THREADS_QUANTITY;
int colorsComplexity = COLORS_COMPLEXITY;
int numberOfColors = NUMBER_OF_COLORS;

// coordendadas na tela
float coordinates_xi = -2.5;
float coordinates_xf = 1.5;
float coordinates_yi = -2;
float coordinates_yf = 2;

// onde cada tarefa deve ser executada
typedef struct {
  int xi;
  int xf;
  int yi;
  int yf;
} job_data;

// resultado de cada tarefa
typedef struct {
  int xi;
  int xf;
  int yi;
  int yf;
} result_data;

static int calculate_mandelbrot_iterations(float c_real, float c_imaginary) {
  float z_real = c_real;
  float z_imaginary = c_imaginary;

  float test_real, test_imaginary;
  unsigned test_index = 0, test_limit = 8;

  do {
    test_real = z_real;
    test_imaginary = z_imaginary;

    test_limit += test_limit;
    if (test_limit > colorsComplexity) {
      test_limit = colorsComplexity;
    }

    for (; test_index < test_limit; test_index++) {
      // FOIL
      float temp_z_real = (z_real * z_real) - (z_imaginary * z_imaginary) + c_real;
      z_imaginary = (2 * z_imaginary * z_real) + c_imaginary;
      z_real = temp_z_real;

      int diverged = (z_real * z_real) + (z_imaginary * z_imaginary) > 4.0;
      if (diverged) {
        return test_index;
      }

      if ((z_real == test_real) && (z_imaginary == test_imaginary)) {
        return colorsComplexity;
      }
    }
  } while (test_limit != colorsComplexity);

  return colorsComplexity;
}

static void generate_mandelbrot(result_data *result) {
  // qual o tamanho ocupado por um pixel, na escala do plano
  const float pixel_width = (coordinates_xf - coordinates_xi) / IMAGE_SIZE;
  const float pixel_height = (coordinates_yf - coordinates_yi) / IMAGE_SIZE;
  
  for (int y = result->yi; y <= result->yf; y++) {
    for (int x = result->xi; x <= result->xf; x++) {
      float c_real = coordinates_xi + (x * pixel_width);
      float c_imaginary = coordinates_yi + (y * pixel_height);
      int iterations = calculate_mandelbrot_iterations(c_real, c_imaginary);
      int pixel_index = x + (y * IMAGE_SIZE);
      ((unsigned *) x_image->data)[pixel_index] = colors[iterations];
    }
  }
}

// criacao das tarefas de acordo com o tamanho da imagem final
static void create_jobs(int image_width, int image_height) {
  // tamanho do grao
  const int grain_width = GRAIN_SIZE;
  const int grain_height = GRAIN_SIZE;

  // tamanho do pedaco da tela por grao
  const int horizontal_chunks = image_width / grain_width;
  const int vertical_chunks = image_height / grain_height;

  // criacao das tarefas
  for(int j = 0; j < vertical_chunks; j++) {
    for(int i = 0; i < horizontal_chunks; i++) {
      int xi = i * grain_width;
      int xf = ((i + 1) * grain_width) - 1;

      int yi = j * grain_height;
      int yf = ((j + 1) * grain_height) - 1;

      // job_data == qual quadrante da tela cada produtor vai ser responsavel
      job_data *job = malloc(sizeof(job_data));
      job->xi = xi;
      job->xf = xf;
      job->yi = yi;
      job->yf = yf;
      queue_push(jobs_queue, job);
      printf("created job \n");
      pthread_cond_signal(jobs_queue->condition_not_empty);
    }
  }
}

// cria as threads trabalhadoras - algoritmo mandelbrot
static void *workers(void *data) {
  while (1) {
    // bloqueia a thread para ser utilizada
    pthread_mutex_lock(jobs_queue->mutex);
    // desbloqueia para a lista de tarefas esteja vazia
    while (jobs_queue->is_empty) {
      printf("worker procurando trabalho\n");
      pthread_cond_wait(jobs_queue->condition_not_empty, jobs_queue->mutex);
    }
    printf("worker achou trabalho\n");
    
    // pega a tarefa da lista
    job_data *job = malloc(sizeof(job_data));
    queue_pop(jobs_queue, job);
    pthread_mutex_unlock(jobs_queue->mutex);

    result_data *result = malloc(sizeof(result_data));
    result->xi = job->xi;
    result->xf = job->xf;
    result->yi = job->yi;
    result->yf = job->yf;
    free(job);

    generate_mandelbrot(result);

    pthread_mutex_lock(results_queue->mutex);
    queue_push(results_queue, result);
    pthread_mutex_unlock(results_queue->mutex);
    pthread_cond_signal(results_queue->condition_not_empty);
  }
}

// cria as threads de impressao dos dados em tela
static void *printer(void *data) {
  for(int i=0; i< QUEUE_SIZE; i++) {
    pthread_mutex_lock(results_queue->mutex);
    while (results_queue->is_empty) {
      pthread_cond_wait(results_queue->condition_not_empty, results_queue->mutex);
    }

    result_data *result = malloc(sizeof(result_data));
    queue_pop(results_queue, result);
    x11_put_image(result->xi, result->yi, result->xi, result->yi, (result->xf - result->xi + 1), (result->yf - result->yi + 1));
    printf("printou um job \n");
    pthread_mutex_unlock(results_queue->mutex);
  }
  x11_flush();
  return NULL;
}

void create_workers_threads() {
  // processos trabalhadores para executar com a qt de threads determinada
  pthread_t workers_threads[threadsQuantity];

  printf("começou a criar as threads workers\n");
  // determina a qt de threads criadas e diz o que cada worker vair ser com o metodo workers
  for (int i = 0; i < threadsQuantity; i++) {
    pthread_create(&workers_threads[i], NULL, workers, NULL);
  }
}

void create_printer_thread() {
  // processo impressor
  pthread_t printer_thread;

  // cria a quantidade de threads a receber as tarefas produzidas de acordo com a qt de tarefas, e diz como cada thread vai ser criada
  pthread_create(&printer_thread, NULL, printer, NULL);
  printf("criou thread printer\n");

  // junta o resultado das threads de consumo
  pthread_join(printer_thread, NULL);
  printf("encerrou thread printer\n");
}

// tranformacao de coordenadas originais para virtuais
void transform_coordinates(int xi_signal, int xf_signal, int yi_signal, int yf_signal) {
  float width = coordinates_xf - coordinates_xi;
  float height = coordinates_yf - coordinates_yi;
  coordinates_xi += width * 0.1 * xi_signal;
  coordinates_xf += width * 0.1 * xf_signal;
  coordinates_yi += height * 0.1 * yi_signal;
  coordinates_yf += height * 0.1 * yf_signal;

  printf("\njobs atualizados novamente para serem printados\n");
  create_jobs(IMAGE_SIZE, IMAGE_SIZE);
  create_printer_thread();
}

int main(int argc, char* argv[]) {
  
  // pega os argumentos enviados no comando
  if (argc == 2) {
    threadsQuantity = atof(argv[1]);
  }
  if (argc == 3) {
    threadsQuantity = atof(argv[1]);
    colorsComplexity = atof(argv[2]);
  }
  if (argc == 4) {
    threadsQuantity = atof(argv[1]);
    colorsComplexity = atof(argv[2]);
    numberOfColors = atof(argv[3]);
  }

  // inicia o X11
  x11_init(IMAGE_SIZE);
  // cria a tabela de cores de acordo com o numero de iteracoes
  colors_init(colors, colorsComplexity, numberOfColors);
  // inicializa as filas com um tamanho especifico e tambem tamanho especifico de cada item para alocar memoria
  jobs_queue = queue_init(QUEUE_SIZE, sizeof(job_data));
  results_queue = queue_init(QUEUE_SIZE, sizeof(result_data));

  // cria as threads necessarias para realizar a execucao e executa
  create_workers_threads();
  create_jobs(IMAGE_SIZE, IMAGE_SIZE);
  create_printer_thread();

  // cria a tela de acordo com o lugar que especificamos na tela
  x11_handle_events(IMAGE_SIZE, transform_coordinates);

  // destroi as filas de tarefas e tambem a instancia do x11
  queue_destroy(jobs_queue);
  queue_destroy(results_queue);
  printf("matamos a thread main e com isso as threads workers\n");
  x11_destroy();

  return 0;
}
