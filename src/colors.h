// paleta de cores que estao sendo utilizadas

#ifndef colors_h
#define colors_h

static int palette[] = {
  14540253,   // #DEE9F5 (Azul claro)
  10513739,   // #A0B3C7 (Azul acinzentado)
  7358088,    // #6F8898 (Azul médio)
  4749702,    // #4880AE (Azul escuro)
  2388752,    // #24499C (Azul profundo)
  4341753,    // #41A6E9 (Azul vívido)
  6924298,    // #6994C6 (Azul pastel)
  9357405,    // #8EAFBD (Azul esverdeado)
  11842360,   // #B49C85 (Bege)
  13684944,   // #D0D0D0 (Cinza claro)
  15294998,   // #E8D7C6 (Creme)
  15461355,   // #EBE4DD (Branco sujo)
  13346204,   // #CA824C (Marrom claro)
  11064646,   // #A8A56E (Verde amarelado)
  8765439,    // #85A4BF (Azul esverdeado)
  5353215,    // #51A04F (Verde brilhante)
  14815782,   // #E21226 (Vermelho)
  15357116    // #EA54BC (Rosa)
};

// escolhe a quantidade de cores que queremos que tenha no desenho
static void colors_init(int *colors, int length) {
  for (int i= 0; i < length - 1; i++) {
    colors[i] = palette[i % 16];
  }
}

#endif