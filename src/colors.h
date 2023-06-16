// paleta de cores que estao sendo utilizadas

#ifndef colors_h
#define colors_h

static void generate_palette(int *colors, int number_of_colors) {
  for (int i= 0; i < number_of_colors; i++) {
    colors[i] = 17000000/number_of_colors * i;
  }
}

// escolhe a quantidade de cores que queremos que tenha no desenho
static void colors_init(int *colors, int length, int number_of_colors) {
  static int palette[] = {};
  
  generate_palette(palette, number_of_colors);

  for (int i= 0; i < length - 1; i++) {
    colors[i] = palette[i % number_of_colors + 1];
  }
}

#endif