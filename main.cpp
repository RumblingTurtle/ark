#include <SDL2/SDL.h>
#include <chrono>
#include <iostream>

typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;
float get_dt(TimePoint a, TimePoint b) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(a - b).count() /
         1000.0f;
}

enum CollisionSide { LEFT, RIGHT, UP, DOWN };
struct CollisionInfo {
  bool collided;
  CollisionSide side;
  float closest_x, closest_y;
};

SDL_Window *window;
SDL_Renderer *renderer;

static constexpr int SCREEN_WIDTH = 640;
static constexpr int SCREEN_HEIGHT = 480;

static constexpr int RECT_WIDTH = 80;
static constexpr int RECT_HEIGHT = 20;

static constexpr int RECT_PADDING = 10.0f;
static constexpr int RECT_EDGE_OFFSET = 10.0f;

static constexpr int RECT_COUNT_X =
    (SCREEN_WIDTH - 2 * RECT_EDGE_OFFSET) / (RECT_WIDTH + RECT_PADDING) + 1;
static constexpr int RECT_COUNT_Y =
    (SCREEN_HEIGHT / 2 - 2 * RECT_EDGE_OFFSET) / (RECT_HEIGHT + RECT_PADDING) +
    1; // Filling just a half of the screen with rectangles

static int BALL_SPEED = 300;
#pragma region SDLINIT
bool init() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "Failed to initialize SDL2" << std::endl;
    return 1;
  }

  window =
      SDL_CreateWindow("ark", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
  if (window == NULL) {
    std::cerr << "Failed to create window" << std::endl;
    return 1;
  }
  renderer = SDL_CreateRenderer(window, -1, 0);
  return 0;
}

void cleanup() {
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
}

void clear_window(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if (SDL_SetRenderDrawColor(renderer, r, g, b, a) == -1) {
    std::runtime_error("Failed to set render draw color");
  }

  SDL_RenderFillRect(renderer, NULL);
}
#pragma endregion

struct Rectangle {
  struct {
    int w;
    int h;
  } dims;

  struct {
    int x;
    int y;
  } pos;

  struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  } color;

  bool active = true;
};

struct Ball {
  struct {
    int x;
    int y;
  };
  float radius;
  float r, g, b, a;

  float vx, vy;
  float speed;
} ball;

Rectangle rectangles[RECT_COUNT_X * RECT_COUNT_Y + 1];
Rectangle &slider_rectangle = rectangles[RECT_COUNT_X * RECT_COUNT_Y];

void draw_rect(const Rectangle &rectangle) {
  if (SDL_SetRenderDrawColor(renderer, rectangle.color.r, rectangle.color.g,
                             rectangle.color.b, rectangle.color.a) == -1) {
    std::runtime_error("Failed to set render draw color for rectangle");
  }

  SDL_Rect sdl_rect{rectangle.pos.x, rectangle.pos.y, rectangle.dims.w,
                    rectangle.dims.h};

  if (SDL_RenderFillRect(renderer, &sdl_rect) < 0) {
    std::cerr << "Failed to draw rect" << std::endl;
  }

  // Outline
  float ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::duration())
                 .count();

  float ms_sin = (std::sin(ms) + 1) / 2;
  if (SDL_SetRenderDrawColor(
          renderer, rectangle.color.b * ms_sin, rectangle.color.r * ms_sin,
          rectangle.color.g * ms_sin, rectangle.color.a) == -1) {
    std::runtime_error("Failed to set render draw color for rectangle");
  }

  if (SDL_RenderDrawRect(renderer, &sdl_rect) < 0) {
    std::cerr << "Failed to draw rect" << std::endl;
  }
}

