#include "SnakeGame.h"

SnakeGame::SnakeGame(Inkplate* disp, std::function<void()> exitCallback)
: display(disp), exitGame(exitCallback), cellSize(24), dir(RIGHT), lastMove(0), speed(0) {}

void SnakeGame::begin() {
  display->clearDisplay();
  display->setFullUpdateThreshold(0);
  display->partialUpdate(true);

  int controlsAreaHeight = 260;
  gridW = (display->width() - 4) / cellSize;
  gridH = (display->height() - controlsAreaHeight - 4) / cellSize;

  snakeLength = 3;
  headIndex = 0;

  int startX = gridW / 2;
  int startY = gridH / 2;

  snake[0] = {startX, startY};
  snake[1] = {startX - 1, startY};
  snake[2] = {startX - 2, startY};

  placeFood();
  drawControls();
  drawBorder();
  display->display();
}

void SnakeGame::placeFood() {
  while (true) {
    int fx = random(0, gridW);
    int fy = random(0, gridH);
    bool occupied = false;
    for (int i = 0; i < snakeLength; ++i) {
      int idx = (headIndex + i) % MAX_SNAKE_LENGTH;
      if (snake[idx].first == fx && snake[idx].second == fy) {
        occupied = true;
        break;
      }
    }
    if (!occupied) { food = {fx, fy}; break; }
  }
}

void SnakeGame::drawCell(int gx, int gy, bool fill) {
  int x = 2 + gx * cellSize;
  int y = 2 + gy * cellSize;
  display->fillRect(x, y, cellSize, cellSize, fill ? BLACK : WHITE);
}

void SnakeGame::drawBorder() {
  int height = gridH * cellSize + 4;
  display->drawRect(0, 0, display->width(), height, BLACK);
}

void SnakeGame::drawControls() {
  int sz = 100;
  int margin = 5;
  int controlCenterX = 4*display->width() / 5;
  int controlCenterY = display->height() - sz * 1.5 - margin;

  display->fillRect(0, display->height() - sz * 3 - margin * 2, display->width(), sz * 3 + margin * 2, WHITE);

  display->fillTriangle(controlCenterX, controlCenterY - sz,
                        controlCenterX - sz / 2, controlCenterY - sz / 2,
                        controlCenterX + sz / 2, controlCenterY - sz / 2, BLACK);

  display->fillTriangle(controlCenterX, controlCenterY + sz,
                        controlCenterX - sz / 2, controlCenterY + sz / 2,
                        controlCenterX + sz / 2, controlCenterY + sz / 2, BLACK);

  display->fillTriangle(controlCenterX - sz, controlCenterY,
                        controlCenterX - sz / 2, controlCenterY - sz / 2,
                        controlCenterX - sz / 2, controlCenterY + sz / 2, BLACK);

  display->fillTriangle(controlCenterX + sz, controlCenterY,
                        controlCenterX + sz / 2, controlCenterY - sz / 2,
                        controlCenterX + sz / 2, controlCenterY + sz / 2, BLACK);
}

void SnakeGame::update() {
  if (millis() - lastMove < speed) return;
  lastMove = millis();

  int tailIdx = (headIndex + snakeLength - 1) % MAX_SNAKE_LENGTH;
  auto oldTail = snake[tailIdx];

  if (!moveSnake()) {
    exitGame();
    return;
  }

  auto newHead = snake[headIndex];

  drawCell(oldTail.first, oldTail.second, false);
  drawCell(newHead.first, newHead.second, true);
  drawCell(food.first, food.second, true);

  display->partialUpdate(true,true);
}

bool SnakeGame::moveSnake() {
  auto head = snake[headIndex];
  int x = head.first, y = head.second;
  switch(dir) {
    case UP:    y = (y - 1 + gridH) % gridH; break;
    case DOWN:  y = (y + 1) % gridH; break;
    case LEFT:  x = (x - 1 + gridW) % gridW; break;
    case RIGHT: x = (x + 1) % gridW; break;
  }

  for (int i = 0; i < snakeLength; ++i) {
    int idx = (headIndex + i) % MAX_SNAKE_LENGTH;
    if (snake[idx].first == x && snake[idx].second == y)
      return false;
  }

  headIndex = (headIndex - 1 + MAX_SNAKE_LENGTH) % MAX_SNAKE_LENGTH;
  snake[headIndex] = {x, y};

  if (x == food.first && y == food.second) {
    if (snakeLength < MAX_SNAKE_LENGTH) ++snakeLength;
    placeFood();
  }

  return true;
}

void SnakeGame::onTouch(uint16_t tx, uint16_t ty) {
  int sz = 100;
  int margin = 5;
  int controlCenterX = 4*display->width() / 5;
  int controlCenterY = display->height() - sz * 1.5 - margin;

  if (tx > controlCenterX - sz / 2 && tx < controlCenterX + sz / 2 &&
      ty > controlCenterY - sz && ty < controlCenterY - sz / 2)
    dir = UP;

  else if (tx > controlCenterX - sz / 2 && tx < controlCenterX + sz / 2 &&
           ty > controlCenterY + sz / 2 && ty < controlCenterY + sz)
    dir = DOWN;

  else if (tx > controlCenterX - sz && tx < controlCenterX - sz / 2 &&
           ty > controlCenterY - sz / 2 && ty < controlCenterY + sz / 2)
    dir = LEFT;

  else if (tx > controlCenterX + sz / 2 && tx < controlCenterX + sz &&
           ty > controlCenterY - sz / 2 && ty < controlCenterY + sz / 2)
    dir = RIGHT;
}
