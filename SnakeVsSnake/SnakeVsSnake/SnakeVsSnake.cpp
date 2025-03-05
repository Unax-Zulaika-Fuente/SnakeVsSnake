#include <windows.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>

// Tamaño de cada celda
#define GRID_SIZE 20
// Ancho y alto del area jugable (sin bordes)
#define PLAYABLE_WIDTH 640
#define PLAYABLE_HEIGHT 480
// Margen extra para mostrar bordes (debe ser multiplo de GRID_SIZE)
#define BORDER_MARGIN 20
// Ancho y alto total de la ventana (area jugable + margenes)
#define WINDOW_WIDTH (PLAYABLE_WIDTH + 3 * BORDER_MARGIN)
#define WINDOW_HEIGHT (PLAYABLE_HEIGHT + 4 * BORDER_MARGIN)

#define INITIAL_FOOD_COUNT 20
#define NO_EAT_THRESHOLD 5000 // 5 segundos sin comer

// Calcula el numero de columnas y filas en el area jugable
const int numCols = PLAYABLE_WIDTH / GRID_SIZE;
const int numRows = PLAYABLE_HEIGHT / GRID_SIZE;

// Enumeracion de direcciones
enum Direction { UP, DOWN, LEFT, RIGHT };

// Devuelve true si las direcciones son opuestas
bool isOpposite(Direction d1, Direction d2) {
    return ((d1 == UP && d2 == DOWN) ||
        (d1 == DOWN && d2 == UP) ||
        (d1 == LEFT && d2 == RIGHT) ||
        (d1 == RIGHT && d2 == LEFT));
}

// Estructura para representar un punto en la grilla
struct Point {
    int x, y;
};

// Clase para representar una serpiente (jugador o enemigo)
class Snake {
public:
    std::vector<Point> body;   // La cabeza es el primer elemento
    Direction dir;             // Direccion actual
    COLORREF color;            // Color de la serpiente
    DWORD lastEaten;           // Tiempo del ultimo alimento

    // Para el jugador: direccion pendiente para cambios rapidos
    bool hasPending;
    Direction pendingDir;

    // Constructor: las coordenadas deben incluir el offset del margen
    Snake(int startX, int startY, COLORREF col) : dir(RIGHT), color(col), hasPending(false) {
        for (int i = 0; i < 3; i++) {
            body.push_back({ startX - i * GRID_SIZE, startY });
        }
        lastEaten = GetTickCount();
    }

    // Aplica el cambio pendiente si es valido
    void ProcessPendingDirection() {
        if (hasPending && !isOpposite(pendingDir, dir)) {
            dir = pendingDir;
        }
        hasPending = false;
    }

    // Actualiza la posicion de la serpiente
    void Update() {
        ProcessPendingDirection();
        for (int i = body.size() - 1; i > 0; i--) {
            body[i] = body[i - 1];
        }
        // Actualiza la cabeza segun la direccion
        switch (dir) {
        case UP:    body[0].y -= GRID_SIZE; break;
        case DOWN:  body[0].y += GRID_SIZE; break;
        case LEFT:  body[0].x -= GRID_SIZE; break;
        case RIGHT: body[0].x += GRID_SIZE; break;
        }
    }

    // Agrega un segmento y actualiza el tiempo
    void Grow() {
        Point last = body.back();
        body.push_back(last);
        lastEaten = GetTickCount();
    }

    // Elimina un segmento si hay mas de 2 (cabeza + 1 cuerpo)
    void Shrink() {
        if (body.size() > 2) {
            body.pop_back();
        }
    }

    // Retorna true si algun segmento ocupa el punto pt
    bool CheckCollision(const Point& pt) {
        for (auto& p : body) {
            if (p.x == pt.x && p.y == pt.y)
                return true;
        }
        return false;
    }
};

// Calcula el centro del cuerpo de la serpiente
Point GetSnakeCenter(Snake* s) {
    int sumX = 0, sumY = 0;
    for (auto& p : s->body) {
        sumX += p.x;
        sumY += p.y;
    }
    Point center = { sumX / s->body.size(), sumY / s->body.size() };
    return center;
}

// Clase para representar un alimento
struct Food {
    Point pos;
    COLORREF color;
    Food(int x, int y) : pos({ x, y }), color(RGB(255, 0, 0)) {}
};

