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
static const int THREADS_QUANTITY = 8;
static const int QUANTITY_ITERATION = 1024;
static const int NUMBER_OF_COLORS = 18;
static int colors[QUANTITY_ITERATION + 1] = {0};

// fila de jobs
static queue *jobs_queue;
// fila de resultados
static queue *results_queue;

int threadsQuantity = THREADS_QUANTITY;
int quantityInteraction = QUANTITY_ITERATION;

// coordendadas na tela
float coordinates_xi = -2.5;
float coordinates_xf = 1.5;
float coordinates_yi = -2;
float coordinates_yf = 2;
const int IMAGE_SIZE = 800;
const int GRAIN_SIZE = 100;

// lista de tarefas a serem executadas
typedef struct {
  int tasks;
} printer_data;

// onde cada tarefa deve ser executada
typedef struct {
  int xi;
  int xf;
  int yi;
  int yf;
} task_data;

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
    if (test_limit > quantityInteraction) {
      test_limit = quantityInteraction;
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
        return quantityInteraction;
      }
    }
  } while (test_limit != quantityInteraction);

  return quantityInteraction;
}

static void generate_mandelbrot(){
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
static int create_tasks(int image_width, int image_height) {
  
  // tamanho do grao
  const int grain_width = GRAIN_SIZE;
  const int grain_height = GRAIN_SIZE;

  // tamanho do pedaco da tela por grao
  const int horizontal_chunks = image_width / grain_width;
  const int vertical_chunks = image_height / grain_height;

  int tasks_created = 0;

  // criacao das tarefas
  for(int j = 0; j < vertical_chunks; j++) {
    for(int i = 0; i < horizontal_chunks; i++) {
      int xi = i * grain_width;
      int xf = ((i + 1) * grain_width) - 1;

      int yi = j * grain_height;
      int yf = ((j + 1) * grain_height) - 1;

      // task_data == qual quadrante da tela cada produtor vai ser responsavel
      task_data *task = malloc(sizeof(task_data));
      task->xi = xi;
      task->xf = xf;
      task->yi = yi;
      task->yf = yf;
      queue_push(jobs_queue, task);
      tasks_created++;
    }
  }

  return tasks_created;
}

// cria as threads trabalhadoras - algoritmo mandelbrot
static void *workers(void *data) {
  while (1) {
    // se a lista de jobs está vazia nao faz nada
    if (jobs_queue->is_empty) {
      break;
    }

    //Bloqueia a fila, pega uma tarefa e desbloqueia
    pthread_mutex_lock(jobs_queue->mutex);
    while (jobs_queue->has_content) {
      pthread_cond_wait(jobs_queue->condition_not_in_use, jobs_queue->mutex);
    }
    task_data *task = malloc(sizeof(task_data));
    queue_pop(jobs_queue, task);
    pthread_mutex_unlock(jobs_queue->mutex);
    pthread_cond_signal(results_queue->condition_not_in_use);

    result_data *result = malloc(sizeof(result_data));
    result->xi = task->xi;
    result->xf = task->xf;
    result->yi = task->yi;
    result->yf = task->yf;
    free(task);

    generate_mandelbrot();

    pthread_mutex_lock(results_queue->mutex);
    while (results_queue->has_content) {
      pthread_cond_wait(results_queue->condition_not_in_use, results_queue->mutex);
    }
    queue_push(results_queue, result);
    pthread_mutex_unlock(results_queue->mutex);
    pthread_cond_signal(results_queue->condition_not_in_use);
    pthread_cond_signal(results_queue->condition_not_empty);
  }

  return NULL;
}

// cria as threads de impressao dos dados em tela
static void *printer(void *data) {
  printer_data *cd = (printer_data *) data;
  int consumed_tasks = 0;

  while (1) {
    if (consumed_tasks == cd->tasks) {
      x11_flush();
      return NULL;
    }

    pthread_mutex_lock(results_queue->mutex);
    while (results_queue->is_empty) {
      pthread_cond_wait(results_queue->condition_not_empty, results_queue->mutex);
    }

    result_data *result = malloc(sizeof(result_data));
    queue_pop(results_queue, result);
    x11_put_image(result->xi, result->yi, result->xi, result->yi, (result->xf - result->xi + 1), (result->yf - result->yi + 1));
    pthread_mutex_unlock(results_queue->mutex);
    consumed_tasks++;
  }
}

// cria as threads necessarias para realizar a execucao
void process_mandelbrot_set() {
  // cria quantidade de tasks de acordo com o tamanho da imagem
  int tasks_created = create_tasks(IMAGE_SIZE, IMAGE_SIZE);

  // processos trabalhadores para executar com a qt de threads determinada
  pthread_t workers_threads[threadsQuantity];
  // processo impressor
  pthread_t printer_thread;

  // determina a qt de threads criadas e diz o que cada worker vair ser com o metodo workers
  for (int i = 0; i < threadsQuantity; i++) {
    pthread_create(&workers_threads[i], NULL, workers, NULL);
  }

  // pega a fila de tarefas produzidas
  printer_data *cd = malloc(sizeof(printer_data));
  // adiciona o numero de tasks
  cd->tasks = tasks_created;
  // cria a quantidade de threads a receber as tarefas produzidas de acordo com a qt de tarefas, e diz como cada thread vai ser criada
  pthread_create(&printer_thread, NULL, printer, cd);

  // junta o resultado das threads de execucao
  for (int i = 0; i < threadsQuantity; i++) {
    pthread_join(workers_threads[i], NULL);
  }

  // junta o resultado das threads de consumo
  pthread_join(printer_thread, NULL);
  free(cd);
}

// tranformacao de coordenadas originais para virtuais
void transform_coordinates(int xi_signal, int xf_signal, int yi_signal, int yf_signal) {
  float width = coordinates_xf - coordinates_xi;
  float height = coordinates_yf - coordinates_yi;
  coordinates_xi += width * 0.1 * xi_signal;
  coordinates_xf += width * 0.1 * xf_signal;
  coordinates_yi += height * 0.1 * yi_signal;
  coordinates_yf += height * 0.1 * yf_signal;
  process_mandelbrot_set();
}

int main(int argc, char* argv[]) {
  
  // pega os argumentos enviados no comando
  if (argc == 3) {
    threadsQuantity = atof(argv[1]);
    quantityInteraction = atof(argv[2]);
  }

  // inicia o X11
  x11_init(IMAGE_SIZE);
  // cria a tabela de cores de acordo com o numero de iteracoes
  colors_init(colors, quantityInteraction, NUMBER_OF_COLORS);
  // inicializa as filas com um tamanho especifico e tambem tamanho especifico de cada item para alocar memoria
  jobs_queue = queue_init(100, sizeof(task_data));
  results_queue = queue_init(100, sizeof(result_data));

  // cria as threads necessarias para realizar a execucao e executa
  process_mandelbrot_set();

  // cria a tela de acordo com o lugar que especificamos na tela
  x11_handle_events(IMAGE_SIZE, transform_coordinates);

  // destroi as filas de tarefas e tambem a instancia do x11
  queue_destroy(jobs_queue);
  queue_destroy(results_queue);
  x11_destroy();

  return 0;
}
