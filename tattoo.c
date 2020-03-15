#include <cairo/cairo-xlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/file.h>
#include <assert.h>

#define LINE_WIDTH 6
#define SPACING 24
#define BRANCH_LEN 300
#define BRANCH_DIFFERENCE 50
#define WIDTH 1600
#define HEIGHT 1000
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
  cairo_set_source_rgba(cairo, 1, 1, 1, 1);
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
  double sectionl = len / 4.0;

  // Represents where a turn has occurred that takes up a space. The first index
  // represents how far left or right the location occured. The second index
  // represents whether the line went left or right. The value is the bits at
  // which point that occurred.
  int lineSegments[9][2];
  memset(lineSegments, 0, sizeof(lineSegments));

  int leftc = 0;
  int rightc = 0;

  for (int i = 0; i < numberc; ++i) {
    int number = numbers[i];
    double reversex = 0;
    int left = number & NUMBER_LEFT;
    // The lines do not all meet at the top, but rather are horizontally offset.
    // Since they are horizontally offset, they cannot meet exactly at the top.
    //
    //  /\
    // // \ <- inner line is offset horizontally and vertically.
    //
    int offsety = left ? SPACING * leftc : SPACING * rightc;
    int offsetx = left ? offsety : -offsety;
    int xIndex = 4;
    cairo_rel_move_to(cairo, 0, offsety);
    int isOppositeSegmentTaken = i > 1 && (left ? leftc < rightc : leftc > rightc);
    if (isOppositeSegmentTaken) {
      cairo_rel_move_to(cairo, left ? -SPACING : SPACING, SPACING);
      offsety += SPACING;
      reversex += left ? -SPACING : SPACING;
    }
    if (left) {
      leftc++;
    } else {
      rightc++;
    }
    // For our basic algorithm here, we track an x offset & y offset that we can
    // fix during turns (former) or straights (latter).
    //
    //    / <- without offsets, the two lines would stack here
    //   x <- and split here.
    //  / \ <- right line is offset vertically
    //
    // Instead we do:
    //
    //    //
    //   // <- right line is offset horizontally
    //  / \ <- right line is offset vertically
    // /   \ <- right line is not longer offset.
    //
    // Note that when the space is taken, we cannot do this:
    //
    //  //     \ /
    // //  or   / <- gap
    // \\      //
    //  \\    //
    for (int yIndex = 0, bit = 8; bit > 0; bit >>= 1, yIndex++) {
      left = number & bit ? !left : left;
      int isTargetSegmentTaken = lineSegments[xIndex][!left] & (bit >> 1);
      xIndex += left ? -1 : 1;
      lineSegments[xIndex][left] |= bit;
      double move;
      if (number & (bit >> 1) && !isTargetSegmentTaken) {
        // If our next action is a turn, we draw a long enough line to
        // intersect with the next path.
        move = sectionl + (offsetx > 0 ? offsetx : -offsetx)/2 - offsety;
        offsety = (offsetx > 0 ? offsetx : -offsetx) / 2;
        offsetx = 0;
      } else {
        move = sectionl - offsety;
        offsety = 0;
      }
      if (!left && isTargetSegmentTaken) {
        move -= SPACING/2;
      }
      double movex = left ? -move : move;
      reversex += movex;
      cairo_rel_line_to(cairo, movex, move);
      cairo_stroke_preserve(cairo);
      if (!left && isTargetSegmentTaken) {
        offsetx = SPACING/2;
        offsety = SPACING/2;
        cairo_rel_move_to(cairo, SPACING, SPACING);
      }
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
  cairo_scale(cairo, 0.75, 1);
  Branch lowl = {
    .left = NULL,
    .right = NULL,
    .lenl = BRANCH_LEN,
    .capl = horizontal,
    .lenr = BRANCH_LEN,
    .capr = horizontal,
    .numberc = 0,
  };
  Branch midl = {
    .left = &lowl,
    .right = NULL,
    .lenl = BRANCH_LEN,
    .capl = diagonal,
    .lenr = BRANCH_LEN,
    .capr = horizontal,
    .numberc = 0,
  };
  Branch lowr = {
    .left = NULL,
    .right = NULL,
    .lenl = BRANCH_LEN * 2 - SPACING - BRANCH_DIFFERENCE,
    .capl = horizontal,
    .lenr = BRANCH_LEN * 2 - SPACING - BRANCH_DIFFERENCE,
    .capr = horizontal,
    .numberc = 5,
    .numbers = {1 | NUMBER_LEFT, 0, 1 | NUMBER_LEFT, 2, 5 | NUMBER_LEFT},
    .numbertext = {NULL, "11", NULL, "25", NULL, NULL, NULL, "0"},
  };
  Branch root = {
    .left = &midl,
    .right = &lowr,
    .lenl = BRANCH_LEN,
    .capl = diagonal,
    .lenr = BRANCH_LEN + BRANCH_DIFFERENCE,
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