// Retorna true si moverse en la direccion d es seguro para la serpiente
bool IsDirectionSafe(Snake* s, Direction d) {
    Point trial = s->body[0];
    switch (d) {
    case UP:    trial.y -= GRID_SIZE; break;
    case DOWN:  trial.y += GRID_SIZE; break;
    case LEFT:  trial.x -= GRID_SIZE; break;
    case RIGHT: trial.x += GRID_SIZE; break;
    }
    // Comprueba que el punto este dentro del area jugable
    if (trial.x < BORDER_MARGIN || trial.x >= BORDER_MARGIN + PLAYABLE_WIDTH ||
        trial.y < BORDER_MARGIN || trial.y >= BORDER_MARGIN + PLAYABLE_HEIGHT)
        return false;
    // Comprueba que no colisione con el cuerpo
    for (size_t i = 1; i < s->body.size(); i++) {
        if (s->body[i].x == trial.x && s->body[i].y == trial.y)
            return false;
    }
    return true;
}

// Clase principal del juego
class Game {
public:
    Snake* player;           // Serpiente del jugador
    Snake* enemy;            // Serpiente del enemigo
    std::vector<Food> foods; // Vector de alimentos
    int foodSpawnInterval;   // Intervalo para crear alimento
    DWORD lastFoodSpawn;     // Ultimo tiempo de spawn
    bool gameOver;           // Estado del juego

    // Tiempo para reaparecer al enemigo
    DWORD enemyRespawnTime;

    bool highlightPlayerImpact;
    Point playerImpactPos;
    bool highlightEnemyImpact;
    Point enemyImpactPos;

    // Constructor: inicia las serpientes y genera alimentos
    Game() : foodSpawnInterval(2000), lastFoodSpawn(0), gameOver(false),
        enemyRespawnTime(0),
        highlightPlayerImpact(false), highlightEnemyImpact(false)
    {
        // Inicia jugador en (BORDER_MARGIN+40, BORDER_MARGIN+40)
        player = new Snake(BORDER_MARGIN + GRID_SIZE * 2, BORDER_MARGIN + GRID_SIZE * 2, RGB(0, 255, 0));
        // Inicia enemigo en la parte inferior derecha del area jugable
        enemy = new Snake(BORDER_MARGIN + PLAYABLE_WIDTH - GRID_SIZE * 3,
            BORDER_MARGIN + PLAYABLE_HEIGHT - GRID_SIZE * 3, RGB(0, 0, 255));
        for (int i = 0; i < INITIAL_FOOD_COUNT; i++) {
            SpawnFood();
        }
    }

    ~Game() {
        delete player;
        if (enemy)
            delete enemy;
    }

    // Crea alimento en una posicion aleatoria dentro del area jugable
    void SpawnFood() {
        int cols = PLAYABLE_WIDTH / GRID_SIZE;
        int rows = PLAYABLE_HEIGHT / GRID_SIZE;
        int x = BORDER_MARGIN + (std::rand() % cols) * GRID_SIZE;
        int y = BORDER_MARGIN + (std::rand() % rows) * GRID_SIZE;
        Point pt = { x, y };
        if (player->CheckCollision(pt) || (enemy && enemy->CheckCollision(pt)))
            return;
        foods.push_back(Food(x, y));
    }

    // Termina el juego si la cabeza del jugador sale del area jugable.
    // Para el enemigo se ajusta la posicion (clamp).
    void CheckBoundaries() {
        Point pHead = player->body[0];
        if (pHead.x < BORDER_MARGIN || pHead.x >= BORDER_MARGIN + PLAYABLE_WIDTH ||
            pHead.y < BORDER_MARGIN || pHead.y >= BORDER_MARGIN + PLAYABLE_HEIGHT)
        {
            gameOver = true;
            return;
        }
        if (enemy) {
            Point eHead = enemy->body[0];
            if (eHead.x < BORDER_MARGIN) eHead.x = BORDER_MARGIN;
            if (eHead.x >= BORDER_MARGIN + PLAYABLE_WIDTH) eHead.x = BORDER_MARGIN + PLAYABLE_WIDTH - GRID_SIZE;
            if (eHead.y < BORDER_MARGIN) eHead.y = BORDER_MARGIN;
            if (eHead.y >= BORDER_MARGIN + PLAYABLE_HEIGHT) eHead.y = BORDER_MARGIN + PLAYABLE_HEIGHT - GRID_SIZE;
            enemy->body[0] = eHead;
        }
    }

