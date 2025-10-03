#ifndef SNAKEGAME_H
#define SNAKEGAME_H

#include <Inkplate.h>
#include <vector>
#include <functional>

class SnakeGame {
public:
  SnakeGame(Inkplate* display, std::function<void()> exitCallback);
  void begin();
  void update();
  void onTouch(uint16_t x, uint16_t y);

private:
  Inkplate* display;
  std::function<void()> exitGame;
  int gridW, gridH, cellSize;
  enum Dir { UP, DOWN, LEFT, RIGHT } dir;
  static const int MAX_SNAKE_LENGTH = 1024;
  std::pair<int, int> snake[MAX_SNAKE_LENGTH];
  int snakeLength;
  int headIndex;
  std::pair<int,int> food;
  unsigned long lastMove;
  int speed;

  void placeFood();
  void drawCell(int gx, int gy, bool fill);
  void drawBorder();
  void drawControls();
  bool moveSnake();
};

#endif
