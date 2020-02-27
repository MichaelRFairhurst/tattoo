#include <cairo/cairo-xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <assert.h>

#define LINE_WIDTH 6
#define SPACING 24
#define LONG_BRANCH_LEN 400
#define SHORT_BRANCH_LEN ((LONG_BRANCH_LEN * 2) / 3)
#define WIDTH 1800
#define HEIGHT 920
#define FONT_SIZE 72

#define MAX_NUMBER_COUNT 10
#define NUMBER_LEFT 0x10

static cairo_surface_t *surface;
static cairo_t *cairo;

enum cap_style {
  horizontal,
  vertical,
  diagonal,
};

typedef struct Branch {
  struct Branch *left;
  struct Branch *right;
  int lenl;
  int capl;
  int lenr;
  int capr;
  int numbers[MAX_NUMBER_COUNT];
  int numberc;
  char* numbertext[8];
} Branch;

void setbg() {
  cairo_set_source_rgba(cairo, 1, 1, 1, 0);
  cairo_rectangle(cairo, 0, 0, WIDTH, HEIGHT);
  cairo_fill(cairo);
}

void rel_line_to_and_back(int x, int y) {
  cairo_rel_line_to(cairo, x, y);
  cairo_stroke_preserve(cairo);
  cairo_rel_move_to(cairo, -x, -y);
}

void branch(int lenl, int lenr) {
  rel_line_to_and_back(-lenl, lenl);
  rel_line_to_and_back(lenr, lenr);
}

int recap(int len, int offset, int cap) {
  if (cap == horizontal) {
    return len - offset;
  }
  if (cap == vertical) {
    return len;
  }
  if (cap == diagonal) {
    return len - offset / 2;
  }
  assert(0);
  return len;
}

void branches(Branch* tree, int offset) {
  cairo_rel_move_to(cairo, 0, -offset/2);
  branch(tree->lenl, tree->lenr);
  cairo_rel_move_to(cairo, 0, offset);
  branch(recap(tree->lenl, offset, tree->capl), recap(tree->lenr, offset, tree->capr));
  cairo_rel_move_to(cairo, 0, -offset/2);
}

void numbers(int numberc, int* numbers, int len, char** text) {
  int leftc = 0;
  int rightc = 0;
  int sectionl = len / 4;
  for (int i = 0; i < numberc; ++i) {
    int number = numbers[i];
    int reversex = 0;
    int left = number & NUMBER_LEFT;
    int offsety = left ? SPACING * leftc : SPACING * rightc;
    cairo_rel_move_to(cairo, 0, offsety);
    if (left) {
      leftc++;
    } else {
      rightc++;
    }
    for (int bit = 8; bit > 0; bit >>= 1) {
      left = number & bit ? !left : left;
      int movex = left ? -sectionl : sectionl;
      int movey = sectionl + (bit == 1 ? -offsety : 0);
      movex -= left ? -(sectionl - movey) : sectionl - movey;
      reversex += movex;
      cairo_rel_line_to(cairo, movex, movey);
      cairo_stroke_preserve(cairo);
    }
    cairo_rel_move_to(cairo, -reversex, -len);
  }

  cairo_set_font_size(cairo, FONT_SIZE);
  cairo_rel_move_to(cairo, -len, len + SPACING * 3);
  for (int i = 0; i < 8; ++i) {
    cairo_text_extents_t extents;
    cairo_rel_move_to(cairo, len / 4, 0);
    cairo_text_extents(cairo, text[i], &extents);
    cairo_rel_move_to(cairo, -extents.x_advance/2, 0);
    cairo_show_text(cairo, text[i]);
    cairo_rel_move_to(cairo, -extents.x_advance/2, 0);
  }
  cairo_rel_move_to(cairo, -len, -len - SPACING * 3);
}

void tree(Branch *root) {
  if (root->numberc != 0) {
    assert(root->lenl == root ->lenr);
    assert(root->numberc < MAX_NUMBER_COUNT);
    numbers(root->numberc, root->numbers, root->lenr, root->numbertext);
    return;
  }
  branches(root, SPACING);
  if (root->left != NULL) {
    cairo_rel_move_to(cairo, -root->lenl, root->lenl);
    cairo_rel_move_to(cairo, -SPACING*1.5, 0);
    tree(root->left);
    cairo_rel_move_to(cairo, SPACING*1.5, 0);
    cairo_rel_move_to(cairo, root->lenl, -root->lenl);
  }
  if (root->right != NULL) {
    cairo_rel_move_to(cairo, root->lenr, root->lenr);
    cairo_rel_move_to(cairo, SPACING*1.5, 0);
    tree(root->right);
    cairo_rel_move_to(cairo, -SPACING*1.5, 0);
    cairo_rel_move_to(cairo, -root->lenr, -root->lenr);
  }
}

void paint() {
  setbg();
  cairo_set_source_rgba(cairo, 0, 0, 0, 1);
  cairo_set_line_width(cairo, LINE_WIDTH);
  cairo_set_line_cap(cairo, CAIRO_LINE_CAP_SQUARE);
  cairo_move_to(cairo, WIDTH/2, SPACING);
  Branch lowl = {
    .left = NULL,
    .right = NULL,
    .lenl = SHORT_BRANCH_LEN,
    .capl = horizontal,
    .lenr = SHORT_BRANCH_LEN,
    .capr = horizontal,
    .numberc = 0,
  };
  Branch midl = {
    .left = &lowl,
    .right = NULL,
    .lenl = SHORT_BRANCH_LEN,
    .capl = diagonal,
    .lenr = SHORT_BRANCH_LEN,
    .capr = horizontal,
    .numberc = 0,
  };
  Branch lowr = {
    .left = NULL,
    .right = NULL,
    .lenl = LONG_BRANCH_LEN,
    .capl = horizontal,
    .lenr = LONG_BRANCH_LEN,
    .capr = horizontal,
    .numberc = 5,
    .numbers = {1 | NUMBER_LEFT, 0, 1 | NUMBER_LEFT, 2, 5 | NUMBER_LEFT},
    .numbertext = {NULL, "11,", NULL, "    25", NULL, NULL, NULL, "0"},
  };
  Branch root = {
    .left = &midl,
    .right = &lowr,
    .lenl = SHORT_BRANCH_LEN,
    .capl = diagonal,
    .lenr = LONG_BRANCH_LEN,
    .capr = diagonal,
  };
  tree(&root);
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stdout, "Provide a filename");
    return 1;
  }
  surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WIDTH, HEIGHT);
  cairo = cairo_create(surface);
  paint();
  cairo_surface_write_to_png(surface, argv[1]);
  return 0;
}