    // Comprueba colisiones entre serpientes y auto-colisiones
    void CheckSnakeCollisions() {
        // Auto-colision del jugador
        for (size_t i = 1; i < player->body.size(); i++) {
            if (player->body[0].x == player->body[i].x &&
                player->body[0].y == player->body[i].y)
            {
                highlightPlayerImpact = true;
                playerImpactPos = player->body[0];
                highlightEnemyImpact = false;
                gameOver = true;
                return;
            }
        }
        if (enemy) {
            // Auto-colision del enemigo
            for (size_t i = 1; i < enemy->body.size(); i++) {
                if (enemy->body[0].x == enemy->body[i].x &&
                    enemy->body[0].y == enemy->body[i].y)
                {
                    highlightEnemyImpact = true;
                    enemyImpactPos = enemy->body[0];
                    highlightPlayerImpact = false;
                    DWORD currentTime = GetTickCount();
                    enemyRespawnTime = currentTime + 5000;
                    delete enemy;
                    enemy = nullptr;
                    return;
                }
            }
            // La cabeza del jugador contra el cuerpo del enemigo
            for (size_t i = 1; i < enemy->body.size(); i++) {
                if (player->body[0].x == enemy->body[i].x &&
                    player->body[0].y == enemy->body[i].y)
                {
                    highlightPlayerImpact = true;
                    playerImpactPos = player->body[0];
                    highlightEnemyImpact = false;
                    gameOver = true;
                    return;
                }
            }
            // Colision cabeza a cabeza
            if (player->body[0].x == enemy->body[0].x &&
                player->body[0].y == enemy->body[0].y)
            {
                highlightPlayerImpact = true;
                playerImpactPos = player->body[0];
                highlightEnemyImpact = false;
                gameOver = true;
                return;
            }
            // La cabeza del enemigo contra el cuerpo del jugador
            for (size_t i = 1; i < player->body.size(); i++) {
                if (enemy->body[0].x == player->body[i].x &&
                    enemy->body[0].y == player->body[i].y)
                {
                    highlightEnemyImpact = true;
                    enemyImpactPos = enemy->body[0];
                    highlightPlayerImpact = false;
                    DWORD currentTime = GetTickCount();
                    enemyRespawnTime = currentTime + 5000;
                    delete enemy;
                    enemy = nullptr;
                    return;
                }
            }
        }
    }

    // Si no come, se reduce la longitud (minimo 2 segmentos)
    void CheckNoEatTimeout() {
        DWORD currentTime = GetTickCount();
        if (currentTime - player->lastEaten > NO_EAT_THRESHOLD) {
            player->Shrink();
            player->lastEaten = currentTime;
        }
        if (enemy && currentTime - enemy->lastEaten > NO_EAT_THRESHOLD) {
            enemy->Shrink();
            enemy->lastEaten = currentTime;
        }
    }

    // Actualiza la logica del juego
    void Update() {
        if (gameOver)
            return;

        player->Update();
        if (enemy) {
            UpdateEnemy();
            enemy->Update();
        }
        CheckBoundaries();
        if (gameOver)
            return;

        // Procesa la comida
        for (size_t i = 0; i < foods.size(); ) {
            if (player->body[0].x == foods[i].pos.x && player->body[0].y == foods[i].pos.y) {
                player->Grow();
                foods.erase(foods.begin() + i);
            }
            else if (enemy && enemy->body[0].x == foods[i].pos.x && enemy->body[0].y == foods[i].pos.y) {
                enemy->Grow();
                foods.erase(foods.begin() + i);
            }
            else {
                i++;
            }
        }

        CheckSnakeCollisions();
        CheckNoEatTimeout();

        DWORD currentTime = GetTickCount();
        if (currentTime - lastFoodSpawn > foodSpawnInterval && foods.size() < 20) {
            SpawnFood();
            lastFoodSpawn = currentTime;
            if (foodSpawnInterval < 5000)
                foodSpawnInterval += 100;
        }

        if (!enemy && currentTime >= enemyRespawnTime) {
            int enemyX = (player->body[0].x < BORDER_MARGIN + PLAYABLE_WIDTH / 2) ?
                BORDER_MARGIN + PLAYABLE_WIDTH - GRID_SIZE * 3 : BORDER_MARGIN + GRID_SIZE;
            int enemyY = (player->body[0].y < BORDER_MARGIN + PLAYABLE_HEIGHT / 2) ?
                BORDER_MARGIN + PLAYABLE_HEIGHT - GRID_SIZE * 3 : BORDER_MARGIN + GRID_SIZE;
            enemy = new Snake(enemyX, enemyY, RGB(0, 0, 255));
            int centerX = BORDER_MARGIN + PLAYABLE_WIDTH / 2;
            int centerY = BORDER_MARGIN + PLAYABLE_HEIGHT / 2;
            int dx = centerX - enemyX;
            int dy = centerY - enemyY;
            if (abs(dx) > abs(dy))
                enemy->dir = (dx > 0) ? RIGHT : LEFT;
            else
                enemy->dir = (dy > 0) ? DOWN : UP;
        }
    }