void draw_ball() {
  if (SDL_SetRenderDrawColor(renderer, ball.r, ball.g, ball.b, 255) == -1) {
    std::runtime_error("Failed to set render draw color for ball");
  }

  for (int i = 0; i < 360; i++) {
    float x = ball.x + ball.radius * std::cos(M_PI * i / 180);
    float y = ball.y + ball.radius * std::sin(M_PI * i / 180);
    if (SDL_RenderDrawLineF(renderer, ball.x, ball.y, x, y) < 0) {
      std::cerr << "Failed to draw ball" << std::endl;
    }
  }
}

void fill_primitives() {
  ball.r = rand() % 255;
  ball.g = rand() % 255;
  ball.b = rand() % 255;
  ball.a = 255;
  ball.radius = 10;
  ball.speed = BALL_SPEED;
  for (int x = 0; x < RECT_COUNT_X; x++) {
    for (int y = 0; y < RECT_COUNT_Y; y++) {
      rectangles[x + RECT_COUNT_Y * y] =
          Rectangle{{RECT_WIDTH, RECT_HEIGHT},
                    {RECT_EDGE_OFFSET + (RECT_WIDTH + RECT_PADDING) * x,
                     RECT_EDGE_OFFSET + (RECT_HEIGHT + RECT_PADDING) * y},
                    {rand() % 255, rand() % 255, rand() % 255, 255}};
    }
  }
  slider_rectangle = {{100, 20},
                      {SCREEN_WIDTH / 2 - 50, SCREEN_HEIGHT - 20},
                      {255, 255, 255, 255}};
}

float clamp(float value, float min, float max) {
  return std::max(min, std::min(max, value));
}

CollisionInfo check_collision(Rectangle &rectangle) {
  CollisionInfo collision_info;

  float clamped_dist_x =
      clamp(ball.x - (rectangle.pos.x + rectangle.dims.w / 2),
            -rectangle.dims.w / 2, rectangle.dims.w / 2);

  float clamped_dist_y =
      clamp(ball.y - (rectangle.pos.y + rectangle.dims.h / 2),
            -rectangle.dims.h / 2, rectangle.dims.h / 2);

  float closest_x = rectangle.dims.w / 2 + rectangle.pos.x + clamped_dist_x;
  float closest_y = rectangle.dims.h / 2 + rectangle.pos.y + clamped_dist_y;

  float ball_to_closest_x = closest_x - ball.x;
  float ball_to_closest_y = closest_y - ball.y;

  float dist_to_closest = std::sqrt(ball_to_closest_x * ball_to_closest_x +
                                    ball_to_closest_y * ball_to_closest_y);
  if (dist_to_closest > ball.radius) {
    collision_info.collided = false;
    return collision_info;
  }

  collision_info.collided = true;

  collision_info.closest_x = ball_to_closest_x;
  collision_info.closest_y = ball_to_closest_y;

  float ball_to_closest_dir_x = ball_to_closest_x / dist_to_closest;
  float ball_to_closest_dir_y = ball_to_closest_y / dist_to_closest;

  // Dot products with ball's direction vectors
  float dot_products[4] = {
      -ball_to_closest_dir_x,
      ball_to_closest_dir_x,
      ball_to_closest_dir_y,
      -ball_to_closest_dir_y,
  };

  // find max dot product
  int max_idx = 0;
  for (int i = 1; i < 4; i++) {
    if (dot_products[i] > dot_products[max_idx]) {
      max_idx = i;
    }
  }

  collision_info.side = (CollisionSide)max_idx;
  return collision_info;
}