    // Actualiza la direccion del enemigo, priorizando la seguridad para no chocar contra su cuerpo.
    void UpdateEnemy() {
        if (!enemy)
            return;

        if (!IsDirectionSafe(enemy, enemy->dir)) {
            std::vector<Direction> candidates = { UP, DOWN, LEFT, RIGHT };
            bool found = false;
            Direction bestDir = enemy->dir;
            int bestScore = 100000;
            Point target;
            if (foods.size() > 5) {
                int bestDist = 100000;
                target = enemy->body[0];
                for (auto& food : foods) {
                    int dx = enemy->body[0].x - food.pos.x;
                    int dy = enemy->body[0].y - food.pos.y;
                    int dist = abs(dx) + abs(dy);
                    if (dist < bestDist) {
                        bestDist = dist;
                        target = food.pos;
                    }
                }
            }
            else {
                target = GetSnakeCenter(player);
            }
            for (Direction d : candidates) {
                if (isOpposite(d, enemy->dir))
                    continue;
                if (!IsDirectionSafe(enemy, d))
                    continue;
                Point trial = enemy->body[0];
                switch (d) {
                case UP:    trial.y -= GRID_SIZE; break;
                case DOWN:  trial.y += GRID_SIZE; break;
                case LEFT:  trial.x -= GRID_SIZE; break;
                case RIGHT: trial.x += GRID_SIZE; break;
                }
                int score = abs(trial.x - target.x) + abs(trial.y - target.y);
                if (score < bestScore) {
                    bestScore = score;
                    bestDir = d;
                    found = true;
                }
            }
            if (found)
                enemy->dir = bestDir;
        }
        else {
            int bestDist = 100000;
            Point target;
            if (foods.size() > 5) {
                target = enemy->body[0];
                for (auto& food : foods) {
                    int dx = enemy->body[0].x - food.pos.x;
                    int dy = enemy->body[0].y - food.pos.y;
                    int dist = abs(dx) + abs(dy);
                    if (dist < bestDist) {
                        bestDist = dist;
                        target = food.pos;
                    }
                }
            }
            else {
                target = GetSnakeCenter(player);
            }
            if (abs(enemy->body[0].x - target.x) > abs(enemy->body[0].y - target.y))
                enemy->dir = (enemy->body[0].x > target.x) ? LEFT : RIGHT;
            else
                enemy->dir = (enemy->body[0].y > target.y) ? UP : DOWN;
        }
    }

    // Calcula el rectangulo de la cabeza con "notching" para que el lado en contacto con el cuerpo se dibuje completo.
    RECT GetHeadRect(Snake* s) {
        RECT r;
        int insetLeft = 4, insetTop = 4, insetRight = 4, insetBottom = 4;
        switch (s->dir) {
        case UP:    insetBottom = 0; break;
        case DOWN:  insetTop = 0; break;
        case LEFT:  insetRight = 0; break;
        case RIGHT: insetLeft = 0; break;
        }
        r.left = s->body[0].x + insetLeft;
        r.top = s->body[0].y + insetTop;
        r.right = s->body[0].x + GRID_SIZE - insetRight;
        r.bottom = s->body[0].y + GRID_SIZE - insetBottom;
        return r;
    }

    // Dibuja todo el juego.
    void Render(HDC hdc) {
        // Dibuja el fondo de la ventana.
        HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
        RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
        FillRect(hdc, &rect, blackBrush);
        DeleteObject(blackBrush);

        // Dibuja los bordes. Se dibujan las celdas exteriores de la ventana.
        HBRUSH borderBrush = CreateSolidBrush(RGB(50, 50, 50));
        for (int row = 0; row < (WINDOW_HEIGHT / GRID_SIZE); row++) {
            for (int col = 0; col < (WINDOW_WIDTH / GRID_SIZE); col++) {
                // Si la celda esta fuera del area jugable, se dibuja.
                if (row < BORDER_MARGIN / GRID_SIZE || row >= BORDER_MARGIN / GRID_SIZE + numRows ||
                    col < BORDER_MARGIN / GRID_SIZE || col >= BORDER_MARGIN / GRID_SIZE + numCols) {
                    RECT r = { col * GRID_SIZE, row * GRID_SIZE, col * GRID_SIZE + GRID_SIZE, row * GRID_SIZE + GRID_SIZE };
                    FillRect(hdc, &r, borderBrush);
                }
            }
        }
        DeleteObject(borderBrush);

        // Dibuja la comida.
        for (auto& food : foods) {
            HBRUSH foodBrush = CreateSolidBrush(food.color);
            RECT r = { food.pos.x, food.pos.y, food.pos.x + GRID_SIZE, food.pos.y + GRID_SIZE };
            FillRect(hdc, &r, foodBrush);
            DeleteObject(foodBrush);
        }

        DWORD now = GetTickCount();
        bool drawFlash = ((now / 250) % 2 == 0);

        // Dibuja el enemigo.
        if (enemy) {
            HBRUSH enemyBrush = CreateSolidBrush(enemy->color);
            for (size_t i = 0; i < enemy->body.size(); i++) {
                RECT r;
                if (i == 0)
                    r = GetHeadRect(enemy);
                else {
                    r.left = enemy->body[i].x;
                    r.top = enemy->body[i].y;
                    r.right = enemy->body[i].x + GRID_SIZE;
                    r.bottom = enemy->body[i].y + GRID_SIZE;
                }
                FillRect(hdc, &r, enemyBrush);
            }
            DeleteObject(enemyBrush);
        }

        // Dibuja el jugador.
        if (gameOver) {
            HBRUSH bodyBrush = CreateSolidBrush(player->color);
            for (size_t i = 1; i < player->body.size(); i++) {
                RECT r = { player->body[i].x, player->body[i].y,
                           player->body[i].x + GRID_SIZE, player->body[i].y + GRID_SIZE };
                FillRect(hdc, &r, bodyBrush);
            }
            DeleteObject(bodyBrush);
            HBRUSH headBrush = CreateSolidBrush(drawFlash ? RGB(255, 255, 0) : player->color);
            RECT headRect = GetHeadRect(player);
            FillRect(hdc, &headRect, headBrush);
            DeleteObject(headBrush);
        }
        else {
            HBRUSH playerBrush = CreateSolidBrush(player->color);
            for (size_t i = 0; i < player->body.size(); i++) {
                RECT r;
                if (i == 0)
                    r = GetHeadRect(player);
                else {
                    r.left = player->body[i].x;
                    r.top = player->body[i].y;
                    r.right = player->body[i].x + GRID_SIZE;
                    r.bottom = player->body[i].y + GRID_SIZE;
                }
                FillRect(hdc, &r, playerBrush);
            }
            DeleteObject(playerBrush);
        }

        // Muestra el mensaje "GAME OVER" si el juego termino.
        if (gameOver) {
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);
            const wchar_t* msg = L"GAME OVER";
            DrawText(hdc, msg, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
};

Game* game = nullptr;
const wchar_t g_szClassName[] = L"SnakeVsSnakeWindow";

// Procedimiento de ventana
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        game = new Game();
        SetTimer(hwnd, 1, 100, NULL);
        break;
    case WM_KEYDOWN:
        if (game->gameOver) {
            // Reinicia el juego al presionar cualquier tecla en Game Over
            delete game;
            game = new Game();
        }
        else {
            switch (wParam) {
            case VK_UP:
                if (game->player->dir != DOWN) {
                    game->player->pendingDir = UP;
                    game->player->hasPending = true;
                }
                break;
            case VK_DOWN:
                if (game->player->dir != UP) {
                    game->player->pendingDir = DOWN;
                    game->player->hasPending = true;
                }
                break;
            case VK_LEFT:
                if (game->player->dir != RIGHT) {
                    game->player->pendingDir = LEFT;
                    game->player->hasPending = true;
                }
                break;
            case VK_RIGHT:
                if (game->player->dir != LEFT) {
                    game->player->pendingDir = RIGHT;
                    game->player->hasPending = true;
                }
                break;
            }
        }
        break;
    case WM_TIMER:
        if (!game->gameOver) {
            game->Update();
        }
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        game->Render(hdc);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow) {
    srand((unsigned)time(NULL));
    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = g_szClassName;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Error al registrar la clase de ventana!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        g_szClassName,
        L"Snake vs Snake - WinAPI",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        MessageBox(NULL, L"Error al crear la ventana!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage(&Msg, NULL, 0, 0) > 0) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    if (game) {
        delete game;
    }
    return Msg.wParam;
}