void check_collisions() {
  for (int i = 0; i < RECT_COUNT_X * RECT_COUNT_Y + 1; i++) {
    Rectangle &rect = rectangles[i];
    if (!rect.active) {
      continue;
    }

    CollisionInfo collision_info = check_collision(rect);

    if (!collision_info.collided) {
      continue;
    }

    if (collision_info.side == CollisionSide::UP ||
        collision_info.side == CollisionSide::DOWN) {
      ball.vy *= -1;
    } else {
      ball.vx *= -1;
    }

    if (i != RECT_COUNT_X * RECT_COUNT_Y) {
      rect.active = false;
    } else {
      // Scale proportionally to the distance from the slider center
      if (collision_info.side == CollisionSide::UP) {
        float ball_to_center_x = slider_rectangle.pos.x +
                                 slider_rectangle.dims.w / 2 -
                                 ball.x; // Position relative to the ball
        float dist_from_center_normalized =
            std::abs(clamp(ball_to_center_x, -slider_rectangle.dims.w / 2,
                           slider_rectangle.dims.w / 2)) /
            slider_rectangle.dims.w / 2;

        float velocity_norm = std::sqrt(ball.vx * ball.vx + ball.vy * ball.vy);
        if (ball_to_center_x * ball.vx > 0 &&
            dist_from_center_normalized > 0.1f) {
          ball.vx *= -1;
        } else {

          ball.vx += ball.vx > 0 ? dist_from_center_normalized
                                 : -dist_from_center_normalized;
        }

      } // We don't care about bumping into the side of the slider. You should
      // lose in this case

      if (collision_info.side == CollisionSide::DOWN) {
        throw std::runtime_error("????!");
      }
    }

    float velocity_norm = std::sqrt(ball.vx * ball.vx + ball.vy * ball.vy);
    ball.vx /= velocity_norm;
    ball.vy /= velocity_norm;

    rect.color.r = rand() % 255;
    rect.color.g = rand() % 255;
    rect.color.b = rand() % 255;
    rect.color.a = 255;
    break;
  }
  if (ball.x - ball.radius < 0 && ball.vx < 0) {
    ball.vx *= -1;
  }
  if (ball.x + ball.radius > SCREEN_WIDTH && ball.vx > 0) {
    ball.vx *= -1;
  }
  if (ball.y - ball.radius < 0) {
    ball.vy *= -1;
  }
}

void move_ball(float dt) {
  check_collisions();
  ball.x += dt * ball.vx * ball.speed;
  ball.y += dt * ball.vy * ball.speed;
}

int main() {
  fill_primitives();

  if (init() != 0) {
    return 1;
  }
  clear_window(0, 0, 0, 255);

  bool quit = false;
  bool ball_attached = true;
  SDL_Event event;
  TimePoint tp_prev = std::chrono::system_clock::now();

  while (!quit) {
    TimePoint tp_current = std::chrono::system_clock::now();
    float dt = get_dt(tp_current, tp_prev);
    tp_prev = tp_current;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        quit = true;
      }
      if (event.type == SDL_MOUSEMOTION) {
        slider_rectangle.pos.x = event.motion.x - slider_rectangle.dims.w / 2;
        slider_rectangle.pos.x = std::max(0, slider_rectangle.pos.x);
        slider_rectangle.pos.x = std::min(
            SCREEN_WIDTH - slider_rectangle.dims.w, slider_rectangle.pos.x);
      }
      if (event.type == SDL_MOUSEBUTTONUP && ball_attached) {
        ball_attached = false;
        ball.vy = -0.75;
        ball.vx = 0.25;
      }
    }

    if (ball_attached) {
      ball.x = slider_rectangle.pos.x + slider_rectangle.dims.w / 2;
      ball.y = slider_rectangle.pos.y - ball.radius;
    } else {
      move_ball(dt);
    }

    clear_window(0, 0, 0, 255);
    for (int i = 0; i < RECT_COUNT_X * RECT_COUNT_Y; i++) {
      if (rectangles[i].active) {
        draw_rect(rectangles[i]);
      }
    }

    draw_rect(slider_rectangle);
    draw_ball();
    SDL_RenderPresent(renderer);
    SDL_Delay(30);
    if (ball.y > SCREEN_HEIGHT) {
      fill_primitives();
      ball_attached = true;
      ball.x = slider_rectangle.pos.x + slider_rectangle.dims.w / 2;
      ball.y = slider_rectangle.pos.y - ball.radius;
    }
  }

  cleanup();
  return 0;
}
